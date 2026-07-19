#pragma once

#include <cstdint>
#include <string_view>

#include "amg/flightwall/renderer.hpp"

namespace amg::flightwall {

struct FrameContext {
  Renderer& renderer;
  std::uint64_t monotonic_ms;
  std::uint64_t frame_number;
};

class Scene {
 public:
  virtual ~Scene() = default;

  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
  virtual void onEnter() noexcept {}
  virtual void onExit() noexcept {}
  virtual void render(FrameContext& context) noexcept = 0;
};

class SceneManager {
 public:
  void activate(Scene& scene) noexcept;
  void render(FrameContext& context) noexcept;

  [[nodiscard]] Scene* active() const noexcept;

 private:
  Scene* active_{nullptr};
};

}  // namespace amg::flightwall
