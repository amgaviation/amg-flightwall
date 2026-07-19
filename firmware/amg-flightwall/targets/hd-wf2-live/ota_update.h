// AMG FlightWall live target — OTA firmware update.
//
// POST /api/ota (multipart firmware.bin) streams through Arduino Update onto
// the inactive app slot (standard esp_ota A/B flow; the factory partition map
// keeps app0/app1 at 2.75 MB each). A SHA-256 of the received image is
// reported in the completion response. The route is auth-guarded by the web
// server before any flash write starts. On success the composition root
// reboots into the new slot after the response has been delivered.
//
// Upload chunks arrive on the async TCP task; this module touches only the
// flash updater and its own state — never scenes or settings.
#pragma once

#include <atomic>
#include <cstdint>

#include <ESPAsyncWebServer.h>
#include <mbedtls/sha256.h>

#include "log_buffer.h"

namespace amg::flightwall::live {

class OtaUpdater {
 public:
  explicit OtaUpdater(LogBuffer& log) noexcept : log_(log) {}

  // Multipart upload chunk handler (async task).
  void handleUpload(AsyncWebServerRequest* request, const String& filename, std::size_t index,
                    std::uint8_t* data, std::size_t length, bool final);

  // Completion handler — sends the JSON result (async task).
  void handleRequest(AsyncWebServerRequest* request);

  // True once a successful update awaits reboot; loop task polls this.
  [[nodiscard]] bool rebootPending() const noexcept { return reboot_pending_.load(); }

 private:
  void fail(const char* reason);

  LogBuffer& log_;
  // Identifies the request that owns the in-flight update. Async upload/complete
  // callbacks run serialized on the AsyncTCP task, so a plain pointer is safe;
  // it is only compared, never dereferenced. A concurrent upload whose request
  // differs is rejected instead of resetting the owner's flash state.
  AsyncWebServerRequest* owner_{nullptr};
  bool in_progress_{false};
  bool failed_{false};
  String error_{};
  String sha256_hex_{};
  std::size_t received_{0};
  mbedtls_sha256_context sha_context_{};
  std::atomic<bool> reboot_pending_{false};
};

}  // namespace amg::flightwall::live
