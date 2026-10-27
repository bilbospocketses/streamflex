# StreamFlex: D-pad streaming overlay research (started 2026-09-28)

This is the research for a later StreamFlex sub-project: it would make a Windows or Linux PC work like an HTPC for the DRM streaming sites, using a standard D-pad remote. The approach is a real browser run fullscreen, with a 10-foot navigation overlay injected into the page.

- `00-original-dpad-streaming-overlay-research.md` is the user's starting research, copied from C:\Temp (the original is deleted).
- Files `1x-*.md` are the deeper research, one per topic, written by research agents on 2026-09-28.
- `SYNTHESIS.md` combines them into findings, open questions, and a recommended prototype path.
- Files `26-` to `29-` are the 2026-10-27 refresh, written for the design (`design/specs/2026-10-27-streaming-overlay-design.md`): `26` re-checks the time-sensitive facts and the store policies, `27` closes the Firefox gaps, `28` covers the native side (libraries, Native Messaging, SDL3, supervision, injection, Windows policy without an installer), and `29` covers Emby and the adapter format.

Background, from the StreamFlex session: an Android-in-a-VM approach was rejected. An AOSP guest cannot reach Widevine L1.

## Decisions (user)

- **2026-09-28: the extension ships as an UNLISTED listing in both the Chrome Web Store and Edge Add-ons,** force-installed through the registry policy `ExtensionInstallForcelist`. This works on Windows Home without a domain: the domain rule restricts only an off-store extension source, not the policy itself. Per-site adapters are fetched remotely as JSON data, so selector fixes need no store review.
- **2026-09-28: consider Firefox and Safari if they are not too difficult.** Research is in `15-firefox-safari.md`. Safari implies a macOS port of StreamFlex.
- **2026-09-28: platform scope.** Windows and Linux only; macOS (Safari) is a backburner item, revisited only if StreamFlex gains mass popularity.
- **2026-09-28: browser targets.** Windows: **Chrome, Edge and Firefox**. Linux: **Firefox only**, the full (non-snap) version, which StreamFlex recommends. The snap Firefox is not investigated, because the full version is known to work. (This supersedes SYNTHESIS.md's "Firefox is a Windows fallback only" and "Linux: Chrome/Firefox".)
- **2026-09-28 (later): browser targets are like-for-like: three browsers on two OSes.** Windows: Chrome, Edge and Firefox. **Linux: Chrome, Edge and Firefox** (the full non-snap Firefox). Users choose among the three primary browsers on either OS. This **supersedes the "Linux: Firefox only" line above**, and SYNTHESIS.md and the 2x files follow it.
- **2026-09-28: Linux Edge is a NON-DRM browser** until a Phase 0 hands-on test shows Widevine working in stable Edge on Linux (the evidence leans against it; see file 24's "Linux Edge and Widevine" section). On Linux it covers the DRM-free services (YouTube, Jellyfin, Plex, Twitch and the like). If the test fails, Linux Edge stays, for non-DRM services only; it is not dropped.
- **2026-09-28: the Linux installer checks for a full Firefox.** If none is present, StreamFlex's install offers to download it, tagged "highly recommended", and shows the prompt only when it is needed. The feasibility of doing this in each package format is to be evaluated.
- **2026-09-28: the service list.** The majors come first: Netflix, Disney+, Prime Video, Max, Apple TV+, Peacock, Hulu, Crunchyroll, Paramount+, possibly MGM+, and maybe a few others. Per-service research is in `2x-*.md` and the tiering is in `SERVICES.md`.
- **2026-09-28: the SERVICES.md decisions (the user took the controller's recommendations on all of them):**
  - **D1** Paramount+ stays **Tier 3**: its ToS bans "enhancing" any portion of the player, and the overlay must drive playback.
  - **D2** ad-skip is banned **per service, enforced by the engine** (YouTube TV first); the adapter data carries the ban.
  - **D3** v1 acts **only on remote presses**: no auto-skip and no auto-next. Automatic actions are revisited per service later.
  - **D4** YouTube: the **`/tv` UA rule with a fallback** to the standard site.
  - **D5** Peacock: a **Windows-only adapter**.
  - **D6** Linux Edge **fails S1** if DRM plays only with an unsupported flag (`--enable-features=msWidevinePlatform`); it stays non-DRM.
  - **D7** **Jellyfin and Plex web move to Tier 1.**
  - **D8** audio services stay **Tier 2 optional** (Spotify), later.
  - **D9** the Linux installer offers **any of the three browsers** (Chrome, Edge, full Firefox) when none is installed, **defaulting to Firefox**. This supersedes "checks for a full Firefox" above.
  - **D10** the extension is scoped **by profile, never by installation** (`25-extension-scope.md`). **Firefox:** the signed unlisted XPI is copied into each StreamFlex profile's `extensions/` folder, with `extensions.autoDisableScopes=0` in that profile's `user.js`, and no policy file is written. **Chrome and Edge:** the store listing plus `ExtensionInstallForcelist` (as decided), with the extension **inert outside StreamFlex's profiles**; Phase 0 also probes CDP `Extensions.loadUnpacked`. Pending the Phase 0 tests in file 25 §5. If the Firefox sideload fails, Firefox falls back to the Chrome/Edge approach.
  - **D11** the Firefox extension is **unlisted on AMO** (self-distributed, signed), to match the Chrome and Edge store decision.
- `01-browser-market-share.md` is the user's desktop browser and OS market-share report (StatCounter-based), kept for reference.
