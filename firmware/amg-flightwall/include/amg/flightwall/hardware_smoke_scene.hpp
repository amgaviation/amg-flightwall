#pragma once

#include <cstdint>

#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

enum class SmokeCheckState { pending, running, passed, failed };

struct HardwareSmokeProgress {
  std::uint16_t host_tests_passed{0};
  SmokeCheckState target_build{SmokeCheckState::pending};
  SmokeCheckState backup_hash{SmokeCheckState::pending};
  SmokeCheckState hardware_test{SmokeCheckState::pending};
  SmokeCheckState source_write_gate{SmokeCheckState::pending};
};

class HardwareSmokeScene final : public Scene {
 public:
  explicit HardwareSmokeScene(HardwareSmokeProgress progress = {}) noexcept;

  void setProgress(HardwareSmokeProgress progress) noexcept;
  [[nodiscard]] std::string_view id() const noexcept override;
  void render(FrameContext& context) noexcept override;

 private:
  HardwareSmokeProgress progress_{};
};

}  // namespace amg::flightwall
