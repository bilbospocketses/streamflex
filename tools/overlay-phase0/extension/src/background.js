// CPTK Overlay Probe 0.0.1, the Phase 0 measurement probe: relays the content script's reports to the native host.
// Only tabs a CPTK token activated are relayed. Activation lives in storage.session, so it ends with the browser.
const api = globalThis.browser ?? globalThis.chrome;
const PROBE_ORIGIN = "http://127.0.0.1:8765";
const TOKEN = /^[0-9a-f]{32}$/;
let port = null;

// Named keys (ArrowUp, Enter, MediaPlayPause, F5) are reported by value; a typed character never is, except on the
// kit's own probe pages. content.js applies the same rule first; this is the second line of defense. An IME's
// "Process" key and a dead or unidentified key keep the physical key's code, so they are redacted too (only
// content.js sees isComposing).
const HIDDEN_KEYS = new Set(["Process", "Unidentified", "Dead"]);

function redactKey(data, origin) {
  if (origin === PROBE_ORIGIN) return data;
  const named = typeof data.key === "string" && /^[A-Za-z][A-Za-z0-9]+$/.test(data.key) && !HIDDEN_KEYS.has(data.key);
  if (named) return data;
  return { ...data, key: "char", code: null };
}

function activeKey(tabId) {
  return `active:${tabId}`;
}

function tabOf(sender) {
  return sender.tab ? sender.tab.id : null;
}

async function isActive(tabId) {
  if (tabId == null) return false;
  const key = activeKey(tabId);
  const stored = await api.storage.session.get(key);
  return stored[key] === true;
}

// Records the host's state for this browser session and tells every active tab, so its box shows it.
async function tellHost(status, failed) {
  await api.storage.session.set({ hostError: failed ? status : null });
  const stored = await api.storage.session.get(null);
  for (const key of Object.keys(stored)) {
    if (!key.startsWith("active:")) continue;
    const tabId = Number(key.slice("active:".length));
    api.tabs.sendMessage(tabId, { type: "host", status }).catch((err) => {
      console.warn("CPTK Overlay Probe: could not tell a tab the host state:", tabId, err);
    });
  }
}

function hostPort() {
  if (port) return port;
  const opened = api.runtime.connectNative("app.cptk.overlay");
  let acked = false;
  opened.onDisconnect.addListener(() => {
    const err = opened.error || api.runtime.lastError;
    if (port === opened) port = null;
    tellHost((err && err.message) || "host exited", true).catch((e) => console.error("CPTK Overlay Probe:", e));
  });
  opened.onMessage.addListener((msg) => {
    if (acked || !msg || msg.type !== "ack") return;
    acked = true;
    tellHost(`connected (pid ${msg.pid})`, false).catch((e) => console.error("CPTK Overlay Probe:", e));
  });
  port = opened;
  return opened;
}

async function hostError() {
  const stored = await api.storage.session.get("hostError");
  return stored.hostError ?? null;
}

async function tabStatus(msg, sender) {
  const active = await isActive(tabOf(sender));
  return { active, hostError: active ? await hostError() : null };
}

async function activate(msg, sender) {
  const tabId = tabOf(sender);
  if (tabId == null || typeof msg.token !== "string" || !TOKEN.test(msg.token)) return { active: false, hostError: null };
  await api.storage.session.set({ [activeKey(tabId)]: true });
  return tabStatus(msg, sender);
}

async function relay(msg, sender) {
  const t = Date.now();
  const tabId = tabOf(sender);
  if (!(await isActive(tabId))) return { ok: false, error: "tab not active" };
  const origin = sender.url ? new URL(sender.url).origin : null;
  let data = { ...msg.data };
  if (msg.event === "key") data = redactKey(data, origin);
  // Read at relay time: a fresh profile's first page can run before any stored answer exists.
  if (msg.event === "start") data.allUrls = await api.permissions.contains({ origins: ["<all_urls>"] });
  try {
    hostPort().postMessage({ type: "report", event: msg.event, data, tab: tabId, frame: sender.frameId ?? null, t });
  } catch (err) {
    const reason = String((err && err.message) || err);
    await tellHost(reason, true);
    return { ok: false, error: reason };
  }
  return { ok: true, hostError: await hostError() };
}

const HANDLERS = new Map([
  ["activate", activate],
  ["status", tabStatus],
  ["report", relay],
]);

api.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  const handler = msg ? HANDLERS.get(msg.type) : undefined;
  if (!handler) return false;
  handler(msg, sender).then(sendResponse, (err) => sendResponse({ ok: false, error: String(err) }));
  return true;
});

api.tabs.onRemoved.addListener((tabId) => {
  api.storage.session.remove(activeKey(tabId)).catch((err) => console.error("CPTK Overlay Probe:", err));
});
