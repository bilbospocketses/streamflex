# The D-pad streaming overlay: design

**Date:** 2026-10-27
**Status:** approved in brainstorming (2026-10-27), awaiting spec review.
**Covers:** todo item 24: streaming services in a real browser, run fullscreen by StreamFlex and driven by a remote or
a gamepad through an injected 10-foot overlay.
**Ships as:** Phase 0 (measurements, no product code), then five releases, v0.7.0 to v0.11.0 (**Phases and
releases**). Each release has its own implementation plan.
**Builds on:**
- the research in `design/research/overlay/`: `README.md` (decisions D1-D11), `SYNTHESIS.md` (research passes R1-R7), `SERVICES.md`,
  and the 2026-10-27 refresh, files `26-` to `29-`;
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
Edge and Firefox, on Windows and on Linux.

Quality is decided by the browser and the OS, not by this design (`SYNTHESIS.md` §3): Edge on Windows is the only path
to Netflix 4K with HDR and Atmos; Chrome on Windows gets Netflix 4K in SDR (on by default since Chrome 143, file 26);
Firefox gets 1080p; Linux browsers are Widevine L3 (about 720p, Prime SD). These are known limits of each target,
shown to the user, never a reason to drop one.

## Decisions

The research's decisions (user, 2026-09-28) are in `design/research/overlay/README.md` and are not repeated here; this
design follows all of them, as amended by the research's own later rulings (Linux Edge is non-DRM, the targets are
like-for-like). The decisions below were made in this design's brainstorm.

| Question | Decision |
|---|---|
| Language | **C**, in the existing tree, for every native piece; the extension is **TypeScript**. A C++ rewrite or a UI framework is revisited at sub-project 4, not here (decided together, 2026-10-27). |
| Phase 0 | **Split:** qa-harness guests automate every measurement a guest can make; one short run sheet on the user's real HTPC covers the four that need a real GPU, display chain or remote (decided together, 2026-10-27). |
| Shape | **One spec, phased releases**, each with its own plan (decided together, 2026-10-27). |
| Adding a service | **A provider catalog:** *Add a streaming service* in the 3c add flow lists the providers, each with its icon and where it plays. A custom URL entry exists for any other site, with the generic engine only (decided together, 2026-10-27). |
| Which browser | **A global *Streaming browser* setting plus a per-tile override** (decided together, 2026-10-27). |
| Typing in a site | **The overlay's own on-screen keyboard**, in StreamFlex's look; a real keyboard also works. No OS keyboard ever appears, in the browser too (decided together, 2026-10-27; it extends the 3c rule). |
| Names | **CPTK identifiers from day one** for everything the overlay installs (decided together, 2026-10-27; todo item 27 renames the app later and moves nothing the overlay installed). |
| Native architecture | **A thin relay host; the launcher owns all state** (approach A, decided together, 2026-10-27). |
| Adapters | **Pure declarative data** with a closed four-action vocabulary (my call, from the Chrome Web Store's remote-code policy, file 26 §5 and file 29). |
| Extension reach | **Host permissions on all URLs, inert until a launch token arrives** (my call while writing this spec: self-hosted providers and custom sites live on origins unknown at build time). |
| Libraries | **libcurl** (HTTPS), **libsodium** (Ed25519, SHA-256), **yyjson** (JSON, vendored), **qrcodegen** (QR, vendored) (my call, from file 28). |

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
| Provider catalog | `catalog.c` | Loads the built-in providers and any the adapter source adds; answers "can provider P play in browser B on this OS". |
| Browser discovery | `browsers.c` | Finds usable Chrome, Edge and full Firefox; refuses snap, Flatpak and Chromium with a reason. |
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

`assets/streaming/providers.ini` holds the **built-in providers: YouTube, Netflix, Jellyfin, Plex and Emby.** Each has
a library icon already (`branding/library/brands.ini`). One section per provider:

```ini
[netflix]
title=Netflix
icon=netflix
url=https://www.netflix.com/browse
drm=yes
adskip=allowed

[youtube]
title=YouTube
icon=youtube
url=https://www.youtube.com/tv
fallback_url=https://www.youtube.com/
user_agent=tv
drm=no
adskip=banned

[jellyfin]
title=Jellyfin
icon=jellyfin
url=ask
drm=no
adskip=allowed
```

- `url=ask` marks a self-hosted provider (Jellyfin, Emby): adding its tile asks for the server address with the
  on-screen keyboard, prefilled `http://`. Plex uses `https://app.plex.tv/desktop` and needs no address.
- `drm=yes` means the provider needs a working CDM: it is not offered under Linux Edge (non-DRM by decision until
  Phase 0 item G12 passes) nor on Linux aarch64 (no Widevine for Firefox there, file 27 §9; Chrome arm64's CDM is
  unmeasured). A provider can also name OSes it never plays on (`blocked_os=linux` for a service that blocks Linux).
- `adskip=banned` is the per-service ad-skip ban (README D2). The **catalog** carries it, not the adapter, so an
  adapter can never lift it.
- `user_agent=tv` selects StreamFlex's built-in TV user-agent string (README D4); `fallback_url` is used when the
  browser cannot override the user agent or the `/tv` page refuses it.

**Providers from the adapter source.** The adapter source's manifest may list further providers with the same fields
(**The adapter source and cache**). They join the catalog at run time; one whose adapter is unavailable to this device
shows the manifest's reason text on its tile and in the add list. StreamFlex adds no wording of its own to that text.

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
- The tile's page (hold OK) gains ***Browser ›***: *Follow the streaming browser* or one installed browser.
- ***Custom site ›*** in the same list takes a URL through the keyboard and writes `:stream custom url=<url>`: the
  generic engine and pointer mode only, no adapter.

### The *Streaming* settings page

*Settings › Streaming* holds:
- ***Streaming browser ›***: the installed, usable browsers only. Default: Edge on Windows, Firefox on Linux (README D9).
  With none usable it reads *No browser found* and leads to the install offer (release R5).
- ***Set up streaming ›***: runs and shows **Setup** for each installed browser, with a status line per browser
  (*Ready*, or what is missing).
- ***Account ›***: **Linking a TV**.

## Browser discovery

Run at startup and when *Settings › Streaming* opens; a few `stat()` calls or registry reads, no state file.

- **Windows:** each browser's `App Paths` registry key (`HKLM` then `HKCU`) and its known install folders; the file
  must exist.
- **Linux** (file 24): an ELF at `/opt/google/chrome/chrome` and `/opt/microsoft/msedge/msedge`; a full Firefox is an ELF
  `firefox` (or `firefox-bin`) beside `application.ini` or `omni.ja` in `/usr/lib/firefox`, `/usr/lib64/firefox`,
  `/usr/lib/firefox-esr`, `/opt/firefox`, `~/.local/opt/firefox`, or the realpath of `firefox` on `PATH`. A
  `/usr/bin/firefox` that resolves into a snap, a Flatpak-only browser (`flatpak info`), and Chromium are refused,
  each with its reason, because their sandboxes cannot reach the native host or they have no CDM.
- The result is a list of usable browsers with their absolute paths. Launches use those paths, never `PATH`.

## The launch path

1. **Resolve.** The tile's provider, plus its browser (the override, else the global setting). If that browser is not
   usable now, or the provider cannot play there, StreamFlex shows a notice naming why and launches nothing.
2. **Prepare the profile** `<data>/profiles/<browser>/<provider>/` (created on first use; one per browser per provider,
   because profile formats differ; a custom site gets `custom-<first 16 hex of the URL's SHA-256>`).
   - **Chromium (Chrome, Edge):** `<exe> --user-data-dir=<dir> --app=<url>#cptk=<token> --start-fullscreen
     --window-position=<x>,<y> --no-first-run --disable-session-crashed-bubble`, with the launcher's display origin as
     the position; on Linux add `--ozone-platform=wayland` or `x11` to match the session; for `user_agent=tv`, add
     `--user-agent=<TV UA>`.
   - **Firefox:** before each launch, and only while that profile is not running (its `parent.lock` is free), write
     `user.js`:
     ```
     user_pref("extensions.autoDisableScopes", 0);
     user_pref("extensions.startupScanScopes", 1);
     user_pref("media.autoplay.default", 0);
     user_pref("general.useragent.override", "<TV UA>");
     ```
     (the last line only for `user_agent=tv`; `startupScanScopes` makes Firefox rescan the profile scope at every
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
  `BackgroundModeEnabled=0`, without which Edge outlives its window. The setup page says these also apply to the
  user's everyday Edge.
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

**Linux.** Every package (`.deb`, the Fedora tarball, Arch) installs, as root:
- `/etc/opt/chrome/policies/managed/cptk-overlay.json` and `/etc/opt/edge/policies/managed/cptk-overlay.json`
  (`ExtensionInstallForcelist`, and for Edge the two hygiene policies);
- the Chromium host manifest in `/etc/opt/chrome/native-messaging-hosts/` and `/etc/opt/edge/native-messaging-hosts/`,
  and the Firefox one in `/usr/lib/mozilla/native-messaging-hosts/` (`.deb`, Arch) or `/usr/lib64/mozilla/...`
  (Fedora); the launcher also writes the per-user `~/.mozilla/native-messaging-hosts/app.cptk.overlay.json`, which
  every non-sandboxed Firefox reads;
- the signed XPI in `/usr/share/streamflex/overlay/`, and the uinput udev rule.
- Chromium takes each policy from the lexicographically last managed file that sets it, replacing lists (file 24 §6.1).
  *Set up streaming* scans both managed directories and reports any other file that sets `ExtensionInstallForcelist`
  as *Another policy file overrides StreamFlex's*, naming it; it never edits other files.

**Status.** *Set up streaming* shows, per installed browser: policy present (or the overriding file), host registered,
and whether the extension has ever said `hello` in that browser. The last is the only proof the extension is installed.

## The extension

**Build.** One TypeScript source in `extension/src/`, bundled with esbuild into two packages from two generated
manifests:
- **Chromium (Chrome and Edge):** MV3, a service-worker background.
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
   provider and its policy; `inert` stops the script as in step 2.
4. It asks for the adapter. With one, the engine runs it; with `{adapter: null, reason}`, a full-screen notice shows
   the reason and only Home and Back work; a custom site runs the generic engine with no adapter.

**The overlay layer.** One element per top frame with a **closed** shadow root and adopted stylesheets, so page CSS
cannot reach it. It draws the focus ring, toasts, the overlay menu, the on-screen keyboard and notices, at StreamFlex's
sizes and colors (the theme's colors are passed in `welcome`). The pointer is the OS pointer; the layer draws a large
halo around it in pointer mode.

**Keys.** One handler on `window` in the capture phase owns every key in every mode (one owner per key per mode):

| Key | Browse | Player | Keyboard | Pointer |
|---|---|---|---|---|
| Arrows | move focus | the adapter's player map | move on the keyboard | move the pointer |
| Enter (OK) | activate | the adapter's player map | type the key | click |
| Backspace, Esc (Back) | close overlay UI, else go back a route | leave player mode | close the keyboard | leave pointer mode |
| Menu (Apps key, pad Start) | overlay menu | overlay menu | overlay menu | overlay menu |

**Back at the start route** shows a toast, *Press Back again to return to StreamFlex*; a second Back while the toast
shows sends `home`.

**The overlay menu:** *Home*, *Pointer mode*, *Reload*, *Back to <provider>'s start*.

### The engine

- **Browse.**
  - **Candidates:** visible elements that are focusable or clickable (`a[href]`, `button`, `[tabindex]`, ARIA roles
    `button`, `link`, `menuitem`, `tab`, `option`, `gridcell`), plus the adapter's `focus.include`, minus its
    `focus.exclude`. Open shadow roots are walked; closed ones through `openOrClosedShadowRoot` (Chromium's
    `chrome.dom.openOrClosedShadowRoot`, Firefox's `element.openOrClosedShadowRoot` property, file 27 §7).
  - **Moving:** the nearest candidate in the pressed direction, scored by the WICG spatial-navigation distance (the
    spec's algorithm, implemented here; the polyfill is archived). With an adapter `focus.row`, Up and Down keep the
    column, as a TV row does.
  - **Showing:** the focused element is scrolled to the center (`scrollIntoView({block: 'center'})`) and ringed.
  - **Change:** a `MutationObserver` re-collects candidates when the page adds content; the focus survives if its
    element does.
  - **Routes:** on every route change (history API or the URL changing) the adapter's `initialFocus` for that route is
    applied, in the first animation frame in which a selector matches. Without one, the first candidate in reading
    order gets focus. This is the lesson from the TV builds: a SPA route with no initial focus makes the first press
    do nothing.
- **Player.** Entered when the route is an adapter's player route, or a `<video>` larger than half the viewport is
  playing. Keys follow the adapter's `player.keys`; without an adapter: OK and Space play or pause, Left and Right seek
  with the arrow keys, Up and Down are volume. Player keys are sent as **trusted keys** through the launcher, since
  sites read `isTrusted`. `disablePictureInPicture` is set on the video.
- **Keyboard.** Opens when a text field or `contenteditable` gets focus. Its layouts come from 3c's on-screen
  keyboard (`osk.c`'s layout tables, exported to JSON at build time, so both keyboards stay one design: QWERTY, the ABC
  grid, the *àé* page). Typing uses the field's native value setter plus an `InputEvent`, the recipe React accepts; an
  adapter's `typing.trusted` switches to trusted keys from the launcher. A real keyboard types as usual.
- **Pointer.** The fallback when an adapter is broken or a site is unknown: arrows ask the launcher to move the real
  pointer (accelerating on hold, in wall time), OK is a real click. Chosen from the overlay menu; Back leaves it.

### Rules the engine enforces

- **One press, one action** (README D3). Nothing acts on a timer: no auto-skip, no auto-next.
- **The ad-skip ban** (README D2): with the catalog's `adskip=banned`, elements matching the adapter's
  `adSkipControls` are never candidates and never clicked, in any mode. In pointer mode the extension reports each
  click's page coordinates first, and the launcher refuses a click inside one of those elements' boxes.
- **Trust is never faked:** no `dispatchEvent` of keyboard or mouse events for anything a site checks; such actions go
  to the launcher. The extension never touches EME, the media pipeline, or the site's network traffic.

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
               "controls": { "skipIntro": [".skipIntro"] } },
  "adSkipControls": [],
  "typing":  { "trusted": false },
  "browsers": { "firefox": { "focus": { "include": [".card", ".listItem", ".itemAction"] } } }
}
```

(The selectors above illustrate the shape; R2 writes the real ones.)

- **Actions** are exactly four: `focus` (a selector list), `click` (a selector list), `sendKey` (a key name from a
  fixed list), `scroll` (a direction). A key maps to **one** action. There are no sequences, conditions, expressions,
  variables or regular expressions.
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
| `{"type":"hello","token":…,"browser":…,"extVersion":…}` | `{"type":"welcome","provider":…,"policy":{"adskip":…},"theme":{…},"engineMin":…}`, or `{"type":"inert"}` when the token is unknown or dead |
| `{"type":"adapter"}` | `{"type":"adapter","adapter":{…}}` or `{"type":"adapter","adapter":null,"reason":…}` |
| `{"type":"home"}` | closes the browser (**The launch path**, step 6) |
| `{"type":"send_keys","keys":[…]}` | `{"type":"ok"}` or `{"type":"error","error":…}` after injecting |
| `{"type":"pointer","op":"move","dx":…,"dy":…}` or `{"type":"pointer","op":"click","x":…,"y":…}` | same |
| `{"type":"log","level":…,"line":…}` | none; written to StreamFlex's log as `Overlay: <provider>: <line>`, at most 20 lines a second |

| Pushed by the launcher | Meaning |
|---|---|
| `{"type":"adapter_updated"}` | ask for the adapter again |

Every message except `hello` needs a connection that has received `welcome`. `send_keys` takes only key names from the
same fixed list adapters use; `pointer` moves are clamped to the browser's display.

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
  XPI copy rule, the fragment token); catalog resolution and the plays-here rules; discovery against fake trees; the
  cache with a fixed clock (signature, hash, the adapter-bytes cut, replay refusal, a rollback to a lower version, expiry, last good copy, `401` and
  `403`); channel framing and limits; the token table; the host's relay against a fake launcher; QR output against
  known vectors.
- **Extension (vitest):** candidate collection and direction scoring on fixed layouts; route and initial-focus
  handling; mode switching and key ownership; the ad-skip ban; schema validation, including that every repo adapter
  passes and each broken fixture fails. **Playwright** runs each built-in adapter against saved page snapshots in
  Chromium with the extension loaded; Firefox runs through `web-ext` and WebDriver BiDi.
- **Headless harness, a new lane:** real `google-chrome` and a full Firefox on Xvfb in the Debian image, a Jellyfin
  container with fixture media, the extension installed by the policy file (Chrome) and by the profile sideload
  (Firefox), the stub adapter source. Keys go in through XTEST; checks read the `Overlay:` lines (launch, `hello`,
  route, focus moves, player mode, Home, exit). Runs in CI like the other shards.
- **qa-harness guests:** the Windows path end to end (setup without elevation, Edge, Chrome and Firefox against
  Jellyfin, Plex and Emby containers), and the Linux desktops.
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
| G4 [5] | Exit detection for all three browsers on both OSes, and Edge lingering with the hygiene policies | the launcher sees the exit within 2 s of the last window closing | fall back to watching the browser's windows |
| G5 [7] | `SendInput` and uinput keys arrive `isTrusted` and grant user activation (fullscreen, media start); uinput keys reach the focused browser on GNOME and KDE Wayland | yes everywhere | Chromium: CDP `Input.*` for trusted actions; Firefox: back to the user |
| G6 [8] | After the browser closes the launcher is in front and takes input (Windows 11, GNOME Wayland, KDE, X11), 20 of 20 | yes | `SDL_HINT_FORCE_RAISEWINDOW` and a raise retry; on GNOME Wayland the launcher keeps its window fullscreen behind the browser so closing reveals it |
| G7 [6, partial] | The key codes Home, Back, Menu and the media keys produce through each path the launcher hooks | the hook sees them | rebind the defaults |
| G8 [14] | The gamepad reaches SDL while the browser has focus, per Windows backend (XInput, RawInput, WGI) with an emulated Xbox pad, and on Linux evdev | at least one Windows backend, and Linux | the bridge pins the working backend; with none, gamepads are documented as launcher-only on that OS |
| G9 [20] | Windows Firefox: native messaging from a kiosk profile, `-wait-for-browser` inside the job | round trip works; exit seen | adjust the Firefox glue |
| G10 [26] | Linux Chrome and Edge: policy from `/etc/opt/*/policies/managed/`, unlisted items force-install by id, the last-file-wins conflict, host manifests from `/etc/opt/*/native-messaging-hosts/` | all pass for native packages | Edge ignoring the policy, or an unlisted item refused: back to the user |
| G11 [24] | Linux discovery classifies Ubuntu default (snap), Ubuntu with Mozilla's deb, Debian ESR, Fedora, Arch, a `/opt` tarball and a Flatpak-only box correctly | all right | fix the probes |
| G12 [27] | Linux Edge plays Widevine content (a DRM test stream) without an unsupported flag | it plays | Linux Edge stays non-DRM (already decided) |
| G13 [28-31, 33, 35] | The Firefox profile sideload: enabled with no prompt, runs in `-kiosk`, a replaced XPI is picked up (with and without `startupScanScopes`), the everyday Firefox untouched, host permissions granted | all pass | Firefox takes the policy fallback with the inert switch (README D10) |
| G14 [32] | A fresh Firefox profile downloads its Widevine CDM, and how long it takes | it does | setup pre-warms each new profile |
| G15 [4] | A one-hour DRM test-stream soak with the extension active, per browser | no error | shrink the content script's footprint |
| G16 [3, 34] | Chrome and Edge `Extensions.loadUnpacked` over a debug pipe (the development loop only) | loads | development uses the store builds |

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
| **P0** | Phase 0 (above). No release. | every G and H item has a result and its branch is recorded |
| **R1 v0.7.0: launch and Home** | `providers.ini` with the built-in providers; discovery; recipes (Firefox without the XPI); the supervisor; the Home binding and hook; the gamepad bridge; `:stream`; *Streaming service ›* and *Custom site ›* in the add flow; *Settings › Streaming › Streaming browser*. No extension. | a remote starts Netflix in Edge at full quality, the site's own keyboard navigation works, and one Home press returns to the menu, every time |
| **R2 v0.8.0: the engine** | the extension (both flavors) and its store submissions; `cptk-overlay-host`; the channel and tokens; the cache, seeded from built-in adapters only (no network); *Set up streaming*; the Linux package files and udev rule; the engine on **Jellyfin, Plex and Emby** (Emby on its Desktop layout, since its TV layout is not free, file 29) | browse, open a title, play and come back with the D-pad alone on each, in all three browsers |
| **R3 v0.9.0: Netflix and YouTube** | the Netflix adapter and player mode; YouTube `/tv` with its user agent and the fallback; the overlay keyboard; pointer mode; the overlay menu | an evening of use with no keyboard or mouse, including one search and one sign-in per service |
| **R4 v0.10.0: the adapter source and Account** | the networked source, the manifest's catalog entries and reasons; *Account* and linking | a linked TV takes adapters from the source, and keeps working while the source is unreachable |
| **R5 v0.11.0: Linux browser install** | the any-of-three offer when no usable browser is found (Firefox preselected; Edge hidden off amd64), the `pkexec` helper per distro with the text fallback; final Linux Edge DRM gating from G12 | a default Ubuntu box gets a working full Firefox from one button; a box with a usable browser never sees the offer |

**Before R2's submissions:** the plan writes to Chrome Web Store support asking whether the adapter format is within
the remote-code policy, and waits for the answer before submitting.

## Out of scope

- Automatic actions of any kind (auto-skip, auto-next): README D3, revisited per service later.
- Detecting site changes and alerting users to adapter updates: the two follow-ups todo item 24 schedules after the
  initial services ship.
- HDR switching and refresh-rate matching: sub-project 4 and later.
- A phone keyboard, macOS and Safari, snap and Flatpak browsers, Chromium.
- Every provider beyond the built-in five: their adapters come through the adapter source.
