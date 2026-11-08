#pragma once

#include "core/process_resolver.h"
#include "core/types.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace peek {

// Live:   follow the element under the cursor.
// Frozen: stop updating; the last element stays selected and its properties stay put.
// Pinned: like Frozen, but the element is also tracked (bounds refreshed) while other applications are used.
enum class InspectMode { Off, Live, Frozen, Pinned };

// Hierarchy data sent to the UI. `reset` replaces the whole tree (nodes are listed parent-first);
// otherwise `nodes` are the children of `parent_id`.
struct FocusTreeUpdate {
    bool reset{};
    int parent_id{-1};
    std::vector<UiaNode> nodes;
};

// Inspects windows and UI Automation elements.
//
// Every UI Automation object lives on one dedicated worker thread (COM MTA) for its whole lifetime; UI
// threads only exchange plain data through the callbacks. Callbacks run on the worker thread and must
// be cheap and thread safe (post a message, do not touch windows directly).
// The worker starts on first use and sleeps (no polling) unless the mode is Live or Pinned.
class FocusInspector {
public:
    using SnapshotCallback = std::function<void(FocusSnapshot)>;
    using TreeCallback = std::function<void(FocusTreeUpdate)>;
    using ModeCallback = std::function<void(InspectMode)>;

    FocusInspector(ProcessResolver& resolver, SnapshotCallback on_snapshot, TreeCallback on_tree, ModeCallback on_mode);
    ~FocusInspector();
    FocusInspector(const FocusInspector&) = delete;
    FocusInspector& operator=(const FocusInspector&) = delete;

    void StartLive();
    void Freeze();                  // Live -> Frozen
    void Pin();                     // Live/Frozen -> Pinned
    void Stop();                    // release the element and stop updating
    void InspectWindow(HWND hwnd);  // select a specific window and pin it (used when jumping from a process)
    void RequestTree();             // build the hierarchy around the selected element
    void ExpandNode(int node_id);   // load the children of one hierarchy node

    [[nodiscard]] InspectMode Mode() const { return static_cast<InspectMode>(mode_.load()); }
    [[nodiscard]] bool Active() const { return Mode() != InspectMode::Off; }

    // One-shot synchronous sample of the element at a point, on the calling thread. Used by the self-test.
    [[nodiscard]] bool InspectOnce(POINT point, FocusSnapshot& snapshot, std::wstring& error) const;

private:
    struct Request {
        enum class Type { Tree, Expand, Window } type{};
        int node_id{};
        HWND hwnd{};
    };

    void EnsureWorker();
    void Wake();
    void Loop();
    void SetMode(InspectMode mode, bool notify);

    ProcessResolver& resolver_;
    SnapshotCallback on_snapshot_;
    TreeCallback on_tree_;
    ModeCallback on_mode_;

    std::atomic<int> mode_{static_cast<int>(InspectMode::Off)};
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Request> requests_;
    bool quit_{false};
    bool changed_{false};
    std::thread worker_;
};

} // namespace peek
