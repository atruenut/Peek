#include "ui/settings_page.h"

#include "core/format.h"
#include "core/strings.h"
#include "shell/shell_integration.h"

#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>

namespace peek {
namespace {
std::wstring ExePath() {
    wchar_t path[MAX_PATH * 2]{};
    GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    return path;
}
}

void SettingsPage::OnCreate() {
    auto heading = [&](const wchar_t* text) { return MakeLabel(text, SS_LEFT); };
    h_general_ = heading(str::kSetGeneralSection);
    l_theme_ = MakeLabel(str::kSetTheme);
    theme_ = MakeCombo(kTheme);
    FillCombo(theme_, {str::kSetThemeSystem, str::kSetThemeLight, str::kSetThemeDark}, 0);
    l_close_ = MakeLabel(str::kSetCloseButton);
    close_ = MakeCombo(kClose);
    FillCombo(close_, {str::kSetCloseHide, str::kSetCloseExit}, 0);
    start_window_ = MakeCheck(str::kSetStartWithWindow, kStartWindow);

    h_hud_ = heading(str::kSetHudSection);
    hud_visible_ = MakeCheck(str::kSetHudVisible, kHudVisible);
    hud_expanded_ = MakeCheck(str::kSetHudExpanded, kHudExpanded);
    click_through_ = MakeCheck(str::kSetClickThrough, kClickThrough);
    l_opacity_ = MakeLabel(str::kSetOpacity);
    opacity_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 0, 0, 10, 10,
                               hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOpacity)), instance_, nullptr);
    SendMessageW(opacity_, TBM_SETRANGE, TRUE, MAKELPARAM(30, 100));
    opacity_value_ = MakeLabel(L"");
    l_corner_ = MakeLabel(str::kSetCorner);
    corner_ = MakeCombo(kCorner);
    FillCombo(corner_, {str::kSetTopLeft, str::kSetTopRight, str::kSetBottomLeft, str::kSetBottomRight}, 1);
    l_monitor_ = MakeLabel(str::kSetMonitor);
    monitor_ = MakeCombo(kMonitor);
    SendMessageW(monitor_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(str::kSetPrimaryMonitor));
    monitors_ = {"primary"};
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR m, HDC, LPRECT, LPARAM data) -> BOOL {
        auto* self = reinterpret_cast<SettingsPage*>(data);
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(m, &info) && !(info.dwFlags & MONITORINFOF_PRIMARY)) {
            self->monitors_.push_back(WideToUtf8(info.szDevice));
            SendMessageW(self->monitor_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(info.szDevice));
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));
    l_fullscreen_ = MakeLabel(str::kSetFullscreen);
    fullscreen_ = MakeCombo(kFullscreen);
    FillCombo(fullscreen_, {str::kSetFullscreenHide, str::kSetFullscreenShow}, 0);

    h_ghost_ = heading(str::kSetGhostSection);
    ghost_enabled_ = MakeCheck(str::kSetGhostEnabled, kGhostEnabled);
    l_retention_ = MakeLabel(str::kSetRetention);
    retention_ = MakeCombo(kRetention);
    l_trace_ = MakeLabel(str::kSetTrace);
    trace_ = MakeCombo(kTrace);
    FillCombo(trace_, {str::kSetTraceOff, str::kSetTrace30, str::kSetTrace5m, str::kSetTraceOn}, 0);
    noise_ = MakeCheck(str::kSetIgnoreNoise, kNoise);

    h_unlock_ = heading(str::kSetUnlockSection);
    reg_ = MakeButton(str::kSetRegisterShell, kRegister);
    unreg_ = MakeButton(str::kSetUnregisterShell, kUnregister);
    shell_status_ = MakeLabel(L"");
    l_depth_ = MakeLabel(str::kSetDepth);
    depth_ = MakeEdit(kDepth, ES_NUMBER);
    l_files_ = MakeLabel(str::kSetMaxFiles);
    files_ = MakeEdit(kMaxFiles, ES_NUMBER);

    h_focus_ = heading(str::kSetFocusSection);
    focus_enabled_ = MakeCheck(str::kSetFocusEnabled, kFocusEnabled);
    l_width_ = MakeLabel(str::kSetHighlightWidth);
    width_ = MakeEdit(kWidth, ES_NUMBER);
    l_color_ = MakeLabel(str::kSetHighlightColor);
    color_ = MakeButton(str::kSetHighlightColorBtn, kColor);
    l_duration_ = MakeLabel(str::kSetHighlightDuration);
    duration_ = MakeEdit(kDuration, ES_NUMBER);

    h_shortcuts_ = heading(str::kSetShortcutSection);
    const wchar_t* names[3] = {str::kSetShortcutActivate, str::kSetShortcutGhost, str::kSetShortcutFocus};
    for (int i = 0; i < 3; ++i) {
        sc_check_[i] = MakeCheck(names[i], kScActivate + i * 2);
        sc_key_[i] = MakeEdit(kScActivateKey + i * 2);
        sc_status_[i] = MakeLabel(L"");
    }
    apply_sc_ = MakeButton(str::kSetApplyShortcuts, kApplyShortcuts);

    h_file_ = heading(str::kSetConfigFile);
    file_path_ = MakeLabel(L"", SS_PATHELLIPSIS);
    open_folder_ = MakeButton(str::kSetOpenFolder, kOpenFolder);
    SettingsChanged();
}

void SettingsPage::OnLayout(int width, int height) {
    (void)height;
    const int m = Px(12), gap = Px(6), row = Px(24);
    const int col_w = (width - 3 * m) / 2;
    const int label_w = Px(150);
    int y = m;
    int x = m;
    auto head = [&](HWND h) { y += Px(4); MoveWindow(h, x, y, col_w, row, TRUE); y += row; };
    auto wide = [&](HWND h) { MoveWindow(h, x, y, col_w, row, TRUE); y += row + gap / 2; };
    auto pair = [&](HWND l, HWND c, int cw = 0) {
        MoveWindow(l, x, y + Px(4), label_w, row - Px(4), TRUE);
        MoveWindow(c, x + label_w, y, cw ? cw : col_w - label_w, Px(240), TRUE);
        y += row + gap / 2;
    };
    head(h_general_);
    pair(l_theme_, theme_); pair(l_close_, close_); wide(start_window_);
    head(h_hud_);
    wide(hud_visible_); wide(hud_expanded_); wide(click_through_);
    MoveWindow(l_opacity_, x, y + Px(4), label_w, row - Px(4), TRUE);
    MoveWindow(opacity_, x + label_w, y, col_w - label_w - Px(50), row, TRUE);
    MoveWindow(opacity_value_, x + col_w - Px(46), y + Px(4), Px(46), row - Px(4), TRUE);
    y += row + gap / 2;
    pair(l_corner_, corner_); pair(l_monitor_, monitor_); pair(l_fullscreen_, fullscreen_);
    head(h_ghost_);
    wide(ghost_enabled_); pair(l_retention_, retention_); pair(l_trace_, trace_); wide(noise_);

    x = 2 * m + col_w;
    y = m;
    head(h_unlock_);
    MoveWindow(reg_, x, y, Px(170), row, TRUE);
    MoveWindow(unreg_, x + Px(176), y, Px(150), row, TRUE);
    y += row + gap / 2;
    MoveWindow(shell_status_, x, y, col_w, row * 2, TRUE);
    y += row * 2 + gap / 2;
    pair(l_depth_, depth_, Px(70)); pair(l_files_, files_, Px(90));
    head(h_focus_);
    wide(focus_enabled_); pair(l_width_, width_, Px(70)); pair(l_color_, color_, Px(100)); pair(l_duration_, duration_, Px(90));
    head(h_shortcuts_);
    for (int i = 0; i < 3; ++i) {
        MoveWindow(sc_check_[i], x, y, Px(170), row, TRUE);
        MoveWindow(sc_key_[i], x + Px(176), y, Px(130), row, TRUE);
        MoveWindow(sc_status_[i], x + Px(312), y + Px(4), std::max(Px(40), col_w - Px(312)), row - Px(4), TRUE);
        y += row + gap / 2;
    }
    MoveWindow(apply_sc_, x, y, Px(150), row, TRUE);
    y += row + gap;
    head(h_file_);
    MoveWindow(file_path_, x, y, col_w, row, TRUE);
    y += row + gap / 2;
    MoveWindow(open_folder_, x, y, Px(130), row, TRUE);
}

void SettingsPage::SettingsChanged() {
    const Settings s = ctx_.settings.Get();
    loading_ = true;
    SendMessageW(theme_, CB_SETCURSEL, static_cast<WPARAM>(s.general.theme), 0);
    SendMessageW(close_, CB_SETCURSEL, static_cast<WPARAM>(s.general.close_button), 0);
    SendMessageW(start_window_, BM_SETCHECK, s.general.start_with_window, 0);
    SendMessageW(hud_visible_, BM_SETCHECK, s.hud.visible, 0);
    SendMessageW(hud_expanded_, BM_SETCHECK, s.hud.mode == HudMode::Expanded, 0);
    SendMessageW(click_through_, BM_SETCHECK, s.hud.click_through, 0);
    SendMessageW(opacity_, TBM_SETPOS, TRUE, s.hud.opacity);
    SetText(opacity_value_, std::to_wstring(s.hud.opacity) + L"%");
    SendMessageW(corner_, CB_SETCURSEL, static_cast<WPARAM>(s.hud.anchor), 0);
    int monitor = 0;
    for (std::size_t i = 0; i < monitors_.size(); ++i) if (monitors_[i] == s.hud.monitor) monitor = static_cast<int>(i);
    SendMessageW(monitor_, CB_SETCURSEL, static_cast<WPARAM>(monitor), 0);
    SendMessageW(fullscreen_, CB_SETCURSEL, static_cast<WPARAM>(s.hud.fullscreen), 0);
    SendMessageW(ghost_enabled_, BM_SETCHECK, s.ghost.enabled, 0);
    // Offer the presets, plus the current value if the INI was edited by hand.
    SendMessageW(retention_, CB_RESETCONTENT, 0, 0);
    int selected = 0, index = 0;
    std::vector<int> values(std::begin(kRetentionPresets), std::end(kRetentionPresets));
    if (std::find(values.begin(), values.end(), s.ghost.retention) == values.end()) { values.push_back(s.ghost.retention); std::sort(values.begin(), values.end()); }
    for (int v : values) {
        const std::wstring text = std::to_wstring(v) + L" events";
        SendMessageW(retention_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        SendMessageW(retention_, CB_SETITEMDATA, static_cast<WPARAM>(index), v);
        if (v == s.ghost.retention) selected = index;
        ++index;
    }
    SendMessageW(retention_, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
    SendMessageW(trace_, CB_SETCURSEL, static_cast<WPARAM>(s.ghost.trace), 0);
    SendMessageW(noise_, BM_SETCHECK, s.ghost.ignore_noise, 0);
    SetText(depth_, std::to_wstring(s.unlock.max_depth));
    SetText(files_, std::to_wstring(s.unlock.max_files));
    SendMessageW(focus_enabled_, BM_SETCHECK, s.focus.enabled, 0);
    SetText(width_, std::to_wstring(s.focus.highlight_width));
    SetText(duration_, std::to_wstring(s.focus.highlight_duration_ms));
    const ShortcutSetting* sc[3] = {&s.shortcuts.activate, &s.shortcuts.ghost, &s.shortcuts.focus};
    for (int i = 0; i < 3; ++i) {
        SendMessageW(sc_check_[i], BM_SETCHECK, sc[i]->enabled, 0);
        SetText(sc_key_[i], Utf8ToWide(sc[i]->key));
    }
    SetText(file_path_, ctx_.settings.Path() + (ctx_.settings.IsPortable() ? L"  (portable)" : L"  (user profile)"));
    loading_ = false;
    UpdateShellStatus();
    UpdateShortcutStatus();
}

void SettingsPage::UpdateShellStatus() {
    const auto status = ShellIntegration::Query(ExePath());
    std::wstring text = str::kShellNotRegistered;
    if (status.registered && status.points_here) text = str::kShellRegistered;
    else if (status.registered) text = std::wstring(str::kShellOtherExe) + status.registered_exe;
    SetText(shell_status_, text);
}

void SettingsPage::UpdateShortcutStatus() {
    for (int i = 0; i < 3; ++i) SetText(sc_status_[i], ctx_.host.ShortcutStatus(i));
}

void SettingsPage::Save(const std::function<void(Settings&)>& change) {
    if (loading_) return;
    if (!ctx_.settings.Update(change)) {
        MessageBoxW(hwnd_, (std::wstring(str::kErrSettingsSave) + L"\n" + ctx_.settings.LastError()).c_str(), str::kAppName, MB_ICONWARNING);
    }
    ctx_.host.Execute(Command::SettingsChanged);
}

void SettingsPage::ReadNumbers() {
    auto number = [&](HWND edit, int fallback) {
        const std::wstring text = GetText(edit);
        return text.empty() ? fallback : _wtoi(text.c_str());
    };
    const int depth = number(depth_, 2), files = number(files_, 500), width = number(width_, 3), duration = number(duration_, 0);
    Save([&](Settings& s) {
        s.unlock.max_depth = depth; s.unlock.max_files = files;
        s.focus.highlight_width = width; s.focus.highlight_duration_ms = duration;
    });
}

void SettingsPage::OnMessage(UINT message, WPARAM, LPARAM lparam) {
    if (message == WM_HSCROLL && reinterpret_cast<HWND>(lparam) == opacity_) {
        const int value = static_cast<int>(SendMessageW(opacity_, TBM_GETPOS, 0, 0));
        SetText(opacity_value_, std::to_wstring(value) + L"%");
        Save([&](Settings& s) { s.hud.opacity = value; });
    }
}

void SettingsPage::OnCommand(int id, int code, HWND) {
    if (loading_) return;
    const bool clicked = code == BN_CLICKED;
    const bool changed = code == CBN_SELCHANGE;
    switch (id) {
        case kTheme: if (changed) Save([&](Settings& s) { s.general.theme = static_cast<ThemeChoice>(Sel(theme_)); }); break;
        case kClose: if (changed) Save([&](Settings& s) { s.general.close_button = static_cast<CloseButton>(Sel(close_)); }); break;
        case kStartWindow: if (clicked) Save([&](Settings& s) { s.general.start_with_window = Checked(start_window_); }); break;
        case kHudVisible: if (clicked) Save([&](Settings& s) { s.hud.visible = Checked(hud_visible_); }); break;
        case kHudExpanded: if (clicked) Save([&](Settings& s) { s.hud.mode = Checked(hud_expanded_) ? HudMode::Expanded : HudMode::Compact; }); break;
        case kClickThrough: if (clicked) Save([&](Settings& s) { s.hud.click_through = Checked(click_through_); }); break;
        case kCorner: if (changed) Save([&](Settings& s) { s.hud.anchor = static_cast<HudAnchor>(std::clamp(Sel(corner_), 0, 3)); }); break;
        case kMonitor:
            if (changed) Save([&](Settings& s) {
                const std::size_t i = static_cast<std::size_t>(std::max(0, Sel(monitor_)));
                s.hud.monitor = i < monitors_.size() ? monitors_[i] : "primary";
            });
            break;
        case kFullscreen: if (changed) Save([&](Settings& s) { s.hud.fullscreen = static_cast<FullscreenBehavior>(Sel(fullscreen_)); }); break;
        case kGhostEnabled: if (clicked) Save([&](Settings& s) { s.ghost.enabled = Checked(ghost_enabled_); }); break;
        case kRetention:
            if (changed) Save([&](Settings& s) {
                s.ghost.retention = static_cast<int>(SendMessageW(retention_, CB_GETITEMDATA, static_cast<WPARAM>(Sel(retention_)), 0));
            });
            break;
        case kTrace: if (changed) Save([&](Settings& s) { s.ghost.trace = static_cast<TraceMode>(Sel(trace_)); }); break;
        case kNoise: if (clicked) Save([&](Settings& s) { s.ghost.ignore_noise = Checked(noise_); }); break;
        case kFocusEnabled: if (clicked) Save([&](Settings& s) { s.focus.enabled = Checked(focus_enabled_); }); break;
        case kDepth: case kMaxFiles: case kWidth: case kDuration:
            if (code == EN_KILLFOCUS) ReadNumbers();
            break;
        case kColor:
            if (clicked) {
                std::uint32_t rgb = 0;
                (void)ParseColor(ctx_.settings.Get().focus.highlight_color, rgb);
                CHOOSECOLORW choose{};
                choose.lStructSize = sizeof(choose);
                choose.hwndOwner = hwnd_;
                choose.rgbResult = RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
                choose.lpCustColors = custom_colors_;
                choose.Flags = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColorW(&choose)) {
                    const std::uint32_t value = (GetRValue(choose.rgbResult) << 16) | (GetGValue(choose.rgbResult) << 8) | GetBValue(choose.rgbResult);
                    Save([&](Settings& s) { s.focus.highlight_color = FormatColor(value); });
                }
            }
            break;
        case kRegister:
            if (clicked) {
                std::wstring error;
                if (!ShellIntegration::Register(ExePath(), error)) MessageBoxW(hwnd_, error.c_str(), str::kAppName, MB_ICONWARNING);
                Save([&](Settings& s) { s.unlock.explorer_integration = ShellIntegration::Query(ExePath()).registered; });
            }
            break;
        case kUnregister:
            if (clicked) {
                std::wstring error;
                if (!ShellIntegration::Unregister(error)) MessageBoxW(hwnd_, error.c_str(), str::kAppName, MB_ICONWARNING);
                Save([&](Settings& s) { s.unlock.explorer_integration = ShellIntegration::Query(ExePath()).registered; });
            }
            break;
        case kApplyShortcuts:
            if (clicked) {
                const std::string keys[3] = {WideToUtf8(GetText(sc_key_[0])), WideToUtf8(GetText(sc_key_[1])), WideToUtf8(GetText(sc_key_[2]))};
                Save([&](Settings& s) {
                    s.shortcuts.activate = {Checked(sc_check_[0]), keys[0]};
                    s.shortcuts.ghost = {Checked(sc_check_[1]), keys[1]};
                    s.shortcuts.focus = {Checked(sc_check_[2]), keys[2]};
                });
            }
            break;
        case kOpenFolder:
            if (clicked) RevealInExplorer(hwnd_, ctx_.settings.Path());
            break;
    }
}

} // namespace peek
