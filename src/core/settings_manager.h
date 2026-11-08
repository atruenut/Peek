#pragma once

#include "core/settings.h"

#include <functional>
#include <mutex>
#include <string>

namespace peek {

// Owns the live Settings and the portable INI file behind them.
// The file is PeekII.ini next to the executable (portable use, e.g. from a USB drive). If that folder is
// not writable the file lives in %APPDATA%\PeekII instead. Thread safe.
class SettingsManager {
public:
    // `explicit_path` (from --config) overrides the location when non-empty.
    explicit SettingsManager(const std::wstring& explicit_path = {});

    [[nodiscard]] const std::wstring& Path() const { return path_; }
    [[nodiscard]] bool IsPortable() const { return portable_; }

    [[nodiscard]] Settings Get() const;
    // Applies `mutate` to a copy, sanitizes it, stores it and writes the file. Returns false if the file
    // could not be written (the in-memory settings are still updated); `last_error` then explains why.
    bool Update(const std::function<void(Settings&)>& mutate);
    bool Save();
    void Reset();
    [[nodiscard]] std::wstring LastError() const;

private:
    void Load();
    bool WriteSettingsFile(const std::string& text, std::wstring& error) const;

    std::wstring path_;
    bool portable_{true};
    mutable std::mutex mutex_;
    Settings settings_;
    std::wstring last_error_;
};

} // namespace peek
