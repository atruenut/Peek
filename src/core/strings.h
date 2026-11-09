#pragma once

// All user-facing text lives here, in one place, in English. The application is single-language;
// keeping strings together means localization could be added later without touching UI code.

namespace peek::str {

inline constexpr wchar_t kAppName[] = L"PeekII";
inline constexpr wchar_t kAppVersion[] = L"0.2.0";

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
inline constexpr wchar_t kEventsShown[] = L"events";

// Menus (HUD context menu and tray).
inline constexpr wchar_t kMenuOpenInvestigation[] = L"Open PeekII window";
inline constexpr wchar_t kMenuShowEvent[] = L"Show in PeekII window";
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
inline constexpr wchar_t kMenuExit[] = L"Exit PeekII";

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
inline constexpr wchar_t kProtectedNote[] = L"This is a Windows service, Windows Explorer or a critical system process. PeekII does not offer to close or end it.";
inline constexpr wchar_t kBtnInvestigate[] = L"Check again";
inline constexpr wchar_t kBtnEndProcess[] = L"End process…";
inline constexpr wchar_t kNoLockOwner[] = L"No application reported using this file.";
inline constexpr wchar_t kOwnersFound[] = L"application(s) using this file";
inline constexpr wchar_t kChooseFileFirst[] = L"Choose or drop a file first.";
inline constexpr wchar_t kPickerTitle[] = L"Choose a file to investigate";
inline constexpr wchar_t kTerminateTitle[] = L"PeekII — End process";
inline constexpr wchar_t kTerminateQuestion[] = L"End this process?";
inline constexpr wchar_t kUnsavedWarning[] = L"Unsaved work in that process may be lost. Try closing the application normally first.";

// Focus.
inline constexpr wchar_t kFocusIntro[] = L"Live follows the pointer; hold Shift to hold the target, Esc stops. PeekII's own windows are never inspected, so you can move onto PeekII and press Freeze or Pin.";
inline constexpr wchar_t kBtnFreeze[] = L"Freeze";
inline constexpr wchar_t kBtnResume[] = L"Resume (live)";
inline constexpr wchar_t kBtnPin[] = L"Pin";
inline constexpr wchar_t kBtnUnpin[] = L"Unpin (live)";
inline constexpr wchar_t kBtnCopyText[] = L"Copy text";
inline constexpr wchar_t kBtnCopyJson[] = L"Copy JSON";
inline constexpr wchar_t kBtnCopyC[] = L"Copy C struct";
inline constexpr wchar_t kBtnStartInspect[] = L"Start inspecting";
inline constexpr wchar_t kBtnStopInspect[] = L"Stop inspecting";
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
inline constexpr wchar_t kSetShortcutSection[] = L"Global shortcuts";
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
inline constexpr wchar_t kSetHighlightWidth[] = L"Highlight width (1\u201312 px)";
inline constexpr wchar_t kSetHighlightColor[] = L"Highlight color";
inline constexpr wchar_t kSetHighlightColorBtn[] = L"Choose…";
inline constexpr wchar_t kSetShortcutActivate[] = L"Open PeekII window";
inline constexpr wchar_t kSetShortcutGhost[] = L"Show Ghost activity";
inline constexpr wchar_t kSetShortcutFocus[] = L"Start Focus inspection";
inline constexpr wchar_t kSetApplyShortcuts[] = L"Apply shortcuts";
inline constexpr wchar_t kSetTheme[] = L"Theme";
inline constexpr wchar_t kSetThemeSystem[] = L"Follow Windows";
inline constexpr wchar_t kSetThemeLight[] = L"Light";
inline constexpr wchar_t kSetThemeDark[] = L"Dark";
inline constexpr wchar_t kSetCloseButton[] = L"Close button";
inline constexpr wchar_t kSetCloseHide[] = L"Hide the window (keep running)";
inline constexpr wchar_t kSetCloseExit[] = L"Exit PeekII";
inline constexpr wchar_t kSetStartWithWindow[] = L"Open the PeekII window at startup";
inline constexpr wchar_t kSetConfigFile[] = L"Settings file";
inline constexpr wchar_t kSetOpenFolder[] = L"Open folder";
inline constexpr wchar_t kSetUnlockSection[] = L"Unlock and Explorer";
inline constexpr wchar_t kSetRegisterShell[] = L"Add Explorer menu entries";
inline constexpr wchar_t kSetUnregisterShell[] = L"Remove entries";
inline constexpr wchar_t kShellNotRegistered[] = L"Explorer menu entries are not installed.";
inline constexpr wchar_t kShellRegistered[] = L"Explorer menu entries are installed (right-click a file; on Windows 11 choose \"Show more options\").";
inline constexpr wchar_t kShellOtherExe[] = L"Entries exist but point to another copy of PeekII: ";
inline constexpr wchar_t kSetDepth[] = L"Folder scan depth (0\u201316)";
inline constexpr wchar_t kSetMaxFiles[] = L"Maximum files per scan";
inline constexpr wchar_t kSetHighlightDuration[] = L"Highlight duration (ms)";
inline constexpr wchar_t kSetShortcutOk[] = L"registered";
inline constexpr wchar_t kSetShortcutOff[] = L"off";
inline constexpr wchar_t kSetShortcutInvalid[] = L"not a valid shortcut";
inline constexpr wchar_t kSetShortcutDuplicate[] = L"already used by another PeekII shortcut";
inline constexpr wchar_t kSetShortcutTaken[] = L"in use by another application";

// Settings: section explanations, tooltips and help.
inline constexpr wchar_t kSetGeneralHint[] = L"Appearance, and what the window's close button does.";
inline constexpr wchar_t kSetHudHint[] = L"A small always-on-top overlay listing recent process and window activity. It never takes keyboard focus. Drag its title bar to move it; right-click it for actions.";
inline constexpr wchar_t kSetGhostHint[] = L"Ghost records processes and windows that start, appear, disappear or exit, including ones too short-lived to notice.";
inline constexpr wchar_t kSetUnlockHint[] = L"Right-click a file or folder in Explorer to see which applications are using it. On Windows 11 choose \"Show more options\" first. Entries are added for the current user only.";
inline constexpr wchar_t kSetFocusHint[] = L"Point at any control on screen to see its properties and a steady highlight around it.";
inline constexpr wchar_t kSetShortcutHint[] = L"System-wide keys that work from any application. Untick one to turn it off, or type another combination (at least one modifier), then press Apply. A key already used by another program is reported beside its entry.";
inline constexpr wchar_t kSetFileHint[] = L"Changes are saved as soon as you make them. Copy this file to move your settings to another PC.";
inline constexpr wchar_t kSetResetDefaults[] = L"Reset to defaults";
inline constexpr wchar_t kSetResetConfirm[] = L"Reset every PeekII setting to its default? Explorer menu entries are not affected.";
inline constexpr wchar_t kSetAboutSection[] = L"About";
inline constexpr wchar_t kSetAboutText[] = L"PeekII is a Windows desktop utility for seeing what is starting, appearing and holding files on your PC. It is a fork of Peek, redesigned and extended. Press F1 on any tab for help.";
inline constexpr wchar_t kSetShowHelp[] = L"Help for this tab (F1)";
inline constexpr wchar_t kSetShortcutCue[] = L"e.g. Ctrl+Alt+Shift+P";

inline constexpr wchar_t kTipTheme[] = L"Light, dark, or follow the Windows app mode.";
inline constexpr wchar_t kTipClose[] = L"Hiding keeps monitoring and the HUD running from the tray icon. Exit stops PeekII completely.";
inline constexpr wchar_t kTipStartWindow[] = L"When off, PeekII starts quietly in the tray.";
inline constexpr wchar_t kTipHudVisible[] = L"Show or hide the overlay. Monitoring continues while it is hidden.";
inline constexpr wchar_t kTipHudExpanded[] = L"Expanded shows more rows plus a detail line for each event; compact shows one line per event.";
inline constexpr wchar_t kTipClickThrough[] = L"Clicks on event rows pass through to the window underneath. The title bar and its buttons stay clickable.";
inline constexpr wchar_t kTipOpacity[] = L"How opaque the HUD is, from 30% to 100%.";
inline constexpr wchar_t kTipCorner[] = L"Which screen corner the HUD snaps to. Dragging it saves a custom position relative to that corner.";
inline constexpr wchar_t kTipMonitor[] = L"Which display shows the HUD. If that display is unplugged the primary one is used.";
inline constexpr wchar_t kTipFullscreen[] = L"Hide the HUD while a full-screen app or game is in front, so it never covers it.";
inline constexpr wchar_t kTipGhostEnabled[] = L"Turns the event recorder on or off. Also available from the tray menu.";
inline constexpr wchar_t kTipRetention[] = L"How many events are kept in memory. Older events are dropped first. Nothing is written to disk.";
inline constexpr wchar_t kTipTrace[] = L"Detailed mode also reads each new process's command line. It costs a little more CPU, so it can be limited to a short time.";
inline constexpr wchar_t kTipNoise[] = L"Hide helper windows (input method, tooltips, shadows) and one-pixel windows that would otherwise flood the list.";
inline constexpr wchar_t kTipRegister[] = L"Adds \"PeekII\" entries to the Explorer right-click menu for files and folders (current user only, no administrator rights needed).";
inline constexpr wchar_t kTipUnregister[] = L"Removes the Explorer menu entries, including those from older builds.";
inline constexpr wchar_t kTipDepth[] = L"How many folder levels below a chosen folder are searched for locked files. 0 checks only the folder's own files.";
inline constexpr wchar_t kTipMaxFiles[] = L"Stops a folder scan after this many files so a huge folder cannot stall PeekII.";
inline constexpr wchar_t kTipFocusEnabled[] = L"Allows the Focus inspector, which reads UI Automation information about the element under the pointer.";
inline constexpr wchar_t kTipWidth[] = L"Thickness of the highlight frame, in screen pixels.";
inline constexpr wchar_t kTipColor[] = L"Color of the highlight frame.";
inline constexpr wchar_t kTipDuration[] = L"Hide the highlight after this many milliseconds. 0 keeps it until you stop inspecting.";
inline constexpr wchar_t kTipShortcutCheck[] = L"Enable this shortcut. It takes effect when you press Apply.";
inline constexpr wchar_t kTipShortcutKey[] = L"Modifiers (Ctrl, Alt, Shift, Win) joined with + and a key, for example Ctrl+Alt+G. Avoid keys other programs rely on, because a global shortcut takes them from every application.";
inline constexpr wchar_t kTipApply[] = L"Registers the enabled shortcuts and reports any conflicts.";
inline constexpr wchar_t kTipOpenFolder[] = L"Show the settings file in Explorer.";
inline constexpr wchar_t kTipReset[] = L"Restore every setting to its default value.";

// Tooltips on the investigation pages.
inline constexpr wchar_t kTipFilter[] = L"Show only events whose process name, path or window title contains this text.";
inline constexpr wchar_t kTipGroup[] = L"Fold a process's start, windows and exit into one entry.";
inline constexpr wchar_t kTipPauseMon[] = L"Stop or resume recording. Events already listed are kept.";
inline constexpr wchar_t kTipClearEvents[] = L"Remove all recorded events from the list and the HUD.";
inline constexpr wchar_t kTipCopyDetails[] = L"Copy the details shown below to the clipboard.";
inline constexpr wchar_t kTipProcessDetails[] = L"Open the Process tab for the selected item.";
inline constexpr wchar_t kTipInspectWindow[] = L"Open the Focus tab on this window, if it still exists.";
inline constexpr wchar_t kTipOpenLocation[] = L"Show the executable in Explorer.";
inline constexpr wchar_t kTipDismissEvent[] = L"Remove this event from the list.";
inline constexpr wchar_t kTipFocusStart[] = L"Start inspecting: move the pointer over anything on screen. Esc stops, Shift holds the target.";
inline constexpr wchar_t kTipFocusFreeze[] = L"Keep the current element while you read or copy it.";
inline constexpr wchar_t kTipFocusPin[] = L"Keep this element even if you move the pointer or switch windows.";
inline constexpr wchar_t kTipCopyText[] = L"Copy the properties as plain text.";
inline constexpr wchar_t kTipCopyJson[] = L"Copy the properties as JSON.";
inline constexpr wchar_t kTipCopyC[] = L"Copy the properties as a C struct initializer.";
inline constexpr wchar_t kTipAddFiles[] = L"Choose one or more files to check.";
inline constexpr wchar_t kTipAddFolder[] = L"Check the files inside a folder, down to the depth set in Settings.";
inline constexpr wchar_t kTipClearTargets[] = L"Forget the current files and results.";
inline constexpr wchar_t kTipCheckAgain[] = L"Look again, for example after closing an application.";
inline constexpr wchar_t kTipShowOwner[] = L"Bring the selected application's window to the front.";
inline constexpr wchar_t kTipAskClose[] = L"Politely asks the application to close its windows, like clicking the X. It may ask you to save first.";
inline constexpr wchar_t kTipEndProcess[] = L"Last resort. Forcibly ends the process after you confirm; unsaved work is lost.";
inline constexpr wchar_t kTipBack[] = L"Return to the previous process.";
inline constexpr wchar_t kTipRefresh[] = L"Read the process details again.";
inline constexpr wchar_t kTipCmdLine[] = L"Read the command line this process was started with.";
inline constexpr wchar_t kTipShowEvents[] = L"Jump to a recorded event of this process in Ghost.";

// Help (F1), one text per tab.
inline constexpr wchar_t kHelpTab[] = L"? Help (F1)";
inline constexpr wchar_t kHelpTitle[] = L"PeekII help";
inline constexpr wchar_t kHelpGhost[] =
    L"GHOST\n\n"
    L"Every process start/exit and window appear/disappear is listed here, even if it lasted milliseconds.\n\n"
    L"\u2022 Type in the filter box to match process names, paths or window titles.\n"
    L"\u2022 Select an event to see what it was; double-click it to open the process.\n"
    L"\u2022 \"Group by process lifetime\" folds a process's start, windows and exit into one line.\n"
    L"\u2022 Pause monitoring to study the list without new events arriving.\n\n"
    L"If the status line says process events are polled, PeekII is probably not running as administrator; processes that live under a quarter of a second may then be missed.";
inline constexpr wchar_t kHelpProcess[] =
    L"PROCESS\n\n"
    L"Shows what is known about one process: status, parent, start time, path, command line, child processes, windows and recorded events.\n\n"
    L"\u2022 Open it from Ghost (double-click an event), Unlock or Focus (Process details).\n"
    L"\u2022 A process is identified by its PID and start time, so a reused PID is never confused with an older process.\n"
    L"\u2022 \"Load command line\" reads the command line on demand.\n"
    L"\u2022 Run PeekII as administrator to see details of elevated processes.";
inline constexpr wchar_t kHelpUnlock[] =
    L"UNLOCK\n\n"
    L"Finds out which applications are using a file or folder, for when Windows says \"file in use\".\n\n"
    L"\u2022 Add files or folders, drop them onto the window, or right-click them in Explorer after adding the menu entries in Settings.\n"
    L"\u2022 Select an owner, then try the gentlest option first: Show window, then Ask to close.\n"
    L"\u2022 End process is the last resort and always asks for confirmation. Windows services, Explorer and critical processes are never offered.\n"
    L"\u2022 Nothing is closed or ended automatically.";
inline constexpr wchar_t kHelpFocus[] =
    L"FOCUS\n\n"
    L"Inspects the UI element under the pointer: window, class, UI Automation name, role, bounds and more.\n\n"
    L"\u2022 Start inspecting, then move the pointer. Hold Shift to hold the current target; press Esc to stop.\n"
    L"\u2022 Freeze keeps the current element; Pin keeps it even while you move around.\n"
    L"\u2022 Copy the result as text, JSON or a C struct.\n"
    L"\u2022 Password fields are never read.";
inline constexpr wchar_t kHelpSettings[] =
    L"SETTINGS\n\n"
    L"Every change is saved immediately, except global shortcuts, which need Apply.\n\n"
    L"\u2022 Hover any option for an explanation.\n"
    L"\u2022 The settings file sits next to PeekII.exe (portable). If that folder is read-only it is kept in your user profile instead.\n"
    L"\u2022 Reset to defaults restores everything except the Explorer menu entries.";

// Messages.
inline constexpr wchar_t kErrStartup[] = L"PeekII could not start. It failed to create ";
inline constexpr wchar_t kErrSettingsSave[] = L"The settings file could not be saved:";
inline constexpr wchar_t kStatusWmiFallback[] = L"Process start and exit are detected by polling four times a second, so a process that lives less than a quarter of a second can be missed. Run PeekII as administrator for exact live notifications.";
inline constexpr wchar_t kStatusStopped[] = L"Monitoring is off.";

} // namespace peek::str
