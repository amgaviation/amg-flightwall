#include "amg/flightwall/scene_support.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace amg::flightwall::scene_support {

std::string_view formatNumber(std::array<char, 16>& buffer, const std::int64_t value) noexcept {
  const auto conversion = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  if (conversion.ec != std::errc{}) {
    return "ERR";
  }
  return std::string_view(buffer.data(),
                          static_cast<std::size_t>(conversion.ptr - buffer.data()));
}

std::string_view formatTwoDigits(std::array<char, 4>& buffer, const int value) noexcept {
  const int clamped = std::clamp(value, 0, 99);
  buffer[0] = static_cast<char>('0' + clamped / 10);
  buffer[1] = static_cast<char>('0' + clamped % 10);
  return std::string_view(buffer.data(), 2);
}

std::string_view formatCompactUsd(std::array<char, 16>& buffer, const std::int64_t cents) noexcept {
  if (cents < 0) {
    return "--";
  }
  const std::int64_t dollars = cents / 100;
  if (dollars < 1'000) {
    return formatNumber(buffer, dollars);
  }
  const std::int64_t thousands = dollars / 1'000;
  const std::int64_t tenths = (dollars % 1'000) / 100;
  const auto conversion =
      std::to_chars(buffer.data(), buffer.data() + buffer.size() - 3, thousands);
  if (conversion.ec != std::errc{}) {
    return "ERR";
  }
  char* cursor = conversion.ptr;
  if (thousands < 100 && tenths > 0) {
    *cursor++ = '.';
    *cursor++ = static_cast<char>('0' + tenths);
  }
  *cursor++ = 'K';
  return std::string_view(buffer.data(), static_cast<std::size_t>(cursor - buffer.data()));
}

bool fontSupports(const char character) noexcept {
  const unsigned char value = static_cast<unsigned char>(character);
  if (value >= 128) {
    return false;
  }
  return std::isalnum(value) != 0 || character == '-' || character == ':' ||
         character == '.' || character == '/' || character == ' ';
}

void sanitizeForFont(std::string& text) {
  for (char& character : text) {
    if (!fontSupports(character)) {
      character = ' ';
    }
  }
}

void toUpperAscii(std::string& text) {
  for (char& character : text) {
    character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
  }
}

std::size_t appendBounded(char* const buffer, const std::size_t capacity, std::size_t length,
                          const std::string_view text) noexcept {
  for (const char character : text) {
    if (length >= capacity) {
      break;
    }
    buffer[length++] = character;
  }
  return length;
}

int marqueeOffset(const std::uint64_t now_ms, const int cycle_px,
                  const int speed_px_per_s) noexcept {
  if (cycle_px <= 0 || speed_px_per_s <= 0) {
    return 0;
  }
  const std::uint64_t travelled = now_ms * static_cast<std::uint64_t>(speed_px_per_s) / 1'000ULL;
  return static_cast<int>(travelled % static_cast<std::uint64_t>(cycle_px));
}

void drawTextRightAligned(Renderer& renderer, const int right_x, const int y,
                          const std::string_view text, const Color color,
                          const int scale) noexcept {
  renderer.text(right_x - renderer.textWidth(text, scale), y, text, color, scale);
}

void drawTextCentered(Renderer& renderer, const int y, const std::string_view text,
                      const Color color, const int scale) noexcept {
  renderer.text((renderer.width() - renderer.textWidth(text, scale)) / 2, y, text, color, scale);
}

void drawMarqueeText(Renderer& renderer, const std::string_view text, const int y,
                     const Color color, const int scale, const std::uint64_t now_ms,
                     const int speed_px_per_s, const int gap_px) noexcept {
  if (text.empty() || scale <= 0) {
    return;
  }
  const int advance = static_cast<int>(text.size()) * glyphAdvance(scale);
  const int cycle = advance + std::max(gap_px, glyphAdvance(scale));
  const int offset = marqueeOffset(now_ms, cycle, speed_px_per_s);
  for (int x = -offset; x < renderer.width(); x += cycle) {
    renderer.text(x, y, text, color, scale);
  }
}

}  // namespace amg::flightwall::scene_support
