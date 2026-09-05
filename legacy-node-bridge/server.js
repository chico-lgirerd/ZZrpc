import { config } from "dotenv";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import http from "node:http";

const __dirname = dirname(fileURLToPath(import.meta.url));
config({ path: join(__dirname, "..", ".env") });
import { connect, setNowPlaying, clear, isConnected, consumeNeedsResend } from "./discord.js";

process.on("uncaughtException", (err) => console.error("[server] uncaught exception (kept alive):", err));
process.on("unhandledRejection", (err) => console.error("[server] unhandled rejection (kept alive):", err));

const PORT = process.env.PORT || 4848;
const DEBUG = process.env.DEBUG === "1";
const IDLE_CLEAR_MS = 5 * 60 * 1000;

let lastTrackId = null;
let lastStartTimestamp = null;
let idleTimer = null;

function resetIdleTimer() {
  clearTimeout(idleTimer);
  idleTimer = setTimeout(() => {
    console.log("[server] idle timeout, clearing activity");
    lastTrackId = null;
    lastStartTimestamp = null;
    clear().catch((err) => console.error("[discord] clear failed:", err));
  }, IDLE_CLEAR_MS);
}

const server = http.createServer((req, res) => {
  if (req.method !== "POST" || req.url !== "/now-playing") {
    res.writeHead(404).end();
    return;
  }

  let body = "";
  req.on("data", (chunk) => (body += chunk));
  req.on("end", async () => {
    let payload;
    try {
      payload = JSON.parse(body);
    } catch {
      res.writeHead(400).end("invalid json");
      return;
    }

    const { trackId, title, artist, imageUrl, durationSeconds, startTimestamp } = payload;
    if (!trackId || !title || !artist) {
      res.writeHead(400).end("missing trackId/title/artist");
      return;
    }

    resetIdleTimer();

    const forceResend = consumeNeedsResend(); // bypass dedupe once right after a reconnect
    const unchanged = trackId === lastTrackId && startTimestamp === lastStartTimestamp;
    if (unchanged && !forceResend) {
      if (DEBUG) console.log(`[server] heartbeat (unchanged): ${title}`);
      res.writeHead(204).end();
      return;
    }
    lastTrackId = trackId;
    lastStartTimestamp = startTimestamp;

    console.log(`[server] now playing: ${title} — ${artist}${forceResend ? " (forced resend after reconnect)" : ""}`);

    if (!isConnected()) {
      console.log("[server] discord not connected, skipping setActivity");
      res.writeHead(202).end();
      return;
    }

    try {
      await setNowPlaying({ title, artist, imageUrl, durationSeconds, startTimestamp });
      res.writeHead(204).end();
    } catch (err) {
      console.error("[discord] setActivity failed:", err);
      res.writeHead(500).end();
    }
  });
});

const clientId = process.env.DISCORD_CLIENT_ID;
if (!clientId) {
  console.error("Missing DISCORD_CLIENT_ID in .env — see README.md");
  process.exit(1);
}

connect(clientId).catch((err) => {
  console.error("[discord] failed to connect (is Discord desktop running?):", err);
});

server.listen(PORT, () => {
  console.log(`[server] listening on http://localhost:${PORT}`);
});
