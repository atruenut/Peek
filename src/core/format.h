#pragma once

#include "core/types.h"

#include <chrono>
#include <string>
#include <string_view>

namespace peek {

[[nodiscard]] std::wstring Utf8ToWide(std::string_view text);
[[nodiscard]] std::string WideToUtf8(std::wstring_view text);

[[nodiscard]] std::wstring FormatTime(const std::chrono::system_clock::time_point& time);
[[nodiscard]] std::wstring FormatDuration(std::chrono::milliseconds value);
[[nodiscard]] std::wstring Elide(const std::wstring& text, std::size_t max_chars);
[[nodiscard]] std::wstring HexHandle(HWND hwnd);
[[nodiscard]] std::wstring RectText(const RECT& rect);
[[nodiscard]] std::wstring Win32ErrorText(DWORD code);
[[nodiscard]] std::wstring ToLower(std::wstring text);
[[nodiscard]] bool ContainsNoCase(const std::wstring& haystack, const std::wstring& needle_lower);

[[nodiscard]] const wchar_t* KindLabel(GhostKind kind);
[[nodiscard]] const wchar_t* CategoryLabel(EventCategory category);
[[nodiscard]] const wchar_t* ShortCategoryLabel(EventCategory category);

// Name shown for an event: process name, falling back to the window title or PID.
[[nodiscard]] std::wstring EventProcessName(const GhostEvent& event);
// One line: "14:02:11  Process started  powershell.exe (PID 4120)".
[[nodiscard]] std::wstring FormatEventSummary(const GhostEvent& event);
// Multi-line plain-text description suitable for the clipboard.
[[nodiscard]] std::wstring FormatEventDetail(const GhostEvent& event, int running /* -1 unknown, 0 no, 1 yes */);

bool CopyTextToClipboard(HWND owner, const std::wstring& text);
void RevealInExplorer(HWND owner, const std::wstring& path);

} // namespace peek
