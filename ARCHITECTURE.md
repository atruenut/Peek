# Peek architecture

Read [docs/REDESIGN_PLAN.md](docs/REDESIGN_PLAN.md) for the audit and decisions, [docs/DEVLOG.md](docs/DEVLOG.md) for status.

## Layers

```
src/core    types, settings (INI), event store, process resolver, investigation model, strings, helpers
src/ghost   GhostMonitor: capture only, no UI
src/unlock  UnlockService: Restart Manager, bounded folder scan, grouping, polite close
src/focus   FocusInspector (UI Automation worker), copy formatters
src/shell   ShellIntegration: per-user Explorer verbs
src/ui      HudWindow, OverlayWindow (Focus frame), MainWindow + pages (Ghost, Process, Unlock, Focus, Settings)
src/app.*   composition root, tray, shortcuts, single instance, settings -> behaviour
```

UI code never calls capture code directly except through `CommandHost` (actions) and shared read-only services.
`Navigator` lets pages jump to each other, which is what ties Ghost, Unlock and Focus together: every surface
resolves to a `ProcessIdentity` (PID + creation time) and `DescribeProcess()` produces the same facts for all of them.

## Threads

| Thread | Owns | Talks to UI by |
| --- | --- | --- |
| UI (STA) | all windows, tray, hotkeys, settings writes | - |
| Ghost window thread | WinEvent hooks (out-of-context callbacks run on the installing thread) | one coalesced posted message |
| Ghost WMI thread (MTA) | WMI notification query | same |
| Ghost poll thread | only when WMI is unavailable | same |
| Focus worker (MTA) | **all** UI Automation objects | latest-snapshot slot + queued tree updates + posted message |
| Unlock worker | one investigation at a time (per page) | result slot + posted message |

`EventStore` and `SettingsManager` are mutex protected. Shutdown order: stop Focus and Ghost, destroy overlay/HUD/window, remove tray.

## Monitoring is independent of windows

`GhostMonitor` writes to `EventStore` whether or not the HUD, tray or window exist. Pausing is a setting
(`Ghost.Enabled`), not a side effect of hiding anything. The 1.5 s HUD timer only runs while the HUD is enabled
(fullscreen check and "still running" refresh); the 1 s status tick only runs during a timed Trace.

## Security and privacy posture

No network, no telemetry, no automatic elevation, no debug privilege. Captured data stays in memory; only
settings are written (Peek.ini). Password-field values are never read by Focus. Termination requires an explicit
confirmation and re-checks PID + creation time; services, Explorer and critical processes are not offered.
