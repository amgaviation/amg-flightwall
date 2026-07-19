#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

struct AircraftContact {
  std::string callsign;
  int map_x{0};
  int map_y{0};
  std::uint32_t altitude_ft{0};
  std::uint16_t ground_speed_kt{0};
  bool watchlisted{false};
};

class ClassicScene final : public Scene {
 public:
  void setContacts(std::vector<AircraftContact> contacts);
  [[nodiscard]] std::string_view id() const noexcept override;
  void render(FrameContext& context) noexcept override;

 private:
  std::vector<AircraftContact> contacts_{};
};

enum class ServiceState { healthy, degraded, offline };

struct OperationStatus {
  std::string label;
  ServiceState state{ServiceState::offline};
  std::uint8_t load_percent{0};
};

class OperationsScene final : public Scene {
 public:
  void setStatuses(std::vector<OperationStatus> statuses);
  [[nodiscard]] std::string_view id() const noexcept override;
  void render(FrameContext& context) noexcept override;

 private:
  std::vector<OperationStatus> statuses_{};
};

}  // namespace amg::flightwall
