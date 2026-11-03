# 29. Emby, and the declarative adapter format

Desk research for StreamFlex todo item 24, written 2026-10-27. Read-only, WebSearch/WebFetch only, nothing installed.
Grades: **VERIFIED** = I fetched the primary page or source and read the quoted text; **REPORTED** = a search snippet,
a forum post, or a page I fetched that is itself second-hand; **UNKNOWN** = not established. Where a fetch tool
summarized a page, the quoted strings are the tool's extraction of the page, not my own reading of raw HTML; that is
noted where it matters.

Context read first: SYNTHESIS.md 4.5, 4.7, 4.8; 23-services-candidates.md (Jellyfin/Plex rows); the adapter source's
interface (its adapters and how their hashes are computed). One grep of the overlay folder found no earlier mention of
Emby.

## Summary table

| # | Question | Answer | Grade |
|---|---|---|---|
| 1 | Does Emby's web app have a 10-foot layout? | Yes: a **Display mode** setting (Auto / Desktop / Mobile-Tablet / TV) in the web app; `tv.emby.media` is "the same thing" as `app.emby.media` "except that tv.emby.media defaults to TV display mode" | VERIFIED (staff, Dec 2022) for TV default; REPORTED for the four-way list |
| 2 | Switched on how? | Settings -> Display -> Display mode, or by loading `tv.emby.media`. Not a URL flag or user agent. | REPORTED (forum), VERIFIED for tv.emby.media |
| 3 | Premiere gating | Browser playback **in TV display mode** requires Premiere; Desktop mode plays free. Feature matrix has a "TV Mode" row under "PC or Mobile Browser" with one checkmark, column not recoverable from the fetch | VERIFIED that staff said it (Dec 2023, Dec 2024); matrix column UNKNOWN |
| 4 | Arrow-key navigation | Emby staff, 2021: outside TV display mode "you will likely have trouble navigating with the remote control". Desktop mode needs Tab/mouse at times (search snippet of Windows accessibility doc) | VERIFIED (2021 quote), REPORTED (rest) |
| 5 | DRM | Not checked for Emby. Direct play of the user's own media is the model; I fetched nothing that states DRM status | UNKNOWN |
| 6 | Login | `app.emby.media`: server credentials from the admin; Emby Connect with PIN. Local web app is served by the server itself | REPORTED (search snippets of Quick Start / connect.html) |
| 7 | Single-page app with stable selectors? | SPA with hash routes and document-level `viewbeforeshow`/`viewshow` events. Class/attribute names used in the wild: `div[is='emby-scroller']`, `.emby-button`, `.mainDetailButtons`, `select.selectSource`. **Not stable**: a popular userscript's changelog says "Adapted to Emby 4.9.0.40, refactored selectors" | VERIFIED (script source read), REPORTED (instability) |
| 8 | Shadow DOM / iframes | Not established. Nothing I read mentions either. The `is='...'` attribute suggests customized built-in elements, which are light DOM | UNKNOWN (inference only) |
| 9 | Open source? | **No.** emby-web-mobile repo was deleted and was never licensed; staff confirmed `tv.emby.media` is closed-source (2017). Selectors cannot be read from source | VERIFIED (2017 staff), REPORTED (Wikipedia: closed since 3.5.3) |
| 10 | MV3 store rule for remote data | Remote **data** allowed, remote **logic** not; "Building an interpreter to run complex commands fetched from a remote source, even if those commands are fetched as data" is a listed violation | VERIFIED |
| 11 | Firefox rule | "Add-ons must be self-contained and not load remote code for execution." No sentence about data vs code | VERIFIED |
| 12 | Precedent for a rule interpreter in the stores | **Consent-O-Matic**: JSON rules, ~15 action types (click, wait, ifcss, foreach...), remote-updated list, listed on Chrome Web Store and AMO, moved to MV3 in v1.1.0. **AdGuard**: rejected repeatedly for remote script rules and parameterized scriptlets | VERIFIED (both sources fetched); store-live status of C-O-M MV3 build REPORTED |
| 13 | Does any prior art have a data-driven per-site nav format? | **No true match.** js-spatial-navigation sections are the closest (selector, restrict, enterTo, leaveFor, defaultElement). HTLauncher has none (per-site code). | VERIFIED |
| 14 | Safe-rollout practice | TUF: version-number rollback defense, expiration; Remote Settings: signed collections with a review/preview workflow (headings only fetched). Consent-O-Matic: jittered 22-48 h refresh, cache fallback, no version check | VERIFIED (TUF page, C-O-M code); Remote Settings details UNKNOWN |

---

## Part A. Emby

### A1. Layout modes, TV layout, and how it is switched on

- **tv.emby.media versus app.emby.media.** Staff (Luke, 2022-12-26): "they are the same thing, except that
  tv.emby.media defaults to TV display mode" and "You can configure app.emby.media to look the same way in Display
  settings." A moderator added that client-server connections are direct in every case and "the hosted web apps only
  supply the interface." **VERIFIED**: https://emby.media/community/topic/115000-tvembymedia-vs-appembymedia
  - Same thread: connection failures on tv.emby.media; staff had "applied for an exemption for now for app.emby.media
    but have not done this for tv.emby.media" and advised app.emby.media. The exemption is not named (mixed-content or
    private-network access is my guess, **UNKNOWN**). So tv.emby.media may be a dead end today (2022 statement;
    current state **UNKNOWN**).
- **Display mode list.** A search snippet reports a moderator listing the modes as Desktop / Mobile-Tablet / TV plus Auto,
  under Settings -> Display -> Display mode. **REPORTED** (snippet; I did not fetch the thread it came from).
  Related fetched thread: https://emby.media/community/topic/104189-display-mode-on-android-tv (staff 2021-12-11: the
  Android TV app "was designed specifically for TV use so does not have display mode"; "if you put it in anything
  other than TV display mode, you will likely have trouble navigating with the remote control." **VERIFIED**).
- **Emby Theater/older desktop app** has the same Display Mode setting with Desktop and TV values (June 2017 thread):
  https://emby.media/community/topic/48358-emby-theater-in-desktop-mode **VERIFIED**. Not the same product as the web
  app; shows the setting's long history.
- **Auto mode behavior** (reported as often choosing Desktop in a desktop browser; one user reported a server update
  flipping them to TV) **REPORTED**, search snippet only.

### A2. Premiere gating

- Staff (Luke, 2023-12-12): "TV display mode requires Emby Premiere." Moderator the same day: "Not if in TV Display
  mode" (about free browser playback). https://emby.media/community/topic/123927-ps4-web-paywall **VERIFIED** (the quote
  is a staff statement; I could not see the surrounding UI).
- Moderator, 2024-12-31: "Browser playback in TV mode requires active Premiere subscription. Full playback is free only
  in Desktop mode." Staff asked whether it "stops playback or just tells you about Premiere" and no one called it a bug.
  https://emby.media/community/topic/135022-premiere-prompt-on-browser **VERIFIED** (a moderator, not staff, wrote the
  rule; reported as such).
- Feature matrix, https://emby.media/support/articles/Premiere-Feature-Matrix.html: under "PC or Mobile Browser" rows
  "Full Playback" (checkmarks in both Free and Premiere columns), "Live TV", and **"TV Mode"** (one checkmark). The
  flattened fetch lost which column the single checkmark is in. **UNKNOWN** which column; consistent with the staff
  statements that it is Premiere.
- https://emby.media/premiere.html (VERIFIED, quoted): "Emby TV app: The latest and greatest Emby app for your TV. Runs
  on Windows, Xbox One, PS4, Chrome, Firefox, and Microsoft Edge." So Emby itself positions the browser TV layout as a
  Premiere product for those browsers.
- **Consequence for StreamFlex:** a StreamFlex user on a free Emby server will be in Desktop display mode
  unless their Emby account has Premiere. The overlay must work against the **Desktop-mode DOM**. This is the strongest
  Emby-specific finding.

### A3. Keyboard / arrow navigation

- Official key list, https://emby.media/support/articles/Keyboard-and-Remote-Support.html (**VERIFIED**, extracted):
  covers Web app, Windows, Linux, Tizen, Xbox One, PS4. Lists alt-left / alt-b (back), esc (back), left/right
  (rewind/fast-forward while the on-screen display is hidden), shift+left/right, spacebar and ctrl-p (play/pause), home
  (app home), page-up/page-down (list scroll). **Not listed: up, down, Enter, any arrow focus movement.** The page
  mentions neither TV mode nor desktop mode.
- Search snippets (**REPORTED**, 2017-2020, mostly Windows app): "arrow key navigation is only supported in TV display
  mode"; a 2020 thread where arrows would not enter the seasons section while Tab worked.
- **Implication:** in Desktop mode, native arrow focus movement is weak or absent, so the overlay's geometric engine
  is needed (inference). Emby's own `esc` and `alt-left` keys are usable as Back targets, and **`home` is bound by the
  site** (conflict with StreamFlex's Home semantics: inference, check on a live server).

### A4. DOM, SPA behavior, selectors, source availability

All from the source of the Greasy Fork userscript "embyLaunchPotplayer" (v1.1.20, Emby 4.9.0.40, Jellyfin 10.8-10.9),
https://greasyfork.org/en/scripts/514529-embylaunchpotplayer/code, read through the fetch tool's extraction (so exact
strings are the tool's; treat them as **VERIFIED to the extent the extraction is faithful**, and re-check on a live
server):

- SPA with view lifecycle events on `document`: `viewbeforeshow` with `e.detail.contextPath` and `e.detail.params`;
  detail pages recognized by `contextPath.startsWith("/item?id=")`. Routes are hash-based (the script reads the id from
  `window.location.hash` with `/\?id=([A-Za-z0-9]+)/`). The script also uses a `MutationObserver` on `document.body`
  (childList + characterData + subtree).
- Selectors the script depends on (Emby side): `div[is='emby-scroller']:not(.hide)` scoping, `.mediaInfo`,
  `.btnManualRecording`, `.mainDetailButtons`, `select.selectSubtitles`, `select.selectSource:not([disabled])`; Emby's
  own button classes `emby-button`, `detailButton`, `button-flat`. Visible/hidden pages are toggled with a `.hide`
  class (so "current view" = the one without `.hide`).
- The Emby client object `ApiClient` is a page global (`ApiClient.getItem`, `getItems`, `accessToken()`); it lives in
  the MAIN world, not reachable from an isolated-world content script (inference from extension isolation rules,
  not an Emby statement).
- **Stability:** same script's changelog 1.1.20: "Adapted to Emby 4.9.0.40, refactored selectors" (**REPORTED**:
  selectors changed between Emby releases, no cadence known).
- `viewshow` fires before cards are rendered (staff 2019-09-20: "There isn't an event for that, sorry." "Or mutation
  observer"). https://emby.media/community/topic/77712-viewshow-event/ **VERIFIED**. So an adapter cannot rely on a
  route event for initial focus; it needs "wait until selector present" semantics.
- **Open source status:** Staff 2017-08-16: "That repository was not licensed and, therefore, cannot be redistributed in
  any form" (emby-web-mobile, later deleted); Luke "Correct yes" to "tv.emby.media app is closed-source and
  proprietary". https://emby.media/community/topic/49862-source-code-license **VERIFIED**. A 2024 forum reply that "Emby
  hasn't been open source for several years" and Wikipedia's "closed-source as of version 3.5.3" are **REPORTED**
  (snippets). Parts of the system remain GPL on GitHub per a 2020 staff reply, **REPORTED**, and I could not confirm
  any web-client component among them. **Selectors therefore cannot be read from source; they must be measured on a live
  Emby server, which this desk pass could not do.**
- **Customization hook:** Emby's dashboard has a Custom CSS field (stored server-side in `branding.xml`) that applies
  "only to local Web app, all other client apps remain unaffected" (staff/mod 2025). **VERIFIED** (forum). Not usable
  for us, but it confirms the web app is the one surface with stable-ish CSS hooks.
- Where the web app lives: the server serves it itself (userscript match pattern `*://*/web/index.html` and
  `*://*/web/`, **VERIFIED**), and `app.emby.media` / `tv.emby.media` are hosted copies. **A content script cannot
  list the user's Emby host in a static `matches`** (inference; same issue as Jellyfin and Plex).

### A5. What I could not establish for Emby

- Shadow DOM/iframe usage, DRM status, the current DOM class names (no live server), whether free Emby accounts see
  the Premiere prompt in Desktop mode for any feature, and current state of tv.emby.media.
- `app.emby.media` itself returned nothing usable to the fetch tool (a bare "Emby" string); it is a JS shell.

---

## Part B. The declarative adapter format

### B1. Chrome Web Store: what counts as remote code

Primary: https://developer.chrome.com/docs/webstore/program-policies/mv3-requirements and
https://developer.chrome.com/docs/webstore/troubleshooting (both fetched, **VERIFIED**).

Quotes (as extracted):
- "the full functionality of an extension must be easily discernible from its submitted code"
- "the logic of how each extension operates should be self contained"
- External resources: "The extension may reference and load data and other information sources that are external to the
  extension," but "these external resources must not contain any logic."
- Listed violations: (a) a `<script>` tag pointing outside the package; (b) "Using JavaScript's eval() method or other
  mechanisms to execute a string fetched from a remote source"; (c) **"Building an interpreter to run complex commands
  fetched from a remote source, even if those commands are fetched as data."**
- Permitted remote execution only through documented APIs: "Debugger API" and "User Scripts API"; "exemptions apply
  solely to the specific section of code covered by these APIs."
- Permitted remote fetches: "Fetching remote resources that are not used to evaluate logic, such as images" and (from
  the migration guide, https://developer.chrome.com/docs/extensions/develop/migrate/improve-security, VERIFIED)
  **"Configuration-driven: Your extension loads and caches a remote configuration (for example a JSON file) at
  runtime"** whose cached settings "determine which features are enabled". An earlier search snippet of the policy page
  also named "a remote configuration file for A/B testing or determining enabled features, where all logic for the
  functionality is contained within the extension package" (**REPORTED**: the sentence did not appear in my fetch
  extraction of Section 3; the quote is from a search result of the same page).
- Enforcement: "If we are unable to determine the full functionality of your extension during the review process, we may
  reject your submission or remove it from the store."

**The decisive ambiguity is the word "complex."** Nothing I fetched defines it. A JSON file with selectors, key maps,
URL patterns and four fixed actions is data driving fixed code (the "configuration-driven" pattern); a file with
conditionals, loops, sub-procedure calls and expressions is an interpreter. Where the line sits is **UNKNOWN**; the two
precedents below bracket it.

Violation ID: a search result calls the MV3 remote-code violation "Blue Argon" (**REPORTED**; the troubleshooting page I
fetched did not print the ID in the extraction).

### B2. Firefox (AMO)

https://extensionworkshop.com/documentation/publish/add-on-policies/ (**VERIFIED**, first 100,000 of ~106,600
characters read; the last 6,600 were read in a second fetch and held none of the searched terms):
- "Add-ons must be self-contained and not load remote code for execution."
- "Add-ons are not allowed to contain obfuscated code, nor code that hides the purpose of the functionality involved."
  "If external resources are used in combination with add-on code, the functionality of the code must not be obscured."
- "potentially dangerous APIs may only be used in ways that are demonstrably safe."
- No statement distinguishing data from code, and no mention of `eval`. So the Firefox text is **less specific than
  Chrome's**, not more permissive: the rule is the same one-liner. Enforcement history: 197 add-ons blocked in Jan
  2020, 129 from one vendor, for executing remote code (**REPORTED**, news coverage; Mozilla Bugzilla bug 1653269 turned
  up in search, not fetched).
- Firefox supports MV3 `userScripts` as an **optional-only** permission with `host_permissions`
  (https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/userScripts, **VERIFIED**).

### B3. Precedents: accepted and rejected

**Accepted-looking: Consent-O-Matic** (https://github.com/cavi-au/Consent-O-Matic; code read via raw GitHub through the
fetch tool, so treat identifiers as extracted):
- Rules are JSON (`rules.schema.json`): per-CMP `detectors` (`presentMatcher`, `showingMatcher`) and `methods`
  (`OPEN_OPTIONS`, `DO_CONSENT`, `SAVE_CONSENT`, `HIDE_CMP`, `UTILITY`, or custom). **15 action types**: click,
  multiclick, list, consent, slide, ifcss, waitcss, foreach, wait, hide, close, ifallowall, ifallownone, runmethod,
  runrooted. **4 matcher types**: css, checkbox, url, onoff. Schema uses `additionalProperties: false` nearly
  everywhere; **no version or format field**. **VERIFIED**.
- README: the extension "only communicates with the web" when "fetching and updating rule lists"; users can add a custom
  rule list by URL. Listed on Chrome Web Store, AMO, Microsoft Store, Mac App Store. **VERIFIED** (README) that it is
  listed; that the *current store build* does remote refresh under MV3 is **REPORTED** only: release v1.1.0 (31 May,
  year not shown) is "no feature changes", "Update manifest to version 3."
- The refresh code: lazy on request, `maxStaleness` 22 h plus random 0-26 h, `fetch(..., {cache:"no-store"})`, results
  cached in `chrome.storage.local` per URL, falls back to the cached copy on a failed fetch, no version comparison.
  `rules-list.json` is `{"references": [206 raw.githubusercontent.com URLs]}` (203 unique).
- It is the nearest functional analog to StreamFlex's adapters (selector-driven click/wait/conditional actions,
  interpreted by bundled code, refreshed from the network, in both stores). It also includes control flow (`ifcss`,
  `foreach`, `list`, `runmethod`), so it sits **above** a minimal vocabulary and still survived, at least in v1.1.0.

**Rejected: AdGuard MV3** (https://adguard.com/en/blog/review-issues-in-chrome-web-store.html, **VERIFIED**; these are
AdGuard's account of reviewer decisions, not reviewer text):
- Rejections for `<script>` tags applying rules; for downloading the Quick Fixes filter ("a violation of the remote
  execution policy"); and for scriptlets "because they could be executed with parameters". AdGuard hardcoded scriptlets
  and made the engine apply a rule only if it matches a pre-built category, otherwise discard it; a limited Quick Fixes
  filter without script execution was rejected again.
- Outcome: Custom filters "temporarily unavailable" (planned via `userScripts`, which needs Developer mode), Quick Fixes
  removed, and fast-track updates "only applies to changes in DNR rulesets and safe rules" (future tense in the post).
  Published 2025-02-03, updated 2025-02-04.
- The lesson for us: **parameterized executable behavior** is what was flagged. A data rule whose meaning is fixed by
  the bundled code, with no free-form parameters to code, matches what AdGuard retreated to.

**uBlock Origin Lite** (https://github.com/uBlockOrigin/uBOL-home/wiki/Frequently-asked-questions-%28FAQ%29,
**VERIFIED**): "never makes network requests to any remote servers"; lists update "when the extension updates to a new
version"; fully declarative so it "must package all the scripts to inject anywhere"; the page attributes this to MV3
service-worker behavior, **not** store policy. It also says "Custom filters and ability to subscribe to external filter
lists is now available" (an edit dated June 2026); the mechanism is **UNKNOWN** (search found nothing on it). A
conservative reading: the most careful MV3 filter-list author chose no remote fetch for shipped lists.

**Userscript managers (Tampermonkey et al.)**: Chrome's `userScripts` API "lets you run arbitrary code" and is the
documented exemption; since Chrome 138 the user must switch on "Allow User Scripts" on the extension's details page
(before 138, Developer mode); API absent until then (https://developer.chrome.com/docs/extensions/reference/api/userScripts,
**VERIFIED**; Tampermonkey's wording from its FAQ **REPORTED**). Not usable as StreamFlex's mechanism without a per-profile
user toggle, which defeats a force-installed appliance flow. Note for later: `userScripts` is the only sanctioned
path if adapters ever need code.

**Stylus, Dark Reader**: Dark Reader's per-site fixes are a text config (domain line, then `INVERT`, `CSS`,
`IGNORE INLINE STYLE`, `IGNORE IMAGE ANALYSIS`); how it is loaded at runtime was **not stated** on the page I fetched
(https://github.com/darkreader/darkreader/blob/main/CONTRIBUTING.md). Stylus not researched. **UNKNOWN**.

### B4. Prior art for declarative per-site navigation formats

| Project | What the format can express | Dynamic content / route changes | Versioning | Grade |
|---|---|---|---|---|
| **uBlock Origin procedural cosmetic filters** (https://github.com/gorhill/uBlock/wiki/Procedural-cosmetic-filters) | Selector plus a chain of operators: `:has`, `:has-text`, `:matches-attr`, `:matches-css`(+before/after), `:matches-media`, `:matches-path`, `:matches-prop`, `:min-text-length`, `:not`, `:others`, `:upward(n or selector)`, `:watch-attr`, `:xpath`. `:matches-path` "should be used as first operator" (a route gate) | "hiding by procedural filters is reevaluated only when nodes in sub-tree are added or removed"; "uBO does not watch for attribute changes" unless `:watch-attr` | Not on the page | VERIFIED |
| **AdGuard ExtendedCss** (https://github.com/AdguardTeam/ExtendedCss) | `:has`, `:contains`/`:has-text`, `:matches-css`, `:matches-attr`, `:matches-property`, `:xpath`, `:nth-ancestor`, `:upward`, `:remove`, `:remove-attr`, `:remove-class`, `:is`, `:not`, `:empty-trimmed` | A "DOM observer" re-applies; actions stop after 50 applications to one element to avoid loops | `EXTENDED_CSS_VERSION` string; behavior tied to v2.0; browser table (Chrome 88, Firefox 84) | VERIFIED |
| **js-spatial-navigation** (https://github.com/luke-chang/js-spatial-navigation, MPL 2.0) | Sections with `selector`, `restrict` (self-only/self-first/none), `enterTo` (last-focused/default-element), `defaultElement`, `leaveFor` per direction, `straightOnly`, `tabIndexIgnoreList`; per-element `data-sn-left/right/up/down` overrides | Imperative `add/remove/focus`; `makeFocusable()` must be re-called after DOM additions | No version shown | VERIFIED |
| **Norigin Spatial Navigation** (https://github.com/NoriginMedia/Norigin-Spatial-Navigation, MIT) | README only: React-hooks library, automatic spatial algorithm; API (focusKey, containers, boundaries) is on a Developer Portal I did not fetch | UNKNOWN | changesets present | VERIFIED (overview), UNKNOWN (API) |
| **LRUD** (https://github.com/bbc/lrud, Apache-2.0) | App-registered tree: `registerNode(id, {parent, orientation, isFocusable...})`, `assignFocus`, `handleKeyEvent`, `move` events. No DOM reading, so it is the wrong shape for a site we do not own | Not covered in the README excerpt | "maintenance mode"; v3 breaking rewrite | VERIFIED |
| **Surfingkeys** (https://github.com/brookhong/Surfingkeys) | **JavaScript** settings; per-site only as `blocklistPattern` / `lurkingPattern` regexes. Settings can load from `~/.surfingkeys.js` (native messaging) or an http(s) URL; last good copy kept on failure | Not described | MV3 status UNKNOWN | VERIFIED |
| **Vimium** (https://github.com/philc/vimium) | The README fetched had nothing on per-site rules | - | - | UNKNOWN |
| **ARIA landmarks** (https://www.w3.org/WAI/ARIA/apg/practices/landmark-regions/) | Eight roles (banner, complementary, contentinfo, form, main, navigation, region, search); "Browser extensions can use landmarks to enhance keyboard navigation." Useful only where sites label them; streaming UIs mostly do not (my inference) | Static markup | W3C | VERIFIED (page), inference (usefulness) |
| **HTLauncher** (GPL-3.0; ideas only; https://github.com/ct06033/HTLauncher) | **No data format.** One extension directory per site (`native/extensions/netflix-remote`, `spotify-remote`), each with `manifest.json`, small `content.js`, `nav.css`, `cursor.css`, a shared `spatial.js` (7,984 bytes in both) and `osk.js` (12,293 bytes in both), and for Spotify a MAIN-world `page-probe.js`. Per-site knowledge is code: Spotify's script holds a URL pattern for the library root, Home/Back keys, a 1.5 s "did the URL change" fallback. Spotify manifest: MV3, service worker, `tabs`, host permission `http://127.0.0.1:47633/*` (the launcher bridge), isolated + MAIN content scripts, `document_idle`, no `all_frames` | Candidates rebuilt **on every keypress**, no MutationObserver; route detection by reading the URL at keypress | Extension 1.0.0; app checks GitHub Releases daily | VERIFIED (tree via GitHub API, manifest and scripts read through the fetch tool; described in prose only) |
| **VacuumTube** (MIT; https://github.com/shy1132/VacuumTube) | Electron wrapper of YouTube Leanback; userstyles are local `.css` files; third-party DBs for SponsorBlock / DeArrow. Injection mechanism not shown on the page I fetched | UNKNOWN | UNKNOWN | VERIFIED (README), UNKNOWN (internals) |

Observation (inference): of the ten, **none expresses "row, tile, initial focus, per-route, key-to-action" as data for a
geometric engine.** The closest data-shaped ones are js-spatial-navigation sections (focus *policy*) and Consent-O-Matic
(*actions with waits*). Our format is a union of those two ideas plus uBlock's route gate.

### B5. JSON Schema, versioning, and rollout practice

- JSON Schema basics (https://json-schema.org/understanding-json-schema/structuring, **VERIFIED**): `$schema` names the
  dialect, `$id` should be an absolute URI, `$defs` holds reusable subschemas, ref-to-ref is disallowed. The page says
  **nothing** about schema versioning or evolution, nor about `additionalProperties` / `unevaluatedProperties`, so the
  forward-compatibility advice below is mine, not the page's.
- Consent-O-Matic's choice: `additionalProperties: false` nearly everywhere, **no** version field, root permitting extra
  keys, refresh by staleness only. That is the existence proof that a format can ship without a `format` integer and
  the cost (an engine and rules that disagree have no way to say so). **VERIFIED**.
- The StreamFlex adapter source already has the carriers: manifest `format: 1`, per-provider integer `version`,
  `sha256`, `issued_at`; Ed25519 signature with `key_id`. **Not present**
  in what I read: a minimum engine version, a per-browser override, an adapter-level `format`.
- **TUF** (https://theupdateframework.io/overview/, **VERIFIED**): rollback attack = "an older, insecure version of a
  file that you already have", defended by checking metadata version numbers; freeze attack = "keeps giving you the same
  file, so you never realize there is an update"; metadata carries an expiration date; goal "minimize the impact of key
  compromises." The roles page (root/targets/snapshot/timestamp) was **not** fetched.
- **Mozilla Remote Settings** (https://remote-settings.readthedocs.io/en/latest/): "a Mozilla service that makes it easy
  to manage evergreen settings data in Firefox"; the index shows a Multi Signoff Workflow with "Change records and request
  review", "Preview changes in the browser", "Approve/Decline changes". The page did not explain any of them, and
  several sub-pages 404ed. Search snippets (mirrors, **REPORTED**) say records are content-signed via Autograph, the
  client canonicalizes JSON and verifies, and signatures are validated at sync. Rollback semantics **UNKNOWN**.
- Adblock Plus filter-list metadata (Version, Expires, Checksum): the cheatsheet I fetched was outdated and held none of
  it. **UNKNOWN** from sources; not relied on.
- Chrome Web Store staged/percentage rollout: not on the distribution page I fetched. **UNKNOWN**. It would apply to
  the extension package only; adapter data rides StreamFlex's own channel.

### B6. Tool notes

- Four search/fetch results tripped the local prompt-injection scanner (strings like "developer mode", "maintenance
  mode" in ordinary documentation, an unrelated "restriction bypass" match). I read each result and found no
  instruction aimed at me; the matches were the Chrome "Developer mode" toggle, the LRUD "maintenance mode" status, and
  similar. No result changed what I did.
- WebFetch summarizes pages with a small model: where I say "extracted" the quote may be trimmed or reflowed. The
  Emby feature-matrix columns and the Greasy Fork selector list are the two places where I would re-check against raw
  HTML before depending on them.

---

## Implications for StreamFlex's adapter format (my inference, not research findings)

1. **Stay on the "configuration-driven" side of Chrome's line, and prove it by construction.** A closed enum of actions
   (click, focus, sendKey, scroll), selectors as strings, key maps, URL patterns, flags. No expressions, no
   user-parameterized behavior, no nesting beyond one level, no loops. Consent-O-Matic's 15 actions with `foreach`/`runmethod`
   survived (at least once); AdGuard's parameterized scriptlets did not. Our four-action vocabulary is comfortably below
   the one precedent that passed. Avoid adding conditionals until a service needs one, then add a single named
   predicate (`ifPresent`, `ifUrl`) rather than a general `if`.
2. **Ship a complete bundled adapter set for all five free services in the package** and treat the remote file as a
   same-schema update. "Full functionality discernible from the submitted code" is satisfied by the bundle; the remote
   copy only changes data. Say so in the store submission notes and describe the vocabulary there. The AMO text is
   thinner, so the same bundle-plus-notes covers it. (Also covers offline and rolled-back cases.)
3. **Put the compatibility handshake in the adapter, not only in the manifest.** Per-adapter `format` (integer engine
   contract), `minEngine` (extension version), `version`, and per-browser override blocks (`chromium`, `firefox`) that
   may only override leaf values (selectors, timings), never the action list. Engine rule: unknown `format` or a
   `minEngine` above the running engine means **skip this adapter and fall back to the bundled one, then to the generic
   engine with cursor mode**, never partially apply. Unknown optional keys: ignore; unknown required-feature keys: skip.
   Use JSON Schema with `additionalProperties: false` for the signed publish-time lint, and tolerate extras at runtime,
   so the server can reject typos while old engines survive new optional keys.
4. **Add anti-rollback and anti-freeze beyond the signature.** The signed manifest already has `issued_at` and
   per-provider `version`: the app should refuse a manifest older than the cached one and an adapter version lower than
   the cached one, and treat a manifest older than N days as "stale, use cache, show nothing", per TUF's two threats.
   Cache with jitter and a last-good fallback like Consent-O-Matic (22-48 h). Add a manifest-level `minEngine` ceiling so a
   bad adapter can be withdrawn by publishing a higher version that equals the last good one.
5. **Route and readiness primitives.** Each adapter carries `routes: [{id, match (path/hash regex), role}]` and each
   rule `waitFor` (selector present) with a timeout and an `onTimeout` of "generic engine". Reasons: Emby's `viewshow`
   fires before cards render (staff: "use a mutation observer"), uBlock gates on path first, HTLauncher checks the URL
   at keypress. Evaluate routes on `pushState`/`popstate`/`hashchange` and on engine start. Emby's `viewbeforeshow` event
   is an optional source, but `detail` objects may not cross the page/isolated-world boundary (untested; check before
   using).
6. **Selector chains with fallbacks, and a version note.** Emby's classes moved between releases and the client is
   closed source, so each adapter field is an ordered list of selectors (first match wins) and the adapter records the
   server versions it was measured on (`measuredOn: ["4.9.0.40"]`). Authoring requires a live Emby server; this
   research could not read the DOM. Same applies to Plex (closed) more than Jellyfin (source readable).
7. **Self-hosted origins need a per-service host-detection block, not a static `matches`.** Emby, Jellyfin and Plex live
   on arbitrary hosts, so `matches` cannot name them. Options to decide: user-entered server URLs delivered over Native
   Messaging and registered with `scripting.registerContentScripts`, or `<all_urls>` with a cheap URL/DOM signature
   probe (`/web/index.html`, hash `#!/`, `is='emby-scroller'`). The first keeps permissions narrow, which matters for
   store review and for the force-install model. The adapter's `detect` block (URL regex plus one DOM probe) is data.
8. **Emby specifics.** Target the **Desktop-mode DOM** (TV mode is Premiere); treat TV mode as an
   optional second route-set keyed on a detected display mode. Do not rely on Emby's own arrow handling in Desktop mode.
   Expect Emby's `home`, `esc`, `alt-left` keys to collide with StreamFlex's Home and Back; adapter key maps need a
   "consume vs pass through" flag per key.
9. **Keep `userScripts` as the documented escape hatch only.** If a service ever needs real code, the sanctioned route is
   `userScripts` (Chrome 138+ toggle, Firefox optional permission), and it is incompatible with a hands-off force-install
   flow; so the format should be designed so that no built-in service needs it.
10. **Pre-submission check.** Before the first store submission, send the adapter schema and a bundled sample to Chrome
    Web Store support (the AdGuard post says developers asked for "clearer guidelines") and ask the "complex commands"
    question in writing. Nothing public defines it, and a written answer is cheaper than a rejection after the format is
    shipped to a fleet.

## What I could not fetch or settle

- A live Emby server or `app.emby.media` DOM (JS shell; no selectors read first-hand); Emby DRM and shadow-DOM facts.
- The Premiere feature matrix column for "TV Mode"; the current status of `tv.emby.media`.
- Any statement from Google or Mozilla defining "complex" or addressing a closed-vocabulary rule interpreter.
- Vimium's rule format; Norigin's API details; Stylus; Dark Reader's runtime loading; Surfingkeys' MV3 status; Remote
  Settings rollback semantics; the TUF role pages; Adblock Plus list metadata; Chrome staged rollout.
- uBlock Origin Lite's June 2026 custom-filter mechanism.
