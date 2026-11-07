#include "core/localization.h"

#include <windows.h>

namespace peek {
namespace {

constexpr Strings kEnglish{
    L"Ghost", L"Unlock", L"Focus",
    L"Waiting for a short process or window…", L"What just flashed past", L"Who is using this file?",
    L"What Windows UI is under the pointer?", L"Unknown process", L"from",
    L"Open location", L"Enter Focus", L"Drop a file here, or choose one", L"Choose file",
    L"No owner reported", L"owner(s) found", L"End process…",
    L"Inspecting — press Esc to stop", L"Start Inspect  ·  Ctrl+Shift+F", L"Move the pointer over a window or control.",
    L"Process", L"EXE", L"HWND", L"Class", L"Title", L"Rectangle", L"Control type", L"Name",
    L"AutomationId", L"Value", L"UIA bounds", L"Unavailable", L"Open process location",
    L"started", L"exited", L"window created", L"shown", L"hidden", L"destroyed",
    L"Open Peek", L"Exit", L"Choose a file to inspect", L"All files",
    L"End", L"Unsaved work in that process may be lost.", L"Peek — End process",
    L"Main window class could not be registered.", L"Main window could not be created.",
    L"Ctrl+Shift+G is already in use; Peek will still run from the tray.",
    L"Settings", L"What should Peek do when you close the window?",
    L"You can change this later from Settings in the tray menu.",
    L"Minimize to tray", L"Exit Peek", L"Remember my choice",
    L"Peek Settings", L"Close button behavior",
    L"Choose what happens when you click the window's close button.", L"Ask every time"
};

constexpr wchar_t kRegistryPath[] = L"Software\\Peek";
constexpr wchar_t kCloseBehaviorValue[] = L"CloseBehavior";

} // namespace

const Strings& GetStrings() { return kEnglish; }

CloseBehavior LoadCloseBehavior() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, kRegistryPath, kCloseBehaviorValue, RRF_RT_REG_DWORD,
                     nullptr, &value, &size) == ERROR_SUCCESS && value <= 2) {
        return static_cast<CloseBehavior>(value);
    }
    return CloseBehavior::Ask;
}

void SaveCloseBehavior(CloseBehavior behavior) {
    const DWORD value = static_cast<DWORD>(behavior);
    RegSetKeyValueW(HKEY_CURRENT_USER, kRegistryPath, kCloseBehaviorValue, REG_DWORD,
                    &value, sizeof(value));
}

} // namespace peek
