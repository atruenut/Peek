# Testing

Nothing in the Windows-specific code has been run yet. Work through this list on a real machine and note
failures in [DEVLOG.md](DEVLOG.md). Run as a normal user first.

## Automated

```bat
build.bat
test.bat                          :: runs "Peek.exe --self-test"
```
The self-test checks event correlation, real WMI process events, Restart Manager on a locked temp file,
and a UI Automation read of a native button. On Linux: `tests/run.sh` (portable unit tests, 57 checks).

## Manual checklist

1. **Start**: tray icon appears, HUD appears top-right, HUD does not take focus (type in Notepad while it appears).
2. **HUD**: not in Alt+Tab, no taskbar button. Drag header, double-click header, header buttons, right-click menus, wheel scroll.
3. **Ghost**: run `powershell -NoProfile -Command exit`; open/close Notepad. Events stay until displaced. Window events
   and process events are separate. Pause/resume; hide HUD and confirm events still arrive in the window.
4. **Retention/filter/grouping**: set 100 events; filter by text, type, running/ended, age; tick group by lifetime.
5. **Process tab**: from an event; parent/child navigation; load command line; copy; show window.
6. **Unlock**: hold a file open (e.g. `powershell -c "$f=[IO.File]::Open('C:\temp\x.txt','OpenOrCreate','ReadWrite','None'); sleep 600"`).
   Check, then show window, ask to close, end process (confirm dialog), check again. A folder scan respects depth/file limits.
7. **Explorer**: add menu entries; right-click one file, then several files, then a folder (Win11: Show more options).
   Several files should produce **one** investigation. Remove entries and confirm they disappear.
8. **Focus**: Live over Notepad, Explorer, a Chromium page, an elevated window (partial data expected). Shift hold,
   Freeze, Pin (move the target window; the frame follows), Esc, hierarchy expand, copy text/JSON/C, password field value hidden.
9. **Shortcuts**: off by default; enable one; conflict with an existing shortcut is reported.
10. **Displays/DPI**: second monitor, mixed DPI, move the HUD between them, change scaling, unplug the chosen monitor.
11. **Fullscreen**: HUD hides in a fullscreen video/game and returns; `Fullscreen=show` keeps it.
12. **Themes**: light, dark, follow Windows; note any controls that stay light.
13. **Portable**: run from a USB/other folder; `Peek.ini` is created beside the exe; read-only folder falls back to `%APPDATA%\Peek`.
14. **Exit**: tray Exit; process ends; no leftover hotkeys, tray icon or hooks. Kill Explorer and confirm the tray icon returns.
