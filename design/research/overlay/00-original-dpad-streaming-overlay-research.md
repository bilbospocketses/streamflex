# D-pad Navigation Overlay for Streaming Sites: Viability Research

*Written 2026-09-28. Based on existing knowledge, not a live web search, so check the resolution caps yourself before committing to a platform.*

**Summary:** It's viable. The navigation overlay is a solved kind of problem, and some choices made early can get well past 720p.

---

## 1. Pick the browser for DRM first, then build the overlay

The resolution limit comes from the browser's DRM, not from the fact that it's a web app:

| Platform / browser | DRM | Typical max (Netflix, Disney+, Prime, Max) |
|---|---|---|
| Linux, Chrome/Firefox | Widevine L3 (software) | 720p. Some services are worse (Prime has often been SD on Linux). |
| Windows, Chrome/Firefox | Widevine L3 | 720p, sometimes 1080p depending on service |
| **Windows, Edge** | **PlayReady hardware DRM (SL3000)** | **1080p, often 4K** (needs a modern Intel/AMD/NVIDIA GPU, the HEVC extension and an HDCP 2.2 display for 4K) |

**Windows + Edge is the strongest target.** On Linux you're stuck at about 720p, and Prime may be lower. Build the overlay so it works on both, but market Windows as the high-quality path.

Two things follow from this:

- **Don't embed the sites in your own webview** (Electron, CEF, WebView2). Electron can only get Widevine L3 through castLabs' build, CEF has no Widevine by default, and WebView2 likely doesn't support hardware PlayReady (confirm that). Any of these puts you back at 720p or blocks playback.
- **Launch real Edge/Chrome from your app** in `--kiosk` or `--app=https://netflix.com` mode with a dedicated `--user-data-dir`, and inject your overlay into it.

## 2. How to inject the overlay

**Option A: browser extension (recommended).**
- A content script runs on each streaming domain with full DOM access, including open shadow roots.
- It listens for `keydown` (a D-pad remote almost certainly sends arrow keys, Enter and Esc/Back as HID keyboard input).
- It draws its own focus ring and calls `.click()` or `.focus()` on the chosen element.
- Load it unpacked or through policy (`ExtensionInstallForcelist` / `--load-extension`) so users never touch a store.
- Chrome and Edge are both Chromium, and Firefox takes the same WebExtension code with small changes.

**Option B: Chrome DevTools Protocol.** The native app starts the browser with `--remote-debugging-port` and injects the script with `Page.addScriptToEvaluateOnNewDocument`. The upside is that the app keeps control of everything, including talking back and forth with the launcher UI. Newer Chrome versions require a non-default `--user-data-dir` for this, which you'd want anyway. A and B can be combined: the extension does the navigation and CDP handles orchestration.

**Option C: OS-level transparent overlay using accessibility APIs** (UI Automation on Windows, AT-SPI on Linux). It works in theory, but it's slower, more fragile and has less context than the DOM. Keep it only as a fallback for native apps.

## 3. The D-pad navigation itself (the hard part)

This is more tractable than it looks because of accessibility compliance. Big streaming sites are under legal pressure to be keyboard-navigable, so most interactive elements are already `button`, `a` or have `tabindex`/`role` set, and **Tab + Enter already works on most of them.** The job is to turn that linear tab order into spatial up/down/left/right movement.

Build it in layers:

### 3.1 Generic spatial engine (covers maybe 70–80%)
- Collect visible focusables: `a[href], button, [tabindex], [role=button|link|tab|menuitem]`, and walk into shadow roots, which Disney+ uses heavily.
- On an arrow key, pick the nearest element in that direction using the geometric scoring from the W3C spatial navigation spec. WICG's `spatial-navigation-polyfill` is a good starting point, and Chromium has an experimental `--enable-spatial-navigation` flag worth trying as a baseline.
- Draw a large, TV-style focus ring as an overlay element and call `scrollIntoView({block:'center'})`.
- Use a `MutationObserver` to catch lazy-loaded rows and carousels.

### 3.2 Per-site adapters (the long tail)
Keep a small JSON/JS config per service:
- selectors for rows and tiles
- row-by-row behavior, so Up/Down moves between rows and remembers the column in each row, like a real TV UI
- how to page carousels, since Netflix rows virtualize and you may need to click their arrow buttons
- selectors for the player controls

Make these configs updatable remotely, because the sites will change their markup and break selectors every few months. That maintenance is the real long-term cost of this approach.

### 3.3 Player mode
When a `<video>` is playing, switch key mappings:
- Enter toggles play/pause, Left/Right seek, Up/Down show controls or audio/subtitle menus, Back exits.
- Most sites already have hotkeys (space, arrows, `f`), so send those rather than touching the `<video>` element directly.
- **Netflix errors out (M7375) if you set `video.currentTime` directly.** Seek through its keyboard shortcuts or its internal player API instead.
- Hover-dependent UI (Netflix's preview-on-hover, controls that only appear on mouse movement) can be handled by sending synthetic `mouseover`/`mousemove` events to the focused element.

### 3.4 Fallback cursor mode
Hold a button to switch to a virtual mouse pointer that the D-pad moves, like Android TV browsers do. This saves you whenever an adapter breaks.

### 3.5 Text input
Search needs an on-screen keyboard in the overlay that sets input values and dispatches `input` events so the site's framework picks them up. Logins are one-time: keep a persistent profile and let users sign in once with any keyboard or phone.

### 3.6 Freebies
- `youtube.com/tv` gives YouTube's full 10-foot D-pad interface with a spoofed TV user agent. There's no DRM on YouTube, so 4K works and no adapter is needed.
- Some services have "lean-back" or kids views that are easier to navigate.

## 4. Risks

- **Maintenance:** DOM changes break adapters. Mitigate with the generic engine, remote configs and the cursor fallback.
- **Terms of service:** client-side navigation helpers are generally fine, the same category as accessibility extensions. Never touch the DRM or the media stream.
- **Linux quality ceiling:** about 720p, which can't be fixed.
- **Focus capture:** full-screen video can trap focus in the player, so keep the Back key's global handling in the extension's own listener (registered in the capture phase).

## Verdict

It's viable, with medium effort for a solid generic engine and ongoing small effort per service. The biggest lever isn't the overlay but **using Edge on Windows for PlayReady**, which gets 1080p–4K instead of 720p.

**Suggested next step:** a prototype extension (a generic spatial engine plus a Netflix adapter) loaded into Edge in kiosk mode, to try it with the actual remote.
