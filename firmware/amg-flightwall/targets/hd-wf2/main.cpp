#include <Arduino.h>

#include "amg/flightwall/application.hpp"
#include "amg/flightwall/renderer.hpp"
#include "amg/flightwall/scenes.hpp"
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

    classic_scene_.setContacts({{"AMG001", 64, 32, 12'000, 286, true}});
    operations_scene_.setStatuses({{"SYSTEM", ServiceState::healthy, 100}});
    application_.modes().setRequestedMode(Mode::classic);
    display_ready_ = true;
    Serial.println("AMG FlightWall: build-only HD-WF2 target initialized");
  }

  void loop(const std::uint64_t monotonic_ms) noexcept {
    if (display_ready_) {
      application_.tick(monotonic_ms);
    }
  }

 private:
  HdWf2Display display_;
  Renderer renderer_{display_};
  ClassicScene classic_scene_;
  OperationsScene operations_scene_;
  Application application_{renderer_, classic_scene_, operations_scene_};
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
