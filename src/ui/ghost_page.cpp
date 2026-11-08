#include "ui/ghost_page.h"

#include "core/format.h"
#include "core/strings.h"

#include <algorithm>
#include <unordered_map>

namespace peek {
namespace {
constexpr UINT_PTR kFilterTimer = 1;     // debounce for typing in the filter box
constexpr UINT_PTR kRunningTimer = 2;    // refresh "still running" state while the page is visible
}

void GhostPage::OnCreate() {
    filter_ = MakeEdit(kFilter);
    SendMessageW(filter_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(str::kFilterCue));
    Tip(filter_, str::kTipFilter);
    type_ = MakeCombo(kType);
    FillCombo(type_, {str::kAllEvents, str::kProcessStarted, str::kWindowAppeared, str::kWindowDisappeared, str::kProcessTerminated}, 0);
    state_ = MakeCombo(kState);
    FillCombo(state_, {str::kAnyState, str::kOnlyRunning, str::kOnlyEnded}, 0);
    time_ = MakeCombo(kTime);
    FillCombo(time_, {str::kAnyTime, str::kLastMinute, str::kLast10Minutes, str::kLastHour}, 0);
    group_ = MakeCheck(str::kGroupByLifecycle, kGroup);
    count_ = MakeLabel(L"");
    pause_ = MakeButton(str::kBtnPauseMonitoring, kPause);
    clear_ = MakeButton(str::kBtnClear, kClear);
    Tip(group_, str::kTipGroup); Tip(pause_, str::kTipPauseMon); Tip(clear_, str::kTipClearEvents);

    list_ = MakeList(kList, LVS_OWNERDATA);
    AddColumn(list_, 0, str::kColTime, 80);
    AddColumn(list_, 1, str::kColEvent, 150);
    AddColumn(list_, 2, str::kColProcess, 170);
    AddColumn(list_, 3, str::kColPid, 60, LVCFMT_RIGHT);
    AddColumn(list_, 4, str::kColState, 70);
    AddColumn(list_, 5, str::kColDetail, 420);

    detail_ = MakeText(kDetail);
    copy_ = MakeButton(str::kBtnCopy, kCopy);
    process_ = MakeButton(str::kBtnProcessDetails, kProcess);
    focus_ = MakeButton(str::kBtnInspectWindow, kFocus);
    location_ = MakeButton(str::kBtnOpenLocation, kLocation);
    dismiss_ = MakeButton(str::kBtnDismiss, kDismiss);
    Tip(copy_, str::kTipCopyDetails); Tip(process_, str::kTipProcessDetails); Tip(focus_, str::kTipInspectWindow);
    Tip(location_, str::kTipOpenLocation); Tip(dismiss_, str::kTipDismissEvent);
    SettingsChanged();
}

void GhostPage::OnLayout(int width, int height) {
    const int m = Px(8), row = Px(24), gap = Px(6);
    int x = width - m;
    // Combo boxes get their drop-down height; the checkbox keeps the row height.
    auto place_right = [&](HWND control, int w, int h) { x -= w; MoveWindow(control, x, m, w, h, TRUE); x -= gap; };
    place_right(group_, Px(190), row);
    place_right(time_, Px(120), Px(240));
    place_right(state_, Px(130), Px(240));
    place_right(type_, Px(160), Px(240));
    MoveWindow(filter_, m, m, std::max(Px(80), x + gap - m - gap), row, TRUE);

    const int bar_y = m + row + gap;
    MoveWindow(count_, m, bar_y + Px(4), Px(240), row - Px(4), TRUE);
    MoveWindow(clear_, width - m - Px(80), bar_y, Px(80), row, TRUE);
    MoveWindow(pause_, width - m - Px(80) - gap - Px(150), bar_y, Px(150), row, TRUE);

    const int list_top = bar_y + row + gap;
    const int detail_h = std::max(Px(150), (height - list_top) * 36 / 100);
    const int detail_top = height - m - detail_h;
    MoveWindow(list_, m, list_top, width - 2 * m, std::max(Px(40), detail_top - gap - list_top), TRUE);

    const int button_w = Px(150);
    MoveWindow(detail_, m, detail_top, width - 2 * m - button_w - gap, detail_h, TRUE);
    int by = detail_top;
    const int bx = width - m - button_w;
    for (HWND button : {copy_, process_, focus_, location_, dismiss_}) {
        MoveWindow(button, bx, by, button_w, row, TRUE);
        by += row + gap;
    }
}

void GhostPage::SettingsChanged() {
    const Settings settings = ctx_.settings.Get();
    loading_ = true;
    LoadFilterControls(settings);
    SetText(pause_, settings.ghost.enabled ? str::kBtnPauseMonitoring : str::kBtnResumeMonitoring);
    loading_ = false;
    needs_rebuild_ = true;
    Refresh();
}

void GhostPage::LoadFilterControls(const Settings& s) {
    if (GetText(filter_) != Utf8ToWide(s.ghost.filter_text)) SetText(filter_, Utf8ToWide(s.ghost.filter_text));
    int type = 0;
    switch (s.ghost.filter_categories) {
        case 1u << 0: type = 1; break;
        case 1u << 1: type = 2; break;
        case 1u << 2: type = 3; break;
        case 1u << 3: type = 4; break;
        default: type = 0; break;
    }
    SendMessageW(type_, CB_SETCURSEL, static_cast<WPARAM>(type), 0);
    SendMessageW(state_, CB_SETCURSEL, static_cast<WPARAM>(s.ghost.filter_state), 0);
    const int minutes = s.ghost.filter_minutes;
    SendMessageW(time_, CB_SETCURSEL, minutes <= 0 ? 0 : (minutes <= 1 ? 1 : (minutes <= 10 ? 2 : 3)), 0);
    SendMessageW(group_, BM_SETCHECK, s.ghost.group_lifecycle ? BST_CHECKED : BST_UNCHECKED, 0);
}

void GhostPage::ApplyFilterControls() {
    if (loading_) return;
    const std::string text = WideToUtf8(GetText(filter_));
    const int type = static_cast<int>(SendMessageW(type_, CB_GETCURSEL, 0, 0));
    const int state = static_cast<int>(SendMessageW(state_, CB_GETCURSEL, 0, 0));
    const int time = static_cast<int>(SendMessageW(time_, CB_GETCURSEL, 0, 0));
    const bool group = SendMessageW(group_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    ctx_.settings.Update([&](Settings& s) {
        s.ghost.filter_text = text;
        s.ghost.filter_categories = type <= 0 ? kAllCategories : (1u << (type - 1));
        s.ghost.filter_state = static_cast<StateFilter>(std::clamp(state, 0, 2));
        static const int kMinutes[] = {0, 1, 10, 60};
        s.ghost.filter_minutes = kMinutes[std::clamp(time, 0, 3)];
        s.ghost.group_lifecycle = group;
    });
    ctx_.host.Execute(Command::SettingsChanged);   // the HUD shares the filter
}

void GhostPage::Refresh() {
    if (!hwnd_) return;
    if (!needs_rebuild_ && built_version_ == ctx_.events.Version()) return;
    Rebuild();
}

bool GhostPage::IsRunning(const GhostEvent& event) {
    if (event.kind == GhostKind::ProcessExited) return false;
    auto found = running_.find(event.sequence);
    if (found != running_.end()) return found->second;
    const bool running = ProcessResolver::IsRunning(event.process.Identity());
    running_[event.sequence] = running;
    return running;
}

void GhostPage::Rebuild() {
    const Settings settings = ctx_.settings.Get();
    EventFilter filter;
    filter.text = Utf8ToWide(settings.ghost.filter_text);
    filter.categories = settings.ghost.filter_categories;
    filter.state = settings.ghost.filter_state;
    filter.max_age = std::chrono::seconds(settings.ghost.filter_minutes * 60);

    built_version_ = ctx_.events.Version();
    needs_rebuild_ = false;
    rows_ = ctx_.events.Query(filter, static_cast<std::size_t>(-1), [](const ProcessInfo& p) {
        return ProcessResolver::IsRunning(p.Identity());
    });
    running_.clear();
    child_row_.assign(rows_.size(), false);

    if (settings.ghost.group_lifecycle && !rows_.empty()) {
        // Newest lifecycle first; inside a lifecycle, oldest event first so it reads as a story.
        std::vector<std::uint64_t> order;
        std::unordered_map<std::uint64_t, std::vector<std::size_t>> members;
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            const std::uint64_t id = rows_[i].lifecycle_id;
            if (!members.count(id)) order.push_back(id);
            members[id].push_back(i);
        }
        std::vector<GhostEvent> grouped;
        grouped.reserve(rows_.size());
        std::vector<bool> child;
        for (std::uint64_t id : order) {
            auto& indexes = members[id];
            for (auto it = indexes.rbegin(); it != indexes.rend(); ++it) {
                child.push_back(it != indexes.rbegin());
                grouped.push_back(std::move(rows_[*it]));
            }
        }
        rows_ = std::move(grouped);
        child_row_ = std::move(child);
    }

    ListView_SetItemCountEx(list_, static_cast<int>(rows_.size()), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    InvalidateRect(list_, nullptr, FALSE);

    // Keep the selection on the same event if it is still listed.
    int selected_index = -1;
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        if (rows_[i].sequence == selected_sequence_) { selected_index = static_cast<int>(i); break; }
    }
    loading_ = true;
    ListView_SetItemState(list_, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    if (selected_index >= 0) ListView_SetItemState(list_, selected_index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    loading_ = false;
    if (selected_index < 0) selected_sequence_ = 0;

    const std::size_t total = ctx_.events.Size();
    std::wstring text = std::to_wstring(rows_.size());
    if (rows_.size() != total) text += L" of " + std::to_wstring(total);
    text += L" events";
    SetText(count_, text);
    UpdateDetails();
}

const GhostEvent* GhostPage::Selected() const {
    for (const GhostEvent& event : rows_) {
        if (event.sequence == selected_sequence_) return &event;
    }
    return nullptr;
}

void GhostPage::SelectEvent(std::uint64_t sequence) {
    // The event may be hidden by the current filter; reset the filter so the jump always lands on it.
    ctx_.settings.Update([](Settings& s) {
        s.ghost.filter_text.clear();
        s.ghost.filter_categories = kAllCategories;
        s.ghost.filter_state = StateFilter::Any;
        s.ghost.filter_minutes = 0;
    });
    selected_sequence_ = sequence;
    SettingsChanged();
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        if (rows_[i].sequence == sequence) {
            ListView_EnsureVisible(list_, static_cast<int>(i), FALSE);
            break;
        }
    }
    SetFocus(list_);
}

void GhostPage::UpdateDetails() {
    const GhostEvent* event = Selected();
    if (!event) {
        SetText(detail_, str::kNoSelection);
        UpdateButtons();
        return;
    }
    const bool running = IsRunning(*event);
    std::wstring text = FormatEventDetail(*event, running ? 1 : 0);
    const auto lifecycle = ctx_.events.Lifecycle(event->lifecycle_id);
    if (lifecycle.size() > 1) {
        text += L"\r\n\r\nProcess lifecycle (" + std::to_wstring(lifecycle.size()) + L" events, oldest first):\r\n";
        for (const GhostEvent& item : lifecycle) text += L"  " + FormatEventSummary(item) + L"\r\n";
    }
    SetText(detail_, text);
    UpdateButtons();
}

void GhostPage::UpdateButtons() {
    const GhostEvent* event = Selected();
    EnableWindow(copy_, event != nullptr);
    EnableWindow(process_, event != nullptr && event->process.pid != 0);
    EnableWindow(focus_, event != nullptr && event->window.hwnd != nullptr && IsWindow(event->window.hwnd));
    EnableWindow(location_, event != nullptr && !event->process.exe_path.empty());
    EnableWindow(dismiss_, event != nullptr);
}

void GhostPage::FillCell(NMLVDISPINFOW* info) {
    if (!(info->item.mask & LVIF_TEXT)) return;
    const std::size_t index = static_cast<std::size_t>(info->item.iItem);
    if (index >= rows_.size()) return;
    const GhostEvent& event = rows_[index];
    switch (info->item.iSubItem) {
        case 0: scratch_ = FormatTime(event.timestamp); break;
        case 1:
            scratch_ = std::wstring(index < child_row_.size() && child_row_[index] ? L"   └ " : L"") +
                       CategoryLabel(CategoryOf(event.kind));
            break;
        case 2: scratch_ = EventProcessName(event); break;
        case 3: scratch_ = event.process.pid ? std::to_wstring(event.process.pid) : L""; break;
        case 4:
            if (event.process.pid == 0) scratch_ = L"";
            else scratch_ = IsRunning(event) ? str::kRunning : str::kEnded;
            break;
        case 5: {
            const bool window_event = event.kind != GhostKind::ProcessStarted && event.kind != GhostKind::ProcessExited;
            scratch_ = window_event && !event.window.title.empty() ? event.window.title : event.process.exe_path;
            if (event.has_lifetime) scratch_ = L"ran " + FormatDuration(event.lifetime) + (scratch_.empty() ? L"" : L"  ·  " + scratch_);
            break;
        }
        default: scratch_.clear(); break;
    }
    info->item.pszText = scratch_.data();
}

LRESULT GhostPage::OnNotify(NMHDR* header) {
    if (header->hwndFrom != list_) return 0;
    switch (header->code) {
        case LVN_GETDISPINFOW: FillCell(reinterpret_cast<NMLVDISPINFOW*>(header)); break;
        case LVN_ITEMCHANGED: {
            auto* changed = reinterpret_cast<NMLISTVIEW*>(header);
            if (!loading_ && (changed->uChanged & LVIF_STATE) && (changed->uNewState & LVIS_SELECTED) &&
                static_cast<std::size_t>(changed->iItem) < rows_.size()) {
                selected_sequence_ = rows_[static_cast<std::size_t>(changed->iItem)].sequence;
                UpdateDetails();
            }
            break;
        }
        case NM_DBLCLK:
            if (const GhostEvent* event = Selected()) ctx_.navigator.OpenProcess(event->process.Identity());
            break;
        case NM_RCLICK: {
            POINT p{};
            GetCursorPos(&p);
            ShowContextMenu(p);
            break;
        }
    }
    return 0;
}

void GhostPage::ShowContextMenu(POINT screen) {
    // Select the row under the pointer first so the menu acts on what the user clicked.
    POINT client = screen;
    ScreenToClient(list_, &client);
    LVHITTESTINFO hit{};
    hit.pt = client;
    if (ListView_HitTest(list_, &hit) >= 0 && static_cast<std::size_t>(hit.iItem) < rows_.size()) {
        selected_sequence_ = rows_[static_cast<std::size_t>(hit.iItem)].sequence;
        ListView_SetItemState(list_, hit.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        UpdateDetails();
    }
    const GhostEvent* event = Selected();
    if (!event) return;
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (event->process.pid ? 0 : MF_GRAYED), kProcess, str::kBtnProcessDetails);
    AppendMenuW(menu, MF_STRING, kCopy, str::kMenuCopySummary);
    AppendMenuW(menu, MF_STRING | (event->process.exe_path.empty() ? MF_GRAYED : 0), kLocation, str::kMenuOpenLocation);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kDismiss, str::kMenuDismiss);
    const UINT command = static_cast<UINT>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, 0, hwnd_, nullptr));
    DestroyMenu(menu);
    if (command == kCopy) CopyTextToClipboard(hwnd_, FormatEventSummary(*event));
    else if (command != 0) OnCommand(static_cast<int>(command), BN_CLICKED, nullptr);
}

void GhostPage::OnCommand(int id, int code, HWND) {
    if (loading_) return;
    switch (id) {
        case kFilter:
            if (code == EN_CHANGE) SetTimer(hwnd_, kFilterTimer, 300, nullptr);
            break;
        case kType:
        case kState:
        case kTime:
            if (code == CBN_SELCHANGE) ApplyFilterControls();
            break;
        case kGroup:
            if (code == BN_CLICKED) ApplyFilterControls();
            break;
        case kClear: ctx_.host.Execute(Command::ClearEvents); break;
        case kPause: ctx_.host.Execute(Command::ToggleMonitoring); break;
        case kCopy:
            if (Selected()) CopyTextToClipboard(hwnd_, GetText(detail_));
            break;
        case kLocation:
            if (const GhostEvent* event = Selected()) RevealInExplorer(hwnd_, event->process.exe_path);
            break;
        case kDismiss:
            if (const GhostEvent* event = Selected()) {
                ctx_.events.Dismiss(event->sequence);
                selected_sequence_ = 0;
                ctx_.host.Execute(Command::EventsChanged);
            }
            break;
        case kProcess:
            if (const GhostEvent* event = Selected()) ctx_.navigator.OpenProcess(event->process.Identity());
            break;
        case kFocus:
            if (const GhostEvent* event = Selected()) ctx_.navigator.OpenFocusForWindow(event->window.hwnd);
            break;
    }
}

void GhostPage::OnMessage(UINT message, WPARAM wparam, LPARAM) {
    if (message == WM_TIMER) {
        if (wparam == kFilterTimer) {
            KillTimer(hwnd_, kFilterTimer);
            ApplyFilterControls();
        } else if (wparam == kRunningTimer) {
            running_.clear();
            InvalidateRect(list_, nullptr, FALSE);
        }
    } else if (message == WM_SHOWWINDOW) {
        if (wparam) { SetTimer(hwnd_, kRunningTimer, 2000, nullptr); Refresh(); }
        else KillTimer(hwnd_, kRunningTimer);
    }
}

} // namespace peek
