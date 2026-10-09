# R4: Remote input, launcher hand-off, living-room polish, text entry / sign-in

*Researched 2026-09-28 (WebSearch/WebFetch only, nothing installed or run). Ratings: **VERIFIED** = primary/official source, or several independent recent sources. **REPORTED** = single, forum, vendor-blog or search-snippet source. **UNKNOWN** = not found, or only my inference (marked "inference"). Many search results carry no publication date; where a date is known it is given. Several REPORTED items come from search-result snippets I could not open in full; treat them as leads.*

## 0. Corrections to `00-original-...` found along the way

1. **Chrome on Windows can now do 4K on Netflix.** Netflix's Windows requirements page lists Edge and Chrome up to 4K, Firefox and Opera up to 1080p, and the Netflix app up to 4K. VERIFIED (Netflix Help, fetched 2026-09-28: https://help.netflix.com/en/node/23931). Secondary sources say Chrome got 4K in Chrome 117 on Windows 11, and that HDR and Atmos stay Edge/app-only: REPORTED (https://windowsforum.com/news/netflix-4k-on-windows-11-chrome-117-adds-4k-edge-app-for-hdr.445924/, https://www.ecoustics.com/news/netflix-4k-chrome-windows/). The original's "Windows Chrome = 720p" line is stale for Netflix. The other services were not checked here.
2. **Edge is still the only browser for 5.1 / Atmos on Windows.** Netflix: "5.1 surround sound is supported only on Windows 10 or later computers through the Edge browser or the Netflix app". VERIFIED (https://help.netflix.com/en/node/14163).
3. The original says the remote "almost certainly sends arrow keys, Enter and Esc/Back". That holds only for some remotes. Back and Home are often *consumer-control* usages, not keyboard keys (section 1).

---

## 1. Remotes: what they send and who intercepts it

### 1.1 What the remote types send

| Remote type | What reaches the OS | Rating / source |
|---|---|---|
| **MCE / eHome IR remote** (Windows eHome driver, or IR-to-USB receiver) | Arrows, OK = Enter, Back = **Backspace**, Play = Space (as commonly remapped). One guide lists the Windows button remapped to Ctrl+Esc, which suggests it does not send that by default. | REPORTED: https://www.howtogeek.com/247332/how-to-control-any-software-in-windows-with-your-mce-remote/ (undated, old); search snippet from Kodi/AVS forums |
| **FLIRC USB receiver** (learns any IR remote, emits a HID keyboard) | Whatever key you record: keyboard keys (Esc, Backspace, Delete...) and media keys. **A forum bug says Esc and Backspace could not be assigned independently on one firmware.** Recheck on current firmware. | REPORTED: https://forum.flirc.tv/index.php?%2Ftopic%2F14182-esc-backspace-cannot-be-assigned-independently%2F= (undated, firmware-specific) |
| **USB / 2.4 GHz "air mouse" remotes** | Usually a composite HID device: a mouse pointer, a keyboard (arrows, Enter, Esc) and a consumer-control interface (media/volume, often Back/Home). The exact usages are model-specific. | UNKNOWN (not sourced) |
| **Bluetooth remotes** (Google TV / onn style) | Consumer-control usages: **AC Back = 0x0224, AC Home = 0x0223**, plus D-pad and media usages. On Linux these become `KEY_BACK` / `KEY_HOMEPAGE`-style evdev keys. Windows's exact virtual-key mapping for 0x223/0x224 (VK_BROWSER_HOME / VK_BROWSER_BACK?) was **not confirmed**. | Usages: REPORTED via https://github.com/FloMaetschke/onn-remote-linux and the USB HUT (https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf, 2004 edition; not opened in full). Windows mapping: UNKNOWN |
| **HDMI-CEC** | Kernel: CEC remote-control messages are passed on as input keystrokes by default (`CEC_LOG_ADDRS_FL_ALLOW_RC_PASSTHRU`), through a `/sys/class/rc` input device. VERIFIED: https://docs.kernel.org/driver-api/media/cec-core.html. libcec (Pulse-Eight) runs on Windows, macOS and Linux, and Kodi/Plex use it directly: https://support.pulse-eight.com/support/solutions/articles/30000053002-what-is-the-pulse-eight-cec-adapter-and-libcec-. **Whether libcec on Windows can emulate a keyboard for arbitrary apps was NOT found: UNKNOWN.** Third-party projects do CEC-to-uinput on Linux: REPORTED (https://github.com/eliottness/cec-controller, https://github.com/dillbyrne/es-cec-input). Which CEC user-control codes a given TV sends (Select, Exit, Root Menu, Setup, Contents...) is TV-specific: UNKNOWN (kodi.wiki returned 403). |

**A PC's own GPU almost never gives you CEC.** NVIDIA staff have said desktop GPU HDMI ports do not wire the CEC pin: REPORTED (https://forums.developer.nvidia.com/t/hdmi-cec-support/31445, undated, old). AMD/Nouveau were getting CEC-over-AUX for DisplayPort/USB-C-to-HDMI adapters: REPORTED (https://www.phoronix.com/news/AMDGPU-Nouveau-CEC-Tunnel). Some Intel NUC-class boards have CEC: REPORTED (search snippet). Plan on a Pulse-Eight USB-CEC adapter, or a remote that does not need CEC.

**Long-press:** no source found for what any of these remotes send on a long press. Keyboard-style remotes will send auto-repeat keydown events, which the extension can count (`event.repeat`). Consumer-control keys usually repeat only for volume. UNKNOWN; test with the real remote.

### 1.2 What Windows or the browser eats before the page sees it

| Key | What happens | Rating / source |
|---|---|---|
| **Media keys** (Play/Pause, Next, Prev, Stop) | Chrome installs a global media-key hook (`GlobalMediaKeysListenerWin`) and routes the keys to the active media session (Global Media Controls). The `chrome://flags/#hardware-media-key-handling` flag turns this off. So these keys **work for streaming video with no overlay code**, and the page probably does not see a normal `keydown` for them. | Hook: VERIFIED (Chromium review, https://groups.google.com/a/chromium.org/g/feature-media-reviews/c/wlSdYV8mcbo/m/u-eS6rp3BgAJ). Flag: REPORTED (https://www.howtogeek.com/426284/how-to-stop-chrome-or-edge-from-taking-over-your-media-keys/, https://www.omgchrome.com/chrome-google-music-media-keys/). "Page does not see keydown": inference |
| **Browser_Back / Browser_Home** (`WM_APPCOMMAND` on Windows) | Windows delivers these as app commands, not key codes. A Firefox bug states the intended design is keydown first, then the command runs if the keydown is not consumed. **Chromium's behavior is UNKNOWN.** The open question is whether a page or extension can `preventDefault` a Browser_Back key. | REPORTED for Firefox: https://bugzilla.mozilla.org/show_bug.cgi?id=865561. Chromium: UNKNOWN |
| **Windows key** | "Keyboard shortcuts that involve the Windows key are reserved for use by the operating system." The Windows key does not reach the page. A low-level hook can see and swallow it. | VERIFIED (https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey) |
| **Backspace as "back"** | Chrome no longer goes back on Backspace by default; the search results are guides on how to re-enable it. Do not rely on Backspace navigating. The overlay must handle Backspace itself. | REPORTED (https://fossbytes.com/how-restore-chrome-backspace-button-navigation/, https://www.simplehelp.net/2018/02/13/how-to-use-the-backspace-key-as-a-back-navigation-button-in-chrome/; both undated) |
| **Extension `chrome.commands`** | Shortcuts must include Ctrl or Alt, except media keys (MediaPlayPause, MediaNextTrack, MediaPrevTrack, MediaStop), which cannot combine with modifiers. **Global** commands (work without browser focus) are restricted to suggested `Ctrl+Shift+[0..9]`. Arrows are allowed. Useful as a *fallback path* for a "Home" key, not a primary one. | VERIFIED (https://developer.chrome.com/docs/extensions/reference/api/commands) |

### 1.3 Recommended key map

The principle is that **the remote or receiver is the place to normalize**. Program the remote, FLIRC or CEC bridge to send plain keyboard keys, so nothing depends on OS or browser interception.

| Remote button | Emit as | Handled by | Why |
|---|---|---|---|
| D-pad | Arrow keys | Overlay (capture phase) | Universal |
| OK / Select | Enter | Overlay | Universal |
| **Back** | **Backspace**, with Esc as an accepted alias | Overlay (capture phase), ignoring editable targets | Avoids Browser_Back, whose interception is unknown, and Chrome no longer uses Backspace for history. Esc is worse as the primary because it also exits fullscreen and closes site modals. |
| **Back, held ~800 ms** (or a dedicated button) | Same key, detected by the repeat count | Overlay, which sends a "go Home" message to the launcher | Long-press = Home for remotes with no Home button |
| **Home** | Choose a key nothing else uses (for example a launcher-owned F-key or `Ctrl+Alt+Home`), or Browser_Home if the remote has it | **Launcher, via a global hook or hotkey (section 2.4)** | The browser needs no cooperation. Works even if the page is hung. |
| Menu / Context | Context Menu key (VK_APPS), or `i` | Overlay: opens the overlay menu (search, on-screen keyboard, cursor mode, refresh) | Rarely used by sites |
| Play/Pause, Next, Prev | Media keys as sent | Chrome/Edge media session | Free, per above |
| Seek | Left/Right while a `<video>` is playing (player mode), or Rewind/FF if the remote has them | Overlay sends the site's own hotkeys | Matches the original doc's player-mode plan |
| Color buttons / number keys | Do not map | n/a | Not needed for v1 |

---

## 2. Hand-off: launcher (SDL) and browser

### 2.1 Windows focus rules (VERIFIED)

Source: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow (updated 2025-10). A process may set the foreground window only if the caller is a desktop app, the foreground process has not called `LockSetForegroundWindow`, and no menus are active, **and** at least one of these holds:
- the foreground-lock timeout has expired;
- the calling process *is* the foreground process;
- the calling process **was started by the foreground process**;
- there is no foreground window;
- the calling process **received the last input event**;
- a debugger is attached to either process.

The docs add that it can still be denied even when these hold, and that otherwise Windows only flashes the taskbar button.

- **AllowSetForegroundWindow(pid)**: the *current* foreground process can grant the right to another PID. The grant is lost on the next user input not directed at that process (https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-allowsetforegroundwindow). **This does not help "launcher takes focus back from the browser"**, because the browser would have to grant it, and it will not. It does help launcher to browser (the launcher is foreground, so `AllowSetForegroundWindow(browser_pid)` is possible, though "started by the foreground process" already covers a fresh launch).
- **Alt-key trick:** "The system automatically enables calls to SetForegroundWindow if the user presses the ALT key". A synthetic Alt press by the launcher before `SetForegroundWindow` is the well-known workaround. VERIFIED that the doc says so (https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-locksetforegroundwindow); the reliability of *simulated* Alt on current Windows 11 is UNKNOWN. A commonly cited alternative is `AttachThreadInput` to the foreground thread: REPORTED (https://gist.github.com/Aetopia/1581b40f00cc0cadc93a0e8ccb65dc8c, PowerToys FancyZones PR https://github.com/microsoft/PowerToys/pull/14383; the FancyZones PR was actually in the result set, contents not opened).
- **SDL2 already ships this:** `SDL_HINT_FORCE_RAISEWINDOW = "1"` adds "an extra level of forcing" when calling `SDL_RaiseWindow` on Windows, because Windows makes programmatic foreground moves "nearly impossible". VERIFIED (https://wiki.libsdl.org/SDL2/SDL_HINT_FORCE_RAISEWINDOW). The SDL version that introduced it was not stated; check the version StreamFlex builds against.
- **The strongest lever is receiving input:** if the launcher's global hotkey/hook is what receives the Home key press, the "process received the last input event" clause is plausibly satisfied. That is an inference. UNKNOWN whether a `WM_HOTKEY` or a hook callback counts. **Prototype first.**

### 2.2 Detecting browser exit (Windows)

- **Single-instance behavior:** Chromium browsers allow one instance per user-data-dir. If one is running, a second launch forwards its arguments to it and exits at once, so `WaitForSingleObject` on the process you spawned returns immediately. REPORTED, consistent across a 2025-era Chromium bug and a tooling write-up (https://issues.chromium.org/issues/433194901, https://github.com/NousResearch/hermes-agent/issues/53822). Using a dedicated `--user-data-dir` gives a separate instance and makes the spawned PID the real browser main process. REPORTED (same sources); for a StreamFlex-owned profile this happens by construction.
- **Edge lingers by design.** Startup Boost and background mode keep `msedge.exe` alive after the last window closes. Policies `StartupBoostEnabled` and `BackgroundModeEnabled` turn them off. REPORTED (https://www.askvg.com/fix-microsoft-edge-msedge-exe-always-keeps-running-in-background-even-after-closing-the-browser/); the policy page exists at https://learn.microsoft.com/DeployEdge/microsoft-edge-browser-policies/startupboostenabled. Whether a private `--user-data-dir` instance is subject to Startup Boost was not verified.
- **Kiosk flag for Edge:** `--edge-kiosk-type=fullscreen` (REPORTED, Microsoft Q&A search snippets); Chrome uses `--kiosk`.
- **Robust wait: a Job Object.** Create the job with a completion port, spawn the browser suspended, assign it, resume, and wait for `JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO`. VERIFIED technique (https://devblogs.microsoft.com/oldnewthing/20130405-00/?p=4743, https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects). Process-per-site is irrelevant: all children are in the tree/job. Caveats, both **inference / UNKNOWN**: Chromium sandboxes its own children with nested jobs (fine on Win 8+); a `--user-data-dir` process that forwards to another instance escapes the job, which is why the unique profile matters.
- Watching the main PID as well as the job costs nothing and gives a cross-check.

### 2.3 Hand-off design options

| # | Design | Pros | Cons / unknowns |
|---|---|---|---|
| A | **Launcher stays resident, browser launched on top.** Launcher hides or lowers itself, runs a global hook, and on Home calls `SDL_RaiseWindow` (with `FORCE_RAISEWINDOW`), or minimizes the browser window with `ShowWindowAsync(SW_MINIMIZE)`. | Instant return, browser session and profile stay warm. | Foreground rights on return (2.1). Minimizing *another* process's window needs no foreground right (my knowledge; not sourced), and the next window in Z-order (the launcher) then takes activation, but this must be tested. Browser keeps playing audio unless the page is paused first. |
| B | **Home = close the browser** (extension asks the launcher to close, or launcher posts `WM_CLOSE` to the browser's top windows, or kills the job). Launcher waits on the job and regains focus because nothing else is left. | Simplest and most reliable focus story (the launcher is the only window). Also stops playback. | Cold start on each launch, probably a few seconds. Session restore must be on. Losing tab state is acceptable in a launcher model. |
| C | **Launcher exits while the browser runs; a tiny relauncher restarts it on browser exit.** | Frees RAM/GPU. | Two binaries; the same hotkey question remains. |
| D | **Extension calls the launcher** (Native Messaging host, or a localhost socket) for Home / Back-at-top-level / "close". | No global hook needed. Overlay knows "we are at the top level". | Chrome/Edge extension to native app plumbing (my knowledge, not researched here). Fails if the extension or page is wedged. Combine with a hook as the backstop. |

**Recommendation:** ship **B + D** first (Home closes; the extension asks), with the low-level-hook Home from 2.4 as the always-works backstop, and keep **A** as a later optimization once the focus prototype answers the questions in section 6.

### 2.4 Can a native global hotkey coexist with the browser?

- `RegisterHotKey`: system-wide, delivered as `WM_HOTKEY` to your thread; fails if the combination is already registered; Win-key combinations are "reserved for use by the operating system"; F12 must not be used. VERIFIED (https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey). It does not require the browser's cooperation. **Whether a key is swallowed from the browser is not stated by the doc: UNKNOWN. Chrome itself uses a keyboard *hook* for media keys, so ordering between a hook and a hotkey is also UNKNOWN.**
- `WH_KEYBOARD_LL`: can consume a key (return nonzero); the installing thread needs a message loop; **the callback must return within `LowLevelHooksTimeout` (max 1000 ms on Win10 1709+) or, on Windows 7 and later, the hook is silently removed with no notification**; Microsoft recommends a dedicated thread that hands work off immediately, and Raw Input where you only need to observe. VERIFIED (https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc). Raw Input can observe but not consume, so it suits "detect Home" but does not stop the browser also seeing the key.
- **Suggested split:** a `RegisterHotKey` (or hook) for Home, and Raw Input if you want long-press timing. Keep the hook callback to a few microseconds. Re-arm the hook on a timer if it might have been dropped.
- **Linux/Wayland:**
  - The GlobalShortcuts portal is v2. It is **bound to the app identity**, the user approves and configures triggers in a dialog, `preferred_trigger` is only a hint, and an app can attempt `BindShortcuts` **only once per session**. VERIFIED (https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.GlobalShortcuts.html).
  - GNOME's backend arrived in GNOME 48 (after Ubuntu 24.04's GNOME 46); wlroots' portal has no GlobalShortcuts implementation; KDE and Hyprland have one. REPORTED (https://github.com/aaddrick/claude-desktop-debian/blob/main/docs/learnings/wayland-global-shortcuts-portal.md, https://wiki.hypr.land/Hypr-Ecosystem/xdg-desktop-portal-hyprland/).
  - GNOME focus: a launcher can transfer focus only by creating an **xdg-activation token** from its focused window and passing it (`XDG_ACTIVATION_TOKEN` env var, D-Bus platform-data, or a portal launch). An already-running app that raises itself gets an "<App> is ready" notification instead of focus. VERIFIED (https://blogs.gnome.org/shell-dev/2024/09/20/understanding-gnome-shells-focus-stealing-prevention/, 2024-09-20). **Consequence: a background launcher on GNOME Wayland cannot pull itself back to the front. Design B (close the browser) is the only clean answer on GNOME Wayland; on X11 an EWMH activate works.**
  - Chrome auto-detects Wayland from v140 (OMG! Ubuntu, 2025-08): https://www.omgubuntu.co.uk/2025/08/chrome-140-wayland-auto-detection-linux. Force `--ozone-platform=wayland|x11` in the launch line if behavior needs to be predictable.

---

## 3. Gamepads in the browser

- The Gamepad API spec requires `getGamepads()` to return an empty list until a **gamepad user gesture** (button press or axis move) happens **while the page is focused**; recent spec work tightens this to a fully active document with user attention. VERIFIED (https://github.com/w3c/gamepad/issues/206, https://lists.w3.org/Archives/Public/public-webapps-github/2026Feb/0236.html, MDN https://developer.mozilla.org/en-US/docs/Web/API/Gamepad_API/Using_the_Gamepad_API).
- Chromium exposes gamepads only to the focused page, so **unfocused windows and background tabs get nothing**. REPORTED (https://issues.chromium.org/issues/40914323, https://issues.chromium.org/issues/392661398).
- Chrome does not fire `gamepadconnected` reliably; poll `getGamepads()` each animation frame. REPORTED (MDN, itch.io devlog).
- The Permissions Policy default for `gamepad` is `*`, so it works in cross-origin player iframes unless a site restricts it. VERIFIED (https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Permissions-Policy/gamepad).
- **Content-script behavior: UNKNOWN.** I found no source on whether a content script's `navigator.getGamepads()` behaves the same as the page's. Inference: it runs in the page's frame, so it should, but this is unconfirmed. **Test in a prototype.**
- **Verdict:** viable as a *secondary* input for the overlay, since the browser will be the focused window during use. But the polling loop lives inside the page and only starts after a first button press (so the first press is "lost", which is fine). It stops when focus leaves the browser, so it cannot serve as the Home path. **Better primary design: have the launcher (or the OS/Steam Input) translate gamepad buttons into the same keyboard events as the remote (section 1.3).** Then the overlay has a single input path, Gamepad API polling can be a later nicety, and the launcher (SDL) already reads gamepads natively.

---

## 4. Living-room polish

### 4.1 Mouse cursor
- Chromium has an old request to auto-hide the cursor on inactivity during fullscreen HTML5 video (bug 102508 / 40659014): https://bugs.chromium.org/p/chromium/issues/detail?id=102508. Its current status was not confirmed. REPORTED.
- Extensions that hide the cursor exist, but per one report they need the mouse to move once after page load: REPORTED (https://chromewebstore.google.com/detail/video-mouse-hider/nmoomabnbphlcgjghcagjfkildcmpigi).
- In kiosk setups on Linux, `unclutter -idle` is the standard trick (X11 only): REPORTED (https://forums.raspberrypi.com/viewtopic.php?t=52759). Windows-side workarounds found were XP-era AutoIt or a blank system cursor: REPORTED, old.
- **Design (inference):** the overlay injects `* { cursor: none !important }` while in D-pad mode and removes it on a real mouse move; the launcher moves the pointer to a screen corner so stray hover states do not fire (Netflix preview-on-hover is the main risk).

### 4.2 Screensaver and sleep
- Chrome, Edge and other players request a display wake lock while a video plays; the timers restart when it pauses. On Windows 11 only a display request is needed. Chromium skips it for muted off-screen video. REPORTED (https://slack.green/en/blog/does-playing-a-video-keep-your-computer-awake) and backed by the Chromium tracker (https://issues.chromium.org/issues/40170933, "Video Wake Locks for S3 Systems"). **Chrome and Edge: assume yes. Brave has open bugs where this fails on Windows 11** (https://github.com/brave/brave-browser/issues/59345, https://github.com/brave/brave-browser/issues/59377), so verify on the chosen browser.
- Gap: nothing keeps the screen awake while the user *browses* a paused catalog for a long time. **Design:** the launcher holds `SetThreadExecutionState(ES_DISPLAY_REQUIRED)` (or `SDL_DisableScreenSaver`) while the browser is alive, and releases it when the launcher is idle in its own menu. (My design; not sourced.)

### 4.3 Audio
- Windows: 5.1 and Atmos only through Edge or the Netflix app, needing the **Dolby Access** app and a Spatial Sound setting of "Dolby Atmos for home theater" or "for Headphones". VERIFIED (https://help.netflix.com/en/node/14163). "Chrome outputs stereo/PCM only" is REPORTED (https://www.avforums.com/threads/atmos-in-pc-browser-for-netflix-etc.2449164/, https://linustechtips.com/topic/1019317-can-netflix-dolby-51-sound-support-in-browser/; forum threads, undated). "Passthrough" as a bitstream to an AVR versus Windows decode and re-encode was **not resolved**: UNKNOWN. Atmos over HDMI needs "Dolby Atmos for home theater" in Windows.
- Linux: **UNKNOWN**. Nothing found on 5.1 from Chrome/Netflix on PipeWire. Widevine L3 caps video at 720p or lower: REPORTED (https://www.forasoft.com/learn/video-streaming/articles-streaming/widevine-l1-l2-l3, https://www.xda-developers.com/linux-finally-working-hdr-but-still-cant-use-it-most-streaming-services/, 2026).

### 4.4 HDR on Windows Edge
- Netflix requirement page: Windows 11 with latest updates, a listed GPU (Nvidia 1050+, Radeon RX 400+, Intel 7th gen or later, or Ryzen), a display with **HDR mode enabled**, the HEVC codec (possibly a paid Microsoft Store extension), 15 Mbps, a plan with Ultra HD/HDR. VERIFIED (https://help.netflix.com/en/node/23931).
- Windows: turn on **HDR video streaming** in Settings > System > Display > HDR; on SDR devices the video must be **full-screen**. VERIFIED (https://support.microsoft.com/en-us/windows/hardware/display-graphics/stream-hdr-video-on-windows). "300 nits or more" for the display is REPORTED (https://www.thewindowsclub.com/enable-hdr-playback-for-video-streaming-apps).
- 4K also needs HDCP 2.2 end to end: VERIFIED (Netflix page above). **A launcher warning ("HDCP/HDR not detected; you will get 1080p") would be useful UX**, but detecting it from the launcher is UNKNOWN.

### 4.5 Refresh-rate matching (23.976 fps)
- **No browser support:** Chrome composites at the display's rate, and streaming sites do not change the mode. REPORTED (https://issues.chromium.org/issues/41367944, https://www.techbloat.com/how-to-sync-refresh-rates-for-smooth-playback-in-windows.html).
- So only the launcher can do it. **Options (design, not sourced):** (a) a fixed mode such as 120 Hz, an integer multiple of 24 but *not* of 23.976 (so it is judder-free only for true 24p); (b) switch to 23.976 Hz or 24 Hz per title via `ChangeDisplaySettingsEx`, but the launcher cannot know the frame rate of a site's video unless the extension reads the `<video>` element (`requestVideoFrameCallback` metadata, `getVideoPlaybackQuality`, or the manifest), which is fragile; (c) offer a manual "match 24p" toggle in the overlay menu. Mode changes on HDR displays can flicker or drop HDR: UNKNOWN. **Recommend (c) or (a) for v1.**

### 4.6 Autoplay
- Chrome allows autoplay with sound if the user has interacted with the domain, if the **Media Engagement Index** is high (videos played over 7 s, unmuted, in an active tab, larger than 200x140 px), or the site is an installed PWA. Muted autoplay is always allowed. VERIFIED (https://developer.chrome.com/blog/autoplay).
- Overrides: `--autoplay-policy=no-user-gesture-required` (REPORTED as no longer honored by some builds: https://support.google.com/chrome/thread/207847413/chrome-doesn-t-respect-autoplay-policy-flag-anymore); **enterprise policies `AutoplayAllowed` and `AutoplayAllowlist`** exist for both Chrome and Edge and are the durable route: VERIFIED (https://chromeenterprise.google/intl/en_us/policies/autoplay-allowed/, https://learn.microsoft.com/en-us/deployedge/microsoft-edge-browser-policies/autoplayallowlist). A persistent, dedicated profile also accumulates MEI. Because every title starts from a D-pad Enter press (a user gesture), most flows work without any override; the risk is "next episode" and preview autoplay.

### 4.7 Picture-in-Picture
- Chrome 134+ can enter PiP **automatically** on tab switch for sites that register a Media Session `enterpictureinpicture` handler and pass MEI checks, with a permission prompt: VERIFIED (https://developer.chrome.com/blog/automatic-picture-in-picture-media-playback, https://developer.chrome.com/blog/automatic-picture-in-picture-initiated-by-the-browser). It can be blocked with `chrome://flags/#auto-picture-in-picture-for-video-playback` (REPORTED, https://www.technetexperts.com/chrome-mediasession-pip-fix/). A single-tab kiosk never triggers a tab switch. The **PiP hover button on the video** in Edge/Chrome, and any policy to remove it, was not researched: UNKNOWN. The overlay can also set `disablePictureInPicture` on `<video>` (MDN: https://developer.mozilla.org/en-US/docs/Web/API/HTMLVideoElement/disablePictureInPicture).

---

## 5. Text entry and sign-in

### 5.1 On-screen keyboard that works with React / Angular inputs
- Setting `input.value = 'x'` does not update React-controlled state. The working recipe: take the **native setter** from the prototype (`Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set`), call it on the element, then dispatch an `input` event with `bubbles: true` (an `InputEvent` with `inputType: 'insertText'` is preferred for React 16+), then `change`/`blur` if validation needs it. For `<textarea>` use `HTMLTextAreaElement.prototype`. VERIFIED by React's own tracker plus several independent write-ups (https://github.com/facebook/react/issues/10135, https://dev.to/flinthive/why-inputvalue-x-doesnt-fill-a-react-form-and-what-actually-works-1hl7, https://coryrylan.com/blog/trigger-input-updates-with-react-controlled-inputs). Angular listens to the same `input` event, so the same recipe applies. No Angular-specific source fetched: inference.
- Two alternatives, **not researched here (my knowledge, treat as UNKNOWN)**: `document.execCommand('insertText', false, s)` on the focused field, which fires real `beforeinput`/`input`; and, when launched with a DevTools port, CDP `Input.insertText`.
- Cheapest robust path for typing: use a **phone as the keyboard**. The launcher or extension serves a tiny LAN page, and text arrives via the recipe above. This is not researched: design idea only.

### 5.2 Sign-in flows on **desktop web**
| Service | Web sign-in options found | Rating / source |
|---|---|---|
| **Netflix** | Two documented on web: a **sign-in code** (sent by email or text, 15-minute expiry) or password. QR / 8-digit TV code are for **TV apps**, not the web site. Passkeys: sources conflict (one says Chrome 122+, others say "not shipped" as of March 2026). | Netflix: VERIFIED (https://help.netflix.com/en/node/311830241325668). Passkeys: UNKNOWN (https://www.androidheadlines.com/2026/06/whynopasskeys-website-shames-tech-giants-passkeys-security.html, https://securityboulevard.com/2026/05/streaming-service-authentication-2026-stopping-account-sharing-with-passkeys/) |
| **Disney+** | Web passkeys claimed broadly available by late 2025, TV cross-device still piloting. The TV flow at disneyplus.com/begin is for TV activation, not desktop sign-in. | REPORTED (one vendor blog, https://mojoauth.com/blog/streaming-service-authentication-account-sharing-passkeys) |
| **Prime Video / Max / Hulu / Apple TV+ / Paramount+ / Peacock** | Not found. | UNKNOWN |

**What this means:** none of the sites offers a "show a QR code on the desktop web to approve on your phone" flow for the web (those exist for TV apps). The pragmatic route is: sign in **once per service** with a keyboard, a phone-as-keyboard page, or the email/text sign-in code (still needs an email or phone number typed in), and keep a **persistent `--user-data-dir`** so cookies survive. Chromium's generic WebAuthn "use a phone" QR prompt (hybrid) works with any site that supports passkeys, but was not researched: UNKNOWN. Session lifetimes and re-authentication frequency were not researched: UNKNOWN. Expect a household re-login every few weeks to months, which is a guess.

---

## 6. Open questions for a prototype (in priority order)

1. **Return focus.** Launcher hides or lowers itself; the browser is fullscreen and focused. Does `SDL_RaiseWindow` with `SDL_HINT_FORCE_RAISEWINDOW=1` return foreground on Windows 11 (current build), when triggered by (a) a `RegisterHotKey`, (b) a `WH_KEYBOARD_LL` hook, (c) a message from the extension? Compare with a synthetic Alt press. Compare with minimizing the browser window instead.
2. **Does swallowing work?** When a hook or hotkey consumes the Home key, does the browser also see a `keydown`? Do a hook and Chrome's own media-key hook conflict?
3. **Browser_Back / Browser_Home.** With the actual remote(s): what does each button emit (use a raw HID / key logger page)? Can an extension content script `preventDefault` a Browser_Back keydown in Chrome and in Edge? Which usage does Windows map 0x0224 to?
4. **Which remote.** Test at least one air-mouse remote, one FLIRC setup, one Bluetooth Google-TV-style remote and (if wanted) CEC through a Pulse-Eight adapter: capture what a long press sends and how Esc/Backspace behave.
5. **Gamepad in a content script.** Does `navigator.getGamepads()` work from a content script in Edge/Chrome, after one button press, in a kiosk window, inside a cross-origin player iframe?
6. **Wait/exit.** With a unique `--user-data-dir`, is the spawned PID the browser main process on both Edge and Chrome? Does Edge keep processes alive (Startup Boost) for that profile? Does a Job Object hold, and does it report zero when the last window closes?
7. **Wake lock.** Does Edge (not just Chrome) hold the display awake during Netflix, Disney+ and Prime playback with the launcher idle? Does the launcher's `SetThreadExecutionState` cover browsing?
8. **Netflix quality on the target box.** With Edge on Windows 11 and the real TV: what does Netflix's own playback-info overlay report (resolution, HDR, audio codec), and does 5.1 reach the AVR with Dolby Access and Windows Spatial Sound configured?
9. **Refresh rate.** Does a 23.976 or 24 Hz mode set by the launcher persist while Edge is fullscreen, and does it survive HDR toggling?
10. **Cursor.** Does the injected `cursor: none` hold through fullscreen video, and does parking the pointer stop hover previews?
11. **Linux/GNOME Wayland.** Confirm that a background launcher cannot regain focus without closing the browser; confirm the GlobalShortcuts portal works on the target GNOME version; confirm whether any Wayland route exists for CEC remotes to reach the browser as key events.
12. **Sign-in.** For each target service, which desktop-web sign-in methods are actually offered today, and how long does a session last?

---

## 7. Source quality notes

- Best-quality sources: Microsoft Learn (2024-2025 revisions), Chrome for Developers, Netflix Help Center, the XDG portal docs, the GNOME Shell blog (2024-09-20), the Linux kernel docs, and Chromium/W3C trackers.
- Weak: forum and vendor-blog claims about remote key codes, Dolby passthrough, passkey status and cursor extensions. Several results were aggregator or SEO pages (for example the `windowsforum.com`, `techbloat.com` and Netflix-QR pages); I used them only as leads and labeled them REPORTED.
- **Not obtained (fetch blocked or empty):** kodi.wiki CEC page (403), Chromium issue pages (sign-in wall), the Microsoft keyboard-HID mapping page (404).
- During the SetForegroundWindow search, the prompt-injection scanner flagged the result set (a "restriction bypass" pattern). I found no instructions in the returned text; the trigger was almost certainly the "bypassing SetForegroundWindow restrictions" gist title. I treated the content as data only.
