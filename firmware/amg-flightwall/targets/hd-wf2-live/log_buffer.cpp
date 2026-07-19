#include "log_buffer.h"

#include <cstdio>
#include <cstring>

#include <HardwareSerial.h>

namespace amg::flightwall::live {

void LogBuffer::append(const char* line) noexcept {
  if (line == nullptr) {
    return;
  }

  {
    const std::lock_guard<std::mutex> guard(mutex_);
    Line& slot = lines_[static_cast<std::size_t>(next_sequence_ % kCapacity)];
    std::strncpy(slot.text.data(), line, kMaxLineLength);
    slot.text[kMaxLineLength] = '\0';
    ++next_sequence_;
    if (next_sequence_ - drained_sequence_ > kCapacity) {
      drained_sequence_ = next_sequence_ - kCapacity;
    }
  }

  Serial.println(line);
}

void LogBuffer::logf(const char* format, ...) noexcept {
  char buffer[kMaxLineLength + 1];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  append(buffer);
}

String LogBuffer::snapshotText(const std::size_t max_lines) const {
  String output;
  const std::lock_guard<std::mutex> guard(mutex_);
  const std::uint64_t available =
      next_sequence_ < kCapacity ? next_sequence_ : static_cast<std::uint64_t>(kCapacity);
  const std::uint64_t requested =
      max_lines < available ? static_cast<std::uint64_t>(max_lines) : available;
  output.reserve(static_cast<unsigned int>(requested * 48));
  for (std::uint64_t sequence = next_sequence_ - requested; sequence < next_sequence_;
       ++sequence) {
    output += lines_[static_cast<std::size_t>(sequence % kCapacity)].text.data();
    output += '\n';
  }
  return output;
}

void LogBuffer::drainNew(const std::function<void(const char*)>& sink) {
  std::uint64_t first = 0;
  std::uint64_t last = 0;
  {
    const std::lock_guard<std::mutex> guard(mutex_);
    first = drained_sequence_;
    last = next_sequence_;
    drained_sequence_ = next_sequence_;
  }
  for (std::uint64_t sequence = first; sequence < last; ++sequence) {
    char copy[kMaxLineLength + 1];
    {
      const std::lock_guard<std::mutex> guard(mutex_);
      std::strncpy(copy, lines_[static_cast<std::size_t>(sequence % kCapacity)].text.data(),
                   kMaxLineLength);
      copy[kMaxLineLength] = '\0';
    }
    sink(copy);
  }
}

}  // namespace amg::flightwall::live
