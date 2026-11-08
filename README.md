<img src="docs/images/peek-icon.png" width="96" height="96" alt="Peek icon" align="right">

# Peek

Peek is a small, native Windows utility that makes processes, windows, files and interface elements
visible without getting in your way. It runs quietly in the tray, shows a narrow always-on-top activity
panel (the HUD), and opens a normal window only when you want to dig deeper.

It is a fork of [HaBaDEDE/Peek](https://github.com/HaBaDEDE/Peek), redesigned around a persistent HUD,
Explorer integration and optional shortcuts. MIT licensed.

> **Status: untested on Windows.** This version was written and cross-compiled on Linux. It builds and
> links with MinGW-w64, and the portable parts (INI, hotkey parsing, settings) have unit tests, but
> nothing that touches Windows (HUD, tray, Explorer menu, WMI, UI Automation) has been run yet.
> [docs/DEVLOG.md](docs/DEVLOG.md) lists exactly what to verify and what is most likely to need fixing.

## What it does

**Ghost** - what just appeared and disappeared?
- Records process started / exited and window appeared / disappeared events. A process can own many
  windows and windows can vanish before their process, so these are separate event types.
- Events stay in a persistent list until newer events push them out (retention 100 / 500 / 2,000 or your
  own value in `Peek.ini`) or you dismiss them. Nothing flashes and disappears.
- Filter by text (process name, path, command line, window title), event type, running/ended, and age.
  Optionally group events by process lifetime. Process identity is PID **plus** creation time, so a reused
  PID is never confused with the original process.
- Uses WMI process trace events (with a polling fallback if WMI is denied) and WinEvent hooks for windows.
  An optional **Trace** mode (30 s, 5 min or continuous) records extra detail such as raw window create
  events and command lines. ETW is not implemented yet.

**Process** - everything about one process
- PID, parent, executable, command line (on request), start time, status, running children, visible
  windows and the Ghost events that mention it. Double-click a parent or child to navigate.

**Unlock** - who is using this file?
- Add several files or folders, drop them on the window, or use the Explorer menu. Results are grouped by
  process, using the Windows Restart Manager.
- Folder scans respect a depth and a file-count limit (Settings).
- Graduated workflow: identify -> process details -> show its window -> **ask it to close** (normal
  `WM_CLOSE`) -> **end process** (confirmed, last resort). Services, Explorer and critical system
  processes are never offered for closing or ending. Nothing is closed automatically and no handles are
  force-closed.

**Focus** - what is under the pointer?
- Shows HWND, window class and title, PID, executable, and UI Automation AutomationId, ControlType, Name,
  Value and bounds. Password field values are never read.
- **Live** follows the pointer (hold Shift to hold the target; Esc stops). **Frozen** keeps the selection.
  **Pinned** keeps it and tracks its position while you use other applications. The state is shown in the tab.
- A click-through highlight frame follows the element's UI Automation bounds (falls back to the window),
  with configurable color, width and duration. Peek's own windows are never inspected.
- Expandable UI Automation hierarchy; copy as plain text, JSON or a C struct.
- Not every application exposes UI Automation data (custom-drawn, protected or elevated windows may be
  partial). Missing properties are shown as "unavailable", not guessed.

## Interaction model

1. **HUD** - narrow, non-activating overlay (`WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`):
   no taskbar button, no Alt+Tab entry, never takes focus. Compact and expanded modes, any screen corner of
   any monitor, DPI aware, hides while a fullscreen app is active (configurable). Drag the header to move
   it, double-click the header to switch mode, right-click for a menu, use the header buttons to pause
   monitoring or hide the HUD. Hiding the HUD does not stop monitoring.
2. **Tray icon** - open the window, toggle HUD, pause monitoring, choose Trace mode, start Focus, Exit.
3. **Investigation window** - the tabs above. Closing it hides it (or exits, see Settings).

Global shortcuts are **off by default**, configurable per shortcut, with conflict detection. Peek uses no
keyboard or mouse hooks.

## Requirements

- Windows 10 or Windows 11 (x64). Windows on ARM: see Build.
- No installer, no runtime, no network access, no telemetry, no accounts.

## Build

You need a MinGW-w64 GCC/G++ (for example [MSYS2](https://www.msys2.org) with `mingw-w64-x86_64-gcc`) on `PATH`.

```bat
build.bat                :: x64 release  -> build\x64\Peek.exe
build.bat x64 debug
build.bat arm64          :: needs aarch64-w64-mingw32-g++ (llvm-mingw); not verified
test.bat                 :: builds if needed, runs the built-in self-test on Windows
clean.bat
```

On Linux or macOS, `scripts/dev/crosscheck.sh` cross-compiles to catch build errors and `tests/run.sh`
runs the portable unit tests. Neither runs the program.

## Usage

Run `Peek.exe`. It starts in the tray with the HUD visible. Options:

```
Peek.exe --show                    open the window at startup
Peek.exe --config <file>           use a specific settings file
Peek.exe --who-locks <path>        investigate a file or folder
Peek.exe --inspect-file <path>     same, with file details first
Peek.exe --register-shell          add the Explorer menu entries (current user)
Peek.exe --unregister-shell        remove them
Peek.exe --self-test [report]      run diagnostics
```

Only one instance runs; later launches hand their request to the running one.

## Explorer integration

Settings -> Unlock and Explorer -> **Add Explorer menu entries** (or `--register-shell`). This writes
per-user verbs under `HKCU\Software\Classes\*\shell` and `...\Directory\shell`: no administrator rights,
fully removable. Right-click a file or folder: **Peek: Who is locking this?** or **Peek: Inspect file**.

- On Windows 11 these classic verbs appear under **Show more options**. The new-style Windows 11 menu
  requires a packaged COM extension with identity, which does not fit a portable executable; see Known limitations.
- Explorer shows classic verbs for at most 15 selected items. Selected files are merged into one investigation.
- If you move the portable folder, register again (the Settings page tells you when entries point elsewhere).

## Configuration

Settings live in `Peek.ini` next to `Peek.exe` (portable). If that folder is not writable, Peek uses
`%APPDATA%\Peek\Peek.ini`. Everything is editable in the Settings tab and applied immediately; the file
is plain UTF-8 and invalid values are corrected on load.

```ini
[General]
Theme=system
CloseButton=hide
StartWithWindow=0
[Hud]
Visible=1
Mode=compact
Opacity=88
Monitor=primary
Anchor=topright
OffsetX=16
OffsetY=16
Fullscreen=hide
ClickThrough=0
[Ghost]
Enabled=1
Retention=500
Trace=off
IgnoreNoiseWindows=1
GroupByLifecycle=0
[Unlock]
MaxDepth=2
MaxFiles=500
[Focus]
Enabled=1
HighlightColor=#3985FF
HighlightWidth=3
HighlightDurationMs=0
[Shortcuts]
ActivateEnabled=0
Activate=Ctrl+Shift+P
GhostEnabled=0
Ghost=Ctrl+Shift+G
FocusEnabled=0
Focus=Ctrl+Shift+F
```

Valid values: `Theme` system/light/dark; `CloseButton` hide/exit; `Mode` compact/expanded; `Anchor`
topleft/topright/bottomleft/bottomright; `Fullscreen` hide/show; `Trace` off/30s/5m/continuous; booleans 0/1.
`Opacity` is 30-100, `Retention` 50-10000. Comments must be on their own line (start with `;` or `#`).

## Troubleshooting

- **No process start/exit events:** WMI may be denied for standard users. Peek falls back to polling and says
  so in the HUD footer; very short-lived processes can be missed in that mode.
- **"Access denied" in Unlock or missing paths in Process:** the other process is elevated or protected.
  Run Peek as administrator to see more. Peek never elevates itself.
- **A shortcut says "in use by another application":** pick another combination.
- **Explorer entries missing:** on Windows 11 use *Show more options*; check Settings for the registration state.
- **HUD hidden:** it hides for fullscreen apps by default, or you hid it; use the tray menu.

## Known limitations

- Untested on Windows (see top). ARM64 builds are untested.
- No ETW backend yet; Trace mode enriches WMI/WinEvent capture only.
- Native controls are themed for dark mode on a best-effort basis; some may stay light on older Windows 10 builds,
  and popup menus are always light.
- Windows 11 new-style context menu is not supported (classic verbs only).
- Restart Manager is used to identify lock owners. Peek does not call `RmShutdown`, because that can affect
  other registered applications; it asks the owning process's windows to close instead.
- The optional mouse gesture for Focus is not implemented.
- Selecting a node in the Focus hierarchy does not move the highlight yet.

## Repository layout

```
src/            C++20 sources (core, ghost, focus, unlock, shell, ui)
resources/      icon, manifest (per-monitor DPI, long paths), version info
tests/          portable unit tests (INI, hotkeys, settings); run with tests/run.sh
scripts/        ci-smoke.ps1 (CI), dev/crosscheck.sh (cross-compile on Linux/macOS),
                generate-brand-assets.py (regenerates the icon; needs Python + Pillow)
docs/           REDESIGN_PLAN.md (audit and decisions), DEVLOG.md (status, what to debug), TESTING.md
ARCHITECTURE.md threads, data flow, security posture
```

## License

MIT. See [LICENSE](LICENSE). Original work copyright (c) 2026 HaBaDEDE; modifications by their authors.
