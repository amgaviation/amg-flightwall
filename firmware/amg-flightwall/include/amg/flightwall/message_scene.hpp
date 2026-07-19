#pragma once

#include <array>
#include <cstddef>

#include "amg/flightwall/data_models.hpp"
#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

// Static or horizontally scrolling text with a settable color. Static text is
// centered at scale 2 when it fits, otherwise word-wrapped into up to four
// scale-1 lines (precomputed in the setter). Empty text renders "NO MESSAGE".
class MessageScene final : public Scene {
 public:
  static constexpr std::size_t max_lines = 4;
  static constexpr std::size_t max_line_chars = 21;

  void setMessage(const MessagePayload& payload);

  [[nodiscard]] std::string_view id() const noexcept override;
  void render(FrameContext& context) noexcept override;

 private:
  MessagePayload payload_{};  // sanitized copy
  std::array<std::array<char, max_line_chars>, max_lines> lines_{};
  std::array<std::size_t, max_lines> line_lengths_{};
  std::size_t line_count_{0};
};

}  // namespace amg::flightwall
