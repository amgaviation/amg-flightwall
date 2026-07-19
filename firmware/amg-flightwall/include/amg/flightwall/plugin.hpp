#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace amg::flightwall {

struct PluginManifest {
  std::string_view id;
  std::string_view version;
  std::uint16_t core_api_major{1};
  std::uint32_t memory_budget_bytes{0};
};

class Plugin {
 public:
  virtual ~Plugin() = default;

  [[nodiscard]] virtual const PluginManifest& manifest() const noexcept = 0;
  virtual bool initialize() noexcept = 0;
  virtual void activate() noexcept = 0;
  virtual void deactivate() noexcept = 0;
  virtual void stop() noexcept = 0;
};

enum class PluginRegistrationResult { registered, invalid_manifest, duplicate_id, incompatible_core_api };

class PluginRegistry {
 public:
  static constexpr std::uint16_t core_api_major = 1;

  [[nodiscard]] PluginRegistrationResult registerPlugin(Plugin& plugin);
  [[nodiscard]] Plugin* find(std::string_view id) const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;

 private:
  std::vector<Plugin*> plugins_{};
};

}  // namespace amg::flightwall
