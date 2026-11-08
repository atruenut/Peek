#include "shell/shell_integration.h"

#include "core/format.h"

#include <shlobj.h>

#include <vector>

namespace peek {
namespace {

struct Verb {
    const wchar_t* base;      // registry path under HKCU
    const wchar_t* key;       // verb key name
    const wchar_t* label;
    const wchar_t* argument;  // command line switch
};

// Each verb is registered for files ("*") and folders ("Directory").
const Verb kVerbs[] = {
    {L"Software\\Classes\\*\\shell", L"Peek.WhoIsLocking", L"Peek: Who is locking this?", L"--who-locks"},
    {L"Software\\Classes\\*\\shell", L"Peek.InspectFile", L"Peek: Inspect file", L"--inspect-file"},
    {L"Software\\Classes\\Directory\\shell", L"Peek.WhoIsLocking", L"Peek: Who is locking files in this folder?", L"--who-locks"},
    {L"Software\\Classes\\Directory\\shell", L"Peek.InspectFile", L"Peek: Inspect folder", L"--inspect-file"},
};

std::wstring KeyPath(const Verb& verb) { return std::wstring(verb.base) + L"\\" + verb.key; }

std::wstring CommandLineFor(const std::wstring& exe, const wchar_t* argument) {
    return L"\"" + exe + L"\" " + argument + L" \"%1\"";
}

bool SetDefault(HKEY key, const std::wstring& value) {
    return RegSetValueExW(key, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

bool SetValue(HKEY key, const wchar_t* name, const std::wstring& value) {
    return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

std::wstring ReadDefault(const std::wstring& path) {
    wchar_t buffer[2048]{};
    DWORD size = sizeof(buffer) - sizeof(wchar_t);
    if (RegGetValueW(HKEY_CURRENT_USER, path.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) return {};
    return buffer;
}

// Extracts the executable from `"C:\path\Peek.exe" --who-locks "%1"`.
std::wstring ExeFromCommand(const std::wstring& command) {
    if (command.size() < 2 || command[0] != L'"') return {};
    const std::size_t end = command.find(L'"', 1);
    return end == std::wstring::npos ? std::wstring{} : command.substr(1, end - 1);
}

} // namespace

ShellIntegration::Status ShellIntegration::Query(const std::wstring& exe_path) {
    Status status;
    status.registered = true;
    status.points_here = true;
    for (const Verb& verb : kVerbs) {
        const std::wstring command = ReadDefault(KeyPath(verb) + L"\\command");
        if (command.empty()) { status.registered = false; status.points_here = false; continue; }
        const std::wstring exe = ExeFromCommand(command);
        if (status.registered_exe.empty()) status.registered_exe = exe;
        if (_wcsicmp(exe.c_str(), exe_path.c_str()) != 0) status.points_here = false;
    }
    if (!status.registered) status.points_here = false;
    return status;
}

bool ShellIntegration::Register(const std::wstring& exe_path, std::wstring& error) {
    error.clear();
    for (const Verb& verb : kVerbs) {
        HKEY key{};
        if (RegCreateKeyExW(HKEY_CURRENT_USER, KeyPath(verb).c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
            error = L"The registry key could not be created: " + KeyPath(verb);
            Unregister(error);   // do not leave a half-registered state behind
            error = L"The Explorer menu entries could not be registered (registry write failed).";
            return false;
        }
        bool ok = SetDefault(key, verb.label);
        ok = SetValue(key, L"Icon", L"\"" + exe_path + L"\",0") && ok;
        ok = SetValue(key, L"MultiSelectModel", L"Document") && ok;   // one launch per item; Peek merges them
        RegCloseKey(key);

        HKEY command{};
        if (ok && RegCreateKeyExW(HKEY_CURRENT_USER, (KeyPath(verb) + L"\\command").c_str(), 0, nullptr, 0, KEY_WRITE, nullptr,
                                  &command, nullptr) == ERROR_SUCCESS) {
            ok = SetDefault(command, CommandLineFor(exe_path, verb.argument));
            RegCloseKey(command);
        } else {
            ok = false;
        }
        if (!ok) {
            std::wstring ignored;
            Unregister(ignored);
            error = L"The Explorer menu entries could not be registered (registry write failed).";
            return false;
        }
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool ShellIntegration::Unregister(std::wstring& error) {
    error.clear();
    bool ok = true;
    for (const Verb& verb : kVerbs) {
        const LSTATUS status = RegDeleteTreeW(HKEY_CURRENT_USER, KeyPath(verb).c_str());
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
            ok = false;
            error = L"The registry key could not be removed: " + KeyPath(verb) + L" (" + Win32ErrorText(static_cast<DWORD>(status)) + L")";
        }
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return ok;
}

} // namespace peek
