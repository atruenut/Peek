#include "core/investigation.h"

#include "core/format.h"
#include "core/strings.h"
#include "core/window_util.h"

#include <tlhelp32.h>

#include <algorithm>
#include <sstream>

namespace peek {
namespace {

std::wstring FileTimeText(const FILETIME& ft) {
    if (FileTimeToU64(ft) == 0) return str::kUnavailable;
    FILETIME local{};
    SYSTEMTIME st{};
    if (!FileTimeToLocalFileTime(&ft, &local) || !FileTimeToSystemTime(&local, &st)) return str::kUnavailable;
    wchar_t text[48]{};
    swprintf(text, std::size(text), L"%04u-%02u-%02u %02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return text;
}

} // namespace

ProcessDetails DescribeProcess(const ProcessResolver& resolver, const EventStore& events,
                               const ProcessIdentity& identity, bool command_line) {
    ProcessDetails details;
    details.identity = identity;
    details.info.pid = identity.pid;

    // Recorded events for exactly this process lifetime (PID plus creation time when both are known).
    for (const GhostEvent& event : events.Snapshot()) {
        if (event.process.Identity().Matches(identity)) details.events.push_back(event);
    }

    details.running = ProcessResolver::IsRunning(identity);
    if (details.running) {
        details.info = resolver.Resolve(identity.pid, 0, {}, command_line);
    }
    // Fill gaps (and everything, for ended processes) from the newest event that knows more.
    for (const GhostEvent& event : details.events) {
        const ProcessInfo& known = event.process;
        ProcessInfo& info = details.info;
        if (info.name.empty()) info.name = known.name;
        if (info.exe_path.empty()) info.exe_path = known.exe_path;
        if (info.command_line.empty()) info.command_line = known.command_line;
        if (!info.parent_pid) info.parent_pid = known.parent_pid;
        if (info.parent_name.empty()) info.parent_name = known.parent_name;
        if (FileTimeToU64(info.start_time) == 0) info.start_time = known.start_time;
    }
    details.from_events_only = !details.running && !details.events.empty();

    // Parent: only trust it if it was already running when the child started; otherwise the PID was reused.
    if (details.info.parent_pid != 0) {
        details.parent_known = true;
        ProcessInfo parent = resolver.Resolve(details.info.parent_pid);
        const ULONGLONG child_start = FileTimeToU64(details.info.start_time);
        const ULONGLONG parent_start = FileTimeToU64(parent.start_time);
        details.parent_running = parent_start != 0 && (child_start == 0 || parent_start <= child_start);
        if (details.parent_running) details.parent = std::move(parent);
        else {
            details.parent.pid = details.info.parent_pid;
            details.parent.name = details.info.parent_name;
        }
    }

    if (details.running) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            const ULONGLONG self_start = FileTimeToU64(details.info.start_time);
            for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
                if (entry.th32ParentProcessID != identity.pid || entry.th32ProcessID == identity.pid) continue;
                ProcessInfo child = resolver.Resolve(entry.th32ProcessID, entry.th32ParentProcessID, entry.szExeFile);
                const ULONGLONG child_start = FileTimeToU64(child.start_time);
                if (self_start != 0 && child_start != 0 && child_start < self_start) continue;   // PID reuse: not our child
                details.children.push_back(std::move(child));
            }
            CloseHandle(snapshot);
        }
        details.windows = TopLevelWindows(identity.pid, true);
    }
    return details;
}

std::wstring FormatProcessDetails(const ProcessDetails& d) {
    const ProcessInfo& p = d.info;
    std::wstringstream out;
    out << str::kPropProcess << L": " << (p.name.empty() ? std::wstring(str::kUnknownProcess) : p.name) << L"\r\n"
        << str::kPropPid << L": " << p.pid << L"\r\n"
        << L"Status: " << (d.running ? L"running" : (d.from_events_only ? L"ended (details from recorded events)" : L"not running")) << L"\r\n"
        << L"Parent PID: " << (p.parent_pid ? std::to_wstring(p.parent_pid) : std::wstring(str::kUnavailable));
    if (d.parent_known) {
        out << L"  " << (d.parent.name.empty() ? L"" : d.parent.name)
            << (d.parent_running ? L"  (running)" : L"  (ended, or its PID was reused)");
    }
    out << L"\r\n"
        << L"Started: " << FileTimeText(p.start_time) << L"\r\n"
        << str::kPropExe << L": " << (p.exe_path.empty() ? std::wstring(str::kUnavailable) : p.exe_path) << L"\r\n"
        << L"Command line: " << (p.command_line.empty() ? std::wstring(L"not loaded or unavailable") : p.command_line) << L"\r\n";
    if (!d.children.empty()) {
        out << L"\r\nRunning child processes (" << d.children.size() << L"):\r\n";
        for (const ProcessInfo& c : d.children) out << L"  " << c.name << L"  (PID " << c.pid << L")\r\n";
    }
    if (!d.windows.empty()) {
        out << L"\r\nWindows (" << d.windows.size() << L"):\r\n";
        for (const WindowInfo& w : d.windows) {
            out << L"  " << HexHandle(w.hwnd) << L"  " << w.class_name << L"  " << (w.title.empty() ? L"(no title)" : w.title) << L"\r\n";
        }
    }
    if (!d.events.empty()) {
        out << L"\r\nRecorded events (" << d.events.size() << L"):\r\n";
        for (const GhostEvent& e : d.events) out << L"  " << FormatEventSummary(e) << L"\r\n";
    }
    return out.str();
}

} // namespace peek
