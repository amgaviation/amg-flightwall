#include "amg/flightwall/scene_rotator.hpp"

#include <utility>

namespace amg::flightwall {

void SceneRotator::setPlaylist(std::vector<SceneSlot> slots) {
  slots_ = std::move(slots);
  active_index_ = 0;
  slot_needs_start_ = true;
  forced_active_ = false;
  forced_duration_ms_ = 0;
  pending_activation_ = false;
}

const std::vector<SceneSlot>& SceneRotator::playlist() const noexcept { return slots_; }

bool SceneRotator::activateNow(const std::string_view id, const std::uint32_t duration_ms) noexcept {
  for (std::size_t index = 0; index < slots_.size(); ++index) {
    const SceneSlot& slot = slots_[index];
    if (slot.scene != nullptr && slot.scene->id() == id) {
      pending_activation_ = true;
      pending_index_ = index;
      pending_duration_ms_ = duration_ms;
      return true;
    }
  }
  return false;
}

bool SceneRotator::pushOverlay(Scene& scene, const std::uint32_t duration_ms,
                               const std::uint8_t priority) noexcept {
  if (overlay_count_ >= max_overlays) {
    return false;
  }
  overlays_[overlay_count_] = OverlayEntry{&scene, duration_ms, priority, overlay_sequence_, 0, false};
  ++overlay_sequence_;
  ++overlay_count_;
  return true;
}

std::size_t SceneRotator::overlayCount() const noexcept { return overlay_count_; }

Scene* SceneRotator::tick(FrameContext& context) noexcept {
  const std::uint64_t now_ms = context.monotonic_ms;

  if (pending_activation_) {
    active_index_ = pending_index_;
    forced_active_ = true;
    forced_duration_ms_ = pending_duration_ms_;
    slot_needs_start_ = true;
    pending_activation_ = false;
  }

  int top = topOverlayIndex();
  while (top >= 0) {
    OverlayEntry& overlay = overlays_[static_cast<std::size_t>(top)];
    if (!overlay.started) {
      overlay.started = true;
      overlay.started_ms = now_ms;
    }
    if (now_ms - overlay.started_ms >= overlay.duration_ms) {
      removeOverlay(static_cast<std::size_t>(top));
      top = topOverlayIndex();
      continue;
    }
    break;
  }
  for (std::size_t index = 0; index < overlay_count_; ++index) {
    if (static_cast<int>(index) != top) {
      overlays_[index].started = false;  // queued/preempted overlays restart fresh at the top
    }
  }

  Scene* scene = nullptr;
  if (top >= 0) {
    playlist_timer_reset_pending_ = true;
    scene = overlays_[static_cast<std::size_t>(top)].scene;
  } else {
    scene = selectPlaylistScene(now_ms);
  }

  transitionTo(scene);
  if (scene != nullptr) {
    scene->render(context);
  }
  return scene;
}

std::string_view SceneRotator::activeSceneId() const noexcept {
  return last_scene_ != nullptr ? last_scene_->id() : std::string_view{};
}

int SceneRotator::topOverlayIndex() const noexcept {
  int best = -1;
  for (std::size_t index = 0; index < overlay_count_; ++index) {
    if (best < 0) {
      best = static_cast<int>(index);
      continue;
    }
    const OverlayEntry& candidate = overlays_[index];
    const OverlayEntry& current = overlays_[static_cast<std::size_t>(best)];
    if (candidate.priority > current.priority ||
        (candidate.priority == current.priority && candidate.sequence < current.sequence)) {
      best = static_cast<int>(index);
    }
  }
  return best;
}

void SceneRotator::removeOverlay(const std::size_t index) noexcept {
  for (std::size_t next = index + 1; next < overlay_count_; ++next) {
    overlays_[next - 1] = overlays_[next];
  }
  --overlay_count_;
}

bool SceneRotator::advanceToNextEnabled() noexcept {
  if (slots_.empty()) {
    return false;
  }
  for (std::size_t step = 1; step <= slots_.size(); ++step) {
    const std::size_t candidate = (active_index_ + step) % slots_.size();
    if (slots_[candidate].enabled && slots_[candidate].scene != nullptr) {
      active_index_ = candidate;
      return true;
    }
  }
  return false;
}

Scene* SceneRotator::selectPlaylistScene(const std::uint64_t now_ms) noexcept {
  if (slots_.empty()) {
    return nullptr;
  }
  if (active_index_ >= slots_.size()) {
    active_index_ = 0;
    slot_needs_start_ = true;
    forced_active_ = false;
  }
  if (playlist_timer_reset_pending_) {
    slot_needs_start_ = true;
    playlist_timer_reset_pending_ = false;
  }

  if (!forced_active_ &&
      (!slots_[active_index_].enabled || slots_[active_index_].scene == nullptr)) {
    if (!advanceToNextEnabled()) {
      return nullptr;
    }
    slot_needs_start_ = true;
  }

  if (slot_needs_start_) {
    slot_started_ms_ = now_ms;
    slot_needs_start_ = false;
  }

  std::uint32_t duration_ms = slots_[active_index_].duration_ms;
  if (forced_active_ && forced_duration_ms_ > 0) {
    duration_ms = forced_duration_ms_;
  }

  if (now_ms - slot_started_ms_ >= duration_ms) {
    forced_active_ = false;
    const bool advanced = advanceToNextEnabled();
    if (!advanced &&
        (!slots_[active_index_].enabled || slots_[active_index_].scene == nullptr)) {
      return nullptr;
    }
    slot_started_ms_ = now_ms;
  }
  return slots_[active_index_].scene;
}

void SceneRotator::transitionTo(Scene* const scene) noexcept {
  if (scene == last_scene_) {
    return;
  }
  if (last_scene_ != nullptr) {
    last_scene_->onExit();
  }
  last_scene_ = scene;
  if (last_scene_ != nullptr) {
    last_scene_->onEnter();
  }
}

}  // namespace amg::flightwall
