#pragma once

#include "core/process_resolver.h"
#include "core/types.h"

#include <atomic>
#include <string>
#include <vector>

namespace peek {

struct UnlockLimits {
    int max_depth{2};      // how many directory levels below a selected folder are scanned
    int max_files{500};    // upper bound on files checked in one investigation
};

struct FileFacts {
    std::wstring path;
    bool exists{};
    bool is_directory{};
    ULONGLONG size{};
    FILETIME modified{};
    DWORD attributes{};
};

// One process that holds at least one of the investigated files, with the files it holds.
struct LockGroup {
    LockOwner owner;
    std::vector<std::wstring> files;
};

struct UnlockReport {
    std::vector<std::wstring> targets;     // what the user asked about
    std::vector<FileFacts> facts;          // one entry per target
    std::vector<LockGroup> groups;         // grouped by process identity, largest first
    std::vector<std::wstring> messages;    // access errors and limit warnings, in plain English
    std::size_t files_checked{};
    bool truncated{};                      // the file limit was reached
    bool cancelled{};
};

enum class CloseResult { Requested, NoWindows, NotAllowed, ProcessGone };

// Finds processes that hold files open, using the Restart Manager. Nothing here closes or kills anything
// except RequestClose, which only asks a process's own windows to close the way a user would.
class UnlockService {
public:
    explicit UnlockService(ProcessResolver& resolver) : resolver_(resolver) {}

    // Blocking; run it on a worker thread. `cancel` may be null. Directories are scanned up to `limits`.
    [[nodiscard]] UnlockReport Investigate(const std::vector<std::wstring>& paths, const UnlockLimits& limits,
                                           const std::atomic_bool* cancel = nullptr) const;

    // Services, Explorer and critical system processes are never offered for closing or ending.
    [[nodiscard]] static bool IsProtected(const LockOwner& owner);
    // Posts WM_CLOSE to the process's visible top-level windows. The application may still prompt or refuse.
    [[nodiscard]] static CloseResult RequestClose(const LockOwner& owner, int* windows_notified = nullptr);

private:
    ProcessResolver& resolver_;
};

} // namespace peek
