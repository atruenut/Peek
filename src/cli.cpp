#include "cli.h"

#include <string_view>

namespace peek {

const wchar_t* UsageText() {
    return L"Peek - Windows observation and investigation utility\n\n"
           L"Usage: Peek.exe [options]\n\n"
           L"  --show                    Open the Peek window at startup\n"
           L"  --config <file>           Use this settings file instead of Peek.ini next to the program\n"
           L"  --who-locks <path>        Find which processes are using a file or folder\n"
           L"  --inspect-file <path>     Show file details and which processes are using it\n"
           L"  --register-shell          Add the Explorer context menu entries for the current user\n"
           L"  --unregister-shell        Remove the Explorer context menu entries\n"
           L"  --self-test [report]      Run the built-in diagnostics and exit\n"
           L"  --help                    Show this text\n";
}

CommandLine ParseCommandLine(int argc, wchar_t** argv) {
    CommandLine result;
    for (int i = 1; i < argc; ++i) {
        const std::wstring_view arg = argv[i];
        auto next = [&](std::wstring& out) {
            if (i + 1 >= argc) { result.error = std::wstring(arg) + L" needs a value."; return false; }
            out = argv[++i];
            return true;
        };
        if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            result.action = CliAction::Help;
        } else if (arg == L"--show") {
            result.show_window = true;
        } else if (arg == L"--config") {
            if (!next(result.config_path)) return result;
        } else if (arg == L"--self-test") {
            result.action = CliAction::SelfTest;
            if (i + 1 < argc && argv[i + 1][0] != L'-') result.report_path = argv[++i];
        } else if (arg == L"--register-shell") {
            result.action = CliAction::RegisterShell;
        } else if (arg == L"--unregister-shell") {
            result.action = CliAction::UnregisterShell;
        } else if (arg == L"--who-locks" || arg == L"--inspect-file") {
            std::wstring path;
            if (!next(path)) return result;
            const CliAction wanted = arg == L"--who-locks" ? CliAction::WhoLocks : CliAction::InspectFile;
            // Explorer starts one process per selected item with the same switch; paths accumulate.
            if (result.action == CliAction::Run || result.action == wanted) {
                result.action = wanted;
                result.paths.push_back(path);
            }
        } else {
            result.error = L"Unknown option: " + std::wstring(arg);
            return result;
        }
    }
    return result;
}

} // namespace peek
