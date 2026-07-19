#include "hub75_output.h"

#include <algorithm>

namespace amg::flightwall::live {

namespace {

[[nodiscard]] HUB75_I2S_CFG::shift_driver driverFromName(const String& name) noexcept {
  if (name == "FM6124") {
    return HUB75_I2S_CFG::FM6124;
  }
  if (name == "ICN2038S") {
    return HUB75_I2S_CFG::ICN2038S;
  }
  if (name == "MBI5124") {
    return HUB75_I2S_CFG::MBI5124;
  }
  if (name == "SHIFTREG") {
    return HUB75_I2S_CFG::SHIFTREG;
  }
  return HUB75_I2S_CFG::FM6126A;  // factory default
}

}  // namespace

Hub75Initialization Hub75Output::begin(const GeometrySettings& geometry) noexcept {
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

  // Factory-verified defaults: 64x64 panel, chain 2 -> raw linear 128x64.
  HUB75_I2S_CFG configuration(static_cast<std::uint16_t>(geometry.panel_w),
                              static_cast<std::uint16_t>(geometry.panel_h),
                              static_cast<std::uint16_t>(geometry.chain), pin_map);
  configuration.driver = driverFromName(geometry.driver);
  configuration.i2sspeed = HUB75_I2S_CFG::HZ_8M;
  configuration.clkphase = geometry.clkphase;
  configuration.latch_blanking = static_cast<std::uint8_t>(geometry.latch_blanking);
  configuration.min_refresh_rate = static_cast<std::uint8_t>(geometry.min_refresh);
  configuration.double_buff = false;
  configuration.setPixelColorDepthBits(8);

  width_ = geometry.panel_w * geometry.chain;
  height_ = geometry.panel_h;
  pixels_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_),
                 colors::black);

  matrix_ = std::make_unique<MatrixPanel_I2S_DMA>(configuration);
  initialized_ = matrix_ && matrix_->begin();
  if (!initialized_) {
    matrix_.reset();
    return Hub75Initialization::dma_allocation_failed;
  }

  matrix_->clearScreen();
  return Hub75Initialization::ready;
}

void Hub75Output::setBrightness(const std::uint8_t brightness) noexcept {
  if (initialized_) {
    matrix_->setBrightness8(brightness);
  }
}

void Hub75Output::encodeRgb565(std::vector<std::uint8_t>& out) const {
  out.resize(pixels_.size() * 2);
  std::size_t cursor = 0;
  for (const Color& color : pixels_) {
    const std::uint16_t value =
        static_cast<std::uint16_t>(((color.red & 0xf8) << 8) | ((color.green & 0xfc) << 3) |
                                   (color.blue >> 3));
    out[cursor++] = static_cast<std::uint8_t>(value >> 8);
    out[cursor++] = static_cast<std::uint8_t>(value & 0xff);
  }
}

int Hub75Output::width() const noexcept { return width_; }

int Hub75Output::height() const noexcept { return height_; }

void Hub75Output::clear(const Color color) noexcept {
  std::fill(pixels_.begin(), pixels_.end(), color);
}

void Hub75Output::setPixel(const int x, const int y, const Color color) noexcept {
  if (contains(x, y)) {
    pixels_[offset(x, y)] = color;
  }
}

Color Hub75Output::pixel(const int x, const int y) const noexcept {
  return contains(x, y) ? pixels_[offset(x, y)] : colors::black;
}

void Hub75Output::present() noexcept {
  if (!initialized_) {
    return;
  }
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      const Color color = pixels_[offset(x, y)];
      matrix_->drawPixelRGB888(static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
                               color.red, color.green, color.blue);
    }
  }
}

bool Hub75Output::contains(const int x, const int y) const noexcept {
  return x >= 0 && x < width_ && y >= 0 && y < height_;
}

std::size_t Hub75Output::offset(const int x, const int y) const noexcept {
  return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
         static_cast<std::size_t>(x);
}

}  // namespace amg::flightwall::live
