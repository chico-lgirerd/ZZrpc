# legacy-node-bridge

Earlier implementation using Discord's classic local-IPC Rich Presence (`@xhayper/discord-rpc`). Simpler than `native/` — no OAuth, just needs Discord desktop running — but can't show per-track cover art; external image URLs are silently dropped on this surface, so it always shows a static fallback image instead.

Kept for reference. Use `native/` unless you specifically want to avoid the OAuth flow and don't care about cover art.

## Setup

### 1. Create a Discord application

- Go to https://discord.com/developers/applications, create a new application
- Copy the **Application (Client) ID** from General Information
- Under Rich Presence → Art Assets, upload an image and name its key `vinyl` (used as the fallback cover)

### 2. Configure and run

```sh
npm install
cp ../.env.example ../.env   # then edit .env, paste your Client ID
npm start
```

You should see `[discord] connected as <your username>` — requires the Discord desktop app running.

### 3. Install the userscript

Same as `native/` — see the main README's step 6. It's the same script, same port, works against either backend.

### 4. Test it

Open zig-zag.fm and start listening. Terminal logs `[server] now playing: <title> — <artist>`; Discord shows the activity with the static `vinyl` image. Idle for 5 minutes clears it.

## Troubleshooting

- **Won't connect to Discord** — make sure Discord desktop (not just the web client) is running.
- **Userscript does nothing** — check the zig-zag.fm tab's devtools console for `[zzrpc]` warnings, confirm you're logged in.
- **Cover art doesn't show** — expected, see the note in `discord.js`.
