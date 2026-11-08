#include "core/hotkey.h"

#include <cctype>
#include <vector>

namespace peek {
namespace {

struct NamedKey {
    const char* name;
    unsigned vk;
};

// Virtual-key codes are written out so this file does not depend on windows.h.
constexpr NamedKey kNamedKeys[] = {
    {"Space", 0x20}, {"Enter", 0x0D}, {"Tab", 0x09}, {"Esc", 0x1B}, {"Backspace", 0x08},
    {"Insert", 0x2D}, {"Delete", 0x2E}, {"Home", 0x24}, {"End", 0x23},
    {"PageUp", 0x21}, {"PageDown", 0x22},
    {"Left", 0x25}, {"Up", 0x26}, {"Right", 0x27}, {"Down", 0x28},
    {"Pause", 0x13}, {"PrintScreen", 0x2C},
};

std::string Lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    return text;
}

bool ParseKey(std::string_view token, unsigned& vk) {
    const std::string lower = Lower(token);
    if (lower.size() == 1) {
        const char c = lower[0];
        if (c >= 'a' && c <= 'z') { vk = static_cast<unsigned>(c - 'a' + 'A'); return true; }
        if (c >= '0' && c <= '9') { vk = static_cast<unsigned>(c); return true; }
    }
    if (lower.size() >= 2 && lower[0] == 'f') {
        int number = 0;
        for (std::size_t i = 1; i < lower.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(lower[i]))) { number = 0; break; }
            number = number * 10 + (lower[i] - '0');
        }
        if (number >= 1 && number <= 24) { vk = 0x70u + static_cast<unsigned>(number - 1); return true; }
    }
    for (const auto& key : kNamedKeys) {
        if (lower == Lower(key.name)) { vk = key.vk; return true; }
    }
    return false;
}

} // namespace

bool ParseHotkey(std::string_view text, HotkeySpec& out) {
    unsigned modifiers = 0;
    unsigned vk = 0;
    bool have_key = false;
    while (!text.empty()) {
        const std::size_t plus = text.find('+');
        const std::string_view token = Trim(plus == std::string_view::npos ? text : text.substr(0, plus));
        text = plus == std::string_view::npos ? std::string_view{} : text.substr(plus + 1);
        if (token.empty()) return false;
        const std::string lower = Lower(token);
        if (lower == "ctrl" || lower == "control") modifiers |= kModControl;
        else if (lower == "alt") modifiers |= kModAlt;
        else if (lower == "shift") modifiers |= kModShift;
        else if (lower == "win" || lower == "windows") modifiers |= kModWin;
        else {
            if (have_key || !ParseKey(token, vk)) return false;
            have_key = true;
        }
    }
    if (!have_key || modifiers == 0) return false;
    out = {modifiers, vk};
    return true;
}

std::string FormatHotkey(const HotkeySpec& spec) {
    if (!spec.Valid()) return {};
    std::string out;
    auto add = [&](const std::string& part) { if (!out.empty()) out += '+'; out += part; };
    if (spec.modifiers & kModWin) add("Win");
    if (spec.modifiers & kModControl) add("Ctrl");
    if (spec.modifiers & kModAlt) add("Alt");
    if (spec.modifiers & kModShift) add("Shift");
    if ((spec.vk >= 'A' && spec.vk <= 'Z') || (spec.vk >= '0' && spec.vk <= '9')) {
        add(std::string(1, static_cast<char>(spec.vk)));
    } else if (spec.vk >= 0x70 && spec.vk <= 0x87) {
        add("F" + std::to_string(spec.vk - 0x70 + 1));
    } else {
        for (const auto& key : kNamedKeys) {
            if (key.vk == spec.vk) { add(key.name); return out; }
        }
        return {};
    }
    return out;
}

} // namespace peek
