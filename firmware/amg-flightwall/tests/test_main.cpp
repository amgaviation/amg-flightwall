#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "amg/flightwall/application.hpp"
#include "amg/flightwall/configuration.hpp"
#include "amg/flightwall/diagnostics.hpp"
#include "amg/flightwall/display.hpp"
#include "amg/flightwall/mode.hpp"
#include "amg/flightwall/plugin.hpp"
#include "amg/flightwall/renderer.hpp"
#include "amg/flightwall/scene.hpp"
#include "amg/flightwall/scenes.hpp"
#include "amg/flightwall/target_profile.hpp"
#include "amg/flightwall/wifi_supervisor.hpp"

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

void testDefaultConfigurationIsValid() {
  const DeviceConfiguration configuration;
  const ConfigurationValidation validation = validateConfiguration(configuration);

  CHECK(validation.valid());
  CHECK(validation.errorCount() == 0);
  CHECK(configuration.schema_version == DeviceConfiguration::current_schema_version);
  CHECK(configuration.startup_mode == Mode::classic);
  CHECK(configuration.brightness == 64);
  CHECK(configuration.airport_filter.empty());
  CHECK(configuration.wifi_profile_key == 0);
  CHECK(configuration.wifi_profile_revision == 0);
}

void testConfigurationRejectsUnsupportedSchema() {
  DeviceConfiguration configuration;
  configuration.schema_version = DeviceConfiguration::current_schema_version + 1;

  const ConfigurationValidation validation = validateConfiguration(configuration);
  CHECK(validation.valid() == false);
  CHECK(validation.contains(ConfigurationError::unsupported_schema));
}

void testConfigurationReportsAllInvalidFields() {
  DeviceConfiguration configuration;
  configuration.startup_mode = static_cast<Mode>(255);
  configuration.brightness = 300;
  CHECK(configuration.airport_filter.assign("kfxe"));
  configuration.wifi_profile_key = 42;

  const ConfigurationValidation validation = validateConfiguration(configuration);
  CHECK(validation.errorCount() == 4);
  CHECK(validation.contains(ConfigurationError::invalid_startup_mode));
  CHECK(validation.contains(ConfigurationError::brightness_out_of_range));
  CHECK(validation.contains(ConfigurationError::invalid_airport_filter));
  CHECK(validation.contains(ConfigurationError::invalid_wifi_profile_reference));
}

void testConfigurationBoundsAirportFilterBeforeValidation() {
  DeviceConfiguration configuration;

  CHECK(configuration.airport_filter.assign("KFXE"));
  CHECK(configuration.airport_filter.view() == "KFXE");
  CHECK(configuration.airport_filter.assign("KFXEA") == false);
  CHECK(validateConfiguration(configuration).contains(
      ConfigurationError::invalid_airport_filter));
}

void testWifiRequiresProvisionedProfile() {
  WifiSupervisor wifi;

  CHECK(wifi.poll(0) == WifiAction::none);
  CHECK(wifi.status().state == WifiState::unprovisioned);

  wifi.setConfiguredProfile({1, 1});
  CHECK(wifi.configuredProfile().key == 1);
  CHECK(wifi.configuredProfile().revision == 1);
  CHECK(wifi.poll(1) == WifiAction::connect_using_configured_profile);
  CHECK(wifi.status().state == WifiState::connecting);
  CHECK(wifi.status().attempt_count == 1);
}

void testWifiRetriesTransientFailuresWithBackoff() {
  WifiSupervisor wifi;
  wifi.setConfiguredProfile({1, 1});
  CHECK(wifi.poll(0) == WifiAction::connect_using_configured_profile);

  CHECK(wifi.observe(wifi.activeAttempt(), WifiObservation::connection_timed_out, 100));
  CHECK(wifi.status().state == WifiState::backoff);
  CHECK(wifi.status().last_failure == WifiFailure::connection_timed_out);
  CHECK(wifi.status().next_attempt_ms >= 900);
  CHECK(wifi.status().next_attempt_ms <= 1'300);
  CHECK(wifi.poll(wifi.status().next_attempt_ms - 1) == WifiAction::none);
  CHECK(wifi.poll(wifi.status().next_attempt_ms) ==
        WifiAction::connect_using_configured_profile);

  const std::uint64_t second_failure_ms = wifi.status().next_attempt_ms + 100;
  CHECK(wifi.observe(wifi.activeAttempt(), WifiObservation::connection_timed_out,
                     second_failure_ms));
  CHECK(wifi.status().next_attempt_ms >= second_failure_ms + 1'600);
  CHECK(wifi.status().next_attempt_ms <= second_failure_ms + 2'400);
}

void testWifiAuthenticationFailureRequiresNewProvisioning() {
  WifiSupervisor wifi;
  wifi.setConfiguredProfile({1, 1});
  CHECK(wifi.poll(0) == WifiAction::connect_using_configured_profile);

  CHECK(wifi.observe(wifi.activeAttempt(), WifiObservation::authentication_rejected, 10));
  CHECK(wifi.status().state == WifiState::requires_attention);
  CHECK(wifi.status().last_failure == WifiFailure::authentication_rejected);
  CHECK(wifi.poll(1'000'000) == WifiAction::none);

  wifi.setEnabled(false);
  CHECK(wifi.status().state == WifiState::disabled);
  wifi.setEnabled(true);
  CHECK(wifi.status().state == WifiState::requires_attention);
  CHECK(wifi.poll(1'000'000) == WifiAction::none);

  wifi.setConfiguredProfile({1, 1});
  CHECK(wifi.status().state == WifiState::requires_attention);
  CHECK(wifi.poll(1'000'001) == WifiAction::none);

  wifi.setConfiguredProfile({1, 2});
  CHECK(wifi.status().state == WifiState::idle);
  CHECK(wifi.status().last_failure == WifiFailure::none);
  CHECK(wifi.poll(1'000'001) == WifiAction::connect_using_configured_profile);
}

void testWifiDisconnectsWhenDisabledOrProfileRemoved() {
  WifiSupervisor wifi;
  wifi.setConfiguredProfile({7, 1});
  CHECK(wifi.poll(0) == WifiAction::connect_using_configured_profile);
  CHECK(wifi.observe(wifi.activeAttempt(), WifiObservation::connected, 1));

  wifi.setEnabled(false);
  CHECK(wifi.status().state == WifiState::disabled);
  CHECK(wifi.poll(2) == WifiAction::disconnect);
  CHECK(wifi.poll(3) == WifiAction::none);

  wifi.setEnabled(true);
  CHECK(wifi.poll(4) == WifiAction::connect_using_configured_profile);
  CHECK(wifi.observe(wifi.activeAttempt(), WifiObservation::connected, 5));
  wifi.setConfiguredProfile({});
  CHECK(wifi.status().state == WifiState::unprovisioned);
  CHECK(wifi.poll(6) == WifiAction::disconnect);
}

void testWifiStopsAfterBoundedTransientRetries() {
  WifiSupervisor wifi;
  wifi.setConfiguredProfile({9, 1});
  std::uint64_t now_ms = 0;

  for (std::uint8_t attempt = 1; attempt <= 8; ++attempt) {
    CHECK(wifi.poll(now_ms) == WifiAction::connect_using_configured_profile);
    CHECK(wifi.observe(wifi.activeAttempt(), WifiObservation::connection_timed_out, now_ms));
    if (attempt < 8) {
      CHECK(wifi.status().state == WifiState::backoff);
      CHECK(wifi.status().next_attempt_ms >= now_ms);
      CHECK(wifi.status().next_attempt_ms - now_ms <= 60'000);
      now_ms = wifi.status().next_attempt_ms;
    }
  }

  CHECK(wifi.status().state == WifiState::requires_attention);
  CHECK(wifi.status().last_failure == WifiFailure::retry_exhausted);
  CHECK(wifi.poll(now_ms + 1'000'000) == WifiAction::none);
}

void testWifiIgnoresObservationsFromReplacedAttempt() {
  WifiSupervisor wifi;
  wifi.setConfiguredProfile({10, 1});
  CHECK(wifi.poll(0) == WifiAction::connect_using_configured_profile);
  const WifiConnectionAttempt replaced_attempt = wifi.activeAttempt();

  wifi.setConfiguredProfile({11, 1});
  CHECK(wifi.poll(1) == WifiAction::disconnect);
  CHECK(wifi.poll(2) == WifiAction::connect_using_configured_profile);
  const WifiConnectionAttempt current_attempt = wifi.activeAttempt();

  CHECK(wifi.observe(replaced_attempt, WifiObservation::authentication_rejected, 3) == false);
  CHECK(wifi.status().state == WifiState::connecting);
  CHECK(wifi.observe(current_attempt, WifiObservation::connected, 4));
  CHECK(wifi.status().state == WifiState::online);
  CHECK(wifi.observe(current_attempt, WifiObservation::connection_timed_out, 5) == false);
  CHECK(wifi.observe(current_attempt, WifiObservation::authentication_rejected, 5) == false);
  CHECK(wifi.status().state == WifiState::online);
  CHECK(wifi.observe(current_attempt, WifiObservation::connected, 5));
  CHECK(wifi.observe(current_attempt, WifiObservation::link_lost, 6));
  CHECK(wifi.status().state == WifiState::backoff);
}

void testWifiJitterIsDeterministicDistributedAndSaturating() {
  WifiSupervisor first;
  WifiSupervisor duplicate;
  WifiSupervisor different;
  first.setConfiguredProfile({21, 1});
  duplicate.setConfiguredProfile({21, 1});
  different.setConfiguredProfile({22, 1});

  CHECK(first.poll(0) == WifiAction::connect_using_configured_profile);
  CHECK(duplicate.poll(0) == WifiAction::connect_using_configured_profile);
  CHECK(different.poll(0) == WifiAction::connect_using_configured_profile);
  CHECK(first.observe(first.activeAttempt(), WifiObservation::connection_timed_out, 0));
  CHECK(duplicate.observe(duplicate.activeAttempt(), WifiObservation::connection_timed_out, 0));
  CHECK(different.observe(different.activeAttempt(), WifiObservation::connection_timed_out, 0));
  CHECK(first.status().next_attempt_ms == duplicate.status().next_attempt_ms);
  CHECK(first.status().next_attempt_ms != different.status().next_attempt_ms);

  WifiSupervisor saturating;
  saturating.setConfiguredProfile({23, 1});
  const std::uint64_t near_maximum = std::numeric_limits<std::uint64_t>::max() - 10;
  CHECK(saturating.poll(near_maximum) == WifiAction::connect_using_configured_profile);
  CHECK(saturating.observe(saturating.activeAttempt(), WifiObservation::connection_timed_out,
                           near_maximum));
  CHECK(saturating.status().next_attempt_ms == std::numeric_limits<std::uint64_t>::max());
}

void testDiagnosticsAggregateTypedHealthWithoutPayloads() {
  HealthRegistry health;
  CHECK(health.report({Subsystem::display, HealthLevel::healthy, HealthCode::ok, 100}, 100));
  CHECK(health.report(
      {Subsystem::network, HealthLevel::degraded, HealthCode::connection_timed_out, 200},
      200));

  const DiagnosticsSnapshot snapshot = health.snapshot(250, 1'000);
  CHECK(snapshot.overall == HealthLevel::degraded);
  CHECK(snapshot.entries[static_cast<std::size_t>(Subsystem::display)].level ==
        HealthLevel::healthy);
  CHECK(snapshot.entries[static_cast<std::size_t>(Subsystem::network)].code ==
        HealthCode::connection_timed_out);
}

void testDiagnosticsMarkExpiredReportsStale() {
  HealthRegistry health;
  CHECK(health.report({Subsystem::network, HealthLevel::healthy, HealthCode::ok, 100}, 100));

  const DiagnosticsSnapshot snapshot = health.snapshot(1'101, 1'000);
  const HealthReport& network =
      snapshot.entries[static_cast<std::size_t>(Subsystem::network)];
  CHECK(snapshot.overall == HealthLevel::unknown);
  CHECK(network.level == HealthLevel::unknown);
  CHECK(network.code == HealthCode::stale);
}

void testDiagnosticsRejectTimestampRegressionAndFutureReports() {
  HealthRegistry health;
  CHECK(health.report(
      {Subsystem::display, HealthLevel::failed, HealthCode::allocation_failed, 200}, 200));
  CHECK(health.report({Subsystem::display, HealthLevel::healthy, HealthCode::ok, 100}, 250) ==
        false);
  CHECK(health.report({Subsystem::network, HealthLevel::healthy, HealthCode::ok, 500}, 250) ==
        false);

  const DiagnosticsSnapshot snapshot = health.snapshot(250, 1'000);
  CHECK(snapshot.overall == HealthLevel::failed);
  CHECK(snapshot.entries[static_cast<std::size_t>(Subsystem::display)].code ==
        HealthCode::allocation_failed);

  HealthRegistry clock_reset_health;
  CHECK(clock_reset_health.report(
      {Subsystem::network, HealthLevel::healthy, HealthCode::ok, 200}, 200));
  const DiagnosticsSnapshot clock_reset_snapshot = clock_reset_health.snapshot(150, 1'000);
  CHECK(clock_reset_snapshot.overall == HealthLevel::unknown);
  CHECK(clock_reset_snapshot.entries[static_cast<std::size_t>(Subsystem::network)].level ==
        HealthLevel::unknown);
  CHECK(clock_reset_snapshot.entries[static_cast<std::size_t>(Subsystem::network)].code ==
        HealthCode::timestamp_invalid);
}

void testDiagnosticsRejectInconsistentSeverityAndCode() {
  HealthRegistry health;
  CHECK(health.report(
            {Subsystem::network, HealthLevel::healthy, HealthCode::authentication_rejected, 100},
            100) == false);

  const DiagnosticsSnapshot snapshot = health.snapshot(100, 1'000);
  CHECK(snapshot.overall == HealthLevel::unknown);
  CHECK(snapshot.entries[static_cast<std::size_t>(Subsystem::network)].code ==
        HealthCode::not_initialized);
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
    run("default configuration is valid", testDefaultConfigurationIsValid);
    run("configuration rejects unsupported schema", testConfigurationRejectsUnsupportedSchema);
    run("configuration reports all invalid fields", testConfigurationReportsAllInvalidFields);
    run("configuration bounds airport filter before validation",
        testConfigurationBoundsAirportFilterBeforeValidation);
    run("Wi-Fi requires provisioned profile", testWifiRequiresProvisionedProfile);
    run("Wi-Fi retries transient failures with backoff", testWifiRetriesTransientFailuresWithBackoff);
    run("Wi-Fi authentication failure requires new provisioning",
        testWifiAuthenticationFailureRequiresNewProvisioning);
    run("Wi-Fi disconnects when disabled or profile removed",
        testWifiDisconnectsWhenDisabledOrProfileRemoved);
    run("Wi-Fi stops after bounded transient retries",
        testWifiStopsAfterBoundedTransientRetries);
    run("Wi-Fi ignores observations from replaced attempt",
        testWifiIgnoresObservationsFromReplacedAttempt);
    run("Wi-Fi jitter is deterministic, distributed, and saturating",
        testWifiJitterIsDeterministicDistributedAndSaturating);
    run("diagnostics aggregate typed health without payloads",
        testDiagnosticsAggregateTypedHealthWithoutPayloads);
    run("diagnostics mark expired reports stale", testDiagnosticsMarkExpiredReportsStale);
    run("diagnostics reject timestamp regression and future reports",
        testDiagnosticsRejectTimestampRegressionAndFutureReports);
    run("diagnostics reject inconsistent severity and code",
        testDiagnosticsRejectInconsistentSeverityAndCode);
    std::cout << tests_run << " tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
