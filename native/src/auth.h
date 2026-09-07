#pragma once

#include <cstdint>
#include <functional>
#include <discordpp.h>

namespace zzrpc {

// loads cached token or does the oauth dance, either way onReady() fires once connected
//
// client is captured by reference in every pending SDK callback this triggers (Authorize,
// GetToken, RefreshToken, UpdateToken) — the caller must keep it alive until all of those have
// fired. Safe today because main() only ever passes a client that lives for the whole process.
void ensureAuthorized(discordpp::Client& client, uint64_t clientId, std::function<void()> onReady);

} // namespace zzrpc
