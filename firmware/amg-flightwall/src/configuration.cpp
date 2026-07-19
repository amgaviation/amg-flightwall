#include "amg/flightwall/configuration.hpp"

namespace amg::flightwall {
namespace {

bool isUppercaseAlphanumeric(const char value) noexcept {
  return (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9');
}

bool isModeValid(const Mode mode) noexcept {
  return mode == Mode::classic || mode == Mode::operations || mode == Mode::automatic;
}

bool isAirportFilterValid(const AirportFilter& filter) noexcept {
  if (!filter.inputFits()) {
    return false;
  }
  const std::string_view value = filter.view();
  if (value.empty()) {
    return true;
  }
  if (value.size() < 3 || value.size() > 4) {
    return false;
  }
  for (const char character : value) {
    if (!isUppercaseAlphanumeric(character)) {
      return false;
    }
  }
  return true;
}

bool isWifiProfileReferenceValid(const std::uint32_t key,
                                 const std::uint32_t revision) noexcept {
  return (key == 0 && revision == 0) || (key != 0 && revision != 0);
}

}  // namespace

bool AirportFilter::assign(const std::string_view value) noexcept {
  if (value.size() > characters_.size()) {
    length_ = 0;
    input_fits_ = false;
    return false;
  }

  input_fits_ = true;
  length_ = static_cast<std::uint8_t>(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    characters_[index] = value[index];
  }
  return true;
}

std::string_view AirportFilter::view() const noexcept {
  return {characters_.data(), length_};
}

bool ConfigurationValidation::contains(const ConfigurationError error) const noexcept {
  for (std::size_t index = 0; index < error_count_; ++index) {
    if (errors_[index] == error) {
      return true;
    }
  }
  return false;
}

void ConfigurationValidation::add(const ConfigurationError error) noexcept {
  if (error_count_ < errors_.size()) {
    errors_[error_count_++] = error;
  }
}

ConfigurationValidation validateConfiguration(const DeviceConfiguration& configuration) noexcept {
  ConfigurationValidation validation;
  if (configuration.schema_version != DeviceConfiguration::current_schema_version) {
    validation.add(ConfigurationError::unsupported_schema);
  }
  if (!isModeValid(configuration.startup_mode)) {
    validation.add(ConfigurationError::invalid_startup_mode);
  }
  if (configuration.brightness > 255) {
    validation.add(ConfigurationError::brightness_out_of_range);
  }
  if (!isAirportFilterValid(configuration.airport_filter)) {
    validation.add(ConfigurationError::invalid_airport_filter);
  }
  if (!isWifiProfileReferenceValid(configuration.wifi_profile_key,
                                   configuration.wifi_profile_revision)) {
    validation.add(ConfigurationError::invalid_wifi_profile_reference);
  }
  return validation;
}

}  // namespace amg::flightwall
