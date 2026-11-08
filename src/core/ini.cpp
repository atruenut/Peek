#include "core/ini.h"

#include <algorithm>
#include <cctype>

namespace peek {
namespace {

std::string_view Trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    return text;
}

bool EqualsNoCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

} // namespace

IniFile::Section* IniFile::FindSection(std::string_view name) {
    for (auto& section : sections_) if (EqualsNoCase(section.name, name)) return &section;
    return nullptr;
}

const IniFile::Section* IniFile::FindSection(std::string_view name) const {
    for (const auto& section : sections_) if (EqualsNoCase(section.name, name)) return &section;
    return nullptr;
}

void IniFile::Parse(std::string_view text) {
    sections_.clear();
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }
    std::string current;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        std::string_view line = Trim(end == std::string_view::npos ? text : text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        if (line.empty() || line.front() == ';' || line.front() == '#') continue;
        if (line.front() == '[') {
            const std::size_t close = line.find(']');
            if (close == std::string_view::npos) continue;
            current = std::string(Trim(line.substr(1, close - 1)));
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) continue;
        const std::string_view key = Trim(line.substr(0, equals));
        if (key.empty()) continue;
        Set(current, key, Trim(line.substr(equals + 1)));
    }
}

std::string IniFile::Serialize() const {
    std::string out;
    for (const auto& section : sections_) {
        if (section.items.empty()) continue;
        if (!out.empty()) out += "\r\n";
        if (!section.name.empty()) out += "[" + section.name + "]\r\n";
        for (const auto& [key, value] : section.items) out += key + "=" + value + "\r\n";
    }
    return out;
}

bool IniFile::Has(std::string_view section, std::string_view key) const {
    const Section* found = FindSection(section);
    if (!found) return false;
    return std::any_of(found->items.begin(), found->items.end(),
                       [&](const auto& item) { return EqualsNoCase(item.first, key); });
}

std::string IniFile::Get(std::string_view section, std::string_view key, std::string_view fallback) const {
    if (const Section* found = FindSection(section)) {
        for (const auto& [name, value] : found->items) if (EqualsNoCase(name, key)) return value;
    }
    return std::string(fallback);
}

void IniFile::Set(std::string_view section, std::string_view key, std::string_view value) {
    Section* target = FindSection(section);
    if (!target) {
        sections_.push_back({std::string(section), {}});
        target = &sections_.back();
    }
    for (auto& [name, existing] : target->items) {
        if (EqualsNoCase(name, key)) { existing = std::string(value); return; }
    }
    target->items.emplace_back(std::string(key), std::string(value));
}

} // namespace peek
