#include "amg/flightwall/plugin.hpp"

#include <algorithm>

namespace amg::flightwall {

PluginRegistrationResult PluginRegistry::registerPlugin(Plugin& plugin) {
  const PluginManifest& manifest = plugin.manifest();
  if (manifest.id.empty() || manifest.version.empty() || manifest.memory_budget_bytes == 0) {
    return PluginRegistrationResult::invalid_manifest;
  }
  if (manifest.core_api_major != core_api_major) {
    return PluginRegistrationResult::incompatible_core_api;
  }
  if (find(manifest.id) != nullptr) {
    return PluginRegistrationResult::duplicate_id;
  }
  plugins_.push_back(&plugin);
  return PluginRegistrationResult::registered;
}

Plugin* PluginRegistry::find(const std::string_view id) const noexcept {
  const auto match = std::find_if(plugins_.begin(), plugins_.end(), [id](const Plugin* plugin) {
    return plugin->manifest().id == id;
  });
  return match == plugins_.end() ? nullptr : *match;
}

std::size_t PluginRegistry::size() const noexcept { return plugins_.size(); }

}  // namespace amg::flightwall
