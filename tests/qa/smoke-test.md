# StreamFlex smoke test

**Smoke target:** `v0.4.1`

## How to use this doc

- **One observable claim per row.** A row's steps say how to reach the claim on each desktop its tag covers; its expected cell is the claim itself, with the log line or frame that shows it where there is one.
- **The ☐ box** is for working through a pass by hand: write `x` for a pass, `F` for a fail and `-` where the row does not apply. The boxes are a worksheet, not a record.
- **The record of a release is its results file**, `results/vX.Y.Z.md`, in the format [`results/README.md`](results/README.md) describes.
- **A row with no verdict is never green.** An empty box or cell is not a pass.
- **A row's tag is part of its claim.** The claim holds on every desktop the tag covers, and the row says nothing about the others.

| Tag | Desktops |
|---|---|
| `[Win]` | Windows 11 |
| `[Ubuntu]` | Ubuntu GNOME |
| `[Kubuntu]` | Kubuntu |
| `[Fedora]` | Fedora Workstation |
| `[Fedora-KDE]` | Fedora KDE |
| `[GNOME]` | Ubuntu GNOME, Fedora Workstation |
| `[KDE]` | Kubuntu, Fedora KDE |
| `[Linux]` | Ubuntu GNOME, Kubuntu, Fedora Workstation, Fedora KDE |
| `[All]` | Windows 11, Ubuntu GNOME, Kubuntu, Fedora Workstation, Fedora KDE |

## Pre-flight

Setup, not tests: nothing here takes a verdict. Every row assumes its desktop was set up as below.

### Windows 11

- **Build.** The release zip, unpacked to `C:\StreamFlex`, which then holds `streamflex.exe`, `config.ini` and the `assets` folder. Whether the zip holds a DLL, and whether it starts with no Visual C++ runtime installed, are claims of module 1, not setup.
- **Config.** `C:\StreamFlex\config.ini`. StreamFlex reads `config.ini` from the folder it was started in first, then from the folder of `streamflex.exe`, so start it from `C:\StreamFlex`. A save writes that file in place and keeps the old one as `config.ini.bak`. Keep a copy of the zip's own `config.ini` when you unpack it; to reset, copy it back over `C:\StreamFlex\config.ini`.
- **Input.** Keys are sent with SendInput:
  - The arrow keys are extended keys: set `KEYEVENTF_EXTENDEDKEY`.
  - The Menu key is `VK_APPS` (0x5D), with its scan code 0x5D and `KEYEVENTF_EXTENDEDKEY`. Never `VK_MENU`: that is the Alt key.
  - SendInput does not auto-repeat, so a held key is sent as repeated key-downs.
  - SendInput cannot produce `SDLK_MENU` (`#40000076`): no Windows virtual key gives it. A row that needs `#40000076` is therefore never tagged to include Windows; it is `[Linux]` or narrower.

  The pad is the ViGEm virtual Xbox 360 pad, connected before StreamFlex starts. Hold a pad press for 150 ms: StreamFlex samples the pad once a frame, so a shorter press can be missed. Leave at least 500 ms between inputs.
- **Logs.** Start `C:\StreamFlex\streamflex.exe -d`. The log is `C:\StreamFlex\streamflex.log`, beside the executable. It is emptied at each start by hand, so copy it away before starting again; StreamFlex's own restart carries on the same log. `-d` writes the debug lines, among them `Video: SDL's windows driver, the <renderer> renderer`, which names the driver and the renderer. A key that arrives logs `Key <name> (#<hex>) detected`, and a pad press logs `Gamepad <Button> detected`, so the log tells an input the harness never delivered from a fail of StreamFlex.
- **Session.** Start StreamFlex in the interactive desktop session. The log shows `Gained keyboard focus` once it has the keyboard.

### Ubuntu GNOME

- **Build.** The release `.deb`, installed as root with `apt-get install -y ./streamflex_<version>_amd64.deb`, which puts StreamFlex in `/usr/bin/streamflex` and its packaged config in `/usr/share/streamflex/config.ini`.
- **Config.** StreamFlex reads `config.ini` from the folder it was started in, then from the folder of the executable, then `~/.config/streamflex/`, then the packaged `/usr/share/streamflex/`. The packaged copy cannot be written, so the first save writes the user's own `~/.config/streamflex/config.ini`, which StreamFlex reads from then on. To reset to the shipped config, delete `~/.config/streamflex` (`ubuntu-guest.sh reset-pass` does this), and make sure no `config.ini` sits in the folder StreamFlex starts in.
- **Input.** Keys are QEMU HMP `sendkey` names (`up`, `down`, `left`, `right`, `ret`, `backspace`, `esc`, `meta_l`, `f5`); `sendkey <key> 2000` holds a key for 2 s. The Menu key:
  - `sendkey compose` reaches SDL as `SDLK_APPLICATION` (`#40000065`), as the 3b Ubuntu run used it.
  - **`SDLK_MENU` (`#40000076`) comes from a uinput keyboard's `KEY_MENU`, never `sendkey menu`**, which did not reach the app in qa-harness's v0.3.0 run. A row that needs `#40000076` says so in its steps, so a hand pass cannot read a harness miss as an app Fail.

  The pad is `tests/qa/virtual-gamepad.py`, copied with the other guest helpers to `/root/sf-qa/`. Serve it as root before any StreamFlex run (`systemd-run --unit=sf-vgp /usr/bin/python3 /root/sf-qa/virtual-gamepad.py serve --keep-open 14400`), and drive it with `send` (`python3 /root/sf-qa/virtual-gamepad.py send "press ButtonStart"`, or `python3 /root/sf-qa/virtual-gamepad.py send "press ButtonStart --hold 2000"`). Its journal shows `created 'Microsoft X-Box 360 pad' (045e:028e) at /dev/input/eventN`, and `bash /root/sf-qa/ubuntu-guest.sh pad-node` shows `ID_INPUT_JOYSTICK=1` and an ACL entry `user:qa:rw-`; without that ACL, StreamFlex running as the desktop user cannot read the pad. The pad needs `uinput` loaded (`modprobe uinput`, which `ubuntu-guest.sh setup` does). Leave at least 500 ms between inputs.
- **Logs.** Start `streamflex -d`. The log is `~/.local/share/streamflex/streamflex.log`, emptied at each start by hand; StreamFlex's own restart carries on the same log. `-d` writes the debug lines, among them `Config file found: <path>` and `Video: SDL's <driver> driver, the <renderer> renderer`, which names the driver and the renderer. A key that arrives logs `Key <name> (#<hex>) detected`, and a pad press logs `Gamepad <Button> detected`; a key repeat is logged before StreamFlex drops it, so a held key shows as more than one such line.
- **Session.** GNOME on Wayland: the user's session `Type` is `wayland`. Launch the *StreamFlex Debug* app-menu entry, which `ubuntu-guest.sh setup` installs. It runs `~/sf-run.sh`, which runs `streamflex -d`, and the app menu's activation token gives the new window keyboard focus. The shipped *StreamFlex* entry runs plain `streamflex`, with no `-d`, so do not use it. Any other launch (for example `systemd-run --user`) must pass `-d`. After it, check the log for `Gained keyboard focus`: without that line, keys go to another window.

  The focus check, and every log line a row expects, need `-d`. Without it, `Key … detected`, `Gamepad … detected`, `Video:`, `Config file found` and `Gained keyboard focus` are never written, so an input that arrived looks as if it never did.

### Kubuntu

- **Build.** The release `.deb`, installed as on Ubuntu GNOME.
- **Config.** As on Ubuntu GNOME: the packaged `/usr/share/streamflex/config.ini` until the first save writes `~/.config/streamflex/config.ini`; delete `~/.config/streamflex` to reset.
- **Input.** As on Ubuntu GNOME: HMP `sendkey`, `sendkey <key> 2000` for a hold, `sendkey compose` for `SDLK_APPLICATION` (`#40000065`), a uinput keyboard's `KEY_MENU` (never `sendkey menu`) for `SDLK_MENU` (`#40000076`), and the pad served with `python3 /root/sf-qa/virtual-gamepad.py serve --keep-open 14400` and driven with `python3 /root/sf-qa/virtual-gamepad.py send "press <Button>"`.
- **Logs.** As on Ubuntu GNOME: `streamflex -d`, logging to `~/.local/share/streamflex/streamflex.log`, with the `Video:` line naming the driver and renderer.
- **Session.** Plasma. Record the session type and whether a compositor is running: the Transparent rows depend on it. Launch the *StreamFlex Debug* entry (`ubuntu-guest.sh setup` installs it; it runs `streamflex -d`) or another launch that passes `-d`, never the shipped *StreamFlex* entry. After each launch, check the log for `Gained keyboard focus`. That check, and every log line a row expects, need `-d`, as on Ubuntu GNOME.

### Fedora Workstation

- **Build.** A build from StreamFlex's own CI, named in the results header by its run id and sha256, until StreamFlex ships an `.rpm` (todo item 25). Never a build compiled inside the guest. No CI job uploads a Fedora build yet; that job is added under todo item 25 when the lane first needs it.
- **Config.** The same search as on Ubuntu GNOME: the folder StreamFlex starts in, the executable's folder, `~/.config/streamflex/`, then the packaged copy under the build's `share/streamflex/`.
  - When the build is installed at its own prefix, StreamFlex finds the packaged config at `<prefix>/share/streamflex/config.ini`, and the desktop user cannot write it there. Then the first save goes to `~/.config/streamflex/config.ini`, and deleting `~/.config/streamflex` resets to the shipped config.
  - When StreamFlex finds a `config.ini` in the folder it starts in or beside the executable, or finds the packaged copy writable, the save writes that file in place. Resetting then means restoring that file.
- **Input.** As on Ubuntu GNOME: HMP `sendkey`, `sendkey <key> 2000` for a hold, `sendkey compose` for `SDLK_APPLICATION` (`#40000065`), a uinput keyboard's `KEY_MENU` (never `sendkey menu`) for `SDLK_MENU` (`#40000076`), and the pad served with `python3 /root/sf-qa/virtual-gamepad.py serve --keep-open 14400` and driven with `python3 /root/sf-qa/virtual-gamepad.py send "press <Button>"`.
- **Logs.** As on Ubuntu GNOME: `streamflex -d`, logging to `~/.local/share/streamflex/streamflex.log`, with the `Video:` line naming the driver and renderer.
- **Session.** GNOME on Wayland. Launch with `-d`, never through the shipped *StreamFlex* entry, which runs plain `streamflex`. The *StreamFlex Debug* entry comes from `ubuntu-guest.sh setup`, which needs the `.deb` layout. Where setup cannot run, make a `-d` launch yourself (see Guest helpers). After each launch, check the log for `Gained keyboard focus`. That check, and every log line a row expects, need `-d`, as on Ubuntu GNOME.

### Fedora KDE

- **Build.** A build from StreamFlex's own CI, as on Fedora Workstation: named by run id and sha256, never compiled inside the guest, until an `.rpm` ships (todo item 25).
- **Config.** As on Fedora Workstation.
- **Input.** As on Ubuntu GNOME: HMP `sendkey`, `sendkey <key> 2000` for a hold, `sendkey compose` for `SDLK_APPLICATION` (`#40000065`), a uinput keyboard's `KEY_MENU` (never `sendkey menu`) for `SDLK_MENU` (`#40000076`), and the pad served with `python3 /root/sf-qa/virtual-gamepad.py serve --keep-open 14400` and driven with `python3 /root/sf-qa/virtual-gamepad.py send "press <Button>"`.
- **Logs.** As on Ubuntu GNOME: `streamflex -d`, logging to `~/.local/share/streamflex/streamflex.log`, with the `Video:` line naming the driver and renderer.
- **Session.** Plasma. Record the session type and whether a compositor is running: the Transparent rows depend on it. Launch with `-d`, as on Fedora Workstation: a `-d` launch you make yourself, never the shipped *StreamFlex* entry. After each launch, check the log for `Gained keyboard focus`. That check, and every log line a row expects, need `-d`.

### Guest helpers

Copy these into `/root/sf-qa/` in the Linux guest, all three in one folder, and run them as root.

Every Linux guest's desktop user is `qa`, with the home `/home/qa`. `ubuntu-guest.sh` takes that user by default (`QA_USER`, whose home it reads from the user database), and the rows' `pgrep -u qa` and `/home/qa` paths assume it. On a guest whose desktop user is someone else, run every subcommand as `QA_USER=<user> bash /root/sf-qa/ubuntu-guest.sh <subcommand>`, and read `qa` and `/home/qa` in the rows as that user and home.

`ubuntu-guest.sh setup` needs the `.deb` layout: it stops when `/usr/share/streamflex/config.ini` is missing. On any other layout, such as a Fedora CI build until an `.rpm` ships, do two things by hand instead:
- run `make-test-images.py` directly (`python3 /root/sf-qa/make-test-images.py <folder> --owner <user>`);
- make a launch that passes `-d` yourself.

- **`tests/qa/ubuntu-guest.sh`**, written for the Ubuntu guest:
  - `facts` records the GPU, the resolution, the session, XWayland, audio, the SDL libraries StreamFlex loads, and uinput;
  - `setup` loads uinput, checks `[Gamepad] Enabled=true` in the packaged config and keeps the shipped copy as the baseline, installs the `sf-run.sh` launch wrapper (it runs `streamflex -d`) and the *StreamFlex Debug* app-menu entry, points Pictures at `~/Bilder` through `user-dirs.dirs`, and makes the picture folder `~/Bilder/Été` and `~/Été/café.txt`;
  - `reset-pass wayland|x11|default` deletes the user config and the kept logs and sets `SDL_VIDEODRIVER` for `sf-run.sh` (`default` leaves it unset, so SDL picks);
  - `add-cafe-entry` adds `Entry5=Café` to `[Main]` of the user config;
  - `pad-node` prints the virtual pad's `/dev/input` node, its udev properties and its ACL.
- **`tests/qa/make-test-images.py`** makes a picture folder: red `rouge.png`, green `VERT.PNG`, blue `BLEU.JPG`, a hidden `.caché.png` and `notes.txt`. The expected count is 3. `BLEU.JPG` is a real JPEG only when python3-gi's GdkPixbuf is present. On Windows the same five files are made by hand in an `Été` folder under the user's Pictures folder, with `caché.png` given the Hidden attribute.
- **`tests/qa/virtual-gamepad.py`** is a uinput Xbox 360 pad (USB 045e:028e) with StreamFlex's button names (`ButtonA` OK, `ButtonB` Back, `ButtonStart` settings, `ButtonDPadUp` and the other D-pad directions). Its subcommands are `press`, `sequence`, `axis`, `serve --keep-open S` with `send`, `list` and `selftest`.

## 1. Install and first launch

| Row | Steps | Expected |
|---|---|---|
| ☐ <a id="t-1-1"></a> **1.1** `[Win]` The zip holds no DLL | Before unpacking the release zip, list it: `Add-Type -AssemblyName System.IO.Compression.FileSystem`, then `@([IO.Compression.ZipFile]::OpenRead('<the zip>').Entries.FullName) -match '\.dll$'`. | The filter prints nothing: the zip holds no `vcruntime140.dll` and no other `.dll`. |
| ☐ <a id="t-1-2"></a> **1.2** `[Win]` It starts with no Visual C++ runtime installed | On a guest with no Visual C++ runtime, record `Get-Package *Visual C++*` and whether `C:\Windows\System32\vcruntime140.dll` exists; if the guest has one, say so, as the check is then weaker. Install no redistributable. Start `C:\StreamFlex\streamflex.exe -d` from `C:\StreamFlex`, as in pre-flight. **Frame.** | StreamFlex starts: no "vcruntime140.dll was not found" dialog appears, and the log reaches `Loading menu 'Main'`. |
| ☐ <a id="t-1-3"></a> **1.3** `[Ubuntu]` The `.deb` installs, its dependencies resolved from the archive | As root, in the folder holding the `.deb`: `dpkg-deb -f streamflex_<version>_amd64.deb Depends` (record the `Depends:` line), then `apt-get install -y ./streamflex_<version>_amd64.deb; echo "apt exit $?"`. | `apt exit 0`, and apt installs every dependency from the Ubuntu archive, with no "unmet dependencies" and no held packages. |
| ☐ <a id="t-1-4"></a> **1.4** `[Kubuntu]` The `.deb` installs, its dependencies resolved from the archive | As 1.3, on Kubuntu: as root, `dpkg-deb -f streamflex_<version>_amd64.deb Depends` (record the `Depends:` line), then `apt-get install -y ./streamflex_<version>_amd64.deb; echo "apt exit $?"`. | `apt exit 0`, and apt installs every dependency from the archive, with no "unmet dependencies" and no held packages. |
| ☐ <a id="t-1-5"></a> **1.5** `[Ubuntu]` Every library the installed program needs resolves | After the install of 1.3, `ldd /usr/bin/streamflex`. | No line of the output reads `not found`. |
| ☐ <a id="t-1-6"></a> **1.6** `[Kubuntu]` Every library the installed program needs resolves | After the install of 1.4, `ldd /usr/bin/streamflex`. | No line of the output reads `not found`. |
| ☐ <a id="t-1-7"></a> **1.7** `[Ubuntu]` SDL2 resolves to Ubuntu's classic SDL2 | Read the `libSDL2-2.0.so.0` line of 1.5's `ldd`, and find the package that owns the file it names with `dpkg -S <that path>` (`ubuntu-guest.sh facts` also names it). | `libSDL2-2.0.so.0` resolves to Ubuntu's classic SDL2 2.32, from `libsdl2-classic`, not from `sdl2-compat`. |
| ☐ <a id="t-1-8"></a> **1.8** `[Ubuntu]` The package installs the program | `dpkg -L streamflex`, after the install. | The list holds `/usr/bin/streamflex`. |
| ☐ <a id="t-1-9"></a> **1.9** `[Kubuntu]` The package installs the program | `dpkg -L streamflex`, after the install. | The list holds `/usr/bin/streamflex`. |
| ☐ <a id="t-1-10"></a> **1.10** `[Ubuntu]` The package installs its config | `dpkg -L streamflex`, after the install. | The list holds `/usr/share/streamflex/config.ini`. |
| ☐ <a id="t-1-11"></a> **1.11** `[Kubuntu]` The package installs its config | `dpkg -L streamflex`, after the install. | The list holds `/usr/share/streamflex/config.ini`. |
| ☐ <a id="t-1-12"></a> **1.12** `[Ubuntu]` The package installs its app-menu entry | `dpkg -L streamflex`, after the install. | The list holds `/usr/share/applications/streamflex.desktop`. |
| ☐ <a id="t-1-13"></a> **1.13** `[Kubuntu]` The package installs its app-menu entry | `dpkg -L streamflex`, after the install. | The list holds `/usr/share/applications/streamflex.desktop`. |
| ☐ <a id="t-1-14"></a> **1.14** `[Ubuntu]` The app menu finds StreamFlex | `sendkey meta_l` (the Activities overview), then type `streamflex`. **Frame.** | The search lists the *StreamFlex* entry. |
| ☐ <a id="t-1-15"></a> **1.15** `[Kubuntu]` The app menu finds StreamFlex | `sendkey meta_l` (the application launcher), then type `streamflex`. **Frame.** | The search lists the *StreamFlex* entry. |
| ☐ <a id="t-1-16"></a> **1.16** `[Ubuntu]` The app-menu entry has StreamFlex's own icon | The search of 1.14. **Frame.** | The *StreamFlex* entry shows StreamFlex's icon, not a generic application icon. |
| ☐ <a id="t-1-17"></a> **1.17** `[Kubuntu]` The app-menu entry has StreamFlex's own icon | The search of 1.15. **Frame.** | The *StreamFlex* entry shows StreamFlex's icon, not a generic application icon. |
| ☐ <a id="t-1-18"></a> **1.18** `[Ubuntu]` The shipped entry starts StreamFlex | On the search of 1.14, `sendkey ret`, then wait 5 s. This launch alone uses the shipped *StreamFlex* entry, because the claim is that entry; it runs plain `streamflex`, with no `-d`, so nothing in it is read from the log. Then `pgrep -a -u qa streamflex`. | StreamFlex starts from that entry: `pgrep` prints one process, and its command line is plain `streamflex`, with no `-d`. A `streamflex -d` line means the *StreamFlex Debug* entry started instead, and the row does not pass. |
| ☐ <a id="t-1-19"></a> **1.19** `[Kubuntu]` The shipped entry starts StreamFlex | On the search of 1.15, `sendkey ret`, then wait 5 s. As in 1.18, this launch uses the shipped *StreamFlex* entry, with no `-d`. Then `pgrep -a -u qa streamflex`. | StreamFlex starts from that entry: `pgrep` prints one process, and its command line is plain `streamflex`, with no `-d`. A `streamflex -d` line means the *StreamFlex Debug* entry started instead, and the row does not pass. |
| ☐ <a id="t-1-20"></a> **1.20** `[All]` StreamFlex covers the whole screen | A launch with the shipped config and, on Linux, `SDL_VIDEODRIVER` unset: the launch of 1.18 or 1.19 on Ubuntu and Kubuntu, a launch as in pre-flight elsewhere. Module 13 repeats this with each driver forced. **Frame**, and look where the desktop's own bar would be: the Windows taskbar; GNOME's top bar, with its clock across the middle, in the top 40 rows; the Plasma panel, at the bottom edge by default. | No taskbar, top bar, panel, clock, window title bar or border shows: every pixel of the bar's rows is the black background, (0,0,0). |
| ☐ <a id="t-1-21"></a> **1.21** `[All]` The shipped Esc hotkey quits | From Main, key Esc (SendInput `VK_ESCAPE`; `sendkey esc`), the shipped config's `Hotkey1=#1B;:quit`. Then look for the process: `Get-Process streamflex` on Windows, `pgrep -a -u qa streamflex` on Linux. | StreamFlex quits: no `streamflex` process is left. |
| ☐ <a id="t-1-22"></a> **1.22** `[Win]` The config beside the program is read | Start `C:\StreamFlex\streamflex.exe -d` from `C:\StreamFlex`, as in pre-flight. | The log shows `Config file found: .\config.ini (C:\StreamFlex\config.ini)`. |
| ☐ <a id="t-1-23"></a> **1.23** `[Linux]` With no user config, the packaged config is read | With no `~/.config/streamflex` and no `config.ini` in the folder StreamFlex starts in: as root `bash /root/sf-qa/ubuntu-guest.sh reset-pass default`, which deletes the user config and leaves `SDL_VIDEODRIVER` unset, then the *StreamFlex Debug* entry. On the Fedoras, delete `~/.config/streamflex` by hand, keep no `config.ini` beside the executable, and make your own `-d` launch. | The log shows `Config file found: /usr/share/streamflex/config.ini` on Ubuntu and Kubuntu, and `Config file found: <prefix>/share/streamflex/config.ini`, the build's own prefix, on the Fedoras. |
| ☐ <a id="t-1-24"></a> **1.24** `[All]` The first screen is Main | The first `-d` launch with the shipped config, as in pre-flight: on Windows the launch of 1.22, on Linux the launch of 1.23. **Frame.** | Main's four buttons, Kodi, Plex, Steam and System, on black, and the log shows `Loading menu 'Main'`. |
| ☐ <a id="t-1-25"></a> **1.25** `[All]` The display's resolution is used | The launch of 1.22 or 1.23. Compare with the display's mode: on Windows the resolution in Settings > System > Display (StreamFlex is DPI aware, so the native pixels, whatever the scale); on Linux the connected output's mode in `ubuntu-guest.sh facts`. Record the `Refresh rate:` line. | The log's `Resolution:` line names the display's mode, for example `Resolution:    1920x1080`. |
| ☐ <a id="t-1-26"></a> **1.26** `[All]` The pad present at start is opened | The pad connected before StreamFlex starts, as in pre-flight; the launch of 1.22 or 1.23. | The log shows `Gamepad connected with device index 0, instance id <N>`, then `Gamepad opened at device index 0, instance id <N>`, with the same N. |
| ☐ <a id="t-1-27"></a> **1.27** `[All]` The opened pad's mapping is logged | The launch of 1.26. | After `Gamepad opened at device index 0`, the log shows a `Gamepad Mapping:` block: SDL's mapping for the pad, a line that starts with its GUID and name and lists its buttons (`a:`, `b:`, `start:` and the rest). |
| ☐ <a id="t-1-28"></a> **1.28** `[All]` StreamFlex has the keyboard after a launch | A `-d` launch as in pre-flight: on Windows the launch of 1.22, in the interactive session; on Ubuntu and Kubuntu the *StreamFlex Debug* entry (the launch of 1.23); on the Fedoras your own `-d` launch. | The log shows `Gained keyboard focus`. |
| ☐ <a id="t-1-29"></a> **1.29** `[All]` The log names SDL's video driver and renderer at startup | The launch of 1.22 or 1.23, or any `-d` launch as in pre-flight. Record the renderer. | The log shows `Video: SDL's <driver> driver, the <renderer> renderer`: `windows` on Windows; on Linux, the driver the launch used, `wayland` or `x11`. |
| ☐ <a id="t-1-30"></a> **1.30** `[Win]` The video information names the Windows driver | The launch of 1.22. | The log's video information shows `Video driver:  windows`. |

## 2. Home grid and navigation

| Row | Steps | Expected |
|---|---|---|
| ☐ <a id="t-2-1"></a> **2.1** `[All]` OK on a submenu tile opens that menu | With the shipped config, a `-d` launch as in pre-flight. Move to *System* with Right three times (pad D-pad Right, or the Right key: SendInput `VK_RIGHT`; `sendkey right`), then press OK (pad A, or Return: SendInput `VK_RETURN`; `sendkey ret`). On Linux the pad presses are `python3 /root/sf-qa/virtual-gamepad.py send "press ButtonDPadRight"` and `send "press ButtonA"`. **Frame.** | The System menu opens: the log shows `Loading menu 'System'`, and the screen shows Shutdown, Restart, Sleep and Settings. |
| ☐ <a id="t-2-2"></a> **2.2** `[All]` The Menus page lists All menus, then each menu | Back to Main (pad B, or Backspace: SendInput `VK_BACK`; `sendkey backspace`). Open settings: key Menu (SendInput `VK_APPS`, as in pre-flight; `sendkey compose`) or pad Start. On the first page, Down twice to *Menus* (pad D-pad Down, or the Down key: SendInput `VK_DOWN`; `sendkey down`), then OK. **Frame.** | The page lists *All menus*, reading `4 × 1`, then a divider, then *Main* and *System*. |
| ☐ <a id="t-2-3"></a> **2.3** `[All]` All menus holds the shipped grid | OK on *All menus*. **Frame.** | Rows reads 1, Columns 4 and Largest button 256 px. |
| ☐ <a id="t-2-4"></a> **2.4** `[All]` Right on Columns steps it from 4 to 5 | Take a log mark. Down to *Columns*, then Right. **Frame.** | Columns reads 5, and the new lines hold `Settings: [Layout] Columns 4 -> 5`. |
| ☐ <a id="t-2-5"></a> **2.5** `[All]` A second Right steps Columns from 5 to 6 | Right again. **Frame.** | Columns reads 6, and the new lines hold `Settings: [Layout] Columns 5 -> 6`. |
| ☐ <a id="t-2-6"></a> **2.6** `[All]` The preview's buttons get smaller as Columns goes up | Compare the preview in the frames of 2.3 (4 columns) and 2.5 (6 columns), and read the new lines since 2.4's mark. | Main's buttons are smaller at 6 columns than at 4: the last `Menu 'Main': 6 x 1 grid, <N> px buttons` line gives a smaller N than the startup line `Menu 'Main': 4 x 1 grid, <N> px buttons`. |
| ☐ <a id="t-2-7"></a> **2.7** `[All]` The preview's titles shrink with its buttons | The frames and lines of 2.6. | Main's titles are smaller at 6 columns than at 4: the last `Menu 'Main': 6 x 1 grid` line ends in a smaller `<T> pt titles` than the startup line's. |
| ☐ <a id="t-2-8"></a> **2.8** `[All]` Moving quickly over the Menus list, the preview keeps the menu it had | Back to the Menus page, with the cursor on *All menus*. Take a log mark. Press Down and Up several times, about 100 ms apart, so the cursor moves over *Main* and *System* without resting, and stop on *System* (on Windows SendInput key-down and key-up pairs; on Linux `sendkey down` and `sendkey up`). **Frame** during the moves. | While the cursor keeps moving, the preview stays on the menu it showed, and the new lines hold at most one `Settings: the preview shows menu` line: the one 2.9 reads, once the cursor rests. |
| ☐ <a id="t-2-9"></a> **2.9** `[All]` Once the cursor rests, the preview shows the menu under it | After the moves of 2.8, rest 1 s on *System*. **Frame.** | The preview shows the System menu (Shutdown, Restart, Sleep, Settings), and the new lines hold `Settings: the preview shows menu 'System'`. |
| ☐ <a id="t-2-10"></a> **2.10** `[All]` The cursor skips the divider | Up to *All menus*, then Down once. **Frame.** | The cursor lands on *Main*: the divider between *All menus* and *Main* never holds it. |
| ☐ <a id="t-2-11"></a> **2.11** `[All]` A menu's own page shows the grid it follows | Down to *System*, then OK. **Frame.** | Rows reads *All menus (1)*, Columns *All menus (6)* (the 6 of 2.5) and Largest button *All menus (256 px)*. |
| ☐ <a id="t-2-12"></a> **2.12** `[All]` A menu's own page notes how it follows the shared grid | The frame of 2.11. | The page's note reads *The lowest step, All menus, follows the shared grid.* |
| ☐ <a id="t-2-13"></a> **2.13** `[All]` A menu's own page shows that menu in the preview | The frame of 2.11. | The preview shows the System menu: Shutdown, Restart, Sleep and Settings. |
| ☐ <a id="t-2-14"></a> **2.14** `[All]` Right on a menu's Rows gives the menu a value of its own | Take a log mark. Right on *Rows*. **Frame.** | Rows reads 1, no longer *All menus (1)*, and the new lines hold `Settings: [System] Rows (none) -> 1`. |
| ☐ <a id="t-2-15"></a> **2.15** `[All]` A second Right steps the menu's Rows to 2 | Right again. **Frame.** | Rows reads 2, and the new lines hold `Settings: [System] Rows 1 -> 2`. |
| ☐ <a id="t-2-16"></a> **2.16** `[All]` The preview lays the menu out two rows tall | The new lines since 2.14's mark, and the frame of 2.15. | The preview's System grid is two rows tall: the last `Menu 'System':` line reads `Menu 'System': 6 x 2 grid, <N> px buttons, <T> pt titles`. |
| ☐ <a id="t-2-17"></a> **2.17** `[All]` Back on the first page, the preview shows Main again | Back twice, to the first page. **Frame.** | The preview shows Main's four buttons again, at the 6-column size of 2.5's frame. |
| ☐ <a id="t-2-18"></a> **2.18** `[All]` The Titles page's Size reads Medium | Down to *Titles* (the row after *Menus*), then OK. **Frame.** | Size reads *Medium*, the shipped `FontSize=14%`. |
| ☐ <a id="t-2-19"></a> **2.19** `[All]` Right on Size gives Large | Take a log mark. Right. **Frame.** | Size reads *Large*, and the new lines hold `Settings: [Titles] FontSize 14% -> 17%`. |
| ☐ <a id="t-2-20"></a> **2.20** `[All]` At Large the preview's titles are bigger | Compare the frames of 2.18 and 2.19, and read the new lines since 2.19's mark. | The preview's titles are bigger at Large than at Medium: the last `Menu 'Main':` line ends in a larger `<T> pt titles` than the one before the mark. |
| ☐ <a id="t-2-21"></a> **2.21** `[All]` Left on Size gives Medium again | Take a log mark. Left. **Frame.** | Size reads *Medium*, and the new lines hold `Settings: [Titles] FontSize 17% -> 14%`. |
| ☐ <a id="t-2-22"></a> **2.22** `[All]` A second Left gives Small | Take a log mark. Left again. **Frame.** | Size reads *Small*, and the new lines hold `Settings: [Titles] FontSize 14% -> 11%`. |
| ☐ <a id="t-2-23"></a> **2.23** `[All]` At Small the preview's titles are smaller | Compare the frames of 2.21 and 2.22, and read the new lines since 2.22's mark. | The preview's titles are smaller at Small than at Medium: the last `Menu 'Main':` line ends in a smaller `<T> pt titles` than at Medium. |
| ☐ <a id="t-2-24"></a> **2.24** `[All]` Back after a menu opens itself returns to Main | Back closes settings (it saves); key Esc quits. Reset the config as in pre-flight, then add `Entry5=Again;settings;:submenu System` to `[System]`: on Windows in `C:\StreamFlex\config.ini`; on Linux in the user config, made first as in 3.1 if no save has made one. Start with `-d` as in pre-flight and take a log mark. OK on *System*, Right four times to *Again*, OK (System opens again from itself), then Back. **Frame.** | Back returns to Main: the new lines hold `Loading menu 'System'` twice, the last `Loading menu` line is `Loading menu 'Main'`, and Main shows. |
| ☐ <a id="t-2-25"></a> **2.25** `[All]` A menu with more buttons than fit shows a scroll arrow | Keep 2.24's config and also add `Entry5=Five;apps;:back` and `Entry6=Six;apps;:back` to `[Main]`, which shows 4 columns. Start with `-d`. **Frame.** | Main shows four of its six buttons, with an arrow in the bottom right corner for the two past the edge, and the log has no `Could not render scroll indicator` line. |
| ☐ <a id="t-2-26"></a> **2.26** `[All]` Titles grow with large buttons | Reset the config as in pre-flight. In `[Layout]`, set `Columns=3` and delete the `IconSize` line, so the buttons grow as large as the grid allows. Start with `-d`. | The titles are 14% of the button: the log shows `Menu 'Main': 3 x 1 grid, <B> px buttons, <T> pt titles`, with T 14% of B, rounded: `556 px buttons, 78 pt titles` on a 1920 x 1080 display. |
| ☐ <a id="t-2-27"></a> **2.27** `[All]` A dense grid's titles stop at the readable minimum | Reset the config. In `[Layout]`, set `Rows=4` and `Columns=8`. Start with `-d`. | The titles stop at 2% of the screen height, rounded: the log shows `Menu 'Main': 8 x 4 grid, <B> px buttons, 22 pt titles` on a display 1080 rows tall. |
| ☐ <a id="t-2-28"></a> **2.28** `[All]` A fixed FontSize is the titles' size in points | Reset the config. In `[Titles]`, set `FontSize=48`, a fixed size (no `%`). Start with `-d`. | The titles are 48 pt, not 14% of the button: the log shows `Menu 'Main': 4 x 1 grid, <B> px buttons, 48 pt titles`. |
| ☐ <a id="t-2-29"></a> **2.29** `[All]` Titles turned off leave nothing under the buttons | Reset the config. In `[Titles]`, set `Enabled=false`. Start with `-d`. **Frame.** | No title shows under any button, and the log shows `Menu 'Main': 4 x 1 grid, <B> px buttons, no titles`. |
| ☐ <a id="t-2-30"></a> **2.30** `[All]` In Shrink mode, a long title is drawn smaller to fit | Reset the config. In `[Layout]`, set `Columns=3` and delete the `IconSize` line; in `[Titles]`, set `OversizeMode=Shrink`; in `[Main]`, change `Entry1`'s title from `Kodi` to `Kodi Media Center Deluxe`, keeping its icon and command. Start with `-d`. **Frame.** On a display smaller than 1920 x 1080, shorten the title until it is a few characters too wide for its button at the menu's size. | The long title is drawn whole, in a smaller size than Plex's title beside it, inside its button. |
| ☐ <a id="t-2-31"></a> **2.31** `[All]` In Shrink mode, a title too long even at the smallest size is cut to fit | Keep 2.30's config and also change `Entry2`'s title from `Plex` to `Supercalifragilisticexpialidocious Extended Edition Collector Set`. Start with `-d`. **Frame** that button, and zoom on both of its edges. | The title is cut to fit: no glyph is drawn past its button's left or right edge, and the log has no `px wide, over its` line. |
| ☐ <a id="t-2-32"></a> **2.32** `[All]` Shadowed titles keep room for their shadows | Reset the config. In `[Layout]`, set `Rows=4` and `Columns=1` and delete the `IconSize` line; in `[Titles]`, set `FontSize=36`, `Padding=20` and `Shadows=false`. Start with `-d`, note the `Menu 'Main': 1 x 4 grid, <B> px buttons` line, and quit with Esc. Set `Shadows=true` and start with `-d` again. | With shadows, the buttons are 2 px smaller, the room kept under each 36 pt title for its shadow: the second launch's `Menu 'Main': 1 x 4 grid` line gives B less 2. |
| ☐ <a id="t-2-33"></a> **2.33** `[All]` A shadowed title cut to fit keeps its shadow inside its button | Reset the config. In `[Titles]`, set `FontSize=36`, `Padding=20` and `Shadows=true`; in `[Main]`, change `Entry1`'s title from `Kodi` to `Kodi Media Center Deluxe Edition`. Start with `-d`. **Frame** that button, and zoom on both of its edges. | The title is cut so that it and its shadow fit the button: neither is drawn past its left or right edge, and the log has no `px wide, over its` line. |

## 3. Launching apps

| Row | Steps | Expected |
|---|---|---|
| ☐ <a id="t-3-1"></a> **3.1** `[All]` A non-ASCII title shows on Main | Add the *Café* entry to `[Main]`, directly after `Entry4=System;settings;:submenu System`. On Windows, make `C:\Users\Public\Été\café.txt` holding `StreamFlex café test`, and add `Entry9=Café;apps;"C:\Users\Public\Été\café.txt"` to `C:\StreamFlex\config.ini`, saved as UTF-8 with no BOM. On Linux, with a user config in place (if no save has made one, copy the packaged config to `~/.config/streamflex/config.ini` as the desktop user: `/usr/share/streamflex/config.ini` from the `.deb`; on the Fedoras, the build's `<prefix>/share/streamflex/config.ini`), run `bash /root/sf-qa/ubuntu-guest.sh add-cafe-entry` as root. A `config.ini` beside a Fedora build's executable is read before `~/.config` (pre-flight), so where one is there, add the same entry to it by hand instead. The helper adds `Entry5=Café;apps;gnome-text-editor "/home/qa/Été/café.txt"`, or `xdg-open` where `gnome-text-editor` is missing. `ubuntu-guest.sh setup` made `~/Été/café.txt`; on the Fedoras, make it by hand, holding `StreamFlex café test`. Start StreamFlex with `-d`, as in pre-flight. **Frame.** | Main shows a *Café* button, its accent intact. |
| ☐ <a id="t-3-2"></a> **3.2** `[All]` A title too wide for its button is cut to fit | Also add, directly after `Entry4=System;settings;:submenu System`, `Entry8=Extraordinarily Wide Title WWWWWWWWWWWWWWWWWWWWWW;apps;notepad.exe` (on Linux, `gnome-text-editor` in place of `notepad.exe`), and start as in 3.1. **Frame** that button, and zoom on both of its edges. | The title is cut to fit: no glyph is drawn past the button's left or right edge, or into a neighbor. |
| ☐ <a id="t-3-3"></a> **3.3** `[All]` Launching a non-ASCII path opens the file | Take a log mark. Move onto *Café* (pad D-pad Right, or the Right key) and press OK: pad A (the ViGEm pad on Windows; `python3 /root/sf-qa/virtual-gamepad.py send "press ButtonA"` on Linux) or Return (SendInput `VK_RETURN`; `sendkey ret`). **Frame.** On Linux, find the editor with `pgrep -a -f 'Été/café.txt'`. On KDE, where the entry runs `xdg-open`, the editor may be handed the file as a `file://` URL instead (`file:///home/qa/%C3%89t%C3%A9/caf%C3%A9.txt`): then find it with `pgrep -a -f 'caf%C3%A9.txt'`, and record which form KDE showed. | The editor opens `café.txt` and shows `StreamFlex café test`: on Windows, Notepad, its title naming `café.txt`; on Linux, the editor the entry names, whose command line holds the path with its accents intact, or on KDE its `file://` URL. |
| ☐ <a id="t-3-4"></a> **3.4** `[All]` StreamFlex sees the app take the screen | The new lines of 3.3's launch. | The new lines include `Application detected`. |
| ☐ <a id="t-3-5"></a> **3.5** `[All]` The launch logs no failure | The new lines of 3.3's launch. | No launch failure is logged: on Windows no `Failed to launch command` and no `Could not launch` line; on Linux no `Application failed to launch` and no `Could not fork` line. |
| ☐ <a id="t-3-6"></a> **3.6** `[All]` Settings do not open while an app is launching | Take a mark, press OK on *Café* as in 3.3, and within 300 ms key Menu (SendInput `VK_APPS`, as in pre-flight; `sendkey compose`). | The new lines hold `Key … (#40000065) detected`, then `Settings: not opened while an application is launching or running`, and no `Settings opened`. |
| ☐ <a id="t-3-7"></a> **3.7** `[All]` Settings do not open while the app runs in front | Once the editor is in front (`Application detected` is logged), key Menu (it goes to the desktop's focused window, the editor) and press pad Start (`python3 /root/sf-qa/virtual-gamepad.py send "press ButtonStart"` on Linux). | The new lines hold no `Settings opened`. |
| ☐ <a id="t-3-8"></a> **3.8** `[All]` The pad is ignored while the app is in front | With the editor in front, take a mark, then press pad D-pad Right, D-pad Down, A, B and Start, each held 150 ms, 500 ms apart (on Linux `python3 /root/sf-qa/virtual-gamepad.py send "press ButtonDPadRight"`, then `ButtonDPadDown`, `ButtonA`, `ButtonB`, `ButtonStart`). | The new lines hold no `Gamepad … detected` line for any of the five. |
| ☐ <a id="t-3-9"></a> **3.9** `[All]` The pad's presses do not reach the app | The presses of 3.8. **Frame** the editor before and after them. | The editor's text is unchanged: `StreamFlex café test`, with nothing added. |
| ☐ <a id="t-3-10"></a> **3.10** `[All]` StreamFlex takes the keyboard back when the app closes | Close the editor by its PID, never by name: on Windows the Notepad whose window title names `café`; on Linux the PID `pgrep -a -f 'Été/café.txt'` prints, or on KDE `pgrep -a -f 'caf%C3%A9.txt'` when the editor got the `file://` URL (as in 3.3). Send nothing until the line below is logged: keys go to the focused window. | The new lines hold `Gained keyboard focus`, then `Application finished`. |
| ☐ <a id="t-3-11"></a> **3.11** `[All]` No settings screen appears after the app closes | After 3.10. **Frame.** | StreamFlex shows Main, not the settings screen. |
| ☐ <a id="t-3-12"></a> **3.12** `[All]` Nothing moved while the app was in front | After 3.10. **Frame.** | The cursor is on *Café*, the button that launched the app, as it was before the launch. |

## 4. Input

| Row | Steps | Expected |
|---|---|---|

## 5. Backgrounds

| Row | Steps | Expected |
|---|---|---|

## 6. The settings screen

| Row | Steps | Expected |
|---|---|---|

## 7. Every setting

| Row | Steps | Expected |
|---|---|---|

## 8. Pickers

| Row | Steps | Expected |
|---|---|---|

## 9. Bindings

| Row | Steps | Expected |
|---|---|---|

## 10. Restart

| Row | Steps | Expected |
|---|---|---|

## 11. The exit hotkey

| Row | Steps | Expected |
|---|---|---|

## 12. Quitting

| Row | Steps | Expected |
|---|---|---|

## 13. Video drivers

| Row | Steps | Expected |
|---|---|---|
| ☐ <a id="t-13-1"></a> **13.1** `[Linux]` With no driver forced, the log names the display server StreamFlex uses | As root `bash /root/sf-qa/ubuntu-guest.sh reset-pass default`, which leaves `SDL_VIDEODRIVER` unset (and deletes `~/.config/streamflex`), then the *StreamFlex Debug* entry; on the Fedoras, your own `-d` launch with `SDL_VIDEODRIVER` unset. Then, as root, `ss -xp`, and read its `streamflex` lines. Record the driver SDL chose: it is the default. | The display connection matches the log's `Video: SDL's <driver> driver` line: a `wayland-0` path for `wayland`, an `X11-unix` path for `x11`. |
| ☐ <a id="t-13-2"></a> **13.2** `[Linux]` With Wayland forced, SDL uses the Wayland driver | As root `bash /root/sf-qa/ubuntu-guest.sh reset-pass wayland`, which sets `SDL_VIDEODRIVER=wayland` for `sf-run.sh` (and deletes `~/.config/streamflex`), then the *StreamFlex Debug* entry; on the Fedoras, your own launch of `SDL_VIDEODRIVER=wayland streamflex -d`. This needs a Wayland session: on Plasma running on X11, record the row as not measured. Record the renderer. | The log shows `Video: SDL's wayland driver, the <renderer> renderer` and `Video driver:  wayland`. |
| ☐ <a id="t-13-3"></a> **13.3** `[Linux]` With Wayland forced, StreamFlex covers the whole screen | The launch of 13.2. **Frame**, and look where the desktop's bar would be: GNOME's top bar, with its clock across the middle, in the top 40 rows; the Plasma panel, at the bottom edge by default. | No top bar, panel, clock, window title bar or border shows: every pixel of the bar's rows is the black background, (0,0,0). |
| ☐ <a id="t-13-4"></a> **13.4** `[Linux]` With Wayland forced, Main's buttons are drawn | The launch of 13.2, with the packaged config (`reset-pass` deleted the user config). **Frame.** | Main's four buttons, Kodi, Plex, Steam and System, are drawn on the black background. |
| ☐ <a id="t-13-5"></a> **13.5** `[Linux]` With Wayland forced, the display's resolution is used | The launch of 13.2. Compare with the connected output's mode in `ubuntu-guest.sh facts`. | The log's `Resolution:` line names the output's mode. |
| ☐ <a id="t-13-6"></a> **13.6** `[Linux]` With Wayland forced, StreamFlex has the keyboard | The launch of 13.2. | The log shows `Gained keyboard focus`. |
| ☐ <a id="t-13-7"></a> **13.7** `[Linux]` With Wayland forced, a launched app opens | `reset-pass` deleted the user config, so make it again and add the *Café* entry, both as in 3.1, then launch as in 13.2. Move onto *Café* and press OK (`python3 /root/sf-qa/virtual-gamepad.py send "press ButtonA"`). **Frame.** | The editor opens `café.txt` and shows `StreamFlex café test`. |
| ☐ <a id="t-13-8"></a> **13.8** `[Linux]` With Wayland forced, StreamFlex takes the keyboard back when the app closes | Close the editor of 13.7 by its PID, never by name: the PID `pgrep -a -f 'Été/café.txt'` prints, or on KDE `pgrep -a -f 'caf%C3%A9.txt'` when the editor got the `file://` URL (as in 3.3; record which form). Send nothing until the line below is logged. | The log shows `Gained keyboard focus`, then `Application finished`. |
| ☐ <a id="t-13-9"></a> **13.9** `[Linux]` With X11 forced, SDL uses the X11 driver | As root `bash /root/sf-qa/ubuntu-guest.sh reset-pass x11`, which sets `SDL_VIDEODRIVER=x11` for `sf-run.sh` (and deletes `~/.config/streamflex`), then the *StreamFlex Debug* entry; on the Fedoras, your own launch of `SDL_VIDEODRIVER=x11 streamflex -d`. This needs XWayland, or an X11 session: record which (`ubuntu-guest.sh facts` records the session and XWayland). Record the renderer. | The log shows `Video: SDL's x11 driver, the <renderer> renderer` and `Video driver:  x11`. |
| ☐ <a id="t-13-10"></a> **13.10** `[Linux]` With X11 forced, StreamFlex covers the whole screen | The launch of 13.9. **Frame**, and look where the desktop's bar would be, as in 13.3. | No top bar, panel, clock, window title bar or border shows: every pixel of the bar's rows is the black background, (0,0,0). |
| ☐ <a id="t-13-11"></a> **13.11** `[Linux]` With X11 forced, Main's buttons are drawn | The launch of 13.9, with the packaged config (`reset-pass` deleted the user config). **Frame.** | Main's four buttons, Kodi, Plex, Steam and System, are drawn on the black background. |
| ☐ <a id="t-13-12"></a> **13.12** `[Linux]` With X11 forced, the display's resolution is used | The launch of 13.9. Compare with the connected output's mode in `ubuntu-guest.sh facts`. | The log's `Resolution:` line names the output's mode. |
| ☐ <a id="t-13-13"></a> **13.13** `[Linux]` With X11 forced, StreamFlex has the keyboard | The launch of 13.9. | The log shows `Gained keyboard focus`. |
| ☐ <a id="t-13-14"></a> **13.14** `[Linux]` With X11 forced, a launched app opens | Make the user config again and add the *Café* entry, both as in 3.1, then launch as in 13.9. Move onto *Café* and press OK (`sendkey ret`). **Frame.** | The editor opens `café.txt` and shows `StreamFlex café test`. |
| ☐ <a id="t-13-15"></a> **13.15** `[Linux]` With X11 forced, StreamFlex takes the keyboard back when the app closes | Close the editor of 13.14 by its PID, never by name, found as in 13.8. Send nothing until the line below is logged. | The log shows `Gained keyboard focus`, then `Application finished`. |

## 14. The config file

| Row | Steps | Expected |
|---|---|---|
| ☐ <a id="t-14-1"></a> **14.1** `[All]` With VSync on, the renderer gives VSync | Reset the config as in pre-flight; its `[General]` has `VSync=true`. Start with `-d` as in pre-flight. | The log shows `Frame timing: VSync wanted on, the renderer gives on`. |
| ☐ <a id="t-14-2"></a> **14.2** `[All]` With VSync off, FPSLimit sets the frame time | Edit the config (on Windows `C:\StreamFlex\config.ini`; on Linux the user config, made first as in 3.1 if no save has made one): in `[General]`, set `VSync=false` and add `FPSLimit=10`, the documented minimum. Start with `-d`. | Each frame is paced to 100 ms: the log shows `Frame timing: FPS limit 10, 100 ms a frame`. |
| ☐ <a id="t-14-3"></a> **14.3** `[All]` Percentages with up to two decimals are read as written | Reset the config. In `[Layout]`, set `IconSpacing=40` and `VCenter=12.5%`; in `[Background]`, set `Overlay=true` and `OverlayOpacity=12.5%`; in `[Highlight]`, set `HPadding=300` and `FillOpacity=33.33%`. Start with `-d`. | No log line holds `Invalid`, and the log's list of settings shows `VCenter` as `12.5%`, `OverlayOpacity` as `12.5%` and `FillOpacity` as `33.33%`. |
| ☐ <a id="t-14-4"></a> **14.4** `[All]` A VCenter below 25% is drawn at 25% | The launch of 14.3. **Frame.** | The buttons' center is kept at a quarter of the screen height: the log's `Effective:` line gives `VCenter <V> px` with V a quarter of the height (`VCenter 270 px` on a display 1080 rows tall), and the buttons sit a quarter of the way down. |
| ☐ <a id="t-14-5"></a> **14.5** `[All]` HPadding is kept to half the gap between buttons | The launch of 14.3. | `HPadding=300` is drawn 20 px wide, half the 40 px gap: the log shows `Effective: IconSpacing 40 px, VCenter <V> px, HPadding 20 px`. |
| ☐ <a id="t-14-6"></a> **14.6** `[All]` A title FontFace the font lacks falls back to the bundled font | Reset the config. In `[Titles]`, set `Font=` to the bundled `DejaVuSans.ttf`, a font with one face (`C:\StreamFlex\assets\fonts\DejaVuSans.ttf` on Windows; `/usr/share/streamflex/assets/fonts/DejaVuSans.ttf` from the `.deb`; `<prefix>/share/streamflex/assets/fonts/DejaVuSans.ttf` on the Fedoras), and add `FontFace=9`, as a config moved from a machine with a font collection would hold. Start with `-d`. **Frame.** | The log shows `Could not open the font <that path> (face 9), using the default font`, then a `Title font:` line naming `OpenSans-Regular.ttf` `(face 0)`, and the titles are drawn. |
| ☐ <a id="t-14-7"></a> **14.7** `[All]` A menu split over two sections keeps both parts | Reset the config. At the end of the file, after `[System]`, add a second `[Main]` section holding `Entry5=Five;apps;:back` and `Rows=2`. Start with `-d`. **Frame.** | The second section's lines belong to Main, not to System before it: the log's menu list gives `Number of Entries: 5` and `Rows 2` under `Menu Name: Main`, and `Number of Entries: 4` under `Menu Name: System`, and Main shows five buttons in two rows. |
| ☐ <a id="t-14-8"></a> **14.8** `[All]` An empty entry is skipped, and the menu keeps the rest | Reset the config. In `[Main]`, add `Entry5=`, `Entry6=Notes;apps` (no command) and `Entry7=Pick;apps;:select`. Start with `-d`. **Frame.** | The log shows `Menu 'Main': 'Entry5' is empty, ignoring it`, and Main shows its four shipped buttons. |
| ☐ <a id="t-14-9"></a> **14.9** `[All]` An entry missing its command is ignored, with the reason | The launch of 14.8. | The log shows `Menu 'Main': 'Entry6' needs a title, an icon and a command, ignoring it`, and no *Notes* button shows. |
| ☐ <a id="t-14-10"></a> **14.10** `[All]` A menu entry using `:select` is ignored, with the reason | The launch of 14.8. | The log shows `Menu 'Main': 'Entry7' uses :select, which only a hotkey or gamepad button can, ignoring it`, and no *Pick* button shows. |
| ☐ <a id="t-14-11"></a> **14.11** `[All]` A menu entry running `:exit` says it does nothing | Reset the config. Add `Entry5=Close;apps;:exit` to `[Main]`. Start with `-d` and take a log mark. Move to *Close* (Right four times) and press OK. | The new lines hold `':exit' works only as a hotkey on Windows, where it closes the app on show; ignoring it`, and Main stays. |
| ☐ <a id="t-14-12"></a> **14.12** `[Linux]` An `:exit` hotkey on Linux says it does nothing | Keep 14.11's config and add `Hotkey2=#4000003B;:exit` (F2) to `[Hotkeys]`. Start with `-d`, take a log mark, and key F2 (`sendkey f2`). | After `Key F2 (#4000003B) detected`, the new lines hold `':exit' works only as a hotkey on Windows, where it closes the app on show; ignoring it`. |
| ☐ <a id="t-14-13"></a> **14.13** `[Linux]` With HOME unset, the log is kept in the home the user database gives | Make the user config as in 3.1, and keep no `config.ini` in `/tmp` or beside the executable. Empty `/home/qa/.local/share/streamflex/streamflex.log`. In a terminal in the desktop user's session, from `/tmp`, start `env -u HOME streamflex -d` (on the Fedoras, the build's own `streamflex`), as a systemd system service or an `env -i` launch would start it. Quit with Esc. | The startup's lines are written to `/home/qa/.local/share/streamflex/streamflex.log`, in the home `getent passwd qa` gives. |
| ☐ <a id="t-14-14"></a> **14.14** `[Linux]` With HOME unset, the user config is read from that home | The launch of 14.13. | The log shows `Config file found: /home/qa/.config/streamflex/config.ini`. |
| ☐ <a id="t-14-15"></a> **14.15** `[Linux]` With LANG unset and the clock on, StreamFlex starts and quits cleanly | Make the user config as in 3.1 and set `Enabled=true` in its `[Clock]`. In a terminal in the desktop user's session, start `env -u LANG streamflex -d; echo "exit $?"`, wait for Main, and key Esc (`sendkey esc`). | StreamFlex starts and quits with no crash: the terminal prints `exit 0`. |
| ☐ <a id="t-14-16"></a> **14.16** `[Linux]` A program StreamFlex starts keeps the LANG StreamFlex was given | Keep 14.15's config and add `StartupCmd=echo "$LANG" > /home/qa/sf-lang.txt` to `[General]`. Delete `/home/qa/sf-lang.txt`, then in the terminal start `LANG=en_US.UTF-8 streamflex -d`, wait for Main, and key Esc. | `/home/qa/sf-lang.txt` holds `en_US.UTF-8`, whole. |
| ☐ <a id="t-14-17"></a> **14.17** `[All]` A pad connected at another pad's old device index is added as a pad of its own | Reset the config and add the *Café* entry as in 3.1 (14.18 launches it). Start with `-d` and pad A connected, as in pre-flight. Connect a second pad, B; disconnect A; connect a third, C; 1 s between steps. On Windows these are ViGEm Xbox 360 pads. On Linux, B is `systemd-run --unit=sf-vgp-b /usr/bin/python3 /root/sf-qa/virtual-gamepad.py serve --keep-open 14400 --control /run/vgp-b.fifo`, A leaves with `python3 /root/sf-qa/virtual-gamepad.py send quit`, and C is made as B, with `sf-vgp-c` and `/run/vgp-c.fifo`. | After A's `Gamepad disconnected`, C is added: the log shows `Gamepad connected with device index <i>, instance id <N>` for it, with N an instance id of its own, neither A's nor B's (from their own `Gamepad connected` lines). |
| ☐ <a id="t-14-18"></a> **14.18** `[All]` After a launch and return, each pad opens again as itself | Without restarting after 14.17, take a log mark, launch *Café* as in 3.3, and close the editor by its PID as in 3.10. Afterwards, disconnect B and C, and connect pad A again as in pre-flight. | The new lines hold exactly one `Gamepad opened at device index <i>, instance id <N>` with B's N and exactly one with C's N: each pad is opened again by its own instance id, though B's device index changed when A left. |

## Index by module

## Retired ids

None yet.

## Global pass criteria

A release passes when every row has a verdict on every desktop its tag covers, and none is `F<n>` or `S<n>`. An unmeasured half is not a pass: it holds the release exactly as a fail does, until it is measured or the user decides to ship with it, and the results file records that decision.
