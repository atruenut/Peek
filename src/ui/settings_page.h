#pragma once

#include "ui/page.h"

#include <string>
#include <vector>

namespace peek {

// Settings tab. Every control writes through to Peek.ini immediately; only shortcuts need "Apply".
class SettingsPage : public Page {
public:
    using Page::Page;
    void SettingsChanged() override;
    void Refresh() override { UpdateShellStatus(); UpdateShortcutStatus(); }

protected:
    void OnCreate() override;
    void OnLayout(int width, int height) override;
    void OnCommand(int id, int code, HWND control) override;
    void OnMessage(UINT message, WPARAM wparam, LPARAM lparam) override;

private:
    enum Id {
        kTheme = 500, kClose, kStartWindow,
        kHudVisible, kHudExpanded, kClickThrough, kOpacity, kCorner, kMonitor, kFullscreen,
        kGhostEnabled, kRetention, kTrace, kNoise,
        kRegister, kUnregister, kDepth, kMaxFiles,
        kFocusEnabled, kWidth, kColor, kDuration,
        kScActivate, kScActivateKey, kScGhost, kScGhostKey, kScFocus, kScFocusKey, kApplyShortcuts,
        kOpenFolder,
    };

    void Save(const std::function<void(Settings&)>& change);
    void UpdateShellStatus();
    void UpdateShortcutStatus();
    void ReadNumbers();
    [[nodiscard]] bool Checked(HWND check) const { return SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED; }
    [[nodiscard]] int Sel(HWND combo) const { return static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0)); }

    struct Heading { HWND label; };
    HWND h_general_{}, h_hud_{}, h_ghost_{}, h_unlock_{}, h_focus_{}, h_shortcuts_{}, h_file_{};
    HWND l_theme_{}, theme_{}, l_close_{}, close_{}, start_window_{};
    HWND hud_visible_{}, hud_expanded_{}, click_through_{}, l_opacity_{}, opacity_{}, opacity_value_{}, l_corner_{}, corner_{},
        l_monitor_{}, monitor_{}, l_fullscreen_{}, fullscreen_{};
    HWND ghost_enabled_{}, l_retention_{}, retention_{}, l_trace_{}, trace_{}, noise_{};
    HWND reg_{}, unreg_{}, shell_status_{}, l_depth_{}, depth_{}, l_files_{}, files_{};
    HWND focus_enabled_{}, l_width_{}, width_{}, l_color_{}, color_{}, l_duration_{}, duration_{};
    HWND sc_check_[3]{}, sc_key_[3]{}, sc_status_[3]{}, apply_sc_{};
    HWND file_path_{}, open_folder_{};
    std::vector<std::string> monitors_;   // device names parallel to the monitor combo (entry 0 = primary)
    COLORREF custom_colors_[16]{};
};

} // namespace peek
