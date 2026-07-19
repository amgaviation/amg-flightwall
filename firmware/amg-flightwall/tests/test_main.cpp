#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "amg/flightwall/application.hpp"
#include "amg/flightwall/display.hpp"
#include "amg/flightwall/mode.hpp"
#include "amg/flightwall/plugin.hpp"
#include "amg/flightwall/renderer.hpp"
#include "amg/flightwall/scene.hpp"
#include "amg/flightwall/scenes.hpp"
#include "amg/flightwall/target_profile.hpp"

namespace {

using namespace amg::flightwall;

struct TestFailure final : std::runtime_error {
  using std::runtime_error::runtime_error;
};

int tests_run = 0;

void check(const bool condition, const std::string_view expression, const int line) {
  if (!condition) {
    throw TestFailure("line " + std::to_string(line) + ": " + std::string(expression));
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

template <typename Test>
void run(const std::string_view name, Test&& test) {
  test();
  ++tests_run;
  std::cout << "PASS " << name << '\n';
}

class CountingScene final : public Scene {
 public:
  explicit CountingScene(std::string_view scene_id) : scene_id_(scene_id) {}

  [[nodiscard]] std::string_view id() const noexcept override { return scene_id_; }
  void onEnter() noexcept override { ++enters; }
  void onExit() noexcept override { ++exits; }
  void render(FrameContext&) noexcept override { ++renders; }

  std::string scene_id_;
  int enters{0};
  int exits{0};
  int renders{0};
};

class FakePlugin final : public Plugin {
 public:
  explicit FakePlugin(PluginManifest value) : manifest_(value) {}

  [[nodiscard]] const PluginManifest& manifest() const noexcept override { return manifest_; }
  bool initialize() noexcept override { return true; }
  void activate() noexcept override {}
  void deactivate() noexcept override {}
  void stop() noexcept override {}

 private:
  PluginManifest manifest_;
};

void testFrameBufferBounds() {
  FrameBufferDisplay display(4, 3);
  display.setPixel(2, 1, colors::white);
  display.setPixel(-1, 0, colors::red);
  display.setPixel(4, 2, colors::red);
  CHECK(display.pixel(2, 1) == colors::white);
  CHECK(display.pixel(-1, 0) == colors::black);
  CHECK(display.litPixelCount() == 1);
  display.present();
  CHECK(display.presentCount() == 1);
}

void testRendererClippingAndText() {
  FrameBufferDisplay display(20, 12);
  Renderer renderer(display);
  renderer.fillRectangle(-2, -2, 5, 5, colors::amg_blue);
  CHECK(display.litPixelCount() == 9);
  renderer.clear(colors::black);
  renderer.text(0, 0, "A1", colors::white);
  CHECK(display.litPixelCount() > 10);
  CHECK(renderer.textWidth("A1") == 11);
}

void testSceneLifecycle() {
  FrameBufferDisplay display(16, 16);
  Renderer renderer(display);
  FrameContext context{renderer, 100, 1};
  CountingScene first("first");
  CountingScene second("second");
  SceneManager manager;

  manager.activate(first);
  manager.activate(first);
  manager.render(context);
  manager.activate(second);

  CHECK(first.enters == 1);
  CHECK(first.exits == 1);
  CHECK(first.renders == 1);
  CHECK(second.enters == 1);
  CHECK(manager.active() == &second);
}

void testAutomaticMode() {
  ModeController modes;
  modes.setRequestedMode(Mode::automatic);
  CHECK(modes.effectiveMode(100) == Mode::classic);

  modes.observe({AutoTrigger::active_mission, 80, 200, 500, "mission AMG-42 active"});
  CHECK(modes.effectiveMode(199) == Mode::classic);
  CHECK(modes.effectiveMode(200) == Mode::operations);
  CHECK(modes.effectiveMode(499) == Mode::operations);
  CHECK(modes.effectiveMode(500) == Mode::classic);

  modes.setRequestedMode(Mode::classic);
  CHECK(modes.effectiveMode(300) == Mode::classic);
}

void testTriggerPriority() {
  ModeController modes;
  modes.observe({AutoTrigger::airport_event, 90, 100, 900, "airport closure"});
  modes.observe({AutoTrigger::crew_notification, 20, 110, 800, "crew update"});
  CHECK(modes.activeTrigger().has_value());
  CHECK(modes.activeTrigger()->type == AutoTrigger::airport_event);
  modes.observe({AutoTrigger::maintenance_alert, 100, 120, 700, "maintenance due"});
  CHECK(modes.activeTrigger()->type == AutoTrigger::maintenance_alert);
}

void testPluginRegistration() {
  PluginRegistry registry;
  FakePlugin flights({"amg.flights", "0.1.0", 1, 64'000});
  FakePlugin duplicate({"amg.flights", "0.2.0", 1, 32'000});
  FakePlugin incompatible({"amg.weather", "0.1.0", 2, 32'000});
  FakePlugin invalid({"", "0.1.0", 1, 32'000});

  CHECK(registry.registerPlugin(flights) == PluginRegistrationResult::registered);
  CHECK(registry.registerPlugin(duplicate) == PluginRegistrationResult::duplicate_id);
  CHECK(registry.registerPlugin(incompatible) == PluginRegistrationResult::incompatible_core_api);
  CHECK(registry.registerPlugin(invalid) == PluginRegistrationResult::invalid_manifest);
  CHECK(registry.find("amg.flights") == &flights);
  CHECK(registry.size() == 1);
}

void testApplicationRendersModes() {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  ClassicScene classic;
  OperationsScene operations;
  classic.setContacts({{"N721AM", 20, 24, 12'000, 286, true}});
  operations.setStatuses({{"FLEET", ServiceState::healthy, 80}});
  Application application(renderer, classic, operations);

  application.modes().setRequestedMode(Mode::classic);
  application.tick(100);
  const std::size_t classic_pixels = display.litPixelCount();
  CHECK(classic_pixels > 0);

  application.modes().setRequestedMode(Mode::operations);
  application.tick(200);
  CHECK(display.presentCount() == 2);
  CHECK(display.litPixelCount() > 0);
  CHECK(display.litPixelCount() != classic_pixels);
}

void testHdWf2MiniProfile() {
  constexpr TargetProfile profile = hdWf2MiniProfile();

  CHECK(profile.display_width == 128);
  CHECK(profile.display_height == 64);
  CHECK(profile.scan_denominator == 32);
  CHECK(profile.flash_bytes == 8U * 1024U * 1024U);
  CHECK(profile.psram_enabled == false);
  CHECK((profile.hub75.values() ==
         std::array<int, 14>{2, 6, 10, 3, 7, 11, 39, 38, 37, 36, 21, 33, 35, 34}));
  CHECK(profile.pins_are_unique());
  CHECK(profile.uses_gpio(19) == false);
  CHECK(profile.uses_gpio(20) == false);
  CHECK(profile.partition_map.is_contiguous());
  CHECK(profile.partition_map.entries[2].offset == 0x00010000U);
  CHECK(profile.partition_map.entries[2].size == 0x002C0000U);
  CHECK(profile.partition_map.entries[3].offset == 0x002D0000U);
  CHECK(profile.partition_map.entries[3].size == 0x002C0000U);
  CHECK(profile.partition_map.end_offset() == profile.flash_bytes);
}

}  // namespace

int main() {
  try {
    run("frame buffer bounds", testFrameBufferBounds);
    run("renderer clipping and text", testRendererClippingAndText);
    run("scene lifecycle", testSceneLifecycle);
    run("automatic mode", testAutomaticMode);
    run("trigger priority", testTriggerPriority);
    run("plugin registration", testPluginRegistration);
    run("application renders modes", testApplicationRendersModes);
    run("HD-WF2 Mini profile", testHdWf2MiniProfile);
    std::cout << tests_run << " tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
