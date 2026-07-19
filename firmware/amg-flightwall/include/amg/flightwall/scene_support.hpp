#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "amg/flightwall/color.hpp"
#include "amg/flightwall/renderer.hpp"

// Shared, portable drawing/formatting helpers for scenes. All draw helpers are
// heap-free so they are safe inside per-frame render paths; the sanitizers are
// intended for setters (which may allocate).
namespace amg::flightwall::scene_support {

// LIFR flight-category color; local because the shared palette header is frozen.
inline constexpr Color magenta{255, 0, 255};

// Marquee speed shared by scrolling rows; 20 px/s keeps offsets exact for
// millisecond timestamps that are multiples of 50.
inline constexpr int default_marquee_speed_px_s = 20;

// Glyph advance of the built-in 5x7 font in pixels at the given scale.
[[nodiscard]] constexpr int glyphAdvance(const int scale) noexcept { return 6 * scale; }

// Formats a signed integer into the buffer; returns "ERR" on overflow.
[[nodiscard]] std::string_view formatNumber(std::array<char, 16>& buffer,
                                            std::int64_t value) noexcept;

// Zero-padded 00..99 (values are clamped).
[[nodiscard]] std::string_view formatTwoDigits(std::array<char, 4>& buffer, int value) noexcept;

// Compact USD amount from cents for the 5x7 font (no '$' glyph): negative ->
// "--", under 1000 dollars -> "845", otherwise "12.4K" / "845K".
[[nodiscard]] std::string_view formatCompactUsd(std::array<char, 16>& buffer,
                                                std::int64_t cents) noexcept;

// True when the renderer font has a real glyph for the character (after the
// renderer's implicit upper-casing): A-Z a-z 0-9 '-' ':' '.' '/' ' '.
[[nodiscard]] bool fontSupports(char character) noexcept;

// Replaces unsupported characters with spaces so provider-supplied text never
// renders the error glyph. For setters only (operates on owned strings).
void sanitizeForFont(std::string& text);

// ASCII upper-case in place (for case-insensitive comparisons in setters).
void toUpperAscii(std::string& text);

// Appends text into a fixed buffer; returns the new length (never overflows).
[[nodiscard]] std::size_t appendBounded(char* buffer, std::size_t capacity, std::size_t length,
                                        std::string_view text) noexcept;

// Deterministic marquee pixel offset within [0, cycle_px).
[[nodiscard]] int marqueeOffset(std::uint64_t now_ms, int cycle_px,
                                int speed_px_per_s) noexcept;

void drawTextRightAligned(Renderer& renderer, int right_x, int y, std::string_view text,
                          Color color, int scale = 1) noexcept;

void drawTextCentered(Renderer& renderer, int y, std::string_view text, Color color,
                      int scale = 1) noexcept;

// Draws horizontally scrolling text across the full panel width at row y; the
// display clips at the panel edges. Fully deterministic in now_ms.
void drawMarqueeText(Renderer& renderer, std::string_view text, int y, Color color, int scale,
                     std::uint64_t now_ms, int speed_px_per_s, int gap_px) noexcept;

}  // namespace amg::flightwall::scene_support
