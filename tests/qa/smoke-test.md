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

  The pad is the ViGEm virtual Xbox 360 pad, connected before StreamFlex starts. Hold a pad press for 150 ms: StreamFlex samples the pad once a frame, so a shorter press can be missed. Leave at least 500 ms between inputs.
- **Logs.** Start `C:\StreamFlex\streamflex.exe -d`. The log is `C:\StreamFlex\streamflex.log`, beside the executable. It is emptied at each start by hand, so copy it away before starting again; StreamFlex's own restart carries on the same log. `-d` writes the debug lines, among them `Video: SDL's windows driver, the <renderer> renderer`, which names the driver and the renderer. A key that arrives logs `Key <name> (#<hex>) detected`, and a pad press logs `Gamepad <Button> detected`, so the log tells an input the harness never delivered from a fail of StreamFlex.
- **Session.** Start StreamFlex in the interactive desktop session. The log shows `Gained keyboard focus` once it has the keyboard.

### Ubuntu GNOME

- **Build.** The release `.deb`, installed as root with `apt-get install -y ./streamflex_<version>_amd64.deb`, which puts StreamFlex in `/usr/bin/streamflex` and its packaged config in `/usr/share/streamflex/config.ini`.
- **Config.** StreamFlex reads `config.ini` from the folder it was started in, then from the folder of the executable, then `~/.config/streamflex/`, then the packaged `/usr/share/streamflex/`. The packaged copy cannot be written, so the first save writes the user's own `~/.config/streamflex/config.ini`, which StreamFlex reads from then on. To reset to the shipped config, delete `~/.config/streamflex` (`ubuntu-guest.sh reset-pass` does this), and make sure no `config.ini` sits in the folder StreamFlex starts in.
- **Input.** Keys are QEMU HMP `sendkey` names (`up`, `down`, `left`, `right`, `ret`, `backspace`, `esc`, `meta_l`, `f5`); `sendkey <key> 2000` holds a key for 2 s. The Menu key:
  - `sendkey compose` reaches SDL as `SDLK_APPLICATION` (`#40000065`), as the 3b Ubuntu run used it.
  - **`SDLK_MENU` (`#40000076`) comes from a uinput keyboard's `KEY_MENU`, never `sendkey menu`**, which did not reach the app in qa-harness's v0.3.0 run. A row that needs `#40000076` says so in its steps, so a hand pass cannot read a harness miss as an app Fail.

  The pad is `tests/qa/virtual-gamepad.py`, served as root before any StreamFlex run (`systemd-run --unit=sf-vgp /usr/bin/python3 virtual-gamepad.py serve --keep-open 14400`) and driven with `send` (`virtual-gamepad.py send "press ButtonStart"`, or `"press ButtonStart --hold 2000"`). Its journal shows `created 'Microsoft X-Box 360 pad' (045e:028e) at /dev/input/eventN`, and `ubuntu-guest.sh pad-node` shows `ID_INPUT_JOYSTICK=1` and an ACL entry `user:qa:rw-`; without that ACL, StreamFlex running as the desktop user cannot read the pad. The pad needs `uinput` loaded (`modprobe uinput`, which `ubuntu-guest.sh setup` does). Leave at least 500 ms between inputs.
- **Logs.** Start `streamflex -d`. The log is `~/.local/share/streamflex/streamflex.log`, emptied at each start by hand; StreamFlex's own restart carries on the same log. `-d` writes the debug lines, among them `Config file found: <path>` and `Video: SDL's <driver> driver, the <renderer> renderer`, which names the driver and the renderer. A key that arrives logs `Key <name> (#<hex>) detected`, and a pad press logs `Gamepad <Button> detected`; a key repeat is logged before StreamFlex drops it, so a held key shows as more than one such line.
- **Session.** GNOME on Wayland: the user's session `Type` is `wayland`. Launch StreamFlex from the app menu, whose activation token gives the new window keyboard focus. Started any other way (for example `systemd-run --user`), check the log for `Gained keyboard focus`; without it, keys go to another window.

### Kubuntu

- **Build.** The release `.deb`, installed as on Ubuntu GNOME.
- **Config.** As on Ubuntu GNOME: the packaged `/usr/share/streamflex/config.ini` until the first save writes `~/.config/streamflex/config.ini`; delete `~/.config/streamflex` to reset.
- **Input.** As on Ubuntu GNOME: HMP `sendkey`, `sendkey <key> 2000` for a hold, `sendkey compose` for `SDLK_APPLICATION` (`#40000065`), a uinput keyboard's `KEY_MENU` (never `sendkey menu`) for `SDLK_MENU` (`#40000076`), and `tests/qa/virtual-gamepad.py serve` for the pad.
- **Logs.** As on Ubuntu GNOME: `streamflex -d`, logging to `~/.local/share/streamflex/streamflex.log`, with the `Video:` line naming the driver and renderer.
- **Session.** Plasma. Record the session type and whether a compositor is running: the Transparent rows depend on it. Check the log for `Gained keyboard focus` after each launch.

### Fedora Workstation

- **Build.** A build from StreamFlex's own CI, named in the results header by its run id and sha256, until StreamFlex ships an `.rpm` (todo item 25). Never a build compiled inside the guest.
- **Config.** The same search as on Ubuntu GNOME: the folder StreamFlex starts in, the executable's folder, `~/.config/streamflex/`, then the packaged copy under the build's `share/streamflex/`. A packaged copy that cannot be written sends the first save to `~/.config/streamflex/config.ini`. Delete `~/.config/streamflex` to reset.
- **Input.** As on Ubuntu GNOME: HMP `sendkey`, `sendkey <key> 2000` for a hold, `sendkey compose` for `SDLK_APPLICATION` (`#40000065`), a uinput keyboard's `KEY_MENU` (never `sendkey menu`) for `SDLK_MENU` (`#40000076`), and `tests/qa/virtual-gamepad.py serve` for the pad.
- **Logs.** As on Ubuntu GNOME: `streamflex -d`, logging to `~/.local/share/streamflex/streamflex.log`, with the `Video:` line naming the driver and renderer.
- **Session.** GNOME on Wayland. Launch from the app menu for keyboard focus, or check the log for `Gained keyboard focus`.

### Fedora KDE

- **Build.** A build from StreamFlex's own CI, as on Fedora Workstation: named by run id and sha256, never compiled inside the guest, until an `.rpm` ships (todo item 25).
- **Config.** As on Fedora Workstation.
- **Input.** As on Ubuntu GNOME: HMP `sendkey`, `sendkey <key> 2000` for a hold, `sendkey compose` for `SDLK_APPLICATION` (`#40000065`), a uinput keyboard's `KEY_MENU` (never `sendkey menu`) for `SDLK_MENU` (`#40000076`), and `tests/qa/virtual-gamepad.py serve` for the pad.
- **Logs.** As on Ubuntu GNOME: `streamflex -d`, logging to `~/.local/share/streamflex/streamflex.log`, with the `Video:` line naming the driver and renderer.
- **Session.** Plasma. Record the session type and whether a compositor is running: the Transparent rows depend on it. Check the log for `Gained keyboard focus` after each launch.

### Guest helpers

Copy these into the Linux guest, all three in one folder, and run them as root.

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

## 2. Home grid and navigation

| Row | Steps | Expected |
|---|---|---|

## 3. Launching apps

| Row | Steps | Expected |
|---|---|---|

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

## 14. The config file

| Row | Steps | Expected |
|---|---|---|

## Index by module

## Retired ids

None yet.

## Global pass criteria

A release passes when every row has a verdict on every desktop its tag covers, and none is `F<n>` or `S<n>`. An unmeasured half is not a pass: it holds the release exactly as a fail does, until it is measured or the user decides to ship with it, and the results file records that decision.
