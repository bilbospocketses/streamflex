# StreamFlex 3b hands-on check: Windows 11

This is the Windows half of the hands-on check for sub-project 3b: every setting on the settings screen, the colour, font and command pickers, key and gamepad bindings, and the *Restart now?* question. It is run in a Windows 11 guest, by hand or by a QA harness, against the PR's CI **Windows build** zip, after the 3a check (`windows-hands-on.md`). The Ubuntu half is `ubuntu-3b-hands-on.md`; run this one first.

## How to read and run each step

The conventions of `windows-hands-on.md`, under its *How to read and run each step*, apply here unchanged: the inputs (*Key* is a SendInput key, *Pad* a button on the ViGEm Xbox 360 pad, held 150 ms, with 500 ms between inputs), the frames (here named `B<step>-<what>.png`), the log and its marks ("**New lines**" are the lines after the mark), and the verdicts (PASS, FAIL, BLOCKED). Paths use `C:\StreamFlex` for the unzipped build.

These steps also use the function keys and the Menu key:

| Name here | VK | Notes |
|---|---|---|
| F5, F6, F7, F9, F12 | `VK_F5` (0x74), `VK_F6` (0x75), `VK_F7` (0x76), `VK_F9` (0x78), `VK_F12` (0x7B) | |
| Menu | `VK_APPS` (0x5D), scan code 0x5D, `KEYEVENTF_EXTENDEDKEY` | **Not `VK_MENU`: that is the Alt key** |

Two things differ from 3a:
- **Every start is `streamflex.exe -d`.** The capture, binding, restart and mappings lines these steps look for are debug-level, so without `-d` they are absent.
- **A restart does not empty the log.** StreamFlex's own restart (B13, B14) carries on the same `streamflex.log`, so take a mark before it as for any other action. A start by hand still empties it: copy it to `logs\B<step>.log` before you start StreamFlex again.

Moving around the settings screen: Up and Down move between rows, Left and Right change a value, OK (Return, or pad **A**) opens a row marked ›, and Back (Backspace, or pad **B**) goes back a page; Back on the first page saves and closes. The first page's rows are, in order: *General*, *Background*, *Menus*, *Titles*, *Highlight*, *Scroll indicators*, *Clock*, *Screensaver*, *Controls*, a divider, then *Discard changes*. Settings open with the cursor on *General*. "**Controls › Keyboard**" means: from the first page, Down to *Controls*, OK, then OK on *Keyboard*. A row written *F5 · Quit StreamFlex* has the label *F5* on its left and the value *Quit StreamFlex* on its right. *The file* is `C:\StreamFlex\config.ini`.

## B0. Setup and record (no verdict)

1. **Record the guest:** the Windows build (`[Environment]::OSVersion.Version`), the screen resolution and scale, and the GPU (`Get-CimInstance Win32_VideoController | Select Name, DriverVersion`).
2. **Unzip the build** to `C:\StreamFlex`, as in 3a's W0 step 2. Check that `streamflex.exe`, `config.ini` and the `assets` folder are there.
3. **Add a Notepad tile** to `[Main]`, directly after its `Entry4=System;settings;:submenu System` line, for the exit hotkey steps:
   ```powershell
   $cfg = 'C:\StreamFlex\config.ini'
   $text = [IO.File]::ReadAllText($cfg)
   $new = [regex]::Replace($text, '(?m)^(Entry4=System;settings;:submenu System)(\r?\n)', "`$1`$2Entry5=Notepad;apps;notepad.exe`$2")
   if ($new -eq $text) { throw 'Entry4 of [Main] not found' }
   [IO.File]::WriteAllText($cfg, $new, [Text.UTF8Encoding]::new($false))
   Copy-Item $cfg C:\StreamFlex-3b-baseline.ini     # "the baseline" that B7 compares with
   ```
   **Every later "the baseline" means `C:\StreamFlex-3b-baseline.ini`.**
4. **Make two mappings files** for B13 and B14. Any text file will do; these hold one comment line, so SDL adds no mapping from them:
   ```powershell
   Set-Content -LiteralPath C:\StreamFlex\pad-a.txt -Value '# StreamFlex hands-on: mappings file A'
   Set-Content -LiteralPath C:\StreamFlex\pad-b.txt -Value '# StreamFlex hands-on: mappings file B'
   ```
5. **Install a font for this user only**, for B12. Make it on any machine with Python 3, from a checkout of the PR's branch: it is the bundled Open Sans with its family renamed to *User Sans*.
   ```powershell
   python tests/headless/make_fonts.py rename assets/fonts/OpenSans-Regular.ttf UserSans.ttf "Open Sans" "User Sans"
   ```
   Copy `UserSans.ttf` into the guest, right-click it and choose **Install** (not *Install for all users*). Record its registry value:
   ```powershell
   Get-ItemProperty 'HKCU:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts' | Select-Object 'User Sans*'
   ```
   It names the full path of the installed file, under `C:\Users\<user>\AppData\Local\Microsoft\Windows\Fonts\`. If Windows refuses the font, install any other font file that the guest does not have, for this user only, and use its family name in B12 instead.
6. **Connect the ViGEm Xbox 360 pad before StreamFlex starts.**
7. **Start `C:\StreamFlex\streamflex.exe -d`** from `C:\StreamFlex`. **Frame** `B0-home.png`. Expected in the log: `Video: SDL's windows driver, the <renderer> renderer` (record the renderer), `Gamepad connected with device index 0, instance id <N>`, then `Gamepad opened at device index 0, instance id <N>`, `Loading menu 'Main'`, and `Gained keyboard focus`. Main has five buttons: Kodi, Plex, Steam, System and Notepad (the row slides along to show Notepad). If the gamepad lines are missing, stop: the pad is not reaching SDL, which is a setup fault, not a verdict.

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
  - The save logs `Settings saved 1 change(s) to C:\StreamFlex\config.ini (backup: C:\StreamFlex\config.ini.bak)`.
  - The file's `[Hotkeys]` gains `Hotkey2=#4000003E;:quit`, and nothing else in the file changes:
    ```powershell
    Compare-Object (Get-Content C:\StreamFlex-3b-baseline.ini -Encoding UTF8) (Get-Content C:\StreamFlex\config.ini -Encoding UTF8)
    ```
    lists that one line, and only it.
  - After a restart, F5 quits. This is checked in B7.

## B2. Rebind a pad button

- **Inputs:**
  1. Pad **Start** (settings open). **Controls › Gamepad**, with the pad: D-pad Down to *Controls*, **A**, D-pad Down to *Gamepad*, **A**. The page has *On*, *Device*, *Mappings file*, a note, *Add binding*, the shipped controls, and a note on the built-in buttons. **Frame** `B2-gamepad.png`.
  2. D-pad Down to *Add binding*, **A**. On the binding's page, **A** on *Key*, then pad **Y** within 5 s.
  3. *Captured: ButtonY*. **A** on *Keep*.
  4. D-pad Down to *Command*, **A**, D-pad Down to *Home* (the eighth row), **A**.
  5. **B** three times, to save and close.
- **Expected:**
  - New lines hold `Settings: capture got ButtonY`. The Gamepad page lists *ButtonY · Home*.
  - The file's `[Gamepad]` gains `ButtonY=:home`. Compared with the baseline, the file differs in that line and B1's, and nothing else.
  - After a restart, Y goes home. This is checked in B7.

## B3. The safety floor

- **Inputs, part 1:** key **Menu**, **Controls › Keyboard**, OK on *Add binding*, OK on *Key*, then key **Backspace** within 5 s.
- **Expected:** the capture ends with nothing kept. The binding's page is back, with *Key: Choose…*, and the note under the preview says *The arrows, OK and Back keep their own meaning, so a hotkey on them would never run*. Backspace did not go back a page. **Frame** `B3-backspace.png`.
- **Inputs, part 2:** on the same page, OK on *Key*, then key **Up**. *Captured: Up*: OK on *Keep*. Down to *Command*, OK, Down to *Quit StreamFlex*, OK.
- **Expected:** refused. The page stays on the binding with *Key: Up* and *Command: None*, and the note says *That would leave no key for Up*. Nothing goes up but the Up arrow in the shipped config, so the floor keeps it. **Frame** `B3-up.png`. Down to *Cancel*, OK: the Keyboard page lists no Up binding.
- Then Back until settings close. New line: `Settings: nothing changed`.

## B4. Pick a system font

- **Inputs:**
  1. Key **Menu**, Down to *Titles*, OK, Down twice to *Font* (it reads *OpenSans-Regular.ttf*), OK.
  2. The picker shows *Loading fonts… (N)*, N counting up, then the list. **Frame** `B4-loading.png` while it loads, if you can catch it.
- **Expected, the list:**
  - One row per family, each drawn in its own face. The bundled fonts come first (DejaVu Sans, FreeSans, Inter, Noto Sans, Open Sans, Roboto, Source Sans Pro), then the system's by name. **Frame** `B4-list.png`.
  - The system fonts are there: *Arial* and *Segoe UI* among them.
  - **The cursor starts on *Open Sans*,** the bundled default the titles use, and not on a *Custom:* row. There is no *Custom:* row at the top.
  - New lines: `Fonts: found <N> families in <M> files, skipped <K> (<ms> ms)`.
- **Inputs:** 3. Move to *Segoe UI* (Down, or Right to page down), then OK.
- **Expected:**
  - The preview's titles change to Segoe UI at once. **Frame** `B4`.
  - New lines: `Settings: [Titles] Font ...OpenSans-Regular.ttf -> C:\WINDOWS\Fonts\segoeui.ttf` (the Windows folder in the case Windows gives it, `C:\WINDOWS` or `C:\Windows`).
- Then Back, Back to close. The file's `[Titles]` reads `Font=C:\WINDOWS\Fonts\segoeui.ttf`, with no `FontFace` line, since Segoe UI's Regular is a file of its own.

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

- **Inputs:** key **Esc** (StreamFlex quits; copy the log to `logs\B1-B6.log`). Start `streamflex.exe -d` again. **Frame** `B7-restart.png`.
- **Expected:**
  - Every change from B1 to B6 is in force: the titles are Segoe UI, gold; the clock shows with the date.
  - Pad: D-pad Right to *System* (the fourth button), **A**, then **Y**: Main shows again (`Gamepad ButtonY detected`, then `Loading menu 'Main'`).
  - The file differs from the baseline in exactly these lines, and in nothing else:
    ```powershell
    Compare-Object (Get-Content C:\StreamFlex-3b-baseline.ini -Encoding UTF8) (Get-Content C:\StreamFlex\config.ini -Encoding UTF8)
    ```
    - `[Titles]`: the `Font=` line, now `C:\WINDOWS\Fonts\segoeui.ttf`, and `Color=#FFFFFF`, now `Color=#FFD700`;
    - `[Clock]`: `Enabled=false` and `ShowDate=false`, now `true`;
    - `[Hotkeys]`: a new `Hotkey2=#4000003E;:quit`;
    - `[Gamepad]`: a new `ButtonY=:home`.
  - Every comment line is still there, unchanged, and every line is in its old order; the new lines come at the end of their sections.
  - Key **F5**: StreamFlex quits (`Key F5 (#4000003E) detected`, then `Quitting program`). Copy the log to `logs\B7.log`.

## B8 (Windows only). The exit hotkey

- **Inputs:**
  1. Start `streamflex.exe -d`. Key **Menu**, **Controls › Keyboard**, OK on *Add binding*, OK on *Key*, key **F9**, *Keep*.
  2. Down to *Command*, OK. The command picker has *Close the app on show* (the `:exit` command), after the *Open submenu:* rows. **Frame** `B8-commands.png`. Choose it.
  3. Back, Back, Back to save and close.
  4. D-pad Right (or the Right key) to *Notepad*, OK. Wait for Notepad to be in front. Take a mark. Key **F9**.
- **Expected:**
  - The Keyboard page listed *F9 · Close the app on show*. The file's `[Hotkeys]` gains `Hotkey3=#40000042;:exit`.
  - At step 4, Notepad closes, with no restart in between. StreamFlex comes back: new lines `Application finished`, `Gained keyboard focus`.
- **Inputs, F12:** key **Menu**, **Controls › Keyboard**, *Add binding*, *Key*, key **F12**, *Keep*, *Command*, *Close the app on show*.
- **Expected:** refused. The note says *The exit hotkey must be F1 to F24, but not F12*, and the page keeps *Command: None*. **Frame** `B8-f12.png`. Down to *Cancel*, OK, then Back until settings close (`Settings: nothing changed`).

## B9. VSync live

- **Inputs:** take a mark. Key **Menu**, OK on *General*. Down to *VSync* (the sixth row), Left (Off). Down to *FPS limit*, which is no longer greyed, and Right (30).
- **Expected:**
  - New lines: `Settings: [General] VSync true -> false`, then `Frame timing: FPS limit 30, 33 ms a frame`, with no restart. **Frame** `B9`.
  - Record the renderer's own VSync support: the `Video:` line from B0, and any `Frame timing: VSync wanted ..., the renderer gives ...` line, and whether an error line `The renderer refused VSync` or `The renderer would not turn VSync off` appears.
- Then Back, Back to save and close. The file has `VSync=false` and a new `FPSLimit=30` in `[General]`.

## B10 (Windows only). The exit hotkey after a change on the Keyboard page

The exit hotkey is registered again whenever the bindings change, with no restart: Windows takes the first `:exit` binding on a key it can register (F1 to F11, F13 to F24).

- **Inputs, part 1:** key **Menu**, **Controls › Keyboard**, *Add binding*, *Key*, key **F6**, *Keep*, *Command*, *Close the app on show*. Back three times to save and close. F9's `:exit` (B8) comes before F6's in the file.
- Launch *Notepad*, wait for it to be in front, then key **F6**, wait 2 s, then key **F9**.
- **Expected:** F6 does nothing (Notepad stays); F9 closes Notepad. Only the first `:exit` is the exit hotkey. **Frame** `B10-f6.png` after F6.
- **Inputs, part 2:** key **Menu**, **Controls › Keyboard**, OK on the *F9 · Close the app on show* row, Down twice to *Remove*, OK. Back until settings save and close. Do not restart.
- Launch *Notepad*, then key **F9**, wait 2 s, then key **F6**.
- **Expected:** F9 does nothing now; F6 closes Notepad. The file's F9 line is gone and the F6 line stays.

## B11 (Windows only). The Menu key's name

On the harness's Linux SDLs the context-menu key is named *Menu* already, so only Windows shows whether StreamFlex names it: SDL 2.32 on Windows calls it *Application*.

- **Inputs:** take a mark. Key **Menu** (settings open), **Controls › Keyboard**, *Add binding*, OK on *Key*, then key **Menu** within 5 s.
- **Expected:** the page shows *Captured: Menu*, not *Captured: Application*. New lines hold `Settings: capture got Menu (#40000065)`. **Frame** `B11-captured.png`.
- **Inputs:** OK on *Keep*, *Command*, *Home*. Then press nothing for 12 s.
- **Expected:**
  - The Keyboard page lists *Menu · Home*, and the note under the preview reads *Press Menu again within 10 s to keep it*, counting down. **Frame** `B11-confirm.png`.
  - New lines: `Settings: press Menu again within 10 s to keep it`, then, 10 s later, `Settings: the binding went back: Menu was not pressed again within 10 s`. The *Menu · Home* row is gone.
- Then Back until settings close (`Settings: nothing changed`).

## B12 (Windows only). The Windows fonts

- **Inputs, part 1:** key **Menu**, *Titles*, *Font*, OK. The cursor starts on *Segoe UI*, the font B4 chose. Move to *Arial*, OK. Back, Back to save and close.
- **Expected:** the titles turn to Arial at once. The file's `[Titles]` reads `Font=C:\WINDOWS\Fonts\arial.ttf` (the Windows folder in the case Windows gives it), and there is **no** `FontFace` line.
- **Inputs, part 2:** key **Menu**, *Titles*, *Font*, OK. Find *User Sans*, the font B0 installed for this user only. **Frame** `B12-usersans.png`. OK. Back, Back to save and close.
- **Expected:** *User Sans* is in the list, drawn in its own face. The file's `Font=` names the full path from B0's registry value, under `C:\Users\<user>\AppData\Local\Microsoft\Windows\Fonts\`.

## B13. Restart now?

- **Setup:** key **Esc** to quit (copy the log to `logs\B8-B12.log`). Add a startup and a quit command that leave a trace, to `[General]` of the file:
  ```powershell
  $cfg = 'C:\StreamFlex\config.ini'
  $text = [IO.File]::ReadAllText($cfg)
  $nl = if ($text -match "`r`n") { "`r`n" } else { "`n" }   # the file's own line ending
  $marks = 'StartupCmd=:fork cmd.exe /c "echo started>>C:\StreamFlex\marks.txt"' + $nl + 'QuitCmd=:fork cmd.exe /c "echo quit>>C:\StreamFlex\marks.txt"' + $nl
  $new = [regex]::Replace($text, '(?m)^\[General\]\r?\n', "`$0$marks")
  if ($new -eq $text) { throw '[General] not found' }
  [IO.File]::WriteAllText($cfg, $new, [Text.UTF8Encoding]::new($false))
  Remove-Item C:\StreamFlex\marks.txt -ErrorAction SilentlyContinue
  ```
  Start `streamflex.exe -d`. Wait 2 s: `marks.txt` holds one `started`.
- **Inputs:**
  1. Take a mark. Key **Menu**, **Controls › Gamepad**, Down to *Mappings file* (*Choose…*), OK. The browser lists every file in a folder, not only images. Go to `C:\StreamFlex` and choose `pad-a.txt`. The note under the preview says *This applies at next start*.
  2. Back, Back, Back. Settings ask *Restart StreamFlex now to apply the mappings file?*, with *Yes* under the cursor and *No* below. **Frame** `B13-question.png`.
  3. OK (*Yes*). StreamFlex closes its window and starts again.
- **Expected:**
  - New lines, in the same log: `Settings saved 1 change(s)`, `Settings: asking to restart StreamFlex to apply the mappings file`, `Restarting StreamFlex to apply the mappings file`, then the fresh copy's start: `Restarted, so the StartupCmd does not run again` and `Gamepad mappings loaded from C:\StreamFlex\pad-a.txt (<N> added)` (N is 0: the file holds no mapping). **Frame** `B13-restarted.png`.
  - `marks.txt` still holds one `started` and no `quit`: the restart ran no QuitCmd, and the fresh copy no StartupCmd.
  - **The exit hotkey works in the fresh copy:** launch *Notepad*, key **F6** (B10's exit hotkey): Notepad closes.
- **Inputs, a second restart in a row:** key **Menu**, **Controls › Gamepad**, *Mappings file*, choose `pad-b.txt`, Back three times, *Yes*. Then:
  ```powershell
  (Get-CimInstance Win32_Process -Filter "Name='streamflex.exe'").CommandLine
  ```
- **Expected:** one `streamflex.exe` runs, and its command line ends `-d --restarted`: `--restarted` once, not twice. The log holds `Gamepad mappings loaded from C:\StreamFlex\pad-b.txt`. `marks.txt` still holds one `started` and no `quit`.
- Then key **Esc**: `marks.txt` now holds `started` then `quit` (a quit runs the QuitCmd). Copy the log to `logs\B13.log`.

## B14. The restart question with the gamepad

- **Inputs:** start `streamflex.exe -d`. With the pad alone: **Start**, **Controls › Gamepad** (D-pad Down to *Controls*, **A**, D-pad Down to *Gamepad*, **A**), D-pad Down to *Mappings file*, **A**, choose `pad-a.txt` with the D-pad and **A**, then **B** three times.
- **Expected:** the question opens with *Yes* under the cursor. **Frame** `B14-yes.png`.
- **Inputs:** D-pad Down (to *No*), **A**.
- **Expected:** settings close and StreamFlex does not restart. New line: `Settings: no restart now, so the mappings file waits for the next start`. No `Restarting StreamFlex` line.
- **Inputs:** **Start**, the same way to *Mappings file*, choose `pad-b.txt`, **B** three times; then on the question, pad **B**.
- **Expected:** Back is *No*: the same `no restart now` line, and no restart.
- **Inputs:** once more, with `pad-a.txt`; on the question, pad **A** at once.
- **Expected:** Yes restarts StreamFlex: `Restarting StreamFlex to apply the mappings file`, and the fresh copy loads `pad-a.txt`.
- Then key **Esc**, and copy the log to `logs\B14.log`.

## Results

| Step | Verdict | Notes |
|---|---|---|
| B0 Setup (no verdict) | | Windows build, resolution and scale, GPU, renderer, User Sans registry value |
| B1 Rebind a key | | |
| B2 Rebind a pad button | | |
| B3 The safety floor | | |
| B4 Pick a system font | | |
| B5 A colour by hex | | |
| B6 Turn the clock on | | |
| B7 Restart and compare | | |
| B8 The exit hotkey | | |
| B9 VSync live | | the renderer's VSync support |
| B10 The exit hotkey after a change | | |
| B11 The Menu key's name | | |
| B12 The Windows fonts | | |
| B13 Restart now? | | |
| B14 The restart question with the gamepad | | |

Send back the table, every frame, the `logs\` folder, `marks.txt`, `C:\StreamFlex-3b-baseline.ini`, and the final `config.ini` and `config.ini.bak`. Give a one-line reason for anything that is not PASS.
