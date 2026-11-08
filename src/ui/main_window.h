#pragma once

#include "ui/focus_page.h"
#include "ui/ghost_page.h"
#include "ui/page.h"
#include "ui/process_page.h"
#include "ui/settings_page.h"
#include "ui/unlock_page.h"

#include <memory>

namespace peek {

// The investigation window: a resizable frame with a custom-drawn tab strip hosting the pages.
// It is secondary to the HUD; closing it hides it (or exits, per the Close button setting).
class MainWindow : public Navigator {
public:
    enum class Tab { Ghost = 0, Process, Unlock, Focus, Settings };

    MainWindow(EventStore& events, SettingsManager& settings, ProcessResolver& resolver, IconCache& icons,
               UnlockService& unlock, FocusInspector& focus, CommandHost& host);
    ~MainWindow();

    bool Create(HINSTANCE instance);
    void Destroy();
    [[nodiscard]] HWND Handle() const { return hwnd_; }
    void Show(int tab = -1);               // -1 keeps the current tab
    void ApplySettings();
    void EventsChanged();
    void DeliverFocus(const FocusSnapshot& snapshot) { focus_page_->SetSnapshot(snapshot); }
    void DeliverTree(const FocusTreeUpdate& update) { focus_page_->ApplyTree(update); }
    void FocusModeChanged(InspectMode mode) { focus_page_->ModeChanged(mode); }
    void ShowUnlock(const std::vector<std::wstring>& paths, bool inspect) { OpenUnlock(paths, inspect); }
    void ShowEvent(std::uint64_t sequence) { OpenGhostEvent(sequence); }

    // Navigator
    void OpenProcess(const ProcessIdentity& identity) override;
    void OpenGhostEvent(std::uint64_t sequence) override;
    void OpenFocusForWindow(HWND hwnd) override;
    void OpenUnlock(const std::vector<std::wstring>& paths, bool inspect) override;
    void ShowHelp() override;
    // F1 from anywhere inside the window.
    [[nodiscard]] bool Owns(HWND hwnd) const { return hwnd_ && (hwnd == hwnd_ || IsChild(hwnd_, hwnd)); }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void Select(Tab tab);
    void LayoutPages();
    void UpdateAppearance();
    void Paint(HDC dc);
    [[nodiscard]] int TabHeight() const;
    [[nodiscard]] RECT TabRect(int index) const;
    [[nodiscard]] RECT HelpRect() const;
    [[nodiscard]] Page* PageFor(Tab tab) const;

    CommandHost& host_;
    PageContext context_;
    HWND hwnd_{};
    HINSTANCE instance_{};
    UINT dpi_{96};
    Theme theme_;
    HFONT font_{};
    HFONT font_bold_{};
    Tab tab_{Tab::Ghost};
    int hover_tab_{-1};
    std::unique_ptr<GhostPage> ghost_page_;
    std::unique_ptr<ProcessPage> process_page_;
    std::unique_ptr<UnlockPage> unlock_page_;
    std::unique_ptr<FocusPage> focus_page_;
    std::unique_ptr<SettingsPage> settings_page_;
};

} // namespace peek
