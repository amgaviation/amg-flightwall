#pragma once

#include <array>
#include <cstdint>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "amg/flightwall/display.hpp"
#include "amg/flightwall/target_profile.hpp"

namespace amg::flightwall {

enum class HdWf2DisplayInitialization { ready, dma_allocation_failed };

class HdWf2Display final : public Display {
 public:
  HdWf2Display() noexcept;

  [[nodiscard]] HdWf2DisplayInitialization begin() noexcept;
  void setBrightness(std::uint8_t brightness) noexcept;

  [[nodiscard]] int width() const noexcept override;
  [[nodiscard]] int height() const noexcept override;
  void clear(Color color) noexcept override;
  void setPixel(int x, int y, Color color) noexcept override;
  [[nodiscard]] Color pixel(int x, int y) const noexcept override;
  void present() noexcept override;

 private:
  static constexpr TargetProfile profile_ = hdWf2MiniProfile();
  static constexpr std::size_t pixel_count_ =
      static_cast<std::size_t>(profile_.display_width * profile_.display_height);

  [[nodiscard]] static HUB75_I2S_CFG makeMatrixConfiguration() noexcept;
  [[nodiscard]] static constexpr bool contains(const int x, const int y) noexcept {
    return x >= 0 && y >= 0 && x < profile_.display_width && y < profile_.display_height;
  }
  [[nodiscard]] static constexpr std::size_t offset(const int x, const int y) noexcept {
    return static_cast<std::size_t>(y * profile_.display_width + x);
  }

  std::array<Color, pixel_count_> pixels_{};
  MatrixPanel_I2S_DMA matrix_;
  bool initialized_{false};
};

}  // namespace amg::flightwall
