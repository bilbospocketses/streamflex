// CPTK Overlay 0.0.1, the Phase 0 probe: relays the content script's reports to the native host.
const api = globalThis.browser ?? globalThis.chrome;
let port = null;

function hostPort() {
  if (port) return port;
  port = api.runtime.connectNative("app.cptk.overlay");
  port.onDisconnect.addListener(() => {
    port = null;
  });
  port.onMessage.addListener(() => {});
  return port;
}

api.runtime.onMessage.addListener((msg, sender) => {
  if (!msg || msg.type !== "report") return;
  hostPort().postMessage({
    ...msg,
    tab: sender.tab ? sender.tab.id : null,
    frame: sender.frameId ?? null,
    t: Date.now(),
  });
});

// Recorded for the content script's "start" report; the background never opens the port on its own.
api.permissions.contains({ origins: ["<all_urls>"] }).then((granted) => {
  api.storage.local.set({ cptkAllUrlsGranted: granted });
});
