// Runs the built probe extension's scripts against a fake WebExtension API, one scenario per call.
// Usage: node extension_sim.js <built extension dir> <scenario>. Prints "ok <scenario>" and exits 0 on a pass.
// The fake API has no storage.local at all, so any use of it fails the scenario.
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const [dir, scenario] = process.argv.slice(2);
const CONTENT = fs.readFileSync(path.join(dir, "content.js"), "utf8");
const BACKGROUND = fs.readFileSync(path.join(dir, "background.js"), "utf8");
const TOKEN = "0123456789abcdef0123456789abcdef";
const NOT_FOUND = "Specified native messaging host not found.";

function event() {
  const fns = [];
  return { fns, addListener: (fn) => fns.push(fn) };
}

const clone = (value) => (value === undefined ? undefined : structuredClone(value));

async function idle() {
  for (let i = 0; i < 20; i++) await new Promise((resolve) => setImmediate(resolve));
}

// One browser: a background context, a storage.session, a native host that acks or is missing, and tabs of pages.
function makeBrowser({ allUrls = true, host = "ack" } = {}) {
  const session = new Map();
  const posted = [];
  const pages = new Map();
  const onMessage = event();
  const onRemoved = event();
  let lastError;

  function connectNative(name) {
    assert.equal(name, "app.cptk.overlay");
    const port = { onDisconnect: event(), onMessage: event(), error: null };
    port.postMessage = (msg) => {
      if (host !== "ack") return;
      posted.push(clone(msg));
      setImmediate(() => port.onMessage.fns.forEach((fn) => fn({ type: "ack", seq: posted.length, pid: 4242 })));
    };
    if (host === "missing") {
      setImmediate(() => {
        lastError = { message: NOT_FOUND };
        port.onDisconnect.fns.forEach((fn) => fn(port));
        lastError = undefined;
      });
    }
    return port;
  }

  const backgroundApi = {
    runtime: {
      onMessage,
      connectNative,
      getManifest: () => ({ name: "CPTK Overlay Probe", version: "0.0.1" }),
      get lastError() {
        return lastError;
      },
    },
    storage: {
      session: {
        async get(keys) {
          const out = {};
          for (const key of keys === null ? [...session.keys()] : [].concat(keys)) {
            if (session.has(key)) out[key] = session.get(key);
          }
          return out;
        },
        async set(items) {
          for (const [key, value] of Object.entries(items)) session.set(key, clone(value));
        },
        async remove(keys) {
          for (const key of [].concat(keys)) session.delete(key);
        },
      },
    },
    permissions: {
      async contains(query) {
        assert.deepEqual(clone(query), { origins: ["<all_urls>"] });
        return allUrls;
      },
    },
    tabs: {
      onRemoved,
      async sendMessage(tabId, msg) {
        const page = pages.get(tabId);
        if (!page) throw new Error("Could not establish connection. Receiving end does not exist.");
        page.onMessage.fns.forEach((fn) => fn(clone(msg), {}));
      },
    },
  };
  vm.runInNewContext(BACKGROUND, { chrome: backgroundApi, console, URL });

  // runtime.sendMessage from a tab: the background's listeners answer through sendResponse.
  function sendFromTab(tabId, url, msg) {
    return new Promise((resolve) => {
      const sender = { tab: { id: tabId }, frameId: 0, url };
      let waiting = false;
      for (const fn of onMessage.fns) {
        if (fn(clone(msg), sender, (reply) => resolve(clone(reply))) === true) waiting = true;
      }
      if (!waiting) resolve(undefined);
    });
  }

  // Loads a page in a tab at document_start; load() then fires DOMContentLoaded. With parsed, the script arrives
  // after DOMContentLoaded has already fired, as it can for a page restored from the back-forward cache.
  function open(tabId, url, { webdriver = false, parsed = false } = {}) {
    const page = { url: new URL(url), onMessage: event(), keydown: [], ready: [], replaced: [], box: null };
    const body = { appendChild: (el) => (page.box = el) };
    const doc = {
      readyState: parsed ? "interactive" : "loading",
      body: parsed ? body : null,
      documentElement: { requestFullscreen: async () => {} },
      createElement: () => ({ style: {}, textContent: "" }),
      querySelector: () => null,
      addEventListener: (type, fn) => {
        if (type === "DOMContentLoaded") page.ready.push(fn);
      },
    };
    const location = {
      get hash() {
        return page.url.hash;
      },
      get href() {
        return page.url.href;
      },
      get origin() {
        return page.url.origin;
      },
      get pathname() {
        return page.url.pathname;
      },
      get search() {
        return page.url.search;
      },
    };
    const history = {
      state: null,
      // Like a browser: a URL that resolves to another origin throws SecurityError and changes nothing.
      replaceState: (state, title, next) => {
        const resolved = new URL(next, page.url);
        if (resolved.origin !== page.url.origin) throw new Error(`SecurityError: replaceState to ${resolved.href}`);
        page.replaced.push(next);
        page.url = resolved;
      },
    };
    const contentApi = {
      runtime: {
        getManifest: () => ({ name: "CPTK Overlay Probe", version: "0.0.1" }),
        sendMessage: (msg) => sendFromTab(tabId, page.url.href, msg),
        onMessage: page.onMessage,
      },
    };
    const window = {
      addEventListener: (type, fn) => {
        if (type === "keydown") page.keydown.push(fn);
      },
    };
    const navigator = { webdriver, userActivation: { isActive: true } };
    pages.set(tabId, page);
    const globals = { chrome: contentApi, location, history, document: doc, window, navigator, console, URL };
    vm.runInNewContext(CONTENT, globals);
    page.load = () => {
      doc.readyState = "interactive";
      doc.body = body;
      page.ready.forEach((fn) => fn());
    };
    page.press = (key, code) => page.keydown.forEach((fn) => fn({ key, code, isTrusted: true }));
    page.boxText = () => (page.box ? page.box.textContent : "");
    return page;
  }

  function close(tabId) {
    pages.delete(tabId);
    onRemoved.fns.forEach((fn) => fn(tabId, {}));
  }

  return { session, posted, open, close, sendFromTab };
}

async function openLoaded(b, tabId, url, options) {
  const page = b.open(tabId, url, options);
  await idle();
  page.load();
  await idle();
  return page;
}

function assertNoToken(b) {
  assert.ok(!JSON.stringify([...b.session]).includes(TOKEN), "the token reached storage.session");
  assert.ok(!JSON.stringify(b.posted).includes(TOKEN), "the token reached the host");
}

const keys = (b) => b.posted.filter((m) => m.event === "key").map((m) => ({ key: m.data.key, code: m.data.code }));

const SCENARIOS = {
  async "inactive-tab-does-nothing"() {
    const b = makeBrowser();
    const page = await openLoaded(b, 1, "https://evil.example/login");
    page.press("a", "KeyA");
    await idle();
    assert.equal(page.keydown.length, 0);
    assert.equal(page.box, null);
    assert.deepEqual(b.posted, []);
    assert.deepEqual([...b.session.keys()], []);
  },

  async "token-activates-only-its-tab"() {
    const b = makeBrowser();
    const page = await openLoaded(b, 1, `https://site.example/watch?v=1#cptk=${TOKEN}&t=30`);
    assert.deepEqual(page.replaced, ["https://site.example/watch?v=1#t=30"]);
    assert.equal(b.session.get("active:1"), true);
    assert.equal(b.posted.length, 1);
    const start = b.posted[0];
    assert.equal(start.type, "report");
    assert.equal(start.event, "start");
    assert.deepEqual(start.data, {
      webdriver: false,
      tokenFromUrl: true,
      allUrls: true,
      origin: "https://site.example",
      version: "0.0.1",
    });
    assert.equal(start.tab, 1);
    assert.equal(start.frame, 0);
    assert.equal(typeof start.t, "number");
    assert.match(page.boxText(), /^CPTK Overlay Probe 0\.0\.1\n/);
    assert.match(page.boxText(), /webdriver: false/);
    assert.match(page.boxText(), /page ready/);

    const other = await openLoaded(b, 2, "https://other.example/");
    other.press("ArrowUp", "ArrowUp");
    await idle();
    assert.equal(other.keydown.length, 0);
    assert.equal(b.posted.length, 1);

    const next = await openLoaded(b, 1, "https://other.example/signin");
    assert.equal(next.keydown.length, 1);
    assert.equal(b.posted.length, 2);
    assert.equal(b.posted[1].data.tokenFromUrl, false);
    assertNoToken(b);
  },

  async "keys-are-redacted"() {
    const b = makeBrowser();
    const page = await openLoaded(b, 1, `https://site.example/#cptk=${TOKEN}`);
    page.press("a", "KeyA");
    page.press("ArrowUp", "ArrowUp");
    page.press("Enter", "Enter");
    page.press("MediaPlayPause", "MediaPlayPause");
    page.press("7", "Digit7");
    page.press("\u{1F600}", "");
    await idle();
    assert.deepEqual(keys(b), [
      { key: "char", code: null },
      { key: "ArrowUp", code: "ArrowUp" },
      { key: "Enter", code: "Enter" },
      { key: "MediaPlayPause", code: "MediaPlayPause" },
      { key: "char", code: null },
      { key: "char", code: null },
    ]);
    const box = page.boxText();
    for (const leaked of ["KeyA", "Digit7", '"a"', '"7"', "\u{1F600}"]) {
      assert.ok(!box.includes(leaked), `the box shows ${leaked}`);
    }
  },

  async "probe-pages-keep-full-keys"() {
    const b = makeBrowser();
    const page = await openLoaded(b, 1, `http://127.0.0.1:8765/probe.html#cptk=${TOKEN}`);
    page.press("a", "KeyA");
    await idle();
    assert.deepEqual(keys(b), [{ key: "a", code: "KeyA" }]);
    assert.match(page.boxText(), /"KeyA"/);
  },

  async "background-redacts-again"() {
    const b = makeBrowser();
    await openLoaded(b, 1, `https://site.example/#cptk=${TOKEN}`);
    const raw = {
      type: "report",
      event: "key",
      data: { key: "a", code: "KeyA", trusted: true, activation: true, origin: "http://127.0.0.1:8765", version: "0.0.1" },
    };
    assert.deepEqual(await b.sendFromTab(1, "https://site.example/", raw), { ok: true, hostError: null });
    assert.deepEqual(keys(b), [{ key: "char", code: null }]);
    assert.deepEqual(await b.sendFromTab(9, "https://site.example/", raw), { ok: false, error: "tab not active" });
    assert.equal(b.posted.length, 2);
    assert.equal(await b.sendFromTab(1, "https://site.example/", { type: "constructor" }), undefined);
  },

  async "allurls-is-read-at-relay-time"() {
    const b = makeBrowser({ allUrls: false });
    await openLoaded(b, 1, `https://site.example/#cptk=${TOKEN}`);
    assert.equal(b.posted[0].data.allUrls, false);
    assert.ok(![...b.session.keys()].some((key) => /allurls/i.test(key)));
  },

  async "host-failure-is-shown"() {
    const b = makeBrowser({ host: "missing" });
    const page = await openLoaded(b, 1, `https://site.example/#cptk=${TOKEN}`);
    assert.equal(b.session.get("hostError"), NOT_FOUND);
    assert.match(page.boxText(), new RegExp(`host: ${NOT_FOUND.replace(".", "\\.")}`));
    const next = await openLoaded(b, 1, "https://site.example/next");
    assert.match(next.boxText(), /host: Specified native messaging host not found\./);
  },

  async "host-connection-is-shown"() {
    const b = makeBrowser();
    const page = await openLoaded(b, 1, `https://site.example/#cptk=${TOKEN}`);
    assert.match(page.boxText(), /host: connected \(pid 4242\)/);
    assert.equal(b.session.get("hostError"), null);
  },

  async "box-draws-on-an-already-parsed-page"() {
    const b = makeBrowser();
    const page = b.open(1, `https://site.example/#cptk=${TOKEN}`, { webdriver: true, parsed: true });
    await idle();
    assert.equal(page.ready.length, 0);
    assert.match(page.boxText(), /webdriver: true/);
    assert.match(page.boxText(), /page ready/);
    assert.equal(b.posted[0].data.webdriver, true);
  },

  async "closed-tab-is-forgotten"() {
    const b = makeBrowser();
    await openLoaded(b, 1, `https://site.example/#cptk=${TOKEN}`);
    b.close(1);
    await idle();
    assert.equal(b.session.has("active:1"), false);
    const page = await openLoaded(b, 1, "https://site.example/");
    assert.equal(page.keydown.length, 0);
  },

  // "//evil.com/x" as a path-only URL would be protocol-relative: another origin, so replaceState would throw.
  async "token-is-stripped-on-a-double-slash-path"() {
    const cases = [
      [`//evil.com/x#cptk=${TOKEN}&a=1`, "//evil.com/x#a=1"],
      [`//evil.com/x?q=1#cptk=${TOKEN}`, "//evil.com/x?q=1"],
    ];
    for (const [given, expected] of cases) {
      const b = makeBrowser();
      const page = await openLoaded(b, 1, `https://site.example${given}`);
      assert.deepEqual(page.replaced, [`https://site.example${expected}`], given);
      assert.equal(page.url.href, `https://site.example${expected}`, given);
      assert.equal(b.session.get("active:1"), true, given);
      assert.equal(b.posted[0].event, "start", given);
      assert.equal(page.keydown.length, 1, given);
      assert.doesNotMatch(page.boxText(), /error/, given);
    }
  },

  async "token-is-stripped-from-the-fragment-only"() {
    const cases = [
      [`/p#cptk=${TOKEN}`, "/p"],
      [`/p?q=1#cptk=${TOKEN}&a=1`, "/p?q=1#a=1"],
      [`/p#a=1&cptk=${TOKEN}`, "/p#a=1"],
      [`/p#a=1&cptk=${TOKEN}&b=2`, "/p#a=1&b=2"],
    ];
    for (const [given, expected] of cases) {
      const b = makeBrowser();
      const page = await openLoaded(b, 1, `https://site.example${given}`);
      assert.deepEqual(page.replaced, [`https://site.example${expected}`], given);
      assert.equal(b.session.get("active:1"), true, given);
    }
    const ignored = [
      `/p#xcptk=${TOKEN}`,
      `/p?cptk=${TOKEN}`,
      `/p#cptk=${TOKEN.toUpperCase()}`,
      `/p#cptk=${TOKEN.slice(1)}`,
      `/p#cptk=${TOKEN}0`,
    ];
    for (const given of ignored) {
      const b = makeBrowser();
      const page = await openLoaded(b, 1, `https://site.example${given}`);
      assert.deepEqual(page.replaced, [], given);
      assert.equal(page.keydown.length, 0, given);
      assert.deepEqual(b.posted, [], given);
    }
  },
};

const run = SCENARIOS[scenario];
if (!Object.hasOwn(SCENARIOS, scenario)) {
  console.error(`unknown scenario: ${scenario}`);
  process.exit(2);
}
run().then(
  () => console.log(`ok ${scenario}`),
  (err) => {
    console.error(err);
    process.exit(1);
  },
);
