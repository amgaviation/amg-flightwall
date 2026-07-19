#include "amg/flightwall/application.hpp"

namespace amg::flightwall {

Application::Application(Renderer& renderer, Scene& classic_scene, Scene& operations_scene) noexcept
    : renderer_(renderer), classic_scene_(classic_scene), operations_scene_(operations_scene) {}

ModeController& Application::modes() noexcept { return modes_; }

const ModeController& Application::modes() const noexcept { return modes_; }

void Application::tick(const std::uint64_t monotonic_ms) noexcept {
  const Mode effective = modes_.effectiveMode(monotonic_ms);
  scenes_.activate(effective == Mode::operations ? operations_scene_ : classic_scene_);
  FrameContext context{renderer_, monotonic_ms, frame_number_++};
  scenes_.render(context);
  renderer_.present();
}

}  // namespace amg::flightwall
