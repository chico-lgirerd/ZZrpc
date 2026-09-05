#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <discordpp.h>

namespace zzrpc {

struct NowPlaying {
    std::string title;
    std::string artist;
    std::optional<std::string> album;
    std::optional<std::string> imageUrl;
    std::optional<int64_t> durationSeconds;
    int64_t startTimestampMs;
};

void setNowPlaying(discordpp::Client& client, const NowPlaying& track);
void clearPresence(discordpp::Client& client);

} // namespace zzrpc
