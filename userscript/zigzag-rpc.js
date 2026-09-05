// ==UserScript==
// @name         zig-zag.fm -> Discord RPC
// @namespace    zzrpc
// @version      1.0
// @description  Reports the currently playing zig-zag.fm track to a local Discord RPC bridge
// @match        https://www.zig-zag.fm/*
// @grant        GM_xmlhttpRequest
// @connect      localhost
// @connect      api.zig-zag.fm
// ==/UserScript==

(function () {
  "use strict";

  const BRIDGE_URL = "http://localhost:4848/now-playing";
  const POLL_MS = 1500;

  const URL_RE = /(-?\d+)~(-?\d+)\/track~(\d+)/;

  let lastTrackId = null;

  function getAuthToken() {
    try {
      const raw = localStorage.getItem("auth-storage");
      if (!raw) return null;
      return JSON.parse(raw)?.state?.authToken ?? null;
    } catch {
      return null;
    }
  }

  function gmGet(url, token) {
    return new Promise((resolve, reject) => {
      GM_xmlhttpRequest({
        method: "GET",
        url,
        headers: token ? { token } : {},
        onload: (res) => resolve(res),
        onerror: (err) => reject(err),
      });
    });
  }

  function gmPost(url, body) {
    return new Promise((resolve, reject) => {
      GM_xmlhttpRequest({
        method: "POST",
        url,
        headers: { "Content-Type": "application/json" },
        data: JSON.stringify(body),
        onload: (res) => resolve(res),
        onerror: (err) => reject(err),
      });
    });
  }

  // reads the site's own progress bar, no api for this
  function getPlaybackPositionSeconds() {
    for (const el of document.querySelectorAll('[role="slider"]')) {
      const label = (el.getAttribute("aria-label") || "").toLowerCase();
      const valuetext = el.getAttribute("aria-valuetext") || "";
      if (label.includes("playback position") || /^\d+:\d{2} of \d+:\d{2}$/.test(valuetext)) {
        const v = parseFloat(el.getAttribute("aria-valuenow"));
        if (Number.isFinite(v)) return v;
      }
    }
    return null;
  }

  let lastMeta = null;
  let lastSentStart = null;

  const SEEK_THRESHOLD_MS = 4000; // below this it's just normal drift, not an actual seek

  async function checkAndReport() {
    const match = location.pathname.match(URL_RE);
    if (!match) return;

    const [, x, y, trackIdStr] = match;
    const trackId = Number(trackIdStr);

    if (trackId !== lastTrackId) {
      const token = getAuthToken();
      if (!token) {
        console.warn("[zzrpc] no auth token found (not logged in?)");
        return;
      }

      try {
        const res = await gmGet(
          `https://api.zig-zag.fm/v2/releases/by-coordinates?xy_x=${x}&xy_y=${y}&map_id=39`,
          token
        );
        if (res.status !== 200) {
          console.warn(`[zzrpc] release lookup failed: HTTP ${res.status}`, res.responseText);
          return;
        }

        const release = JSON.parse(res.responseText);
        const track = release.tracks?.find((t) => t.id === trackId);
        if (!track) {
          console.warn(`[zzrpc] track ${trackId} not found in release`, release);
          return;
        }

        lastTrackId = trackId;
        lastMeta = {
          title: track.track_name || release.release_name,
          artist: (release.main_artist || []).join(", ") || "Unknown artist",
          album: release.release_name,
          imageUrl: release.image_url,
          durationSeconds: track.duration_seconds,
        };
        lastSentStart = null;
      } catch (err) {
        console.error("[zzrpc] failed to report track:", err);
        return;
      }
    }

    if (!lastMeta) return;

    const positionSeconds = getPlaybackPositionSeconds();
    const now = Date.now();
    const computedStart =
      positionSeconds != null ? now - positionSeconds * 1000 : lastSentStart ?? now;

    const isRealChange =
      lastSentStart == null || Math.abs(computedStart - lastSentStart) >= SEEK_THRESHOLD_MS;

    const startTimestamp = isRealChange ? computedStart : lastSentStart;
    if (isRealChange) lastSentStart = startTimestamp;

    gmPost(BRIDGE_URL, { trackId, ...lastMeta, startTimestamp }).catch((err) =>
      console.error("[zzrpc] send failed:", err)
    );
  }

  setInterval(checkAndReport, POLL_MS);
  checkAndReport();
})();
