#pragma once

#include <cstdint>

#include "amg/flightwall/mode.hpp"
#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

class Application {
 public:
  Application(Renderer& renderer, Scene& classic_scene, Scene& operations_scene) noexcept;

  [[nodiscard]] ModeController& modes() noexcept;
  [[nodiscard]] const ModeController& modes() const noexcept;
  void tick(std::uint64_t monotonic_ms) noexcept;

 private:
  Renderer& renderer_;
  Scene& classic_scene_;
  Scene& operations_scene_;
  SceneManager scenes_{};
  ModeController modes_{};
  std::uint64_t frame_number_{0};
};

}  // namespace amg::flightwall
