import { Client } from "@xhayper/discord-rpc";
import { ActivityType } from "discord-api-types/v10";

let client = null;
let connected = false;
let reconnectTimer = null;
let needsResend = false;
const RECONNECT_MS = 5000;

function scheduleReconnect() {
  if (reconnectTimer) return;
  reconnectTimer = setInterval(() => {
    console.log("[discord] attempting to reconnect...");
    client.login().catch(() => {});
  }, RECONNECT_MS);
}

function stopReconnecting() {
  clearInterval(reconnectTimer);
  reconnectTimer = null;
}

export async function connect(clientId) {
  client = new Client({ clientId });

  client.on("ready", () => {
    connected = true;
    needsResend = true;
    stopReconnecting();
    console.log(`[discord] connected as ${client.user?.username ?? "unknown"}`);
  });

  client.on("disconnected", () => {
    connected = false;
    console.log(`[discord] disconnected, retrying every ${RECONNECT_MS / 1000}s (is the Discord desktop app running?)`);
    scheduleReconnect();
  });

  // no listener here = node kills the whole process on an error event
  client.on("error", (err) => {
    console.error("[discord] client error (kept process alive):", err);
  });

  try {
    await client.login();
  } catch (err) {
    console.error("[discord] initial login failed, retrying:", err.message ?? err);
    scheduleReconnect();
  }
}

export function isConnected() {
  return connected;
}

export function consumeNeedsResend() {
  const v = needsResend;
  needsResend = false;
  return v;
}

// real cover art doesn't work on this rpc surface, tried it, discord just ignores the url
const STATIC_IMAGE_KEY = "vinyl";

export async function setNowPlaying({ title, artist, imageUrl, durationSeconds, startTimestamp }) {
  if (!connected || !client?.user) return;

  const start = startTimestamp ?? Date.now();

  console.log(
    `[discord] setActivity (image not sent, static key used) trackImage=${imageUrl ?? "(none)"} durationSeconds=${durationSeconds ?? "(none)"}`
  );

  await client.user.setActivity({
    type: ActivityType.Listening,
    details: title.slice(0, 128),
    state: artist.slice(0, 128),
    startTimestamp: start,
    ...(durationSeconds ? { endTimestamp: start + durationSeconds * 1000 } : {}),
    largeImageKey: STATIC_IMAGE_KEY,
    largeImageText: title.slice(0, 128),
    instance: false,
  });
}

export async function clear() {
  if (!connected || !client?.user) return;
  await client.user.clearActivity();
}
