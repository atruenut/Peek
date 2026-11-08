#include "focus/focus_format.h"

#include "core/format.h"
#include "core/strings.h"

#include <cwchar>
#include <sstream>

namespace peek {
namespace {

std::wstring OrUnavailable(const std::wstring& value) { return value.empty() ? std::wstring(str::kUnavailable) : value; }

std::wstring JsonString(const std::wstring& value) { return L"\"" + JsonEscape(value) + L"\""; }

std::wstring JsonRect(const RECT& r) {
    std::wstringstream s;
    s << L"{\"left\": " << r.left << L", \"top\": " << r.top << L", \"right\": " << r.right << L", \"bottom\": " << r.bottom << L"}";
    return s.str();
}

bool IsHex(wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'); }

} // namespace

std::wstring JsonEscape(const std::wstring& text) {
    std::wstring out;
    for (wchar_t c : text) {
        switch (c) {
            case L'"': out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default:
                if (c < 0x20) {
                    wchar_t buffer[8]{};
                    swprintf(buffer, std::size(buffer), L"\\u%04X", static_cast<unsigned>(c));
                    out += buffer;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::wstring CEscape(const std::wstring& text) {
    std::wstring out;
    bool after_hex_escape = false;
    for (wchar_t c : text) {
        // A \x escape is greedy, so a following hex digit would be swallowed; close and reopen the literal.
        if (after_hex_escape && IsHex(c)) out += L"\" L\"";
        after_hex_escape = false;
        switch (c) {
            case L'"': out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default:
                if (c < 0x20 || c > 0x7E) {
                    wchar_t buffer[8]{};
                    swprintf(buffer, std::size(buffer), L"\\x%04X", static_cast<unsigned>(c));
                    out += buffer;
                    after_hex_escape = true;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::wstring FocusAsText(const FocusSnapshot& s) {
    std::wstringstream out;
    out << str::kPropProcess << L": " << OrUnavailable(s.process.name) << L"\r\n"
        << str::kPropPid << L": " << s.window.pid << L"\r\n"
        << str::kPropExe << L": " << OrUnavailable(s.process.exe_path) << L"\r\n"
        << str::kPropHwnd << L": " << HexHandle(s.window.hwnd) << L"\r\n"
        << str::kPropClass << L": " << OrUnavailable(s.window.class_name) << L"\r\n"
        << str::kPropTitle << L": " << OrUnavailable(s.window.title) << L"\r\n"
        << str::kPropWindowRect << L": " << RectText(s.window.bounds) << L"\r\n";
    if (s.has_cursor) out << str::kPropCursor << L": " << s.cursor.x << L", " << s.cursor.y << L"\r\n";
    if (!s.automation.available) {
        out << L"UI Automation: " << str::kUnavailable << L"\r\n";
    } else {
        const AutomationInfo& a = s.automation;
        out << str::kPropControlType << L": " << OrUnavailable(a.control_type) << L"\r\n"
            << str::kPropName << L": " << OrUnavailable(a.name) << L"\r\n"
            << str::kPropAutomationId << L": " << OrUnavailable(a.automation_id) << L"\r\n"
            << str::kPropUiaClass << L": " << OrUnavailable(a.class_name) << L"\r\n"
            << str::kPropFramework << L": " << OrUnavailable(a.framework) << L"\r\n"
            << str::kPropValue << L": "
            << (a.value_hidden ? std::wstring(str::kValueHidden) : (a.value_available ? a.value : std::wstring(str::kUnavailable))) << L"\r\n"
            << str::kPropUiaBounds << L": " << (a.bounds_valid ? RectText(a.bounds) : std::wstring(str::kUnavailable)) << L"\r\n";
    }
    return out.str();
}

std::wstring FocusAsJson(const FocusSnapshot& s) {
    const AutomationInfo& a = s.automation;
    std::wstringstream out;
    out << L"{\n"
        << L"  \"window\": {\n"
        << L"    \"hwnd\": " << JsonString(HexHandle(s.window.hwnd)) << L",\n"
        << L"    \"class\": " << JsonString(s.window.class_name) << L",\n"
        << L"    \"title\": " << JsonString(s.window.title) << L",\n"
        << L"    \"bounds\": " << JsonRect(s.window.bounds) << L"\n  },\n"
        << L"  \"process\": {\n"
        << L"    \"pid\": " << s.window.pid << L",\n"
        << L"    \"parentPid\": " << s.process.parent_pid << L",\n"
        << L"    \"name\": " << JsonString(s.process.name) << L",\n"
        << L"    \"path\": " << JsonString(s.process.exe_path) << L"\n  },\n"
        << L"  \"element\": ";
    if (!a.available) {
        out << L"null,\n";
    } else {
        out << L"{\n"
            << L"    \"automationId\": " << JsonString(a.automation_id) << L",\n"
            << L"    \"controlType\": " << JsonString(a.control_type) << L",\n"
            << L"    \"className\": " << JsonString(a.class_name) << L",\n"
            << L"    \"framework\": " << JsonString(a.framework) << L",\n"
            << L"    \"name\": " << JsonString(a.name) << L",\n"
            << L"    \"value\": " << (a.value_available && !a.value_hidden ? JsonString(a.value) : std::wstring(L"null")) << L",\n"
            << L"    \"valueHidden\": " << (a.value_hidden ? L"true" : L"false") << L",\n"
            << L"    \"enabled\": " << (a.enabled ? L"true" : L"false") << L",\n"
            << L"    \"offscreen\": " << (a.offscreen ? L"true" : L"false") << L",\n"
            << L"    \"bounds\": " << (a.bounds_valid ? JsonRect(a.bounds) : std::wstring(L"null")) << L"\n  },\n";
    }
    out << L"  \"cursor\": ";
    if (s.has_cursor) out << L"{\"x\": " << s.cursor.x << L", \"y\": " << s.cursor.y << L"}\n";
    else out << L"null\n";
    out << L"}\n";
    return out.str();
}

std::wstring FocusAsC(const FocusSnapshot& s) {
    const AutomationInfo& a = s.automation;
    const RECT& r = a.bounds_valid ? a.bounds : s.window.bounds;
    std::wstringstream out;
    out << L"// Captured by Peek Focus. Bounds are screen pixels (UI Automation bounds when available, else the window).\n"
        << L"typedef struct PeekElement {\n"
        << L"    const wchar_t* window_class;\n"
        << L"    const wchar_t* window_title;\n"
        << L"    const wchar_t* automation_id;\n"
        << L"    const wchar_t* control_type;\n"
        << L"    const wchar_t* name;\n"
        << L"    unsigned long pid;\n"
        << L"    long left, top, right, bottom;\n"
        << L"} PeekElement;\n\n"
        << L"static const PeekElement kPeekElement = {\n"
        << L"    L\"" << CEscape(s.window.class_name) << L"\",\n"
        << L"    L\"" << CEscape(s.window.title) << L"\",\n"
        << L"    L\"" << CEscape(a.automation_id) << L"\",\n"
        << L"    L\"" << CEscape(a.control_type) << L"\",\n"
        << L"    L\"" << CEscape(a.name) << L"\",\n"
        << L"    " << s.window.pid << L",\n"
        << L"    " << r.left << L", " << r.top << L", " << r.right << L", " << r.bottom << L"\n};\n";
    return out.str();
}

} // namespace peek
