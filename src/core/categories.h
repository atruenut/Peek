#pragma once

// Event categories shown to the user. Pure C++ (no windows.h) so settings code can use it.

#include <cstdint>

namespace peek {

// A process may own many windows and a window can disappear long before its process does,
// so window and process events are deliberately separate categories.
enum class EventCategory {
    ProcessStarted = 0,
    WindowAppeared = 1,
    WindowDisappeared = 2,
    ProcessTerminated = 3,
};

constexpr std::uint32_t kAllCategories = 0xF;

[[nodiscard]] constexpr std::uint32_t CategoryBit(EventCategory category) {
    return 1u << static_cast<unsigned>(category);
}

} // namespace peek
