// Host-side unit tests for the pure C++ core (INI, hotkeys, settings).
// Build and run on any platform:  tests/run.sh
#include "core/hotkey.h"
#include "core/ini.h"
#include "core/settings.h"

#include <cstdio>
#include <string>

namespace {
int g_failures = 0;
int g_checks = 0;

void Check(bool ok, const char* expr, int line) {
    ++g_checks;
    if (!ok) { ++g_failures; std::printf("FAIL line %d: %s\n", line, expr); }
}
} // namespace
#define CHECK(expr) Check((expr), #expr, __LINE__)

using namespace peek;

static void IniTests() {
    IniFile ini;
    ini.Parse("\xEF\xBB\xBF; comment\r\n[Hud]\r\nOpacity = 70\r\n# other\r\nMode=expanded\r\n\r\n[ghost]\r\nRetention=2000\r\nbroken line\r\n");
    CHECK(ini.Get("hud", "opacity") == "70");
    CHECK(ini.Get("HUD", "MODE") == "expanded");
    CHECK(ini.Get("Ghost", "Retention") == "2000");
    CHECK(!ini.Has("Ghost", "broken line"));
    CHECK(ini.Get("Hud", "Missing", "fallback") == "fallback");

    ini.Set("Hud", "Opacity", "55");
    IniFile again;
    again.Parse(ini.Serialize());
    CHECK(again.Get("Hud", "Opacity") == "55");
    CHECK(again.Get("Ghost", "Retention") == "2000");

    IniFile utf8;
    utf8.Set("Ghost", "FilterText", "caf\xC3\xA9 \xE2\x80\x94 test");
    IniFile reread;
    reread.Parse(utf8.Serialize());
    CHECK(reread.Get("Ghost", "FilterText") == "caf\xC3\xA9 \xE2\x80\x94 test");
}

static void HotkeyTests() {
    HotkeySpec spec;
    CHECK(ParseHotkey("Ctrl+Shift+G", spec));
    CHECK(spec.modifiers == (kModControl | kModShift) && spec.vk == 'G');
    CHECK(FormatHotkey(spec) == "Ctrl+Shift+G");
    CHECK(ParseHotkey(" alt + f9 ", spec));
    CHECK(spec.modifiers == kModAlt && spec.vk == 0x78);
    CHECK(FormatHotkey(spec) == "Alt+F9");
    CHECK(ParseHotkey("Win+Ctrl+Space", spec));
    CHECK(FormatHotkey(spec) == "Win+Ctrl+Space");
    HotkeySpec untouched{1, 2};
    CHECK(!ParseHotkey("G", untouched));            // no modifier
    CHECK(!ParseHotkey("Ctrl+", untouched));
    CHECK(!ParseHotkey("Ctrl+Shift", untouched));   // no key
    CHECK(!ParseHotkey("Ctrl+G+H", untouched));     // two keys
    CHECK(!ParseHotkey("Ctrl+F25", untouched));
    CHECK(!ParseHotkey("", untouched));
    CHECK(untouched.modifiers == 1 && untouched.vk == 2);
}

static void SettingsTests() {
    Settings defaults;
    CHECK(!defaults.shortcuts.activate.enabled);     // shortcuts must be off by default
    CHECK(!defaults.shortcuts.ghost.enabled);
    CHECK(!defaults.shortcuts.focus.enabled);
    CHECK(!defaults.ghost.group_lifecycle);
    CHECK(defaults.hud.visible);

    // Round trip.
    Settings changed;
    changed.hud.mode = HudMode::Expanded;
    changed.hud.opacity = 60;
    changed.hud.anchor = HudAnchor::BottomLeft;
    changed.ghost.retention = 2000;
    changed.ghost.trace = TraceMode::Minutes5;
    changed.ghost.filter_text = "power\xC5\x9Bhell";
    changed.ghost.filter_categories = CategoryBit(EventCategory::ProcessStarted) | CategoryBit(EventCategory::ProcessTerminated);
    changed.ghost.filter_state = StateFilter::Running;
    changed.shortcuts.ghost = {true, "Alt+F9"};
    changed.focus.highlight_color = "#112233";
    IniFile ini = ToIni(changed);
    IniFile parsed; parsed.Parse(ini.Serialize());
    Settings loaded;
    LoadFromIni(parsed, loaded);
    CHECK(loaded.hud.mode == HudMode::Expanded);
    CHECK(loaded.hud.opacity == 60);
    CHECK(loaded.hud.anchor == HudAnchor::BottomLeft);
    CHECK(loaded.ghost.retention == 2000);
    CHECK(loaded.ghost.trace == TraceMode::Minutes5);
    CHECK(loaded.ghost.filter_text == changed.ghost.filter_text);
    CHECK(loaded.ghost.filter_categories == changed.ghost.filter_categories);
    CHECK(loaded.ghost.filter_state == StateFilter::Running);
    CHECK(loaded.shortcuts.ghost.enabled && loaded.shortcuts.ghost.key == "Alt+F9");
    CHECK(loaded.focus.highlight_color == "#112233");

    // Garbage input is clamped or repaired, never accepted.
    IniFile bad;
    bad.Parse("[Hud]\nOpacity=5000\nOffsetX=-40\nCompactRows=0\nMode=weird\nAnchor=nowhere\n"
              "[Ghost]\nRetention=3\nTrace=banana\nFilterCategories=0\nFilterMinutes=-9\n"
              "[Unlock]\nMaxDepth=99\nMaxFiles=0\n"
              "[Focus]\nHighlightColor=notacolor\nHighlightWidth=500\n"
              "[Shortcuts]\nGhost=NoModifierKey\nFocus=Ctrl+Shift+F\nFocusEnabled=maybe\n");
    Settings repaired;
    LoadFromIni(bad, repaired);
    CHECK(repaired.hud.opacity == 100);
    CHECK(repaired.hud.offset_x == 0);
    CHECK(repaired.hud.compact_rows == 1);
    CHECK(repaired.hud.mode == HudMode::Compact);        // unknown text keeps the default
    CHECK(repaired.hud.anchor == HudAnchor::TopRight);
    CHECK(repaired.ghost.retention == 50);
    CHECK(repaired.ghost.trace == TraceMode::Off);
    CHECK(repaired.ghost.filter_categories == kAllCategories);
    CHECK(repaired.ghost.filter_minutes == 0);
    CHECK(repaired.unlock.max_depth == 16);
    CHECK(repaired.unlock.max_files == 1);
    CHECK(repaired.focus.highlight_color == "#3985FF");
    CHECK(repaired.focus.highlight_width == 12);
    CHECK(repaired.shortcuts.ghost.key == "Ctrl+Shift+G");
    CHECK(!repaired.shortcuts.focus.enabled);             // unparsable bool keeps the default (off)

    std::uint32_t rgb = 0;
    CHECK(ParseColor("#3985FF", rgb) && rgb == 0x3985FF);
    CHECK(FormatColor(0x3985FF) == "#3985FF");
    CHECK(!ParseColor("#12345", rgb));
    CHECK(!ParseColor("zzzzzz", rgb));
}

int main() {
    IniTests();
    HotkeyTests();
    SettingsTests();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
