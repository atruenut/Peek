#pragma once

#include "core/types.h"

#include <string>

namespace peek {

// Clipboard formats for a Focus capture. Unavailable properties are reported as unavailable (text),
// null (JSON) or an empty string (C), never as invented values.
[[nodiscard]] std::wstring FocusAsText(const FocusSnapshot& snapshot);
[[nodiscard]] std::wstring FocusAsJson(const FocusSnapshot& snapshot);
[[nodiscard]] std::wstring FocusAsC(const FocusSnapshot& snapshot);

// Escapes for embedding in JSON / C wide string literals. Exposed for unit tests.
[[nodiscard]] std::wstring JsonEscape(const std::wstring& text);
[[nodiscard]] std::wstring CEscape(const std::wstring& text);

} // namespace peek
