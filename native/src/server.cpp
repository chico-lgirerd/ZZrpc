#include "server.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

namespace zzrpc {

namespace {
// matches Discord's rich presence field limits (see legacy-node-bridge/discord.js's .slice(0, 128))
constexpr size_t kMaxFieldLength = 128;

std::string truncateField(std::string value) {
    if (value.size() > kMaxFieldLength) value.resize(kMaxFieldLength);
    return value;
}

bool looksLikeHttpsUrl(const std::string& value) {
    return value.rfind("https://", 0) == 0;
}
} // namespace

void UpdateQueue::push(IncomingUpdate update) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(update));
}

std::optional<IncomingUpdate> UpdateQueue::tryPop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return std::nullopt;
    IncomingUpdate update = std::move(queue_.front());
    queue_.pop();
    return update;
}

ServerHandle startServer(int port, UpdateQueue& queue) {
    auto server = std::make_shared<httplib::Server>();

    // now-playing bodies are a handful of short fields; reject anything wildly oversized upfront
    server->set_payload_max_length(8 * 1024);

    server->Post("/now-playing", [&queue](const httplib::Request& req, httplib::Response& res) {
      nlohmann::json body;
      try {
          body = nlohmann::json::parse(req.body);
      } catch (const std::exception&) {
          res.status = 400;
          res.set_content("invalid json", "text/plain");
          return;
      }

      if (!body.contains("trackId") || !body.contains("title") || !body.contains("artist") ||
          !body.contains("startTimestamp")) {
          res.status = 400;
          res.set_content("missing trackId/title/artist/startTimestamp", "text/plain");
          return;
      }

      IncomingUpdate update;
      update.trackId = body.at("trackId").get<int64_t>();
      update.title = truncateField(body.at("title").get<std::string>());
      update.artist = truncateField(body.at("artist").get<std::string>());
      update.startTimestampMs = body.at("startTimestamp").get<int64_t>();
      if (body.contains("album") && !body.at("album").is_null()) {
          update.album = truncateField(body.at("album").get<std::string>());
      }
      if (body.contains("imageUrl") && !body.at("imageUrl").is_null()) {
          std::string imageUrl = body.at("imageUrl").get<std::string>();
          // only accept real https URLs — Discord's SetLargeImage also takes asset keys, but
          // nothing here should ever produce one of those, so anything else is malformed input
          if (looksLikeHttpsUrl(imageUrl)) {
              update.imageUrl = std::move(imageUrl);
          }
      }
      if (body.contains("durationSeconds") && !body.at("durationSeconds").is_null()) {
          update.durationSeconds = body.at("durationSeconds").get<int64_t>();
      }
      if (body.contains("paused") && !body.at("paused").is_null()) {
          update.paused = body.at("paused").get<bool>();
      }

      queue.push(std::move(update));
      res.status = 204;
    });

    std::thread thread([server, port]() {
      std::cout << "[server] listening on http://localhost:" << port << "\n";
      server->listen("127.0.0.1", port);
    });

    return ServerHandle{std::move(thread), [server]() { server->stop(); }};
}

} // namespace zzrpc
