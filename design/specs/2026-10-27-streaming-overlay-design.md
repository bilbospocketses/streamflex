# The D-pad streaming overlay: design

**Date:** 2026-10-27, revised 2026-11-03
**Status:** approved in brainstorming (2026-10-27), awaiting spec review. Revised 2026-11-03: eight built-in
providers, one rule for ads and skip buttons on every provider, and four rules from the terms review in
`design/research/overlay/30-terms-review-2026-11.md` (**Decisions**, **Rules the engine enforces**, **The overlay
layer**); Linux supports Firefox only, and Windows keeps three browsers with Edge recommended and each provider's
caveats shown (**Quality by browser**).
**Covers:** todo item 24: streaming services in a real browser, run fullscreen by StreamFlex and driven by a remote or
a gamepad through an injected 10-foot overlay.
**Ships as:** Phase 0 (measurements, no product code), then five releases, v0.7.0 to v0.11.0 (**Phases and
releases**). Each release has its own implementation plan.
**Builds on:**
- the research in `design/research/overlay/`: `README.md` (decisions D1-D11), `SYNTHESIS.md` (research passes R1-R7), `SERVICES.md`,
  the 2026-10-27 refresh, files `26-` to `29-`, and the terms review, file `30-` (2026-11-03);
- `design/specs/2026-09-29-menu-editing-design.md` (3c): its add flow, its on-screen keyboard and its layouts;
- `design/specs/2026-09-29-settings-all-settings-design.md` (3b): the bindings page and the list picker;
- `design/specs/2026-10-06-sdl3-port-design.md`: SDL3, the floors, and the rule that a duration the user feels is
  measured in milliseconds, not frames.

The service that serves adapters and links accounts is specified separately. This document covers everything the
public repo builds: the launcher side, the native host, the extension, and the built-in adapters. It consumes the
service as an interface (**The adapter source and cache**, **Linking a TV**).

## Context

The streaming services are the reason a living-room PC needs a 10-foot launcher, and none of them has a TV interface
in a desktop browser except YouTube. Running Android in a VM was rejected because an AOSP guest cannot reach Widevine
L1 (todo item 24). The research settled the direction on 2026-09-28: **a real browser, fullscreen, in a profile of its
own per service, with a force-installed extension that turns remote keys into site navigation.** It runs on Chrome,
Edge and Firefox on Windows, with Edge the recommended default, and on Firefox (the full, non-snap build) on Linux
(decided together, 2026-11-03).

Quality is decided by the browser and the OS, not by this design (`SYNTHESIS.md` §3): Edge on Windows is the only path
to Netflix 4K with HDR and Atmos; Chrome on Windows gets Netflix 4K in SDR (on by default since Chrome 143, file 26);
Firefox gets 1080p; Linux Firefox is Widevine L3 (about 720p, Prime SD), and on Linux arm64 (a Raspberry Pi) Firefox has
no Widevine, so DRM services are not offered there (file 27 §9). These are known limits of each target, shown to the
user (**Quality by browser**), never a reason to drop one.

## Decisions

The research's decisions (user, 2026-09-28) are in `design/research/overlay/README.md` and are not repeated here; this
design follows all of them, as amended by the research's own later rulings and by this design's 2026-11-03 revision,
which supersedes D2, refines D3, and replaces the like-for-like targets with Linux Firefox only (all recorded there as
dated amendments; the Linux Edge ruling is moot). The decisions below were made in this design's brainstorm and its
2026-11-03 revision.

| Question | Decision |
|---|---|
| Language | **C**, in the existing tree, for every native piece; the extension is **TypeScript**. A C++ rewrite or a UI framework is revisited at sub-project 4, not here (decided together, 2026-10-27). |
| Phase 0 | **Split:** qa-harness guests automate every measurement a guest can make; one short run sheet on the user's real HTPC covers the four that need a real GPU, display chain or remote (decided together, 2026-10-27). |
| Shape | **One spec, phased releases**, each with its own plan (decided together, 2026-10-27). |
| Adding a service | **A provider catalog:** *Add a streaming service* in the 3c add flow lists the providers, each with its icon and where it plays. A custom URL entry exists for any other site, with the generic engine only (decided together, 2026-10-27). |
| Which browser | **A global *Streaming browser* setting plus a per-tile override** (decided together, 2026-10-27). |
| Linux browsers | **Firefox only, the full non-snap build** (decided together, 2026-11-03; it amends the research's like-for-like targets, README and `SERVICES.md`): on Linux, Chrome and Firefox get the same DRM ceiling (Widevine L3); Linux Edge has no Widevine; Firefox is scoped per profile with no root (D10), while Chrome's force-install needs the root-owned `/etc/opt/chrome/policies`; and each extra Linux browser is another set of quirks to maintain. Chrome may be added later only if a provider plays in Chrome but not in Firefox there, or to give a Raspberry Pi DRM. Known limit: Firefox has no Widevine on Linux arm64, so a Raspberry Pi gets no DRM services (file 27 §9); known limits never defer Linux. Phase 0 G10 and G12 are retired. |
| Windows browsers | **Chrome, Edge and Firefox, with Edge the recommended default; the user sees each provider's caveats per browser** (decided together, 2026-11-03): Netflix gets 4K HDR, Dolby Vision and Atmos only in Edge (PlayReady), and Edge is never worse than the other two in `SERVICES.md` §1a. A provider that does not list a browser shows that as a caveat (**Quality by browser**). |
| Typing in a site | **The overlay's own on-screen keyboard**, in StreamFlex's look; a real keyboard also works. No OS keyboard ever appears, in the browser too (decided together, 2026-10-27; it extends the 3c rule). |
| Names | **CPTK identifiers from day one** for everything the overlay installs (decided together, 2026-10-27; todo item 27 renames the app later and moves nothing the overlay installed). |
| Native architecture | **A thin relay host; the launcher owns all state** (approach A, decided together, 2026-10-27). |
| Adapters | **Pure declarative data** with a closed four-action vocabulary (my call, from the Chrome Web Store's remote-code policy, file 26 §5 and file 29). |
| Extension reach | **Host permissions on all URLs, inert until a launch token arrives** (my call while writing this spec: self-hosted providers and custom sites live on origins unknown at build time). |
| Libraries | **libcurl** (HTTPS), **libsodium** (Ed25519, SHA-256), **yyjson** (JSON, vendored), **qrcodegen** (QR, vendored) (my call, from file 28). |
| Built-in providers | **Eight:** YouTube, Netflix, Jellyfin, Plex and Emby, plus **Tubi, Pluto TV and Twitch** (decided together, 2026-11-03). |
| Ads and skip buttons | **One rule for every provider, enforced by the engine** (user, 2026-11-03): ads are never blocked; no skip button is ever pressed automatically, for an ad, an intro or a recap, and StreamFlex has no auto-skip function. A provider's skip-ad control is reachable with the D-pad but never focused automatically. A skip-intro or skip-recap control is reachable and gets focus when it appears, and is never pressed automatically. This supersedes README D2 (a per-service ban carried in data) and refines D3 (**Rules the engine enforces**). |
| Scrubbing | **Allowed everywhere** (decided together, 2026-11-03): the remote sends only the site's own seek keys, so a site that refuses to seek during an ad still refuses. |
| The provider's player | **No write to any provider's player** (decided together, 2026-11-03, from the terms review, file 30): `disablePictureInPicture` is dropped, so picture-in-picture is whatever the provider allows. |
| Picture-in-picture | **Off where the browser allows it, with the fallbacks kept everywhere** (decided together, 2026-11-03): browser settings in StreamFlex's own profile and setup turn it off (Firefox's prefs; Chrome's and Edge's automatic picture-in-picture set to block in the profile's `Preferences`; Edge's hover button on Windows), never a write to the page; the player's Back rule and the launcher, which closes a picture-in-picture window and raises the main one, cover every browser (**Setup: what each browser needs**, Phase 0 G17). |
| The provider's page | **No write to the provider's page at all** (decided together, 2026-11-03, file 30): the overlay reads, moves focus with `focus()`, and draws inside its own layer; it sets no attribute, style, class or DOM node on the provider's elements. A test enforces it. The on-screen keyboard therefore types only through trusted keys (my call, from this rule; **The engine**). |
| What the overlay draws | **Our layers never hide the provider's player UI, advertising or notices for long** (decided together, 2026-11-03, file 30): the focus ring outlines and never covers; the keyboard and the overlay menu are temporary and close away; nothing the overlay draws covers an ad or the provider's copyright or trademark notices (**The overlay layer**). |
| Provider names and marks | **Not affiliated** (decided together, 2026-11-03, file 30): the README and the overlay's *About* text say StreamFlex is not affiliated with or endorsed by the streaming services it opens. The tile icons stay as `assets/icons/library/brands/NOTICE.md` handles them. |

## Architecture

```
 StreamFlex launcher (C, SDL3)                    Browser: Chrome, Edge or Firefox
 ├─ provider catalog       ─┐                     ├─ CPTK Overlay extension (TypeScript)
 ├─ browser discovery       │ spawns, supervises  │   ├─ background: one native port
 ├─ launch recipes          ├────────────────────►│   └─ content script: engine, focus ring,
 ├─ supervisor (job / pgrp) │                     │      overlay layer, keyboard, player mode,
 ├─ input bridge ─── SendInput / uinput ─────────►│      pointer mode, adapter interpreter
 ├─ overlay channel ◄──── named pipe / socket ──┐ │
 ├─ adapter cache                               │ └─ spawns per port ► cptk-overlay-host (C)
 └─ Account page (Link this TV)                 └────────────── stdio ◄─┘
```

**Launcher units.** Each is one file (or a pair) under `src/overlay/`, with one job and a narrow interface:

| Unit | File | Job |
|---|---|---|
| Provider catalog | `catalog.c` | Loads the built-in providers and any the adapter source adds; answers "can provider P play in browser B on this OS" and "what does P get there" (**Quality by browser**). |
| Browser discovery | `browsers.c` | Finds usable Chrome, Edge and Firefox on Windows, and a full Firefox on Linux; refuses a snap or Flatpak Firefox with a reason. |
| Launch recipes | `recipe.c` | Builds the command line and prepares the profile for a provider in a browser. |
| Supervisor | `supervise.c`, `platform/supervise_win32.c`, `platform/supervise_linux.c` | Spawns the browser, detects its exit, closes it on Home. |
| Input bridge | `inputbridge.c` + platform files | The Home hook, gamepad-to-key translation while the browser is in front, trusted key and pointer injection. |
| Overlay channel | `channel.c` + platform files | The per-user local socket, its framing, the launch tokens, and the message handlers. |
| Adapter cache | `adcache.c`, `adsource.c` | The cache on disk and the background fetches from the adapter source. |
| Account page | `settings_account.c` | *Link this TV*, the linked state, *Unlink*. |
| Setup | `setup.c` + platform files | Writes and checks what each browser needs (policy, host registration). |

**Separate pieces:**
- `src/overlay_host/host.c`: the `cptk-overlay-host` executable, a stateless relay (**The channel**).
- `extension/`: the extension source, its two manifests, its tests, and `extension/adapters/` (the built-in adapters).
- `assets/streaming/providers.ini`: the built-in catalog.

**What each piece owns.** The launcher owns the adapter cache, key and pointer injection, the browser process, Home,
and which tile a browser window belongs to. The host owns nothing: a browser starts one per native port and kills it
when the port closes (file 28 §5), so it only relays. The extension owns what happens inside the page.

## Providers and tiles

### The catalog

`assets/streaming/providers.ini` holds the **eight built-in providers: YouTube, Netflix, Jellyfin, Plex, Emby, Tubi,
Pluto TV and Twitch.** Each has a library icon already (`branding/library/brands.ini`). One section per provider:

```ini
[netflix]
title=Netflix
icon=netflix
url=https://www.netflix.com/browse
drm=yes
quality.windows.edge=supported 2160p hdr dv atmos
quality.windows.chrome=supported 2160p
quality.windows.firefox=supported 1080p
quality.linux.firefox=supported 720p

[youtube]
title=YouTube
icon=youtube
url=https://www.youtube.com/tv
fallback_url=https://www.youtube.com/
user_agent=tv
drm=no

[jellyfin]
title=Jellyfin
icon=jellyfin
url=ask
drm=no

[tubi]
title=Tubi
icon=tubi
url=https://tubitv.com/
drm=yes

[pluto-tv]
title=Pluto TV
icon=pluto-tv
url=https://pluto.tv/
drm=yes

[twitch]
title=Twitch
icon=twitch
url=https://www.twitch.tv/
drm=no
```

- `url=ask` marks a self-hosted provider (Jellyfin, Emby): adding its tile asks for the server address with the
  on-screen keyboard, prefilled `http://`. Plex uses `https://app.plex.tv/desktop` and needs no address.
- `drm=yes` means the provider needs a working CDM: it is not offered on Linux aarch64, where Firefox has no Widevine
  (file 27 §9), so a Raspberry Pi gets no DRM services. A provider can also name OSes it never plays on
  (`blocked_os=linux` for a service that blocks Linux).
- Tubi and Pluto TV are `drm=yes` until research test S2 (`SERVICES.md` §5) shows whether they use DRM; the research
  marks it unknown. A wrong `yes` only keeps them off Linux aarch64; a wrong `no` would offer a service that
  cannot play there. Twitch plays without DRM (`SERVICES.md`).
- There is no ad or skip field. One rule covers every provider (**Rules the engine enforces**), so no catalog entry, and
  no entry from the adapter source, can change it.
- `quality.<os>.<browser>` is the provider's row in **Quality by browser** (below), one key per OS and browser.
- `user_agent=tv` selects StreamFlex's built-in TV user-agent string (README D4); `fallback_url` is used when the
  browser cannot override the user agent or the `/tv` page refuses it.

**Providers from the adapter source.** The adapter source's manifest may list further providers with the same fields
(**The adapter source and cache**). They join the catalog at run time; one whose adapter is unavailable to this device
shows the manifest's reason text on its tile and in the add list. StreamFlex adds no wording of its own to that text.

### Quality by browser

Each provider carries what it gets in each browser on each OS, so the user sees the caveats before choosing a
browser (decided together, 2026-11-03). A key `quality.<windows|linux>.<edge|chrome|firefox>` (on Linux, `firefox`
only) holds space-separated words:
- one status: `supported` (the provider lists the browser, or it is reported to play there), `unlisted` (the
  provider's list leaves it out; it may still play) or `blocked` (the provider refuses it);
- the highest resolution reported (`2160p`, `1080p`, `720p`, `480p`), or `unknown`; a `blocked` key carries none;
- any of `hdr`, `dv` (Dolby Vision) and `atmos`.

A missing key means nothing is known, and no caveat is shown. A `blocked` browser is treated as `blocked_os` is: the
provider is not offered there. A key with an unknown word is ignored whole, with a log line naming it
(`Overlay: <provider>: unreadable quality key <key>`); the provider stays.

The text is neutral, facts only, built from the key: *up to 4K, HDR, Dolby Vision, Atmos*; *up to 1080p*; *does not
list this browser*; *does not play in this browser* (2160p reads 4K). A provider that does not list a browser shows
that as its caveat there. The built-in values are seeded from `SERVICES.md` §1a (the 2026-09-28 research, reported
cells included), and change only with new research:

| Provider | Windows Edge | Windows Chrome | Windows Firefox | Linux Firefox |
|---|---|---|---|---|
| Netflix | supported 2160p hdr dv atmos | supported 2160p | supported 1080p | supported 720p |
| YouTube | supported 2160p | supported 2160p | supported 2160p | supported 2160p |
| Jellyfin, Plex | supported unknown | supported unknown | supported unknown | supported unknown |
| Emby | no key (§1a has no row) | no key | no key | no key |
| Tubi | supported unknown | supported unknown | supported unknown | unlisted unknown |
| Pluto TV | supported unknown | supported unknown | supported unknown | supported unknown |
| Twitch | supported 1080p | supported 1080p | supported 1080p | supported unknown |

**Where it shows.** On Windows, *Settings › Streaming › Streaming browser* marks Edge *(recommended)* and gives each
browser row, for every provider with a tile in the menus, that provider's line wherever the browsers differ for it,
for example *Edge (recommended): Netflix up to 4K, HDR, Dolby Vision, Atmos · Chrome: Netflix up to 4K · Firefox:
Netflix up to 1080p*. A tile's ***Browser ›*** page gives each browser row the tile's own provider line. On Linux
there is one browser, so the lines show only on the tile's page, as what the provider gets there.

### A streaming tile

A tile is an ordinary menu entry whose command is a new special command:

```
:stream <provider> [browser=<edge|chrome|firefox>] [url=<server address>]
```

- `browser=` is the per-tile override; without it the tile follows *Settings › Streaming › Streaming browser*.
- `url=` is the server address of a self-hosted provider.
- 3c's add flow gains ***Streaming service ›*** beside *App ›*: the catalog, alphabetical, each row showing the
  provider's icon and either nothing (it plays here) or why it does not (*Needs a DRM browser*, *Not available on
  Linux*, or the manifest's reason). Choosing one writes the entry with the provider's title and icon.
- The tile's page (hold OK) gains ***Browser ›*** on Windows: *Follow the streaming browser* or one installed browser,
  each with the provider's line from **Quality by browser**. On Linux, Firefox is the only browser, and the page shows
  its line.
- ***Custom site ›*** in the same list takes a URL through the keyboard and writes `:stream custom url=<url>`: the
  generic engine and pointer mode only, no adapter.

### The *Streaming* settings page

*Settings › Streaming* holds:
- ***Streaming browser ›***: on Windows, the installed, usable browsers only, each with its **Quality by browser**
  lines; the default is Edge, marked *(recommended)* (decided together, 2026-11-03). On Linux the row shows the full
  Firefox, the only Linux browser. With none usable it reads *No browser found* and leads to the install offer
  (release R5).
- ***Set up streaming ›***: runs and shows **Setup** for each installed browser, with a status line per browser
  (*Ready*, or what is missing).
- ***Account ›***: **Linking a TV**.
- At the foot, one line: *StreamFlex is not affiliated with or endorsed by the streaming services it opens. Their
  names are used only to identify them, and their marks belong to their owners.* The overlay's *About* says the same.

## Browser discovery

Run at startup and when *Settings › Streaming* opens; a few `stat()` calls or registry reads, no state file.

- **Windows:** each browser's `App Paths` registry key (`HKLM` then `HKCU`) and its known install folders; the file
  must exist.
- **Linux** (file 24): Firefox only (decided together, 2026-11-03). A full Firefox is an ELF
  `firefox` (or `firefox-bin`) beside `application.ini` or `omni.ja` in `/usr/lib/firefox`, `/usr/lib64/firefox`,
  `/usr/lib/firefox-esr`, `/opt/firefox`, `~/.local/opt/firefox`, or the realpath of `firefox` on `PATH`. A
  `/usr/bin/firefox` that resolves into a snap and a Flatpak-only Firefox (`flatpak info`) are refused, each with its
  reason, because their sandboxes cannot reach the native host. Chrome and Edge are not looked for on Linux.
- The result is a list of usable browsers with their absolute paths. Launches use those paths, never `PATH`.

## The launch path

1. **Resolve.** The tile's provider, plus its browser (the override, else the global setting). If that browser is not
   usable now, or the provider cannot play there, StreamFlex shows a notice naming why and launches nothing.
2. **Prepare the profile** `<data>/profiles/<browser>/<provider>/` (created on first use; one per browser per provider,
   because profile formats differ; a custom site gets `custom-<first 16 hex of the URL's SHA-256>`).
   - **Chromium (Chrome, Edge; Windows only):** `<exe> --user-data-dir=<dir> --app=<url>#cptk=<token> --start-fullscreen
     --window-position=<x>,<y> --no-first-run --disable-session-crashed-bubble`, with the launcher's display origin as
     the position; for `user_agent=tv`, add
     `--user-agent=<TV UA>`. Before each launch, and only while the profile is not running, the launcher sets the
     `auto-picture-in-picture` content setting to block in the profile's `Default/Preferences` (**Picture-in-picture**,
     under Setup), creating the file on first use and changing nothing else in it.
   - **Firefox:** before each launch, and only while that profile is not running (its `parent.lock` is free), write
     `user.js`:
     ```
     user_pref("extensions.autoDisableScopes", 0);
     user_pref("extensions.startupScanScopes", 1);
     user_pref("media.autoplay.default", 0);
     user_pref("media.videocontrols.picture-in-picture.enabled", false);
     user_pref("media.videocontrols.picture-in-picture.video-toggle.enabled", false);
     user_pref("dom.media-pip.enabled", false);
     user_pref("dom.documentpip.enabled", false);
     user_pref("general.useragent.override", "<TV UA>");
     ```
     (the four picture-in-picture lines: **Picture-in-picture**, under Setup; the last line only for
     `user_agent=tv`; `startupScanScopes` makes Firefox rescan the profile scope at every
     start, so a replaced XPI is picked up, which Phase 0 G13 confirms), and copy the signed XPI shipped with
     StreamFlex to `<dir>/extensions/overlay@cptk.app.xpi` when it differs. Then
     `<exe> -kiosk <url>#cptk=<token> -profile <dir> -no-remote`, plus `-wait-for-browser` on Windows.
3. **The token.** Each launch gets a fresh 128-bit random token, hex-encoded, in the first URL's fragment. The
   content script moves it into the profile's `storage.local` and removes it from the address (`history.replaceState`).
   The launcher keeps a table of live tokens, each tied to its provider, browser and process; a token dies with its
   browser.
4. **Spawn and supervise.**
   - **Windows:** `CreateProcessW` with `CREATE_SUSPENDED`, assign a Job Object with `KILL_ON_JOB_CLOSE`, no breakaway
     flags and no UI limits, and a completion port; resume. `JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO` means the browser is
     gone (file 28 §7: nested jobs work on Windows 8 and later; Firefox's own `-wait-for-browser` job coexists).
   - **Linux:** `fork`, `setpgid(0, 0)`, `execv` of the discovered path; `waitpid` on the leader, then a check that the
     process group is empty.
5. **While the browser runs** the launcher stays resident behind it and draws nothing:
   - it serves the overlay channel;
   - it translates gamepad input into keys for the browser (**The input bridge**);
   - it keeps the display awake (`SDL_DisableScreenSaver`) and parks the pointer in a corner at launch;
   - if a picture-in-picture window opens anyway (Chrome's automatic picture-in-picture, the browser's own control, a
     site's button), the launcher closes the picture-in-picture window and brings the browser's main window back to
     the front, since the remote cannot drive the small floating window and it stays on top of every other window;
     closing it returns the video to the page. The close acts on the browser's window (`WM_CLOSE`, or
     `_NET_CLOSE_WINDOW` on X11, as the window's own close button would), never on a provider's page (file 30). On
     Windows it sees the new top-level window among the job's processes;
   - it logs `Overlay: <provider> started in <browser> (pid <n>)`.
6. **Home** (**The input bridge**) closes the browser: `WM_CLOSE` to every top-level window of the job's processes, or
   `SIGTERM` to the process group; after a **5 s grace period** (a functional timer, logged when it fires), the job is
   terminated or the group gets `SIGKILL`.
7. **Return.** On exit the launcher raises its window, redraws the menu with the tile still selected, and logs
   `Overlay: <provider> closed (home | exited | crashed, code <n>)`.

**One browser at a time.** While one runs, the launcher's menu is hidden behind it, so a second launch cannot start.

## The input bridge

- **Home.** A launcher binding on 3b's bindings page: by default the remote's Home key (`Browser_Home` on Windows,
  `KEY_HOMEPAGE` on Linux) and the gamepad's Guide button, rebindable. While a browser runs it is caught by:
  - **Windows:** a `WH_KEYBOARD_LL` hook on its own thread, whose callback only posts a message and returns at once
    (Windows silently removes a hook slower than `LowLevelHooksTimeout`, file 28 §9); the hook is re-armed if it is
    found removed.
  - **Linux:** the GlobalShortcuts portal where the desktop offers it (GNOME 48 and later; KDE's first version is
    unknown), else the remote's own evdev device read without a grab.
  - The extension's `home` message does the same, so Home works even where the hook cannot see the key.
- **Gamepad to keys.** SDL keeps reporting the gamepad while the browser has focus
  (`SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1`). The launcher translates: D-pad and stick to arrows (with the
  launcher's wall-time repeat, 500 ms then every 25 ms), A to Enter, B to Backspace, Start to the overlay menu key,
  Guide to Home, the shoulder buttons to Page Up and Page Down. SDL's source says Windows.Gaming.Input and DirectInput
  do not deliver Xbox One pads in the background while XInput and RawInput do (file 28 §6), so the launcher pins a
  backend that does (Phase 0 G8 picks it).
- **Trusted injection.** On request from the extension (**The channel**), and for the gamepad translation:
  - **Windows:** `SendInput` for keys and pointer (absolute moves, left click). UIPI allows equal or lower integrity,
    and the browser runs at the launcher's medium integrity.
  - **Linux:** one `uinput` device with keys, relative pointer axes and a left button, created at the first need. The
    packages install `/usr/lib/udev/rules.d/70-cptk-uinput.rules`:
    `KERNEL=="uinput", SUBSYSTEM=="misc", TAG+="uaccess", OPTIONS+="static_node=uinput"`. Without access the launcher
    logs it, and the gamepad and pointer mode do not work in the browser; remotes that are keyboards still do.

## Setup: what each browser needs

**Names.** Native host `app.cptk.overlay`; extension *CPTK Overlay*, one listing in the Chrome Web Store, one in Edge
Add-ons, and an unlisted signed XPI from AMO (gecko id `overlay@cptk.app`). The Edge listing is required: on a machine
not joined to a domain, Edge force-installs only items listed in Edge Add-ons (file 28 §10).

**Windows** (StreamFlex ships as a zip with no installer). *Set up streaming* writes, for the current user only and
with no elevation:
- `HKCU\Software\Policies\Google\Chrome\ExtensionInstallForcelist`: a new numbered value
  `<CWS id>;https://clients2.google.com/service/update2/crx`, added beside any existing values, never replacing them;
- `HKCU\Software\Policies\Microsoft\Edge\ExtensionInstallForcelist`: the same with the Edge Add-ons id and
  `https://edge.microsoft.com/extensionwebstorebase/v1/crx`; plus `StartupBoostEnabled=0` and
  `BackgroundModeEnabled=0`, without which Edge outlives its window, and `PictureInPictureOverlayEnabled=0`
  (**Picture-in-picture**, below). The setup page says these also apply to the user's everyday Edge, and that the
  last one also hides the picture-in-picture hover button in the everyday Edge on that Windows account.
- `HKCU\Software\{Google\Chrome,Microsoft\Edge,Mozilla}\NativeMessagingHosts\app.cptk.overlay` pointing at two host
  manifests in `<data>/overlay/` (Chromium's with `allowed_origins` naming both store ids; Firefox's with
  `allowed_extensions`), whose `path` is the absolute path of `cptk-overlay-host.exe` beside `streamflex.exe`.
- Chrome and Edge read user-scope policy from HKCU (file 28 §10, verified in Chromium's loader); whether a standard
  user may write `HKCU\Software\Policies` is Phase 0 item G2. **If not**, setup runs a small elevated helper once
  (`ShellExecuteExW` with `runas`) that writes the same policy under HKLM; that needs a keyboard or mouse for the UAC
  prompt unless G3 shows a remote can answer it, and the page says so before it starts. Host registration stays in
  HKCU either way.
- At every start the launcher re-checks the host registration and silently rewrites the HKCU host keys if the zip was
  moved. Policy is written only from the setup page.

**Linux** (Firefox only, decided together 2026-11-03; no browser policy is written). Every package (`.deb`, the
Fedora tarball, Arch) installs, as root:
- the Firefox host manifest in `/usr/lib/mozilla/native-messaging-hosts/` (`.deb`, Arch) or `/usr/lib64/mozilla/...`
  (Fedora); the launcher also writes the per-user `~/.mozilla/native-messaging-hosts/app.cptk.overlay.json`, which
  every non-sandboxed Firefox reads;
- the signed XPI in `/usr/share/streamflex/overlay/`, and the uinput udev rule.
- The extension reaches the profile by the sideload (**The launch path**, step 2), so nothing under `/etc/opt` is
  written: Chrome's force-install would need the root-owned `/etc/opt/chrome/policies`, one reason Linux is Firefox
  only.

**Picture-in-picture** (decided together, 2026-11-03). A video in a floating window cannot be driven from the
remote, so StreamFlex turns picture-in-picture off where the browser itself allows it. These are browser settings,
written by the launcher and setup, never writes to a provider's page (file 30):
- **Firefox:** four prefs in the profile's own `user.js` (**The launch path**, step 2), all false:
  `media.videocontrols.picture-in-picture.enabled`, `media.videocontrols.picture-in-picture.video-toggle.enabled`,
  `dom.media-pip.enabled` (a site's `requestPictureInPicture()`) and `dom.documentpip.enabled` (Document
  picture-in-picture). They are not locked, but the launcher rewrites `user.js` before every start, so each launch
  begins with them off. No Firefox policy is written: a policy reaches every profile of that Firefox, the user's
  everyday one included, and README D10 scopes Firefox by profile.
- **Chrome and Edge, automatic picture-in-picture** (decided together, 2026-11-03): the content setting
  `auto-picture-in-picture` (allow, block or ask; default ask) is set to **block** in the StreamFlex profile's own
  `Preferences` before each launch (**The launch path**, step 2). No policy sets it, and Chromium may reset a
  protected pref it finds changed outside the browser, so Phase 0 G17 measures whether it holds; where it does not,
  the fallbacks below cover it.
- **Edge (Windows):** also `PictureInPictureOverlayEnabled=0`, beside the hygiene policies. It hides only the hover
  button on a video, needs Edge 118 or later, and does not apply to a profile signed in with a Microsoft account.
  Edge has no other picture-in-picture policy.
- **Chrome:** no other setting. Chrome has no picture-in-picture policy, so a site's own button and the browser's
  hover control stay as the browser has them.
- **The fallbacks stay on every browser:** Back in the player never sends the page a key that opens
  picture-in-picture (**The engine**, Player), and the launcher closes a picture-in-picture window that opens anyway
  and brings the main window back to the front (**The launch path**, step 5). On Chrome beyond the automatic setting,
  and on Edge beyond it and the hover button, they are the only protection.
- *Set up streaming* says so: *StreamFlex's browser profile turns picture-in-picture off where the browser allows
  it, because a video in a floating window can't be driven from the remote.*

**Status.** *Set up streaming* shows, per installed browser: policy present (Windows), host registered,
and whether the extension has ever said `hello` in that browser. The last is the only proof the extension is installed.

## The extension

**Build.** One TypeScript source in `extension/src/`, bundled with esbuild into two packages from two generated
manifests:
- **Chromium (Chrome and Edge, on Windows):** MV3, a service-worker background.
- **Firefox:** MV3, an event-page background, `browser_specific_settings.gecko.id`, `strict_min_version` at the oldest
  supported ESR (140 today, file 27 §8).

Permissions: `nativeMessaging`, `storage`, and host permissions `<all_urls>`. All URLs are needed because
self-hosted providers and custom sites live on origins unknown at build time; the extension does nothing on a page
until the launch token says the profile is StreamFlex's. A policy-installed extension raises no permission prompt, and
Firefox grants an MV3 add-on's declared origins at install (file 27 §4). The store listings state the reason.

The extension's version is its own semver, released to the stores separately from StreamFlex's version; the Firefox
XPI ships inside each StreamFlex release (no `update_url`), so Firefox gets engine fixes with StreamFlex releases.

**The content script** runs in the isolated world, in all frames (`match_origin_as_fallback` on Chromium), at
`document_start`:
1. If the URL fragment holds `cptk=<token>`, store it in `storage.local` and strip it from the address.
2. With no stored token, stop: no listener, no port, no DOM change. This is the inert switch for the user's everyday
   profile (README D10).
3. Otherwise the top frame asks the background to open the native port and say `hello`. A `welcome` brings the
   provider and the theme; `inert` stops the script as in step 2.
4. It asks for the adapter. With one, the engine runs it; with `{adapter: null, reason}`, a full-screen notice shows
   the reason and only Home and Back work, and either one closes the browser and returns to StreamFlex, so the notice
   never stays over a page in use; a custom site runs the generic engine with no adapter.

**The overlay layer.** One element per top frame with a **closed** shadow root and adopted stylesheets, so page CSS
cannot reach it. It draws the focus ring, toasts, the overlay menu, the on-screen keyboard and notices, at StreamFlex's
sizes and colors (the theme's colors are passed in `welcome`). The pointer is the OS pointer; the layer draws a large
ring around it in pointer mode. The layer element is the overlay's own and the only node it adds to the page
(**Rules the engine enforces**).

**What the layer may cover** (decided together, 2026-11-03; the providers' terms ask that the player, its controls,
its advertising and its copyright and trademark notices stay visible, file 30). The layer never hides the provider's
player UI, advertising or notices for long:
- **The focus ring outlines the focused control and never covers it.** It is drawn outside the control's border box,
  around it, with nothing inside the box.
- **Everything else the layer draws is temporary and closes away.** The keyboard closes when typing is done or on
  Back; the overlay menu closes on Back or when an item is chosen; a toast closes on its own after a few seconds (a
  functional timer, one constant); a notice closes on Home or Back. In player mode nothing but the ring stays on
  screen.
- **Nothing the layer draws covers an ad or the provider's copyright or trademark notices.** While the adapter's
  `adShowing` selectors match a visible element (**The adapter format**), the layer draws only the focus ring: a toast
  waits until the ad ends, and the menu and the keyboard do not open (Home still works, through the launcher). With
  no adapter the engine cannot see an ad, and the rule rests on the layer's other limits: an outline only, and
  temporary panels.

**Keys.** One handler on `window` in the capture phase owns every key in every mode (one owner per key per mode):

| Key | Browse | Player | Keyboard | Pointer |
|---|---|---|---|---|
| Arrows | move focus | the adapter's player map | move on the keyboard | move the pointer |
| Enter (OK) | activate | the adapter's player map | type the key | click |
| Backspace, Esc (Back) | close overlay UI, else go back a route | leave player mode | close the keyboard | leave pointer mode |
| Menu (Apps key, pad Start) | overlay menu | overlay menu | overlay menu | overlay menu |

**Back at the start route** shows a toast, *Press Back again to return to StreamFlex*; a second Back while the toast
shows sends `home`.

**The overlay menu:** *Home*, *Pointer mode*, *Reload*, *Back to <provider>'s start*, *About*. *About* shows the
extension's version and the line *StreamFlex is not affiliated with or endorsed by the streaming services it opens.
Their names are used only to identify them, and their marks belong to their owners.*, and closes on Back.

### The engine

- **Browse.**
  - **Candidates:** visible elements that are focusable or clickable (`a[href]`, `button`, `[tabindex]`, ARIA roles
    `button`, `link`, `menuitem`, `tab`, `option`, `gridcell`), plus the adapter's `focus.include`, minus its
    `focus.exclude`. Open shadow roots are walked; closed ones through `openOrClosedShadowRoot` (Chromium's
    `chrome.dom.openOrClosedShadowRoot`, Firefox's `element.openOrClosedShadowRoot` property, file 27 §7).
  - **Moving:** the nearest candidate in the pressed direction, scored by the WICG spatial-navigation distance (the
    spec's algorithm, implemented here; the polyfill is archived). With an adapter `focus.row`, Up and Down keep the
    column, as a TV row does.
  - **Showing:** the focused element is scrolled to the center (`scrollIntoView({block: 'center'})`) and ringed. A
    candidate that can take `focus()` gets it; one that cannot (a clickable element with no `tabindex`) is held as the
    engine's own focus and only ringed. The engine never adds a `tabindex` or any other attribute to make it focusable.
  - **Change:** a `MutationObserver` re-collects candidates when the page adds content; the focus survives if its
    element does.
  - **Routes:** on every route change (history API or the URL changing) the adapter's `initialFocus` for that route is
    applied, in the first animation frame in which a selector matches. Without one, the first candidate in reading
    order gets focus. This is the lesson from the TV builds: a SPA route with no initial focus makes the first press
    do nothing.
- **Player.** Entered when the route is an adapter's player route, or a `<video>` larger than half the viewport is
  playing. Keys follow the adapter's `player.keys`; without an adapter: OK and Space play or pause, Left and Right seek
  with the arrow keys, Up and Down are volume. Player keys are sent as **trusted keys** through the launcher, since
  sites read `isTrusted`. The engine writes nothing to the player: picture-in-picture is whatever the provider allows
  (decided together, 2026-11-03, file 30). **Back in the player** leaves player mode and sends nothing to the page,
  unless the adapter's `player.keys` maps `Back` to one action (a `sendKey`, or a `click` on the player's own back
  control); the remote's Back key itself never reaches the page. This matters on Hulu, whose Esc minimizes the video
  to picture-in-picture (file 21, Hulu §5, REPORTED): a Hulu adapter never maps Back to Escape, and names Hulu's
  documented control once a guest test confirms it. A picture-in-picture window that opens anyway is the launcher's
  to undo (**The launch path**, step 5).
- **Keyboard.** Opens when a text field or `contenteditable` gets focus. Its layouts come from 3c's on-screen
  keyboard (`osk.c`'s layout tables, exported to JSON at build time, so both keyboards stay one design: QWERTY, the ABC
  grid, the *àé* page). Each character typed goes to the launcher (`send_text`, **The channel**), which types it into
  the focused field as trusted OS keystrokes, as a real keyboard does; the key handler lets through the keystrokes it
  asked for. The engine never sets a field's value and never dispatches an `InputEvent` (the rule against writing to
  the page, below), so the only thing that reaches the provider's page is what a real keyboard would type. A real
  keyboard types as usual.
- **Pointer.** The fallback when an adapter is broken or a site is unknown: arrows ask the launcher to move the real
  pointer (accelerating on hold, in wall time), OK is a real click. Chosen from the overlay menu; Back leaves it, and
  the launcher parks the pointer in its corner again (`pointer` `park`). The overlay never hides the pointer with a
  style on the page (no `cursor: none`).

### Rules the engine enforces

- **One press, one action** (README D3, as refined 2026-11-03). Nothing acts on a timer or on its own: no auto-skip,
  no auto-next, and no setting or field that turns either on. The one thing that happens without a press is focus
  moving to a skip-intro or skip-recap control when it appears (below). It moves focus only; only a press acts.
- **Ads and skip buttons: one rule for every provider** (user, 2026-11-03; it supersedes README D2's per-service ban):
  - Ads are never blocked, and no skip button is ever pressed automatically, for an ad, an intro or a recap.
    Automatic skipping of intros and recaps is the provider's own setting, where it offers one.
  - **A skip-ad control** (the adapter's `adSkipControls`) is an ordinary candidate: the user reaches it with the
    D-pad and presses OK (in player mode, Back leaves player mode first, as for any control). The engine never moves
    focus to it on its own: an `initialFocus`, the reading-order default, re-placing a lost focus, and a key's
    `focus` or `click` action never land on it. In pointer mode a click on it is the user's own click, like any other.
  - **A skip-intro or skip-recap control** (`player.controls.skipIntro`, `player.controls.skipRecap`) is reachable
    the same way, and when one appears the engine moves focus to it, so one OK skips. It appears when an element
    matching the selectors becomes a visible, enabled candidate where none was; the `MutationObserver` that
    re-collects candidates sees it, and it is focused once per appearance, not taken back if the user moves away.
    While it has focus, OK presses it in any mode; in player mode any other key does what the player map says and
    takes focus off it.
  - **Manual scrubbing is allowed everywhere** (decided together, 2026-11-03). The remote sends only the site's own
    seek keys (jump back or forward, hold to fast-forward), so a site that refuses to seek during an ad still refuses.
- **Trust is never faked:** no `dispatchEvent` of keyboard or mouse events for anything a site checks; such actions go
  to the launcher. The extension never touches EME, the media pipeline, or the site's network traffic.
- **No write to the provider's page** (decided together, 2026-11-03, file 30). The overlay only reads the page, moves
  focus with `focus()` (and scrolls the focused element into view), and draws inside its own closed-shadow-root layer.
  It sets no attribute, style, class, `tabindex` or property on the provider's elements, the `<video>` included, and
  adds, moves or removes none of their nodes; its one layer element is its own. Typing goes through trusted keys
  (**The engine**), and the pointer is parked by the launcher, never hidden with a page style. Removing the launch
  token from the address (`history.replaceState`, **The launch path**) changes only the URL's fragment StreamFlex
  added, never a page element.

## The adapter format

An adapter is one JSON object for one provider, **data only**. The Chrome Web Store allows remote configuration that
decides enabled features and forbids "building an interpreter to run complex commands fetched from a remote source,
even if those commands are fetched as data" (file 26 §5). The format is held to selectors, flags and single actions so
that it stays on the allowed side; the precedent is a remotely refreshed JSON rule set already in both stores (file
29), and the plan asks Chrome Web Store support in writing before the first submission.

```json
{
  "format": 1,
  "provider": "jellyfin",
  "version": 4,
  "minEngine": 1,
  "routes": [
    { "id": "home",   "path": "/web/#/home*",  "initialFocus": [".card", "button.headerHomeButton"] },
    { "id": "player", "path": "/web/#/video*", "player": true }
  ],
  "focus":   { "include": [".card", ".listItem"], "exclude": [".hide"], "row": ".itemsContainer" },
  "player":  { "keys": { "OK": { "sendKey": "Space" }, "Left": { "sendKey": "ArrowLeft" } },
               "controls": { "skipIntro": [".skipIntro"], "skipRecap": [] } },
  "adSkipControls": [],
  "adShowing": [],
  "browsers": { "firefox": { "focus": { "include": [".card", ".listItem", ".itemAction"] } } }
}
```

(The selectors above illustrate the shape; R2 writes the real ones.)

- **Actions** are exactly four: `focus` (a selector list), `click` (a selector list), `sendKey` (a key name from a
  fixed list), `scroll` (a direction). A key maps to **one** action. There are no sequences, conditions, expressions,
  variables or regular expressions.
- **Skip controls** come in two kinds, so the engine can treat them apart (**Rules the engine enforces**):
  `adSkipControls` names the provider's skip-ad controls (reachable, never focused or pressed by the engine), and
  `player.controls.skipIntro` and `player.controls.skipRecap` name its skip-intro and skip-recap controls (focused when
  they appear, never pressed by the engine). All three are selector lists, not actions, so the four-action vocabulary
  is unchanged. The schema has no field that makes the engine press anything on its own.
- **Ad state.** `adShowing` is a selector list whose visible match means an ad is on screen (the player's ad state, or
  an ad element on a page). It only keeps the layer clear of the ad (**What the layer may cover**); it never changes
  what a key does, and it is not an action. There is no typing field: the keyboard always types through trusted keys.
- **Selectors** are CSS selector lists tried in order, first match wins. **Paths** are globs over the URL's path and
  fragment (`*` is any run of characters).
- **Versioning.** `format` is the schema's major version; the engine refuses one it does not know. `minEngine` is the
  lowest engine (extension) version the adapter needs. `version` identifies one published adapter. The app serves
  the version the newest accepted manifest names, even if it is lower than one it served before, so the source rolls
  a bad adapter back by pointing its manifest at an earlier version (**The adapter source and cache**).
- **Per-browser overrides.** `browsers.<chrome|edge|firefox>` replaces single leaf fields for that browser.
- **Validation.** `extension/adapters/schema.json` (JSON Schema). CI validates every adapter in the repo against it;
  the engine validates again at load and rejects the **whole** adapter on any miss, falling back to the generic engine
  with a toast (*<Provider>'s controls could not be loaded; using basic navigation*) and a log line naming the field.
- **No page-world code in v1.** Netflix is driven by its documented keys sent as trusted keys, never its internal
  player API (`SYNTHESIS.md` §4.8; setting `video.currentTime` breaks Netflix with M7375).
- **The built-in adapters** are `extension/adapters/<provider>.json`. They ship inside StreamFlex's packages, where
  they seed the adapter cache; the extension bundles none and always asks the launcher. An adapter is a few KB, far
  under the host's 1 MB message limit.

## The adapter source and cache

**The source** is a base URL, `https://api.cptk.app` by default, with the source's Ed25519 public keys built into
StreamFlex and selected by `key_id`. A test hook (`STREAMFLEX_TEST_ADAPTER_SOURCE` and `STREAMFLEX_TEST_ADAPTER_KEY`,
honored only by test builds, as the harness's other test hooks are) points it at the harness's stub source.

**What StreamFlex calls:**
- `GET /adapters/manifest`, with the device token when the TV is linked: a signed envelope whose payload lists each
  provider's current adapter version, its SHA-256 and its URL, or marks it unavailable. Optional fields carry catalog
  entries for providers not built in (the catalog fields above) and a reason text for an unavailable provider.
- `GET /adapters/<provider>/<version>`: either the adapter file's bytes, checked against the manifest's SHA-256, or a
  signed envelope holding the adapter, with the device token. `401` means the token is no longer valid (**Linking a
  TV**); `403` means the account may not have that adapter, and the manifest's reason is shown.

**Checking.** An envelope is `{payload, signature, key_id}`: the Ed25519 signature (libsodium,
`crypto_sign_verify_detached`) is checked over the payload's bytes **exactly as received**, before parsing (yyjson). An
unknown `key_id` is refused. Inside an envelope, the adapter's bytes for hashing are cut from the payload text, not
re-serialized: they are everything after the first `,"adapter":` up to the payload's final `}`. A plain adapter file is
hashed as received (`crypto_hash_sha256`).

**Fetching.** libcurl on an SDL background thread, with certificate verification on (Schannel on Windows, the
distro's TLS on Linux): at startup, when a tile opens, and when an adapter carrying an expiry nears it (a lead time
held in one launcher constant). **A launch never waits on the network**; it uses what is cached. A failed fetch backs
off (1 min, doubling, capped at 1 h) and is logged once per backoff step.

**The cache** is `<data>/adapters/`: `manifest.envelope` as received, and `<provider>/<version>.json` (or `.envelope`).
Files are written atomically (write a temporary file, then rename), readable by the user only. It is seeded at first
start from the built-in adapters. **The newest accepted manifest is the authority:** for each provider the cache
serves exactly the version that manifest names, higher or lower than before, once that version's file passes every
check; until it does, the **last good copy** keeps serving. A manifest whose `issued_at` is older than the last
accepted one is refused, so an old manifest cannot be replayed to force an old version. An adapter whose expiry has passed is not served; the provider then shows *Can't reach the adapter
source to renew <provider>* while offline, or the manifest's reason.

When the cache gains or changes an adapter for a provider whose browser is live, the launcher pushes
`adapter_updated`, and the page takes the new adapter without a restart.

## Linking a TV

*Settings › Streaming › Account* is the app's side of the device-code flow (RFC 8628 shape, JSON bodies).

- **Link this TV:** `POST /device/code` with `{"device_name": <name>}`; the name starts as the computer's host name
  and can be edited with the on-screen keyboard (1 to 64 characters, no control characters). The page shows the user
  code (`KXQ-4M7` form) large, the verification address, a QR code of the complete verification link (qrcodegen,
  drawn as SDL rectangles with a quiet zone), and a countdown from `expires_in`.
- It polls `POST /device/token` every `interval` seconds; `slow_down` lengthens the wait; `authorization_pending`
  polls again; `expired_token`, `access_denied` and `invalid_grant` end the attempt with a message and a *Try again*
  row; an `access_denied` with a `reason` shows that reason.
- **On success** the device token is stored in `<data>/account/token`, readable by the user only, and **never
  logged**; the manifest is fetched at once.
- **Linked:** the plan name the service reports, the account's devices (`GET /account`; this TV marked), and
  ***Unlink*** (`DELETE /device`), which drops the token and fetches the manifest again without one.
- **A `401`** on any call drops the token; the page reads *This TV was unlinked. Link it again.*

## The channel

**Transport.** The launcher listens on a per-user local socket:
- **Windows:** `\\.\pipe\cptk-overlay-<user SID>`, created with a DACL granting the current user only; the launcher
  checks the client's user (`GetNamedPipeClientProcessId`, then that process's token) before reading.
- **Linux:** `$XDG_RUNTIME_DIR/cptk-overlay/socket`, in a mode-0700 directory; the launcher checks `SO_PEERCRED`.

**The host.** The browser starts `cptk-overlay-host` for each native port (Chromium passes the caller's origin,
Firefox the manifest path and add-on id, file 28 §5). It connects to the launcher's socket, passes the origin or add-on
id, then copies frames both ways until either side closes, and exits. With no launcher listening it answers every
`hello` with `inert` and exits. It holds no state, reads no file, and logs nothing but its own start failures (to the
launcher's log when the socket is up, else to stderr, which the browser discards).

**Framing.** Both legs use Native Messaging's framing: a 32-bit native-endian length, then UTF-8 JSON. Host to
extension is capped at 1 MB; extension to host at 64 MiB (Chrome's documented cap, file 28 §5). An oversize or
malformed frame closes the connection.

**Messages:**

| From the extension | The launcher's reply |
|---|---|
| `{"type":"hello","token":…,"browser":…,"extVersion":…}` | `{"type":"welcome","provider":…,"theme":{…},"engineMin":…}`, or `{"type":"inert"}` when the token is unknown or dead |
| `{"type":"adapter"}` | `{"type":"adapter","adapter":{…}}` or `{"type":"adapter","adapter":null,"reason":…}` |
| `{"type":"home"}` | closes the browser (**The launch path**, step 6) |
| `{"type":"send_keys","keys":[…]}` | `{"type":"ok"}` or `{"type":"error","error":…}` after injecting |
| `{"type":"send_text","text":…}` | `{"type":"ok"}`, or `{"type":"error","error":…,"untyped":…}` naming the characters it could not type |
| `{"type":"pointer","op":"move","dx":…,"dy":…}`, `{"type":"pointer","op":"click","x":…,"y":…}` or `{"type":"pointer","op":"park"}` | same |
| `{"type":"log","level":…,"line":…}` | none; written to StreamFlex's log as `Overlay: <provider>: <line>`, at most 20 lines a second |

| Pushed by the launcher | Meaning |
|---|---|
| `{"type":"adapter_updated"}` | ask for the adapter again |

Every message except `hello` needs a connection that has received `welcome`. `send_keys` takes only key names from the
same fixed list adapters use; `pointer` moves are clamped to the browser's display. `send_text` carries the on-screen
keyboard's typed characters, printable ones only (no control characters), at most 256 a message, and its text is
never logged. The launcher types each character as trusted keystrokes: on Windows `SendInput` with
`KEYEVENTF_UNICODE`; on Linux through uinput, with the key and Shift or AltGr that produce it in the session's
keyboard layout. A character that layout cannot produce is not typed and is named back in `untyped`, and the overlay
keyboard says so with a toast.

## Logging

All launcher lines start `Overlay:` (overlay work) or `Account:` (linking), following the log's existing style:
launches and exits, setup results per browser, cache updates (`Overlay: the <provider> adapter is now version <n>`),
rejections (`Overlay: the manifest failed its signature check`), the extension's own lines, and every fallback taken.
Never logged: the device token, the launch tokens, typed text, and any page address beyond the provider's origin.

## Errors and fallbacks

| Failure | What the user sees | Log |
|---|---|---|
| Browser missing or unusable | the tile's notice names it and points to *Settings › Streaming* | `Overlay: <browser> is not usable (<reason>)` |
| Provider cannot play in this browser or OS | the tile's notice (*Needs a DRM browser*, *Not available on Linux*) | the reason |
| Profile already open elsewhere | *<Provider> is already open in <browser>* | the lock file |
| Spawn fails | a notice with the system error | the error |
| No `hello` within 20 s of the launch | nothing in the browser; Home still works; *Set up streaming* shows the browser as not ready | `Overlay: no contact from the extension in <browser>` |
| Host finds no launcher | the extension stays inert | none |
| Adapter invalid | toast; generic engine | the failing field |
| Manifest or adapter fails a check | nothing; the last good copy stays | the check that failed |
| Network down | nothing; cached adapters serve until any expiry | once per backoff step |
| Browser crashes | the menu returns | `crashed, code <n>` |

## Testing

- **C unit tests (ctest):** recipe command lines and profile preparation per browser and OS (`user.js` content, the
  XPI copy rule, the fragment token); catalog resolution and the plays-here rules for all eight built-in providers,
  and that the catalog has no ad or skip key (an unknown key changes nothing); the quality keys: each built-in
  provider's seeded row, the caveat text built from a key (Netflix's four lines, word for word), *does not list this
  browser* for `unlisted`, a `blocked` browser not offered, an unknown word ignoring its whole key with the log line,
  and *(recommended)* on Windows Edge only; discovery against fake trees, including that Linux reports no Chrome or
  Edge; the
  cache with a fixed clock (signature, hash, the adapter-bytes cut, replay refusal, a rollback to a lower version, expiry, last good copy, `401` and
  `403`); channel framing and limits; the token table; the host's relay against a fake launcher; QR output against
  known vectors.
- **Extension (vitest):** candidate collection and direction scoring on fixed layouts; route and initial-focus
  handling; mode switching and key ownership; the skip controls: a skip-ad control is reachable with the D-pad but
  never focused or pressed by the engine (not on appearance, not by `initialFocus`, the reading-order default, a lost
  focus re-placed, or a key's `focus` or `click` action), and a skip-intro or skip-recap control gets focus when it
  appears and is never pressed by the engine; that no adapter field can turn on automatic pressing (the schema is
  closed, and a fixture that tries fails); schema validation, including that every repo adapter passes and each broken
  fixture fails; the keyboard types only through `send_text`, never setting a field's value or dispatching an event;
  in player mode the remote's Back never reaches the page, and only an adapter's `Back` mapping sends anything;
  what the layer may cover: the ring's drawn box lies outside the focused control's border box on every side and
  draws nothing inside it, and while an `adShowing` selector matches a visible element the layer draws only the ring
  (a toast waits, Menu opens nothing, the keyboard does not open). **Playwright** runs each built-in adapter against
  saved page snapshots in Chromium with the extension loaded; Firefox runs through `web-ext` and WebDriver BiDi.
- **No write to the provider's page (Playwright, both browsers):** for each built-in adapter's snapshots, served with
  their own scripts removed so every mutation is the overlay's, a `MutationObserver` on the document (`subtree`,
  `childList`, `attributes`, `characterData`) records every mutation of a provider-owned node during a scripted
  session (browse, open a title, player mode, the keyboard typing a search, the overlay menu, pointer mode, Back), and
  the test requires zero. The overlay's own layer element, everything inside it, and the one insertion that adds it
  are excluded; nothing else is.
- **Headless harness, a new lane:** a full Firefox on Xvfb in the Debian image (Linux's one browser), a Jellyfin
  container with fixture media, the extension installed by the profile sideload, the stub adapter source. Keys go in
  through XTEST; checks read the `Overlay:` lines (launch, `hello`,
  route, focus moves, player mode, Home, exit). Runs in CI like the other shards.
- **qa-harness guests:** the Windows path end to end (setup without elevation, Edge, Chrome and Firefox against
  Jellyfin, Plex and Emby containers), and Firefox on the Linux desktops.
- **Smoke doc:** a new module, **19. Streaming overlay**, gains its rows and automation halves in each release's PR
  (the binding rule for user-visible changes). Its hands-on rows are only what no lane can see: picture and sound
  quality through the user's display chain, and a real remote.

## Phase 0: measure before building

No product code. It uses stock browsers, a trivial extension in both flavors (published unlisted to Edge Add-ons and
the Chrome Web Store for the deployment tests, and signed by AMO), a throwaway C spawner and a key-logger page. Item
numbers in brackets are `SYNTHESIS.md` §7's.

**In qa-harness guests (automated):**

| # | Measures | Pass | If it fails |
|---|---|---|---|
| G1 [2] | Windows 11 Home: the unlisted store items force-install from **HKCU** policy (Chrome, Edge) into a fresh per-service profile, and the content script runs in an `--app=` window; the same from HKLM | installed and running at first launch, no prompt | HKLM only: setup's elevated path becomes the default. Neither: back to the user (Alternative B, WebView2, or Chrome Enterprise Core) |
| G2 [new] | A standard user can write `HKCU\Software\Policies\…` | the write succeeds | setup uses the elevated helper |
| G3 [new] | The UAC consent prompt can be answered with arrows, Tab and Enter | answered | the elevated setup is documented as a keyboard step |
| G4 [5] | Exit detection for the three Windows browsers and Linux Firefox, and Edge lingering with the hygiene policies | the launcher sees the exit within 2 s of the last window closing | fall back to watching the browser's windows |
| G5 [7] | `SendInput` and uinput keys arrive `isTrusted` and grant user activation (fullscreen, media start); uinput keys reach the focused browser on GNOME and KDE Wayland | yes everywhere | Chromium: CDP `Input.*` for trusted actions; Firefox: back to the user |
| G6 [8] | After the browser closes the launcher is in front and takes input (Windows 11, GNOME Wayland, KDE, X11), 20 of 20 | yes | `SDL_HINT_FORCE_RAISEWINDOW` and a raise retry; on GNOME Wayland the launcher keeps its window fullscreen behind the browser so closing reveals it |
| G7 [6, partial] | The key codes Home, Back, Menu and the media keys produce through each path the launcher hooks | the hook sees them | rebind the defaults |
| G8 [14] | The gamepad reaches SDL while the browser has focus, per Windows backend (XInput, RawInput, WGI) with an emulated Xbox pad, and on Linux evdev | at least one Windows backend, and Linux | the bridge pins the working backend; with none, gamepads are documented as launcher-only on that OS |
| G9 [20] | Windows Firefox: native messaging from a kiosk profile, `-wait-for-browser` inside the job | round trip works; exit seen | adjust the Firefox glue |
| G10 [26] | **Retired 2026-11-03** (Linux supports Firefox only; decided together). It measured Linux Chrome and Edge policy from `/etc/opt/*/policies/managed/`. The id is not reused. | not run | none |
| G11 [24] | Linux discovery classifies Ubuntu default (snap), Ubuntu with Mozilla's deb, Debian ESR, Fedora, Arch, a `/opt` tarball and a Flatpak-only box correctly | all right | fix the probes |
| G12 [27] | **Retired 2026-11-03** (Linux supports Firefox only; decided together). It measured Linux Edge Widevine, the research's S1, which is moot. The id is not reused. | not run | none |
| G13 [28-31, 33, 35] | The Firefox profile sideload: enabled with no prompt, runs in `-kiosk`, a replaced XPI is picked up (with and without `startupScanScopes`), the everyday Firefox untouched, host permissions granted | all pass | Firefox takes the policy fallback with the inert switch (README D10) |
| G14 [32] | A fresh Firefox profile downloads its Widevine CDM, and how long it takes | it does | setup pre-warms each new profile |
| G15 [4] | A one-hour DRM test-stream soak with the extension active, per browser | no error | shrink the content script's footprint |
| G16 [3, 34] | Chrome and Edge on Windows, `Extensions.loadUnpacked` over a debug pipe (the development loop only) | loads | development uses the store builds |
| G17 [new] | Picture-in-picture, per browser and OS (the three on Windows, Firefox on Linux): each setting in **Picture-in-picture** holds after a restart, Firefox's four prefs across the `user.js` rewrite at each start; a site's `requestPictureInPicture()` (and Document picture-in-picture) fails harmlessly where Firefox's prefs are set; Chrome's and Edge's `auto-picture-in-picture` block in the profile's `Preferences` holds across launches or is reset; the launcher sees a picture-in-picture window, closes it and brings the main window back (Windows through the job's processes, Linux X11, Linux Wayland, where it may not be able to list windows) | every setting holds; the site's request fails with no broken page; the window is seen and closed, and the main window raised within 2 s on Windows and X11 | a setting that does not hold is dropped and the fallbacks carry it; a reset `auto-picture-in-picture` block is left to the fallbacks; where the launcher cannot see the window (Wayland is the likely case), the Back rule is the only protection there, documented as a known limit |
| G18 [new] | Typing: `send_text` reaches a React text field as trusted input on Windows (`KEYEVENTF_UNICODE`) and in Linux Firefox (uinput through the session's layout, on GNOME and KDE Wayland and X11), letters, digits, punctuation and the *àé* page; on Linux the probe records each character's path (direct, dead or compose); on Wayland, whether the launcher can learn the session's layout at all is unknown | every character arrives in the field, `isTrusted`, and the React state matches | Windows: type the keyboard's characters through virtual-key codes where `KEYEVENTF_UNICODE` fails; Linux: a character the layout cannot produce is refused with its toast; if the layout cannot be learned on Wayland, back to the user |

**On the user's HTPC (one run sheet, written for the user, about an hour):**

| # | Measures | Pass | If it fails |
|---|---|---|---|
| H1 [1] | Netflix's stats overlay (resolution, DRM level, HDR, audio) in stock Edge, then one variable at a time: own profile, `--app=` and fullscreen, the trivial extension; then Chrome; then Firefox (`-profile`, `-kiosk`, sideloaded extension); Disney+ and Prime at 1080p in Edge | every variable matches its baseline | drop the variable that costs quality; if the extension itself costs it, back to the user (Alternative B or WebView2) |
| H2 [6] | What the user's remote sends for every button, short and long, through the logger page and the launcher's raw-input logger | every button reaches a key | rebind the defaults, or name the remotes that work |
| H3 [11] | Edge's Netflix audio at the AVR (5.1, Atmos) | as the matrix says | the stated ceiling changes |
| H4 [12] | 2160p HDR through the real chain; playback after sleep and resume | as the matrix says | resume: the browser is closed on resume and the user relaunches the tile |

**Not run:** items 9 (each site's DOM) and 16 (sign-in methods) are adapter work in R2 and R3; 10 and 21 are known
limits; 13 is moot (no internal player API); 15's checks fold into H1; 17 (WebView2) runs only if H1 fails; 18 is for
the record only; 19 runs only if G13 fails; 22 and 23 are out of scope.

**Output:** `design/research/overlay/phase0/` holds each item's result, the run sheet and the H results, and a short
summary of which branches above were taken. Any "back to the user" result stops the build at that point.

## Phases and releases

Each release has its own plan, its smoke rows, and its own CHANGELOG section.

| | Ships | Done when |
|---|---|---|
| **P0** | Phase 0 (above). No release. | every G item not retired and every H item has a result, and its branch is recorded |
| **R1 v0.7.0: launch and Home** | `providers.ini` with the built-in providers; discovery; recipes (Firefox without the XPI); the supervisor; the Home binding and hook; the gamepad bridge; `:stream`; *Streaming service ›* and *Custom site ›* in the add flow; *Settings › Streaming › Streaming browser*. No extension. | a remote starts Netflix in Edge at full quality, the site's own keyboard navigation works, and one Home press returns to the menu, every time |
| **R2 v0.8.0: the engine** | the extension (both flavors) and its store submissions; `cptk-overlay-host`; the channel and tokens; the cache, seeded from built-in adapters only (no network); *Set up streaming*; the Linux package files and udev rule; the engine on **Jellyfin, Plex and Emby** (Emby on its Desktop layout, since its TV layout is not free, file 29) | browse, open a title, play and come back with the D-pad alone on each, in the three Windows browsers and Linux Firefox |
| **R3 v0.9.0: Netflix, YouTube, Tubi, Pluto TV and Twitch** | the Netflix adapter and player mode; YouTube `/tv` with its user agent and the fallback; the Tubi, Pluto TV and Twitch adapters; the overlay keyboard; pointer mode; the overlay menu | an evening of use with no keyboard or mouse, including one search per service and one sign-in on Netflix and on YouTube; research test S2 has run on Linux, and Tubi's and Pluto TV's `drm` match what it found |
| **R4 v0.10.0: the adapter source and Account** | the networked source, the manifest's catalog entries and reasons; *Account* and linking | a linked TV takes adapters from the source, and keeps working while the source is unreachable |
| **R5 v0.11.0: Linux browser install** | the full-Firefox offer when no usable Firefox is found (README D9, as amended 2026-11-03), the `pkexec` helper per distro with the text fallback | a default Ubuntu box gets a working full Firefox from one button; a box with a usable browser never sees the offer |

**Before R2's submissions:** the plan writes to Chrome Web Store support asking whether the adapter format is within
the remote-code policy, and waits for the answer before submitting.

## Out of scope

- Automatic skipping of any kind, for ads, intros or recaps, and any function to turn it on (user, 2026-11-03): a
  provider's own auto-skip setting, where it has one, is the user's to set with the provider.
- Auto-next and other automatic actions: README D3, revisited per service later.
- Detecting site changes and alerting users to adapter updates: the two follow-ups todo item 24 schedules after the
  initial services ship.
- HDR switching and refresh-rate matching: sub-project 4 and later.
- A phone keyboard, macOS and Safari, snap and Flatpak browsers, Chromium.
- Chrome and Edge on Linux (decided together, 2026-11-03). Chrome may be added later only if a provider plays in
  Chrome but not in Firefox on Linux, or to give a Raspberry Pi DRM; Linux Edge has no Widevine.
- Every provider beyond the built-in eight: their adapters come through the adapter source.
