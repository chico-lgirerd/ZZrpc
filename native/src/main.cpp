#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <discordpp.h>

#include "auth.h"
#include "presence.h"
#include "server.h"

namespace {

constexpr auto kIdleClearAfter = std::chrono::minutes(5);
constexpr auto kLoopInterval = std::chrono::milliseconds(16);

} // namespace

int main() {
    const char* clientIdEnv = std::getenv("DISCORD_CLIENT_ID");
    if (!clientIdEnv) {
        std::cerr << "Missing DISCORD_CLIENT_ID in the environment — see README.md "
                     "(e.g. `set -a; source ../.env; set +a` before running this binary)\n";
        return 1;
    }
    uint64_t clientId = std::stoull(clientIdEnv);

    int port = 4848;
    if (const char* portEnv = std::getenv("PORT")) {
        port = std::stoi(portEnv);
    }

    discordpp::Client client;

    std::atomic<bool> needsResend{false}; // force one resend after (re)connect, dedupe would otherwise skip it
    std::atomic<bool> ready{false}; // UpdateRichPresence errors out if called before this
    client.SetStatusChangedCallback(
      [&needsResend, &ready](discordpp::Client::Status status, discordpp::Client::Error error,
                              int32_t errorDetail) {
        std::cout << "[discord] status: " << discordpp::Client::StatusToString(status) << "\n";
        if (status == discordpp::Client::Status::Ready) {
            ready = true;
            needsResend = true;
        } else {
            ready = false;
            if (error != discordpp::Client::Error::None) {
                std::cerr << "[discord] error: " << discordpp::Client::ErrorToString(error)
                           << " (detail " << errorDetail << ")\n";
            }
        }
      });

    zzrpc::UpdateQueue queue;
    std::thread serverThread;
    bool serverStarted = false;

    std::optional<int64_t> lastTrackId;
    std::optional<int64_t> lastStartTimestampMs;
    auto lastActivityAt = std::chrono::steady_clock::now();
    bool cleared = true;

    zzrpc::ensureAuthorized(client, clientId, [&]() {
      if (!serverStarted) {
          serverStarted = true;
          serverThread = zzrpc::startServer(port, queue);
      }
    });

    while (true) {
        discordpp::RunCallbacks();

        while (ready) {
            auto update = queue.tryPop();
            if (!update) break;
            lastActivityAt = std::chrono::steady_clock::now();
            cleared = false;

            bool forceResend = needsResend.exchange(false);
            bool unchanged = lastTrackId == update->trackId &&
              lastStartTimestampMs == update->startTimestampMs;

            if (unchanged && !forceResend) {
                continue;
            }

            lastTrackId = update->trackId;
            lastStartTimestampMs = update->startTimestampMs;

            std::cout << "[server] now playing: " << update->title << " — " << update->artist
                       << (forceResend ? " (forced resend after reconnect)" : "") << "\n";

            zzrpc::setNowPlaying(client, {
                                          update->title,
                                          update->artist,
                                          update->album,
                                          update->imageUrl,
                                          update->durationSeconds,
                                          update->startTimestampMs,
                                        });
        }

        // nothing posted in a while, probably tab closed or paused
        if (!cleared && std::chrono::steady_clock::now() - lastActivityAt > kIdleClearAfter) {
            std::cout << "[server] idle timeout, clearing activity\n";
            zzrpc::clearPresence(client);
            lastTrackId.reset();
            lastStartTimestampMs.reset();
            cleared = true;
        }

        std::this_thread::sleep_for(kLoopInterval);
    }
}
