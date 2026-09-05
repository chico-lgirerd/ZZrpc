#include "server.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

namespace zzrpc {

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

std::thread startServer(int port, UpdateQueue& queue) {
    return std::thread([port, &queue]() {
      httplib::Server server;

      server.Post("/now-playing", [&queue](const httplib::Request& req, httplib::Response& res) {
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
        update.title = body.at("title").get<std::string>();
        update.artist = body.at("artist").get<std::string>();
        update.startTimestampMs = body.at("startTimestamp").get<int64_t>();
        if (body.contains("album") && !body.at("album").is_null()) {
            update.album = body.at("album").get<std::string>();
        }
        if (body.contains("imageUrl") && !body.at("imageUrl").is_null()) {
            update.imageUrl = body.at("imageUrl").get<std::string>();
        }
        if (body.contains("durationSeconds") && !body.at("durationSeconds").is_null()) {
            update.durationSeconds = body.at("durationSeconds").get<int64_t>();
        }

        queue.push(std::move(update));
        res.status = 204;
      });

      std::cout << "[server] listening on http://localhost:" << port << "\n";
      server.listen("0.0.0.0", port);
    });
}

} // namespace zzrpc
