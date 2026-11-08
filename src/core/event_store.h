#pragma once

#include "core/settings.h"
#include "core/types.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace peek {

struct EventFilter {
    std::wstring text;                           // case-insensitive; process name, path, command line, window title/class
    std::uint32_t categories{kAllCategories};    // bitmask of CategoryBit()
    StateFilter state{StateFilter::Any};
    std::chrono::seconds max_age{0};             // 0 = no limit

    [[nodiscard]] bool IsEmpty() const {
        return text.empty() && categories == kAllCategories && state == StateFilter::Any && max_age.count() == 0;
    }
};

// Thread-safe, bounded, newest-first event history.
//
// Events are never dropped by time or by the HUD going away: they stay until displaced by newer events
// (the retention limit), dismissed, or cleared. Events that belong to the same process lifetime share a
// lifecycle id; a lifetime is identified by PID plus process creation time so a reused PID starts a new one.
class EventStore {
public:
    using RunningCheck = std::function<bool(const ProcessInfo&)>;

    explicit EventStore(std::size_t capacity = 500);

    void SetCapacity(std::size_t capacity);
    [[nodiscard]] std::size_t Capacity() const;
    [[nodiscard]] std::size_t Size() const;

    void Add(GhostEvent event);
    bool Dismiss(std::uint64_t sequence);
    void Clear();

    // Newest first. `limit` caps the number of returned events.
    [[nodiscard]] std::vector<GhostEvent> Snapshot(std::size_t limit = static_cast<std::size_t>(-1)) const;
    // Newest first, filtered. `running` is only consulted when the filter asks for a run state.
    [[nodiscard]] std::vector<GhostEvent> Query(const EventFilter& filter, std::size_t limit,
                                                const RunningCheck& running = {}) const;
    [[nodiscard]] std::optional<GhostEvent> Find(std::uint64_t sequence) const;
    [[nodiscard]] std::optional<GhostEvent> Latest() const;
    // All retained events of one process lifetime, oldest first.
    [[nodiscard]] std::vector<GhostEvent> Lifecycle(std::uint64_t lifecycle_id) const;

    // Increments on every change; the UI uses it to skip redundant repaints.
    [[nodiscard]] std::uint64_t Version() const { return version_.load(std::memory_order_relaxed); }

private:
    struct ActiveLifecycle {
        std::uint64_t id{};
        ProcessIdentity identity;
        bool has_start{};
        std::chrono::system_clock::time_point started{};
        ProcessInfo start_process;
    };

    [[nodiscard]] static bool MatchesStatic(const GhostEvent& event, const EventFilter& filter,
                                            const std::wstring& needle_lower,
                                            std::chrono::system_clock::time_point now);
    void Trim();

    mutable std::mutex mutex_;
    std::size_t capacity_;
    std::deque<GhostEvent> events_;
    std::unordered_map<DWORD, ActiveLifecycle> active_;
    std::uint64_t next_sequence_{1};
    std::uint64_t next_lifecycle_{1};
    std::atomic<std::uint64_t> version_{0};
};

} // namespace peek
