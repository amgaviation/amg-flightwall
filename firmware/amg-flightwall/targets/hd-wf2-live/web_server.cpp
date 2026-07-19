#include "web_server.h"

#include <cstring>

#include <AsyncJson.h>
#include <WiFi.h>

#include "webui_assets.h"

namespace amg::flightwall::live {

namespace {

constexpr const char* kJsonContentType = "application/json";

// [{id, name, configurable fields}] catalog per the contract.
constexpr const char* kSceneCatalog =
    "[{\"id\":\"clock\",\"name\":\"Clock\",\"fields\":[\"time.tz\",\"time.ntp\"]},"
    "{\"id\":\"flights\",\"name\":\"Flight Radar\",\"fields\":[\"location.lat\","
    "\"location.lon\",\"location.radius_nm\",\"flights.provider\",\"flights.poll_s\","
    "\"flights.watchlist\"]},"
    "{\"id\":\"metar\",\"name\":\"METAR\",\"fields\":[\"metar.station\",\"metar.poll_s\"]},"
    "{\"id\":\"amg_ops\",\"name\":\"AMG Ops\",\"fields\":[\"amg.base_url\",\"amg.poll_s\","
    "\"amg.notify_on_submission\",\"amg.notify_on_request\"]},"
    "{\"id\":\"missions\",\"name\":\"Mission Board\",\"fields\":[\"amg.base_url\","
    "\"amg.poll_s\"]},"
    "{\"id\":\"countdown\",\"name\":\"Countdown\",\"fields\":[\"countdown.label\","
    "\"countdown.target_epoch\"]},"
    "{\"id\":\"message\",\"name\":\"Message\",\"fields\":[\"message.text\","
    "\"message.color\",\"message.scroll\"]}]";

[[nodiscard]] bool constantTimeEquals(const String& left, const String& right) noexcept {
  if (left.length() != right.length()) {
    return false;  // hash lengths are fixed; length is not secret
  }
  volatile std::uint8_t difference = 0;
  for (unsigned int index = 0; index < left.length(); ++index) {
    difference = static_cast<std::uint8_t>(
        difference | (static_cast<std::uint8_t>(left[index]) ^
                      static_cast<std::uint8_t>(right[index])));
  }
  return difference == 0;
}

void sendJsonError(AsyncWebServerRequest* request, const int code, const char* message) {
  String body = "{\"ok\":false,\"error\":\"";
  body += message;
  body += "\"}";
  request->send(code, kJsonContentType, body);
}

}  // namespace

bool CommandQueue::push(Command command) {
  const std::lock_guard<std::mutex> guard(mutex_);
  if (queue_.size() >= kCapacity) {
    return false;
  }
  queue_.push_back(std::move(command));
  return true;
}

bool CommandQueue::pop(Command& command) {
  const std::lock_guard<std::mutex> guard(mutex_);
  if (queue_.empty()) {
    return false;
  }
  command = std::move(queue_.front());
  queue_.pop_front();
  return true;
}

bool WebServer::authorized(AsyncWebServerRequest* request) const {
  String admin_hash;
  bool setup_mode = false;
  {
    const std::lock_guard<std::mutex> guard(state_.mutex);
    admin_hash = state_.admin_hash;
    setup_mode = state_.setup_mode;
  }
  if (setup_mode || admin_hash.length() == 0) {
    return true;  // captive portal, or first run before a password exists
  }
  if (!request->hasHeader("X-Auth")) {
    return false;
  }
  String provided = request->getHeader("X-Auth")->value();
  provided.toLowerCase();
  return constantTimeEquals(provided, admin_hash);
}

void WebServer::enqueueOrFail(AsyncWebServerRequest* request, Command command) {
  if (!commands_.push(std::move(command))) {
    sendJsonError(request, 429, "busy");
    return;
  }
  request->send(200, kJsonContentType, "{\"ok\":true}");
}

void WebServer::begin() {
  registerApi();
  registerAssets();
  server_.onNotFound([this](AsyncWebServerRequest* request) { handleNotFound(request); });
  server_.begin();
}

void WebServer::registerApi() {
  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    String json;
    {
      const std::lock_guard<std::mutex> guard(state_.mutex);
      json = state_.status_json;
    }
    request->send(200, kJsonContentType, json);
  });

  server_.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    String json;
    {
      const std::lock_guard<std::mutex> guard(state_.mutex);
      json = state_.config_json;
    }
    request->send(200, kJsonContentType, json);
  });

  auto* config_handler = new AsyncCallbackJsonWebHandler(
      "/api/config", [this](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!authorized(request)) {
          sendJsonError(request, 401, "unauthorized");
          return;
        }
        JsonObjectConst body = json.as<JsonObjectConst>();
        if (body.isNull()) {
          sendJsonError(request, 400, "invalid json");
          return;
        }

        GeometrySettings active_geometry;
        {
          const std::lock_guard<std::mutex> guard(state_.mutex);
          active_geometry = state_.geometry;
        }
        DeviceSettings preview;
        preview.display.geometry = active_geometry;
        if (!SettingsStore::applyJson(body, preview)) {
          sendJsonError(request, 400, "invalid config");
          return;
        }
        const bool reboot_required = preview.display.geometry != active_geometry;

        Command command;
        command.type = Command::Type::apply_config;
        serializeJson(json, command.json);
        if (!commands_.push(std::move(command))) {
          sendJsonError(request, 429, "busy");
          return;
        }
        String response = "{\"ok\":true,\"reboot_required\":";
        response += reboot_required ? "true}" : "false}";
        request->send(200, kJsonContentType, response);
      });
  config_handler->setMethod(HTTP_PUT);
  server_.addHandler(config_handler);

  auto* secrets_handler = new AsyncCallbackJsonWebHandler(
      "/api/secrets", [this](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!authorized(request)) {
          sendJsonError(request, 401, "unauthorized");
          return;
        }
        if (json.as<JsonObjectConst>().isNull()) {
          sendJsonError(request, 400, "invalid json");
          return;
        }
        Command command;
        command.type = Command::Type::set_secrets;
        serializeJson(json, command.json);
        enqueueOrFail(request, std::move(command));
      });
  secrets_handler->setMethod(HTTP_POST);
  server_.addHandler(secrets_handler);

  server_.on("/api/scenes", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, kJsonContentType, kSceneCatalog);
  });

  auto* activate_handler = new AsyncCallbackJsonWebHandler(
      "/api/scene/activate", [this](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!authorized(request)) {
          sendJsonError(request, 401, "unauthorized");
          return;
        }
        JsonObjectConst body = json.as<JsonObjectConst>();
        if (body.isNull() || !body["id"].is<const char*>()) {
          sendJsonError(request, 400, "id required");
          return;
        }
        Command command;
        command.type = Command::Type::activate_scene;
        command.id = body["id"].as<const char*>();
        const int duration_s = body["duration_s"].is<int>() ? body["duration_s"].as<int>() : 10;
        command.duration_ms = static_cast<std::uint32_t>(duration_s > 0 ? duration_s : 10) * 1000U;
        enqueueOrFail(request, std::move(command));
      });
  activate_handler->setMethod(HTTP_POST);
  server_.addHandler(activate_handler);

  auto* message_handler = new AsyncCallbackJsonWebHandler(
      "/api/message", [this](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!authorized(request)) {
          sendJsonError(request, 401, "unauthorized");
          return;
        }
        JsonObjectConst body = json.as<JsonObjectConst>();
        if (body.isNull() || !body["text"].is<const char*>()) {
          sendJsonError(request, 400, "text required");
          return;
        }
        Command command;
        command.type = Command::Type::show_message;
        serializeJson(json, command.json);
        enqueueOrFail(request, std::move(command));
      });
  message_handler->setMethod(HTTP_POST);
  server_.addHandler(message_handler);

  auto* notify_handler = new AsyncCallbackJsonWebHandler(
      "/api/notify", [this](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!authorized(request)) {
          sendJsonError(request, 401, "unauthorized");
          return;
        }
        JsonObjectConst body = json.as<JsonObjectConst>();
        if (body.isNull() || !body["title"].is<const char*>()) {
          sendJsonError(request, 400, "title required");
          return;
        }
        Command command;
        command.type = Command::Type::notify;
        serializeJson(json, command.json);
        enqueueOrFail(request, std::move(command));
      });
  notify_handler->setMethod(HTTP_POST);
  server_.addHandler(notify_handler);

  server_.on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", log_.snapshotText());
  });

  server_.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!authorized(request)) {
      sendJsonError(request, 401, "unauthorized");
      return;
    }
    Command command;
    command.type = Command::Type::reboot;
    enqueueOrFail(request, std::move(command));
  });

  server_.on(
      "/api/ota", HTTP_POST,
      [this](AsyncWebServerRequest* request) {
        if (!authorized(request)) {
          sendJsonError(request, 401, "unauthorized");
          return;
        }
        ota_.handleRequest(request);
      },
      [this](AsyncWebServerRequest* request, const String& filename, std::size_t index,
             std::uint8_t* data, std::size_t length, bool final) {
        if (!authorized(request)) {
          return;  // completion handler reports the 401
        }
        ota_.handleUpload(request, filename, index, data, length, final);
      });

  server_.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!authorized(request)) {
      sendJsonError(request, 401, "unauthorized");
      return;
    }
    const int result = WiFi.scanComplete();
    if (result == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true);
      request->send(200, kJsonContentType, "{\"status\":\"started\"}");
      return;
    }
    if (result == WIFI_SCAN_RUNNING) {
      request->send(200, kJsonContentType, "{\"status\":\"scanning\"}");
      return;
    }
    JsonDocument document;
    document["status"] = "done";
    JsonArray networks = document["networks"].to<JsonArray>();
    for (int index = 0; index < result; ++index) {
      JsonObject network = networks.add<JsonObject>();
      network["ssid"] = WiFi.SSID(index);
      network["rssi"] = WiFi.RSSI(index);
      network["secure"] = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    String body;
    serializeJson(document, body);
    request->send(200, kJsonContentType, body);
  });

  auto* wifi_handler = new AsyncCallbackJsonWebHandler(
      "/api/wifi", [this](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!authorized(request)) {
          sendJsonError(request, 401, "unauthorized");
          return;
        }
        JsonObjectConst body = json.as<JsonObjectConst>();
        if (body.isNull() || !body["ssid"].is<const char*>()) {
          sendJsonError(request, 400, "ssid required");
          return;
        }
        Command command;
        command.type = Command::Type::wifi_join;
        command.id = body["ssid"].as<const char*>();
        command.secret = body["pass"].is<const char*>() ? body["pass"].as<const char*>() : "";
        if (!commands_.push(std::move(command))) {
          sendJsonError(request, 429, "busy");
          return;
        }
        request->send(200, kJsonContentType, "{\"ok\":true,\"rebooting\":true}");
      });
  wifi_handler->setMethod(HTTP_POST);
  server_.addHandler(wifi_handler);

  events_.onConnect([](AsyncEventSourceClient* client) {
    client->send("connected", "log", 0);
  });
  server_.addHandler(&events_);
}

namespace {

// Manifest shape emitted by tools/build_webui.py (Agent WEBUI).
[[nodiscard]] String assetEtag() {
  String etag = "\"";
  etag += webui::kWebuiBuildTag;
  etag += "\"";
  return etag;
}

[[nodiscard]] const webui::AssetManifestEntry* findAsset(const char* path) {
  for (std::size_t index = 0; index < webui::kWebuiAssetCount; ++index) {
    if (strcmp(webui::kWebuiAssets[index].path, path) == 0) {
      return &webui::kWebuiAssets[index];
    }
  }
  return nullptr;
}

void serveAsset(AsyncWebServerRequest* request, const webui::AssetManifestEntry& asset) {
  const String etag = assetEtag();
  if (request->hasHeader("If-None-Match") &&
      request->getHeader("If-None-Match")->value() == etag) {
    request->send(304);
    return;
  }
  AsyncWebServerResponse* response =
      request->beginResponse(200, asset.content_type, asset.data, asset.length);
  if (asset.gzip) {
    response->addHeader("Content-Encoding", "gzip");
  }
  response->addHeader("ETag", etag);
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

}  // namespace

void WebServer::registerAssets() {
  for (std::size_t index = 0; index < webui::kWebuiAssetCount; ++index) {
    const webui::AssetManifestEntry& asset = webui::kWebuiAssets[index];
    server_.on(asset.path, HTTP_GET,
               [&asset](AsyncWebServerRequest* request) { serveAsset(request, asset); });
  }
  const webui::AssetManifestEntry* index_asset = findAsset("/index.html");
  if (index_asset != nullptr) {
    server_.on("/", HTTP_GET, [index_asset](AsyncWebServerRequest* request) {
      serveAsset(request, *index_asset);
    });
  }
}

void WebServer::handleNotFound(AsyncWebServerRequest* request) {
  bool setup_mode = false;
  {
    const std::lock_guard<std::mutex> guard(state_.mutex);
    setup_mode = state_.setup_mode;
  }

  const String url = request->url();
  if (setup_mode && request->method() == HTTP_GET && !url.startsWith("/api/")) {
    // Captive portal: bounce every foreign hostname to the device root.
    const String ap_ip = WiFi.softAPIP().toString();
    if (request->host() != ap_ip) {
      request->redirect(String("http://") + ap_ip + "/");
      return;
    }
  }
  if (request->method() == HTTP_GET && !url.startsWith("/api/")) {
    // Hash-routed SPA fallback.
    const webui::AssetManifestEntry* index_asset = findAsset("/index.html");
    if (index_asset != nullptr) {
      serveAsset(request, *index_asset);
      return;
    }
  }
  sendJsonError(request, 404, "not found");
}

bool WebServer::hasEventClients() const noexcept {
  return const_cast<AsyncEventSource&>(events_).count() > 0;
}

void WebServer::sendLogLine(const char* line) { events_.send(line, "log", 0); }

void WebServer::sendStatus(const String& json) { events_.send(json.c_str(), "status", 0); }

void WebServer::sendFrame(const char* base64_payload) {
  events_.send(base64_payload, "frame", 0);
}

}  // namespace amg::flightwall::live
