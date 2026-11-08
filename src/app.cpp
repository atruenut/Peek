#include "app.h"

#include "core/com_scope.h"
#include "core/format.h"
#include "core/strings.h"
#include "shell/shell_integration.h"

#include <commctrl.h>

namespace peek {
namespace {
constexpr wchar_t kControllerClass[] = L"Peek.Controller";
constexpr wchar_t kMutexName[] = L"Local\\Peek.SingleInstance";

enum TrayCommand : UINT {
    kTrayOpen = 1, kTrayHud, kTrayExpanded, kTrayMonitor, kTrayFocus, kTrayTraceOff, kTrayTrace30, kTrayTrace5, kTrayTraceOn,
    kTraySettings, kTrayExit,
};

}

App::App(const CommandLine& command_line)
    : command_line_(command_line),
      settings_(command_line.config_path),
      events_(settings_.Get().ghost.retention),
      unlock_(resolver_),
      ghost_(events_, resolver_, [this] {
          // Capture threads only set a flag and post one message; they never touch windows.
          if (!ghost_dirty_.exchange(true) && controller_) PostMessageW(controller_, kGhostMessage, 0, 0);
      }),
      focus_(resolver_,
             [this](FocusSnapshot snapshot) {
                 {
                     std::lock_guard lock(focus_mutex_);
                     pending_snapshot_ = std::move(snapshot);
                 }
                 if (controller_) PostMessageW(controller_, kFocusSnapshotMessage, 0, 0);
             },
             [this](FocusTreeUpdate update) {
                 {
                     std::lock_guard lock(focus_mutex_);
                     pending_trees_.push_back(std::move(update));
                 }
                 if (controller_) PostMessageW(controller_, kFocusTreeMessage, 0, 0);
             },
             [this](InspectMode mode) {
                 if (controller_) PostMessageW(controller_, kFocusModeMessage, static_cast<WPARAM>(mode), 0);
             }),
      window_(events_, settings_, resolver_, icons_, unlock_, focus_, *this),
      hud_(events_, settings_, icons_, *this) {}

App::~App() = default;

bool App::ForwardToRunningInstance(const CommandLine& cl) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    const bool exists = mutex && GetLastError() == ERROR_ALREADY_EXISTS;
    if (mutex) CloseHandle(mutex);   // the first instance keeps its own handle; we only probe here
    if (!exists) return false;

    HWND target = nullptr;
    for (int attempt = 0; attempt < 40 && !target; ++attempt) {   // the first instance may still be starting
        target = FindWindowW(kControllerClass, nullptr);
        if (!target) Sleep(50);
    }
    if (!target) return false;
    auto send = [&](wchar_t code, const std::wstring& path) {
        std::wstring payload(1, code);
        payload += path;
        COPYDATASTRUCT data{kCopyDataMagic, static_cast<DWORD>((payload.size() + 1) * sizeof(wchar_t)), payload.data()};
        SendMessageTimeoutW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG, 3000, nullptr);
    };
    if (cl.action == CliAction::WhoLocks || cl.action == CliAction::InspectFile) {
        for (const std::wstring& path : cl.paths) send(cl.action == CliAction::WhoLocks ? L'L' : L'I', path);
    } else {
        send(L'S', L"");   // plain launch: bring the window forward
    }
    return true;
}

int App::Run(HINSTANCE instance) {
    instance_ = instance;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);

    ComScope com(COINIT_APARTMENTTHREADED);   // UI thread: STA (shell dialogs). Capture threads create their own MTA.
    CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                         nullptr, EOAC_NONE, nullptr);   // process-wide; required before WMI subscriptions

    HANDLE instance_mutex = CreateMutexW(nullptr, FALSE, kMutexName);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ControllerProc;
    wc.hInstance = instance;
    wc.lpszClassName = kControllerClass;
    RegisterClassExW(&wc);
    // A hidden top-level window (not message-only) so it receives the TaskbarCreated broadcast.
    controller_ = CreateWindowExW(WS_EX_TOOLWINDOW, kControllerClass, L"Peek controller", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, this);
    if (!controller_) {
        MessageBoxW(nullptr, str::kErrMainWindow, str::kAppName, MB_ICONERROR);
        return 1;
    }
    taskbar_created_ = RegisterWindowMessageW(L"TaskbarCreated");

    if (!window_.Create(instance) || !hud_.Create(instance) || !overlay_.Create(instance)) {
        MessageBoxW(nullptr, str::kErrMainWindow, str::kAppName, MB_ICONERROR);
        return 1;
    }
    AddTrayIcon();
    ApplySettings();

    const Settings settings = settings_.Get();
    const bool investigate = command_line_.action == CliAction::WhoLocks || command_line_.action == CliAction::InspectFile;
    if (investigate) QueueInvestigation(command_line_.paths, command_line_.action == CliAction::InspectFile);
    else if (settings.general.start_with_window || command_line_.show_window) window_.Show();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (window_.Handle() && IsDialogMessageW(window_.Handle(), &msg)) continue;   // Tab / arrow navigation in pages
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Shutdown order: stop producers first, then destroy windows, then the tray icon.
    KillTimer(controller_, kThrottle);
    focus_.Stop();
    ghost_.Stop();
    overlay_.Destroy();
    hud_.Destroy();
    window_.Destroy();
    if (tray_added_) Shell_NotifyIconW(NIM_DELETE, &tray_);
    for (int id = 1; id <= 3; ++id) UnregisterHotKey(controller_, id);
    DestroyWindow(controller_);
    controller_ = nullptr;
    if (instance_mutex) CloseHandle(instance_mutex);
    return static_cast<int>(msg.wParam);
}

// ---------------------------------------------------------------------------------------------------

void App::Execute(Command command, std::uint64_t argument) {
    switch (command) {
        case Command::ShowInvestigation: window_.Show(static_cast<int>(static_cast<std::int64_t>(argument))); break;
        case Command::ShowEvent: window_.ShowEvent(argument); break;
        case Command::ShowSettings: window_.Show(static_cast<int>(MainWindow::Tab::Settings)); break;
        case Command::ToggleHud: settings_.Update([](Settings& s) { s.hud.visible = !s.hud.visible; }); ApplySettings(); break;
        case Command::ToggleHudMode:
            settings_.Update([](Settings& s) { s.hud.mode = s.hud.mode == HudMode::Compact ? HudMode::Expanded : HudMode::Compact; });
            ApplySettings();
            break;
        case Command::ToggleMonitoring: settings_.Update([](Settings& s) { s.ghost.enabled = !s.ghost.enabled; }); ApplySettings(); break;
        case Command::ToggleFocus:
            if (focus_.Active()) focus_.Stop();
            else if (settings_.Get().focus.enabled) focus_.StartLive();
            break;
        case Command::ClearEvents: events_.Clear(); RefreshViews(); break;
        case Command::EventsChanged: RefreshViews(); break;
        case Command::SettingsChanged: ApplySettings(); break;
        case Command::Exit: PostQuitMessage(0); break;
    }
}

std::wstring App::ShortcutStatus(int slot) const {
    return slot >= 0 && slot < 3 ? shortcut_status_[slot] : std::wstring();
}

void App::ApplySettings() {
    const Settings s = settings_.Get();
    events_.SetCapacity(static_cast<std::size_t>(s.ghost.retention));
    ApplyMonitoring(s);
    RegisterShortcuts(s);
    hud_.ApplySettings();
    window_.ApplySettings();
    if (!s.focus.enabled && focus_.Active()) focus_.Stop();
    UpdateStatus();
    UpdateTrayTip();
}

void App::ApplyMonitoring(const Settings& s) {
    ghost_.SetIgnoreNoise(s.ghost.ignore_noise);
    if (s.ghost.enabled && !monitoring_started_) {
        monitoring_started_ = ghost_.Start();
    } else if (!s.ghost.enabled && monitoring_started_) {
        ghost_.Stop();
        monitoring_started_ = false;
    }
    if (monitoring_started_ && s.ghost.trace != applied_trace_) ghost_.SetTrace(s.ghost.trace);
    applied_trace_ = s.ghost.trace;
}

void App::RegisterShortcuts(const Settings& s) {
    for (int id = 1; id <= 3; ++id) UnregisterHotKey(controller_, id);
    const ShortcutSetting* settings[3] = {&s.shortcuts.activate, &s.shortcuts.ghost, &s.shortcuts.focus};
    HotkeySpec registered[3]{};
    for (int i = 0; i < 3; ++i) {
        shortcut_status_[i] = str::kSetShortcutOff;
        if (!settings[i]->enabled) continue;
        HotkeySpec spec;
        if (!ParseHotkey(settings[i]->key, spec)) { shortcut_status_[i] = str::kSetShortcutInvalid; continue; }
        bool duplicate = false;
        for (int j = 0; j < i; ++j) if (registered[j].Valid() && registered[j] == spec) duplicate = true;
        if (duplicate) { shortcut_status_[i] = str::kSetShortcutDuplicate; continue; }
        if (RegisterHotKey(controller_, i + 1, spec.modifiers | MOD_NOREPEAT, spec.vk)) {
            registered[i] = spec;
            shortcut_status_[i] = str::kSetShortcutOk;
        } else {
            shortcut_status_[i] = str::kSetShortcutTaken;   // another application owns this combination
        }
    }
}

void App::RefreshViews() {
    hud_.EventsChanged();
    window_.EventsChanged();
    UpdateStatus();
}

void App::UpdateStatus() {
    const Settings s = settings_.Get();
    const GhostStatus status = ghost_.Status();
    std::wstring text;
    bool degraded = false;
    if (!s.ghost.enabled) text = str::kHudPaused;
    else if (!monitoring_started_ || !status.message.empty()) {
        degraded = true;
        text = status.message.empty() ? std::wstring(str::kStatusStopped) : status.message;
    } else {
        text = std::wstring(str::kHudMonitoring) + L" · " + std::to_wstring(events_.Size()) + L" " + str::kEventsShown;
        if (ghost_.TraceActive()) {
            const int left = ghost_.TraceSecondsLeft();
            text += left >= 0 ? L" · trace " + std::to_wstring(left) + L"s" : L" · trace on";
        }
    }
    hud_.SetStatus(text, s.ghost.enabled && monitoring_started_, degraded);
    // A one-second status tick is only needed while a timed trace counts down.
    if (ghost_.TraceActive() && ghost_.TraceSecondsLeft() >= 0) SetTimer(controller_, kStatus, 1000, nullptr);
    else KillTimer(controller_, kStatus);
}

// ---------------------------------------------------------------------------------------------------
// Tray

void App::AddTrayIcon() {
    tray_ = {};
    tray_.cbSize = sizeof(tray_);
    tray_.hWnd = controller_;
    tray_.uID = 1;
    tray_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    tray_.uCallbackMessage = kTrayMessage;
    tray_.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(101));
    lstrcpynW(tray_.szTip, L"Peek", static_cast<int>(std::size(tray_.szTip)));
    tray_added_ = Shell_NotifyIconW(NIM_ADD, &tray_) != FALSE;
}

void App::UpdateTrayTip() {
    if (!tray_added_) return;
    const Settings s = settings_.Get();
    const wchar_t* tip = s.ghost.enabled ? L"Peek — monitoring" : L"Peek — monitoring paused";
    lstrcpynW(tray_.szTip, tip, static_cast<int>(std::size(tray_.szTip)));
    tray_.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &tray_);
}

void App::ShowTrayMenu() {
    const Settings s = settings_.Get();
    HMENU menu = CreatePopupMenu();
    HMENU trace = CreatePopupMenu();
    AppendMenuW(trace, MF_STRING, kTrayTraceOff, str::kSetTraceOff);
    AppendMenuW(trace, MF_STRING, kTrayTrace30, str::kSetTrace30);
    AppendMenuW(trace, MF_STRING, kTrayTrace5, str::kSetTrace5m);
    AppendMenuW(trace, MF_STRING, kTrayTraceOn, str::kSetTraceOn);
    CheckMenuRadioItem(trace, kTrayTraceOff, kTrayTraceOn, kTrayTraceOff + static_cast<UINT>(s.ghost.trace), MF_BYCOMMAND);
    AppendMenuW(menu, MF_STRING, kTrayOpen, str::kMenuOpenInvestigation);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (s.hud.visible ? MF_CHECKED : 0), kTrayHud, str::kMenuHudVisible);
    AppendMenuW(menu, MF_STRING | (s.hud.mode == HudMode::Expanded ? MF_CHECKED : 0), kTrayExpanded, str::kMenuHudExpanded);
    AppendMenuW(menu, MF_STRING | (s.ghost.enabled ? MF_CHECKED : 0), kTrayMonitor, str::kMenuMonitoring);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(trace), str::kSetTrace);
    AppendMenuW(menu, MF_STRING | (focus_.Active() ? MF_CHECKED : 0) | (s.focus.enabled ? 0 : MF_GRAYED), kTrayFocus, str::kMenuInspectElement);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kTraySettings, str::kMenuSettings);
    AppendMenuW(menu, MF_STRING, kTrayExit, str::kMenuExit);

    POINT p{};
    GetCursorPos(&p);
    SetForegroundWindow(controller_);   // required so the menu closes when the user clicks elsewhere
    const UINT command = static_cast<UINT>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, p.x, p.y, 0, controller_, nullptr));
    PostMessageW(controller_, WM_NULL, 0, 0);
    DestroyMenu(menu);   // also destroys the submenu
    switch (command) {
        case kTrayOpen: window_.Show(); break;
        case kTrayHud: Execute(Command::ToggleHud); break;
        case kTrayExpanded: Execute(Command::ToggleHudMode); break;
        case kTrayMonitor: Execute(Command::ToggleMonitoring); break;
        case kTrayFocus: Execute(Command::ToggleFocus); break;
        case kTrayTraceOff: case kTrayTrace30: case kTrayTrace5: case kTrayTraceOn:
            settings_.Update([&](Settings& st) { st.ghost.trace = static_cast<TraceMode>(command - kTrayTraceOff); });
            // Re-selecting a timed mode restarts its period.
            if (monitoring_started_) ghost_.SetTrace(static_cast<TraceMode>(command - kTrayTraceOff));
            ApplySettings();
            break;
        case kTraySettings: Execute(Command::ShowSettings); break;
        case kTrayExit: Execute(Command::Exit); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------------
// Investigations from Explorer, Focus highlight, message handling

void App::QueueInvestigation(const std::vector<std::wstring>& paths, bool inspect) {
    // Explorer starts one process per selected file; wait briefly so they merge into one investigation.
    pending_paths_.insert(pending_paths_.end(), paths.begin(), paths.end());
    pending_inspect_ = pending_inspect_ || inspect;
    SetTimer(controller_, kInvestigate, 400, nullptr);
}

void App::HandleCopyData(const COPYDATASTRUCT& data) {
    if (data.dwData != kCopyDataMagic || data.cbData < sizeof(wchar_t) || !data.lpData) return;
    const wchar_t* text = static_cast<const wchar_t*>(data.lpData);
    const std::size_t length = data.cbData / sizeof(wchar_t);
    std::wstring payload(text, length);
    while (!payload.empty() && payload.back() == L'\0') payload.pop_back();
    if (payload.empty()) return;
    const wchar_t code = payload[0];
    const std::wstring path = payload.substr(1);
    if (code == L'S') window_.Show();
    else if ((code == L'L' || code == L'I') && !path.empty()) QueueInvestigation({path}, code == L'I');
}

void App::ShowHighlight(const FocusSnapshot& s) {
    const Settings settings = settings_.Get();
    if (!s.element_alive || IsRectEmpty(&s.highlight)) { overlay_.Hide(); return; }
    std::uint32_t rgb = 0x3985FF;
    (void)ParseColor(settings.focus.highlight_color, rgb);
    const COLORREF color = RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    const int duration = focus_mode_ == InspectMode::Live ? settings.focus.highlight_duration_ms : 0;   // frozen/pinned stay visible
    overlay_.Show(s.highlight, color, settings.focus.highlight_width, duration);
}

LRESULT CALLBACK App::ControllerProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    App* self = nullptr;
    if (message == WM_NCCREATE) {
        self = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(message, wparam, lparam) : DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT App::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == taskbar_created_ && taskbar_created_ != 0) {
        AddTrayIcon();   // Explorer restarted
        return 0;
    }
    switch (message) {
        case kTrayMessage:
            if (LOWORD(lparam) == WM_LBUTTONUP || LOWORD(lparam) == WM_LBUTTONDBLCLK) window_.Show();
            else if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_CONTEXTMENU) ShowTrayMenu();
            return 0;

        case kGhostMessage:
            // Coalesce bursts: refresh now, then at most every 100 ms while events keep coming.
            ghost_dirty_ = false;
            if (!throttle_pending_) {
                RefreshViews();
                throttle_pending_ = true;
                SetTimer(controller_, kThrottle, 100, nullptr);
            }
            return 0;

        case WM_TIMER:
            if (wparam == kThrottle) {
                KillTimer(controller_, kThrottle);
                throttle_pending_ = false;
                if (ghost_dirty_) PostMessageW(controller_, kGhostMessage, 0, 0);
                else RefreshViews();
            } else if (wparam == kInvestigate) {
                KillTimer(controller_, kInvestigate);
                const std::vector<std::wstring> paths = std::move(pending_paths_);
                const bool inspect = pending_inspect_;
                pending_paths_.clear();
                pending_inspect_ = false;
                if (!paths.empty()) window_.ShowUnlock(paths, inspect);
            } else if (wparam == kStatus) {
                UpdateStatus();
            }
            return 0;

        case kFocusSnapshotMessage: {
            std::optional<FocusSnapshot> snapshot;
            {
                std::lock_guard lock(focus_mutex_);
                snapshot = std::move(pending_snapshot_);
                pending_snapshot_.reset();
            }
            if (snapshot && focus_.Active()) {
                last_snapshot_ = *snapshot;
                ShowHighlight(*snapshot);
                window_.DeliverFocus(*snapshot);
            }
            return 0;
        }
        case kFocusTreeMessage: {
            std::vector<FocusTreeUpdate> updates;
            {
                std::lock_guard lock(focus_mutex_);
                updates.swap(pending_trees_);
            }
            for (const FocusTreeUpdate& update : updates) window_.DeliverTree(update);
            return 0;
        }
        case kFocusModeMessage: {
            focus_mode_ = static_cast<InspectMode>(wparam);
            if (focus_mode_ == InspectMode::Off) { overlay_.Hide(); last_snapshot_.reset(); }
            else if (last_snapshot_) ShowHighlight(*last_snapshot_);   // Frozen/Pinned: keep the frame, drop the auto-hide
            window_.FocusModeChanged(focus_mode_);
            return 0;
        }

        case WM_HOTKEY:
            if (wparam == 1) window_.Show();
            else if (wparam == 2) window_.Show(static_cast<int>(MainWindow::Tab::Ghost));
            else if (wparam == 3) Execute(Command::ToggleFocus);
            return 0;

        case WM_COPYDATA:
            if (lparam) HandleCopyData(*reinterpret_cast<COPYDATASTRUCT*>(lparam));
            return TRUE;

        case WM_QUERYENDSESSION: return TRUE;
        case WM_ENDSESSION:
            if (wparam) PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(controller_, message, wparam, lparam);
}

} // namespace peek
