# StreamFlex D-pad overlay: service tiering (2026-09-28, US)

Sources: `21-services-majors-a.md` (Netflix, Disney+, Prime Video, Max, Apple TV+, Hulu), `22-services-majors-b.md` (Peacock, Paramount+, Crunchyroll, MGM+, YouTube, YouTube TV), `23-services-candidates.md` (the others), and the "Linux Edge and Widevine (resolved 2026-09-28)" section of `24-linux-full-firefox.md`. Nothing here is new research; every claim comes from one of those files, which hold the URLs.

**Binding decisions applied** (`README.md`, plus the user's decision of 2026-09-28 on Linux Edge):
- There are six targets: Chrome, Edge and Firefox on Windows and on Linux (Linux Firefox means the full, non-snap build). macOS is out of scope.
- **Linux Edge is a non-DRM browser only.** It covers YouTube, Jellyfin, Plex web, Twitch and similar services. Any service that needs Widevine is "not supported on Linux Edge until the Phase 0 Edge/Widevine test (S1) passes".
- Low Linux quality ceilings (Widevine L3, about 480p to 720p) are **known limits** of a target. They are recorded here and are never a reason to defer Linux.
- The majors come first, then a few others.

**What "tier" means here.** The schema does not define tiers, so this file uses them as build order. Tier 1 adapters are built and tested first. Tier 2 comes next. Tier 3 is built only on demand or when cheap. "Exclude" means no adapter.

---

## 1. Tier table

### Legend

- **V** = VERIFIED, **R** = REPORTED, **U** = UNKNOWN (the tag carried from the source file).
- **~** = expected, not observed. The value is derived from the Linux Widevine L3 fact ("SD or, best case, 720p", XDA 2026-06-04), not measured on that service.
- **gated** = the service needs Widevine, so it is **not supported on Linux Edge until the Phase 0 Edge/Widevine test (S1) passes**. Microsoft's own Q&A answer (V) says "Widevine DRM is not available on Linux now".
- **Win** = Windows, **LX** = Linux. Resolution is the maximum the browser gets. Every Linux DRM cell is SDR and stereo.

### 1a. Support and quality matrix

| Service | Tier | Win Chrome | Win Edge | Win Firefox | LX Chrome | LX Edge | LX Firefox |
|---|---|---|---|---|---|---|---|
| **Netflix** | 1 | 2160p SDR stereo, Chrome 140+ R (default-on U) | 2160p HDR/DV, Atmos V | 1080p V | table says 1080p V; in practice ~720p R | gated | table says 1080p V; in practice ~720p R |
| **Disney+** | 1 | 1080p V | 1080p V | 1080p or lower V | ~480p R | gated | 480p-720p R |
| **Prime Video** | 1 | 1080p V | 1080p V | 1080p V | SD (~480p) R | gated | SD V |
| **YouTube** (/tv) | 1 | 2160p R | 2160p R | 2160p R | works, no DRM ~U | works, no DRM ~U (offered) | up to 4K R |
| **Jellyfin** | 1 (D7) | supported V | supported V | supported V | supported V | supported V | supported V (and ESR) |
| **Plex web** | 1 (D7) | supported R | supported R | supported R | supported R | supported R | supported R |
| **Max** | 2 | 1080p R | 1080p R | U (at most 1080p expected) | SD-720p R; block status U | gated | SD-720p R; block status U |
| **Apple TV+** | 2 | 1080p SDR V | 1080p SDR V | 1080p SDR V | ~SD-720p U; block U | gated | ~SD-720p U; plays per a 2023 forum post R |
| **Hulu** | 2 | 720p R | 720p R | 480p R (HDCP 1.4 needed) | plays R; ~480p U; Linux not on Hulu's list R | gated | plays after DRM enable R; ~480p U |
| **Peacock** | 2, Windows only | supported V, resolution U | supported V, resolution U | supported V, resolution U | **blocked R** (error 6007) | gated (the Linux block would apply anyway) | **blocked R** (error 6007) |
| **Crunchyroll** | 2 | supported R, 1080p R | supported R, 1080p R | supported R, 1080p R | ~720p U | gated | ~720p U; one unverified Widevine-install failure |
| **YouTube TV** | 2 | on the official list V; resolution U | **not on the official list** V; may work U | on the official list V; resolution U | not listed V; ~720p (480p-720p R) | gated | not listed V; ~720p (480p-720p R) |
| **Paramount+** | 3 (D1) | supported V (stale list); 1080p Essential, 4K Premium R; 4K in a browser U | same as Win Chrome | same as Win Chrome | not listed V; ~720p if it plays U | gated | not listed V; ~720p if it plays U |
| **MGM+** | 3 | supported R, resolution U | supported R, resolution U | supported R, resolution U | ~720p U | gated | ~720p U |
| **Tubi** | 2 | listed V | listed V | listed V | listed for Chrome, Linux not named V | DRM status U (test S2) | listed for Firefox, Linux not named V |
| **Pluto TV** | 2 | supported R | supported R | supported R | works R (third-party page) | works R; DRM status U (test S2) | works R |
| **Twitch** | 2 | up to source (1080p60) R | same | same | supported, no DRM R | supported, no DRM R | supported, no DRM R |
| **Spotify web** (audio) | 2, optional, later (D8) | listed V | listed V | listed V | works with the "Protected content" setting V | gated (needs Widevine V) | "Enable DRM" V; older failure reports R |
| **ESPN** | 2, conditional | old list V | old list V | old list V | U | U (DRM status U) | U (old reports that it works R) |
| **The Roku Channel** | 3, conditional | web app exists V; browsers U | U | U | U | Widevine likely (R, forum thread title): gated | U |

The Linux Chrome, Edge and Firefox columns for the majors come from `21` (pass 3) and `22`. Those files say that the three Linux browsers behave alike because the cap comes from Widevine L3, not from the browser. Linux Edge is the exception: it has no Widevine by default.

**Tier 3 "others", compressed.** Source: `23`. These services all use DRM, so every Linux Edge cell is gated.

| Service | Windows (Chrome / Edge / Firefox) | Linux Chrome / Firefox |
|---|---|---|
| Fubo (now with Hulu + Live TV) | all three R | not stated U |
| Sling TV | all three V; 4K on Edge via PlayReady R | not supported; a UA workaround exists R |
| Philo | all three V | not supported (absent from the list) V |
| DAZN | all three, up to 1080p V | "may work", up to 720p V (the only documented Linux ceiling) |
| NBA League Pass | Edge 1080p, Firefox 720p R | unsupported R |
| MLB.tv | all three R | not listed R |
| Starz | all three R | U; old DRM-error reports on Firefox R |
| AMC+ | U ("a web browser") | U |
| Shudder | Chrome and Firefox V; Edge not named | U; one third-party page says it works R |
| BritBox | all three R | U |
| Discovery+ | all three V | not listed V |
| Criterion Channel | Chrome and Firefox R | reportedly blocked R |
| Kanopy | all three V | not listed V |
| Apple Music web | all three R | conflicting reports R |
| Pandora | no list read R | not supported R |

### 1b. Difficulty, terms of service and biggest risk

| Service | Tier | Overlay difficulty | ToS risk, in one line | Single biggest risk |
|---|---|---|---|---|
| Netflix | 1 | Medium | Bans "insert any code or product or manipulate the content of the Netflix service in any way" (terms dated 2026-04-10) V | A programmatic `video.currentTime` seek triggers error M7375 R; seek must use key events, buttons or the internal player API |
| Disney+ | 1 | Medium | Disney+/ESPN/Hulu agreement (2026-02-05) bans "robot, spider, script or other automated means" and "bypass, modify, ... tamper with ... functions" V | The browse page DOM is changing during the Hulu integration; the player mixes plain DOM with a shadow root (the ad badge) |
| Prime Video | 1 | Medium-high | Low: the Prime terms (2023-12-07) have no robot or extension clause; Amazon's Conditions of Use (2026-08-14) ban "data mining, robots" V | Constant A/B tests of a dense DOM; Linux gets SD |
| YouTube (/tv) | 1 | Low on /tv; low-medium on the standard site | No UA clause; bans "automated means" (terms dated 2023-12-15, not re-checked) V; /tv needs a TV-device user agent (D4) | The /tv user-agent gate; the fallback is the standard site plus a focus-ring overlay |
| Jellyfin | 1 (D7) | Low | Self-hosted; not researched | The TV layout still has keyboard-focus gaps (Quick Connect, forgot password) R |
| Plex web | 1 (D7) | Low-medium | Not researched | The web app has no TV layout, so the overlay does all the work; Plex HTPC already exists as an alternative |
| Max | 2 | Medium | US terms unreadable; the France terms (2026-06-08) ban "modify ... the Platform" and "frame ... any portion" V; the US wording is U | Few native keys, hashed class names, and three hostnames (`max.com`, `hbomax.com`, `play.hbomax.com`) |
| Apple TV+ | 2 | Medium | Apple Media Services terms (2026-09-14) ban an "automated process ... monitoring of any portion of the Content or Services"; the risk is theoretical V | Apple ID 2FA sign-in from the couch; possible shadow DOM |
| Hulu | 2 | Medium-high | Same agreement as Disney+; also bans "code ... that ... limit the functionality" V | Keys act only on the focused control; UI churn from the Disney+ integration |
| Peacock | 2, Windows only | Medium | Terms (2025-12-03) ban "software and/or tools" that "interfere with" the service; a modified-display clause is summarizer paraphrase only V/R | Linux playback is blocked (error 6007) on the OS where an HTPC most likely runs |
| Crunchyroll | 2 | Low-medium | Bans any "mod, hack, exploit ... tool" used to "modify ... or otherwise interfere"; the terms page is undated V | Linux Widevine behavior is unresolved |
| YouTube TV | 2 | Medium | Terms (2026-02-11) ban any technology that can "skip ... advertising ... on a recorded program" and "post-market modification devices" V | The household location check plus the circumvention clause; an ad-skip feature would breach the terms (D2) |
| Paramount+ | 3 | Medium (not probed) | **§9.6 bans altering or "enhancing" any portion of the video player; §5.1(d) bans ad blockers (terms dated 2026-09-15)** V | §9.6 |
| MGM+ | 3 | Unknown (medium assumed) | Terms (2023-11-02) ban anything that "modif[ies] ... alter[s] ... features" V | Almost no verified technical data; many subscribers watch through Prime Video Channels |
| Tubi | 2 | Low-medium | Not researched | Linux is not named on its list; DRM status U |
| Pluto TV | 2 | Medium | Not researched | The live channel grid is hard to navigate with focus |
| Twitch | 2 | Medium | Not researched | Chat-heavy and directory-heavy pages |
| Spotify web | 2, optional, later (D8) | Low-medium | Not researched | Older reports that Linux Firefox fails |

## 2. Tier lists

**Tier 1 (build first)**
- **Netflix**: the largest service, with an official Linux browser list; the overlay difficulty is medium.
- **Disney+**: a major service, not blocked on Linux (about 480p), and the auto-skip extensions keep working selectors.
- **Prime Video**: a major service with the only official, global keyboard key set (V). Linux gets SD, which is recorded as a known limit.
- **YouTube (/tv)**: no DRM, so it works on all six targets, and the /tv Leanback UI is already built for D-pad focus. It uses the /tv UA rule with a fallback to the standard site (D4).
- **Jellyfin and Plex web (D7)**: DRM-free, safe on all six targets and low difficulty. Jellyfin already ships a TV layout; Plex web has none, so the overlay adds the most value there.

**Tier 2 (next wave)**
- **Max**: a major service, but its keys and selectors are weak and its domains keep changing. It is Tier 2 because the hotkey evidence is secondary.
- **Apple TV+**: a second-class web app with 2FA sign-in and possible shadow DOM.
- **Hulu**: it shares an agreement and a future with Disney+. Build it once the Disney+ adapter and UI settle.
- **Peacock (a Windows-only adapter, D5; excluded on Linux)**: a major service, but Linux playback is blocked in every browser (R, from 2021 to 2023 reports).
- **Crunchyroll**: a simple player and a flat catalog; its terms are lenient in the ways that matter.
- **YouTube TV**: live TV. It is not on the official list for Edge or for any Linux browser, and ad-skip must never be built (D2).
- **Tubi and Pluto TV**: the largest free services (Tubi has 110M MAU globally V). They need a Linux smoke test.
- **Twitch**: no DRM, and a large US audience (about 37M R).
- **Spotify web (optional, later; D8)**: the largest of the others (777M MAU V), audio only. Commit only after a Linux test (S2).
- **ESPN (conditional)**: large, but there is no Linux statement at all. MLB.tv folds under ESPN.

**Tier 3 (on demand)**
- **Paramount+ (final, D1)**: ToS §9.6 and §5.1(d).
- **MGM+**: a small catalog, no technical data, and often reached through Prime Video Channels.
- **The Roku Channel**: no browser or Linux data.
- **Fubo, Sling TV and Philo**: live-TV guides are a hard overlay target, and Sling and Philo are effectively Windows-only.
- **DAZN, NBA League Pass and MLB.tv**: seasonal sports. Linux is unsupported or capped at 720p. Treat MLB.tv as part of ESPN.
- **Starz, AMC+, BritBox, Discovery+, Shudder, Criterion Channel and Kanopy**: DRM video with Linux unconfirmed or blocked. Several are sold in the $29.99 Prime Video bundle, so they matter more as a group.
- **Apple Music web and Pandora**: audio, with doubtful Linux support. Pandora is shrinking (down 7% year over year V).

**Excluded**
- **Freevee**: shut down, and its content moved into Prime Video (V).
- **CuriosityStream**: a flat, shrinking subscriber base with no evidence of any benefit.
- **Peacock on Linux**: blocked (R). Revisit if test S4 shows the block is gone.

## 3. Decisions made (2026-09-28)

The user took the recommended answer on every decision. They are recorded in `README.md` under "the SERVICES.md decisions".

- **D1. Paramount+ is Tier 3, final.** Its ToS §9.6 bans "enhancing" any portion of the video player and §5.1(d) bans ad blockers, while the overlay must drive playback.
- **D2. Ad-skip is banned per service, enforced by the engine; the adapter data carries the ban (YouTube TV first).** The YouTube TV terms (2026-02-11, V) ban any technology that can "skip ... advertising ... on a recorded program"; because the engine enforces the ban, a remote adapter update cannot switch ad-skip on by mistake. Manual scrubbing with the remote stays allowed.
- **D3. v1 acts only on remote presses: no auto-skip and no auto-next. Automatic actions are revisited later, per service.** Unattended clicking is arguably the "automated means" that Netflix, Disney+/Hulu and Apple ban; a person-driven remote is not what those clauses target.
- **D4. YouTube uses the /tv UA rule, with a fallback to the standard site.** /tv gives a native D-pad UI cheaply today (live-tested V), and the fallback covers the day Google closes the user-agent gate.
- **D5. Peacock is a Windows-only adapter.** It works on Windows, but Linux playback is blocked by an OS-level check (R), so the launcher must hide or explain Peacock on Linux.
- **D6. An unsupported flag does NOT pass S1.** If Linux Edge plays DRM only with `--enable-features=msWidevinePlatform`, it stays non-DRM, because relying on an undocumented switch that Microsoft can remove repeats the failure pattern of Chrome's `--load-extension`.
- **D7. Jellyfin and Plex web move to Tier 1.** They are DRM-free, safe on all six targets and low difficulty: cheap wins and a Linux showcase.
- **D8. Spotify stays Tier 2, optional, and later.** It is the largest audience in the triage, but it is audio only and needs a Linux test (S2) first.
- **D9. The Linux installer offers any of the three browsers (Chrome, Edge, full Firefox) when none is installed, defaulting to Firefox.** This matches the like-for-like browser targets, and it supersedes the earlier "checks for a full Firefox" rule in README.
- **D10. The extension is scoped by profile, never by installation.** Firefox sideloads the signed XPI into each StreamFlex profile and writes no policy file. Chrome and Edge use the store listing plus force-install, and the extension stays inert outside StreamFlex profiles. The reason: an installation-wide policy would also put the extension into the user's everyday browser profiles, which is the scope trap. Analysis and Phase 0 tests are in `25-extension-scope.md`.
- **D11. The Firefox extension is unlisted on AMO (self-distributed and signed).** This matches the unlisted Chrome Web Store and Edge Add-ons decision.

## 4. Cross-service engineering findings

1. **Do not trust the sites' own keys; drive the `<video>` element, except on Netflix.**
   - Max natively handles only F, Space/K/P and Z/X; hotkey extensions all exist to add the arrow keys (R, several sources).
   - Hulu's arrow keys act only on the focused control (R).
   - Disney+ Space is unreliable when the control bar is hidden (R, a 2026-09 extension fix).
   - So for Disney+, Max and Hulu, set `video.currentTime`, `volume`, `play()` and `pause()` directly, as `CHJ85/Stream-Assistant` does.
   - **Netflix is the opposite:** a `currentTime` seek triggers M7375 (R, one gist). Use key events, the on-screen buttons, or the internal player API (SYNTHESIS §7 item 13).
   - **Prime Video** has an official global key set (V), so sending keys there is safe.
2. **Skip Intro and Next Episode are on-screen buttons on every service; no site maps them to a key.** Match selectors in this order: `data-testid`, then a stable class or class prefix, then the button text. Selector anchors:
   - Disney+: `.skip__button`, `[data-gv2elementkey="playNext"]`
   - Prime Video: `.atvwebplayersdk-skipelement-button`, `.atvwebplayersdk-nextupcard-button`
   - Max: `[data-testid="player-ux-skip-button"]`, or the class prefix `SkipButton-`
   - Hulu: `.SkipButton` (low confidence)
   - Apple TV+: `.skip-button` (low confidence)

   Max's hashed styled-components class names can change silently. That supports the decision to fetch adapters as remote JSON.
3. **Fullscreen.** A content script's `requestFullscreen()` is refused without a real user gesture (R, an extension changelog). Instead use `windows.update`, kiosk mode or F11, or click the site's own button; Max has its own (`[data-testid="player-ux-fullscreen-button"]`). Whether a physical keypress counts as the gesture is U, and SYNTHESIS §7 item 7 tests it.
4. **Shadow DOM.**
   - Disney+ puts the ad timer inside `ad-badge-overlay`'s open shadow root (R, from extension code).
   - YouTube /tv is a custom-element app with `ytlr-` tags (V).
   - Apple TV+ may use shadow DOM (R, background only).

   The engine must pierce open shadow roots, and adapters must be able to name a shadow host.
5. **Ad states must be recognized, never skipped.** Hulu's ad tier plays non-skippable ad pods as a separate overlay state. Disney+ exposes its ad timer. YouTube TV and Paramount+ ban skipping or blocking ads (D2).
6. **Multi-host match patterns.** Max needs `max.com`, `hbomax.com` and `play.hbomax.com` (the player lives on its own host). Sign-in pages also sit on separate hosts: `auth.hbomax.com`, `auth.hulu.com` and `amazon.com/ap/signin`.
7. **The YouTube /tv UA rule behaves identically in Chrome, Edge and Firefox** (V, a gate test from a Windows host):
   - The gate keys on the user-agent string only. Linux Chrome, Linux Edge and Linux Firefox user agents all got the ordinary site, exactly like a Windows desktop user agent, and PS4, Switch and Tizen user agents got Leanback.
   - The mechanism is `declarativeNetRequest` `modifyHeaders` in Chrome and Edge (MV3; pinheadseastar's `rules.json` proves it works in MV3) and DNR or webRequest in Firefox.
   - It is not yet tested inside an installed extension on real Linux (S6).
8. **Peacock's Linux block is OS-level, not browser-level** (R):
   - The error text names the OS ("your Linux settings are not supported", 6007), and ChromeOS users get the same code.
   - Chrome, Chromium, Vivaldi and Firefox all failed.
   - Changing the user agent did not help, and the landing page loads normally for Linux user agents (V), so the check runs at sign-in or playback.
   - UA spoofing is therefore no fix, and it would be a ToS problem anyway.
9. **Linux DRM ceilings are set by each service, capped by Widevine L3.** This is the same for Chrome and Firefox. Linux Edge has no Widevine by default (V, Microsoft Q&A), and PlayReady, the source of Edge's higher quality on Windows, is Windows-only. The launcher should show each service's known Linux ceiling to the user, and test S3 replaces the "~" cells with measurements.
10. **Hulu needs HDCP 1.4 even for 720p** (R), and Linux HDCP can fail where Windows passes on the same hardware (R, NVIDIA forum). Hulu on Linux may therefore be SD for a reason separate from Widevine.
11. **Couch sign-in is a real gap.**
    - None of the majors is known to offer a QR or phone sign-in on the desktop web. Netflix and Disney+ have none (R), code flows such as `amazon.com/code` and Max's TV sign-in page are TV-only (R), and the rest are U.
    - Apple TV+ demands Apple ID 2FA.
    - An on-screen keyboard, or phone-as-keyboard, is required, not optional.
12. **Disney+ and Hulu share one subscriber agreement** (2026-02-05, V) and are converging in the UI. Plan one adapter family with shared parts, and do not build a Hulu-specific adapter until the Disney+ side is stable.
13. **Some sites already do the work.** Jellyfin's TV layout (12.0, 2026-09-08, V) and YouTube /tv are built for remote focus. There the overlay should fill gaps only, not replace the site's own navigation.

## 5. New Phase 0 tests

This list adds to SYNTHESIS §7. Where a test extends an existing item, the item number is given as it stood on 2026-09-28; SYNTHESIS may be renumbered in parallel.

- **S1.** Linux Edge Widevine (file 24, the same as SYNTHESIS item 27). On clean Ubuntu and Fedora VMs, install stable Edge from packages.microsoft.com. Check `edge://flags/#edge-widevine-drm` and `edge://components`, then play Netflix or the Bitmovin/Shaka DRM demo with and without `--enable-features=msWidevinePlatform`. Record the Edge version and the result.
- **S2.** One hour on Linux: Spotify, Plex, Tubi, Pluto TV and Twitch in Chrome, Edge and full Firefox. Record plays or fails, and whether Tubi and Pluto use DRM, which decides their Linux Edge cells.
- **S3.** Measured Linux resolution, in Chrome and full Firefox, for Netflix, Disney+, Prime Video, Max, Apple TV+, Hulu, Crunchyroll, Paramount+, MGM+ and YouTube TV. This extends SYNTHESIS item 10, which covers only Netflix, Disney+, Prime and Max.
- **S4.** Peacock on Linux in 2026: sign in and play in Chrome and full Firefox. Does error 6007 still appear? The newest evidence is from 2023.
- **S5.** Max and Apple TV+ on Linux: blocked or playing, in Chrome and full Firefox. Max blocked Linux in 2020; Apple's status is unknown.
- **S6.** The YouTube /tv UA rule inside a real installed extension (DNR in Chrome and Edge; DNR or webRequest in Firefox) on real Linux, signed in, with playback, and with Client Hints left as the browser sends them.
- **S7.** YouTube TV on Windows Edge, which is not on the official list: does it play, and at what quality?
- **S8.** Logged-in DOM and keyboard probe of Peacock, Paramount+, Crunchyroll, MGM+, YouTube TV, Apple TV+, Hulu, Tubi, Pluto TV and Plex: UI technology, shadow roots, whether Tab reaches the tiles, and player shortcuts. This extends SYNTHESIS item 9, which covers only Netflix, Disney+, Prime and Max.
- **S9.** Does the Disney+ "S" key skip intros? One unattributed search summary says so.
- **S10.** Crunchyroll on Linux Firefox: does the Widevine component install and play? One unverified 2026 report says it does not.
- **S11.** MGM+ bought through Prime Video Channels: does it play inside Prime Video, so that the Prime adapter covers it?
- **S12.** Windows resolution for the unknown cells: Max on Firefox, Peacock on all three browsers, Paramount+ Premium 4K in a browser, and YouTube TV on Chrome and Firefox.

## 6. Where the sources conflict

- **Netflix on Linux, 1080p or 720p.** Netflix's help page (V) lists Chrome 117+, Edge 118+ and Firefox 129+ on Linux at "Up to Full HD (1080p)". Reports say Widevine L3 gives 720p in practice (R, 2018). **Trust 720p for planning:** the page is a support-matrix claim that also carries a "we can't guarantee" disclaimer, while the 720p figure is consistent with the L3 cap everywhere else. S3 settles it.
- **Linux Edge.** `21` assumed that Linux Edge equals Linux Chrome. `22` and `24` cite Microsoft's own answer: "Widevine DRM is not available on Linux now" (V), plus a user report that DRM broke in Edge 143 (R). Netflix's page lists Linux Edge 118+. **Trust `22` and `24`:** Microsoft's answer is primary, Netflix's list is not a test, and the user's decision already treats Linux Edge as non-DRM until S1.
- **How to seek on Netflix.** The carried gist says to use the internal player API. `21`'s verdict says to use key events and on-screen buttons. Both agree on the key point, which is never `currentTime`. SYNTHESIS item 13 tests the API.
- **Hulu's future.** `21` pass 1 said Hulu was "dissolving into Disney+". Pass 2 cites Variety (May 2026, R): the Hulu app is not being shut down. Trust pass 2 (it is newer), but the Tier 2 placement does not depend on it.
- **Tubi and Pluto TV on Linux Edge.** `23` marks them "supported" or "works" at the browser-family level. If either uses Widevine, Linux Edge fails. This is U until S2.

## 7. Biggest remaining UNKNOWNs

- Whether Edge on Linux can play Widevine at all, out of the box or with a flag (S1).
- Measured Linux resolution for every DRM service. Almost every Linux cell is "~", and only Prime's Linux Firefox SD is VERIFIED.
- Whether Peacock, Max and Apple TV+ block Linux today (S4, S5).
- Whether the /tv trick survives a real installed extension on Linux, and how long Google keeps the UA gate open.
- Player keys, UI technology and keyboard reach for Peacock, Paramount+, Crunchyroll, MGM+, YouTube TV and Apple TV+ (all U).
- The US HBO Max terms (only the France version was read) and the date of Crunchyroll's terms.
- US subscriber counts for almost every major service. Market weight in this file rests on the README's ordering, not on numbers.
- How the stores' review treats an unlisted "remote control for streaming sites" extension. SYNTHESIS lists this as not researched by anyone.
