# R27: Firefox gaps opened by the target decision

Researched 2026-10-27 for StreamFlex todo item 24. Desk research only; nothing installed, nothing in `design/research/overlay/` edited. Context read: `15-firefox-safari.md`, `SYNTHESIS.md` §4.9 and §9.

**Grades**
- VERIFIED = a primary page or Mozilla source file was fetched and read for this report.
- REPORTED = a single or secondary source, or a snippet.
- UNKNOWN = not found, or not fetchable.
- INFERENCE = my reading of verified material, not stated by any source. Labeled so it is not mistaken for a finding.

**How the source was read.** searchfox.org pages (its search page is JavaScript-rendered and the big files exceed WebFetch's 10 MB cap) could not be fetched. I read the same files from Mozilla's GitHub org mirror `mozilla-firefox/firefox` instead, with `curl` plus `gh api` code search. `main` was about Firefox 161 Nightly at fetch time. For the ESR question I also read the tags `FIREFOX_153_5_1esr_RELEASE` and `FIREFOX_140_17_0esr_RELEASE`. File URLs below are `https://github.com/mozilla-firefox/firefox/blob/main/<path>`. MDN pages were fetched through WebFetch, whose small summarizing model paraphrases; where MDN and the source disagree, the source wins and the conflict is called out.

---

## 0. Summary table

| # | Question | Answer | Grade |
|---|---|---|---|
| 1a | Does `general.useragent.override` in a profile's `user.js` still work, and is it per-site? | Yes, and it is global for the profile. It sets both the HTTP `User-Agent` header (`nsHttpHandler`) and `navigator.userAgent` (`Navigator.cpp`). There is no per-domain form in the current source: the only handler reads the single pref `general.useragent.override`. | VERIFIED (source); the historical removal of `.<domain>` is UNKNOWN (no history fetched) |
| 1b | resistFingerprinting interference? | Yes. With RFP on for the document, the override is skipped in both code paths and a spoofed UA is returned. With RFP off (the default) nothing else in those paths touches it. | VERIFIED |
| 1c | Extension alternative (`webRequest` / DNR header rewrite plus `navigator.userAgent`)? | Allowed. Blocking `webRequest.onBeforeSendHeaders` stays in Firefox MV3 and MDN's own example rewrites `User-Agent`. DNR `modifyHeaders` exists since Firefox 113. A header rewrite alone does not change `navigator.userAgent`; that needs a page-world override. The pref is simpler and covers both. | VERIFIED (MDN example, BCD, R5 blog) for allowed; header-name limits for DNR UNKNOWN |
| 1d | Does YouTube `/tv` accept it in desktop Firefox in 2026? | UNKNOWN. Only a 2019 extension (header rewrite of the `/tv*` request to a Tizen smart-TV UA) and a 2025 AMO add-on "YouTube TV Spoofer" (26 users, no reviews) exist. Nobody reports a current result. Must be tested on the target. | REPORTED (existence) / UNKNOWN (works now) |
| 2a | Autoplay-with-sound prefs per profile | `media.autoplay.default = 0` in `user.js` allows autoplay with sound for the whole profile. Values are 0 allowed, 1 blocked audible (the default), 5 blocked all (MDN's "default 0, 2 = prompt" is stale). `media.autoplay.blocking_policy` default 0 = sticky activation, so after any user keypress, audible play is already allowed under the default pref. | VERIFIED |
| 2b | Equivalent of Chromium's `AutoplayAllowlist` | Site permission `autoplay-media` (ALLOW_ACTION) overrides the pref. Settable by policy `Permissions.Autoplay.Allow` (per Firefox install, not per profile) or by seeding `permissions.sqlite`. No allow-list pref exists. Because each service has its own profile, the per-profile pref `media.autoplay.default=0` is the practical equivalent. | VERIFIED (policy and code); the `permissions.sqlite` seed is INFERENCE |
| 3 | Hardware media keys to Media Session | Yes: `media.hardwaremediakeys.enabled`, default `true`. Windows via SMTC (`WindowsSMTCProvider`), Linux via MPRIS (`MPRISServiceHandler`, one service per instance). It starts monitoring only once the page has media (a main media controller exists). No kiosk gating anywhere under `dom/`. | VERIFIED (source); behavior inside a live `-kiosk` run not tested, UNKNOWN |
| 4 | Dev loop equal to `Extensions.loadUnpacked` | Two paths. (a) `web-ext run` installs a temporary add-on through the Remote Debugging Protocol, but copies a `--firefox-profile` to a temp dir unless `--keep-profile-changes`. (b) WebDriver BiDi `webExtension.install` (`path`, `archivePath`, `base64`), temporary by default, `moz:permanent: true` for an XPI. Both work on release Firefox with an arbitrary `-profile`. A temporary add-on does get its MV3 host permissions (see §4). | VERIFIED (source and docs) |
| 5 | Trusted input from outside the page | BiDi `input.performActions` is implemented with `EventUtils.synthesizeKey/Mouse` (widget-path synthesis, partly in the parent process). Firefox's own WPT tests expect `isTrusted: true`. User activation: UNKNOWN (not tested). `--remote-debugging-port` makes `navigator.webdriver` true, shows a remote-control cue in the UI, and applies the remote agent's automation prefs. Bad for streaming sites; OS-level input has none of these. | isTrusted VERIFIED (test expectation); activation UNKNOWN; webdriver flag VERIFIED |
| 6 | `requestFullscreen()` in `-kiosk` | Not needed to remove chrome: kiosk sets `window.fullScreen = true` and never shows the toolbox. A page's `requestFullscreen()` still enters DOM-fullscreen state (the element fills the window) but does not change the window; exiting it does not restore or leave window fullscreen. The video's own fullscreen button therefore works and Esc leaves only DOM fullscreen. | VERIFIED (source); any fullscreen warning banner UNKNOWN |
| 7 | Closed shadow roots | `element.openOrClosedShadowRoot` (a property, since Firefox 63), isolated-world content scripts only. `browser.dom.openOrClosedShadowRoot()` does NOT exist in Firefox (BCD: `version_added: false`); that is Chrome 88+ and Safari 26+. MDN's page implies otherwise; BCD and source do not. | VERIFIED |
| 8 | ESR | Current ESR is 153.5.1 (released 2026-10-20). ESR 140.17.0 (2026-09-29) is also still maintained, as is ESR 115.43.1. Every item above that is a code or pref fact was checked on both the 153.5.1 and 140.17.0 tags and exists in both (one difference: the RemoteAgent automation-flag split is absent in 140). MV3 supported. | VERIFIED (product-details and tags) |
| 9 | Widevine on Linux aarch64 (Pi 5) | Not from Mozilla today. Mozilla's live GMP update service returns only OpenH264 for `Linux_aarch64-gcc3` (Widevine for `Linux_x86_64-gcc3`), and `widevinecdm.json` lists no Linux aarch64 platform. Bug 1612995 is open, but a comment reports Google now ships a Linux aarch64 CDM and it ran on Firefox 153 on a Pi 5 with a local build-config change. | VERIFIED (server response and source); the bug comments are VERIFIED as read, dates are relative |

---

## 1. Per-profile user-agent override (YouTube `/tv`)

### 1.1 The pref works, globally, for the whole profile

- `netwerk/protocol/http/nsHttpHandler.cpp`: the handler registers `UA_PREF_PREFIX "general.useragent."` and reads exactly one override, `Preferences::GetCString(UA_PREF("override"), mUserAgentOverride)`. `UserAgent()` returns it unless `aShouldResistFingerprinting` and a spoofed UA exists: "`using general.useragent.override : %s`". VERIFIED. https://github.com/mozilla-firefox/firefox/blob/main/netwerk/protocol/http/nsHttpHandler.cpp
- `dom/base/Navigator.cpp` `Navigator::GetUserAgent`: when not resisting fingerprinting it reads `general.useragent.override` directly, so `navigator.userAgent` matches the header. VERIFIED. https://github.com/mozilla-firefox/firefox/blob/main/dom/base/Navigator.cpp
- A `gh` code search for `general.useragent.override` across the whole tree returns only `Navigator.cpp`, `nsHttpHandler.cpp`, `Preferences.cpp` (an allow-list of dynamic prefs) and tests. No per-domain handler exists. `general.useragent.override.<domain>` is gone from the code (VERIFIED absence); when it was removed is UNKNOWN.
- `browser/app/profile/firefox.js` and `modules/libpref/init/all.js` do not set it, so a `user.js` value stands. VERIFIED.
- The remote agent's recommended prefs (see §5) set `general.useragent.updates.enabled=false` only; they do not touch the override. VERIFIED.
- Consequence for the design: the override applies to every site in that profile. That is fine for a per-service YouTube profile (one service per profile). It would break a profile that visits other sites.

### 1.2 resistFingerprinting

`privacy.resistFingerprinting` (RFP) wins over the override in both `nsHttpHandler::UserAgent` and `Navigator::GetUserAgent`; the spoofed Firefox UA is used instead. The default is RFP off, so nothing to do unless a profile enables it. VERIFIED (same two files).

### 1.3 Extension alternative

- Blocking `webRequest` is kept in Firefox MV3 (Mozilla add-ons blog, 2024-03-13, per R5, not refetched here). MDN's `onBeforeSendHeaders` page carries a `User-Agent` rewrite as its own example and requires `webRequest` plus `webRequestBlocking`. VERIFIED: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onBeforeSendHeaders
- `declarativeNetRequest` is supported from Firefox 113 (mdn/browser-compat-data `webextensions/api/declarativeNetRequest.json`); `modifyHeaders` needs host permissions. Whether any header name (such as `User-Agent`) is restricted for Firefox: UNKNOWN (the `ModifyHeaderInfo` page was not fetched).
- A header rewrite does not change `navigator.userAgent`. A page-world content script (`world: "MAIN"`, Firefox 128+) could redefine it; Xray wrappers hide that from the isolated world. INFERENCE for the combined recipe. The pref avoids all of this.

### 1.4 Does `/tv` accept it?

- 2019 extension `ticklemynausea/youtube-tv-ua-spoof` (MV2): a blocking `onBeforeSendHeaders` listener on `https://www.youtube.com/tv*` that sets `User-Agent` to `Mozilla/5.0 (SMART-TV; Linux; Tizen 4.0.0.2) AppleWebkit/605.1.15 (KHTML, like Gecko) SamsungBrowser/9.2 TV Safari/605.1.15`. Last commit 2019-10-31. It rewrites only the request header, which shows YouTube's `/tv` gate in 2019 was server-side on the UA. REPORTED (source read, but 7 years old): https://github.com/ticklemynausea/youtube-tv-ua-spoof
- AMO "YouTube TV Spoofer" 1.1.1, updated 2025-07-04, 26 users, "spoofs User-Agent on www.youtube.com/tv to think you're a switch", permission only www.youtube.com. The page shows one rating and no review text, so it is not evidence that it works now. REPORTED: https://addons.mozilla.org/en-US/firefox/addon/youtube-tv-spoofer/
- Whether the `/tv` (Cobalt/Leanback) UI loads, runs and plays on the Gecko engine with a TV UA in 2026: UNKNOWN. The synthesis already notes `/tv` is confirmed on Chromium (VacuumTube). Treat the Firefox recipe as "pref in `user.js`, then test on the Pi and on Windows"; keep D4's fallback to the standard site if it fails.

---

## 2. Autoplay

- `modules/libpref/init/StaticPrefList.yaml`: `media.autoplay.default` is `int32_t`, value **1**, comment "Block autoplay, asking for permission by default. 0=Allowed, 1=Blocked, 5=All Blocked". `media.autoplay.blocking_policy` is `uint32_t`, value **0** = "use sticky activation (default)"; 1 = transient activation; 2 = user input depth. `media.autoplay.allow-extension-background-pages` is true. VERIFIED. https://github.com/mozilla-firefox/firefox/blob/main/modules/libpref/init/StaticPrefList.yaml
- Conflict: MDN's autoplay guide says `media.autoplay.default` has default 0 and 2 = prompt. The source says default 1 and the valid values are 0, 1, 5. The source wins. https://developer.mozilla.org/en-US/docs/Web/Media/Guides/Autoplay
- `dom/media/autoplay/AutoplayPolicy.cpp`: decision order is (1) site permission, (2) default pref. Site ALLOW_ACTION returns "allowed". Site DENY returns allowed only if inaudible or using the blocking model. Otherwise default ALLOWED (0) returns allowed, BLOCKED (1) allows inaudible or user-gesture-activated media, BLOCKED_ALL (5) only user-gesture-activated media. VERIFIED. https://github.com/mozilla-firefox/firefox/blob/main/dom/media/autoplay/AutoplayPolicy.cpp
- So a remote key press (a real key event) gives sticky activation and audible playback is then allowed even at the default pref; `media.autoplay.default = 0` removes the dependence on any prior activation, covering the first-load autoplay case.
- Allow-list equivalent: the site permission `autoplay-media`. The policy `Permissions` → `Autoplay` → `Allow` / `Block` / `Default` / `Locked` writes it (`addAllowDenyPermissions("autoplay-media", …)`) and maps `Default` to `media.autoplay.default` (`allow-audio-video` = 0, `block-audio` = 1, `block-audio-video` = 5). VERIFIED (`browser/components/enterprisepolicies/Policies.sys.mjs`, lines ~2703-2727). Policy availability: Firefox 74 / ESR 68.6, `Default` and `Locked` from 76 / 68.8.0 (admin docs). https://firefox-admin-docs.mozilla.org/reference/policies/permissions/ . Policies are per install, not per profile (R5 §1.1).
- Seeding the permission without a policy would mean shipping a `permissions.sqlite` in the profile. That is plausible (it is the permission manager's own store) but I did not verify the schema or Firefox's behavior when handed a pre-made one. INFERENCE; prefer the pref.
- Recommendation: `user.js` `user_pref("media.autoplay.default", 0);` per service profile. There is no pref-form per-site allow-list.

---

## 3. Media keys

- `media.hardwaremediakeys.enabled`: `bool`, `true` (default), in `StaticPrefList.yaml` (main, ESR 153.5.1 and ESR 140.17.0 all `true`). VERIFIED.
- `dom/media/mediacontrol/MediaControlKeyManager.cpp`: `StartMonitoringControlKeys()` returns false if the pref is off; otherwise creates `widget::CreateMediaControlKeySource()`. `OnPreferenceChange()` monitors only "when the pref is on and having a main controller" (a page with media). VERIFIED. https://github.com/mozilla-firefox/firefox/blob/main/dom/media/mediacontrol/MediaControlKeyManager.cpp
- Windows backend: `widget/windows/WindowsSMTCProvider.cpp` (System Media Transport Controls). Linux backend: `widget/gtk/MPRISServiceHandler.cpp`, which registers a D-Bus service named `org.mpris.MediaPlayer2.firefox.instance<id>` (`SetServiceName`), so two Firefox instances (two profiles) each register their own player. VERIFIED (files fetched / located).
- Kiosk: a `gh` code search for `kiosk` under `dom/` returns nothing, under `widget/` only `gtk/nsWindow`, under `toolkit/` only `xre` (startup). Nothing gates media control on kiosk. VERIFIED absence by code search; a live check in `-kiosk` has not been done, UNKNOWN.
- Design consequences (INFERENCE): the keys are system-global, not focus-bound, so with two simultaneous media-playing Firefox instances (or Firefox plus another player) the OS picks the target. MPRIS needs a session D-Bus (the Pi kiosk must have one). On Linux the launcher can also drive playback from outside through MPRIS (`PlayPause`, `Next`, `Previous`), which would not need page cooperation beyond the page's `navigator.mediaSession` action handlers.

---

## 4. Development loop

### 4.1 `web-ext run`
- "Builds and then temporarily installs an extension on the target application." `--firefox-profile` / `-p` is the **base** profile and is **copied to a temporary profile**; `--keep-profile-changes` saves changes back, `--profile-create-if-missing` creates the directory, `--pref` sets prefs without touching the profile, `--args` passes extra binary arguments, `--devtools` needs Firefox 106+. VERIFIED. https://extensionworkshop.com/documentation/develop/web-ext-command-reference/
- Source (`mozilla/web-ext` `src/firefox/index.js`, `preferences.js`, `remote.js`): spawns Firefox with `no-remote: true`, `profile: <path>`, a debugger-server `listen: <port>`; installs with the RDP request `installTemporaryAddon`; sets prefs including `devtools.debugger.remote-enabled=true`, `extensions.autoDisableScopes=10`, `xpinstall.signatures.required=false` (which has an effect only on builds that allow it, not release; INFERENCE from the signing rules). VERIFIED (read).
- A temporary add-on is removed on restart. https://extensionworkshop.com/documentation/develop/temporary-installation-in-firefox/ (VERIFIED). It needs no signature (the page does not say so; INFERENCE from the page's contrast with signed installs).
- To make `web-ext run` use the launcher's actual per-service profile directory, pass `--firefox-profile <dir> --keep-profile-changes` and `--args=-kiosk`; otherwise it works on a throwaway copy. Untested.

### 4.2 WebDriver BiDi `webExtension.install`
- `remote/webdriver-bidi/modules/root/webExtension.sys.mjs`: `install` takes `extensionData` of type `path` (unpacked), `archivePath` or `base64`, plus Firefox extensions `moz:allowPrivateBrowsing` and `moz:permanent`. Temporary by default (`AddonManager.installTemporaryAddon`); `moz:permanent: true` installs an XPI permanently and is refused for an unpacked directory ("Permanent installation of unpacked extensions is not supported"). `uninstall` and a Firefox-only `moz:listExtensions` exist. VERIFIED (source). https://github.com/mozilla-firefox/firefox/blob/main/remote/webdriver-bidi/modules/root/webExtension.sys.mjs and `remote/shared/Addon.sys.mjs`
- The same module exists in ESR 140.17.0 (`moz:permanent` present) and ESR 153.5.1. VERIFIED.
- The remote agent starts from `--remote-debugging-port [<port>]` (`remote/components/RemoteAgent.sys.mjs`, `#handleRemoteDebuggingPortFlag`; help text lines ~653-658), also `--remote-allow-hosts` and `--remote-allow-origins`, and is part of release builds. It works with an arbitrary `-profile`; nothing in the handler ties it to a profile. The websocket path and capability negotiation were not read: UNKNOWN. Side effects: see §5.

### 4.3 Does a temporary add-on keep its MV3 host permissions? (and a sideloaded one?)
This is a gap in R5 §1.3 ("UNKNOWN, test") that the source answers.
- In MV3, all manifest origins, content-script `matches` included, are treated as optional origin controls: `originControls: this.manifestVersion >= 3` ("Whether to treat all origin permissions (including content scripts) from the manifest as optional"). `toolkit/components/extensions/Extension.sys.mjs`. VERIFIED.
- They are granted automatically at install by `_setupStartupPermissions()` when `startupReason === "ADDON_INSTALL"` and any of: manifest `granted_host_permissions` (schema default false), the pref `extensions.originControls.grantByDefault`, or the add-on is required by policy. **`grantByDefault` is `true` in `modules/libpref/init/all.js`** ("Include origin permissions in the install prompt for MV3 extensions"; the `default: false` in `Extension.sys.mjs` is only the fallback). Same value on ESR 153.5.1 and ESR 140.17.0. VERIFIED.
- So: a temporary add-on (BiDi or `web-ext`) and a sideloaded profile-folder XPI with `autoDisableScopes=0` both get their manifest origins granted at first install. INFERENCE for the sideload path: it assumes a profile-folder add-on is first started with reason `ADDON_INSTALL`, which I did not read in `XPIProvider`. Add it to the hands-on test list.
- Caveat (VERIFIED code, INFERENCE on impact): on `ADDON_UPGRADE` origins are auto-granted only when the add-on is required by policy. If a later XPI adds a new host pattern, a sideloaded (non-policy) profile will not get it automatically unless the profile is rebuilt. Keep `host_permissions` fixed across releases, or put `extensions.originControls.grantByDefault` aside and use a fresh profile per version.
- Corrects R5's "whether a policy `force_installed` extension gets them granted: UNKNOWN": the code grants them for policy-required add-ons (`Services.policies?.isAddonRequiredByPolicy`). VERIFIED.

---

## 5. Trusted input from outside the page

- Firefox's BiDi `input.performActions` dispatches key events through `EventUtils.synthesizeKey` (`remote/shared/webdriver/Event.sys.mjs` loads `chrome://remote/content/external/EventUtils.js`; `remote/webdriver-bidi/modules/windowglobal/input.sys.mjs` calls `event.sendKeyDown/sendKeyUp`; mouse and wheel through `synthesizeMouseAtPoint` / `synthesizeWheelAtPoint`). Some pointer events are synthesized in the parent process (`asyncEnabled`). VERIFIED (read).
- `isTrusted`: Firefox's WPT BiDi input tests assert `"isTrusted": True` for performActions events (`testing/web-platform/tests/webdriver/tests/bidi/input/perform_actions/queue.py`, `__init__.py`). VERIFIED as a test expectation that runs against Firefox.
- User activation (sticky/transient) granted by those events, and whether sites that check it (autoplay, `requestFullscreen`, EME start) accept them: UNKNOWN, not tested. INFERENCE only: trusted key and mouse events normally activate the document.
- `navigator.webdriver`: `Navigator::Webdriver()` returns true when Marionette or the Remote Agent reports `isBrowserAutomationRunning`. `RemoteAgent` sets `#isBrowserAutomation = true` for command-line startup (`--remote-debugging-port`); only the dynamic runtime start (`startAtRuntime`, behind pref `remote.experimental.dynamicstart.enabled`) can pass `isBrowserAutomation: false`. So `--remote-debugging-port` means `navigator.webdriver === true` on every page. VERIFIED (source, main). ESR 140.17.0's `RemoteAgent.sys.mjs` has no `isBrowserAutomation` split (the string is absent); its `navigator.webdriver` logic was not read: UNKNOWN. MDN's page still says only Marionette sets it (stale). https://developer.mozilla.org/en-US/docs/Web/API/Navigator/webdriver
- Indicator: `browser.js` `gRemoteControl.updateVisualCue()` sets the `remotecontrol` attribute and a remote-control icon in the URL bar for DevTools / Marionette / the Remote Agent. In `-kiosk` the toolbox is never shown (`showNavToolbox` returns early when `BrowserHandler.kiosk`, `browser-fullScreenAndPointerLock.js`), so no visible cue is expected; not tested, UNKNOWN. The flag itself is page-visible and is the real risk for streaming sites that bot-detect.
- Automation prefs: unless `remote.prefs.recommended` is `false`, the agent applies `RecommendedPreferences.sys.mjs` on the default branch at startup: for example `privacy.trackingprotection.enabled=false`, `dom.disable_open_during_load=false`, `focusmanager.testmode=true`, `dom.input_events.security.minNumTicks=0`, `extensions.autoDisableScopes=0`, `security.fileuri.strict_origin_policy=false`, `signon.rememberSignons=false`. VERIFIED. Set `remote.prefs.recommended=false` in `user.js` if the agent is ever used outside a dev run (pref documented at https://firefox-source-docs.mozilla.org/remote/Prefs.html).
- Comparison: OS-level `SendInput` / `uinput` produces real widget input: no `navigator.webdriver`, no agent port, no automation prefs, trusted by definition, and focus-dependent (the Firefox window must be the foreground one). BiDi is therefore a development tool here, not a runtime input path; same conclusion the synthesis reached for CDP `Input.*`. INFERENCE.

---

## 6. Fullscreen in `-kiosk`

- `BrowserContentHandler.sys.mjs`: `-kiosk` (or `-kiosk-monitor <n>`) sets `gKiosk`. `browser-init.js`: with kiosk, `window.fullScreen = true` (not for popups). `browser-fullScreenAndPointerLock.js`: `showNavToolbox` returns immediately in kiosk, the fullscreen toggler is never shown; `browser-commands.js` `fullScreen()` is `window.fullScreen = !window.fullScreen || BrowserHandler.kiosk`, so F11 cannot leave. VERIFIED.
- `dom/base/nsGlobalWindowOuter.cpp` `SetFullscreenInternal`: if the window is already fullscreen for `FullscreenReason::ForFullscreenMode` (browser fullscreen, which kiosk is), a DOM `requestFullscreen()` changes nothing at the widget; and "If we are exiting from DOM fullscreen while we initially make the window fullscreen because of browser fullscreen mode, don't restore the window. But we still need to exit the DOM fullscreen state." VERIFIED (source comment and branch).
- So: the page does not need `requestFullscreen()` to hide chrome. A video's own fullscreen button still works (the element goes to DOM fullscreen inside the already-fullscreen window; Esc or the button ends that state and leaves the window fullscreen). Player UIs that adapt to `document.fullscreenElement` behave normally.
- Not verified: whether a "page is now fullscreen" warning or approval prompt appears in kiosk (prefs such as `full-screen-api.warning.*` were not read), and whether the user-activation requirement for `requestFullscreen()` still applies. UNKNOWN.

---

## 7. Closed shadow roots

- `dom/webidl/Element.webidl`: `[NeedsSubjectPrincipal, Func="Document::IsCallerChromeOrAddon"] readonly attribute ShadowRoot? openOrClosedShadowRoot;` (line ~299 on main, 299 on ESR 153.5.1, 288 on ESR 140.17.0; ESR 140 also has `openOrClosedAssignedSlot`). VERIFIED. https://github.com/mozilla-firefox/firefox/blob/main/dom/webidl/Element.webidl
- No `dom` WebExtension namespace schema in Firefox: a code search for `openOrClosedShadowRoot` under `toolkit/components/extensions` finds only xpcshell tests (`test_ext_shadowdom.js`, which uses `host.openOrClosedShadowRoot`, i.e. the property). VERIFIED.
- `mdn/browser-compat-data` `webextensions/api/dom.json`: `dom` and `dom.openOrClosedShadowRoot` — chrome 88, **firefox `version_added: false`** with note "Use `element.openOrClosedShadowRoot`, available since Firefox 63", **safari 26**. VERIFIED. https://github.com/mdn/browser-compat-data/blob/main/webextensions/api/dom.json
- The MDN page for `browser.dom.openOrClosedShadowRoot` describes the method but the compat data says Firefox lacks it. Final spelling for a cross-browser wrapper: `chrome.dom?.openOrClosedShadowRoot(el) ?? el.openOrClosedShadowRoot` (INFERENCE for the wrapper; the two spellings themselves are VERIFIED). Note this also updates R5's Safari row ("no equivalent API"): BCD lists `browser.dom.openOrClosedShadowRoot` in Safari 26.

---

## 8. Firefox ESR

- `https://product-details.mozilla.org/1.0/firefox_versions.json` (fetched 2026-10-27): `FIREFOX_ESR` = `153.5.1esr`; `FIREFOX_ESR115` = `115.43.1esr`; `LATEST_FIREFOX_VERSION` = `158.0.1`. VERIFIED. `firefox.json` history: ESR 153.0 on 2026-07-21, 153.5.0 on 2026-10-13, 153.5.1 on 2026-10-20; `firefox_history_stability_releases.json` also lists `140.17.0` on 2026-09-29, so ESR 140 is still being patched. VERIFIED.
- Debian tracker for `firefox-esr` (fetched): testing and unstable `153.5.1esr-1`; stable (trixie) and oldstable (bookworm) security-updates column shows `153.5.1esr-1~deb13u1` / `~deb12u1` while the release column shows `140.12.0esr-1~…` (so a trixie install may be on 140.x or 153.x depending on updates applied). REPORTED (WebFetch summary of the tracker table): https://tracker.debian.org/pkg/firefox-esr . What Raspberry Pi OS ships: UNKNOWN (not fetched).
- API and pref presence, both ESR tags checked: `media.hardwaremediakeys.enabled=true`, `media.autoplay.default=1`, `extensions.originControls.grantByDefault=true` (`all.js`), `Element.openOrClosedShadowRoot`, BiDi `webExtension.install` with `moz:permanent` (140 shows 3 matches). The only difference found: ESR 140's `RemoteAgent.sys.mjs` has no `isBrowserAutomation` option. VERIFIED. Present on every channel: `general.useragent.override` (long-standing pref; the ESR `nsHttpHandler` was not fetched, INFERENCE) and the `Permissions.Autoplay` policy (ESR 68.6 and later).
- MV3: Mozilla supports MV3 alongside MV2 and keeps blocking `webRequest` (add-ons blog, 2024-03-13, via R5); Firefox uses event pages (`background.scripts`), not service workers. Both ESRs are far past Firefox 109 (when MV3 shipped, per general knowledge: INFERENCE, not re-fetched). `world: "MAIN"` needs 128+, which both ESRs satisfy. Check the exact ESR 115 behavior only if Windows 7/8 support ever matters; ESR 115 is outside this research.

---

## 9. Widevine on Firefox for Linux aarch64

- Mozilla's Balrog GMP update service, queried live 2026-10-27 (`https://aus5.mozilla.org/update/3/GMP/158.0/20261013000000/<ABI>/en-US/release/Linux/default/default/update.xml`): `Linux_x86_64-gcc3` returns `gmp-gmpopenh264` and `gmp-widevinecdm` 4.10.3050.0; `WINNT_x86_64-msvc` the same pair; **`Linux_aarch64-gcc3` returns only `gmp-gmpopenh264`**. VERIFIED (server responses read).
- `toolkit/content/gmp-sources/widevinecdm.json` (`Widevine-4.10.3112.0`) lists platforms Darwin (aarch64, x86_64), Linux_x86_64-gcc3 (and asan), WINNT (aarch64, x86, x86_64 variants). No Linux aarch64. VERIFIED. https://github.com/mozilla-firefox/firefox/blob/main/toolkit/content/gmp-sources/widevinecdm.json
- Bugzilla 1612995 "Widevine support for linux arm64": status NEW, open, P3. Comment 17 (about 2 months before today): "I've confirmed that Google are now providing Widevine CDM for Linux aarch64"; it worked with Firefox 153 on a Raspberry Pi 5 (with the commenter's build and config changes). Mozilla's Andrew Osmond said Mozilla would first discuss official shipping with Google and then make the Balrog and config changes. A later comment (about 1 month ago): Widevine working on Firefox 156 on Ubuntu 24.04 ARM64 with a local build-config change. Dependent bug 2076804 "Widevine for Linux ARM64 needs sandbox to allow fstatat" was added about 26 days ago and is ASSIGNED. Dates are relative on the page. VERIFIED (read through WebFetch). https://bugzilla.mozilla.org/show_bug.cgi?id=1612995
- Meaning: stock Firefox on a Pi 5 has no Widevine today; the CDM binary now exists, but not through Mozilla's updater, and the sandbox bug is still open. Workarounds in forums (copying the CDM into the profile's `gmp-widevinecdm` directory, the Debian/Pi OS `libwidevinecdm0` package) are REPORTED only (a Pine64 thread from 2022 and a Gentoo thread from 2024; not retested, possibly stale). Treat Netflix, Prime, Disney+ and Max on Firefox Pi 5 as unavailable until that bug resolves; YouTube is unaffected (no DRM). Even with Widevine, Linux is L3 (R1/R5).

---

## 10. Items for a hands-on test

1. `user_pref("general.useragent.override", "<TV UA>")` in a throwaway profile on Windows and on the Pi: does `youtube.com/tv` load the TV UI, play, and take arrow/Enter/Back? Does it break after a Firefox update?
2. `-kiosk -profile <dir> -no-remote` with a sideloaded MV3 XPI (`autoDisableScopes=0`): are manifest origins granted without a prompt (§4.3), does the content script run, and is the startup reason really `ADDON_INSTALL`? Then replace the XPI with a version adding a host pattern and see whether it is granted.
3. In `-kiosk`, play a YouTube or audio test page, then press the physical Play/Pause key on Windows (SMTC) and send `playerctl play-pause` on the Pi (MPRIS): does the page's Media Session handler fire?
4. In `-kiosk`, click a video's fullscreen button: does anything change visually, and does a fullscreen notice appear?
5. BiDi session over `--remote-debugging-port`: does a `performActions` key press count as user activation (audible `video.play()` without a prior gesture, with `media.autoplay.default` at its default 1)? What do the target sites do when `navigator.webdriver` is true?
6. `web-ext run --firefox-profile <dir> --keep-profile-changes --profile-create-if-missing --args=-kiosk`: does it reuse the per-service profile and stay in kiosk?
7. Pi 5: confirm `about:support` shows no Widevine plugin on the stock build; try the `libwidevinecdm0` route only if Netflix on Firefox is ever required.

---

## 11. Sources

**Mozilla source (GitHub mirror, read raw)**
- https://github.com/mozilla-firefox/firefox/blob/main/netwerk/protocol/http/nsHttpHandler.cpp
- https://github.com/mozilla-firefox/firefox/blob/main/dom/base/Navigator.cpp
- https://github.com/mozilla-firefox/firefox/blob/main/modules/libpref/init/StaticPrefList.yaml
- https://github.com/mozilla-firefox/firefox/blob/main/modules/libpref/init/all.js
- https://github.com/mozilla-firefox/firefox/blob/main/browser/app/profile/firefox.js
- https://github.com/mozilla-firefox/firefox/blob/main/dom/media/autoplay/AutoplayPolicy.cpp
- https://github.com/mozilla-firefox/firefox/blob/main/browser/components/enterprisepolicies/Policies.sys.mjs
- https://github.com/mozilla-firefox/firefox/blob/main/dom/media/mediacontrol/MediaControlKeyManager.cpp
- https://github.com/mozilla-firefox/firefox/blob/main/widget/gtk/MPRISServiceHandler.cpp
- https://github.com/mozilla-firefox/firefox/blob/main/toolkit/components/extensions/Extension.sys.mjs
- https://github.com/mozilla-firefox/firefox/blob/main/toolkit/components/extensions/schemas/manifest.json
- https://github.com/mozilla-firefox/firefox/blob/main/remote/components/RemoteAgent.sys.mjs
- https://github.com/mozilla-firefox/firefox/blob/main/remote/shared/RecommendedPreferences.sys.mjs
- https://github.com/mozilla-firefox/firefox/blob/main/remote/shared/Addon.sys.mjs
- https://github.com/mozilla-firefox/firefox/blob/main/remote/webdriver-bidi/modules/root/webExtension.sys.mjs
- https://github.com/mozilla-firefox/firefox/blob/main/remote/webdriver-bidi/modules/windowglobal/input.sys.mjs
- https://github.com/mozilla-firefox/firefox/blob/main/remote/shared/webdriver/Event.sys.mjs
- https://github.com/mozilla-firefox/firefox/blob/main/testing/web-platform/tests/webdriver/tests/bidi/input/perform_actions/queue.py
- https://github.com/mozilla-firefox/firefox/blob/main/browser/base/content/browser.js (`gRemoteControl`)
- https://github.com/mozilla-firefox/firefox/blob/main/browser/base/content/browser-fullScreenAndPointerLock.js
- https://github.com/mozilla-firefox/firefox/blob/main/browser/base/content/browser-commands.js
- https://github.com/mozilla-firefox/firefox/blob/main/browser/base/content/browser-init.js
- https://github.com/mozilla-firefox/firefox/blob/main/browser/components/BrowserContentHandler.sys.mjs
- https://github.com/mozilla-firefox/firefox/blob/main/dom/base/nsGlobalWindowOuter.cpp
- https://github.com/mozilla-firefox/firefox/blob/main/dom/webidl/Element.webidl
- https://github.com/mozilla-firefox/firefox/blob/main/toolkit/content/gmp-sources/widevinecdm.json
- ESR checks: tags `FIREFOX_153_5_1esr_RELEASE` and `FIREFOX_140_17_0esr_RELEASE` of the same repository

**Mozilla services and docs**
- Release data: https://product-details.mozilla.org/1.0/firefox_versions.json , https://product-details.mozilla.org/1.0/firefox.json , https://product-details.mozilla.org/1.0/firefox_history_stability_releases.json
- GMP update service: https://aus5.mozilla.org/update/3/GMP/158.0/20261013000000/Linux_aarch64-gcc3/en-US/release/Linux/default/default/update.xml (and `Linux_x86_64-gcc3`, `WINNT_x86_64-msvc`)
- https://bugzilla.mozilla.org/show_bug.cgi?id=1612995
- https://firefox-admin-docs.mozilla.org/reference/policies/permissions/
- https://firefox-source-docs.mozilla.org/remote/Prefs.html
- https://extensionworkshop.com/documentation/develop/web-ext-command-reference/
- https://extensionworkshop.com/documentation/develop/temporary-installation-in-firefox/
- https://github.com/mozilla/web-ext (`src/firefox/index.js`, `preferences.js`, `remote.js`)
- https://developer.mozilla.org/en-US/docs/Web/Media/Guides/Autoplay (stale on defaults)
- https://developer.mozilla.org/en-US/docs/Web/API/Navigator/webdriver (stale on Firefox)
- https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onBeforeSendHeaders
- https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/dom/openOrClosedShadowRoot (contradicted by BCD)
- https://github.com/mdn/browser-compat-data (`webextensions/api/dom.json`, `webextensions/api/declarativeNetRequest.json`)

**Other**
- https://github.com/ticklemynausea/youtube-tv-ua-spoof (source read via `gh api`)
- https://addons.mozilla.org/en-US/firefox/addon/youtube-tv-spoofer/
- https://tracker.debian.org/pkg/firefox-esr

**Could not fetch or read**
- searchfox.org (search page is JavaScript-only; large files exceed the fetch cap), `support.mozilla.org`, `firefox-source-docs.mozilla.org/remote/Usage.html` (404), the W3C WebDriver / BiDi spec sections on Actions and `webExtension.install` (the fetch stopped before them), the DNR `ModifyHeaderInfo` page, `XPIProvider` startup-reason logic for sideloaded add-ons, Raspberry Pi OS package data, and anything that would show current YouTube `/tv` behavior.
