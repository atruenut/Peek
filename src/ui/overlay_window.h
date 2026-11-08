#pragma once

#include <windows.h>

namespace peek {

// Outlines a screen rectangle (the inspected UI element). Click-through, never activates, no taskbar button.
// UI thread only.
class OverlayWindow {
public:
    ~OverlayWindow() { Destroy(); }
    bool Create(HINSTANCE instance);
    void Destroy();

    // Draws a frame just outside `bounds` (screen pixels). `width_dips` is scaled for the monitor's DPI.
    // Calling again with identical arguments does nothing, so a stable target never flickers.
    // `duration_ms` > 0 hides the frame after that long unless the target changes first.
    void Show(const RECT& bounds, COLORREF color, int width_dips, int duration_ms);
    void Hide();
    [[nodiscard]] bool Visible() const { return hwnd_ && IsWindowVisible(hwnd_); }
    [[nodiscard]] HWND Handle() const { return hwnd_; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    HWND hwnd_{};
    RECT shown_{};
    COLORREF color_{};
    int width_dips_{};
    int duration_ms_{};
    bool expired_{false};   // hidden by the duration timer; stays hidden until the target changes
};

} // namespace peek
