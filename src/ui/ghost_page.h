#pragma once

#include "ui/page.h"

#include <unordered_map>
#include <vector>

namespace peek {

// Ghost tab: the persistent event history with filtering, lifecycle grouping and per-event details.
class GhostPage : public Page {
public:
    using Page::Page;
    void Refresh() override;
    void SettingsChanged() override;
    void SelectEvent(std::uint64_t sequence);

protected:
    void OnCreate() override;
    void OnLayout(int width, int height) override;
    void OnCommand(int id, int code, HWND control) override;
    LRESULT OnNotify(NMHDR* header) override;
    void OnMessage(UINT message, WPARAM wparam, LPARAM lparam) override;

private:
    enum Id { kFilter = 100, kType, kState, kTime, kGroup, kClear, kPause, kList, kDetail, kCopy, kLocation,
              kDismiss, kProcess, kFocus };

    void Rebuild();
    void LoadFilterControls(const Settings& settings);
    void ApplyFilterControls();
    void UpdateDetails();
    void UpdateButtons();
    void ShowContextMenu(POINT screen);
    [[nodiscard]] const GhostEvent* Selected() const;
    [[nodiscard]] bool IsRunning(const GhostEvent& event);
    void FillCell(NMLVDISPINFOW* info);

    HWND filter_{}, type_{}, state_{}, time_{}, group_{}, clear_{}, pause_{}, count_{};
    HWND list_{}, detail_{}, copy_{}, location_{}, dismiss_{}, process_{}, focus_{};

    std::vector<GhostEvent> rows_;
    std::vector<bool> child_row_;                      // grouped mode: row continues the lifecycle above it
    std::unordered_map<std::uint64_t, bool> running_;  // sequence -> process still running (refreshed by timer)
    std::uint64_t built_version_{~0ull};
    std::uint64_t selected_sequence_{0};
    std::wstring scratch_;
    std::wstring last_filter_text_;
    bool needs_rebuild_{true};
};

} // namespace peek
