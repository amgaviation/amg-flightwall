#include "hd_wf2_display.hpp"

#include <algorithm>

namespace amg::flightwall {

HUB75_I2S_CFG HdWf2Display::makeMatrixConfiguration() noexcept {
  const Hub75Pins& pins = profile_.hub75;
  const HUB75_I2S_CFG::i2s_pins pin_map{
      static_cast<std::int8_t>(pins.r1),
      static_cast<std::int8_t>(pins.g1),
      static_cast<std::int8_t>(pins.b1),
      static_cast<std::int8_t>(pins.r2),
      static_cast<std::int8_t>(pins.g2),
      static_cast<std::int8_t>(pins.b2),
      static_cast<std::int8_t>(pins.row_a),
      static_cast<std::int8_t>(pins.row_b),
      static_cast<std::int8_t>(pins.row_c),
      static_cast<std::int8_t>(pins.row_d),
      static_cast<std::int8_t>(pins.row_e),
      static_cast<std::int8_t>(pins.latch),
      static_cast<std::int8_t>(pins.output_enable),
      static_cast<std::int8_t>(pins.clock),
  };

  HUB75_I2S_CFG configuration(profile_.display_width, profile_.display_height, 1, pin_map);
  configuration.driver = HUB75_I2S_CFG::FM6124;
  configuration.double_buff = false;
  configuration.setPixelColorDepthBits(4);
  return configuration;
}

HdWf2Display::HdWf2Display() noexcept : matrix_(makeMatrixConfiguration()) {
  clear(colors::black);
}

HdWf2DisplayInitialization HdWf2Display::begin() noexcept {
  initialized_ = matrix_.begin();
  if (!initialized_) {
    return HdWf2DisplayInitialization::dma_allocation_failed;
  }

  setBrightness(64);
  matrix_.clearScreen();
  present();
  return HdWf2DisplayInitialization::ready;
}

void HdWf2Display::setBrightness(const std::uint8_t brightness) noexcept {
  matrix_.setBrightness8(brightness);
}

int HdWf2Display::width() const noexcept { return profile_.display_width; }

int HdWf2Display::height() const noexcept { return profile_.display_height; }

void HdWf2Display::clear(const Color color) noexcept {
  std::fill(pixels_.begin(), pixels_.end(), color);
}

void HdWf2Display::setPixel(const int x, const int y, const Color color) noexcept {
  if (contains(x, y)) {
    pixels_[offset(x, y)] = color;
  }
}

Color HdWf2Display::pixel(const int x, const int y) const noexcept {
  return contains(x, y) ? pixels_[offset(x, y)] : colors::black;
}

void HdWf2Display::present() noexcept {
  if (!initialized_) {
    return;
  }

  for (int y = 0; y < profile_.display_height; ++y) {
    for (int x = 0; x < profile_.display_width; ++x) {
      const Color color = pixels_[offset(x, y)];
      matrix_.drawPixelRGB888(x, y, color.red, color.green, color.blue);
    }
  }
}

}  // namespace amg::flightwall
