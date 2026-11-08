#include "app.h"
#include "cli.h"
#include "self_test.h"
#include "shell/shell_integration.h"

#include <windows.h>
#include <shellapi.h>

namespace {
std::wstring SelfPath() {
    wchar_t path[MAX_PATH * 2]{};
    GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    return path;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    peek::CommandLine cl = peek::ParseCommandLine(argc, argv);
    if (argv) LocalFree(argv);

    if (!cl.error.empty()) {
        MessageBoxW(nullptr, (cl.error + L"\n\n" + peek::UsageText()).c_str(), L"PeekII", MB_ICONERROR);
        return 2;
    }
    switch (cl.action) {
        case peek::CliAction::Help:
            MessageBoxW(nullptr, peek::UsageText(), L"PeekII", MB_ICONINFORMATION);
            return 0;
        case peek::CliAction::SelfTest:
            return peek::RunSelfTest(cl.report_path);
        case peek::CliAction::RegisterShell: {
            std::wstring error;
            const bool ok = peek::ShellIntegration::Register(SelfPath(), error);
            MessageBoxW(nullptr, ok ? L"Explorer menu entries were added for the current user." : error.c_str(), L"PeekII", ok ? MB_ICONINFORMATION : MB_ICONERROR);
            return ok ? 0 : 1;
        }
        case peek::CliAction::UnregisterShell: {
            std::wstring error;
            const bool ok = peek::ShellIntegration::Unregister(error);
            MessageBoxW(nullptr, ok ? L"Explorer menu entries were removed." : error.c_str(), L"PeekII", ok ? MB_ICONINFORMATION : MB_ICONERROR);
            return ok ? 0 : 1;
        }
        default: break;
    }
    if (peek::App::ForwardToRunningInstance(cl)) return 0;   // another PeekII is running; it handles the request
    peek::App app(cl);
    return app.Run(instance);
}
