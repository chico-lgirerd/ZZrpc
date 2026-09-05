#include "auth.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

namespace zzrpc {
namespace {

struct StoredToken {
    std::string accessToken;
    std::string refreshToken;
    int64_t expiresAt = 0;
    // don't hardcode this on reuse, discord decides User vs Bearer and it broke auth once
    discordpp::AuthorizationTokenType tokenType = discordpp::AuthorizationTokenType::User;
};

int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string tokenFilePath() {
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    std::string base = xdgConfig && *xdgConfig
      ? std::string(xdgConfig)
      : std::string(std::getenv("HOME") ? std::getenv("HOME") : ".") + "/.config";
    return base + "/zzrpc/token.txt";
}

std::optional<StoredToken> loadToken() {
    std::ifstream in(tokenFilePath());
    if (!in) return std::nullopt;

    StoredToken token;
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (key == "access_token") token.accessToken = value;
        else if (key == "refresh_token") token.refreshToken = value;
        else if (key == "expires_at") token.expiresAt = std::stoll(value);
        else if (key == "token_type")
            token.tokenType = static_cast<discordpp::AuthorizationTokenType>(std::stoi(value));
    }

    if (token.accessToken.empty() || token.refreshToken.empty()) return std::nullopt;
    return token;
}

void saveToken(const StoredToken& token) {
    std::string path = tokenFilePath();
    std::string dir = path.substr(0, path.find_last_of('/'));
    std::system(("mkdir -p '" + dir + "'").c_str());

    std::ofstream out(path, std::ios::trunc);
    out << "access_token=" << token.accessToken << "\n";
    out << "refresh_token=" << token.refreshToken << "\n";
    out << "expires_at=" << token.expiresAt << "\n";
    out << "token_type=" << static_cast<int>(token.tokenType) << "\n";
}

void runFullAuthFlow(discordpp::Client& client, uint64_t clientId, std::function<void()> onReady) {
    auto verifier =
      std::make_shared<discordpp::AuthorizationCodeVerifier>(client.CreateAuthorizationCodeVerifier());

    discordpp::AuthorizationArgs args;
    args.SetClientId(clientId);
    args.SetScopes(discordpp::Client::GetDefaultPresenceScopes());
    args.SetCodeChallenge(verifier->Challenge());

    std::cout << "[auth] opening Discord authorization prompt (browser/overlay)...\n";

    client.Authorize(
      args,
      [&client, clientId, verifier, onReady](discordpp::ClientResult result, std::string code,
                                             std::string redirectUri) {
        if (!result.Successful()) {
            std::cerr << "[auth] Authorize failed: " << result.ToString() << "\n";
            return;
        }

        client.GetToken(
          clientId, code, verifier->Verifier(), redirectUri,
          [&client, onReady](discordpp::ClientResult result, std::string accessToken,
                              std::string refreshToken, discordpp::AuthorizationTokenType tokenType,
                              int32_t expiresIn, std::string) {
            if (!result.Successful()) {
                std::cerr << "[auth] GetToken failed: " << result.ToString() << "\n";
                return;
            }

            saveToken({accessToken, refreshToken, nowSeconds() + expiresIn, tokenType});

            client.UpdateToken(tokenType, accessToken, [&client, onReady](discordpp::ClientResult result) {
              if (!result.Successful()) {
                  std::cerr << "[auth] UpdateToken failed: " << result.ToString() << "\n";
                  return;
              }
              std::cout << "[auth] authorized, connecting...\n";
              client.Connect();
              onReady();
            });
          });
      });
}

void refreshAndConnect(discordpp::Client& client, uint64_t clientId, const StoredToken& stored,
                        std::function<void()> onReady) {
    client.RefreshToken(
      clientId, stored.refreshToken,
      [&client, clientId, onReady](discordpp::ClientResult result, std::string accessToken,
                                    std::string refreshToken, discordpp::AuthorizationTokenType tokenType,
                                    int32_t expiresIn, std::string) {
        if (!result.Successful()) {
            std::cerr << "[auth] token refresh failed (" << result.ToString()
                       << "), falling back to a fresh login\n";
            runFullAuthFlow(client, clientId, onReady);
            return;
        }

        saveToken({accessToken, refreshToken, nowSeconds() + expiresIn, tokenType});

        client.UpdateToken(tokenType, accessToken, [&client, onReady](discordpp::ClientResult result) {
          if (!result.Successful()) {
              std::cerr << "[auth] UpdateToken (post-refresh) failed: " << result.ToString() << "\n";
              return;
          }
          std::cout << "[auth] refreshed token, connecting...\n";
          client.Connect();
          onReady();
        });
      });
}

} // namespace

void ensureAuthorized(discordpp::Client& client, uint64_t clientId, std::function<void()> onReady) {
    auto stored = loadToken();
    if (!stored) {
        runFullAuthFlow(client, clientId, onReady);
        return;
    }

    // refresh a day early instead of waiting for it to actually expire
    const int64_t refreshMarginSeconds = 24 * 60 * 60;
    if (stored->expiresAt - nowSeconds() < refreshMarginSeconds) {
        refreshAndConnect(client, clientId, *stored, onReady);
        return;
    }

    client.UpdateToken(stored->tokenType, stored->accessToken,
                        [&client, clientId, stored, onReady](discordpp::ClientResult result) {
      if (!result.Successful()) {
          std::cerr << "[auth] cached token rejected (" << result.ToString()
                     << "), refreshing\n";
          refreshAndConnect(client, clientId, *stored, onReady);
          return;
      }
      std::cout << "[auth] reusing cached token, connecting...\n";
      client.Connect();
      onReady();
    });
}

} // namespace zzrpc
