#include <Arduino.h>

#include "amg/flightwall/hardware_smoke_scene.hpp"
#include "amg/flightwall/renderer.hpp"
#include "hd_wf2_display.hpp"

namespace {

using namespace amg::flightwall;

class FlightWallRuntime final {
 public:
  void setup() noexcept {
    const HdWf2DisplayInitialization result = display_.begin();
    if (result != HdWf2DisplayInitialization::ready) {
      Serial.println("AMG FlightWall: HUB75 DMA allocation failed");
      return;
    }

    scenes_.activate(smoke_scene_);
    display_ready_ = true;
    Serial.println("AMG FlightWall: hardware smoke screen initialized");
  }

  void loop(const std::uint64_t monotonic_ms) noexcept {
    if (display_ready_) {
      FrameContext context{renderer_, monotonic_ms, frame_number_++};
      scenes_.render(context);
      renderer_.present();
    }
  }

 private:
  HdWf2Display display_;
  Renderer renderer_{display_};
  HardwareSmokeScene smoke_scene_{{0, SmokeCheckState::running, SmokeCheckState::pending,
                                    SmokeCheckState::running, SmokeCheckState::pending}};
  SceneManager scenes_{};
  std::uint64_t frame_number_{0};
  bool display_ready_{false};
};

FlightWallRuntime& runtime() noexcept {
  static FlightWallRuntime instance;
  return instance;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  runtime().setup();
}

void loop() {
  runtime().loop(millis());
  delay(100);
}
