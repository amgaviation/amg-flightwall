#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "amg/flightwall/amg_mission_board_scene.hpp"
#include "amg/flightwall/amg_ops_scene.hpp"
#include "amg/flightwall/application.hpp"
#include "amg/flightwall/clock_scene.hpp"
#include "amg/flightwall/countdown_scene.hpp"
#include "amg/flightwall/data_models.hpp"
#include "amg/flightwall/display.hpp"
#include "amg/flightwall/flight_radar_scene.hpp"
#include "amg/flightwall/hardware_smoke_scene.hpp"
#include "amg/flightwall/message_scene.hpp"
#include "amg/flightwall/metar_scene.hpp"
#include "amg/flightwall/notification_scene.hpp"
#include "amg/flightwall/renderer.hpp"
#include "amg/flightwall/scene_rotator.hpp"
#include "amg/flightwall/scenes.hpp"

namespace {

using amg::flightwall::AmgMetricsSnapshot;
using amg::flightwall::AmgMissionBoardScene;
using amg::flightwall::AmgOpsScene;
using amg::flightwall::Application;
using amg::flightwall::ClassicScene;
using amg::flightwall::ClockScene;
using amg::flightwall::Color;
using amg::flightwall::CountdownScene;
using amg::flightwall::FlightRadarScene;
using amg::flightwall::FlightSnapshot;
using amg::flightwall::FrameBufferDisplay;
using amg::flightwall::FrameContext;
using amg::flightwall::HardwareSmokeScene;
using amg::flightwall::MessageScene;
using amg::flightwall::MetarScene;
using amg::flightwall::MetarSnapshot;
using amg::flightwall::Mode;
using amg::flightwall::NotificationScene;
using amg::flightwall::OperationStatus;
using amg::flightwall::OperationsScene;
using amg::flightwall::Renderer;
using amg::flightwall::SceneManager;
using amg::flightwall::SceneRotator;
using amg::flightwall::SceneSlot;
using amg::flightwall::ServiceState;
using amg::flightwall::SmokeCheckState;

namespace colors = amg::flightwall::colors;

void writePpm(const FrameBufferDisplay& display, const std::filesystem::path& path) {
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    throw std::runtime_error("unable to create " + path.string());
  }
  output << "P6\n" << display.width() << ' ' << display.height() << "\n255\n";
  for (const Color color : display.pixels()) {
    const char channels[] = {static_cast<char>(color.red), static_cast<char>(color.green),
                             static_cast<char>(color.blue)};
    output.write(channels, sizeof(channels));
  }
}

void saveFrame(Application& application, FrameBufferDisplay& display, const Mode mode,
               const std::uint64_t now_ms, const std::filesystem::path& path) {
  application.modes().setRequestedMode(mode);
  application.tick(now_ms);
  writePpm(display, path);
  std::cout << "wrote " << path << " (" << display.litPixelCount() << " lit pixels)\n";
}

// --- Rotator scene fixtures ------------------------------------------------

struct SceneFixtures {
  ClockScene clock;
  FlightRadarScene flights;
  MetarScene metar;
  AmgOpsScene amg_ops;
  AmgMissionBoardScene missions;
  MessageScene message;
  CountdownScene countdown;
  NotificationScene notification;
};

void loadFixtures(SceneFixtures& fixtures) {
  fixtures.clock.setClock({17, 42, 8, 7, 19, 6, true});

  FlightSnapshot flights;
  flights.contacts = {
      {"N721AM", 26.15, -80.25, 12'000, 286, 90, 4.2, true},
      {"DAL1842", 26.40, -80.40, 8'400, 242, 180, 12.9, false},
      {"EJA550", 25.90, -80.00, 19'600, 355, 45, 18.3, false},
  };
  flights.fetched_at_ms = 1'000;
  flights.valid = true;
  fixtures.flights.setOwnPosition(26.07, -80.15);
  fixtures.flights.setRangeNm(30);
  fixtures.flights.setSnapshot(flights);

  MetarSnapshot metar;
  metar.station = "KTEB";
  metar.raw = "KTEB 191751Z 24012KT 10SM FEW060 29/18 A2992";
  metar.flight_category = "VFR";
  metar.wind = "24012KT";
  metar.visibility = "10SM";
  metar.temp_c = 29;
  metar.dewpoint_c = 18;
  metar.fetched_at_ms = 1'000;
  metar.valid = true;
  fixtures.metar.setSnapshot(metar);

  AmgMetricsSnapshot amg;
  amg.new_request_count = 3;
  amg.latest_requests = {{"KTEB-KPBI", "J S", 42}};
  amg.active_mission_count = 2;
  amg.missions = {{"N123AM KTEB-KOPF", "enroute", 95}, {"N88AM KOPF-KTEB", "scheduled", -1}};
  amg.recent_submissions = {{"contact", "M R", 3}};
  amg.submissions_cursor = "cursor-1";
  amg.revenue_today_cents = 452'000;
  amg.revenue_mtd_cents = 1'240'000;
  amg.currency = "usd";
  amg.site_state = "ok";
  amg.fetched_at_ms = 1'000;
  amg.valid = true;
  fixtures.amg_ops.setSnapshot(amg);
  fixtures.missions.setSnapshot(amg);

  fixtures.message.setMessage({"WELCOME TO AMG HANGAR ONE", colors::amg_blue, 0, true});
  fixtures.countdown.setCountdown({"NEXT DEP", 5'025});
  fixtures.notification.setNotification(
      {"ALERT", "NEW CHARTER REQUEST FROM WEB", colors::red, 200, 15'000});
}

std::vector<SceneSlot> fixturePlaylist(SceneFixtures& fixtures) {
  return {{&fixtures.clock, true, 10'000},    {&fixtures.flights, true, 10'000},
          {&fixtures.metar, true, 10'000},    {&fixtures.amg_ops, true, 10'000},
          {&fixtures.missions, true, 10'000}, {&fixtures.countdown, true, 10'000},
          {&fixtures.message, true, 10'000}};
}

// Renders one frame of the requested scene through the rotator (the
// notification scene is overlay-driven by design).
bool renderSceneFrame(SceneRotator& rotator, SceneFixtures& fixtures, const std::string_view id,
                      FrameContext& context) {
  if (id == "notification") {
    return rotator.pushOverlay(fixtures.notification, 60'000, 200) &&
           rotator.tick(context) == &fixtures.notification;
  }
  if (!rotator.activateNow(id, 60'000)) {
    return false;
  }
  return rotator.tick(context) != nullptr;
}

void dumpAscii(const FrameBufferDisplay& display, std::ostream& out) {
  for (int y = 0; y < display.height(); ++y) {
    for (int x = 0; x < display.width(); ++x) {
      const Color color = display.pixel(x, y);
      const int brightness =
          std::max({static_cast<int>(color.red), static_cast<int>(color.green),
                    static_cast<int>(color.blue)});
      out << (brightness == 0 ? ' ' : brightness < 85 ? '.' : brightness < 170 ? '+' : '#');
    }
    out << '\n';
  }
}

constexpr std::string_view scene_id_list =
    "clock flights metar amg_ops missions countdown message notification";

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc >= 2 && std::string_view(argv[1]) == "--scene") {
      if (argc < 3) {
        std::cerr << "usage: --scene <id>  (valid: " << scene_id_list << ")\n";
        return 1;
      }
      const std::string_view scene_id = argv[2];
      FrameBufferDisplay scene_display(128, 64);
      Renderer scene_renderer(scene_display);
      SceneFixtures fixtures;
      loadFixtures(fixtures);
      SceneRotator rotator;
      rotator.setPlaylist(fixturePlaylist(fixtures));
      FrameContext scene_context{scene_renderer, 5'000, 0};
      if (!renderSceneFrame(rotator, fixtures, scene_id, scene_context)) {
        std::cerr << "unknown scene id: " << scene_id << " (valid: " << scene_id_list << ")\n";
        return 1;
      }
      scene_renderer.present();
      dumpAscii(scene_display, std::cout);
      return 0;
    }

    const std::filesystem::path output_directory = argc > 1 ? argv[1] : "build/host/artifacts";
    std::filesystem::create_directories(output_directory);

    // 128x64 is a simulator profile, not yet a verified hardware target.
    FrameBufferDisplay display(128, 64);
    Renderer renderer(display);

    ClassicScene classic;
    classic.setContacts({
        {"N721AM", 18, 24, 12000, 286, true},
        {"DAL1842", 42, 45, 8400, 242, false},
        {"EJA550", 63, 31, 19600, 355, false},
    });

    OperationsScene operations;
    operations.setStatuses({
        {"FLEET", ServiceState::healthy, 82},
        {"CREW", ServiceState::healthy, 67},
        {"WX", ServiceState::degraded, 48},
        {"MAINT", ServiceState::healthy, 24},
    });

    Application application(renderer, classic, operations);
    saveFrame(application, display, Mode::classic, 1'000, output_directory / "classic.ppm");
    saveFrame(application, display, Mode::operations, 2'000, output_directory / "operations.ppm");

    HardwareSmokeScene hardware_smoke(
        {0, SmokeCheckState::pending, SmokeCheckState::pending, SmokeCheckState::pending,
         SmokeCheckState::pending});
    SceneManager smoke_scenes;
    smoke_scenes.activate(hardware_smoke);
    FrameContext smoke_context{renderer, 3'000, 0};
    smoke_scenes.render(smoke_context);
    renderer.present();
    const std::filesystem::path smoke_path = output_directory / "hardware-smoke.ppm";
    writePpm(display, smoke_path);
    std::cout << "wrote " << smoke_path << " (" << display.litPixelCount() << " lit pixels)\n";

    // Rotator-driven dumps of every platform scene with fixture data.
    SceneFixtures fixtures;
    loadFixtures(fixtures);
    SceneRotator rotator;
    rotator.setPlaylist(fixturePlaylist(fixtures));
    FrameContext rotator_context{renderer, 10'000, 0};
    const std::array<std::pair<std::string_view, std::string_view>, 8> scene_files{{
        {"clock", "clock.ppm"},
        {"flights", "flights.ppm"},
        {"metar", "metar.ppm"},
        {"amg_ops", "amg-ops.ppm"},
        {"missions", "missions.ppm"},
        {"countdown", "countdown.ppm"},
        {"message", "message.ppm"},
        {"notification", "notification.ppm"},
    }};
    for (const auto& scene_file : scene_files) {
      rotator_context.monotonic_ms += 1'000;
      ++rotator_context.frame_number;
      if (!renderSceneFrame(rotator, fixtures, scene_file.first, rotator_context)) {
        throw std::runtime_error("failed to render scene " + std::string(scene_file.first));
      }
      renderer.present();
      const std::filesystem::path scene_path = output_directory / scene_file.second;
      writePpm(display, scene_path);
      std::cout << "wrote " << scene_path << " (" << display.litPixelCount()
                << " lit pixels)\n";
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "simulator error: " << error.what() << '\n';
    return 1;
  }
}
