#include "core/event_store.h"

#include "core/format.h"

#include <algorithm>

namespace peek {
namespace {
constexpr std::size_t kMaxTrackedLifecycles = 8192;
}

EventStore::EventStore(std::size_t capacity) : capacity_(std::clamp<std::size_t>(capacity, 1, 100000)) {}

void EventStore::SetCapacity(std::size_t capacity) {
    std::lock_guard lock(mutex_);
    capacity_ = std::clamp<std::size_t>(capacity, 1, 100000);
    Trim();
    version_.fetch_add(1);
}

std::size_t EventStore::Capacity() const {
    std::lock_guard lock(mutex_);
    return capacity_;
}

std::size_t EventStore::Size() const {
    std::lock_guard lock(mutex_);
    return events_.size();
}

void EventStore::Trim() {
    while (events_.size() > capacity_) events_.pop_back();
}

void EventStore::Add(GhostEvent event) {
    std::lock_guard lock(mutex_);
    event.sequence = next_sequence_++;
    const ProcessIdentity identity = event.process.Identity();

    if (active_.size() > kMaxTrackedLifecycles) active_.clear();   // bounded; only affects lifetime grouping

    switch (event.kind) {
        case GhostKind::ProcessStarted: {
            ActiveLifecycle lifecycle;
            lifecycle.id = next_lifecycle_++;
            lifecycle.identity = identity;
            lifecycle.has_start = true;
            lifecycle.started = event.timestamp;
            lifecycle.start_process = event.process;
            event.lifecycle_id = lifecycle.id;
            active_[event.process.pid] = std::move(lifecycle);   // a reused PID simply replaces the old entry
            break;
        }
        case GhostKind::ProcessExited: {
            auto found = active_.find(event.process.pid);
            if (found != active_.end() && found->second.identity.Matches(identity)) {
                const ActiveLifecycle& lifecycle = found->second;
                event.lifecycle_id = lifecycle.id;
                if (lifecycle.has_start) {
                    event.lifetime = std::chrono::duration_cast<std::chrono::milliseconds>(event.timestamp - lifecycle.started);
                    event.has_lifetime = true;
                    const ProcessInfo& started = lifecycle.start_process;
                    if (event.process.exe_path.empty()) event.process.exe_path = started.exe_path;
                    if (event.process.command_line.empty()) event.process.command_line = started.command_line;
                    if (event.process.parent_name.empty()) event.process.parent_name = started.parent_name;
                    if (event.process.parent_path.empty()) event.process.parent_path = started.parent_path;
                    if (!event.process.parent_pid) event.process.parent_pid = started.parent_pid;
                    if (FileTimeToU64(event.process.start_time) == 0) event.process.start_time = started.start_time;
                }
                active_.erase(found);
            } else {
                event.lifecycle_id = next_lifecycle_++;   // exit of a process we never saw start
            }
            break;
        }
        default: {
            auto found = active_.find(event.process.pid);
            if (found != active_.end() && found->second.identity.Matches(identity)) {
                event.lifecycle_id = found->second.id;
            } else {
                // The owning process started before monitoring began (or its start was missed).
                ActiveLifecycle lifecycle;
                lifecycle.id = next_lifecycle_++;
                lifecycle.identity = identity;
                event.lifecycle_id = lifecycle.id;
                if (event.process.pid) active_[event.process.pid] = std::move(lifecycle);
            }
            break;
        }
    }

    events_.push_front(std::move(event));
    Trim();
    version_.fetch_add(1);
}

bool EventStore::Dismiss(std::uint64_t sequence) {
    std::lock_guard lock(mutex_);
    const auto found = std::find_if(events_.begin(), events_.end(),
                                    [&](const GhostEvent& event) { return event.sequence == sequence; });
    if (found == events_.end()) return false;
    events_.erase(found);
    version_.fetch_add(1);
    return true;
}

void EventStore::Clear() {
    std::lock_guard lock(mutex_);
    events_.clear();
    version_.fetch_add(1);
    // `active_` is kept on purpose: processes that are still running keep their lifecycle ids.
}

std::vector<GhostEvent> EventStore::Snapshot(std::size_t limit) const {
    std::lock_guard lock(mutex_);
    const std::size_t count = std::min(limit, events_.size());
    return {events_.begin(), events_.begin() + static_cast<std::ptrdiff_t>(count)};
}

bool EventStore::MatchesStatic(const GhostEvent& event, const EventFilter& filter, const std::wstring& needle_lower,
                               std::chrono::system_clock::time_point now) {
    if ((filter.categories & CategoryBit(CategoryOf(event.kind))) == 0) return false;
    if (filter.max_age.count() > 0 && now - event.timestamp > filter.max_age) return false;
    if (needle_lower.empty()) return true;
    return ContainsNoCase(event.process.name, needle_lower) || ContainsNoCase(event.process.exe_path, needle_lower) ||
           ContainsNoCase(event.process.command_line, needle_lower) || ContainsNoCase(event.window.title, needle_lower) ||
           ContainsNoCase(event.window.class_name, needle_lower);
}

std::vector<GhostEvent> EventStore::Query(const EventFilter& filter, std::size_t limit, const RunningCheck& running) const {
    const std::wstring needle = ToLower(filter.text);
    const auto now = std::chrono::system_clock::now();
    const bool needs_state = filter.state != StateFilter::Any && running;

    std::vector<GhostEvent> candidates;
    {
        std::lock_guard lock(mutex_);
        for (const auto& event : events_) {
            if (!MatchesStatic(event, filter, needle, now)) continue;
            candidates.push_back(event);
            if (!needs_state && candidates.size() >= limit) break;
        }
    }
    if (!needs_state) return candidates;

    // Process state is an OS query, so it runs outside the lock.
    std::vector<GhostEvent> result;
    for (auto& event : candidates) {
        const bool alive = event.kind != GhostKind::ProcessExited && running(event.process);
        if ((filter.state == StateFilter::Running) != alive) continue;
        result.push_back(std::move(event));
        if (result.size() >= limit) break;
    }
    return result;
}

std::optional<GhostEvent> EventStore::Find(std::uint64_t sequence) const {
    std::lock_guard lock(mutex_);
    for (const auto& event : events_) if (event.sequence == sequence) return event;
    return std::nullopt;
}

std::optional<GhostEvent> EventStore::Latest() const {
    std::lock_guard lock(mutex_);
    if (events_.empty()) return std::nullopt;
    return events_.front();
}

std::vector<GhostEvent> EventStore::Lifecycle(std::uint64_t lifecycle_id) const {
    std::vector<GhostEvent> result;
    {
        std::lock_guard lock(mutex_);
        for (const auto& event : events_) if (event.lifecycle_id == lifecycle_id) result.push_back(event);
    }
    std::reverse(result.begin(), result.end());
    return result;
}

} // namespace peek
