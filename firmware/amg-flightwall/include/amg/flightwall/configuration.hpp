#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "amg/flightwall/mode.hpp"

namespace amg::flightwall {

enum class ConfigurationError {
  unsupported_schema,
  invalid_startup_mode,
  brightness_out_of_range,
  invalid_airport_filter,
  invalid_wifi_profile_reference,
};

class AirportFilter {
 public:
  static constexpr std::size_t capacity = 4;

  [[nodiscard]] bool assign(std::string_view value) noexcept;
  [[nodiscard]] std::string_view view() const noexcept;
  [[nodiscard]] bool empty() const noexcept { return length_ == 0; }
  [[nodiscard]] bool inputFits() const noexcept { return input_fits_; }

 private:
  std::array<char, capacity> characters_{};
  std::uint8_t length_{0};
  bool input_fits_{true};
};

struct DeviceConfiguration {
  static constexpr std::uint16_t current_schema_version = 1;

  std::uint16_t schema_version{current_schema_version};
  Mode startup_mode{Mode::classic};
  std::uint16_t brightness{64};
  AirportFilter airport_filter{};
  std::uint32_t wifi_profile_key{0};
  std::uint32_t wifi_profile_revision{0};
  bool diagnostics_enabled{true};
};

class ConfigurationValidation {
 public:
  [[nodiscard]] bool valid() const noexcept { return error_count_ == 0; }
  [[nodiscard]] std::size_t errorCount() const noexcept { return error_count_; }
  [[nodiscard]] bool contains(ConfigurationError error) const noexcept;

 private:
  void add(ConfigurationError error) noexcept;

  std::array<ConfigurationError, 5> errors_{};
  std::size_t error_count_{0};

  friend ConfigurationValidation validateConfiguration(
      const DeviceConfiguration& configuration) noexcept;
};

[[nodiscard]] ConfigurationValidation validateConfiguration(
    const DeviceConfiguration& configuration) noexcept;

}  // namespace amg::flightwall
