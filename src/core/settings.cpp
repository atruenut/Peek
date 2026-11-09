#include "core/settings.h"

#include "core/hotkey.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace peek {
namespace {

std::string Lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

template <typename Enum, std::size_t N>
struct EnumNames {
    struct Entry { Enum value; const char* name; };
    Entry entries[N];

    Enum Parse(const std::string& text, Enum fallback) const {
        const std::string lower = Lower(text);
        for (const auto& entry : entries) if (lower == entry.name) return entry.value;
        return fallback;
    }
    const char* Name(Enum value) const {
        for (const auto& entry : entries) if (entry.value == value) return entry.name;
        return entries[0].name;
    }
};

constexpr EnumNames<ThemeChoice, 3> kTheme{{{ThemeChoice::System, "system"}, {ThemeChoice::Light, "light"}, {ThemeChoice::Dark, "dark"}}};
constexpr EnumNames<CloseButton, 2> kClose{{{CloseButton::HideWindow, "hide"}, {CloseButton::ExitApp, "exit"}}};
constexpr EnumNames<HudMode, 2> kMode{{{HudMode::Compact, "compact"}, {HudMode::Expanded, "expanded"}}};
constexpr EnumNames<HudAnchor, 4> kAnchor{{{HudAnchor::TopLeft, "topleft"}, {HudAnchor::TopRight, "topright"},
                                           {HudAnchor::BottomLeft, "bottomleft"}, {HudAnchor::BottomRight, "bottomright"}}};
constexpr EnumNames<FullscreenBehavior, 2> kFullscreen{{{FullscreenBehavior::Hide, "hide"}, {FullscreenBehavior::Show, "show"}}};
constexpr EnumNames<TraceMode, 4> kTrace{{{TraceMode::Off, "off"}, {TraceMode::Seconds30, "30s"},
                                          {TraceMode::Minutes5, "5m"}, {TraceMode::Continuous, "continuous"}}};
constexpr EnumNames<StateFilter, 3> kState{{{StateFilter::Any, "any"}, {StateFilter::Running, "running"}, {StateFilter::Terminated, "terminated"}}};

bool ReadBool(const IniFile& ini, const char* section, const char* key, bool fallback) {
    if (!ini.Has(section, key)) return fallback;
    const std::string value = Lower(ini.Get(section, key));
    if (value == "1" || value == "true" || value == "yes" || value == "on") return true;
    if (value == "0" || value == "false" || value == "no" || value == "off") return false;
    return fallback;
}

int ReadInt(const IniFile& ini, const char* section, const char* key, int fallback) {
    if (!ini.Has(section, key)) return fallback;
    const std::string value = ini.Get(section, key);
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0') return fallback;
    return static_cast<int>(std::clamp<long>(parsed, -1000000, 1000000));
}

const char* Bool(bool value) { return value ? "1" : "0"; }

void SanitizeShortcut(ShortcutSetting& shortcut, const char* fallback) {
    HotkeySpec spec;
    if (!ParseHotkey(shortcut.key, spec)) shortcut.key = fallback;
    else shortcut.key = FormatHotkey(spec);
}

} // namespace

bool ParseColor(const std::string& text, std::uint32_t& rgb) {
    std::string value = text;
    if (!value.empty() && value[0] == '#') value.erase(0, 1);
    if (value.size() != 6) return false;
    std::uint32_t parsed = 0;
    for (char c : value) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        parsed = parsed * 16 + static_cast<std::uint32_t>(std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : std::tolower(c) - 'a' + 10);
    }
    rgb = parsed;
    return true;
}

std::string FormatColor(std::uint32_t rgb) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "#%06X", rgb & 0xFFFFFFu);
    return buffer;
}

void Sanitize(Settings& s) {
    s.hud.opacity = std::clamp(s.hud.opacity, 30, 100);
    s.hud.offset_x = std::clamp(s.hud.offset_x, 0, 4000);
    s.hud.offset_y = std::clamp(s.hud.offset_y, 0, 4000);
    s.hud.compact_rows = std::clamp(s.hud.compact_rows, 1, 30);
    s.hud.expanded_rows = std::clamp(s.hud.expanded_rows, 1, 40);
    if (s.hud.monitor.empty()) s.hud.monitor = "primary";

    s.ghost.retention = std::clamp(s.ghost.retention, 50, 10000);
    s.ghost.filter_categories &= kAllCategories;
    if (s.ghost.filter_categories == 0) s.ghost.filter_categories = kAllCategories;
    s.ghost.filter_minutes = std::clamp(s.ghost.filter_minutes, 0, 24 * 60);
    if (s.ghost.filter_text.size() > 256) s.ghost.filter_text.resize(256);

    s.unlock.max_depth = std::clamp(s.unlock.max_depth, 0, 16);
    s.unlock.max_files = std::clamp(s.unlock.max_files, 1, 100000);

    std::uint32_t color = 0;
    if (!ParseColor(s.focus.highlight_color, color)) s.focus.highlight_color = "#3985FF";
    else s.focus.highlight_color = FormatColor(color);
    s.focus.highlight_width = std::clamp(s.focus.highlight_width, 1, 12);
    s.focus.highlight_duration_ms = std::clamp(s.focus.highlight_duration_ms, 0, 600000);

    SanitizeShortcut(s.shortcuts.activate, "Ctrl+Alt+Shift+P");
    SanitizeShortcut(s.shortcuts.ghost, "Ctrl+Alt+Shift+G");
    SanitizeShortcut(s.shortcuts.focus, "Ctrl+Alt+Shift+F");
}

void LoadFromIni(const IniFile& ini, Settings& s) {
    s.general.theme = kTheme.Parse(ini.Get("General", "Theme"), s.general.theme);
    s.general.close_button = kClose.Parse(ini.Get("General", "CloseButton"), s.general.close_button);
    s.general.start_with_window = ReadBool(ini, "General", "StartWithWindow", s.general.start_with_window);

    s.hud.visible = ReadBool(ini, "Hud", "Visible", s.hud.visible);
    s.hud.mode = kMode.Parse(ini.Get("Hud", "Mode"), s.hud.mode);
    s.hud.opacity = ReadInt(ini, "Hud", "Opacity", s.hud.opacity);
    if (ini.Has("Hud", "Monitor")) s.hud.monitor = ini.Get("Hud", "Monitor");
    s.hud.anchor = kAnchor.Parse(ini.Get("Hud", "Anchor"), s.hud.anchor);
    s.hud.offset_x = ReadInt(ini, "Hud", "OffsetX", s.hud.offset_x);
    s.hud.offset_y = ReadInt(ini, "Hud", "OffsetY", s.hud.offset_y);
    s.hud.compact_rows = ReadInt(ini, "Hud", "CompactRows", s.hud.compact_rows);
    s.hud.expanded_rows = ReadInt(ini, "Hud", "ExpandedRows", s.hud.expanded_rows);
    s.hud.fullscreen = kFullscreen.Parse(ini.Get("Hud", "Fullscreen"), s.hud.fullscreen);
    s.hud.click_through = ReadBool(ini, "Hud", "ClickThrough", s.hud.click_through);

    s.ghost.enabled = ReadBool(ini, "Ghost", "Enabled", s.ghost.enabled);
    s.ghost.retention = ReadInt(ini, "Ghost", "Retention", s.ghost.retention);
    s.ghost.trace = kTrace.Parse(ini.Get("Ghost", "Trace"), s.ghost.trace);
    s.ghost.ignore_noise = ReadBool(ini, "Ghost", "IgnoreNoiseWindows", s.ghost.ignore_noise);
    s.ghost.group_lifecycle = ReadBool(ini, "Ghost", "GroupByLifecycle", s.ghost.group_lifecycle);
    if (ini.Has("Ghost", "FilterText")) s.ghost.filter_text = ini.Get("Ghost", "FilterText");
    s.ghost.filter_categories = static_cast<std::uint32_t>(std::max(0, ReadInt(ini, "Ghost", "FilterCategories", static_cast<int>(s.ghost.filter_categories))));
    s.ghost.filter_state = kState.Parse(ini.Get("Ghost", "FilterState"), s.ghost.filter_state);
    s.ghost.filter_minutes = ReadInt(ini, "Ghost", "FilterMinutes", s.ghost.filter_minutes);

    s.unlock.explorer_integration = ReadBool(ini, "Unlock", "ExplorerIntegration", s.unlock.explorer_integration);
    s.unlock.max_depth = ReadInt(ini, "Unlock", "MaxDepth", s.unlock.max_depth);
    s.unlock.max_files = ReadInt(ini, "Unlock", "MaxFiles", s.unlock.max_files);

    s.focus.enabled = ReadBool(ini, "Focus", "Enabled", s.focus.enabled);
    if (ini.Has("Focus", "HighlightColor")) s.focus.highlight_color = ini.Get("Focus", "HighlightColor");
    s.focus.highlight_width = ReadInt(ini, "Focus", "HighlightWidth", s.focus.highlight_width);
    s.focus.highlight_duration_ms = ReadInt(ini, "Focus", "HighlightDurationMs", s.focus.highlight_duration_ms);

    auto shortcut = [&](const char* name, ShortcutSetting& target) {
        target.enabled = ReadBool(ini, "Shortcuts", (std::string(name) + "Enabled").c_str(), target.enabled);
        if (ini.Has("Shortcuts", name)) target.key = ini.Get("Shortcuts", name);
    };
    // Files written before version 2 saved the old "all off" shortcut defaults; ignore them once so the new
    // defaults (on) apply. From then on the file records the user's own choice.
    if (ReadInt(ini, "General", "SettingsVersion", 1) >= 2) {
        shortcut("Activate", s.shortcuts.activate);
        shortcut("Ghost", s.shortcuts.ghost);
        shortcut("Focus", s.shortcuts.focus);
    }

    Sanitize(s);
}

IniFile ToIni(const Settings& s) {
    IniFile ini;
    ini.Set("General", "SettingsVersion", "2");
    ini.Set("General", "Theme", kTheme.Name(s.general.theme));
    ini.Set("General", "CloseButton", kClose.Name(s.general.close_button));
    ini.Set("General", "StartWithWindow", Bool(s.general.start_with_window));

    ini.Set("Hud", "Visible", Bool(s.hud.visible));
    ini.Set("Hud", "Mode", kMode.Name(s.hud.mode));
    ini.Set("Hud", "Opacity", std::to_string(s.hud.opacity));
    ini.Set("Hud", "Monitor", s.hud.monitor);
    ini.Set("Hud", "Anchor", kAnchor.Name(s.hud.anchor));
    ini.Set("Hud", "OffsetX", std::to_string(s.hud.offset_x));
    ini.Set("Hud", "OffsetY", std::to_string(s.hud.offset_y));
    ini.Set("Hud", "CompactRows", std::to_string(s.hud.compact_rows));
    ini.Set("Hud", "ExpandedRows", std::to_string(s.hud.expanded_rows));
    ini.Set("Hud", "Fullscreen", kFullscreen.Name(s.hud.fullscreen));
    ini.Set("Hud", "ClickThrough", Bool(s.hud.click_through));

    ini.Set("Ghost", "Enabled", Bool(s.ghost.enabled));
    ini.Set("Ghost", "Retention", std::to_string(s.ghost.retention));
    ini.Set("Ghost", "Trace", kTrace.Name(s.ghost.trace));
    ini.Set("Ghost", "IgnoreNoiseWindows", Bool(s.ghost.ignore_noise));
    ini.Set("Ghost", "GroupByLifecycle", Bool(s.ghost.group_lifecycle));
    ini.Set("Ghost", "FilterText", s.ghost.filter_text);
    ini.Set("Ghost", "FilterCategories", std::to_string(s.ghost.filter_categories));
    ini.Set("Ghost", "FilterState", kState.Name(s.ghost.filter_state));
    ini.Set("Ghost", "FilterMinutes", std::to_string(s.ghost.filter_minutes));

    ini.Set("Unlock", "ExplorerIntegration", Bool(s.unlock.explorer_integration));
    ini.Set("Unlock", "MaxDepth", std::to_string(s.unlock.max_depth));
    ini.Set("Unlock", "MaxFiles", std::to_string(s.unlock.max_files));

    ini.Set("Focus", "Enabled", Bool(s.focus.enabled));
    ini.Set("Focus", "HighlightColor", s.focus.highlight_color);
    ini.Set("Focus", "HighlightWidth", std::to_string(s.focus.highlight_width));
    ini.Set("Focus", "HighlightDurationMs", std::to_string(s.focus.highlight_duration_ms));

    ini.Set("Shortcuts", "ActivateEnabled", Bool(s.shortcuts.activate.enabled));
    ini.Set("Shortcuts", "Activate", s.shortcuts.activate.key);
    ini.Set("Shortcuts", "GhostEnabled", Bool(s.shortcuts.ghost.enabled));
    ini.Set("Shortcuts", "Ghost", s.shortcuts.ghost.key);
    ini.Set("Shortcuts", "FocusEnabled", Bool(s.shortcuts.focus.enabled));
    ini.Set("Shortcuts", "Focus", s.shortcuts.focus.key);
    return ini;
}

} // namespace peek
