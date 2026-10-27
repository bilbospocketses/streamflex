# Streaming overlay Phase 0: the guest protocol

For qa-harness-sf. Each item below is one measurement from the spec's Phase 0
(`design/specs/2026-10-27-streaming-overlay-design.md`). qa-harness automates them in its own repo against its own
guests; this file says what to set up, what to do, what to read, and what passes. Every result goes into
`results.md` beside this file, one row per item, with the evidence path.

## Common setup

- **The kit:** build `tools/overlay-phase0/` in the guest (README there) or copy the built `probe_host`,
  `probe_spawn` (with its SDL3 library), `pages/`, `setup/` and `ids.json` in. The signed XPI is in
  `C:/Users/jscha/ClaudeScratch/streamflex-handoff/overlay-phase0/cptk-overlay-0.0.1.xpi` on the host.
- **The page server:** Windows `powershell -File pages\serve.ps1`; Linux
  `python3 -m http.server 8765 --bind 127.0.0.1 --directory pages`. The probe page is
  `http://127.0.0.1:8765/probe.html`.
- **A launch token** is any 32 lowercase hex characters, for example `0123456789abcdef0123456789abcdef`, appended
  as `#cptk=<token>`.
- **Evidence:** the probe host's `probe.log` (`%LOCALAPPDATA%\cptk-probe\probe.log`, `~/.cptk-probe/probe.log`),
  `probe_spawn`'s JSON lines, and a screenshot where a line says so.
- **Browsers:** Windows Edge `C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe`, Chrome
  `C:\Program Files\Google\Chrome\Application\chrome.exe`, Firefox `C:\Program Files\Mozilla Firefox\firefox.exe`;
  Linux `/opt/microsoft/msedge/msedge`, `/opt/google/chrome/chrome`, and the full Firefox's own path. Record each
  browser's version with every result.
- **Fresh profiles:** every run uses a new, empty profile directory unless the item says otherwise.
- **Chromium launch line:** `<exe> --user-data-dir=<profile> --app=<url> --start-fullscreen --no-first-run
  --disable-session-crashed-bubble`. **Firefox:** run `setup/firefox-profile.ps1` (Windows) or
  `setup/firefox_profile.py` (Linux) on the profile first, then `<exe> -kiosk <url> -profile <profile> -no-remote`
  (Windows adds `-wait-for-browser`).

## The items

### G1. Store force-install from HKCU policy (Windows 11 Home)

1. As the standard user: `powershell -File setup\windows.ps1 -IdsFile ids.json -HostPath <abs probe_host.exe>
   -ManifestDir %LOCALAPPDATA%\cptk-probe\hosts`.
2. Start Edge with the Chromium launch line on `http://127.0.0.1:8765/probe.html#cptk=<token>`; wait 60 s.
3. Repeat for Chrome.
4. Repeat 1-3 with `-Scope HKLM` from an elevated shell, after `-Remove` of the HKCU setup.

**Read:** `probe.log` has a `start` line from each browser's origin; `edge://policy` / `chrome://policy` list the
forcelist with status OK (screenshot). **Pass:** both browsers, HKCU, first launch, no prompt.

### G2. A standard user writes HKCU\Software\Policies

As a standard (non-admin) user: `reg add "HKCU\Software\Policies\Google\Chrome" /v CptkProbe /t REG_DWORD /d 1`,
then delete it. **Pass:** exit code 0. (If G1's HKCU step succeeded, G2 passed with it.)

### G3. UAC by remote

As an administrator, start `powershell -Command "Start-Process powershell -Verb RunAs"` and answer the consent
prompt with keys only: Left, Right, Tab and Enter, sent through the guest's keyboard. Record which button has focus
first and which keys reach Yes. **Pass:** the prompt can be answered Yes without a mouse.

### G4. Exit detection (all three browsers, both OSes)

`probe_spawn run <browser launch line>`; after 20 s close the window with Alt+F4 through the guest's keyboard,
and note the time sent. Then repeat and send `close` on `probe_spawn`'s stdin instead. **Read:** the `exited` line's
`t` minus the Alt+F4 time, and `ms_since_close`. Edge runs with G1's hygiene policies in place. **Pass:** each exit
seen within 2000 ms. Record Edge without the hygiene policies too, as information.

### G5. Trusted injection

With the probe page open in each browser (setup as G1 on Windows, `linux_setup.py` on Linux), run
`probe_spawn keys 3000 F` and, after reloading, `probe_spawn keys 3000 P`. On Linux run it on GNOME Wayland and KDE
Wayland. **Read:** `probe.log` `key` lines (`trusted`, `activation`), `fullscreen` and `play` lines. **Pass:**
`trusted: true`, `fullscreen ok`, `play ok`, on every browser and desktop.

### G6. Focus after the browser closes

`probe_spawn focus <browser launch line on probe.html>`; after 15 s send Alt+F4, then 2 s later press Enter through
the guest's keyboard. Twenty times per OS and desktop (Windows 11, GNOME Wayland, KDE, an X11 session). **Read:**
`focus_after_exit.gained` and `key_after_exit.ms`. **Pass:** 20 of 20 with `gained: true` and a key seen.

### G7. Key codes through the launcher's paths

`probe_spawn keylog 60` while pressing, through the guest's keyboard and a USB-forwarded or emulated consumer-control
device where the guest has one: Home (Browser Home), Back (Browser Back), Menu (Apps), Play/Pause, the arrows,
Enter, Backspace. **Read:** the `raw_key` / `raw_hid` lines. **Pass:** every key appears. (The user's real remote
is run sheet H2.)

### G8. Gamepads in the background

Windows guest with an emulated Xbox pad (ViGEmBus); Linux guest with a uinput gamepad. Start
`probe_spawn pad 60 <backend>`, then give the browser focus (probe page open), then press A, B and the D-pad.
Backends: `default`, `xinput`, `rawinput`, `wgi` (Windows); `default` (Linux). **Read:** `button` lines with
`focused: false`. **Pass:** at least one Windows backend, and Linux, report buttons while unfocused. Record every
backend's result.

### G9. Windows Firefox native messaging and exit

Profile prepared with `firefox-profile.ps1`; `windows.ps1` run for the host keys. `probe_spawn run <firefox launch
line on probe.html>`; press F once; then Alt+F4. **Read:** `probe.log` `start` from Firefox; `exited` seen.
**Pass:** both.

### G10. Linux Chrome and Edge policy

`sudo python3 setup/linux_setup.py --ids ids.json --host <abs probe_host>`, then start each browser on the probe
page. Add a second managed file `zz-other.json` that sets `ExtensionInstallForcelist` to another id and start
again. **Read:** `chrome://policy`, `edge://policy` (screenshots), `probe.log` `start` lines, the script's conflict
line. **Pass:** both browsers install and report on the first launch; with `zz-other.json` the extension is gone
and the script names the file.

### G11. Linux browser discovery

On each of: Ubuntu default (the snap Firefox), Ubuntu with Mozilla's deb (pinned as Mozilla documents), Debian with
`firefox-esr`, Fedora, Arch, a `/opt/firefox` tarball, and a Flatpak-only box, record for each candidate path in the
spec's **Browser discovery** list whether it exists, is an ELF, has `application.ini` or `omni.ja` beside it, and
where `realpath $(command -v firefox)` points. **Pass:** the spec's rules classify every box correctly (full
Firefox found only where one is installed natively).

### G12. Linux Edge and Widevine

Stock Edge on Ubuntu and Fedora; open `edge://components` (screenshot: is a Widevine CDM listed?) and play the
Shaka Player demo's Widevine stream (`https://shaka-player-demo.appspot.com/`, asset "Angel One (multicodec,
multilingual, Widevine)"). **Pass:** it plays without `--enable-features=msWidevinePlatform`. Record the result
with the flag too, as information.

### G13. The Firefox profile sideload

Windows and Linux, current release Firefox, with an everyday Firefox installed:
1. Prepare a fresh profile; start Firefox `-kiosk` on the probe page. **Read:** `probe.log` `start` with
   `allUrls: true`.
2. Close it, rebuild the XPI as version 0.0.2 (change `VERSION` in `build.py` locally, sign it the same way), copy
   it in with the profile script, start again. **Read:** the `start` line's `version` is 0.0.2.
3. Repeat 2 with `--no-scan-scopes` (`-NoScanScopes`), on a profile that already ran 0.0.1.
4. Open the everyday Firefox: `about:addons` and `about:policies` (screenshots).

**Pass:** 1 runs with no prompt; 2 shows 0.0.2; the everyday Firefox has no CPTK add-on and no CPTK policy. Step 3's
result says whether `startupScanScopes` is needed.

### G14. Widevine in a fresh Firefox profile

A fresh prepared profile on the Shaka demo's Widevine stream; time from launch to playback, and whether
`gmp-widevinecdm` appeared in the profile. **Pass:** it plays; record the time.

### G15. A one-hour soak

Each browser, prepared as above, on the Shaka demo's Widevine stream for 60 minutes with the probe active, Space
sent through `probe_spawn keys` every 5 minutes. **Pass:** no playback error, no reload.

### G16. Chrome and Edge `Extensions.loadUnpacked` (the development loop)

Start each browser with `--user-data-dir=<fresh> --remote-debugging-pipe --enable-unsafe-extension-debugging` from a
small CDP client and send `Extensions.loadUnpacked` with `extension/dist/chromium`. **Read:** the extension list
(screenshot), any banner, `navigator.webdriver` in the `start` line. **Pass:** it loads. (Development only.)

## Results format

`results.md`, one row per item: `| G<n> | pass / fail / partial | browsers and versions | evidence path | note |`.
