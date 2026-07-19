#include "ota_update.h"

#include <Update.h>

namespace amg::flightwall::live {

void OtaUpdater::fail(const char* reason) {
  if (!failed_) {
    failed_ = true;
    error_ = reason;
    log_.logf("ota: failed (%s)", reason);
  }
  if (in_progress_) {
    Update.abort();
    mbedtls_sha256_free(&sha_context_);
    in_progress_ = false;
  }
}

void OtaUpdater::handleUpload(AsyncWebServerRequest* request, const String& filename,
                              const std::size_t index, std::uint8_t* data,
                              const std::size_t length, const bool final) {
  (void)request;

  if (index == 0) {
    failed_ = false;
    error_ = "";
    sha256_hex_ = "";
    received_ = 0;
    log_.logf("ota: upload start (%s)", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      fail("update begin rejected");
      return;
    }
    in_progress_ = true;
    mbedtls_sha256_init(&sha_context_);
    mbedtls_sha256_starts(&sha_context_, 0);
  }

  if (failed_ || !in_progress_) {
    return;
  }

  if (length > 0) {
    if (Update.write(data, length) != length) {
      fail("flash write failed");
      return;
    }
    mbedtls_sha256_update(&sha_context_, data, length);
    received_ += length;
  }

  if (final) {
    std::uint8_t digest[32] = {};
    mbedtls_sha256_finish(&sha_context_, digest);
    mbedtls_sha256_free(&sha_context_);
    static constexpr char kHex[] = "0123456789abcdef";
    char hex[65];
    for (std::size_t position = 0; position < sizeof(digest); ++position) {
      hex[position * 2] = kHex[digest[position] >> 4];
      hex[position * 2 + 1] = kHex[digest[position] & 0x0f];
    }
    hex[64] = '\0';
    sha256_hex_ = hex;

    if (!Update.end(true)) {
      in_progress_ = false;
      failed_ = true;
      error_ = Update.errorString();
      log_.logf("ota: end failed (%s)", error_.c_str());
      return;
    }
    in_progress_ = false;
    log_.logf("ota: image accepted, %u bytes, sha256 %s",
              static_cast<unsigned>(received_), sha256_hex_.c_str());
    reboot_pending_.store(true);
  }
}

void OtaUpdater::handleRequest(AsyncWebServerRequest* request) {
  if (failed_) {
    String body = "{\"ok\":false,\"error\":\"";
    body += error_;
    body += "\"}";
    request->send(500, "application/json", body);
    return;
  }
  String body = "{\"ok\":true,\"bytes\":";
  body += static_cast<unsigned>(received_);
  body += ",\"sha256\":\"";
  body += sha256_hex_;
  body += "\",\"rebooting\":true}";
  request->send(200, "application/json", body);
}

}  // namespace amg::flightwall::live
