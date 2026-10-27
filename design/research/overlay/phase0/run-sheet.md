# Streaming overlay Phase 0: your run sheet (about an hour)

Four checks only a real GPU, display chain and remote can answer. Use your HTPC, your TV and AVR as they are wired
for watching, and your real remote. Write each answer in the table at the end. You type every command line below
with a keyboard; the remote is for moving around the page and for playback.

Type every command in a Windows PowerShell window (Start, type `powershell`, Enter) unless a step says otherwise.

**Before you start:**
1. Have the Netflix, Disney+ and Prime Video logins ready.
2. Part B, H3 and H4 need CPTK Overlay live on the Chrome Web Store and Edge Add-ons; the streamflex tab tells you
   when both listings are. Part A and H2 do not.
3. Assemble the kit once, on the PC with the StreamFlex checkout and its `build-phase0` build (`<checkout>` below is
   that checkout's folder):
   `powershell -ExecutionPolicy Bypass -File "<checkout>\tools\overlay-phase0\make-kit.ps1" -Build "<checkout>\build-phase0\Release" -Xpi "C:\Users\jscha\ClaudeScratch\streamflex-handoff\overlay-phase0\cptk-overlay-0.0.1.xpi"`
   It copies everything this sheet uses into `$env:USERPROFILE\cptk-kit` (the kit folder) and lists the files. If
   that PC is not the HTPC, copy the folder to the same place on the HTPC.

The setup comes partway through H1, not here: it installs the probe into every Chrome and Edge profile, so the rows
before it must run without it.

## H1. Does anything we add lower the picture or sound? (about 35 minutes)

Netflix's stats overlay is **Ctrl+Shift+Alt+D** while a title plays. Use one 4K HDR title with Atmos for every row
(any Netflix original marked "Ultra HD", "Dolby Vision" or "HDR", and "Dolby Atmos"). Let it play 60 seconds before
reading, and read: **Playing bitrate (video)** resolution, the **DRM** line, whether it says **HDR** or **Dolby
Vision**, and the **audio** codec and channels.

**Part A: without the probe.** Run these rows first, in this order.

| Row | Browser | How to start it | Fill in |
|---|---|---|---|
| H1.1 | Edge | normally, your usual profile | baseline |
| H1.2 | Edge | `& "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe" "--user-data-dir=$env:LOCALAPPDATA\cptk-probe\p1" --no-first-run https://www.netflix.com` (sign in once) | |
| H1.3 | Edge | `& "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe" "--user-data-dir=$env:LOCALAPPDATA\cptk-probe\p1" "--app=https://www.netflix.com#cptk=0123456789abcdef0123456789abcdef" --start-fullscreen --no-first-run` | |
| H1.5 | Chrome | normally | baseline |
| H1.6a | Chrome | `& "$env:ProgramFiles\Google\Chrome\Application\chrome.exe" "--user-data-dir=$env:LOCALAPPDATA\cptk-probe\c1" "--app=https://www.netflix.com#cptk=0123456789abcdef0123456789abcdef" --start-fullscreen --no-first-run` (sign in once) | |
| H1.7 | Firefox | normally | baseline (1080p expected) |

**Setup** (once, after Part A):
1. Close every browser window.
2. Run the setup, as yourself:
   `powershell -ExecutionPolicy Bypass -File "$env:USERPROFILE\cptk-kit\setup\windows.ps1" -IdsFile "$env:USERPROFILE\cptk-kit\ids.json" -HostPath "$env:USERPROFILE\cptk-kit\probe_host.exe" -ManifestDir "$env:LOCALAPPDATA\cptk-probe\hosts"`
   It prints `setup: wrote (policy under HKCU:\Software, host keys under HKCU:\Software)`.

**Part B: with the probe.** Check that the probe box shows in the bottom-left corner on each row.

| Row | Browser | How to start it | Fill in |
|---|---|---|---|
| H1.4 | Edge | as H1.3 | |
| H1.6b | Chrome | as H1.6a | |
| H1.8 | Firefox | prepared profile: `powershell -ExecutionPolicy Bypass -File "$env:USERPROFILE\cptk-kit\setup\firefox-profile.ps1" -ProfileDir "$env:LOCALAPPDATA\cptk-probe\f1" -Xpi "$env:USERPROFILE\cptk-kit\cptk-overlay-0.0.1.xpi"`, then `& "$env:ProgramFiles\Mozilla Firefox\firefox.exe" -kiosk "https://www.netflix.com#cptk=0123456789abcdef0123456789abcdef" -profile "$env:LOCALAPPDATA\cptk-probe\f1" -no-remote -wait-for-browser` | |
| H1.9 | Edge | as H1.4, but with `"--app=https://www.disneyplus.com"`, then again with `"--app=https://www.primevideo.com"`, in place of the Netflix `--app=` argument. The `--app` window has no address bar, so start it on each site's address directly (sign in on first use; 1080p expected) | |

## H2. What does your remote send? (about 10 minutes)

1. Start the page server and leave its window open:
   `powershell -ExecutionPolicy Bypass -File "$env:USERPROFILE\cptk-kit\pages\serve.ps1"`
2. In a second PowerShell window, start `& "$env:USERPROFILE\cptk-kit\probe_spawn.exe" keylog 300`, and open
   `http://127.0.0.1:8765/keylog.html` in Edge.
3. Press every button once, then hold each of OK, Back and Home for two seconds.
4. Save both: the second PowerShell window's text and a screenshot of the page.

## H3. Atmos at the receiver (about 5 minutes)

In H1.4's setup, play the Atmos title and read the receiver's display. Note what it shows (Dolby Atmos, Dolby
Digital Plus 5.1, PCM 2.0).

## H4. The display chain and sleep (about 10 minutes)

1. In H1.4's setup, confirm 2160p and HDR in the stats overlay.
2. Put the PC to sleep for one minute with the title paused, wake it, and press play with the remote. Note whether it
   plays, errors or goes black.

## Cleanup: put your browsers back (about 5 minutes)

The setup left your everyday Chrome and Edge "Managed by your organization", with the probe installed and Edge's
Startup Boost and background mode turned off. This puts them back.

1. Close every browser window.
2. Run the setup again with `-Remove`:
   `powershell -ExecutionPolicy Bypass -File "$env:USERPROFILE\cptk-kit\setup\windows.ps1" -IdsFile "$env:USERPROFILE\cptk-kit\ids.json" -HostPath "$env:USERPROFILE\cptk-kit\probe_host.exe" -ManifestDir "$env:LOCALAPPDATA\cptk-probe\hosts" -Remove`
   It prints `setup: removed (policy under HKCU:\Software, host keys under HKCU:\Software)`.
3. Open Edge and Chrome normally. `edge://policy` and `chrome://policy` list nothing from CPTK, "Managed by your
   organization" is gone from the menu, and CPTK Overlay is gone from the extensions page. If any of it is still
   there, close the browser and open it once more, then note it in the table's Cleanup row.
4. Delete the test profiles and host files: `Remove-Item -LiteralPath "$env:LOCALAPPDATA\cptk-probe" -Recurse -Force`

## Your answers

| Row | Resolution | DRM line | HDR / DV | Audio | Notes |
|---|---|---|---|---|---|
| H1.1 | | | | | |
| H1.2 | | | | | |
| H1.3 | | | | | |
| H1.4 | | | | | |
| H1.5 | | | | | |
| H1.6a | | | | | |
| H1.6b | | | | | |
| H1.7 | | | | | |
| H1.8 | | | | | |
| H1.9 Disney+ | | | | | |
| H1.9 Prime | | | | | |
| H3 | | | | | |
| H4 | | | | | |
| Cleanup | | | | | |

Send the filled table, the H2 files and any screenshots back to the streamflex tab.
