#pragma once

#include "cli.h"
#include "core/event_store.h"
#include "core/hotkey.h"
#include "core/icon_cache.h"
#include "core/process_resolver.h"
#include "core/settings_manager.h"
#include "focus/focus_inspector.h"
#include "ghost/ghost_monitor.h"
#include "ui/hud_window.h"
#include "ui/main_window.h"
#include "ui/overlay_window.h"
#include "unlock/unlock_service.h"

#include <shellapi.h>

#include <mutex>
#include <optional>

namespace peek {

constexpr ULONG_PTR kCopyDataMagic = 0x5045454B;   // "PEEK"; WM_COPYDATA from a second instance

// Composition root. Owns every service and window, and is the one place that turns settings into behaviour.
// Monitoring (GhostMonitor + EventStore) is independent of every window: the HUD, the tray icon and the
// investigation window can all be hidden and capture continues.
class App : public CommandHost {
public:
    explicit App(const CommandLine& command_line);
    ~App();
    int Run(HINSTANCE instance);

    void Execute(Command command, std::uint64_t argument = 0) override;
    [[nodiscard]] std::wstring ShortcutStatus(int slot) const override;

    // True if another Peek instance is running; the request was forwarded to it.
    static bool ForwardToRunningInstance(const CommandLine& command_line);

private:
    static constexpr UINT kTrayMessage = WM_APP + 1;
    static constexpr UINT kGhostMessage = WM_APP + 2;
    static constexpr UINT kFocusSnapshotMessage = WM_APP + 3;
    static constexpr UINT kFocusTreeMessage = WM_APP + 4;
    static constexpr UINT kFocusModeMessage = WM_APP + 5;
    enum Timer : UINT_PTR { kThrottle = 1, kInvestigate = 2, kStatus = 3 };

    static LRESULT CALLBACK ControllerProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

    void ApplySettings();
    void ApplyMonitoring(const Settings& settings);
    void RegisterShortcuts(const Settings& settings);
    void RefreshViews();
    void UpdateStatus();
    void AddTrayIcon();
    void UpdateTrayTip();
    void ShowTrayMenu();
    void QueueInvestigation(const std::vector<std::wstring>& paths, bool inspect);
    void ShowHighlight(const FocusSnapshot& snapshot);
    void HandleCopyData(const COPYDATASTRUCT& data);

    CommandLine command_line_;
    SettingsManager settings_;
    EventStore events_;
    ProcessResolver resolver_;
    IconCache icons_{64};
    UnlockService unlock_;
    GhostMonitor ghost_;
    FocusInspector focus_;
    MainWindow window_;
    HudWindow hud_;
    OverlayWindow overlay_;

    HINSTANCE instance_{};
    HWND controller_{};
    UINT taskbar_created_{};
    NOTIFYICONDATAW tray_{};
    bool tray_added_{};

    std::atomic_bool ghost_dirty_{false};
    bool throttle_pending_{false};
    TraceMode applied_trace_{TraceMode::Off};
    bool monitoring_started_{false};

    std::mutex focus_mutex_;
    std::optional<FocusSnapshot> pending_snapshot_;
    std::vector<FocusTreeUpdate> pending_trees_;
    std::optional<FocusSnapshot> last_snapshot_;
    InspectMode focus_mode_{InspectMode::Off};

    std::vector<std::wstring> pending_paths_;
    bool pending_inspect_{false};

    std::wstring shortcut_status_[3];
};

} // namespace peek
