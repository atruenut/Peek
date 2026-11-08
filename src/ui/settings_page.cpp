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
    // A control and its label share one tooltip so hovering either explains the option.
    auto tip = [&](HWND label, HWND control, const wchar_t* text) { Tip(control, text); if (label) Tip(label, text); };

    h_general_ = MakeHeading(str::kSetGeneralSection);
    hint_general_ = MakeHint(str::kSetGeneralHint);
    l_theme_ = MakeLabel(str::kSetTheme);
    theme_ = MakeCombo(kTheme);
    FillCombo(theme_, {str::kSetThemeSystem, str::kSetThemeLight, str::kSetThemeDark}, 0);
    tip(l_theme_, theme_, str::kTipTheme);
    l_close_ = MakeLabel(str::kSetCloseButton);
    close_ = MakeCombo(kClose);
    FillCombo(close_, {str::kSetCloseHide, str::kSetCloseExit}, 0);
    tip(l_close_, close_, str::kTipClose);
    start_window_ = MakeCheck(str::kSetStartWithWindow, kStartWindow);
    Tip(start_window_, str::kTipStartWindow);

    h_hud_ = MakeHeading(str::kSetHudSection);
    hint_hud_ = MakeHint(str::kSetHudHint);
    hud_visible_ = MakeCheck(str::kSetHudVisible, kHudVisible);
    Tip(hud_visible_, str::kTipHudVisible);
    hud_expanded_ = MakeCheck(str::kSetHudExpanded, kHudExpanded);
    Tip(hud_expanded_, str::kTipHudExpanded);
    click_through_ = MakeCheck(str::kSetClickThrough, kClickThrough);
    Tip(click_through_, str::kTipClickThrough);
    l_opacity_ = MakeLabel(str::kSetOpacity);
    opacity_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 0, 0, 10, 10,
                               hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOpacity)), instance_, nullptr);
    SendMessageW(opacity_, TBM_SETRANGE, TRUE, MAKELPARAM(30, 100));
    opacity_value_ = MakeLabel(L"", SS_RIGHT);
    tip(l_opacity_, opacity_, str::kTipOpacity);
    l_corner_ = MakeLabel(str::kSetCorner);
    corner_ = MakeCombo(kCorner);
    FillCombo(corner_, {str::kSetTopLeft, str::kSetTopRight, str::kSetBottomLeft, str::kSetBottomRight}, 1);
    tip(l_corner_, corner_, str::kTipCorner);
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
    tip(l_monitor_, monitor_, str::kTipMonitor);
    l_fullscreen_ = MakeLabel(str::kSetFullscreen);
    fullscreen_ = MakeCombo(kFullscreen);
    FillCombo(fullscreen_, {str::kSetFullscreenHide, str::kSetFullscreenShow}, 0);
    tip(l_fullscreen_, fullscreen_, str::kTipFullscreen);

    h_ghost_ = MakeHeading(str::kSetGhostSection);
    hint_ghost_ = MakeHint(str::kSetGhostHint);
    ghost_enabled_ = MakeCheck(str::kSetGhostEnabled, kGhostEnabled);
    Tip(ghost_enabled_, str::kTipGhostEnabled);
    l_retention_ = MakeLabel(str::kSetRetention);
    retention_ = MakeCombo(kRetention);
    tip(l_retention_, retention_, str::kTipRetention);
    l_trace_ = MakeLabel(str::kSetTrace);
    trace_ = MakeCombo(kTrace);
    FillCombo(trace_, {str::kSetTraceOff, str::kSetTrace30, str::kSetTrace5m, str::kSetTraceOn}, 0);
    tip(l_trace_, trace_, str::kTipTrace);
    noise_ = MakeCheck(str::kSetIgnoreNoise, kNoise);
    Tip(noise_, str::kTipNoise);

    h_about_ = MakeHeading(str::kSetAboutSection);
    about_text_ = MakeHint((std::wstring(str::kAppName) + L" " + str::kAppVersion + L"\r\n" + str::kSetAboutText).c_str());
    help_ = MakeButton(str::kSetShowHelp, kHelp);

    h_unlock_ = MakeHeading(str::kSetUnlockSection);
    hint_unlock_ = MakeHint(str::kSetUnlockHint);
    reg_ = MakeButton(str::kSetRegisterShell, kRegister);
    Tip(reg_, str::kTipRegister);
    unreg_ = MakeButton(str::kSetUnregisterShell, kUnregister);
    Tip(unreg_, str::kTipUnregister);
    shell_status_ = MakeLabel(L"");
    l_depth_ = MakeLabel(str::kSetDepth);
    depth_ = MakeEdit(kDepth, ES_NUMBER);
    tip(l_depth_, depth_, str::kTipDepth);
    l_files_ = MakeLabel(str::kSetMaxFiles);
    files_ = MakeEdit(kMaxFiles, ES_NUMBER);
    tip(l_files_, files_, str::kTipMaxFiles);

    h_focus_ = MakeHeading(str::kSetFocusSection);
    hint_focus_ = MakeHint(str::kSetFocusHint);
    focus_enabled_ = MakeCheck(str::kSetFocusEnabled, kFocusEnabled);
    Tip(focus_enabled_, str::kTipFocusEnabled);
    l_width_ = MakeLabel(str::kSetHighlightWidth);
    width_ = MakeEdit(kWidth, ES_NUMBER);
    tip(l_width_, width_, str::kTipWidth);
    l_color_ = MakeLabel(str::kSetHighlightColor);
    color_swatch_ = MakeLabel(L"", SS_NOTIFY | WS_BORDER);
    color_ = MakeButton(str::kSetHighlightColorBtn, kColor);
    color_text_ = MakeLabel(L"");
    tip(l_color_, color_, str::kTipColor);
    Tip(color_swatch_, str::kTipColor);
    l_duration_ = MakeLabel(str::kSetHighlightDuration);
    duration_ = MakeEdit(kDuration, ES_NUMBER);
    tip(l_duration_, duration_, str::kTipDuration);

    h_shortcuts_ = MakeHeading(str::kSetShortcutSection);
    hint_shortcuts_ = MakeHint(str::kSetShortcutHint);
    const wchar_t* names[3] = {str::kSetShortcutActivate, str::kSetShortcutGhost, str::kSetShortcutFocus};
    for (int i = 0; i < 3; ++i) {
        sc_check_[i] = MakeCheck(names[i], kScActivate + i * 2);
        Tip(sc_check_[i], str::kTipShortcutCheck);
        sc_key_[i] = MakeEdit(kScActivateKey + i * 2);
        SendMessageW(sc_key_[i], EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(str::kSetShortcutCue));
        Tip(sc_key_[i], str::kTipShortcutKey);
        sc_status_[i] = MakeLabel(L"");
    }
    apply_sc_ = MakeButton(str::kSetApplyShortcuts, kApplyShortcuts);
    Tip(apply_sc_, str::kTipApply);

    h_file_ = MakeHeading(str::kSetConfigFile);
    hint_file_ = MakeHint(str::kSetFileHint);
    file_path_ = MakeLabel(L"", SS_PATHELLIPSIS);
    open_folder_ = MakeButton(str::kSetOpenFolder, kOpenFolder);
    Tip(open_folder_, str::kTipOpenFolder);
    reset_ = MakeButton(str::kSetResetDefaults, kReset);
    Tip(reset_, str::kTipReset);
    SettingsChanged();
}

// Runs the layout until the client size is stable: showing or hiding the scroll bar changes the width.
void SettingsPage::OnLayout(int width, int height) {
    if (in_layout_) return;
    in_layout_ = true;
    for (int pass = 0; pass < 3; ++pass) {
        int content = 0;
        LayoutOnce(width, height, content);
        const int max_scroll = std::max(0, content - height);
        const int wanted = std::clamp(scroll_y_, 0, max_scroll);
        SCROLLINFO info{};
        info.cbSize = sizeof(info);
        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin = 0;
        info.nMax = std::max(0, content - 1);
        info.nPage = static_cast<UINT>(height);
        info.nPos = wanted;
        SetScrollInfo(hwnd_, SB_VERT, &info, TRUE);
        RECT client{};
        GetClientRect(hwnd_, &client);
        const bool moved = wanted != scroll_y_;
        scroll_y_ = wanted;
        if (!moved && client.right == width && client.bottom == height) break;
        width = client.right;
        height = client.bottom;
    }
    in_layout_ = false;
}

void SettingsPage::LayoutOnce(int width, int height, int& content_height) {
    (void)height;
    const int m = Px(12), gap = Px(6), row = Px(24), step = row + gap / 2;
    const int col_w = std::max(Px(260), (width - 3 * m) / 2);
    int x = m;
    int y = m - scroll_y_;

    // Label columns are as wide as the longest label in that column, so nothing is clipped at any DPI.
    auto widest = [&](std::initializer_list<HWND> labels) {
        int w = 0;
        for (HWND l : labels) w = std::max(w, TextWidth(l));
        return std::min(w + Px(14), col_w / 2);
    };
    const int left_labels = widest({l_theme_, l_close_, l_opacity_, l_corner_, l_monitor_, l_fullscreen_, l_retention_, l_trace_});
    const int right_labels = widest({l_depth_, l_files_, l_width_, l_color_, l_duration_});
    int label_w = left_labels;

    auto head = [&](HWND h) { y += Px(8); MoveWindow(h, x, y, col_w, row, TRUE); y += row; };
    auto hint = [&](HWND h) { const int hh = WrappedHeight(h, col_w); MoveWindow(h, x, y, col_w, hh, TRUE); y += hh + gap; };
    auto wide = [&](HWND h) { MoveWindow(h, x, y, col_w, row, TRUE); y += step; };
    // Combo boxes are given their drop-down height; every other control keeps the row height.
    auto pair = [&](HWND l, HWND c, int cw, bool combo) {
        MoveWindow(l, x, y + Px(4), label_w, row - Px(4), TRUE);
        MoveWindow(c, x + label_w, y, cw ? cw : col_w - label_w, combo ? Px(240) : row, TRUE);
        y += step;
    };
    auto button_width = [&](HWND b) { return TextWidth(b) + Px(28); };

    // Left column.
    head(h_general_); hint(hint_general_);
    pair(l_theme_, theme_, 0, true); pair(l_close_, close_, 0, true); wide(start_window_);
    head(h_hud_); hint(hint_hud_);
    wide(hud_visible_); wide(hud_expanded_); wide(click_through_);
    MoveWindow(l_opacity_, x, y + Px(4), label_w, row - Px(4), TRUE);
    MoveWindow(opacity_, x + label_w, y, col_w - label_w - Px(48), row, TRUE);
    MoveWindow(opacity_value_, x + col_w - Px(44), y + Px(4), Px(44), row - Px(4), TRUE);
    y += step;
    pair(l_corner_, corner_, 0, true); pair(l_monitor_, monitor_, 0, true); pair(l_fullscreen_, fullscreen_, 0, true);
    head(h_ghost_); hint(hint_ghost_);
    wide(ghost_enabled_); pair(l_retention_, retention_, 0, true); pair(l_trace_, trace_, 0, true); wide(noise_);
    head(h_about_);
    {
        const int hh = WrappedHeight(about_text_, col_w);
        MoveWindow(about_text_, x, y, col_w, hh, TRUE);
        y += hh + gap;
        MoveWindow(help_, x, y, button_width(help_), row, TRUE);
        y += step;
    }
    const int left_end = y;

    // Right column.
    x = 2 * m + col_w;
    y = m - scroll_y_;
    label_w = right_labels;
    head(h_unlock_); hint(hint_unlock_);
    {
        const int w1 = button_width(reg_), w2 = button_width(unreg_);
        MoveWindow(reg_, x, y, w1, row, TRUE);
        MoveWindow(unreg_, x + w1 + gap, y, w2, row, TRUE);
        y += step;
        const int hh = std::max(row, WrappedHeight(shell_status_, col_w));
        MoveWindow(shell_status_, x, y, col_w, hh, TRUE);
        y += hh + gap;
    }
    pair(l_depth_, depth_, Px(70), false); pair(l_files_, files_, Px(90), false);
    head(h_focus_); hint(hint_focus_);
    wide(focus_enabled_); pair(l_width_, width_, Px(70), false);
    {
        MoveWindow(l_color_, x, y + Px(4), label_w, row - Px(4), TRUE);
        const int sw = Px(28), bw = button_width(color_);
        MoveWindow(color_swatch_, x + label_w, y + Px(2), sw, row - Px(4), TRUE);
        MoveWindow(color_, x + label_w + sw + gap, y, bw, row, TRUE);
        MoveWindow(color_text_, x + label_w + sw + bw + 2 * gap, y + Px(4), Px(80), row - Px(4), TRUE);
        y += step;
    }
    pair(l_duration_, duration_, Px(90), false);
    head(h_shortcuts_); hint(hint_shortcuts_);
    {
        int check_w = 0;
        for (HWND c : sc_check_) check_w = std::max(check_w, TextWidth(c) + Px(28));
        const int key_w = Px(130);
        for (int i = 0; i < 3; ++i) {
            MoveWindow(sc_check_[i], x, y, check_w, row, TRUE);
            MoveWindow(sc_key_[i], x + check_w + gap, y, key_w, row, TRUE);
            const int status_x = check_w + key_w + 2 * gap + gap;
            MoveWindow(sc_status_[i], x + status_x, y + Px(4), std::max(Px(40), col_w - status_x), row - Px(4), TRUE);
            y += step;
        }
        MoveWindow(apply_sc_, x, y, button_width(apply_sc_), row, TRUE);
        y += step + gap;
    }
    head(h_file_); hint(hint_file_);
    MoveWindow(file_path_, x, y, col_w, row, TRUE);
    y += step;
    {
        const int w1 = button_width(open_folder_), w2 = button_width(reset_);
        MoveWindow(open_folder_, x, y, w1, row, TRUE);
        MoveWindow(reset_, x + w1 + gap, y, w2, row, TRUE);
        y += step;
    }
    content_height = std::max(left_end, y) + m + scroll_y_;
}

void SettingsPage::ScrollTo(int position) {
    if (position == scroll_y_) return;
    RECT client{};
    GetClientRect(hwnd_, &client);
    scroll_y_ = std::max(0, position);   // OnLayout clamps to the content height
    OnLayout(client.right, client.bottom);
}

HBRUSH SettingsPage::OnControlColor(HWND control) { return control == color_swatch_ ? swatch_brush_ : nullptr; }

void SettingsPage::UpdateSwatch() {
    std::uint32_t rgb = 0;
    const std::string& text = ctx_.settings.Get().focus.highlight_color;
    (void)ParseColor(text, rgb);
    if (swatch_brush_) DeleteObject(swatch_brush_);
    swatch_brush_ = CreateSolidBrush(RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF));
    SetText(color_text_, Utf8ToWide(text));
    InvalidateRect(color_swatch_, nullptr, TRUE);
}

void SettingsPage::Accept() {
    const HWND focused = GetFocus();
    if (focused == depth_ || focused == files_ || focused == width_ || focused == duration_) ReadNumbers();
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
    UpdateSwatch();
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

void SettingsPage::OnMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_VSCROLL && lparam == 0) {
        SCROLLINFO info{};
        info.cbSize = sizeof(info);
        info.fMask = SIF_ALL;
        GetScrollInfo(hwnd_, SB_VERT, &info);
        int position = info.nPos;
        switch (LOWORD(wparam)) {
            case SB_LINEUP: position -= Px(24); break;
            case SB_LINEDOWN: position += Px(24); break;
            case SB_PAGEUP: position -= static_cast<int>(info.nPage); break;
            case SB_PAGEDOWN: position += static_cast<int>(info.nPage); break;
            case SB_THUMBTRACK: case SB_THUMBPOSITION: position = info.nTrackPos; break;
            case SB_TOP: position = 0; break;
            case SB_BOTTOM: position = info.nMax; break;
        }
        ScrollTo(position);
        return;
    }
    if (message == WM_MOUSEWHEEL) {
        ScrollTo(scroll_y_ - GET_WHEEL_DELTA_WPARAM(wparam) * Px(48) / WHEEL_DELTA);
        return;
    }
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
        case kReset:
            if (clicked && MessageBoxW(hwnd_, str::kSetResetConfirm, str::kAppName, MB_YESNO | MB_ICONQUESTION) == IDYES) {
                ctx_.settings.Reset();
                ctx_.host.Execute(Command::SettingsChanged);
            }
            break;
        case kHelp:
            if (clicked) ctx_.navigator.ShowHelp();
            break;
        case kOpenFolder:
            if (clicked) RevealInExplorer(hwnd_, ctx_.settings.Path());
            break;
    }
}

} // namespace peek
