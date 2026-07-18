#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace amg::flightwall {

struct FlashPartition {
  const char* name;
  std::uint32_t offset;
  std::uint32_t size;
};

struct FlashPartitionMap {
  std::array<FlashPartition, 6> entries;

  [[nodiscard]] constexpr bool is_contiguous() const noexcept {
    for (std::size_t index = 1; index < entries.size(); ++index) {
      if (entries[index - 1].offset + entries[index - 1].size != entries[index].offset) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] constexpr std::uint32_t end_offset() const noexcept {
    const FlashPartition& last = entries.back();
    return last.offset + last.size;
  }
};

struct Hub75Pins {
  int r1;
  int g1;
  int b1;
  int r2;
  int g2;
  int b2;
  int row_a;
  int row_b;
  int row_c;
  int row_d;
  int row_e;
  int latch;
  int output_enable;
  int clock;

  [[nodiscard]] constexpr std::array<int, 14> values() const noexcept {
    return {r1, g1, b1, r2, g2, b2, row_a, row_b, row_c, row_d, row_e, latch,
            output_enable, clock};
  }
};

struct TargetProfile {
  int display_width;
  int display_height;
  int scan_denominator;
  std::uint32_t flash_bytes;
  bool psram_enabled;
  Hub75Pins hub75;
  FlashPartitionMap partition_map;

  [[nodiscard]] constexpr bool uses_gpio(const int gpio) const noexcept {
    for (const int configured : hub75.values()) {
      if (configured == gpio) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] constexpr bool pins_are_unique() const noexcept {
    const auto pins = hub75.values();
    for (std::size_t left = 0; left < pins.size(); ++left) {
      for (std::size_t right = left + 1; right < pins.size(); ++right) {
        if (pins[left] == pins[right]) {
          return false;
        }
      }
    }
    return true;
  }
};

[[nodiscard]] constexpr TargetProfile hdWf2MiniProfile() noexcept {
  constexpr std::array<FlashPartition, 6> partitions{
      FlashPartition{"nvs", 0x00009000U, 0x00005000U},
      FlashPartition{"otadata", 0x0000E000U, 0x00002000U},
      FlashPartition{"app0", 0x00010000U, 0x002C0000U},
      FlashPartition{"app1", 0x002D0000U, 0x002C0000U},
      FlashPartition{"factoryprov", 0x00590000U, 0x00001000U},
      FlashPartition{"spiffs", 0x00591000U, 0x0026F000U},
  };

  return TargetProfile{
      128,
      64,
      32,
      8U * 1024U * 1024U,
      false,
      Hub75Pins{2, 6, 10, 3, 7, 11, 39, 38, 37, 36, 21, 33, 35, 34},
      FlashPartitionMap{partitions},
  };
}

static_assert(hdWf2MiniProfile().pins_are_unique(), "HD-WF2 HUB75 pins must be unique");
static_assert(!hdWf2MiniProfile().uses_gpio(19), "GPIO19 is reserved for native USB D-");
static_assert(!hdWf2MiniProfile().uses_gpio(20), "GPIO20 is reserved for native USB D+");
static_assert(hdWf2MiniProfile().partition_map.is_contiguous(),
              "HD-WF2 data and app partitions must be contiguous");
static_assert(hdWf2MiniProfile().partition_map.end_offset() ==
                  hdWf2MiniProfile().flash_bytes,
              "HD-WF2 partition map must fill the 8 MB flash");

}  // namespace amg::flightwall
