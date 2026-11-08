#pragma once

#include <string>
#include <vector>

namespace peek {

enum class CliAction {
    Run,               // normal start (tray, HUD, optional window)
    SelfTest,
    WhoLocks,          // --who-locks <path>      investigate locks on the given files
    InspectFile,       // --inspect-file <path>   same, with file details shown first
    RegisterShell,     // --register-shell
    UnregisterShell,   // --unregister-shell
    Help,
};

struct CommandLine {
    CliAction action{CliAction::Run};
    std::vector<std::wstring> paths;
    std::wstring config_path;      // --config <file>
    std::wstring report_path;      // --self-test [report]
    bool show_window{false};       // --show
    std::wstring error;            // non-empty when the arguments could not be understood
};

[[nodiscard]] CommandLine ParseCommandLine(int argc, wchar_t** argv);
[[nodiscard]] const wchar_t* UsageText();

} // namespace peek
