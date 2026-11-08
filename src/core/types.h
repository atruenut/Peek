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
    std::wstring control_type;       // localized control type, e.g. "button"
    std::wstring class_name;         // UIA ClassName property
    std::wstring framework;          // UIA FrameworkId, e.g. "Win32", "WPF", "Chrome"
    std::wstring name;
    std::wstring value;
    bool value_available{};          // a value pattern exists and was readable
    bool value_hidden{};             // the element is a password field; the value is deliberately not read
    bool enabled{true};
    bool offscreen{};
    RECT bounds{};                   // UIA BoundingRectangle in screen pixels; valid only if bounds_valid
    bool bounds_valid{};
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
    DWORD app_type{};                // RM_APP_TYPE (3 = service, 4 = Explorer, 1000 = critical)
    DWORD app_status{};
    bool restartable{};
};

struct FocusSnapshot {
    WindowInfo window;
    ProcessInfo process;
    AutomationInfo automation;
    POINT cursor{};
    bool has_cursor{};
    RECT highlight{};                // what the overlay should outline: UIA bounds, else window bounds
    bool highlight_from_uia{};
    bool element_alive{true};        // false once a pinned element no longer exists
};

// One node of the lazily loaded UI Automation hierarchy shown in the Focus tab.
struct UiaNode {
    int id{};
    int parent{-1};
    std::wstring name;
    std::wstring control_type;
    std::wstring automation_id;
    std::wstring class_name;
    RECT bounds{};
    bool has_children{true};         // unknown until expanded; assume true so an expander is shown
    bool is_selected{};              // the element that is being inspected
};

} // namespace peek
