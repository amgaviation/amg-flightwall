#include "amg/flightwall/scenes.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <string_view>
#include <utility>

namespace amg::flightwall {
namespace {

Color statusColor(const ServiceState state) noexcept {
  switch (state) {
    case ServiceState::healthy: return colors::green;
    case ServiceState::degraded: return colors::amber;
    case ServiceState::offline: return colors::red;
  }
  return colors::red;
}

template <typename Value>
std::string_view numberText(const Value value, std::array<char, 16>& buffer) noexcept {
  const auto conversion = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  return conversion.ec == std::errc{}
             ? std::string_view(buffer.data(), static_cast<std::size_t>(conversion.ptr - buffer.data()))
             : std::string_view{"ERR"};
}

}  // namespace

void ClassicScene::setContacts(std::vector<AircraftContact> contacts) { contacts_ = std::move(contacts); }

std::string_view ClassicScene::id() const noexcept { return "classic"; }

void ClassicScene::render(FrameContext& context) noexcept {
  Renderer& renderer = context.renderer;
  renderer.clear(colors::black);
  renderer.text(2, 2, "AMG FLIGHTWALL", colors::amg_blue);
  renderer.line(1, 10, renderer.width() - 2, 10, colors::muted);

  const int details_x = std::max(76, renderer.width() * 2 / 3);
  renderer.rectangle(1, 12, details_x - 3, renderer.height() - 14, colors::muted);
  renderer.line(details_x - 1, 12, details_x - 1, renderer.height() - 2, colors::muted);

  for (const auto& contact : contacts_) {
    const int x = std::clamp(contact.map_x, 3, details_x - 5);
    const int y = std::clamp(contact.map_y, 15, renderer.height() - 4);
    const Color color = contact.watchlisted ? colors::red : colors::cyan;
    renderer.line(x - 2, y, x + 2, y, color);
    renderer.line(x, y - 2, x, y + 2, color);
    renderer.pixel(x, y, colors::white);
  }

  if (contacts_.empty()) {
    renderer.text(8, renderer.height() / 2, "NO TRAFFIC", colors::muted);
  } else {
    const AircraftContact& selected = contacts_.front();
    renderer.text(details_x + 2, 14, std::string_view(selected.callsign).substr(0, 7),
                  selected.watchlisted ? colors::red : colors::white);
    std::array<char, 16> number_buffer{};
    renderer.text(details_x + 2, 24, "ALT", colors::muted);
    renderer.text(details_x + 2, 32, numberText(selected.altitude_ft, number_buffer), colors::white);
    renderer.text(details_x + 2, 42, "SPD", colors::muted);
    renderer.text(details_x + 2, 50, numberText(selected.ground_speed_kt, number_buffer), colors::white);
  }
}

void OperationsScene::setStatuses(std::vector<OperationStatus> statuses) { statuses_ = std::move(statuses); }

std::string_view OperationsScene::id() const noexcept { return "operations"; }

void OperationsScene::render(FrameContext& context) noexcept {
  Renderer& renderer = context.renderer;
  renderer.clear(colors::black);
  renderer.text(2, 2, "AMG OPERATIONS", colors::amg_blue);
  renderer.line(1, 10, renderer.width() - 2, 10, colors::muted);

  const int label_width = std::min(42, renderer.width() / 3);
  const int bar_width = std::max(8, renderer.width() - label_width - 12);
  int y = 15;
  for (const auto& status : statuses_) {
    if (y + 7 >= renderer.height()) {
      break;
    }
    const Color color = statusColor(status.state);
    renderer.text(2, y, std::string_view(status.label).substr(0, 6), colors::white);
    renderer.rectangle(label_width, y, bar_width, 7, colors::muted);
    const int filled = (bar_width - 2) * std::min<int>(status.load_percent, 100) / 100;
    renderer.fillRectangle(label_width + 1, y + 1, filled, 5, color);
    renderer.fillRectangle(renderer.width() - 7, y + 1, 5, 5, color);
    y += 10;
  }

  if (statuses_.empty()) {
    renderer.text(8, renderer.height() / 2, "NO STATUS", colors::muted);
  }
}

}  // namespace amg::flightwall
