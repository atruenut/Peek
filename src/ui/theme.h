#pragma once

#include "core/settings.h"

#include <windows.h>

namespace peek {

// Restrained palette: neutral surfaces, one accent, and four muted event colors.
struct Theme {
    bool dark{};
    COLORREF background{};
    COLORREF surface{};
    COLORREF text{};
    COLORREF muted{};
    COLORREF accent{};
    COLORREF border{};
    COLORREF hover{};
    COLORREF selected{};
    COLORREF event_started{};
    COLORREF event_appeared{};
    COLORREF event_disappeared{};
    COLORREF event_terminated{};
    COLORREF ok{};
    COLORREF warning{};
    COLORREF error{};

    [[nodiscard]] COLORREF EventColor(EventCategory category) const;
};

// True when Windows is set to dark mode for applications.
[[nodiscard]] bool SystemPrefersDark();
[[nodiscard]] Theme MakeTheme(ThemeChoice choice);

} // namespace peek
