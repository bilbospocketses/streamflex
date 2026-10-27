# Streaming overlay Phase 0: the guest protocol

For qa-harness-sf. Each item below is one measurement from the spec's Phase 0
(`design/specs/2026-10-27-streaming-overlay-design.md`). qa-harness automates them in its own repo against its own
guests; this file says what to set up, what to do, what to read, and what passes. Every result goes into
`results.md` beside this file, one row per item, with the evidence path.

Every Windows command below is typed in a Windows PowerShell window, from the kit folder, unless the step says
otherwise. Every Linux command is typed in a terminal, from `tools/overlay-phase0/`.

## Common setup

- **The kit (Windows):** on the host, after building `tools/overlay-phase0/` (README there), run
  `powershell -ExecutionPolicy Bypass -File tools\overlay-phase0\make-kit.ps1 -Build build-phase0\Release -Xpi
  C:\Users\jscha\ClaudeScratch\streamflex-handoff\overlay-phase0\cptk-overlay-0.0.1.xpi` from the repo root. It
  copies `probe_host.exe`, `probe_spawn.exe` with every DLL beside it (SDL3.dll), `setup\`, `pages\`, `extension\`
  (`src\` and `build.py`), `ids.json` and the signed XPI into one folder, `$env:USERPROFILE\cptk-kit` by default,
  and prints what it copied. Copy that folder into the guest.
- **The kit (Linux):** build `tools/overlay-phase0/` in the guest (README there) and copy the signed XPI in.
  `probe_host` and `probe_spawn` are the absolute paths of `build-phase0/probe_host` and `build-phase0/probe_spawn`.
- **`probe_spawn`** below means `.\probe_spawn.exe` in the Windows kit folder and the absolute path of
  `build-phase0/probe_spawn` on Linux.
- **Store listings:** G1, G5, G10 and G15's Chrome and Edge runs force-install the published Chrome Web Store and
  Edge Add-ons items, so they wait until both listings are live. G13 needs only the AMO-signed XPI and no Chrome or
  Edge listing; no other item needs a listing.
- **The page server:** Windows `powershell -ExecutionPolicy Bypass -File pages\serve.ps1`; Linux
  `python3 -m http.server 8765 --bind 127.0.0.1 --directory pages`. The probe page is
  `http://127.0.0.1:8765/probe.html`.
- **A launch token** is any 32 lowercase hex characters, for example `0123456789abcdef0123456789abcdef`, appended
  as `#cptk=<token>`. It activates the probe only in the tab it arrives in, and only until that browser closes: a
  step that opens a second tab or restarts the browser opens a token URL again.
- **The probe box** sits in the page's bottom-left corner while the probe is active. It shows `webdriver: <value>`
  when the extension starts, and `host: <reason>` when the native host is missing, forbidden or has exited; a step
  that reads either takes a screenshot of it. Key reports name single-character keys only as `char`, except on the
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
at most one reload, with no prompt. `first_load` is not a pass condition. It is data for the launcher's design: it
tells R1 whether the launcher must open the token URL only after the extension is installed.

### G2. A standard user writes HKCU\Software\Policies

As a standard (non-admin) user: `reg add "HKCU\Software\Policies\Google\Chrome" /v CptkProbe /t REG_DWORD /d 1`,
then delete it. **Pass:** exit code 0. (If G1's HKCU step succeeded as a standard user, G2 passed with it.)

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

With `http://127.0.0.1:8765/probe.html#cptk=<token>` open in each browser (setup as G1 on Windows, `linux_setup.py`
as in G10 on Linux), run `probe_spawn keys 3000 F` and, after reloading, `probe_spawn keys 3000 P`. It must be the
kit's probe page: anywhere else the `key` reports name F and P only as `char`. On Linux run it on GNOME Wayland and
KDE Wayland, with `sudo` or as a user in the `input` group (uinput needs it). **Read:** the extension's `key`
reports (`trusted`, `activation`) and its `fullscreen` and `play` reports in `probe.log`. **Pass:** `trusted: true`,
`fullscreen ok`, `play ok`, on every browser and desktop. In Firefox the prepared profile sets
`media.autoplay.default` to 0 (allow), so `play ok` there proves no user activation: Firefox passes on
`trusted: true`, `activation: true` and `fullscreen ok`, and its `play` result is recorded as information.

### G6. Focus after the browser closes

`probe_spawn focus <browser launch line on probe.html>`; after 15 s send Alt+F4, then 2 s later press Enter through
the guest's keyboard. Twenty times per OS and desktop (Windows 11, GNOME Wayland, KDE, an X11 session). **Read:**
`focus_after_exit.gained` and `key_after_exit.ms`. **Pass:** 20 of 20 with `gained: true` and a key seen.

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

### G10. Linux Chrome and Edge policy

`sudo python3 setup/linux_setup.py --ids ids.json --host <abs probe_host>`, then start each browser on
`http://127.0.0.1:8765/probe.html#cptk=<token>` and wait 60 s. For each, record `first_load: probe box present
yes/no` (screenshot). If no, reload that tab once (Ctrl+R; the token is still in its address), wait 30 s, and
record `after one reload: yes/no` (screenshot); if `first_load` is yes, record `after one reload: not needed`.
Then add a second managed file,
`/etc/opt/chrome/policies/managed/zz-other.json`, that sets `ExtensionInstallForcelist` to another id, run the same
`linux_setup.py` command again, and start Chrome again on the token URL. **Read:** `chrome://policy`,
`edge://policy` (screenshots), the host start lines and extension start reports in `probe.log`, and the re-run's
conflict line. Where a browser installed the extension but `probe.log` has no line from it, screenshot the probe
box: its `host: <reason>` line is the failure's reason. **Pass:** both browsers install the extension on the first
launch, and the probe starts and reports after at most one reload, with no prompt; with `zz-other.json` the
extension is gone from Chrome and the re-run names the file. `first_load` is not a pass condition. It is data for
the launcher's design: it tells R1 whether the launcher must open the token URL only after the extension is
installed.

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
1. Prepare a fresh profile; start Firefox `-kiosk` on `http://127.0.0.1:8765/probe.html#cptk=<token>`. **Read:**
   an extension start report with `allUrls: true` in `probe.log`.
2. Close it, rebuild the XPI as version 0.0.2 (change `VERSION` in the kit's `extension/build.py` locally, sign it
   the same way), copy it in with the profile script, and start again on the token URL (the restart needs it).
   **Read:** the extension start report's `version` is 0.0.2.
3. Repeat 2 with `--no-scan-scopes` (`-NoScanScopes`), on a profile that already ran 0.0.1.
4. Open the everyday Firefox: `about:addons` and `about:policies` (screenshots).

**Pass:** 1 runs with no prompt; 2 shows 0.0.2; the everyday Firefox has no CPTK add-on and no CPTK policy. Step 3's
result says whether `startupScanScopes` is needed.

### G14. Widevine in a fresh Firefox profile

A fresh prepared profile on the Shaka demo's Widevine stream; time from launch to playback, and whether
`gmp-widevinecdm` appeared in the profile. **Pass:** it plays; record the time.

### G15. A one-hour soak

Each browser, prepared as above, on the Shaka demo's Widevine stream for 60 minutes with the probe active: start it
on `https://shaka-player-demo.appspot.com/#cptk=<token>` and choose the asset in that same tab, so the probe box
shows. Space is sent through `probe_spawn keys` every 5 minutes. **Read:** the probe box at the start and at the
end of the hour (screenshots). **Pass:** the probe box is present in both screenshots, no playback error, no
reload.

### G16. Chrome and Edge `Extensions.loadUnpacked` (the development loop)

Build the unpacked extension first: `python extension/build.py` in the kit (Python 3), or build it on the host and
copy `extension/dist/chromium` in. Start each browser with `--user-data-dir=<fresh> --remote-debugging-pipe
--enable-unsafe-extension-debugging` from a small CDP client, send `Extensions.loadUnpacked` with the absolute path
of `extension/dist/chromium`, then open `http://127.0.0.1:8765/probe.html#cptk=0123456789abcdef0123456789abcdef`.
**Read:** the extension list (screenshot), any banner, and the probe box in the page's bottom-left corner
(screenshot): its `webdriver: <value>` line is this item's `navigator.webdriver` result. Nothing comes from
`probe.log`: the unpacked build has its own extension id, which is not in the host manifests' `allowed_origins`, so
the browser refuses the native port and the box's `host: <reason>` line is expected here, not a failure. **Pass:**
it loads in both browsers and the probe box appears. (Development only.)

## Results format

`results.md`, one row per item: `| G<n> | pass / fail / partial | browsers and versions | evidence path | note |`.
G1 and G10 put each browser's `first_load` and `after one reload` values in the note.
