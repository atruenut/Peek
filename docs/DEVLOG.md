# Development log

Written for the person picking this up on Windows. Development happened on Linux with a MinGW-w64
cross-compiler, so **nothing that calls Windows APIs has been executed**. Compilation and linking succeed
(`scripts/dev/crosscheck.sh`, 0 warnings); the portable core has 57 unit checks (`tests/run.sh`).

## Where things stand

| Phase | Scope | Code | Tested on Windows |
| --- | --- | --- | --- |
| 1 | English-only, INI settings, HUD, persistent Ghost, optional shortcuts | done | **no** |
| 2 | Explorer verbs, multi-file/folder Unlock, graduated close/end | done (retry = "Check again") | **no** |
| 3 | Focus Live/Frozen/Pinned, highlight, UIA tree, text/JSON/C copy | done | **no** |
| 4 | Unified model, process tree navigation | partly (Process tab, Navigator, shared `DescribeProcess`) | **no** |
| 4 | ETW backend, optional local history | **not started** | - |
| 5 | Test matrix, release build | **not started** (see [TESTING.md](TESTING.md)) | - |

## Start here: first run on Windows

1. `build.bat` then `test.bat`. If a self-test line fails, its detail says which API call to look at.
2. Launch `PeekII.exe`. If nothing shows, check the tray; the HUD defaults to top-right of the primary monitor.
3. Walk through [TESTING.md](TESTING.md). Record failures below.

### Things I am least sure about (most likely to need a fix)

1. **HUD right-click menus** (`HudWindow::ShowRowMenu`): they call `SetForegroundWindow` on the non-activating
   HUD so the menu dismisses properly. If the menu will not close on outside click, or focus jumps oddly, look here.
2. **HUD hit testing**: header returns `HTCAPTION` (drag), rows return `HTTRANSPARENT` only when `ClickThrough=1`.
   Verify drag works with `WS_EX_NOACTIVATE`, and that `WM_EXITSIZEMOVE` fires so the position is saved.
3. **Dark mode of native controls** (`Page::StyleControl`): uses `SetWindowTheme(..., L"DarkMode_Explorer"/L"DarkMode_CFD")`.
   Expect some controls (checkbox text, combo boxes, tree lines) to look wrong; the fallback is Light theme.
4. **Layout**: pages are laid out by hand with fixed DIP sizes and no visual testing. Expect clipped labels or
   overlaps at some DPIs; adjust `OnLayout` in each `*_page.cpp`. Minimum window size is 900x620 DIP.
5. **Fullscreen detection** (`HudWindow::FullscreenAppActive`): `SHQueryUserNotificationState` plus a
   "borderless window covers the monitor" check. Verify with games, video players and borderless windows.
6. **Explorer multi-select merge**: Explorer starts one process per file; `App::QueueInvestigation` waits 400 ms
   and merges them. If a slow machine splits one selection into several investigations, raise the delay.
   Single-instance forwarding uses a named mutex plus `WM_COPYDATA` to the hidden `PeekII.Controller` window.
7. **UI Automation**: all UIA objects live on one MTA worker thread. Verify Live mode is not sluggish on
   Chromium/Electron windows (50 ms poll, 1.5 s UIA timeouts) and that Pinned tracking follows window moves.
8. **WMI** (`GhostMonitor::WmiLoop`): carried over from the original and restructured onto its own thread. Verify
   process events as a standard user and elevated; the polling fallback should engage when access is denied.
9. **Focus tree expand**: `TVN_ITEMEXPANDING` is cancelled for unloaded nodes and the expander opens when
   `ApplyTree` delivers children. Confirm this does not loop or leave nodes stuck.
10. **New in the PeekII pass (all untested on Windows)**: Settings scrolling (`SettingsPage::OnLayout`: `SetScrollInfo` on a page
    created with `WS_VSCROLL`; check the scroll bar appears only when needed and the wheel works), tooltips
    (`Page::Tip`, `TTM_ADDTOOLW` with `TTF_IDISHWND`; check they appear on hover and in dark mode), F1 handling (handled in the
    message loop in `App::Run`, because it is not certain that child controls forward `WM_HELP`), the "? Help (F1)" link in the
    tab strip, the color swatch (`WM_CTLCOLORSTATIC` returning a brush), Enter-to-commit in number fields (`IDOK` from
    `IsDialogMessage` routed to `Page::Accept`), and the multi-size icon (PNG-compressed ICO entries; check the tray, title bar
    and taskbar icons).
11. **ARM64**: `build.bat arm64` needs an `aarch64-w64-mingw32` toolchain; never built.

## Decisions worth knowing

- **Branch**: work is on `redesign/peek-desktop`; nothing was merged into `main`.
- **Explorer**: classic per-user registry verbs, not `IExplorerCommand`. A Windows 11 top-level menu entry needs
  a packaged (sparse/MSIX) COM extension with identity, which conflicts with portable single-exe use. Revisit if
  you want the new menu. "Find process using this file" was merged into "Who is locking this?" (same answer);
  "Inspect file" additionally shows file details.
- **Restart Manager** is used to *identify* owners only. `RmShutdown` can close other registered apps and services,
  so "Ask to close" posts `WM_CLOSE` to the owner's own windows instead. Services/Explorer/critical processes are never offered.
- **Retry**: "Check again" re-runs the investigation; PeekII does not re-run your file operation itself.
- **Settings removed**: `Focus.State` (Frozen/Pinned refer to a live element and cannot be restored on restart) and
  `Focus.MouseGesture` (not implemented; the spec made it optional). Added `Ghost.GroupByLifecycle`.
- **Focus freeze without a hotkey**: PeekII's own windows are ignored in Live mode, so moving the pointer onto PeekII
  keeps the last target and you can press Freeze/Pin. Holding Shift also holds the target (polled while Live only; no hooks).
- **Trace mode** currently adds raw window create events and command-line capture. Timed modes expire in `GhostMonitor`.
- **Noise reduction**: only visible windows' show/hide/destroy are reported; see `docs/REDESIGN_PLAN.md`.
- **Dismissing/clearing events** is global (HUD and window share one store).
- Original copyright (c) HaBaDEDE is kept in `LICENSE` and the version resource.

## Not done / next steps

- Some page text is still written inline in `src/ui/*_page.cpp` (labels such as "Parent", "Child", "Status"); the rest lives in
  `src/core/strings.h`. Finish moving them if localization is ever wanted.
- ETW process capture (needs a privileged session; design a graceful "not available" path first).
- Optional local event history file (off by default, INI-controlled).
- Highlight the selected node in the Focus hierarchy; process tree view beyond parent/children.
- `docs/images`: no screenshots exist for the new UI; add them after the first Windows run.
- CI (`.github/workflows/windows.yml`): a Linux job runs unit tests and the cross-compile; a Windows job builds with MSYS2 and runs the
  self-test. Neither has run on GitHub since the rewrite; check the first run.

## Findings from Windows runs

- 2026-11-08: first launch on Windows failed with "The Peek window could not be created." Cause: `App::ControllerProc` passed the
  member `controller_` (still null during `CreateWindowExW`) to `DefWindowProc` for `WM_NCCREATE`, so creation failed. Fixed by
  storing the handle in `WM_NCCREATE`. Startup errors now name the window that failed and include the Windows error code.
  Lesson: every window procedure must use the `hwnd` parameter or set its member handle in `WM_NCCREATE`. All six window classes were checked.

- 2026-11-08: Settings tab rendered the number boxes and the "Choose..." button as tall blank blocks (screenshot from the first
  successful launch). Cause: the layout helper used one height (240 DIP, meant for combo drop-downs) for every control, and edit
  boxes had no border. Fixed: combos alone get the drop-down height, edits have a client edge, label columns are measured from
  the text, hints wrap by measured height, and the page scrolls when it is taller than the window. Rest of the UI work in that
  pass: tooltips on every control, section hints, F1 help, color swatch, Reset to defaults, friendlier WMI message.
- 2026-11-08: renamed Peek -> PeekII (exe, INI, window classes, mutex, Explorer verbs, resources) and replaced the icon
  (`scripts/generate-brand-assets.py`). Old `Peek.*` Explorer verbs are removed on register/unregister. An old `Peek.ini` is NOT
  migrated; settings start from defaults.

(Add further dated notes here.)
