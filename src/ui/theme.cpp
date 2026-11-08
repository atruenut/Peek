#include "ui/theme.h"

namespace peek {

COLORREF Theme::EventColor(EventCategory category) const {
    switch (category) {
        case EventCategory::ProcessStarted: return event_started;
        case EventCategory::WindowAppeared: return event_appeared;
        case EventCategory::WindowDisappeared: return event_disappeared;
        case EventCategory::ProcessTerminated: return event_terminated;
    }
    return muted;
}

bool SystemPrefersDark() {
    DWORD value = 1;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return false;
    }
    return value == 0;
}

Theme MakeTheme(ThemeChoice choice) {
    const bool dark = choice == ThemeChoice::Dark || (choice == ThemeChoice::System && SystemPrefersDark());
    Theme t;
    t.dark = dark;
    if (dark) {
        t.background = RGB(32, 32, 32);
        t.surface = RGB(43, 43, 43);
        t.text = RGB(238, 238, 238);
        t.muted = RGB(160, 160, 160);
        t.accent = RGB(96, 165, 250);
        t.border = RGB(68, 68, 68);
        t.hover = RGB(54, 54, 54);
        t.selected = RGB(52, 66, 88);
        t.event_started = RGB(96, 165, 250);
        t.event_appeared = RGB(96, 190, 140);
        t.event_disappeared = RGB(150, 150, 150);
        t.event_terminated = RGB(224, 120, 120);
        t.ok = RGB(96, 190, 140);
        t.warning = RGB(226, 176, 80);
        t.error = RGB(224, 120, 120);
    } else {
        t.background = RGB(243, 243, 243);
        t.surface = RGB(255, 255, 255);
        t.text = RGB(28, 28, 28);
        t.muted = RGB(100, 100, 100);
        t.accent = RGB(0, 95, 184);
        t.border = RGB(218, 218, 218);
        t.hover = RGB(236, 236, 236);
        t.selected = RGB(220, 234, 250);
        t.event_started = RGB(0, 95, 184);
        t.event_appeared = RGB(24, 130, 80);
        t.event_disappeared = RGB(120, 120, 120);
        t.event_terminated = RGB(190, 60, 60);
        t.ok = RGB(24, 130, 80);
        t.warning = RGB(176, 120, 20);
        t.error = RGB(190, 60, 60);
    }
    return t;
}

} // namespace peek
