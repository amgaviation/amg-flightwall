#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "amg/flightwall/application.hpp"
#include "amg/flightwall/display.hpp"
#include "amg/flightwall/renderer.hpp"
#include "amg/flightwall/scenes.hpp"

namespace {

using amg::flightwall::Application;
using amg::flightwall::ClassicScene;
using amg::flightwall::Color;
using amg::flightwall::FrameBufferDisplay;
using amg::flightwall::Mode;
using amg::flightwall::OperationStatus;
using amg::flightwall::OperationsScene;
using amg::flightwall::Renderer;
using amg::flightwall::ServiceState;

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

}  // namespace

int main(int argc, char** argv) {
  try {
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
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "simulator error: " << error.what() << '\n';
    return 1;
  }
}
