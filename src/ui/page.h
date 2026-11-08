#pragma once

#include "core/event_store.h"
#include "core/icon_cache.h"
#include "core/process_resolver.h"
#include "core/settings_manager.h"
#include "focus/focus_inspector.h"
#include "ui/commands.h"
#include "ui/navigator.h"
#include "ui/theme.h"
#include "unlock/unlock_service.h"

#include <windows.h>
#include <commctrl.h>

#include <string>

namespace peek {

// Everything a page may talk to. All references outlive the pages.
struct PageContext {
    EventStore& events;
    SettingsManager& settings;
    ProcessResolver& resolver;
    IconCache& icons;
    UnlockService& unlock;
    FocusInspector& focus;
    CommandHost& host;
    Navigator& navigator;
};

// A page is a plain child window that hosts native controls. Subclasses create their controls in OnCreate
// and position them in OnLayout. Theme colors and the font are applied to every child automatically.
// All methods run on the UI thread.
class Page {
public:
    explicit Page(PageContext& context) : ctx_(context) {}
    virtual ~Page() = default;
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;

    bool Create(HWND parent, HINSTANCE instance);
    [[nodiscard]] HWND Handle() const { return hwnd_; }
    void Place(const RECT& area, bool visible);
    void SetAppearance(const Theme& theme, UINT dpi, HFONT font);

    // Called when the page becomes visible and whenever shared data changed while it is visible.
    virtual void Refresh() {}
    // Called when the settings changed somewhere else.
    virtual void SettingsChanged() {}
    // Application-defined messages routed by MainWindow (WM_APP range).
    virtual bool HandleAppMessage(UINT, WPARAM, LPARAM) { return false; }

protected:
    virtual void OnCreate() = 0;
    virtual void OnLayout(int width, int height) = 0;
    virtual void OnCommand(int id, int code, HWND control) { (void)id; (void)code; (void)control; }
    virtual LRESULT OnNotify(NMHDR* header) { (void)header; return 0; }
    virtual void OnMessage(UINT, WPARAM, LPARAM) {}
    virtual void OnAppearance() {}

    [[nodiscard]] int Px(int dips) const { return MulDiv(dips, static_cast<int>(dpi_), 96); }

    // Control factories. Each control gets the page font and theme.
    HWND MakeLabel(const wchar_t* text, DWORD extra_style = 0);
    HWND MakeButton(const wchar_t* text, int id, DWORD extra_style = 0);
    HWND MakeCheck(const wchar_t* text, int id);
    HWND MakeEdit(int id, DWORD extra_style = 0);
    HWND MakeCombo(int id);
    HWND MakeList(int id, DWORD extra_style = 0);
    HWND MakeTree(int id);
    HWND MakeText(int id);   // read-only multi-line text with a vertical scroll bar

    void AddColumn(HWND list, int index, const wchar_t* title, int width_dips, int format = LVCFMT_LEFT);
    void FillCombo(HWND combo, std::initializer_list<const wchar_t*> items, int selected);
    void SetText(HWND control, const std::wstring& text);
    [[nodiscard]] std::wstring GetText(HWND control) const;
    void StyleControl(HWND control, const wchar_t* kind);

    PageContext& ctx_;
    HWND hwnd_{};
    HINSTANCE instance_{};
    UINT dpi_{96};
    Theme theme_;
    HFONT font_{};
    HBRUSH background_brush_{};
    HBRUSH surface_brush_{};
    bool loading_{false};   // set while controls are being filled so change handlers do not save

private:
    static LRESULT CALLBACK PageProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT Handle(UINT message, WPARAM wparam, LPARAM lparam);
    void RestyleChildren();
    HWND Track(HWND control);

    std::vector<std::pair<HWND, const wchar_t*>> styled_;
};

} // namespace peek
