# StreamFlex 3a hands-on check: Windows 11

This is the Windows half of the hands-on check for the settings screen of sub-project 3a. It is run in a Windows 11 guest, by hand or by a QA harness, against the PR's CI **Windows build** zip. It covers the nine hands-on steps of the 3a plan (each heading's "brief step"), and it adds checks for the fixes of 3a's final fix wave (Batch C):

- **Imp 1:** a held Start button, or a held Menu key, opens settings once.
- **Imp 2:** one image rule everywhere: upper-case extensions count, and hidden files do not.
- **The Menu key:** it arrives as `SDLK_APPLICATION`. On Windows it cannot arrive as `SDLK_MENU`, so that half is checked on Ubuntu.
- **Refusals and focus:** settings do not open while an application is launching or running (#116), and gamepad input is ignored while StreamFlex is not the focused window.
- **Display details:** the Menus list's preview follows the cursor only after it rests about 300 ms, and a title too long for its button is cut and never runs past it.
- **The zip carries no DLL:** it has no `vcruntime140.dll`, so a guest with no Visual C++ runtime starts StreamFlex.
- **The real transparent window**, which no headless check can show: with Mode set to Transparent, the actual window turns see-through, and turns solid again when Mode is set back.

## How to read and run each step

- **Inputs.** *Key* means a keyboard key sent with SendInput. *Pad* means a button on the ViGEm virtual Xbox 360 pad. A pad press is held for 150 ms unless the step says otherwise. StreamFlex samples the pad once a frame, so a shorter press can be missed. Leave at least 500 ms between inputs. The keys:

  | Name here | VK | Notes |
  |---|---|---|
  | Up, Down, Left, Right | `VK_UP` / `VK_DOWN` / `VK_LEFT` / `VK_RIGHT` | extended keys: set `KEYEVENTF_EXTENDEDKEY` |
  | Return | `VK_RETURN` (0x0D) | OK |
  | Backspace | `VK_BACK` (0x08) | Back |
  | Esc | `VK_ESCAPE` (0x1B) | the shipped config's `Hotkey1=#1B;:quit` |
  | Menu | `VK_APPS` (0x5D), scan code 0x5D, `KEYEVENTF_EXTENDEDKEY` | **Not `VK_MENU`: that is the Alt key** |

  The pad controls (StreamFlex's names): **A** = OK (`:select`), **B** = Back (`:back`), **Start** opens settings (`:settings`), and **D-pad Up/Down/Left/Right** move (`:up` and so on).
- **Frames.** Save a screenshot wherever a step says **Frame**, named `W<step>-<what>.png`. Keep them all.
- **The log.** Every run starts `streamflex.exe -d`, which writes `C:\StreamFlex\streamflex.log`. The file is emptied at each start, so copy it to `logs\W<step>.log` before any restart. To see only what an action logged, take a mark first:
  ```powershell
  $log = 'C:\StreamFlex\streamflex.log'
  $mark = @(Get-Content -LiteralPath $log -Encoding UTF8).Count
  # ... the action ...
  Get-Content -LiteralPath $log -Encoding UTF8 | Select-Object -Skip $mark
  ```
  "**New lines**" below means the lines after the mark.
- **Verdicts.** Each step is **PASS** only when every item under *Expected* holds. Otherwise it is **FAIL**; record which item failed, with its frame and log lines. **BLOCKED** means the harness could not deliver the input. Show this with the log: a key that arrives always logs `Key <name> (#<hex>) detected`, and a pad press logs `Gamepad <Button> detected`. BLOCKED is a harness finding, not a StreamFlex one.
- Paths below use `C:\StreamFlex` for the unzipped build. Where the build lives elsewhere, use that folder throughout.

## W0. Setup and record (no verdict)

1. **Record the guest:** the Windows build (`[Environment]::OSVersion.Version`), the screen resolution and scale, and the GPU (`Get-CimInstance Win32_VideoController | Select Name, DriverVersion`).
2. **List the zip, then unzip it** to `C:\StreamFlex`. Check that `streamflex.exe`, `config.ini` and the `assets` folder are there. The zip carries no DLL:
   ```powershell
   Add-Type -AssemblyName System.IO.Compression.FileSystem
   $z = [IO.Compression.ZipFile]::OpenRead('<the zip>'); $z.Entries.FullName; $z.Dispose()
   [IO.Compression.ZipFile]::OpenRead('<the zip>').Entries.FullName | Where-Object { $_ -match '\.dll$' }   # prints nothing
   ```
   Expected: no `vcruntime140.dll` and no other `.dll` in the list. Then, on a guest with **no** Visual C++ runtime installed (record `Get-Package *Visual C++*` and whether `vcruntime140.dll` is in System32; if the guest has one, say so, and the check is weaker), StreamFlex must start in step 6 with no "vcruntime140.dll was not found" dialog. **PASS** when the list is clean and step 6 starts. Do not install any VC++ redistributable to make it run.
3. **Check the gamepad is on out of the box.** The gamepad is on by default: the built-in default is `true` and the shipped config says `Enabled=true`. Change nothing, and record what the file holds:
   ```powershell
   $cfg = 'C:\StreamFlex\config.ini'
   Select-String -LiteralPath $cfg -Pattern '^\s*Enabled\s*=' -Context 0,0    # every Enabled line, with its line number
   Copy-Item $cfg C:\StreamFlex-baseline.ini     # the "original" that steps 6 and 8 compare with
   ```
   Expected: under `[Gamepad]` the file has `Enabled=true` or no `Enabled` line at all, and never `Enabled=false`. If it says `false`, that is a FAIL of the default: record it, then carry on with the file as shipped and mark the pad checks BLOCKED. **Every later "the original" means `C:\StreamFlex-baseline.ini`**, which is now the zip's own file.
4. **Make the picture folder.** It must be in the Pictures known folder of the user StreamFlex runs as. `Été` and `caché` are built from code points, so the command's own encoding cannot mangle them:
   ```powershell
   Add-Type -AssemblyName System.Drawing
   $E = "$([char]0xC9)t$([char]0xE9)"                                   # Été
   $pics = [Environment]::GetFolderPath('MyPictures')
   $dir = New-Item -ItemType Directory -Force -Path (Join-Path $pics $E)
   function New-Solid([string]$path, [System.Drawing.Color]$c, $fmt) {
       $b = [System.Drawing.Bitmap]::new(640, 360); $g = [System.Drawing.Graphics]::FromImage($b)
       $g.Clear($c); $g.Dispose(); $b.Save($path, $fmt); $b.Dispose() }
   New-Solid "$dir\rouge.png" ([System.Drawing.Color]::FromArgb(255, 0, 0)) ([System.Drawing.Imaging.ImageFormat]::Png)
   New-Solid "$dir\VERT.PNG"  ([System.Drawing.Color]::FromArgb(0, 160, 0)) ([System.Drawing.Imaging.ImageFormat]::Png)
   New-Solid "$dir\BLEU.JPG"  ([System.Drawing.Color]::FromArgb(0, 0, 255)) ([System.Drawing.Imaging.ImageFormat]::Jpeg)
   $hidden = "$dir\cach$([char]0xE9).png"                                # caché.png
   New-Solid $hidden ([System.Drawing.Color]::FromArgb(255, 255, 0)) ([System.Drawing.Imaging.ImageFormat]::Png)
   (Get-Item -LiteralPath $hidden -Force).Attributes += 'Hidden'
   Set-Content -LiteralPath "$dir\notes.txt" -Value 'Not an image.'
   Get-ChildItem -LiteralPath $dir -Force | Select-Object Name, Attributes
   ```
   Expected listing: `BLEU.JPG`, `caché.png` (Hidden), `notes.txt`, `rouge.png` and `VERT.PNG`. StreamFlex must show and count **3 images**: BLEU.JPG, rouge.png and VERT.PNG.
5. **Connect the ViGEm Xbox 360 pad before StreamFlex starts.**
6. **Start `C:\StreamFlex\streamflex.exe -d`** (the `-d` matters: the `Video:` line in step 7 is written only at debug level) in the interactive desktop session, from `C:\StreamFlex`. **Frame** `W0-home.png`.
7. **Expected in the log:** `Video: SDL's windows driver, the <renderer> renderer` (record the renderer name), `Video driver:  windows`, `Resolution:` with the guest's resolution, `Gamepad connected with device index 0, instance id <N>` and then `Gamepad opened at device index 0, instance id <N>` (the same N), a `Gamepad Mapping:` block, `Loading menu 'Main'`, and `Gained keyboard focus`. The screen shows Main's four buttons (Kodi, Plex, Steam, System) on black. If `Gamepad connected` is missing, stop: the pad is not reaching SDL. That is a setup fault, not a verdict.

## W1. Start opens settings (brief step 1)

- **Inputs:** Pad **Start**, with the config exactly as the zip shipped it (no edit from W0).
- **Expected:**
  - The pad works out of the box: `Gamepad ButtonStart detected` appears with no `Enabled` edit.
  - The column is on the left, headed "Settings", with these rows: *Background › Color*, *Menus › 2 menus*, *Titles › Medium*, a divider, then *Discard changes* (grayed). There is a key hint at the bottom.
  - The preview on the right is a 16:9 outlined picture of Main's menu. Its caption reads *Preview: Main · 4 × 1, … px buttons*.
  - **Frame** `W1-open.png`.
  - New lines include `Gamepad ButtonStart detected`, `Settings opened over menu 'Main'`, and `Settings: the preview is at X,Y, W x H`. Note the rectangle; the pixel checks below use it.
- Then pad **B**. Settings close. New lines: `Settings: nothing changed`, `Settings closed`.

## W1a. A held Start opens settings once (Batch C Imp 1)

- **Inputs:** from the home screen, take a mark, then hold pad **Start** for **2000 ms** and release. Wait 1 s.
- **Expected:**
  - Settings are open. **Frame** `W1a-held-start.png`.
  - The new lines hold exactly **one** `Settings opened over menu 'Main'` and **no** `Settings closed`. The old behavior repeated the button after 500 ms and toggled settings shut and open again, so any second `Settings opened`, or any `Settings closed`, is a FAIL.
- Then pad **B** to close them (`Settings closed`).

## W1b. The Menu key, single and held (Batch C Imp 1, and the key code)

- **Inputs, part 1:** take a mark, then a single **Menu** key (down, up).
- **Expected:**
  - New lines hold `Key … (#40000065) detected`, then `Settings opened over menu 'Main'`. Match on the keycode `(#40000065)`, not the name: SDL names it `Application` or `Menu` depending on its version (the tested Windows build logs `Key Menu (#40000065)`).
  - If the Key line shows another code, the SendInput event is wrong: BLOCKED, not FAIL.
- Then **Backspace**: `Settings closed`.
- **Inputs, part 2 (held):** SendInput does not auto-repeat a held key, so act as the keyboard would. Take a mark, then send a **Menu** key-down, 30 further Menu key-downs 33 ms apart (no key-up between them), and then one key-up. Wait 1 s.
- **Expected:**
  - Settings are open. **Frame** `W1b-held-menu.png`.
  - The new lines hold exactly **one** `Settings opened` and **no** `Settings closed`.
  - They also hold **at least two** `Key … (#40000065) detected` lines. This proves the repeats reached StreamFlex. If there is only one, the repeats were not delivered, and the check is BLOCKED rather than PASS. (A repeat is logged before StreamFlex drops it, so one line means the guest sent no repeats.)
- Then **Backspace** to close.
- **Both Menu keycodes** open settings: 0x40000065 (`SDLK_APPLICATION`) and 0x40000076 (`SDLK_MENU`). Part 1 covers the first; the second is N/A on Windows, as below.
- **SDLK_MENU (0x40000076)** has no Windows virtual key. Write "N/A on Windows; checked on Ubuntu" in the report.

## W2. Background modes (brief step 2)

- **Inputs:** Pad **Start** (settings open on *Background*), then **A**. The Background page shows *Mode: Color* and *Color: Black*.
  1. **D-pad Down** (to *Color*), then **D-pad Right**. Color becomes *Charcoal*.
     - The preview's background turns dark gray. **Frame** `W2-charcoal.png`.
     - New line: `Settings: [Background] Color #000000 -> #1E1E1E`.
     - Pixel check: a pixel inside the preview, near its top-left corner and clear of the outline, reads about (30,30,30).
  2. **D-pad Up** (to *Mode*), then **D-pad Right**. Mode becomes *Image*, with an *Image: Choose…* row.
     - The preview stays charcoal: an incomplete mode previews as the color.
     - New line: `Settings: [Background] Mode Color -> Image`. **Frame** `W2-image.png`.
  3. **D-pad Right**. Mode becomes *Slideshow*, with *Folder*, *Change every* and *Fade* rows.
     - New line: `Mode Image -> Slideshow`. **Frame** `W2-slideshow.png`.
  4. **D-pad Right**. Mode becomes *Transparent*, and the column shows a note that the desktop shows through.
     - The preview shows a **checkerboard** of two alternating colors. **Frame** `W2-transparent.png`.
     - New lines: `Mode Slideshow -> Transparent`, and `Background set up: Transparent`.
     - Record whether any part of the real desktop shows through the window now. This is an observation, not a verdict: the real window is judged in W7b.
  5. **D-pad Left** three times. Mode steps back through Slideshow and Image to *Color*.
     - The preview is charcoal again, with no checkerboard, and the rest of the window is solid. **Frame** `W2-back-to-color.png`.
     - New lines: `Transparent -> Slideshow`, `Slideshow -> Image`, `Image -> Color`.
- **PASS** when every line above appears and every frame matches its description.

## W3. Image, through a non-ASCII folder (brief step 3; Batch C Imp 2 in the browser)

- **Inputs:** still on the Background page, with the cursor on *Mode*.
  1. **D-pad Right** (Mode becomes *Image*), **D-pad Down** (to *Image: Choose…*), then **A**.
     - The browser opens in Pictures. New line: `Settings: browsing C:\Users\<user>\Pictures`. **Frame** `W3-pictures.png`.
  2. **D-pad Down** until `Été` is highlighted. Folders come first, sorted without regard to case, so `Été` may come after other folders; use frames to find it. Then **A**.
     - New line: `Settings: browsing C:\Users\<user>\Pictures\Été`, with the accents intact in the log.
     - The list shows **exactly** `BLEU.JPG`, `rouge.png` and `VERT.PNG`, in that order.
     - `caché.png` (hidden) and `notes.txt` are **not** listed. **Frame** `W3-ete.png`.
  3. Move the highlight onto `BLEU.JPG`, then `rouge.png`, then `VERT.PNG`, pausing 1 s on each.
     - The preview's background follows: blue, then red, then green.
     - One new `Settings: the preview shows …\Été\<file>` line per image.
     - **Frame** each: `W3-bleu.png`, `W3-rouge.png`, `W3-vert.png`. Pixel check inside the preview: about (0,0,255), (255,0,0) and (0,160,0).
  4. **D-pad Up** to `rouge.png`, then **A**.
     - New line: `Settings: chose C:\Users\<user>\Pictures\Été\rouge.png`.
     - Back on the Background page, the Image row shows `rouge.png`, and the preview is red. **Frame** `W3-chosen.png`.

## W3b. A slideshow folder (Batch C Imp 2 in the count)

- **Inputs:**
  1. **D-pad Up** (to *Mode*), **D-pad Right** (*Slideshow*), **D-pad Down** (to *Folder: Choose…*), then **A**.
  2. The browser opens in Pictures or in `Été`. If it opens in Pictures, highlight `Été` and press **A**.
  3. The first row reads *Use this folder (3 images)*. **Frame** `W3b-use-folder.png`. Then **A** on that row.
- **Expected:**
  - The *Use this folder* row says **3 images**, not 2 and not 4.
  - New line: `Settings: chose C:\Users\<user>\Pictures\Été`.
  - The Folder row reads `Été · 3 images` (log: `Settings: the Folder row shows Été · 3 images`).
  - The preview shows one of the three solid colors. **Frame** `W3b-slideshow.png`.
- Then **B** to the top level. The Background row now reads *Slideshow*.

## W4. Menus (brief step 4)

- **Inputs:**
  1. **D-pad Down** (to *Menus › 2 menus*), then **A**. The Menus page lists *All menus 4 × 1*, a divider, *Main* and *System*.
  2. **A** on *All menus*: the rows are Rows 1, Columns 4 and Largest button 256 px.
  3. **D-pad Down** (to *Columns*), then **D-pad Right** twice.
     - Columns reads 6. The preview's buttons get smaller and their titles scale with them. **Frames** `W4-cols5.png` and `W4-cols6.png`.
     - New lines: `Settings: [Layout] Columns 4 -> 5` and `Settings: [Layout] Columns 5 -> 6`.
  3a. **The preview rests first.** **B** to the Menus page. Press **D-pad Down** and **D-pad Up** several times in quick succession (about 100 ms apart) over *Main* and *System*. While the cursor keeps moving the preview stays on the menu it had; it switches to the menu under the cursor about 300 ms after the last move. Rest 1 s on *System*: the preview shows System. **FAIL** if the preview lays out every row passed. Then **A** on *All menus* and **D-pad Down** to *Columns*, as step 3 left it.
  4. **B** (to the Menus page), then **D-pad Down** twice (to *System*; the divider is skipped), then **A**.
     - The System page shows *Rows: All menus (1)*, *Columns: All menus (6)*, *Largest button*, and a note.
     - The preview switches to the System menu (Shutdown, Restart, Sleep, Settings). **Frame** `W4-system.png`.
  5. **D-pad Right** twice on *Rows*.
     - Rows reads 2, and the preview changes to fit two rows. **Frame** `W4-system-rows2.png`.
     - New lines: `Settings: [System] Rows (none) -> 1` and `Settings: [System] Rows 1 -> 2`.
  6. **B** twice, back to the top level. The preview shows Main again, 6 across.

## W5. Titles (brief step 5)

- **Inputs:**
  1. **D-pad Down** to *Titles › Medium*, then **A**. *Size: Medium*.
  2. **D-pad Right**: *Large*, with bigger preview titles. **Frame** `W5-large.png`.
  3. **D-pad Left**: *Medium*.
  4. **D-pad Left**: *Small*, with smaller titles. **Frame** `W5-small.png`.
- **Expected new lines:** `Settings: [Titles] FontSize 14% -> 17%`, then `17% -> 14%`, then `14% -> 11%`.
- Then **B** to the top level.
- A title too long for its button is checked in W9, where a long title is added to Main.

## W6. Save, close, and the file (brief step 6)

- **Inputs:** at the top level, pad **B**. Settings close. Wait 2 s, then key **Esc**. StreamFlex quits.
- **Expected:**
  - Settings close onto Main, with the slideshow running, 6 columns, and small titles. **Frame** `W6-closed.png`.
  - New lines: `Settings saved 7 change(s) to C:\StreamFlex\config.ini (backup: C:\StreamFlex\config.ini.bak)`, `Settings closed`, then `Quitting program`. The process exits with code 0.
  - Copy the log to `logs\W1-W6.log`.
  - **The file.** Compare it with the original:
    ```powershell
    Compare-Object (Get-Content C:\StreamFlex-baseline.ini -Encoding UTF8) (Get-Content C:\StreamFlex\config.ini -Encoding UTF8)
    (Get-FileHash C:\StreamFlex-baseline.ini).Hash -eq (Get-FileHash C:\StreamFlex\config.ini.bak).Hash   # True
    ```
    Expect exactly these differences, and nothing else:
    - `[Background]`:
      - `Mode=Color` became `Mode=Slideshow`;
      - `Color=#000000` became `Color=#1E1E1E`;
      - two new lines after the section's last key (`OverlayOpacity=50%`): `Image=C:\Users\<user>\Pictures\Été\rouge.png` and `SlideshowDirectory=C:\Users\<user>\Pictures\Été`. The accents must be intact when the file is read as UTF-8.
    - `[Layout]`: `Columns=4` became `Columns=6`.
    - `[Titles]`: `FontSize=14%` became `FontSize=11%`.
    - `[System]`: a new `Rows=2` line, directly under the `[System]` header.
  - Every comment line (`# …`, and the commented `#Image=` and `#SlideshowDirectory=` lines) is still there, unchanged. The line endings match the original's. `config.ini.bak` has the same hash as the original. No `config.ini.tmp` or `config.ini.bak.tmp` is left behind.
- **PASS** only if the difference list is exactly the one above.

## W7. Restart: everything as saved (brief step 7; Batch C Imp 2 in the slideshow scan)

- **Inputs:** start `streamflex.exe -d` again. Wait 5 s. **Frame** `W7-restart.png`. Wait 35 s more (the default *Change every* is 30 s). **Frame** `W7-restart-35s.png`.
- **Expected:**
  - The background is a slideshow of the `Été` pictures: the two frames show different solid colors.
  - Main shows 6 columns and small titles. Opening System would show two rows' worth of sizing; that is optional to check.
  - The log holds `Found 3 images in directory C:\Users\<user>\Pictures\Été:`, followed by exactly three paths: the ones ending `BLEU.JPG`, `rouge.png` and `VERT.PNG`, in any order. No `caché.png`, no `notes.txt`.
  - The log holds `Background set up: Slideshow`.
  - Copy the log to `logs\W7.log`.

## W7b. The real transparent window

- **Inputs:**
  1. Pad **Start**, **A** (Background), **D-pad Right** (Mode goes from *Slideshow* to *Transparent*).
  2. **B**, **B**. This saves and closes.
  3. Wait 2 s. **Frame** `W7b-transparent.png`.
- **Expected:**
  - The Windows desktop, meaning its wallpaper and any windows behind, is visible around and between StreamFlex's buttons. The buttons and titles are still drawn.
  - New lines: `Settings saved 1 change(s)`, and `Background set up: Transparent`.
- **Inputs, back:**
  1. Pad **Start**, **A**, **D-pad Left** three times (*Slideshow*, *Image*, *Color*).
  2. **B**, **B**.
  3. **Frame** `W7b-opaque.png`.
- **Expected:**
  - The desktop no longer shows through: the background is solid charcoal, (30,30,30).
  - New lines: `Background set up: Color`, and `Settings saved 1 change(s)`.
- **PASS** when both frames match.

## W8. A read-only config (brief step 8; Batch C New 1)

- **Inputs:**
  1. `Set-ItemProperty C:\StreamFlex\config.ini -Name IsReadOnly -Value $true`, then record `$h = (Get-FileHash C:\StreamFlex\config.ini).Hash`.
  2. Pad **Start**, **D-pad Down** twice (*Titles*), **A**, **D-pad Right** (*Medium*), **B** (to the top level), **B** (save).
- **Expected:**
  - The rows are replaced by *Couldn't save to C:\StreamFlex\config.ini: permission denied*, with *Try again* and *Leave without saving* under it. **Both rows lie fully inside the column**; the message wraps or shortens rather than pushing them off (Batch C New 1). **Frame** `W8-failed.png`.
  - New line (an error, always logged): `Couldn't save to C:\StreamFlex\config.ini: permission denied`.
- **Inputs:** **D-pad Down** (to *Leave without saving*), then **A**.
- **Expected:**
  - Settings close, and the titles stay Medium on screen.
  - New lines: `Settings: leaving without saving`, `Settings closed`.
  - The file's hash still equals `$h`. No `config.ini.tmp` or `config.ini.bak.tmp` exists.
  - **Frame** `W8-left.png`.
- Then key **Esc** (StreamFlex quits), and copy the log to `logs\W7b-W8.log`.

## W9. Launching a non-ASCII path (brief step 9: Task 2's `start_process()` fix)

- **Setup:**
  ```powershell
  Set-ItemProperty C:\StreamFlex\config.ini -Name IsReadOnly -Value $false
  $E = "$([char]0xC9)t$([char]0xE9)"; $cafe = "Caf$([char]0xE9)"; $cafetxt = "caf$([char]0xE9).txt"
  $dir = New-Item -ItemType Directory -Force -Path "C:\Users\Public\$E"
  Set-Content -LiteralPath "$dir\$cafetxt" -Value 'StreamFlex café test' -Encoding UTF8
  $cfg = 'C:\StreamFlex\config.ini'
  $text = [IO.File]::ReadAllText($cfg)
  $entry = "Entry9=$cafe;apps;`"C:\Users\Public\$E\$cafetxt`""
  $new = [regex]::Replace($text, '(?m)^(Entry4=System;settings;:submenu System)(\r?\n)', "`$1`$2$entry`$2")
  if ($new -eq $text) { throw 'Entry4 of [Main] not found' }
  [IO.File]::WriteAllText($cfg, $new, [Text.UTF8Encoding]::new($false))
  Select-String -LiteralPath $cfg -Pattern '^Entry9=' -Encoding UTF8
  ```
  Also add, the same way and directly after the `Entry4=System;...` line, a title far too wide for its button: `Entry8=Extraordinarily Wide Title WWWWWWWWWWWWWWWWWWWWWW;apps;notepad.exe`.
  The last line prints `Entry9=Café;apps;"C:\Users\Public\Été\café.txt"`.
- **Inputs:**
  1. Start `streamflex.exe -d`. Main now shows five buttons, the last one *Café*.
  2. **The long title.** Main now has six buttons. Look at the *Extraordinarily Wide Title* button (**Frame** `W9-longtitle.png`). Expected: its title is cut to fit, and no letter is drawn past the button's left or right edge or into a neighbor. Zoom on both edges. **FAIL** if any glyph is outside the button.
  3. **D-pad Right** onto *Café*. **Frame** `W9-cafe.png`.
  4. Take a mark, pad **A**, and within 300 ms key **Menu**. The app is launching.
  5. Once Notepad is in front, key **Menu** and pad **Start** again (the pad is ignored while StreamFlex is behind, so use the Menu key sent to the desktop and confirm it is not delivered to StreamFlex).
- **Expected:**
  - Notepad opens `café.txt`: its title bar names `café.txt` and it shows the text. **Frame** `W9-notepad.png`.
  - New lines include `Application detected`. There is **no** `Failed to launch command` and **no** `Could not launch` line.
  - **Settings cannot open while an app launches or runs (#116).** After steps 4 and 5 the new lines hold **no** `Settings opened`, and the StreamFlex window shows no settings screen when Notepad closes (**Frame** `W9-no-settings.png`).
- **Then:** close that Notepad window, by the PID of the window whose title names `café`. Never close Notepad by name. StreamFlex regains focus: new lines `Gained keyboard focus` and `Application finished`. Keys go to the focused window, so wait for `Gained keyboard focus` before sending anything. Then key **Esc**: `Quitting program`, exit code 0. Copy the log to `logs\W9.log`.

## W10. Nothing reacts behind an application (SDL ignores the pad while unfocused)

- **Inputs:** start `streamflex.exe -d` and launch Notepad from a tile (the *Café* button from W9, or a tile that runs `notepad.exe`). Wait for `Application detected` and for Notepad to be in front. Take a mark. With Notepad in front, send pad **D-pad Right**, **D-pad Down**, **A**, **B** and **Start**, each held 150 ms, 500 ms apart.
- **Expected:**
  - Nothing reaches StreamFlex: the new lines hold no `Gamepad ButtonDPadRight detected` (nor `Down`, `A`, `B`, `Start`) and no `Settings opened`. Notepad's text is unchanged.
  - Close Notepad by its PID. New lines then hold `Gained keyboard focus` and `Application finished`, and StreamFlex is on the same button as before, with no settings open (**Frame** `W10-after.png`). No navigation and no settings happened while it was behind.
  - This is SDL's default of ignoring pad input while the window is unfocused. Any pad line logged while Notepad was in front is a **FAIL**: record which buttons.
- Then key **Esc** and copy the log to `logs\W10.log`.

## What to send back

- A table of W1 to W10 (with W1a, W1b, W3b and W7b, and the zip check in W0), each marked PASS, FAIL, BLOCKED or N/A, with a one-line reason for anything that is not PASS.
- Every frame, the `logs\` folder, and both `C:\StreamFlex-baseline.ini` and the final `config.ini` / `config.ini.bak`.
- The W0 record: Windows build, resolution and scale, GPU, what the shipped config holds for `[Gamepad] Enabled`, the zip listing, and the renderer from the `Video:` line.
