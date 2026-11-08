#include "core/settings_manager.h"

#include "core/format.h"

#include <shlobj.h>
#include <windows.h>

#include <vector>

namespace peek {
namespace {

std::wstring ExeDirectory() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    std::wstring path(buffer.data(), length);
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring{} : path.substr(0, slash);
}

bool DirectoryWritable(const std::wstring& directory) {
    const std::wstring probe = directory + L"\\.peek-write-test";
    HANDLE file = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
    return true;
}

bool ReadWholeFile(const std::wstring& path, std::string& out) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart > 1024 * 1024) { CloseHandle(file); return false; }
    out.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    const bool ok = out.empty() || (ReadFile(file, out.data(), static_cast<DWORD>(out.size()), &read, nullptr) && read == out.size());
    CloseHandle(file);
    return ok;
}

std::wstring ResolvePath(const std::wstring& explicit_path, bool& portable) {
    portable = true;
    if (!explicit_path.empty()) {
        portable = false;
        return explicit_path;
    }
    const std::wstring exe_dir = ExeDirectory();
    if (!exe_dir.empty() && DirectoryWritable(exe_dir)) return exe_dir + L"\\Peek.ini";
    portable = false;
    wchar_t* appdata = nullptr;
    std::wstring directory;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata)) && appdata) {
        directory = std::wstring(appdata) + L"\\Peek";
    }
    if (appdata) CoTaskMemFree(appdata);
    if (directory.empty()) return exe_dir + L"\\Peek.ini";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory + L"\\Peek.ini";
}

} // namespace

SettingsManager::SettingsManager(const std::wstring& explicit_path) {
    path_ = ResolvePath(explicit_path, portable_);
    Load();
}

void SettingsManager::Load() {
    std::string text;
    Settings loaded;
    if (ReadWholeFile(path_, text)) {
        IniFile ini;
        ini.Parse(text);
        LoadFromIni(ini, loaded);
    } else {
        Sanitize(loaded);
    }
    std::lock_guard lock(mutex_);
    settings_ = loaded;
}

Settings SettingsManager::Get() const {
    std::lock_guard lock(mutex_);
    return settings_;
}

std::wstring SettingsManager::LastError() const {
    std::lock_guard lock(mutex_);
    return last_error_;
}

bool SettingsManager::Update(const std::function<void(Settings&)>& mutate) {
    {
        std::lock_guard lock(mutex_);
        Settings next = settings_;
        mutate(next);
        Sanitize(next);
        settings_ = next;
    }
    return Save();
}

void SettingsManager::Reset() {
    {
        std::lock_guard lock(mutex_);
        settings_ = Settings{};
        Sanitize(settings_);
    }
    Save();
}

bool SettingsManager::WriteSettingsFile(const std::string& text, std::wstring& error) const {
    const std::wstring temporary = path_ + L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = Win32ErrorText(GetLastError()); return false; }
    DWORD written = 0;
    const bool wrote = ::WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size();
    const DWORD write_error = GetLastError();
    CloseHandle(file);
    if (!wrote) { DeleteFileW(temporary.c_str()); error = Win32ErrorText(write_error); return false; }
    if (MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    const DWORD move_error = GetLastError();
    DeleteFileW(temporary.c_str());
    error = Win32ErrorText(move_error);
    return false;
}

bool SettingsManager::Save() {
    std::string text;
    {
        std::lock_guard lock(mutex_);
        text = "; Peek settings. Safe to edit by hand while Peek is not running.\r\n"
               "; Invalid values are repaired when the file is loaded.\r\n\r\n" + ToIni(settings_).Serialize();
    }
    std::wstring error;
    const bool ok = WriteSettingsFile(text, error);
    std::lock_guard lock(mutex_);
    last_error_ = ok ? std::wstring{} : path_ + L": " + error;
    return ok;
}

} // namespace peek
