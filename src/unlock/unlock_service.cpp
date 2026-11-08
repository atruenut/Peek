#include "unlock/unlock_service.h"

#include "core/format.h"
#include "core/window_util.h"

#include <restartmanager.h>

#include <algorithm>
#include <array>
#include <map>

namespace peek {
namespace {

std::wstring RestartManagerError(DWORD status) {
    switch (status) {
        case ERROR_ACCESS_DENIED:
            return L"Access denied. Peek can only see locks held by processes it is allowed to query. "
                   L"Run Peek as administrator to inspect files used by elevated programs.";
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND: return L"The file or folder no longer exists.";
        case ERROR_MAX_SESSIONS_REACHED:
            return L"Windows has too many Restart Manager sessions open right now. Try again in a moment.";
        case ERROR_INVALID_NAME: return L"The path is not valid.";
        default: return Win32ErrorText(status);
    }
}

// Collects the Restart Manager's view of one file. Returns the Win32 status; owners are filled on success.
DWORD QueryOwners(const std::wstring& path, std::vector<RM_PROCESS_INFO>& owners) {
    owners.clear();
    DWORD session = 0;
    std::array<wchar_t, CCH_RM_SESSION_KEY + 1> key{};
    DWORD status = RmStartSession(&session, 0, key.data());
    if (status != ERROR_SUCCESS) return status;

    const wchar_t* files[] = {path.c_str()};
    status = RmRegisterResources(session, 1, files, 0, nullptr, 0, nullptr);
    if (status == ERROR_SUCCESS) {
        UINT needed = 0;
        UINT count = 0;
        DWORD reasons = 0;
        // The set of owners can change between the sizing call and the real one, so retry a few times.
        for (int attempt = 0; attempt < 4; ++attempt) {
            status = RmGetList(session, &needed, &count, nullptr, &reasons);
            if (status == ERROR_SUCCESS) break;           // nothing holds the file
            if (status != ERROR_MORE_DATA) break;
            owners.assign(needed, RM_PROCESS_INFO{});
            count = needed;
            status = RmGetList(session, &needed, &count, owners.data(), &reasons);
            if (status == ERROR_SUCCESS) { owners.resize(count); break; }
            if (status != ERROR_MORE_DATA) break;
        }
        if (status != ERROR_SUCCESS) owners.clear();
    }
    RmEndSession(session);
    return status;
}

FileFacts ReadFacts(const std::wstring& path) {
    FileFacts facts;
    facts.path = path;
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        facts.exists = true;
        facts.attributes = data.dwFileAttributes;
        facts.is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        facts.size = (static_cast<ULONGLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
        facts.modified = data.ftLastWriteTime;
    }
    return facts;
}

// Breadth-first, bounded directory scan. Reparse points (junctions, symlinks) are never followed, which
// also prevents cycles. Appends files to `out` until `max_files` is reached.
void CollectFiles(const std::wstring& root, const UnlockLimits& limits, std::vector<std::wstring>& out,
                  bool& truncated, std::vector<std::wstring>& messages, const std::atomic_bool* cancel) {
    struct Level { std::wstring path; int depth; };
    std::vector<Level> queue{{root, 0}};
    for (std::size_t head = 0; head < queue.size(); ++head) {
        if (cancel && cancel->load()) return;
        const Level level = queue[head];
        WIN32_FIND_DATAW data{};
        HANDLE find = FindFirstFileExW((level.path + L"\\*").c_str(), FindExInfoBasic, &data,
                                       FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (find == INVALID_HANDLE_VALUE) {
            const DWORD error = GetLastError();
            if (error != ERROR_FILE_NOT_FOUND) messages.push_back(level.path + L": " + RestartManagerError(error));
            continue;
        }
        do {
            const std::wstring name = data.cFileName;
            if (name == L"." || name == L"..") continue;
            const std::wstring full = level.path + L"\\" + name;
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 && level.depth < limits.max_depth) {
                    queue.push_back({full, level.depth + 1});
                }
            } else {
                if (static_cast<int>(out.size()) >= limits.max_files) { truncated = true; FindClose(find); return; }
                out.push_back(full);
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
}

} // namespace

UnlockReport UnlockService::Investigate(const std::vector<std::wstring>& paths, const UnlockLimits& limits,
                                        const std::atomic_bool* cancel) const {
    UnlockReport report;
    report.targets = paths;

    std::vector<std::wstring> files;
    for (const std::wstring& path : paths) {
        FileFacts facts = ReadFacts(path);
        report.facts.push_back(facts);
        if (!facts.exists) {
            report.messages.push_back(path + L": the file or folder does not exist.");
            continue;
        }
        if (facts.is_directory) {
            CollectFiles(path, limits, files, report.truncated, report.messages, cancel);
        } else if (static_cast<int>(files.size()) < limits.max_files) {
            files.push_back(path);
        } else {
            report.truncated = true;
        }
    }
    if (report.truncated) {
        report.messages.push_back(L"Only the first " + std::to_wstring(limits.max_files) +
                                  L" files were checked. Raise the file limit in Settings to scan more.");
    }

    // Group by process identity (PID plus creation time) so a reused PID never merges two processes.
    std::map<std::pair<DWORD, ULONGLONG>, std::size_t> index;
    std::map<DWORD, ProcessInfo> resolved;           // one process lookup per PID per investigation
    bool access_reported = false;

    for (const std::wstring& file : files) {
        if (cancel && cancel->load()) { report.cancelled = true; break; }
        ++report.files_checked;
        std::vector<RM_PROCESS_INFO> owners;
        const DWORD status = QueryOwners(file, owners);
        if (status != ERROR_SUCCESS) {
            if (status == ERROR_ACCESS_DENIED) {
                if (access_reported) continue;
                access_reported = true;
            }
            report.messages.push_back(file + L": " + RestartManagerError(status));
            continue;
        }
        for (const RM_PROCESS_INFO& info : owners) {
            const DWORD pid = info.Process.dwProcessId;
            auto cached = resolved.find(pid);
            if (cached == resolved.end()) {
                ProcessInfo process = resolver_.Resolve(pid);
                process.start_time = info.Process.ProcessStartTime;   // Restart Manager's value identifies the lock owner
                if (process.name.empty()) process.name = info.strAppName;
                cached = resolved.emplace(pid, std::move(process)).first;
            }
            const auto key = std::make_pair(pid, FileTimeToU64(info.Process.ProcessStartTime));
            auto found = index.find(key);
            if (found == index.end()) {
                LockGroup group;
                group.owner.process = cached->second;
                group.owner.application_name = info.strAppName;
                group.owner.service_name = info.strServiceShortName;
                group.owner.app_type = static_cast<DWORD>(info.ApplicationType);
                group.owner.app_status = info.AppStatus;
                group.owner.restartable = info.bRestartable != FALSE;
                report.groups.push_back(std::move(group));
                found = index.emplace(key, report.groups.size() - 1).first;
            }
            report.groups[found->second].files.push_back(file);
        }
    }

    std::stable_sort(report.groups.begin(), report.groups.end(),
                     [](const LockGroup& a, const LockGroup& b) { return a.files.size() > b.files.size(); });
    return report;
}

bool UnlockService::IsProtected(const LockOwner& owner) {
    if (owner.process.pid == 0 || owner.process.pid == GetCurrentProcessId()) return true;
    switch (owner.app_type) {
        case RmService:
        case RmExplorer:
        case RmCritical: return true;
        default: return false;
    }
}

CloseResult UnlockService::RequestClose(const LockOwner& owner, int* windows_notified) {
    if (windows_notified) *windows_notified = 0;
    if (IsProtected(owner)) return CloseResult::NotAllowed;
    if (!ProcessResolver::IsRunning(owner.process.Identity())) return CloseResult::ProcessGone;
    int notified = 0;
    for (const WindowInfo& window : TopLevelWindows(owner.process.pid, true)) {
        if (PostMessageW(window.hwnd, WM_CLOSE, 0, 0)) ++notified;
    }
    if (windows_notified) *windows_notified = notified;
    return notified > 0 ? CloseResult::Requested : CloseResult::NoWindows;
}

} // namespace peek
