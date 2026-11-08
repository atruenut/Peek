#include "focus/focus_inspector.h"

#include "core/com_scope.h"
#include "core/format.h"
#include "core/window_util.h"

#include <ole2.h>
#include <uiautomation.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>

namespace peek {
namespace {

using Microsoft::WRL::ComPtr;

constexpr std::size_t kMaxChildren = 80;
constexpr int kMaxAncestors = 24;
constexpr std::size_t kMaxValueChars = 4000;

std::wstring TakeBstr(BSTR value) {
    std::wstring result = value ? value : L"";
    if (value) SysFreeString(value);
    return result;
}

bool UsableRect(const RECT& r) { return r.right > r.left && r.bottom > r.top; }

std::wstring ReadStringProperty(IUIAutomationElement* element, HRESULT (STDMETHODCALLTYPE IUIAutomationElement::*getter)(BSTR*)) {
    BSTR text{};
    if (FAILED((element->*getter)(&text))) return {};
    return TakeBstr(text);
}

// Reads every property Peek displays. Each property is optional: providers differ widely, so a failed read
// leaves the field empty instead of guessing.
AutomationInfo ReadProperties(IUIAutomationElement* element) {
    AutomationInfo info;
    if (!element) return info;
    info.available = true;
    info.automation_id = ReadStringProperty(element, &IUIAutomationElement::get_CurrentAutomationId);
    info.control_type = ReadStringProperty(element, &IUIAutomationElement::get_CurrentLocalizedControlType);
    info.class_name = ReadStringProperty(element, &IUIAutomationElement::get_CurrentClassName);
    info.framework = ReadStringProperty(element, &IUIAutomationElement::get_CurrentFrameworkId);
    info.name = ReadStringProperty(element, &IUIAutomationElement::get_CurrentName);

    BOOL flag = TRUE;
    if (SUCCEEDED(element->get_CurrentIsEnabled(&flag))) info.enabled = flag != FALSE;
    flag = FALSE;
    if (SUCCEEDED(element->get_CurrentIsOffscreen(&flag))) info.offscreen = flag != FALSE;

    flag = FALSE;
    element->get_CurrentIsPassword(&flag);
    if (flag) {
        info.value_hidden = true;   // never read the contents of a password field
    } else {
        ComPtr<IUIAutomationValuePattern> pattern;
        if (SUCCEEDED(element->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&pattern))) && pattern) {
            BSTR text{};
            if (SUCCEEDED(pattern->get_CurrentValue(&text))) {
                info.value = TakeBstr(text);
                info.value_available = true;
                if (info.value.size() > kMaxValueChars) info.value = info.value.substr(0, kMaxValueChars) + L"…";
            }
        }
    }

    RECT bounds{};
    if (SUCCEEDED(element->get_CurrentBoundingRectangle(&bounds)) && UsableRect(bounds) && !info.offscreen) {
        info.bounds = bounds;
        info.bounds_valid = true;
    }
    return info;
}

UiaNode ReadNode(IUIAutomationElement* element, int id, int parent) {
    UiaNode node;
    node.id = id;
    node.parent = parent;
    node.name = ReadStringProperty(element, &IUIAutomationElement::get_CurrentName);
    if (node.name.size() > 120) node.name = node.name.substr(0, 119) + L"…";
    node.control_type = ReadStringProperty(element, &IUIAutomationElement::get_CurrentLocalizedControlType);
    node.automation_id = ReadStringProperty(element, &IUIAutomationElement::get_CurrentAutomationId);
    node.class_name = ReadStringProperty(element, &IUIAutomationElement::get_CurrentClassName);
    element->get_CurrentBoundingRectangle(&node.bounds);
    return node;
}

// Builds a snapshot from an element. `hwnd` is the window that owns it (may be null when unknown).
FocusSnapshot BuildSnapshot(ProcessResolver& resolver, IUIAutomationElement* element, HWND hwnd, const POINT* cursor,
                            DWORD& cached_pid, ProcessInfo& cached_process) {
    FocusSnapshot snapshot;
    if (cursor) { snapshot.cursor = *cursor; snapshot.has_cursor = true; }
    snapshot.window = ReadWindowInfo(hwnd);
    if (snapshot.window.pid != 0) {
        if (cached_pid != snapshot.window.pid) {
            cached_process = resolver.Resolve(snapshot.window.pid);
            cached_pid = snapshot.window.pid;
        }
        snapshot.process = cached_process;
    }
    snapshot.automation = ReadProperties(element);
    if (snapshot.automation.bounds_valid) {
        snapshot.highlight = snapshot.automation.bounds;
        snapshot.highlight_from_uia = true;
    } else {
        snapshot.highlight = snapshot.window.bounds;   // fall back to the window when UIA bounds are missing
    }
    return snapshot;
}

} // namespace

FocusInspector::FocusInspector(ProcessResolver& resolver, SnapshotCallback on_snapshot, TreeCallback on_tree, ModeCallback on_mode)
    : resolver_(resolver), on_snapshot_(std::move(on_snapshot)), on_tree_(std::move(on_tree)), on_mode_(std::move(on_mode)) {}

FocusInspector::~FocusInspector() {
    {
        std::lock_guard lock(mutex_);
        quit_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void FocusInspector::EnsureWorker() {
    std::lock_guard lock(mutex_);
    if (!worker_.joinable()) worker_ = std::thread([this] { Loop(); });
}

void FocusInspector::Wake() {
    {
        std::lock_guard lock(mutex_);
        changed_ = true;
    }
    wake_.notify_all();
}

void FocusInspector::SetMode(InspectMode mode, bool notify) {
    const int previous = mode_.exchange(static_cast<int>(mode));
    if (notify && previous != static_cast<int>(mode) && on_mode_) on_mode_(mode);
    Wake();
}

void FocusInspector::StartLive() { EnsureWorker(); SetMode(InspectMode::Live, true); }

void FocusInspector::Freeze() {
    if (Mode() == InspectMode::Live) SetMode(InspectMode::Frozen, true);
}

void FocusInspector::Pin() {
    const InspectMode mode = Mode();
    if (mode == InspectMode::Live || mode == InspectMode::Frozen) SetMode(InspectMode::Pinned, true);
}

void FocusInspector::Stop() { SetMode(InspectMode::Off, true); }

void FocusInspector::InspectWindow(HWND hwnd) {
    EnsureWorker();
    {
        std::lock_guard lock(mutex_);
        requests_.push_back({Request::Type::Window, 0, hwnd});
        changed_ = true;
    }
    wake_.notify_all();
}

void FocusInspector::RequestTree() {
    if (!Active()) return;
    EnsureWorker();
    {
        std::lock_guard lock(mutex_);
        requests_.push_back({Request::Type::Tree, 0, nullptr});
        changed_ = true;
    }
    wake_.notify_all();
}

void FocusInspector::ExpandNode(int node_id) {
    if (!Active()) return;
    EnsureWorker();
    {
        std::lock_guard lock(mutex_);
        requests_.push_back({Request::Type::Expand, node_id, nullptr});
        changed_ = true;
    }
    wake_.notify_all();
}

bool FocusInspector::InspectOnce(POINT point, FocusSnapshot& snapshot, std::wstring& error) const {
    error.clear();
    snapshot = {};
    ComScope com;
    if (!com.Usable()) { error = L"COM could not be initialized."; return false; }
    ComPtr<IUIAutomation> automation;
    const HRESULT created = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation));
    if (FAILED(created)) error = L"UI Automation could not be started.";
    HWND hwnd = WindowFromPoint(point);
    if (!hwnd) { if (error.empty()) error = L"No window was found at that point."; return false; }
    ComPtr<IUIAutomationElement> element;
    if (automation && FAILED(automation->ElementFromPoint(point, &element))) element.Reset();
    DWORD cached_pid = 0;
    ProcessInfo cached;
    snapshot = BuildSnapshot(resolver_, element.Get(), hwnd, &point, cached_pid, cached);
    return true;
}

void FocusInspector::Loop() {
    ComScope com;
    ComPtr<IUIAutomation> automation;
    ComPtr<IUIAutomationTreeWalker> walker;
    if (com.Usable() &&
        SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation)))) {
        ComPtr<IUIAutomation6> automation6;
        if (SUCCEEDED(automation.As(&automation6))) {
            // A hung target application must not hold the inspector for the default 20 seconds.
            automation6->put_ConnectionTimeout(1500);
            automation6->put_TransactionTimeout(1500);
        }
        automation->get_ControlViewWalker(&walker);
    }

    ComPtr<IUIAutomationElement> current;       // the selected element
    HWND current_hwnd{};
    bool current_dead = false;
    FocusSnapshot last;
    POINT last_point{LONG_MIN, LONG_MIN};
    DWORD cached_pid = 0;
    ProcessInfo cached_process;
    std::vector<ComPtr<IUIAutomationElement>> node_elements;
    std::vector<bool> node_loaded;

    auto emit = [&](FocusSnapshot snapshot) {
        last = snapshot;
        if (on_snapshot_) on_snapshot_(std::move(snapshot));
    };

    auto list_children = [&](IUIAutomationElement* parent, int parent_id, std::vector<UiaNode>& out) {
        if (!walker || !parent) return;
        ComPtr<IUIAutomationElement> child;
        if (FAILED(walker->GetFirstChildElement(parent, &child)) || !child) return;
        while (child && out.size() < kMaxChildren) {
            const int id = static_cast<int>(node_elements.size());
            node_elements.push_back(child);
            node_loaded.push_back(false);
            out.push_back(ReadNode(child.Get(), id, parent_id));
            ComPtr<IUIAutomationElement> next;
            if (FAILED(walker->GetNextSiblingElement(child.Get(), &next))) break;
            child = next;
        }
    };

    auto build_tree = [&]() {
        if (!current || !walker || !on_tree_) return;
        node_elements.clear();
        node_loaded.clear();
        std::vector<ComPtr<IUIAutomationElement>> chain;   // selected element first, then its ancestors
        chain.push_back(current);
        for (int i = 0; i < kMaxAncestors; ++i) {
            ComPtr<IUIAutomationElement> parent;
            if (FAILED(walker->GetParentElement(chain.back().Get(), &parent)) || !parent) break;
            chain.push_back(parent);
        }
        FocusTreeUpdate update;
        update.reset = true;
        int parent_id = -1;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const int id = static_cast<int>(node_elements.size());
            node_elements.push_back(*it);
            node_loaded.push_back(true);   // the path to the selected element is already shown
            UiaNode node = ReadNode(it->Get(), id, parent_id);
            node.has_children = true;
            update.nodes.push_back(std::move(node));
            parent_id = id;
        }
        update.nodes.back().is_selected = true;
        node_loaded.back() = true;
        list_children(current.Get(), parent_id, update.nodes);
        on_tree_(std::move(update));
    };

    auto select_element = [&](ComPtr<IUIAutomationElement> element, HWND hwnd, const POINT* cursor) {
        current = std::move(element);
        current_hwnd = hwnd;
        current_dead = false;
        emit(BuildSnapshot(resolver_, current.Get(), hwnd, cursor, cached_pid, cached_process));
    };

    auto release_all = [&]() {
        current.Reset();
        current_hwnd = nullptr;
        current_dead = false;
        node_elements.clear();
        node_loaded.clear();
        last_point = {LONG_MIN, LONG_MIN};
    };

    bool escape_was_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

    for (;;) {
        std::deque<Request> work;
        {
            std::unique_lock lock(mutex_);
            const InspectMode mode = Mode();
            const auto interval = mode == InspectMode::Live ? std::chrono::milliseconds(50)
                                : mode == InspectMode::Pinned ? std::chrono::milliseconds(300)
                                : std::chrono::hours(1);   // Off and Frozen: nothing to poll, sleep until woken
            wake_.wait_for(lock, interval, [&] { return quit_ || changed_ || !requests_.empty(); });
            if (quit_) break;
            changed_ = false;
            work.swap(requests_);
        }

        InspectMode mode = Mode();
        if (mode == InspectMode::Off && current) release_all();

        for (const Request& request : work) {
            if (!automation) break;
            switch (request.type) {
                case Request::Type::Window: {
                    ComPtr<IUIAutomationElement> element;
                    if (IsWindow(request.hwnd) && SUCCEEDED(automation->ElementFromHandle(request.hwnd, &element)) && element) {
                        select_element(element, request.hwnd, nullptr);
                        SetMode(InspectMode::Pinned, true);
                        mode = InspectMode::Pinned;
                        build_tree();
                    }
                    break;
                }
                case Request::Type::Tree:
                    if (mode != InspectMode::Off) build_tree();
                    break;
                case Request::Type::Expand: {
                    const int id = request.node_id;
                    if (id < 0 || id >= static_cast<int>(node_elements.size()) || node_loaded[static_cast<std::size_t>(id)]) break;
                    node_loaded[static_cast<std::size_t>(id)] = true;
                    FocusTreeUpdate update;
                    update.parent_id = id;
                    list_children(node_elements[static_cast<std::size_t>(id)].Get(), id, update.nodes);
                    if (on_tree_) on_tree_(std::move(update));
                    break;
                }
            }
        }

        if (!automation) continue;

        if (mode == InspectMode::Live) {
            const bool escape_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
            if (escape_down && !escape_was_down) {
                escape_was_down = true;
                SetMode(InspectMode::Off, true);   // Esc stops inspecting; the key is not intercepted
                continue;
            }
            escape_was_down = escape_down;
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) continue;   // holding Shift freezes the target temporarily
            POINT point{};
            if (!GetCursorPos(&point) || (point.x == last_point.x && point.y == last_point.y)) continue;
            last_point = point;
            HWND hwnd = WindowFromPoint(point);
            if (!hwnd) continue;
            DWORD owner = 0;
            GetWindowThreadProcessId(hwnd, &owner);
            if (owner == GetCurrentProcessId()) continue;   // never inspect Peek itself: the last target stays selected
            ComPtr<IUIAutomationElement> element;
            if (FAILED(automation->ElementFromPoint(point, &element)) || !element) {
                automation->ElementFromHandle(hwnd, &element);
            }
            if (element) select_element(element, hwnd, &point);
        } else if (mode == InspectMode::Pinned && current) {
            // Track the pinned element: refresh its bounds so the highlight follows moves and resizes.
            RECT bounds{};
            const HRESULT hr = current->get_CurrentBoundingRectangle(&bounds);
            if (FAILED(hr)) {
                if (!current_dead) {
                    current_dead = true;
                    FocusSnapshot gone = last;
                    gone.element_alive = false;
                    emit(std::move(gone));
                }
            } else if (!EqualRect(&bounds, &last.automation.bounds) && UsableRect(bounds)) {
                FocusSnapshot updated = BuildSnapshot(resolver_, current.Get(), current_hwnd, nullptr, cached_pid, cached_process);
                updated.cursor = last.cursor;
                updated.has_cursor = last.has_cursor;
                emit(std::move(updated));
            }
        } else if (mode == InspectMode::Pinned && !current) {
            SetMode(InspectMode::Off, true);
        }
        if (mode != InspectMode::Live) escape_was_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    }
}

} // namespace peek
