#pragma once

// Application settings model and its INI mapping. Pure C++ (no Win32) so it can be unit tested anywhere.
// Strings are UTF-8. Every value is validated and clamped by Sanitize(), so a hand-edited or corrupt INI
// file can never produce an unusable configuration.

#include "core/categories.h"
#include "core/ini.h"

#include <cstdint>
#include <string>

namespace peek {

enum class ThemeChoice { System, Light, Dark };
enum class CloseButton { HideWindow, ExitApp };
enum class HudMode { Compact, Expanded };
enum class HudAnchor { TopLeft, TopRight, BottomLeft, BottomRight };
enum class FullscreenBehavior { Hide, Show };
enum class TraceMode { Off, Seconds30, Minutes5, Continuous };
enum class StateFilter { Any, Running, Terminated };

struct ShortcutSetting {
    bool enabled{false};
    std::string key;
};

struct Settings {
    struct General {
        ThemeChoice theme{ThemeChoice::System};
        CloseButton close_button{CloseButton::HideWindow};
        bool start_with_window{false};
    } general;

    struct Hud {
        bool visible{true};
        HudMode mode{HudMode::Compact};
        int opacity{88};                    // percent, 30..100
        std::string monitor{"primary"};     // "primary" or a display device name such as \\.\DISPLAY2
        HudAnchor anchor{HudAnchor::TopRight};
        int offset_x{16};                   // device-independent pixels from the anchored corner
        int offset_y{16};
        int compact_rows{6};
        int expanded_rows{14};
        FullscreenBehavior fullscreen{FullscreenBehavior::Hide};
        bool click_through{false};          // event rows ignore the mouse; only the header is interactive
    } hud;

    struct Ghost {
        bool enabled{true};
        int retention{500};                 // events kept in memory, 50..10000
        TraceMode trace{TraceMode::Off};
        bool ignore_noise{true};            // skip IME, tooltip, shadow and similar helper windows
        bool group_lifecycle{false};        // list events grouped by process lifetime
        std::string filter_text;
        std::uint32_t filter_categories{kAllCategories};
        StateFilter filter_state{StateFilter::Any};
        int filter_minutes{0};              // 0 = no age limit
    } ghost;

    struct Unlock {
        bool explorer_integration{false};   // reflects the Explorer registration state last requested
        int max_depth{2};                   // directory recursion depth, 0..16
        int max_files{500};                 // file-count cap for one investigation, 1..100000
    } unlock;

    struct Focus {
        bool enabled{true};
        std::string highlight_color{"#3985FF"};
        int highlight_width{3};             // pixels at 96 DPI, 1..12
        int highlight_duration_ms{0};       // 0 = until inspection stops
    } focus;

    struct Shortcuts {
        // On by default. Three modifiers keep them clear of the shortcuts other apps use (Ctrl+Shift+P opens a private
        // window in Firefox and the command palette in VS Code, and a global hotkey would steal those everywhere).
        ShortcutSetting activate{true, "Ctrl+Alt+Shift+P"};
        ShortcutSetting ghost{true, "Ctrl+Alt+Shift+G"};
        ShortcutSetting focus{true, "Ctrl+Alt+Shift+F"};
    } shortcuts;
};

// Clamp every numeric field into its valid range and repair unknown strings.
void Sanitize(Settings& settings);
void LoadFromIni(const IniFile& ini, Settings& settings);
[[nodiscard]] IniFile ToIni(const Settings& settings);

// "#RRGGBB" <-> 0x00RRGGBB.
[[nodiscard]] bool ParseColor(const std::string& text, std::uint32_t& rgb);
[[nodiscard]] std::string FormatColor(std::uint32_t rgb);

// Retention presets offered in the UI.
constexpr int kRetentionPresets[] = {100, 500, 2000};

} // namespace peek
