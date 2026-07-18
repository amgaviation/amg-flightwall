#pragma once

#include <string_view>

#include "amg/flightwall/display.hpp"

namespace amg::flightwall {

class Renderer {
 public:
  explicit Renderer(Display& display) noexcept;

  [[nodiscard]] int width() const noexcept;
  [[nodiscard]] int height() const noexcept;
  void clear(Color color) noexcept;
  void pixel(int x, int y, Color color) noexcept;
  void line(int x0, int y0, int x1, int y1, Color color) noexcept;
  void rectangle(int x, int y, int width, int height, Color color) noexcept;
  void fillRectangle(int x, int y, int width, int height, Color color) noexcept;
  void text(int x, int y, std::string_view value, Color color, int scale = 1) noexcept;
  [[nodiscard]] int textWidth(std::string_view value, int scale = 1) const noexcept;
  void present() noexcept;

 private:
  void glyph(int x, int y, char character, Color color, int scale) noexcept;

  Display& display_;
};

}  // namespace amg::flightwall
