#pragma once

#include "core/investigation.h"
#include "ui/page.h"

#include <vector>

namespace peek {

// Process tab: everything known about one process, and links to its parent, children and windows.
class ProcessPage : public Page {
public:
    using Page::Page;
    void ShowProcess(const ProcessIdentity& identity);
    void Refresh() override {}

protected:
    void OnCreate() override;
    void OnLayout(int width, int height) override;
    void OnCommand(int id, int code, HWND control) override;
    LRESULT OnNotify(NMHDR* header) override;

private:
    enum Id { kBack = 200, kRefresh, kCommandLine, kCopy, kLocation, kList, kDetail, kOpenWindow, kInspectWindow,
              kShowEvents };
    struct Related {
        enum class Kind { Parent, Child, Window } kind{};
        ProcessIdentity identity;
        HWND hwnd{};
    };

    void Load(bool with_command_line);
    void FillRelations();
    void Activate(int row);

    HWND title_{}, back_{}, refresh_{}, cmd_{}, copy_{}, location_{}, list_{}, detail_{}, open_window_{}, inspect_{}, events_{};
    ProcessDetails details_;
    std::vector<Related> related_;
    std::vector<ProcessIdentity> history_;
    bool have_process_{false};
};

} // namespace peek
