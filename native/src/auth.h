#pragma once

#include <cstdint>
#include <functional>
#include <discordpp.h>

namespace zzrpc {

// loads cached token or does the oauth dance, either way onReady() fires once connected
void ensureAuthorized(discordpp::Client& client, uint64_t clientId, std::function<void()> onReady);

} // namespace zzrpc
