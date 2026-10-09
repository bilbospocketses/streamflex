# R5: Firefox and Safari, are they worth supporting?

Researched 2026-09-28 with WebSearch/WebFetch only. Nothing was installed or run. Context read first: `11-drm-resolution.md` (R1) and `13-injection-engineering.md` (R3).

**Evidence tags**
- VERIFIED = an official or primary page was fetched, or several independent recent sources agree.
- REPORTED = a single source, a forum, a search snippet of a page I could not fetch, or a secondary article.
- UNKNOWN = nothing found, or sources conflict.
- INFERENCE = my own reasoning, not sourced. Marked so it is not mistaken for a finding.

Caveats on method: WebFetch summaries are made by a small model, so wording is paraphrase. Several Apple Developer pages and a few Mozilla pages (support.mozilla.org, bugzilla 1959827) would not fetch (JS-rendered, 403, 502). Where I only had a search snippet I say so. One search (the SDL2 macOS signing query) tripped the local prompt-injection scanner ("Role-Playing/DAN"); the returned text was ordinary code-signing content with no instructions in it, and nothing from it was acted on.

---

## 0. Bottom line

1. **Neither browser raises video quality on Windows or Linux.** Firefox is capped at 1080p on Netflix on every OS (Netflix's own table), and Widevine on Firefox is L3. Chrome and Edge reach 4K on Netflix on Windows (R1). Firefox gains nothing on Prime, Disney+, Max or Apple TV+ either, since those are 1080p or lower in every desktop browser.
2. **Firefox's real value is deployment and kiosk robustness, not quality.** Policy force-install has no documented domain or managed-device gate, so it should work on Windows Home. Firefox `-kiosk` is not documented as disabling extensions (Edge `--kiosk` officially does). Chrome removed `--load-extension`; Firefox has no such removal. That makes Firefox the least fragile injection target on paper.
3. **Firefox on Linux is the awkward one** because Ubuntu ships Firefox as a snap. Native messaging from a snap needs a portal. The older WebExtensions portal is an Ubuntu distro patch; the newer `xdg-native-messaging-proxy` path landed in Firefox 157 behind a pref and is reported available on Ubuntu 26.04+. The unconfined route (Mozilla's own tarball or APT build) avoids all of it.
4. **Safari is the only browser that gives 4K HDR/Dolby Vision on Netflix on a Mac and 4K HDR on Apple TV+**, but only those two services benefit. Everything else stays at 1080p or lower. The cost is a new platform: a macOS launcher port, an Xcode container app, a paid developer account, notarization, and no silent enablement without a supervised MDM.
5. **One codebase serves Chrome, Edge and Firefox** with a small build step (two manifests, `browser.*` namespace). Safari shares the JavaScript but needs a Swift host app and gives up closed-shadow-root access.

---

## 1. Firefox

### 1.1 Deployment

**Unlisted, self-distributed AMO signing**
- Process: sign through the AMO Developer Hub web upload, `web-ext sign` (v8+ supports first and update submissions), or the Add-on Create API v5 / Signing API v4. VERIFIED: https://extensionworkshop.com/documentation/publish/signing-and-distribution-overview/
- Review: automated validation before signing. "It can take up to 24 hours for your submission to be signed and published, or longer if your submission is selected for manual review." All add-ons remain subject to manual review at any time afterwards. VERIFIED: same page.
- Store-side review is bypassed for self-distribution (no listing review). VERIFIED: https://extensionworkshop.com/documentation/publish/self-distribution/
- An add-on ID in `browser_specific_settings.gecko.id` is required to publish. VERIFIED: https://extensionworkshop.com/documentation/develop/manifest-v3-migration-guide/
- Comparison table on MDN lists Firefox review as "seconds plus post-publish review" versus Chrome under 1 hour. REPORTED (MDN, secondary summary): https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Build_a_cross_browser_extension

**Updates (`update_url`)**
- Put `browser_specific_settings.gecko.update_url` in the manifest. Firefox polls it and installs higher versions. Update manifest is JSON: `addons.<id>.updates[]` with `version`, `update_link`, optional `update_hash` (`sha256:`/`sha512:`), optional `applications.gecko.strict_min_version`. Must be hosted on HTTPS; `update_link` must be HTTPS or carry a hash. VERIFIED: https://extensionworkshop.com/documentation/manage/updating-your-extension/
- Each new XPI must itself be signed, so every extension release costs one AMO signing round (up to 24 h). VERIFIED (inferred from the two pages above).
- Without `update_url`, Firefox checks AMO for a newer listed version. VERIFIED: self-distribution page.
- XPI must be served as `Content-Type: application/x-xpinstall`. VERIFIED: self-distribution page.
- Default poll interval 86400 s; testable down to 120 s via `extensions.update.interval`. VERIFIED: updating page.
- Per-site selector data is fetched remotely as JSON already (decided), so most day-to-day changes need no re-signing. Only code changes do.

**Enterprise policies, Windows**
- File route: `distribution\policies.json` next to `firefox.exe`. VERIFIED: https://mozilla.github.io/policy-templates/ and https://extensionworkshop.com/documentation/enterprise/enterprise-development/
- Registry route: `HKLM\Software\Policies\Mozilla\Firefox` and `HKCU\Software\Policies\Mozilla\Firefox`. `ExtensionSettings` is stored as `Software\Policies\Mozilla\Firefox\ExtensionSettings` (REG_MULTI_SZ) per Mozilla's admin reference: https://firefox-admin-docs.mozilla.org/reference/policies/extensionsettings/ (VERIFIED). A third-party guide puts it under a `...\Firefox\Extensions` subkey (REPORTED), so **check the exact key on a real install**.
- Firefox Windows Home: no source says Home is excluded. Vendor guides (Pendo, Nexthink) deploy the same `policies.json` for unmanaged setups. Neither Mozilla's `ExtensionSettings` page nor its overview page mentions a domain or managed-device requirement. Verdict: **no documented gate, VERIFIED by absence plus vendor practice; confirm on a Windows Home box.** Pages: https://firefox-admin-docs.mozilla.org/reference/policies/extensionsettings/ , https://support.pendo.io/hc/en-us/articles/21165410719771-Install-on-Firefox-for-Windows-or-macOS-using-policies-json
- Gotcha: a Firefox installer update can remove the `distribution` folder, deleting `policies.json`. REPORTED: https://support.mozilla.org/en-US/questions/1237979 (search snippet). The registry route survives updates. INFERENCE: prefer the registry on Windows, or re-write the file at each launcher start.
- Scoping: policies are per Firefox install, not per profile. INFERENCE: if the user also uses Firefox normally, a `force_installed` overlay extension would apply to their normal browsing too, unless the launcher ships its own Firefox copy (portable/tarball) with its own `distribution` folder. This should be tested.

**`ExtensionSettings` / `force_installed`** (VERIFIED, https://firefox-admin-docs.mozilla.org/reference/policies/extensionsettings/)
- `installation_mode`: `allowed`, `blocked`, `force_installed` (cannot be removed by the user), `normal_installed` (user can disable).
- `install_url`: where to fetch the XPI. As of **Firefox 153 it is optional for AMO-hosted extensions** (Firefox looks up by ID). `file:///` URLs are supported and Firefox reinstalls when the file changes.
- `updates_disabled` (Firefox 89+); from **Firefox 152**, setting it to `false` forces automatic updates and stops the user disabling them.
- `restricted_domains`, applies to the default `*` entry only.
- **No domain-join or managed-device restriction is documented.** This is the opposite of Chrome and Edge on Windows (R3 section 1.3).
- Even force-installed extensions must implement their own disclosure and consent experience. VERIFIED: https://extensionworkshop.com/documentation/enterprise/enterprise-development/

**Enterprise policies, Linux**
- System-wide path: `/etc/firefox/policies/policies.json`. VERIFIED (Mozilla docs): https://mozilla.github.io/policy-templates/ and https://firefox-admin-docs.mozilla.org/guides/getting-started/
- **Ubuntu snap Firefox reads it:** Ubuntu's doc says the snap uses `/etc/firefox/policies/policies.json` as a fallback because the snap filesystem is read-only. REPORTED (the Ubuntu page predates Firefox 91, so it may have changed): https://discourse.ubuntu.com/t/how-to-customize-the-firefox-snap-with-enterprise-policies/23074 and search summary of the Ubuntu doc.
- Counter-evidence: GitHub issue mozilla/policy-templates #936 (Ubuntu 22.04, snap Firefox 103) reports the snap not applying a policy in that location; no resolution in the thread. REPORTED (single issue, 2022): https://github.com/mozilla/policy-templates/issues/936
- A Mozilla Discourse thread (Firefox snap 134.0.1, 2025) says `file://` XPI installs via policy worked in snap 116 but broke in 134 unless the XPI sits in `/etc/firefox/policies`. No answer was given. REPORTED: https://discourse.mozilla.org/t/ubuntu-firefox-snap-local-extension-addon-registration/139104 . **Design consequence (INFERENCE): on the snap, use an `https://` `install_url`, not `file://`.**
- Net for the question "does the snap read `/etc/firefox/policies`?": **REPORTED yes, with a 2022 counter-report and a 2025 file-access regression. Needs a hands-on test on 24.04 and 26.04.**
- Flatpak Firefox: Mozilla has a separate "Configuring a Flatpak package" guide (not fetched). UNKNOWN.

### 1.2 Native messaging

**Windows** (VERIFIED, https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_manifests)
- Registry key `HKCU\SOFTWARE\Mozilla\NativeMessagingHosts\<name>` (per user) or `HKLM\...` (global). Default value = full path to the host manifest JSON. Since Firefox 64 the 32-bit registry view is checked first, then the native view.
- Host manifest uses **`allowed_extensions`** (a list of gecko IDs), not Chrome's `allowed_origins`. VERIFIED: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging
- The Chrome and Firefox host manifests are different files, so the launcher installs two, plus two registry keys (`HKCU\Software\Google\Chrome\NativeMessagingHosts`, Firefox as above). Same host executable can serve both.

**Linux, unconfined Firefox** (VERIFIED, same Native_manifests page)
- Global: `/usr/lib/mozilla/native-messaging-hosts/<name>.json` or `/usr/lib64/mozilla/...`. Per user: `~/.mozilla/native-messaging-hosts/<name>.json`.

**Linux, snap Firefox**
- Historic status: native messaging did **not** work in the snap. Ubuntu added it through the WebExtensions XDG desktop portal as a distro patch, available on Ubuntu 22.04 and later (needs xdg-desktop-portal 1.14.4-1ubuntu2~22.04.1 or newer). The portal looks up manifests in `~/.mozilla/native-messaging-hosts/` and `/usr/lib/mozilla/native-messaging-hosts/`, and the host runs **outside** the sandbox. VERIFIED (Ubuntu's own call-for-testing, July 2022): https://discourse.ubuntu.com/t/call-for-testing-native-messaging-support-in-the-firefox-snap/29759
- Test results in that thread (Aug 2022) were mixed: GNOME Shell and Plasma integration worked; KeePassXC and 1Password extensions failed, blamed on upstream. REPORTED: same thread. No 2024-2026 update in that thread.
- The old portal "isn't widely available yet in a release of the XDG desktop portals project"; it has **never been in an upstream xdg-desktop-portal release** and ships only downstream in Ubuntu. VERIFIED (Firefox source docs): https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/native-messaging-portal-design.html . Same statement in search results about the freedesktop list and PR #705 (REPORTED): https://github.com/flatpak/xdg-desktop-portal/pull/705
- **Newer path: `xdg-native-messaging-proxy`** (D-Bus service, no per-extension user prompt, `GetManifest` / `Start` / `Close`). Firefox docs say it "is available in Ubuntu 26.04+". VERIFIED (Firefox docs, same page). The Firefox frontend bug 1955255 ("[flatpak] [snap] Implement xdg-native-messaging-proxy frontend") is RESOLVED FIXED on 2025-06-18, target **Firefox 157**. It is off by default: pref `widget.use-xdg-desktop-portal.native-messaging-proxy` = 0 (off), 1 (always), 2 (auto-detect). The snap maintainers must flip the pref. VERIFIED (bugzilla fetch): https://bugzilla.mozilla.org/show_bug.cgi?id=1955255 . **Whether Ubuntu's current snap has flipped it: UNKNOWN.**
- Ubuntu 26.04 LTS exists; 26.04.1 was released 2026-08-27 (REPORTED, search summary of Wikipedia).
- The proxy repo warns that exposing it is "potentially INSECURE" because a native host can be a sandbox-escape vector; hosts must not be modifiable by the confined browser. VERIFIED: https://github.com/flatpak/xdg-native-messaging-proxy and the Firefox docs above. Our host is a small launcher IPC bridge, so keep its manifest root-owned.
- Firefox and Chromium agreed on using this proxy rather than an xdg-desktop-portal portal. REPORTED (KeePassXC issue summarizing): https://github.com/keepassxreboot/keepassxc/issues/12327

**Linux, Flatpak Firefox**
- The Firefox docs list Flatpak as supported by the design ("not flatpak-specific"). VERIFIED (design doc). But the proxy repo moved under the Flatpak org only in April-June 2025 and, per one summary, was still going through the "make it official" step. REPORTED: https://lists.freedesktop.org/archives/flatpak/2025-April/002383.html
- Whether the Flathub Firefox build enables the pref, and which host distros carry the proxy package: **UNKNOWN.**

**Practical answer (INFERENCE from the above):** on Linux the least risky Firefox is a **non-snap, non-flatpak** one (Mozilla's tarball or APT repo). It reads `~/.mozilla/native-messaging-hosts` directly and `/etc/firefox/policies` directly. If the target must be Ubuntu's snap, native messaging is a hands-on test on 24.04 (old portal) and 26.04 (proxy).

### 1.3 API differences from Chromium MV3 that matter for a content-script overlay

| Topic | Firefox | Source / tag |
|---|---|---|
| MV3 | Supported. **MV2 also supported for the foreseeable future**, with 12 months notice promised before any change. Blocking `webRequest` kept. | VERIFIED: https://blog.mozilla.org/addons/2024/03/13/manifest-v3-manifest-v2-march-2024-update/ (also BleepingComputer summary) |
| Background | **Event pages** (`background.scripts`, non-persistent), **not** service workers. Listeners must be top-level; state goes to storage. | VERIFIED: https://extensionworkshop.com/documentation/develop/manifest-v3-migration-guide/ ; MDN cross-browser page |
| Host permissions | Separate `host_permissions` key. From Firefox 127 they show at install, and **the user can grant or revoke any host permission ad hoc**, so code must check availability. Whether a policy `force_installed` extension gets them granted automatically: **UNKNOWN, test.** | VERIFIED for the model; UNKNOWN for policy install |
| Scripting | `tabs.executeScript` etc. replaced by `scripting`. No arbitrary string code. | VERIFIED (migration guide) |
| `world: "MAIN"` | Supported from **Firefox 128 (2024-07-09)** in `scripting.executeScript`, `contentScripts.register` and the manifest `content_scripts.world` key. Page can detect and interfere, same as Chrome. | VERIFIED: https://developer.mozilla.org/en-US/docs/Mozilla/Firefox/Releases/128 (via search) and https://bugzilla.mozilla.org/show_bug.cgi?id=1736575 |
| Closed shadow roots | `browser.dom.openOrClosedShadowRoot(element)` per MDN, and the older `element.openOrClosedShadowRoot` form in content scripts per the W3C thread. **Both spellings appear, so feature-detect both.** Returns open or closed root, or `null`. Available to **isolated-world** content scripts only, not MAIN and not user scripts. | VERIFIED (MDN + W3C): https://developer.mozilla.org/en-US/docs/Web/API/Element/openOrClosedShadowRoot , https://github.com/w3c/webextensions/issues/612 |
| Chrome equivalent | `chrome.dom.openOrClosedShadowRoot` (R3). Same call shape, so a small wrapper covers both. | VERIFIED (R3, MDN) |
| Namespace | Firefox: `browser.*` with promises, `chrome.*` also present. **Chrome now ships `browser.*` from Chrome 148** (all extensions except devtools; full from 152), and `runtime.onMessage` may return a Promise. The polyfill is a no-op on Chrome 152+. | VERIFIED: https://developer.chrome.com/docs/extensions/develop/concepts/browser-namespace and MDN cross-browser page |
| Manifest keys | `browser_specific_settings.gecko.id` and `update_url` are Firefox-only. Chrome does not support that key. `background.service_worker` (Chrome) versus `background.scripts` (Firefox). | VERIFIED: MDN cross-browser page. Practical answer: **two generated manifests from one source.** |
| Key events | Content-script keyboard handling is ordinary DOM: capture-phase listeners work the same. Synthetic events are `isTrusted: false` in Firefox as in Chrome. Firefox content scripts see the page through Xray wrappers, so page-defined properties are not visible from the isolated world (use MAIN world for that). Per-service behavior with the four target sites: **UNKNOWN.** | Xray note VERIFIED (MDN cross-browser page); rest INFERENCE |
| Native messaging API | `runtime.connectNative` / `sendNativeMessage`; same API, host manifest differs (section 1.2). | VERIFIED |
| `chrome.debugger` | Not implemented in Firefox. Irrelevant if we use native messaging. | REPORTED (MDN) |

**How much is shared?** INFERENCE from the table: essentially all content-script code (spatial engine, focus ring, per-site adapters, JSON adapter fetch) is shared. What differs is per-browser glue: manifest, background entry (`scripts` vs `service_worker`), the shadow-root call wrapper, and the native host registration. With Chrome 148+ as the minimum and Firefox and Safari already on `browser.*`, the webextension-polyfill can be dropped for a new project. If Chrome 148 cannot be guaranteed on the HTPC, keep the polyfill (it stays useful because parts of Chrome still use callbacks with `runtime.lastError`, REPORTED via https://github.com/mozilla/webextension-polyfill and https://bestchromeextensions.com/2025/01/20/webextension-polyfill-cross-browser-extensions/).

### 1.4 DRM and quality in Firefox

Cross-refs to R1's table (R1 is the source for Chrome/Edge columns).

| Service | Firefox Windows | Firefox Linux | Tag |
|---|---|---|---|
| **Netflix** | **1080p** (Firefox 129+) | **1080p** per Netflix table, "not guaranteed"; users report 720p default | VERIFIED (Netflix table, fetched): https://help.netflix.com/en/node/30081 ; Linux 720p is REPORTED (R1) |
| **Prime Video** | 1080p max | **SD only** | VERIFIED (Amazon help, per R1): https://www.primevideo.com/help?nodeId=GUX9FYHU5D8LC9EJ |
| **Disney+** | 1080p or lower; "4K not available on computer browsers" | UNKNOWN (expect L3 limits) | VERIFIED for "no browser 4K" (R1 snippet); rest UNKNOWN |
| **Max / HBO Max** | UNKNOWN; no computers in the official 4K device list | UNKNOWN | R1 |
| **Hulu** | 480p reported (HDCP cannot be verified) | UNKNOWN | REPORTED (R1) |
| **Apple TV+** | 1080p (Chrome, Firefox and Edge are all "up to 1080p") | Linux not listed | VERIFIED: https://support.apple.com/en-us/119599 |
| **YouTube** | 4K (no DRM; VP9/AV1) | 4K | REPORTED (R1) |

- **PlayReady in Firefox:** Firefox 132 (October 2024) added Microsoft PlayReady on Windows "for select sites"; it enables a 1080p baseline and 4K Ultra HD "with key streaming partners", rolled out gradually and reportedly only Netflix, with Netflix doing its own A/B testing. REPORTED (single secondary source, comments included): https://www.ghacks.net/2024/10/29/firefox-132-mozilla-paves-way-for-4k-netflix-playback/ ; also Thurrott, Notebookcheck and AlternativeTo (same claim, REPORTED).
- Related prefs: `media.eme.playready.enabled` (kill switch), `media.eme.mfcdm.origin-filter.enabled` (allow list vs block list). REPORTED (search summaries). A bugzilla item (1959827, "Revert PlayReady CDM filtering back to an allow list by default (release only)") exists; the fetch failed (HTTP 502), so **the exact state after Firefox 137 is UNKNOWN**. Sources: https://bugzilla.mozilla.org/show_bug.cgi?id=1959827
- **Despite that, Netflix's own table still shows Firefox at 1080p on all three OSes** (fetched 2026-09-28; the table's last-changed date was not visible). Treat Firefox 4K on Netflix as **not available in practice, UNKNOWN whether any A/B users get it.** No source I found shows a working 2160p on Firefox in 2026.
- No source found for Firefox reaching PlayReady SL3000 (hardware) the way Chrome 140+ does. DoveRunner documents SL3000 only for Chrome. VERIFIED absence: https://docs.doverunner.com/content-security/multi-drm/advanced-guides/playready-sl3000-windows-chrome/
- **HDR, Dolby Vision, 5.1/Atmos in Firefox:** no source found for any service. Netflix's own Windows page limits HDR/DV/Atmos to Edge and the Netflix app (R1). **Treat Firefox as SDR stereo/5.1-if-lucky. UNKNOWN per service, needs hardware.**
- Linux is the same Widevine L3 ceiling for Firefox as for Chrome (R1 section Q5), so **Firefox on Linux is at parity with Chrome on Linux, no better and no worse**, aside from Netflix's table listing both at 1080p.

### 1.5 Kiosk and fullscreen

- `-kiosk URL`: "Open URL full screen without user interface. Firefox 71 and later." VERIFIED: https://wiki.mozilla.org/Firefox/CommandLineOptions
- Extensions in kiosk: **no source says extensions are disabled in `-kiosk`.** Third-party kiosk extensions exist for Firefox "launched with --kiosk", requiring Firefox 115+ (AMO listings, REPORTED): https://addons.mozilla.org/en-US/firefox/addon/kiosk-extension/ , https://addons.mozilla.org/en-US/firefox/addon/modern-kiosk/ , https://github.com/racketlogger/ModernKiosk . That is a positive indicator that WebExtension content scripts run. Contrast: Edge `--kiosk` officially does not support extensions (R3 6.1). **Confirm on the real target: content script runs, force-installed extension is enabled, native messaging connects.**
- Profile isolation: `-profile <dir>` (absolute path; relative paths not supported on macOS), `-no-remote` (no remote commands, implies new instance), `-new-instance`. VERIFIED: https://wiki.mozilla.org/Firefox/CommandLineOptions . Two `-no-remote` instances on Windows become separate processes.
- **Exit detection on Windows is a trap.** With the Windows launcher process, `firefox.exe` "always appears to exit very quickly" while the browser keeps running. `-wait-for-browser` makes the launcher process stay alive until the browser exits. VERIFIED: https://wiki.mozilla.org/Firefox/CommandLineOptions and https://bugzilla.mozilla.org/show_bug.cgi?id=1488554 (search summary, REPORTED). Bugzilla 1740619 puts the launcher process in a job when `--wait-for-browser` is set (REPORTED, title only). **Launcher design: pass `-wait-for-browser`, wait on that process handle, or place the whole process tree in a Windows Job Object.** (Chrome and Edge need the same care for their own reasons; R3.)
- Exit key: kiosk mode hides UI; Alt+F4 is the sure exit (INFERENCE, not sourced for Firefox). Because a remote may not have Alt, the launcher needs its own kill hotkey path. Same as R3.
- Fullscreen video inside kiosk: same user-activation rules as Chromium for `requestFullscreen()` (R3 3.1). Not Firefox-specific.

---

## 2. Safari (macOS only)

### 2.1 Distribution

- **Converter:** `xcrun safari-web-extension-converter` turns a Chrome/Firefox web extension into an Xcode project (macOS/iOS app + Safari Web Extension target). The Apple page did not render in my fetch, so details are REPORTED via search results: https://developer.apple.com/documentation/safariservices/converting-a-web-extension-for-safari , https://evilmartians.com/chronicles/how-to-quickly-and-weightlessly-convert-chrome-extensions-to-safari
- **Cost:** Apple Developer Program is **US$99 per membership year**, unlimited apps, region-priced. VERIFIED by several 2026 sources that agree, Apple's own page not fetched: https://developer.apple.com/programs/whats-included/ (search result), https://magora-systems.com/apple-developer-fee/
- **Route A, Mac App Store:** package the container app plus extension through App Store Connect. Only extensions distributed through the App Store were loadable without "Allow unsigned extensions" before Safari 18.4. VERIFIED (Apple forum, Apple engineer, below).
- **Route A2, unlisted App Store distribution:** Apple's "unlisted app distribution" gives a direct-link-only App Store listing. Apple's page does not limit it by platform and other summaries say it covers macOS, but **no source I fetched states macOS explicitly.** Requirements: the app must already be on the store or be submitted to App Review; the Account Holder submits the request form; an Apple-side approval. VERIFIED (Apple page): https://developer.apple.com/support/unlisted-app-distribution ; macOS coverage REPORTED: https://www.runway.team/blog/unlisted-app-distribution-on-the-app-store . App Review approving a fullscreen launcher that spawns a browser is **UNKNOWN**, and Mac App Store apps must be sandboxed, which complicates spawning Safari and talking to a launcher (INFERENCE).
- **Route B, Developer ID-signed and notarized, outside the store:** supported since **Safari 18.4** (macOS). An Apple systems engineer wrote: "Support for Developer ID-signed and notarized Safari Web Extensions was introduced in Safari 18.4 for macOS." Before that, such extensions needed the Develop-menu "Allow Unsigned Extensions", which resets each Safari launch. VERIFIED (Apple engineer, Apple forum): https://developer.apple.com/forums/thread/782005 . Safari 18.4 shipped 2025-03-31 (REPORTED, search summary of Apple release notes; the notes page itself would not fetch): https://developer.apple.com/documentation/safari-release-notes/safari-18_4-release-notes
- **Does the user still have to enable the extension by hand?** REPORTED yes: on a fresh install the user opens Safari > Settings > Extensions and ticks the extension. Per-site access is a second gate. Extensions offer "Allow for One Day", "Always Allow on This Website", "Always Allow on Every Website", "Deny", and "granting permission in the manifest doesn't automatically give your extension access to those web pages". REPORTED (multiple secondary/community sources): https://developer.apple.com/forums/thread/720885 , https://www.idownloadblog.com/2023/10/04/how-to-control-safari-extensions-on-a-website/ , https://developer.apple.com/documentation/safariservices/adjusting-website-access-permissions
- **MDM-free silent enablement: none found.** The only managed path is Declarative Device Management `com.apple.configuration.safari.extensions.settings`: extension state (Allowed / Always On / Always Off), private-browsing state, and per-domain allow or deny. It **requires a supervised device**, macOS 15 or later, via Device Enrollment (Mac) or Automated Device Enrollment. **Unsupervised Macs do not qualify.** VERIFIED (Apple deployment guide, fetched): https://support.apple.com/guide/deployment/safari-extensions-management-declarative-depff7fad9d8/web . A home Mac mini is not supervised. Verdict: **VERIFIED that silent enablement needs supervision; UNKNOWN whether a home user can self-supervise via Apple Configurator (I did not research it).**
- Safari Web Extension review time in MDN's table: 24-48 hours average. REPORTED (secondary).

### 2.2 API gaps versus Chromium

| Topic | Safari | Tag |
|---|---|---|
| Native messaging | **`runtime.sendNativeMessage`** works and calls `beginRequest` in `SafariWebExtensionHandler` (Swift). Requires `"nativeMessaging"` permission. **The receiver is the extension's own handler, not an arbitrary host executable, and the app id parameter is ignored.** `runtime.connectNative` is reported to work only if the containing app is running, and Safari "ignores the application.id parameter and only allows... a port connection with the containing macOS app". Older forum threads say `connectNative` does not work at all (thread 684828 replies address only `sendNativeMessage`). Net: **connectNative status is UNKNOWN or conflicting**; design for `sendNativeMessage` request/reply. | sendNativeMessage: VERIFIED (Apple forum, multiple threads): https://developer.apple.com/forums/thread/684828 ; connectNative: REPORTED, conflicting: https://developer.apple.com/documentation/safariservices/messaging-a-web-extension-s-native-app (page body did not fetch) |
| Reaching the launcher | The Safari extension handler is **sandboxed**. Reports say the App Sandbox is required for Safari extensions, and a sandboxed app extension cannot open an XPC connection to the container app directly, but can share an App Group (shared `UserDefaults`, named services inside the group). So the chain is content script -> background -> `sendNativeMessage` -> Swift handler -> App Group / local mechanism -> launcher. | REPORTED (Apple forums): https://developer.apple.com/forums/thread/689091 , https://developer.apple.com/forums/thread/703702 |
| Closed shadow roots | **No equivalent API.** Safari/WebKit position on exposing it is "neutral" in the WebExtensions Community Group. A shadow-root scan by walking every element is expensive. | VERIFIED (W3C issue, opened 2024-05-14, no vendor commitment): https://github.com/w3c/webextensions/issues/612 |
| Content scripts | Supported. `world: "MAIN"`: `registerContentScripts` and `scripting.executeScript` accept MAIN (Safari 16.4 reports); MAIN in the manifest `content_scripts` key was **not** supported in the reports found. Current Safari 18/26 status **UNKNOWN**. Also, a Safari 18 report says `tabs.executeScript` ignores `frameId`. | REPORTED: https://developer.apple.com/forums/thread/728849 , https://developer.apple.com/forums/thread/765629 |
| Injection timing | If the user allows the site after the page has loaded, a `DOMContentLoaded`-driven script never runs and the extension looks broken. | REPORTED (Apple compatibility guidance summary): https://developer.apple.com/documentation/safariservices/assessing-your-safari-web-extension-s-browser-compatibility |
| `webRequest` | Blocking not supported; Manifest V3 non-persistent background cannot listen to `webRequest` events. Not needed by us. | REPORTED |
| `chrome.debugger` | Not supported. Irrelevant. | REPORTED |
| Background | Supports both background pages and service workers. | REPORTED (MDN cross-browser page) |
| Namespace | `browser.*` with promises. | VERIFIED (MDN cross-browser page) |
| Key events | Standard DOM key events in content scripts. Whether Netflix/Prime/Disney+/Max react to synthetic vs trusted keys in Safari: **UNKNOWN.** Real remote keys arrive as real key events, as on other browsers. | INFERENCE |

### 2.3 FairPlay quality on macOS Safari

| Service | Safari macOS | Tag |
|---|---|---|
| **Netflix** | **Up to Ultra HD (2160p)**; Safari 14+; Ultra HD needs **macOS 11 or later**; macOS 10.15 gets Full HD. Netflix's browser table says nothing about HDR, Dolby Vision or Atmos for Safari. Secondary sources: 4K needs a Mac with a **T2 chip** (2018+) or newer Apple Silicon, a **60 Hz 4K display over an HDCP 2.2 link**, a Premium plan, ~25 Mbps; Safari is the only Mac browser for it. One source says only some Apple Silicon chips (M1 Pro/Max/Ultra, M2/M3 Pro/Max/Ultra) get 4K HDR from macOS 14.4, which is not consistent with other reports and is **UNKNOWN**. | Resolution rule VERIFIED (Netflix table, fetched): https://help.netflix.com/en/node/30081 ; hardware rules REPORTED: https://appleinsider.com/articles/20/09/30/mac-with-t2-security-chip-required-to-play-4k-netflix-streams-in-macos-big-sur , https://9to5mac.com/2020/10/01/4k-netflix-streaming/ , https://www.idownloadblog.com/2020/10/01/netflix-4k-streaming-mac-requirements/ |
| **Apple TV+** | **Up to 4K HDR in Safari on Mac**; Chrome, Firefox and Edge give up to 1080p. Dolby Vision and Atmos are described only for the Apple TV app. | VERIFIED: https://support.apple.com/en-us/119599 |
| **Disney+** | No 4K/HDR on any web browser, Safari included (Apple Community answer). | REPORTED (single community source; R1 has the official Disney statement): https://discussions.apple.com/thread/253619856 |
| **Prime Video** | Web max is HD (1080p); 4K needs a device or the Fire TV app. Chrome on Mac is reported as SD/720p, Safari sharper. | VERIFIED for "HD max" (Amazon help, R1); REPORTED for Chrome-versus-Safari |
| **Max / HBO Max** | UNKNOWN. | UNKNOWN |
| **Hulu, Peacock, Paramount+** | UNKNOWN. | UNKNOWN |
| **Atmos / 5.1 in Safari** | A MacRumors thread titled "Netflix on Safari with Dolby Atmos" exists, content not read. UNKNOWN. | UNKNOWN: https://forums.macrumors.com/threads/netflix-on-safari-with-dolby-atmos.2329766/ |

**HDCP and display:** the same rule as Windows: every path must be HDCP 2.2, and HDMI dummy plugs or cheap adapters can drop quality (R1 Q6). The macOS rule is REPORTED (secondary) for Netflix and not tested for the other services.

**Reading this:** Safari's quality upside is real for **Netflix 4K (HDR/DV per secondary sources) and Apple TV+ 4K HDR**. For Disney+, Prime, Max and the rest it is at most the same 1080p Chrome/Edge would give on Windows.

### 2.4 Porting the launcher to macOS

- **Upstream flex-launcher (complexlogic/flex-launcher) does not support macOS.** Its README says "Windows and Linux (including Raspberry Pi devices)" and lists no macOS instructions. It depends on SDL, SDL_image and SDL_ttf and uses the Unlicense (public domain). VERIFIED (fetched): https://github.com/complexlogic/flex-launcher
- **SDL2 on macOS:** supported. SDL 2 needs Xcode 6 and the 10.9 SDK at least; 32-bit Intel and macOS 10.8 support were dropped in SDL 2.24.0; it ships as a framework; both Xcode and CMake work; a `clang-fat.sh` script builds Intel + ARM universal binaries. The launcher must be packaged as a `.app` bundle. SDL 2 no longer changes the working directory, so a Finder launch starts in `/`; use `SDL_GetBasePath()`. VERIFIED (SDL repo docs, fetched): https://github.com/libsdl-org/SDL/blob/SDL2/docs/README-macos.md . Note the SDL2 branch is the maintenance branch; SDL3 exists but StreamFlex is on SDL2 (INFERENCE that SDL2 stays on macOS is fine for years).
- **Signing and notarization of the launcher itself:** required to distribute outside the Mac App Store without Gatekeeper warnings. Needs a Developer ID Application certificate ($99/year), hardened runtime (`codesign --options runtime --timestamp`), and Apple notarization. A bundled `SDL2.framework` may need library-validation handling (the "Disable Library Validation" entitlement is mentioned for external libraries). REPORTED (several how-tos; the SDL2-specific advice is one snippet): https://gist.github.com/rsms/929c9c2fec231f0cf843a1a746a416f5 , https://blog.xojo.com/2026/03/24/code-signing-on-macos-what-developers-need-to-know-part-3/ , https://help.apple.com/xcode/mac/current/en.lproj/dev033e997ca.html . The SDL README does not address signing.
- **What is missing in flex-launcher beyond SDL** (INFERENCE from its Windows/Linux-only support): process spawning and exit detection for Safari, fullscreen control, autostart (launchd), a controller/remote input path, and an HDMI-CEC or remote story. All need macOS equivalents.
- **Safari fullscreen on macOS:** Safari has no kiosk flag. AppleScript or `Ctrl+Cmd+F` gets fullscreen, but the address/tab bar can remain visible. Community kiosk AppleScripts are old (Lion/Mountain Lion era). REPORTED: https://github.com/mkb/safari-kiosk-mode , https://discussions.apple.com/thread/6838637 . Third-party apps (Kiosker) exist for a locked-down web view. INFERENCE: scripting Safari needs macOS Automation and Accessibility (TCC) permission prompts on first run. Not sourced; needs a test.
- **Escape hatch:** a `WKWebView` or a Safari-derived shell does **not** get FairPlay for 3rd-party streaming services in the same way (UNKNOWN; not researched, and R1's advice is to avoid embedding).
- Total port: launcher port (medium) + Xcode container app + Swift native-message handler + App Group bridge (medium-high) + signing/notarization pipeline (medium). All on hardware I cannot test here.

---

## 3. Difficulty and value verdicts

Effort is relative to the decided Chrome/Edge extension work. "Quality gained" is versus the best Chrome/Edge result on the same OS.

| Target | Effort | Quality gained | Other value | Verdict |
|---|---|---|---|---|
| **Firefox, Windows** | **Low to medium.** Shared content-script code; add a Firefox manifest, an AMO-signing step, a `policies.json` or registry write, one native-host registry key + manifest with `allowed_extensions`, and `-wait-for-browser` handling. | **None; negative for Netflix** (1080p versus Edge/Chrome 4K). No HDR/DV/Atmos path found. | Independent of Chrome's `--load-extension` removal and of the Chrome/Edge stores; no domain gate documented; `-kiosk` not documented as blocking extensions (Edge's is). | **Worth doing as a second-tier fallback**, not as the default. Sensible if the store route for Chrome/Edge turns out fragile. |
| **Firefox, Linux** | **Medium to high** on Ubuntu (snap: policy quirks, `file://` regression, native messaging via portal or proxy of uncertain status); **medium** if the launcher uses a non-snap Firefox. | **None**; at parity with Chrome on Linux (both Widevine L3; Prime SD). | Removes the need for Google's Chrome .deb; avoids Chrome's Linux policy path. | **Defer.** Chrome on Linux with a managed-policy CRX (R3) is simpler. Reconsider if Chrome is unacceptable to the user. |
| **Safari, macOS** | **High.** New platform: launcher port, Xcode container app, Swift handler, sandbox bridge to the launcher, $99/year, Developer ID + notarization (Safari 18.4+) or App Store review, manual enablement plus per-site allow, no closed-shadow-root API, `sendNativeMessage` only. | **High for two services** (Netflix 4K HDR/DV, Apple TV+ 4K HDR). **None for the rest** (Disney+, Prime, Max still 1080p or lower). | None beyond quality; Mac mini HTPC users are a niche. | **Do not build now.** Revisit only if there is a concrete demand for Mac HTPCs and Netflix/Apple TV+ 4K. |

---

## 4. Open questions for a hands-on test

Firefox
1. Windows Home: does `HKLM\Software\Policies\Mozilla\Firefox\ExtensionSettings` (or `distribution\policies.json`) with `force_installed` and an HTTPS `install_url` install a self-signed-by-AMO unlisted XPI on an unmanaged machine? Which registry key form is right (`ExtensionSettings` value versus `Extensions` subkey)?
2. Does a force-installed extension get its `host_permissions` granted automatically, or does Firefox leave them un-granted and require user action?
3. Does the extension run under `-kiosk -profile <dir> -no-remote` with a policy-installed XPI? Does `-kiosk` change anything about private windows or extension enablement?
4. Does `-wait-for-browser`, or a Job Object, give reliable exit detection in kiosk on Windows?
5. Does Netflix on a current Firefox release ever get 2160p through PlayReady (any A/B users)? What does Stats for Nerds show, and does the answer change with HDCP 2.2 hardware?
6. Snap Firefox on Ubuntu 24.04 and 26.04: does `/etc/firefox/policies/policies.json` apply? Does an HTTPS `install_url` install? Does the native-messaging proxy pref default to on in the snap, and does a `connectNative` round trip work?
7. Flathub Firefox: is `widget.use-xdg-desktop-portal.native-messaging-proxy` enabled, and does the proxy exist on the target distro?
8. Firefox content script: is the closed-shadow-root API spelled `browser.dom.openOrClosedShadowRoot` or `element.openOrClosedShadowRoot` in the current release? (Feature-detect both.)
9. Do Netflix, Prime, Disney+ and Max respond to synthetic (untrusted) keyboard events in Firefox the same way as in Chrome?

Safari
10. Does a Developer ID-notarized Safari extension in an app installed by hand still need per-site "Always Allow" and the Settings > Extensions checkbox on Safari 18.4+ and the current Safari? Can a script pre-set them (private APIs are out of scope)?
11. Can an unsupervised Mac be supervised by the owner (Apple Configurator) to use the DDM Safari extension payload? Practical for a home user?
12. Does `runtime.connectNative` work in current Safari on macOS, and what is the lowest-latency path from the Swift handler to the launcher (App Group file, distributed notification, local socket)?
13. Which Apple Silicon Macs actually get Netflix 2160p, HDR and Dolby Vision, and does Atmos work in Safari? Which Macs fail with an AV receiver or dummy plug in the chain?
14. Do Disney+, Prime, Max, Hulu and Peacock behave differently in Safari versus Chrome on macOS? (No official statements found.)
15. Can Safari be put in a true kiosk-like fullscreen (no toolbar) without third-party apps, and what TCC permissions does the launcher need to script it?
16. Would App Review accept an unlisted macOS launcher plus Safari extension that drives a third-party browser, or does Developer ID distribution have to be used?

---

## 5. Sources

**Mozilla / Firefox (primary)**
- Extension Workshop, signing and distribution overview: https://extensionworkshop.com/documentation/publish/signing-and-distribution-overview/
- Extension Workshop, self-distribution: https://extensionworkshop.com/documentation/publish/self-distribution/
- Extension Workshop, updating / update manifests: https://extensionworkshop.com/documentation/manage/updating-your-extension/
- Extension Workshop, enterprise development: https://extensionworkshop.com/documentation/enterprise/enterprise-development/
- Extension Workshop, MV3 migration guide: https://extensionworkshop.com/documentation/develop/manifest-v3-migration-guide/
- Mozilla policy templates (overview) and Firefox admin docs: https://mozilla.github.io/policy-templates/ , https://firefox-admin-docs.mozilla.org/reference/policies/extensionsettings/ , https://firefox-admin-docs.mozilla.org/guides/getting-started/
- MDN native messaging and native manifests: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging , https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_manifests
- MDN build a cross-browser extension: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Build_a_cross_browser_extension
- MDN `openOrClosedShadowRoot`: https://developer.mozilla.org/en-US/docs/Web/API/Element/openOrClosedShadowRoot
- MDN Firefox 128 release notes (via search): https://developer.mozilla.org/en-US/docs/Mozilla/Firefox/Releases/128
- Bugzilla, world MAIN: https://bugzilla.mozilla.org/show_bug.cgi?id=1736575
- Firefox command line options: https://wiki.mozilla.org/Firefox/CommandLineOptions
- Firefox source docs, native messaging for a confined Firefox: https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/native-messaging-portal-design.html
- Bugzilla 1955255 (native messaging proxy frontend): https://bugzilla.mozilla.org/show_bug.cgi?id=1955255
- Bugzilla 1488554, 1740619 (launcher process; search summaries): https://bugzilla.mozilla.org/show_bug.cgi?id=1488554 , https://bugzilla.mozilla.org/show_bug.cgi?id=1740619
- Mozilla add-ons blog, MV3/MV2 March 2024 update: https://blog.mozilla.org/addons/2024/03/13/manifest-v3-manifest-v2-march-2024-update/

**Ubuntu / Linux sandboxing**
- Ubuntu, native messaging in the Firefox snap (call for testing): https://discourse.ubuntu.com/t/call-for-testing-native-messaging-support-in-the-firefox-snap/29759
- Ubuntu, Firefox snap enterprise policies: https://discourse.ubuntu.com/t/how-to-customize-the-firefox-snap-with-enterprise-policies/23074
- policy-templates issue 936: https://github.com/mozilla/policy-templates/issues/936
- Mozilla Discourse, Firefox snap local XPI: https://discourse.mozilla.org/t/ubuntu-firefox-snap-local-extension-addon-registration/139104
- xdg-native-messaging-proxy: https://github.com/flatpak/xdg-native-messaging-proxy ; freedesktop list: https://lists.freedesktop.org/archives/flatpak/2025-April/002383.html ; xdg-desktop-portal PR 705: https://github.com/flatpak/xdg-desktop-portal/pull/705 ; KeePassXC 12327: https://github.com/keepassxreboot/keepassxc/issues/12327

**Chrome (cross-browser)**
- Chrome, browser namespace: https://developer.chrome.com/docs/extensions/develop/concepts/browser-namespace
- webextension-polyfill: https://github.com/mozilla/webextension-polyfill
- W3C WebExtensions CG issue 612: https://github.com/w3c/webextensions/issues/612

**Streaming quality**
- Netflix supported browsers: https://help.netflix.com/en/node/30081
- Apple TV+ web playback: https://support.apple.com/en-us/119599
- gHacks, Firefox 132 PlayReady: https://www.ghacks.net/2024/10/29/firefox-132-mozilla-paves-way-for-4k-netflix-playback/
- Bugzilla 1959827 (fetch failed, snippet only): https://bugzilla.mozilla.org/show_bug.cgi?id=1959827
- DoveRunner PlayReady SL3000 (Chrome only): https://docs.doverunner.com/content-security/multi-drm/advanced-guides/playready-sl3000-windows-chrome/
- Netflix Mac 4K (secondary): https://appleinsider.com/articles/20/09/30/mac-with-t2-security-chip-required-to-play-4k-netflix-streams-in-macos-big-sur , https://9to5mac.com/2020/10/01/4k-netflix-streaming/ , https://www.idownloadblog.com/2020/10/01/netflix-4k-streaming-mac-requirements/
- Disney+ Safari HDR (community): https://discussions.apple.com/thread/253619856

**Apple / Safari**
- Developer ID Safari extensions need Safari 18.4: https://developer.apple.com/forums/thread/782005 ; release notes: https://developer.apple.com/documentation/safari-release-notes/safari-18_4-release-notes
- Converter and distribution docs (bodies did not fetch; titles/snippets only): https://developer.apple.com/documentation/safariservices/converting-a-web-extension-for-safari , https://developer.apple.com/documentation/safariservices/distributing-your-safari-web-extension , https://developer.apple.com/documentation/safariservices/messaging-a-web-extension-s-native-app , https://developer.apple.com/documentation/safariservices/assessing-your-safari-web-extension-s-browser-compatibility
- Native messaging forum threads: https://developer.apple.com/forums/thread/684828 , https://developer.apple.com/forums/thread/689091 , https://developer.apple.com/forums/thread/703702
- `world: "MAIN"` and `frameId` reports: https://developer.apple.com/forums/thread/728849 , https://developer.apple.com/forums/thread/765629
- Per-site permission behavior: https://developer.apple.com/forums/thread/720885 , https://developer.apple.com/documentation/safariservices/adjusting-website-access-permissions
- Safari extension management (DDM, supervised only): https://support.apple.com/guide/deployment/safari-extensions-management-declarative-depff7fad9d8/web
- Unlisted app distribution: https://developer.apple.com/support/unlisted-app-distribution ; https://www.runway.team/blog/unlisted-app-distribution-on-the-app-store
- Apple Developer Program fee: https://developer.apple.com/programs/whats-included/ ; https://magora-systems.com/apple-developer-fee/
- flex-launcher: https://github.com/complexlogic/flex-launcher
- SDL2 macOS README: https://github.com/libsdl-org/SDL/blob/SDL2/docs/README-macos.md
- macOS signing/notarization how-tos: https://gist.github.com/rsms/929c9c2fec231f0cf843a1a746a416f5 , https://blog.xojo.com/2026/03/24/code-signing-on-macos-what-developers-need-to-know-part-3/ , https://help.apple.com/xcode/mac/current/en.lproj/dev033e997ca.html
- Safari kiosk (old): https://github.com/mkb/safari-kiosk-mode , https://discussions.apple.com/thread/6838637
