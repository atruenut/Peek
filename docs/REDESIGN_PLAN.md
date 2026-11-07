# Peek Desktop redesign plan

This document records the audit of the original code base, the decisions made for
the redesign, and the technical risks. It is the first thing to read when picking
the project up. Progress, test status and debugging instructions live in
[DEVLOG.md](DEVLOG.md).

## 1. Audit of the original repository

About 3,300 lines of C++20 across 28 source files. It is small and mostly clean.

| Area | Verdict |
| --- | --- |
| `ProcessResolver` | Reuse. Add a PID + creation-time identity and a "still running" check. Avoid the per-call full Toolhelp snapshot where possible. |
| `EventStore` | Reuse and extend: runtime-adjustable capacity, dismissal, filtering, lifecycle ids, change counter. |
| `GhostMonitor` | Reuse the WMI + WinEvent + 250 ms fallback design. **Restructure threading** (see risk R1). |
| `UnlockService` | Reuse. Extend to multiple files, grouping by process and directory scanning (Phase 2). |
| `FocusInspector` | Reuse. Extend with Live/Frozen/Pinned and a UIA tree (Phase 3). |
| `IconCache` | Reuse unchanged. |
| `PrivilegeHelper` | Reuse unchanged. Its PID-reuse guard on termination is good. |
| `MainWindow` | **Replace.** A fixed 420x540 owner-drawn GDI window cannot become a resizable investigation window. Rewritten with native controls. |
| `OverlayWindow` | Keep as the Focus highlight. A new, separate `HudWindow` is added. |
| `localization.*` | **Remove.** Chinese strings and registry-backed language switching. Replaced by a single English `strings.h`. |
| `CMakeLists.txt`, PowerShell build scripts | **Replace** with `build.bat` (GCC/G++, MinGW-w64). |
| `docs/` website, screenshots, `packaging/`, `bucket/`, `dist/Peek.exe` | **Remove.** They describe the old UI, contain Chinese, and point at the upstream author's release channels. |

### Chinese content found

`README.md` (bilingual), `docs/index.html`, `docs/site.js`, `packaging/winget/.../locale.zh-CN.yaml`,
`src/core/localization.cpp`, `src/ui/main_window.cpp`, `src/app.cpp`, and a stray mention in `TESTING.md`.
Old screenshots also show the Chinese UI.

## 2. Target architecture

```
src/
  core/    types, strings, ini, settings, event_store, process_resolver, format, hotkey, icon_cache, privilege_helper
  ghost/   ghost_monitor (capture only, no UI)
  focus/   focus_inspector
  unlock/  unlock_service
  ui/      theme, hud_window, main_window, overlay_window
  app.*    composition root and shared services
```

Rules: capture threads never touch UI objects; they post a coalesced message. UI code never calls
blocking Win32/WMI/COM APIs on the UI thread except for cheap, bounded lookups.

### Three interaction layers

1. **HUD** (`HudWindow`): layered, topmost, `WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW`, no taskbar button, not in Alt+Tab.
   Compact and expanded modes, anchored to a corner of a chosen monitor, DPI aware, hidden for fullscreen apps.
2. **Contextual actions**: right-click menu on a HUD row (copy, open location, open in Investigation window, dismiss).
   Explorer integration comes in Phase 2.
3. **Investigation window** (`MainWindow`): resizable, tab control plus ListView/edit controls. Secondary to the HUD.

### Settings

Portable `Peek.ini` next to the executable. If that directory is not writable the file falls back to
`%APPDATA%\Peek\Peek.ini`. A small hand-written UTF-8 INI reader/writer (no Win32 dependency, unit-testable on any host).
Values are validated and clamped on load. No registry use.

## 3. Phases and status

| Phase | Scope | Status |
| --- | --- | --- |
| 1 | Foundation, English-only, settings, HUD, persistent Ghost events, optional shortcuts | in progress |
| 2 | Explorer integration, multi-file Unlock, graduated close/terminate workflow | planned |
| 3 | Focus Live/Frozen/Pinned, UIA tree, JSON/C copy | planned |
| 4 | Unified process/window/file model, process tree, ETW investigation, Trace mode improvements | planned |
| 5 | Test matrix on Windows 10/11, ARM64, DPI, fullscreen, portable; release build | planned |

## 4. Technical risks

| # | Risk | Mitigation |
| --- | --- | --- |
| R1 | `WINEVENT_OUTOFCONTEXT` callbacks run on the thread that installed the hook, which was the UI thread. Each callback ran `Resolve()` (a full process snapshot), so a burst of window events could stall the UI. | Hooks move to a dedicated thread with its own message loop. Callbacks only snapshot cheap window data; process info comes from a small cache. |
| R2 | Showing a menu from a non-activating HUD needs foreground activation. | Use the standard `SetForegroundWindow` + `TrackPopupMenu` + `WM_NULL` sequence on explicit right-click only. Needs on-device verification. |
| R3 | `WS_EX_LAYERED` + `HTTRANSPARENT` hit testing behaves differently across Windows 10/11 builds. | Hit testing is centralised in one function and the click-through mode is a setting. |
| R4 | Fullscreen detection is heuristic. | `SHQueryUserNotificationState` plus a foreground-window-covers-monitor check. Behaviour is a setting (`hide` / `show`). |
| R5 | Cannot run or visually test Windows UI in the development environment. | Everything is cross-compiled with MinGW-w64 as a compile check. [DEVLOG.md](DEVLOG.md) lists exactly what must be verified on Windows. |
| R6 | ARM64 build needs a different toolchain. | `build.bat arm64` selects `aarch64-w64-mingw32-g++` (llvm-mingw). Not verified. |
| R7 | WMI process trace events can be denied for standard users. | Existing fallback poller retained; failure is shown in the HUD status line rather than a modal box. |

## 5. Explicit design decisions

- Global hotkeys default to **off**; each has its own enable flag and a configurable key string. Conflicts are
  detected both between Peek's own shortcuts and via `RegisterHotKey` failure, and reported in Settings.
- No low-level keyboard or mouse hooks anywhere.
- Window noise reduction: a window `CREATE` alone is not reported. `SHOW`, `HIDE` and `DESTROY` of a *visible*
  window are, so one visible window produces one "appeared" and one "disappeared" record. Raw create events are
  recorded only while Trace mode is active.
- Command-line capture (WMI query per process) is done only in Trace mode or on demand for a selected process.
- Event categories shown to the user: process started, window appeared, window disappeared, process terminated.
