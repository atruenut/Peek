#pragma once

#include "core/event_store.h"
#include "core/icon_cache.h"
#include "core/settings_manager.h"
#include "ui/commands.h"
#include "ui/theme.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace peek {

// The always-available desktop overlay: a narrow, persistent event ticker.
//
// It never takes focus (WS_EX_NOACTIVATE), has no taskbar button or Alt+Tab entry (WS_EX_TOOLWINDOW),
// sits above other windows, and is positioned relative to a corner of a chosen monitor. Hiding it only
// hides the window; monitoring is owned elsewhere and is unaffected. All methods run on the UI thread.
class HudWindow {
public:
    HudWindow(EventStore& events, SettingsManager& settings, IconCache& icons, CommandHost& host);
    ~HudWindow();
    HudWindow(const HudWindow&) = delete;
    HudWindow& operator=(const HudWindow&) = delete;

    bool Create(HINSTANCE instance);
    void Destroy();
    [[nodiscard]] HWND Handle() const { return hwnd_; }

    // Re-read settings: visibility, placement, opacity, theme, row counts and the active filter.
    void ApplySettings();
    // Events were added, dismissed or cleared.
    void EventsChanged();
    // Footer text and the state of the status dot.
    void SetStatus(const std::wstring& text, bool monitoring, bool degraded);

private:
    enum class Zone { None, Header, ModeButton, PauseButton, HideButton, Row, Footer };
    struct Hit {
        Zone zone{Zone::None};
        int row{-1};
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

    void Rebuild();
    void ApplyPlacement();
    void ApplyVisibility();
    void UpdateFonts();
    void Paint(HDC target);
    void PaintHeader(HDC dc, const RECT& client);
    void PaintRows(HDC dc);
    void PaintFooter(HDC dc, const RECT& client);

    [[nodiscard]] Hit HitTest(POINT client) const;
    [[nodiscard]] RECT HeaderRect() const;
    [[nodiscard]] RECT ButtonRect(Zone button) const;
    [[nodiscard]] RECT RowRect(int row) const;
    [[nodiscard]] int RowHeight() const;
    [[nodiscard]] int HeaderHeight() const;
    [[nodiscard]] int FooterHeight() const;
    [[nodiscard]] int Px(int dips) const;

    void OnClick(POINT client);
    void OnDoubleClick(POINT client);
    void ShowRowMenu(int row, POINT screen);
    void SaveDraggedPosition();
    void ShowHeaderMenu(POINT screen);
    [[nodiscard]] HMONITOR PickMonitor() const;
    [[nodiscard]] int VisibleRows() const;
    void ClampScroll();
    void RefreshRunningState();
    [[nodiscard]] bool FullscreenAppActive() const;
    [[nodiscard]] EventFilter CurrentFilter(const Settings& settings) const;

    EventStore& events_;
    SettingsManager& settings_manager_;
    IconCache& icons_;
    CommandHost& host_;

    HWND hwnd_{};
    HINSTANCE instance_{};
    UINT dpi_{96};
    Theme theme_;
    Settings settings_;
    HFONT font_{};
    HFONT font_bold_{};
    HFONT font_small_{};

    std::vector<GhostEvent> rows_;                    // matching events, newest first (capped); scroll_ selects the visible slice
    std::unordered_map<std::uint64_t, bool> running_; // event sequence -> process still running
    int scroll_{0};
    std::uint64_t selected_{0};
    Hit hover_;
    bool tracking_mouse_{false};
    bool placing_{false};                             // guards against re-entrancy while we move the window ourselves
    bool fullscreen_suppressed_{false};

    std::wstring status_text_;
    bool monitoring_{true};
    bool degraded_{false};
    std::wstring hint_;
};

} // namespace peek
