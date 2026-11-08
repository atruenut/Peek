#pragma once

#include "ui/page.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace peek {

// Unlock tab: which processes are using these files or folders, and a graduated way to deal with them:
// identify, inspect, locate the window, ask the application to close, and only then (confirmed) end it.
class UnlockPage : public Page {
public:
    using Page::Page;
    ~UnlockPage() override;
    // Replaces the targets and investigates them. `inspect` also shows the file details first.
    void ShowTargets(const std::vector<std::wstring>& paths, bool inspect);

protected:
    void OnCreate() override;
    void OnLayout(int width, int height) override;
    void OnCommand(int id, int code, HWND control) override;
    LRESULT OnNotify(NMHDR* header) override;
    void OnMessage(UINT message, WPARAM wparam, LPARAM lparam) override;

private:
    static constexpr UINT kDone = WM_APP + 40;
    enum Id { kAddFiles = 300, kAddFolder, kClearTargets, kCheck, kTargets, kList, kDetail, kProcess, kShowWindow,
              kLocation, kClose, kEnd };

    void AddTargets(const std::vector<std::wstring>& paths);
    void Start();
    void Cancel();
    void ShowReport();
    void UpdateDetail();
    void UpdateButtons();
    [[nodiscard]] const LockGroup* SelectedGroup() const;
    void PickFiles();
    void PickFolders();
    void SetStatus(const std::wstring& text);

    HWND intro_{}, add_files_{}, add_folder_{}, clear_{}, check_{}, status_{}, targets_{}, list_{}, detail_{};
    HWND process_{}, show_window_{}, location_{}, close_{}, end_{};

    std::vector<std::wstring> targets_list_;
    bool inspect_mode_{false};

    std::thread worker_;
    std::shared_ptr<std::atomic_bool> cancel_;
    std::mutex mutex_;
    std::shared_ptr<UnlockReport> pending_;
    std::uint64_t run_{0};
    std::uint64_t displayed_run_{0};
    bool busy_{false};
    UnlockReport report_;
    std::wstring action_note_;       // result of the last close/end attempt, shown under the details
};

} // namespace peek
