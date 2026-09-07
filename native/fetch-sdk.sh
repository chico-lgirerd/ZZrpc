#!/usr/bin/env bash
# checks for the vendored social sdk and opens the download page if it's missing
# can't be fully automated: discord gates the zip behind a logged-in dev portal page, no stable direct url
set -euo pipefail

SDK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/discord_social_sdk"

if [ -f "$SDK_DIR/include/discordpp.h" ]; then
  echo "Social SDK already present at $SDK_DIR"
  exit 0
fi

echo "Social SDK not found at $SDK_DIR"
echo
echo "Manual steps (Discord doesn't allow downloading this without being logged in):"
echo "  1. Open your application in the Discord Developer Portal"
echo "  2. Sidebar -> Downloads -> grab the latest C++ package"
echo "  3. Extract it so you end up with:"
echo "       $SDK_DIR/include/discordpp.h"
echo "       $SDK_DIR/lib/release/libdiscord_partner_sdk.so     (Linux)"
echo "       $SDK_DIR/lib/release/libdiscord_partner_sdk.dylib  (macOS)"
echo "     the C++ package ships both; CMake picks the right one automatically"
echo

URL="https://discord.com/developers/applications"
if command -v xdg-open >/dev/null 2>&1; then
  xdg-open "$URL" >/dev/null 2>&1 &
elif command -v open >/dev/null 2>&1; then
  open "$URL" >/dev/null 2>&1 &
fi
