#include "ui/focus_page.h"

#include "core/format.h"
#include "core/strings.h"
#include "focus/focus_format.h"

namespace peek {

void FocusPage::OnCreate() {
    state_ = MakeLabel(L"");
    start_ = MakeButton(str::kBtnStartInspect, kStart);
    freeze_ = MakeButton(str::kBtnFreeze, kFreeze);
    pin_ = MakeButton(str::kBtnPin, kPin);
    copy_text_ = MakeButton(str::kBtnCopyText, kCopyText);
    copy_json_ = MakeButton(str::kBtnCopyJson, kCopyJson);
    copy_c_ = MakeButton(str::kBtnCopyC, kCopyC);
    process_ = MakeButton(str::kBtnProcessDetails, kProcess);
    location_ = MakeButton(str::kBtnOpenLocation, kLocation);
    hint_ = MakeLabel(str::kFocusIntro);
    list_ = MakeList(kList);
    AddColumn(list_, 0, str::kColProperty, 130);
    AddColumn(list_, 1, str::kColValue, 320);
    tree_ = MakeTree(kTree);
    UpdateButtons();
}

void FocusPage::OnLayout(int width, int height) {
    const int m = Px(8), row = Px(24), gap = Px(6);
    int x = m;
    for (auto [b, w] : {std::pair{start_, 130}, {freeze_, 110}, {pin_, 90}}) {
        MoveWindow(b, x, m, Px(w), row, TRUE);
        x += Px(w) + gap;
    }
    MoveWindow(state_, x + Px(6), m + Px(4), std::max(Px(60), width - m - x), row - Px(4), TRUE);
    x = m;
    const int y = m + row + gap;
    for (auto [b, w] : {std::pair{copy_text_, 110}, {copy_json_, 110}, {copy_c_, 110}, {process_, 130}, {location_, 120}}) {
        MoveWindow(b, x, y, Px(w), row, TRUE);
        x += Px(w) + gap;
    }
    const int top = y + row + gap;
    const int body_h = std::max(Px(60), height - top - m - row - gap);
    const int left_w = (width - 2 * m - gap) * 55 / 100;
    MoveWindow(list_, m, top, left_w, body_h, TRUE);
    MoveWindow(tree_, m + left_w + gap, top, width - 2 * m - left_w - gap, body_h, TRUE);
    MoveWindow(hint_, m, top + body_h + gap, width - 2 * m, row, TRUE);
}

void FocusPage::ModeChanged(InspectMode mode) {
    mode_ = mode;
    if ((mode == InspectMode::Frozen || mode == InspectMode::Pinned) && have_snapshot_) ctx_.focus.RequestTree();
    UpdateButtons();
}

void FocusPage::UpdateButtons() {
    const wchar_t* label = L"Not inspecting";
    switch (mode_) {
        case InspectMode::Live: label = L"LIVE — following the element under the pointer"; break;
        case InspectMode::Frozen: label = L"FROZEN — the selection will not change"; break;
        case InspectMode::Pinned: label = L"PINNED — tracking this element while you use other applications"; break;
        default: break;
    }
    SetText(state_, label);
    SetText(start_, mode_ == InspectMode::Off ? str::kBtnStartInspect : str::kBtnStopInspect);
    SetText(freeze_, mode_ == InspectMode::Frozen ? str::kBtnResume : str::kBtnFreeze);
    SetText(pin_, mode_ == InspectMode::Pinned ? str::kBtnUnpin : str::kBtnPin);
    EnableWindow(start_, ctx_.settings.Get().focus.enabled || mode_ != InspectMode::Off);
    EnableWindow(freeze_, mode_ == InspectMode::Live || mode_ == InspectMode::Frozen);
    EnableWindow(pin_, (mode_ == InspectMode::Live || mode_ == InspectMode::Frozen) ? have_snapshot_ : mode_ == InspectMode::Pinned);
    for (HWND h : {copy_text_, copy_json_, copy_c_}) EnableWindow(h, have_snapshot_);
    EnableWindow(process_, have_snapshot_ && snapshot_.window.pid != 0);
    EnableWindow(location_, have_snapshot_ && !snapshot_.process.exe_path.empty());
}

void FocusPage::AddRow(const wchar_t* property, const std::wstring& value) {
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount(list_);
    item.pszText = const_cast<wchar_t*>(property);
    const int index = ListView_InsertItem(list_, &item);
    ListView_SetItemText(list_, index, 1, const_cast<wchar_t*>(value.c_str()));
}

void FocusPage::SetSnapshot(const FocusSnapshot& s) {
    snapshot_ = s;
    have_snapshot_ = true;
    SendMessageW(list_, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(list_);
    auto orNone = [](const std::wstring& v) { return v.empty() ? std::wstring(str::kUnavailable) : v; };
    if (!s.element_alive) AddRow(L"Status", L"The pinned element no longer exists");
    AddRow(str::kPropProcess, orNone(s.process.name));
    AddRow(str::kPropPid, std::to_wstring(s.window.pid));
    AddRow(str::kPropExe, orNone(s.process.exe_path));
    AddRow(str::kPropHwnd, HexHandle(s.window.hwnd));
    AddRow(str::kPropClass, orNone(s.window.class_name));
    AddRow(str::kPropTitle, orNone(s.window.title));
    AddRow(str::kPropWindowRect, RectText(s.window.bounds));
    if (s.has_cursor) AddRow(str::kPropCursor, std::to_wstring(s.cursor.x) + L", " + std::to_wstring(s.cursor.y));
    const AutomationInfo& a = s.automation;
    if (!a.available) {
        AddRow(L"UI Automation", L"unavailable for this element (custom-drawn, protected or inaccessible)");
    } else {
        AddRow(str::kPropControlType, orNone(a.control_type));
        AddRow(str::kPropName, orNone(a.name));
        AddRow(str::kPropAutomationId, orNone(a.automation_id));
        AddRow(str::kPropUiaClass, orNone(a.class_name));
        AddRow(str::kPropFramework, orNone(a.framework));
        AddRow(str::kPropValue, a.value_hidden ? std::wstring(str::kValueHidden) : (a.value_available ? a.value : std::wstring(str::kUnavailable)));
        AddRow(str::kPropUiaBounds, a.bounds_valid ? RectText(a.bounds) : std::wstring(L"unavailable (highlight falls back to the window)"));
    }
    SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list_, nullptr, TRUE);
    UpdateButtons();
}

HTREEITEM FocusPage::InsertNode(const UiaNode& node, HTREEITEM parent) {
    std::wstring text = node.control_type.empty() ? std::wstring(L"element") : node.control_type;
    if (!node.name.empty()) text += L"  \"" + node.name + L"\"";
    if (!node.automation_id.empty()) text += L"  [" + node.automation_id + L"]";
    TVINSERTSTRUCTW insert{};
    insert.hParent = parent;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_STATE;
    insert.item.pszText = text.data();
    insert.item.lParam = node.id;
    insert.item.cChildren = node.has_children ? 1 : 0;
    insert.item.state = node.is_selected ? TVIS_BOLD : 0;
    insert.item.stateMask = TVIS_BOLD;
    HTREEITEM item = reinterpret_cast<HTREEITEM>(SendMessageW(tree_, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&insert)));
    items_[node.id] = item;
    return item;
}

void FocusPage::ApplyTree(const FocusTreeUpdate& update) {
    if (update.reset) {
        TreeView_DeleteAllItems(tree_);
        items_.clear();
        HTREEITEM selected = nullptr;
        for (const UiaNode& node : update.nodes) {
            const auto parent = items_.find(node.parent);
            const HTREEITEM item = InsertNode(node, parent == items_.end() ? TVI_ROOT : parent->second);
            if (node.is_selected) selected = item;
        }
        for (const UiaNode& node : update.nodes) {
            if (!node.is_selected && node.parent >= 0 && items_.count(node.parent)) {}
        }
        // Expand the path down to the inspected element.
        for (HTREEITEM p = selected ? TreeView_GetParent(tree_, selected) : nullptr; p; p = TreeView_GetParent(tree_, p)) {
            TreeView_Expand(tree_, p, TVE_EXPAND);
        }
        if (selected) TreeView_EnsureVisible(tree_, selected);
        return;
    }
    const auto parent = items_.find(update.parent_id);
    if (parent == items_.end()) return;
    for (const UiaNode& node : update.nodes) InsertNode(node, parent->second);
    TVITEMW item{};
    item.mask = TVIF_CHILDREN;
    item.hItem = parent->second;
    item.cChildren = update.nodes.empty() ? 0 : 1;
    TreeView_SetItem(tree_, &item);
    if (!update.nodes.empty()) TreeView_Expand(tree_, parent->second, TVE_EXPAND);
}

LRESULT FocusPage::OnNotify(NMHDR* header) {
    if (header->hwndFrom == tree_ && header->code == TVN_ITEMEXPANDINGW) {
        auto* view = reinterpret_cast<NMTREEVIEWW*>(header);
        if (view->action == TVE_EXPAND && TreeView_GetChild(tree_, view->itemNew.hItem) == nullptr) {
            ctx_.focus.ExpandNode(static_cast<int>(view->itemNew.lParam));   // children arrive via ApplyTree
            return TRUE;   // the expander opens once the children are loaded
        }
    }
    return 0;
}

void FocusPage::OnCommand(int id, int code, HWND) {
    if (code != BN_CLICKED) return;
    switch (id) {
        case kStart:
            ctx_.host.Execute(Command::ToggleFocus);
            break;
        case kFreeze:
            if (mode_ == InspectMode::Frozen) ctx_.focus.StartLive(); else ctx_.focus.Freeze();
            ModeChanged(ctx_.focus.Mode());
            break;
        case kPin:
            if (mode_ == InspectMode::Pinned) ctx_.focus.StartLive(); else ctx_.focus.Pin();
            ModeChanged(ctx_.focus.Mode());
            break;
        case kCopyText: CopyTextToClipboard(hwnd_, FocusAsText(snapshot_)); break;
        case kCopyJson: CopyTextToClipboard(hwnd_, FocusAsJson(snapshot_)); break;
        case kCopyC: CopyTextToClipboard(hwnd_, FocusAsC(snapshot_)); break;
        case kProcess: ctx_.navigator.OpenProcess(snapshot_.process.Identity()); break;
        case kLocation: RevealInExplorer(hwnd_, snapshot_.process.exe_path); break;
    }
}

} // namespace peek
