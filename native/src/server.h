#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>

namespace zzrpc {

struct IncomingUpdate {
    int64_t trackId;
    std::string title;
    std::string artist;
    std::optional<std::string> album;
    std::optional<std::string> imageUrl;
    std::optional<int64_t> durationSeconds;
    int64_t startTimestampMs;
};

// http thread pushes here, main loop pops, keeps the discord client single-threaded
class UpdateQueue {
public:
    void push(IncomingUpdate update);
    std::optional<IncomingUpdate> tryPop();

private:
    std::mutex mutex_;
    std::queue<IncomingUpdate> queue_;
};

std::thread startServer(int port, UpdateQueue& queue);

} // namespace zzrpc
