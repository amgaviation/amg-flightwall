#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "amg/flightwall/color.hpp"

namespace amg::flightwall {

// Portable data models pushed into scenes by the platform layer. The core
// never fetches data or does wall-clock math; snapshots carry a
// platform-supplied timestamp and validity flag and scenes render a labeled
// stale state whenever `valid` is false.

struct FlightContact {
  std::string callsign;
  double lat{0.0};
  double lon{0.0};
  int altitude_ft{0};
  int ground_speed_kt{0};
  int heading_deg{0};
  double distance_nm{0.0};
  bool watchlisted{false};
};

struct FlightSnapshot {
  std::vector<FlightContact> contacts;
  std::uint64_t fetched_at_ms{0};
  bool valid{false};
};

struct MetarSnapshot {
  std::string station;
  std::string raw;
  std::string flight_category;
  std::string wind;
  std::string visibility;
  int temp_c{0};
  int dewpoint_c{0};
  std::uint64_t fetched_at_ms{0};
  bool valid{false};
};

struct AmgMissionItem {
  std::string label;
  std::string status;
  int eta_min{-1};
};

struct AmgRequestItem {
  std::string label;
  std::string name;
  int age_min{-1};
};

struct AmgSubmissionItem {
  std::string kind;
  std::string name;
  int age_min{-1};
};

struct AmgMetricsSnapshot {
  int new_request_count{0};
  std::vector<AmgRequestItem> latest_requests;
  int active_mission_count{0};
  std::vector<AmgMissionItem> missions;
  std::vector<AmgSubmissionItem> recent_submissions;
  std::string submissions_cursor;
  std::int64_t revenue_today_cents{-1};
  std::int64_t revenue_mtd_cents{-1};
  std::string currency;
  std::string site_state;
  std::uint64_t fetched_at_ms{0};
  bool valid{false};
};

struct NotificationEvent {
  std::string title;
  std::string body;
  Color color;
  std::uint8_t priority{0};
  std::uint32_t duration_ms{15'000};
};

struct MessagePayload {
  std::string text;
  Color color;
  std::uint32_t duration_ms{0};
  bool scroll{true};
};

struct ClockInfo {
  int hour{0};
  int minute{0};
  int second{0};
  int month{0};
  int day{0};
  int weekday{0};  // 0 = Sunday
  bool valid{false};  // platform supplies wall-clock; core never does time math
};

struct CountdownInfo {
  std::string label;
  std::int64_t seconds_remaining{-1};
};

}  // namespace amg::flightwall
