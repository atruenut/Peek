#include "ui/unlock_page.h"

#include "core/format.h"
#include "core/privilege_helper.h"
#include "core/strings.h"
#include "core/window_util.h"

#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <set>

namespace peek {
namespace {

std::wstring SizeText(ULONGLONG bytes) {
    wchar_t text[64]{};
    if (bytes < 1024) swprintf(text, std::size(text), L"%llu bytes", bytes);
    else if (bytes < 1024ull * 1024) swprintf(text, std::size(text), L"%.1f KB", static_cast<double>(bytes) / 1024.0);
    else if (bytes < 1024ull * 1024 * 1024) swprintf(text, std::size(text), L"%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else swprintf(text, std::size(text), L"%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    return text;
}

std::wstring FileTimeText(const FILETIME& ft) {
    FILETIME local{};
    SYSTEMTIME st{};
    if (!FileTimeToLocalFileTime(&ft, &local) || !FileTimeToSystemTime(&local, &st)) return L"unknown";
    wchar_t text[48]{};
    swprintf(text, std::size(text), L"%04u-%02u-%02u %02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return text;
}

const wchar_t* AppTypeText(DWORD type) {
    switch (type) {
        case 1: return L"Application (main window)";
        case 2: return L"Application (other window)";
        case 3: return L"Windows service";
        case 4: return L"Windows Explorer";
        case 5: return L"Console application";
        case 1000: return L"Critical system process";
        default: return L"Application";
    }
}

} // namespace

UnlockPage::~UnlockPage() {
    if (cancel_) cancel_->store(true);
    if (worker_.joinable()) worker_.join();
}

void UnlockPage::OnCreate() {
    intro_ = MakeLabel(str::kUnlockIntro);
    add_files_ = MakeButton(str::kBtnAddFiles, kAddFiles);
    add_folder_ = MakeButton(str::kBtnAddFolder, kAddFolder);
    clear_ = MakeButton(str::kBtnClear, kClearTargets);
    check_ = MakeButton(str::kBtnInvestigate, kCheck);
    status_ = MakeLabel(L"", SS_RIGHT);
    targets_ = MakeText(kTargets);
    list_ = MakeList(kList);
    AddColumn(list_, 0, str::kColProcess, 190);
    AddColumn(list_, 1, str::kColPid, 70, LVCFMT_RIGHT);
    AddColumn(list_, 2, str::kColFiles, 60, LVCFMT_RIGHT);
    AddColumn(list_, 3, str::kColType, 180);
    AddColumn(list_, 4, str::kColPath, 400);
    detail_ = MakeText(kDetail);
    process_ = MakeButton(str::kBtnProcessDetails, kProcess);
    show_window_ = MakeButton(str::kBtnShowWindow, kShowWindow);
    location_ = MakeButton(str::kBtnOpenLocation, kLocation);
    close_ = MakeButton(str::kBtnAskClose, kClose);
    end_ = MakeButton(str::kBtnEndProcess, kEnd);
    Tip(add_files_, str::kTipAddFiles); Tip(add_folder_, str::kTipAddFolder); Tip(clear_, str::kTipClearTargets);
    Tip(check_, str::kTipCheckAgain); Tip(process_, str::kTipProcessDetails); Tip(show_window_, str::kTipShowOwner);
    Tip(location_, str::kTipOpenLocation); Tip(close_, str::kTipAskClose); Tip(end_, str::kTipEndProcess);
    DragAcceptFiles(hwnd_, TRUE);
    SetText(detail_, str::kUnlockEmpty);
    UpdateButtons();
}

void UnlockPage::OnLayout(int width, int height) {
    const int m = Px(8), row = Px(24), gap = Px(6);
    MoveWindow(intro_, m, m + Px(3), width - 2 * m, row, TRUE);
    int x = m;
    const int y = m + row + gap;
    for (auto [button, w] : {std::pair{add_files_, 110}, {add_folder_, 110}, {clear_, 70}, {check_, 130}}) {
        MoveWindow(button, x, y, Px(w), row, TRUE);
        x += Px(w) + gap;
    }
    MoveWindow(status_, x, y + Px(4), std::max(Px(40), width - m - x), row - Px(4), TRUE);
    const int targets_top = y + row + gap;
    const int targets_h = Px(56);
    MoveWindow(targets_, m, targets_top, width - 2 * m, targets_h, TRUE);
    const int list_top = targets_top + targets_h + gap;
    const int list_h = std::max(Px(90), (height - list_top) * 36 / 100);
    MoveWindow(list_, m, list_top, width - 2 * m, list_h, TRUE);
    const int action_y = list_top + list_h + gap;
    x = m;
    for (auto [button, w] : {std::pair{process_, 130}, {show_window_, 120}, {location_, 120}, {close_, 150}, {end_, 130}}) {
        MoveWindow(button, x, action_y, Px(w), row, TRUE);
        x += Px(w) + gap;
    }
    const int detail_top = action_y + row + gap;
    MoveWindow(detail_, m, detail_top, width - 2 * m, std::max(Px(40), height - m - detail_top), TRUE);
}

void UnlockPage::AddTargets(const std::vector<std::wstring>& paths) {
    for (const std::wstring& path : paths) {
        if (path.empty()) continue;
        if (std::find_if(targets_list_.begin(), targets_list_.end(),
                         [&](const std::wstring& existing) { return _wcsicmp(existing.c_str(), path.c_str()) == 0; }) == targets_list_.end()) {
            targets_list_.push_back(path);
        }
    }
    std::wstring text;
    for (const std::wstring& path : targets_list_) text += path + L"\r\n";
    SetText(targets_, text);
}

void UnlockPage::ShowTargets(const std::vector<std::wstring>& paths, bool inspect) {
    targets_list_.clear();
    inspect_mode_ = inspect;
    AddTargets(paths);
    action_note_.clear();
    Start();
}

void UnlockPage::Cancel() {
    if (cancel_) cancel_->store(true);
}

void UnlockPage::Start() {
    if (targets_list_.empty()) { SetStatus(str::kChooseFileFirst); return; }
    Cancel();
    if (worker_.joinable()) worker_.join();   // the cancelled run finishes quickly; its result is discarded
    const Settings settings = ctx_.settings.Get();
    const UnlockLimits limits{settings.unlock.max_depth, settings.unlock.max_files};
    auto cancel = std::make_shared<std::atomic_bool>(false);
    cancel_ = cancel;
    const std::uint64_t run = ++run_;
    busy_ = true;
    SetStatus(str::kChecking);
    UpdateButtons();
    const std::vector<std::wstring> paths = targets_list_;
    HWND page = hwnd_;
    worker_ = std::thread([this, paths, limits, cancel, run, page] {
        auto report = std::make_shared<UnlockReport>(ctx_.unlock.Investigate(paths, limits, cancel.get()));
        {
            std::lock_guard lock(mutex_);
            if (run == run_) pending_ = std::move(report);   // a newer run supersedes this one
        }
        PostMessageW(page, kDone, 0, 0);
    });
}

void UnlockPage::OnMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == kDone) {
        std::shared_ptr<UnlockReport> report;
        {
            std::lock_guard lock(mutex_);
            report = std::move(pending_);
        }
        if (!report) return;
        report_ = std::move(*report);
        busy_ = false;
        ShowReport();
    } else if (message == WM_DROPFILES) {
        HDROP drop = reinterpret_cast<HDROP>(wparam);
        const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::wstring> paths;
        for (UINT i = 0; i < count; ++i) {
            const UINT length = DragQueryFileW(drop, i, nullptr, 0);
            std::wstring path(length + 1, L'\0');
            DragQueryFileW(drop, i, path.data(), length + 1);
            path.resize(length);
            paths.push_back(path);
        }
        DragFinish(drop);
        AddTargets(paths);
        Start();
    }
    (void)lparam;
}

void UnlockPage::ShowReport() {
    ListView_DeleteAllItems(list_);
    for (const LockGroup& group : report_.groups) {
        const ProcessInfo& p = group.owner.process;
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = ListView_GetItemCount(list_);
        const std::wstring name = p.name.empty() ? group.owner.application_name : p.name;
        item.pszText = const_cast<wchar_t*>(name.c_str());
        const int index = ListView_InsertItem(list_, &item);
        const std::wstring pid = std::to_wstring(p.pid);
        const std::wstring files = std::to_wstring(group.files.size());
        ListView_SetItemText(list_, index, 1, const_cast<wchar_t*>(pid.c_str()));
        ListView_SetItemText(list_, index, 2, const_cast<wchar_t*>(files.c_str()));
        ListView_SetItemText(list_, index, 3, const_cast<wchar_t*>(AppTypeText(group.owner.app_type)));
        ListView_SetItemText(list_, index, 4, const_cast<wchar_t*>(p.exe_path.c_str()));
    }
    if (report_.cancelled) SetStatus(L"Cancelled.");
    else if (report_.groups.empty()) SetStatus(std::wstring(str::kNoLockOwner) + L"  (" + std::to_wstring(report_.files_checked) + L" file(s) checked)");
    else SetStatus(std::to_wstring(report_.groups.size()) + L" " + str::kOwnersFound + L"  (" + std::to_wstring(report_.files_checked) + L" file(s) checked)");
    if (!report_.groups.empty()) ListView_SetItemState(list_, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    UpdateDetail();
    UpdateButtons();
}

const LockGroup* UnlockPage::SelectedGroup() const {
    const int row = ListView_GetNextItem(list_, -1, LVNI_SELECTED);
    if (row < 0 || static_cast<std::size_t>(row) >= report_.groups.size()) return nullptr;
    return &report_.groups[static_cast<std::size_t>(row)];
}

void UnlockPage::UpdateDetail() {
    std::wstring text;
    if (inspect_mode_ || SelectedGroup() == nullptr) {
        for (const FileFacts& facts : report_.facts) {
            text += facts.path + L"\r\n";
            if (!facts.exists) { text += L"  does not exist\r\n"; continue; }
            text += facts.is_directory ? L"  Folder" : L"  File, " + SizeText(facts.size);
            text += L"; modified " + FileTimeText(facts.modified) + L"\r\n";
        }
        if (!text.empty()) text += L"\r\n";
    }
    if (const LockGroup* group = SelectedGroup()) {
        const ProcessInfo& p = group->owner.process;
        text += std::wstring(str::kPropProcess) + L": " + (p.name.empty() ? group->owner.application_name : p.name) + L"  (PID " + std::to_wstring(p.pid) + L")\r\n";
        text += std::wstring(str::kPropExe) + L": " + (p.exe_path.empty() ? std::wstring(str::kUnavailable) : p.exe_path) + L"\r\n";
        text += std::wstring(L"Type: ") + AppTypeText(group->owner.app_type) + L"\r\n";
        if (!group->owner.service_name.empty()) text += L"Service: " + group->owner.service_name + L"\r\n";
        text += L"Restart Manager can restart it: " + std::wstring(group->owner.restartable ? L"yes" : L"no") + L"\r\n";
        if (!p.accessible) text += L"Some details are unavailable: Windows denied access to this process. Run PeekII as administrator to see more.\r\n";
        if (UnlockService::IsProtected(group->owner)) text += std::wstring(L"\r\n") + str::kProtectedNote + L"\r\n";
        text += L"\r\nFiles held (" + std::to_wstring(group->files.size()) + L"):\r\n";
        constexpr std::size_t kShown = 40;
        for (std::size_t i = 0; i < group->files.size() && i < kShown; ++i) text += L"  " + group->files[i] + L"\r\n";
        if (group->files.size() > kShown) text += L"  … and " + std::to_wstring(group->files.size() - kShown) + L" more\r\n";
    }
    if (!action_note_.empty()) text += L"\r\n" + action_note_ + L"\r\n";
    if (!report_.messages.empty()) {
        text += L"\r\nNotes:\r\n";
        for (const std::wstring& message : report_.messages) text += L"  " + message + L"\r\n";
    }
    if (text.empty()) text = str::kUnlockEmpty;
    SetText(detail_, text);
}

void UnlockPage::UpdateButtons() {
    const LockGroup* group = SelectedGroup();
    const bool any = group != nullptr;
    EnableWindow(check_, !targets_list_.empty() && !busy_);
    EnableWindow(process_, any);
    EnableWindow(show_window_, any && true);
    EnableWindow(location_, any && !group->owner.process.exe_path.empty());
    const bool allowed = any && !UnlockService::IsProtected(group->owner);
    EnableWindow(close_, allowed);
    EnableWindow(end_, allowed);
}

void UnlockPage::SetStatus(const std::wstring& text) { SetText(status_, text); }

LRESULT UnlockPage::OnNotify(NMHDR* header) {
    if (header->hwndFrom != list_) return 0;
    if (header->code == LVN_ITEMCHANGED) {
        action_note_.clear();
        UpdateDetail();
        UpdateButtons();
    } else if (header->code == NM_DBLCLK) {
        if (const LockGroup* group = SelectedGroup()) ctx_.navigator.OpenProcess(group->owner.process.Identity());
    }
    return 0;
}

void UnlockPage::OnCommand(int id, int code, HWND) {
    if (code != BN_CLICKED) return;
    const LockGroup* group = SelectedGroup();
    switch (id) {
        case kAddFiles: PickFiles(); break;
        case kAddFolder: PickFolders(); break;
        case kClearTargets:
            Cancel();
            targets_list_.clear();
            report_ = {};
            action_note_.clear();
            SetText(targets_, L"");
            ListView_DeleteAllItems(list_);
            SetText(detail_, str::kUnlockEmpty);
            SetStatus(L"");
            busy_ = false;
            UpdateButtons();
            break;
        case kCheck: action_note_.clear(); Start(); break;
        case kProcess:
            if (group) ctx_.navigator.OpenProcess(group->owner.process.Identity());
            break;
        case kShowWindow:
            if (group) {
                const auto windows = TopLevelWindows(group->owner.process.pid, true);
                if (windows.empty()) action_note_ = L"This process has no visible window to show.";
                else { BringWindowToFront(windows.front().hwnd); action_note_ = L"Showing: " + windows.front().title; }
                UpdateDetail();
            }
            break;
        case kLocation:
            if (group) RevealInExplorer(hwnd_, group->owner.process.exe_path);
            break;
        case kClose:
            if (group) {
                int notified = 0;
                switch (UnlockService::RequestClose(group->owner, &notified)) {
                    case CloseResult::Requested:
                        action_note_ = L"Asked " + std::to_wstring(notified) + L" window(s) to close. The application may ask you to save first. "
                                       L"Choose \"Check again\" when it has closed.";
                        break;
                    case CloseResult::NoWindows:
                        action_note_ = L"This process has no visible window to close. Use \"Show window\" or end the process as a last resort.";
                        break;
                    case CloseResult::NotAllowed: action_note_ = str::kProtectedNote; break;
                    case CloseResult::ProcessGone: action_note_ = L"The process has already exited. Check again."; break;
                }
                UpdateDetail();
            }
            break;
        case kEnd:
            if (group) {
                const ProcessInfo& p = group->owner.process;
                const std::wstring question = std::wstring(str::kTerminateQuestion) + L"\n\n" +
                    (p.name.empty() ? group->owner.application_name : p.name) + L"  (PID " + std::to_wstring(p.pid) + L")\n\n" + str::kUnsavedWarning;
                if (MessageBoxW(hwnd_, question.c_str(), str::kTerminateTitle, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {
                    std::wstring error;
                    if (PrivilegeHelper::Terminate(p.pid, &p.start_time, error)) {
                        action_note_ = L"The process was ended. Choose \"Check again\", then retry your file operation.";
                    } else {
                        action_note_ = L"The process could not be ended: " + error;
                    }
                    UpdateDetail();
                }
            }
            break;
    }
}

void UnlockPage::PickFiles() {
    std::vector<wchar_t> buffer(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrFilter = L"All files\0*.*\0";
    dialog.lpstrTitle = str::kPickerTitle;
    dialog.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&dialog)) return;
    // Multi-select result: directory, then file names, each NUL-terminated; a single file is a full path.
    std::vector<std::wstring> parts;
    for (const wchar_t* p = buffer.data(); *p; p += wcslen(p) + 1) parts.emplace_back(p);
    std::vector<std::wstring> paths;
    if (parts.size() == 1) paths = parts;
    else for (std::size_t i = 1; i < parts.size(); ++i) paths.push_back(parts[0] + L"\\" + parts[i]);
    AddTargets(paths);
    Start();
}

void UnlockPage::PickFolders() {
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_ALLOWMULTISELECT);
    if (FAILED(dialog->Show(hwnd_))) return;
    Microsoft::WRL::ComPtr<IShellItemArray> results;
    if (FAILED(dialog->GetResults(&results))) return;
    DWORD count = 0;
    results->GetCount(&count);
    std::vector<std::wstring> paths;
    for (DWORD i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<IShellItem> item;
        if (FAILED(results->GetItemAt(i, &item))) continue;
        PWSTR name = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name)) && name) {
            paths.emplace_back(name);
            CoTaskMemFree(name);
        }
    }
    AddTargets(paths);
    Start();
}

} // namespace peek
