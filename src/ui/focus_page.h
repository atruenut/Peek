#pragma once

#include "ui/page.h"

#include <unordered_map>

namespace peek {

// Focus tab: properties of the inspected element, its UI Automation hierarchy and copy actions.
// The inspector and the on-screen highlight are owned by App; this page only displays and controls them.
class FocusPage : public Page {
public:
    using Page::Page;
    void SetSnapshot(const FocusSnapshot& snapshot);
    void ApplyTree(const FocusTreeUpdate& update);
    void ModeChanged(InspectMode mode);

protected:
    void OnCreate() override;
    void OnLayout(int width, int height) override;
    void OnCommand(int id, int code, HWND control) override;
    LRESULT OnNotify(NMHDR* header) override;

private:
    enum Id { kStart = 400, kFreeze, kPin, kCopyText, kCopyJson, kCopyC, kProcess, kLocation, kList, kTree };

    void UpdateButtons();
    void AddRow(const wchar_t* property, const std::wstring& value);
    HTREEITEM InsertNode(const UiaNode& node, HTREEITEM parent);

    HWND state_{}, start_{}, freeze_{}, pin_{}, copy_text_{}, copy_json_{}, copy_c_{}, process_{}, location_{};
    HWND hint_{}, list_{}, tree_{};
    FocusSnapshot snapshot_;
    bool have_snapshot_{false};
    InspectMode mode_{InspectMode::Off};
    std::unordered_map<int, HTREEITEM> items_;
};

} // namespace peek
