// AMG FlightWall live target — bounded log ring buffer.
//
// 256-line ring capturing the target's log lines. Every append is echoed to
// Serial immediately. SSE fanout is pull-based: the composition root drains
// lines added since the last drain from the loop task and forwards them to
// connected event-stream clients, so no cross-task network sends happen from
// inside logging calls. All public methods are mutex-guarded and safe to call
// from any task, but appends from async callbacks should be avoided per the
// concurrency rule (enqueue a command instead).
#pragma once

#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>

#include <WString.h>

namespace amg::flightwall::live {

class LogBuffer {
 public:
  static constexpr std::size_t kCapacity = 256;
  static constexpr std::size_t kMaxLineLength = 160;

  // Appends one line (truncated to kMaxLineLength) and echoes it to Serial.
  void append(const char* line) noexcept;

  // printf-style convenience wrapper around append().
  void logf(const char* format, ...) noexcept __attribute__((format(printf, 2, 3)));

  // Joins the newest `max_lines` lines (oldest first) into one string.
  [[nodiscard]] String snapshotText(std::size_t max_lines = kCapacity) const;

  // Invokes `sink` once per line appended since the previous drain call.
  // Intended to be called from the loop task only.
  void drainNew(const std::function<void(const char*)>& sink);

 private:
  struct Line {
    std::array<char, kMaxLineLength + 1> text{};
  };

  mutable std::mutex mutex_{};
  std::array<Line, kCapacity> lines_{};
  std::uint64_t next_sequence_{0};   // sequence of the next line to write
  std::uint64_t drained_sequence_{0};  // first sequence not yet drained
};

}  // namespace amg::flightwall::live
