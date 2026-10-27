// CPTK Overlay 0.0.1, the Phase 0 probe. It does nothing unless this browser profile was started with a CPTK token.
(() => {
  "use strict";
  const api = globalThis.browser ?? globalThis.chrome;
  const VERSION = api.runtime.getManifest().version;
  const lines = [];
  let box = null;

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
    if (box) box.textContent = `CPTK probe ${VERSION}\n` + lines.join("\n");
  }

  function report(event, data) {
    const message = { type: "report", event, data: { ...data, origin: location.origin, version: VERSION } };
    Promise.resolve(api.runtime.sendMessage(message)).catch(() => {});
    show(`${event} ${JSON.stringify(data)}`);
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

  async function main() {
    const m = location.hash.match(/(?:^#|&)cptk=([0-9a-f]{32})(?=&|$)/);
    if (m) {
      await api.storage.local.set({ cptkToken: m[1] });
      const cleaned = location.href.replace(/([#&])cptk=[0-9a-f]{32}&?/, "$1").replace(/[#&]$/, "");
      history.replaceState(history.state, "", cleaned);
    }
    const stored = await api.storage.local.get(["cptkToken", "cptkAllUrlsGranted"]);
    if (!stored.cptkToken) return;
    report("start", {
      webdriver: navigator.webdriver === true,
      tokenFromUrl: Boolean(m),
      allUrls: stored.cptkAllUrlsGranted ?? null,
    });
    window.addEventListener("keydown", onKey, true);
    document.addEventListener("DOMContentLoaded", () => show("page ready"));
  }

  main().catch((err) => show(`error ${err}`));
})();
