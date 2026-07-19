#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

// One playlist entry. `duration_ms == 0` advances on the next tick.
struct SceneSlot {
  Scene* scene{nullptr};
  bool enabled{true};
  std::uint32_t duration_ms{10'000};
};

// Deterministic playlist rotator with an overlay stack. Time is injected via
// FrameContext::monotonic_ms only, so behaviour is fully host-testable. The
// rotator renders (never presents); callers own the present() cadence.
//
// Overlays: the highest-priority entry renders instead of the playlist and
// expires after its duration of *display* time. Lower-priority entries queue
// and start (or restart, when preempted) fresh once they reach the top. When
// the last overlay expires the interrupted playlist slot restarts with its
// full duration.
class SceneRotator {
 public:
  static constexpr std::size_t max_overlays = 8;

  // Replaces the playlist and restarts rotation at the first usable slot.
  // Pending forced activations are dropped; queued overlays are kept.
  void setPlaylist(std::vector<SceneSlot> slots);
  [[nodiscard]] const std::vector<SceneSlot>& playlist() const noexcept;

  // Forces the playlist slot whose scene id matches to become active on the
  // next tick for `duration_ms` (0 = the slot's configured duration), even if
  // the slot is disabled. Returns false when no slot has that id.
  bool activateNow(std::string_view id, std::uint32_t duration_ms) noexcept;

  // Queues an overlay; returns false when the bounded queue is full. The
  // caller keeps ownership of the scene and must keep it alive until expiry.
  bool pushOverlay(Scene& scene, std::uint32_t duration_ms, std::uint8_t priority) noexcept;
  [[nodiscard]] std::size_t overlayCount() const noexcept;

  // Renders the active overlay or playlist slot; returns the scene rendered
  // (nullptr when nothing is usable). Advances slots as durations elapse and
  // fires onEnter/onExit on every active-scene change.
  Scene* tick(FrameContext& context) noexcept;

  // Id of the scene rendered by the most recent tick ("" when none).
  [[nodiscard]] std::string_view activeSceneId() const noexcept;

 private:
  struct OverlayEntry {
    Scene* scene{nullptr};
    std::uint32_t duration_ms{0};
    std::uint8_t priority{0};
    std::uint64_t sequence{0};
    std::uint64_t started_ms{0};
    bool started{false};
  };

  [[nodiscard]] int topOverlayIndex() const noexcept;
  void removeOverlay(std::size_t index) noexcept;
  [[nodiscard]] bool advanceToNextEnabled() noexcept;
  Scene* selectPlaylistScene(std::uint64_t now_ms) noexcept;
  void transitionTo(Scene* scene) noexcept;

  std::vector<SceneSlot> slots_{};
  std::size_t active_index_{0};
  std::uint64_t slot_started_ms_{0};
  bool slot_needs_start_{true};
  bool forced_active_{false};
  std::uint32_t forced_duration_ms_{0};
  bool pending_activation_{false};
  std::size_t pending_index_{0};
  std::uint32_t pending_duration_ms_{0};
  bool playlist_timer_reset_pending_{false};
  std::array<OverlayEntry, max_overlays> overlays_{};
  std::size_t overlay_count_{0};
  std::uint64_t overlay_sequence_{0};
  Scene* last_scene_{nullptr};
};

}  // namespace amg::flightwall
