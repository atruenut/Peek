#include "core/format.h"

#include "core/strings.h"

#include <shellapi.h>

#include <ctime>
#include <sstream>

namespace peek {

std::wstring Utf8ToWide(std::string_view text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

std::string WideToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring FormatTime(const std::chrono::system_clock::time_point& time) {
    const std::time_t raw = std::chrono::system_clock::to_time_t(time);
    std::tm local{};
    localtime_s(&local, &raw);
    wchar_t text[16]{};
    wcsftime(text, std::size(text), L"%H:%M:%S", &local);
    return text;
}

std::wstring FormatDuration(std::chrono::milliseconds value) {
    const auto ms = value.count();
    if (ms < 1000) return std::to_wstring(ms) + L" ms";
    if (ms < 60000) {
        wchar_t text[32]{};
        swprintf(text, std::size(text), L"%.1f s", static_cast<double>(ms) / 1000.0);
        return text;
    }
    return std::to_wstring(ms / 60000) + L" min " + std::to_wstring((ms / 1000) % 60) + L" s";
}

std::wstring Elide(const std::wstring& text, std::size_t max_chars) {
    if (text.size() <= max_chars) return text;
    if (max_chars < 2) return text.substr(0, max_chars);
    return text.substr(0, max_chars - 1) + L"…";
}

std::wstring HexHandle(HWND hwnd) {
    std::wstringstream stream;
    stream << L"0x" << std::uppercase << std::hex << reinterpret_cast<std::uintptr_t>(hwnd);
    return stream.str();
}

std::wstring RectText(const RECT& rect) {
    return std::to_wstring(rect.left) + L", " + std::to_wstring(rect.top) + L"  ·  " +
           std::to_wstring(rect.right - rect.left) + L" × " + std::to_wstring(rect.bottom - rect.top);
}

std::wstring Win32ErrorText(DWORD code) {
    wchar_t* text = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                        nullptr, code, 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    std::wstring result = (length && text) ? std::wstring(text, length) : L"Error " + std::to_wstring(code);
    if (text) LocalFree(text);
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' || result.back() == L' ')) result.pop_back();
    return result;
}

std::wstring ToLower(std::wstring text) {
    if (!text.empty()) CharLowerBuffW(text.data(), static_cast<DWORD>(text.size()));
    return text;
}

bool ContainsNoCase(const std::wstring& haystack, const std::wstring& needle_lower) {
    if (needle_lower.empty()) return true;
    return ToLower(haystack).find(needle_lower) != std::wstring::npos;
}

const wchar_t* KindLabel(GhostKind kind) {
    switch (kind) {
        case GhostKind::ProcessStarted: return str::kProcessStarted;
        case GhostKind::ProcessExited: return str::kProcessTerminated;
        case GhostKind::WindowCreated: return str::kWindowCreated;
        case GhostKind::WindowShown: return str::kWindowShown;
        case GhostKind::WindowHidden: return str::kWindowHidden;
        case GhostKind::WindowDestroyed: return str::kWindowDestroyed;
    }
    return L"";
}

const wchar_t* CategoryLabel(EventCategory category) {
    switch (category) {
        case EventCategory::ProcessStarted: return str::kProcessStarted;
        case EventCategory::WindowAppeared: return str::kWindowAppeared;
        case EventCategory::WindowDisappeared: return str::kWindowDisappeared;
        case EventCategory::ProcessTerminated: return str::kProcessTerminated;
    }
    return L"";
}

const wchar_t* ShortCategoryLabel(EventCategory category) {
    switch (category) {
        case EventCategory::ProcessStarted: return str::kShortStarted;
        case EventCategory::WindowAppeared: return str::kShortAppeared;
        case EventCategory::WindowDisappeared: return str::kShortDisappeared;
        case EventCategory::ProcessTerminated: return str::kShortTerminated;
    }
    return L"";
}

std::wstring EventProcessName(const GhostEvent& event) {
    if (!event.process.name.empty()) return event.process.name;
    if (!event.window.title.empty()) return event.window.title;
    if (event.process.pid) return std::wstring(str::kUnknownProcess) + L" (PID " + std::to_wstring(event.process.pid) + L")";
    return str::kUnknownProcess;
}

std::wstring FormatEventSummary(const GhostEvent& event) {
    std::wstring text = FormatTime(event.timestamp) + L"  " + KindLabel(event.kind) + L"  " + EventProcessName(event) +
                        L" (PID " + std::to_wstring(event.process.pid) + L")";
    if (!event.window.title.empty()) text += L"  —  " + event.window.title;
    return text;
}

std::wstring FormatEventDetail(const GhostEvent& event, int running) {
    std::wstring text;
    auto line = [&](const wchar_t* label, const std::wstring& value) {
        if (value.empty()) return;
        text += std::wstring(label) + L": " + value + L"\r\n";
    };
    line(L"Time", FormatTime(event.timestamp));
    line(L"Event", KindLabel(event.kind));
    line(L"Process", event.process.name);
    line(L"PID", event.process.pid ? std::to_wstring(event.process.pid) : std::wstring{});
    if (running >= 0) line(L"State", running ? str::kRunning : str::kEnded);
    line(L"Executable", event.process.exe_path);
    line(L"Command line", event.process.command_line);
    if (event.process.parent_pid) {
        line(L"Parent", (event.process.parent_name.empty() ? std::wstring{} : event.process.parent_name + L" ") +
                        L"(PID " + std::to_wstring(event.process.parent_pid) + L")");
    }
    line(L"Parent path", event.process.parent_path);
    if (event.window.hwnd) line(L"HWND", HexHandle(event.window.hwnd));
    line(L"Window class", event.window.class_name);
    line(L"Window title", event.window.title);
    if (event.has_lifetime) line(L"Lifetime", FormatDuration(event.lifetime));
    return text;
}

bool CopyTextToClipboard(HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    bool ok = false;
    if (memory) {
        if (void* target = GlobalLock(memory)) {
            memcpy(target, text.c_str(), bytes);
            GlobalUnlock(memory);
            ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
        }
        if (!ok) GlobalFree(memory);   // ownership passes to the clipboard only on success
    }
    CloseClipboard();
    return ok;
}

void RevealInExplorer(HWND owner, const std::wstring& path) {
    if (path.empty()) return;
    const std::wstring arguments = L"/select,\"" + path + L"\"";
    ShellExecuteW(owner, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
}

} // namespace peek
