# PeekII redesign plan

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
| `docs/` website, screenshots, `packaging/`, `bucket/`, `dist/PeekII.exe` | **Remove.** They describe the old UI, contain Chinese, and point at the upstream author's release channels. |

### Chinese content found (all removed)

`README.md` (bilingual), `docs/index.html`, `docs/site.js`, `packaging/winget/.../locale.zh-CN.yaml`,
`src/core/localization.cpp`, `src/ui/main_window.cpp`, `src/app.cpp`, and and a stray mention in the old `TESTING.md`.
Old screenshots also show the Chinese UI.

## 2. Architecture as built

```
src/
  core/    types, strings, ini, settings(+manager), event_store, process_resolver, investigation, window_util,
           format, hotkey, icon_cache, privilege_helper, com_scope
  ghost/   ghost_monitor (capture only, no UI)
  focus/   focus_inspector (UI Automation worker), focus_format (text/JSON/C)
  unlock/  unlock_service
  shell/   shell_integration (Explorer verbs)
  ui/      theme, gdi, hud_window, overlay_window, main_window, page + ghost/process/unlock/focus/settings pages
  app.*, cli.*, main.cpp   composition root, tray, shortcuts, command line, single instance
```

See [../ARCHITECTURE.md](../ARCHITECTURE.md) for threading and data flow.

Rules: capture threads never touch UI objects; they post a coalesced message. UI code never calls
blocking Win32/WMI/COM APIs on the UI thread except for cheap, bounded lookups (Unlock investigations run
on a worker; all UI Automation runs on one worker).

### Three interaction layers

1. **HUD** (`HudWindow`): layered, topmost, `WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW`, no taskbar button, not in Alt+Tab.
   Compact and expanded modes, anchored to a corner of a chosen monitor, DPI aware, hidden for fullscreen apps.
2. **Contextual actions**: HUD row menu, tray menu, Explorer right-click entries, and `Navigator` links between
   pages (event -> process -> window -> Focus; lock owner -> process).
3. **Investigation window** (`MainWindow`): resizable frame with a custom-drawn tab strip hosting pages built from
   native controls: Ghost, Process, Unlock, Focus, Settings. Secondary to the HUD.

### Settings

Portable `PeekII.ini` next to the executable. If that directory is not writable the file falls back to
`%APPDATA%\PeekII\PeekII.ini`. A small hand-written UTF-8 INI reader/writer (no Win32 dependency, unit-testable on any host).
Values are validated and clamped on load. No registry use for settings (the optional Explorer entries are the only registry writes).

## 3. Phases and status

| Phase | Scope | Status |
| --- | --- | --- |
| 1 | Foundation, English-only, settings, HUD, persistent Ghost events, optional shortcuts | code complete, untested on Windows |
| 2 | Explorer integration, multi-file Unlock, graduated close/terminate workflow | code complete, untested |
| 3 | Focus Live/Frozen/Pinned, UIA tree, JSON/C copy | code complete, untested |
| 4 | Unified process/window/file model, process tree, ETW investigation, Trace mode improvements | partly (no ETW) |
| 5 | Test matrix on Windows 10/11, ARM64, DPI, fullscreen, portable; release build | not started ([TESTING.md](TESTING.md)) |

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

- Global hotkeys default to **on** (changed 2026-11-08 at the owner's request; three-modifier keys avoid stealing common app shortcuts); each has its own enable flag and a configurable key string. Conflicts are
  detected both between PeekII's own shortcuts and via `RegisterHotKey` failure, and reported in Settings.
- No low-level keyboard or mouse hooks anywhere.
- Window noise reduction: a window `CREATE` alone is not reported. `SHOW`, `HIDE` and `DESTROY` of a *visible*
  window are, so one visible window produces one "appeared" and one "disappeared" record. Raw create events are
  recorded only while Trace mode is active.
- Command-line capture (WMI query per process) is done only in Trace mode or on demand for a selected process.
- Event categories shown to the user: process started, window appeared, window disappeared, process terminated.
