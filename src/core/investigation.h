#pragma once

// The shared investigation model. Ghost events, file locks and Focus captures all end up identifying a
// process; this turns a process identity (PID + creation time) into the same set of facts everywhere:
// status, parent, running children, windows and the Ghost events that mention it.

#include "core/event_store.h"
#include "core/process_resolver.h"
#include "core/types.h"

#include <string>
#include <vector>

namespace peek {

struct ProcessDetails {
    ProcessIdentity identity;
    ProcessInfo info;                       // best available information (live process, else last Ghost event)
    bool running{};
    bool from_events_only{};                // the process has ended; details come from recorded events
    bool parent_known{};
    bool parent_running{};                  // the parent still exists *and* is the same process (PID not reused)
    ProcessInfo parent;
    std::vector<ProcessInfo> children;      // children that are running now
    std::vector<WindowInfo> windows;        // visible top-level windows
    std::vector<GhostEvent> events;         // recorded events for this process, newest first
};

// Gathers details. Reads a process snapshot and walks windows, so call it on user action, not in a loop.
// `command_line` loads the command line (a WMI query) and is therefore opt-in.
[[nodiscard]] ProcessDetails DescribeProcess(const ProcessResolver& resolver, const EventStore& events,
                                             const ProcessIdentity& identity, bool command_line);

// Plain-text report for the details pane and the clipboard.
[[nodiscard]] std::wstring FormatProcessDetails(const ProcessDetails& details);

} // namespace peek
