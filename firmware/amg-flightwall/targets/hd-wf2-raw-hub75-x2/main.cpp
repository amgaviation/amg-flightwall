#include <Arduino.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <driver/gpio.h>

namespace {

constexpr int kWidth = 128;
constexpr int kHeight = 64;
constexpr int kScanRows = 32;
constexpr std::uint32_t kPatternDurationMs = 2500;
constexpr std::uint32_t kOutputOnTimeUs = 70;

constexpr gpio_num_t kR1 = GPIO_NUM_4;
constexpr gpio_num_t kG1 = GPIO_NUM_8;
constexpr gpio_num_t kB1 = GPIO_NUM_12;
constexpr gpio_num_t kR2 = GPIO_NUM_5;
constexpr gpio_num_t kG2 = GPIO_NUM_9;
constexpr gpio_num_t kB2 = GPIO_NUM_13;
constexpr gpio_num_t kRowA = GPIO_NUM_39;
constexpr gpio_num_t kRowB = GPIO_NUM_38;
constexpr gpio_num_t kRowC = GPIO_NUM_37;
constexpr gpio_num_t kRowD = GPIO_NUM_36;
constexpr gpio_num_t kRowE = GPIO_NUM_21;
constexpr gpio_num_t kLatch = GPIO_NUM_33;
constexpr gpio_num_t kOutputEnable = GPIO_NUM_35;
constexpr gpio_num_t kClock = GPIO_NUM_34;

constexpr std::array<gpio_num_t, 14> kOutputPins{
    kOutputEnable, kR1,   kG1,   kB1,   kR2,   kG2,  kB2,
    kRowA,         kRowB, kRowC, kRowD, kRowE, kLatch, kClock,
};

enum Color : std::uint8_t {
  black = 0,
  red = 1U << 0U,
  green = 1U << 1U,
  blue = 1U << 2U,
  yellow = red | green,
  cyan = green | blue,
  magenta = red | blue,
  white = red | green | blue,
};

enum class GpioInitialization : std::uint8_t {
  ready,
  reset_failed,
  direction_failed,
  drive_capability_failed,
  initial_level_failed,
};

using Framebuffer =
    std::array<std::uint8_t, static_cast<std::size_t>(kWidth * kHeight)>;

constexpr std::array<std::array<std::uint8_t, 7>, 3> kAmgGlyphs{{
    {{0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {{0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
    {{0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110}},
}};

static_assert(kHeight == kScanRows * 2, "HUB75 upper/lower halves must share a scan row");
static_assert(kWidth % 16 == 0, "FM6124E control words repeat every 16 columns");

inline void setPin(const gpio_num_t pin, const bool high) noexcept {
  gpio_set_level(pin, high ? 1 : 0);
}

inline void pulseClock() noexcept {
  setPin(kClock, true);
  asm volatile("nop\n\tnop\n\t");
  setPin(kClock, false);
}

void setAllColorLines(const bool high) noexcept {
  setPin(kR1, high);
  setPin(kG1, high);
  setPin(kB1, high);
  setPin(kR2, high);
  setPin(kG2, high);
  setPin(kB2, high);
}

void setColorLines(const std::uint8_t upper, const std::uint8_t lower) noexcept {
  setPin(kR1, (upper & Color::red) != 0U);
  setPin(kG1, (upper & Color::green) != 0U);
  setPin(kB1, (upper & Color::blue) != 0U);
  setPin(kR2, (lower & Color::red) != 0U);
  setPin(kG2, (lower & Color::green) != 0U);
  setPin(kB2, (lower & Color::blue) != 0U);
}

GpioInitialization configurePins() noexcept {
  for (const gpio_num_t pin : kOutputPins) {
    if (gpio_reset_pin(pin) != ESP_OK) {
      return GpioInitialization::reset_failed;
    }
    if (gpio_set_direction(pin, GPIO_MODE_OUTPUT) != ESP_OK) {
      return GpioInitialization::direction_failed;
    }
    if (gpio_set_drive_capability(pin, GPIO_DRIVE_CAP_3) != ESP_OK) {
      return GpioInitialization::drive_capability_failed;
    }

    const int initial_level = pin == kOutputEnable ? 1 : 0;
    if (gpio_set_level(pin, initial_level) != ESP_OK) {
      return GpioInitialization::initial_level_failed;
    }
  }

  return GpioInitialization::ready;
}

void writeFm6124eControlWord(const std::array<bool, 16>& word,
                             const int latch_clock_count) noexcept {
  setPin(kLatch, false);

  for (int column = 0; column < kWidth; ++column) {
    setAllColorLines(word[static_cast<std::size_t>(column % 16)]);
    if (column >= kWidth - latch_clock_count) {
      setPin(kLatch, true);
    }
    pulseClock();
  }

  setPin(kLatch, false);
}

void initializeFm6124e() noexcept {
  // REG1 selects the driver's global current level; REG2 enables LED output.
  constexpr std::array<bool, 16> reg1{
      false, false, false, false, false, true, true, true,
      true,  true,  true,  false, false, false, false, false,
  };
  constexpr std::array<bool, 16> reg2{
      false, false, false, false, false, false, false, false,
      false, true,  false, false, false, false, false, false,
  };

  setPin(kOutputEnable, true);
  writeFm6124eControlWord(reg1, 11);
  writeFm6124eControlWord(reg2, 12);

  setAllColorLines(false);
  for (int column = 0; column < kWidth; ++column) {
    pulseClock();
  }

  setPin(kLatch, true);
  pulseClock();
  setPin(kLatch, false);
  setPin(kOutputEnable, false);
  pulseClock();
}

constexpr std::size_t pixelOffset(const int x, const int y) noexcept {
  return static_cast<std::size_t>(y * kWidth + x);
}

void clear(Framebuffer& framebuffer, const Color color = Color::black) noexcept {
  framebuffer.fill(static_cast<std::uint8_t>(color));
}

void setPixel(Framebuffer& framebuffer, const int x, const int y,
              const Color color) noexcept {
  if (x >= 0 && x < kWidth && y >= 0 && y < kHeight) {
    framebuffer[pixelOffset(x, y)] = static_cast<std::uint8_t>(color);
  }
}

void fillRectangle(Framebuffer& framebuffer, const int x, const int y, const int width,
                   const int height, const Color color) noexcept {
  for (int row = y; row < y + height; ++row) {
    for (int column = x; column < x + width; ++column) {
      setPixel(framebuffer, column, row, color);
    }
  }
}

void drawRectangle(Framebuffer& framebuffer, const int x, const int y, const int width,
                   const int height, const Color color) noexcept {
  fillRectangle(framebuffer, x, y, width, 1, color);
  fillRectangle(framebuffer, x, y + height - 1, width, 1, color);
  fillRectangle(framebuffer, x, y, 1, height, color);
  fillRectangle(framebuffer, x + width - 1, y, 1, height, color);
}

void drawGlyph(Framebuffer& framebuffer, const std::array<std::uint8_t, 7>& glyph,
               const int x, const int y, const int scale, const Color color) noexcept {
  for (int glyph_y = 0; glyph_y < 7; ++glyph_y) {
    for (int glyph_x = 0; glyph_x < 5; ++glyph_x) {
      const std::uint8_t mask = static_cast<std::uint8_t>(1U << (4 - glyph_x));
      if ((glyph[static_cast<std::size_t>(glyph_y)] & mask) != 0U) {
        fillRectangle(framebuffer, x + glyph_x * scale, y + glyph_y * scale, scale,
                      scale, color);
      }
    }
  }
}

void renderPattern(Framebuffer& framebuffer, const std::uint8_t pattern) noexcept {
  clear(framebuffer);

  switch (pattern) {
    case 0:
      clear(framebuffer, Color::red);
      break;
    case 1:
      clear(framebuffer, Color::green);
      break;
    case 2:
      clear(framebuffer, Color::blue);
      break;
    case 3:
      for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
          if ((((x / 8) + (y / 8)) & 1) == 0) {
            setPixel(framebuffer, x, y, Color::white);
          }
        }
      }
      break;
    case 4:
      fillRectangle(framebuffer, 0, 0, 32, kHeight, Color::red);
      fillRectangle(framebuffer, 32, 0, 32, kHeight, Color::green);
      fillRectangle(framebuffer, 64, 0, 32, kHeight, Color::blue);
      fillRectangle(framebuffer, 96, 0, 32, kHeight, Color::white);
      break;
    case 5:
      drawRectangle(framebuffer, 0, 0, kWidth, kHeight, Color::white);
      drawGlyph(framebuffer, kAmgGlyphs[0], 13, 11, 6, Color::red);
      drawGlyph(framebuffer, kAmgGlyphs[1], 49, 11, 6, Color::green);
      drawGlyph(framebuffer, kAmgGlyphs[2], 85, 11, 6, Color::blue);
      break;
    default:
      for (int y = 0; y < kHeight; ++y) {
        const Color color = static_cast<Color>(1U << static_cast<unsigned>((y / 8) % 3));
        fillRectangle(framebuffer, 0, y, kWidth, 1, color);
      }
      drawRectangle(framebuffer, 0, 0, kWidth, kHeight, Color::white);
      break;
  }
}

void selectRow(const std::uint8_t row) noexcept {
  setPin(kRowA, (row & 0x01U) != 0U);
  setPin(kRowB, (row & 0x02U) != 0U);
  setPin(kRowC, (row & 0x04U) != 0U);
  setPin(kRowD, (row & 0x08U) != 0U);
  setPin(kRowE, (row & 0x10U) != 0U);
}

void refreshRow(const Framebuffer& framebuffer, const std::uint8_t row) noexcept {
  setPin(kOutputEnable, true);
  selectRow(row);

  const int upper_y = row;
  const int lower_y = row + kScanRows;
  for (int x = 0; x < kWidth; ++x) {
    setColorLines(framebuffer[pixelOffset(x, upper_y)],
                  framebuffer[pixelOffset(x, lower_y)]);
    if (x == kWidth - 1) {
      // The HD-WF2 waveform clocks the final pixel with LAT asserted.
      setPin(kLatch, true);
    }
    pulseClock();
  }
  setPin(kLatch, false);

  setPin(kOutputEnable, false);
  delayMicroseconds(kOutputOnTimeUs);
  setPin(kOutputEnable, true);
}

void announcePattern(const std::uint8_t pattern) {
  constexpr std::array<const char*, 7> names{
      "solid red", "solid green", "solid blue", "white checkerboard",
      "RGBW columns", "AMG", "row-address bands",
  };
  Serial.printf("RAW HUB75: pattern %u - %s\n", static_cast<unsigned>(pattern),
                names[pattern]);
}

const char* gpioInitializationName(const GpioInitialization result) noexcept {
  switch (result) {
    case GpioInitialization::ready:
      return "ready";
    case GpioInitialization::reset_failed:
      return "reset failed";
    case GpioInitialization::direction_failed:
      return "direction failed";
    case GpioInitialization::drive_capability_failed:
      return "drive capability failed";
    case GpioInitialization::initial_level_failed:
      return "initial level failed";
  }

  return "unknown failure";
}

class RawHub75Diagnostic final {
 public:
  void setup() noexcept {
    const GpioInitialization gpio_result = configurePins();
    if (gpio_result != GpioInitialization::ready) {
      Serial.printf("AMG FlightWall: raw HUB75 GPIO initialization failed: %s\n",
                    gpioInitializationName(gpio_result));
      return;
    }

    initializeFm6124e();
    pattern_started_ms_ = millis();
    last_heartbeat_ms_ = pattern_started_ms_;
    renderPattern(framebuffer_, active_pattern_);
    initialized_ = true;

    Serial.println("AMG FlightWall: standalone raw HUB75/FM6124E diagnostic initialized");
    Serial.println("RAW HUB75: 128x64, 1/32 scan, X2 pin profile (75EX2 cross-check)");
    announcePattern(active_pattern_);
  }

  void loop() noexcept {
    if (!initialized_) {
      delay(1000);
      return;
    }

    refreshRow(framebuffer_, active_row_);
    active_row_ = static_cast<std::uint8_t>((active_row_ + 1U) % kScanRows);

    if (active_row_ != 0U) {
      return;
    }

    ++completed_frames_;
    const std::uint32_t now = millis();
    if (now - pattern_started_ms_ >= kPatternDurationMs) {
      active_pattern_ = static_cast<std::uint8_t>((active_pattern_ + 1U) % 7U);
      renderPattern(framebuffer_, active_pattern_);
      pattern_started_ms_ = now;
      announcePattern(active_pattern_);
    }

    if (now - last_heartbeat_ms_ >= 5000U) {
      Serial.printf("RAW HUB75: alive, frames=%lu\n",
                    static_cast<unsigned long>(completed_frames_));
      last_heartbeat_ms_ = now;
    }

    yield();
  }

 private:
  Framebuffer framebuffer_{};
  std::uint8_t active_pattern_{0};
  std::uint8_t active_row_{0};
  std::uint32_t pattern_started_ms_{0};
  std::uint32_t last_heartbeat_ms_{0};
  std::uint32_t completed_frames_{0};
  bool initialized_{false};
};

RawHub75Diagnostic& runtime() noexcept {
  static RawHub75Diagnostic instance;
  return instance;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  runtime().setup();
}

void loop() {
  runtime().loop();
}
