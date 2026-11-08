#include "ui/process_page.h"

#include "core/format.h"
#include "core/strings.h"
#include "core/window_util.h"

namespace peek {

void ProcessPage::OnCreate() {
    title_ = MakeLabel(str::kProcessIntro);
    back_ = MakeButton(str::kBtnBack, kBack);
    refresh_ = MakeButton(str::kBtnRefresh, kRefresh);
    cmd_ = MakeButton(str::kBtnCommandLine, kCommandLine);
    copy_ = MakeButton(str::kBtnCopy, kCopy);
    location_ = MakeButton(str::kBtnOpenLocation, kLocation);
    list_ = MakeList(kList);
    AddColumn(list_, 0, str::kColRelation, 90);
    AddColumn(list_, 1, str::kColProcess, 200);
    AddColumn(list_, 2, str::kColPid, 70, LVCFMT_RIGHT);
    AddColumn(list_, 3, str::kColDetail, 330);
    detail_ = MakeText(kDetail);
    open_window_ = MakeButton(str::kBtnShowWindow, kOpenWindow);
    inspect_ = MakeButton(str::kBtnInspectWindow, kInspectWindow);
    events_ = MakeButton(str::kBtnShowInGhost, kShowEvents);
    Tip(back_, str::kTipBack); Tip(refresh_, str::kTipRefresh); Tip(cmd_, str::kTipCmdLine); Tip(copy_, str::kTipCopyDetails);
    Tip(location_, str::kTipOpenLocation); Tip(open_window_, str::kTipShowOwner); Tip(inspect_, str::kTipInspectWindow);
    Tip(events_, str::kTipShowEvents);
    for (HWND h : {back_, refresh_, cmd_, copy_, location_, open_window_, inspect_, events_}) EnableWindow(h, FALSE);
}

void ProcessPage::OnLayout(int width, int height) {
    const int m = Px(8), row = Px(24), gap = Px(6);
    MoveWindow(title_, m, m + Px(3), width - 2 * m, row, TRUE);
    int x = m;
    const int y = m + row + gap;
    for (HWND b : {back_, refresh_, cmd_, copy_, location_, events_}) {
        const int w = b == cmd_ ? Px(150) : (b == events_ ? Px(130) : Px(100));
        MoveWindow(b, x, y, w, row, TRUE);
        x += w + gap;
    }
    const int top = y + row + gap;
    const int list_h = std::max(Px(100), (height - top) * 38 / 100);
    MoveWindow(list_, m, top, width - 2 * m, list_h, TRUE);
    const int action_y = top + list_h + gap;
    MoveWindow(open_window_, m, action_y, Px(150), row, TRUE);
    MoveWindow(inspect_, m + Px(156), action_y, Px(170), row, TRUE);
    const int detail_top = action_y + row + gap;
    MoveWindow(detail_, m, detail_top, width - 2 * m, std::max(Px(40), height - m - detail_top), TRUE);
}

void ProcessPage::ShowProcess(const ProcessIdentity& identity) {
    if (have_process_ && details_.identity.pid != identity.pid) history_.push_back(details_.identity);
    else if (have_process_ && details_.identity.start != identity.start) history_.push_back(details_.identity);
    details_ = {};
    details_.identity = identity;
    have_process_ = true;
    Load(false);
}

void ProcessPage::Load(bool with_command_line) {
    const bool keep_command = with_command_line || !details_.info.command_line.empty();
    details_ = DescribeProcess(ctx_.resolver, ctx_.events, details_.identity, keep_command);
    SetText(detail_, FormatProcessDetails(details_));
    const std::wstring name = details_.info.name.empty() ? std::wstring(str::kUnknownProcess) : details_.info.name;
    SetText(title_, name + L"  —  PID " + std::to_wstring(details_.identity.pid) +
                    (details_.running ? L"  —  running" : L"  —  ended"));
    FillRelations();
    EnableWindow(back_, !history_.empty());
    EnableWindow(refresh_, TRUE);
    EnableWindow(cmd_, details_.running);
    EnableWindow(copy_, TRUE);
    EnableWindow(location_, !details_.info.exe_path.empty());
    EnableWindow(events_, !details_.events.empty());
    EnableWindow(inspect_, FALSE);
    EnableWindow(open_window_, FALSE);
}

void ProcessPage::FillRelations() {
    related_.clear();
    ListView_DeleteAllItems(list_);
    auto add = [&](const wchar_t* relation, const std::wstring& name, DWORD pid, const std::wstring& detail, Related related) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = ListView_GetItemCount(list_);
        item.pszText = const_cast<wchar_t*>(relation);
        const int index = ListView_InsertItem(list_, &item);
        ListView_SetItemText(list_, index, 1, const_cast<wchar_t*>(name.c_str()));
        const std::wstring pid_text = pid ? std::to_wstring(pid) : L"";
        ListView_SetItemText(list_, index, 2, const_cast<wchar_t*>(pid_text.c_str()));
        ListView_SetItemText(list_, index, 3, const_cast<wchar_t*>(detail.c_str()));
        related_.push_back(related);
    };
    if (details_.parent_known) {
        const ProcessInfo& p = details_.parent;
        Related r{Related::Kind::Parent, p.Identity(), nullptr};
        if (!details_.parent_running) r.identity = ProcessIdentity{p.pid, 0};
        add(L"Parent", p.name.empty() ? std::wstring(str::kUnknownProcess) : p.name, p.pid,
            details_.parent_running ? p.exe_path : std::wstring(L"not running, or its PID was reused"), r);
    }
    for (const ProcessInfo& child : details_.children) {
        add(L"Child", child.name, child.pid, child.exe_path, Related{Related::Kind::Child, child.Identity(), nullptr});
    }
    for (const WindowInfo& window : details_.windows) {
        add(L"Window", window.class_name, window.pid,
            HexHandle(window.hwnd) + L"  " + (window.title.empty() ? std::wstring(L"(no title)") : window.title),
            Related{Related::Kind::Window, details_.identity, window.hwnd});
    }
}

LRESULT ProcessPage::OnNotify(NMHDR* header) {
    if (header->hwndFrom != list_) return 0;
    if (header->code == LVN_ITEMCHANGED) {
        const int row = ListView_GetNextItem(list_, -1, LVNI_SELECTED);
        const bool window = row >= 0 && static_cast<std::size_t>(row) < related_.size() &&
                            related_[static_cast<std::size_t>(row)].kind == Related::Kind::Window &&
                            IsWindow(related_[static_cast<std::size_t>(row)].hwnd);
        EnableWindow(open_window_, window);
        EnableWindow(inspect_, window);
    } else if (header->code == NM_DBLCLK) {
        Activate(ListView_GetNextItem(list_, -1, LVNI_SELECTED));
    }
    return 0;
}

void ProcessPage::Activate(int row) {
    if (row < 0 || static_cast<std::size_t>(row) >= related_.size()) return;
    const Related& related = related_[static_cast<std::size_t>(row)];
    if (related.kind == Related::Kind::Window) BringWindowToFront(related.hwnd);
    else ShowProcess(related.identity);   // navigate to the parent or child
}

void ProcessPage::OnCommand(int id, int code, HWND) {
    if (code != BN_CLICKED) return;
    const int row = ListView_GetNextItem(list_, -1, LVNI_SELECTED);
    switch (id) {
        case kBack:
            if (!history_.empty()) {
                details_.identity = history_.back();
                history_.pop_back();
                Load(false);
            }
            break;
        case kRefresh: Load(false); break;
        case kCommandLine: Load(true); break;
        case kCopy: CopyTextToClipboard(hwnd_, FormatProcessDetails(details_)); break;
        case kLocation: RevealInExplorer(hwnd_, details_.info.exe_path); break;
        case kShowEvents:
            if (!details_.events.empty()) ctx_.navigator.OpenGhostEvent(details_.events.front().sequence);
            break;
        case kOpenWindow:
            if (row >= 0 && static_cast<std::size_t>(row) < related_.size()) BringWindowToFront(related_[static_cast<std::size_t>(row)].hwnd);
            break;
        case kInspectWindow:
            if (row >= 0 && static_cast<std::size_t>(row) < related_.size()) ctx_.navigator.OpenFocusForWindow(related_[static_cast<std::size_t>(row)].hwnd);
            break;
    }
}

} // namespace peek
