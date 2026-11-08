#pragma once

#include "core/categories.h"

#include <windows.h>
#include <chrono>
#include <cstdint>
#include <string>

namespace peek {

[[nodiscard]] inline ULONGLONG FileTimeToU64(const FILETIME& value) {
    return (static_cast<ULONGLONG>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
}

// A process is identified by PID *and* creation time, because Windows reuses PIDs.
// A zero start time means "unknown" and matches any start time for the same PID.
struct ProcessIdentity {
    DWORD pid{};
    ULONGLONG start{};

    [[nodiscard]] bool Known() const { return pid != 0 && start != 0; }
    [[nodiscard]] bool Matches(const ProcessIdentity& other) const {
        if (pid == 0 || pid != other.pid) return false;
        return start == 0 || other.start == 0 || start == other.start;
    }
};

struct ProcessInfo {
    DWORD pid{};
    DWORD parent_pid{};
    std::wstring name;
    std::wstring exe_path;
    std::wstring command_line;
    std::wstring parent_name;
    std::wstring parent_path;
    FILETIME start_time{};
    bool elevated{};
    bool accessible{};

    [[nodiscard]] ProcessIdentity Identity() const { return {pid, FileTimeToU64(start_time)}; }
};

struct WindowInfo {
    HWND hwnd{};
    DWORD pid{};
    std::wstring class_name;
    std::wstring title;
    RECT bounds{};
};

struct AutomationInfo {
    bool available{};
    std::wstring automation_id;
    std::wstring control_type;
    std::wstring name;
    std::wstring value;
    RECT bounds{};
};

// Raw capture kinds. Several kinds map to one user-facing category.
enum class GhostKind {
    ProcessStarted,
    ProcessExited,
    WindowCreated,
    WindowShown,
    WindowHidden,
    WindowDestroyed,
};

[[nodiscard]] constexpr EventCategory CategoryOf(GhostKind kind) {
    switch (kind) {
        case GhostKind::ProcessStarted: return EventCategory::ProcessStarted;
        case GhostKind::ProcessExited: return EventCategory::ProcessTerminated;
        case GhostKind::WindowCreated:
        case GhostKind::WindowShown: return EventCategory::WindowAppeared;
        case GhostKind::WindowHidden:
        case GhostKind::WindowDestroyed: return EventCategory::WindowDisappeared;
    }
    return EventCategory::ProcessStarted;
}

struct GhostEvent {
    std::uint64_t sequence{};
    std::uint64_t lifecycle_id{};   // events of one process lifetime share an id
    GhostKind kind{};
    std::chrono::system_clock::time_point timestamp{};
    ProcessInfo process;
    WindowInfo window;
    std::chrono::milliseconds lifetime{};
    bool has_lifetime{};
};

struct LockOwner {
    ProcessInfo process;
    std::wstring application_name;
    std::wstring service_name;
    DWORD app_status{};
    bool restartable{};
};

struct FocusSnapshot {
    WindowInfo window;
    ProcessInfo process;
    AutomationInfo automation;
    POINT cursor{};
};

} // namespace peek
