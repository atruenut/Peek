#pragma once

#include "core/event_store.h"
#include "core/process_resolver.h"
#include "core/settings.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace peek {

struct GhostStatus {
    bool running{};
    bool window_hooks{};       // WinEvent hooks installed
    bool wmi{};                // WMI process start/stop subscription active
    bool polling{};            // snapshot fallback active instead of WMI
    std::wstring message;      // human-readable degradation note, empty when everything works
};

// Captures process lifecycle and window events into an EventStore. No UI code lives here.
//
// Threads (all owned and joined by this class):
//   window thread - installs the WinEvent hooks and pumps their messages. Out-of-context hook callbacks
//                   run on the thread that installed them, so this keeps them away from the UI thread.
//   WMI thread    - blocks on Win32_ProcessStartTrace / Win32_ProcessStopTrace notifications.
//   poll thread   - only started when WMI is unavailable; samples the PID list every 250 ms.
// The callback `notify` is invoked from these threads and must be cheap and thread safe.
class GhostMonitor {
public:
    using Notify = std::function<void()>;

    GhostMonitor(EventStore& store, ProcessResolver& resolver, Notify notify);
    ~GhostMonitor();
    GhostMonitor(const GhostMonitor&) = delete;
    GhostMonitor& operator=(const GhostMonitor&) = delete;

    // Starts capture threads and returns immediately; use Status() for the outcome.
    bool Start();
    void Stop();
    [[nodiscard]] bool Running() const { return running_; }
    // Waits until the capture sources have finished starting (or failed). Used by the self-test.
    bool WaitUntilReady(std::chrono::milliseconds timeout);

    void SetIgnoreNoise(bool ignore) { ignore_noise_ = ignore; }
    // Off: lightweight. 30s/5m: detailed capture until the period ends. Continuous: detailed until changed.
    void SetTrace(TraceMode mode);
    [[nodiscard]] bool TraceActive() const;
    [[nodiscard]] int TraceSecondsLeft() const;   // -1 when continuous or off

    [[nodiscard]] GhostStatus Status() const;

private:
    struct WindowState {
        WindowInfo info;
        bool visible{};
        std::chrono::system_clock::time_point shown_at{};
    };
    struct CachedProcess {
        ProcessInfo info;
        ULONGLONG checked_tick{};
    };

    static void CALLBACK WinEventCallback(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                          LONG object_id, LONG child_id, DWORD event_thread, DWORD event_time);
    void WindowLoop();
    void OnWindowEvent(DWORD event, HWND hwnd);
    void EmitWindow(GhostKind kind, const WindowInfo& window, bool has_lifetime, std::chrono::milliseconds lifetime);
    [[nodiscard]] bool IsNoise(const WindowInfo& window) const;
    [[nodiscard]] ProcessInfo ProcessFor(DWORD pid);

    void WmiLoop();
    void PollLoop();
    void EnsurePollThread();
    void Emit(GhostEvent event);
    void SetMessage(const std::wstring& message);

    EventStore& store_;
    ProcessResolver& resolver_;
    Notify notify_;

    std::atomic_bool running_{false};
    std::atomic_bool stopping_{false};
    std::atomic_bool ignore_noise_{true};
    std::atomic_bool hooks_ok_{false};
    std::atomic_bool wmi_ok_{false};
    std::atomic_bool polling_{false};
    std::atomic<int> trace_mode_{static_cast<int>(TraceMode::Off)};
    std::atomic<long long> trace_deadline_ms_{0};   // steady-clock milliseconds; 0 = none

    HANDLE stop_event_{};
    HANDLE window_ready_{};
    HANDLE wmi_ready_{};
    std::thread window_thread_;
    std::thread wmi_thread_;
    std::thread poll_thread_;
    std::mutex thread_mutex_;   // guards poll_thread_ start/join

    mutable std::mutex status_mutex_;
    std::wstring message_;

    // Touched only by the window thread.
    std::unordered_map<HWND, WindowState> windows_;
    HWINEVENTHOOK create_destroy_hook_{};
    HWINEVENTHOOK show_hide_hook_{};

    std::mutex process_cache_mutex_;
    std::unordered_map<DWORD, CachedProcess> process_cache_;

    static GhostMonitor* instance_;
};

} // namespace peek
