#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace peek {

// Lets any page of the investigation window jump to another: this is what turns Ghost, Unlock and Focus
// into one tool instead of three. Implemented by MainWindow.
class Navigator {
public:
    virtual ~Navigator() = default;
    virtual void OpenProcess(const ProcessIdentity& identity) = 0;       // Process tab
    virtual void OpenGhostEvent(std::uint64_t sequence) = 0;             // Ghost tab, event selected
    virtual void OpenFocusForWindow(HWND hwnd) = 0;                      // Focus tab, window pinned
    virtual void OpenUnlock(const std::vector<std::wstring>& paths, bool inspect) = 0;
};

} // namespace peek
