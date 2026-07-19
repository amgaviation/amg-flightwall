#include "amg/flightwall/hardware_smoke_scene.hpp"

#include <array>
#include <charconv>
#include <string_view>

namespace amg::flightwall {
namespace {

Color stateColor(const SmokeCheckState state, const bool pulse_on) noexcept {
  switch (state) {
    case SmokeCheckState::pending:
      return colors::muted;
    case SmokeCheckState::running:
      return pulse_on ? colors::amber : colors::muted;
    case SmokeCheckState::passed:
      return colors::green;
    case SmokeCheckState::failed:
      return colors::red;
  }
  return colors::red;
}

std::string_view stateText(const SmokeCheckState state) noexcept {
  switch (state) {
    case SmokeCheckState::pending:
      return "WAIT";
    case SmokeCheckState::running:
      return "RUN";
    case SmokeCheckState::passed:
      return "PASS";
    case SmokeCheckState::failed:
      return "FAIL";
  }
  return "FAIL";
}

void statusRow(Renderer& renderer, const int y, const std::string_view label,
               const SmokeCheckState state, const bool pulse_on) noexcept {
  renderer.text(2, y, label, colors::white);
  renderer.text(80, y, stateText(state), stateColor(state, pulse_on));
}

}  // namespace

HardwareSmokeScene::HardwareSmokeScene(const HardwareSmokeProgress progress) noexcept
    : progress_(progress) {}

void HardwareSmokeScene::setProgress(const HardwareSmokeProgress progress) noexcept {
  progress_ = progress;
}

std::string_view HardwareSmokeScene::id() const noexcept { return "hardware-smoke"; }

void HardwareSmokeScene::render(FrameContext& context) noexcept {
  Renderer& renderer = context.renderer;
  renderer.clear(colors::black);
  renderer.text(2, 2, "AMG HW TEST", colors::amg_blue);
  renderer.line(1, 10, renderer.width() - 2, 10, colors::muted);
  const bool pulse_on = (context.monotonic_ms / 500) % 2 == 0;

  renderer.text(2, 13, "CORE", colors::white);
  std::array<char, 8> test_count{};
  const auto conversion = std::to_chars(test_count.data(), test_count.data() + test_count.size(),
                                        progress_.host_tests_passed);
  const std::string_view count =
      conversion.ec == std::errc{}
          ? std::string_view(test_count.data(),
                             static_cast<std::size_t>(conversion.ptr - test_count.data()))
          : std::string_view{"ERR"};
  renderer.text(38, 13, count,
                progress_.host_tests_passed > 0 ? colors::green : colors::muted);
  renderer.text(80, 13, progress_.host_tests_passed > 0 ? "PASS" : "WAIT",
                progress_.host_tests_passed > 0 ? colors::green : colors::muted);

  statusRow(renderer, 23, "BUILD", progress_.target_build, pulse_on);
  statusRow(renderer, 33, "HASH", progress_.backup_hash, pulse_on);
  statusRow(renderer, 43, "HW TEST", progress_.hardware_test, pulse_on);
  statusRow(renderer, 53, "SRC GATE", progress_.source_write_gate, pulse_on);
}

}  // namespace amg::flightwall
