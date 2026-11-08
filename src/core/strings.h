#pragma once

// All user-facing text lives here, in one place, in English. The application is single-language;
// keeping strings together means localization could be added later without touching UI code.

namespace peek::str {

inline constexpr wchar_t kAppName[] = L"Peek";

// Event kinds and categories.
inline constexpr wchar_t kProcessStarted[] = L"Process started";
inline constexpr wchar_t kWindowAppeared[] = L"Window appeared";
inline constexpr wchar_t kWindowDisappeared[] = L"Window disappeared";
inline constexpr wchar_t kProcessTerminated[] = L"Process terminated";
inline constexpr wchar_t kWindowCreated[] = L"Window created";
inline constexpr wchar_t kWindowShown[] = L"Window shown";
inline constexpr wchar_t kWindowHidden[] = L"Window hidden";
inline constexpr wchar_t kWindowDestroyed[] = L"Window destroyed";
inline constexpr wchar_t kShortStarted[] = L"start";
inline constexpr wchar_t kShortAppeared[] = L"window +";
inline constexpr wchar_t kShortDisappeared[] = L"window −";
inline constexpr wchar_t kShortTerminated[] = L"exit";

// HUD.
inline constexpr wchar_t kHudWaiting[] = L"Waiting for process and window activity…";
inline constexpr wchar_t kHudPaused[] = L"Monitoring paused";
inline constexpr wchar_t kHudNoMatch[] = L"No events match the current filter";
inline constexpr wchar_t kHudMonitoring[] = L"Monitoring";
inline constexpr wchar_t kHudToggleMode[] = L"Switch between compact and expanded view";
inline constexpr wchar_t kHudPause[] = L"Pause or resume monitoring";
inline constexpr wchar_t kHudHide[] = L"Hide the HUD (monitoring continues)";
inline constexpr wchar_t kUnknownProcess[] = L"Unknown process";
inline constexpr wchar_t kRunning[] = L"running";
inline constexpr wchar_t kEnded[] = L"ended";
inline constexpr wchar_t kUnknown[] = L"unknown";
inline constexpr wchar_t kFrom[] = L"from";
inline constexpr wchar_t kEventsShown[] = L"events";

// Menus (HUD context menu and tray).
inline constexpr wchar_t kMenuOpenInvestigation[] = L"Open Peek window";
inline constexpr wchar_t kMenuShowEvent[] = L"Show in Peek window";
inline constexpr wchar_t kMenuCopySummary[] = L"Copy summary";
inline constexpr wchar_t kMenuCopyPath[] = L"Copy executable path";
inline constexpr wchar_t kMenuOpenLocation[] = L"Open file location";
inline constexpr wchar_t kMenuDismiss[] = L"Dismiss";
inline constexpr wchar_t kMenuClearEvents[] = L"Clear all events";
inline constexpr wchar_t kMenuHudVisible[] = L"Show HUD";
inline constexpr wchar_t kMenuHudExpanded[] = L"Expanded HUD";
inline constexpr wchar_t kMenuMonitoring[] = L"Monitor process and window activity";
inline constexpr wchar_t kMenuInspectElement[] = L"Inspect UI element (Focus)";
inline constexpr wchar_t kMenuSettings[] = L"Settings…";
inline constexpr wchar_t kMenuExit[] = L"Exit Peek";

// Investigation window.
inline constexpr wchar_t kTabGhost[] = L"Ghost";
inline constexpr wchar_t kTabProcess[] = L"Process";
inline constexpr wchar_t kTabUnlock[] = L"Unlock";
inline constexpr wchar_t kTabFocus[] = L"Focus";
inline constexpr wchar_t kTabSettings[] = L"Settings";

inline constexpr wchar_t kColTime[] = L"Time";
inline constexpr wchar_t kColEvent[] = L"Event";
inline constexpr wchar_t kColProcess[] = L"Process";
inline constexpr wchar_t kColPid[] = L"PID";
inline constexpr wchar_t kColState[] = L"State";
inline constexpr wchar_t kColDetail[] = L"Window / path";
inline constexpr wchar_t kColProperty[] = L"Property";
inline constexpr wchar_t kColValue[] = L"Value";
inline constexpr wchar_t kColPath[] = L"Executable path";

inline constexpr wchar_t kFilterCue[] = L"Filter by process, path or window title";
inline constexpr wchar_t kAllEvents[] = L"All events";
inline constexpr wchar_t kAnyState[] = L"Running or ended";
inline constexpr wchar_t kOnlyRunning[] = L"Still running";
inline constexpr wchar_t kOnlyEnded[] = L"Ended";
inline constexpr wchar_t kAnyTime[] = L"Any time";
inline constexpr wchar_t kLastMinute[] = L"Last minute";
inline constexpr wchar_t kLast10Minutes[] = L"Last 10 minutes";
inline constexpr wchar_t kLastHour[] = L"Last hour";
inline constexpr wchar_t kBtnClear[] = L"Clear";
inline constexpr wchar_t kBtnCopy[] = L"Copy details";
inline constexpr wchar_t kBtnOpenLocation[] = L"Open location";
inline constexpr wchar_t kBtnDismiss[] = L"Dismiss";
inline constexpr wchar_t kBtnPauseMonitoring[] = L"Pause monitoring";
inline constexpr wchar_t kBtnResumeMonitoring[] = L"Resume monitoring";
inline constexpr wchar_t kBtnCommandLine[] = L"Load command line";
inline constexpr wchar_t kGroupByLifecycle[] = L"Group by process lifetime";
inline constexpr wchar_t kBtnProcessDetails[] = L"Process details";
inline constexpr wchar_t kBtnInspectWindow[] = L"Inspect window (Focus)";
inline constexpr wchar_t kProcessIntro[] = L"No process selected. Double-click an event in Ghost, a result in Unlock, or use \"Process details\".";
inline constexpr wchar_t kBtnBack[] = L"Back";
inline constexpr wchar_t kBtnRefresh[] = L"Refresh";
inline constexpr wchar_t kBtnShowWindow[] = L"Show window";
inline constexpr wchar_t kBtnShowInGhost[] = L"Show events in Ghost";
inline constexpr wchar_t kColRelation[] = L"Relation";
inline constexpr wchar_t kNoSelection[] = L"Select an event to see details.";

// Unlock.
inline constexpr wchar_t kUnlockIntro[] = L"Choose or drop a file to see which applications are using it.";
inline constexpr wchar_t kBtnAddFiles[] = L"Add files\u2026";
inline constexpr wchar_t kBtnAddFolder[] = L"Add folder\u2026";
inline constexpr wchar_t kBtnAskClose[] = L"Ask to close";
inline constexpr wchar_t kChecking[] = L"Checking\u2026";
inline constexpr wchar_t kColFiles[] = L"Files";
inline constexpr wchar_t kColType[] = L"Type";
inline constexpr wchar_t kUnlockEmpty[] = L"Nothing investigated yet. Add files or folders, drop them here, or right-click a file in Explorer.";
inline constexpr wchar_t kProtectedNote[] = L"This is a Windows service, Windows Explorer or a critical system process. Peek does not offer to close or end it.";
inline constexpr wchar_t kBtnChooseFile[] = L"Choose file…";
inline constexpr wchar_t kBtnInvestigate[] = L"Check again";
inline constexpr wchar_t kBtnEndProcess[] = L"End process…";
inline constexpr wchar_t kNoLockOwner[] = L"No application reported using this file.";
inline constexpr wchar_t kOwnersFound[] = L"application(s) using this file";
inline constexpr wchar_t kChooseFileFirst[] = L"Choose or drop a file first.";
inline constexpr wchar_t kPickerTitle[] = L"Choose a file to investigate";
inline constexpr wchar_t kAllFiles[] = L"All files";
inline constexpr wchar_t kTerminateTitle[] = L"Peek — End process";
inline constexpr wchar_t kTerminateQuestion[] = L"End this process?";
inline constexpr wchar_t kUnsavedWarning[] = L"Unsaved work in that process may be lost. Try closing the application normally first.";

// Focus.
inline constexpr wchar_t kFocusIntro[] = L"Live follows the pointer; hold Shift to hold the target, Esc stops. Peek's own windows are never inspected, so you can move onto Peek and press Freeze or Pin.";
inline constexpr wchar_t kBtnFreeze[] = L"Freeze";
inline constexpr wchar_t kBtnResume[] = L"Resume (live)";
inline constexpr wchar_t kBtnPin[] = L"Pin";
inline constexpr wchar_t kBtnUnpin[] = L"Unpin (live)";
inline constexpr wchar_t kBtnCopyText[] = L"Copy text";
inline constexpr wchar_t kBtnCopyJson[] = L"Copy JSON";
inline constexpr wchar_t kBtnCopyC[] = L"Copy C struct";
inline constexpr wchar_t kBtnStartInspect[] = L"Start inspecting";
inline constexpr wchar_t kBtnStopInspect[] = L"Stop inspecting";
inline constexpr wchar_t kFocusMovePointer[] = L"Move the pointer over a window or control.";
inline constexpr wchar_t kUnavailable[] = L"unavailable";
inline constexpr wchar_t kPropProcess[] = L"Process";
inline constexpr wchar_t kPropPid[] = L"PID";
inline constexpr wchar_t kPropExe[] = L"Executable";
inline constexpr wchar_t kPropHwnd[] = L"HWND";
inline constexpr wchar_t kPropClass[] = L"Window class";
inline constexpr wchar_t kPropTitle[] = L"Window title";
inline constexpr wchar_t kPropWindowRect[] = L"Window rectangle";
inline constexpr wchar_t kPropControlType[] = L"Control type";
inline constexpr wchar_t kPropName[] = L"Name";
inline constexpr wchar_t kPropAutomationId[] = L"AutomationId";
inline constexpr wchar_t kPropValue[] = L"Value";
inline constexpr wchar_t kPropUiaBounds[] = L"UIA bounds";
inline constexpr wchar_t kPropCursor[] = L"Cursor";
inline constexpr wchar_t kPropUiaClass[] = L"UIA class name";
inline constexpr wchar_t kPropFramework[] = L"UI framework";
inline constexpr wchar_t kValueHidden[] = L"(hidden: password field)";

// Settings.
inline constexpr wchar_t kSetHudSection[] = L"Desktop HUD";
inline constexpr wchar_t kSetGhostSection[] = L"Ghost monitoring";
inline constexpr wchar_t kSetFocusSection[] = L"Focus";
inline constexpr wchar_t kSetShortcutSection[] = L"Global shortcuts (all off by default)";
inline constexpr wchar_t kSetGeneralSection[] = L"General";
inline constexpr wchar_t kSetHudVisible[] = L"Show the HUD";
inline constexpr wchar_t kSetHudExpanded[] = L"Expanded mode";
inline constexpr wchar_t kSetClickThrough[] = L"Event rows ignore the mouse";
inline constexpr wchar_t kSetOpacity[] = L"Opacity";
inline constexpr wchar_t kSetCorner[] = L"Corner";
inline constexpr wchar_t kSetMonitor[] = L"Monitor";
inline constexpr wchar_t kSetFullscreen[] = L"Fullscreen apps";
inline constexpr wchar_t kSetPrimaryMonitor[] = L"Primary monitor";
inline constexpr wchar_t kSetTopLeft[] = L"Top left";
inline constexpr wchar_t kSetTopRight[] = L"Top right";
inline constexpr wchar_t kSetBottomLeft[] = L"Bottom left";
inline constexpr wchar_t kSetBottomRight[] = L"Bottom right";
inline constexpr wchar_t kSetFullscreenHide[] = L"Hide the HUD";
inline constexpr wchar_t kSetFullscreenShow[] = L"Keep the HUD visible";
inline constexpr wchar_t kSetGhostEnabled[] = L"Monitor process and window activity";
inline constexpr wchar_t kSetRetention[] = L"Events to keep";
inline constexpr wchar_t kSetTrace[] = L"Trace mode";
inline constexpr wchar_t kSetTraceOff[] = L"Off (lightweight)";
inline constexpr wchar_t kSetTrace30[] = L"Detailed for 30 seconds";
inline constexpr wchar_t kSetTrace5m[] = L"Detailed for 5 minutes";
inline constexpr wchar_t kSetTraceOn[] = L"Detailed continuously";
inline constexpr wchar_t kSetIgnoreNoise[] = L"Ignore helper windows (IME, tooltips, shadows)";
inline constexpr wchar_t kSetFocusEnabled[] = L"Enable Focus inspection";
inline constexpr wchar_t kSetHighlightWidth[] = L"Highlight width (px)";
inline constexpr wchar_t kSetHighlightColor[] = L"Highlight color";
inline constexpr wchar_t kSetHighlightColorBtn[] = L"Choose…";
inline constexpr wchar_t kSetShortcutActivate[] = L"Open Peek window";
inline constexpr wchar_t kSetShortcutGhost[] = L"Show Ghost activity";
inline constexpr wchar_t kSetShortcutFocus[] = L"Start Focus inspection";
inline constexpr wchar_t kSetApplyShortcuts[] = L"Apply shortcuts";
inline constexpr wchar_t kSetTheme[] = L"Theme";
inline constexpr wchar_t kSetThemeSystem[] = L"Follow Windows";
inline constexpr wchar_t kSetThemeLight[] = L"Light";
inline constexpr wchar_t kSetThemeDark[] = L"Dark";
inline constexpr wchar_t kSetCloseButton[] = L"Close button";
inline constexpr wchar_t kSetCloseHide[] = L"Hide the window (keep running)";
inline constexpr wchar_t kSetCloseExit[] = L"Exit Peek";
inline constexpr wchar_t kSetStartWithWindow[] = L"Open the Peek window at startup";
inline constexpr wchar_t kSetConfigFile[] = L"Settings file";
inline constexpr wchar_t kSetOpenFolder[] = L"Open folder";
inline constexpr wchar_t kSetUnlockSection[] = L"Unlock and Explorer";
inline constexpr wchar_t kSetRegisterShell[] = L"Add Explorer menu entries";
inline constexpr wchar_t kSetUnregisterShell[] = L"Remove entries";
inline constexpr wchar_t kShellNotRegistered[] = L"Explorer menu entries are not installed.";
inline constexpr wchar_t kShellRegistered[] = L"Explorer menu entries are installed (right-click a file; on Windows 11 choose \"Show more options\").";
inline constexpr wchar_t kShellOtherExe[] = L"Entries exist but point to another copy of Peek: ";
inline constexpr wchar_t kSetDepth[] = L"Folder scan depth";
inline constexpr wchar_t kSetMaxFiles[] = L"Maximum files per scan";
inline constexpr wchar_t kSetHighlightDuration[] = L"Highlight duration (ms, 0 = keep)";
inline constexpr wchar_t kSetShortcutOk[] = L"registered";
inline constexpr wchar_t kSetShortcutOff[] = L"off";
inline constexpr wchar_t kSetShortcutInvalid[] = L"not a valid shortcut";
inline constexpr wchar_t kSetShortcutDuplicate[] = L"already used by another Peek shortcut";
inline constexpr wchar_t kSetShortcutTaken[] = L"in use by another application";

// Messages.
inline constexpr wchar_t kErrMainClass[] = L"The Peek window class could not be registered.";
inline constexpr wchar_t kErrMainWindow[] = L"The Peek window could not be created.";
inline constexpr wchar_t kErrGhostUnavailable[] = L"Peek — Ghost unavailable";
inline constexpr wchar_t kErrSettingsSave[] = L"The settings file could not be saved:";
inline constexpr wchar_t kStatusWmiFallback[] = L"Process start/exit events are polled (WMI unavailable).";
inline constexpr wchar_t kStatusActive[] = L"Monitoring active.";
inline constexpr wchar_t kStatusStopped[] = L"Monitoring is off.";

} // namespace peek::str
