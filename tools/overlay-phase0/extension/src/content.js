// CPTK Overlay Probe 0.0.1, the Phase 0 measurement probe. It does nothing unless a CPTK token in the URL fragment
// activated this tab.
(() => {
  "use strict";
  const api = globalThis.browser ?? globalThis.chrome;
  const { name: NAME, version: VERSION } = api.runtime.getManifest();
  const PROBE_ORIGIN = "http://127.0.0.1:8765";
  const TOKEN_IN_HASH = /(?:^#|&)cptk=([0-9a-f]{32})(?=&|$)/;
  const lines = [];
  let box = null;
  let hostShown = null;

  function show(text) {
    lines.push(text);
    if (lines.length > 8) lines.shift();
    if (!box && document.body) {
      box = document.createElement("div");
      box.style.cssText =
        "position:fixed;left:8px;bottom:8px;z-index:2147483647;background:#000c;color:#fff;" +
        "font:16px monospace;padding:8px;white-space:pre;pointer-events:none";
      document.body.appendChild(box);
    }
    if (box) box.textContent = `${NAME} ${VERSION}\n` + lines.join("\n");
  }

  function showHost(status) {
    if (!status || status === hostShown) return;
    hostShown = status;
    show(`host: ${status}`);
  }

  function reason(err) {
    return err && err.message ? err.message : String(err);
  }

  // Named keys (ArrowUp, Enter, MediaPlayPause, F5) are reported by value; a typed character never is, except on
  // the kit's own probe pages. background.js applies the same rule again before posting.
  function redactKey(data) {
    if (location.origin === PROBE_ORIGIN) return data;
    if (typeof data.key === "string" && /^[A-Za-z][A-Za-z0-9]+$/.test(data.key)) return data;
    return { ...data, key: "char", code: null };
  }

  function send(message) {
    return Promise.resolve(api.runtime.sendMessage(message));
  }

  function report(event, data) {
    const safe = event === "key" ? redactKey(data) : data;
    show(`${event} ${JSON.stringify(safe)}`);
    send({ type: "report", event, data: { ...safe, origin: location.origin, version: VERSION } })
      .then((reply) => {
        if (reply && reply.ok) showHost(reply.hostError);
        else show(`relay: ${reply ? reply.error : "no reply"}`);
      })
      .catch((err) => show(`relay: ${reason(err)}`));
  }

  async function onKey(e) {
    report("key", {
      key: e.key,
      code: e.code,
      trusted: e.isTrusted,
      activation: navigator.userActivation ? navigator.userActivation.isActive : null,
    });
    if (e.code === "KeyF") {
      try {
        await document.documentElement.requestFullscreen();
        report("fullscreen", { ok: true });
      } catch (err) {
        report("fullscreen", { ok: false, error: String(err && err.name) });
      }
    } else if (e.code === "KeyP") {
      const video = document.querySelector("video");
      if (!video) {
        report("play", { ok: false, error: "no video" });
        return;
      }
      video.muted = false;
      try {
        await video.play();
        report("play", { ok: true });
      } catch (err) {
        report("play", { ok: false, error: String(err && err.name) });
      }
    }
  }

  // Removes the token from the fragment only, on the same boundary the match uses, and keeps the rest of it. The
  // full URL is passed: a path-only one starting with "//" would parse as protocol-relative and throw.
  function stripToken() {
    let hash = location.hash.replace(TOKEN_IN_HASH, (found) => (found[0] === "#" ? "#" : ""));
    hash = hash.replace(/^#&/, "#");
    if (hash === "#") hash = "";
    const url = new URL(location.href);
    url.hash = hash;
    history.replaceState(null, "", url.href);
  }

  async function main() {
    const m = location.hash.match(TOKEN_IN_HASH);
    let state;
    if (m) {
      stripToken();
      state = await send({ type: "activate", token: m[1] });
    } else {
      state = await send({ type: "status" });
    }
    if (!state || !state.active) return;
    api.runtime.onMessage.addListener((msg) => {
      if (msg && msg.type === "host") showHost(msg.status);
    });
    show(`webdriver: ${navigator.webdriver}`);
    showHost(state.hostError);
    report("start", { webdriver: navigator.webdriver === true, tokenFromUrl: Boolean(m) });
    window.addEventListener("keydown", onKey, true);
    if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", () => show("page ready"));
    else show("page ready");
  }

  main().catch((err) => show(`error ${reason(err)}`));
})();
