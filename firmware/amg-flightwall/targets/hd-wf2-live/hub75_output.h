// AMG FlightWall live target — HUB75 panel output.
//
// Implements the portable core `Display` over MatrixPanel_I2S_DMA on the
// HD-WF2 75EX2 port (X2 RGB pins via hdWf2MiniX2Profile(); the X1 color
// buffer is dead and must never be driven). Geometry, driver chip, clkphase,
// latch blanking, and minimum refresh come from DeviceSettings at begin()
// time so panel tuning never requires a reflash. Single buffer + full repaint
// per frame; the NO_GFX MatrixPanel build is used.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "amg/flightwall/display.hpp"
#include "amg/flightwall/target_profile.hpp"
#include "settings_store.h"

namespace amg::flightwall::live {

enum class Hub75Initialization { ready, dma_allocation_failed };

class Hub75Output final : public Display {
 public:
  // Allocates the DMA matrix using runtime geometry. Call once from setup().
  [[nodiscard]] Hub75Initialization begin(const GeometrySettings& geometry) noexcept;

  void setBrightness(std::uint8_t brightness) noexcept;

  // Encodes the current frame as RGB565 (big-endian per pixel) into `out`;
  // used by the SSE `frame` channel. Loop-task only.
  void encodeRgb565(std::vector<std::uint8_t>& out) const;

  // Display interface.
  [[nodiscard]] int width() const noexcept override;
  [[nodiscard]] int height() const noexcept override;
  void clear(Color color) noexcept override;
  void setPixel(int x, int y, Color color) noexcept override;
  [[nodiscard]] Color pixel(int x, int y) const noexcept override;
  void present() noexcept override;

 private:
  [[nodiscard]] bool contains(int x, int y) const noexcept;
  [[nodiscard]] std::size_t offset(int x, int y) const noexcept;

  TargetProfile profile_{hdWf2MiniX2Profile()};
  std::unique_ptr<MatrixPanel_I2S_DMA> matrix_{};
  std::vector<Color> pixels_{};
  int width_{0};
  int height_{0};
  bool initialized_{false};
};

}  // namespace amg::flightwall::live
