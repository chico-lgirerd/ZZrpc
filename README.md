# [ZZrpc](https://chico-lgirerd.github.io/ZZrpc/)

Shows your currently-playing [zig-zag.fm](https://www.zig-zag.fm) track as Discord Rich Presence — title, artist, album, cover art, and a live elapsed/remaining timer, updating in real time as you browse and listen.

## How it works

```
zig-zag.fm tab (Tampermonkey userscript)
   │  watches the page URL for the current track, resolves title/artist/
   │  album/art via zig-zag's own API, tracks seeks via the site's own
   │  progress bar
   │  POST → http://localhost:4848/now-playing
   ▼
native/ (C++, links Discord's Social SDK directly)
   │  authenticates once via Discord OAuth2, then pushes the track to
   │  Discord as Rich Presence whenever it changes — timer freezes and
   │  the state shows "(paused)" while zig-zag.fm playback is paused
   ▼
Discord
```

zig-zag.fm has no public API for this — the userscript talks to the same internal endpoints the site's own frontend uses.

## Setup

### 1. Register a Discord application for Social SDK access

> **Only needed once, by whoever builds/distributes ZZrpc.**
> I'm hoping to find a way around this before public release :) 
> The client ID gets baked into the binary — end users installing a prebuilt release
> just approve a Discord login prompt against that existing app, using
> their own account. They never register anything themselves. Building
> from source? You need your own registered app for local testing.

- Create a Developer Team at https://discord.com/developers/teams if you don't have one
- Create an application at https://discord.com/developers/applications, assigned to that team
- OAuth2 tab: add redirect URL `http://127.0.0.1/callback`, enable **Public Client**
- Sidebar → **Discord Social SDK → Getting Started**, fill out the form, Submit — this unlocks **Downloads**

### 2. Download and vendor the SDK

Too large to commit, and Discord doesn't expose a stable direct-download URL — it's gated behind your logged-in Downloads page, so this step is manual on every fresh clone.

```sh
cd native
./fetch-sdk.sh   # checks if it's already there, opens the Downloads page if not
```

- Sidebar → **Downloads**, grab the latest **C++** package
- Extract it into `native/discord_social_sdk/` so you end up with:
  - `native/discord_social_sdk/include/discordpp.h`
  - `native/discord_social_sdk/lib/release/libdiscord_partner_sdk.so` (Linux) or
    `native/discord_social_sdk/lib/release/libdiscord_partner_sdk.dylib` (macOS) — the
    C++ package ships both; CMake picks the right one for your platform automatically

### 3. Configure

```sh
cp .env.example .env
```

Edit `.env`, set `DISCORD_CLIENT_ID` to your application's client ID.

### 4. Build

Needs a C++20 compiler and CMake ≥ 3.16 — `cpp-httplib` and `nlohmann/json` are fetched automatically.

```sh
cd native
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

(`-j` with no number lets the generator pick a sensible parallelism; on Linux
you can pin it with `-j"$(nproc)"`, on macOS `-j"$(sysctl -n hw.ncpu)"`.)

The binary and its bundled SDK `.so` land together in `build/dist/` (not
`build/` itself) — that folder is self-contained and relocatable, so it's
also what gets zipped up for distribution.

### 5. Run

```sh
cd native
set -a; source ../.env; set +a
./build/dist/zzrpc
```

First run opens a Discord authorization prompt in your browser — approve it once. The token is cached at `~/.config/zzrpc/token.txt` and reused (refreshed automatically before it expires) on later runs.

### 6. Install the userscript

- Install [Tampermonkey](https://www.tampermonkey.net/) if you don't have it
- Open `userscript/zigzag-rpc.js`, copy its contents into a new Tampermonkey script, save

### 7. Test it

Open zig-zag.fm and start listening. The terminal should log `[server] now playing: <title> — <artist>`, and your Discord profile should show the activity within a couple seconds — including real cover art. Seeking within a track, switching tracks, and killing/relaunching Discord should all update or recover correctly without needing a restart.

## Presence card fields

Top to bottom, mapped to where each one comes from:

| Shown as | Set by |
|---|---|
| "Listening to zig-zag.fm" | `Activity::SetName` |
| Large image | `ActivityAssets::SetLargeImage` (track's cover art, or a static fallback) |
| Track title | `Activity::SetDetails` |
| Artist (appends "(paused)" when paused) | `Activity::SetState` |
| Elapsed / remaining bar (frozen while paused) | `ActivityTimestamps::SetStart`/`SetEnd` — omitted entirely when paused |
| Album (hover on the image) | `ActivityAssets::SetLargeText` |

## Troubleshooting

- **Link errors about undefined `discordpp::` symbols** — `discordpp.h` is a single-header library; its method bodies only get compiled into whichever `.cpp` defines `DISCORDPP_IMPLEMENTATION` before including it (that's `native/src/discordpp_impl.cpp`). Make sure it's still in `CMakeLists.txt`'s executable sources.
- **A cached token gets rejected (`UnexpectedClose`, detail `4004`)** — Discord's gateway "authentication failed" code. If it recurs, delete `~/.config/zzrpc/token.txt` to force a fresh login.
- **Cover art not showing** — use `ActivityAssets::SetLargeImage` (accepts a key or a real URL), not `SetLargeUrl`, which is an unrelated click-through link on the image.
- **Userscript does nothing** — check the zig-zag.fm tab's devtools console for `[zzrpc]` warnings, and confirm you're logged into zig-zag.fm (the script reads your session token from `localStorage`).

## Public release / App Verification

`docs/terms.html` and `docs/privacy.html` are ZZrpc's Terms of Service and
Privacy Policy, written for Discord's **App Verification** process
(required once the app scales past whatever threshold applies to your
app — check the App Verification tab on your application in the
Developer Portal for the live checklist). Host them via GitHub Pages
(`Settings → Pages → source: main /docs`) and paste the resulting URLs
into that form, alongside the separate identity-verification step
Discord runs through Stripe.

## Legacy version

`legacy-node-bridge/` is an earlier Node implementation using Discord's classic local-IPC Rich Presence instead of the Social SDK — simpler (no OAuth, just needs Discord desktop open) but can't show per-track cover art at all, since that surface silently drops external image URLs. Kept for reference; not the recommended path. Its own setup instructions are in `legacy-node-bridge/README.md`.
