#pragma once

#include <cstdint>

namespace amg::flightwall {

struct Color {
  std::uint8_t red{0};
  std::uint8_t green{0};
  std::uint8_t blue{0};

  [[nodiscard]] constexpr bool operator==(const Color& other) const noexcept {
    return red == other.red && green == other.green && blue == other.blue;
  }

  [[nodiscard]] constexpr bool operator!=(const Color& other) const noexcept {
    return !(*this == other);
  }
};

namespace colors {
inline constexpr Color black{0, 0, 0};
inline constexpr Color white{255, 255, 255};
inline constexpr Color amg_blue{23, 108, 255};
inline constexpr Color cyan{0, 220, 255};
inline constexpr Color green{36, 220, 105};
inline constexpr Color amber{255, 174, 0};
inline constexpr Color red{255, 59, 48};
inline constexpr Color muted{70, 84, 104};
}  // namespace colors

}  // namespace amg::flightwall
