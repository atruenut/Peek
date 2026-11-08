#include "ui/hud_window.h"

#include "core/format.h"
#include "core/process_resolver.h"
#include "core/strings.h"
#include "ui/gdi.h"

#include <shellapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>

namespace peek {
namespace {

constexpr wchar_t kClassName[] = L"Peek.Hud";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 1500;
constexpr std::size_t kMaxRows = 200;   // events fetched for scrolling; the store itself may hold more

enum MenuId : UINT {
    kMenuShow = 1,
    kMenuCopy,
    kMenuCopyExe,
    kMenuLocation,
    kMenuDismissRow,
    kMenuExpanded,
    kMenuMonitor,
    kMenuOpenWindow,
    kMenuOpenSettings,
    kMenuClear,
    kMenuHideHud,
};

std::wstring RightText(const GhostEvent& event) {
    return std::wstring(ShortCategoryLabel(CategoryOf(event.kind))) + L"  " + FormatTime(event.timestamp);
}

} // namespace

HudWindow::HudWindow(EventStore& events, SettingsManager& settings, IconCache& icons, CommandHost& host)
    : events_(events), settings_manager_(settings), icons_(icons), host_(host) {}

HudWindow::~HudWindow() { Destroy(); }

bool HudWindow::Create(HINSTANCE instance) {
    instance_ = instance;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.hInstance = instance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    // Layered + topmost + no-activate + tool window: no focus theft, no taskbar button, no Alt+Tab entry.
    hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kClassName,
                            L"Peek HUD", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, instance, this);
    if (!hwnd_) return false;
    ApplySettings();
    return true;
}

void HudWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);   // WM_DESTROY still needs hwnd_ while it is being processed
        hwnd_ = nullptr;
    }
    for (HFONT* font : {&font_, &font_bold_, &font_small_}) {
        if (*font) { DeleteObject(*font); *font = nullptr; }
    }
}

// ---------------------------------------------------------------------------------------------------
// Settings, placement, visibility

void HudWindow::ApplySettings() {
    if (!hwnd_) return;
    settings_ = settings_manager_.Get();
    theme_ = MakeTheme(settings_.general.theme);
    SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>(settings_.hud.opacity * 255 / 100), LWA_ALPHA);
    scroll_ = 0;
    Rebuild();
    ApplyPlacement();
    ApplyVisibility();
}

void HudWindow::EventsChanged() {
    if (!hwnd_) return;
    Rebuild();
    if (IsWindowVisible(hwnd_)) InvalidateRect(hwnd_, nullptr, FALSE);
}

void HudWindow::SetStatus(const std::wstring& text, bool monitoring, bool degraded) {
    if (status_text_ == text && monitoring_ == monitoring && degraded_ == degraded) return;
    status_text_ = text;
    monitoring_ = monitoring;
    degraded_ = degraded;
    if (hwnd_ && IsWindowVisible(hwnd_)) InvalidateRect(hwnd_, nullptr, FALSE);
}

EventFilter HudWindow::CurrentFilter(const Settings& settings) const {
    EventFilter filter;
    filter.text = Utf8ToWide(settings.ghost.filter_text);
    filter.categories = settings.ghost.filter_categories;
    filter.state = settings.ghost.filter_state;
    filter.max_age = std::chrono::seconds(settings.ghost.filter_minutes * 60);
    return filter;
}

int HudWindow::VisibleRows() const {
    return settings_.hud.mode == HudMode::Compact ? settings_.hud.compact_rows : settings_.hud.expanded_rows;
}

void HudWindow::Rebuild() {
    const EventFilter filter = CurrentFilter(settings_);
    rows_ = events_.Query(filter, kMaxRows, [](const ProcessInfo& process) {
        return ProcessResolver::IsRunning(process.Identity());
    });
    ClampScroll();
    RefreshRunningState();
}

void HudWindow::ClampScroll() {
    const int max_scroll = std::max(0, static_cast<int>(rows_.size()) - VisibleRows());
    scroll_ = std::clamp(scroll_, 0, max_scroll);
}

void HudWindow::RefreshRunningState() {
    running_.clear();
    const int first = scroll_;
    const int last = std::min(static_cast<int>(rows_.size()), scroll_ + VisibleRows());
    for (int i = first; i < last; ++i) {
        const GhostEvent& event = rows_[static_cast<std::size_t>(i)];
        running_[event.sequence] = event.kind != GhostKind::ProcessExited &&
                                   ProcessResolver::IsRunning(event.process.Identity());
    }
}

HMONITOR HudWindow::PickMonitor() const {
    if (settings_.hud.monitor != "primary") {
        struct Search { std::wstring name; HMONITOR found{}; } search{Utf8ToWide(settings_.hud.monitor), nullptr};
        EnumDisplayMonitors(nullptr, nullptr,
            [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
                auto* s = reinterpret_cast<Search*>(data);
                MONITORINFOEXW info{};
                info.cbSize = sizeof(info);
                if (GetMonitorInfoW(monitor, &info) && s->name == info.szDevice) {
                    s->found = monitor;
                    return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&search));
        if (search.found) return search.found;   // otherwise the chosen display is gone: fall back to primary
    }
    return MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
}

void HudWindow::UpdateFonts() {
    for (HFONT* font : {&font_, &font_bold_, &font_small_}) {
        if (*font) { DeleteObject(*font); *font = nullptr; }
    }
    font_ = gdi::MakeFont(dpi_, 9, FW_NORMAL);
    font_bold_ = gdi::MakeFont(dpi_, 9, FW_SEMIBOLD);
    font_small_ = gdi::MakeFont(dpi_, 8, FW_NORMAL);
}

int HudWindow::Px(int dips) const { return gdi::Scale(dips, dpi_); }
int HudWindow::HeaderHeight() const { return Px(26); }
int HudWindow::FooterHeight() const { return Px(20); }
int HudWindow::RowHeight() const { return Px(settings_.hud.mode == HudMode::Compact ? 22 : 38); }

void HudWindow::ApplyPlacement() {
    if (!hwnd_) return;
    HMONITOR monitor = PickMonitor();
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    UINT dpi_x = 96, dpi_y = 96;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y)) && dpi_x != 0) {
        if (dpi_x != dpi_ || !font_) { dpi_ = dpi_x; UpdateFonts(); }
    } else if (!font_) {
        UpdateFonts();
    }

    const int width = Px(settings_.hud.mode == HudMode::Compact ? 300 : 420);
    const int height = HeaderHeight() + VisibleRows() * RowHeight() + FooterHeight() + 2;
    const RECT& work = info.rcWork;
    const int margin_x = Px(settings_.hud.offset_x);
    const int margin_y = Px(settings_.hud.offset_y);
    const bool left = settings_.hud.anchor == HudAnchor::TopLeft || settings_.hud.anchor == HudAnchor::BottomLeft;
    const bool top = settings_.hud.anchor == HudAnchor::TopLeft || settings_.hud.anchor == HudAnchor::TopRight;
    int x = left ? work.left + margin_x : work.right - margin_x - width;
    int y = top ? work.top + margin_y : work.bottom - margin_y - height;
    x = std::clamp<int>(x, work.left, std::max<int>(work.left, work.right - width));
    y = std::clamp<int>(y, work.top, std::max<int>(work.top, work.bottom - height));

    placing_ = true;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    placing_ = false;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void HudWindow::ApplyVisibility() {
    if (!hwnd_) return;
    if (!settings_.hud.visible) {
        KillTimer(hwnd_, kTimerId);
        fullscreen_suppressed_ = false;
        ShowWindow(hwnd_, SW_HIDE);
        return;
    }
    fullscreen_suppressed_ = settings_.hud.fullscreen == FullscreenBehavior::Hide && FullscreenAppActive();
    if (fullscreen_suppressed_) {
        ShowWindow(hwnd_, SW_HIDE);
    } else {
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
    SetTimer(hwnd_, kTimerId, kTimerMs, nullptr);   // fullscreen watch + running-state refresh while the HUD is enabled
}

bool HudWindow::FullscreenAppActive() const {
    QUERY_USER_NOTIFICATION_STATE state{};
    if (SUCCEEDED(SHQueryUserNotificationState(&state)) &&
        (state == QUNS_BUSY || state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE)) {
        return true;
    }
    HWND foreground = GetForegroundWindow();
    if (!foreground || foreground == hwnd_ || !IsWindowVisible(foreground)) return false;
    wchar_t class_name[64]{};
    GetClassNameW(foreground, class_name, static_cast<int>(std::size(class_name)));
    const std::wstring name = class_name;
    if (name == L"Progman" || name == L"WorkerW" || name == L"Shell_TrayWnd" || name == L"Shell_SecondaryTrayWnd") return false;
    RECT rect{};
    if (!GetWindowRect(foreground, &rect)) return false;
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST), &info)) return false;
    const bool covers = rect.left <= info.rcMonitor.left && rect.top <= info.rcMonitor.top &&
                        rect.right >= info.rcMonitor.right && rect.bottom >= info.rcMonitor.bottom;
    // A maximized window with a title bar is not "fullscreen"; borderless covering windows are.
    const bool captioned = (GetWindowLongPtrW(foreground, GWL_STYLE) & WS_CAPTION) == WS_CAPTION;
    return covers && !captioned;
}

void HudWindow::SaveDraggedPosition() {
    if (!hwnd_ || placing_) return;
    RECT rect{};
    GetWindowRect(hwnd_, &rect);
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return;
    UINT dpi_x = 96, dpi_y = 96;
    GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
    if (dpi_x == 0) dpi_x = 96;

    const POINT center{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
    const POINT mid{(info.rcWork.left + info.rcWork.right) / 2, (info.rcWork.top + info.rcWork.bottom) / 2};
    const bool left = center.x < mid.x;
    const bool top = center.y < mid.y;
    const int off_x = left ? rect.left - info.rcWork.left : info.rcWork.right - rect.right;
    const int off_y = top ? rect.top - info.rcWork.top : info.rcWork.bottom - rect.bottom;
    const std::string monitor_name = (info.dwFlags & MONITORINFOF_PRIMARY) ? "primary" : WideToUtf8(info.szDevice);

    settings_manager_.Update([&](Settings& s) {
        s.hud.monitor = monitor_name;
        s.hud.anchor = top ? (left ? HudAnchor::TopLeft : HudAnchor::TopRight)
                           : (left ? HudAnchor::BottomLeft : HudAnchor::BottomRight);
        s.hud.offset_x = std::max(0, MulDiv(off_x, 96, static_cast<int>(dpi_x)));
        s.hud.offset_y = std::max(0, MulDiv(off_y, 96, static_cast<int>(dpi_x)));
    });
    host_.Execute(Command::SettingsChanged);   // snaps the HUD to its anchor and refreshes the Settings page
}

// ---------------------------------------------------------------------------------------------------
// Layout and hit testing

RECT HudWindow::HeaderRect() const {
    RECT client{};
    GetClientRect(hwnd_, &client);
    return RECT{0, 0, client.right, HeaderHeight()};
}

RECT HudWindow::ButtonRect(Zone button) const {
    const RECT header = HeaderRect();
    const int size = Px(22);
    const int top = (header.bottom - size) / 2;
    int slot = 0;
    switch (button) {
        case Zone::HideButton: slot = 1; break;
        case Zone::PauseButton: slot = 2; break;
        case Zone::ModeButton: slot = 3; break;
        default: return RECT{};
    }
    const int right = header.right - Px(4) - (slot - 1) * (size + Px(2));
    return RECT{right - size, top, right, top + size};
}

RECT HudWindow::RowRect(int row) const {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int top = HeaderHeight() + row * RowHeight();
    return RECT{1, top, client.right - 1, top + RowHeight()};
}

HudWindow::Hit HudWindow::HitTest(POINT p) const {
    if (!hwnd_) return {};
    RECT client{};
    GetClientRect(hwnd_, &client);
    if (p.y < HeaderHeight()) {
        for (Zone button : {Zone::ModeButton, Zone::PauseButton, Zone::HideButton}) {
            const RECT r = ButtonRect(button);
            if (PtInRect(&r, p)) return {button, -1};
        }
        return {Zone::Header, -1};
    }
    const int body_bottom = HeaderHeight() + VisibleRows() * RowHeight();
    if (p.y < body_bottom) {
        const int row = (p.y - HeaderHeight()) / RowHeight();
        const int index = scroll_ + row;
        if (index < static_cast<int>(rows_.size())) return {Zone::Row, row};
        return {Zone::None, -1};
    }
    return {Zone::Footer, -1};
}

// ---------------------------------------------------------------------------------------------------
// Painting

void HudWindow::Paint(HDC target) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    HGDIOBJ old_bitmap = SelectObject(dc, bitmap);

    gdi::Fill(dc, client, theme_.background);
    SelectObject(dc, font_);
    PaintRows(dc);
    PaintHeader(dc, client);
    PaintFooter(dc, client);
    gdi::Frame(dc, client, theme_.border);

    BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

void HudWindow::PaintHeader(HDC dc, const RECT& client) {
    RECT header{1, 1, client.right - 1, HeaderHeight()};
    gdi::Fill(dc, header, theme_.surface);
    gdi::Line(dc, 1, HeaderHeight() - 1, client.right - 1, HeaderHeight() - 1, theme_.border);

    const COLORREF dot = !monitoring_ ? theme_.muted : (degraded_ ? theme_.warning : theme_.ok);
    const int d = Px(8);
    const int dot_y = (HeaderHeight() - d) / 2;
    SetDCBrushColor(dc, dot);
    SelectObject(dc, GetStockObject(DC_BRUSH));
    SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, Px(9), dot_y, Px(9) + d, dot_y + d);

    SelectObject(dc, font_bold_);
    RECT title{Px(24), 0, ButtonRect(Zone::ModeButton).left - Px(4), HeaderHeight()};
    gdi::Text(dc, str::kAppName, title, theme_.text);

    for (Zone button : {Zone::ModeButton, Zone::PauseButton, Zone::HideButton}) {
        const RECT r = ButtonRect(button);
        if (hover_.zone == button) gdi::Fill(dc, r, theme_.hover);
        const int cx = (r.left + r.right) / 2;
        const int cy = (r.top + r.bottom) / 2;
        const int a = Px(4);
        const int w = std::max(1, Px(1));
        switch (button) {
            case Zone::ModeButton: {
                const bool expanded = settings_.hud.mode == HudMode::Expanded;
                const int dy = expanded ? a / 2 : -a / 2;
                gdi::Line(dc, cx - a, cy + dy, cx, cy - dy, theme_.muted, w);
                gdi::Line(dc, cx, cy - dy, cx + a, cy + dy, theme_.muted, w);
                break;
            }
            case Zone::PauseButton:
                if (monitoring_) {
                    RECT left{cx - a, cy - a, cx - a + std::max(2, Px(3)), cy + a + 1};
                    RECT right{cx + a - std::max(2, Px(3)) + 1, cy - a, cx + a + 1, cy + a + 1};
                    gdi::Fill(dc, left, theme_.muted);
                    gdi::Fill(dc, right, theme_.muted);
                } else {
                    POINT tri[3] = {{cx - a + 1, cy - a}, {cx - a + 1, cy + a}, {cx + a, cy}};
                    SetDCBrushColor(dc, theme_.muted);
                    SelectObject(dc, GetStockObject(DC_BRUSH));
                    Polygon(dc, tri, 3);
                }
                break;
            case Zone::HideButton:
                gdi::Line(dc, cx - a, cy - a, cx + a + 1, cy + a + 1, theme_.muted, w);
                gdi::Line(dc, cx - a, cy + a, cx + a + 1, cy - a - 1, theme_.muted, w);
                break;
            default: break;
        }
    }
}

void HudWindow::PaintRows(HDC dc) {
    const bool expanded = settings_.hud.mode == HudMode::Expanded;
    if (rows_.empty()) {
        RECT area{Px(10), HeaderHeight(), 0, 0};
        RECT client{};
        GetClientRect(hwnd_, &client);
        area.right = client.right - Px(10);
        area.bottom = HeaderHeight() + RowHeight() * 2;
        SelectObject(dc, font_);
        const wchar_t* text = !monitoring_ ? str::kHudPaused
                              : (CurrentFilter(settings_).IsEmpty() ? str::kHudWaiting : str::kHudNoMatch);
        gdi::Text(dc, text, area, theme_.muted, DT_LEFT | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
        return;
    }

    const int visible = VisibleRows();
    for (int row = 0; row < visible; ++row) {
        const int index = scroll_ + row;
        if (index >= static_cast<int>(rows_.size())) break;
        const GhostEvent& event = rows_[static_cast<std::size_t>(index)];
        const RECT r = RowRect(row);
        const bool selected = event.sequence == selected_;
        if (selected) gdi::Fill(dc, r, theme_.selected);
        else if (hover_.zone == Zone::Row && hover_.row == row) gdi::Fill(dc, r, theme_.hover);

        const EventCategory category = CategoryOf(event.kind);
        RECT bar{r.left, r.top + Px(2), r.left + Px(3), r.bottom - Px(2)};
        gdi::Fill(dc, bar, theme_.EventColor(category));

        const int icon_size = Px(16);
        const int icon_x = r.left + Px(9);
        const int line_h = expanded ? RowHeight() / 2 : RowHeight();
        const int icon_y = r.top + (line_h - icon_size) / 2 + (expanded ? Px(1) : 0);
        if (!event.process.exe_path.empty()) {
            if (HICON icon = icons_.Get(event.process.exe_path)) {
                DrawIconEx(dc, icon_x, icon_y, icon, icon_size, icon_size, 0, nullptr, DI_NORMAL);
            }
        }

        const int text_x = icon_x + icon_size + Px(6);
        const std::wstring right = RightText(event);
        SelectObject(dc, font_small_);
        const int right_w = gdi::TextWidth(dc, right);
        RECT right_rect{r.right - Px(8) - right_w, r.top, r.right - Px(8), r.top + line_h};
        gdi::Text(dc, right, right_rect, theme_.muted, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(dc, font_);
        std::wstring name = EventProcessName(event);
        if (expanded && event.process.pid) name += L"  (" + std::to_wstring(event.process.pid) + L")";
        RECT name_rect{text_x, r.top, right_rect.left - Px(6), r.top + line_h};
        gdi::Text(dc, name, name_rect, theme_.text);

        if (expanded) {
            // Second line: state dot, event type and the most useful identifying detail.
            const auto it = running_.find(event.sequence);
            const bool is_running = it != running_.end() && it->second;
            const int d = Px(6);
            const int dot_y = r.top + line_h + (line_h - d) / 2;
            SetDCBrushColor(dc, is_running ? theme_.ok : theme_.muted);
            SelectObject(dc, GetStockObject(DC_BRUSH));
            SelectObject(dc, GetStockObject(NULL_PEN));
            Ellipse(dc, icon_x + Px(3), dot_y, icon_x + Px(3) + d, dot_y + d);

            std::wstring detail = CategoryLabel(category);
            if (event.has_lifetime) detail += L" · ran " + FormatDuration(event.lifetime);
            const bool window_event = event.kind != GhostKind::ProcessStarted && event.kind != GhostKind::ProcessExited;
            const std::wstring& extra = window_event && !event.window.title.empty() ? event.window.title : event.process.exe_path;
            if (!extra.empty()) detail += L" · " + extra;
            SelectObject(dc, font_small_);
            RECT detail_rect{text_x, r.top + line_h, r.right - Px(8), r.bottom};
            gdi::Text(dc, detail, detail_rect, theme_.muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_NOPREFIX);
        }
    }

    // A thin scroll indicator when more matching events exist than rows are shown.
    if (static_cast<int>(rows_.size()) > visible) {
        RECT client{};
        GetClientRect(hwnd_, &client);
        const int track_top = HeaderHeight();
        const int track_h = visible * RowHeight();
        const int thumb_h = std::max(Px(12), track_h * visible / static_cast<int>(rows_.size()));
        const int max_scroll = static_cast<int>(rows_.size()) - visible;
        const int thumb_y = track_top + (track_h - thumb_h) * scroll_ / std::max(1, max_scroll);
        RECT thumb{client.right - Px(3), thumb_y, client.right - 1, thumb_y + thumb_h};
        gdi::Fill(dc, thumb, theme_.border);
    }
}

void HudWindow::PaintFooter(HDC dc, const RECT& client) {
    RECT footer{1, client.bottom - FooterHeight() - 1, client.right - 1, client.bottom - 1};
    gdi::Fill(dc, footer, theme_.surface);
    gdi::Line(dc, 1, footer.top, client.right - 1, footer.top, theme_.border);
    SelectObject(dc, font_small_);
    std::wstring text = !hint_.empty() ? hint_ : status_text_;
    if (hint_.empty() && !CurrentFilter(settings_).IsEmpty()) text += L" · filtered";
    RECT inner{footer.left + Px(8), footer.top, footer.right - Px(8), footer.bottom};
    gdi::Text(dc, text, inner, theme_.muted);
}

// ---------------------------------------------------------------------------------------------------
// Input

void HudWindow::OnClick(POINT p) {
    const Hit hit = HitTest(p);
    switch (hit.zone) {
        case Zone::ModeButton: host_.Execute(Command::ToggleHudMode); break;
        case Zone::PauseButton: host_.Execute(Command::ToggleMonitoring); break;
        case Zone::HideButton: host_.Execute(Command::ToggleHud); break;
        case Zone::Row: {
            const std::size_t index = static_cast<std::size_t>(scroll_ + hit.row);
            if (index < rows_.size()) {
                selected_ = rows_[index].sequence;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            break;
        }
        default: break;
    }
}

void HudWindow::OnDoubleClick(POINT p) {
    const Hit hit = HitTest(p);
    if (hit.zone == Zone::Row) {
        const std::size_t index = static_cast<std::size_t>(scroll_ + hit.row);
        if (index < rows_.size()) host_.Execute(Command::ShowEvent, rows_[index].sequence);
    }
}

void HudWindow::ShowRowMenu(int row, POINT screen) {
    const std::size_t index = static_cast<std::size_t>(scroll_ + row);
    if (index >= rows_.size()) return;
    const GhostEvent event = rows_[index];
    selected_ = event.sequence;
    InvalidateRect(hwnd_, nullptr, FALSE);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kMenuShow, str::kMenuShowEvent);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuCopy, str::kMenuCopySummary);
    AppendMenuW(menu, MF_STRING | (event.process.exe_path.empty() ? MF_GRAYED : 0), kMenuCopyExe, str::kMenuCopyPath);
    AppendMenuW(menu, MF_STRING | (event.process.exe_path.empty() ? MF_GRAYED : 0), kMenuLocation, str::kMenuOpenLocation);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuDismissRow, str::kMenuDismiss);

    // A menu needs foreground activation to dismiss correctly. This only happens on an explicit right-click.
    SetForegroundWindow(hwnd_);
    const UINT command = static_cast<UINT>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, 0, hwnd_, nullptr));
    PostMessageW(hwnd_, WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (command) {
        case kMenuShow: host_.Execute(Command::ShowEvent, event.sequence); break;
        case kMenuCopy: CopyTextToClipboard(hwnd_, FormatEventSummary(event)); break;
        case kMenuCopyExe: CopyTextToClipboard(hwnd_, event.process.exe_path); break;
        case kMenuLocation: RevealInExplorer(hwnd_, event.process.exe_path); break;
        case kMenuDismissRow:
            events_.Dismiss(event.sequence);
            host_.Execute(Command::EventsChanged);
            break;
        default: break;
    }
}

void HudWindow::ShowHeaderMenu(POINT screen) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (settings_.hud.mode == HudMode::Expanded ? MF_CHECKED : 0), kMenuExpanded, str::kMenuHudExpanded);
    AppendMenuW(menu, MF_STRING, kMenuMonitor, monitoring_ ? str::kBtnPauseMonitoring : str::kBtnResumeMonitoring);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuOpenWindow, str::kMenuOpenInvestigation);
    AppendMenuW(menu, MF_STRING, kMenuOpenSettings, str::kMenuSettings);
    AppendMenuW(menu, MF_STRING, kMenuClear, str::kMenuClearEvents);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuHideHud, str::kHudHide);

    SetForegroundWindow(hwnd_);
    const UINT command = static_cast<UINT>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, 0, hwnd_, nullptr));
    PostMessageW(hwnd_, WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (command) {
        case kMenuExpanded: host_.Execute(Command::ToggleHudMode); break;
        case kMenuMonitor: host_.Execute(Command::ToggleMonitoring); break;
        case kMenuOpenWindow: host_.Execute(Command::ShowInvestigation, static_cast<std::uint64_t>(-1)); break;
        case kMenuOpenSettings: host_.Execute(Command::ShowSettings); break;
        case kMenuClear: host_.Execute(Command::ClearEvents); break;
        case kMenuHideHud: host_.Execute(Command::ToggleHud); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------------
// Window procedure

LRESULT CALLBACK HudWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    HudWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        self = static_cast<HudWindow*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<HudWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(message, wparam, lparam);
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT HudWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_ERASEBKGND: return 1;

        case WM_NCHITTEST: {
            POINT p{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd_, &p);
            const Hit hit = HitTest(p);
            switch (hit.zone) {
                case Zone::Header: return HTCAPTION;                 // drag handle
                case Zone::Row: return settings_.hud.click_through ? HTTRANSPARENT : HTCLIENT;
                case Zone::None: return settings_.hud.click_through ? HTTRANSPARENT : HTCLIENT;
                default: return HTCLIENT;                            // buttons and footer
            }
        }

        case WM_NCLBUTTONDBLCLK:
            if (wparam == HTCAPTION) { host_.Execute(Command::ToggleHudMode); return 0; }
            break;

        case WM_EXITSIZEMOVE: SaveDraggedPosition(); return 0;

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd_, &paint);
            if (font_) Paint(dc);
            EndPaint(hwnd_, &paint);
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT p{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            const Hit hit = HitTest(p);
            if (!tracking_mouse_) {
                TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd_, 0};
                tracking_mouse_ = TrackMouseEvent(&track) != FALSE;
            }
            if (hit.zone != hover_.zone || hit.row != hover_.row) {
                hover_ = hit;
                switch (hit.zone) {
                    case Zone::ModeButton: hint_ = str::kHudToggleMode; break;
                    case Zone::PauseButton: hint_ = str::kHudPause; break;
                    case Zone::HideButton: hint_ = str::kHudHide; break;
                    default: hint_.clear(); break;
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            hover_ = {};
            hint_.clear();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;

        case WM_LBUTTONUP: OnClick(POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}); return 0;
        case WM_LBUTTONDBLCLK: OnDoubleClick(POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}); return 0;

        case WM_MOUSEWHEEL: {
            const int steps = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
            if (steps != 0) {
                const int before = scroll_;
                scroll_ -= steps * 2;
                ClampScroll();
                if (scroll_ != before) {
                    RefreshRunningState();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
            }
            return 0;
        }

        case WM_CONTEXTMENU: {
            POINT screen{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (screen.x == -1 && screen.y == -1) GetCursorPos(&screen);
            POINT client = screen;
            ScreenToClient(hwnd_, &client);
            const Hit hit = HitTest(client);
            if (hit.zone == Zone::Row) ShowRowMenu(hit.row, screen);
            else ShowHeaderMenu(screen);
            return 0;
        }

        case WM_TIMER:
            if (wparam == kTimerId) {
                const bool suppress = settings_.hud.visible && settings_.hud.fullscreen == FullscreenBehavior::Hide &&
                                      FullscreenAppActive();
                if (suppress != fullscreen_suppressed_) {
                    fullscreen_suppressed_ = suppress;
                    if (suppress) ShowWindow(hwnd_, SW_HIDE);
                    else SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                }
                if (!suppress && IsWindowVisible(hwnd_)) {
                    RefreshRunningState();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return 0;
            }
            break;

        case WM_DPICHANGED:
        case WM_DISPLAYCHANGE:
            ApplyPlacement();   // our own placement logic picks the monitor and DPI; ignore the suggested rectangle
            return 0;

        case WM_SETTINGCHANGE:
            if (lparam && lstrcmpW(reinterpret_cast<const wchar_t*>(lparam), L"ImmersiveColorSet") == 0) {
                theme_ = MakeTheme(settings_.general.theme);
                InvalidateRect(hwnd_, nullptr, FALSE);
            } else if (wparam == SPI_SETWORKAREA) {
                ApplyPlacement();
            }
            break;

        case WM_DESTROY:
            KillTimer(hwnd_, kTimerId);
            break;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

} // namespace peek
