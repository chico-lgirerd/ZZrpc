#include "presence.h"

#include <iostream>

namespace zzrpc {

namespace {
// needs an asset with this exact name uploaded under Rich Presence > Art Assets
constexpr const char* kFallbackImageKey = "vinyl";
} // namespace

void setNowPlaying(discordpp::Client& client, const NowPlaying& track) {
    discordpp::Activity activity;
    activity.SetType(discordpp::ActivityTypes::Listening);
    activity.SetName("zig-zag.fm"); // otherwise defaults to the app's own name
    activity.SetDetails(track.title);
    activity.SetState(track.paused ? track.artist + " (paused)" : track.artist);

    // dropping timestamps entirely freezes the elapsed bar instead of counting through the pause
    if (!track.paused) {
        discordpp::ActivityTimestamps timestamps;
        timestamps.SetStart(static_cast<uint64_t>(track.startTimestampMs));
        if (track.durationSeconds) {
            timestamps.SetEnd(static_cast<uint64_t>(track.startTimestampMs + *track.durationSeconds * 1000));
        }
        activity.SetTimestamps(std::move(timestamps));
    }

    discordpp::ActivityAssets assets;
    // SetLargeImage takes a real url directly, no need for SetLargeUrl (that's just a click link)
    assets.SetLargeImage(track.imageUrl.value_or(kFallbackImageKey));
    assets.SetLargeText(track.album.value_or(track.title));
    activity.SetAssets(std::move(assets));

    client.UpdateRichPresence(std::move(activity), [](discordpp::ClientResult result) {
      if (!result.Successful()) {
          std::cerr << "[discord] UpdateRichPresence failed: " << result.ToString() << "\n";
      }
    });
}

void clearPresence(discordpp::Client& client) {
    client.ClearRichPresence();
}

} // namespace zzrpc
