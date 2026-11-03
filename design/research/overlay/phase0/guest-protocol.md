# Streaming overlay Phase 0: the guest protocol

For qa-harness-sf. Each item below is one measurement from the spec's Phase 0
(`design/specs/2026-10-27-streaming-overlay-design.md`). qa-harness automates them in its own repo against its own
guests; this file says what to set up, what to do, what to read, and what passes. Every result goes into
`results.md` beside this file, one row per item, with the evidence path.

Every Windows command below is typed in a Windows PowerShell window, from the kit folder, unless the step says
otherwise. Every Linux command is typed in a terminal, from `tools/overlay-phase0/` in the guest's checkout of the
repo (`<repo>` below), except the build, which runs from `<repo>` itself.

## Common setup

- **The probe** is **CPTK Overlay Probe** 0.0.1, published under its own store listings (Gecko id
  `overlay-probe@cptk.app`); the product's CPTK Overlay listing is not created until R1. Its native host is still
  `app.cptk.overlay`. Its signed Firefox package is `cptk-overlay-probe-0.0.1.xpi` (the `web-ext` artifact, renamed
  to that). G13 also uses `cptk-overlay-probe-0.0.2.xpi`, the same probe signed unlisted as version 0.0.2, which the
  user makes in the store steps (step 8.6) and leaves in the same handoff folder; copy it into the guest beside the
  kit.
- **The kit (Windows):** on the host, after building `tools/overlay-phase0/` (README there), run
  `powershell -ExecutionPolicy Bypass -File tools\overlay-phase0\make-kit.ps1 -Build build-phase0\Release -Xpi
  C:\Users\jscha\ClaudeScratch\streamflex-handoff\overlay-phase0\cptk-overlay-probe-0.0.1.xpi` from the repo root. It
  copies `probe_host.exe`, `probe_spawn.exe` with every DLL beside it (SDL3.dll), `setup\`, `pages\`, `extension\`
  (`src\` and `build.py`), `ids.json` and the signed XPI into one folder, `$env:USERPROFILE\cptk-kit` by default,
  and prints what it copied. Copy that folder into the guest. The kit carries the Visual C++ runtime DLLs
  (`vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`) beside the executables, so a clean guest needs no
  runtime install.
- **The kit (Linux):** build `tools/overlay-phase0/` in the guest, running the README's Linux build commands from
  `<repo>`, which puts the build in `<repo>/build-phase0`, and copy the signed XPI in. `probe_host` and `probe_spawn`
  are the absolute paths `<repo>/build-phase0/probe_host` and `<repo>/build-phase0/probe_spawn`.
- **`probe_spawn`** below means `.\probe_spawn.exe` in the Windows kit folder and `<repo>/build-phase0/probe_spawn`
  (its absolute path) on Linux.
- **The interactive session (Windows):** G5, G6 and G7 run in the guest's interactive desktop session, never over
  SSH. `SendInput` and raw input in session 0, where an SSH command runs, reach no desktop the browser is on.
- **Linux runs Firefox only** (decided together, 2026-11-03): every Linux step below uses the full Firefox, and G10 and
  G12 are retired.
- **Store listings:** G1, G5 and G15's Chrome and Edge runs (Windows) force-install the probe's published Chrome Web
  Store and Edge Add-ons items, so they wait until both listings are live. G13 needs only the two AMO-signed XPIs
  and `ids.json` (its host registration reads it), and no Chrome or Edge listing; no other item needs a listing.
- **The page server:** Windows `powershell -ExecutionPolicy Bypass -File pages\serve.ps1`; Linux
  `python3 -m http.server 8765 --bind 127.0.0.1 --directory pages`. The probe page is
  `http://127.0.0.1:8765/probe.html`.
- **A launch token** is any 32 lowercase hex characters, for example `0123456789abcdef0123456789abcdef`, appended
  as `#cptk=<token>`. It activates the probe only in the tab it arrives in, and only until that browser closes: a
  step that opens a second tab or restarts the browser opens a token URL again.
- **The probe box** sits in the page's bottom-left corner while the probe is active. It shows `webdriver: <value>`
  when the extension starts, then `host: connected (pid N)` once the native host answers (the success line, never a
  failure reason), or `host: <reason>` when the native host is missing, forbidden or has exited; a step that reads
  any of them takes a screenshot of it. Key reports name single-character keys only as `char`, except on the
  kit's own pages at `http://127.0.0.1:8765`, so any step that checks a letter key does it on the kit's probe page.
- **Evidence:** the probe host's `probe.log` (`$env:LOCALAPPDATA\cptk-probe\probe.log`, `~/.cptk-probe/probe.log`),
  `probe_spawn`'s JSON lines, and a screenshot where a line says so.
- **Two kinds of `start` in `probe.log`:**
  - The **host start line** is `{"event":"start",...,"argv":[...]}`, written by `probe_host` when a browser launches
    it; its `argv` carries the calling extension's origin (Chrome, Edge) or id (Firefox).
  - The **extension start report** is a `{"event":"msg",...}` line whose `body.event` is `start`; its `body.data`
    holds `webdriver`, `tokenFromUrl`, `allUrls`, `origin` and `version`.
  - The extension's other reports (`key`, `fullscreen`, `play`) are `msg` lines the same way, named by `body.event`.
- **Browsers:** Windows Edge `C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe`, Chrome
  `C:\Program Files\Google\Chrome\Application\chrome.exe`, Firefox `C:\Program Files\Mozilla Firefox\firefox.exe`;
  Linux `/opt/microsoft/msedge/msedge`, `/opt/google/chrome/chrome`, and the full Firefox's own path. Record each
  browser's version with every result.
- **Fresh profiles:** every run uses a new, empty profile directory unless the item says otherwise.
- **Chromium launch line:** `<exe> --user-data-dir=<profile> --app=<url> --start-fullscreen --no-first-run
  --disable-session-crashed-bubble`. **Firefox:** run
  `powershell -ExecutionPolicy Bypass -File setup\firefox-profile.ps1 -ProfileDir <profile> -Xpi <xpi>` (Windows) or
  `python3 setup/firefox_profile.py --profile <profile> --xpi <xpi>` (Linux) on the profile first, then
  `<exe> -kiosk <url> -profile <profile> -no-remote` (Windows adds `-wait-for-browser`). Started on its own in
  PowerShell, a launch line is `& '<exe>' <arguments>`; after `probe_spawn run` or `probe_spawn focus`, `<exe>` is
  the first argument.

## The items

### G1. Store force-install from HKCU policy (Windows 11 Home)

1. As the test user: `powershell -ExecutionPolicy Bypass -File setup\windows.ps1 -IdsFile ids.json -HostPath
   "$PWD\probe_host.exe" -ManifestDir "$env:LOCALAPPDATA\cptk-probe\hosts"`.
2. Start Edge with the Chromium launch line on `http://127.0.0.1:8765/probe.html#cptk=<token>`; wait 60 s. Record
   `first_load: probe box present yes/no` (screenshot). If no, reload that tab once (Ctrl+R; its address still holds
   the token, because no probe ran to remove it), wait 30 s, and record `after one reload: yes/no` (screenshot);
   if `first_load` is yes, record `after one reload: not needed`.
3. Repeat for Chrome.
4. First, as the test user, run step 1 again with `-Remove`. Then, from a **Run as administrator** PowerShell in the
   kit folder: `powershell -ExecutionPolicy Bypass -File setup\windows.ps1 -Scope HKLM -IdsFile ids.json -HostPath
   "$PWD\probe_host.exe" -ManifestDir "$env:ProgramData\cptk-probe\hosts"`, which writes the policy and the host
   keys under `HKLM:\Software`. Repeat steps 2-3 as the test user, then undo the HKLM setup with the same elevated
   command plus `-Remove`.

**Read:** `probe.log` has a host start line from each browser (its `argv` names the extension's origin) and an
extension start report; `edge://policy` / `chrome://policy` list the forcelist with status OK (screenshot). Where
either `probe.log` line is missing, screenshot the probe box: its `host: <reason>` line is the failure's reason (no
box at all means the extension did not install or did not run).
**Pass:** both browsers, HKCU: the policy installs the extension on the first launch, and the probe starts after
at most one reload, with no prompt; and `probe.log` holds, from each browser, a host start line and an extension
start report. The probe box alone never passes this row: a box showing `host: <reason>` is a failure. `first_load` is not a pass condition. It is data for the launcher's design: it
tells R1 whether the launcher must open the token URL only after the extension is installed. Step 4's HKLM run is
not a pass condition either, but record it per browser as `HKLM: pass/fail` by the same rule, beside
`HKCU: pass/fail`: the spec's fallback for this item turns on which of the two worked.

### G2. A standard user writes HKCU\Software\Policies

As a standard (non-admin) user: `reg add "HKCU\Software\Policies\Google\Chrome" /v CptkProbe /t REG_DWORD /d 1`,
then delete it. **Pass:** exit code 0. (If G1's HKCU step succeeded as a standard user, G2 passed with it.)

### G3. UAC by remote

As an administrator, start `powershell -Command "Start-Process powershell -Verb RunAs"` and answer the consent
prompt with keys only: Left, Right, Tab and Enter, sent through the guest's keyboard. Record which button has focus
first and which keys reach Yes. **Pass:** the prompt can be answered Yes without a mouse.

### G4. Exit detection (the three Windows browsers, and Firefox on Linux)

`probe_spawn run <browser launch line>`; after 20 s close the window with Alt+F4 through the guest's keyboard,
and note the time sent. Then repeat and send `close` on `probe_spawn`'s stdin instead. **Read:** the `exited` line's
`t` minus the Alt+F4 time, and `ms_since_close`. On Linux read the `group_empty` line in its place: the leader
reaped and no process left in its group, the signal the spec's launcher waits for. Linux `exited` comes only once
every descendant is gone, one that left the group included; record it, and `leader_exited`, as information. Edge
runs with G1's hygiene policies in place. **Pass:** each exit seen within 2000 ms (on Linux, `group_empty`). Record
Edge without the hygiene policies too, as information.

### G5. Trusted injection

With `http://127.0.0.1:8765/probe.html#cptk=<token>` open in each browser (setup as G1 on Windows; on Linux, Firefox
only, its profile prepared and the host registered as in G13), run `probe_spawn keys 3000 F` and, after reloading,
`probe_spawn keys 3000 P`. `keys` sends to
whatever window has focus once its delay ends, and the window the command was typed in has it, so within the 3 s
bring the browser to the front (click it, or Alt+Tab). It must be the
kit's probe page: anywhere else the `key` reports name F and P only as `char`. On Linux run it on GNOME Wayland and
KDE Wayland, with `sudo` (uinput needs root: Ubuntu 24.04 has no udev rule that opens it to the `input` group).
**Read:** the extension's `key` reports (`trusted`, `activation`) and its `fullscreen` and `play` reports in
`probe.log`. "`fullscreen ok`" means a `msg` line whose `body.event` is `fullscreen` with `body.data.ok: true`;
"`play ok`" the same with `body.event` `play`. **Pass:** `trusted: true`, `fullscreen ok`, `play ok`, on every
browser and desktop. In Firefox the prepared profile sets `media.autoplay.default` to 0 (allow), so `play ok` there
proves no user activation: Firefox passes on `trusted: true`, `activation: true` and `fullscreen ok`, and its `play`
result is recorded as information.

### G6. Focus after the browser closes

`probe_spawn focus <browser launch line on probe.html>`, with Edge run with G1's hygiene policies in place, as in
G4 (without them Edge's process can outlive its window, and `focus` never prints `exited`). After 15 s send Alt+F4
through the guest's keyboard, then read `probe_spawn`'s stdout and press Enter through the guest's keyboard only
once it has printed `exited`: `focus` drops every key that reaches it before then. Twenty times per OS and desktop
(Windows 11, GNOME Wayland, KDE, an X11 session).

**Read:** per run, `focus_after_exit`'s `raised`, `lost`, `back_before_raise`, `gained` and `ms`, and
`key_after_exit.ms`; record all six for every run, because which path brought the launcher's focus back is what R1
needs. `back_before_raise: true` means the desktop gave the launcher focus back as the browser closed, before the
probe raised its window; no later focus gain comes then, so `gained` is false.

**Pass:** a run passes on `key_after_exit.ms` 0 or more and either `gained: true` or `back_before_raise: true`. A run
with `lost: false` (the browser never took focus from the launcher) is invalid, not a pass: record it, then run once
more in its place. The item passes on 20 of 20 valid runs.

### G7. Key codes through the launcher's paths

`probe_spawn keylog 60` while pressing, through the guest's keyboard and a USB-forwarded or emulated consumer-control
device where the guest has one: Home (Browser Home), Back (Browser Back), Menu (Apps), Play/Pause, the arrows,
Enter, Backspace. On Linux run it with `sudo` or as a user in the `input` group (evdev needs it). **Read:** the
`raw_key` / `raw_hid` lines. **Pass:** every key appears. (The user's real remote is run sheet H2.)

### G8. Gamepads in the background

Windows guest with an emulated Xbox pad (ViGEmBus); Linux guest with a uinput gamepad. Start
`probe_spawn pad 60 <backend>`, then give the browser focus (probe page open), then press A, B and the D-pad.
Backends: `default`, `xinput`, `rawinput`, `wgi` (Windows); `default` (Linux). **Read:** `button` lines with
`focused: false`. **Pass:** at least one Windows backend, and Linux, report buttons while unfocused. Record every
backend's result.

### G9. Windows Firefox native messaging and exit

Profile prepared with `firefox-profile.ps1`; `windows.ps1` run for the host keys. `probe_spawn run <firefox launch
line on probe.html#cptk=<token>>`; press F once; then Alt+F4. **Read:** a host start line from Firefox in
`probe.log`; `exited` seen. With no host start line, screenshot the probe box before Alt+F4: its `host: <reason>`
line is the failure's reason. **Pass:** both.

### G10. Retired 2026-11-03

Linux supports Firefox only (decided together, 2026-11-03), so Linux Chrome and Edge policy is not measured and this
item is not run. Its id is not reused. Record it in `results.md` as `retired`.

### G11. Linux browser discovery

On each of: Ubuntu default (the snap Firefox), Ubuntu with Mozilla's deb (pinned as Mozilla documents), Debian with
`firefox-esr`, Fedora, Arch, a `/opt/firefox` tarball, and a Flatpak-only box, record for each candidate path in the
spec's **Browser discovery** list whether it exists, is an ELF, has `application.ini` or `omni.ja` beside it, and
where `realpath $(command -v firefox)` points. **Pass:** the spec's rules classify every box correctly (full
Firefox found only where one is installed natively).

### G12. Retired 2026-11-03

Linux supports Firefox only (decided together, 2026-11-03), so Linux Edge is not a target and this item, Linux Edge
and Widevine (the research's S1), is not run. Its id is not reused. Record it in `results.md` as `retired`.

### G13. The Firefox profile sideload

Windows and Linux, current release Firefox, with an everyday Firefox installed. Register the native host first, as
in G9: on Windows `windows.ps1` as in G1 step 1, on Linux `sudo python3 setup/linux_setup.py --ids ids.json --host
<abs probe_host>`. Without it nothing reaches `probe.log`.
1. Prepare a fresh profile; start Firefox `-kiosk` on `http://127.0.0.1:8765/probe.html#cptk=<token>`. **Read:**
   an extension start report with `allUrls: true` in `probe.log`.
2. Close it, copy in `cptk-overlay-probe-0.0.2.xpi` (the signed 0.0.2 from the handoff folder, *Common setup*) with
   the profile script, and start again on the token URL (the restart needs it). **Read:** the extension start
   report's `version` is 0.0.2.
3. Repeat 2 with `--no-scan-scopes` (`-NoScanScopes`), on a profile that already ran 0.0.1. In that mode the profile
   script writes `extensions.startupScanScopes` as 0, which overrides the 1 that 0.0.1's run left in `prefs.js`.
4. Open the everyday Firefox: `about:addons` and `about:policies` (screenshots).

**Pass:** 1 runs with no prompt; 2 shows 0.0.2; the everyday Firefox has no CPTK Overlay Probe add-on and no CPTK policy. Step 3's
result says whether `startupScanScopes` is needed.

### G14. Widevine in a fresh Firefox profile

A fresh prepared profile on the Shaka demo's Widevine stream; time from launch to playback, and whether
`gmp-widevinecdm` appeared in the profile. **Pass:** it plays; record the time.

### G15. A one-hour soak

Each browser, prepared as above, on the Shaka demo's Widevine stream for 60 minutes with the probe active: start it
on `https://shaka-player-demo.appspot.com/#cptk=<token>` and choose the asset in that same tab, so the probe box
shows. Every 5 minutes send Space twice, 2 s apart, with `probe_spawn keys 3000 Space; probe_spawn keys 2000 Space`
on one line, bringing the browser to the front (click the video, or Alt+Tab) within the first 3 s, as in G5. The
pair keeps the probe's key path busy for the hour (each press is a `key` report in `probe.log`) and leaves playback
as it was: the first Space pauses, the second resumes. After each pair, record whether the video is playing on the
same asset; a page that reloaded or left the asset counts as a reload. **Read:** the probe box
at the start and at the end of the hour (screenshots), and the twelve after-pair records. **Pass:** the probe box is
present in both screenshots, no playback error, no reload.

### G16. Chrome and Edge `Extensions.loadUnpacked` (the development loop, Windows)

Build the unpacked extension first: `python extension/build.py` in the kit (Python 3), or build it on the host and
copy `extension/dist/chromium` in. Start each browser with `--user-data-dir=<fresh> --remote-debugging-pipe
--enable-unsafe-extension-debugging` from a small CDP client, send `Extensions.loadUnpacked` with the absolute path
of `extension/dist/chromium`, then open `http://127.0.0.1:8765/probe.html#cptk=0123456789abcdef0123456789abcdef`.
**Read:** the extension list (screenshot), any banner, and the probe box in the page's bottom-left corner
(screenshot): its `webdriver: <value>` line is this item's `navigator.webdriver` result. Nothing comes from
`probe.log`: the unpacked build has its own extension id, which is not in the host manifests' `allowed_origins`, so
the browser refuses the native port and the box's `host: <reason>` line is expected here, not a failure. **Pass:**
it loads in both browsers and the probe box appears. (Development only.)

### G17. Picture-in-picture

**Kit:** steps 1-3 need only the browsers and the Shaka demo. Step 4 needs a `probe_spawn pip` watch (report each
new top-level window of the launched browser's processes, and raise the main one), which the kit does not have yet;
until a kit task adds it, record step 4 as not run and the row as partial.

Chrome, Edge and Firefox on Windows, and Firefox on Linux (GNOME Wayland, KDE Wayland, an X11 session), fresh
profiles:
1. **The settings.** Apply the settings the spec's **Picture-in-picture** lists, by hand: Firefox's four
   `user_pref(..., false)` lines appended to the profile's `user.js` after the profile script (it keeps user lines),
   and, after the first close, the profile script run again before the restart, so the restart follows the
   launcher's rewrite of `user.js`; Edge on Windows with
   `reg add "HKCU\Software\Policies\Microsoft\Edge" /v PictureInPictureOverlayEnabled /t REG_DWORD /d 0`, from a shell
   that can write that key (G2 says whether a standard user's can); Chrome and Edge: in one throwaway profile per
   browser, set *Automatic picture-in-picture* to *Don't allow* (`chrome://settings/content`,
   `edge://settings/content`), close the browser, and note the key that changed in that profile's
   `Default/Preferences`; then write the same key into a fresh profile's `Default/Preferences` before its first
   launch, and again before each later launch, as the launcher does. Start each browser, read `about:config`,
   `edge://policy` and `edge://settings/content`, or `chrome://settings/content` (screenshots), close it, start it
   again, and read them again.
2. **A site's own request.** Open `https://shaka-player-demo.appspot.com/`, play any clear (non-DRM) asset, hover
   the video, and press the player's picture-in-picture control (in its overflow menu) with the mouse. Record whether
   a picture-in-picture window opened, which kind (a video window or a document window), whether the browser's own
   hover button showed, and any error on the page (screenshot).
3. **Automatic picture-in-picture (Chrome and Edge).** In each profile from step 1, with the asset playing, press
   Ctrl+T and record whether a picture-in-picture window opens, and whether the `Preferences` key still reads block
   after the browser closes. Repeat in a fresh profile with the key left unset, as the baseline.
4. **The launcher's watch.** `probe_spawn pip <browser launch line>`, then open picture-in-picture as in step 2.
   **Read:** the watch's line for the new window and its raise of the main window, with the time from the window's
   appearance.

**Pass:** every setting reads the same after the restart; where Firefox's settings are set, the site's request
opens no window and the page keeps playing; the block in Chrome's and Edge's `Preferences` stops step 3's window
where the baseline opened one, across launches (record either way: where the browser resets it, the fallbacks
carry it); the watch sees the window and raises the main window
within 2000 ms on Windows and X11. On Wayland record what the watch can see, as data.

### G18. Typing through the launcher

**Kit:** this item needs a `probe_spawn text` command (each character as trusted keystrokes: `SendInput` with
`KEYEVENTF_UNICODE` on Windows, uinput with the key and Shift or AltGr the session's layout gives on Linux) and a
probe page with a React-controlled text field that reports its value, its React state and each event's
`isTrusted`. The kit has neither yet; until a kit task adds both, G18 is not run.

Chrome, Edge and Firefox on Windows, and Firefox on Linux (GNOME Wayland, KDE Wayland, an X11 session), with the
session's keyboard layout set to US, then to a layout with AltGr (German):
1. Focus the page's text field and run `probe_spawn text 3000 <string>`, bringing the browser to the front within
   the 3 s, as in G5. The string holds lowercase and uppercase letters, digits, the keyboard's punctuation, and every
   character of the keyboard's *àé* page.
2. On Linux, record how `probe_spawn` learned the session's layout, and on Wayland whether it could at all.

**Read:** the page's report: the field's value, the React state, and `isTrusted` per event. **Pass:** every
character arrives in the field, every event is trusted, and the React state equals the value, on every browser,
OS and desktop. A character the layout cannot produce counts as passed when `probe_spawn` refuses it by name and
types nothing for it.

## Results format

`results.md`, one row per item: `| G<n> | pass / fail / partial | browsers and versions | evidence path | note |`.
G1 puts each browser's `first_load` and `after one reload` values in the note; G10 and G12 read `retired`. G1's note
also gives each
browser's `HKCU: pass/fail` and `HKLM: pass/fail`.
