#pragma once

#include <windows.h>

#include <string>

namespace peek {

// Explorer context menu entries, registered per user under HKCU\Software\Classes (no administrator rights,
// nothing written outside the registry, fully removable). These are classic shell verbs: on Windows 11
// they appear under "Show more options". Selecting several files starts the command once per file; the
// running PeekII instance merges those requests into one investigation (see App).
class ShellIntegration {
public:
    struct Status {
        bool registered{};          // all verbs exist
        bool points_here{};         // and they launch this executable
        std::wstring registered_exe;
    };

    [[nodiscard]] static Status Query(const std::wstring& exe_path);
    static bool Register(const std::wstring& exe_path, std::wstring& error);
    static bool Unregister(std::wstring& error);
};

} // namespace peek
