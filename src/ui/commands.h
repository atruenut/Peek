#pragma once

#include <cstdint>

namespace peek {

// Actions that any UI surface (HUD, tray, shortcuts, investigation window) can request from the application.
// Keeping them in one place means every surface offers the same behaviour.
enum class Command {
    ShowInvestigation,   // arg: tab index (see MainWindow::Tab), or -1 for the current tab
    ShowEvent,           // arg: event sequence number; opens the Ghost tab on that event
    ShowSettings,
    ToggleHud,           // show or hide the HUD; monitoring is unaffected
    ToggleHudMode,       // compact <-> expanded
    ToggleMonitoring,    // pause or resume Ghost capture
    ToggleFocus,         // start or stop Focus inspection
    ClearEvents,
    EventsChanged,       // a UI surface dismissed events directly in the store; refresh every view
    SettingsChanged,     // a UI surface changed settings; re-apply them everywhere
    Exit,
};

class CommandHost {
public:
    virtual ~CommandHost() = default;
    virtual void Execute(Command command, std::uint64_t argument = 0) = 0;
};

} // namespace peek
