#include "ui/page.h"

#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>

namespace peek {
namespace {
constexpr wchar_t kPageClass[] = L"PeekII.Page";
}

bool Page::Create(HWND parent, HINSTANCE instance) {
    instance_ = instance;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PageProc;
    wc.hInstance = instance;
    wc.lpszClassName = kPageClass;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    hwnd_ = CreateWindowExW(WS_EX_CONTROLPARENT, kPageClass, L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | ExtraStyle(),
                            0, 0, 10, 10, parent, nullptr, instance, this);
    return hwnd_ != nullptr;
}

void Page::Place(const RECT& area, bool visible) {
    if (!hwnd_) return;
    SetWindowPos(hwnd_, nullptr, area.left, area.top, area.right - area.left, area.bottom - area.top,
                 SWP_NOZORDER | SWP_NOACTIVATE | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    if (visible) RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

// Moves every child with painting suspended, then repaints the whole page once. Moving controls one by one
// leaves stale pixels (old label text, a vacated checkbox) where a control used to be.
void Page::Relayout(int width, int height) {
    if (!hwnd_) return;
    if (redraw_depth_++ == 0) SendMessageW(hwnd_, WM_SETREDRAW, FALSE, 0);
    OnLayout(width, height);
    if (--redraw_depth_ == 0) {
        SendMessageW(hwnd_, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

void Page::SetAppearance(const Theme& theme, UINT dpi, HFONT font) {
    theme_ = theme;
    dpi_ = dpi;
    font_ = font;
    if (background_brush_) DeleteObject(background_brush_);
    if (surface_brush_) DeleteObject(surface_brush_);
    if (bold_font_) { DeleteObject(bold_font_); bold_font_ = nullptr; }
    LOGFONTW face{};
    if (font && GetObjectW(font, sizeof(face), &face)) {
        face.lfWeight = FW_SEMIBOLD;
        bold_font_ = CreateFontIndirectW(&face);
    }
    background_brush_ = CreateSolidBrush(theme_.background);
    surface_brush_ = CreateSolidBrush(theme_.surface);
    if (!hwnd_) return;
    RestyleChildren();
    OnAppearance();
    RECT client{};
    GetClientRect(hwnd_, &client);
    Relayout(client.right, client.bottom);
}

void Page::RestyleChildren() {
    EnumChildWindows(hwnd_, [](HWND child, LPARAM font) -> BOOL {
        SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(font_));
    for (auto& [control, kind] : styled_) StyleControl(control, kind);
    if (bold_font_) for (HWND heading : headings_) SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(bold_font_), TRUE);
}

HWND Page::Track(HWND control) {
    if (control) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), FALSE);
    }
    return control;
}

void Page::StyleControl(HWND control, const wchar_t* kind) {
    // Native controls follow the dark theme through the undocumented-but-stable "DarkMode_*" visual styles
    // (Windows 10 1903+). On older builds the call fails harmlessly and the control stays light.
    const wchar_t* style = theme_.dark ? kind : L"Explorer";
    if (!theme_.dark && lstrcmpW(kind, L"CFD") == 0) style = nullptr;
    if (theme_.dark) {
        std::wstring name = lstrcmpW(kind, L"CFD") == 0 ? L"DarkMode_CFD" : L"DarkMode_Explorer";
        SetWindowTheme(control, name.c_str(), nullptr);
    } else {
        SetWindowTheme(control, style, nullptr);
    }
    if (kind && lstrcmpW(kind, L"LIST") == 0) {
        ListView_SetBkColor(control, theme_.surface);
        ListView_SetTextBkColor(control, theme_.surface);
        ListView_SetTextColor(control, theme_.text);
    } else if (kind && lstrcmpW(kind, L"TREE") == 0) {
        TreeView_SetBkColor(control, theme_.surface);
        TreeView_SetTextColor(control, theme_.text);
    }
}

HWND Page::MakeLabel(const wchar_t* text, DWORD extra_style) {
    return Track(CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_NOPREFIX | extra_style, 0, 0, 10, 10,
                                 hwnd_, nullptr, instance_, nullptr));
}

HWND Page::MakeButton(const wchar_t* text, int id, DWORD extra_style) {
    HWND button = Track(CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | extra_style,
                                        0, 0, 10, 10, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr));
    styled_.emplace_back(button, L"Explorer");
    return button;
}

HWND Page::MakeCheck(const wchar_t* text, int id) {
    HWND check = Track(CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 10, 10,
                                       hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr));
    styled_.emplace_back(check, L"Explorer");
    return check;
}

HWND Page::MakeEdit(int id, DWORD extra_style) {
    HWND edit = Track(CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | extra_style, 0, 0, 10, 10,
                                      hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr));
    styled_.emplace_back(edit, L"CFD");
    return edit;
}

HWND Page::MakeCombo(int id) {
    HWND combo = Track(CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                       0, 0, 10, 200, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr));
    styled_.emplace_back(combo, L"CFD");
    return combo;
}

HWND Page::MakeList(int id, DWORD extra_style) {
    HWND list = Track(CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | extra_style,
                                      0, 0, 10, 10, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr));
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    styled_.emplace_back(list, L"LIST");
    return list;
}

HWND Page::MakeTree(int id) {
    HWND tree = Track(CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
                                          TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP,
                                      0, 0, 10, 10, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr));
    styled_.emplace_back(tree, L"TREE");
    return tree;
}

HWND Page::MakeText(int id) {
    HWND text = Track(CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                      0, 0, 10, 10, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr));
    styled_.emplace_back(text, L"CFD");
    return text;
}

HWND Page::MakeHeading(const wchar_t* text) {
    HWND heading = MakeLabel(text, SS_LEFT);
    headings_.push_back(heading);
    if (bold_font_) SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(bold_font_), FALSE);
    return heading;
}

HWND Page::MakeHint(const wchar_t* text) {
    HWND hint = MakeLabel(text, SS_LEFT);
    hints_.push_back(hint);
    return hint;
}

void Page::Tip(HWND control, const wchar_t* text) {
    if (!control || !text) return;
    if (!tooltip_) {
        tooltip_ = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT,
                                   CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd_, nullptr, instance_, nullptr);
        if (!tooltip_) return;
        SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, 360);
        SendMessageW(tooltip_, TTM_SETDELAYTIME, TTDT_INITIAL, 500);
        SendMessageW(tooltip_, TTM_SETDELAYTIME, TTDT_AUTOPOP, 20000);
    }
    TOOLINFOW tool{};
    tool.cbSize = sizeof(tool);
    tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    tool.hwnd = hwnd_;
    tool.uId = reinterpret_cast<UINT_PTR>(control);
    tool.lpszText = const_cast<wchar_t*>(text);
    SendMessageW(tooltip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
}

int Page::TextWidth(HWND control) const {
    HDC dc = GetDC(hwnd_);
    HGDIOBJ old = SelectObject(dc, font_ ? font_ : GetStockObject(DEFAULT_GUI_FONT));
    const std::wstring text = GetText(control);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(dc, old);
    ReleaseDC(hwnd_, dc);
    return size.cx;
}

int Page::WrappedHeight(HWND control, int width) const {
    HDC dc = GetDC(hwnd_);
    HGDIOBJ old = SelectObject(dc, font_ ? font_ : GetStockObject(DEFAULT_GUI_FONT));
    const std::wstring text = GetText(control);
    RECT box{0, 0, std::max(width, 20), 0};
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &box, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    ReleaseDC(hwnd_, dc);
    return box.bottom;
}

void Page::AddColumn(HWND list, int index, const wchar_t* title, int width_dips, int format) {
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
    column.pszText = const_cast<wchar_t*>(title);
    column.cx = Px(width_dips);
    column.fmt = format;
    column.iSubItem = index;
    ListView_InsertColumn(list, index, &column);
}

void Page::FillCombo(HWND combo, std::initializer_list<const wchar_t*> items, int selected) {
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (const wchar_t* item : items) SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
}

void Page::SetText(HWND control, const std::wstring& text) { SetWindowTextW(control, text.c_str()); }

std::wstring Page::GetText(HWND control) const {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(std::max(0, copied)));
    return text;
}

LRESULT CALLBACK Page::PageProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    Page* self = nullptr;
    if (message == WM_NCCREATE) {
        self = static_cast<Page*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<Page*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(hwnd, message, wparam, lparam);
    return self->Handle(message, wparam, lparam);
}

LRESULT Page::Handle(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            OnCreate();
            return 0;
        case WM_SIZE:
            Relayout(LOWORD(lparam), HIWORD(lparam));
            return 0;
        case WM_COMMAND:
            OnCommand(LOWORD(wparam), HIWORD(wparam), reinterpret_cast<HWND>(lparam));
            return 0;
        case WM_NOTIFY:
            return OnNotify(reinterpret_cast<NMHDR*>(lparam));
        case WM_ERASEBKGND: {
            RECT rect{};
            GetClientRect(hwnd_, &rect);
            FillRect(reinterpret_cast<HDC>(wparam), &rect, background_brush_ ? background_brush_ : reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
            return 1;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            if (message == WM_CTLCOLORSTATIC) {
                if (HBRUSH custom = OnControlColor(reinterpret_cast<HWND>(lparam))) return reinterpret_cast<LRESULT>(custom);
            }
            const bool hint = std::find(hints_.begin(), hints_.end(), reinterpret_cast<HWND>(lparam)) != hints_.end();
            SetTextColor(dc, hint ? theme_.muted : theme_.text);
            SetBkColor(dc, theme_.background);
            return reinterpret_cast<LRESULT>(background_brush_);
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            SetTextColor(dc, theme_.text);
            SetBkColor(dc, theme_.surface);
            return reinterpret_cast<LRESULT>(surface_brush_);
        }
        case WM_DESTROY:
            if (bold_font_) { DeleteObject(bold_font_); bold_font_ = nullptr; }
            if (background_brush_) { DeleteObject(background_brush_); background_brush_ = nullptr; }
            if (surface_brush_) { DeleteObject(surface_brush_); surface_brush_ = nullptr; }
            break;
        default:
            if (message >= WM_APP && message < 0xC000) {
                if (HandleAppMessage(message, wparam, lparam)) return 0;
            }
            OnMessage(message, wparam, lparam);
            break;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

} // namespace peek
