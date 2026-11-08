#pragma once

#include "core/types.h"

#include <vector>

namespace peek {

// Reads class name, title, owning PID and bounds of a window. Cheap; safe to call from any thread.
[[nodiscard]] WindowInfo ReadWindowInfo(HWND hwnd);

// Top-level windows owned by a process. When `visible_only` is set, hidden and tool windows are skipped.
[[nodiscard]] std::vector<WindowInfo> TopLevelWindows(DWORD pid, bool visible_only = true);

// Restores and raises a window and flashes its taskbar button. Windows may refuse to steal foreground
// focus; the flash makes the window findable in that case. Returns false if the window is gone.
bool BringWindowToFront(HWND hwnd);

} // namespace peek
