# R3: Injection Engineering (extension deployment, content scripts, trusted input, spatnav, player control, kiosk)

Researched 2026-09-28 with live web search and fetch. Grades: **VERIFIED** = official/primary source or several independent recent sources; **REPORTED** = single, secondary or forum source; **UNKNOWN** = not found or contradictory. Nothing was installed or run. Note: page-fetch summaries were produced by a small model, so wording is paraphrase; the URLs are the evidence.

## 0. Bottom line and corrections to 00-original

1. **`--load-extension` is dead in branded Chrome** (137+), so section 2 of the original ("load unpacked or `--load-extension`") is wrong for Chrome. For Edge it is UNKNOWN (see 1.1). Do not design around it.
2. **Off-store force-install is gated by "managed" status on Windows** (AD domain, per both Google and Microsoft docs). A stand-alone home HTPC running Windows is not managed, so `ExtensionInstallForcelist` with a self-hosted CRX will not work there. Linux is different (see 1.3).
3. **Edge `--kiosk` officially does not support extensions.** The original plan of "Edge in kiosk mode + injected overlay" is at risk. Use `--app=` or plain fullscreen instead, or a store-listed extension.
4. Closed shadow roots need `chrome.dom.openOrClosedShadowRoot`; "full DOM access including open shadow roots" was right but incomplete.
5. WICG spatial-navigation is **archived** (2026-03-23); no browser ships it. The Chromium `--enable-spatial-navigation` flag still exists but is not a product.
6. `--remote-debugging-port` needs a non-default `--user-data-dir` from Chrome 136, and Edge behaves the same. Original said this; it is confirmed.
7. Original note "Netflix errors M7375 on `currentTime`" is confirmed by a Sept 2026 project issue (REPORTED); the internal API path is still in use in 2026 extensions (REPORTED).

## 1. Deploying the extension without a store

### 1.1 `--load-extension`
- **Chrome branded 137+: removed on all desktop OSes; still works in Chromium and Chrome for Testing. No supported flag or policy to restore it.** VERIFIED.
  - PSA (Chromium extensions group): https://groups.google.com/a/chromium.org/g/chromium-extensions/c/1-g8EFx2BBY/m/S0ET5wPjCAAJ
  - RFC: https://groups.google.com/a/chromium.org/g/chromium-extensions/c/aEHdhDZ-V0E/m/UWP4-k32AgAJ
  - Chrome blog (June 2025): https://developer.chrome.com/blog/extension-news-june-2025
  - Chrome 139 also removed `--disable-extensions-except` and `--extensions-on-chrome-urls`: https://groups.google.com/a/chromium.org/g/chromium-extensions/c/FxMU1TvxWWg/m/daZVTYNlBQAJ (VERIFIED)
- A temporary escape `--disable-features=DisableLoadExtensionCommandLineSwitch` worked in 137 to 139 per user reports (REPORTED): https://github.com/uBlockOrigin/uBOL-home/discussions/342. No evidence it still works in 2026; treat as gone. A Sept 22 2026 issue on Chrome 153 says bundled extensions still never load (REPORTED): https://github.com/omacom/omarchy/issues/12930
- **Edge branded: UNKNOWN.** Playwright docs state both "Chrome and Edge removed the command-line flags needed to side-load extensions" (REPORTED, one source that cites the Chrome thread): https://playwright.dev/docs/chrome-extensions. Microsoft documents a policy `ExtensionInstallTypeBlocklist` value `command_line` (Edge 123+), which implies the switch existed then: https://learn.microsoft.com/lb-lu/DeployEdge/microsoft-edge-browser-policies/extensioninstalltypeblocklist. Microsoft's current sideload doc mentions only Developer mode + Load unpacked (updated 2026-08-12): https://learn.microsoft.com/en-us/microsoft-edge/extensions/getting-started/extension-sideloading. **Must be tested on a current stable Edge.** An Edge-specific removal notice was not found.
- Alternative for tooling: a CDP-based extension install (`Extensions.loadUnpacked`, Puppeteer/WebDriver BiDi `webExtension.install`) is mentioned as the replacement for testing; the PSA above names WebDriver BiDi and Puppeteer. Whether `Extensions.loadUnpacked` requires `--enable-unsafe-extension-debugging` and works in branded Chrome/Edge: UNKNOWN, prototype it. (I did not find a primary source in this pass.)

### 1.2 Developer-mode unpacked
- Works everywhere: enable Developer mode, Load unpacked. VERIFIED: https://learn.microsoft.com/en-us/microsoft-edge/extensions/getting-started/extension-sideloading and the Chrome PSA above.
- Cost: a manual, one-time-per-profile step, and the extension shows in the profile only while that user-data-dir persists. Since the launcher uses its own `--user-data-dir`, the step is once per HTPC install, but a launcher cannot script it without UI automation or CDP.
- Nag: "Disable developer mode extensions" popup at browser start for unpacked extensions on Chrome and Edge; historically not dismissable except by patching (REPORTED, old forum and blog sources, some claim Chrome later stopped showing it): https://groups.google.com/a/chromium.org/g/chromium-extensions/c/LHtbj6Up5dU , https://techjourney.net/remove-disable-developer-mode-extensions-warning-popup-in-chrome-edge/ . Current 2026 behavior UNKNOWN; on a couch HTPC a modal on every launch is unacceptable, so test it.

### 1.3 Policy force-install (`ExtensionInstallForcelist`) with a self-hosted CRX
- **Windows Chrome:** off-store force-install needs the machine joined to an Active Directory domain. VERIFIED (official): https://support.google.com/chrome/a/answer/7532015?hl=en . Other sources (REPORTED) add Azure AD join or Chrome Enterprise Core enrollment (https://issues.chromium.org/issues/331061788 title only; search-summary of https://support.google.com/chrome/a/answer/7532015 variants). Consistent with "unmanaged home PC = no".
- **Windows Edge:** off-store self-hosted needs AD domain join; Entra-joined does not qualify unless hybrid joined. VERIFIED (official, updated 2026-06-15): https://learn.microsoft.com/en-us/deployedge/microsoft-edge-manage-extensions-webstore . Non-domain machines are limited to extensions listed on the Edge Add-ons site. The Add-ons listing can be **unlisted/hidden** (a store route that is still "store" but not public; not researched here, see open questions).
- **Linux Chrome:** policy JSON at `/etc/opt/chrome/policies/managed/*.json`; reports say Linux is the platform where off-store force-install works without a domain (REPORTED, secondary): https://support.google.com/chrome/a/answer/7517525?hl=en (policy path, official), https://groups.google.com/a/chromium.org/g/chromium-extensions/c/hx00nL6tA10 . The omarchy issue also recommends this route on Linux (REPORTED). Prototype on the target distro; I could not fetch the Linux Google page body.
- **Self-hosted update URL:** the update manifest is an XML `gupdate` file with `appid`, `codebase` and `version`; CRX served with `Content-Type: application/x-chrome-extension`; keep the `.pem` or the extension ID changes. VERIFIED (Edge doc above; same format for Chrome). A `file://` or `localhost` update URL is reported as used for local setups (REPORTED, forum).
- **Windows workaround idea:** `ExtensionInstallAllowlist` + `ExtensionInstallSources` lets a user install by opening the CRX URL; still needs a user click and still hits the managed-device check on Windows for some builds (REPORTED). Not viable unattended.
- **Windows registry side of policy** (writing HKLM\Software\Policies\...) needs admin and does not bypass the domain rule above.

### 1.4 Deployment conclusion
| Platform | Best route | Confidence |
|---|---|---|
| Linux Chrome | Managed policy file + self-hosted CRX + update XML | REPORTED, prototype needed |
| Windows Edge | Edge Add-ons store (unlisted) OR manual Load unpacked once, OR `--load-extension` if it still works | UNKNOWN, must test |
| Windows Chrome | Chrome Web Store (unlisted) or manual Load unpacked once | VERIFIED constraint |
| Any, fallback | Skip the extension: use CDP (see 3) | see below |

## 2. MV3 content scripts on these sites

### 2.1 Mechanics (all VERIFIED, https://developer.chrome.com/docs/extensions/reference/manifest/content-scripts and https://developer.chrome.com/docs/extensions/develop/concepts/content-scripts)
- `all_frames` default false (top frame only). Each frame is matched independently against `matches`.
- `match_about_blank` default false; injects into `about:blank` frames whose parent URL matches.
- `match_origin_as_fallback` default false; lets scripts run in `about:`, `data:`, `blob:` frames whose creating origin matches. This is the switch needed for player iframes created from blob or srcdoc.
- `world`: `ISOLATED` (default) is private; `MAIN` shares page JS and is exposed to page interference. In ISOLATED the extension's own CSP applies (no eval, no external script). In MAIN the page's CSP applies.
- **Implication:** a DOM overlay with ISOLATED content script is not blocked by page CSP because it is created by the extension world and styles can be inline via element `style` or an adopted stylesheet. In MAIN, page CSP applies and can block injected `<script>`. Whether any of the four sites' CSP blocks inline `<style>` or `style=` injection: UNKNOWN; I found no site-specific source. Use a Shadow-DOM host with `adoptedStyleSheets` (not blocked by `style-src` in the way `<style>` inline can be) as the safe pattern. This reasoning is mine, needs a check.
- Seeing page globals (Netflix `netflix.appContext`) needs a MAIN-world script (`"world":"MAIN"` in manifest or `chrome.scripting.executeScript`), talking to the isolated script through `window.postMessage`/`CustomEvent`. A concrete 2026 example of this pattern is the "esNetflixSeek" event bridge in https://github.com/msr2903/himotoki-sub/issues/52 (REPORTED).

### 2.2 Closed shadow roots
- `chrome.dom.openOrClosedShadowRoot(element)` returns the open or closed shadow root; Chrome 88+ (docs also show a Chrome 148 availability line; the page states MV3): https://developer.chrome.com/docs/extensions/reference/api/dom (VERIFIED, the version detail looks odd, confirm).
- Cannot cross into closed roots from MAIN world. Cannot read it from a page script. Only extension content scripts can.

### 2.3 Per-service DOM (shadow DOM, iframes, canvas UI)
- **Netflix, Disney+, Prime Video, Max/HBO Max: UNKNOWN.** I found no reliable public source on which of them use shadow DOM, iframes or canvas UI. The original doc's "Disney+ uses shadow DOM heavily" is unverified. Disney+ documents ordinary Tab/Shift+Tab keyboard navigation of its web UI (REPORTED via search result summary of https://help.disneyplus.com/article/disneyplus-accessibility ), which suggests real focusable elements.
- Action: prototype step per service: dump `document.querySelectorAll('*')` for elements with `.shadowRoot`, check `window.frames.length`, and check whether tiles are `<a>/<button>` or canvas. Note DRM video is always a `<video>` element (EME), never canvas.
- For the extension side, walk shadow roots recursively with `openOrClosedShadowRoot` so open and closed both work.

## 3. Trusted input

### 3.1 `isTrusted` and synthetic events
- `element.dispatchEvent(new KeyboardEvent(...))` and `.click()`-style script events are `isTrusted: false`. Browsers ignore untrusted events for default actions (typing in an input, some form submits) and page code can filter on `isTrusted`. VERIFIED as web platform behavior (search result summary; see https://blog.crawlex.net/blog/synthesizing-human-input-events/ for a recent write-up, REPORTED).
- Whether Netflix, Disney+, Prime, Max check `isTrusted` on their player hotkeys: UNKNOWN; no source. `.click()` on real buttons is a click activation and works in general; native `<video>` autoplay/fullscreen need **transient user activation**, which untrusted events do not grant. `requestFullscreen()` from a content script therefore fails without a real gesture. VERIFIED web behavior (platform rule; no fresh source fetched).

### 3.2 CDP `Input.dispatchKeyEvent` / `Input.dispatchMouseEvent`
- Events enter Chromium's input pipeline as if from the OS and are `isTrusted: true`, and give user activation. VERIFIED: https://chromedevtools.github.io/devtools-protocol/tot/Input/ , plus the search summary of the crawlex blog above (REPORTED). Caveats: layout-dependent character mapping errors, and wrappers that add extra untrusted events.
- Needs a debugging connection to the browser, i.e. `--remote-debugging-port` (or `--remote-debugging-pipe`) at launch, or an extension using the `chrome.debugger` API (shows a "started debugging this browser" infobar; not suitable, and blocked on some pages).

### 3.3 `--remote-debugging-port` status
- Chrome 136+: switch is ignored on the default user-data-dir; a non-default `--user-data-dir` is required. VERIFIED: https://developer.chrome.com/blog/remote-debugging-port . Chrome for Testing keeps old behavior.
- **Edge: same behavior**, error "DevTools remote debugging requires a non-default data directory" (REPORTED, Microsoft Q&A + third-party summaries): https://learn.microsoft.com/en-au/answers/questions/5976829/microsoft-edge-remote-debugging-is-not-working-whe . Not a blocker for us because the launcher already uses a dedicated profile.
- Security: an open localhost debug port lets any local process drive the browser and read cookies of that profile. Bind to 127.0.0.1 (default) or use `--remote-debugging-pipe`.

### 3.4 Do the streaming sites detect CDP / automation?
- Anti-bot fingerprinting (Runtime.enable timing/getter leak, `$cdc_`, Playwright bindings, `navigator.webdriver`) is real on bot-protection vendors' sites. In 2025 a Chrome/V8 change stopped the classic `Runtime.enable` getter check from firing on current versions (REPORTED, several 2026 blogs): https://usefoil.com/learn/cdp-detection , https://blog.crawlex.net/blog/detecting-cdp-runtime-enable/ . Detection that still works keys on residue and side effects.
- Whether **Netflix/Disney+/Prime/Max** specifically look for it: UNKNOWN; no source. Evidence points to DRM/EME capability checks (headless Chrome lacks a working CDM), not CDP flags, for streaming (REPORTED, weak): https://www.npmjs.com/package/cloakbrowser .
- Design mitigation: a plain Edge/Chrome launched normally with a debug port, driven by a **CDP-minimal** client (only `Input.*`, `Page.addScriptToEvaluateOnNewDocument`; avoid `Runtime.enable` and console-domain listening), leaves few residues. `navigator.webdriver` is only set true by `--enable-automation` or ChromeDriver; a hand-launched browser with only the debug port has it false (from Chromium behavior, not re-verified here).
- Risk to record: ToS/account risk if a service decides to flag debug-port sessions. UNKNOWN.

### 3.5 OS-level keys from the native launcher
- **Windows `SendInput`:** produces real input that reaches the focused window and browser as a trusted key event, no debug port needed. Works only when the browser window has focus (a kiosk fullscreen window normally does). Blocked from lower to higher integrity (UIPI) windows; run the launcher at the same or higher integrity. (Platform knowledge, no source fetched this pass, treat as REPORTED-by-me.)
- **Linux `uinput`:** creates a kernel-level virtual keyboard that Wayland and X11 treat as a real device; works where Wayland forbids app-level synthetic input. VERIFIED: https://www.kernel.org/doc/html/latest/input/uinput.html ; Wayland context REPORTED: https://gist.github.com/danielrosehill/d3913d4c8cc69acaf3ee7772771c2f1d . Needs `/dev/uinput` permission (udev rule, `input` group).
- **Best split:** the remote's HID keys already arrive as real keyboard events, so navigation keys need no injection at all. Injection is only needed for things the overlay cannot do (e.g. a trusted click to satisfy fullscreen activation, or on-screen-keyboard text into a focused field). SendInput/uinput keeps everything trusted with zero debug-port exposure. CDP is the better tool when we need coordinates-based clicks without moving the real cursor.

## 4. Spatial navigation engines, 2026

| Option | Status | Fit |
|---|---|---|
| WICG spatial-navigation spec + polyfill | Repo **archived 2026-03-23**; spec moved to CSS WG (`css-nav-1`); polyfill "roughly matches" the spec and has known issues; no browser implements it. VERIFIED: https://github.com/WICG/spatial-navigation | Usable as reference or as a quick base; do not depend on upstream fixes. `spatial-navigation-polyfill` on npm. |
| Chromium `--enable-spatial-navigation` | Off by default, non-standard, still a flag; announced only as an experiment; a separate `spatialNavigationSearch` API intent exists. REPORTED (older, 2019-ish sources): https://groups.google.com/a/chromium.org/d/topic/blink-dev/Mjm4FzfxekY . Whether it still exists and works in Chrome 150+/Edge in 2026: UNKNOWN. | A baseline to try; can't be relied on for two browsers and needs launch flag (works in kiosk, no extension needed). |
| Norigin Spatial Navigation | React hooks library, built for Tizen/webOS/VIDAA, ~488 stars, MIT, issues active as of 2025-09 and 2026-03 (REPORTED): https://github.com/NoriginMedia/norigin-spatial-navigation | **Wrong fit**: it needs the app to register focusable components (React tree). Not for navigating someone else's page. |
| js-spatial-navigation (luke-chang), BBC LRUD | Not researched in this pass. UNKNOWN. LRUD is tree-declaration based like Norigin, so the same objection applies (my inference). | Same as above |
| Smart-TV framework approach | Declare focusable regions/containers, remember last-focused child per container, geometric fallback. | Idea worth copying for per-site adapters: row-memory + geometric fallback. |

Recommendation: write our own small geometric engine (DOM-driven, works on any page), taking the WICG algorithm as reference, with per-site adapter hooks for rows and carousels. Do not adopt a component-registration library.

## 5. Player control per service

- **Netflix keyboard (official):** Enter/Space play-pause, F fullscreen, Esc exit fullscreen, Left/Right 10 s, Up/Down volume, M mute, **S skip intro**. VERIFIED: https://help.netflix.com/en/node/24855 . Audio/subtitle menu key "T" is only community-reported (REPORTED: search summary, https://streamingvideopause.com/blog/netflix-keyboard-shortcuts/ ). Not official; use the on-screen menu button instead.
- **Netflix internal API:** `netflix.appContext.state.playerApp.getAPI().videoPlayer` -> `getAllPlayerSessionIds()[0]` -> `getVideoPlayerBySessionId(id)` -> `.seek(ms)`, `.play()`, `.pause()`, `.setVolume()`. Gist dated **2021-10-07**, no recent validation on the page: https://gist.github.com/JacobRBlomquist/5bf6b046334ed84bac030260a93567ba . Independent confirmation that extensions still call `playerApp.getAPI()` in 2026 and that writing `video.currentTime` gives M7375 or desync: https://github.com/msr2903/himotoki-sub/issues/52 and #92 (both Sept 2026; REPORTED, but concrete). Also: `playerApp` is undefined on non-player pages, so guard it. **Verdict: REPORTED current, must be smoke-tested each release; it is an unofficial internal API that can vanish without notice.** Seek keys (Left/Right) are the stable fallback.
- **Prime Video (official):** Space play/pause, F fullscreen, Esc exit fullscreen or stop playback, Left/Right 10 s, Up/Down volume, M mute, **C** subtitles on/off + cycle language, **A** cycle audio track (incl. audio description, Dialogue Boost). VERIFIED: https://www.primevideo.com/help?nodeId=GE4VW9VWJD7FM6DR . No skip-intro key listed; use the on-screen "Skip Intro" button.
- **Disney+:** Tab/Shift+Tab navigation of the site is documented; a shortcut list exists in the help center player-controls article (https://help.disneyplus.com/article/disneyplus-player-controls) but the page returned empty on fetch, so the key list is UNKNOWN from a primary source. Press-coverage lists (Android Central, How-To Geek, MakeUseOf; REPORTED) show Space, arrows, F, M, etc. Verify by test; skip intro and subtitle/audio menus were not documented anywhere I could reach.
- **Max / HBO Max:** No official keyboard-shortcut page found. Native keys per extension listings: F fullscreen, Space/P/K pause (REPORTED, https://github.com/jvjvjv/hbo-max-keyboard-shortcuts and the Chrome Web Store extension listings). Arrow seek, skip intro (I), etc. come from third-party extensions injecting their own handlers, meaning **the native player does not do them**. Treat all of these as UNKNOWN natively; a custom player mode must find the `<video>` element and act via its controls or (for non-Netflix) `video.currentTime`, which is generally safe on non-Netflix sites but not verified per service. Note: service was renamed to HBO Max in 2025 (from memory; not re-verified).
- **General player-mode advice:** send trusted real keys (SendInput/uinput or CDP) for the documented hotkeys, use click on real buttons for skip intro and menus, and keep per-site selectors in updatable config. Menus need a real mouse-move first on some players to show controls (UNKNOWN which).

## 6. Kiosk mode

### 6.1 Edge
- Syntax: `msedge.exe --kiosk <url> --edge-kiosk-type=fullscreen` (digital signage) or `--edge-kiosk-type=public-browsing`; `--kiosk-idle-timeout-minutes=N`, `--no-first-run`. Applies to Edge 87+ (docs updated 2026-08-28). VERIFIED: https://learn.microsoft.com/en-us/deployedge/microsoft-edge-configure-kiosk-mode
- **Runs an InPrivate session** in both types; F11 and F12 blocked; Ctrl+N and Ctrl+T blocked; multi-tab only in public-browsing. VERIFIED (same page).
- **"Kiosk for Linux is not supported."** and the "Functional limitations" list names `Extensions` policies among things that "don't work with kiosk mode". VERIFIED (same page). A Microsoft Q&A answer (Sept 2022) says extensions are not supported in Edge kiosk and no workaround exists (REPORTED): https://learn.microsoft.com/en-us/answers/questions/1020229/cannot-use-browser-extensions-in-edge-kiosk-mode-(
- Extensions in InPrivate need explicit "Allow in InPrivate" (VERIFIED, standard Edge feature; `MandatoryExtensionsForInPrivateNavigation` policy exists). Combined with the above, an overlay extension in Edge kiosk fullscreen is a **no**, until tested otherwise.
- **Assigned Access** (Windows 11): single-app kiosk with Edge, with the kiosk features above; the Microsoft doc's requirements table only cites Windows 10 builds (VERIFIED, stale wording); Assigned Access is a Windows-account lock-down and not needed for an HTPC that a person also administers.
- **Exit:** Alt+F4, or Ctrl+Alt+Del route. No verified official exit key for Edge kiosk beyond that; Chrome kiosk exit is Alt+F4 (REPORTED: https://www.airdroid.com/uem/chrome-kiosk-mode-windows-10/ ).
- **Alternative that keeps extensions:** normal (non-kiosk) window in real fullscreen. Options: `msedge --app=<url> --start-fullscreen` or F11 fullscreen sent by the launcher. App-mode windows run extensions/content scripts (my expectation from Chromium behavior; not source-verified: test).

### 6.2 Chrome
- `--kiosk` and `--app=` supported on Windows/Linux. Whether extensions run in `--kiosk` desktop Chrome: **UNKNOWN / conflicting** (some guides say unsupported, others show `--kiosk --load-extension` combos; all low-quality sources): https://www.manageengine.com/mobile-device-management/articles/chrome-kiosk-mode.html . Test. Since `--load-extension` is gone in branded Chrome, only policy-installed or previously loaded extensions in the profile can apply.
- Kiosk exit: Alt+F4 (REPORTED). Purpose-built "kiosk exit" extensions exist, confirming extensions do load in some kiosk setups (REPORTED, weak): https://chromewebstore.google.com/detail/kiosk-exit-button/daliphodbodnmaljedemmkdlafkoopal
- Suggested flags for the HTPC window: `--user-data-dir=...` (dedicated profile), `--start-fullscreen` or `--app=URL`, `--no-first-run`, `--disable-session-crashed-bubble`, `--autoplay-policy=no-user-gesture-required` (last one not verified here, from Chromium behavior).

### 6.3 Fullscreen API
- Element fullscreen (video player) needs user activation and the browser shows an "Press Esc to exit" hint; Esc exits element fullscreen in normal windows. Kiosk-mode browser windows are already fullscreen so player F key still toggles the element to the same size. (Platform knowledge; no source fetched.) Full-screen video traps focus in the player: keep the overlay's key handler in the **capture phase** at the top window (as the original said) and also inside frames if the player is in one.

### 6.4 Linux, Wayland
- **Chrome 140+ (Aug 2025) auto-detects Wayland** with `--ozone-platform-hint=auto` default: https://www.omgubuntu.co.uk/2025/08/chrome-140-wayland-auto-detection-linux (REPORTED, one news site). Older Chromium kiosk-on-Wayland issue exists: https://issues.chromium.org/issues/40189002 (title only, status UNKNOWN).
- **cage:** Wayland compositor that runs one app fullscreen; used with Chromium kiosk in 2025 (REPORTED): https://stefansblog.org/posts/cage-kiosk---ein-wayland-kiosk-auf-basis-von-nixos-und-chromium/ , https://www.phoronix.com/forums/forum/linux-graphics-x-org-drivers/wayland-display-server/1069526-cage-is-a-new-wayland-compositor-for-kiosk-full-screen-one-app-deployments
- **GNOME Kiosk:** separate Wayland compositor on mutter; GNOME 48 added `window-config.ini`, GNOME 49 added `set-fullscreen`, monitor assignment, tags, and systemd remote sessions. VERIFIED: https://blogs.gnome.org/shell-dev/2025/09/10/gnome-kiosk-updates/
- **Widevine on Linux:** L3 software CDM ceiling is a separate research item (R1). Ubuntu/GNOME Kiosk + Chrome needs the Widevine CDM component and a working VA-API/GPU path; not researched here.
- Input on Wayland: a client cannot inject events, so use `uinput` (see 3.5) from the launcher. The compositor (cage, GNOME Kiosk) sees uinput devices as real keyboards.
- Edge on Linux exists but Edge kiosk is unsupported there (official above). Use Chrome for Linux.

## 7. Recommended architecture

**Primary (Linux Chrome, and Windows if a store or profile route is acceptable):**
1. Launcher (native app) starts the browser with a **dedicated `--user-data-dir`** in **fullscreen** (`--start-fullscreen` or `--app=`), not `--kiosk`, so extensions and the overlay run.
2. **Overlay = MV3 extension.** ISOLATED-world content script on the four sites: geometric spatial engine, focus ring inside a Shadow-DOM host, recursive shadow walking with `openOrClosedShadowRoot`, `all_frames` + `match_origin_as_fallback` where players are in frames, per-site adapters loaded from updatable JSON. A small **MAIN-world** helper only on Netflix for the internal `seek()` bridge.
3. **Key path:** remote HID keys already reach the page as real keyboard events; the content script handles arrows/Enter/Back in the capture phase. Where a trusted key or click is needed (fullscreen, skip, hotkeys), the extension asks the launcher through **Native Messaging** (`chrome.runtime.connectNative`) and the launcher does `SendInput` (Windows) or `uinput` (Linux). No debug port.
4. **Deployment:** Linux via policy file + self-hosted CRX. Windows via store (unlisted) or one-time Load unpacked (verify banner).

**Alternative B (no extension at all):** launcher starts Edge/Chrome with `--remote-debugging-port` on a dedicated profile and drives everything by CDP: `Page.addScriptToEvaluateOnNewDocument` injects the overlay (MAIN world, so page CSP applies and closed shadow roots are unreachable), `Input.dispatchKeyEvent` for trusted keys. Works in Edge and Chrome, needs no store, works with `--kiosk` because no extension is involved. Costs: debug-port exposure, automation residue risk, closed-shadow-root blind spot, page CSP can block script/style injection. Best as the **Windows Edge fallback**.

**Alternative C:** OS overlay via UIA/AT-SPI (original Option C). Still last resort.

**Alternative D (Chromium-derived non-branded):** Chrome for Testing or Chromium regain `--load-extension`, but branded Widevine/PlayReady DRM is the whole point here, so this loses 1080p+ (Chromium without a CDM will not play at all). Not recommended; R1 owns DRM detail.

## 8. Gotchas
- Chrome branded has no `--load-extension`; Playwright/Selenium recipes online are stale.
- Edge kiosk = InPrivate + no extensions; `--kiosk` silently defeats the whole extension design.
- Unpacked-extension "Disable developer mode extensions" popup: modal at every start (needs a 2026 check).
- Off-store force-install on Windows silently requires domain; failure looks like "policy applied, nothing installed" (see Edge troubleshooting: https://learn.microsoft.com/en-us/troubleshoot/microsoft-edge/development/self-host-extension-deploy ).
- CRX signing: keep the `.pem`; losing it changes the extension ID, which breaks the policy entry.
- Content scripts run per frame; without `all_frames` the overlay will not see an iframe player; with it, guard against double-init.
- Netflix: `playerApp` missing on non-player pages; never write `currentTime` (M7375).
- Untrusted `.click()`/`dispatchEvent` cannot start fullscreen or gesture-gated media.
- Debug port: any local process can drive the browser; bind to loopback or use a pipe.
- Wayland: no app-level key injection; `uinput` needs permissions.
- Kiosk exit path must be designed (Alt+F4 or a launcher hotkey that kills the browser), especially since a remote may not have Alt.
- Prompt-injection scanner in this environment falsely flagged pages containing the words "Developer mode" (Chrome/Edge UI toggle name); no instructions were actually embedded.

## 9. Open questions for a prototype
1. Does `msedge --load-extension=<dir>` load in current stable Edge? With `--user-data-dir`? Does it disable the extension by default (as reported for Chrome 137)?
2. Does an unpacked extension show a modal/banner on every launch in current Chrome 150+ and Edge? Can it be silenced by policy?
3. Can an **unlisted** Edge Add-ons / Chrome Web Store item be force-installed by policy on an unmanaged Windows PC, or only installed by URL click? (Store-listed off-domain force-install is allowed per Microsoft's doc quoted above.)
4. Linux: does the managed-policy self-hosted CRX force-install work on an unmanaged box with the target distro's Chrome? Does it survive the first-run and profile creation?
5. Do extensions run in `msedge --app=` and `chrome --app=` windows? In Chrome `--kiosk`? In Edge `--kiosk` with `--edge-kiosk-type=fullscreen` and an "Allow in InPrivate" pre-set (unlikely)?
6. Shadow DOM, iframes, canvas UI per service (Section 2.3). Dump the tree on Netflix, Disney+, Prime, Max home, title, and player pages.
7. Do any of the four sites respond differently (login challenge, degraded quality, blocks) when a debug port is attached or when `Input.dispatchKeyEvent` is used?
8. Does Netflix's `playerApp.getAPI().videoPlayer.getVideoPlayerBySessionId(...).seek()` still work on today's build? Same for Disney+/Prime/Max: is `video.currentTime` safe there?
9. Native Messaging from an extension to the launcher: does it work with a policy-installed CRX? What is the allowed-origins registration on Linux/Windows? (Standard mechanism; not researched here.)
10. Does `SendInput` reach Edge in fullscreen when the launcher runs at normal integrity? Does a `uinput` keyboard satisfy `requestFullscreen` user activation? (Expected yes, needs proof.)
11. Does `--enable-spatial-navigation` still exist in Chrome/Edge in 2026 and how does it behave on Netflix?
12. Skip-intro and audio/subtitle menu selectors and reliability on all four services.
13. js-spatial-navigation and BBC LRUD status (not researched here).
