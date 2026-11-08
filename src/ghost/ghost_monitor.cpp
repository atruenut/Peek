#include "ghost/ghost_monitor.h"

#include "core/com_scope.h"
#include "core/strings.h"

#include <psapi.h>
#include <wbemidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace peek {

using Microsoft::WRL::ComPtr;
GhostMonitor* GhostMonitor::instance_ = nullptr;

namespace {

constexpr std::size_t kMaxTrackedWindows = 8192;
constexpr std::size_t kMaxCachedProcesses = 512;
constexpr ULONGLONG kProcessCacheMs = 2000;

DWORD WmiDword(IWbemClassObject* object, const wchar_t* name) {
    VARIANT value;
    VariantInit(&value);
    DWORD result = 0;
    if (SUCCEEDED(object->Get(name, 0, &value, nullptr, nullptr))) {
        if (value.vt == VT_UI4 || value.vt == VT_UINT) result = value.ulVal;
        else if (value.vt == VT_I4 || value.vt == VT_INT) result = static_cast<DWORD>(value.lVal);
    }
    VariantClear(&value);
    return result;
}

std::wstring WmiString(IWbemClassObject* object, const wchar_t* name) {
    VARIANT value;
    VariantInit(&value);
    std::wstring result;
    if (SUCCEEDED(object->Get(name, 0, &value, nullptr, nullptr)) && value.vt == VT_BSTR && value.bstrVal) {
        result = value.bstrVal;
    }
    VariantClear(&value);
    return result;
}

WindowInfo SnapshotWindow(HWND hwnd) {
    WindowInfo info;
    info.hwnd = hwnd;
    GetWindowThreadProcessId(hwnd, &info.pid);
    wchar_t text[512]{};
    const int title_len = GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
    if (title_len > 0) info.title.assign(text, title_len);
    const int class_len = GetClassNameW(hwnd, text, static_cast<int>(std::size(text)));
    if (class_len > 0) info.class_name.assign(text, class_len);
    GetWindowRect(hwnd, &info.bounds);
    return info;
}

std::wstring HResultText(const wchar_t* operation, HRESULT result) {
    std::wstringstream text;
    text << operation << L" failed (0x" << std::uppercase << std::hex << static_cast<unsigned long>(result) << L").";
    return text.str();
}


std::vector<DWORD> SnapshotPids() {
    std::vector<DWORD> pids(1024);
    DWORD bytes = 0;
    while (EnumProcesses(pids.data(), static_cast<DWORD>(pids.size() * sizeof(DWORD)), &bytes)) {
        if (bytes < pids.size() * sizeof(DWORD)) break;
        pids.resize(pids.size() * 2);
    }
    pids.resize(bytes / sizeof(DWORD));
    pids.erase(std::remove(pids.begin(), pids.end(), 0u), pids.end());
    return pids;
}

ProcessInfo PidOnly(DWORD pid) {
    ProcessInfo info;
    info.pid = pid;
    return info;
}

long long SteadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Helper windows that appear constantly and tell the user nothing about what an application is doing.
bool IsNoiseClass(const std::wstring& name) {
    static const wchar_t* const kNoise[] = {
        L"IME", L"MSCTFIME UI", L"CiceroUIWndFrame", L"tooltips_class32", L"SysShadow",
        L"#32768", L"DDEMLEvent", L"DDEMLMom", L"ForegroundStaging", L"MS_WebcheckMonitor",
    };
    for (const wchar_t* candidate : kNoise) if (name == candidate) return true;
    return false;
}

} // namespace

GhostMonitor::GhostMonitor(EventStore& store, ProcessResolver& resolver, Notify notify)
    : store_(store), resolver_(resolver), notify_(std::move(notify)) {}

GhostMonitor::~GhostMonitor() { Stop(); }

void GhostMonitor::SetMessage(const std::wstring& message) {
    std::lock_guard lock(status_mutex_);
    message_ = message;
}

GhostStatus GhostMonitor::Status() const {
    GhostStatus status;
    status.running = running_;
    status.window_hooks = hooks_ok_;
    status.wmi = wmi_ok_;
    status.polling = polling_;
    std::lock_guard lock(status_mutex_);
    status.message = message_;
    return status;
}

void GhostMonitor::SetTrace(TraceMode mode) {
    trace_mode_ = static_cast<int>(mode);
    switch (mode) {
        case TraceMode::Seconds30: trace_deadline_ms_ = SteadyMs() + 30 * 1000; break;
        case TraceMode::Minutes5: trace_deadline_ms_ = SteadyMs() + 5 * 60 * 1000; break;
        default: trace_deadline_ms_ = 0; break;
    }
}

bool GhostMonitor::TraceActive() const {
    switch (static_cast<TraceMode>(trace_mode_.load())) {
        case TraceMode::Off: return false;
        case TraceMode::Continuous: return true;
        default: return SteadyMs() < trace_deadline_ms_.load();
    }
}

int GhostMonitor::TraceSecondsLeft() const {
    const auto mode = static_cast<TraceMode>(trace_mode_.load());
    if (mode != TraceMode::Seconds30 && mode != TraceMode::Minutes5) return -1;
    const long long left = trace_deadline_ms_.load() - SteadyMs();
    return left > 0 ? static_cast<int>((left + 999) / 1000) : 0;
}

bool GhostMonitor::Start() {
    if (running_) return true;
    if (instance_) {
        SetMessage(L"Only one Ghost monitor can run in a process.");
        return false;
    }
    stopping_ = false;
    hooks_ok_ = false;
    wmi_ok_ = false;
    polling_ = false;
    SetMessage(L"");

    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    window_ready_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    wmi_ready_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stop_event_ || !window_ready_ || !wmi_ready_) {
        SetMessage(L"Ghost synchronization events could not be created.");
        for (HANDLE* handle : {&stop_event_, &window_ready_, &wmi_ready_}) {
            if (*handle) { CloseHandle(*handle); *handle = nullptr; }
        }
        return false;
    }
    instance_ = this;
    running_ = true;
    window_thread_ = std::thread([this] { WindowLoop(); });
    wmi_thread_ = std::thread([this] { WmiLoop(); });
    return true;
}

bool GhostMonitor::WaitUntilReady(std::chrono::milliseconds timeout) {
    if (!running_) return false;
    HANDLE handles[] = {window_ready_, wmi_ready_};
    return WaitForMultipleObjects(2, handles, TRUE, static_cast<DWORD>(timeout.count())) == WAIT_OBJECT_0;
}

void GhostMonitor::Stop() {
    if (!running_ && !instance_) return;
    stopping_ = true;
    if (stop_event_) SetEvent(stop_event_);
    if (window_thread_.joinable()) window_thread_.join();   // unhooks on its own thread
    if (wmi_thread_.joinable()) wmi_thread_.join();
    {
        std::lock_guard lock(thread_mutex_);
        if (poll_thread_.joinable()) poll_thread_.join();
    }
    for (HANDLE* handle : {&window_ready_, &wmi_ready_, &stop_event_}) {
        if (*handle) { CloseHandle(*handle); *handle = nullptr; }
    }
    if (instance_ == this) instance_ = nullptr;
    windows_.clear();
    running_ = false;
    hooks_ok_ = false;
    wmi_ok_ = false;
    polling_ = false;
}

void GhostMonitor::Emit(GhostEvent event) {
    store_.Add(std::move(event));
    if (notify_) notify_();
}

// ---------------------------------------------------------------------------------------------------
// Window events

void CALLBACK GhostMonitor::WinEventCallback(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG object_id, LONG child_id,
                                             DWORD, DWORD) {
    GhostMonitor* self = instance_;
    if (!self || !hwnd || object_id != OBJID_WINDOW || child_id != CHILDID_SELF) return;
    self->OnWindowEvent(event, hwnd);
}

void GhostMonitor::WindowLoop() {
    // Out-of-context WinEvent callbacks are delivered to the thread that installed the hook through its
    // message queue, so this thread must keep pumping messages until asked to stop.
    create_destroy_hook_ = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, nullptr, WinEventCallback, 0, 0,
                                           WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    show_hide_hook_ = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE, nullptr, WinEventCallback, 0, 0,
                                      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    hooks_ok_ = create_destroy_hook_ != nullptr && show_hide_hook_ != nullptr;
    if (!hooks_ok_) SetMessage(L"Window events could not be hooked; only process activity is reported.");
    SetEvent(window_ready_);

    while (!stopping_) {
        const DWORD wait = MsgWaitForMultipleObjects(1, &stop_event_, FALSE, INFINITE, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0 || wait == WAIT_FAILED) break;
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (create_destroy_hook_) { UnhookWinEvent(create_destroy_hook_); create_destroy_hook_ = nullptr; }
    if (show_hide_hook_) { UnhookWinEvent(show_hide_hook_); show_hide_hook_ = nullptr; }
}

bool GhostMonitor::IsNoise(const WindowInfo& window) const {
    if (!ignore_noise_) return false;
    if (IsNoiseClass(window.class_name)) return true;
    const long width = window.bounds.right - window.bounds.left;
    const long height = window.bounds.bottom - window.bounds.top;
    return width <= 1 && height <= 1;
}

ProcessInfo GhostMonitor::ProcessFor(DWORD pid) {
    const ULONGLONG now = GetTickCount64();
    {
        std::lock_guard lock(process_cache_mutex_);
        const auto found = process_cache_.find(pid);
        if (found != process_cache_.end()) {
            if (now - found->second.checked_tick < kProcessCacheMs) return found->second.info;
            // Re-validate cheaply: same creation time means it is still the same process.
            FILETIME start{};
            if (ProcessResolver::QueryStartTime(pid, start) &&
                FileTimeToU64(start) == FileTimeToU64(found->second.info.start_time)) {
                found->second.checked_tick = now;
                return found->second.info;
            }
        }
    }
    ProcessInfo info = resolver_.Resolve(pid);
    std::lock_guard lock(process_cache_mutex_);
    if (process_cache_.size() >= kMaxCachedProcesses) process_cache_.clear();
    process_cache_[pid] = {info, now};
    return info;
}

void GhostMonitor::EmitWindow(GhostKind kind, const WindowInfo& window, bool has_lifetime, std::chrono::milliseconds lifetime) {
    if (!window.pid) return;
    GhostEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.kind = kind;
    event.window = window;
    event.process = ProcessFor(window.pid);
    event.has_lifetime = has_lifetime;
    event.lifetime = lifetime;
    Emit(std::move(event));
}

void GhostMonitor::OnWindowEvent(DWORD event, HWND hwnd) {
    const auto now = std::chrono::system_clock::now();
    auto known = windows_.find(hwnd);

    if (known == windows_.end()) {
        if (event == EVENT_OBJECT_DESTROY) return;                 // never saw it; nothing useful to report
        if (GetAncestor(hwnd, GA_ROOT) != hwnd) return;            // child control, not a top-level window
        if (windows_.size() >= kMaxTrackedWindows) windows_.clear();
        WindowState state;
        state.info = SnapshotWindow(hwnd);
        known = windows_.emplace(hwnd, std::move(state)).first;
    }
    WindowState& state = known->second;

    switch (event) {
        case EVENT_OBJECT_CREATE:
            // A new window is normally invisible. Only a visible window is worth reporting, so creation is
            // recorded on its own only while Trace mode is active.
            state.info = SnapshotWindow(hwnd);
            if (TraceActive() && !IsNoise(state.info)) EmitWindow(GhostKind::WindowCreated, state.info, false, {});
            break;

        case EVENT_OBJECT_SHOW:
            if (state.visible) break;
            state.info = SnapshotWindow(hwnd);   // the title is usually set after creation
            state.visible = true;
            state.shown_at = now;
            if (!IsNoise(state.info)) EmitWindow(GhostKind::WindowShown, state.info, false, {});
            break;

        case EVENT_OBJECT_HIDE: {
            const bool was_visible = state.visible;
            state.visible = false;
            WindowInfo current = SnapshotWindow(hwnd);
            if (current.title.empty()) current.title = state.info.title;   // keep the last known title
            if (!current.pid) current.pid = state.info.pid;
            // A window that was already open when monitoring began has no recorded show time.
            const bool timed = was_visible && state.shown_at.time_since_epoch().count() != 0;
            if (!IsNoise(current)) {
                EmitWindow(GhostKind::WindowHidden, current, timed,
                           timed ? std::chrono::duration_cast<std::chrono::milliseconds>(now - state.shown_at)
                                 : std::chrono::milliseconds{});
            }
            state.info = current;
            break;
        }

        case EVENT_OBJECT_DESTROY: {
            // If the window was hidden first, its disappearance was already reported.
            if (state.visible && !IsNoise(state.info)) {
                const bool timed = state.shown_at.time_since_epoch().count() != 0;
                EmitWindow(GhostKind::WindowDestroyed, state.info, timed,
                           timed ? std::chrono::duration_cast<std::chrono::milliseconds>(now - state.shown_at)
                                 : std::chrono::milliseconds{});
            }
            windows_.erase(known);
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------------
// Process events

void GhostMonitor::EnsurePollThread() {
    std::lock_guard lock(thread_mutex_);
    if (stopping_ || poll_thread_.joinable()) return;
    polling_ = true;
    poll_thread_ = std::thread([this] { PollLoop(); });
}

void GhostMonitor::WmiLoop() {
    ComScope com(COINIT_MULTITHREADED);
    std::wstring failure;
    if (!com.Usable()) failure = HResultText(L"CoInitializeEx", com.Result());

    ComPtr<IWbemLocator> locator;
    ComPtr<IWbemServices> services;
    ComPtr<IEnumWbemClassObject> start_events;
    ComPtr<IEnumWbemClassObject> stop_events;

    if (failure.empty()) {
        HRESULT status = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&locator));
        if (SUCCEEDED(status)) {
            BSTR ns = SysAllocString(L"ROOT\\CIMV2");
            status = locator->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
            SysFreeString(ns);
        }
        if (SUCCEEDED(status)) {
            status = CoSetProxyBlanket(services.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                                       RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        }
        auto subscribe = [&](const wchar_t* query_text, ComPtr<IEnumWbemClassObject>& target) {
            if (FAILED(status)) return;
            BSTR language = SysAllocString(L"WQL");
            BSTR query = SysAllocString(query_text);
            status = services->ExecNotificationQuery(language, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                                     nullptr, &target);
            SysFreeString(language);
            SysFreeString(query);
        };
        subscribe(L"SELECT * FROM Win32_ProcessStartTrace", start_events);
        subscribe(L"SELECT * FROM Win32_ProcessStopTrace", stop_events);
        if (FAILED(status)) failure = HResultText(L"WMI process subscription", status);
    }

    wmi_ok_ = failure.empty() && start_events && stop_events;
    if (!wmi_ok_) {
        SetMessage(failure + L" " + str::kStatusWmiFallback);
        EnsurePollThread();
    }
    SetEvent(wmi_ready_);

    auto consume = [&](IEnumWbemClassObject* enumerator, GhostKind kind) {
        IWbemClassObject* objects[16]{};
        ULONG returned = 0;
        const HRESULT next = enumerator->Next(100, static_cast<ULONG>(std::size(objects)), objects, &returned);
        if (FAILED(next)) return false;
        for (ULONG i = 0; i < returned; ++i) {
            const DWORD pid = WmiDword(objects[i], L"ProcessID");
            const DWORD parent = WmiDword(objects[i], L"ParentProcessID");
            const std::wstring name = WmiString(objects[i], L"ProcessName");
            objects[i]->Release();
            if (!pid) continue;
            GhostEvent item;
            item.timestamp = std::chrono::system_clock::now();
            item.kind = kind;
            // The command line costs a WMI round trip per process, so it is only fetched in Trace mode.
            item.process = resolver_.Resolve(pid, parent, name, kind == GhostKind::ProcessStarted && TraceActive());
            if (item.process.name.empty()) item.process.name = name;
            if (!item.process.parent_pid) item.process.parent_pid = parent;
            Emit(std::move(item));
        }
        return true;
    };

    while (wmi_ok_ && !stopping_) {
        // Next() with a timeout returns WBEM_S_TIMEDOUT (a success code) when nothing arrived.
        if (!consume(start_events.Get(), GhostKind::ProcessStarted) || !consume(stop_events.Get(), GhostKind::ProcessExited)) {
            if (!stopping_) {
                wmi_ok_ = false;
                SetMessage(L"The WMI process subscription ended unexpectedly. " + std::wstring(str::kStatusWmiFallback));
                EnsurePollThread();
                if (notify_) notify_();
            }
            break;
        }
    }

    stop_events.Reset();
    start_events.Reset();
    services.Reset();
    locator.Reset();
}

void GhostMonitor::PollLoop() {
    std::unordered_map<DWORD, ProcessInfo> known;
    for (DWORD pid : SnapshotPids()) known.emplace(pid, PidOnly(pid));
    std::unordered_set<DWORD> observed_starts;

    while (!stopping_) {
        if (WaitForSingleObject(stop_event_, 250) != WAIT_TIMEOUT) break;
        std::vector<DWORD> pids = SnapshotPids();
        std::unordered_map<DWORD, ProcessInfo> current;
        current.reserve(pids.size());

        for (DWORD pid : pids) {
            if (auto previous = known.find(pid); previous != known.end()) {
                current.emplace(pid, previous->second);
                continue;
            }
            // Keep the loop fast: no command-line query here (that is a WMI call and can stall long enough
            // to miss the next short-lived process).
            ProcessInfo resolved = resolver_.Resolve(pid, 0, {}, false);
            if (resolved.name.empty() && resolved.exe_path.empty()) { current.emplace(pid, PidOnly(pid)); continue; }
            GhostEvent event;
            event.timestamp = std::chrono::system_clock::now();
            event.kind = GhostKind::ProcessStarted;
            event.process = resolved;
            Emit(std::move(event));
            current.emplace(pid, std::move(resolved));
            observed_starts.insert(pid);
        }

        for (const auto& [pid, process] : known) {
            if (current.contains(pid) || !observed_starts.erase(pid)) continue;
            GhostEvent event;
            event.timestamp = std::chrono::system_clock::now();
            event.kind = GhostKind::ProcessExited;
            event.process = process;
            Emit(std::move(event));
        }
        known = std::move(current);
    }
}

} // namespace peek
