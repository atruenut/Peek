#pragma once

// Hotkey text <-> (modifiers, virtual key). Pure C++ so it can be unit tested anywhere.
// Modifier bits intentionally equal the Win32 MOD_* values.

#include <string>
#include <string_view>

namespace peek {

constexpr unsigned kModAlt = 0x1;
constexpr unsigned kModControl = 0x2;
constexpr unsigned kModShift = 0x4;
constexpr unsigned kModWin = 0x8;

struct HotkeySpec {
    unsigned modifiers{};
    unsigned vk{};
    [[nodiscard]] bool Valid() const { return modifiers != 0 && vk != 0; }
    [[nodiscard]] bool operator==(const HotkeySpec& other) const {
        return modifiers == other.modifiers && vk == other.vk;
    }
};

// Accepts text such as "Ctrl+Shift+G", "alt + F9", "Win+Ctrl+Space". At least one modifier is required so a
// plain key is never captured globally. Returns false and leaves `out` untouched on error.
[[nodiscard]] bool ParseHotkey(std::string_view text, HotkeySpec& out);

// Canonical text, e.g. "Ctrl+Shift+G". Empty when the spec is not valid.
[[nodiscard]] std::string FormatHotkey(const HotkeySpec& spec);

} // namespace peek
