#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "amg/flightwall/amg_mission_board_scene.hpp"
#include "amg/flightwall/amg_ops_scene.hpp"
#include "amg/flightwall/application.hpp"
#include "amg/flightwall/clock_scene.hpp"
#include "amg/flightwall/configuration.hpp"
#include "amg/flightwall/countdown_scene.hpp"
#include "amg/flightwall/data_models.hpp"
#include "amg/flightwall/diagnostics.hpp"
#include "amg/flightwall/display.hpp"
#include "amg/flightwall/flight_radar_scene.hpp"
#include "amg/flightwall/hardware_smoke_scene.hpp"
#include "amg/flightwall/message_scene.hpp"
#include "amg/flightwall/metar_scene.hpp"
#include "amg/flightwall/mode.hpp"
#include "amg/flightwall/notification_scene.hpp"
#include "amg/flightwall/plugin.hpp"
#include "amg/flightwall/renderer.hpp"
#include "amg/flightwall/scene.hpp"
#include "amg/flightwall/scene_rotator.hpp"
#include "amg/flightwall/scene_support.hpp"
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

void testHardwareSmokeSceneCommunicatesVerifiedProgress() {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  HardwareSmokeScene scene({23, SmokeCheckState::passed, SmokeCheckState::passed,
                            SmokeCheckState::running, SmokeCheckState::passed});
  SceneManager manager;
  FrameContext context{renderer, 1'000, 0};

  manager.activate(scene);
  manager.render(context);

  std::size_t green_pixels = 0;
  std::size_t amber_pixels = 0;
  std::size_t red_pixels = 0;
  for (const Color pixel : display.pixels()) {
    green_pixels += pixel == colors::green ? 1 : 0;
    amber_pixels += pixel == colors::amber ? 1 : 0;
    red_pixels += pixel == colors::red ? 1 : 0;
  }

  CHECK(scene.id() == "hardware-smoke");
  CHECK(display.litPixelCount() > 500);
  CHECK(green_pixels > 0);
  CHECK(amber_pixels > 0);
  CHECK(red_pixels == 0);

  context.monotonic_ms = 1'500;
  manager.render(context);
  amber_pixels = 0;
  for (const Color pixel : display.pixels()) {
    amber_pixels += pixel == colors::amber ? 1 : 0;
  }
  CHECK(amber_pixels == 0);
}

std::uint64_t framebufferHash(const FrameBufferDisplay& display) {
  constexpr std::uint64_t offset_basis = 14'695'981'039'346'656'037ULL;
  constexpr std::uint64_t prime = 1'099'511'628'211ULL;
  std::uint64_t hash = offset_basis;
  for (const Color pixel : display.pixels()) {
    for (const std::uint8_t channel : {pixel.red, pixel.green, pixel.blue}) {
      hash ^= channel;
      hash *= prime;
    }
  }
  return hash;
}

void testHardwareSmokeSceneFramebufferRegression() {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  HardwareSmokeScene scene;
  SceneManager manager;
  FrameContext context{renderer, 1'000, 0};

  manager.activate(scene);
  manager.render(context);
  CHECK(framebufferHash(display) == 12'401'996'579'662'572'810ULL);

  scene.setProgress({0, SmokeCheckState::failed, SmokeCheckState::failed,
                     SmokeCheckState::failed, SmokeCheckState::failed});
  manager.render(context);
  CHECK(framebufferHash(display) == 5'436'806'316'733'823'986ULL);
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

std::size_t countColor(const FrameBufferDisplay& display, const Color color) {
  std::size_t count = 0;
  for (const Color pixel : display.pixels()) {
    count += pixel == color ? 1 : 0;
  }
  return count;
}

std::uint64_t renderSceneHash(Scene& scene, const std::uint64_t now_ms) {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  FrameContext context{renderer, now_ms, 0};
  scene.render(context);
  return framebufferHash(display);
}

FlightSnapshot flightFixture() {
  FlightSnapshot snapshot;
  snapshot.contacts = {
      {"N721AM", 26.15, -80.25, 12'000, 286, 90, 4.2, true},
      {"DAL1842", 26.40, -80.40, 8'400, 242, 180, 12.9, false},
      {"EJA550", 25.90, -80.00, 19'600, 355, 45, 18.3, false},
  };
  snapshot.fetched_at_ms = 1'000;
  snapshot.valid = true;
  return snapshot;
}

MetarSnapshot metarFixture() {
  MetarSnapshot snapshot;
  snapshot.station = "KTEB";
  snapshot.raw = "KTEB 191751Z 24012KT 10SM FEW060 29/18 A2992";
  snapshot.flight_category = "VFR";
  snapshot.wind = "24012KT";
  snapshot.visibility = "10SM";
  snapshot.temp_c = 29;
  snapshot.dewpoint_c = 18;
  snapshot.fetched_at_ms = 1'000;
  snapshot.valid = true;
  return snapshot;
}

AmgMetricsSnapshot amgFixture() {
  AmgMetricsSnapshot snapshot;
  snapshot.new_request_count = 3;
  snapshot.latest_requests = {{"KTEB-KPBI", "J S", 42}};
  snapshot.active_mission_count = 2;
  snapshot.missions = {{"N123AM KTEB-KOPF", "enroute", 95},
                       {"N88AM KOPF-KTEB", "scheduled", -1}};
  snapshot.recent_submissions = {{"contact", "M R", 3}};
  snapshot.submissions_cursor = "cursor-1";
  snapshot.revenue_today_cents = 452'000;
  snapshot.revenue_mtd_cents = 1'240'000;
  snapshot.currency = "usd";
  snapshot.site_state = "ok";
  snapshot.fetched_at_ms = 1'000;
  snapshot.valid = true;
  return snapshot;
}

void testSceneRotatorRotatesAndSkipsDisabled() {
  CountingScene first("first");
  CountingScene second("second");
  CountingScene third("third");
  SceneRotator rotator;
  rotator.setPlaylist({{&first, true, 100}, {&second, false, 100}, {&third, true, 200}});
  FrameBufferDisplay display(8, 8);
  Renderer renderer(display);
  FrameContext context{renderer, 0, 0};

  CHECK(rotator.tick(context) == &first);
  CHECK(first.enters == 1);
  CHECK(rotator.activeSceneId() == "first");
  context.monotonic_ms = 99;
  CHECK(rotator.tick(context) == &first);
  context.monotonic_ms = 100;
  CHECK(rotator.tick(context) == &third);
  CHECK(first.exits == 1);
  CHECK(third.enters == 1);
  context.monotonic_ms = 299;
  CHECK(rotator.tick(context) == &third);
  context.monotonic_ms = 300;
  CHECK(rotator.tick(context) == &first);
  CHECK(second.renders == 0);
  CHECK(second.enters == 0);
}

void testSceneRotatorSingleSlotStays() {
  CountingScene only("only");
  SceneRotator rotator;
  rotator.setPlaylist({{&only, true, 50}});
  FrameBufferDisplay display(8, 8);
  Renderer renderer(display);
  FrameContext context{renderer, 0, 0};

  CHECK(rotator.tick(context) == &only);
  context.monotonic_ms = 1'000;
  CHECK(rotator.tick(context) == &only);
  context.monotonic_ms = 5'000;
  CHECK(rotator.tick(context) == &only);
  CHECK(only.enters == 1);
  CHECK(only.exits == 0);
  CHECK(only.renders == 3);
}

void testSceneRotatorOverlayPreemptionAndExpiry() {
  CountingScene base("base");
  CountingScene next("next");
  CountingScene note("note");
  CountingScene alert("alert");
  SceneRotator rotator;
  rotator.setPlaylist({{&base, true, 1'000}, {&next, true, 500}});
  FrameBufferDisplay display(8, 8);
  Renderer renderer(display);
  FrameContext context{renderer, 0, 0};

  CHECK(rotator.tick(context) == &base);
  CHECK(rotator.pushOverlay(note, 200, 1));
  context.monotonic_ms = 10;
  CHECK(rotator.tick(context) == &note);
  CHECK(base.exits == 1);
  CHECK(rotator.pushOverlay(alert, 100, 5));
  context.monotonic_ms = 20;
  CHECK(rotator.tick(context) == &alert);  // higher priority preempts
  CHECK(note.exits == 1);
  context.monotonic_ms = 119;
  CHECK(rotator.tick(context) == &alert);
  context.monotonic_ms = 120;
  CHECK(rotator.tick(context) == &note);  // queued overlay restarts fresh
  context.monotonic_ms = 319;
  CHECK(rotator.tick(context) == &note);
  context.monotonic_ms = 320;
  CHECK(rotator.tick(context) == &base);  // playlist resumes, slot timer reset
  CHECK(rotator.overlayCount() == 0);
  CHECK(base.enters == 2);
  CHECK(note.enters == 2);
  CHECK(alert.enters == 1);
  context.monotonic_ms = 1'319;
  CHECK(rotator.tick(context) == &base);  // full duration after the overlay
  context.monotonic_ms = 1'320;
  CHECK(rotator.tick(context) == &next);
}

void testSceneRotatorActivateNow() {
  CountingScene first("first");
  CountingScene second("second");
  CountingScene third("third");
  SceneRotator rotator;
  rotator.setPlaylist({{&first, true, 100}, {&second, true, 100}, {&third, false, 100}});
  FrameBufferDisplay display(8, 8);
  Renderer renderer(display);
  FrameContext context{renderer, 0, 0};

  CHECK(rotator.tick(context) == &first);
  CHECK(rotator.activateNow("third", 300));  // disabled slots can be forced
  context.monotonic_ms = 10;
  CHECK(rotator.tick(context) == &third);
  context.monotonic_ms = 309;
  CHECK(rotator.tick(context) == &third);
  context.monotonic_ms = 310;
  CHECK(rotator.tick(context) == &first);  // advances past the disabled slot
  CHECK(rotator.activateNow("unknown", 100) == false);
}

void testSceneRotatorOverlayQueueIsBounded() {
  CountingScene base("base");
  CountingScene overlay("overlay");
  SceneRotator rotator;
  rotator.setPlaylist({{&base, true, 1'000}});
  for (std::size_t index = 0; index < SceneRotator::max_overlays; ++index) {
    CHECK(rotator.pushOverlay(overlay, 1'000, 0));
  }
  CHECK(rotator.pushOverlay(overlay, 1'000, 200) == false);
  CHECK(rotator.overlayCount() == SceneRotator::max_overlays);
}

void testClockSceneGoldenAndStale() {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  ClockScene scene;
  scene.setClock({7, 45, 30, 7, 19, 6, true});
  FrameContext context{renderer, 1'000, 0};
  scene.render(context);
  CHECK(display.litPixelCount() > 0);
  const std::uint64_t valid_hash = framebufferHash(display);
  CHECK(valid_hash == 5'447'952'058'334'295'885ULL);

  ClockScene fresh;
  ClockScene invalid;
  invalid.setClock({99, 99, 99, 99, 99, 99, false});
  const std::uint64_t stale_marker = renderSceneHash(fresh, 1'000);
  CHECK(renderSceneHash(invalid, 1'000) == stale_marker);
  CHECK(stale_marker != valid_hash);
}

void testFlightRadarSceneContactsAndStale() {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  FlightRadarScene scene;
  scene.setOwnPosition(26.07, -80.15);
  scene.setRangeNm(30);
  scene.setSnapshot(flightFixture());
  FrameContext context{renderer, 1'000, 0};
  scene.render(context);
  CHECK(display.litPixelCount() > 0);
  CHECK(countColor(display, colors::amber) > 0);  // watchlisted contact
  CHECK(countColor(display, colors::cyan) > 0);   // other contacts
  const std::uint64_t valid_hash = framebufferHash(display);

  FlightRadarScene fresh;
  FlightRadarScene invalid;
  invalid.setOwnPosition(26.07, -80.15);
  FlightSnapshot garbage = flightFixture();
  garbage.valid = false;
  invalid.setSnapshot(garbage);
  const std::uint64_t stale_marker = renderSceneHash(fresh, 1'000);
  CHECK(renderSceneHash(invalid, 1'000) == stale_marker);
  CHECK(stale_marker != valid_hash);

  FlightRadarScene empty;
  FlightSnapshot no_traffic;
  no_traffic.valid = true;
  empty.setSnapshot(no_traffic);
  CHECK(renderSceneHash(empty, 1'000) != stale_marker);
}

void testMetarSceneCategoryColorsAndStale() {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  MetarScene scene;
  scene.setSnapshot(metarFixture());
  FrameContext context{renderer, 1'000, 0};
  scene.render(context);
  CHECK(display.litPixelCount() > 0);
  CHECK(countColor(display, colors::green) > 0);  // VFR category

  MetarSnapshot lifr = metarFixture();
  lifr.flight_category = "LIFR";
  scene.setSnapshot(lifr);
  scene.render(context);
  CHECK(countColor(display, scene_support::magenta) > 0);
  CHECK(countColor(display, colors::green) == 0);

  MetarScene fresh;
  MetarScene invalid;
  MetarSnapshot garbage = metarFixture();
  garbage.station.clear();
  garbage.flight_category.clear();
  garbage.valid = false;
  invalid.setSnapshot(garbage);
  const std::uint64_t stale_marker = renderSceneHash(fresh, 1'000);
  CHECK(renderSceneHash(invalid, 1'000) == stale_marker);
}

void testAmgOpsSceneCountersAndStale() {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  AmgOpsScene scene;
  scene.setSnapshot(amgFixture());
  FrameContext context{renderer, 1'000, 0};
  scene.render(context);
  CHECK(display.litPixelCount() > 0);
  CHECK(countColor(display, colors::amber) > 0);  // NEW REQ 3 highlighted

  AmgMetricsSnapshot quiet = amgFixture();
  quiet.new_request_count = 0;
  scene.setSnapshot(quiet);
  scene.render(context);
  CHECK(countColor(display, colors::amber) == 0);

  AmgOpsScene fresh;
  AmgOpsScene invalid;
  AmgMetricsSnapshot garbage = amgFixture();
  garbage.site_state.clear();
  garbage.valid = false;
  invalid.setSnapshot(garbage);
  const std::uint64_t stale_marker = renderSceneHash(fresh, 1'000);
  CHECK(renderSceneHash(invalid, 1'000) == stale_marker);
}

void testAmgMissionBoardSceneRowsAndStale() {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  AmgMissionBoardScene scene;
  scene.setSnapshot(amgFixture());
  FrameContext context{renderer, 1'000, 0};
  scene.render(context);
  CHECK(display.litPixelCount() > 0);
  CHECK(countColor(display, colors::green) > 0);  // enroute chip
  CHECK(countColor(display, colors::cyan) > 0);   // scheduled chip

  AmgMissionBoardScene empty;
  AmgMetricsSnapshot no_missions = amgFixture();
  no_missions.missions.clear();
  empty.setSnapshot(no_missions);
  CHECK(renderSceneHash(empty, 1'000) != 0);

  AmgMissionBoardScene fresh;
  AmgMissionBoardScene invalid;
  AmgMetricsSnapshot garbage = amgFixture();
  garbage.valid = false;
  invalid.setSnapshot(garbage);
  const std::uint64_t stale_marker = renderSceneHash(fresh, 1'000);
  CHECK(renderSceneHash(invalid, 1'000) == stale_marker);
}

void testMessageSceneScrollAndStale() {
  MessageScene scene;
  scene.setMessage({"CHARTER READY", colors::amber, 0, true});
  // Cycle: 13 chars * 12 px + 32 px gap = 188 px at 20 px/s -> 9400 ms period.
  const std::uint64_t scroll_start = renderSceneHash(scene, 1'000);
  CHECK(scroll_start == renderSceneHash(scene, 1'000 + 9'400));
  CHECK(scroll_start != renderSceneHash(scene, 1'000 + 4'700));

  MessageScene wrapped;
  wrapped.setMessage({"CREW BRIEF AT HANGAR TWO AT EIGHTEEN HUNDRED", colors::white, 0, false});
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  FrameContext context{renderer, 1'000, 0};
  wrapped.render(context);
  CHECK(display.litPixelCount() > 0);

  MessageScene fresh;
  MessageScene empty;
  empty.setMessage({"", colors::white, 0, true});
  const std::uint64_t stale_marker = renderSceneHash(fresh, 1'000);
  CHECK(renderSceneHash(empty, 1'000) == stale_marker);
  CHECK(stale_marker != 0);
}

void testCountdownSceneTicksAndStale() {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  CountdownScene scene;
  scene.setCountdown({"NEXT DEP", 90'061});  // 1 day 01:01:01
  FrameContext context{renderer, 5'000, 0};
  scene.render(context);
  CHECK(display.litPixelCount() > 0);
  const std::uint64_t first_hash = framebufferHash(display);
  CHECK(first_hash == 17'494'560'472'044'079'601ULL);

  context.monotonic_ms = 6'000;  // one second later the display ticks down
  scene.render(context);
  CHECK(framebufferHash(display) != first_hash);

  CountdownScene fresh;
  CountdownScene invalid;
  invalid.setCountdown({"", -5});
  const std::uint64_t stale_marker = renderSceneHash(fresh, 5'000);
  CHECK(renderSceneHash(invalid, 5'000) == stale_marker);
  CHECK(stale_marker != first_hash);
}

void testNotificationSceneBannerAndFallback() {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  NotificationScene scene;
  scene.setNotification({"ALERT", "NEW CHARTER REQUEST FROM WEB", colors::red, 200, 15'000});
  FrameContext context{renderer, 1'000, 0};
  scene.render(context);
  CHECK(display.litPixelCount() > 0);
  CHECK(display.pixel(0, 0) == colors::red);
  CHECK(display.pixel(127, 63) == colors::red);
  // Body cycle: 28 chars * 6 px + 24 px gap = 192 px at 20 px/s -> 9600 ms.
  const std::uint64_t first_hash = framebufferHash(display);
  CHECK(renderSceneHash(scene, 1'000 + 9'600) == first_hash);
  CHECK(renderSceneHash(scene, 1'000 + 4'800) != first_hash);

  NotificationScene fallback;  // no payload: default border + placeholder title
  FrameBufferDisplay fallback_display(128, 64);
  Renderer fallback_renderer(fallback_display);
  FrameContext fallback_context{fallback_renderer, 1'000, 0};
  fallback.render(fallback_context);
  CHECK(fallback_display.litPixelCount() > 0);
  CHECK(fallback_display.pixel(0, 0) == colors::amg_blue);
}

void testNotificationOverlayDrivesRotator() {
  FrameBufferDisplay display(128, 64);
  Renderer renderer(display);
  ClockScene clock;
  clock.setClock({7, 45, 30, 7, 19, 6, true});
  NotificationScene notification;
  notification.setNotification({"ALERT", "NEW REQUEST", colors::red, 200, 5'000});
  SceneRotator rotator;
  rotator.setPlaylist({{&clock, true, 10'000}});
  FrameContext context{renderer, 0, 0};

  CHECK(rotator.tick(context) == &clock);
  CHECK(rotator.pushOverlay(notification, 5'000, 200));
  context.monotonic_ms = 100;
  CHECK(rotator.tick(context) == &notification);
  CHECK(display.pixel(0, 0) == colors::red);
  context.monotonic_ms = 5'100;
  CHECK(rotator.tick(context) == &clock);
  CHECK(display.pixel(0, 0) == colors::black);
  CHECK(rotator.overlayCount() == 0);
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
    run("hardware smoke scene communicates verified progress",
        testHardwareSmokeSceneCommunicatesVerifiedProgress);
    run("hardware smoke scene framebuffer regression",
        testHardwareSmokeSceneFramebufferRegression);
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
    run("scene rotator rotates and skips disabled", testSceneRotatorRotatesAndSkipsDisabled);
    run("scene rotator single slot stays", testSceneRotatorSingleSlotStays);
    run("scene rotator overlay preemption and expiry",
        testSceneRotatorOverlayPreemptionAndExpiry);
    run("scene rotator activate now", testSceneRotatorActivateNow);
    run("scene rotator overlay queue is bounded", testSceneRotatorOverlayQueueIsBounded);
    run("clock scene golden and stale", testClockSceneGoldenAndStale);
    run("flight radar scene contacts and stale", testFlightRadarSceneContactsAndStale);
    run("metar scene category colors and stale", testMetarSceneCategoryColorsAndStale);
    run("amg ops scene counters and stale", testAmgOpsSceneCountersAndStale);
    run("amg mission board scene rows and stale", testAmgMissionBoardSceneRowsAndStale);
    run("message scene scroll and stale", testMessageSceneScrollAndStale);
    run("countdown scene ticks and stale", testCountdownSceneTicksAndStale);
    run("notification scene banner and fallback", testNotificationSceneBannerAndFallback);
    run("notification overlay drives rotator", testNotificationOverlayDrivesRotator);
    std::cout << tests_run << " tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
