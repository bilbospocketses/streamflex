# StreamFlex 3a hands-on check: Ubuntu 26.04 GNOME/Wayland

This is the Linux half of the hands-on check for the settings screen of sub-project 3a. It is run in an Ubuntu 26.04 guest (GDM autologin as `qa`), by hand or by a QA harness, against the PR's CI **Debian build** `.deb`. It covers the nine items the 3a plan set for Linux:

1. install the `.deb`, with its dependencies resolved;
2. launch from the app menu;
3. fullscreen under Wayland;
4. the keyboard and a virtual gamepad;
5. a settings round trip that saves to `~/.config/streamflex`;
6. Pictures through a non-English `user-dirs.dirs`;
7. a non-ASCII path;
8. the real Transparent window under the compositor;
9. a clean exit.

It also checks the Batch C fixes: a held Start or Menu opens settings once (Imp 1); upper-case extensions count and hidden files do not (Imp 2); and the Menu key works both as `SDLK_APPLICATION` and as `SDLK_MENU`. It also checks that settings do not open while an app launches or runs (#116), that the pad is ignored while another window has focus, that the Menus list's preview follows the cursor only after it rests about 300 ms, and that a title too long for its button is cut.

## What to copy into the guest

Into `/root/sf-qa/` (as root):

- the `.deb` (`streamflex_<version>_amd64.deb`);
- `tests/qa/virtual-gamepad.py`, `tests/qa/make-test-images.py` and `tests/qa/ubuntu-guest.sh`, all from the PR's branch.

## How to read and run each step

- **Keys** are QEMU HMP `sendkey` names, sent with `Send-QaGuestKeys`. They reach whatever window has focus.

  | Name here | HMP key |
  |---|---|
  | Up, Down, Left, Right | `up`, `down`, `left`, `right` |
  | Return (OK) | `ret` |
  | Backspace (Back) | `backspace` |
  | Esc (the shipped `:quit` hotkey) | `esc` |
  | Super (the Activities overview) | `meta_l` |
  | the PC context-menu key | `compose` |
  | the "Menu" key | `menu` |

  A held key uses HMP's hold time: `sendkey compose 2000` holds it for 2 s. With the harness this is `Send-QaGuestKeys -Keys 'compose 2000'`. Confirm first that this passes the hold time through; see the README.
- **The pad** is `virtual-gamepad.py`, served once as root and driven with `send`. Each `send` returns when the press is done:
  ```sh
  python3 /root/sf-qa/virtual-gamepad.py send "press ButtonStart"            # a 150 ms press
  python3 /root/sf-qa/virtual-gamepad.py send "press ButtonStart --hold 2000"
  ```
  The names are StreamFlex's own: `ButtonA` = OK, `ButtonB` = Back, `ButtonStart` = settings, and `ButtonDPadUp`, `ButtonDPadDown`, `ButtonDPadLeft` and `ButtonDPadRight` move. Leave at least 500 ms between inputs.
- **Frames:** `Save-QaGuestScreen` wherever a step says **Frame**, named `U<item>-<pass>-<what>.png`. Keep them all.
- **The log.** Every run in a pass is started through `/home/qa/sf-run.sh`, which runs `streamflex -d`. The live log is `/home/qa/.local/share/streamflex/streamflex.log`, and it is emptied at each start. When StreamFlex exits, `sf-run.sh` copies it to `~/sf-logs/last.log` and writes `~/sf-logs/exit-code`. Rename `last.log` (for example to `A-run1.log`) before the next start. To see what one action logged:
  ```sh
  L=/home/qa/.local/share/streamflex/streamflex.log
  mark=$(wc -l < $L)        # before the action
  tail -n +$((mark + 1)) $L # after it: the "new lines"
  ```
- **Verdicts.** Each item is **PASS** only when everything under *Expected* holds; otherwise it is **FAIL**, with the frame and log lines. **BLOCKED** means the harness could not deliver an input: every key that arrives logs `Key <name> (#<hex>) detected`, and every pad press logs `Gamepad <Button> detected`. BLOCKED is a harness finding, not a StreamFlex one.
- **Two passes.** Items 3 to 9 run twice:
  - **Pass A:** `SDL_VIDEODRIVER=wayland`, driven with the **pad**.
  - **Pass B:** `SDL_VIDEODRIVER=x11` (XWayland), driven with the **keyboard**.

  Items 1 and 2 run once, before pass A. The driver is set by `ubuntu-guest.sh reset-pass`, and `sf-run.sh` reads it. Wayland is forced in pass A because SDL2 on GNOME may choose X11 by itself (item 2 records what it chooses).
- **Launching a run.** Use the app menu. Its launch carries GNOME's activation token, so StreamFlex gets keyboard focus:
  1. `Send-QaGuestKeys -Keys meta_l`;
  2. `Send-QaGuestKeys -Text 'streamflex debug'`;
  3. `Send-QaGuestKeys -Keys ret`, then wait 5 s.

  If the harness launches it instead with `systemd-run --user --machine=qa@.host /home/qa/sf-run.sh`, check for `Gained keyboard focus` in the log. Without that line, keys go elsewhere: use the app menu.

## U0. Record the unknowns, then set up (no verdict)

1. As root: `bash /root/sf-qa/ubuntu-guest.sh facts`. Keep the whole output; it records what is not known about the guest in advance:
   - the GPU and its DRM driver (virgl or llvmpipe/simpledrm);
   - the connected output and its mode, which is the screen resolution;
   - the session type (it must be `wayland`);
   - whether XWayland runs, and `DISPLAY` / `WAYLAND_DISPLAY` in the user manager's environment;
   - the GL renderer, if `glxinfo` is present;
   - audio;
   - uinput.

   The SDL video driver and renderer come later, from StreamFlex's own log. They are written at debug level, so every run that records them uses `-d`, as `sf-run.sh` does.
2. Do item U1 (the install) now, because setup needs the installed package.
3. As root: `bash /root/sf-qa/ubuntu-guest.sh setup`. It:
   - loads `uinput`, and stops if `/dev/uinput` does not appear;
   - checks `[Gamepad] Enabled=true` in `/usr/share/streamflex/config.ini`. The gamepad is on by default, so the shipped file already says `true` and the step changes nothing (its `sed` is a no-op). The shipped copy is kept as `/root/config.ini.shipped`, and **`/home/qa/baseline-config.ini` becomes "the original" for every comparison below**;
   - installs `/home/qa/sf-run.sh` and the app-menu entry *StreamFlex Debug*;
   - points Pictures at `~/Bilder` in `~/.config/user-dirs.dirs` (`XDG_PICTURES_DIR="$HOME/Bilder"`);
   - makes `~/Bilder/Été`, holding `rouge.png` (red), `VERT.PNG` (green), `BLEU.JPG` (blue), a hidden `.caché.png` and `notes.txt`;
   - makes `~/Été/café.txt`.

   Expected: `setup done`, with no WARNING line. Note the editor it names for item 7. If `BLEU.JPG` is reported as "PNG data", record that too: StreamFlex still opens it, but that half no longer covers a real JPEG.
4. **The pad is on out of the box.** `diff /root/config.ini.shipped /home/qa/baseline-config.ini` prints nothing, and `sed -n '/^\[Gamepad\]/,/^\[/p' /home/qa/baseline-config.ini` shows `Enabled=true` or no `Enabled` line, never `false`. If it says `false`, the default is wrong: FAIL, and mark the pad checks BLOCKED. Start (U4 part 1) must then open settings with no edit to the config.
5. Confirm that `XDG_PICTURES_DIR` is **not** in the user manager's environment (see the `facts` output). If it is there, the variable overrides the file and item 6 cannot test the file.
6. Start the pad, as root, before any StreamFlex run. It serves for 4 hours:
   ```sh
   systemd-run --unit=sf-vgp /usr/bin/python3 /root/sf-qa/virtual-gamepad.py serve --keep-open 14400
   sleep 2; journalctl -u sf-vgp --no-pager | tail -n 3
   bash /root/sf-qa/ubuntu-guest.sh pad-node
   ```
   Expected:
   - the journal shows `created 'Microsoft X-Box 360 pad' (045e:028e) at /dev/input/eventN`;
   - `pad-node` shows `ID_INPUT_JOYSTICK=1`, and an ACL entry `user:qa:rw-` (the `uaccess` tag).

   If the ACL is missing, StreamFlex (running as qa) cannot read the pad. Record it, grant it with `setfacl -m u:qa:rw /dev/input/eventN`, and carry on.

## U1. Install the `.deb`; its dependencies resolve on Ubuntu (item 1)

- **As root:**
  ```sh
  cd /root/sf-qa
  dpkg-deb -f streamflex_*_amd64.deb Depends
  apt-get install -y ./streamflex_*_amd64.deb; echo "apt exit $?"
  ldd /usr/bin/streamflex | grep -E 'SDL|inih|not found'
  dpkg -L streamflex | grep -E 'bin/streamflex$|config.ini$|\.desktop$'
  ```
- **Expected:**
  - `apt exit 0`, and apt installs every dependency from the Ubuntu archive, with no "unmet dependencies" and no held packages. Record the `Depends:` line.
  - `ldd` shows no `not found`.
  - `libSDL2-2.0.so.0` resolves to Ubuntu's classic SDL2 2.32 (`facts` after the install names the owning package; expect `libsdl2-classic`, not `sdl2-compat`).
  - The package installs `/usr/bin/streamflex`, `/usr/share/streamflex/config.ini` and `/usr/share/applications/streamflex.desktop`.

## U2. Launch from the app menu (item 2; run once, before pass A)

- **Inputs:**
  1. `meta_l`, then type `streamflex`. **Frame** `U2-search.png`: the *StreamFlex* entry shows with its icon, not a generic one.
  2. `ret`, then wait 5 s. **Frame** `U2-running.png`.
- **Expected:**
  - StreamFlex fills the screen with Main's four buttons (Kodi, Plex, RetroArch or Steam, System) on black.
  - `pgrep -a -u qa streamflex` shows one process.
  - Record which display server it chose: as root, `ss -xp | grep streamflex`. A `wayland-0` path means Wayland; `X11-unix` means XWayland. This is the default, with no `SDL_VIDEODRIVER`.
- **Then** key `esc`: StreamFlex exits, and `pgrep` shows nothing.
- This run has no `-d`, so it leaves no debug log.
- **PASS** if it launches and fills the screen, and the icon shows.

## Pass A and pass B

Before each pass, as root:

- `bash /root/sf-qa/ubuntu-guest.sh reset-pass wayland` (pass A), or `reset-pass x11` (pass B).

This deletes `~/.config/streamflex` and the kept logs, so each pass starts from the packaged system config. Then run U3 to U9 in the order below.

- **Pass A** (the pad):
  - OK is `ButtonA`, Back is `ButtonB`, the arrows are the D-pad, and settings open with `ButtonStart`.
- **Pass B** (the keyboard):
  - OK is `ret`, Back is `backspace`, the arrows are the arrow keys, and settings open with `compose`, or with whichever key U4 showed as `#40000065`.

## U3. Fullscreen, and what SDL chose (item 3)

- **Inputs:** launch *StreamFlex Debug* (see "Launching a run"). **Frame** `U3-<pass>-home.png`.
- **Expected:**
  - The log, from a `-d` run, names the pass's driver: `Video: SDL's wayland driver, the <renderer> renderer` in pass A, `Video: SDL's x11 driver, the <renderer> renderer` in pass B (record the renderer), and `Video driver:  wayland` or `x11` as before. The `Video:` line is debug-level: without `-d` it is absent.
  - `Resolution:` matches the output's mode from U0.
  - The log also holds `Config file found: /usr/share/streamflex/config.ini`, `Gamepad connected with device index 0, instance id <N>` and then `Gamepad opened at device index 0, instance id <N>` (the same N), `Loading menu 'Main'` and `Gained keyboard focus`.
  - The frame shows StreamFlex over the **whole** screen: no GNOME top bar, no clock, no window title bar or border. Every pixel in the top 40 rows is (0,0,0), including across the middle where GNOME's clock would be.
- Record the resolution, the driver, the renderer and the `Refresh rate:` line.

## U4. Keyboard and pad (item 4; Batch C Imp 1 and the Menu key)

Take a mark before each part, and close settings after each part with Back (`ButtonB` or `backspace`); the log then shows `Settings closed`.

1. **A single Start, out of the box.** `send "press ButtonStart"`, with the config as packaged (no `Enabled` edit).
   - Expected: settings open with the column on the left and the preview on the right. **Frame** `U4-<pass>-start.png`.
   - New lines: `Gamepad ButtonStart detected` and `Settings opened over menu 'Main'`.
2. **A held Start.** `send "press ButtonStart --hold 2000"`, then wait 1 s.
   - Expected: settings open, and they are still open. **Frame** `U4-<pass>-held-start.png`.
   - The new lines hold exactly one `Settings opened` and no `Settings closed`.
3. **The two Menu keys, once each.** Key `compose`, record the new lines, and close. Then key `menu`, record, and close.
   - Expected: between them, the two keys log `Key … (#40000065) detected` (`SDLK_APPLICATION`) and `Key … (#40000076) detected` (`SDLK_MENU`), and **each one opens settings** (`Settings opened` after its Key line).
   - If one of the two codes never arrives, that half is BLOCKED; note which HMP key gave which code.
   Both codes open settings, as the configuration guide says: #40000065 and #40000076. Match on the keycode, not the name: SDL may name either code `Menu` (the tested Ubuntu build logs `Key Menu` for both), and `#40000065` reads `Application` on other SDL versions.
4. **The Menu keys held.** For each key that arrived in part 3: `Send-QaGuestKeys -Keys '<key> 2000'`, wait 1 s, then take a **Frame**.
   - Expected: settings are open. The new lines hold **at least two** `Key … detected` lines for that key, which shows the auto-repeat reached StreamFlex, exactly one `Settings opened`, and no `Settings closed`.
   - With only one Key line, the hold did not repeat: BLOCKED, not PASS. (A repeat is logged before StreamFlex drops it, so one line means the guest sent no repeats.)
5. **Settings cannot open while an app runs (#116).** This uses the *Café* entry, so do it after U7's setup, or run `add-cafe-entry` now. Launch *Café* (OK on it). The moment the editor starts, and again once it is in front, send Start (`send "press ButtonStart"`) and key `compose`.
   - Expected: the new lines hold no `Settings opened` and no settings screen appears, in StreamFlex or after the editor closes. There may be no pad line at all (see U7b).
- **PASS** when parts 1 to 5 all hold for every code that arrived.

## U5 and U6. The settings round trip, and Pictures through `user-dirs.dirs` (items 5 and 6)

One settings visit, saved once. Open settings (Start or `compose`). The rows are *Background › Colour*, *Menus › 2 menus*, *Titles › Medium*, a divider and *Discard changes*. Each arrow below is the D-pad in pass A and an arrow key in pass B.

1. **Background.** OK. Then:
   - Down, Right: *Colour* becomes *Charcoal*. New line: `Settings: [Background] Color #000000 -> #1E1E1E`.
   - Up, then Right three times: Mode goes through *Image* and *Slideshow* to *Transparent*. The preview shows a **checkerboard**. **Frame** `U5-<pass>-checker.png`.
   - Left three times, back to *Colour*: the preview is charcoal again.
2. **Pictures (item 6).** Right (Mode becomes *Image*), Down, OK. **Frame** `U6-<pass>-browser.png`.
   - Expected: the browser opens in **`/home/qa/Bilder`**, not `~/Pictures`. New line: `Settings: browsing /home/qa/Bilder`. That is Batch B #99, reading `XDG_PICTURES_DIR` from `user-dirs.dirs`.
3. **The picture folder.** Highlight `Été` and press OK.
   - New line: `Settings: browsing /home/qa/Bilder/Été`.
   - The list is **exactly** `BLEU.JPG`, `rouge.png` and `VERT.PNG`, in that order. There is no `.caché.png` and no `notes.txt`. **Frame** `U6-<pass>-ete.png`.
   - Move over the three images, pausing 1 s on each. (The three images have upper-case extensions among them: `VERT.PNG` and `BLEU.JPG` are found, and the hidden file is not.) The preview turns blue, red, then green, and a `Settings: the preview shows …` line is logged for each.
   - Go back to `rouge.png` and press OK. New line: `Settings: chose /home/qa/Bilder/Été/rouge.png`.
4. **A slideshow folder.** Up, then Right (Mode becomes *Slideshow*), Down (*Folder*), OK. Go into `Été` if the browser did not open there.
   - The first row reads **Use this folder (3 images)**. **Frame** `U6-<pass>-use-folder.png`.
   - OK on it. The Folder row reads `Été · 3 images`.
   - Then Back, to the top level.
5. **Menus.**
   - Down, OK (the Menus page). **The preview rests first:** press Down and Up several times about 100 ms apart over *Main* and *System*. The preview stays on the menu it had while the cursor keeps moving, and switches about 300 ms after the last move; rest 1 s on *System* and it shows System. Return to *All menus*, then OK.
   - Down (*Columns*), then Right twice. New lines: `[Layout] Columns 4 -> 5` and `5 -> 6`. The preview's buttons shrink. **Frame** `U5-<pass>-cols6.png`.
   - Back, Down twice (*System*), OK. The preview switches to System. Right twice on *Rows*. New lines: `[System] Rows (none) -> 1` and `1 -> 2`.
   - Back twice, to the top level.
6. **Titles.** Down (*Titles*), OK, then Right, Left, Left. New lines: `[Titles] FontSize 14% -> 17%`, `17% -> 14%`, then `14% -> 11%`. Back.
7. **Save.** Back at the top level.
   - Expected: settings close onto Main, with the slideshow running, 6 across, and small titles. **Frame** `U5-<pass>-saved.png`.
   - New line: `Settings saved 7 change(s) to /home/qa/.config/streamflex/config.ini (backup: none)`.
8. **The files.** As root:
   ```sh
   cmp /usr/share/streamflex/config.ini /home/qa/baseline-config.ini && echo "system copy unchanged"
   stat -c '%U %a %n' /home/qa/.config/streamflex/config.ini
   diff /home/qa/baseline-config.ini /home/qa/.config/streamflex/config.ini
   ls -a /home/qa/.config/streamflex/
   ```
   - Expected: the system copy is unchanged, and the new file belongs to `qa`.
   - `diff` shows exactly these differences, and nothing else:
     - `[Background]`: `Mode=Slideshow`, `Color=#1E1E1E`, and two new lines after `OverlayOpacity=50%`: `Image=/home/qa/Bilder/Été/rouge.png` and `SlideshowDirectory=/home/qa/Bilder/Été`;
     - `[Layout]`: `Columns=6`;
     - `[Titles]`: `FontSize=11%`;
     - `[System]`: a new `Rows=2`, directly under its header.
   - Every comment line is intact. There is no `config.ini.bak` (the save made the file, so there was nothing to back up), and no `.tmp` file.
9. **Quit and restart (items 5 and 9).**
   - Key `esc`. Expected: `~/sf-logs/exit-code` holds `0`, and the log ends with `Quitting program`. Rename `last.log` to `<pass>-run1.log`.
   - Launch again, wait 5 s, and take a **Frame** `U5-<pass>-restart.png`.
   - Expected: `Config file found: /home/qa/.config/streamflex/config.ini`, `Background set up: Slideshow`, and **`Found 3 images in directory /home/qa/Bilder/Été:`** followed by exactly the three paths ending `BLEU.JPG`, `rouge.png` and `VERT.PNG`. There is no `.caché.png`: Batch C Imp 2, the slideshow scan. An old scan logs `Found 1` or `Found 2`, so the count alone decides. Main shows 6 across and small titles.
- **PASS (item 5)** when steps 1 and 3 to 9 hold. **PASS (item 6)** when steps 2 and 4 hold.

## U8. The real Transparent window (item 8)

On Linux, StreamFlex paints the chroma-key colour (`#010101`), and a compositor with a shader has to key it out. The documented way is picom on X11. **GNOME's compositor does not do this, so on this guest the desktop is not expected to show through.** This item records what the real window does, and checks that Transparent mode runs cleanly.

- **Inputs:** open settings; OK (*Background*); Right (Mode goes from *Slideshow* to *Transparent*); Back; Back (save). Wait 2 s. **Frame** `U8-<pass>-transparent.png`.
- **Expected:**
  - New lines: `Background set up: Transparent`, and `Settings saved 1 change(s)`.
  - StreamFlex is still running and drawing its buttons.
  - Record whether the desktop shows through (expected: no), and the colour of a background pixel away from the buttons (expected: about (1,1,1)).
- **Then back:** open settings; OK; Left three times (*Colour*); Back; Back. **Frame** `U8-<pass>-colour.png`. The background is charcoal (30,30,30), and a new line reads `Background set up: Color`.
- **PASS** when StreamFlex stays up through both changes and both lines appear. If the desktop does show through, record it as a surprise: no verdict change.

## U5c. A read-only config (as in the Windows W8)

- **Inputs:**
  1. As qa: `chmod 444 ~/.config/streamflex/config.ini; sha256sum ~/.config/streamflex/config.ini`.
  2. Open settings; Down twice (*Titles*); OK; Right (*Medium*); Back; Back.
- **Expected:**
  - The rows are replaced by *Couldn't save to /home/qa/.config/streamflex/config.ini: permission denied*, with *Try again* and *Leave without saving* under it. Both rows sit fully inside the column (Batch C New 1). **Frame** `U5c-<pass>-failed.png`.
- **Inputs:** Down (*Leave without saving*), then OK.
- **Expected:**
  - Settings close. New lines: `Settings: leaving without saving` and `Settings closed`.
  - The file's `sha256sum` is unchanged, and no `.tmp` file is left.
- **Then:**
  1. `chmod 644 ~/.config/streamflex/config.ini`.
  2. Key `esc`. Expected: `exit-code` is `0`.
  3. Rename `last.log` to `<pass>-run2.log`.

## U7. A non-ASCII path (item 7)

- **Setup** (as root): `bash /root/sf-qa/ubuntu-guest.sh add-cafe-entry`. It prints the line it added to `[Main]`, for example `Entry5=Café;apps;gnome-text-editor "/home/qa/Été/café.txt"`.
- **A long title.** As qa, add a title far too wide for its button after `Entry4` in `~/.config/streamflex/config.ini` (or the system copy, if the user file does not exist yet): `Entry8=Extraordinarily Wide Title WWWWWWWWWWWWWWWWWWWWWW;apps;gnome-text-editor`.
- **Inputs:** launch. Main shows six buttons, the last one *Café*. **Frame** `U7-<pass>-longtitle.png`: the long title is cut to fit, and no letter is drawn past its button's edges or into a neighbour (zoom on both edges); FAIL otherwise. Then move Right onto *Café* (**Frame** `U7-<pass>-cafe.png`), and OK.
- **Expected:**
  - An editor window opens `café.txt`, showing `StreamFlex café test`. **Frame** `U7-<pass>-editor.png`.
  - New lines include `Application detected`, and no `Could not` line.
  - `pgrep -a -u qa -f 'Été/café.txt'` shows the editor, with the accents intact.
- **Then:**
  1. Close that editor window, by the PID `pgrep` printed; never by name.
  2. StreamFlex takes focus back: new lines `Gained keyboard focus` and `Application finished`. Keys go to whichever window has focus, so **do not send `esc` until `Gained keyboard focus` is logged**. If it never comes, record it; only then bring StreamFlex forward (for example with the app menu), and note how.
  3. Key `esc`: `exit-code` is `0`. Rename `last.log` to `<pass>-run3.log`.
- In pass A the editor must open under Wayland from StreamFlex's own launch, and in pass B the same.

## U7b. Nothing reacts behind an application (SDL ignores the pad while unfocused)

- **Inputs:** launch, then OK on *Café* (U7), so the editor is in front. Wait for `Application detected` and for the editor to have focus. Take a mark. Send the pad `ButtonDPadRight`, `ButtonDPadDown`, `ButtonA`, `ButtonB` and `ButtonStart` (`send "press <name>"`), 500 ms apart. In pass B, do the same with the pad too: this is about the pad, not the keyboard.
- **Expected:**
  - No `Gamepad Button… detected` line and no `Settings opened` in the new lines. The editor's text is unchanged.
  - Close the editor by PID. New lines hold `Gained keyboard focus` and `Application finished`, and StreamFlex is on the same button as before, with no settings open (**Frame** `U7b-<pass>-after.png`).
  - Any pad line while the editor was in front is a **FAIL**; record which buttons.

## U9. A clean exit (item 9)

- Every run in the pass ended with `exit-code` `0`, and a log ending in `Quitting program`.
- `~/sf-logs/stderr.txt` of each run holds no error lines other than U5c's expected `Couldn't save …: permission denied`. Lines from the guest's graphics stack are not StreamFlex's and do not count: under `SDL_VIDEODRIVER=wayland` (pass A) Mesa and EGL print loader lines at every start, such as `MESA: error: ZINK: failed to choose pdev` and `libEGL warning: …`, on a virtio GPU with no 3D. Record them, but judge only StreamFlex's own lines.
- **At the end of pass B only**, with StreamFlex running (launch it once more): as root, `systemctl stop sf-vgp`. Expected:
  - `journalctl -u sf-vgp` ends with `stopped by SIGTERM` and `destroyed the device`;
  - `grep -c 'X-Box 360 pad' /proc/bus/input/devices` prints `0`;
  - StreamFlex logs `Gamepad disconnected` and keeps running (**Frame** `U9-unplugged.png`).

  Then key `esc`: `exit-code` is `0`.
- **PASS** when all of the above hold.

## What to send back

- The U0 `facts` output, and the `setup` output.
- A table of items 1 to 9 (with U5c and U7b), each marked PASS, FAIL, BLOCKED or N/A for pass A and for pass B, with a one-line reason for anything that is not PASS. It also records:
  - the video driver SDL chose by default (U2), and the renderer from the `Video:` line (U3);
  - which HMP key gave `#40000065` and which gave `#40000076` (U4);
  - whether the desktop showed through in U8.
- Every frame; the `~/sf-logs` of both passes; `baseline-config.ini`; the final `~/.config/streamflex/config.ini`; and `journalctl -u sf-vgp`.
