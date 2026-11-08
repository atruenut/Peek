#pragma once

// Minimal portable INI reader/writer. UTF-8, no Win32 dependency, so it can be unit tested anywhere.
// Section and key names are matched case-insensitively (ASCII). Comments start with ';' or '#'.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace peek {

class IniFile {
public:
    void Parse(std::string_view text);
    [[nodiscard]] std::string Serialize() const;

    [[nodiscard]] bool Has(std::string_view section, std::string_view key) const;
    [[nodiscard]] std::string Get(std::string_view section, std::string_view key,
                                  std::string_view fallback = {}) const;
    void Set(std::string_view section, std::string_view key, std::string_view value);

private:
    struct Section {
        std::string name;
        std::vector<std::pair<std::string, std::string>> items;
    };
    Section* FindSection(std::string_view name);
    const Section* FindSection(std::string_view name) const;

    std::vector<Section> sections_;
};

} // namespace peek
