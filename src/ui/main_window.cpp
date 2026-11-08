#include "ui/main_window.h"

#include "core/strings.h"
#include "ui/gdi.h"

#include <dwmapi.h>
#include <windowsx.h>

namespace peek {
namespace {
constexpr wchar_t kClassName[] = L"Peek.MainWindow";
const wchar_t* const kTabNames[] = {str::kTabGhost, str::kTabProcess, str::kTabUnlock, str::kTabFocus, str::kTabSettings};
constexpr int kTabCount = 5;
}

MainWindow::MainWindow(EventStore& events, SettingsManager& settings, ProcessResolver& resolver, IconCache& icons,
                       UnlockService& unlock, FocusInspector& focus, CommandHost& host)
    : host_(host), context_{events, settings, resolver, icons, unlock, focus, host, *this} {
    ghost_page_ = std::make_unique<GhostPage>(context_);
    process_page_ = std::make_unique<ProcessPage>(context_);
    unlock_page_ = std::make_unique<UnlockPage>(context_);
    focus_page_ = std::make_unique<FocusPage>(context_);
    settings_page_ = std::make_unique<SettingsPage>(context_);
}

MainWindow::~MainWindow() { Destroy(); }

bool MainWindow::Create(HINSTANCE instance) {
    instance_ = instance;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    hwnd_ = CreateWindowExW(WS_EX_CONTROLPARENT, kClassName, str::kAppName, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700, nullptr, nullptr, instance, this);
    if (!hwnd_) return false;
    dpi_ = GetDpiForWindow(hwnd_);
    SetWindowPos(hwnd_, nullptr, 0, 0, MulDiv(980, dpi_, 96), MulDiv(700, dpi_, 96), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    for (Page* page : {static_cast<Page*>(ghost_page_.get()), static_cast<Page*>(process_page_.get()), static_cast<Page*>(unlock_page_.get()),
                       static_cast<Page*>(focus_page_.get()), static_cast<Page*>(settings_page_.get())}) {
        if (!page->Create(hwnd_, instance)) return false;
    }
    UpdateAppearance();
    Select(Tab::Ghost);
    return true;
}

void MainWindow::Destroy() {
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
    for (HFONT* f : {&font_, &font_bold_}) if (*f) { DeleteObject(*f); *f = nullptr; }
}

Page* MainWindow::PageFor(Tab tab) const {
    switch (tab) {
        case Tab::Ghost: return ghost_page_.get();
        case Tab::Process: return process_page_.get();
        case Tab::Unlock: return unlock_page_.get();
        case Tab::Focus: return focus_page_.get();
        case Tab::Settings: return settings_page_.get();
    }
    return nullptr;
}

int MainWindow::TabHeight() const { return MulDiv(34, dpi_, 96); }

RECT MainWindow::TabRect(int index) const {
    const int w = MulDiv(96, dpi_, 96);
    return RECT{MulDiv(8, dpi_, 96) + index * w, 0, MulDiv(8, dpi_, 96) + (index + 1) * w, TabHeight()};
}

void MainWindow::UpdateAppearance() {
    const Settings s = context_.settings.Get();
    theme_ = MakeTheme(s.general.theme);
    dpi_ = GetDpiForWindow(hwnd_);
    for (HFONT* f : {&font_, &font_bold_}) if (*f) { DeleteObject(*f); *f = nullptr; }
    font_ = gdi::MakeFont(dpi_, 9, FW_NORMAL);
    font_bold_ = gdi::MakeFont(dpi_, 9, FW_SEMIBOLD);
    const BOOL dark = theme_.dark;
    DwmSetWindowAttribute(hwnd_, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    for (int i = 0; i < kTabCount; ++i) PageFor(static_cast<Tab>(i))->SetAppearance(theme_, dpi_, font_);
    LayoutPages();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void MainWindow::ApplySettings() {
    if (!hwnd_) return;
    UpdateAppearance();
    for (int i = 0; i < kTabCount; ++i) PageFor(static_cast<Tab>(i))->SettingsChanged();
}

void MainWindow::EventsChanged() {
    if (hwnd_ && IsWindowVisible(hwnd_) && tab_ == Tab::Ghost) ghost_page_->Refresh();
}

void MainWindow::LayoutPages() {
    RECT client{};
    GetClientRect(hwnd_, &client);
    RECT area{0, TabHeight(), client.right, client.bottom};
    for (int i = 0; i < kTabCount; ++i) PageFor(static_cast<Tab>(i))->Place(area, static_cast<Tab>(i) == tab_);
}

void MainWindow::Select(Tab tab) {
    tab_ = tab;
    LayoutPages();
    PageFor(tab)->Refresh();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::Show(int tab) {
    if (!hwnd_) return;
    if (tab >= 0 && tab < kTabCount) Select(static_cast<Tab>(tab));
    ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(hwnd_);
}

void MainWindow::OpenProcess(const ProcessIdentity& identity) {
    process_page_->ShowProcess(identity);
    Show(static_cast<int>(Tab::Process));
}

void MainWindow::OpenGhostEvent(std::uint64_t sequence) {
    Show(static_cast<int>(Tab::Ghost));
    ghost_page_->SelectEvent(sequence);
}

void MainWindow::OpenFocusForWindow(HWND hwnd) {
    Show(static_cast<int>(Tab::Focus));
    context_.focus.InspectWindow(hwnd);
}

void MainWindow::OpenUnlock(const std::vector<std::wstring>& paths, bool inspect) {
    Show(static_cast<int>(Tab::Unlock));
    unlock_page_->ShowTargets(paths, inspect);
}

void MainWindow::Paint(HDC dc) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    RECT strip{0, 0, client.right, TabHeight()};
    gdi::Fill(dc, strip, theme_.surface);
    gdi::Line(dc, 0, TabHeight() - 1, client.right, TabHeight() - 1, theme_.border);
    for (int i = 0; i < kTabCount; ++i) {
        RECT r = TabRect(i);
        const bool selected = static_cast<int>(tab_) == i;
        if (!selected && hover_tab_ == i) gdi::Fill(dc, r, theme_.hover);
        SelectObject(dc, selected ? font_bold_ : font_);
        gdi::Text(dc, kTabNames[i], r, selected ? theme_.text : theme_.muted, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (selected) {
            RECT bar{r.left + MulDiv(10, dpi_, 96), r.bottom - MulDiv(3, dpi_, 96), r.right - MulDiv(10, dpi_, 96), r.bottom};
            gdi::Fill(dc, bar, theme_.accent);
        }
    }
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    MainWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        self = static_cast<MainWindow*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(message, wparam, lparam) : DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd_, &paint);
            if (font_) {
                RECT client{};
                GetClientRect(hwnd_, &client);
                HDC mem = CreateCompatibleDC(dc);
                HBITMAP bitmap = CreateCompatibleBitmap(dc, client.right, TabHeight());
                HGDIOBJ old = SelectObject(mem, bitmap);
                Paint(mem);
                BitBlt(dc, 0, 0, client.right, TabHeight(), mem, 0, 0, SRCCOPY);
                SelectObject(mem, old);
                DeleteObject(bitmap);
                DeleteDC(mem);
            }
            EndPaint(hwnd_, &paint);
            return 0;
        }
        case WM_SIZE:
            if (wparam != SIZE_MINIMIZED && font_) LayoutPages();
            return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            info->ptMinTrackSize = {MulDiv(900, dpi_, 96), MulDiv(620, dpi_, 96)};
            return 0;
        }
        case WM_DPICHANGED: {
            const RECT* suggested = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            UpdateAppearance();
            return 0;
        }
        case WM_SETTINGCHANGE:
            if (lparam && lstrcmpW(reinterpret_cast<const wchar_t*>(lparam), L"ImmersiveColorSet") == 0 &&
                context_.settings.Get().general.theme == ThemeChoice::System) UpdateAppearance();
            break;
        case WM_LBUTTONDOWN: {
            POINT p{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            for (int i = 0; i < kTabCount; ++i) {
                const RECT r = TabRect(i);
                if (PtInRect(&r, p)) { Select(static_cast<Tab>(i)); break; }
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            POINT p{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            int hover = -1;
            for (int i = 0; i < kTabCount; ++i) { const RECT r = TabRect(i); if (PtInRect(&r, p)) hover = i; }
            if (hover != hover_tab_) {
                hover_tab_ = hover;
                TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd_, 0};
                TrackMouseEvent(&track);
                RECT strip{0, 0, 4000, TabHeight()};
                InvalidateRect(hwnd_, &strip, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            hover_tab_ = -1;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wparam) == IDCANCEL || LOWORD(wparam) == IDOK) return 0;   // from IsDialogMessage (Esc/Enter)
            break;
        case WM_CLOSE:
            if (context_.settings.Get().general.close_button == CloseButton::ExitApp) host_.Execute(Command::Exit);
            else ShowWindow(hwnd_, SW_HIDE);
            return 0;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

} // namespace peek
