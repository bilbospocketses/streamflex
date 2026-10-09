# R2: TV web apps in a desktop browser, prior art, terms of service

Researched 2026-09-28 by WebSearch/WebFetch only. Nothing was installed or run locally.

Rating key: **VERIFIED** = primary/official source, or several independent recent sources. **REPORTED** = single, forum, README or secondary source. **UNKNOWN** = searched and found nothing usable.

Limits of this pass: several primary pages were 403/503 to the fetcher (Netflix tech blog, Prime terms, HBO Max terms, Disney+ subscriber agreement, LTT thread). WebFetch summarizes through a small model, so quoted clauses are as returned by it and should be re-read on the source page before being relied on. GitHub dates come from api.github.com `pushed_at`.

---

## 1. Headline findings

1. **The "load the TV UI in desktop Chrome/Edge" shortcut works for exactly one service: YouTube.** No evidence for any of the other twelve. This does NOT remove the overlay work. Netflix, Disney+, Prime, Max etc. TV apps are either native (Gibbon for Netflix) or partner-signed packages tied to the device, not public URLs.
2. **Correction to the original doc (00):** Netflix now lists **Chrome 117+ and Edge 118+ on Windows as up to 2160p** (Netflix Help Center, plus an Aug 2026 article saying Chrome gets 4K via PlayReady). The original doc said Edge-only for 4K. Netflix lists **Linux at up to 1080p on Chrome/Firefox/Edge**, which conflicts with the "Linux = 720p" assumption in 00 and with forum reports. Hands-on test needed.
3. **Closest prior art is very recent:** HTLauncher (created 2026-09-09, GPL-3.0) already does "Edge app window + bundled extension for remote semantics + real Widevine" for Netflix and Spotify, and YouTube via PS4 Leanback UA. It has 0 stars and is a single-author repo; treat as a design reference, not a dependency.
4. **No evidence anywhere of a service banning an account for a UI-only extension.** Teleparty (formerly Netflix Party), Language Reactor and similar are still published in 2026. What does happen is **breakage**: Language Reactor's Netflix integration broke around 2026-06-30 and was still broken on 2026-07-08 due to player-internals changes, not enforcement. Maintenance, not legal risk, is the real cost.
5. ToS text for Netflix, Disney and YouTube prohibits automated access, DRM circumvention and reverse engineering. A D-pad overlay that only moves focus and sends clicks/keys is not those things on a plain reading, but nobody has tested this and it is not legal advice. See section 4.

---

## 2. Per-service table: TV/10-foot web UI in a desktop browser, and DRM result

"TV UI loadable" means: can the service's actual TV/10-foot interface be loaded in desktop Chrome/Edge without the device.

| Service | TV UI loadable in desktop browser? | DRM result on desktop (browser) | Rating / source notes |
|---|---|---|---|
| **YouTube** | **Yes.** `youtube.com/tv` with a console/TV UA (e.g. PS4 "Leanback Shell" Cobalt UA). No DRM for normal video. | Not applicable for normal content. HTLauncher README claims 4K/60 via Edge + PS4 Leanback UA. Available quality follows window/screen size (1080p screen -> 1080p max offered). | Working today: VERIFIED (VacuumTube, MIT, 1.1k stars, pushed 2026-09-28, wraps Leanback with spoofed UA; HTLauncher README). 4K/60 claim: REPORTED. A page claiming "Leanback is dead" (alibaba lifetips) looks like SEO filler and is contradicted by the active projects. |
| **Netflix** | **No.** TV UI is Gibbon, a native rendering layer on the device with a JS app on top (Netflix tech blog: "an SDK installed natively on the device, a JavaScript application that can be updated at any time, and a rendering layer known as Gibbon"). The web client is a separate SPA ("Akira") with the Cadmium player. A smart-TV UA on netflix.com does not yield the TV UI; reports are of error E109 / capability checks failing. | Windows: Chrome 117+ and Edge 118+ up to 2160p if the machine meets UHD requirements; Firefox/Opera 1080p. macOS: Safari 4K, Chrome 1080p, Edge 720p. **Linux: all browsers up to 1080p per Netflix.** Force-4K extension exists and states it only fixes wrongly-failing capability detection (needs real hardware support). | Gibbon description: REPORTED (tech blog fetched via search snippet; direct fetch 403). Cadmium/Akira split: REPORTED (single reverse-engineering gist, sshh12). Resolution table: VERIFIED (Netflix Help Center node 30081, fetched). Chrome 4K news: REPORTED (pcquest, son-video, Aug and Jul 2026). E109/UA note: REPORTED (forum/blog). |
| **Prime Video** | **No evidence.** Fire TV / Tizen apps are device packages. Amazon documents Fire TV UAs (Silk) but spoofing them does not give a TV UI; the results said spoofing "wouldn't help". | Browser: Windows Edge/Chrome get HD; Linux often lower (720p or SD reports). Linux L1 is impossible on desktop per forum consensus. | Linux resolution: REPORTED (Linux Mint, Arch, Level1Techs forums). TV UI: UNKNOWN. |
| **Disney+** | **No evidence.** Tizen app id exists (`MCmYXNxgcu.DisneyPlus`) but it is a partner-signed package. Disney's own help says it cannot be streamed via a TV's web browser. | Desktop browsers 720p (Windows). **Linux browsers 480p (854x480)**, and one thread claims Disney support confirmed this as intentional. | 480p Linux: REPORTED (LTT thread title/summary via search; page itself 403; Starry Hope and others agree on 720p desktop cap). App package: REPORTED (Samsung/Disney help pages). |
| **Max (HBO Max)** | **No evidence.** | Windows Edge/Chrome hardware tier can do 1080p; a Microsoft Q&A thread reports Edge dropped from 1080p at one point. Linux 720p or lower. | REPORTED (Microsoft Q&A, XDA). UNKNOWN for 2026 specifics. |
| **Hulu** | **No evidence.** UA switching reportedly lets Hulu play on Linux, that is all. | Unknown for 2026. | REPORTED (Linux Mint / maketecheasier, old). |
| **Peacock** | **No.** Peacock reportedly blocks Linux regardless of UA. | Windows browser works, quality reported poor vs TV app. | REPORTED (BleepingComputer forum, Level1Techs, Channels community). |
| **Paramount+** | **No evidence.** | Browser reaches "full 1080p+"; 4K premium tier exists. | REPORTED (reelgood/what-hifi/forums). |
| **Apple TV+** | **No evidence.** tv.apple.com is the web app. | Chrome/Firefox/Edge up to 1080p on Windows/Mac per Apple's own support text; 4K needs Apple devices or Safari. | Apple support: VERIFIED-ish (support.apple.com HT207949 appeared in results, text quoted via search summary, not fetched). One older source says 720p: older, contradicted. |
| **Plex** | **Not as a URL.** Plex HTPC is the 10-foot client (Qt/native app), reintroduced May 2022. Whether it embeds a web TV UI loadable in a browser: UNKNOWN. Plex Web (app.plex.tv) is the normal browser UI. | Not DRM content. | HTPC history: VERIFIED (Plex blog, HowToGeek, AndroidPolice). Status: see prior art. |
| **Jellyfin** | **Partly.** jellyfin-web has an "experimental layout" (now default for all non-TV devices per the January 2026 State of the Fin) plus gamepad-to-key and keyboard nav fixes. A separate TV layout exists in jellyfin-web (issue #608 "Next steps for TV layout"). Whether it can be forced in a desktop browser by URL/setting: UNKNOWN. | Not DRM content. | REPORTED (jellyfin.org post, GitHub). The v12.0 release page fetch returned an implausible date (Sept 2024), so do not cite a release date for it. |
| **Twitch** | **UNKNOWN.** No Twitch TV web UI found. Results were only about UA spoofing for login problems. | No DRM on live video. | UNKNOWN. |
| **Spotify** | **UNKNOWN.** HTLauncher runs Spotify as an Edge app window (the normal web player), not a TV UI. | Web player needs Widevine. | HTLauncher README: REPORTED. |

### Why the TV apps cannot simply be reused

- **Tizen/webOS/Fire TV apps are signed partner packages.** A general result: commercial streaming apps are "partner-signed with DRM provisioned against the specific TV they shipped on", so a copy wouldn't activate (REPORTED, Apps2Samsung search snippet).
- **Tizen emulator/simulator has no DRM CDM** (REPORTED, Samsung developer docs and Dolby OptiView docs in results). So even a legitimately extracted package would not play protected content on a PC.
- **Netflix specifically:** Gibbon is not a DOM UI, so there is nothing to load in Chrome. The tech blog says React-Gibbon's "widget" is a drawing primitive replacing divs/spans (REPORTED, via search summary).
- **Netflix spoofing is multi-signal:** one account of Netflix says you need to spoof "user agent AND screen resolution AND media capabilities AND DRM negotiation AND player config" (REPORTED, DEV community post by the netflix-force-4k author).

**Which services refuse or break on a spoofed UA:** Netflix (E109/component errors, REPORTED), Peacock (blocks Linux, REPORTED), Twitch (login blocked on odd browsers/UAs, REPORTED via Opera/Vivaldi/Brave forums). Amazon: UA spoof does not help (REPORTED). YouTube Leanback is the only place spoofing is the intended trick.

---

## 3. Prior art

| Project | What | License | Last activity | Lessons / relevance |
|---|---|---|---|---|
| **HTLauncher** (ct06033/HTLauncher) | Google-TV-style launcher for Windows HTPCs, Electron. Netflix and Spotify run as **fullscreen Edge app windows with a bundled Chrome/Edge extension** that gives Back/Home semantics; YouTube via Edge with PS4 Leanback UA; on-screen keyboard for logins/search; spatial focus; every button remappable. | GPL-3.0 | Created 2026-09-09, pushed 2026-09-18, 0 stars | Closest match to the plan. Confirms architecture (real Edge + extension + launcher). GPL-3.0: cannot copy code into a non-GPL project. VERIFIED it exists and what README says; README claims are REPORTED. |
| **VacuumTube** (shy1132) | Electron wrapper of YouTube Leanback with spoofed UA, adblock, SponsorBlock, controller support. | MIT | Pushed 2026-09-28, 1.1k stars, Win/Mac/Linux | Proves the YouTube shortcut is alive. Electron is fine here because no DRM. |
| **flex-launcher** (complexlogic) | C-based HTPC app launcher, Windows and Linux, remote or gamepad, TV-friendly UI. | Unlicense | Pushed 2026-07-07, 525 stars | Good launcher-side reference; no page-level nav. |
| **PC-Launcher** (PC-Launcher) | C#/.NET launcher opening Netflix/Disney+/Hulu URLs in the default browser, gamepad support. | MIT | Pushed 2025-04-14 (v1.0.0), 9 stars | Stale, small. Notes browser focus needs tweaking on some systems. |
| **NetflixController** (FThompson) | Chrome extension: gamepad navigation, virtual keyboard, mutation observers, page handlers per Netflix page type (browse, player, search, title). | No license file | Pushed 2020-10-26, 22 stars | Best documented per-page-handler design for Netflix; dead for 6 years so selectors will be stale. Its TODO lists "virtual mouse from right stick", which matches the cursor fallback idea. |
| **NetflixNavigator** (cheald) | Gamepad API Chrome extension for Netflix HTPC use. | MIT | Old (CoffeeScript, Chrome 40+), 6 stars | Historical only. |
| **netflix-navigator** (dutiyesh) / Chrome Web Store "Netflix Navigator" | Arrow-key browsing, Enter for info, S to search. Store rating 3.8. | Not checked | Not checked | Keyboard-only; users asked for Disney+/Prime. |
| **netflix.player.controls** (andrewleech), **netflix-remote** (butttons, shelbymorris90) | Configurable keys in the player; phone-as-remote. | Not checked | Not checked | Player-side control precedents. |
| **Norigin-Spatial-Navigation** | React spatial-navigation library for browsers and smart TVs. | MIT | Pushed 2026-09-25, 488 stars | For building a UI you own (the launcher), not for driving third-party DOM. |
| **WICG spatial-navigation** | Spec + polyfill for arrow-key directional focus. | Other | **Archived**, last push 2026-03-23, 225 stars | The polyfill is archived; the geometric algorithm in the spec is still the reference. Do not depend on it. |
| **Kodi Netflix add-on** (CastagnaIT/plugin.video.netflix) | Native Kodi plugin via inputstream.adaptive + Widevine. | MIT | Pushed 2026-07-10, 2,034 stars, **development suspended** | Its own README says website changes broke playback and that when it works it is "limited to SD quality and only on some Linux/Android devices". Table shows Windows and macOS no 1080p. **This is the ceiling of a non-browser approach: software Widevine CDM only, SD.** VERIFIED (README). |
| **Kodi Prime add-on** (Sandmann79/xbmc, plugin.video.amazon-test) | Prime Video add-on using inputstream.adaptive Widevine. | GPL-3.0 | Pushed 2026-08-03, 810 stars | Still maintained. Resolution ceiling not established in this pass: UNKNOWN. |
| **Kodi Chrome Launcher** | Launches Chrome in kiosk from Kodi so streaming sites play with browser DRM. | Not checked | Old (2016-2019 tutorials) | Older precedent for "launch a real browser in kiosk". Exit is Alt+F4, i.e. no remote-friendly return path, the gap HTLauncher's Home key fills. |
| **Plex HTPC** | Official 10-foot player, reintroduced 2022. | Proprietary | **No update since July 2025** per a Jan 2026 forum post; Plex employee said on 2026-01-17 there are still plans, no details | Not a streaming-site solution; shows how fragile vendor 10-foot clients are. VERIFIED post via forum fetch. |
| **Jellyfin Desktop** (formerly Media Player) | Qt WebEngine + libmpv client. | GPL-2.0 | Pushed 2026-08-31, 5.8k stars. Jan 2026 post: Windows/macOS stable builds not available then. | Web UI in Qt WebEngine plus native mpv, i.e. the "wrap a web UI + native player" pattern. No Widevine mention. |
| **TV Bro** (truefedex) | Android TV browser, D-pad, UA switching, Android WebView. | "Other" | Pushed 2026-07-29, 1.7k stars | Cursor mode pattern for TV browsers. A comparison article says DRM content from Netflix/Prime plays only in SD in TV browsers (REPORTED). |
| **mrowser** and similar | Android TV browsers with D-pad mouse cursor. | Not checked | Not checked | Cursor-mode precedent. |
| **Kylo** (Hillcrest Labs) | 10-foot Firefox-based browser for Windows/macOS with a home screen of 128 site buttons (Netflix, Hulu, etc.). | Open source | Discontinued; abandoned after Hillcrest was acquired by CEVA in July 2019 | Same idea 16 years ago; died from lack of a maintainer plus DRM changes (my inference, not sourced). Wikipedia/Macworld. |
| **Steam Big Picture / Steam Deck** | Steam's CEF browser; Netflix reported unreliable there historically. Steam Deck route is Chrome/Firefox as a non-Steam app (Decky NonSteamLaunchers auto-downloads Chrome), Widevine L3, 720p-1080p. | n/a | Guide undated in fetch | Confirms "real Chrome as a non-Steam game with controller layout" works for Netflix, Disney+, Max, Apple TV+, Twitch; Prime sometimes errors. Steam Link streaming of DRM content is blocked (REPORTED). |

Additional search gaps: no GitHub project was found doing multi-service spatial navigation for Disney+/Prime/Max in one extension. The Chrome Web Store hits were Netflix-only. Reported as UNKNOWN rather than "does not exist".

---

## 4. Terms of service and legal

**Not legal advice. These are plain readings of quoted text.**

| Service | Relevant text (as returned by fetcher) | Date | Rating |
|---|---|---|---|
| **Netflix** Terms of Use s1.8 | Prohibits using "any robot, spider, scraper or other automated means to access the Netflix service"; "circumvent, remove, alter, deactivate, degrade, block, obscure or thwart any of the content protections"; reverse engineering software "accessible" through the service. "We may terminate or restrict your use of our service if you violate these Terms of Use." | Updated April 10, 2026 | VERIFIED (help.netflix.com/legal/termsofuse, fetched) |
| **Disney** Terms of Use | Prohibits robot/spider/script/automated means "including ... AI Tool, data mining or web scraping"; circumventing "any content protection system or digital rights management"; "modify the Disney Products ... framing, mirroring"; may suspend access "if we have objective reason to believe you have used the Disney Products in violation". | Effective May 24, 2024 | VERIFIED (disneytermsofuse.com, fetched). Note "framing, mirroring" is relevant only if the overlay wraps the site in an iframe or clone; injecting into the real page in a real browser is different. |
| **YouTube** Terms | Prohibits automated means (robots, botnets, scrapers), circumventing or disabling parts of the service, and using the service except as permitted by the service. | Effective 2023-12-15 | VERIFIED (youtube.com/t/terms, fetched, partial quotes). A "Section 4.2 ... interface provided by YouTube" quote appeared only on an SEO-style page, so treat it as unverified. |
| **Prime Video / Max / Hulu / Peacock / Paramount+ / Apple TV+** | Not obtained. Prime pages returned 404/503, HBO Max returned only an index page. | n/a | UNKNOWN. Needs manual read. |

**Reading for this project:** none of these clauses names browser extensions, accessibility or input remapping. The "automated means" and "circumvent" clauses target scraping and DRM defeat. A remote-control overlay that maps keys to focus and click events is user-driven input, one action per key press. The clearer risk areas are (a) anything that programmatically triggers playback or navigation on its own (auto-play chains, skipping, background crawling of catalogs), (b) anything touching the media pipeline (Netflix errors if `video.currentTime` is set directly per 00, unverified here), and (c) framing or mirroring a service's UI inside your own shell (Disney clause). Spoofing a UA is not named in any fetched clause; the only UA-spoofing statement in the results (YouTube ToS "Section 4.2") is from an unreliable source.

### Enforcement precedent (bans for UI-only extensions)

- **Teleparty / Netflix Party:** in the results, no account-ban reports and no official Netflix statement about it. Still in Chrome Web Store, Edge Add-ons, Firefox and app stores; Play Store listing updated 2026-06-18 (REPORTED, castofus article; store pages). Its disclaimer says it is not affiliated with any streaming service (REPORTED). Absence of ban evidence is only absence in these searches: UNKNOWN as a positive claim.
- **Language Reactor:** Netflix integration broke from about 2026-06-30 (1,000+ issues logged 06-30 to 07-07, still unresolved 07-08). Cause attributed to Netflix player-internals changes, no evidence of deliberate blocking, no ban reports (REPORTED, one blog, deeplingo). Maintainer had last updated on 2026-07-01.
- **uBlock Origin on Netflix:** a Feb 2025 uAssets issue titled "netflix.com: detection" says the extension breaks Netflix, i.e. Netflix does run extension/tamper detection of some kind (REPORTED, GitHub issue; only the title was seen).
- **Household-bypass extensions** (Nikflix, HouseholdNoMore) exist and "Netflix continues to update its systems" against them (REPORTED). Different category: those defeat a business rule, not just drive the UI.
- **Netflix's own help pages** have a "You're entering a third-party app" page and a general troubleshooting stance of disabling extensions. No policy on remote-control extensions found.

**Is a remote-control extension normal use?** UNKNOWN as a legal question, but in practice: gamepad/keyboard extensions for Netflix have existed since about 2015 to 2020 (NetflixNavigator, NetflixController) with no reported enforcement, HTLauncher ships one publicly now, and the Steam Deck route uses stock Chrome. Realistic exposure is breakage, not bans.

---

## 5. Implications for the plan

- **The shortcut only saves work for YouTube.** Use Leanback via UA for YouTube (like HTLauncher/VacuumTube), and keep the generic spatial engine + per-site adapters for everything else.
- **Update the DRM section of 00:** Chrome and Edge on Windows can both reach 2160p on Netflix per Netflix's own table; Linux per Netflix is 1080p, unverified in practice.
- **Do not use Electron/CEF for DRM sites** (unchanged). Electron is fine for the launcher and YouTube, as HTLauncher and VacuumTube show.
- **Budget for maintenance:** NetflixController died in 2020, Kodi's Netflix add-on is suspended, Language Reactor broke for at least a week in mid-2026. Remote-updatable selectors and a cursor fallback are justified by evidence, not just caution.
- **Licensing:** HTLauncher is GPL-3.0; borrow ideas, not code, unless the project will also be GPL.

---

## 6. Open questions needing a hands-on test

1. Does netflix.com with a TV UA (Tizen/webOS/PS4/Xbox strings) change anything visible, or just error? Expect error; confirm.
2. Actual resolution and playback on Linux Chrome vs Firefox for Netflix (Netflix says 1080p; forums say 720p), Prime, Disney+ (480p claim), Max.
3. Windows Chrome 117+ vs Edge 118+ Netflix 4K on the target PC (Ctrl+Shift+Alt+D overlay shows PlayReady SL3000 and resolution), and Prime/Disney+/Max on Windows Chrome vs Edge.
4. Does an Edge `--app=` window with `--user-data-dir` keep Widevine/PlayReady and remembered logins? (HTLauncher claims yes.)
5. Jellyfin web: can the TV layout be forced in a desktop browser (setting or URL), and does it navigate by D-pad well enough to skip an adapter?
6. Plex: does Plex Web have any usable keyboard/D-pad mode in a browser, and what does Plex HTPC load internally?
7. Do Twitch or Spotify offer any TV/10-foot web UI reachable by UA? Nothing found.
8. Do Prime Video, Max, Hulu, Peacock, Paramount+ and Apple ToS contain anything about extensions or UA spoofing? Read them directly (fetch failed).
9. Does Netflix's extension/tamper detection (the uBlock-related issue) affect a content-script overlay that only adds DOM elements and dispatches key/click events?
10. Does the same-named `Sandmann79` Prime add-on still deliver more than SD on Windows? Only relevant if a Kodi fallback is ever considered.

---

## Sources

- Netflix supported browsers and resolution: https://help.netflix.com/en/node/30081
- Netflix Terms of Use (updated 2026-04-10): https://help.netflix.com/legal/termsofuse
- Netflix Chrome 4K news (Aug 2026): https://www.pcquest.com/news/netflix-just-unlocked-4k-in-chrome-but-your-pc-may-still-block-it-12231817
- Son-Video Chrome 4K (Jul 2026): https://blog.son-video.com/en/2026/07/watching-netflix-in-4k-on-chrome-is-finally-possible/
- Netflix force-4k extension: https://github.com/Pickle-Pixel/netflix-force-4k
- Netflix spoofing multi-signal post: https://dev.to/picklepixel/how-i-made-netflix-give-me-4k-because-apparently-my-browser-wasnt-good-enough-4fa2
- Netflix tech blog on Gibbon/React (direct fetch 403, summary via search): https://netflixtechblog.com/crafting-a-high-performance-tv-user-interface-using-react-3350e5a6ad3b
- Netflix web architecture reverse-engineering: https://gist.github.com/sshh12/dda3a89514f850c459380b18b1f7eb7b
- Disney Terms of Use: https://disneytermsofuse.com/english/
- YouTube Terms: https://www.youtube.com/t/terms
- VacuumTube: https://github.com/shy1132/VacuumTube (API: https://api.github.com/repos/shy1132/VacuumTube)
- HTLauncher: https://github.com/ct06033/HTLauncher (API: https://api.github.com/repos/ct06033/HTLauncher)
- flex-launcher: https://github.com/complexlogic/flex-launcher
- PC-Launcher: https://github.com/PC-Launcher/PC-Launcher
- NetflixController: https://github.com/FThompson/NetflixController
- NetflixNavigator: https://github.com/cheald/NetflixNavigator
- Netflix Navigator (dutiyesh): https://github.com/dutiyesh/netflix-navigator
- Kodi Netflix add-on: https://github.com/CastagnaIT/plugin.video.netflix
- Kodi Prime add-on: https://github.com/Sandmann79/xbmc
- Kodi Chrome Launcher: https://forum.kodi.tv/showthread.php?tid=170965
- Plex desktop status thread: https://forums.plex.tv/t/are-the-desktop-apps-dead/935518
- Plex HTPC blog: https://www.plex.tv/blog/way-to-be-htpc/
- Jellyfin Desktop: https://github.com/jellyfin/jellyfin-desktop
- Jellyfin State of the Fin 2026-01-06: https://jellyfin.org/posts/state-of-the-fin-2026-01-06/
- jellyfin-web TV layout issue: https://github.com/jellyfin/jellyfin-web/issues/608
- TV Bro: https://github.com/truefedex/tv-bro
- Norigin Spatial Navigation: https://github.com/NoriginMedia/Norigin-Spatial-Navigation
- WICG spatial-navigation (archived): https://github.com/WICG/spatial-navigation
- Kylo: https://en.wikipedia.org/wiki/Kylo_(web_browser)
- Steam Deck streaming guide: https://decky.net/en/blogs/news-en/netflix-disney-on-steam-deck-guide
- Level1Techs Linux DRM thread: https://forum.level1techs.com/t/watch-drm-streaming-i-e-peacock-amazon-netflix-in-full-resolution/246528
- Disney+ Linux 480p thread (page 403; snippet only): https://linustechtips.com/topic/1640895-disney-intentionally-limits-linux-browsers-to-480p-confirmed-by-support-my-investigation/
- Language Reactor June 2026 breakage: https://deeplingo.ca/resources/articles/language-reactor-not-working-netflix
- uBlock Origin Netflix detection issue: https://github.com/uBlockOrigin/uAssets/issues/27248
- Teleparty: https://www.teleparty.com/support and https://chromewebstore.google.com/detail/netflix-party-is-now-tele/oocalimimngaihdkbihfgmpkcpnmlaoa
- Samsung TV simulator/emulator docs: https://developer.samsung.com/smarttv/develop/getting-started/using-sdk/tv-emulator.html
- Amazon Fire TV UA strings: https://developer.amazon.com/docs/fire-tv/user-agent-strings.html
