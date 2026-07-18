#include "amg/flightwall/display.hpp"

#include <algorithm>
#include <stdexcept>

namespace amg::flightwall {

FrameBufferDisplay::FrameBufferDisplay(const int width, const int height)
    : width_(width), height_(height) {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("display dimensions must be positive");
  }
  pixels_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), colors::black);
}

int FrameBufferDisplay::width() const noexcept { return width_; }

int FrameBufferDisplay::height() const noexcept { return height_; }

void FrameBufferDisplay::clear(const Color color) noexcept {
  std::fill(pixels_.begin(), pixels_.end(), color);
}

void FrameBufferDisplay::setPixel(const int x, const int y, const Color color) noexcept {
  if (contains(x, y)) {
    pixels_[offset(x, y)] = color;
  }
}

Color FrameBufferDisplay::pixel(const int x, const int y) const noexcept {
  return contains(x, y) ? pixels_[offset(x, y)] : colors::black;
}

void FrameBufferDisplay::present() noexcept { ++present_count_; }

std::size_t FrameBufferDisplay::presentCount() const noexcept { return present_count_; }

std::size_t FrameBufferDisplay::litPixelCount() const noexcept {
  return static_cast<std::size_t>(std::count_if(pixels_.begin(), pixels_.end(), [](const Color color) {
    return color != colors::black;
  }));
}

const std::vector<Color>& FrameBufferDisplay::pixels() const noexcept { return pixels_; }

bool FrameBufferDisplay::contains(const int x, const int y) const noexcept {
  return x >= 0 && y >= 0 && x < width_ && y < height_;
}

std::size_t FrameBufferDisplay::offset(const int x, const int y) const noexcept {
  return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
}

}  // namespace amg::flightwall
