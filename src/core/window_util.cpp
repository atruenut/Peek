#include "core/window_util.h"

namespace peek {

WindowInfo ReadWindowInfo(HWND hwnd) {
    WindowInfo info;
    info.hwnd = hwnd;
    if (!hwnd) return info;
    GetWindowThreadProcessId(hwnd, &info.pid);
    wchar_t text[512]{};
    int length = GetClassNameW(hwnd, text, static_cast<int>(std::size(text)));
    if (length > 0) info.class_name.assign(text, static_cast<std::size_t>(length));
    // GetWindowTextW on a window of another process does not block on a hung target; it returns the cached title.
    length = GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
    if (length > 0) info.title.assign(text, static_cast<std::size_t>(length));
    GetWindowRect(hwnd, &info.bounds);
    return info;
}

std::vector<WindowInfo> TopLevelWindows(DWORD pid, bool visible_only) {
    struct Context { DWORD pid; bool visible_only; std::vector<WindowInfo> found; } context{pid, visible_only, {}};
    EnumWindows([](HWND hwnd, LPARAM data) -> BOOL {
        auto* ctx = reinterpret_cast<Context*>(data);
        DWORD owner = 0;
        GetWindowThreadProcessId(hwnd, &owner);
        if (owner != ctx->pid) return TRUE;
        if (ctx->visible_only) {
            if (!IsWindowVisible(hwnd)) return TRUE;
            if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
            if (GetWindow(hwnd, GW_OWNER) != nullptr && !(GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW)) return TRUE;
        }
        ctx->found.push_back(ReadWindowInfo(hwnd));
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));
    return context.found;
}

bool BringWindowToFront(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
    FLASHWINFO flash{sizeof(flash), hwnd, FLASHW_ALL | FLASHW_TIMERNOFG, 3, 0};
    FlashWindowEx(&flash);
    return true;
}

} // namespace peek
