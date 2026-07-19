#pragma once

#include <cstddef>
#include <vector>

#include "amg/flightwall/color.hpp"

namespace amg::flightwall {

class Display {
 public:
  virtual ~Display() = default;

  [[nodiscard]] virtual int width() const noexcept = 0;
  [[nodiscard]] virtual int height() const noexcept = 0;
  virtual void clear(Color color) noexcept = 0;
  virtual void setPixel(int x, int y, Color color) noexcept = 0;
  [[nodiscard]] virtual Color pixel(int x, int y) const noexcept = 0;
  virtual void present() noexcept = 0;
};

class FrameBufferDisplay final : public Display {
 public:
  FrameBufferDisplay(int width, int height);

  [[nodiscard]] int width() const noexcept override;
  [[nodiscard]] int height() const noexcept override;
  void clear(Color color) noexcept override;
  void setPixel(int x, int y, Color color) noexcept override;
  [[nodiscard]] Color pixel(int x, int y) const noexcept override;
  void present() noexcept override;

  [[nodiscard]] std::size_t presentCount() const noexcept;
  [[nodiscard]] std::size_t litPixelCount() const noexcept;
  [[nodiscard]] const std::vector<Color>& pixels() const noexcept;

 private:
  [[nodiscard]] bool contains(int x, int y) const noexcept;
  [[nodiscard]] std::size_t offset(int x, int y) const noexcept;

  int width_;
  int height_;
  std::vector<Color> pixels_;
  std::size_t present_count_{0};
};

}  // namespace amg::flightwall
