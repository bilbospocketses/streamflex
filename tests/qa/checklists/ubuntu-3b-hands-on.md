# StreamFlex 3b hands-on check: Ubuntu 26.04 GNOME/Wayland

This is the Linux half of the hands-on check for sub-project 3b: every setting on the settings screen, the colour, font and command pickers, key and gamepad bindings, and the *Restart now?* question. It is run in the Ubuntu 26.04 guest of the 3a check (`ubuntu-hands-on.md`), by hand or by a QA harness, against the PR's CI **Debian build** `.deb`, after the Windows half (`windows-3b-hands-on.md`). The steps are the Windows ones, numbered the same; B8, B10, B11 and B12 are Windows only, and are not run here.

## How to read and run each step

The conventions of `ubuntu-hands-on.md`, under its *How to read and run each step*, apply here unchanged: the keys (QEMU HMP `sendkey` names), the pad (`virtual-gamepad.py`, served once and driven with `send`), the frames (here named `B<step>-<what>.png`), the log and its marks ("**New lines**" are the lines after the mark), the verdicts (PASS, FAIL, BLOCKED), and launching a run from the app menu's *StreamFlex Debug* entry, which runs `/home/qa/sf-run.sh` and so `streamflex -d`.

These steps also use these keys:

| Name here | HMP key |
|---|---|
| F5 | `f5` |
| Menu (the PC context-menu key, `#40000065`) | `compose` |

Three things differ from 3a:
- **One pass, not two.** Run every step once, under the driver SDL chooses by itself: before B0's start, as root, `bash /root/sf-qa/ubuntu-guest.sh reset-pass default`. Drive the settings screen with the keyboard, and use the pad where a step says *Pad*.
- **Every run uses `-d`** (`sf-run.sh` gives it). The capture, binding, restart and mappings lines these steps look for are debug-level, so without `-d` they are absent.
- **A restart does not empty the log.** StreamFlex's own restart (B13, B14) carries on the same `streamflex.log`, in the same process, so take a mark before it as for any other action. A start by hand still empties it: rename `~/sf-logs/last.log` before you start StreamFlex again.

Moving around the settings screen: Up and Down move between rows, Left and Right change a value, OK (`ret`, or pad `ButtonA`) opens a row marked ›, and Back (`backspace`, or pad `ButtonB`) goes back a page; Back on the first page saves and closes. The first page's rows are, in order: *General*, *Background*, *Menus*, *Titles*, *Highlight*, *Scroll indicators*, *Clock*, *Screensaver*, *Controls*, a divider, then *Discard changes*. Settings open with the cursor on *General*. "**Controls › Keyboard**" means: from the first page, Down to *Controls*, OK, then OK on *Keyboard*. A row written *F5 · Quit StreamFlex* has the label *F5* on its left and the value *Quit StreamFlex* on its right.

The config file: the first save writes your own copy, `~/.config/streamflex/config.ini` (below, *the file*), which StreamFlex reads from then on.

## B0. Setup and record (no verdict)

1. **The guest and the package** are 3a's: U0 (`facts` and `setup`, recorded again) and U1 (the `.deb` of this PR) of `ubuntu-hands-on.md`. Record the `facts` output: the GPU, the resolution, the session.
2. As root: `bash /root/sf-qa/ubuntu-guest.sh reset-pass default`. It removes `~/.config/streamflex`, so StreamFlex starts from the packaged config, and checks it still equals `/home/qa/baseline-config.ini`, **the baseline** every comparison below uses.
3. **Make two mappings files** for B13 and B14, as `qa`. Any text file will do; these hold one comment line, so SDL adds no mapping from them:
   ```sh
   echo '# StreamFlex hands-on: mappings file A' > /home/qa/pad-a.txt
   echo '# StreamFlex hands-on: mappings file B' > /home/qa/pad-b.txt
   ```
4. **Start the pad** before any StreamFlex run, as in 3a's U0 step 6 (`systemd-run --unit=sf-vgp ... serve --keep-open 14400`, then `pad-node`).
5. **Launch *StreamFlex Debug*.** **Frame** `B0-home.png`. Expected in the log: `Config file found: /usr/share/streamflex/config.ini`, `Video: SDL's <driver> driver, the <renderer> renderer` (record both), `Gamepad connected with device index 0, instance id <N>`, then `Gamepad opened at device index 0, instance id <N>`, `Loading menu 'Main'`, and `Gained keyboard focus`. If the gamepad lines are missing, stop: the pad is not reaching SDL, which is a setup fault, not a verdict.

## B1. Rebind a key

- **Inputs:**
  1. Key **Menu** (settings open). **Controls › Keyboard**. The page lists *Add binding*, then *Escape · Quit StreamFlex* (the shipped `Hotkey1`), then a note that the arrows, OK and Back keep their own meaning. **Frame** `B1-keyboard.png`.
  2. OK on *Add binding*. The binding's page has *Key: Choose…*, *Command: None* and *Cancel*.
  3. Take a mark. OK on *Key*: the page says *Press the key or button…*, and the note under the preview counts down from 5 s. Key **F5**.
  4. The page shows *Captured: F5*, with *Keep*, *Try again* and *Cancel*. OK on *Keep*.
  5. Down to *Command*, then OK. The command picker opens on *None*. Down to *Quit StreamFlex* (the tenth row), then OK.
  6. Back, Back, Back: to Controls, to the first page, then save and close.
- **Expected:**
  - The new lines hold `Settings: capture got F5 (#4000003E)`.
  - After step 5 the Keyboard page lists *F5 · Quit StreamFlex* under the Escape row. **Frame** `B1-bound.png`.
  - The save logs `Settings saved 1 change(s) to /home/qa/.config/streamflex/config.ini`, the first save making your own copy.
  - The file's `[Hotkeys]` gains `Hotkey2=#4000003E;:quit`, and nothing else in it differs from the baseline: `diff /home/qa/baseline-config.ini ~/.config/streamflex/config.ini` shows that one added line.
  - After a restart, F5 quits. This is checked in B7.

## B2. Rebind a pad button

- **Inputs:**
  1. Pad `ButtonStart` (settings open). **Controls › Gamepad**, with the pad: `ButtonDPadDown` to *Controls*, `ButtonA`, `ButtonDPadDown` to *Gamepad*, `ButtonA`. The page has *On*, *Device*, *Mappings file*, a note, *Add binding*, the shipped controls, and a note on the built-in buttons. **Frame** `B2-gamepad.png`.
  2. `ButtonDPadDown` to *Add binding*, `ButtonA`. On the binding's page, `ButtonA` on *Key*, then `ButtonY` within 5 s.
  3. *Captured: ButtonY*. `ButtonA` on *Keep*.
  4. `ButtonDPadDown` to *Command*, `ButtonA`, `ButtonDPadDown` to *Home* (the eighth row), `ButtonA`.
  5. `ButtonB` three times, to save and close.
- **Expected:**
  - New lines hold `Settings: capture got ButtonY`. The Gamepad page lists *ButtonY · Home*.
  - The file's `[Gamepad]` gains `ButtonY=:home`. Against the baseline, the file differs in that line and B1's, and nothing else.
  - After a restart, Y goes home. This is checked in B7.

## B3. The safety floor

- **Inputs, part 1:** key **Menu**, **Controls › Keyboard**, OK on *Add binding*, OK on *Key*, then key `backspace` within 5 s.
- **Expected:** the capture ends with nothing kept. The binding's page is back, with *Key: Choose…*, and the note under the preview says *The arrows, OK and Back keep their own meaning, so a hotkey on them would never run*. Backspace did not go back a page. **Frame** `B3-backspace.png`.
- **Inputs, part 2:** on the same page, OK on *Key*, then key `up`. *Captured: Up*: OK on *Keep*. Down to *Command*, OK, Down to *Quit StreamFlex*, OK.
- **Expected:** refused. The page stays on the binding with *Key: Up* and *Command: None*, and the note says *That would leave no key for Up*. Nothing goes up but the Up arrow in the shipped config, so the floor keeps it. **Frame** `B3-up.png`. Down to *Cancel*, OK: the Keyboard page lists no Up binding.
- Then Back until settings close. New line: `Settings: nothing changed`.

## B4. Pick a system font

- **Inputs:**
  1. Key **Menu**, Down to *Titles*, OK, Down twice to *Font* (it reads *OpenSans-Regular.ttf*), OK.
  2. The picker shows *Loading fonts… (N)*, N counting up, then the list. **Frame** `B4-loading.png` while it loads, if you can catch it.
- **Expected, the list:**
  - One row per family, each drawn in its own face. The bundled fonts come first (DejaVu Sans, FreeSans, Inter, Noto Sans, Open Sans, Roboto, Source Sans Pro), then the system's by name, *DejaVu Serif* and *Ubuntu* among them. **Frame** `B4-list.png`.
  - The cursor starts on *Open Sans*, the bundled default the titles use, and not on a *Custom:* row.
  - New lines: `Fonts: found <N> families in <M> files, skipped <K> (<ms> ms)`.
- **Inputs:** 3. Move to *DejaVu Serif* (or *Ubuntu*; Down, or Right to page down), then OK.
- **Expected:**
  - The preview's titles change to that font at once. **Frame** `B4`.
  - New lines: `Settings: [Titles] Font ...OpenSans-Regular.ttf -> /usr/share/fonts/...` naming the system file (for DejaVu Serif, `/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf`).
- Then Back, Back to close. The file's `[Titles]` reads `Font=` with that path, and has a `FontFace=` line only if the face chosen is a collection's later face (record which).

## B5. A colour by hex

- **Inputs:**
  1. Key **Menu**, Down to *Titles*, OK, Down to *Colour* (it reads *White*), OK. The colour picker shows 24 swatches, six across, with a *Custom #FFFFFF* row below; the cursor is on White, which is marked.
  2. Down until the cursor is on the *Custom* row, then OK. The hex editor opens on `#FFFFFF`, its first digit boxed.
  3. Type `FFD700`: Right twice (to the third digit), Down twice (F to D); Right, Down 8 times (F to 7); Right, Up once (F wraps to 0); Right, Up once. The editor reads `#FFD700`. OK keeps it.
  4. OK on *Colour* again. Move the cursor to Black, the first swatch, and stop there.
  5. OK, to choose Black.
- **Expected:**
  - After step 3, the titles turn gold in the preview, and the *Colour* row reads *Custom #FFD700*. **Frame** `B5-gold.png`.
  - At step 4, the preview's titles are black, and the note under the preview reads *Black #000000 · Low contrast: 1.0:1 against the background; 3:1 or more reads well*. **Frame** `B5-black.png`.
  - After step 5, with the cursor on the *Colour* row (*Black*), the note under the preview gives the same warning, without the name.
- **Inputs:** 6. Put the gold back: OK on *Colour*, Down to *Custom* (the hex editor now starts at `#000000`), OK, and type `FFD700`: Down once (0 to F), Right, Down once, Right, Down 3 times (0 to D), Right, Up 7 times (0 to 7). OK keeps it. 7. Back, Back to save and close.
- **Expected:** the titles are gold again, the warning is gone, and they stay gold once settings close. The file's `[Titles]` reads `Color=#FFD700`.

## B6. Turn the clock on

- **Inputs:** key **Menu**, Down to *Clock*, OK. Every row but *Show* is greyed; with the cursor on a greyed row the note says *The clock is off*. On *Show*, Right (On). Down to *Show date*, Right (On).
- **Expected:**
  - The clock appears in the preview at once, with the date, and the menu sits below it: no button or title overlaps the clock. New lines include `Layout area: from y <Y>`, with Y below the clock's date. **Frame** `B6`.
  - The other rows are no longer greyed, but *Shadow colour*, which waits for *Shadows*.
- Then Back, Back to save and close. The file's `[Clock]` reads `Enabled=true` and `ShowDate=true`.

## B7. Restart and compare

- **Inputs:** key `esc` (StreamFlex quits; rename `~/sf-logs/last.log` to `B1-B6.log`). Launch *StreamFlex Debug* again. **Frame** `B7-restart.png`.
- **Expected:**
  - `Config file found: /home/qa/.config/streamflex/config.ini`.
  - Every change from B1 to B6 is in force: the titles are in B4's font, gold; the clock shows with the date.
  - Pad: `ButtonDPadRight` to *System* (the fourth button), `ButtonA`, then `ButtonY`: Main shows again (`Gamepad ButtonY detected`, then `Loading menu 'Main'`).
  - The file differs from the baseline in exactly these lines, and in nothing else (`diff /home/qa/baseline-config.ini ~/.config/streamflex/config.ini`):
    - `[Titles]`: the `Font=` line, now B4's font, and `Color=#FFFFFF`, now `Color=#FFD700`;
    - `[Clock]`: `Enabled=false` and `ShowDate=false`, now `true`;
    - `[Hotkeys]`: a new `Hotkey2=#4000003E;:quit`;
    - `[Gamepad]`: a new `ButtonY=:home`.
  - Every comment line is still there, unchanged, and every line is in its old order; the new lines come at the end of their sections.
  - Key `f5`: StreamFlex quits (`Key F5 (#4000003E) detected`, then `Quitting program`; `exit-code` is `0`). Rename `last.log` to `B7.log`.

## B8 (Windows only). The exit hotkey

Not run on Linux: `:exit` is a Windows hotkey, and the command picker on Linux does not offer it. Mark it N/A.

## B9. VSync live

- **Inputs:** launch *StreamFlex Debug*. Take a mark. Key **Menu**, OK on *General*. Down to *VSync* (the sixth row), Left (Off). Down to *FPS limit*, which is no longer greyed, and Right (30).
- **Expected:**
  - New lines: `Settings: [General] VSync true -> false`, then `Frame timing: FPS limit 30, 33 ms a frame`, with no restart. **Frame** `B9`.
  - Record the renderer's own VSync support: the `Video:` line from B0, and any `Frame timing: VSync wanted ..., the renderer gives ...` line, and whether an error line `The renderer refused VSync` or `The renderer would not turn VSync off` appears.
- Then Back, Back to save and close. The file has `VSync=false` and a new `FPSLimit=30` in `[General]`.

## B10, B11 and B12 (Windows only)

The exit hotkey after a change on the Keyboard page, the Menu key's name, and the Windows fonts are checked on Windows alone. Mark them N/A.

## B13. Restart now?

- **Setup:** key `esc` to quit (rename `last.log` to `B9.log`). Add a startup and a quit command that leave a trace, as `qa`, to `[General]` of the file:
  ```sh
  sed -i -e '/^\[General\]$/a StartupCmd=:fork echo started >> /home/qa/marks.txt' \
         -e '/^\[General\]$/a QuitCmd=:fork echo quit >> /home/qa/marks.txt' ~/.config/streamflex/config.ini
  sed -n '/^\[General\]$/,/^$/p' ~/.config/streamflex/config.ini   # shows both new lines under [General]
  rm -f /home/qa/marks.txt
  ```
  Launch *StreamFlex Debug*. Wait 2 s: `/home/qa/marks.txt` holds one `started`. Record the process: `pgrep -u qa -x streamflex`.
- **Inputs:**
  1. Take a mark. Key **Menu**, **Controls › Gamepad**, Down to *Mappings file* (*Choose…*), OK. The browser lists every file in a folder, not only images. Go to `/home/qa` and choose `pad-a.txt`. The note under the preview says *This applies at next start*.
  2. Back, Back, Back. Settings ask *Restart StreamFlex now to apply the mappings file?*, with *Yes* under the cursor and *No* below. **Frame** `B13-question.png`.
  3. OK (*Yes*). StreamFlex starts again.
- **Expected:**
  - New lines, in the same log: `Settings saved 1 change(s)`, `Settings: asking to restart StreamFlex to apply the mappings file`, `Restarting StreamFlex to apply the mappings file`, then the fresh start: `Restarted, so the StartupCmd does not run again` and `Gamepad mappings loaded from /home/qa/pad-a.txt (<N> added)` (N is 0: the file holds no mapping). **Frame** `B13-restarted.png`.
  - `pgrep -u qa -x streamflex` gives the same process id as before: the new start took the old one's place.
  - `marks.txt` still holds one `started` and no `quit`: the restart ran no QuitCmd, and the fresh start no StartupCmd.
- **Inputs, a second restart in a row:** key **Menu**, **Controls › Gamepad**, *Mappings file*, choose `pad-b.txt`, Back three times, *Yes*. Then:
  ```sh
  tr '\0' ' ' < /proc/$(pgrep -u qa -x streamflex)/cmdline; echo
  ```
- **Expected:** the command line reads `/usr/bin/streamflex -d --restarted`: `--restarted` once, not twice. The log holds `Gamepad mappings loaded from /home/qa/pad-b.txt`. `marks.txt` still holds one `started` and no `quit`.
- Then key `esc`: `marks.txt` now holds `started` then `quit` (a quit runs the QuitCmd), and `exit-code` is `0`. Rename `last.log` to `B13.log`.

## B14. The restart question with the gamepad

- **Inputs:** launch *StreamFlex Debug*. With the pad alone: `ButtonStart`, **Controls › Gamepad** (`ButtonDPadDown` to *Controls*, `ButtonA`, `ButtonDPadDown` to *Gamepad*, `ButtonA`), `ButtonDPadDown` to *Mappings file*, `ButtonA`, choose `pad-a.txt` with the D-pad and `ButtonA`, then `ButtonB` three times.
- **Expected:** the question opens with *Yes* under the cursor. **Frame** `B14-yes.png`.
- **Inputs:** `ButtonDPadDown` (to *No*), `ButtonA`.
- **Expected:** settings close and StreamFlex does not restart. New line: `Settings: no restart now, so the mappings file waits for the next start`. No `Restarting StreamFlex` line.
- **Inputs:** `ButtonStart`, the same way to *Mappings file*, choose `pad-b.txt`, `ButtonB` three times; then on the question, `ButtonB`.
- **Expected:** Back is *No*: the same `no restart now` line, and no restart.
- **Inputs:** once more, with `pad-a.txt`; on the question, `ButtonA` at once.
- **Expected:** Yes restarts StreamFlex: `Restarting StreamFlex to apply the mappings file`, and the fresh start loads `pad-a.txt`.
- Then key `esc`, and rename `last.log` to `B14.log`.

## Results

| Step | Verdict | Notes |
|---|---|---|
| B0 Setup (no verdict) | | GPU, resolution, session, SDL driver and renderer |
| B1 Rebind a key | | |
| B2 Rebind a pad button | | |
| B3 The safety floor | | |
| B4 Pick a system font | | the font chosen, and any `FontFace` |
| B5 A colour by hex | | |
| B6 Turn the clock on | | |
| B7 Restart and compare | | |
| B8 The exit hotkey | N/A | Windows only |
| B9 VSync live | | the renderer's VSync support |
| B10 The exit hotkey after a change | N/A | Windows only |
| B11 The Menu key's name | N/A | Windows only |
| B12 The Windows fonts | N/A | Windows only |
| B13 Restart now? | | |
| B14 The restart question with the gamepad | | |

Send back the table, every frame, the `~/sf-logs` folder with the renamed logs, `marks.txt`, `baseline-config.ini`, the final `~/.config/streamflex/config.ini` and its `.bak`, and `journalctl -u sf-vgp`. Give a one-line reason for anything that is not PASS.
