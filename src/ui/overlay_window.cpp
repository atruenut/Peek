#include "ui/overlay_window.h"

#include <shellscalingapi.h>

#include <algorithm>

namespace peek {
namespace {
constexpr wchar_t kClassName[] = L"Peek.FocusOverlay";
constexpr UINT_PTR kExpireTimer = 1;
}

bool OverlayWindow::Create(HINSTANCE instance) {
    if (hwnd_) return true;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                            kClassName, L"Peek highlight", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, this);
    if (!hwnd_) return false;
    SetLayeredWindowAttributes(hwnd_, 0, 230, LWA_ALPHA);
    return true;
}

void OverlayWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void OverlayWindow::Show(const RECT& bounds, COLORREF color, int width_dips, int duration_ms) {
    if (!hwnd_ || bounds.right <= bounds.left || bounds.bottom <= bounds.top) return;
    const bool same = EqualRect(&bounds, &shown_) && color == color_ && width_dips == width_dips_ && duration_ms == duration_ms_;
    if (same && (Visible() || expired_)) return;

    HMONITOR monitor = MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST);
    UINT dpi_x = 96, dpi_y = 96;
    GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
    const int width = std::max(1, MulDiv(width_dips, static_cast<int>(dpi_x ? dpi_x : 96), 96));

    // The frame sits just outside the element so it never covers the content being inspected.
    const RECT outer{bounds.left - width, bounds.top - width, bounds.right + width, bounds.bottom + width};
    const int w = outer.right - outer.left;
    const int h = outer.bottom - outer.top;
    HRGN region = CreateRectRgn(0, 0, w, h);
    HRGN hole = CreateRectRgn(width, width, w - width, h - width);
    CombineRgn(region, region, hole, RGN_DIFF);
    DeleteObject(hole);
    if (!SetWindowRgn(hwnd_, region, FALSE)) DeleteObject(region);   // on success the system owns the region

    shown_ = bounds;
    color_ = color;
    width_dips_ = width_dips;
    duration_ms_ = duration_ms;
    expired_ = false;
    SetWindowPos(hwnd_, HWND_TOPMOST, outer.left, outer.top, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hwnd_, nullptr, FALSE);
    if (duration_ms > 0) SetTimer(hwnd_, kExpireTimer, static_cast<UINT>(duration_ms), nullptr);
    else KillTimer(hwnd_, kExpireTimer);
}

void OverlayWindow::Hide() {
    if (!hwnd_) return;
    KillTimer(hwnd_, kExpireTimer);
    ShowWindow(hwnd_, SW_HIDE);
    shown_ = {};   // the next Show always draws
    expired_ = false;
}

LRESULT CALLBACK OverlayWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    OverlayWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        self = static_cast<OverlayWindow*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    switch (message) {
        case WM_NCHITTEST: return HTTRANSPARENT;
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_ERASEBKGND: return 1;
        case WM_TIMER:
            if (wparam == kExpireTimer && self) {
                KillTimer(hwnd, kExpireTimer);
                ShowWindow(hwnd, SW_HIDE);
                self->expired_ = true;
                return 0;
            }
            break;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT rect{};
            GetClientRect(hwnd, &rect);
            HBRUSH brush = CreateSolidBrush(self ? self->color_ : RGB(57, 133, 255));
            FillRect(dc, &rect, brush);
            DeleteObject(brush);
            EndPaint(hwnd, &paint);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

} // namespace peek
