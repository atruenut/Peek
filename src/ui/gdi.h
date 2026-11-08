#pragma once

#include <windows.h>

#include <string>

namespace peek::gdi {

inline int Scale(int value, UINT dpi) { return MulDiv(value, static_cast<int>(dpi), 96); }

inline void Fill(HDC dc, const RECT& rect, COLORREF color) {
    SetDCBrushColor(dc, color);
    FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

inline void Frame(HDC dc, const RECT& rect, COLORREF color) {
    SetDCBrushColor(dc, color);
    FrameRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

inline void Text(HDC dc, const std::wstring& text, RECT rect, COLORREF color,
                 UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX) {
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, format);
}

inline int TextWidth(HDC dc, const std::wstring& text) {
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    return size.cx;
}

inline void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

inline HFONT MakeFont(UINT dpi, int points, int weight) {
    LOGFONTW log{};
    log.lfHeight = -MulDiv(points, static_cast<int>(dpi), 72);
    log.lfWeight = weight;
    log.lfQuality = CLEARTYPE_QUALITY;
    lstrcpyW(log.lfFaceName, L"Segoe UI");
    return CreateFontIndirectW(&log);
}

} // namespace peek::gdi
