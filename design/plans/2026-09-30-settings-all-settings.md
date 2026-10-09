# Every setting on the settings screen (3b) — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every `config.ini` setting that 3a left file-only becomes editable from the remote, live, with a colour picker, a font picker, a command picker and key and gamepad bindings, and Back saves only what changed.

**Architecture:**
- **One parse path.** Every remaining key joins 3a's settings table (`settings.c`) and is read through `setting_parse()`. A new SDL-side module, `config_fields.c`, is the only code that moves a `SettingValue` into `Config` or back. `config_handler()` loses its per-key branches.
- **`Config` holds what the file says.** A new pure module, `derive.c`, computes the values the launcher draws with (px, alpha bytes, clamps) as often as needed, into a global `Effective eff`. `validate_settings()` shrinks to nothing that converts, and nothing writes a fallback back into `Config` any more.
- **A start, stop and reload for each feature** (overlay, highlight, scroll indicators, clock, screensaver, gamepad) in `launcher.c`. Startup calls the same `start` the settings screen does.
- **New pure modules,** each unit-tested without SDL:
  - `listpick.c`, the shared list picker;
  - `colorpick.c`, the swatch grid and hex editor, with the contrast maths;
  - `fontlist.c`, which groups font faces into families;
  - `bindings.c`, the binding lists, the safety floor and the capture state machine.
- **`inidoc` gains list mode,** and `config_save` gains list edits, for `[Hotkeys]` and `[Gamepad]`.
- **The SDL side of the pickers and of capture** goes in a new `settings_pickers.c`, so `settings_screen.c` keeps the pages.

**Tech Stack:** C (C99, as the codebase is), SDL2 (floor **raised to 2.0.18** by this plan), SDL2_image (2.0.5), SDL2_ttf (2.0.15), inih, CMake 3.18+ and CTest, Docker (headless checks), GitHub Actions.

**Spec:** `design/specs/2026-09-29-settings-all-settings-design.md`. Read it first; this plan implements it. It builds on 3a's spec (`design/specs/2026-09-27-settings-screen-design.md`) and 3a's plan (`design/plans/2026-09-27-settings-screen.md`), whose code is now on `master`.

## Spec corrections found while planning

The spec was written against `feat/settings-screen` before 3a's fix wave. Each of these is a wrong or loose assumption about the code as it now is on `master` (`cb1e1db`). The plan follows the right-hand side.

1. **`execute_command()` is in `src/launcher.c`, not `util.c`, and `:exit` is silent on every platform wherever it is not a Windows hotkey.** A Linux hotkey, or a menu entry on either platform, runs nothing and logs nothing. Only `add_hotkey()` on Windows (`util.c`) turns `:exit` into the exit hotkey. The fix logs `':exit' works only as a hotkey on Windows` for every `:exit` that reaches `execute_command()`.
2. **`FPSLimit`'s `> 10` is in two places:** `config_handler()` and `create_window()`. Both go, the first through the settings table and the second through the new frame timing.
3. **Both Menu key codes already open settings** (`menu_key()`, `launcher.c`). 3b's capture shows either code as *Menu*; nothing else changes.
4. **More values are rewritten in `Config` than the spec lists.**
   - `load_font()` replaces the configured font path with the bundled one when the font fails.
   - `calculate_shadow_alpha()` writes the shadow colour's alpha.
   - `init_screensaver()`, `init_clock()`, the scroll arrow's failure path and `create_window()` switch `screensaver_enabled`, `clock_enabled`, `scroll_indicators` and `vsync` off or on.

   All of them move out of `Config`: into `Effective`, into each feature's own running state, or into the `TextInfo` that opened the font.
5. **`validate_settings()` also writes `geo.vcenter`.** `derive_settings()` computes it now, and `refresh_effective()` copies it into `geo`.
6. **The built-in highlight fill's alpha moves from `0x40` to `0x3F`.** Until now the default colour carried alpha `0x40`, while `FillOpacity=25%` (the sample config's value) gave `0x3F`. With one source, the percentage, the built-in default gives what the sample config always gave. The pixel checks' tolerance (4) covers it.
7. **`derive_settings()` takes a plain `DeriveInput`, not `const Config *`.** `Config` holds SDL types, and `derive.c` must stay pure. `config_fields.c` fills the input from `Config`.
8. **Settings rows have no reason field.** `SettingsRow` gains `why`. A greyed row that has a reason can take the cursor, so the caption can say why it is greyed; Left, Right and OK do nothing on it. 3a's Discard row, greyed with no reason, is still skipped.
9. **The folder browser has only image modes.** It gains `BROWSER_FILE`, which lists every file that is not hidden, for *Mappings file*.
10. **`add_gamepad_control()` has the same function-local static tail pointer as `add_hotkey()`.** Both go.
11. **Several textures are built once and never rebuilt or freed.**
    - The overlay texture is built once in `main()` and never destroyed.
    - The scroll arrows are built once in `main()`.
    - The highlight's texture cache is keyed on its geometry only, so a colour change would not redraw it.

    Each becomes part of its feature's start, stop and reload.
12. **The clock hands its rendered surfaces to the main thread through plain `bool`s** (`state.clock_rendering`, `state.clock_ready`). That is a data race, and `stop_clock()` must wait for the thread safely. They become `SDL_atomic_t`, as the slideshow's are.
13. **The gamepad code confuses an instance id with a device index in two places.**
    - `connect_gamepad()` searches its list with `gamepad->id == device_index`.
    - The removal event filters with `config.gamepad_device == event.jdevice.which`, whose `which` is an instance id.

    The *Device* setting needs both right, so both are fixed.
14. **The font scan's thread only lists files.** SDL_ttf keeps one FreeType library, which must not open faces on another thread while the main thread draws text. So the scan thread walks the folders or the registry, and the main thread reads the faces a few files per frame, while the picker shows *Loading fonts… (N)*.
15. **Nothing may log from a thread other than the main one:** `output_log()` formats into one static buffer. The scan's count and time are logged by the main thread when it finishes.
16. **Windows' registry API needs `advapi32`,** which the launcher does not link by name today.
17. **Percentages in the file can have decimals** (`Opacity=12.5%`: `convert_percent_to_int()` read them with `atof`). The table keeps percentages in hundredths, so `12.5%` survives a round trip. *Padding* stays whole, as `layout_parse_title_padding()` always required.
18. **`IconSpacing` and `[Clock] Margin` accept px as well as percentages,** and so does `[Titles] Padding`. A px value shows as *N px*, sorted before the percentages.
19. **The spec gives *Default menu* and *Device* two behaviours**, stepped (the pages) and the list picker (the pickers section). They get both: Left and Right step, and OK opens the list picker.
20. **The new number and percentage keys are parsed strictly.** `atoi` and `atof` used to read `IconSpacing=40px` as 40 and `Opacity=abc%` as 0%. Both are now refused with a log line, as the spec asks for invalid values. 3a's `SlideshowImageDuration` keeps its `atoi` reading, and so do the two new seconds keys, which share its type.
21. **The Keyboard and Gamepad pages list the file's lines**, read when settings open, not the in-memory lists. Only the file has each line's original text, which the save needs to find it again, and a Windows `:exit` hotkey never reaches the in-memory list. Applying a binding change rebuilds the in-memory lists from the model. *Discard* rebuilds them from the lines as settings opened. The spec's "replace and remove functions" become `clear_hotkeys()`, `clear_gamepad_controls()` and a rebuild.
22. **The harness can hold only the virtual pad's Start button.** Its hook gains `STREAMFLEX_TEST_PAD_BUTTON`, naming the button to hold, so a gamepad capture can be driven.
23. **The top page now starts with *General*,** so every existing harness key sequence that walks the top page gains one `Down` (Task 6 lists each).
24. **`Intensity` of 0% dims nothing,** and today it switches the screensaver off in `Config`. Now it is kept as the file says. The screensaver does not start and logs why, and the *Dim level* row's steps start at 10%.
25. **`DeviceIndex` had no check** (`atoi`). It is now a whole number from -1 (any pad) to 15.
26. **`settings_screen.c` is 1,371 lines** after 3a's fix wave, not 1,114.
27. **`SET_TYPE_CHOICE` served only the background mode.** Each choice setting now carries its own names in its `SettingDef`.

## Existing bugs fixed on the way

| Bug | What happens today | Fixed in |
|---|---|---|
| `FPSLimit=10` is ignored | The docs say "the minimum is 10"; `config_handler()` and `create_window()` both need more than 10 | Task 1 (parse), Task 4 (timing) |
| A negative `[Clock] FontSize` | `(unsigned int) atoi("-5")` passes the `if (font_size)` test and wraps to 4294967291 | Task 1 |
| `:exit` outside a Windows hotkey | Runs nothing and says nothing | Task 3 |
| Repeated keys leak | `DefaultMenu`, `StartupCmd`, `QuitCmd`, `Font` and `ControllerMappingsFile` are `strdup`ed over the last copy without freeing it | Task 3 (the table stores through `config_fields.c`, which frees) |
| The gamepad's instance id and device index are compared | A pad whose instance id differs from its device index (any pad plugged in after another was removed) is added twice or never removed | Task 4 |
| The clock's thread handoff | `state.clock_ready` is a plain `bool` written on the clock thread and read on the main thread | Task 4 |

## Global Constraints

- **The build's SDD workspace** is `.superpowers/sdd/2026-09-30-settings-all/` (git-ignored), as 3a's was. At the start, copy this section, *Existing bugs fixed on the way* and *Review Focus* into its `global-constraints.md`, which every task brief points to, and keep the ledger in its `progress.md`.
- **Every repo command uses an absolute path:** `git -C C:/Users/jscha/source/repos/streamflex ...`, and files under `C:/Users/jscha/source/repos/streamflex/`. Never `cd` into the repo.
- **Branch:** work on `feat/settings-all`, cut only after the PR carrying this plan has merged:

  ```
  pwsh C:/Users/jscha/.claude/scripts/git-new-branch.ps1 -Repo C:/Users/jscha/source/repos/streamflex -Branch feat/settings-all
  ```

  Stage named files only; never `git add -A`. Commit messages are conventional (`feat:`, `fix:`, `test:`, `docs:`, `ci:`, `refactor:`) with no AI attribution. The PR is squash-merged.
- **The pure modules must not include SDL, `launcher.h` or any SDL-using header:**
  - 3a's `fileio.c`, `inidoc.c`, `config_save.c`, `settings.c`, `browser.c` and `layout.c`;
  - 3b's new `derive.c`, `listpick.c`, `colorpick.c`, `fontlist.c` and `bindings.c`.

  They may include `<launcher_config.h>` (macros only). Each test executable links them without SDL. They allocate through `alloc.h`, and each one that allocates joins `test_alloc`.
- **Use no SDL API newer than 2.0.18,** no SDL_image API newer than 2.0.5, and no SDL_ttf API newer than 2.0.15.
  - The 2.0.18 floor is for `SDL_RenderSetVSync()`. Task 4 raises the floor in `CMakeLists.txt`.
  - Still no `TTF_SetFontSize()` (SDL_ttf 2.0.18): every font size is its own `TTF_OpenFont()` or `TTF_OpenFontIndex()`.
  - `TTF_OpenFontIndex`, `TTF_FontFaces`, `TTF_FontFaceFamilyName`, `TTF_FontFaceStyleName` and `TTF_GlyphIsProvided` are all older than 2.0.15.
- **The MSVC build is `/W4 /WX`** (the Windows CI job, `-DEXTRA_WARNINGS=ON`). Every new line must compile warning-free there, and under GCC's `-Wall -Wextra -Wpedantic -Wconversion`. In practice:
  - an unused parameter gets `UNUSED(x)`;
  - an `int`/`unsigned`/`long` mix gets an explicit cast;
  - no declaration hides an outer one (C4456/C4457).
- **Keep the codebase's style:**
  - four-space indents and braces on the same line;
  - one comment line above each function, in the existing voice ("// A function to ...");
  - `snprintf`, never `sprintf` or `strcpy` into fixed buffers.
- **Only the main thread logs** (`output_log()` formats into one static buffer), creates or destroys textures, or opens and closes fonts.
- **Every local Windows build command starts with the CMake PATH line,** because shell state does not persist between tool calls:

  ```powershell
  $env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
  ```

  - **Build:** `cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release`.
  - **Tests:** `ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure`.
  - **Check the output for `100% tests passed`, not only the exit code.** A missing PATH line makes `ctest` a CommandNotFound, which sets no exit code.
- **Sharing the host's memory.** The testing sessions (qa-harness, qa-test and sf-test) run VM guests and containers on this host. Their agreed rules, which this build follows:
  - keep at least 8 GB of host RAM free;
  - name every container this build starts with the prefix `sfdev-` (`docker run --name sfdev-<label> ...`);
  - announce each batch before it starts and when it ends, to all three sessions with `SendMessage`, giving the containers' names and about 0.4 GB each;
  - run at most two harness containers at once.

  Check free memory before each batch (`Get-CimInstance Win32_OperatingSystem | Select-Object FreePhysicalMemory`, in KB).
- **The headless harness** needs Docker Desktop running and never needs the user's display. Run it with:

  ```powershell
  docker build -t streamflex-test C:/Users/jscha/source/repos/streamflex/tests/headless
  docker run --rm --name sfdev-<label> --security-opt seccomp=unconfined -v C:/Users/jscha/source/repos/streamflex:/src:ro -v C:/Users/jscha/ClaudeScratch/streamflex-headless-out:/out streamflex-test bash /src/tests/headless/run.sh <label>
  ```

  - Fedora: `docker build -t streamflex-test-fedora -f C:/Users/jscha/source/repos/streamflex/tests/headless/Dockerfile.fedora C:/Users/jscha/source/repos/streamflex/tests/headless`, and the same `docker run` with `streamflex-test-fedora` and a label ending in `-fedora`.
  - The leak pass: the same `docker run` with the arguments `bash /src/tests/headless/run.sh <label> leaks`.
  - It prints one `PASS` or `FAIL` line per check, then `N failed`, and exits non-zero when anything failed.
  - Each check's config, log, stdout and stderr are kept in `C:/Users/jscha/ClaudeScratch/streamflex-headless-out/<label>/`. Each task names its labels (`t3`, `t3-leaks` and so on).
- **Linux unit tests** run in the harness's Debian image, as the non-root `tester` user, with ASan, UBSan and `-DEXTRA_WARNINGS=ON`. The script lives outside the repo, in `C:/Users/jscha/ClaudeScratch/sf-final/unit.sh`. If that folder was cleaned, write it again with exactly this:

  ```bash
  #!/bin/bash
  # Linux unit tests with ASan+UBSan and the extra warnings, run as the non-root user "tester"
  # usage: unit.sh <label>
  set -u
  label=$1
  out=/out/$label; rm -rf "$out"; mkdir -p "$out"
  rm -rf /work; mkdir -p /work
  tar -C /src --exclude=./build --exclude=./.git --exclude=./headless-out -cf - . | tar -C /work -xf -
  chown -R tester /work
  flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
  su tester -c "cmake -S /work -B /work/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='$flags' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' -DEXTRA_WARNINGS=ON" > "$out/configure.log" 2>&1 || { echo CONFIGURE FAILED; tail -20 "$out/configure.log"; exit 2; }
  su tester -c "cmake --build /work/build -j$(nproc)" > "$out/build.log" 2>&1 || { echo BUILD FAILED; tail -30 "$out/build.log"; exit 2; }
  echo "warnings outside src/external: $(grep -iE 'warning:' "$out/build.log" | grep -v '/work/src/external/' | grep -vcE '^g?make')"
  su tester -c "setarch $(uname -m) -R ctest --test-dir /work/build --output-on-failure" > "$out/ctest.log" 2>&1
  code=$?
  grep -E 'tests passed|Failed|FAIL' "$out/ctest.log" | head -20
  exit $code
  ```

  Run it with the harness image built above:

  ```powershell
  docker run --rm --name sfdev-<label> --security-opt seccomp=unconfined -v C:/Users/jscha/source/repos/streamflex:/src:ro -v C:/Users/jscha/ClaudeScratch/sf-final:/tools:ro -v C:/Users/jscha/ClaudeScratch/streamflex-headless-out:/out streamflex-test bash /tools/unit.sh <label>
  ```

  Pass means both `warnings outside src/external: 0` and `100% tests passed`.
- **Never launch StreamFlex on the user's desktop.** Everything a running launcher must prove goes through the headless harness. The one exception is the hands-on step in Task 16, which runs in guests.
- **Every QA run in a guest goes to the sf-test session** (user, 2026-09-30: "any qa work you need done for streamflex please send to sf-test. it is pre-cleared to accept tests from you and run them based on your instructions"). Send it with `SendMessage` to `sf-test`, and include:
  - the build's commit or tag;
  - where its artifacts are, with their SHA-256 checksums;
  - the checklist to run, with the order;
  - anything it must not touch.

  sf-test replies with numbered findings and how to reproduce each. Each finding is fixed with a failing check first, then handed back to sf-test to check again.
- **The user's standing rules for this build:**
  - fix everything a review finds, Minors included;
  - never trim a repeat run: parallelise instead;
  - write a failing check before every fix.
- **Exact values from the spec:**
  - *App timeout* steps: 3, 5, 10, 15, 20 and 30 s.
  - *FPS limit* steps: Off, 30, 60, 75, 120, 144, 165 and 240.
  - *Overlay opacity*, *Opacity*, *Fill opacity* and *Outline opacity*: 0-100% in steps of 5.
  - *Icon spacing*: 0-10% of the screen width in steps of 1.
  - *Vertical centre*: 25-75% in steps of 5.
  - Titles *Padding*: 0-20% in steps of 2.
  - *Outline size*: 0-10 px.
  - *Corner radius*: 0-100 in steps of 5.
  - *Vertical padding* and *Horizontal padding*: 0-100 px in steps of 5.
  - Clock *Size*: 20-120 in steps of 5.
  - Clock *Margin*: 0-10% in steps of 1.
  - *Idle time* steps: 3, 5, 10, 15 and 30 s, then 1, 2, 5, 10 and 15 min.
  - *Dim level*: 10-100% in steps of 10.
  - Capture waits 5 s, and navigation taken away must be confirmed within 10 s.
  - **The 24 swatches, in order:**
    - Black `#000000`, Charcoal `#1E1E1E`, Graphite `#33383D`, Slate `#2E3440`, Midnight `#121A2E`, Navy `#0B1F3A`, Teal `#07606C`, Forest `#1E3B2F`, Plum `#3B1F3A`, Burgundy `#4A1520`;
    - White `#FFFFFF`, Light grey `#C8C8C8`, Grey `#808080`, Dark grey `#4A4A4A`;
    - Red `#D03030`, Orange `#E07020`, Amber `#F0B000`, Yellow `#F0E040`, Lime `#80C040`, Green `#30A050`, Cyan `#20B0C0`, Blue `#3070D0`, Indigo `#5048C0`, Pink `#D04890`.
  - The contrast warning's threshold is 3:1 (WCAG).
- **The menu key** is `SDLK_APPLICATION` (`0x40000065`) or `SDLK_MENU` (`0x40000076`). `xdotool key Menu` sends the first.

## Review Focus

These five input classes are the most likely to bite someone using this, and the spec's own tests do not reach them. Each line names its test and the task that adds it.

1. **A value in the file in a form the table never read, or outside the new steps:**
   - `Opacity=12.5%`, `IconSpacing=40`, `HPadding=300`;
   - `FPSLimit=10`, which is newly accepted.

   An unrelated change must save every such line byte for byte. Each row must show its value in its sorted place, and stepping away and back must find it again.
   - Task 1: `test_settings` round-trips and sorted places.
   - Task 6: harness fixture `f55-odd`, where changing one other setting changes exactly one line.
2. **A feature switched off while it is mid-work:**
   - the clock's render thread holding a new minute's surfaces;
   - the screensaver dimmed.

   Stop must wait for the thread and free what it made, and the screen must come back undimmed.
   - Task 4: test hook `STREAMFLEX_TEST_CLOCK_DELAY_MS`.
   - Task 6: harness check *Clock off while it renders*, whose leak pass proves nothing is left behind.
3. **A font that cannot be used as configured:**
   - `FontFace=9` in a file with one face;
   - a font file removed after it was chosen;
   - a family with no Regular style.

   Each falls back to the bundled font with a log line, the file's value stays, and the picker still opens.
   - Task 3: harness fixture `f41-fontface`.
   - Task 10: `test_fontlist` (a family with no Regular), and the harness check *a font file that is gone*.
4. **`[Hotkeys]` hand-edited while settings are open:**
   - a binding's line changed;
   - a line removed;
   - a `HotkeyN` added by hand that takes the number the save would have used.

   A change is written as a new line and a removal skipped, each with a log line, and a new binding never reuses a number.
   - Task 12: `test_config_save`, *list edits after a hand edit*.
5. **The key that starts a capture is held down,** so its auto-repeats arrive during the capture. Or the capture ends with no binding: the 5 s run out, or the key pressed is one the floor refuses (Back itself, or a key SDL cannot name). Repeats and the starting press's release are ignored, and a timeout or a refusal leaves the binding as it was, with the reason in the caption.
   - Task 13: `test_bindings`, *the capture ignores the starting key's repeats and release*.
   - Task 14: harness check *a held Return starts one capture, and the key pressed after it is the one bound*.

## File structure

| File | Status | Responsibility |
|---|---|---|
| `src/settings.h`, `src/settings.c` | modify | New value types; every remaining key in the table; `setting_find()`; the nine pages, greyed rows with reasons, picker and binding rows |
| `src/derive.h`, `src/derive.c` | new | `derive_settings()`: configured values to the ones the launcher draws with |
| `src/config_fields.h`, `src/config_fields.c` | new | The only code that moves a `SettingValue` into `Config` or a menu, or back; the built-in defaults; `derive_input()` |
| `src/listpick.h`, `src/listpick.c` | new | The shared list picker's model |
| `src/colorpick.h`, `src/colorpick.c` | new | The swatch grid, the hex editor, luminance and contrast |
| `src/fontlist.h`, `src/fontlist.c` | new | Font faces grouped into families; the face a family writes |
| `src/fontscan.h`, `src/fontscan.c` | new | Finding font files: Windows' registry, Linux's folders; the listing thread |
| `src/bindings.h`, `src/bindings.c` | new | The binding lists, the safety floor, confirmations, the capture state machine, list edits for the save |
| `src/settings_pickers.h`, `src/settings_pickers.c` | new | The pickers' drawing and keys, capture, and applying bindings live |
| `src/settings_screen.c` | modify | Applying every setting through `config_fields.c` and the refresh groups; the new pages; handing pickers and capture to `settings_pickers.c` |
| `src/inidoc.h`, `src/inidoc.c` | modify | List mode |
| `src/config_save.h`, `src/config_save.c` | modify | List edits; notes for edits the save could not make as asked |
| `src/browser.h`, `src/browser.c` | modify | `BROWSER_FILE` |
| `src/launcher.h`, `src/launcher.c` | modify | `Config`'s new field types; `Effective eff`; start, stop and reload for every feature; frame timing; the title font's reload; the gamepad fixes; `:exit`'s log; capture routing |
| `src/util.h`, `src/util.c` | modify | `config_handler()` through the table; `validate_settings()` reduced; `clear_hotkeys()`, `clear_gamepad_controls()`, the gamepad label table shared |
| `src/image.h`, `src/image.c` | modify | `TextInfo` owns its font path and face; `load_font()` with a face; the highlight and scroll arrow from `eff`; a surface's mean luminance |
| `src/clock.h`, `src/clock.c` | modify | Colours and margin from `eff`; the thread handoff through atomics |
| `src/debug.c` | modify | The settings dump prints configured values |
| `src/platform/platform.h`, `win32.c` | modify | `clear_exit_hotkey()` |
| `src/CMakeLists.txt`, `CMakeLists.txt` | modify | New sources; `advapi32` on Windows; the SDL 2.0.18 floor |
| `config/config_settings.cmake`, `config/launcher_config.h.in` | modify | `FontFace` keys; default opacities as macros |
| `tests/CMakeLists.txt`, `tests/test_*.c` | new/modify | `test_derive`, `test_listpick`, `test_colorpick`, `test_fontlist`, `test_bindings`; `test_settings`, `test_inidoc`, `test_config_save`, `test_browser`, `test_alloc` extended |
| `tests/headless/checks/*.sh`, `tests/headless/fixtures/*` | new/modify | The General row's extra `Down`; the new checks and fixtures |
| `docs/configuration.md`, `docs/compilation.md`, `CHANGELOG.md`, `CONTRIBUTING.md` | modify | Documentation |
| `tests/qa/checklists/*.md` | modify | The hands-on rows for 3b |

---

### Task 1: The settings table — every remaining key, and the types they need

Every key 3a left file-only joins `DEFS`, with the value types it needs: on/off, whole numbers, percentages (with decimals), commands, fonts, the default menu and the gamepad device. Two new keys, `[Titles] FontFace` and `[Clock] FontFace`, are table rows with no row on screen. The table also answers "which setting is this key?" for the parser (`setting_find()`). This task changes only the pure model and its tests. Task 3 moves the parser onto it.

**Files:**
- Modify: `config/config_settings.cmake` (after line 25, and after line 55)
- Modify: `config/launcher_config.h.in` (after lines 60 and 92, and after line 154)
- Modify: `src/settings.h` (lines 1-78: types, ids, `SettingDef`; the declarations at the end)
- Modify: `src/settings.c` (lines 1-453: names, `DEFS`, parse, format, equal, candidates, step, describe; new `setting_find()`, `setting_command_label()`)
- Modify: `src/settings_screen.c:242-378` (a `default:` in `read_value()` and `apply_slot()` until Task 3 replaces both)
- Test: `tests/test_settings.c`

**Interfaces:**
- Consumes: 3a's `SettingDef`, `SettingValue`, `setting_parse()`, `setting_format()`, `setting_equal()`, `setting_step()`, `setting_describe()`.
- Produces (every later task relies on these names):
  - `SettingType` gains `SET_TYPE_BOOL`, `SET_TYPE_NUMBER`, `SET_TYPE_PERCENT`, `SET_TYPE_COMMAND`, `SET_TYPE_FONT`, `SET_TYPE_MENU`, `SET_TYPE_DEVICE`.
  - `SettingRefresh` gains `SET_REFRESH_TITLE_FONT`, `SET_REFRESH_HIGHLIGHT`, `SET_REFRESH_SCROLL`, `SET_REFRESH_CLOCK`, `SET_REFRESH_SCREENSAVER`, `SET_REFRESH_GAMEPAD`, `SET_REFRESH_FRAME`.
  - `SET_FLAG_PX`, `SET_FLAG_WHOLE`, `SET_FLAG_HIDDEN`, `SET_FLAG_NEXT_START`.
  - The `SettingId`s listed in Step 3, and `SettingDef`'s new fields `lo`, `hi`, `step`, `steps`, `step_count`, `names`, `labels`, `legacy`, `legacy_index`, `unit`, `max_px`, `flags`, `fallback`, `inherit_label`.
  - `const SettingDef *setting_find(const char *section, const char *key);`
  - `void setting_command_label(const char *command, char *out, size_t size);`
  - A `PERCENT` value's `number` is hundredths of a percent when `percent` is true, and px when it is false. A `BOOL`'s is 1 or 0. A `DEVICE`'s is the device index, -1 for any.

- [ ] **Step 1: Add the FontFace keys and the default opacities to the build's settings**

In `config/config_settings.cmake`, after line 25 (`set(SETTING_TITLE_FONT_COLOR "Color")`), add:

```cmake
set(SETTING_TITLE_FONT_FACE "FontFace")
```

and after line 55 (`set(SETTING_CLOCK_FONT "Font")`), add:

```cmake
set(SETTING_CLOCK_FONT_FACE "FontFace")
```

In `config/launcher_config.h.in`, after line 60 (`#define SETTING_TITLE_FONT "@SETTING_TITLE_FONT@"`), add:

```c
#define SETTING_TITLE_FONT_FACE "@SETTING_TITLE_FONT_FACE@"
```

after line 92 (`#define SETTING_CLOCK_FONT "@SETTING_CLOCK_FONT@"`), add:

```c
#define SETTING_CLOCK_FONT_FACE "@SETTING_CLOCK_FONT_FACE@"
```

and after line 154 (`#define DEFAULT_BACKGROUND_OVERLAY_OPACITY ...`), add the opacities the sample config already writes, so the table and `Config` share one source for them:

```c
#define DEFAULT_TITLE_OPACITY "@DEFAULT_TITLE_OPACITY@"
#define DEFAULT_HIGHLIGHT_FILL_OPACITY "@DEFAULT_HIGHLIGHT_FILL_OPACITY@"
#define DEFAULT_HIGHLIGHT_OUTLINE_OPACITY "@DEFAULT_HIGHLIGHT_OUTLINE_OPACITY@"
#define DEFAULT_SCROLL_INDICATOR_OPACITY "@DEFAULT_SCROLL_INDICATOR_OPACITY@"
#define DEFAULT_CLOCK_OPACITY "@DEFAULT_CLOCK_OPACITY@"
```

(`config_settings.cmake` already sets all five: lines 138, 146, 152, 167 and 186.)

- [ ] **Step 2: Write the failing tests**

Add to `tests/test_settings.c`, above `main()`:

```c
// A function to test the new types' round trips: what the parser reads is written back unchanged
static void test_new_round_trips(void)
{
    static const struct { SettingId id; const char *text; } cases[] = {
        { SET_ID_WRAP_ENTRIES, "true" },
        { SET_ID_VSYNC, "false" },
        { SET_ID_FPS_LIMIT, "10" },                  // The documented minimum, which the old parser refused
        { SET_ID_FPS_LIMIT, "144" },
        { SET_ID_APPLICATION_TIMEOUT, "15" },
        { SET_ID_ON_LAUNCH, "Quit" },
        { SET_ID_STARTUP_CMD, ":submenu Games" },
        { SET_ID_QUIT_CMD, "\"C:\\Program Files\\Kodi\\kodi.exe\" --standalone" },
        { SET_ID_DEFAULT_MENU, "Main" },
        { SET_ID_CHROMA_KEY_COLOR, "#010101" },
        { SET_ID_OVERLAY_OPACITY, "50%" },
        { SET_ID_OVERLAY_OPACITY, "12.5%" },
        { SET_ID_OVERLAY_OPACITY, "33.33%" },
        { SET_ID_ICON_SPACING, "5%" },
        { SET_ID_ICON_SPACING, "40" },               // px, as the old parser read it
        { SET_ID_ICON_SPACING, "2000000000" },       // f15-limits: huge, and still read
        { SET_ID_VCENTER, "50%" },
        { SET_ID_TITLE_FONT, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf" },
        { SET_ID_TITLE_FONT_FACE, "2" },
        { SET_ID_TITLE_OVERSIZE, "Shrink" },
        { SET_ID_TITLE_OVERSIZE, "None" },
        { SET_ID_TITLE_PADDING, "8%" },
        { SET_ID_TITLE_PADDING, "20" },
        { SET_ID_HIGHLIGHT_OUTLINE_SIZE, "3" },
        { SET_ID_HIGHLIGHT_CORNER_RADIUS, "25" },
        { SET_ID_HIGHLIGHT_HPADDING, "300" },        // Past the last step; the clamp is Effective's
        { SET_ID_CLOCK_FONT_SIZE, "50" },
        { SET_ID_CLOCK_MARGIN, "5%" },
        { SET_ID_CLOCK_TIME_FORMAT, "12hr" },
        { SET_ID_CLOCK_DATE_FORMAT, "Little" },
        { SET_ID_SCREENSAVER_IDLE_TIME, "300" },
        { SET_ID_SCREENSAVER_INTENSITY, "70%" },
        { SET_ID_GAMEPAD_DEVICE, "-1" },
        { SET_ID_GAMEPAD_DEVICE, "2" },
        { SET_ID_GAMEPAD_MAPPINGS, "/home/me/gamecontrollerdb.txt" }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        SettingValue value = parsed(cases[i].id, cases[i].text);
        CHECK_STR(formatted(cases[i].id, &value), cases[i].text);
    }

    // Spellings the parser reads, and how they are written back
    SettingValue value = parsed(SET_ID_WRAP_ENTRIES, "True");
    CHECK_INT(value.number, 1);
    CHECK_STR(formatted(SET_ID_WRAP_ENTRIES, &value), "true");
    value = parsed(SET_ID_TITLE_OVERSIZE, "Truncated");                     // The old parser's only spelling
    CHECK_STR(formatted(SET_ID_TITLE_OVERSIZE, &value), "Truncate");
    value = parsed(SET_ID_OVERLAY_OPACITY, "12.50%");
    CHECK_INT(value.number, 1250);
    CHECK(value.percent);
    CHECK_STR(formatted(SET_ID_OVERLAY_OPACITY, &value), "12.5%");
    value = parsed(SET_ID_TITLE_FONT, "\"C:\\Fonts\\My Font.ttf\"");         // Quotes dropped, as clean_path does
    CHECK_STR(value.text, "C:\\Fonts\\My Font.ttf");
    value = parsed(SET_ID_APPLICATION_TIMEOUT, "15s");                     // atoi, as before
    CHECK_INT(value.number, 15);

    // An absent FPSLimit, FontFace or command writes nothing: the key is removed
    SettingValue inherit;
    memset(&inherit, 0, sizeof(inherit));
    inherit.inherit = true;
    CHECK_STR(formatted(SET_ID_FPS_LIMIT, &inherit), "");
    CHECK_STR(formatted(SET_ID_TITLE_FONT_FACE, &inherit), "");
    CHECK_STR(formatted(SET_ID_STARTUP_CMD, &inherit), "");
}

// A function to test what the new types refuse, including the spec's two parser bugs
static void test_new_rejects(void)
{
    SettingValue value;
    CHECK(!setting_parse(setting_def(SET_ID_FPS_LIMIT), "9", &value));
    CHECK(!setting_parse(setting_def(SET_ID_CLOCK_FONT_SIZE), "-5", &value));   // Wrapped to 4294967291 before
    CHECK(!setting_parse(setting_def(SET_ID_CLOCK_FONT_SIZE), "0", &value));
    CHECK(!setting_parse(setting_def(SET_ID_WRAP_ENTRIES), "yes", &value));
    CHECK(!setting_parse(setting_def(SET_ID_WRAP_ENTRIES), "TRUE", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ON_LAUNCH), "blank", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "101%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "abc%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "50", &value));   // No px for an opacity
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "5.555%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "5.%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ICON_SPACING), "40px", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ICON_SPACING), "-4", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ICON_SPACING), "2147483648", &value));  // Past an int
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_PADDING), "8.5%", &value));  // Padding is whole
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_PADDING), "51%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_HIGHLIGHT_CORNER_RADIUS), "101", &value));
    CHECK(!setting_parse(setting_def(SET_ID_HIGHLIGHT_OUTLINE_SIZE), "-1", &value));
    CHECK(!setting_parse(setting_def(SET_ID_APPLICATION_TIMEOUT), "2", &value));
    CHECK(!setting_parse(setting_def(SET_ID_APPLICATION_TIMEOUT), "31", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SCREENSAVER_IDLE_TIME), "901", &value));
    CHECK(!setting_parse(setting_def(SET_ID_GAMEPAD_DEVICE), "-2", &value));
    CHECK(!setting_parse(setting_def(SET_ID_GAMEPAD_DEVICE), "16", &value));
    CHECK(!setting_parse(setting_def(SET_ID_STARTUP_CMD), "", &value));
    CHECK(!setting_parse(setting_def(SET_ID_DEFAULT_MENU), "", &value));
}

// A function to test the new types' steps: limits, the Off step, and a file's value in its place
static void test_new_steps(void)
{
    SettingValue off = parsed(SET_ID_WRAP_ENTRIES, "false");
    CHECK_INT(stepped(SET_ID_WRAP_ENTRIES, off, &off, 1, 1).number, 1);
    CHECK_INT(stepped(SET_ID_WRAP_ENTRIES, off, &off, 1, 5).number, 1);

    // FPS limit: Off, 30, 60, 75, 120, 144, 165, 240; a file's 10 sits between Off and 30
    SettingValue ten = parsed(SET_ID_FPS_LIMIT, "10");
    CHECK(stepped(SET_ID_FPS_LIMIT, ten, &ten, -1, 1).inherit);
    CHECK_INT(stepped(SET_ID_FPS_LIMIT, ten, &ten, 1, 1).number, 30);
    CHECK_INT(stepped(SET_ID_FPS_LIMIT, ten, &ten, 1, 20).number, 240);
    SettingValue back = stepped(SET_ID_FPS_LIMIT, stepped(SET_ID_FPS_LIMIT, ten, &ten, 1, 3), &ten, -1, 3);
    CHECK_INT(back.number, 10);

    // Opacity: 0-100% in fives; a file's 12.5% sits between 10% and 15%
    SettingValue odd = parsed(SET_ID_OVERLAY_OPACITY, "12.5%");
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, -1, 1).number, 1000);
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, 1, 1).number, 1500);
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, 1, 30).number, 10000);
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, -1, 30).number, 0);

    // Icon spacing: a file's px value comes before every percentage
    SettingValue px = parsed(SET_ID_ICON_SPACING, "40");
    SettingValue first_pct = stepped(SET_ID_ICON_SPACING, px, &px, 1, 1);
    CHECK(first_pct.percent);
    CHECK_INT(first_pct.number, 0);
    CHECK(!stepped(SET_ID_ICON_SPACING, first_pct, &px, -1, 1).percent);
    CHECK_INT(stepped(SET_ID_ICON_SPACING, px, &px, 1, 20).number, 1000);   // 10% at most

    // Vertical centre: 25-75% in fives
    SettingValue centre = parsed(SET_ID_VCENTER, "50%");
    CHECK_INT(stepped(SET_ID_VCENTER, centre, &centre, 1, 20).number, 7500);
    CHECK_INT(stepped(SET_ID_VCENTER, centre, &centre, -1, 20).number, 2500);

    // Padding: 0-20% in twos; a file's px sits first
    SettingValue padding = parsed(SET_ID_TITLE_PADDING, "8%");
    CHECK_INT(stepped(SET_ID_TITLE_PADDING, padding, &padding, 1, 1).number, 1000);
    CHECK_INT(stepped(SET_ID_TITLE_PADDING, padding, &padding, 1, 20).number, 2000);

    // A padding past the last step (300 px) stays reachable at the end
    SettingValue wide = parsed(SET_ID_HIGHLIGHT_HPADDING, "300");
    CHECK_INT(stepped(SET_ID_HIGHLIGHT_HPADDING, wide, &wide, -1, 1).number, 100);
    CHECK_INT(stepped(SET_ID_HIGHLIGHT_HPADDING, wide, &wide, 1, 1).number, 300);

    // Choices step through their own names only: Too long is Truncate or Shrink, and a file's None stays
    SettingValue none = parsed(SET_ID_TITLE_OVERSIZE, "None");
    CHECK_INT(stepped(SET_ID_TITLE_OVERSIZE, none, &none, -1, 1).number, 1);
    CHECK_INT(stepped(SET_ID_TITLE_OVERSIZE, none, &none, -1, 5).number, 0);

    // The spec's lists: app timeout, idle time, dim level, clock size
    SettingValue timeout = parsed(SET_ID_APPLICATION_TIMEOUT, "3");
    CHECK_INT(stepped(SET_ID_APPLICATION_TIMEOUT, timeout, &timeout, 1, 4).number, 20);
    SettingValue idle = parsed(SET_ID_SCREENSAVER_IDLE_TIME, "3");
    CHECK_INT(stepped(SET_ID_SCREENSAVER_IDLE_TIME, idle, &idle, 1, 5).number, 60);
    CHECK_INT(stepped(SET_ID_SCREENSAVER_IDLE_TIME, idle, &idle, 1, 20).number, 900);
    SettingValue dim = parsed(SET_ID_SCREENSAVER_INTENSITY, "70%");
    CHECK_INT(stepped(SET_ID_SCREENSAVER_INTENSITY, dim, &dim, -1, 20).number, 1000);
    SettingValue size = parsed(SET_ID_CLOCK_FONT_SIZE, "50");
    CHECK_INT(stepped(SET_ID_CLOCK_FONT_SIZE, size, &size, 1, 50).number, 120);
    CHECK_INT(stepped(SET_ID_CLOCK_FONT_SIZE, size, &size, -1, 50).number, 20);

    // Paths, fonts, commands, the default menu and the device are chosen elsewhere, never stepped here
    SettingValue cmd = parsed(SET_ID_STARTUP_CMD, ":quit");
    CHECK_STR(stepped(SET_ID_STARTUP_CMD, cmd, &cmd, 1, 1).text, ":quit");
    SettingValue device = parsed(SET_ID_GAMEPAD_DEVICE, "1");
    CHECK_INT(stepped(SET_ID_GAMEPAD_DEVICE, device, &device, 1, 1).number, 1);
}

// A function to test how the new types read on screen
static void test_new_descriptions(void)
{
    SettingValue value = parsed(SET_ID_WRAP_ENTRIES, "true");
    CHECK_STR(described(SET_ID_WRAP_ENTRIES, &value, NULL), "On");
    value = parsed(SET_ID_FPS_LIMIT, "60");
    CHECK_STR(described(SET_ID_FPS_LIMIT, &value, NULL), "60 fps");
    value.inherit = true;
    CHECK_STR(described(SET_ID_FPS_LIMIT, &value, NULL), "Off");
    value = parsed(SET_ID_OVERLAY_OPACITY, "12.5%");
    CHECK_STR(described(SET_ID_OVERLAY_OPACITY, &value, NULL), "12.5%");
    value = parsed(SET_ID_ICON_SPACING, "40");
    CHECK_STR(described(SET_ID_ICON_SPACING, &value, NULL), "40 px");
    value = parsed(SET_ID_HIGHLIGHT_HPADDING, "30");
    CHECK_STR(described(SET_ID_HIGHLIGHT_HPADDING, &value, NULL), "30 px");
    value = parsed(SET_ID_HIGHLIGHT_CORNER_RADIUS, "25");
    CHECK_STR(described(SET_ID_HIGHLIGHT_CORNER_RADIUS, &value, NULL), "25");
    value = parsed(SET_ID_APPLICATION_TIMEOUT, "15");
    CHECK_STR(described(SET_ID_APPLICATION_TIMEOUT, &value, NULL), "15 s");
    value = parsed(SET_ID_SCREENSAVER_IDLE_TIME, "300");
    CHECK_STR(described(SET_ID_SCREENSAVER_IDLE_TIME, &value, NULL), "5 min");
    value = parsed(SET_ID_ON_LAUNCH, "Blank");
    CHECK_STR(described(SET_ID_ON_LAUNCH, &value, NULL), "Blank screen");
    value = parsed(SET_ID_CLOCK_TIME_FORMAT, "12hr");
    CHECK_STR(described(SET_ID_CLOCK_TIME_FORMAT, &value, NULL), "2:05 PM");
    value = parsed(SET_ID_CLOCK_DATE_FORMAT, "Big");
    CHECK_STR(described(SET_ID_CLOCK_DATE_FORMAT, &value, NULL), "Sep 28");
    value = parsed(SET_ID_TITLE_FONT, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    CHECK_STR(described(SET_ID_TITLE_FONT, &value, NULL), "DejaVuSans.ttf");
    value = parsed(SET_ID_STARTUP_CMD, ":submenu Games");
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "Open submenu: Games");
    value = parsed(SET_ID_STARTUP_CMD, ":quit");
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "Quit StreamFlex");
    value = parsed(SET_ID_STARTUP_CMD, "kodi --standalone");
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "kodi --standalone");
    value.inherit = true;
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "None");
    value = parsed(SET_ID_GAMEPAD_DEVICE, "-1");
    CHECK_STR(described(SET_ID_GAMEPAD_DEVICE, &value, NULL), "Any");
    value = parsed(SET_ID_GAMEPAD_DEVICE, "1");
    CHECK_STR(described(SET_ID_GAMEPAD_DEVICE, &value, NULL), "Pad 1");

    char label[64];
    setting_command_label(":select", label, sizeof(label));
    CHECK_STR(label, "OK");
    setting_command_label(":exit", label, sizeof(label));
    CHECK_STR(label, "Close the app on show");
}

// A function to test finding a setting by section and key, as the parser does
static void test_find(void)
{
    CHECK(setting_find("Titles", "Color") == setting_def(SET_ID_TITLE_COLOR));
    CHECK(setting_find("Background", "Color") == setting_def(SET_ID_BACKGROUND_COLOR));
    CHECK(setting_find("Highlight", "Enabled") == setting_def(SET_ID_HIGHLIGHT_ENABLED));
    CHECK(setting_find("Scroll Indicators", "Enabled") == setting_def(SET_ID_SCROLL_ENABLED));
    CHECK(setting_find("Layout", "MaxButtons") == setting_def(SET_ID_LAYOUT_COLUMNS));   // Its alias
    CHECK(setting_find("Titles", "FontFace") == setting_def(SET_ID_TITLE_FONT_FACE));
    CHECK(setting_find("Clock", "FontFace") == setting_def(SET_ID_CLOCK_FONT_FACE));
    CHECK(setting_find("General", "Nope") == NULL);
    CHECK(setting_find("Main", "Rows") == NULL);                   // Per-menu keys belong to menu sections
    CHECK(setting_find("Hotkeys", "Hotkey1") == NULL);

    // Every global setting is found by its own section and key, and has a label and a section
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        const SettingDef *def = setting_def((SettingId) id);
        CHECK_INT((int) def->id, id);
        CHECK(def->section != NULL && def->label != NULL && def->key != NULL);
        CHECK(setting_find(def->section, def->key) == def);
    }
}
```

and call them from `main()`, after the existing calls:

```c
    test_new_round_trips();
    test_new_rejects();
    test_new_steps();
    test_new_descriptions();
    test_find();
```

- [ ] **Step 3: Run the tests to see them fail**

Run:

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_settings
```

Expected: the build fails with `'SET_ID_WRAP_ENTRIES': undeclared identifier` and the same for the other new names.

- [ ] **Step 4: Give `settings.h` the new types, ids and fields**

Replace `src/settings.h` lines 15-78 (from `typedef enum {` for `SettingType` through the end of `SettingDef`) with:

```c
typedef enum {
    SET_TYPE_COUNT,       // A whole number: Rows, Columns
    SET_TYPE_CHOICE,      // One of the setting's names (def->names): Mode, OnLaunch, OversizeMode
    SET_TYPE_ICON_SIZE,   // IconSize: Fill (no key), or px
    SET_TYPE_COLOR,       // #RRGGBB, stepped through the presets
    SET_TYPE_PATH,        // A file or folder, chosen in the browser, never stepped
    SET_TYPE_SECONDS,     // Whole seconds, stepped through def->steps
    SET_TYPE_MILLIS,      // Seconds with decimals in the file, ms here: SlideshowTransitionTime
    SET_TYPE_TITLE_SIZE,  // FontSize: a percentage of the button, or a fixed size
    SET_TYPE_BOOL,        // true or false, shown On or Off
    SET_TYPE_NUMBER,      // A whole number with limits: OutlineSize, FPSLimit, FontFace
    SET_TYPE_PERCENT,     // "N%" with up to two decimals (number in hundredths), or px where allowed
    SET_TYPE_COMMAND,     // A command, chosen in the command picker
    SET_TYPE_FONT,        // A font file, chosen in the font picker
    SET_TYPE_MENU,        // A menu's name: DefaultMenu, stepped through the menus by the pages
    SET_TYPE_DEVICE       // A gamepad's device index, -1 for any, stepped through the pads by the pages
} SettingType;

#define SET_FLAG_PX 1          // PERCENT: a plain whole number in the file is px
#define SET_FLAG_WHOLE 2       // PERCENT: whole percentages only (Padding, which layout.c reads whole)
#define SET_FLAG_HIDDEN 4      // No row of its own: FontFace, set by the font picker with its font
#define SET_FLAG_NEXT_START 8  // Applies at next start: ControllerMappingsFile

typedef enum {
    SET_REFRESH_NONE,        // The launcher reads the value when it next needs it
    SET_REFRESH_LAYOUT,      // Lay the menu on show out again
    SET_REFRESH_TITLES,      // Render every menu's titles again
    SET_REFRESH_BACKGROUND,  // Set the background up again, its overlay included
    SET_REFRESH_TITLE_FONT,  // Open the title font again, then render the titles
    SET_REFRESH_HIGHLIGHT,   // Render the highlight again
    SET_REFRESH_SCROLL,      // Render the scroll indicators again
    SET_REFRESH_CLOCK,       // Restart the clock, then lay the menu out again around it
    SET_REFRESH_SCREENSAVER, // Restart the screensaver
    SET_REFRESH_GAMEPAD,     // Restart the gamepad
    SET_REFRESH_FRAME        // Work the frame timing out again: VSync and FPSLimit
} SettingRefresh;

typedef enum {
    SET_ID_BACKGROUND_MODE,
    SET_ID_BACKGROUND_COLOR,
    SET_ID_BACKGROUND_IMAGE,
    SET_ID_SLIDESHOW_DIRECTORY,
    SET_ID_SLIDESHOW_DURATION,
    SET_ID_SLIDESHOW_FADE,
    SET_ID_LAYOUT_ROWS,
    SET_ID_LAYOUT_COLUMNS,
    SET_ID_LAYOUT_ICON_SIZE,
    SET_ID_TITLE_SIZE,
    // [General]
    SET_ID_DEFAULT_MENU,
    SET_ID_WRAP_ENTRIES,
    SET_ID_RESET_ON_BACK,
    SET_ID_MOUSE_SELECT,
    SET_ID_INHIBIT_OS_SCREENSAVER,
    SET_ID_VSYNC,
    SET_ID_FPS_LIMIT,
    SET_ID_ON_LAUNCH,
    SET_ID_APPLICATION_TIMEOUT,
    SET_ID_STARTUP_CMD,
    SET_ID_QUIT_CMD,
    // [Background], beyond 3a's
    SET_ID_CHROMA_KEY_COLOR,
    SET_ID_OVERLAY,
    SET_ID_OVERLAY_COLOR,
    SET_ID_OVERLAY_OPACITY,
    // [Layout], beyond 3a's
    SET_ID_ICON_SPACING,
    SET_ID_VCENTER,
    // [Titles], beyond 3a's
    SET_ID_TITLES_ENABLED,
    SET_ID_TITLE_FONT,
    SET_ID_TITLE_FONT_FACE,
    SET_ID_TITLE_COLOR,
    SET_ID_TITLE_OPACITY,
    SET_ID_TITLE_SHADOWS,
    SET_ID_TITLE_SHADOW_COLOR,
    SET_ID_TITLE_OVERSIZE,
    SET_ID_TITLE_PADDING,
    // [Highlight]
    SET_ID_HIGHLIGHT_ENABLED,
    SET_ID_HIGHLIGHT_FILL_COLOR,
    SET_ID_HIGHLIGHT_FILL_OPACITY,
    SET_ID_HIGHLIGHT_OUTLINE_SIZE,
    SET_ID_HIGHLIGHT_OUTLINE_COLOR,
    SET_ID_HIGHLIGHT_OUTLINE_OPACITY,
    SET_ID_HIGHLIGHT_CORNER_RADIUS,
    SET_ID_HIGHLIGHT_VPADDING,
    SET_ID_HIGHLIGHT_HPADDING,
    // [Scroll Indicators]
    SET_ID_SCROLL_ENABLED,
    SET_ID_SCROLL_FILL_COLOR,
    SET_ID_SCROLL_OUTLINE_SIZE,
    SET_ID_SCROLL_OUTLINE_COLOR,
    SET_ID_SCROLL_OPACITY,
    // [Clock]
    SET_ID_CLOCK_ENABLED,
    SET_ID_CLOCK_SHOW_DATE,
    SET_ID_CLOCK_ALIGNMENT,
    SET_ID_CLOCK_FONT,
    SET_ID_CLOCK_FONT_FACE,
    SET_ID_CLOCK_COLOR,
    SET_ID_CLOCK_SHADOWS,
    SET_ID_CLOCK_SHADOW_COLOR,
    SET_ID_CLOCK_OPACITY,
    SET_ID_CLOCK_FONT_SIZE,
    SET_ID_CLOCK_MARGIN,
    SET_ID_CLOCK_TIME_FORMAT,
    SET_ID_CLOCK_DATE_FORMAT,
    SET_ID_CLOCK_WEEKDAY,
    // [Screensaver]
    SET_ID_SCREENSAVER_ENABLED,
    SET_ID_SCREENSAVER_IDLE_TIME,
    SET_ID_SCREENSAVER_INTENSITY,
    SET_ID_SCREENSAVER_PAUSE,
    // [Gamepad]
    SET_ID_GAMEPAD_ENABLED,
    SET_ID_GAMEPAD_DEVICE,
    SET_ID_GAMEPAD_MAPPINGS,
    SET_ID_MENU_ROWS,        // The per-menu settings come last: one of each for every menu
    SET_ID_MENU_COLUMNS,
    SET_ID_MENU_ICON_SIZE,
    SET_ID_COUNT
} SettingId;

#define SET_ID_GLOBAL_COUNT SET_ID_MENU_ROWS
#define SET_ID_PER_MENU_COUNT (SET_ID_COUNT - SET_ID_MENU_ROWS)

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} SettingColor;

typedef struct {
    bool inherit;                // The key is absent: a menu follows [Layout], IconSize is Fill, FPSLimit is Off
    int number;                  // COUNT, CHOICE (index), ICON_SIZE (px), SECONDS (s), MILLIS (ms), TITLE_SIZE,
                                 // BOOL (1/0), NUMBER, DEVICE (-1 any), PERCENT (hundredths, or px)
    bool percent;                // TITLE_SIZE, PERCENT: `number` is a percentage (PERCENT: in hundredths)
    SettingColor color;          // COLOR
    char text[SETTING_TEXT_MAX]; // PATH, FONT, COMMAND, MENU; "" when none is chosen
} SettingValue;

typedef struct {
    SettingId id;
    const char *label;           // The row's label on screen
    const char *section;         // NULL: the section of the menu being edited
    const char *key;
    const char *alias;           // An older key the parser reads as this one (MaxButtons); NULL if none
    SettingType type;
    int min;                     // What the file may hold: COUNT, NUMBER, DEVICE, SECONDS, MILLIS;
    int max;                     // PERCENT's percentages, in hundredths
    bool can_inherit;            // The lowest step removes the key
    SettingRefresh refresh;
    int lo;                      // The steps: NUMBER and PERCENT from lo to hi by `step`, CHOICE the
    int hi;                      // names from index lo to hi
    int step;
    const int *steps;            // The steps as a list instead (SECONDS, NUMBER); NULL for none
    int step_count;
    const char *const *names;    // CHOICE: the file's names, NULL-terminated...
    const char *const *labels;   // ...and the screen's, one for each
    const char *legacy;          // CHOICE: an older spelling the parser also reads; NULL if none...
    int legacy_index;            // ...and the name it stands for
    const char *unit;            // NUMBER: after the number on screen (" px"); NULL for none
    int max_px;                  // PERCENT with SET_FLAG_PX: the largest px the file may hold
    int flags;                   // SET_FLAG_*
    const char *fallback;        // The built-in value as the file would write it, for Config's start; NULL:
                                 // launcher.c's initializer holds it
    const char *inherit_label;   // What the lowest step shows when it removes the key ("Off", "None")
} SettingDef;
```

Replace the declarations of the table functions (lines 155-160, from `const SettingDef *setting_def(SettingId id);` to `void setting_describe(...)`) with:

```c
const SettingDef *setting_def(SettingId id);
const SettingDef *setting_find(const char *section, const char *key);
bool setting_parse(const SettingDef *def, const char *text, SettingValue *value);
void setting_format(const SettingDef *def, const SettingValue *value, char *out, size_t size);
bool setting_equal(const SettingDef *def, const SettingValue *a, const SettingValue *b);
SettingValue setting_step(const SettingDef *def, const SettingValue *current, const SettingValue *entry, int direction);
void setting_describe(const SettingDef *def, const SettingValue *value, const SettingValue *inherited, char *out, size_t size);
void setting_command_label(const char *command, char *out, size_t size);
```

- [ ] **Step 5: Give `settings.c` the names, the steps and the whole table**

Replace `src/settings.c` lines 15-60 (from the `MODE_NAMES` comment through the end of `DEFS`) with:

```c
// The names each choice setting reads and writes, NULL-terminated, and what the screen calls them
static const char *const MODE_NAMES[] = { "Color", "Image", "Slideshow", "Transparent", NULL };
static const char *const MODE_LABELS[] = { "Colour", "Image", "Slideshow", "Transparent" };
static const char *const ON_LAUNCH_NAMES[] = { "Blank", "None", "Quit", NULL };
static const char *const ON_LAUNCH_LABELS[] = { "Blank screen", "Keep showing", "Quit" };
static const char *const OVERSIZE_NAMES[] = { "Truncate", "Shrink", "None", NULL };
static const char *const OVERSIZE_LABELS[] = { "Truncate", "Shrink", "Leave as is" };
static const char *const ALIGNMENT_NAMES[] = { "Left", "Right", NULL };
static const char *const ALIGNMENT_LABELS[] = { "Left", "Right" };
static const char *const TIME_NAMES[] = { "24hr", "12hr", "Auto", NULL };
static const char *const TIME_LABELS[] = { "14:05", "2:05 PM", "Auto" };
static const char *const DATE_NAMES[] = { "Big", "Little", "Auto", NULL };
static const char *const DATE_LABELS[] = { "Sep 28", "28 Sep", "Auto" };
#define MODE_IMAGE 1
#define MODE_SLIDESHOW 2

static const struct {
    const char *name;
    SettingColor color;
} PRESETS[] = {
    { "Black",    { 0x00, 0x00, 0x00 } },
    { "Charcoal", { 0x1E, 0x1E, 0x1E } },
    { "Graphite", { 0x33, 0x38, 0x3D } },
    { "Slate",    { 0x2E, 0x34, 0x40 } },
    { "Midnight", { 0x12, 0x1A, 0x2E } },
    { "Navy",     { 0x0B, 0x1F, 0x3A } },
    { "Teal",     { 0x07, 0x60, 0x6C } },
    { "Forest",   { 0x1E, 0x3B, 0x2F } },
    { "Plum",     { 0x3B, 0x1F, 0x3A } },
    { "Burgundy", { 0x4A, 0x15, 0x20 } }
};

static const int ICON_STEPS[] = { 64, 96, 128, 160, 192, 256, 320, 384, 512, 768, 1024 };
static const int SECOND_STEPS[] = { 5, 10, 15, 30, 60, 120, 300, 600, 1800, 3600 };
static const int MILLI_STEPS[] = { 0, 500, 1000, 1500, 2000, 2500, 3000 };
static const int TITLE_STEPS[] = { 11, 14, 17 };  // Small, Medium, Large
static const int TIMEOUT_STEPS[] = { 3, 5, 10, 15, 20, 30 };
static const int IDLE_STEPS[] = { 3, 5, 10, 15, 30, 60, 120, 300, 600, 900 };
static const int FPS_STEPS[] = { 30, 60, 75, 120, 144, 165, 240 };

static const char *const TRANSPARENT_NOTE =
    "The desktop shows through. On Linux this needs a compositor: see Transparent Backgrounds in the configuration docs.";
static const char *const MENU_NOTE = "The lowest step, All menus, follows the shared grid.";

// Rows marked with the same comment belong to the same page (Task 5 lays the pages out)
static const SettingDef DEFS[SET_ID_COUNT] = {
    // 3a's
    [SET_ID_BACKGROUND_MODE] = { .id = SET_ID_BACKGROUND_MODE, .label = "Mode", .section = "Background",
        .key = SETTING_BACKGROUND_MODE, .type = SET_TYPE_CHOICE, .refresh = SET_REFRESH_BACKGROUND,
        .lo = 0, .hi = 3, .names = MODE_NAMES, .labels = MODE_LABELS },
    [SET_ID_BACKGROUND_COLOR] = { .id = SET_ID_BACKGROUND_COLOR, .label = "Colour", .section = "Background",
        .key = SETTING_BACKGROUND_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_BACKGROUND_IMAGE] = { .id = SET_ID_BACKGROUND_IMAGE, .label = "Image", .section = "Background",
        .key = SETTING_BACKGROUND_IMAGE, .type = SET_TYPE_PATH, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_SLIDESHOW_DIRECTORY] = { .id = SET_ID_SLIDESHOW_DIRECTORY, .label = "Folder", .section = "Background",
        .key = SETTING_SLIDESHOW_DIRECTORY, .type = SET_TYPE_PATH, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_SLIDESHOW_DURATION] = { .id = SET_ID_SLIDESHOW_DURATION, .label = "Change every", .section = "Background",
        .key = SETTING_SLIDESHOW_IMAGE_DURATION, .type = SET_TYPE_SECONDS, .min = 5, .max = 3600,
        .steps = SECOND_STEPS, .step_count = LENGTH(SECOND_STEPS) },
    [SET_ID_SLIDESHOW_FADE] = { .id = SET_ID_SLIDESHOW_FADE, .label = "Fade", .section = "Background",
        .key = SETTING_SLIDESHOW_TRANSITION_TIME, .type = SET_TYPE_MILLIS, .min = 0, .max = 3000 },
    [SET_ID_LAYOUT_ROWS] = { .id = SET_ID_LAYOUT_ROWS, .label = "Rows", .section = "Layout", .key = SETTING_ROWS,
        .type = SET_TYPE_COUNT, .min = 1, .max = 10, .refresh = SET_REFRESH_LAYOUT },
    [SET_ID_LAYOUT_COLUMNS] = { .id = SET_ID_LAYOUT_COLUMNS, .label = "Columns", .section = "Layout",
        .key = SETTING_COLUMNS, .alias = SETTING_MAX_BUTTONS, .type = SET_TYPE_COUNT, .min = 1, .max = 12,
        .refresh = SET_REFRESH_LAYOUT },
    [SET_ID_LAYOUT_ICON_SIZE] = { .id = SET_ID_LAYOUT_ICON_SIZE, .label = "Largest button", .section = "Layout",
        .key = SETTING_ICON_SIZE, .type = SET_TYPE_ICON_SIZE, .can_inherit = true, .refresh = SET_REFRESH_LAYOUT },
    [SET_ID_TITLE_SIZE] = { .id = SET_ID_TITLE_SIZE, .label = "Size", .section = "Titles",
        .key = SETTING_TITLE_FONT_SIZE, .type = SET_TYPE_TITLE_SIZE, .refresh = SET_REFRESH_TITLES },

    // General
    [SET_ID_DEFAULT_MENU] = { .id = SET_ID_DEFAULT_MENU, .label = "Default menu", .section = "General",
        .key = SETTING_DEFAULT_MENU, .type = SET_TYPE_MENU },
    [SET_ID_WRAP_ENTRIES] = { .id = SET_ID_WRAP_ENTRIES, .label = "Wrap around", .section = "General",
        .key = SETTING_WRAP_ENTRIES, .type = SET_TYPE_BOOL },
    [SET_ID_RESET_ON_BACK] = { .id = SET_ID_RESET_ON_BACK, .label = "Reset on Back", .section = "General",
        .key = SETTING_RESET_ON_BACK, .type = SET_TYPE_BOOL },
    [SET_ID_MOUSE_SELECT] = { .id = SET_ID_MOUSE_SELECT, .label = "Mouse select", .section = "General",
        .key = SETTING_MOUSE_SELECT, .type = SET_TYPE_BOOL },
    [SET_ID_INHIBIT_OS_SCREENSAVER] = { .id = SET_ID_INHIBIT_OS_SCREENSAVER, .label = "Block the OS screensaver",
        .section = "General", .key = SETTING_INHIBIT_OS_SCREENSAVER, .type = SET_TYPE_BOOL },
    [SET_ID_VSYNC] = { .id = SET_ID_VSYNC, .label = "VSync", .section = "General", .key = SETTING_VSYNC,
        .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_FRAME },
    [SET_ID_FPS_LIMIT] = { .id = SET_ID_FPS_LIMIT, .label = "FPS limit", .section = "General",
        .key = SETTING_FPS_LIMIT, .type = SET_TYPE_NUMBER, .min = 10, .max = 1000, .can_inherit = true,
        .refresh = SET_REFRESH_FRAME, .steps = FPS_STEPS, .step_count = LENGTH(FPS_STEPS), .unit = " fps",
        .inherit_label = "Off" },
    [SET_ID_ON_LAUNCH] = { .id = SET_ID_ON_LAUNCH, .label = "After launching an app", .section = "General",
        .key = SETTING_ON_LAUNCH, .type = SET_TYPE_CHOICE, .lo = 0, .hi = 2, .names = ON_LAUNCH_NAMES,
        .labels = ON_LAUNCH_LABELS },
    [SET_ID_APPLICATION_TIMEOUT] = { .id = SET_ID_APPLICATION_TIMEOUT, .label = "App timeout", .section = "General",
        .key = SETTING_APPLICATION_TIMEOUT, .type = SET_TYPE_SECONDS, .min = 3, .max = 30,
        .steps = TIMEOUT_STEPS, .step_count = LENGTH(TIMEOUT_STEPS) },
    [SET_ID_STARTUP_CMD] = { .id = SET_ID_STARTUP_CMD, .label = "Startup command", .section = "General",
        .key = SETTING_STARTUP_CMD, .type = SET_TYPE_COMMAND, .can_inherit = true, .inherit_label = "None" },
    [SET_ID_QUIT_CMD] = { .id = SET_ID_QUIT_CMD, .label = "Quit command", .section = "General",
        .key = SETTING_QUIT_CMD, .type = SET_TYPE_COMMAND, .can_inherit = true, .inherit_label = "None" },

    // Background, beyond 3a's
    [SET_ID_CHROMA_KEY_COLOR] = { .id = SET_ID_CHROMA_KEY_COLOR, .label = "See-through colour", .section = "Background",
        .key = SETTING_CHROMA_KEY_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_OVERLAY] = { .id = SET_ID_OVERLAY, .label = "Overlay", .section = "Background",
        .key = SETTING_BACKGROUND_OVERLAY, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_OVERLAY_COLOR] = { .id = SET_ID_OVERLAY_COLOR, .label = "Overlay colour", .section = "Background",
        .key = SETTING_BACKGROUND_OVERLAY_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_OVERLAY_OPACITY] = { .id = SET_ID_OVERLAY_OPACITY, .label = "Overlay opacity", .section = "Background",
        .key = SETTING_BACKGROUND_OVERLAY_OPACITY, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000,
        .refresh = SET_REFRESH_BACKGROUND, .lo = 0, .hi = 10000, .step = 500,
        .fallback = DEFAULT_BACKGROUND_OVERLAY_OPACITY },

    // Layout, beyond 3a's (the All menus page)
    [SET_ID_ICON_SPACING] = { .id = SET_ID_ICON_SPACING, .label = "Icon spacing", .section = "Layout",
        .key = SETTING_ICON_SPACING, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000, .refresh = SET_REFRESH_LAYOUT,
        .lo = 0, .hi = 1000, .step = 100, .max_px = INT_MAX, .flags = SET_FLAG_PX, .fallback = DEFAULT_ICON_SPACING },
    [SET_ID_VCENTER] = { .id = SET_ID_VCENTER, .label = "Vertical centre", .section = "Layout",
        .key = SETTING_VCENTER, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000, .refresh = SET_REFRESH_LAYOUT,
        .lo = 2500, .hi = 7500, .step = 500, .fallback = DEFAULT_VCENTER },

    // Titles, beyond 3a's
    [SET_ID_TITLES_ENABLED] = { .id = SET_ID_TITLES_ENABLED, .label = "Show titles", .section = "Titles",
        .key = SETTING_TITLES_ENABLED, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_TITLE_FONT },
    [SET_ID_TITLE_FONT] = { .id = SET_ID_TITLE_FONT, .label = "Font", .section = "Titles",
        .key = SETTING_TITLE_FONT, .type = SET_TYPE_FONT, .refresh = SET_REFRESH_TITLE_FONT },
    [SET_ID_TITLE_FONT_FACE] = { .id = SET_ID_TITLE_FONT_FACE, .label = "Font face", .section = "Titles",
        .key = SETTING_TITLE_FONT_FACE, .type = SET_TYPE_NUMBER, .min = 0, .max = 65535, .can_inherit = true,
        .refresh = SET_REFRESH_TITLE_FONT, .flags = SET_FLAG_HIDDEN, .inherit_label = "0" },
    [SET_ID_TITLE_COLOR] = { .id = SET_ID_TITLE_COLOR, .label = "Colour", .section = "Titles",
        .key = SETTING_TITLE_FONT_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_TITLES },
    [SET_ID_TITLE_OPACITY] = { .id = SET_ID_TITLE_OPACITY, .label = "Opacity", .section = "Titles",
        .key = SETTING_TITLE_OPACITY, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000, .refresh = SET_REFRESH_TITLES,
        .lo = 0, .hi = 10000, .step = 500, .fallback = DEFAULT_TITLE_OPACITY },
    [SET_ID_TITLE_SHADOWS] = { .id = SET_ID_TITLE_SHADOWS, .label = "Shadows", .section = "Titles",
        .key = SETTING_TITLE_SHADOWS, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_TITLES },
    [SET_ID_TITLE_SHADOW_COLOR] = { .id = SET_ID_TITLE_SHADOW_COLOR, .label = "Shadow colour", .section = "Titles",
        .key = SETTING_TITLE_SHADOW_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_TITLES },
    [SET_ID_TITLE_OVERSIZE] = { .id = SET_ID_TITLE_OVERSIZE, .label = "Too long", .section = "Titles",
        .key = SETTING_TITLE_OVERSIZE_MODE, .type = SET_TYPE_CHOICE, .refresh = SET_REFRESH_TITLES,
        .lo = 0, .hi = 1, .names = OVERSIZE_NAMES, .labels = OVERSIZE_LABELS, .legacy = "Truncated", .legacy_index = 0 },
    [SET_ID_TITLE_PADDING] = { .id = SET_ID_TITLE_PADDING, .label = "Padding", .section = "Titles",
        .key = SETTING_TITLE_PADDING, .type = SET_TYPE_PERCENT, .min = 0, .max = 5000, .refresh = SET_REFRESH_TITLES,
        .lo = 0, .hi = 2000, .step = 200, .max_px = LAYOUT_MAX_BUTTON, .flags = SET_FLAG_PX | SET_FLAG_WHOLE },

    // Highlight
    [SET_ID_HIGHLIGHT_ENABLED] = { .id = SET_ID_HIGHLIGHT_ENABLED, .label = "Show", .section = "Highlight",
        .key = SETTING_HIGHLIGHT_ENABLED, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_HIGHLIGHT },
    [SET_ID_HIGHLIGHT_FILL_COLOR] = { .id = SET_ID_HIGHLIGHT_FILL_COLOR, .label = "Fill colour", .section = "Highlight",
        .key = SETTING_HIGHLIGHT_FILL_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_HIGHLIGHT },
    [SET_ID_HIGHLIGHT_FILL_OPACITY] = { .id = SET_ID_HIGHLIGHT_FILL_OPACITY, .label = "Fill opacity",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_FILL_OPACITY, .type = SET_TYPE_PERCENT, .min = 0,
        .max = 10000, .refresh = SET_REFRESH_HIGHLIGHT, .lo = 0, .hi = 10000, .step = 500,
        .fallback = DEFAULT_HIGHLIGHT_FILL_OPACITY },
    [SET_ID_HIGHLIGHT_OUTLINE_SIZE] = { .id = SET_ID_HIGHLIGHT_OUTLINE_SIZE, .label = "Outline size",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_OUTLINE_SIZE, .type = SET_TYPE_NUMBER, .min = 0,
        .max = INT_MAX, .refresh = SET_REFRESH_HIGHLIGHT, .lo = 0, .hi = 10, .step = 1, .unit = " px" },
    [SET_ID_HIGHLIGHT_OUTLINE_COLOR] = { .id = SET_ID_HIGHLIGHT_OUTLINE_COLOR, .label = "Outline colour",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_OUTLINE_COLOR, .type = SET_TYPE_COLOR,
        .refresh = SET_REFRESH_HIGHLIGHT },
    [SET_ID_HIGHLIGHT_OUTLINE_OPACITY] = { .id = SET_ID_HIGHLIGHT_OUTLINE_OPACITY, .label = "Outline opacity",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_OUTLINE_OPACITY, .type = SET_TYPE_PERCENT, .min = 0,
        .max = 10000, .refresh = SET_REFRESH_HIGHLIGHT, .lo = 0, .hi = 10000, .step = 500,
        .fallback = DEFAULT_HIGHLIGHT_OUTLINE_OPACITY },
    [SET_ID_HIGHLIGHT_CORNER_RADIUS] = { .id = SET_ID_HIGHLIGHT_CORNER_RADIUS, .label = "Corner radius",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_CORNER_RADIUS, .type = SET_TYPE_NUMBER, .min = 0,
        .max = 100, .refresh = SET_REFRESH_HIGHLIGHT, .lo = 0, .hi = 100, .step = 5 },
    [SET_ID_HIGHLIGHT_VPADDING] = { .id = SET_ID_HIGHLIGHT_VPADDING, .label = "Vertical padding",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_VPADDING, .type = SET_TYPE_NUMBER, .min = 0,
        .max = INT_MAX, .refresh = SET_REFRESH_LAYOUT, .lo = 0, .hi = 100, .step = 5, .unit = " px" },
    [SET_ID_HIGHLIGHT_HPADDING] = { .id = SET_ID_HIGHLIGHT_HPADDING, .label = "Horizontal padding",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_HPADDING, .type = SET_TYPE_NUMBER, .min = 0,
        .max = INT_MAX, .refresh = SET_REFRESH_LAYOUT, .lo = 0, .hi = 100, .step = 5, .unit = " px" },

    // Scroll indicators
    [SET_ID_SCROLL_ENABLED] = { .id = SET_ID_SCROLL_ENABLED, .label = "Show", .section = "Scroll Indicators",
        .key = SETTING_SCROLL_INDICATORS, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_SCROLL },
    [SET_ID_SCROLL_FILL_COLOR] = { .id = SET_ID_SCROLL_FILL_COLOR, .label = "Fill colour",
        .section = "Scroll Indicators", .key = SETTING_SCROLL_INDICATOR_FILL_COLOR, .type = SET_TYPE_COLOR,
        .refresh = SET_REFRESH_SCROLL },
    [SET_ID_SCROLL_OUTLINE_SIZE] = { .id = SET_ID_SCROLL_OUTLINE_SIZE, .label = "Outline size",
        .section = "Scroll Indicators", .key = SETTING_SCROLL_INDICATOR_OUTLINE_SIZE, .type = SET_TYPE_NUMBER,
        .min = 0, .max = INT_MAX, .refresh = SET_REFRESH_SCROLL, .lo = 0, .hi = 10, .step = 1, .unit = " px" },
    [SET_ID_SCROLL_OUTLINE_COLOR] = { .id = SET_ID_SCROLL_OUTLINE_COLOR, .label = "Outline colour",
        .section = "Scroll Indicators", .key = SETTING_SCROLL_INDICATOR_OUTLINE_COLOR, .type = SET_TYPE_COLOR,
        .refresh = SET_REFRESH_SCROLL },
    [SET_ID_SCROLL_OPACITY] = { .id = SET_ID_SCROLL_OPACITY, .label = "Opacity", .section = "Scroll Indicators",
        .key = SETTING_SCROLL_INDICATOR_OPACITY, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000,
        .refresh = SET_REFRESH_SCROLL, .lo = 0, .hi = 10000, .step = 500, .fallback = DEFAULT_SCROLL_INDICATOR_OPACITY },

    // Clock
    [SET_ID_CLOCK_ENABLED] = { .id = SET_ID_CLOCK_ENABLED, .label = "Show", .section = "Clock",
        .key = SETTING_CLOCK_ENABLED, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_SHOW_DATE] = { .id = SET_ID_CLOCK_SHOW_DATE, .label = "Show date", .section = "Clock",
        .key = SETTING_CLOCK_SHOW_DATE, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_ALIGNMENT] = { .id = SET_ID_CLOCK_ALIGNMENT, .label = "Alignment", .section = "Clock",
        .key = SETTING_CLOCK_ALIGNMENT, .type = SET_TYPE_CHOICE, .refresh = SET_REFRESH_CLOCK, .lo = 0, .hi = 1,
        .names = ALIGNMENT_NAMES, .labels = ALIGNMENT_LABELS },
    [SET_ID_CLOCK_FONT] = { .id = SET_ID_CLOCK_FONT, .label = "Font", .section = "Clock", .key = SETTING_CLOCK_FONT,
        .type = SET_TYPE_FONT, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_FONT_FACE] = { .id = SET_ID_CLOCK_FONT_FACE, .label = "Font face", .section = "Clock",
        .key = SETTING_CLOCK_FONT_FACE, .type = SET_TYPE_NUMBER, .min = 0, .max = 65535, .can_inherit = true,
        .refresh = SET_REFRESH_CLOCK, .flags = SET_FLAG_HIDDEN, .inherit_label = "0" },
    [SET_ID_CLOCK_COLOR] = { .id = SET_ID_CLOCK_COLOR, .label = "Colour", .section = "Clock",
        .key = SETTING_CLOCK_FONT_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_SHADOWS] = { .id = SET_ID_CLOCK_SHADOWS, .label = "Shadows", .section = "Clock",
        .key = SETTING_CLOCK_SHADOWS, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_SHADOW_COLOR] = { .id = SET_ID_CLOCK_SHADOW_COLOR, .label = "Shadow colour", .section = "Clock",
        .key = SETTING_CLOCK_SHADOW_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_OPACITY] = { .id = SET_ID_CLOCK_OPACITY, .label = "Opacity", .section = "Clock",
        .key = SETTING_CLOCK_OPACITY, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000, .refresh = SET_REFRESH_CLOCK,
        .lo = 0, .hi = 10000, .step = 500, .fallback = DEFAULT_CLOCK_OPACITY },
    [SET_ID_CLOCK_FONT_SIZE] = { .id = SET_ID_CLOCK_FONT_SIZE, .label = "Size", .section = "Clock",
        .key = SETTING_CLOCK_FONT_SIZE, .type = SET_TYPE_NUMBER, .min = 1, .max = INT_MAX,
        .refresh = SET_REFRESH_CLOCK, .lo = 20, .hi = 120, .step = 5 },
    [SET_ID_CLOCK_MARGIN] = { .id = SET_ID_CLOCK_MARGIN, .label = "Margin", .section = "Clock",
        .key = SETTING_CLOCK_MARGIN, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000, .refresh = SET_REFRESH_CLOCK,
        .lo = 0, .hi = 1000, .step = 100, .max_px = INT_MAX, .flags = SET_FLAG_PX, .fallback = DEFAULT_CLOCK_MARGIN },
    [SET_ID_CLOCK_TIME_FORMAT] = { .id = SET_ID_CLOCK_TIME_FORMAT, .label = "Time", .section = "Clock",
        .key = SETTING_CLOCK_TIME_FORMAT, .type = SET_TYPE_CHOICE, .refresh = SET_REFRESH_CLOCK, .lo = 0, .hi = 2,
        .names = TIME_NAMES, .labels = TIME_LABELS },
    [SET_ID_CLOCK_DATE_FORMAT] = { .id = SET_ID_CLOCK_DATE_FORMAT, .label = "Date", .section = "Clock",
        .key = SETTING_CLOCK_DATE_FORMAT, .type = SET_TYPE_CHOICE, .refresh = SET_REFRESH_CLOCK, .lo = 0, .hi = 2,
        .names = DATE_NAMES, .labels = DATE_LABELS },
    [SET_ID_CLOCK_WEEKDAY] = { .id = SET_ID_CLOCK_WEEKDAY, .label = "Weekday", .section = "Clock",
        .key = SETTING_CLOCK_INCLUDE_WEEKDAY, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_CLOCK },

    // Screensaver
    [SET_ID_SCREENSAVER_ENABLED] = { .id = SET_ID_SCREENSAVER_ENABLED, .label = "On", .section = "Screensaver",
        .key = SETTING_SCREENSAVER_ENABLED, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_SCREENSAVER },
    [SET_ID_SCREENSAVER_IDLE_TIME] = { .id = SET_ID_SCREENSAVER_IDLE_TIME, .label = "Idle time",
        .section = "Screensaver", .key = SETTING_SCREENSAVER_IDLE_TIME, .type = SET_TYPE_SECONDS, .min = 3,
        .max = 900, .steps = IDLE_STEPS, .step_count = LENGTH(IDLE_STEPS) },
    [SET_ID_SCREENSAVER_INTENSITY] = { .id = SET_ID_SCREENSAVER_INTENSITY, .label = "Dim level",
        .section = "Screensaver", .key = SETTING_SCREENSAVER_INTENSITY, .type = SET_TYPE_PERCENT, .min = 0,
        .max = 10000, .refresh = SET_REFRESH_SCREENSAVER, .lo = 1000, .hi = 10000, .step = 1000,
        .fallback = DEFAULT_SCREENSAVER_INTENSITY },
    [SET_ID_SCREENSAVER_PAUSE] = { .id = SET_ID_SCREENSAVER_PAUSE, .label = "Pause slideshow",
        .section = "Screensaver", .key = SETTING_SCREENSAVER_PAUSE_SLIDESHOW, .type = SET_TYPE_BOOL },

    // Gamepad
    [SET_ID_GAMEPAD_ENABLED] = { .id = SET_ID_GAMEPAD_ENABLED, .label = "On", .section = "Gamepad",
        .key = SETTING_GAMEPAD_ENABLED, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_GAMEPAD },
    [SET_ID_GAMEPAD_DEVICE] = { .id = SET_ID_GAMEPAD_DEVICE, .label = "Device", .section = "Gamepad",
        .key = SETTING_GAMEPAD_DEVICE, .type = SET_TYPE_DEVICE, .min = -1, .max = 15, .refresh = SET_REFRESH_GAMEPAD },
    [SET_ID_GAMEPAD_MAPPINGS] = { .id = SET_ID_GAMEPAD_MAPPINGS, .label = "Mappings file", .section = "Gamepad",
        .key = SETTING_GAMEPAD_MAPPINGS_FILE, .type = SET_TYPE_PATH, .flags = SET_FLAG_NEXT_START },

    // Per menu
    [SET_ID_MENU_ROWS] = { .id = SET_ID_MENU_ROWS, .label = "Rows", .key = SETTING_ROWS, .type = SET_TYPE_COUNT,
        .min = 1, .max = 10, .can_inherit = true, .refresh = SET_REFRESH_LAYOUT },
    [SET_ID_MENU_COLUMNS] = { .id = SET_ID_MENU_COLUMNS, .label = "Columns", .key = SETTING_COLUMNS,
        .type = SET_TYPE_COUNT, .min = 1, .max = 12, .can_inherit = true, .refresh = SET_REFRESH_LAYOUT },
    [SET_ID_MENU_ICON_SIZE] = { .id = SET_ID_MENU_ICON_SIZE, .label = "Largest button", .key = SETTING_ICON_SIZE,
        .type = SET_TYPE_ICON_SIZE, .can_inherit = true, .refresh = SET_REFRESH_LAYOUT }
};

// A function to find a global setting by the section and key config.ini gives it (or its older
// name); NULL for a key the table does not hold, and for the per-menu keys, which menu sections hold
const SettingDef *setting_find(const char *section, const char *key)
{
    for (int i = 0; i < SET_ID_GLOBAL_COUNT; i++) {
        const SettingDef *def = &DEFS[i];
        if (strcmp(def->section, section) == 0 &&
            (strcmp(def->key, key) == 0 || (def->alias != NULL && strcmp(def->alias, key) == 0)))
            return def;
    }
    return NULL;
}
```

`LENGTH` (line 13) must now be defined above `DEFS`, where it already is. `settings.c` includes `layout.h`, which gives `LAYOUT_MAX_BUTTON`.

Replace `setting_parse()` (lines 93-148) with this version and its three helpers:

```c
// A function to read a whole number, strictly: an optional minus and digits, within an int.
// f15-limits' IconSpacing=2000000000 must still read, as atoi read it.
static bool parse_integer(const char *text, int *number)
{
    const char *digits = text[0] == '-' ? text + 1 : text;
    size_t length = strlen(digits);
    if (length == 0 || length > 10)
        return false;
    long long n = 0;
    for (size_t i = 0; i < length; i++) {
        if (digits[i] < '0' || digits[i] > '9')
            return false;
        n = n * 10 + (digits[i] - '0');
    }
    if (n > INT_MAX)
        return false;
    *number = (int) (text[0] == '-' ? -n : n);
    return true;
}

// A function to read "N%", "N.N%" or "N.NN%" into hundredths of a percent; with `whole`, "N%" only
static bool parse_percent(const char *text, bool whole, int *hundredths)
{
    size_t length = strlen(text);
    if (length < 2 || text[length - 1] != '%')
        return false;
    size_t end = length - 1;
    size_t i = 0;
    int number = 0;
    while (i < end && text[i] >= '0' && text[i] <= '9') {
        if (i >= 5)
            return false;
        number = number * 10 + (text[i] - '0');
        i++;
    }
    if (i == 0)
        return false;
    int fraction = 0;
    if (i < end) {
        if (whole || text[i] != '.')
            return false;
        size_t first = ++i;
        while (i < end && text[i] >= '0' && text[i] <= '9' && i - first < 2) {
            fraction = fraction * 10 + (text[i] - '0');
            i++;
        }
        if (i != end || i == first)
            return false;
        if (i - first == 1)
            fraction *= 10;
    }
    *hundredths = number * 100 + fraction;
    return true;
}

// A function to find a name among a choice setting's names, or its older spelling; -1 when neither
static int choice_index(const SettingDef *def, const char *text)
{
    for (int i = 0; def->names[i] != NULL; i++) {
        if (strcmp(def->names[i], text) == 0)
            return i;
    }
    return def->legacy != NULL && strcmp(def->legacy, text) == 0 ? def->legacy_index : -1;
}

// A function to copy a path from the file, dropping quotes round it as clean_path() does
static bool copy_path_text(const char *text, char *out)
{
    size_t length = strlen(text);
    if (length == 0 || length >= SETTING_TEXT_MAX)
        return false;
    if (length >= 3 && text[0] == '"' && text[length - 1] == '"') {
        memcpy(out, text + 1, length - 2);
        out[length - 2] = '\0';
    }
    else
        memcpy(out, text, length + 1);
    return true;
}

// A function to read a setting's value from config.ini's text, as the launcher has always read it,
// except where the spec fixes a bug (FPSLimit=10, a negative clock size) or asks for a log line
// in place of a silent misreading ("40px", "abc%")
bool setting_parse(const SettingDef *def, const char *text, SettingValue *value)
{
    SettingValue v;
    memset(&v, 0, sizeof(v));
    switch (def->type) {
        case SET_TYPE_COUNT:
            if (!layout_parse_count(text, &v.number))
                return false;
            break;
        case SET_TYPE_ICON_SIZE:
            if (!layout_parse_icon_size(text, &v.number))
                return false;
            break;
        case SET_TYPE_CHOICE:
            v.number = choice_index(def, text);
            if (v.number < 0)
                return false;
            break;
        case SET_TYPE_COLOR:
            if (!parse_hex_color(text, &v.color))
                return false;
            break;
        case SET_TYPE_PATH:
        case SET_TYPE_FONT:
            if (!copy_path_text(text, v.text))
                return false;
            break;
        case SET_TYPE_COMMAND:
        case SET_TYPE_MENU: {
            size_t length = strlen(text);
            if (length == 0 || length >= SETTING_TEXT_MAX)
                return false;
            memcpy(v.text, text, length + 1);
            break;
        }
        case SET_TYPE_SECONDS:
            v.number = atoi(text);
            if (v.number < def->min || v.number > def->max)
                return false;
            break;
        case SET_TYPE_MILLIS: {
            double seconds = atof(text);
            if (!(seconds >= 0.0) || seconds * 1000.0 >= (double) def->max + 0.5)
                return false;
            v.number = (int) (seconds * 1000.0 + 0.5);
            break;
        }
        case SET_TYPE_TITLE_SIZE:
            if (!layout_parse_title_size(text, &v.number, &v.percent))
                return false;
            break;
        case SET_TYPE_BOOL:
            if (strcmp(text, "true") == 0 || strcmp(text, "True") == 0)
                v.number = 1;
            else if (strcmp(text, "false") != 0 && strcmp(text, "False") != 0)
                return false;
            break;
        case SET_TYPE_NUMBER:
        case SET_TYPE_DEVICE:
            if (!parse_integer(text, &v.number) || v.number < def->min || v.number > def->max)
                return false;
            break;
        case SET_TYPE_PERCENT:
            if (parse_percent(text, (def->flags & SET_FLAG_WHOLE) != 0, &v.number)) {
                v.percent = true;
                if (v.number < def->min || v.number > def->max)
                    return false;
            }
            else if (!(def->flags & SET_FLAG_PX) || text[0] == '-' || !parse_integer(text, &v.number) ||
                     v.number > def->max_px)
                return false;
            break;
    }
    *value = v;
    return true;
}
```

Add `format_percent()` below `format_millis()`, and replace `setting_format()` (lines 165-199) with:

```c
// A function to write hundredths of a percent with only the decimals they need: 1250 is "12.5%"
static void format_percent(int hundredths, char *out, size_t size)
{
    int whole = hundredths / 100;
    int part = hundredths % 100;
    if (part == 0)
        snprintf(out, size, "%d%%", whole);
    else if (part % 10 == 0)
        snprintf(out, size, "%d.%d%%", whole, part / 10);
    else
        snprintf(out, size, "%d.%02d%%", whole, part);
}

// A function to write a value as config.ini holds it; "" for a value that removes the key
void setting_format(const SettingDef *def, const SettingValue *value, char *out, size_t size)
{
    if (size == 0)
        return;
    out[0] = '\0';
    if (value->inherit)
        return;
    switch (def->type) {
        case SET_TYPE_COUNT:
        case SET_TYPE_ICON_SIZE:
        case SET_TYPE_SECONDS:
        case SET_TYPE_NUMBER:
        case SET_TYPE_DEVICE:
            snprintf(out, size, "%d", value->number);
            break;
        case SET_TYPE_CHOICE:
            if (value->number >= 0 && value->number < choice_count(def))
                snprintf(out, size, "%s", def->names[value->number]);
            break;
        case SET_TYPE_COLOR:
            snprintf(out, size, "#%02X%02X%02X", value->color.r, value->color.g, value->color.b);
            break;
        case SET_TYPE_PATH:
        case SET_TYPE_FONT:
        case SET_TYPE_COMMAND:
        case SET_TYPE_MENU:
            snprintf(out, size, "%s", value->text);
            break;
        case SET_TYPE_MILLIS:
            format_millis(value->number, out, size);
            break;
        case SET_TYPE_TITLE_SIZE:
            if (value->percent)
                snprintf(out, size, "%d%%", value->number);
            else
                snprintf(out, size, "%d", value->number);
            break;
        case SET_TYPE_BOOL:
            snprintf(out, size, "%s", value->number ? "true" : "false");
            break;
        case SET_TYPE_PERCENT:
            if (value->percent)
                format_percent(value->number, out, size);
            else
                snprintf(out, size, "%d", value->number);
            break;
    }
}
```

Add this helper above `setting_parse()`, where both `setting_format()` and `setting_describe()` can use it:

```c
// A function to count a choice setting's names
static int choice_count(const SettingDef *def)
{
    int count = 0;
    while (def->names[count] != NULL)
        count++;
    return count;
}
```

Replace `setting_equal()` (lines 201-216) with:

```c
// A function to compare two values of a setting
bool setting_equal(const SettingDef *def, const SettingValue *a, const SettingValue *b)
{
    if (a->inherit || b->inherit)
        return a->inherit == b->inherit;
    switch (def->type) {
        case SET_TYPE_COLOR:
            return a->color.r == b->color.r && a->color.g == b->color.g && a->color.b == b->color.b;
        case SET_TYPE_PATH:
        case SET_TYPE_FONT:
        case SET_TYPE_COMMAND:
        case SET_TYPE_MENU:
            return strcmp(a->text, b->text) == 0;
        case SET_TYPE_TITLE_SIZE:
        case SET_TYPE_PERCENT:
            return a->percent == b->percent && a->number == b->number;
        default:
            return a->number == b->number;
    }
}
```

In the candidates code, raise `MAX_CANDIDATES` (line 226) from `48` to `64`. In `sort_key()` (lines 240-249) and `same_candidate()` (lines 252-261), treat `PERCENT` like `TITLE_SIZE`, with px sorted first:

```c
// A function to give a step its place: following the default first, then a custom colour, a fixed
// title size or a px value, then the rest in order
static long sort_key(const SettingDef *def, const Candidate *c)
{
    if (c->inherit)
        return LONG_MIN;
    if (def->type == SET_TYPE_COLOR)
        return preset_index(c->color);
    if (def->type == SET_TYPE_TITLE_SIZE)
        return c->percent ? c->number : -1;
    if (def->type == SET_TYPE_PERCENT)
        return c->percent ? 1000000L + c->number : c->number;
    return c->number;
}

// A function to compare two steps
static bool same_candidate(const SettingDef *def, const Candidate *a, const Candidate *b)
{
    if (a->inherit || b->inherit)
        return a->inherit == b->inherit;
    if (def->type == SET_TYPE_COLOR)
        return a->color.r == b->color.r && a->color.g == b->color.g && a->color.b == b->color.b;
    if (def->type == SET_TYPE_TITLE_SIZE || def->type == SET_TYPE_PERCENT)
        return a->percent == b->percent && a->number == b->number;
    return a->number == b->number;
}
```

Replace the `switch` in `build_candidates()` (lines 306-337) with:

```c
    switch (def->type) {
        case SET_TYPE_COUNT:
            for (int n = def->min; n <= def->max; n++)
                add_candidate(def, list, &count, number_candidate(n, false));
            break;
        case SET_TYPE_CHOICE:
            for (int n = def->lo; n <= def->hi; n++)
                add_candidate(def, list, &count, number_candidate(n, false));
            break;
        case SET_TYPE_BOOL:
            add_candidate(def, list, &count, number_candidate(0, false));
            add_candidate(def, list, &count, number_candidate(1, false));
            break;
        case SET_TYPE_ICON_SIZE:
            for (int i = 0; i < LENGTH(ICON_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(ICON_STEPS[i], false));
            break;
        case SET_TYPE_SECONDS:
        case SET_TYPE_NUMBER:
            if (def->steps != NULL) {
                for (int i = 0; i < def->step_count; i++)
                    add_candidate(def, list, &count, number_candidate(def->steps[i], false));
            }
            else if (def->step > 0) {
                for (int n = def->lo; n <= def->hi; n += def->step)
                    add_candidate(def, list, &count, number_candidate(n, false));
            }
            break;
        case SET_TYPE_PERCENT:
            for (int n = def->lo; def->step > 0 && n <= def->hi; n += def->step)
                add_candidate(def, list, &count, number_candidate(n, true));
            break;
        case SET_TYPE_MILLIS:
            for (int i = 0; i < LENGTH(MILLI_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(MILLI_STEPS[i], false));
            break;
        case SET_TYPE_TITLE_SIZE:
            for (int i = 0; i < LENGTH(TITLE_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(TITLE_STEPS[i], true));
            break;
        case SET_TYPE_COLOR:
            for (int i = 0; i < LENGTH(PRESETS); i++) {
                Candidate c = number_candidate(0, false);
                c.color = PRESETS[i].color;
                add_candidate(def, list, &count, c);
            }
            break;
        case SET_TYPE_PATH:
        case SET_TYPE_FONT:
        case SET_TYPE_COMMAND:
        case SET_TYPE_MENU:
        case SET_TYPE_DEVICE:
            break;
    }
```

In `setting_step()` (line 359), the types the pages or the pickers change are never stepped here:

```c
    if (def->type == SET_TYPE_PATH || def->type == SET_TYPE_FONT || def->type == SET_TYPE_COMMAND ||
        def->type == SET_TYPE_MENU || def->type == SET_TYPE_DEVICE || direction == 0)
        return result;
```

Add the command labels above `setting_describe()`:

```c
// The special commands and what the screen calls them
static const struct {
    const char *command;
    const char *label;
} COMMAND_LABELS[] = {
    { ":left", "Left" }, { ":right", "Right" }, { ":up", "Up" }, { ":down", "Down" },
    { ":select", "OK" }, { ":back", "Back" }, { ":home", "Home" }, { ":settings", "Settings" },
    { ":quit", "Quit StreamFlex" }, { ":shutdown", "Shut down" }, { ":restart", "Restart" },
    { ":sleep", "Sleep" }, { ":exit", "Close the app on show" }
};

// A function to describe a command as the screen shows it: a special command by what it does,
// ":submenu X" as the submenu it opens, anything else as it is written
void setting_command_label(const char *command, char *out, size_t size)
{
    for (int i = 0; i < LENGTH(COMMAND_LABELS); i++) {
        if (strcmp(command, COMMAND_LABELS[i].command) == 0) {
            snprintf(out, size, "%s", COMMAND_LABELS[i].label);
            return;
        }
    }
    if (strncmp(command, ":submenu ", 9) == 0 && command[9] != '\0')
        snprintf(out, size, "Open submenu: %s", command + 9);
    else
        snprintf(out, size, "%s", command);
}
```

In `setting_describe()`, replace the `SET_TYPE_CHOICE` case (line 413-415) with:

```c
        case SET_TYPE_CHOICE:
            snprintf(out, size, "%s", value->number >= 0 && value->number < choice_count(def)
                                      ? def->labels[value->number] : "?");
            break;
```

replace the `SET_TYPE_PATH` case (lines 424-429) with:

```c
        case SET_TYPE_PATH:
        case SET_TYPE_FONT:
            if (value->text[0] == '\0')
                snprintf(out, size, "Choose" ELLIPSIS);
            else
                base_name(value->text, out, size);
            break;
```

and add these cases before the closing brace of its `switch`:

```c
        case SET_TYPE_BOOL:
            snprintf(out, size, "%s", value->number ? "On" : "Off");
            break;
        case SET_TYPE_NUMBER:
            if (value->inherit)
                snprintf(out, size, "%s", def->inherit_label != NULL ? def->inherit_label : "");
            else
                snprintf(out, size, "%d%s", value->number, def->unit != NULL ? def->unit : "");
            break;
        case SET_TYPE_PERCENT:
            if (value->percent)
                format_percent(value->number, out, size);
            else
                snprintf(out, size, "%d px", value->number);
            break;
        case SET_TYPE_COMMAND:
            if (value->inherit || value->text[0] == '\0')
                snprintf(out, size, "None");
            else
                setting_command_label(value->text, out, size);
            break;
        case SET_TYPE_MENU:
            snprintf(out, size, "%s", value->text);
            break;
        case SET_TYPE_DEVICE:
            if (value->number < 0)
                snprintf(out, size, "Any");
            else
                snprintf(out, size, "Pad %d", value->number);
            break;
```

The `SECONDS` case already writes `%d min` for whole minutes and `%d s` otherwise, which is what *App timeout* and *Idle time* show.

Finally, in `settings_rows()` (line 699) `note_row(TRANSPARENT_NOTE)` and the `leave_background()` notice (line 807) use `MODE_LABELS`, which is unchanged, so they need no edit. The mode checks against `MODE_IMAGE` and `MODE_SLIDESHOW` stay.

- [ ] **Step 6: Keep `settings_screen.c` compiling until Task 3 replaces its two switches**

In `src/settings_screen.c`, in both `read_value()` (the `switch (id)` at line 247) and `apply_slot()` (the `switch (slot->def->id)` at line 312), replace the last case:

```c
        case SET_ID_COUNT:
            break;
```

with:

```c
        default:   // Task 3 moves every setting through config_fields.c
            break;
```

- [ ] **Step 7: Run the tests to see them pass**

Run:

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake -S C:/Users/jscha/source/repos/streamflex -B C:/Users/jscha/source/repos/streamflex/build
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

The configure step is needed because `launcher_config.h.in` changed. Expected: `100% tests passed`. `settings` now reports more checks than before, with 0 failed, and every 3a test in it still passes unchanged.

Then run the Linux unit tests with the extra warnings (see **Linux unit tests** in Global Constraints) with the label `t1`. Expected: `warnings outside src/external: 0` and `100% tests passed`.

- [ ] **Step 8: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add config/config_settings.cmake config/launcher_config.h.in src/settings.h src/settings.c src/settings_screen.c tests/test_settings.c
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: every remaining config.ini key in the settings table"
```

### Task 2: `derive_settings()` — configured values to the values the launcher draws with

A new pure module turns what the file says into what the launcher draws with:
- percentages into px;
- opacities into alpha bytes;
- the clamps that `validate_settings()` applied in place.

It runs as often as needed and gives the same answer every time, and a clamp limits only its output. This task adds the module and its tests. Task 3 puts it to work.

**Files:**
- Create: `src/derive.h`, `src/derive.c`
- Modify: `tests/CMakeLists.txt` (a `test_derive` suite)
- Test: `tests/test_derive.c`

**Interfaces:**
- Consumes: nothing (pure).
- Produces:

```c
typedef struct { unsigned char r, g, b, a; } DeriveColor;
typedef struct DeriveInput DeriveInput;   // fields in Step 3
typedef struct Effective Effective;       // fields in Step 3
void derive_settings(const DeriveInput *in, Effective *out);
int derive_alpha(int hundredths);         // 0-255, truncated as convert_percent_to_int() did
```

  Task 3's `derive_input()` fills a `DeriveInput`. Every draw path reads `Effective` from then on.

- [ ] **Step 1: Write the failing tests**

Create `tests/test_derive.c`:

```c
#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "derive.h"

// A function to start from the built-in defaults on a 1920 x 1080 screen, as the launcher would
static DeriveInput defaults(void)
{
    DeriveInput in;
    memset(&in, 0, sizeof(in));
    in.screen_width = 1920;
    in.screen_height = 1080;
    in.titles_enabled = true;
    in.title_padding = 0;
    in.title_padding_pct = 8;
    in.title_color = (DeriveColor) { 0xFF, 0xFF, 0xFF, 0 };
    in.title_shadow_color = (DeriveColor) { 0x00, 0x00, 0x00, 0 };
    in.title_opacity = 10000;
    in.overlay_color = (DeriveColor) { 0x00, 0x00, 0x00, 0 };
    in.overlay_opacity = 5000;
    in.highlight_fill = (DeriveColor) { 0xFF, 0xFF, 0xFF, 0 };
    in.highlight_fill_opacity = 2500;
    in.highlight_outline = (DeriveColor) { 0x00, 0x00, 0xFF, 0 };
    in.highlight_outline_opacity = 10000;
    in.highlight_outline_size = 0;
    in.highlight_rx = 0;
    in.highlight_hpadding = 30;
    in.highlight_vpadding = 30;
    in.scroll_fill = (DeriveColor) { 0xFF, 0xFF, 0xFF, 0 };
    in.scroll_outline = (DeriveColor) { 0x00, 0x00, 0x00, 0 };
    in.scroll_opacity = 10000;
    in.scroll_outline_size = 0;
    in.clock_color = (DeriveColor) { 0xFF, 0xFF, 0xFF, 0 };
    in.clock_shadow_color = (DeriveColor) { 0x00, 0x00, 0x00, 0 };
    in.clock_opacity = 10000;
    in.icon_spacing = 500;
    in.icon_spacing_percent = true;
    in.vcenter = 5000;
    in.clock_margin = 500;
    in.clock_margin_percent = true;
    in.screensaver_intensity = 7000;
    return in;
}

// A function to test that the defaults give the values validate_settings() gave them
static void test_defaults(void)
{
    DeriveInput in = defaults();
    Effective eff;
    derive_settings(&in, &eff);
    CHECK_INT(eff.icon_spacing, 96);            // 5% of 1920
    CHECK_INT(eff.vcenter, 540);                // 50% of 1080
    CHECK_INT(eff.clock_margin, 54);            // 5% of 1080
    CHECK_INT(eff.highlight_hpadding, 30);
    CHECK_INT(eff.highlight_vpadding, 30);
    CHECK_INT(eff.title_color.a, 255);
    CHECK_INT(eff.title_shadow_color.a, 191);   // 0.75 of the title's alpha
    CHECK_INT(eff.overlay_color.a, 127);        // 50%
    CHECK_INT(eff.highlight_fill.a, 63);        // 25%, as FillOpacity=25% always gave
    CHECK_INT(eff.highlight_outline.a, 255);
    CHECK_INT(eff.scroll_fill.a, 255);
    CHECK_INT(eff.scroll_outline.a, 255);       // One opacity for both
    CHECK_INT(eff.clock_color.a, 255);
    CHECK_INT(eff.clock_shadow_color.a, 191);
    CHECK_INT(eff.screensaver_alpha, 178);      // 70%
    CHECK_INT(eff.title_padding_pct, 8);
    CHECK_INT(eff.highlight_fill.r, 0xFF);      // The colour itself passes through
    CHECK_INT(eff.highlight_outline.b, 0xFF);
}

// A function to test that deriving twice gives the same answer: nothing is spent on the first run
static void test_twice(void)
{
    DeriveInput in = defaults();
    Effective first, second;
    derive_settings(&in, &first);
    derive_settings(&in, &second);
    CHECK(memcmp(&first, &second, sizeof(first)) == 0);
}

// A function to test that a clamp limits only the effective value, so raising the setting that
// caused it gives the full value back
static void test_clamps_restore(void)
{
    DeriveInput in = defaults();
    Effective eff;

    // HPadding is kept to half the gap between buttons; with a wider gap it comes back
    in.highlight_hpadding = 100;
    in.icon_spacing = 50;
    in.icon_spacing_percent = false;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_hpadding, 25);
    in.icon_spacing = 400;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_hpadding, 100);

    // The outline is kept inside the smaller padding
    in.highlight_outline_size = 50;
    in.highlight_vpadding = 10;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_outline_size, 10);
    in.highlight_vpadding = 80;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_outline_size, 50);

    // Rounded corners with an outline cannot be drawn (NanoSVG): the radius goes, and comes back
    in.highlight_rx = 20;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_rx, 0);
    in.highlight_outline_size = 0;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_rx, 20);

    // A gap wider than the screen is the screen's width
    in.icon_spacing = 5000;
    derive_settings(&in, &eff);
    CHECK_INT(eff.icon_spacing, 1920);

    // The clock's margin is at most 10% of the screen height
    in.clock_margin = 5000;
    derive_settings(&in, &eff);
    CHECK_INT(eff.clock_margin, 108);
    in.clock_margin = 20;
    in.clock_margin_percent = false;
    derive_settings(&in, &eff);
    CHECK_INT(eff.clock_margin, 20);

    // The vertical centre stays between 25% and 75%
    in.vcenter = 1000;
    derive_settings(&in, &eff);
    CHECK_INT(eff.vcenter, 270);
    in.vcenter = 9000;
    derive_settings(&in, &eff);
    CHECK_INT(eff.vcenter, 810);

    // The scroll arrow's outline is at most 1% of the screen height
    in.scroll_outline_size = 40;
    derive_settings(&in, &eff);
    CHECK_INT(eff.scroll_outline_size, 10);
}

// A function to test every percentage and opacity conversion at its ends and between
static void test_conversions(void)
{
    CHECK_INT(derive_alpha(0), 0);
    CHECK_INT(derive_alpha(10000), 255);
    CHECK_INT(derive_alpha(5000), 127);
    CHECK_INT(derive_alpha(1250), 31);          // 12.5%
    CHECK_INT(derive_alpha(-5), 0);             // Out of range reads as the nearest end
    CHECK_INT(derive_alpha(20000), 255);

    DeriveInput in = defaults();
    Effective eff;
    in.icon_spacing = 1250;                     // 12.5% of 1920
    derive_settings(&in, &eff);
    CHECK_INT(eff.icon_spacing, 240);
    in.title_opacity = 0;
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_color.a, 0);
    CHECK_INT(eff.title_shadow_color.a, 0);
    in.screensaver_intensity = 0;               // Dims nothing: the screensaver cannot start
    derive_settings(&in, &eff);
    CHECK_INT(eff.screensaver_alpha, 0);
}

// A function to test that titles turned off take their padding with them, as validate_settings() did
static void test_titles_off(void)
{
    DeriveInput in = defaults();
    Effective eff;
    in.titles_enabled = false;
    in.title_padding = 20;
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_padding, 0);
    CHECK_INT(eff.title_padding_pct, 0);
    in.titles_enabled = true;
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_padding, 20);
    CHECK_INT(eff.title_padding_pct, 8);
}

int main(void)
{
    test_defaults();
    test_twice();
    test_clamps_restore();
    test_conversions();
    test_titles_off();
    return check_report();
}
```

Add to `tests/CMakeLists.txt`, after the `test_settings` block:

```cmake
# Unit tests for deriving the values the launcher draws with from the configured ones (pure)
add_executable(test_derive test_derive.c "${PROJECT_SOURCE_DIR}/src/derive.c")
target_include_directories(test_derive PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME derive COMMAND test_derive)
```

- [ ] **Step 2: Run the test to see it fail**

Run:

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake -S C:/Users/jscha/source/repos/streamflex -B C:/Users/jscha/source/repos/streamflex/build
```

Expected: the configure fails with `Cannot find source file: .../src/derive.c`.

- [ ] **Step 3: Write `derive.h`**

Create `src/derive.h`:

```c
// The values the launcher draws with, worked out from the values config.ini holds: percentages of
// the screen in px, opacities as alpha bytes, and the limits one setting puts on another. Pure: no
// SDL, no globals, and nothing is changed in place, so it can run after every change, as often as
// needed, with the same answer each time. tests/test_derive.c builds it on its own.
#ifndef DERIVE_H
#define DERIVE_H

#include <stdbool.h>

#define DERIVE_MAX_CLOCK_MARGIN_PM 100        // The clock's margin: at most 10% of the screen height
#define DERIVE_MIN_VCENTER_PM 250             // The vertical centre: 25% to 75% of the screen height
#define DERIVE_MAX_VCENTER_PM 750
#define DERIVE_MAX_SCROLL_OUTLINE_PM 10       // The scroll arrow's outline: at most 1% of the screen height
#define DERIVE_SHADOW_ALPHA_PERCENT 75        // A shadow is three quarters as opaque as its text

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} DeriveColor;

// The configured values: as config.ini says, percentages in hundredths. A colour's `a` is ignored.
typedef struct DeriveInput {
    int screen_width;
    int screen_height;
    bool titles_enabled;
    int title_padding;             // px; used when title_padding_pct is 0
    int title_padding_pct;         // Whole percent of the button
    DeriveColor title_color;
    DeriveColor title_shadow_color;
    int title_opacity;             // Hundredths of a percent, as every opacity below
    DeriveColor overlay_color;
    int overlay_opacity;
    DeriveColor highlight_fill;
    int highlight_fill_opacity;
    DeriveColor highlight_outline;
    int highlight_outline_opacity;
    int highlight_outline_size;
    int highlight_rx;
    int highlight_hpadding;
    int highlight_vpadding;
    DeriveColor scroll_fill;
    DeriveColor scroll_outline;
    int scroll_opacity;
    int scroll_outline_size;
    DeriveColor clock_color;
    DeriveColor clock_shadow_color;
    int clock_opacity;
    int icon_spacing;              // Hundredths of a percent of the screen width, or px
    bool icon_spacing_percent;
    int vcenter;                   // Hundredths of a percent of the screen height
    int clock_margin;              // Hundredths of a percent of the screen height, or px
    bool clock_margin_percent;
    int screensaver_intensity;     // Hundredths of a percent
} DeriveInput;

// The values the launcher draws with
typedef struct Effective {
    int icon_spacing;              // px
    int vcenter;                   // px from the top of the screen
    int clock_margin;              // px
    int title_padding;             // px; 0 without titles
    int title_padding_pct;         // Whole percent; 0 without titles, or for a px padding
    int highlight_hpadding;        // px, at most half the gap between buttons
    int highlight_vpadding;
    int highlight_outline_size;    // px, inside the smaller padding
    int highlight_rx;              // 0 when there is an outline (NanoSVG cannot draw both)
    int scroll_outline_size;       // px
    DeriveColor title_color;       // Every colour below with its alpha from its opacity
    DeriveColor title_shadow_color;
    DeriveColor overlay_color;
    DeriveColor highlight_fill;
    DeriveColor highlight_outline;
    DeriveColor scroll_fill;
    DeriveColor scroll_outline;
    DeriveColor clock_color;
    DeriveColor clock_shadow_color;
    int screensaver_alpha;         // 0-255 at full dim; below 1 the screensaver cannot dim
} Effective;

int derive_alpha(int hundredths);
void derive_settings(const DeriveInput *in, Effective *out);

#endif
```

- [ ] **Step 4: Write `derive.c`**

Create `src/derive.c`:

```c
#include <string.h>
#include "derive.h"

// A function to return the smaller of two ints
static int min_int(int a, int b)
{
    return a < b ? a : b;
}

// A function to return the larger of two ints
static int max_int(int a, int b)
{
    return a > b ? a : b;
}

// A function to take hundredths of a percent of a size, truncated as convert_percent_to_int() did
static int hundredths_of(int size, int hundredths)
{
    return (int) ((long long) size * hundredths / 10000);
}

// A function to turn an opacity in hundredths of a percent into an alpha byte; out of range reads
// as the nearest end
int derive_alpha(int hundredths)
{
    return hundredths_of(255, max_int(0, min_int(hundredths, 10000)));
}

// A function to give a colour the alpha of an opacity
static DeriveColor with_alpha(DeriveColor color, int alpha)
{
    color.a = (unsigned char) alpha;
    return color;
}

// A function to work out every value the launcher draws with from the configured ones. It changes
// nothing it is given, so it can run again after any change.
void derive_settings(const DeriveInput *in, Effective *out)
{
    memset(out, 0, sizeof(*out));
    int w = in->screen_width;
    int h = in->screen_height;

    // A gap wider than the screen leaves no room for a single button
    int spacing = in->icon_spacing_percent ? hundredths_of(w, in->icon_spacing) : in->icon_spacing;
    out->icon_spacing = max_int(0, min_int(spacing, w));

    int vcenter = hundredths_of(h, in->vcenter);
    out->vcenter = max_int(h * DERIVE_MIN_VCENTER_PM / 1000, min_int(vcenter, h * DERIVE_MAX_VCENTER_PM / 1000));

    int margin = in->clock_margin_percent ? hundredths_of(h, in->clock_margin) : in->clock_margin;
    out->clock_margin = max_int(0, min_int(margin, h * DERIVE_MAX_CLOCK_MARGIN_PM / 1000));

    // Titles turned off take their padding with them
    out->title_padding = in->titles_enabled ? in->title_padding : 0;
    out->title_padding_pct = in->titles_enabled ? in->title_padding_pct : 0;

    // The highlight's padding never overlaps the next button, and its outline stays inside it
    out->highlight_hpadding = max_int(0, min_int(in->highlight_hpadding, out->icon_spacing / 2));
    out->highlight_vpadding = max_int(0, in->highlight_vpadding);
    int max_outline = max_int(0, min_int(out->highlight_hpadding, out->highlight_vpadding));
    out->highlight_outline_size = max_int(0, min_int(in->highlight_outline_size, max_outline));

    // NanoSVG cannot draw rounded corners with an outline
    out->highlight_rx = out->highlight_outline_size > 0 ? 0 : max_int(0, in->highlight_rx);

    out->scroll_outline_size = max_int(0, min_int(in->scroll_outline_size, h * DERIVE_MAX_SCROLL_OUTLINE_PM / 1000));

    int title_alpha = derive_alpha(in->title_opacity);
    out->title_color = with_alpha(in->title_color, title_alpha);
    out->title_shadow_color = with_alpha(in->title_shadow_color, title_alpha * DERIVE_SHADOW_ALPHA_PERCENT / 100);
    out->overlay_color = with_alpha(in->overlay_color, derive_alpha(in->overlay_opacity));
    out->highlight_fill = with_alpha(in->highlight_fill, derive_alpha(in->highlight_fill_opacity));
    out->highlight_outline = with_alpha(in->highlight_outline, derive_alpha(in->highlight_outline_opacity));
    int scroll_alpha = derive_alpha(in->scroll_opacity);
    out->scroll_fill = with_alpha(in->scroll_fill, scroll_alpha);
    out->scroll_outline = with_alpha(in->scroll_outline, scroll_alpha);
    int clock_alpha = derive_alpha(in->clock_opacity);
    out->clock_color = with_alpha(in->clock_color, clock_alpha);
    out->clock_shadow_color = with_alpha(in->clock_shadow_color, clock_alpha * DERIVE_SHADOW_ALPHA_PERCENT / 100);
    out->screensaver_alpha = derive_alpha(in->screensaver_intensity);
}
```

The shadow's alpha is the text's alpha times 75 / 100, truncated. `calculate_shadow_alpha()` computed `(Uint8) (0.75F * alpha)`, which is the same for every byte (255 → 191, 127 → 95).

- [ ] **Step 5: Run the test to see it pass**

Run:

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake -S C:/Users/jscha/source/repos/streamflex -B C:/Users/jscha/source/repos/streamflex/build
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: `100% tests passed`, `derive` among them. Then run the **Linux unit tests** with the label `t2`: `warnings outside src/external: 0` and `100% tests passed`.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/derive.h src/derive.c tests/test_derive.c tests/CMakeLists.txt
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: derive the values the launcher draws with, without changing the configured ones"
```

### Task 3: One parse path, and `Config` holds what the file says

What this task does:
- `config_handler()` reads every key in a settings section through the table.
- A new SDL-side module, `config_fields.c`, is the only code that moves a `SettingValue` into `Config` (or a menu) and back. The parser and the settings screen both use it, so 3a's two big switches in `settings_screen.c` go.
- `Config`'s converted fields become the file's values, and `derive_settings()` runs after the parse and after every change, into a global `Effective eff`. Every draw path reads `eff`.
- `load_font()` no longer writes a fallback path into `Config`: the `TextInfo` owns the path it opened, with the new face index.
- `:exit` outside a Windows hotkey logs why it does nothing.

The frame timing's `config.vsync` write stays for Task 4, which replaces it.

**Files:**
- Create: `src/config_fields.h`, `src/config_fields.c`
- Modify: `src/launcher.h` (the `Config` struct, lines 245-324; new prototypes at the end)
- Modify: `src/launcher.c` (the `Config` initializer, lines 55-166; globals; `init_sdl_ttf()`; `compute_menu_layout()`; `cleanup()`; `execute_command()`; `init_screensaver()`; the overlay block in `main()`; `main()`'s startup order)
- Modify: `src/util.h`, `src/util.c` (`config_handler()`, lines 157-494; `validate_settings()` and `convert_percent_to_int()` removed, lines 974-1100)
- Modify: `src/image.h`, `src/image.c` (`TextInfo`; `load_font()`; `title_font()`; `render_text()`'s Shrink; `render_highlight()`; `render_scroll_indicators()`)
- Modify: `src/clock.c` (`init_clock()`; `calculate_clock_positioning()`)
- Modify: `src/debug.h`, `src/debug.c` (`DEBUG_COLOR`; `debug_settings()`)
- Modify: `src/settings_screen.c` (`read_value()`, `replace_path()`, `apply_slot()`, `open_fonts()`)
- Modify: `src/CMakeLists.txt` (`config_fields.c`, `config_fields.h`, `derive.c`, `derive.h` in `SOURCES`)
- Create: `tests/headless/fixtures/f41-clocksize.ini`, `f41-exit.ini`, `f41-fontface.ini`, `f41-values.ini`
- Create: `tests/headless/checks/41-parse.sh`

**Interfaces:**
- Consumes: Task 1's table (`setting_find()`, `setting_parse()`, every `SET_ID_*`), and Task 2's `derive_settings()`, `DeriveInput`, `Effective`.
- Produces:

```c
// config_fields.h
void config_apply_defaults(void);                                       // Before the parse
void config_store(SettingId id, Menu *menu, const SettingValue *value); // menu: per-menu ids only
SettingValue config_read(SettingId id, const Menu *menu);
DeriveInput derive_input(void);
// launcher.h
extern Effective eff;
extern TextInfo title_info;     // declared in image.h's users; launcher.c defines it
void refresh_effective(void);   // derive into eff, then geo.vcenter and the draw colours
// image.h
int load_font(TextInfo *info, const char *configured, int face, const char *default_font);
```

  `TextInfo.font_path` becomes an owned `char *`, the file actually opened, and gains `int font_face`. `Config` gains `title_font_face`, `clock_font_face`, `icon_spacing_percent` and `clock_margin_percent`. Its opacity, `vcenter` and intensity fields become `int` hundredths of a percent.

- [ ] **Step 1: Write the failing harness checks**

Create `tests/headless/fixtures/f41-clocksize.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Clock]
Enabled=true
FontSize=-5

[Main]
Entry1=One;apps;:quit
```

Create `tests/headless/fixtures/f41-exit.ini`:

```ini
[General]
DefaultMenu=Main

[Hotkeys]
Hotkey1=#4000003B;:exit

[Main]
Entry1=Close;apps;:exit
```

Create `tests/headless/fixtures/f41-fontface.ini` (DejaVuSans.ttf has one face):

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Titles]
Font=/work/build/assets/fonts/DejaVuSans.ttf
FontFace=9

[Main]
Entry1=One;apps;:quit
```

Create `tests/headless/fixtures/f41-values.ini`, which holds values the table reads in forms it never read before, or past the new steps:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit
FPSLimit=10

[Layout]
IconSpacing=40
VCenter=12.5%

[Background]
Overlay=true
OverlayOpacity=12.5%

[Highlight]
HPadding=300
FillOpacity=33.33%

[Main]
Entry1=One;apps;:quit
```

Create `tests/headless/checks/41-parse.sh`:

```bash
# One parse path (3b): the table reads every key, Config keeps what the file says, and the
# values the launcher draws with are worked out from it. The spec's three parser bugs are fixed.

# A negative [Clock] FontSize is refused with a log line, and the clock still shows at its default
run_quick f41-clocksize
ok=1
grep -q "Invalid FontSize value '-5' in \[Clock\], ignoring it" "$out/f41-clocksize.log" \
    && grep -A12 'Clock ===' "$out/f41-clocksize.log" | grep -qE '^FontSize:\s+50$' \
    && ran_clean f41-clocksize && ok=0
result "a negative clock FontSize is refused with a log line (exit $(cat "$out/f41-clocksize.code"))" $ok

# :exit on Linux, as a hotkey (F2) or an entry's command, says it works only on Windows
run_keys f41-exit F2 Return
ok=1
[ "$(grep -c "':exit' works only as a hotkey on Windows" "$out/f41-exit.log")" = 2 ] && ran_clean f41-exit && ok=0
result ":exit outside a Windows hotkey says why it does nothing (exit $(cat "$out/f41-exit.code"))" $ok

# A FontFace the file does not have falls back to the bundled font, and the setting stays as written
run_quick f41-fontface
ok=1
grep -q 'Could not open the font /work/build/assets/fonts/DejaVuSans.ttf (face 9), using the default font' "$out/f41-fontface.log" \
    && grep -A14 'Titles ===' "$out/f41-fontface.log" | grep -qE '^FontFace:\s+9$' \
    && grep -A14 'Titles ===' "$out/f41-fontface.log" | grep -qE '^Font:\s+/work/build/assets/fonts/DejaVuSans.ttf$' \
    && ran_clean f41-fontface && ok=0
result "a FontFace the font does not have falls back and stays as written (exit $(cat "$out/f41-fontface.code"))" $ok

# Values in forms the table never read before: kept as written in Config, and the effective ones
# worked out from them (12.5% of 1080 is 135 px, clamped to the 25% minimum, 270; HPadding 300 is
# kept to half the 40 px gap)
run_quick f41-values
ok=1
log=$out/f41-values.log
! grep -q 'Invalid' "$log" \
    && grep -A6 'General ===' "$log" | grep -qE '^FPSLimit:\s+10$' \
    && grep -A6 'Layout ===' "$log" | grep -qE '^IconSpacing:\s+40$' \
    && grep -A6 'Layout ===' "$log" | grep -qE '^VCenter:\s+12.5%$' \
    && grep -q 'Effective: IconSpacing 40 px, VCenter 270 px, HPadding 20 px' "$log" \
    && ran_clean f41-values && ok=0
result "values in new forms are kept as written, and the drawn ones worked out from them (exit $(cat "$out/f41-values.code"))" $ok
grep -E '^(FPSLimit|IconSpacing|VCenter|Effective):' "$log" | sed 's/^/      /'
```

- [ ] **Step 2: Run the harness to see the new checks fail**

Run the headless harness (Global Constraints) with the label `t3-red`. Expected:
- `FAIL` for each of the four new checks. There is no "Invalid FontSize ... in [Clock]" line, no `:exit` line, no `(face 9)` line and no `Effective:` line.
- Every other check `PASS`.

- [ ] **Step 3: Write `config_fields.h`**

Create `src/config_fields.h`:

```c
// The one place a setting's value moves between the settings table's form (SettingValue) and the
// launcher's Config, or a menu's grid: the parser stores every key through it, and the settings
// screen reads and stores through it. SDL-side, since Config holds SDL types.
#ifndef CONFIG_FIELDS_H
#define CONFIG_FIELDS_H

#include "settings.h"
#include "derive.h"

void config_apply_defaults(void);
void config_store(SettingId id, Menu *menu, const SettingValue *value);
SettingValue config_read(SettingId id, const Menu *menu);
DeriveInput derive_input(void);

#endif
```

(It is included after `launcher.h`, which defines `Menu`.)

- [ ] **Step 4: Give `Config` the file's values**

In `src/launcher.h`, replace the `Config` struct (lines 244-324) with:

```c
// Configuration settings: what config.ini says, or the built-in default. Nothing converts them in
// place: the values the launcher draws with are derived from them into `eff` (derive.h).
typedef struct {
    char *default_menu;
    unsigned int max_buttons; // The Columns setting (MaxButtons is its older name)
    unsigned int rows;
    bool vsync;
    int fps_limit;            // -1 when FPSLimit is absent
    Uint32 application_timeout;
    ModeBackground background_mode; // Defines image or color background mode
    SDL_Color background_color; // Background color
    SDL_Color chroma_key_color;
    char *background_image; // Path to background image
    char *slideshow_directory;
    bool background_overlay;
    SDL_Color background_overlay_color; // Its alpha is eff's, from the opacity
    int background_overlay_opacity;     // Hundredths of a percent, as every opacity below
    Uint16 icon_size;
    int icon_spacing;                   // Hundredths of a percent of the screen width, or px
    bool icon_spacing_percent;
    bool titles_enabled;
    char *title_font_path;              // As configured; title_info.font_path is the file opened
    int title_font_face;
    unsigned int title_font_size;
    int title_font_size_pct;    // FontSize as a percentage of the button; 0 = the fixed title_font_size
    SDL_Color title_font_color; // Color struct for title text
    bool title_shadows;
    SDL_Color title_shadow_color;
    int title_opacity;
    ModeOversize title_oversize_mode;
    int title_padding;
    int title_padding_pct;      // Padding as a percentage of the button; 0 = the fixed title_padding
    bool highlight;
    SDL_Color highlight_fill_color;
    SDL_Color highlight_outline_color;
    int highlight_outline_size;
    int highlight_fill_opacity;
    int highlight_outline_opacity;
    int highlight_rx;
    int highlight_vpadding;
    int highlight_hpadding;
    int vcenter;                        // Hundredths of a percent of the screen height
    bool scroll_indicators;
    SDL_Color scroll_indicator_fill_color;
    int scroll_indicator_outline_size;
    SDL_Color scroll_indicator_outline_color;
    int scroll_indicator_opacity;
    bool wrap_entries;
    bool reset_on_back;
    bool mouse_select;
    bool inhibit_os_screensaver;
    char *startup_cmd;
    char *quit_cmd;
    ModeOnLaunch on_launch;
    bool screensaver_enabled;
    Uint32 screensaver_idle_time;
    int screensaver_intensity;
    bool screensaver_pause_slideshow;
    bool gamepad_enabled;
    int gamepad_device;
    char *gamepad_mappings_file;
    bool debug;
    char *exe_path;
    char *config_path; // The file the settings were read from
    Menu *first_menu;
    size_t num_menus;
    bool clock_enabled;
    bool clock_show_date;
    Alignment clock_alignment;
    char *clock_font_path;              // As configured; the clock's text_info.font_path is the file opened
    int clock_font_face;
    int clock_margin;                   // Hundredths of a percent of the screen height, or px
    bool clock_margin_percent;
    SDL_Color clock_font_color;
    int clock_opacity;
    unsigned int clock_font_size;
    bool clock_shadows;
    SDL_Color clock_shadow_color;
    TimeFormat clock_time_format;
    DateFormat clock_date_format;
    bool clock_include_weekday;
    Uint32 slideshow_image_duration;
    Uint32 slideshow_transition_time;
} Config;
```

At the top of `launcher.h`, after `#include "layout.h"`, add `#include "derive.h"`. At the end, after `int show_home(void);`, add:

```c
extern Effective eff;
void refresh_effective(void);
```

In `src/launcher.c`'s initializer (lines 55-166), remove the lines that set fields that no longer exist, or whose value now comes from `config_apply_defaults()`:
- `.background_overlay_opacity[0]    = '\0',`
- `.icon_spacing                     = -1,`
- `.title_opacity[0]                 = '\0',`
- `.highlight_fill_opacity[0]        = '\0',`
- `.highlight_outline_opacity[0]     = '\0',`
- `.vcenter[0]             = '\0',`
- `.icon_spacing_str[0]              = '\0',`
- `.scroll_indicator_opacity[0]      = '\0',`
- `.screensaver_intensity_str[0]     = '\0',`
- `.clock_margin_str[0]              = '\0',`
- `.clock_margin                     = -1,`
- `.clock_opacity[0]                 = '\0',`

The fields they set are zero until `config_apply_defaults()` runs, first thing in `main()` (Step 8). Every other initializer line stays. The colours keep their `*_A` alpha bytes, which nothing reads any more except the background and chroma key colours' `0xFF`.

- [ ] **Step 5: Write `config_fields.c`**

Create `src/config_fields.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "launcher.h"
#include <launcher_config.h>
#include "config_fields.h"
#include "debug.h"

extern Config config;
extern Geometry geo;

// A function to put a value's colour into an SDL colour, leaving its alpha
static void store_rgb(SDL_Color *color, const SettingValue *value)
{
    color->r = value->color.r;
    color->g = value->color.g;
    color->b = value->color.b;
}

// A function to read an SDL colour into a value
static void read_rgb(SettingValue *value, SDL_Color color)
{
    value->color.r = color.r;
    value->color.g = color.g;
    value->color.b = color.b;
}

// A function to replace a string setting with a copy of a value's text; NULL for none
static void store_text(char **field, const SettingValue *value)
{
    free(*field);
    *field = value->inherit || value->text[0] == '\0' ? NULL : strdup(value->text);
}

// A function to read a string setting into a value; one that is unset follows the default
static void read_text(SettingValue *value, const char *field, bool can_inherit)
{
    snprintf(value->text, sizeof(value->text), "%s", field != NULL ? field : "");
    value->inherit = can_inherit && field == NULL;
}

// A function to read a percentage or px setting into a value
static void read_percent(SettingValue *value, int number, bool percent)
{
    value->number = number;
    value->percent = percent;
}

// A function to derive a colour's input for derive_settings(), its alpha unused
static DeriveColor derive_color(SDL_Color color)
{
    return (DeriveColor) { color.r, color.g, color.b, 0 };
}

// A function to set the values config.ini's defaults give as text (the percentages), before the
// file is read: the file's own values then replace them
void config_apply_defaults(void)
{
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        const SettingDef *def = setting_def((SettingId) id);
        SettingValue value;
        if (def->fallback == NULL)
            continue;
        if (setting_parse(def, def->fallback, &value))
            config_store((SettingId) id, NULL, &value);
        else
            log_error("The built-in %s value '%s' does not read, ignoring it", def->key, def->fallback);
    }
}

// A function to store a setting's value into Config, or into `menu` for a per-menu setting
void config_store(SettingId id, Menu *menu, const SettingValue *value)
{
    int n = value->number;
    bool on = n != 0;
    switch (id) {
        case SET_ID_BACKGROUND_MODE: config.background_mode = (ModeBackground) n; break;
        case SET_ID_BACKGROUND_COLOR: store_rgb(&config.background_color, value); break;
        case SET_ID_BACKGROUND_IMAGE: store_text(&config.background_image, value); break;
        case SET_ID_SLIDESHOW_DIRECTORY: store_text(&config.slideshow_directory, value); break;
        case SET_ID_SLIDESHOW_DURATION: config.slideshow_image_duration = (Uint32) n * 1000; break;
        case SET_ID_SLIDESHOW_FADE: config.slideshow_transition_time = (Uint32) n; break;
        case SET_ID_LAYOUT_ROWS: config.rows = (unsigned int) n; break;
        case SET_ID_LAYOUT_COLUMNS: config.max_buttons = (unsigned int) n; break;
        case SET_ID_LAYOUT_ICON_SIZE: config.icon_size = value->inherit ? 0 : (Uint16) n; break;
        case SET_ID_TITLE_SIZE:
            if (value->percent)
                config.title_font_size_pct = n;
            else {
                config.title_font_size_pct = 0;
                config.title_font_size = (unsigned int) n;
            }
            break;
        case SET_ID_DEFAULT_MENU: store_text(&config.default_menu, value); break;
        case SET_ID_WRAP_ENTRIES: config.wrap_entries = on; break;
        case SET_ID_RESET_ON_BACK: config.reset_on_back = on; break;
        case SET_ID_MOUSE_SELECT: config.mouse_select = on; break;
        case SET_ID_INHIBIT_OS_SCREENSAVER: config.inhibit_os_screensaver = on; break;
        case SET_ID_VSYNC: config.vsync = on; break;
        case SET_ID_FPS_LIMIT: config.fps_limit = value->inherit ? -1 : n; break;
        case SET_ID_ON_LAUNCH: config.on_launch = (ModeOnLaunch) n; break;
        case SET_ID_APPLICATION_TIMEOUT: config.application_timeout = (Uint32) n * 1000; break;
        case SET_ID_STARTUP_CMD: store_text(&config.startup_cmd, value); break;
        case SET_ID_QUIT_CMD: store_text(&config.quit_cmd, value); break;
        case SET_ID_CHROMA_KEY_COLOR: store_rgb(&config.chroma_key_color, value); break;
        case SET_ID_OVERLAY: config.background_overlay = on; break;
        case SET_ID_OVERLAY_COLOR: store_rgb(&config.background_overlay_color, value); break;
        case SET_ID_OVERLAY_OPACITY: config.background_overlay_opacity = n; break;
        case SET_ID_ICON_SPACING:
            config.icon_spacing = n;
            config.icon_spacing_percent = value->percent;
            break;
        case SET_ID_VCENTER: config.vcenter = n; break;
        case SET_ID_TITLES_ENABLED: config.titles_enabled = on; break;
        case SET_ID_TITLE_FONT: store_text(&config.title_font_path, value); break;
        case SET_ID_TITLE_FONT_FACE: config.title_font_face = value->inherit ? 0 : n; break;
        case SET_ID_TITLE_COLOR: store_rgb(&config.title_font_color, value); break;
        case SET_ID_TITLE_OPACITY: config.title_opacity = n; break;
        case SET_ID_TITLE_SHADOWS: config.title_shadows = on; break;
        case SET_ID_TITLE_SHADOW_COLOR: store_rgb(&config.title_shadow_color, value); break;
        case SET_ID_TITLE_OVERSIZE: config.title_oversize_mode = (ModeOversize) n; break;
        case SET_ID_TITLE_PADDING:
            // A percentage in hundredths is whole here (SET_FLAG_WHOLE), as layout.c reads it
            config.title_padding_pct = value->percent ? n / 100 : 0;
            config.title_padding = value->percent ? 0 : n;
            break;
        case SET_ID_HIGHLIGHT_ENABLED: config.highlight = on; break;
        case SET_ID_HIGHLIGHT_FILL_COLOR: store_rgb(&config.highlight_fill_color, value); break;
        case SET_ID_HIGHLIGHT_FILL_OPACITY: config.highlight_fill_opacity = n; break;
        case SET_ID_HIGHLIGHT_OUTLINE_SIZE: config.highlight_outline_size = n; break;
        case SET_ID_HIGHLIGHT_OUTLINE_COLOR: store_rgb(&config.highlight_outline_color, value); break;
        case SET_ID_HIGHLIGHT_OUTLINE_OPACITY: config.highlight_outline_opacity = n; break;
        case SET_ID_HIGHLIGHT_CORNER_RADIUS: config.highlight_rx = n; break;
        case SET_ID_HIGHLIGHT_VPADDING: config.highlight_vpadding = n; break;
        case SET_ID_HIGHLIGHT_HPADDING: config.highlight_hpadding = n; break;
        case SET_ID_SCROLL_ENABLED: config.scroll_indicators = on; break;
        case SET_ID_SCROLL_FILL_COLOR: store_rgb(&config.scroll_indicator_fill_color, value); break;
        case SET_ID_SCROLL_OUTLINE_SIZE: config.scroll_indicator_outline_size = n; break;
        case SET_ID_SCROLL_OUTLINE_COLOR: store_rgb(&config.scroll_indicator_outline_color, value); break;
        case SET_ID_SCROLL_OPACITY: config.scroll_indicator_opacity = n; break;
        case SET_ID_CLOCK_ENABLED: config.clock_enabled = on; break;
        case SET_ID_CLOCK_SHOW_DATE: config.clock_show_date = on; break;
        case SET_ID_CLOCK_ALIGNMENT: config.clock_alignment = (Alignment) n; break;
        case SET_ID_CLOCK_FONT: store_text(&config.clock_font_path, value); break;
        case SET_ID_CLOCK_FONT_FACE: config.clock_font_face = value->inherit ? 0 : n; break;
        case SET_ID_CLOCK_COLOR: store_rgb(&config.clock_font_color, value); break;
        case SET_ID_CLOCK_SHADOWS: config.clock_shadows = on; break;
        case SET_ID_CLOCK_SHADOW_COLOR: store_rgb(&config.clock_shadow_color, value); break;
        case SET_ID_CLOCK_OPACITY: config.clock_opacity = n; break;
        case SET_ID_CLOCK_FONT_SIZE: config.clock_font_size = (unsigned int) n; break;
        case SET_ID_CLOCK_MARGIN:
            config.clock_margin = n;
            config.clock_margin_percent = value->percent;
            break;
        case SET_ID_CLOCK_TIME_FORMAT: config.clock_time_format = (TimeFormat) n; break;
        case SET_ID_CLOCK_DATE_FORMAT: config.clock_date_format = (DateFormat) n; break;
        case SET_ID_CLOCK_WEEKDAY: config.clock_include_weekday = on; break;
        case SET_ID_SCREENSAVER_ENABLED: config.screensaver_enabled = on; break;
        case SET_ID_SCREENSAVER_IDLE_TIME: config.screensaver_idle_time = (Uint32) n * 1000; break;
        case SET_ID_SCREENSAVER_INTENSITY: config.screensaver_intensity = n; break;
        case SET_ID_SCREENSAVER_PAUSE: config.screensaver_pause_slideshow = on; break;
        case SET_ID_GAMEPAD_ENABLED: config.gamepad_enabled = on; break;
        case SET_ID_GAMEPAD_DEVICE: config.gamepad_device = n; break;
        case SET_ID_GAMEPAD_MAPPINGS: store_text(&config.gamepad_mappings_file, value); break;
        case SET_ID_MENU_ROWS: menu->overrides.rows = value->inherit ? 0 : n; break;
        case SET_ID_MENU_COLUMNS: menu->overrides.columns = value->inherit ? 0 : n; break;
        case SET_ID_MENU_ICON_SIZE: menu->overrides.icon_cap = value->inherit ? 0 : n; break;
        case SET_ID_COUNT: break;
    }
}

// A function to read a setting's value from Config, or from `menu` for a per-menu setting
SettingValue config_read(SettingId id, const Menu *menu)
{
    SettingValue v;
    memset(&v, 0, sizeof(v));
    switch (id) {
        case SET_ID_BACKGROUND_MODE: v.number = (int) config.background_mode; break;
        case SET_ID_BACKGROUND_COLOR: read_rgb(&v, config.background_color); break;
        case SET_ID_BACKGROUND_IMAGE: read_text(&v, config.background_image, false); break;
        case SET_ID_SLIDESHOW_DIRECTORY: read_text(&v, config.slideshow_directory, false); break;
        case SET_ID_SLIDESHOW_DURATION: v.number = (int) (config.slideshow_image_duration / 1000); break;
        case SET_ID_SLIDESHOW_FADE: v.number = (int) config.slideshow_transition_time; break;
        case SET_ID_LAYOUT_ROWS: v.number = (int) config.rows; break;
        case SET_ID_LAYOUT_COLUMNS: v.number = (int) config.max_buttons; break;
        case SET_ID_LAYOUT_ICON_SIZE:
            v.inherit = config.icon_size == 0;
            v.number = config.icon_size;
            break;
        case SET_ID_TITLE_SIZE:
            v.percent = config.title_font_size_pct > 0;
            v.number = v.percent ? config.title_font_size_pct : (int) config.title_font_size;
            break;
        case SET_ID_DEFAULT_MENU: read_text(&v, config.default_menu, false); break;
        case SET_ID_WRAP_ENTRIES: v.number = config.wrap_entries; break;
        case SET_ID_RESET_ON_BACK: v.number = config.reset_on_back; break;
        case SET_ID_MOUSE_SELECT: v.number = config.mouse_select; break;
        case SET_ID_INHIBIT_OS_SCREENSAVER: v.number = config.inhibit_os_screensaver; break;
        case SET_ID_VSYNC: v.number = config.vsync; break;
        case SET_ID_FPS_LIMIT:
            v.inherit = config.fps_limit < 0;
            v.number = config.fps_limit;
            break;
        case SET_ID_ON_LAUNCH: v.number = (int) config.on_launch; break;
        case SET_ID_APPLICATION_TIMEOUT: v.number = (int) (config.application_timeout / 1000); break;
        case SET_ID_STARTUP_CMD: read_text(&v, config.startup_cmd, true); break;
        case SET_ID_QUIT_CMD: read_text(&v, config.quit_cmd, true); break;
        case SET_ID_CHROMA_KEY_COLOR: read_rgb(&v, config.chroma_key_color); break;
        case SET_ID_OVERLAY: v.number = config.background_overlay; break;
        case SET_ID_OVERLAY_COLOR: read_rgb(&v, config.background_overlay_color); break;
        case SET_ID_OVERLAY_OPACITY: read_percent(&v, config.background_overlay_opacity, true); break;
        case SET_ID_ICON_SPACING: read_percent(&v, config.icon_spacing, config.icon_spacing_percent); break;
        case SET_ID_VCENTER: read_percent(&v, config.vcenter, true); break;
        case SET_ID_TITLES_ENABLED: v.number = config.titles_enabled; break;
        case SET_ID_TITLE_FONT: read_text(&v, config.title_font_path, false); break;
        case SET_ID_TITLE_FONT_FACE:
            v.inherit = config.title_font_face == 0;
            v.number = config.title_font_face;
            break;
        case SET_ID_TITLE_COLOR: read_rgb(&v, config.title_font_color); break;
        case SET_ID_TITLE_OPACITY: read_percent(&v, config.title_opacity, true); break;
        case SET_ID_TITLE_SHADOWS: v.number = config.title_shadows; break;
        case SET_ID_TITLE_SHADOW_COLOR: read_rgb(&v, config.title_shadow_color); break;
        case SET_ID_TITLE_OVERSIZE: v.number = (int) config.title_oversize_mode; break;
        case SET_ID_TITLE_PADDING:
            // A 0 px padding reads as 0%: the same, and the one the steps hold
            if (config.title_padding_pct > 0 || config.title_padding == 0)
                read_percent(&v, config.title_padding_pct * 100, true);
            else
                read_percent(&v, config.title_padding, false);
            break;
        case SET_ID_HIGHLIGHT_ENABLED: v.number = config.highlight; break;
        case SET_ID_HIGHLIGHT_FILL_COLOR: read_rgb(&v, config.highlight_fill_color); break;
        case SET_ID_HIGHLIGHT_FILL_OPACITY: read_percent(&v, config.highlight_fill_opacity, true); break;
        case SET_ID_HIGHLIGHT_OUTLINE_SIZE: v.number = config.highlight_outline_size; break;
        case SET_ID_HIGHLIGHT_OUTLINE_COLOR: read_rgb(&v, config.highlight_outline_color); break;
        case SET_ID_HIGHLIGHT_OUTLINE_OPACITY: read_percent(&v, config.highlight_outline_opacity, true); break;
        case SET_ID_HIGHLIGHT_CORNER_RADIUS: v.number = config.highlight_rx; break;
        case SET_ID_HIGHLIGHT_VPADDING: v.number = config.highlight_vpadding; break;
        case SET_ID_HIGHLIGHT_HPADDING: v.number = config.highlight_hpadding; break;
        case SET_ID_SCROLL_ENABLED: v.number = config.scroll_indicators; break;
        case SET_ID_SCROLL_FILL_COLOR: read_rgb(&v, config.scroll_indicator_fill_color); break;
        case SET_ID_SCROLL_OUTLINE_SIZE: v.number = config.scroll_indicator_outline_size; break;
        case SET_ID_SCROLL_OUTLINE_COLOR: read_rgb(&v, config.scroll_indicator_outline_color); break;
        case SET_ID_SCROLL_OPACITY: read_percent(&v, config.scroll_indicator_opacity, true); break;
        case SET_ID_CLOCK_ENABLED: v.number = config.clock_enabled; break;
        case SET_ID_CLOCK_SHOW_DATE: v.number = config.clock_show_date; break;
        case SET_ID_CLOCK_ALIGNMENT: v.number = (int) config.clock_alignment; break;
        case SET_ID_CLOCK_FONT: read_text(&v, config.clock_font_path, false); break;
        case SET_ID_CLOCK_FONT_FACE:
            v.inherit = config.clock_font_face == 0;
            v.number = config.clock_font_face;
            break;
        case SET_ID_CLOCK_COLOR: read_rgb(&v, config.clock_font_color); break;
        case SET_ID_CLOCK_SHADOWS: v.number = config.clock_shadows; break;
        case SET_ID_CLOCK_SHADOW_COLOR: read_rgb(&v, config.clock_shadow_color); break;
        case SET_ID_CLOCK_OPACITY: read_percent(&v, config.clock_opacity, true); break;
        case SET_ID_CLOCK_FONT_SIZE: v.number = (int) config.clock_font_size; break;
        case SET_ID_CLOCK_MARGIN: read_percent(&v, config.clock_margin, config.clock_margin_percent); break;
        case SET_ID_CLOCK_TIME_FORMAT: v.number = (int) config.clock_time_format; break;
        case SET_ID_CLOCK_DATE_FORMAT: v.number = (int) config.clock_date_format; break;
        case SET_ID_CLOCK_WEEKDAY: v.number = config.clock_include_weekday; break;
        case SET_ID_SCREENSAVER_ENABLED: v.number = config.screensaver_enabled; break;
        case SET_ID_SCREENSAVER_IDLE_TIME: v.number = (int) (config.screensaver_idle_time / 1000); break;
        case SET_ID_SCREENSAVER_INTENSITY: read_percent(&v, config.screensaver_intensity, true); break;
        case SET_ID_SCREENSAVER_PAUSE: v.number = config.screensaver_pause_slideshow; break;
        case SET_ID_GAMEPAD_ENABLED: v.number = config.gamepad_enabled; break;
        case SET_ID_GAMEPAD_DEVICE: v.number = config.gamepad_device; break;
        case SET_ID_GAMEPAD_MAPPINGS: read_text(&v, config.gamepad_mappings_file, false); break;
        case SET_ID_MENU_ROWS:
            v.inherit = menu->overrides.rows == 0;
            v.number = menu->overrides.rows;
            break;
        case SET_ID_MENU_COLUMNS:
            v.inherit = menu->overrides.columns == 0;
            v.number = menu->overrides.columns;
            break;
        case SET_ID_MENU_ICON_SIZE:
            v.inherit = menu->overrides.icon_cap == 0;
            v.number = menu->overrides.icon_cap;
            break;
        case SET_ID_COUNT:
            break;
    }
    return v;
}

// A function to gather the configured values derive_settings() works from
DeriveInput derive_input(void)
{
    DeriveInput in;
    memset(&in, 0, sizeof(in));
    in.screen_width = geo.screen_width;
    in.screen_height = geo.screen_height;
    in.titles_enabled = config.titles_enabled;
    in.title_padding = config.title_padding;
    in.title_padding_pct = config.title_padding_pct;
    in.title_color = derive_color(config.title_font_color);
    in.title_shadow_color = derive_color(config.title_shadow_color);
    in.title_opacity = config.title_opacity;
    in.overlay_color = derive_color(config.background_overlay_color);
    in.overlay_opacity = config.background_overlay_opacity;
    in.highlight_fill = derive_color(config.highlight_fill_color);
    in.highlight_fill_opacity = config.highlight_fill_opacity;
    in.highlight_outline = derive_color(config.highlight_outline_color);
    in.highlight_outline_opacity = config.highlight_outline_opacity;
    in.highlight_outline_size = config.highlight_outline_size;
    in.highlight_rx = config.highlight_rx;
    in.highlight_hpadding = config.highlight_hpadding;
    in.highlight_vpadding = config.highlight_vpadding;
    in.scroll_fill = derive_color(config.scroll_indicator_fill_color);
    in.scroll_outline = derive_color(config.scroll_indicator_outline_color);
    in.scroll_opacity = config.scroll_indicator_opacity;
    in.scroll_outline_size = config.scroll_indicator_outline_size;
    in.clock_color = derive_color(config.clock_font_color);
    in.clock_shadow_color = derive_color(config.clock_shadow_color);
    in.clock_opacity = config.clock_opacity;
    in.icon_spacing = config.icon_spacing;
    in.icon_spacing_percent = config.icon_spacing_percent;
    in.vcenter = config.vcenter;
    in.clock_margin = config.clock_margin;
    in.clock_margin_percent = config.clock_margin_percent;
    in.screensaver_intensity = config.screensaver_intensity;
    return in;
}
```

Add to `src/CMakeLists.txt`'s `SOURCES`, after `settings.h` (line 30):

```cmake
  derive.c
  derive.h
  config_fields.c
  config_fields.h
```

- [ ] **Step 6: Read every settings key through the table**

In `src/util.c`:
- Add `#include "config_fields.h"` after `#include "settings.h"` (line 15).
- Remove the `store_background_setting` prototype (line 26) and its definition (lines 157-186).
- Remove `parse_mode_setting`'s prototype (line 21) and definition (lines 621-631). `get_mode_setting()` (lines 633-636) and the `mode_settings` table stay: `debug.c` and `reload_background()` still name the modes through them.

Replace `config_handler()` from its start (line 188) down to the `// Parse menus/entries` comment (line 496) with:

```c
// A function to tell whether a section holds settings rather than a menu's entries
static bool settings_section(const char *section)
{
    static const char *const sections[] = {
        "General", "Layout", "Background", "Titles", "Highlight", "Scroll Indicators", "Clock",
        "Screensaver", "Gamepad"
    };
    for (size_t i = 0; i < sizeof(sections) / sizeof(sections[0]); i++) {
        if (MATCH(section, sections[i]))
            return true;
    }
    return false;
}

// A function to handle config file parsing: every setting through the settings table, the hotkeys
// and gamepad controls into their lists, and every other section as a menu
int config_handler(void *user, const char *section, const char *name, const char *value)
{
    UNUSED(user);

    if (MATCH(section, "Hotkeys")) {
        char *rest = NULL;
        char *keycode = strtok_r((char*) value, ";", &rest);
        if (keycode != NULL) {
            char *cmd = strtok_r(NULL, "", &rest);
            if (cmd != NULL)
                add_hotkey(keycode, cmd);
        }
        return 0;
    }

    if (settings_section(section)) {
        const SettingDef *def = setting_find(section, name);
        if (def == NULL) {
            // Any other key in [Gamepad] is a control; one in any other settings section is ignored
            if (MATCH(section, "Gamepad"))
                add_gamepad_control(name, value);
            return 0;
        }
        bool alias = def->alias != NULL && MATCH(name, def->alias);
        SettingValue parsed;
        if (!setting_parse(def, value, &parsed))
            log_error("Invalid %s value '%s' in [%s], ignoring it", name, value, section);
        else if (!alias || !columns_set) {
            config_store(def->id, NULL, &parsed);
            // Columns wins over its older name, MaxButtons, whichever comes first
            if (def->id == SET_ID_LAYOUT_COLUMNS && !alias)
                columns_set = true;
        }
        return 0;
    }

```

This keeps the menu branch that follows (the `else {` at line 497 becomes a plain block): replace `    // Parse menus/entries\n    else {` with `    // Parse menus/entries\n    {`. In that branch's per-menu keys (lines 529-539), replace the three assignments with the store:

```c
            SettingValue parsed;
            if (!setting_parse(setting_def(id), value, &parsed))
                log_error("Invalid %s value '%s' in menu '%s', ignoring it", name, value, section);
            else
                config_store(id, menu, &parsed);
            return 0;
```

Remove `is_percent()` (lines 638-646), `convert_percent_to_int()` (lines 974-984) and `validate_settings()` (lines 986-1100) from `util.c`. Remove their prototypes, and `INVALID_PERCENT_VALUE`, from `util.h`.

- [ ] **Step 7: Derive the drawn values, and draw with them**

In `src/launcher.c`:
- Add `#include "config_fields.h"` after `#include "clock.h"` (line 17).
- Add these globals after `Uint32 repeat_period;` (line 204):

```c
Effective eff;                        // The values drawn with, derived from config (derive.h)
SDL_Color title_color;                // eff's colours as SDL colours, for the text they draw
SDL_Color title_shadow_color;
SDL_Color clock_color;
SDL_Color clock_shadow_color;
```

- Add this function above `init_sdl_ttf()`:

```c
// A function to derive the values the launcher draws with from config, after the parse and after
// every change the settings screen makes
void refresh_effective()
{
    DeriveInput in = derive_input();
    derive_settings(&in, &eff);
    geo.vcenter = eff.vcenter;
    title_color = (SDL_Color) { eff.title_color.r, eff.title_color.g, eff.title_color.b, eff.title_color.a };
    title_shadow_color = (SDL_Color) { eff.title_shadow_color.r, eff.title_shadow_color.g,
                                       eff.title_shadow_color.b, eff.title_shadow_color.a };
    clock_color = (SDL_Color) { eff.clock_color.r, eff.clock_color.g, eff.clock_color.b, eff.clock_color.a };
    clock_shadow_color = (SDL_Color) { eff.clock_shadow_color.r, eff.clock_shadow_color.g,
                                       eff.clock_shadow_color.b, eff.clock_shadow_color.a };
}
```

- Replace `init_sdl_ttf()` (lines 318-352) with:

```c
// A function to initialize SDL's TTF subsystem and open the title font
static void init_sdl_ttf()
{
    if (TTF_Init() == -1)
        log_fatal("Could not initialize SDL_ttf\n%s", TTF_GetError());

    title_info = (TextInfo) {
        .font_size = (int) config.title_font_size,
        .shadow = config.title_shadows,
        .font_path = NULL,
        .max_width = 0, // Set per menu in render_buttons: its button size, less room for a shadow
        .min_size = geo.title_min_size,
        .oversize_mode = config.title_oversize_mode,
        .color = &title_color,
        .shadow_color = config.title_shadows ? &title_shadow_color : NULL
    };
    if (load_font(&title_info, config.title_font_path, config.title_font_face, FILENAME_DEFAULT_FONT))
        log_fatal("Could not load title font");
    fixed_title_font = title_info.font;
    geo.font_height = config.titles_enabled ? TTF_FontHeight(title_info.font) : 0;

    // A percentage FontSize sizes each menu's titles from its buttons, so measure the font's line
    // height per point once, at a large size, for the layout to reserve room for any size
    TTF_Font *probe = TTF_OpenFontIndex(title_info.font_path, TITLE_MEASURE_SIZE, title_info.font_face);
    geo.title_line_pm = probe != NULL ? TTF_FontHeight(probe) * 1000 / TITLE_MEASURE_SIZE : 1500;
    if (probe != NULL)
        TTF_CloseFont(probe);
}
```

- In `cleanup()`, after `title_info.font = NULL;` (line 384), add `free(title_info.font_path);` and `title_info.font_path = NULL;`. Replace the clock font's close (lines 385-386) with:

```c
    if (clk != NULL && clk->text_info.font != NULL)
        TTF_CloseFont(clk->text_info.font);
    if (clk != NULL)
        free(clk->text_info.font_path);
```

- In `compute_menu_layout()` (lines 848-861), replace `.spacing = config.icon_spacing,`, `.hpad = config.highlight_hpadding,`, `.vpad = config.highlight_vpadding,`, `.title_padding = titles ? config.title_padding : 0,` and `.title_padding_pct = titles ? config.title_padding_pct : 0,` with:

```c
        .spacing           = eff.icon_spacing,
        ...
        .hpad              = eff.highlight_hpadding,
        .vpad              = eff.highlight_vpadding,
        .title_padding     = eff.title_padding,
        .title_padding_pct = eff.title_padding_pct,
```

  `derive_settings()` already zeroes both paddings without titles.
- In `init_screensaver()` (lines 661-681), replace everything from `// Convert intensity string to float` down to `screensaver->alpha_end_value = 255.0f;` with:

```c
    // Full dim is the Intensity setting's alpha; one that dims nothing cannot run
    screensaver->alpha_end_value = (float) eff.screensaver_alpha;
    if (eff.screensaver_alpha < 1) {
        log_error("Invalid screensaver intensity value, disabling feature");
        free(screensaver);
        screensaver = NULL;
        return;
    }
```

  `config.screensaver_enabled` is no longer switched off here. In `main()` and the main loop, the screensaver is now used only when `screensaver != NULL`: replace `if (config.screensaver_enabled && (!settings_is_open() || state.screensaver_active))` (line 1843) with `if (screensaver != NULL && (!settings_is_open() || state.screensaver_active))`.
- In `main()`'s overlay block (lines 1721-1726), replace the four `config.background_overlay_color.*` arguments with `eff.overlay_color.r`, `eff.overlay_color.g`, `eff.overlay_color.b` and `eff.overlay_color.a`.
- In `main()`, call `config_apply_defaults();` as the first line after `config.exe_path = SDL_GetBasePath();` (line 1630). Replace `validate_settings(&geo);` (line 1654) with `refresh_effective();`.
- In `execute_command()`, add after the `SCMD_SLEEP` branch (line 1220):

```c
        else if (!strcmp(special_command, SCMD_EXIT))
            log_error("':exit' works only as a hotkey on Windows, where it closes the app on show; ignoring it");
```

In `src/image.h`, replace `TextInfo` (lines 13-23), the `calculate_shadow_alpha` macro (line 11) and the `load_font` prototype (line 27) with:

```c
typedef struct {
    TTF_Font *font;
    int font_size;
    char *font_path;       // The file the font was opened from (owned): the configured one, or the bundled fallback
    int font_face;         // The face within it
    SDL_Color *color;
    bool shadow;
    SDL_Color *shadow_color;
    int max_width;
    int min_size; // Shrink mode stops here: the readable minimum
    ModeOversize oversize_mode;
} TextInfo;
```

```c
int load_font(TextInfo *info, const char *configured, int face, const char *default_font);
```

In `src/image.c`:
- Add `extern TextInfo title_info;` and `extern Effective eff;` after `extern SDL_Renderer *renderer;` (line 32).
- In `title_font()` (line 70), open `TTF_OpenFontIndex(title_info.font_path, size, title_info.font_face)`.
- In `render_text()` (lines 471 and 479), open `TTF_OpenFontIndex(info->font_path, size, info->font_face)`.
- In `render_highlight()` (lines 351-370), draw from `eff`:

```c
    if (eff.highlight_outline_size) {
        float stroke_opacity = ((float) eff.highlight_outline.a) / 255.0f;
        format_highlight_outline(&outline_buffer,
            eff.highlight_outline_size,
            eff.highlight_outline,
            stroke_opacity
        );
    }
    else
        outline_buffer = "";

    float fill_opacity = ((float) eff.highlight_fill.a) / 255.0f;
    format_highlight(&buffer,
        width,
        height,
        eff.highlight_rx,
        eff.highlight_fill,
        fill_opacity,
        outline_buffer
    );
```

  Its cleanup's `if (config.highlight_outline_size)` (line 377) becomes `if (eff.highlight_outline_size)`.
- In `render_scroll_indicators()` (lines 389-395):

```c
    float opacity = (float) eff.scroll_fill.a / 255.0f;
    format_scroll_indicator(&buffer,
        eff.scroll_fill,
        eff.scroll_outline_size,
        eff.scroll_outline,
        opacity
    );
```

  Keep the line `    if (scroll->texture == NULL)` exactly as it is: `run.sh scrollfail` injects its fault before it.
- Replace `load_font()` (lines 577-615) with:

```c
// A function to open a text's font: the configured file and face (a relative path is also tried
// beside the executable), else the bundled font. The configured path is never changed: what was
// opened is kept in info->font_path, and a failure says so in the log.
int load_font(TextInfo *info, const char *configured, int face, const char *default_font)
{
    free(info->font_path);
    info->font_path = NULL;
    info->font = NULL;
    info->font_face = 0;
    if (configured != NULL) {
        info->font = TTF_OpenFontIndex(configured, info->font_size, face);
        if (info->font != NULL)
            info->font_path = strdup(configured);

        // A relative path in the config means the folder StreamFlex is in, not the one it was started
        // from (the Windows config names .\assets\fonts\...; a shortcut's "Start in" folder can be anywhere)
        else if (config.exe_path != NULL && is_relative_path(configured)) {
            char exe_font_path[MAX_PATH_CHARS + 1];
            join_paths(exe_font_path, sizeof(exe_font_path), 2, config.exe_path, configured);
            info->font = TTF_OpenFontIndex(exe_font_path, info->font_size, face);
            if (info->font != NULL)
                info->font_path = strdup(exe_font_path);
        }
        if (info->font != NULL)
            info->font_face = face;
        else
            log_error("Could not open the font %s (face %i), using the default font\n%s", configured, face, TTF_GetError());
    }
    if (info->font == NULL) {
        char *default_font_path = find_default_font(default_font);
        if (default_font_path != NULL)
            info->font = TTF_OpenFont(default_font_path, info->font_size);
        if (info->font == NULL) {
            free(default_font_path);
            log_fatal("Could not load default font");
            return 1;
        }
        info->font_path = default_font_path;
    }
    return 0;
}
```

In `src/clock.c`:
- Add `extern Effective eff;`, `extern SDL_Color clock_color;` and `extern SDL_Color clock_shadow_color;` after `extern Geometry geo;` (line 25).
- In `calculate_clock_positioning()` (lines 183-195), replace every `config.clock_margin` with `eff.clock_margin`.
- In `init_clock()` (lines 202-225), replace the `TextInfo` set-up, the shadow block and the font's load with:

```c
    clk->text_info = (TextInfo) {
        .font = NULL,
        .font_size = (int) config.clock_font_size,
        .font_path = NULL,
        .color = &clock_color,
        .shadow = config.clock_shadows,
        .shadow_color = config.clock_shadows ? &clock_shadow_color : NULL,
        .oversize_mode = OVERSIZE_NONE
    };
    clk->time_format = config.clock_time_format;
    clk->date_format = config.clock_date_format;
    clk->time_info = NULL;

    // Load the font
    int error = load_font(&clk->text_info, config.clock_font_path, config.clock_font_face, FILENAME_DEFAULT_CLOCK_FONT);
    if (error) {
        config.clock_enabled = false;
        return;
    }
```

  Task 4 removes that last `config.clock_enabled` write with the rest of the clock's start and stop.

In `src/settings_screen.c`:
- Add `#include "config_fields.h"` after `#include "settings.h"` (line 13), and `extern TextInfo title_info;` after `extern LayoutGeometry layout;` (line 32).
- Replace `read_value()` (lines 241-298) and `replace_path()` (lines 300-305) with:

```c
// A function to read a setting's value from the running launcher
static SettingValue read_value(SettingId id, int menu_index)
{
    return config_read(id, menu_index >= 0 ? menus[menu_index] : NULL);
}
```

- Replace the first `switch` in `apply_slot()` (lines 310-362) with:

```c
    config_store(slot->def->id, slot->menu >= 0 ? menus[slot->menu] : NULL, &slot->value);
    if (slot->def->id == SET_ID_SLIDESHOW_FADE)
        update_slideshow_timing();
    refresh_effective();
```

  The `refresh` switch that follows (lines 363-377) stays for now: its `default:` covers the groups Task 6 wires.
- In `open_fonts()` (line 408), fall back to the font the titles really opened. The clock's is a later choice:

```c
        path = title_info.font_path;
```

In `src/debug.h`, replace `DEBUG_COLOR` (line 28) with `#define DEBUG_COLOR(setting_name, color) log_debug("%-25s #%.2X%.2X%.2X", setting_name ":", color.r, color.g, color.b)`. The colour's alpha is no longer the setting: its opacity is.

In `src/debug.c`, add `#include "settings.h"` and `#include "config_fields.h"`, and above `debug_settings()`, add:

```c
// A function to show a setting as config.ini would write it, through the settings table
static const char *debug_setting(SettingId id, char *out, size_t size)
{
    SettingValue value = config_read(id, NULL);
    setting_format(setting_def(id), &value, out, size);
    return out[0] != '\0' ? out : "(none)";
}
```

In `debug_settings()`, with `char text[SETTING_TEXT_MAX];` declared at its top:
- replace `DEBUG_INT(SETTING_FPS_LIMIT, config.fps_limit);` with `DEBUG_STR(SETTING_FPS_LIMIT, debug_setting(SET_ID_FPS_LIMIT, text, sizeof(text)));`;
- after `DEBUG_COLOR(SETTING_BACKGROUND_OVERLAY_COLOR, config.background_overlay_color);`, add `DEBUG_STR(SETTING_BACKGROUND_OVERLAY_OPACITY, debug_setting(SET_ID_OVERLAY_OPACITY, text, sizeof(text)));`;
- replace `DEBUG_INT(SETTING_ICON_SPACING, config.icon_spacing);` and the `VCENTER` line with:

```c
    DEBUG_STR(SETTING_ICON_SPACING, debug_setting(SET_ID_ICON_SPACING, text, sizeof(text)));
    DEBUG_STR(SETTING_VCENTER, debug_setting(SET_ID_VCENTER, text, sizeof(text)));
```

- after the `Padding` line (`DEBUG_STR(SETTING_TITLE_PADDING, title_value);`), add:

```c
    DEBUG_STR(SETTING_TITLE_OPACITY, debug_setting(SET_ID_TITLE_OPACITY, text, sizeof(text)));
    DEBUG_STR(SETTING_TITLE_FONT_FACE, debug_setting(SET_ID_TITLE_FONT_FACE, text, sizeof(text)));
```

  The `-A10` windows the harness reads under `Titles ===` end at `Padding`, which keeps its place.
- after `DEBUG_INT(SETTING_HIGHLIGHT_HPADDING, config.highlight_hpadding);`, add:

```c
    DEBUG_BOOL(SETTING_HIGHLIGHT_ENABLED, config.highlight);
    DEBUG_STR(SETTING_HIGHLIGHT_FILL_OPACITY, debug_setting(SET_ID_HIGHLIGHT_FILL_OPACITY, text, sizeof(text)));
    DEBUG_STR(SETTING_HIGHLIGHT_OUTLINE_OPACITY, debug_setting(SET_ID_HIGHLIGHT_OUTLINE_OPACITY, text, sizeof(text)));
```

- after the scroll indicators' `OUTLINE_COLOR` line, add `DEBUG_STR(SETTING_SCROLL_INDICATOR_OPACITY, debug_setting(SET_ID_SCROLL_OPACITY, text, sizeof(text)));`;
- replace `DEBUG_INT(SETTING_CLOCK_MARGIN, config.clock_margin);` with `DEBUG_STR(SETTING_CLOCK_MARGIN, debug_setting(SET_ID_CLOCK_MARGIN, text, sizeof(text)));`, and after `DEBUG_BOOL(SETTING_CLOCK_INCLUDE_WEEKDAY, ...)`, add `DEBUG_STR(SETTING_CLOCK_OPACITY, debug_setting(SET_ID_CLOCK_OPACITY, text, sizeof(text)));` and `DEBUG_STR(SETTING_CLOCK_FONT_FACE, debug_setting(SET_ID_CLOCK_FONT_FACE, text, sizeof(text)));`;
- replace the screensaver's `INTENSITY` line with `DEBUG_STR(SETTING_SCREENSAVER_INTENSITY, debug_setting(SET_ID_SCREENSAVER_INTENSITY, text, sizeof(text)));`;
- at the end of `debug_settings()`, before the closing brace, log the values drawn with:

```c
    log_debug("Effective: IconSpacing %i px, VCenter %i px, HPadding %i px, VPadding %i px, "
        "OutlineSize %i px, CornerRadius %i, Clock Margin %i px\n",
        eff.icon_spacing, eff.vcenter, eff.highlight_hpadding, eff.highlight_vpadding,
        eff.highlight_outline_size, eff.highlight_rx, eff.clock_margin);
```

`debug.c` also needs `extern Effective eff;`. The `FontFace` line reads `(none)` when the key is absent. The harness's `f41-fontface` reads `FontFace: 9`.

- [ ] **Step 8: Build, and see the harness pass**

Build and run the unit tests (Global Constraints). Expected: `100% tests passed`. The MSVC `/W4 /WX` build is only in CI, so also run the **Linux unit tests** with the label `t3`: `warnings outside src/external: 0`.

Run the headless harness with the label `t3`. Expected:
- `0 failed`: the four new checks pass, and every 3a check still passes (`f12-padding`'s `OutlineSize: 5`, `f14-junk`'s `Invalid MaxButtons value '7x'`, `f15-limits`, `f40-*`, `f60-color`'s pixels).
- The leak pass (label `t3-leaks`) is also `0 failed`: `TextInfo`'s path is freed for both fonts.

Then run both again on Fedora (labels `t3-fedora` and `t3-fedora-leaks`), in parallel with the Debian runs where the host's memory allows.

- [ ] **Step 9: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/config_fields.h src/config_fields.c src/launcher.h src/launcher.c src/util.h src/util.c src/image.h src/image.c src/clock.c src/debug.h src/debug.c src/settings_screen.c src/CMakeLists.txt tests/headless/checks/41-parse.sh tests/headless/fixtures/f41-clocksize.ini tests/headless/fixtures/f41-exit.ini tests/headless/fixtures/f41-fontface.ini tests/headless/fixtures/f41-values.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "refactor: one parse path through the settings table; Config keeps what the file says"
```

### Task 4: A start, stop and reload for every feature, and live frame timing

Each of these features gets a `start`, a `stop` and a `reload`:
- the overlay;
- the highlight;
- the scroll indicators;
- the clock;
- the screensaver;
- the gamepad.

`start` does nothing when its setting is off or it already runs. `stop` frees everything the feature holds. `reload` is stop then start, which lays the menu out again where the feature moves it. Startup calls the same `start`s. Nothing touches a feature's state unless it runs: every draw and update tests the feature's pointer, never its setting.

This task also makes these apply live:
- the frame timing: VSync switches live through `SDL_RenderSetVSync()`, which raises the SDL floor to 2.0.18;
- the OS screensaver block;
- the default menu;
- the title font.

It fixes the gamepad's id confusion and the clock's thread handoff on the way.

**Files:**
- Modify: `CMakeLists.txt:34` (`MIN_SDL_VERSION`), `CMakeLists.txt:191-196` (the shlibdeps comment), `docs/compilation.md:13`
- Modify: `src/launcher.h` (`State`, lines 113-125; prototypes at the end)
- Modify: `src/launcher.c` (`init_sdl()`, `create_window()`, `cleanup()`, `init_screensaver()`, `reload_background()`, `calculate_layout_area()`, `apply_layout()`, `place_entries()`, `reload_titles()`, `draw_scene()`, `present_frame()`, `draw_screen()`, `connect_gamepad()`, `open_controller()`, `test_pad_update()`, `update_clock()`, `pre_launch()`, `post_launch()`, `main()`)
- Modify: `src/clock.h`, `src/clock.c` (`init_clock()` returns an error instead of switching the clock off; atomics)
- Create: `tests/headless/fixtures/f42-all.ini`, `f42-fps10.ini`
- Create: `tests/headless/checks/42-features.sh`

**Interfaces:**
- Consumes: Task 3's `eff`, `refresh_effective()`, `title_color`/`title_shadow_color`, and `load_font(info, configured, face, default)`.
- Produces (declared in `launcher.h`; Task 6 calls each from its refresh groups):

```c
void reload_highlight(void);
void reload_scroll(void);
void reload_clock(void);        // Also lays the menu out again: the clock's size moves the buttons
void reload_screensaver(void);
void reload_gamepad(void);      // Also the Device setting: closes the pads and opens the chosen one
void reload_title_font(void);   // Closes the size cache and the fixed font, opens the font again, then reload_titles()
void apply_frame_timing(void);  // VSync and FPSLimit, live
void apply_os_screensaver(void);
void apply_default_menu(void);  // Points :home at config.default_menu
bool gamepad_running(void);
```

  `reload_background()` now also rebuilds the overlay. `State`'s `clock_rendering` and `clock_ready` become `SDL_atomic_t`.

- [ ] **Step 1: Write the failing harness checks**

Create `tests/headless/fixtures/f42-all.ini`, with every feature on, the screensaver after 3 s and the clock with its date:

```ini
[General]
DefaultMenu=Main

[Background]
Overlay=true
OverlayOpacity=40%

[Highlight]
OutlineSize=4
OutlineColor=#FF0000

[Clock]
Enabled=true
ShowDate=true

[Screensaver]
Enabled=true
IdleTime=3

[Main]
Entry1=One;apps;:quit
Entry2=Two;apps;:quit
Entry3=Three;apps;:quit
Entry4=Four;apps;:quit
Entry5=Five;apps;:quit
Entry6=Six;apps;:quit
```

Create `tests/headless/fixtures/f42-fps10.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit
VSync=false
FPSLimit=10

[Main]
Entry1=One;apps;:quit
```

Create `tests/headless/checks/42-features.sh`:

```bash
# Each feature starts, stops and frees what it holds; the frame timing is worked out from VSync and
# FPSLimit without changing either. The leak pass (run.sh leaks) proves the stops free everything.

# FPSLimit=10 is the documented minimum: with VSync off it sets the frame time, and VSync stays off
run_quick f42-fps10
ok=1
grep -q 'Frame timing: FPS limit 10, 100 ms a frame' "$out/f42-fps10.log" \
    && grep -A4 'General ===' "$out/f42-fps10.log" | grep -qE '^VSync:\s+false$' \
    && ran_clean f42-fps10 && ok=0
result "FPSLimit=10 with VSync off sets 100 ms frames, and leaves VSync as written (exit $(cat "$out/f42-fps10.code"))" $ok

# Every feature running at once, the screensaver on and off again, the virtual pad attached, and a
# clean quit: every feature's stop runs at quit
wake() { xdotool key Right; sleep 1; }
rm -f /tmp/pad-none
STREAMFLEX_TEST_PAD=/tmp/pad-none UNTIL='Screensaver off' run_after_line f42-all 'Screensaver on' +wake
log=$out/f42-all.log
ok=1
grep -q 'Overlay started' "$log" && grep -q 'Highlight started' "$log" && grep -q 'Scroll indicators started' "$log" \
    && grep -q 'Clock started' "$log" && grep -q 'Screensaver started' "$log" && grep -q 'Gamepad started' "$log" \
    && grep -q 'Gamepad connected with device index 0' "$log" \
    && sed -n '/Quitting program/,$p' "$log" | grep -q 'Clock stopped' \
    && sed -n '/Quitting program/,$p' "$log" | grep -q 'Gamepad stopped' \
    && ran_clean f42-all && ok=0
result "every feature starts, and stops at quit (exit $(cat "$out/f42-all.code"))" $ok
grep -E '(started|stopped|Screensaver o)' "$log" | sed 's/^/      /'
```

- [ ] **Step 2: Run the harness to see the new checks fail**

Run the headless harness with the label `t4-red`. Expected: both new checks `FAIL`. There is no `Frame timing:` line, and no `started` or `stopped` lines. Every other check passes.

- [ ] **Step 3: Raise the SDL floor to 2.0.18**

- In `CMakeLists.txt`, line 34, set `set(MIN_SDL_VERSION "2.0.18")`.
- Replace the comment at lines 191-196 with:

```cmake
    # dpkg-shlibdeps derives every dependency and its minimum version (glibc
    # included) from the built binary. No explicit CPACK_DEBIAN_PACKAGE_DEPENDS:
    # CPack appends shlibdeps' list to it without merging, so each library
    # would appear twice. SDL2's floor comes from its symbols file:
    # SDL_RenderSetVSync (2.0.18) makes the package depend on libsdl2-2.0-0
    # (>= 2.0.18), matching MIN_SDL_VERSION above.
```

- In `docs/compilation.md`, line 13, write ` - SDL ≥ 2.0.18`.

- [ ] **Step 4: Make the clock's handoff atomic, and let `init_clock()` fail without switching the clock off**

In `src/launcher.h`, in `State` (lines 123-124), replace `bool clock_rendering;` and `bool clock_ready;` with:

```c
    SDL_atomic_t clock_rendering;   // Both also written by the clock's render thread
    SDL_atomic_t clock_ready;
```

In `src/clock.h`, change `void init_clock(Clock *clk);` to `int init_clock(Clock *clk);`.

In `src/clock.c`:
- In `init_clock()`, change the return type to `int`. Replace the font's failure branch with:

```c
    int error = load_font(&clk->text_info, config.clock_font_path, config.clock_font_face, FILENAME_DEFAULT_CLOCK_FONT);
    if (error)
        return 1;
```

  and end the function with `return 0;`.
- In `render_clock()`, replace `state.clock_ready = true;` with `SDL_AtomicSet(&state.clock_ready, 1);`.
- In `render_clock_async()`, before `render_clock(clk);`, add the harness's delay:

```c
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: each render waits, so a key can land during one
    const char *delay = getenv("STREAMFLEX_TEST_CLOCK_DELAY_MS");
    if (delay != NULL)
        SDL_Delay((Uint32) atoi(delay));
#endif
```

`clock.c` needs `#include <stdlib.h>`, which it already has.

- [ ] **Step 5: Start, stop and reload each feature**

In `src/launcher.c`, add `static bool gamepad_on = false;   // The game controller subsystem is running` after the globals, and `static bool vsync_on = true;     // The renderer presents with VSync` beside it.

Replace `init_screensaver()` (lines 655-698) with:

```c
// A function to start the screensaver, when it is on and not already running
static void start_screensaver()
{
    if (!config.screensaver_enabled || screensaver != NULL)
        return;
    if (eff.screensaver_alpha < 1) {
        log_error("Invalid screensaver intensity value, disabling feature");
        return;
    }
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, geo.screen_width, geo.screen_height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == NULL) {
        log_error("Could not start the screensaver\n%s", SDL_GetError());
        return;
    }
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0xFF));
    screensaver = malloc(sizeof(Screensaver));
    screensaver->alpha_end_value = (float) eff.screensaver_alpha;
    screensaver->transition_change_rate = screensaver->alpha_end_value / ((float) SCREENSAVER_TRANSITION_TIME / (float) refresh_period);
    screensaver->texture = load_texture(surface);
    screensaver->alpha = 0.0f;
    SDL_SetTextureAlphaMod(screensaver->texture, 0);
    log_debug("Screensaver started");
}

// A function to stop the screensaver, lifting its dim and resuming a slideshow it paused
static void stop_screensaver()
{
    if (screensaver == NULL)
        return;
    if (state.screensaver_active && background_shown == BACKGROUND_SLIDESHOW) {
        state.slideshow_paused = false;
        ticks.slideshow_load = ticks.main;
    }
    state.screensaver_active = false;
    state.screensaver_transition = false;
    if (screensaver->texture != NULL)
        SDL_DestroyTexture(screensaver->texture);
    free(screensaver);
    screensaver = NULL;
    log_debug("Screensaver stopped");
}

// A function to restart the screensaver after one of its settings changed
void reload_screensaver()
{
    stop_screensaver();
    start_screensaver();
}

// A function to stop the overlay, freeing its texture
static void stop_overlay()
{
    if (background_overlay == NULL)
        return;
    SDL_DestroyTexture(background_overlay);
    background_overlay = NULL;
    log_debug("Overlay stopped");
}

// A function to start the overlay, when it is on: a screen-sized texture of its colour and opacity
static void start_overlay()
{
    if (!config.background_overlay || background_overlay != NULL)
        return;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, geo.screen_width, geo.screen_height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == NULL) {
        log_error("Could not start the overlay\n%s", SDL_GetError());
        return;
    }
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format,
        eff.overlay_color.r, eff.overlay_color.g, eff.overlay_color.b, eff.overlay_color.a));
    background_overlay = load_texture(surface);
    log_debug("Overlay started");
}

// A function to start the highlight, when it is on; its texture is rendered for each button size
// as menus load
static void start_highlight()
{
    if (!config.highlight || highlight != NULL)
        return;
    highlight = malloc(sizeof(Highlight));
    *highlight = (Highlight) { .texture = NULL, .button = 0, .hpad = 0, .vpad = 0, .title_block = 0 };
    log_debug("Highlight started");
}

// A function to stop the highlight, freeing its texture
static void stop_highlight()
{
    if (highlight == NULL)
        return;
    if (highlight->texture != NULL)
        SDL_DestroyTexture(highlight->texture);
    free(highlight);
    highlight = NULL;
    log_debug("Highlight stopped");
}

// A function to render the highlight again after one of its settings changed
void reload_highlight()
{
    stop_highlight();
    start_highlight();
    if (current_menu != NULL)
        apply_layout(current_menu);
}

// A function to start the scroll indicators, when they are on. An arrow that cannot be drawn
// leaves them stopped, with the setting as it was.
static void start_scroll()
{
    if (!config.scroll_indicators || scroll != NULL)
        return;
    scroll = malloc(sizeof(Scroll));
    scroll->texture = NULL;
    int scroll_indicator_height = (int) ((float) geo.screen_height * SCROLL_INDICATOR_HEIGHT);
    if (render_scroll_indicators(scroll, scroll_indicator_height, &geo)) {
        log_error("Could not render scroll indicator, disabling feature");
        free(scroll);
        scroll = NULL;
        return;
    }
    log_debug("Scroll indicators started");
}

// A function to stop the scroll indicators, freeing their texture
static void stop_scroll()
{
    if (scroll == NULL)
        return;
    if (scroll->texture != NULL)
        SDL_DestroyTexture(scroll->texture);
    free(scroll);
    scroll = NULL;
    log_debug("Scroll indicators stopped");
}

// A function to render the scroll indicators again after one of their settings changed
void reload_scroll()
{
    stop_scroll();
    start_scroll();
}

// A function to start the clock, when it is on: open its font and render the time now
static void start_clock()
{
    if (!config.clock_enabled || clk != NULL)
        return;
    clk = calloc(1, sizeof(Clock));
    SDL_AtomicSet(&state.clock_rendering, 0);
    SDL_AtomicSet(&state.clock_ready, 0);
    if (init_clock(clk)) {
        free(clk->text_info.font_path);
        free(clk);
        clk = NULL;
        log_error("The clock cannot start: no font opens");
        return;
    }
    ticks.clock_update = ticks.main;
    log_debug("Clock started");
}

// A function to stop the clock: wait for a render in flight on its thread, then free what it made
static void stop_clock()
{
    if (clk == NULL)
        return;
    bool waited = clock_thread != NULL;
    if (clock_thread != NULL) {
        SDL_WaitThread(clock_thread, NULL);
        clock_thread = NULL;
    }
    SDL_AtomicSet(&state.clock_rendering, 0);
    SDL_AtomicSet(&state.clock_ready, 0);
    if (clk->time_surface != NULL)
        SDL_FreeSurface(clk->time_surface);
    if (clk->date_surface != NULL)
        SDL_FreeSurface(clk->date_surface);
    if (clk->time_texture != NULL)
        SDL_DestroyTexture(clk->time_texture);
    if (clk->date_texture != NULL)
        SDL_DestroyTexture(clk->date_texture);
    if (clk->text_info.font != NULL)
        TTF_CloseFont(clk->text_info.font);
    free(clk->text_info.font_path);
    free(clk);
    clk = NULL;
    log_debug(waited ? "Clock stopped (it waited for a render in progress)" : "Clock stopped");
}

// A function to restart the clock after one of its settings changed, rendering it at once, then lay
// the menu out again: the clock's size moves the buttons
void reload_clock()
{
    stop_clock();
    start_clock();
    calculate_layout_area();
    if (current_menu != NULL)
        apply_layout(current_menu);
}
```

`calculate_layout_area()` and `apply_layout()` are declared `static` at the top of `launcher.c`, so these functions can call them.

Replace `update_clock()` (lines 1511-1548) with the same body using the atomics, plus the harness's every-second render:

```c
// A function to update the clock display
static void update_clock(bool block)
{
    if (ticks.main - ticks.clock_update > CLOCK_UPDATE_PERIOD) {
        if (!SDL_AtomicGet(&state.clock_rendering)) {

            // Check to see if the time has changed
            get_time(clk);
#ifdef STREAMFLEX_TEST_HOOKS
            // Only the headless harness builds this: with a slow render asked for, render every second
            if (getenv("STREAMFLEX_TEST_CLOCK_DELAY_MS") != NULL)
                clk->render_time = true;
#endif
            if (clk->render_time) {
                SDL_AtomicSet(&state.clock_rendering, 1);
                if (block)
                    render_clock(clk);
                else {
                    clock_thread = SDL_CreateThread(render_clock_async, "Clock Thread", (void*) clk);
#ifdef STREAMFLEX_TEST_HOOKS
                    // Only the headless harness builds this: a check waits for a render to be in flight
                    if (getenv("STREAMFLEX_TEST_CLOCK_DELAY_MS") != NULL)
                        log_debug("Clock: rendering on its thread");
#endif
                }
            }
            else
                ticks.clock_update = ticks.main;
        }

        // Render texture
        if (SDL_AtomicGet(&state.clock_ready)) {
            SDL_WaitThread(clock_thread, NULL);
            clock_thread = NULL;
            SDL_DestroyTexture(clk->time_texture);
            clk->time_texture = load_texture(clk->time_surface);
            clk->time_surface = NULL;
            if (clk->render_date) {
                SDL_DestroyTexture(clk->date_texture);
                clk->date_texture = load_texture(clk->date_surface);
                clk->date_surface = NULL;
            }
            ticks.clock_update = ticks.main;
            clk->render_time = false;
            clk->render_date = false;
            SDL_AtomicSet(&state.clock_rendering, 0);
            SDL_AtomicSet(&state.clock_ready, 0);
        }
    }
}
```

Add the gamepad's start and stop above `poll_gamepad()`:

```c
// A function to tell whether the gamepad subsystem is running
bool gamepad_running()
{
    return gamepad_on;
}

// A function to open every game controller present that the Device setting allows, by listing them:
// a pad already connected sends no connect event when the subsystem restarts
static void connect_present_pads()
{
    int count = SDL_NumJoysticks();
    for (int i = 0; i < count; i++) {
        if (SDL_IsGameController(i) == SDL_TRUE && (config.gamepad_device < 0 || config.gamepad_device == i)) {
            log_debug("Gamepad connected with device index %i", i);
            connect_gamepad(i, !state.application_running, true);
        }
    }
}

// A function to start the gamepad, when it is on: the game controller subsystem, the mappings file
// (once: SDL can add mappings but not remove them), the default controls and the pads present
static void start_gamepad()
{
    static bool mappings_loaded = false;
    if (!config.gamepad_enabled || gamepad_on)
        return;
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
        log_error("Could not start the gamepad\n%s", SDL_GetError());
        return;
    }
    gamepad_on = true;
    if (!mappings_loaded && config.gamepad_mappings_file != NULL) {
        mappings_loaded = true;
        if (SDL_GameControllerAddMappingsFromFile(config.gamepad_mappings_file) < 0)
            log_error("Could not load gamepad mappings from %s\n%s", config.gamepad_mappings_file, SDL_GetError());
    }
    add_default_gamepad_controls();
    connect_present_pads();
    log_debug("Gamepad started");
}

// A function to stop the gamepad: close every pad and the subsystem, and forget any press in progress
static void stop_gamepad()
{
    if (!gamepad_on)
        return;
#ifdef STREAMFLEX_TEST_HOOKS
    test_pad_stop();
#endif
    disconnect_gamepad(-1, true, true);
    for (GamepadControl *i = gamepad_controls; i != NULL; i = i->next)
        i->repeat = 0;
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    gamepad_on = false;
    log_debug("Gamepad stopped");
}

// A function to restart the gamepad after On or Device changed
void reload_gamepad()
{
    stop_gamepad();
    start_gamepad();
}
```

Move `test_pad_update()` (lines 1384-1405) above these functions and give it a stop, so the virtual pad is let go before its subsystem is:

```c
#ifdef STREAMFLEX_TEST_HOOKS
static SDL_Joystick *test_pad = NULL;   // The harness's virtual gamepad, while attached
static int test_pad_index = -1;

// A function only the headless harness builds, since it has no gamepad: with STREAMFLEX_TEST_PAD
// set, it attaches a virtual one while the gamepad runs, and holds its Start button while the file
// that names exists
static void test_pad_update()
{
    const char *held = getenv("STREAMFLEX_TEST_PAD");
    if (held == NULL || !gamepad_on)
        return;
    if (test_pad_index < 0) {
        test_pad_index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                             SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        test_pad = test_pad_index >= 0 ? SDL_JoystickOpen(test_pad_index) : NULL;
        if (test_pad == NULL)
            log_error("Test hook: no virtual gamepad\n%s", SDL_GetError());
    }
    if (test_pad != NULL)
        SDL_JoystickSetVirtualButton(test_pad, SDL_CONTROLLER_BUTTON_START, file_exists(held) ? SDL_PRESSED : SDL_RELEASED);
}

// A function to let the virtual gamepad go before its subsystem stops
static void test_pad_stop()
{
    if (test_pad != NULL)
        SDL_JoystickClose(test_pad);
    if (test_pad_index >= 0)
        SDL_JoystickDetachVirtual(test_pad_index);
    test_pad = NULL;
    test_pad_index = -1;
}
#endif
```

Fix the id confusion in `connect_gamepad()` (line 1281): search by the pad's instance id, which is what `init_gamepad()` stores:

```c
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(device_index);
        for (gamepad = gamepads; gamepad != NULL; gamepad = gamepad->next) {
            if (gamepad->id == (int) id)
                break;
        }
```

In `open_controller()` (line 1265), name the pad it failed to open: `log_error("Could not open gamepad at device index %i", gamepad->device_index);`.

- [ ] **Step 6: Work the frame timing out live, and apply the OS screensaver block and the default menu**

Add above `create_window()`:

```c
// A function to work out the frame timing from VSync and FPSLimit, live: VSync, or the FPS limit's
// own frame time when it is off and the limit lies between the minimum and the display's rate
// (otherwise VSync, as always). The settings stay as written; the renderer follows (SDL 2.0.18).
// The gamepad's repeat, the slideshow's fade and the screensaver's dim are timed in frames, so
// they are worked out again too.
void apply_frame_timing()
{
    int rate = display_mode.refresh_rate;
    bool vsync = config.vsync || config.fps_limit < MIN_FPS_LIMIT || config.fps_limit > rate;
    if (renderer != NULL && vsync != vsync_on) {
        if (SDL_RenderSetVSync(renderer, vsync ? 1 : 0) != 0) {
            log_error("Could not turn VSync %s\n%s", vsync ? "on" : "off", SDL_GetError());
            vsync = vsync_on;
        }
    }
    vsync_on = vsync;
    refresh_period = 1000 / (Uint32) (vsync ? rate : config.fps_limit);
    delay_period = GAMEPAD_REPEAT_DELAY / refresh_period;
    repeat_period = GAMEPAD_REPEAT_INTERVAL / refresh_period;
    if (!repeat_period)
        repeat_period = 1;
    update_slideshow_timing();
    if (screensaver != NULL)
        screensaver->transition_change_rate = screensaver->alpha_end_value / ((float) SCREENSAVER_TRANSITION_TIME / (float) refresh_period);
    if (vsync)
        log_debug("Frame timing: VSync at %i Hz, %u ms a frame", rate, refresh_period);
    else
        log_debug("Frame timing: FPS limit %i, %u ms a frame", config.fps_limit, refresh_period);
}

// A function to let the OS screensaver run, or block it, as InhibitOSScreensaver says
void apply_os_screensaver()
{
    if (config.inhibit_os_screensaver)
        SDL_DisableScreenSaver();
    else
        SDL_EnableScreenSaver();
}

// A function to point :home at the default menu the config names; one that is gone keeps the last
void apply_default_menu()
{
    Menu *menu = config.default_menu != NULL ? get_menu(config.default_menu) : NULL;
    if (menu != NULL)
        default_menu = menu;
}
```

In `create_window()`, replace lines 254-270 (from `Uint32 renderer_flags` through the gamepad timing) with:

```c
    // Create HW accelerated renderer, get screen resolution for geometry calculations
    apply_frame_timing();
    Uint32 renderer_flags = SDL_RENDERER_ACCELERATED;
    if (vsync_on)
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
```

`apply_frame_timing()` runs there before the renderer exists, so it sets no VSync of its own, and `vsync_on` gives the renderer's flag.

In `present_frame()` (line 1149), replace `if (!config.vsync) {` with `if (!vsync_on) {`.

In `init_sdl()` (lines 217-218), remove the two lines that add `SDL_INIT_GAMECONTROLLER`: `start_gamepad()` starts it now.

- [ ] **Step 7: Draw and update only what runs**

In `src/launcher.c`:
- `apply_layout()`, line 908: `if (config.highlight && (highlight->button ...` becomes `if (highlight != NULL && (highlight->button ...`.
- `place_entries()`, line 983: `if (config.highlight) {` becomes `if (highlight != NULL) {`.
- `draw_scene()`:
  - line 1094, `if (config.background_overlay)`, becomes `if (background_overlay != NULL)`;
  - line 1099, `if (config.scroll_indicators) {`, becomes `if (scroll != NULL) {`;
  - line 1117, `if (config.clock_enabled) {`, becomes `if (clk != NULL) {`;
  - line 1124, `if (config.highlight)`, becomes `if (highlight != NULL)`.
- `draw_screen()`, line 1162: `if (state.screensaver_active)` becomes `if (state.screensaver_active && screensaver != NULL)`.
- `calculate_layout_area()`, line 825: `if (config.clock_enabled && clk != NULL) {` becomes `if (clk != NULL) {`.
- `pre_launch()` is unchanged.
- `post_launch()`, lines 1569-1572, becomes:

```c
    if (gamepad_on)
        connect_gamepad(-1, true, false);
    if (clk != NULL)
        update_clock(true);
```

- In the main loop:
  - line 1845, `if (config.clock_enabled)`, becomes `if (clk != NULL)`;
  - line 1843's screensaver test, from Task 3, is `if (screensaver != NULL && ...)`.
- The events at lines 1780-1794 become:

```c
                case SDL_JOYDEVICEADDED:
                    if (gamepad_on && SDL_IsGameController(event.jdevice.which) == SDL_TRUE &&
                        (config.gamepad_device < 0 || config.gamepad_device == event.jdevice.which)) {
                        log_debug("Gamepad connected with device index %i", event.jdevice.which);
                        connect_gamepad(event.jdevice.which, !state.application_running, true);
                    }
                    break;

                case SDL_JOYDEVICEREMOVED:
                    // `which` is the instance id here: only a pad in the list is removed, whatever Device says
                    log_debug("Gamepad disconnected");
                    disconnect_gamepad(event.jdevice.which, true, true);
                    break;
```

In `reload_background()`, before `set_draw_color();` (line 782), rebuild the overlay:

```c
    stop_overlay();
    start_overlay();
```

In `reload_titles()`, before the loop, bring the title's text settings up to date, since Task 6's Titles page changes them:

```c
    title_info.shadow = config.title_shadows;
    title_info.shadow_color = config.title_shadows ? &title_shadow_color : NULL;
    title_info.oversize_mode = config.title_oversize_mode;
    geo.font_height = config.titles_enabled ? TTF_FontHeight(fixed_title_font) : 0;
```

Add `reload_title_font()` below `reload_titles()`:

```c
// A function to open the title font again after its file or face changed: close the size cache and
// the fixed font, open the font, measure its height per point again, then render every menu's titles
void reload_title_font()
{
    title_fonts_free();
    if (fixed_title_font != NULL)
        TTF_CloseFont(fixed_title_font);
    fixed_title_font = NULL;
    title_info.font = NULL;
    title_info.font_size = (int) config.title_font_size;
    if (load_font(&title_info, config.title_font_path, config.title_font_face, FILENAME_DEFAULT_FONT))
        return;   // log_fatal has quit: not even the bundled font opens
    fixed_title_font = title_info.font;
    TTF_Font *probe = TTF_OpenFontIndex(title_info.font_path, TITLE_MEASURE_SIZE, title_info.font_face);
    geo.title_line_pm = probe != NULL ? TTF_FontHeight(probe) * 1000 / TITLE_MEASURE_SIZE : 1500;
    if (probe != NULL)
        TTF_CloseFont(probe);
    log_debug("Titles: opened %s (face %i)", title_info.font_path, title_info.font_face);
    reload_titles();
}
```

`reload_titles()` sets `geo.font_height` from the new fixed font.

- [ ] **Step 8: Start everything the same way at startup, and stop everything at quit**

In `main()`:
- Remove `if (config.gamepad_enabled) add_default_gamepad_controls();` (lines 1640-1641).
- Remove the mappings file's block (lines 1665-1674).
- Remove the screensaver's `init_screensaver()` call, the clock's block, the highlight's block, the scroll indicators' block and the overlay's block (lines 1679-1729, keeping `calculate_layout_area();`).

In their place, after `reload_background();` (line 1677), write:

```c
    // Start every feature that is on: the settings screen starts and stops them the same way
    start_gamepad();
    apply_os_screensaver();
    start_screensaver();
    start_clock();
    start_highlight();
    start_scroll();

    // Work out where the buttons may go, now that the clock's size is known
    calculate_layout_area();
```

`reload_background()` starts the overlay, and it now runs after `create_window()` and `refresh_effective()`, which the overlay's colour needs.

In `cleanup()`:
- After `settings_close_now();`, stop every feature while the renderer and SDL still run:

```c
    stop_clock();
    stop_screensaver();
    stop_scroll();
    stop_highlight();
    stop_overlay();
    stop_gamepad();
```

  `stop_clock()` also waits for the clock's thread, so the later `SDL_WaitThread(clock_thread, NULL);` (line 363) is removed.
- The later clock font close (Task 3's lines), `free(highlight);`, `free(scroll);`, `free(screensaver);` and `free(clk);` (lines 405-408) are removed: the stops did them.
- The last two lines, `if (config.gamepad_enabled) disconnect_gamepad(-1, false, true);`, are removed: `stop_gamepad()` closed the pads.

Every `stop_*` and `reload_*` except `reload_highlight` is declared `static` at the top of `launcher.c` with the other prototypes. The public ones go in `launcher.h`:

```c
void reload_highlight(void);
void reload_scroll(void);
void reload_clock(void);
void reload_screensaver(void);
void reload_gamepad(void);
void reload_title_font(void);
void apply_frame_timing(void);
void apply_os_screensaver(void);
void apply_default_menu(void);
bool gamepad_running(void);
```

The static prototypes to add at the top of `launcher.c`: `start_screensaver`, `stop_screensaver`, `start_overlay`, `stop_overlay`, `start_highlight`, `stop_highlight`, `start_scroll`, `stop_scroll`, `start_clock`, `stop_clock`, `start_gamepad`, `stop_gamepad`, `connect_present_pads`, and under `#ifdef STREAMFLEX_TEST_HOOKS` `test_pad_update` and `test_pad_stop`. Remove the now-unused `init_screensaver` prototype.

- [ ] **Step 9: Build, and see the harness pass**

Build and run the unit tests (Global Constraints): `100% tests passed`. Run the **Linux unit tests** with the label `t4`: `warnings outside src/external: 0`.

Run the headless harness on Debian and Fedora (labels `t4`, `t4-fedora`) and the leak pass on both (labels `t4-leaks`, `t4-fedora-leaks`), two at a time. Expected:
- `0 failed` everywhere.
- `f42-all`'s stops at quit are in its log, and the leak pass has no `LEAK` line.
- `scrollfail` still passes: run it once, with the label `t4-scrollfail` and the mode `scrollfail`.
- `f50-padheld` and `f50-paddefault` still pass. The virtual pad now attaches through `start_gamepad()`'s running subsystem, and its connect event comes through the fixed `connect_gamepad()`.

Build the Debian package in the harness image to confirm the new SDL floor reaches it:

```powershell
docker run --rm --name sfdev-t4-deb -v C:/Users/jscha/source/repos/streamflex:/src:ro streamflex-test bash -c "cp -r /src /w && cmake -S /w -B /w/b -DPACKAGE=DEB > /dev/null && cmake --build /w/b --target package > /dev/null && dpkg-deb -f /w/b/*.deb Depends"
```

Expected: the `Depends` line holds `libsdl2-2.0-0 (>= 2.0.18)`. If shlibdeps gives a lower floor, stop and report. The comment in Step 3 would be wrong, and the fix is an explicit `libsdl2-2.0-0 (>= 2.0.18)` in place of shlibdeps' SDL entry, which needs the user's call.

- [ ] **Step 10: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add CMakeLists.txt docs/compilation.md src/launcher.h src/launcher.c src/clock.h src/clock.c tests/headless/checks/42-features.sh tests/headless/fixtures/f42-all.ini tests/headless/fixtures/f42-fps10.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: a start, stop and reload for every feature, and live frame timing (SDL 2.0.18)"
```

### Task 5: The pages — nine sections, greyed rows with reasons, and picker rows

The model gains:
- one page per config section, with the rows the spec lists (the Controls page gets its Keyboard row in Task 14);
- greyed rows that say why they are greyed;
- rows marked › that open a picker (`SETTINGS_ROW_PICK`, and `SETTINGS_EVENT_PICK`);
- the *Default menu* and *Device* rows, which step through the launcher's menus and the pads present.

This task is pure model and tests. Task 6 draws and applies it.

**Files:**
- Modify: `src/settings.h` (`SettingsPage`, `SettingsRowKind`, `SettingsRow`, `SettingsEventKind`; new functions)
- Modify: `src/settings.c` (`SettingsState`; `settings_create`/`settings_free`; `setting_row()`; `settings_rows()`; `selectable()`; `settings_command()`; `settings_path()`; new `settings_choose_value()`, `settings_set_pads()`)
- Test: `tests/test_settings.c` (the 3a page tests updated; new page tests)

**Interfaces:**
- Consumes: Task 1's table.
- Produces:

```c
// settings.h
typedef enum {
    SETTINGS_PAGE_TOP, SETTINGS_PAGE_GENERAL, SETTINGS_PAGE_BACKGROUND, SETTINGS_PAGE_MENUS,
    SETTINGS_PAGE_MENU, SETTINGS_PAGE_TITLES, SETTINGS_PAGE_HIGHLIGHT, SETTINGS_PAGE_SCROLL,
    SETTINGS_PAGE_CLOCK, SETTINGS_PAGE_SCREENSAVER, SETTINGS_PAGE_CONTROLS, SETTINGS_PAGE_GAMEPAD,
    SETTINGS_PAGE_SAVE_FAILED
} SettingsPage;
// SettingsRowKind gains SETTINGS_ROW_PICK: OK opens a picker for its slot; Left and Right step it
// when its type steps (colours, the default menu, the device)
// SettingsRow gains: const char *why;   // A greyed row's reason; NULL for none
// SettingsEventKind gains SETTINGS_EVENT_PICK: open the picker for `slot`
SettingsEvent settings_choose_value(SettingsState *state, SettingSlot *slot, const SettingValue *value);
void settings_set_pads(SettingsState *state, const char *const *names, int count);
int settings_pad_count(const SettingsState *state);
const char *settings_pad_name(const SettingsState *state, int index);   // NULL past the pads named
#define SETTINGS_MAX_PADS 16
```

  The top page's rows, in order: General, Background, Menus, Titles, Highlight, Scroll indicators, Clock, Screensaver, Controls, a divider, and Discard changes (index 10).

- [ ] **Step 1: Write the failing tests**

In `tests/test_settings.c`, give `open_model()` the values a typical config gives the new settings. Add this helper above it:

```c
// A function to set a global setting's entry value from the file's text
static void entry(SettingsState *state, SettingId id, const char *text)
{
    SettingValue value = parsed(id, text);
    settings_set_entry(state, id, -1, &value);
}
```

and add these lines before `return state;` in `open_model()`:

```c
    entry(state, SET_ID_DEFAULT_MENU, "Main");
    entry(state, SET_ID_VSYNC, "true");
    entry(state, SET_ID_TITLES_ENABLED, "true");
    entry(state, SET_ID_TITLE_SHADOWS, "false");
    entry(state, SET_ID_HIGHLIGHT_ENABLED, "true");
    entry(state, SET_ID_HIGHLIGHT_OUTLINE_SIZE, "0");
    entry(state, SET_ID_SCROLL_ENABLED, "true");
    entry(state, SET_ID_OVERLAY, "false");
    entry(state, SET_ID_CLOCK_ENABLED, "false");
    entry(state, SET_ID_CLOCK_SHOW_DATE, "false");
    entry(state, SET_ID_SCREENSAVER_ENABLED, "false");
    entry(state, SET_ID_SCREENSAVER_IDLE_TIME, "300");
    entry(state, SET_ID_GAMEPAD_ENABLED, "true");
    entry(state, SET_ID_GAMEPAD_DEVICE, "-1");
```

Replace `test_top_and_menus()` (lines 271-357) with:

```c
// A function to test the top level, the Menus page, one menu's page and Discard
static void test_top_and_menus(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 11);
    static const char *const labels[] = { "General", "Background", "Menus", "Titles", "Highlight",
                                          "Scroll indicators", "Clock", "Screensaver", "Controls" };
    for (int i = 0; i < 9; i++) {
        CHECK_STR(rows[i].label, labels[i]);
        CHECK_INT(rows[i].kind, SETTINGS_ROW_LINK);
    }
    CHECK_STR(rows[0].value, "Main");            // General: the default menu
    CHECK_STR(rows[1].value, "Colour");
    CHECK_STR(rows[2].value, "2 menus");
    CHECK_STR(rows[3].value, "Medium");
    CHECK_STR(rows[4].value, "On");
    CHECK_STR(rows[5].value, "On");
    CHECK_STR(rows[6].value, "Off");
    CHECK_STR(rows[7].value, "Off");
    CHECK_STR(rows[8].value, "Gamepad on");
    CHECK_INT(rows[9].kind, SETTINGS_ROW_DIVIDER);
    CHECK_STR(rows[10].label, "Discard changes");
    CHECK(!rows[10].enabled);
    CHECK(rows[10].why == NULL);                  // Greyed with no reason: the cursor skips it
    CHECK_INT(settings_cursor(state), 0);
    for (int i = 0; i < 8; i++)
        CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_NONE);   // Discard is greyed
    CHECK_INT(settings_cursor(state), 8);
    for (int i = 0; i < 6; i++)
        settings_command(state, SETTINGS_UP);
    CHECK_INT(settings_cursor(state), 2);

    // Menus: All menus, a divider, then each menu with its grid as columns x rows
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENUS);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus");
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 4);
    CHECK_STR(rows[0].label, "All menus");
    CHECK_STR(rows[0].value, "4 " TIMES " 1");
    CHECK_STR(rows[2].label, "Main");
    CHECK_STR(rows[2].value, "All menus");
    CHECK_STR(rows[3].label, "Games");
    CHECK_STR(rows[3].value, "6 " TIMES " 3");
    CHECK_INT(settings_preview_menu(state), -1);
    settings_command(state, SETTINGS_DOWN);                  // Over the divider, onto Main
    CHECK_INT(settings_cursor(state), 2);
    CHECK_INT(settings_preview_menu(state), 0);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_preview_menu(state), 1);

    // Games' page: its own rows and columns, and IconSize following All menus
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENU);
    CHECK_INT(settings_preview_menu(state), 1);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Games");
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 4);                                     // Three rows and the note
    CHECK_STR(rows[0].value, "3");
    CHECK_STR(rows[1].value, "6");
    CHECK_STR(rows[2].value, "All menus (Fill)");

    // Rows 3 -> 2 -> 1 -> All menus (1)
    SettingsEvent event = settings_command(state, SETTINGS_LEFT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK(event.slot == settings_slot(state, SET_ID_MENU_ROWS, 1));
    CHECK_INT(event.before.number, 3);
    settings_command(state, SETTINGS_LEFT);
    settings_command(state, SETTINGS_LEFT);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].value, "All menus (1)");
    CHECK(settings_changed(settings_slot(state, SET_ID_MENU_ROWS, 1)));
    CHECK_INT(settings_command(state, SETTINGS_LEFT).kind, SETTINGS_EVENT_NONE);

    // Back at the top, Discard is offered; it puts every value back and greys out again
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    CHECK_INT(settings_cursor(state), 2);                    // Where it was
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK(rows[10].enabled);
    for (int i = 0; i < 7; i++)
        settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_cursor(state), 10);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_DISCARD);
    CHECK_INT(settings_slot(state, SET_ID_MENU_ROWS, 1)->value.number, 3);
    CHECK(!settings_any_changed(state));
    CHECK_INT(settings_cursor(state), 8);                    // Off the greyed Discard, onto Controls

    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_CLOSE);
    CHECK_INT(settings_command(state, SETTINGS_HOME).kind, SETTINGS_EVENT_CLOSE_HOME);
    CHECK_INT(settings_command(state, SETTINGS_CLOSE).kind, SETTINGS_EVENT_CLOSE);
    settings_free(state);
}
```

In `test_background_page()` (lines 360-422):
- Insert `settings_command(state, SETTINGS_DOWN);` before its first `settings_command(state, SETTINGS_OK);`: Background is now the second row.
- The Background page's counts grow by the overlay's three rows:
  - `CHECK_INT(count, 2);` becomes `CHECK_INT(count, 5);` twice (Colour, then Image);
  - Slideshow's `CHECK_INT(count, 4);` becomes `CHECK_INT(count, 7);`;
  - Transparent's `CHECK_INT(count, 2);` becomes `CHECK_INT(count, 6);`.
- After the Transparent count, add:

```c
    CHECK(rows[2].slot == settings_slot(state, SET_ID_CHROMA_KEY_COLOR, -1));
    CHECK_INT(rows[2].kind, SETTINGS_ROW_PICK);
```

- After the first `CHECK_STR(rows[1].value, "Black");`, add:

```c
    CHECK_INT(rows[1].kind, SETTINGS_ROW_PICK);                // OK opens the colour picker...
    CHECK_STR(rows[2].label, "Overlay");
    CHECK_STR(rows[3].label, "Overlay colour");
    CHECK(!rows[3].enabled);                                    // ...and the overlay's rows wait for it
    CHECK_STR(rows[3].why, "Turn Overlay on to change this");
    CHECK(!rows[4].enabled);
```

In `test_all_menus_and_titles_pages()` (lines 454-507):
- Insert `settings_command(state, SETTINGS_DOWN);` before the existing `settings_command(state, SETTINGS_DOWN);` at line 463: Menus is now the third row.
- The All menus page's `CHECK_INT(count, 3);` becomes `CHECK_INT(count, 5);`, and after the `ICON_SIZE` checks add:

```c
    CHECK(rows[3].slot == settings_slot(state, SET_ID_ICON_SPACING, -1));
    CHECK(rows[4].slot == settings_slot(state, SET_ID_VCENTER, -1));
```

- The walk to Titles: after the two `SETTINGS_BACK`s the cursor is on Menus (2); one `SETTINGS_DOWN` reaches Titles, so `CHECK_INT(settings_cursor(state), 2);` becomes `CHECK_INT(settings_cursor(state), 3);`.
- The Titles page's `CHECK_INT(count, 1);` becomes `CHECK_INT(count, 9);`. `rows[0]` stays Size: the tests after it hold.

In `test_one_menu()` (line 534), `rows[1]` becomes `rows[2]`.

In `test_background_entered_incomplete()` (line 545) and in both halves of `test_more_menus_than_rows()` (lines 575 and 590), add one `settings_command(state, SETTINGS_DOWN);` before the first `settings_command(state, SETTINGS_OK);`: Background and Menus each moved one row down.

Add these tests above `main()`:

```c
// A function to find a row on the page on show by its label
static const SettingsRow *row_labelled(SettingsState *state, SettingsRow *rows, const char *label)
{
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    for (int i = 0; i < count; i++) {
        if (strcmp(rows[i].label, label) == 0)
            return &rows[i];
    }
    return NULL;
}

// A function to move the cursor onto a row by its label: up to the first row it may rest on, then
// down until it is there
static void cursor_to(SettingsState *state, const char *label)
{
    SettingsRow rows[SETTINGS_MAX_ROWS];
    for (int i = 0; i < SETTINGS_MAX_ROWS; i++)
        settings_command(state, SETTINGS_UP);
    for (int i = 0; i < SETTINGS_MAX_ROWS; i++) {
        settings_rows(state, rows, SETTINGS_MAX_ROWS);
        if (strcmp(rows[settings_cursor(state)].label, label) == 0)
            return;
        settings_command(state, SETTINGS_DOWN);
    }
}

// A function to open a top-level page by its label
static void open_page(SettingsState *state, const char *label)
{
    cursor_to(state, label);
    settings_command(state, SETTINGS_OK);
}

// A function to test the General page: its rows, a row greyed with its reason, and the default menu
static void test_general_page(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    open_page(state, "General");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_GENERAL);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "General");
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 11);
    static const char *const labels[] = { "Default menu", "Wrap around", "Reset on Back", "Mouse select",
        "Block the OS screensaver", "VSync", "FPS limit", "After launching an app", "App timeout",
        "Startup command", "Quit command" };
    for (int i = 0; i < 11; i++)
        CHECK_STR(rows[i].label, labels[i]);
    CHECK_INT(rows[0].kind, SETTINGS_ROW_PICK);
    CHECK_INT(rows[9].kind, SETTINGS_ROW_PICK);
    CHECK_STR(rows[9].value, "None");

    // FPS limit is greyed while VSync is on, says why, and can take the cursor, which changes nothing
    const SettingsRow *fps = row_labelled(state, rows, "FPS limit");
    CHECK(fps != NULL && !fps->enabled);
    CHECK_STR(fps->why, "Used only while VSync is off");
    cursor_to(state, "FPS limit");
    CHECK_STR(rows[settings_cursor(state)].label, "FPS limit");
    CHECK_INT(settings_command(state, SETTINGS_RIGHT).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_NONE);
    settings_command(state, SETTINGS_UP);                        // VSync off: FPS limit opens up
    CHECK_INT(settings_command(state, SETTINGS_LEFT).kind, SETTINGS_EVENT_CHANGED);
    fps = row_labelled(state, rows, "FPS limit");
    CHECK(fps->enabled && fps->why == NULL);

    // Default menu steps through the menus in file order, and OK opens its list
    cursor_to(state, "Default menu");
    CHECK_INT(settings_cursor(state), 0);
    SettingsEvent event = settings_command(state, SETTINGS_RIGHT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK_STR(event.slot->value.text, "Games");
    CHECK_STR(event.before.text, "Main");
    CHECK_INT(settings_command(state, SETTINGS_RIGHT).kind, SETTINGS_EVENT_NONE);   // Games is the last
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_PICK);
    CHECK(event.slot == settings_slot(state, SET_ID_DEFAULT_MENU, -1));

    // A command picked in the command picker
    SettingValue quit = parsed(SET_ID_STARTUP_CMD, ":quit");
    SettingSlot *startup = settings_slot(state, SET_ID_STARTUP_CMD, -1);
    CHECK_INT(settings_choose_value(state, startup, &quit).kind, SETTINGS_EVENT_CHANGED);
    CHECK_INT(settings_choose_value(state, startup, &quit).kind, SETTINGS_EVENT_NONE);
    CHECK_STR(row_labelled(state, rows, "Startup command")->value, "Quit StreamFlex");
    settings_free(state);
}

// A function to test the pages whose rows follow a switch: Titles, Highlight, Clock
static void test_greyed_rows(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];

    // Titles: Shadow colour waits for Shadows; titles off greys everything but Show titles
    open_page(state, "Titles");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TITLES);
    static const char *const labels[] = { "Size", "Show titles", "Font", "Colour", "Opacity", "Shadows",
                                          "Shadow colour", "Too long", "Padding" };
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    for (int i = 0; i < count; i++)
        CHECK_STR(rows[i].label, labels[i]);
    CHECK_STR(row_labelled(state, rows, "Shadow colour")->why, "Turn Shadows on to change this");
    CHECK_INT(row_labelled(state, rows, "Font")->kind, SETTINGS_ROW_PICK);
    cursor_to(state, "Show titles");
    settings_command(state, SETTINGS_LEFT);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    for (int i = 0; i < count; i++) {
        if (strcmp(rows[i].label, "Show titles") == 0)
            CHECK(rows[i].enabled);
        else
            CHECK(!rows[i].enabled && rows[i].why != NULL && strcmp(rows[i].why, "Titles are off") == 0);
    }
    settings_command(state, SETTINGS_BACK);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[3].value, "Off");                     // The top page's summary follows

    // Highlight: the outline's colour and opacity wait for a size; corners and an outline exclude each other
    open_page(state, "Highlight");
    CHECK_STR(row_labelled(state, rows, "Outline colour")->why, "The outline's size is 0");
    CHECK(row_labelled(state, rows, "Corner radius")->enabled);
    cursor_to(state, "Outline size");
    settings_command(state, SETTINGS_RIGHT);
    CHECK(row_labelled(state, rows, "Outline colour")->enabled);
    CHECK_STR(row_labelled(state, rows, "Corner radius")->why, "Rounded corners cannot be drawn with an outline");
    settings_command(state, SETTINGS_BACK);

    // Clock: off greys all but Show; with it on, the date's rows wait for Show date
    open_page(state, "Clock");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_CLOCK);
    CHECK_STR(row_labelled(state, rows, "Size")->why, "The clock is off");
    cursor_to(state, "Show");
    settings_command(state, SETTINGS_RIGHT);
    CHECK(row_labelled(state, rows, "Size")->enabled);
    CHECK_STR(row_labelled(state, rows, "Date")->why, "Turn Show date on to change this");
    CHECK_STR(row_labelled(state, rows, "Weekday")->why, "Turn Show date on to change this");
    settings_free(state);
}

// A function to test the Screensaver, Scroll indicators and Controls pages, and the gamepad's Device
static void test_other_pages(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    open_page(state, "Screensaver");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_SCREENSAVER);
    CHECK_STR(row_labelled(state, rows, "Idle time")->why, "The screensaver is off");
    CHECK_STR(row_labelled(state, rows, "Idle time")->value, "5 min");
    settings_command(state, SETTINGS_BACK);

    open_page(state, "Scroll indicators");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_SCROLL);
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 5);
    settings_command(state, SETTINGS_BACK);

    // Controls > Gamepad: On, Device, Mappings file (next start) and its note
    open_page(state, "Controls");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_CONTROLS);
    open_page(state, "Gamepad");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_GAMEPAD);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Controls" ARROW "Gamepad");
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 4);
    CHECK_STR(rows[1].label, "Device");
    CHECK_STR(rows[1].value, "Any");
    CHECK_INT(rows[2].kind, SETTINGS_ROW_BROWSE);
    CHECK_INT(rows[3].kind, SETTINGS_ROW_NOTE);
    CHECK(strstr(rows[3].note, "next start") != NULL);

    // Device steps through the pads present, by name; one the file names that is gone stays reachable
    static const char *const pads[] = { "Xbox Controller", "8BitDo Pro 2" };
    settings_set_pads(state, pads, 2);
    cursor_to(state, "Device");
    SettingsEvent event = settings_command(state, SETTINGS_RIGHT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK_INT(event.slot->value.number, 0);
    CHECK_STR(row_labelled(state, rows, "Device")->value, "Xbox Controller");
    settings_command(state, SETTINGS_RIGHT);
    CHECK_STR(row_labelled(state, rows, "Device")->value, "8BitDo Pro 2");
    CHECK_INT(settings_command(state, SETTINGS_RIGHT).kind, SETTINGS_EVENT_NONE);
    SettingValue gone = parsed(SET_ID_GAMEPAD_DEVICE, "5");
    settings_set_entry(state, SET_ID_GAMEPAD_DEVICE, -1, &gone);
    CHECK_STR(row_labelled(state, rows, "Device")->value, "Pad 5 (not connected)");
    settings_command(state, SETTINGS_LEFT);
    CHECK_INT(settings_slot(state, SET_ID_GAMEPAD_DEVICE, -1)->value.number, 1);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_PICK);

    // The gamepad off greys the rest
    cursor_to(state, "On");
    settings_command(state, SETTINGS_LEFT);
    CHECK_STR(row_labelled(state, rows, "Device")->why, "The gamepad is off");
    settings_free(state);
}
```

Call the three new tests from `main()`, after `test_more_menus_than_rows();`:

```c
    test_general_page();
    test_greyed_rows();
    test_other_pages();
```

- [ ] **Step 2: Run the tests to see them fail**

Build `test_settings` (as in Task 1, Step 3). Expected: the build fails with `'SETTINGS_PAGE_GENERAL': undeclared identifier`, `'SETTINGS_ROW_PICK': undeclared identifier` and `'struct <anonymous>' has no member named 'why'` (or MSVC's wording of them).

- [ ] **Step 3: Give `settings.h` the pages, the picker rows and their functions**

In `src/settings.h`:
- Add `#define SETTINGS_MAX_PADS 16      // Most gamepads the Device row names` after `SETTINGS_MAX_DEPTH`.
- Replace `SettingsPage` with:

```c
typedef enum {
    SETTINGS_PAGE_TOP,
    SETTINGS_PAGE_GENERAL,
    SETTINGS_PAGE_BACKGROUND,
    SETTINGS_PAGE_MENUS,
    SETTINGS_PAGE_MENU,          // One menu's grid, or All menus ([Layout])
    SETTINGS_PAGE_TITLES,
    SETTINGS_PAGE_HIGHLIGHT,
    SETTINGS_PAGE_SCROLL,
    SETTINGS_PAGE_CLOCK,
    SETTINGS_PAGE_SCREENSAVER,
    SETTINGS_PAGE_CONTROLS,
    SETTINGS_PAGE_GAMEPAD,
    SETTINGS_PAGE_SAVE_FAILED
} SettingsPage;
```

- Add `SETTINGS_ROW_PICK,           // OK opens a picker for its slot; Left and Right step it when its type steps` after `SETTINGS_ROW_BROWSE` in `SettingsRowKind`.
- Add `const char *why;             // A greyed row's reason (the cursor may rest on it); NULL for none` after `bool enabled;` in `SettingsRow`.
- Add `SETTINGS_EVENT_PICK,         // Open the picker for `slot`` after `SETTINGS_EVENT_BROWSE` in `SettingsEventKind`.
- Declare, after `settings_choose()`:

```c
SettingsEvent settings_choose_value(SettingsState *state, SettingSlot *slot, const SettingValue *value);
void settings_set_pads(SettingsState *state, const char *const *names, int count);
int settings_pad_count(const SettingsState *state);
const char *settings_pad_name(const SettingsState *state, int index);
```

- [ ] **Step 4: Build the pages in `settings.c`**

Add the pads to `SettingsState`, after `char more_menus[128];`:

```c
    char *pads[SETTINGS_MAX_PADS]; // The gamepads present, by device index, for the Device row
    int pad_count;
```

In `settings_free()`, before `alloc_free(state->names);`, free them:

```c
    for (int i = 0; i < state->pad_count; i++)
        alloc_free(state->pads[i]);
```

Add below `settings_free()`:

```c
// A function to name the gamepads present, by device index, for the Device row; names past
// SETTINGS_MAX_PADS are left out. Out of memory, a pad keeps no name and shows as "Pad N".
void settings_set_pads(SettingsState *state, const char *const *names, int count)
{
    for (int i = 0; i < state->pad_count; i++)
        alloc_free(state->pads[i]);
    state->pad_count = count < SETTINGS_MAX_PADS ? count : SETTINGS_MAX_PADS;
    for (int i = 0; i < state->pad_count; i++)
        state->pads[i] = alloc_strdup(names[i]);
}

// A function to count the pads named
int settings_pad_count(const SettingsState *state)
{
    return state->pad_count;
}

// A function to get a pad's name by device index, for the Device list; NULL past the pads named
const char *settings_pad_name(const SettingsState *state, int index)
{
    if (index < 0 || index >= state->pad_count)
        return NULL;
    return state->pads[index] != NULL ? state->pads[index] : "Pad";
}
```

Add the reasons above `new_row()`:

```c
static const char *const WHY_OVERLAY = "Turn Overlay on to change this";
static const char *const WHY_TITLES = "Titles are off";
static const char *const WHY_SHADOWS = "Turn Shadows on to change this";
static const char *const WHY_HIGHLIGHT = "The highlight is off";
static const char *const WHY_NO_OUTLINE = "The outline's size is 0";
static const char *const WHY_ROUNDED = "Rounded corners cannot be drawn with an outline";
static const char *const WHY_SCROLL = "The scroll indicators are off";
static const char *const WHY_CLOCK = "The clock is off";
static const char *const WHY_DATE = "Turn Show date on to change this";
static const char *const WHY_SCREENSAVER = "The screensaver is off";
static const char *const WHY_GAMEPAD = "The gamepad is off";
static const char *const WHY_VSYNC = "Used only while VSync is off";
static const char *const MAPPINGS_NOTE = "The mappings file applies at next start: SDL can add mappings, but never take one back.";
```

Replace `setting_row()` (lines 601-608) with:

```c
// A function to tell how a setting's row acts: stepped, a picker, or the folder browser
static SettingsRowKind row_kind(const SettingDef *def)
{
    switch (def->type) {
        case SET_TYPE_PATH:
            return SETTINGS_ROW_BROWSE;
        case SET_TYPE_COLOR:
        case SET_TYPE_FONT:
        case SET_TYPE_COMMAND:
        case SET_TYPE_MENU:
        case SET_TYPE_DEVICE:
            return SETTINGS_ROW_PICK;
        default:
            return SETTINGS_ROW_SETTING;
    }
}

// A function to make a setting's row. A device shows the pad's name when it is present.
static SettingsRow setting_row(SettingsState *state, SettingSlot *slot)
{
    SettingsRow row = new_row(row_kind(slot->def), slot->def->label);
    row.slot = slot;
    setting_describe(slot->def, &slot->value, inherited_value(state, slot), row.value, sizeof(row.value));
    int pad = slot->value.number;
    if (slot->def->type == SET_TYPE_DEVICE && pad >= 0) {
        if (pad < state->pad_count && state->pads[pad] != NULL)
            snprintf(row.value, sizeof(row.value), "%s", state->pads[pad]);
        else if (pad >= state->pad_count)
            snprintf(row.value, sizeof(row.value), "Pad %d (not connected)", pad);
    }
    return row;
}

// A function to make a global setting's row
static SettingsRow global_row(SettingsState *state, SettingId id)
{
    return setting_row(state, settings_slot(state, id, -1));
}

// A function to grey a row out with its reason when `grey` holds
static SettingsRow greyed(SettingsRow row, bool grey, const char *why)
{
    if (grey) {
        row.enabled = false;
        row.why = why;
    }
    return row;
}

// A function to tell whether an on/off setting is on
static bool is_on(SettingsState *state, SettingId id)
{
    return settings_slot(state, id, -1)->value.number != 0;
}

// A function to summarise an on/off setting for the top page
static const char *on_off(SettingsState *state, SettingId id)
{
    return is_on(state, id) ? "On" : "Off";
}
```

Replace the `SETTINGS_PAGE_TOP` case of `settings_rows()` (lines 673-685) with:

```c
        case SETTINGS_PAGE_TOP: {
            n = add_row(rows, n, max, link_row("General", settings_slot(state, SET_ID_DEFAULT_MENU, -1)->value.text,
                                               SETTINGS_PAGE_GENERAL, -1));
            SettingSlot *mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1);
            setting_describe(mode->def, &mode->value, NULL, text, sizeof(text));
            n = add_row(rows, n, max, link_row("Background", text, SETTINGS_PAGE_BACKGROUND, -1));
            snprintf(text, sizeof(text), state->menu_count == 1 ? "%d menu" : "%d menus", state->menu_count);
            n = add_row(rows, n, max, link_row("Menus", text, SETTINGS_PAGE_MENUS, -1));
            SettingSlot *size = settings_slot(state, SET_ID_TITLE_SIZE, -1);
            setting_describe(size->def, &size->value, NULL, text, sizeof(text));
            n = add_row(rows, n, max, link_row("Titles", is_on(state, SET_ID_TITLES_ENABLED) ? text : "Off",
                                               SETTINGS_PAGE_TITLES, -1));
            n = add_row(rows, n, max, link_row("Highlight", on_off(state, SET_ID_HIGHLIGHT_ENABLED), SETTINGS_PAGE_HIGHLIGHT, -1));
            n = add_row(rows, n, max, link_row("Scroll indicators", on_off(state, SET_ID_SCROLL_ENABLED), SETTINGS_PAGE_SCROLL, -1));
            n = add_row(rows, n, max, link_row("Clock", on_off(state, SET_ID_CLOCK_ENABLED), SETTINGS_PAGE_CLOCK, -1));
            SettingSlot *idle = settings_slot(state, SET_ID_SCREENSAVER_IDLE_TIME, -1);
            char after[64] = "Off";
            if (is_on(state, SET_ID_SCREENSAVER_ENABLED)) {
                setting_describe(idle->def, &idle->value, NULL, text, sizeof(text));
                snprintf(after, sizeof(after), "After %s", text);
            }
            n = add_row(rows, n, max, link_row("Screensaver", after, SETTINGS_PAGE_SCREENSAVER, -1));
            n = add_row(rows, n, max, link_row("Controls", is_on(state, SET_ID_GAMEPAD_ENABLED) ? "Gamepad on" : "Gamepad off",
                                               SETTINGS_PAGE_CONTROLS, -1));
            n = add_row(rows, n, max, new_row(SETTINGS_ROW_DIVIDER, ""));
            n = add_row(rows, n, max, action_row("Discard changes", SETTINGS_ACTION_DISCARD, settings_any_changed(state)));
            break;
        }
        case SETTINGS_PAGE_GENERAL: {
            static const SettingId ids[] = { SET_ID_DEFAULT_MENU, SET_ID_WRAP_ENTRIES, SET_ID_RESET_ON_BACK,
                SET_ID_MOUSE_SELECT, SET_ID_INHIBIT_OS_SCREENSAVER, SET_ID_VSYNC, SET_ID_FPS_LIMIT, SET_ID_ON_LAUNCH,
                SET_ID_APPLICATION_TIMEOUT, SET_ID_STARTUP_CMD, SET_ID_QUIT_CMD };
            for (int i = 0; i < LENGTH(ids); i++) {
                SettingsRow row = global_row(state, ids[i]);
                if (ids[i] == SET_ID_FPS_LIMIT)
                    row = greyed(row, is_on(state, SET_ID_VSYNC), WHY_VSYNC);
                n = add_row(rows, n, max, row);
            }
            break;
        }
```

In the `SETTINGS_PAGE_BACKGROUND` case (lines 686-701), replace the final `else` branch and add the overlay's rows after the mode's:

```c
            else {
                n = add_row(rows, n, max, note_row(TRANSPARENT_NOTE));
                n = add_row(rows, n, max, global_row(state, SET_ID_CHROMA_KEY_COLOR));
            }
            bool overlay = is_on(state, SET_ID_OVERLAY);
            n = add_row(rows, n, max, global_row(state, SET_ID_OVERLAY));
            n = add_row(rows, n, max, greyed(global_row(state, SET_ID_OVERLAY_COLOR), !overlay, WHY_OVERLAY));
            n = add_row(rows, n, max, greyed(global_row(state, SET_ID_OVERLAY_OPACITY), !overlay, WHY_OVERLAY));
            break;
```

In the `SETTINGS_PAGE_MENU` case (lines 721-729), All menus gains its two rows:

```c
            if (m >= 0)
                n = add_row(rows, n, max, note_row(MENU_NOTE));
            else {
                n = add_row(rows, n, max, global_row(state, SET_ID_ICON_SPACING));
                n = add_row(rows, n, max, global_row(state, SET_ID_VCENTER));
            }
            break;
```

Replace the `SETTINGS_PAGE_TITLES` case (lines 730-732) with the Titles page and the other new pages:

```c
        case SETTINGS_PAGE_TITLES: {
            bool off = !is_on(state, SET_ID_TITLES_ENABLED);
            static const SettingId ids[] = { SET_ID_TITLE_SIZE, SET_ID_TITLES_ENABLED, SET_ID_TITLE_FONT,
                SET_ID_TITLE_COLOR, SET_ID_TITLE_OPACITY, SET_ID_TITLE_SHADOWS, SET_ID_TITLE_SHADOW_COLOR,
                SET_ID_TITLE_OVERSIZE, SET_ID_TITLE_PADDING };
            for (int i = 0; i < LENGTH(ids); i++) {
                SettingsRow row = global_row(state, ids[i]);
                if (ids[i] == SET_ID_TITLE_SHADOW_COLOR)
                    row = greyed(row, !is_on(state, SET_ID_TITLE_SHADOWS), WHY_SHADOWS);
                if (ids[i] != SET_ID_TITLES_ENABLED)
                    row = greyed(row, off, WHY_TITLES);
                n = add_row(rows, n, max, row);
            }
            break;
        }
        case SETTINGS_PAGE_HIGHLIGHT: {
            bool off = !is_on(state, SET_ID_HIGHLIGHT_ENABLED);
            bool outline = settings_slot(state, SET_ID_HIGHLIGHT_OUTLINE_SIZE, -1)->value.number > 0;
            static const SettingId ids[] = { SET_ID_HIGHLIGHT_ENABLED, SET_ID_HIGHLIGHT_FILL_COLOR,
                SET_ID_HIGHLIGHT_FILL_OPACITY, SET_ID_HIGHLIGHT_OUTLINE_SIZE, SET_ID_HIGHLIGHT_OUTLINE_COLOR,
                SET_ID_HIGHLIGHT_OUTLINE_OPACITY, SET_ID_HIGHLIGHT_CORNER_RADIUS, SET_ID_HIGHLIGHT_VPADDING,
                SET_ID_HIGHLIGHT_HPADDING };
            for (int i = 0; i < LENGTH(ids); i++) {
                SettingsRow row = global_row(state, ids[i]);
                if (ids[i] == SET_ID_HIGHLIGHT_OUTLINE_COLOR || ids[i] == SET_ID_HIGHLIGHT_OUTLINE_OPACITY)
                    row = greyed(row, !outline, WHY_NO_OUTLINE);
                if (ids[i] == SET_ID_HIGHLIGHT_CORNER_RADIUS)
                    row = greyed(row, outline, WHY_ROUNDED);
                if (ids[i] != SET_ID_HIGHLIGHT_ENABLED)
                    row = greyed(row, off, WHY_HIGHLIGHT);
                n = add_row(rows, n, max, row);
            }
            break;
        }
        case SETTINGS_PAGE_SCROLL: {
            bool off = !is_on(state, SET_ID_SCROLL_ENABLED);
            bool outline = settings_slot(state, SET_ID_SCROLL_OUTLINE_SIZE, -1)->value.number > 0;
            static const SettingId ids[] = { SET_ID_SCROLL_ENABLED, SET_ID_SCROLL_FILL_COLOR,
                SET_ID_SCROLL_OUTLINE_SIZE, SET_ID_SCROLL_OUTLINE_COLOR, SET_ID_SCROLL_OPACITY };
            for (int i = 0; i < LENGTH(ids); i++) {
                SettingsRow row = global_row(state, ids[i]);
                if (ids[i] == SET_ID_SCROLL_OUTLINE_COLOR)
                    row = greyed(row, !outline, WHY_NO_OUTLINE);
                if (ids[i] != SET_ID_SCROLL_ENABLED)
                    row = greyed(row, off, WHY_SCROLL);
                n = add_row(rows, n, max, row);
            }
            break;
        }
        case SETTINGS_PAGE_CLOCK: {
            bool off = !is_on(state, SET_ID_CLOCK_ENABLED);
            bool date = is_on(state, SET_ID_CLOCK_SHOW_DATE);
            static const SettingId ids[] = { SET_ID_CLOCK_ENABLED, SET_ID_CLOCK_SHOW_DATE, SET_ID_CLOCK_WEEKDAY,
                SET_ID_CLOCK_ALIGNMENT, SET_ID_CLOCK_FONT, SET_ID_CLOCK_FONT_SIZE, SET_ID_CLOCK_COLOR,
                SET_ID_CLOCK_OPACITY, SET_ID_CLOCK_SHADOWS, SET_ID_CLOCK_SHADOW_COLOR, SET_ID_CLOCK_MARGIN,
                SET_ID_CLOCK_TIME_FORMAT, SET_ID_CLOCK_DATE_FORMAT };
            for (int i = 0; i < LENGTH(ids); i++) {
                SettingsRow row = global_row(state, ids[i]);
                if (ids[i] == SET_ID_CLOCK_WEEKDAY || ids[i] == SET_ID_CLOCK_DATE_FORMAT)
                    row = greyed(row, !date, WHY_DATE);
                if (ids[i] == SET_ID_CLOCK_SHADOW_COLOR)
                    row = greyed(row, !is_on(state, SET_ID_CLOCK_SHADOWS), WHY_SHADOWS);
                if (ids[i] != SET_ID_CLOCK_ENABLED)
                    row = greyed(row, off, WHY_CLOCK);
                n = add_row(rows, n, max, row);
            }
            break;
        }
        case SETTINGS_PAGE_SCREENSAVER: {
            bool off = !is_on(state, SET_ID_SCREENSAVER_ENABLED);
            static const SettingId ids[] = { SET_ID_SCREENSAVER_ENABLED, SET_ID_SCREENSAVER_IDLE_TIME,
                SET_ID_SCREENSAVER_INTENSITY, SET_ID_SCREENSAVER_PAUSE };
            for (int i = 0; i < LENGTH(ids); i++)
                n = add_row(rows, n, max, greyed(global_row(state, ids[i]), off && i > 0, WHY_SCREENSAVER));
            break;
        }
        case SETTINGS_PAGE_CONTROLS:
            n = add_row(rows, n, max, link_row("Gamepad", on_off(state, SET_ID_GAMEPAD_ENABLED), SETTINGS_PAGE_GAMEPAD, -1));
            break;
        case SETTINGS_PAGE_GAMEPAD: {
            bool off = !is_on(state, SET_ID_GAMEPAD_ENABLED);
            n = add_row(rows, n, max, global_row(state, SET_ID_GAMEPAD_ENABLED));
            n = add_row(rows, n, max, greyed(global_row(state, SET_ID_GAMEPAD_DEVICE), off, WHY_GAMEPAD));
            n = add_row(rows, n, max, greyed(global_row(state, SET_ID_GAMEPAD_MAPPINGS), off, WHY_GAMEPAD));
            n = add_row(rows, n, max, note_row(MAPPINGS_NOTE));
            break;
        }
```

Replace `selectable()` (lines 742-746) with:

```c
// A function to tell whether the cursor may rest on a row: any row but a divider or a note, unless
// it is greyed with no reason to give (Discard with nothing to discard)
static bool selectable(const SettingsRow *row)
{
    return row->kind != SETTINGS_ROW_DIVIDER && row->kind != SETTINGS_ROW_NOTE && (row->enabled || row->why != NULL);
}
```

Add the dynamic steps above `settings_command()`:

```c
// A function to step the default menu through the menus, in file order; false at either end
static bool step_menu(SettingsState *state, const SettingSlot *slot, int direction, SettingValue *next)
{
    int index = -1;
    for (int i = 0; i < state->menu_count && index < 0; i++) {
        if (strcmp(state->names[i], slot->value.text) == 0)
            index = i;
    }
    int to = index < 0 ? (direction > 0 ? 0 : -1) : index + (direction > 0 ? 1 : -1);
    if (to < 0 || to >= state->menu_count)
        return false;
    *next = slot->value;
    snprintf(next->text, sizeof(next->text), "%s", state->names[to]);
    return true;
}

// A function to step the device through Any and the pads present, in order; a device index the file
// names that is not present stays reachable in its place. False at either end.
static bool step_device(SettingsState *state, const SettingSlot *slot, int direction, SettingValue *next)
{
    int list[SETTINGS_MAX_PADS + 3];
    int count = 0;
    int wanted[SETTINGS_MAX_PADS + 3];
    int wanted_count = 0;
    wanted[wanted_count++] = -1;
    for (int i = 0; i < state->pad_count; i++)
        wanted[wanted_count++] = i;
    wanted[wanted_count++] = slot->entry.number;
    wanted[wanted_count++] = slot->value.number;
    for (int i = 0; i < wanted_count; i++) {
        int at = count;
        bool seen = false;
        for (int k = 0; k < count && !seen; k++)
            seen = list[k] == wanted[i];
        if (seen)
            continue;
        while (at > 0 && list[at - 1] > wanted[i]) {
            list[at] = list[at - 1];
            at--;
        }
        list[at] = wanted[i];
        count++;
    }
    int index = 0;
    while (index < count && list[index] != slot->value.number)
        index++;
    int to = index + (direction > 0 ? 1 : -1);
    if (to < 0 || to >= count)
        return false;
    *next = slot->value;
    next->number = list[to];
    return true;
}
```

In `settings_command()`, replace the `SETTINGS_LEFT`/`SETTINGS_RIGHT` case (lines 839-851) with:

```c
        case SETTINGS_LEFT:
        case SETTINGS_RIGHT:
            if (row != NULL && row->enabled && (row->kind == SETTINGS_ROW_SETTING || row->kind == SETTINGS_ROW_PICK)) {
                SettingSlot *slot = row->slot;
                int direction = command == SETTINGS_RIGHT ? 1 : -1;
                SettingValue next = slot->value;
                bool moved = true;
                if (slot->def->type == SET_TYPE_MENU)
                    moved = step_menu(state, slot, direction, &next);
                else if (slot->def->type == SET_TYPE_DEVICE)
                    moved = step_device(state, slot, direction, &next);
                else
                    next = setting_step(slot->def, &slot->value, &slot->entry, direction);
                if (moved && !setting_equal(slot->def, &next, &slot->value)) {
                    event.before = slot->value;
                    slot->value = next;
                    event.kind = SETTINGS_EVENT_CHANGED;
                    event.slot = slot;
                }
            }
            break;
```

In its `SETTINGS_OK` case, after `if (row == NULL || !selectable(row)) break;`, add `if (!row->enabled && row->kind != SETTINGS_ROW_ACTION) break;`: a greyed row that took the cursor does nothing. Then add the picker's branch after the `BROWSE` branch:

```c
            else if (row->kind == SETTINGS_ROW_PICK) {
                event.kind = SETTINGS_EVENT_PICK;
                event.slot = row->slot;
            }
```

(The existing `ACTION` branches test `row->action`, and a greyed Discard never takes the cursor, so nothing else changes.)

Add below `settings_choose()`:

```c
// A function to set a value chosen in a picker (a colour, a font, a command, a menu, a device)
SettingsEvent settings_choose_value(SettingsState *state, SettingSlot *slot, const SettingValue *value)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    state->notice[0] = '\0';
    if (setting_equal(slot->def, &slot->value, value))
        return event;
    event.before = slot->value;
    slot->value = *value;
    event.kind = SETTINGS_EVENT_CHANGED;
    event.slot = slot;
    return event;
}
```

In `settings_path()`, name the new pages in its `switch`:

```c
            case SETTINGS_PAGE_GENERAL: name = "General"; break;
            case SETTINGS_PAGE_HIGHLIGHT: name = "Highlight"; break;
            case SETTINGS_PAGE_SCROLL: name = "Scroll indicators"; break;
            case SETTINGS_PAGE_CLOCK: name = "Clock"; break;
            case SETTINGS_PAGE_SCREENSAVER: name = "Screensaver"; break;
            case SETTINGS_PAGE_CONTROLS: name = "Controls"; break;
            case SETTINGS_PAGE_GAMEPAD: name = "Gamepad"; break;
```

`push_page()` already records `entry_mode` for every page, and only the Background page reads it.

- [ ] **Step 5: Run the tests to see them pass**

`settings_set_pads()` allocates, so `test_alloc` proves it. In `tests/test_alloc.c`, add above `main()`:

```c
// A function to prove that naming the pads fails cleanly: a pad whose name could not be copied has
// none, and nothing is left allocated once the model is freed
static void prove_pads(void)
{
    static const char *const names[] = { "Main" };
    static const char *const pads[] = { "Xbox Controller", "8BitDo Pro 2" };
    for (int n = 1;; n++) {
        SettingsState *state = settings_create(names, 1);
        arm(n);
        settings_set_pads(state, pads, 2);
        disarm();
        settings_free(state);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("naming the pads");
}
```

and call `prove_pads();` after `prove_settings();` in `main()`.

Build and run the unit tests (Global Constraints). Expected: `100% tests passed`, with `settings` and `alloc` 0 failed. Run the **Linux unit tests** with the label `t5`: `warnings outside src/external: 0`.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/settings.h src/settings.c tests/test_settings.c tests/test_alloc.c
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: the settings pages for every section, with greyed rows that say why"
```

### Task 6: The screen applies every setting live, through its refresh group

The settings screen draws Task 5's pages and applies each change through the refresh group its `SettingDef` names. Beyond the groups:
- the OS screensaver block and the default menu take effect at once;
- *Discard* re-runs exactly the groups whose settings it puts back;
- a greyed row's reason shows in the caption;
- the preview shows the dim level while the Screensaver page is open;
- the Device row names the pads present.

The existing harness sequences gain the General row's `Down`. New checks walk every page, switch every feature off and on under ASan, stop the clock mid-render, and check VSync, the default menu and the dim preview.

OK on a picker row opens the pickers in Tasks 9, 10 and 15. Until then `SETTINGS_EVENT_PICK` does nothing, and rows that step (colours, the default menu, the device) still step with Left and Right.

**Files:**
- Modify: `src/settings_screen.c` (`apply_slot()`, `apply_all()`, `handle_event()`, `handle_command()`, `measure_layout()`, `draw_row()`, `draw_caption()`, `settings_draw()`, `settings_open()`)
- Modify: `tests/headless/checks/50-settings.sh`, `60-settings-background.sh`, `65-settings-reasons.sh` (the General row's `Down`)
- Create: `tests/headless/checks/55-settings-pages.sh`
- Create: `tests/headless/fixtures/f55-tour.ini`, `f55-frame.ini`, `f55-default.ini`, `f55-dim.ini`, `f55-clock.ini`, `f55-odd.ini`

**Interfaces:**
- Consumes:
  - Task 3's `config_store()`, `config_read()`, `refresh_effective()`;
  - Task 4's `reload_*()`, `apply_frame_timing()`, `apply_os_screensaver()`, `apply_default_menu()`, `gamepad_running()`;
  - Task 5's pages, `SETTINGS_ROW_PICK`, `SettingsRow.why`, `settings_set_pads()`.
- Produces:
  - The debug lines the harness reads: `Settings: page <path>` whenever the page changes, and `Settings: refreshed <group>` for each group run.
  - `static void run_refresh(SettingRefresh refresh)` in `settings_screen.c`, which Task 9's pickers call through `apply_slot()`.

- [ ] **Step 1: Give the existing sequences the General row's `Down`**

The top page starts with *General* now, so every key sequence that opens settings with `Menu` and then walks the top page needs one more `Down` after that `Menu`.

In `tests/headless/checks/50-settings.sh`:
- line 6: `ALL_MENUS_COLUMNS_UP="Menu Down Down Return Return Down Right BackSpace BackSpace BackSpace"`;
- line 33: `CFG=$cfg run_keys f50-inherit Menu Down Down Return Down Down Return Left Left Left BackSpace BackSpace BackSpace`;
- line 41: `CFG=$cfg run_keys f50-discard Menu Down Down Return Return Down Right BackSpace BackSpace Down Down Down Down Down Down Down Return BackSpace`. Back at the top the cursor is on Menus (row 2), and Discard is row 10: seven `Down`s, the divider skipped;
- line 206: `CFG=$cfg run_keys f50-empty Menu Down Down Return Down Down Return Right BackSpace BackSpace BackSpace`;
- line 242: `CFG=$FX/f50-menus.ini run_keys f50-menus Menu Down Down Return +fast_downs +revisit Menu`;
- line 254: `CFG=$cfg run_keys f50-titlefonts Menu Down Down Down Return Right BackSpace BackSpace` (Titles' first row is still Size);
- line 266: `CFG=$FX/f50-home.ini run_keys f50-home Return Menu Down Down Return Down Home`;
- line 306: `CFG=$FX/f50-manymenus.ini run_keys f50-manymenus Menu Down Down Return +all_downs Menu`.

In `tests/headless/checks/60-settings-background.sh`, every sequence opens Background, the old first row:
- line 21 becomes `Menu +shows_black Down Return Down Right +shows_charcoal BackSpace BackSpace`;
- lines 31, 40, 71, 84, 97, 106, 116, 125, 136, 180, 219, 232 and 243: the `Menu Return` in each becomes `Menu Down Return`.

In `tests/headless/checks/65-settings-reasons.sh`, lines 12, 35, 45, 53, 64 and 73: the same, `Menu Return` becomes `Menu Down Return`.

Background's first rows are unchanged (Mode, then the mode's rows), so the `Down`s after `Return` still reach Colour, Image or Folder.

- [ ] **Step 2: Write the failing harness checks**

Create `tests/headless/fixtures/f55-tour.ini`, with every feature on so every page's rows can step:

```ini
[General]
DefaultMenu=Main

[Background]
Overlay=true

[Clock]
Enabled=true
ShowDate=true

[Screensaver]
Enabled=true
IdleTime=900

[Main]
Entry1=Games;games;:submenu Games
Entry2=Two;apps;:quit

[Games]
Entry1=One;apps;:quit
```

Create `tests/headless/fixtures/f55-frame.ini` and `tests/headless/fixtures/f55-clock.ini`:

```ini
[General]
DefaultMenu=Main

[Main]
Entry1=One;apps;:quit
```

```ini
[General]
DefaultMenu=Main

[Clock]
Enabled=true

[Main]
Entry1=One;apps;:quit
```

Create `tests/headless/fixtures/f55-default.ini` (Home is a hotkey for `:home`, as in `f50-home`):

```ini
[General]
DefaultMenu=Main

[Hotkeys]
Hotkey1=#4000004A;:home

[Main]
Entry1=Games;games;:submenu Games

[Games]
Entry1=One;apps;:quit
```

Create `tests/headless/fixtures/f55-dim.ini`:

```ini
[General]
DefaultMenu=Main

[Background]
Color=#FFFFFF

[Screensaver]
Enabled=true
IdleTime=900
Intensity=50%

[Main]
Entry1=One;apps;:quit
```

Create `tests/headless/fixtures/f55-odd.ini`, which holds Review Focus 1's values:

```ini
; Values the table reads in forms it never read before, or past the new steps
[General]
DefaultMenu=Main
WrapEntries=false
FPSLimit=10
VSync=false

[Layout]
IconSpacing=40
VCenter=12.5%

[Background]
Overlay=true
OverlayOpacity=12.5%

[Highlight]
HPadding=300
FillOpacity=33.33%

[Main]
Entry1=One;apps;:quit
```

Create `tests/headless/checks/55-settings-pages.sh`:

```bash
# The pages of every section (3b): every page opened and every row stepped, every feature switched
# off and on (the leak pass proves each stop frees what its start made), the clock stopped in the
# middle of a render, VSync and the FPS limit live, the default menu, the dim preview, and values in
# new forms saved untouched

ARROW=$(printf ' \xE2\x80\xBA ')

# A function to make the keys that walk one page: each row stepped right, left, left and right (an
# on/off that is on ends on; one that is off ends on too, which opens the rows it greys), then down;
# 14 times, more rows than any page has
walk() { local i; for i in $(seq 14); do printf 'Right Left Left Right Down '; done; }
tour() {
    local keys="Return $(walk) BackSpace Down"          # General
    keys="$keys Return $(walk) BackSpace Down"           # Background
    keys="$keys Return Return $(walk) BackSpace BackSpace Down"   # Menus, All menus
    local page
    for page in Titles Highlight Scroll Clock Screensaver; do
        keys="$keys Return $(walk) BackSpace Down"
    done
    keys="$keys Return Return $(walk) BackSpace BackSpace Down"   # Controls, Gamepad
    keys="$keys Return BackSpace"                        # Discard, then close with nothing to save
    # shellcheck disable=SC2086
    xdotool key --delay 100 $keys
    sleep 3
}
cfg=$(writable_config f55-tour)
CFG=$cfg UNTIL='Settings closed' run_keys f55-tour Menu +tour
log=$out/f55-tour.log
ok=1
for page in General Background Menus "Menus${ARROW}All menus" Titles Highlight "Scroll indicators" Clock \
            Screensaver Controls "Controls${ARROW}Gamepad"; do
    grep -qF "Settings: page Settings${ARROW}${page}" "$log" || { echo "      page never opened: $page"; ok=2; }
done
[ "$ok" = 1 ] && [ "$(grep -c 'Settings: \[' "$log")" -ge 40 ] && grep -q 'Settings: discarded the changes' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f55-tour.ini" "$cfg" && ran_clean f55-tour && ok=0
result "settings: every page opens and every row steps, and Discard leaves the file untouched (exit $(cat "$out/f55-tour.code"))" $ok
echo "      $(grep -c 'Settings: \[' "$log") changes"

# The same run switched every feature off and on: each stopped and started again at least once
ok=0
for feature in Overlay Highlight 'Scroll indicators' Clock Screensaver Gamepad; do
    stops=$(sed -n '/Settings opened/,/Settings closed/p' "$log" | grep -c "^$feature stopped")
    starts=$(sed -n '/Settings opened/,/Settings closed/p' "$log" | grep -c "^$feature started")
    [ "$stops" -ge 1 ] && [ "$starts" -ge 1 ] || { echo "      $feature: $stops stops, $starts starts"; ok=1; }
done
result "settings: every feature stops and starts again live" $ok

# A greyed row's reason shows under the preview: the FPS limit while VSync is on
ok=1
grep -q 'Settings: the note under the preview says Used only while VSync is off' "$log" && ok=0
result "settings: a greyed row says why under the preview" $ok

# The clock switched off while its render thread is at work (the harness build's
# STREAMFLEX_TEST_CLOCK_DELAY_MS makes each render take 3 s, and one every second): stopping waits
# for the thread and frees what it made, and Enabled=false is saved
render_then_off() {
    local before i
    before=$(grep -c 'Clock: rendering on its thread' "$LOG")
    for i in $(seq 50); do
        [ "$(grep -c 'Clock: rendering on its thread' "$LOG")" -gt "$before" ] && break
        sleep 0.2
    done
    xdotool key Left
    sleep 4
}
cfg=$(writable_config f55-clock)
STREAMFLEX_TEST_CLOCK_DELAY_MS=3000 CFG=$cfg UNTIL='Settings saved' \
    run_keys f55-clock Menu Down Down Down Down Down Down Return +render_then_off BackSpace BackSpace
ok=1
grep -q 'Clock stopped (it waited for a render in progress)' "$out/f55-clock.log" \
    && sed -n '/^\[Clock\]/,/^\[/p' "$cfg" | grep -qx 'Enabled=false' && ran_clean f55-clock && ok=0
result "settings: the clock switched off mid-render waits for it and frees it (exit $(cat "$out/f55-clock.code"))" $ok

# VSync off, then an FPS limit of 30: the frame timing follows live, and both are saved
cfg=$(writable_config f55-frame)
CFG=$cfg run_keys f55-frame Menu Return Down Down Down Down Down Left Down Right BackSpace BackSpace
ok=1
grep -q 'Frame timing: FPS limit 30, 33 ms a frame' "$out/f55-frame.log" \
    && grep -qx 'VSync=false' "$cfg" && grep -qx 'FPSLimit=30' "$cfg" && ran_clean f55-frame && ok=0
result "settings: VSync and the FPS limit apply live and save (exit $(cat "$out/f55-frame.code"))" $ok
grep -E 'Frame timing|VSync' "$out/f55-frame.log" | sed 's/^/      /'

# The default menu changed to Games: :home (a Home hotkey) closes settings to Games, and it saves
cfg=$(writable_config f55-default)
CFG=$cfg run_keys f55-default Menu Return Right Home
ok=1
sed -n '/Key Home (#4000004A) detected/,$p' "$out/f55-default.log" | grep -q "Loading menu 'Games'" \
    && grep -qx 'DefaultMenu=Games' "$cfg" && ran_clean f55-default && ok=0
result "settings: a new default menu is where :home goes, at once (exit $(cat "$out/f55-default.code"))" $ok

# While the Screensaver page is open, the preview shows the dim level: white under a 50% black
shows_dim() { look "$1" "$2" dim "Settings: page Settings${ARROW}Screensaver" 30,30=128,128,128; }
CFG=$FX/f55-dim.ini run_keys f55-dim Menu Down Down Down Down Down Down Down Return +shows_dim Menu
ok=1
grep -qx 'dim yes' "$out/f55-dim.seen" && ran_clean f55-dim && ok=0
result "settings: the Screensaver page's preview shows the dim level (exit $(cat "$out/f55-dim.code"))" $ok

# Values in forms the table never read before, or past its steps, survive a save of something else
# byte for byte: Wrap around is changed, and that is the only line that changes. Icon spacing is
# stepped away from its 40 px and back, which changes nothing.
cfg=$(writable_config f55-odd)
CFG=$cfg run_keys f55-odd Menu Return Down Right BackSpace Down Down Return Return Down Down Down Right Left BackSpace BackSpace BackSpace
ok=1
[ "$(changed_lines "$FX/f55-odd.ini" "$cfg")" = 2 ] && grep -qx 'WrapEntries=true' "$cfg" \
    && grep -q 'Settings saved 1 change(s)' "$out/f55-odd.log" && ran_clean f55-odd && ok=0
result "settings: values in new forms are saved untouched when something else changes (exit $(cat "$out/f55-odd.code"))" $ok
diff "$FX/f55-odd.ini" "$cfg" | sed 's/^/      /'
```

- [ ] **Step 3: Run the harness to see the new checks fail**

Run the headless harness with the label `t6-red`. Expected:
- Every existing settings check, now with its extra `Down`, fails on this commit. Task 5 already added General, so the old sequences were broken there, and the new ones need this task's screen code. Most of the new checks fail too: there are no `Settings: page` lines, the features do not stop and start, and there is no dim.
- Once Step 4 is written they all pass, which Step 5 shows.

- [ ] **Step 4: Apply every setting through its group, and draw the new rows**

In `src/settings_screen.c`, replace `apply_slot()` and `apply_all()` (lines 307-387 after Task 3's edits) with:

```c
// A function to name a refresh group for the log
static const char *refresh_name(SettingRefresh refresh)
{
    static const char *const names[] = { "nothing", "the layout", "the titles", "the background",
        "the title font", "the highlight", "the scroll indicators", "the clock", "the screensaver",
        "the gamepad", "the frame timing" };
    return names[refresh];
}

// A function to name the gamepads present by device index, for the Device row: every joystick SDL
// lists, by the name its controller mapping gives, while the gamepad runs
static void list_pads(void)
{
    const char *names[SETTINGS_MAX_PADS];
    char fallback[SETTINGS_MAX_PADS][40];
    int count = gamepad_running() ? SDL_NumJoysticks() : 0;
    if (count > SETTINGS_MAX_PADS)
        count = SETTINGS_MAX_PADS;
    for (int i = 0; i < count; i++) {
        names[i] = SDL_IsGameController(i) ? SDL_GameControllerNameForIndex(i) : NULL;
        if (names[i] == NULL) {
            snprintf(fallback[i], sizeof(fallback[i]), "Pad %d", i);
            names[i] = fallback[i];
        }
    }
    settings_set_pads(model, names, count < 0 ? 0 : count);
}

// A function to refresh what a group of settings affects in the running launcher
static void run_refresh(SettingRefresh refresh)
{
    switch (refresh) {
        case SET_REFRESH_NONE:
            return;
        case SET_REFRESH_LAYOUT:
            refresh_layout();
            break;
        case SET_REFRESH_TITLES:
            reload_titles();
            break;
        case SET_REFRESH_BACKGROUND:
            reload_background();
            break;
        case SET_REFRESH_TITLE_FONT:
            reload_title_font();
            break;
        case SET_REFRESH_HIGHLIGHT:
            reload_highlight();
            break;
        case SET_REFRESH_SCROLL:
            reload_scroll();
            break;
        case SET_REFRESH_CLOCK:
            reload_clock();
            break;
        case SET_REFRESH_SCREENSAVER:
            reload_screensaver();
            break;
        case SET_REFRESH_GAMEPAD:
            reload_gamepad();
            list_pads();
            break;
        case SET_REFRESH_FRAME:
            apply_frame_timing();
            break;
    }
    log_debug("Settings: refreshed %s", refresh_name(refresh));
}

// A function to put a setting's value into the running launcher, then refresh what it affects.
// Two settings act beyond any group: the OS screensaver block, and the menu :home goes to.
static void apply_slot(const SettingSlot *slot, bool refresh)
{
    config_store(slot->def->id, slot->menu >= 0 ? menus[slot->menu] : NULL, &slot->value);
    if (slot->def->id == SET_ID_SLIDESHOW_FADE)
        update_slideshow_timing();
    refresh_effective();
    if (slot->def->id == SET_ID_INHIBIT_OS_SCREENSAVER)
        apply_os_screensaver();
    else if (slot->def->id == SET_ID_DEFAULT_MENU)
        apply_default_menu();
    if (refresh)
        run_refresh(slot->def->refresh);
}

// A function to put every value back into the launcher after Discard, refreshing once each group
// whose settings changed back, the title font first (it renders the titles and lays the menu out)
static void apply_all(void)
{
    bool due[SET_REFRESH_FRAME + 1];
    memset(due, 0, sizeof(due));
    for (int i = 0; i < settings_slot_count(model); i++) {
        SettingSlot *slot = settings_slot_at(model, i);
        SettingValue now = config_read(slot->def->id, slot->menu >= 0 ? menus[slot->menu] : NULL);
        if (setting_equal(slot->def, &now, &slot->value))
            continue;
        apply_slot(slot, false);
        due[slot->def->refresh] = true;
    }
    static const SettingRefresh order[] = { SET_REFRESH_TITLE_FONT, SET_REFRESH_TITLES, SET_REFRESH_BACKGROUND,
        SET_REFRESH_HIGHLIGHT, SET_REFRESH_SCROLL, SET_REFRESH_CLOCK, SET_REFRESH_SCREENSAVER,
        SET_REFRESH_GAMEPAD, SET_REFRESH_FRAME, SET_REFRESH_LAYOUT };
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        if (due[order[i]])
            run_refresh(order[i]);
    }
}
```

The Discard check `f50-discard` reads `Menu 'Main': 4 x 1 grid` between the discard and the close. Its Columns change is in the layout group, so `refresh_layout()` still logs it.

In `handle_event()`, add the picker's case before `SETTINGS_EVENT_MOVED`:

```c
        case SETTINGS_EVENT_PICK:
            // The pickers open here from Task 9 on
            break;
```

In `handle_command()`, log the page path when it changes. Replace its last three lines with:

```c
    SettingsPage before = settings_page(model);
    SettingsEvent event = settings_command(model, key);
    handle_event(&event);
    if (model == NULL)
        return;
    if (before != SETTINGS_PAGE_BACKGROUND && settings_page(model) == SETTINGS_PAGE_BACKGROUND)
        counted_folder[0] = '\0';   // The Background page opened: count the Folder's images again
    char path[512];
    settings_path(model, path, sizeof(path));
    if (strcmp(path, logged_path) != 0) {
        copy_string(logged_path, path, sizeof(logged_path));
        log_debug("Settings: page %s", path);
    }
```

with `static char logged_path[512];            // The page path last logged` among the file's statics. Reset it to `""` in `settings_open()` beside `drawn_note[0] = '\0';`.

In `measure_layout()`, widen the column's sample. Replace `labels` and `values` with:

```c
    static const char *const labels[] = {
        "General", "Background", "Menus", "Titles", "Highlight", "Scroll indicators", "Clock", "Screensaver",
        "Controls", "Discard changes", "Mode", "Colour", "Image", "Folder", "Change every", "Fade", "Rows",
        "Columns", "Largest button", "Size", "All menus", "Try again", "Leave without saving", "Use this folder",
        "Default menu", "Wrap around", "Reset on Back", "Mouse select", "Block the OS screensaver", "VSync",
        "FPS limit", "After launching an app", "App timeout", "Startup command", "Quit command",
        "See-through colour", "Overlay", "Overlay colour", "Overlay opacity", "Icon spacing", "Vertical centre",
        "Show titles", "Font", "Opacity", "Shadows", "Shadow colour", "Too long", "Padding", "Show",
        "Fill colour", "Fill opacity", "Outline size", "Outline colour", "Outline opacity", "Corner radius",
        "Vertical padding", "Horizontal padding", "Show date", "Weekday", "Alignment", "Margin", "Time", "Date",
        "On", "Idle time", "Dim level", "Pause slideshow", "Gamepad", "Device", "Mappings file"
    };
    static const char *const values[] = {
        LEFT_ARROW " Transparent " RIGHT_ARROW, LEFT_ARROW " Custom #000000 " RIGHT_ARROW,
        LEFT_ARROW " All menus (1024 px) " RIGHT_ARROW, LEFT_ARROW " Fixed 512 " RIGHT_ARROW,
        "12 \xC3\x97 10 " RIGHT_ARROW, LEFT_ARROW " Keep showing " RIGHT_ARROW,
        LEFT_ARROW " Pad 15 (not connected) " RIGHT_ARROW, LEFT_ARROW " 12.5% " RIGHT_ARROW
    };
```

The column's width is still capped at 32% of the screen, and `draw_text()` cuts longer values with "...".

In `draw_row()`, show a picker row's arrows and grey a greyed row's value. Replace the `value` formatting and the two `draw_text()` calls (lines 1111-1120) with:

```c
    bool steps = row->slot != NULL && (row->slot->def->type == SET_TYPE_COLOR ||
                 row->slot->def->type == SET_TYPE_MENU || row->slot->def->type == SET_TYPE_DEVICE);
    char value[320];
    if (row->enabled && highlighted && (row->kind == SETTINGS_ROW_SETTING || (row->kind == SETTINGS_ROW_PICK && steps)))
        snprintf(value, sizeof(value), LEFT_ARROW " %s " RIGHT_ARROW, row->value);
    else if (row->kind == SETTINGS_ROW_LINK || row->kind == SETTINGS_ROW_BROWSE || row->kind == SETTINGS_ROW_PICK)
        snprintf(value, sizeof(value), "%s " RIGHT_ARROW, row->value);
    else
        snprintf(value, sizeof(value), "%s", row->value);
    int value_width = min_int(text_width(font_row, value), width / 2);
    Uint8 label_alpha = row->enabled ? 255 : ALPHA_DIM;
    Uint8 value_alpha = !row->enabled ? ALPHA_DIM : highlighted ? 255 : ALPHA_VALUE;
    draw_text(font_row, row->label, x + pad, text_y, width - value_width - 3 * pad, label_alpha, false);
    draw_text(font_row, value, x + width - pad, text_y, width / 2, value_alpha, true);
```

Add above `draw_caption()`:

```c
// A function to say why the row under the cursor is greyed, or "" when it is not
static const char *cursor_why(void)
{
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(model, rows, SETTINGS_MAX_ROWS);
    int cursor = settings_cursor(model);
    if (cursor < 0 || cursor >= count || rows[cursor].enabled || rows[cursor].why == NULL)
        return "";
    return rows[cursor].why;
}
```

In `draw_caption()`, replace `const char *note = browser != NULL ? browser_caption() : settings_notice(model);` with:

```c
    const char *note = browser != NULL ? browser_caption()
                     : settings_notice(model)[0] != '\0' ? settings_notice(model) : cursor_why();
```

Add above `settings_draw()`:

```c
// A function to dim the scene as the screensaver would, while the Screensaver page is open
static void dim_preview(void)
{
    if (settings_page(model) != SETTINGS_PAGE_SCREENSAVER || eff.screensaver_alpha < 1)
        return;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8) eff.screensaver_alpha);
    SDL_RenderFillRect(renderer, NULL);
}
```

In `settings_draw()`, call `dim_preview();` directly after each of its two `draw_scene(true);` calls. The first is inside the preview's render target, and the second is before the column's backing.

In `settings_open()`, after the loop that sets every global entry (lines 1321-1324), call `list_pads();`.

`settings_screen.c` needs `extern Effective eff;`, which `launcher.h` declares.

- [ ] **Step 5: See the harness pass**

Build and run the unit tests: `100% tests passed`. Run the **Linux unit tests** (label `t6`): `warnings outside src/external: 0`.

Run the headless harness on Debian and Fedora (labels `t6`, `t6-fedora`), then the leak pass on both (`t6-leaks`, `t6-fedora-leaks`). Expected: `0 failed` in all four. Every 3a settings check passes with its extra `Down`.

If a check fails, read its log in `C:/Users/jscha/ClaudeScratch/streamflex-headless-out/<label>/` before changing anything.
- `f55-tour`'s many keys arrive 100 ms apart. A slow host can make one land while a feature reloads, which is what the check is for, so do not slow the keys down.
- If `f55-frame` shows `Could not turn VSync off`, the harness's Mesa renderer refused `SDL_RenderSetVSync()`. Stop and report it: the frame timing must then say VSync, and the check's expectation is the user's call.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/settings_screen.c tests/headless/checks/50-settings.sh tests/headless/checks/55-settings-pages.sh tests/headless/checks/60-settings-background.sh tests/headless/checks/65-settings-reasons.sh tests/headless/fixtures/f55-tour.ini tests/headless/fixtures/f55-frame.ini tests/headless/fixtures/f55-default.ini tests/headless/fixtures/f55-dim.ini tests/headless/fixtures/f55-clock.ini tests/headless/fixtures/f55-odd.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: every setting on the settings screen applies live through its refresh group"
```

### Task 7: The shared list picker's model (`listpick.c`)

A pure model of a list to choose from. It serves:
- the command picker;
- the font picker;
- the *Default menu* and *Device* lists;
- a binding's command.

Each row has a label, the value it chooses, and whether it can be chosen, with the reason when it cannot. The cursor moves with Up and Down, and pages with Left and Right. A value from the file that matches no row is pinned first as *Custom: …*, so choosing it keeps it. OK chooses, and Back cancels.

**Files:**
- Create: `src/listpick.h`, `src/listpick.c`
- Modify: `tests/CMakeLists.txt` (`test_listpick`; `listpick.c` joins `test_alloc`)
- Test: `tests/test_listpick.c`, `tests/test_alloc.c`

**Interfaces:**
- Consumes: `alloc.h`.
- Produces:

```c
#define LISTPICK_TEXT_MAX 1024
typedef struct {
    char *label;        // What the row shows
    char *value;        // What choosing it gives
    bool enabled;
    const char *why;    // Why it cannot be chosen; NULL when it can (a static string)
    bool custom;        // The pinned row for a value that matched no other
} ListPickRow;
typedef enum { LISTPICK_UP, LISTPICK_DOWN, LISTPICK_PAGE_UP, LISTPICK_PAGE_DOWN, LISTPICK_OK, LISTPICK_BACK } ListPickCommand;
typedef enum { LISTPICK_NONE, LISTPICK_MOVED, LISTPICK_CHOSEN, LISTPICK_CANCELLED } ListPickResult;
typedef struct ListPick ListPick;
ListPick *listpick_create(void);
void listpick_free(ListPick *pick);
bool listpick_add(ListPick *pick, const char *label, const char *value, bool enabled, const char *why);
bool listpick_has(const ListPick *pick, const char *value);
bool listpick_select(ListPick *pick, const char *value, const char *custom_row_label);
ListPickResult listpick_command(ListPick *pick, ListPickCommand command, int page_rows);
int listpick_count(const ListPick *pick);
const ListPickRow *listpick_row(const ListPick *pick, int index);
int listpick_cursor(const ListPick *pick);
const char *listpick_chosen(const ListPick *pick);
const char *listpick_why(const ListPick *pick);
```

- [ ] **Step 1: Write the failing tests**

Create `tests/test_listpick.c`:

```c
#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "listpick.h"

// A function to make a picker of the rows a small command picker holds
static ListPick *sample(void)
{
    ListPick *pick = listpick_create();
    CHECK(listpick_add(pick, "None", "", true, NULL));
    CHECK(listpick_add(pick, "Left", ":left", true, NULL));
    CHECK(listpick_add(pick, "Right", ":right", true, NULL));
    CHECK(listpick_add(pick, "Quit StreamFlex", ":quit", true, NULL));
    CHECK(listpick_add(pick, "Close the app on show", ":exit", false, "Only on Windows"));
    CHECK(listpick_add(pick, "Kodi", "kodi --standalone", true, NULL));
    return pick;
}

// A function to test moving, paging and the ends
static void test_moves(void)
{
    ListPick *pick = sample();
    CHECK_INT(listpick_count(pick), 6);
    CHECK_INT(listpick_cursor(pick), 0);
    CHECK_INT(listpick_command(pick, LISTPICK_UP, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_DOWN, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 1);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_DOWN, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 4);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_DOWN, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 5);                     // The last row, not past it
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_DOWN, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_UP, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 2);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_UP, 0), LISTPICK_MOVED);   // A page is at least one row
    CHECK_INT(listpick_cursor(pick), 1);
    listpick_free(pick);
}

// A function to test choosing, a row that cannot be chosen, and cancelling
static void test_choose(void)
{
    ListPick *pick = sample();
    CHECK(listpick_select(pick, ":quit", "Custom: :quit"));
    CHECK_INT(listpick_cursor(pick), 3);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_CHOSEN);
    CHECK_STR(listpick_chosen(pick), ":quit");

    // A row that cannot be chosen takes the cursor, and OK says why
    listpick_command(pick, LISTPICK_DOWN, 3);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_NONE);
    CHECK_STR(listpick_why(pick), "Only on Windows");
    listpick_command(pick, LISTPICK_DOWN, 3);
    CHECK(listpick_why(pick) == NULL);                        // Moving clears the reason
    CHECK_INT(listpick_command(pick, LISTPICK_BACK, 3), LISTPICK_CANCELLED);

    // None chooses the empty value
    CHECK(listpick_select(pick, "", "Custom: "));
    CHECK_INT(listpick_cursor(pick), 0);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_CHOSEN);
    CHECK_STR(listpick_chosen(pick), "");
    listpick_free(pick);
}

// A function to test a value that matches no row: pinned first as Custom, chosen as itself
static void test_custom(void)
{
    ListPick *pick = sample();
    CHECK(!listpick_has(pick, "retroarch -f"));
    CHECK(listpick_select(pick, "retroarch -f", "Custom: retroarch -f"));
    CHECK_INT(listpick_count(pick), 7);
    const ListPickRow *row = listpick_row(pick, 0);
    CHECK(row->custom);
    CHECK_STR(row->label, "Custom: retroarch -f");
    CHECK_STR(row->value, "retroarch -f");
    CHECK_INT(listpick_cursor(pick), 0);
    CHECK_STR(listpick_row(pick, 1)->label, "None");        // The rest keep their order
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_CHOSEN);
    CHECK_STR(listpick_chosen(pick), "retroarch -f");

    // Selecting again does not pin a second one, and a value that matches unpins it
    CHECK(listpick_select(pick, "retroarch -f", "Custom: retroarch -f"));
    CHECK_INT(listpick_count(pick), 7);
    CHECK(listpick_select(pick, ":left", "Custom: :left"));
    CHECK_INT(listpick_count(pick), 6);
    CHECK_STR(listpick_row(pick, listpick_cursor(pick))->value, ":left");
    CHECK(listpick_row(pick, 6) == NULL);
    listpick_free(pick);
}

// A function to test an empty picker, as the font picker is while it loads
static void test_empty(void)
{
    ListPick *pick = listpick_create();
    CHECK_INT(listpick_count(pick), 0);
    CHECK_INT(listpick_command(pick, LISTPICK_DOWN, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_BACK, 3), LISTPICK_CANCELLED);
    CHECK(listpick_row(pick, 0) == NULL);
    listpick_free(pick);
    listpick_free(NULL);
}

int main(void)
{
    test_moves();
    test_choose();
    test_custom();
    test_empty();
    return check_report();
}
```

Add to `tests/CMakeLists.txt`, after `test_derive`:

```cmake
# Unit tests for the shared list picker's model (pure)
add_executable(test_listpick test_listpick.c "${PROJECT_SOURCE_DIR}/src/listpick.c" "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_listpick PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME listpick COMMAND test_listpick)
```

and add `"${PROJECT_SOURCE_DIR}/src/listpick.c"` to `test_alloc`'s sources.

In `tests/test_alloc.c`, add `#include "listpick.h"` and, above `main()`:

```c
// A function to prove that the list picker fails cleanly: a row that could not be added is not
// there, a Custom row that could not be pinned is not either, and nothing is left allocated
static void prove_listpick(void)
{
    for (int n = 1;; n++) {
        arm(n);
        ListPick *pick = listpick_create();
        bool added = pick != NULL && listpick_add(pick, "Left", ":left", true, NULL) &&
                     listpick_add(pick, "Right", ":right", true, NULL);
        bool pinned = added && listpick_select(pick, "custom command", "Custom: custom command");
        disarm();
        CHECK_RUN(failed || (added && pinned), n);
        if (!failed)
            CHECK_RUN(listpick_count(pick) == 3 && listpick_row(pick, 0)->custom, n);
        listpick_free(pick);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("the list picker");
}
```

and call `prove_listpick();` after `prove_pads();`.

- [ ] **Step 2: Run the tests to see them fail**

Reconfigure and build (Global Constraints). Expected: the configure fails with `Cannot find source file: .../src/listpick.c`.

- [ ] **Step 3: Write `listpick.h`**

Create `src/listpick.h`:

```c
// The shared list picker's model: rows to choose from, each with a label, the value it gives, and
// whether it can be chosen (with the reason when it cannot); a cursor, paged with Left and Right;
// and a pinned "Custom: ..." row for a value from the file that matches no other row, so choosing
// it keeps it. It serves the command, font, default menu and device pickers. Pure: no SDL, no
// globals; memory comes from alloc.h. settings_pickers.c draws it.
#ifndef LISTPICK_H
#define LISTPICK_H

#include <stdbool.h>

#define LISTPICK_TEXT_MAX 1024  // Longest label or value a row holds

typedef struct {
    char *label;        // What the row shows
    char *value;        // What choosing it gives
    bool enabled;
    const char *why;    // Why it cannot be chosen; NULL when it can (a static string)
    bool custom;        // The pinned row for a value that matched no other
} ListPickRow;

typedef enum {
    LISTPICK_UP,
    LISTPICK_DOWN,
    LISTPICK_PAGE_UP,
    LISTPICK_PAGE_DOWN,
    LISTPICK_OK,
    LISTPICK_BACK
} ListPickCommand;

typedef enum {
    LISTPICK_NONE,       // Nothing happened (an end, or OK on a row that cannot be chosen)
    LISTPICK_MOVED,
    LISTPICK_CHOSEN,     // listpick_chosen() gives the value
    LISTPICK_CANCELLED
} ListPickResult;

typedef struct ListPick ListPick;

ListPick *listpick_create(void);
void listpick_free(ListPick *pick);
bool listpick_add(ListPick *pick, const char *label, const char *value, bool enabled, const char *why);
bool listpick_has(const ListPick *pick, const char *value);
bool listpick_select(ListPick *pick, const char *value, const char *custom_row_label);
ListPickResult listpick_command(ListPick *pick, ListPickCommand command, int page_rows);
int listpick_count(const ListPick *pick);
const ListPickRow *listpick_row(const ListPick *pick, int index);
int listpick_cursor(const ListPick *pick);
const char *listpick_chosen(const ListPick *pick);
const char *listpick_why(const ListPick *pick);

#endif
```

- [ ] **Step 4: Write `listpick.c`**

Create `src/listpick.c`:

```c
#include <stdio.h>
#include <string.h>
#include "listpick.h"
#include "alloc.h"

struct ListPick {
    ListPickRow *rows;
    int count;
    int capacity;
    int cursor;
    const char *chosen;   // The value OK chose, in its row
    const char *why;      // Why the last OK did nothing; NULL otherwise
};

// A function to make an empty picker; NULL when out of memory
ListPick *listpick_create(void)
{
    return alloc_calloc(1, sizeof(ListPick));
}

// A function to free a row's text
static void free_row(ListPickRow *row)
{
    alloc_free(row->label);
    alloc_free(row->value);
}

// A function to free the picker
void listpick_free(ListPick *pick)
{
    if (pick == NULL)
        return;
    for (int i = 0; i < pick->count; i++)
        free_row(&pick->rows[i]);
    alloc_free(pick->rows);
    alloc_free(pick);
}

// A function to make room for one more row
static bool grow(ListPick *pick)
{
    if (pick->count < pick->capacity)
        return true;
    int capacity = pick->capacity ? pick->capacity * 2 : 16;
    ListPickRow *rows = alloc_realloc(pick->rows, (size_t) capacity * sizeof(ListPickRow));
    if (rows == NULL)
        return false;
    pick->rows = rows;
    pick->capacity = capacity;
    return true;
}

// A function to make a row at `at`, moving the rows after it down; false when out of memory, with
// nothing changed
static bool insert_row(ListPick *pick, int at, const char *label, const char *value, bool enabled,
                       const char *why, bool custom)
{
    ListPickRow row;
    row.label = alloc_strdup(label);
    row.value = alloc_strdup(value);
    row.enabled = enabled;
    row.why = enabled ? NULL : why;
    row.custom = custom;
    if (row.label == NULL || row.value == NULL || !grow(pick)) {
        free_row(&row);
        return false;
    }
    memmove(&pick->rows[at + 1], &pick->rows[at], (size_t) (pick->count - at) * sizeof(ListPickRow));
    pick->rows[at] = row;
    pick->count++;
    return true;
}

// A function to add a row at the end; false when out of memory, with nothing added
bool listpick_add(ListPick *pick, const char *label, const char *value, bool enabled, const char *why)
{
    return insert_row(pick, pick->count, label, value, enabled, why, false);
}

// A function to find the row that gives a value, not counting the Custom row; -1 when none does
static int find(const ListPick *pick, const char *value)
{
    for (int i = 0; i < pick->count; i++) {
        if (!pick->rows[i].custom && strcmp(pick->rows[i].value, value) == 0)
            return i;
    }
    return -1;
}

// A function to tell whether a row gives a value, for a caller that keeps its rows unique
bool listpick_has(const ListPick *pick, const char *value)
{
    return find(pick, value) >= 0;
}

// A function to put the cursor on the row that gives a value. A value no row gives is pinned
// first, labelled `custom_row_label` (the caller's "Custom: ..."), so choosing it keeps it; a Custom
// row pinned before goes. False when out of memory, with the cursor on the first row.
bool listpick_select(ListPick *pick, const char *value, const char *custom_row_label)
{
    pick->why = NULL;
    if (pick->count > 0 && pick->rows[0].custom) {
        if (strcmp(pick->rows[0].value, value) == 0) {
            pick->cursor = 0;
            return true;
        }
        free_row(&pick->rows[0]);
        memmove(&pick->rows[0], &pick->rows[1], (size_t) (pick->count - 1) * sizeof(ListPickRow));
        pick->count--;
    }
    int at = find(pick, value);
    pick->cursor = at >= 0 ? at : 0;
    if (at >= 0)
        return true;
    return insert_row(pick, 0, custom_row_label, value, true, NULL, true);
}

// A function to act on one key: move, page, choose or cancel
ListPickResult listpick_command(ListPick *pick, ListPickCommand command, int page_rows)
{
    int before = pick->cursor;
    int last = pick->count - 1;
    pick->why = NULL;
    if (page_rows < 1)
        page_rows = 1;
    switch (command) {
        case LISTPICK_UP:
            if (pick->cursor > 0)
                pick->cursor--;
            break;
        case LISTPICK_DOWN:
            if (pick->cursor < last)
                pick->cursor++;
            break;
        case LISTPICK_PAGE_UP:
            pick->cursor = pick->cursor - page_rows < 0 ? 0 : pick->cursor - page_rows;
            break;
        case LISTPICK_PAGE_DOWN:
            pick->cursor = pick->cursor + page_rows > last ? (last > 0 ? last : 0) : pick->cursor + page_rows;
            break;
        case LISTPICK_OK: {
            if (pick->cursor < 0 || pick->cursor > last)
                return LISTPICK_NONE;
            const ListPickRow *row = &pick->rows[pick->cursor];
            if (!row->enabled) {
                pick->why = row->why;
                return LISTPICK_NONE;
            }
            pick->chosen = row->value;
            return LISTPICK_CHOSEN;
        }
        case LISTPICK_BACK:
            return LISTPICK_CANCELLED;
    }
    return pick->cursor != before ? LISTPICK_MOVED : LISTPICK_NONE;
}

// A function to count the rows
int listpick_count(const ListPick *pick)
{
    return pick->count;
}

// A function to get a row; NULL past the end
const ListPickRow *listpick_row(const ListPick *pick, int index)
{
    return index >= 0 && index < pick->count ? &pick->rows[index] : NULL;
}

// A function to get the cursor
int listpick_cursor(const ListPick *pick)
{
    return pick->cursor;
}

// A function to get the value OK chose, valid until the picker changes or is freed; "" before any
const char *listpick_chosen(const ListPick *pick)
{
    return pick->chosen != NULL ? pick->chosen : "";
}

// A function to say why the last OK chose nothing; NULL when it did, or when it was not OK
const char *listpick_why(const ListPick *pick)
{
    return pick->why;
}
```

Add `listpick.c` and `listpick.h` to `src/CMakeLists.txt`'s `SOURCES`, after `config_fields.h`.

- [ ] **Step 5: Run the tests to see them pass**

Reconfigure, build and run the unit tests: `100% tests passed`, with `listpick` and `alloc` among them. Run the **Linux unit tests** with the label `t7`: `warnings outside src/external: 0`.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/listpick.h src/listpick.c src/CMakeLists.txt tests/test_listpick.c tests/test_alloc.c tests/CMakeLists.txt
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: the shared list picker's model"
```

### Task 8: The colour picker's model and the contrast maths (`colorpick.c`)

A pure model of the colour picker:
- a 6 × 4 grid of the spec's 24 named swatches, moved with all four arrows;
- a *Custom #RRGGBB* row below it, which opens a hex editor. There, Left and Right choose a digit, Up and Down step it through 0-F with wrapping, OK keeps the colour and Back leaves the editor.

Back on the grid cancels, and the preview goes back to the colour the picker opened with. The same module holds the contrast warning's maths: WCAG relative luminance, the contrast ratio, the mean luminance of an image's pixels, and a colour laid over a background at an opacity.

**Files:**
- Create: `src/colorpick.h`, `src/colorpick.c`
- Modify: `tests/CMakeLists.txt` (`test_colorpick`, linked with `m` on Unix)
- Test: `tests/test_colorpick.c`

**Interfaces:**
- Consumes: `SettingColor` (`settings.h`).
- Produces:

```c
#define COLORPICK_COLUMNS 6
#define COLORPICK_ROWS 4
#define COLORPICK_SWATCHES 24
#define COLORPICK_CUSTOM 24                   // The cursor on the Custom #RRGGBB row
#define COLORPICK_MIN_CONTRAST 3.0            // WCAG's 3:1 for large text
typedef enum { COLORPICK_UP, COLORPICK_DOWN, COLORPICK_LEFT, COLORPICK_RIGHT, COLORPICK_OK, COLORPICK_BACK } ColourPickCommand;
typedef enum { COLORPICK_NONE, COLORPICK_MOVED, COLORPICK_CHOSEN, COLORPICK_CANCELLED } ColourPickResult;
typedef struct { int cursor; int column; bool editing; int digit; SettingColor original; SettingColor hex; SettingColor chosen; } ColourPick;
void colorpick_open(ColourPick *pick, SettingColor current);
ColourPickResult colorpick_command(ColourPick *pick, ColourPickCommand command);
SettingColor colorpick_shown(const ColourPick *pick);
const char *colorpick_name(int index);
SettingColor colorpick_swatch(int index);
int colorpick_find(SettingColor color);
double colour_luminance(SettingColor color);
double colour_contrast(double a, double b);
double colour_mean_luminance(const unsigned char *rgba, int width, int height, int pitch);
double colour_over(double below, SettingColor over, int alpha);
```

- [ ] **Step 1: Write the failing tests**

Create `tests/test_colorpick.c`:

```c
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "colorpick.h"

// A function to make a colour
static SettingColor rgb(unsigned char r, unsigned char g, unsigned char b)
{
    SettingColor color = { r, g, b };
    return color;
}

// A function to compare two colours
static bool same(SettingColor a, SettingColor b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

// A function to compare two numbers to 3 places
static bool near(double a, double b)
{
    return fabs(a - b) < 0.001;
}

// A function to test the swatches: 24, in the spec's order, with names
static void test_swatches(void)
{
    CHECK_STR(colorpick_name(0), "Black");
    CHECK(same(colorpick_swatch(0), rgb(0x00, 0x00, 0x00)));
    CHECK_STR(colorpick_name(9), "Burgundy");
    CHECK(same(colorpick_swatch(9), rgb(0x4A, 0x15, 0x20)));
    CHECK_STR(colorpick_name(10), "White");
    CHECK_STR(colorpick_name(13), "Dark grey");
    CHECK_STR(colorpick_name(14), "Red");
    CHECK(same(colorpick_swatch(14), rgb(0xD0, 0x30, 0x30)));
    CHECK_STR(colorpick_name(23), "Pink");
    CHECK(same(colorpick_swatch(23), rgb(0xD0, 0x48, 0x90)));
    CHECK_INT(colorpick_find(rgb(0x07, 0x60, 0x6C)), 6);        // Teal
    CHECK_INT(colorpick_find(rgb(0x07, 0x60, 0x6D)), -1);
}

// A function to test moving through the grid, onto the Custom row and back
static void test_grid(void)
{
    ColourPick pick;
    colorpick_open(&pick, rgb(0x0B, 0x1F, 0x3A));               // Navy, the sixth swatch
    CHECK_INT(pick.cursor, 5);
    CHECK_INT(colorpick_command(&pick, COLORPICK_RIGHT), COLORPICK_NONE);   // The row's end
    CHECK_INT(colorpick_command(&pick, COLORPICK_UP), COLORPICK_NONE);      // The grid's top
    CHECK_INT(colorpick_command(&pick, COLORPICK_LEFT), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, 4);
    CHECK(same(colorpick_shown(&pick), rgb(0x12, 0x1A, 0x2E)));   // The preview follows: Midnight
    for (int i = 0; i < 3; i++)
        CHECK_INT(colorpick_command(&pick, COLORPICK_DOWN), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, 22);                                    // Row 4, column 5: Indigo
    CHECK_INT(colorpick_command(&pick, COLORPICK_DOWN), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, COLORPICK_CUSTOM);
    CHECK(same(colorpick_shown(&pick), rgb(0x0B, 0x1F, 0x3A)));   // Custom shows the colour it opened with
    CHECK_INT(colorpick_command(&pick, COLORPICK_DOWN), COLORPICK_NONE);
    CHECK_INT(colorpick_command(&pick, COLORPICK_LEFT), COLORPICK_NONE);   // One wide row
    CHECK_INT(colorpick_command(&pick, COLORPICK_UP), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, 22);                                    // Back to the column it left
    CHECK_INT(colorpick_command(&pick, COLORPICK_OK), COLORPICK_CHOSEN);
    CHECK(same(pick.chosen, rgb(0x50, 0x48, 0xC0)));

    // Back on the grid cancels, and the preview goes back
    colorpick_open(&pick, rgb(0x0B, 0x1F, 0x3A));
    colorpick_command(&pick, COLORPICK_DOWN);
    CHECK_INT(colorpick_command(&pick, COLORPICK_BACK), COLORPICK_CANCELLED);
    CHECK(same(colorpick_shown(&pick), rgb(0x0B, 0x1F, 0x3A)));
}

// A function to test the hex editor: digits chosen, stepped with wrapping, kept or left
static void test_hex(void)
{
    ColourPick pick;
    colorpick_open(&pick, rgb(0x12, 0x34, 0x5F));               // Not a swatch: the cursor on Custom
    CHECK_INT(pick.cursor, COLORPICK_CUSTOM);
    CHECK_INT(colorpick_command(&pick, COLORPICK_OK), COLORPICK_MOVED);
    CHECK(pick.editing);
    CHECK_INT(pick.digit, 0);
    CHECK_INT(colorpick_command(&pick, COLORPICK_LEFT), COLORPICK_NONE);    // The first digit
    CHECK_INT(colorpick_command(&pick, COLORPICK_UP), COLORPICK_MOVED);
    CHECK(same(colorpick_shown(&pick), rgb(0x22, 0x34, 0x5F)));   // 1 -> 2 in the red's first digit
    for (int i = 0; i < 5; i++)
        colorpick_command(&pick, COLORPICK_RIGHT);
    CHECK_INT(pick.digit, 5);
    CHECK_INT(colorpick_command(&pick, COLORPICK_RIGHT), COLORPICK_NONE);   // The last digit
    CHECK_INT(colorpick_command(&pick, COLORPICK_UP), COLORPICK_MOVED);
    CHECK(same(colorpick_shown(&pick), rgb(0x22, 0x34, 0x50)));   // F wraps to 0
    colorpick_command(&pick, COLORPICK_DOWN);
    colorpick_command(&pick, COLORPICK_DOWN);
    CHECK(same(colorpick_shown(&pick), rgb(0x22, 0x34, 0x5E)));   // 0 wraps to F, then E

    // Back leaves the editor, not the picker; OK in it keeps the colour
    CHECK_INT(colorpick_command(&pick, COLORPICK_BACK), COLORPICK_MOVED);
    CHECK(!pick.editing);
    CHECK_INT(pick.cursor, COLORPICK_CUSTOM);
    CHECK_INT(colorpick_command(&pick, COLORPICK_OK), COLORPICK_MOVED);   // Opens on the edited colour
    CHECK(same(colorpick_shown(&pick), rgb(0x22, 0x34, 0x5E)));
    CHECK_INT(colorpick_command(&pick, COLORPICK_OK), COLORPICK_CHOSEN);
    CHECK(same(pick.chosen, rgb(0x22, 0x34, 0x5E)));
}

// A function to test luminance and contrast against WCAG's own figures
static void test_contrast(void)
{
    CHECK(near(colour_luminance(rgb(0, 0, 0)), 0.0));
    CHECK(near(colour_luminance(rgb(255, 255, 255)), 1.0));
    CHECK(near(colour_luminance(rgb(0x80, 0x80, 0x80)), 0.2159));
    CHECK(near(colour_contrast(1.0, 0.0), 21.0));
    CHECK(near(colour_contrast(0.0, 1.0), 21.0));           // Either way round
    double grey = colour_luminance(rgb(0x76, 0x76, 0x76));   // WCAG's #767676 on white is 4.54:1
    CHECK(fabs(colour_contrast(1.0, grey) - 4.54) < 0.01);

    // A black overlay at half opacity over white; a colour at full opacity is the colour
    CHECK(near(colour_over(1.0, rgb(0, 0, 0), 128), 1.0 - 128.0 / 255.0));
    CHECK(near(colour_over(0.3, rgb(255, 255, 255), 255), 1.0));
    CHECK(near(colour_over(0.3, rgb(255, 255, 255), 0), 0.3));
}

// A function to test an image's mean luminance, read from its pixels
static void test_mean(void)
{
    unsigned char pixels[2 * 2 * 4];
    memset(pixels, 0, sizeof(pixels));
    for (int i = 0; i < 4; i++)
        pixels[i * 4 + 3] = 0xFF;
    memset(pixels, 0xFF, 4);                                  // One white pixel, three black
    CHECK(near(colour_mean_luminance(pixels, 2, 2, 8), 0.25));

    // A pitch wider than the row, and an image larger than the sampling grid
    static unsigned char big[300 * 200 * 4];
    for (int i = 0; i < 300 * 200; i++) {
        big[i * 4] = big[i * 4 + 1] = big[i * 4 + 2] = 0xFF;
        big[i * 4 + 3] = 0xFF;
    }
    CHECK(near(colour_mean_luminance(big, 300, 200, 300 * 4), 1.0));
    CHECK(near(colour_mean_luminance(big, 0, 0, 0), 0.0));    // Nothing to read
}

int main(void)
{
    test_swatches();
    test_grid();
    test_hex();
    test_contrast();
    test_mean();
    return check_report();
}
```

Add to `tests/CMakeLists.txt`, after `test_listpick`:

```cmake
# Unit tests for the colour picker's model and the contrast maths (pure; the maths needs libm)
add_executable(test_colorpick test_colorpick.c "${PROJECT_SOURCE_DIR}/src/colorpick.c")
target_include_directories(test_colorpick PRIVATE "${PROJECT_SOURCE_DIR}/src")
if (UNIX)
  target_link_libraries(test_colorpick m)
endif ()
add_test(NAME colorpick COMMAND test_colorpick)
```

- [ ] **Step 2: Run the tests to see them fail**

Reconfigure (Global Constraints). Expected: the configure fails with `Cannot find source file: .../src/colorpick.c`.

- [ ] **Step 3: Write `colorpick.h` and `colorpick.c`**

Create `src/colorpick.h`:

```c
// The colour picker's model: a 6 x 4 grid of named swatches moved with all four arrows, and below it
// a Custom #RRGGBB row whose hex editor steps one digit at a time; and the contrast warning's maths
// (WCAG relative luminance and contrast, an image's mean luminance, a colour laid over another).
// Pure: no SDL, no globals, no allocation. settings_pickers.c draws it.
#ifndef COLORPICK_H
#define COLORPICK_H

#include <stdbool.h>
#include "settings.h"

#define COLORPICK_COLUMNS 6
#define COLORPICK_ROWS 4
#define COLORPICK_SWATCHES 24
#define COLORPICK_CUSTOM 24               // The cursor on the Custom #RRGGBB row
#define COLORPICK_MIN_CONTRAST 3.0        // WCAG's 3:1: below it the caption warns

typedef enum {
    COLORPICK_UP,
    COLORPICK_DOWN,
    COLORPICK_LEFT,
    COLORPICK_RIGHT,
    COLORPICK_OK,
    COLORPICK_BACK
} ColourPickCommand;

typedef enum {
    COLORPICK_NONE,
    COLORPICK_MOVED,      // What the preview shows may have changed
    COLORPICK_CHOSEN,     // `chosen` holds the colour
    COLORPICK_CANCELLED
} ColourPickResult;

typedef struct {
    int cursor;              // 0-23 a swatch, row by row; COLORPICK_CUSTOM the Custom row
    int column;              // The column Up goes back to from the Custom row
    bool editing;            // The hex editor is open
    int digit;               // 0-5: the digit of #RRGGBB that Up and Down step
    SettingColor original;   // The colour the picker opened with
    SettingColor hex;        // The hex editor's colour
    SettingColor chosen;     // The colour OK chose
} ColourPick;

void colorpick_open(ColourPick *pick, SettingColor current);
ColourPickResult colorpick_command(ColourPick *pick, ColourPickCommand command);
SettingColor colorpick_shown(const ColourPick *pick);
const char *colorpick_name(int index);
SettingColor colorpick_swatch(int index);
int colorpick_find(SettingColor color);
double colour_luminance(SettingColor color);
double colour_contrast(double a, double b);
double colour_mean_luminance(const unsigned char *rgba, int width, int height, int pitch);
double colour_over(double below, SettingColor over, int alpha);

#endif
```

Create `src/colorpick.c`:

```c
#include <math.h>
#include <string.h>
#include "colorpick.h"

#define SAMPLES 64   // The most points across and down an image's mean luminance reads

// The swatches, row by row: 3a's ten presets, four neutrals, then ten accents
static const struct {
    const char *name;
    SettingColor color;
} SWATCHES[COLORPICK_SWATCHES] = {
    { "Black",      { 0x00, 0x00, 0x00 } },
    { "Charcoal",   { 0x1E, 0x1E, 0x1E } },
    { "Graphite",   { 0x33, 0x38, 0x3D } },
    { "Slate",      { 0x2E, 0x34, 0x40 } },
    { "Midnight",   { 0x12, 0x1A, 0x2E } },
    { "Navy",       { 0x0B, 0x1F, 0x3A } },
    { "Teal",       { 0x07, 0x60, 0x6C } },
    { "Forest",     { 0x1E, 0x3B, 0x2F } },
    { "Plum",       { 0x3B, 0x1F, 0x3A } },
    { "Burgundy",   { 0x4A, 0x15, 0x20 } },
    { "White",      { 0xFF, 0xFF, 0xFF } },
    { "Light grey", { 0xC8, 0xC8, 0xC8 } },
    { "Grey",       { 0x80, 0x80, 0x80 } },
    { "Dark grey",  { 0x4A, 0x4A, 0x4A } },
    { "Red",        { 0xD0, 0x30, 0x30 } },
    { "Orange",     { 0xE0, 0x70, 0x20 } },
    { "Amber",      { 0xF0, 0xB0, 0x00 } },
    { "Yellow",     { 0xF0, 0xE0, 0x40 } },
    { "Lime",       { 0x80, 0xC0, 0x40 } },
    { "Green",      { 0x30, 0xA0, 0x50 } },
    { "Cyan",       { 0x20, 0xB0, 0xC0 } },
    { "Blue",       { 0x30, 0x70, 0xD0 } },
    { "Indigo",     { 0x50, 0x48, 0xC0 } },
    { "Pink",       { 0xD0, 0x48, 0x90 } }
};

// A function to get a swatch's name
const char *colorpick_name(int index)
{
    return index >= 0 && index < COLORPICK_SWATCHES ? SWATCHES[index].name : "";
}

// A function to get a swatch's colour
SettingColor colorpick_swatch(int index)
{
    SettingColor black = { 0, 0, 0 };
    return index >= 0 && index < COLORPICK_SWATCHES ? SWATCHES[index].color : black;
}

// A function to find a colour among the swatches; -1 when it is none of them
int colorpick_find(SettingColor color)
{
    for (int i = 0; i < COLORPICK_SWATCHES; i++) {
        const SettingColor *s = &SWATCHES[i].color;
        if (s->r == color.r && s->g == color.g && s->b == color.b)
            return i;
    }
    return -1;
}

// A function to open the picker on a colour: the cursor on its swatch, else on the Custom row
void colorpick_open(ColourPick *pick, SettingColor current)
{
    memset(pick, 0, sizeof(*pick));
    pick->original = current;
    pick->hex = current;
    pick->chosen = current;
    int found = colorpick_find(current);
    pick->cursor = found >= 0 ? found : COLORPICK_CUSTOM;
    pick->column = found >= 0 ? found % COLORPICK_COLUMNS : 0;
}

// A function to step one hex digit of a colour (0 the red's first, 5 the blue's second) up or
// down, wrapping F to 0 and 0 to F
static SettingColor step_digit(SettingColor color, int digit, int direction)
{
    unsigned char *channel = digit < 2 ? &color.r : digit < 4 ? &color.g : &color.b;
    int shift = digit % 2 == 0 ? 4 : 0;
    int nibble = (*channel >> shift) & 0x0F;
    nibble = (nibble + (direction > 0 ? 1 : 15)) % 16;
    *channel = (unsigned char) ((*channel & ~(0x0F << shift)) | (nibble << shift));
    return color;
}

// A function to act on a key in the hex editor
static ColourPickResult edit(ColourPick *pick, ColourPickCommand command)
{
    switch (command) {
        case COLORPICK_LEFT:
            if (pick->digit == 0)
                return COLORPICK_NONE;
            pick->digit--;
            return COLORPICK_MOVED;
        case COLORPICK_RIGHT:
            if (pick->digit == 5)
                return COLORPICK_NONE;
            pick->digit++;
            return COLORPICK_MOVED;
        case COLORPICK_UP:
        case COLORPICK_DOWN:
            pick->hex = step_digit(pick->hex, pick->digit, command == COLORPICK_UP ? 1 : -1);
            return COLORPICK_MOVED;
        case COLORPICK_OK:
            pick->chosen = pick->hex;
            return COLORPICK_CHOSEN;
        case COLORPICK_BACK:
            pick->editing = false;
            return COLORPICK_MOVED;
    }
    return COLORPICK_NONE;
}

// A function to act on one key: move through the grid and onto the Custom row, open the hex
// editor, choose, or cancel
ColourPickResult colorpick_command(ColourPick *pick, ColourPickCommand command)
{
    if (pick->editing)
        return edit(pick, command);
    int before = pick->cursor;
    int row = pick->cursor / COLORPICK_COLUMNS;
    int column = pick->cursor % COLORPICK_COLUMNS;
    switch (command) {
        case COLORPICK_LEFT:
            if (pick->cursor != COLORPICK_CUSTOM && column > 0)
                pick->cursor--;
            break;
        case COLORPICK_RIGHT:
            if (pick->cursor != COLORPICK_CUSTOM && column < COLORPICK_COLUMNS - 1)
                pick->cursor++;
            break;
        case COLORPICK_UP:
            if (pick->cursor == COLORPICK_CUSTOM)
                pick->cursor = (COLORPICK_ROWS - 1) * COLORPICK_COLUMNS + pick->column;
            else if (row > 0)
                pick->cursor -= COLORPICK_COLUMNS;
            break;
        case COLORPICK_DOWN:
            if (pick->cursor == COLORPICK_CUSTOM)
                break;
            if (row < COLORPICK_ROWS - 1)
                pick->cursor += COLORPICK_COLUMNS;
            else {
                pick->column = column;
                pick->cursor = COLORPICK_CUSTOM;
            }
            break;
        case COLORPICK_OK:
            if (pick->cursor == COLORPICK_CUSTOM) {
                pick->editing = true;
                pick->digit = 0;
                return COLORPICK_MOVED;
            }
            pick->chosen = SWATCHES[pick->cursor].color;
            return COLORPICK_CHOSEN;
        case COLORPICK_BACK:
            pick->cursor = before;
            pick->editing = false;
            return COLORPICK_CANCELLED;
    }
    if (pick->cursor != COLORPICK_CUSTOM)
        pick->column = pick->cursor % COLORPICK_COLUMNS;
    return pick->cursor != before ? COLORPICK_MOVED : COLORPICK_NONE;
}

// A function to say what the preview shows: the swatch under the cursor; on the Custom row, the hex
// editor's colour (the colour the picker opened with, until it is edited)
SettingColor colorpick_shown(const ColourPick *pick)
{
    if (pick->cursor == COLORPICK_CUSTOM)
        return pick->hex;
    return SWATCHES[pick->cursor].color;
}

// A function to turn an sRGB channel into linear light, as WCAG defines it
static double linear(unsigned char channel)
{
    double c = (double) channel / 255.0;
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

// A function to give a colour's relative luminance: 0 for black, 1 for white (WCAG 2)
double colour_luminance(SettingColor color)
{
    return 0.2126 * linear(color.r) + 0.7152 * linear(color.g) + 0.0722 * linear(color.b);
}

// A function to give the contrast ratio of two luminances, 1 to 21, whichever is lighter
double colour_contrast(double a, double b)
{
    double light = a > b ? a : b;
    double dark = a > b ? b : a;
    return (light + 0.05) / (dark + 0.05);
}

// A function to give an image's mean relative luminance (RGBA bytes, alpha ignored), read at most
// SAMPLES points across and down; 0 for an empty image
double colour_mean_luminance(const unsigned char *rgba, int width, int height, int pitch)
{
    if (width <= 0 || height <= 0)
        return 0.0;
    int across = width < SAMPLES ? width : SAMPLES;
    int down = height < SAMPLES ? height : SAMPLES;
    double sum = 0.0;
    for (int j = 0; j < down; j++) {
        int y = (int) ((long long) j * height / down);
        for (int i = 0; i < across; i++) {
            int x = (int) ((long long) i * width / across);
            const unsigned char *p = rgba + (long long) y * pitch + (long long) x * 4;
            SettingColor color = { p[0], p[1], p[2] };
            sum += colour_luminance(color);
        }
    }
    return sum / (double) (across * down);
}

// A function to give the luminance of a colour laid at an alpha (0-255) over a background of a given
// luminance. It mixes the luminances, not the colours: close enough for an advisory warning.
double colour_over(double below, SettingColor over, int alpha)
{
    double a = (double) alpha / 255.0;
    return a * colour_luminance(over) + (1.0 - a) * below;
}
```

Add `colorpick.c` and `colorpick.h` to `src/CMakeLists.txt`'s `SOURCES`, after `listpick.h`.

The mean-luminance test's `0.25`: one white pixel (luminance 1) and three black (0) average to 0.25. `near()` allows 0.001.

- [ ] **Step 4: Run the tests to see them pass**

Reconfigure, build and run the unit tests: `100% tests passed`, with `colorpick` among them. Run the **Linux unit tests** (label `t8`): `warnings outside src/external: 0`.

- [ ] **Step 5: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/colorpick.h src/colorpick.c src/CMakeLists.txt tests/test_colorpick.c tests/CMakeLists.txt
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: the colour picker's model and the contrast maths"
```

### Task 9: The pickers on screen — list, colour and command, with the contrast warning

A new SDL-side module, `settings_pickers.c`, draws Task 7's and Task 8's models in the settings column and runs their keys. OK on a picker row opens:
- the **colour picker** for a colour;
- the **list picker** for the default menu, the device and a command.

The colour picker previews the highlighted swatch live, and Back puts the colour back. The command picker lists:
- *None*;
- the navigation and special commands;
- *Open submenu: <name>* for every menu;
- on Windows, `:exit`;
- then every command the menus' entries run, labelled with the entry's title and deduplicated.

The caption warns when a title or clock colour falls below 3:1 contrast against what lies behind it. That is the background colour, or an image's mean luminance, measured when the image is decoded, with the overlay composited over either. Task 10 adds the font picker to the same module, and Task 14 the binding command and capture.

**Files:**
- Create: `src/settings_pickers.h`, `src/settings_pickers.c`
- Modify: `src/settings_screen.c` (a `PickerHost` for the pickers; `handle_event()`, `handle_command()`, `draw_column()`, `draw_caption()`, `settings_open()`, `free_screen()`)
- Modify: `src/launcher.h` (`Slideshow.transition_luminance`; `extern double background_luminance;`), `src/launcher.c` (`reload_background()`, `update_slideshow()`)
- Modify: `src/image.h`, `src/image.c` (`surface_luminance()`; `load_texture_from_file()` measures; the slideshow thread measures)
- Modify: `src/CMakeLists.txt`
- Create: `tests/headless/checks/58-settings-pickers.sh`, `tests/headless/fixtures/f58-pickers.ini`

**Interfaces:**
- Consumes:
  - Task 7's `ListPick`;
  - Task 8's `ColourPick`, `colour_luminance()`, `colour_contrast()`, `colour_over()`, `colour_mean_luminance()`;
  - Task 5's `settings_choose_value()`, `settings_pad_count()`, `settings_pad_name()`;
  - Task 6's `apply_slot()` and `handle_event()`, through the host.
- Produces:

```c
typedef struct {
    SettingsState *model;
    Menu **menus;
    int menu_count;
    void (*apply)(const SettingSlot *slot, bool refresh);          // Put a value into the launcher
    void (*event)(const SettingsEvent *event);                    // Hand an event to the pages
    void (*text)(TTF_Font *font, const char *text, int x, int y, int max_width, Uint8 alpha, bool right);
    int (*row)(const SettingsRow *row, bool highlighted, int x, int y, int width, int note_room);
    TTF_Font *font_row;
    TTF_Font *font_small;
    int row_height;
    int column_width;
    int margin;
} PickerHost;
void pickers_begin(const PickerHost *host);
void pickers_end(void);
bool pickers_active(void);
void pickers_open(SettingSlot *slot);
void pickers_command(const char *command);
void pickers_draw(int x, int top, int bottom);
void pickers_path(char *out, size_t size);
const char *pickers_hint(void);
const char *pickers_note(void);
void contrast_warning(SettingId id, SettingColor color, char *out, size_t size);
// image.h
double surface_luminance(SDL_Surface *surface);   // -1 when it cannot be read
// launcher.h
extern double background_luminance;              // The image on show's mean luminance; -1 unknown
```

- [ ] **Step 1: Write the failing harness checks**

Create `tests/headless/fixtures/f58-pickers.ini`:

```ini
[General]
DefaultMenu=Main

[Main]
Entry1=Games;games;:submenu Games
Entry2=Kodi;apps;kodi --standalone

[Games]
Entry1=One;apps;:quit
```

Create `tests/headless/checks/58-settings-pickers.sh`:

```bash
# The pickers (3b): a colour typed in the hex editor and shown in the preview, the command picker,
# the default menu's list, and the contrast warning

# Background > Colour > Custom: #000000 becomes #102030 one digit at a time, the preview shows it,
# and it saves
shows_hex() { look "$1" "$2" hex 'Settings: previewing #102030' 30,30=16,32,48; }
hex_keys="Down Down Down Down Return Up Right Right Up Up Right Right Up Up Up"
cfg=$(writable_config f60-color)
CFG=$cfg run_keys f58-hex Menu Down Return Down Return $hex_keys +shows_hex Return BackSpace BackSpace
ok=1
grep -qx 'Color=#102030' "$cfg" && grep -q 'Settings: \[Background\] Color #000000 -> #102030' "$out/f58-hex.log" \
    && grep -qx 'hex yes' "$out/f58-hex.seen" && ran_clean f58-hex && ok=0
result "pickers: a colour typed in the hex editor shows in the preview and saves (exit $(cat "$out/f58-hex.code"))" $ok

# Back in the colour picker puts the colour back: nothing is saved
CFG=$FX/f60-color.ini run_keys f58-colourback Menu Down Return Down Return Right Right BackSpace BackSpace BackSpace
ok=1
grep -q 'Settings: previewing #33383D' "$out/f58-colourback.log" && grep -q 'Settings: nothing changed' "$out/f58-colourback.log" \
    && ran_clean f58-colourback && ok=0
result "pickers: Back in the colour picker puts the colour back (exit $(cat "$out/f58-colourback.code"))" $ok

# General > Startup command: the command picker lists None, the special commands, the submenus and
# the entries' commands; Quit StreamFlex is the tenth row. General > Default menu: Games from its list.
cfg=$(writable_config f58-pickers)
CFG=$cfg run_keys f58-command Menu Return Down Down Down Down Down Down Down Down Down Return \
    Down Down Down Down Down Down Down Down Down Return Up Up Up Up Up Up Up Up Up Return Down Return BackSpace BackSpace
ok=1
grep -qx 'StartupCmd=:quit' "$cfg" && grep -qx 'DefaultMenu=Games' "$cfg" \
    && grep -q 'Settings: the command picker lists 16 rows' "$out/f58-command.log" \
    && ran_clean f58-command && ok=0
result "pickers: a command and a default menu chosen from their lists save (exit $(cat "$out/f58-command.code"))" $ok
grep -E 'Settings: (the command picker|\[General\])' "$out/f58-command.log" | sed 's/^/      /'

# Titles > Colour: black titles on the black background are 1:1, and the caption says so
CFG=$FX/f60-color.ini run_keys f58-contrast Menu Down Down Down Return Down Down Down Return Menu
ok=1
grep -q 'Settings: the note under the preview says White #FFFFFF' "$out/f58-contrast.log" \
    && ! grep -q 'Low contrast' "$out/f58-contrast.log" && ran_clean f58-contrast && ok=0
result "pickers: white titles on black raise no contrast warning (exit $(cat "$out/f58-contrast.code"))" $ok
CFG=$FX/f60-color.ini run_keys f58-lowcontrast Menu Down Down Down Return Down Down Down Return Up Up Up Left Left Left Left Menu
ok=1
grep -q 'Settings: the note under the preview says Black #000000 · Low contrast: 1.0:1 against the background' "$out/f58-lowcontrast.log" \
    && ran_clean f58-lowcontrast && ok=0
result "pickers: black titles on black warn of low contrast (exit $(cat "$out/f58-lowcontrast.code"))" $ok
```

The row counts the checks rely on:
- **The command picker, 16 rows in f58-pickers:**
  - None;
  - 12 special commands: Left, Right, Up, Down, OK, Back, Home, Settings, Quit StreamFlex, Shut down, Restart, Sleep;
  - 2 submenus: Main, Games;
  - 1 entry command: `kodi --standalone`. The entries' `:submenu Games` and `:quit` are already listed.

  *Quit StreamFlex* is the tenth row (index 9), and *Startup command* is General's tenth row. Nine `Up`s from it reach *Default menu*.
- **The Titles page:** Size, Show titles, Font, Colour. Colour is its fourth row, so three `Down`s.
- **The colour picker:**
  - It opens on the titles' white, which is swatch 10: row 2, column 5.
  - `Up` reaches row 1, and the next two `Up`s do nothing.
  - Four `Left`s reach Black, swatch 0.
- **The note:** the separator between a colour's name and the warning is ` · ` (U+00B7).

- [ ] **Step 2: Run the harness to see the new checks fail**

Run the headless harness with the label `t9-red`. Expected: the five new checks `FAIL`. OK on a picker row does nothing yet, so nothing is chosen, previewed or warned of. Everything else passes.

- [ ] **Step 3: Measure what lies behind the text: an image's mean luminance**

In `src/image.h`, declare `double surface_luminance(SDL_Surface *surface);` after `load_texture_from_file`. In `src/image.c`, add `#include "colorpick.h"`, and add above `load_texture_from_file()`:

```c
// A function to measure a decoded image's mean relative luminance, for the contrast warning: at
// most 64 points across and down, read through the surface's own format. -1 when it cannot be
// read. It touches only the surface, so the slideshow's thread can use it.
double surface_luminance(SDL_Surface *surface)
{
    enum { SIDE = 64 };
    int across = surface->w < SIDE ? surface->w : SIDE;
    int down = surface->h < SIDE ? surface->h : SIDE;
    if (across <= 0 || down <= 0 || SDL_LockSurface(surface) != 0)
        return -1.0;
    unsigned char samples[SIDE * SIDE * 4];
    int bpp = surface->format->BytesPerPixel;
    for (int j = 0; j < down; j++) {
        int y = (int) ((long long) j * surface->h / down);
        for (int i = 0; i < across; i++) {
            int x = (int) ((long long) i * surface->w / across);
            const Uint8 *p = (const Uint8*) surface->pixels + (long long) y * surface->pitch + (long long) x * bpp;
            Uint32 pixel = 0;
            if (bpp == 1)
                pixel = p[0];
            else if (bpp == 2) {
                Uint16 two;
                memcpy(&two, p, sizeof(two));
                pixel = two;
            }
            else if (bpp == 3)
                pixel = SDL_BYTEORDER == SDL_BIG_ENDIAN ? (Uint32) (p[0] << 16 | p[1] << 8 | p[2])
                                                        : (Uint32) (p[0] | p[1] << 8 | p[2] << 16);
            else
                memcpy(&pixel, p, sizeof(pixel));
            unsigned char *out = &samples[(j * across + i) * 4];
            SDL_GetRGB(pixel, surface->format, &out[0], &out[1], &out[2]);
            out[3] = 0xFF;
        }
    }
    SDL_UnlockSurface(surface);
    return colour_mean_luminance(samples, across, down, across * 4);
}
```

Replace `load_texture_from_file()` with a measured version, and keep the old name for its other callers:

```c
// A function to load a texture from a file, measuring its mean luminance when asked (NULL: not)
SDL_Texture *load_texture_measured(const char *path, double *luminance)
{
    if (luminance != NULL)
        *luminance = -1.0;
    if (path == NULL)
        return NULL;
    SDL_Surface *surface = IMG_Load(path);
    if (surface == NULL) {
        log_error("Could not load image %s\n%s", path, IMG_GetError());
        return NULL;
    }
    if (luminance != NULL)
        *luminance = surface_luminance(surface);
    return load_texture(surface);
}

// A function to load a texture from a file
SDL_Texture *load_texture_from_file(const char *path)
{
    return load_texture_measured(path, NULL);
}
```

Declare `SDL_Texture *load_texture_measured(const char *path, double *luminance);` in `image.h`.

In `load_next_slideshow_background_async()` (image.c line 162), measure the new image on its thread:

```c
    slideshow->transition_surface = load_next_slideshow_background(slideshow, true);
    slideshow->transition_luminance = slideshow->transition_surface != NULL
                                      ? surface_luminance(slideshow->transition_surface) : -1.0;
```

In `src/launcher.h`:
- add `double transition_luminance;   // The next image's mean luminance, measured on the loader thread` to `Slideshow`, after `only_one`;
- add `extern double background_luminance;` after `extern SDL_Texture *background_override;`.

In `src/launcher.c`:
- add the global `double background_luminance = -1.0;   // The image on show's mean luminance, for the contrast warning; -1 unknown` after `background_shown`;
- in `init_slideshow()`'s initializer, add `.transition_luminance = -1.0`.
- In `reload_background()`, set `background_luminance = -1.0;` after destroying the old texture. For an image, load with `background_texture = load_texture_measured(config.background_image, &background_luminance);`. For a slideshow's first image, measure before the texture takes the surface:

```c
            if (surface != NULL) {
                background_luminance = surface_luminance(surface);
                background_texture = load_texture(surface);
                ticks.slideshow_load = ticks.main;
            }
```

- In `update_slideshow()`, where the next image becomes the background, set `background_luminance = slideshow->transition_luminance;`. That happens in two places: the no-fade branch, after `background_texture = load_texture(...)`, and where the fade finishes, after `background_texture = slideshow->transition_texture;`.
- Also in `init_slideshow()`, where a folder with one image shows it, use `background_texture = load_texture_measured(slideshow->images[0], &background_luminance);`.

- [ ] **Step 4: Write `settings_pickers.h`**

Create `src/settings_pickers.h`:

```c
// The settings screen's pickers: the list picker (commands, the default menu, the device) and the
// colour picker, drawn in the settings column in place of a page, and the contrast warning. The
// screen hands them what they draw with and how to apply a value (PickerHost).
#ifndef SETTINGS_PICKERS_H
#define SETTINGS_PICKERS_H

#include <stdbool.h>
#include <stddef.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "launcher.h"
#include "settings.h"

typedef struct {
    SettingsState *model;
    Menu **menus;
    int menu_count;
    void (*apply)(const SettingSlot *slot, bool refresh);          // Put a value into the launcher
    void (*event)(const SettingsEvent *event);                    // Hand an event to the pages
    void (*text)(TTF_Font *font, const char *text, int x, int y, int max_width, Uint8 alpha, bool right);
    int (*row)(const SettingsRow *row, bool highlighted, int x, int y, int width, int note_room);
    TTF_Font *font_row;
    TTF_Font *font_small;
    int row_height;
    int column_width;
    int margin;
} PickerHost;

void pickers_begin(const PickerHost *host);
void pickers_end(void);
bool pickers_active(void);
void pickers_open(SettingSlot *slot);
void pickers_command(const char *command);
void pickers_draw(int x, int top, int bottom);
void pickers_path(char *out, size_t size);
const char *pickers_hint(void);
const char *pickers_note(void);
void contrast_warning(SettingId id, SettingColor color, char *out, size_t size);

#endif
```

- [ ] **Step 5: Write `settings_pickers.c`**

Create `src/settings_pickers.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "launcher.h"
#include <launcher_config.h>
#include "settings.h"
#include "settings_pickers.h"
#include "listpick.h"
#include "colorpick.h"
#include "util.h"
#include "debug.h"

extern Config config;
extern Effective eff;
extern SDL_Renderer *renderer;
extern ModeBackground background_shown;

#define DOT " \xC2\xB7 "       // U+00B7 with a space either side
#define ALPHA_MARK 230         // The outline round the swatch under the cursor

typedef enum {
    PICKER_NONE,
    PICKER_LIST,
    PICKER_COLOUR
} PickerKind;

static PickerHost host;
static PickerKind kind = PICKER_NONE;
static SettingSlot *slot = NULL;        // The setting the picker chooses for
static SettingValue original;           // Its value when the picker opened: Back puts it back
static ListPick *list = NULL;
static int list_first = 0;              // The list's first row on show
static int list_page = 1;               // How many of its rows fit: Left and Right page this far
static ColourPick colour;
static char note[512];                  // What the caption says, built as it is asked for

// The special commands the command picker offers, in its order, after None
static const char *const SPECIALS[] = {
    SCMD_LEFT, SCMD_RIGHT, SCMD_UP, SCMD_DOWN, SCMD_SELECT, SCMD_BACK, SCMD_HOME, SCMD_SETTINGS,
    SCMD_QUIT, SCMD_SHUTDOWN, SCMD_RESTART, SCMD_SLEEP
};

// A function to set the pickers up as settings open
void pickers_begin(const PickerHost *given)
{
    host = *given;
    kind = PICKER_NONE;
}

// A function to close the picker on show, if any
static void close_picker(void)
{
    listpick_free(list);
    list = NULL;
    kind = PICKER_NONE;
    slot = NULL;
    note[0] = '\0';
}

// A function to let go of everything as settings close
void pickers_end(void)
{
    close_picker();
}

// A function to tell whether a picker is open
bool pickers_active(void)
{
    return kind != PICKER_NONE;
}

// A function to add a command to the command picker once, labelled as the screen describes it
static void add_command(const char *command, const char *label)
{
    char text[LISTPICK_TEXT_MAX];
    if (listpick_has(list, command))
        return;
    if (label == NULL) {
        setting_command_label(command, text, sizeof(text));
        label = text;
    }
    listpick_add(list, label, command, true, NULL);
}

// A function to fill the command picker: None, the special commands, each menu as a submenu, on
// Windows the exit hotkey's command, then every command the menus' entries run, by entry title
static void fill_commands(void)
{
    char text[LISTPICK_TEXT_MAX];
    listpick_add(list, "None", "", true, NULL);
    for (size_t i = 0; i < sizeof(SPECIALS) / sizeof(SPECIALS[0]); i++)
        add_command(SPECIALS[i], NULL);
    for (int m = 0; m < host.menu_count; m++) {
        snprintf(text, sizeof(text), SCMD_SUBMENU " %s", host.menus[m]->name);
        add_command(text, NULL);
    }
#ifdef _WIN32
    add_command(SCMD_EXIT, NULL);
#endif
    for (int m = 0; m < host.menu_count; m++) {
        for (Entry *e = host.menus[m]->first_entry; e != NULL; e = e->next)
            add_command(e->cmd, e->title);
    }
    log_debug("Settings: the command picker lists %i rows", listpick_count(list));
}

// A function to write a setting's value as the list picker's value text
static void list_value(const SettingSlot *s, char *out, size_t size)
{
    if (s->def->type == SET_TYPE_DEVICE)
        snprintf(out, size, "%d", s->value.number);
    else
        snprintf(out, size, "%s", s->value.inherit ? "" : s->value.text);
}

// A function to open the list picker for a menu, device or command setting
static void open_list(SettingSlot *s)
{
    char text[LISTPICK_TEXT_MAX];
    list = listpick_create();
    if (list == NULL) {
        log_error("Settings: the list cannot open: out of memory");
        return;
    }
    if (s->def->type == SET_TYPE_MENU) {
        for (int m = 0; m < host.menu_count; m++)
            listpick_add(list, host.menus[m]->name, host.menus[m]->name, true, NULL);
    }
    else if (s->def->type == SET_TYPE_DEVICE) {
        listpick_add(list, "Any", "-1", true, NULL);
        for (int i = 0; i < settings_pad_count(host.model); i++) {
            snprintf(text, sizeof(text), "%d", i);
            listpick_add(list, settings_pad_name(host.model, i), text, true, NULL);
        }
    }
    else
        fill_commands();
    list_value(s, text, sizeof(text));
    char custom[LISTPICK_TEXT_MAX + 16];
    snprintf(custom, sizeof(custom), "Custom: %s", text);
    listpick_select(list, text, custom);
    list_first = 0;
    kind = PICKER_LIST;
}

// A function to open the picker a setting's row asks for
void pickers_open(SettingSlot *s)
{
    close_picker();
    slot = s;
    original = s->value;
    if (s->def->type == SET_TYPE_COLOR) {
        colorpick_open(&colour, s->value.color);
        kind = PICKER_COLOUR;
    }
    else if (s->def->type == SET_TYPE_MENU || s->def->type == SET_TYPE_DEVICE || s->def->type == SET_TYPE_COMMAND)
        open_list(s);
    if (kind != PICKER_NONE)
        log_debug("Settings: opened the picker for [%s] %s", s->def->section, s->def->key);
}

// A function to hand a chosen value to the pages, which apply and log it, and close the picker
static void choose(SettingValue value)
{
    SettingSlot *s = slot;
    s->value = original;   // The colour picker's preview changed it; the choice is made from the original
    close_picker();
    SettingsEvent event = settings_choose_value(host.model, s, &value);
    if (event.kind == SETTINGS_EVENT_NONE)
        host.apply(s, true);   // The same as it was: put back what the preview showed
    else
        host.event(&event);
}

// A function to show a colour in the preview, live, without choosing it
static void preview_colour(SettingColor shown)
{
    slot->value = original;
    slot->value.color = shown;
    host.apply(slot, true);
    log_debug("Settings: previewing #%02X%02X%02X", shown.r, shown.g, shown.b);
}

// A function to act on a key in the list picker
static void list_command(const char *command)
{
    ListPickCommand key;
    if (MATCH(command, SCMD_UP))
        key = LISTPICK_UP;
    else if (MATCH(command, SCMD_DOWN))
        key = LISTPICK_DOWN;
    else if (MATCH(command, SCMD_LEFT))
        key = LISTPICK_PAGE_UP;
    else if (MATCH(command, SCMD_RIGHT))
        key = LISTPICK_PAGE_DOWN;
    else if (MATCH(command, SCMD_SELECT))
        key = LISTPICK_OK;
    else
        key = LISTPICK_BACK;
    ListPickResult result = listpick_command(list, key, list_page);
    if (result == LISTPICK_CANCELLED)
        close_picker();
    else if (result == LISTPICK_CHOSEN) {
        SettingValue value = original;
        const char *chosen = listpick_chosen(list);
        value.inherit = false;
        if (slot->def->type == SET_TYPE_DEVICE)
            value.number = atoi(chosen);
        else {
            snprintf(value.text, sizeof(value.text), "%s", chosen);
            value.inherit = slot->def->can_inherit && chosen[0] == '\0';
        }
        choose(value);
    }
}

// A function to act on a key in the colour picker
static void colour_command(const char *command)
{
    ColourPickCommand key;
    if (MATCH(command, SCMD_UP))
        key = COLORPICK_UP;
    else if (MATCH(command, SCMD_DOWN))
        key = COLORPICK_DOWN;
    else if (MATCH(command, SCMD_LEFT))
        key = COLORPICK_LEFT;
    else if (MATCH(command, SCMD_RIGHT))
        key = COLORPICK_RIGHT;
    else if (MATCH(command, SCMD_SELECT))
        key = COLORPICK_OK;
    else
        key = COLORPICK_BACK;
    ColourPickResult result = colorpick_command(&colour, key);
    if (result == COLORPICK_MOVED)
        preview_colour(colorpick_shown(&colour));
    else if (result == COLORPICK_CANCELLED) {
        slot->value = original;
        host.apply(slot, true);
        log_debug("Settings: the colour picker put [%s] %s back", slot->def->section, slot->def->key);
        close_picker();
    }
    else if (result == COLORPICK_CHOSEN) {
        SettingValue value = original;
        value.color = colour.chosen;
        choose(value);
    }
}

// A function to act on a key while a picker is open. Home and the key that opened settings leave
// the picker as Back does, then close settings as the pages would.
void pickers_command(const char *command)
{
    if (MATCH(command, SCMD_HOME) || MATCH(command, SCMD_SETTINGS)) {
        if (kind == PICKER_COLOUR)
            colour_command(SCMD_BACK);
        else
            close_picker();
        SettingsEvent event = settings_command(host.model, MATCH(command, SCMD_HOME) ? SETTINGS_HOME : SETTINGS_CLOSE);
        host.event(&event);
        return;
    }
    if (!MATCH(command, SCMD_UP) && !MATCH(command, SCMD_DOWN) && !MATCH(command, SCMD_LEFT) &&
        !MATCH(command, SCMD_RIGHT) && !MATCH(command, SCMD_SELECT) && !MATCH(command, SCMD_BACK)) {
        log_debug("Settings: ignoring '%s' while a picker is open", command);
        return;
    }
    if (kind == PICKER_LIST)
        list_command(command);
    else if (kind == PICKER_COLOUR)
        colour_command(command);
}

// A function to draw the list picker's rows, scrolled to keep the cursor in view
static void draw_list(int x, int top, int bottom)
{
    int count = listpick_count(list);
    int cursor = listpick_cursor(list);
    list_page = (bottom - top) / host.row_height > 1 ? (bottom - top) / host.row_height : 1;
    if (cursor < list_first)
        list_first = cursor;
    if (cursor >= list_first + list_page)
        list_first = cursor - list_page + 1;
    int y = top;
    for (int i = list_first; i < count && y + host.row_height <= bottom; i++) {
        const ListPickRow *row = listpick_row(list, i);
        SettingsRow shown;
        memset(&shown, 0, sizeof(shown));
        shown.kind = SETTINGS_ROW_ACTION;
        shown.enabled = row->enabled;
        copy_string(shown.label, row->label, sizeof(shown.label));
        y += host.row(&shown, i == cursor, x, y, host.column_width, 0);
    }
}

// A function to draw the colour picker: the swatches, the current one marked and the one under the
// cursor outlined, then the Custom row, and in the hex editor each digit with the chosen one boxed
static void draw_colour(int x, int top, int bottom)
{
    int pad = host.margin / 2;
    int cell = (host.column_width - 2 * pad) / COLORPICK_COLUMNS;
    int current = colorpick_find(colour.original);
    for (int i = 0; i < COLORPICK_SWATCHES; i++) {
        SettingColor c = colorpick_swatch(i);
        SDL_Rect box = { x + pad + (i % COLORPICK_COLUMNS) * cell + 3, top + (i / COLORPICK_COLUMNS) * cell + 3,
                         cell - 6, cell - 6 };
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 0xFF);
        SDL_RenderFillRect(renderer, &box);
        if (i == current) {
            SDL_Rect mark = { box.x + box.w / 3, box.y + box.h / 3, box.w / 3, box.h / 3 };
            SDL_SetRenderDrawColor(renderer, 0xFF - c.r, 0xFF - c.g, 0xFF - c.b, 0xFF);
            SDL_RenderFillRect(renderer, &mark);
        }
        if (i == colour.cursor) {
            SDL_Rect outline = { box.x - 3, box.y - 3, box.w + 6, box.h + 6 };
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_MARK);
            SDL_RenderDrawRect(renderer, &outline);
            SDL_Rect inner = { outline.x + 1, outline.y + 1, outline.w - 2, outline.h - 2 };
            SDL_RenderDrawRect(renderer, &inner);
        }
    }
    int y = top + COLORPICK_ROWS * cell + pad;
    SettingColor shown = colour.editing || colour.cursor == COLORPICK_CUSTOM ? colour.hex : colour.original;
    SettingsRow custom;
    memset(&custom, 0, sizeof(custom));
    custom.kind = SETTINGS_ROW_ACTION;
    custom.enabled = true;
    snprintf(custom.label, sizeof(custom.label), "Custom");
    snprintf(custom.value, sizeof(custom.value), "#%02X%02X%02X", shown.r, shown.g, shown.b);
    y += host.row(&custom, colour.cursor == COLORPICK_CUSTOM && !colour.editing, x, y, host.column_width, 0);
    if (!colour.editing || y + host.row_height > bottom)
        return;

    // The hex editor: # and six digits in cells as wide as the widest digit, the chosen one boxed
    char digits[8];
    snprintf(digits, sizeof(digits), "%02X%02X%02X", colour.hex.r, colour.hex.g, colour.hex.b);
    int w = 0;
    int h = 0;
    TTF_SizeUTF8(host.font_row, "W", &w, &h);
    int step = w + pad;
    host.text(host.font_row, "#", x + pad, y, step, 255, false);
    for (int i = 0; i < 6; i++) {
        char one[2] = { digits[i], '\0' };
        int cx = x + pad + (i + 1) * step;
        host.text(host.font_row, one, cx, y, step, 255, false);
        if (i == colour.digit) {
            SDL_Rect box = { cx - pad / 2, y, step, h };
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_MARK);
            SDL_RenderDrawRect(renderer, &box);
        }
    }
}

// A function to draw the picker on show in the column, between two heights
void pickers_draw(int x, int top, int bottom)
{
    if (kind == PICKER_LIST)
        draw_list(x, top, bottom);
    else if (kind == PICKER_COLOUR)
        draw_colour(x, top, bottom);
}

// A function to write the column's path line: the page's path, then the setting being chosen
void pickers_path(char *out, size_t size)
{
    settings_path(host.model, out, size);
    size_t used = strlen(out);
    if (slot != NULL)
        snprintf(out + used, size - used, " \xE2\x80\xBA %s", slot->def->label);
}

// A function to give the key hint for the picker on show
const char *pickers_hint(void)
{
    if (kind == PICKER_COLOUR && colour.editing)
        return "Left and right choose a digit" DOT "Up and down change it" DOT "OK keeps" DOT "Back returns";
    if (kind == PICKER_COLOUR)
        return "Arrows move" DOT "OK chooses" DOT "Back cancels";
    return "Left and right page" DOT "OK chooses" DOT "Back cancels";
}

// A function to warn when a title or clock colour stands out too little from what lies behind it:
// the background colour or the image on show (its mean luminance), under the overlay when it is on.
// It says nothing for a transparent background, or an image not yet measured.
void contrast_warning(SettingId id, SettingColor color, char *out, size_t size)
{
    out[0] = '\0';
    if (id != SET_ID_TITLE_COLOR && id != SET_ID_CLOCK_COLOR)
        return;
    double behind;
    if (background_shown == BACKGROUND_COLOR) {
        SettingColor bg = { config.background_color.r, config.background_color.g, config.background_color.b };
        behind = colour_luminance(bg);
    }
    else if ((background_shown == BACKGROUND_IMAGE || background_shown == BACKGROUND_SLIDESHOW) && background_luminance >= 0.0)
        behind = background_luminance;
    else
        return;
    if (config.background_overlay) {
        SettingColor over = { eff.overlay_color.r, eff.overlay_color.g, eff.overlay_color.b };
        behind = colour_over(behind, over, eff.overlay_color.a);
    }
    double ratio = colour_contrast(colour_luminance(color), behind);
    if (ratio < COLORPICK_MIN_CONTRAST)
        snprintf(out, size, "Low contrast: %.1f:1 against the background; 3:1 or more reads well", ratio);
}

// A function to say what the caption says while a picker is open: why OK did nothing in a list; in
// the colour picker, the colour under the cursor, and a contrast warning for it
const char *pickers_note(void)
{
    note[0] = '\0';
    if (kind == PICKER_LIST && listpick_why(list) != NULL)
        snprintf(note, sizeof(note), "%s", listpick_why(list));
    else if (kind == PICKER_COLOUR) {
        SettingColor shown = colorpick_shown(&colour);
        int index = colour.editing ? -1 : colorpick_find(shown);
        char warning[160];
        contrast_warning(slot->def->id, shown, warning, sizeof(warning));
        snprintf(note, sizeof(note), "%s #%02X%02X%02X%s%s", index >= 0 ? colorpick_name(index) : "Custom",
            shown.r, shown.g, shown.b, warning[0] != '\0' ? DOT : "", warning);
    }
    return note;
}
```

Add `settings_pickers.c` and `settings_pickers.h` to `src/CMakeLists.txt`'s `SOURCES`, after `settings_screen.h`.

- [ ] **Step 6: Hand picker rows to the pickers**

In `src/settings_screen.c`:
- Add `#include "settings_pickers.h"` after `#include "settings_screen.h"`, and `#include "colorpick.h"`.
- In `settings_open()`, after `measure_layout();`, set the pickers up:

```c
    PickerHost host = {
        .model = model, .menus = menus, .menu_count = menu_count, .apply = apply_slot, .event = handle_event,
        .text = draw_text, .row = draw_row, .font_row = font_row, .font_small = font_small,
        .row_height = row_height, .column_width = column_width, .margin = margin
    };
    pickers_begin(&host);
```

  `handle_event()` and `apply_slot()` are defined above `settings_open()`. `draw_text()` and `draw_row()` are too.
- In `free_screen()`, call `pickers_end();` before `if (browser != NULL)`.
- In `handle_event()`, replace Task 6's `SETTINGS_EVENT_PICK` case with:

```c
        case SETTINGS_EVENT_PICK:
            pickers_open(event->slot);
            return;
```

- In `handle_command()`, before `if (browser != NULL) {`, route keys to an open picker:

```c
    if (pickers_active()) {
        pickers_command(command);
        return;
    }
```

- In `draw_column()`:
  - the path line: `if (pickers_active()) pickers_path(path, sizeof(path)); else if (browser != NULL) ...`;
  - the rows: `if (pickers_active()) pickers_draw(x, top, hint_y - margin); else if (browser != NULL) draw_browser_rows(...); else draw_model_rows(...);`;
  - the hint: `pickers_active() ? pickers_hint() : browser != NULL ? ... : ...`.
- Add above `draw_caption()`:

```c
// A function to warn in the caption when the row under the cursor is a title or clock colour that
// stands out too little from the background
static const char *row_warning(void)
{
    static char warning[160];
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(model, rows, SETTINGS_MAX_ROWS);
    int cursor = settings_cursor(model);
    warning[0] = '\0';
    if (cursor >= 0 && cursor < count && rows[cursor].slot != NULL && rows[cursor].slot->def->type == SET_TYPE_COLOR)
        contrast_warning(rows[cursor].slot->def->id, rows[cursor].slot->value.color, warning, sizeof(warning));
    return warning;
}
```

- In `draw_caption()`, extend the note:

```c
    const char *note = pickers_active() ? pickers_note()
                     : browser != NULL ? browser_caption()
                     : settings_notice(model)[0] != '\0' ? settings_notice(model)
                     : cursor_why()[0] != '\0' ? cursor_why() : row_warning();
```

- [ ] **Step 7: See the harness pass**

Build and run the unit tests: `100% tests passed`. Run the **Linux unit tests** (label `t9`): `warnings outside src/external: 0`.

Run the headless harness (labels `t9`, `t9-fedora`) and the leak pass (`t9-leaks`, `t9-fedora-leaks`). Expected: `0 failed` in each. The five new checks pass, and every earlier check still does. `f60-color` steps the colour with `Right`, which a picker row still does.

- [ ] **Step 8: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/settings_pickers.h src/settings_pickers.c src/settings_screen.c src/launcher.h src/launcher.c src/image.h src/image.c src/CMakeLists.txt tests/headless/checks/58-settings-pickers.sh tests/headless/fixtures/f58-pickers.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: the colour, command and list pickers, with the contrast warning"
```

### Task 10: The font picker — installed fonts by family, each drawn in its own face

The font picker lists the installed fonts by family, the bundled fonts first. Each row is drawn in its own face, loaded only for the rows on show, from a small cache. The first time it opens:
- a thread lists the font files: Windows' `Fonts` registry keys, or Linux's font folders;
- the main thread then reads each file's faces a few files per frame (SDL_ttf's one FreeType library is not safe to use from two threads);
- meanwhile the picker shows *Loading fonts… (N)*.

The list is kept for the session. Choosing a family writes its Regular face's file (else its first face's), with `FontFace=<index>` when the face is not the first in its file, and removes `FontFace` otherwise.

**Files:**
- Create: `src/fontlist.h`, `src/fontlist.c` (pure)
- Create: `src/fontscan.h`, `src/fontscan.c` (SDL-side, per platform)
- Modify: `src/settings_pickers.h`, `src/settings_pickers.c` (the font picker; `PickerHost.quiet`; `pickers_tick()`; `pickers_quit()`)
- Modify: `src/settings_screen.c` (the `quiet` handler; `pickers_tick()` each frame), `src/launcher.c` (`pickers_quit()` in `cleanup()`)
- Modify: `src/CMakeLists.txt` (sources; `advapi32` on Windows), `tests/CMakeLists.txt`
- Test: `tests/test_fontlist.c`, `tests/test_alloc.c`
- Create: `tests/headless/checks/59-settings-fonts.sh`

**Interfaces:**
- Consumes: Task 7's `ListPick`, Task 9's pickers, and Task 3's `TextInfo.font_path` and `font_face`.
- Produces:

```c
// fontlist.h (pure)
typedef struct FontList FontList;
FontList *fontlist_create(void);
void fontlist_free(FontList *list);
bool fontlist_add(FontList *list, const char *path, int face, const char *family, const char *style, bool bundled);
void fontlist_finish(FontList *list);                 // Sorts the families: bundled first, then by name
int fontlist_count(const FontList *list);
const char *fontlist_family(const FontList *list, int index);
const char *fontlist_path(const FontList *list, int index);   // The face a family writes: Regular, else its first
int fontlist_face(const FontList *list, int index);
int fontlist_find(const FontList *list, const char *path, int face);   // The family holding a face; -1
// fontscan.h (SDL side)
typedef struct FontScan FontScan;
FontScan *fontscan_start(const char *bundled_folder);  // Lists font files on a thread
bool fontscan_done(FontScan *scan);
int fontscan_count(const FontScan *scan);
const char *fontscan_file(const FontScan *scan, int index);
bool fontscan_bundled(const FontScan *scan, int index);
void fontscan_free(FontScan *scan);                    // Waits for the thread
// settings_pickers.h
void pickers_tick(void);    // Each frame: read more faces while the fonts load
void pickers_quit(void);    // At quit: the font list, kept for the session until then
```

  `PickerHost` gains `void (*quiet)(const SettingsEvent *event);`, which logs and stores a change without refreshing. The font picker stores the face with it, then the font with a refresh.

- [ ] **Step 1: Write the failing tests**

Create `tests/test_fontlist.c`:

```c
#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "fontlist.h"

// A function to find a family's place by name
static int family(const FontList *list, const char *name)
{
    for (int i = 0; i < fontlist_count(list); i++) {
        if (strcmp(fontlist_family(list, i), name) == 0)
            return i;
    }
    return -1;
}

// A function to test grouping faces into families and choosing each family's face
static void test_families(void)
{
    FontList *list = fontlist_create();
    CHECK(fontlist_add(list, "/f/DejaVuSans-Bold.ttf", 0, "DejaVu Sans", "Bold", false));
    CHECK(fontlist_add(list, "/f/DejaVuSans.ttf", 0, "DejaVu Sans", "Book", false));
    CHECK(fontlist_add(list, "/f/Noto.ttc", 0, "Noto Sans CJK", "Bold", false));
    CHECK(fontlist_add(list, "/f/Noto.ttc", 3, "Noto Sans CJK", "Regular", false));
    CHECK(fontlist_add(list, "/f/roboto.ttf", 0, "roboto", "regular", false));
    CHECK(fontlist_add(list, "/f/Roboto-Italic.ttf", 0, "Roboto", "Italic", false));
    CHECK(fontlist_add(list, "/app/OpenSans-Regular.ttf", 0, "Open Sans", "Regular", true));
    fontlist_finish(list);

    // One family per name, whatever its case; the bundled font first, then by name
    CHECK_INT(fontlist_count(list), 4);
    CHECK_STR(fontlist_family(list, 0), "Open Sans");
    CHECK_STR(fontlist_family(list, 1), "DejaVu Sans");
    CHECK_STR(fontlist_family(list, 2), "Noto Sans CJK");
    CHECK_INT(family(list, "roboto"), 3);                    // The first spelling seen names it

    // Regular when the family has one (a collection's face 3 here); otherwise its first face
    int noto = family(list, "Noto Sans CJK");
    CHECK_STR(fontlist_path(list, noto), "/f/Noto.ttc");
    CHECK_INT(fontlist_face(list, noto), 3);
    int dejavu = family(list, "DejaVu Sans");
    CHECK_STR(fontlist_path(list, dejavu), "/f/DejaVuSans-Bold.ttf");   // No Regular: the first face
    CHECK_INT(fontlist_face(list, dejavu), 0);
    CHECK_STR(fontlist_path(list, family(list, "roboto")), "/f/roboto.ttf");

    // A face finds its family, whichever face of it is written in the file
    CHECK_INT(fontlist_find(list, "/f/Noto.ttc", 0), noto);
    CHECK_INT(fontlist_find(list, "/f/DejaVuSans.ttf", 0), dejavu);
    CHECK_INT(fontlist_find(list, "/f/Noto.ttc", 7), -1);
    CHECK_INT(fontlist_find(list, "/nowhere.ttf", 0), -1);
    fontlist_free(list);
}

// A function to test an empty list, as a scan that found nothing gives
static void test_empty(void)
{
    FontList *list = fontlist_create();
    fontlist_finish(list);
    CHECK_INT(fontlist_count(list), 0);
    CHECK(fontlist_family(list, 0) == NULL);
    CHECK(fontlist_path(list, 0) == NULL);
    CHECK_INT(fontlist_face(list, 0), 0);
    fontlist_free(list);
    fontlist_free(NULL);
}

int main(void)
{
    test_families();
    test_empty();
    return check_report();
}
```

Add to `tests/CMakeLists.txt`, after `test_colorpick`:

```cmake
# Unit tests for grouping font faces into families (pure)
add_executable(test_fontlist test_fontlist.c "${PROJECT_SOURCE_DIR}/src/fontlist.c" "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_fontlist PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME fontlist COMMAND test_fontlist)
```

and add `"${PROJECT_SOURCE_DIR}/src/fontlist.c"` to `test_alloc`'s sources. In `tests/test_alloc.c`, add `#include "fontlist.h"` and, above `main()`:

```c
// A function to prove that the font list fails cleanly: a face that could not be added is not
// there, and nothing is left allocated
static void prove_fontlist(void)
{
    for (int n = 1;; n++) {
        arm(n);
        FontList *list = fontlist_create();
        bool added = list != NULL && fontlist_add(list, "/f/a.ttf", 0, "A", "Bold", false) &&
                     fontlist_add(list, "/f/a.ttf", 1, "A", "Regular", false) &&
                     fontlist_add(list, "/f/b.ttf", 0, "B", "Regular", true);
        disarm();
        if (list != NULL)
            fontlist_finish(list);
        CHECK_RUN(failed || (added && fontlist_count(list) == 2 && fontlist_face(list, 1) == 1), n);
        fontlist_free(list);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("the font list");
}
```

and call `prove_fontlist();` after `prove_listpick();`.

Create `tests/headless/checks/59-settings-fonts.sh`:

```bash
# The font picker (3b), over a fixture font folder (the harness build's STREAMFLEX_TEST_FONT_DIRS
# stands in for the system's): the bundled fonts are found, a file SDL_ttf cannot open is skipped,
# a family is chosen, and its file is saved with no FontFace for a first face

rm -rf "$TESTER_HOME/fonts"
mkdir -p "$TESTER_HOME/fonts/deeper"
cp /work/assets/fonts/Roboto-Regular.ttf "$TESTER_HOME/fonts/deeper/"
printf 'not a font\n' > "$TESTER_HOME/fonts/broken.ttf"
chown -R tester:tester "$TESTER_HOME/fonts"

# Titles > Font: the list loads, the cursor starts on the font in use (Open Sans, the fifth family),
# and four Ups choose DejaVu Sans, the first
wait_fonts() { wait_line 'Fonts: found' "$2"; sleep 1; }
cfg=$(writable_config f60-color)
STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg UNTIL='Settings saved' \
    run_keys f59-fonts Menu Down Down Down Return Down Down Return +wait_fonts Up Up Up Up Return BackSpace BackSpace
log=$out/f59-fonts.log
ok=1
grep -qE '^Fonts: found 7 families in 9 files, skipped 1 \([0-9]+ ms\)$' "$log" \
    && grep -qE '^Font=/work/build/assets/fonts/DejaVuSans.ttf$' "$cfg" && ! grep -q '^FontFace=' "$cfg" \
    && grep -q 'Titles: opened /work/build/assets/fonts/DejaVuSans.ttf (face 0)' "$log" \
    && ran_clean f59-fonts && ok=0
result "fonts: the picker lists the fonts by family and saves the one chosen (exit $(cat "$out/f59-fonts.code"))" $ok
grep -E '^(Fonts:|Titles: opened)' "$log" | sed 's/^/      /'

# A config whose font file has gone: the picker still opens, the titles use the bundled font, and
# the config's value stays as it was
mkdir -p "$TESTER_HOME/cfg"
cfg=$TESTER_HOME/cfg/f59-gone.ini
printf '[General]\nDefaultMenu=Main\n\n[Titles]\nFont=%s/fonts/gone.ttf\n\n[Main]\nEntry1=One;apps;:quit\n' "$TESTER_HOME" > "$cfg"
chown tester:tester "$cfg"
STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg run_keys f59-gone Menu Down Down Down Return Down Down Return +wait_fonts Menu
ok=1
grep -q "Could not open the font $TESTER_HOME/fonts/gone.ttf (face 0), using the default font" "$out/f59-gone.log" \
    && grep -q 'Fonts: found' "$out/f59-gone.log" && grep -q "Font=$TESTER_HOME/fonts/gone.ttf" "$cfg" \
    && grep -q 'Settings: nothing changed' "$out/f59-gone.log" && ran_clean f59-gone && ok=0
result "fonts: a font file that is gone falls back, and the picker still opens (exit $(cat "$out/f59-gone.code"))" $ok
```

The counts:
- **9 files:** the build's 7 bundled fonts, the fixture's copy of Roboto, and its `broken.ttf`.
- **7 families,** because Roboto's copy joins the bundled Roboto:
  1. DejaVu Sans
  2. FreeSans
  3. Inter
  4. Noto Sans
  5. Open Sans
  6. Roboto
  7. Source Sans Pro

  Every family has a bundled face, so they sort by name.
- **Four `Up`s** from Open Sans (the fifth) reach DejaVu Sans. Its first sighting is the bundled folder's, which the scan lists before the fixture's.

- [ ] **Step 2: Run the tests to see them fail**

Reconfigure (Global Constraints). Expected: the configure fails with `Cannot find source file: .../src/fontlist.c`.

- [ ] **Step 3: Write `fontlist.h` and `fontlist.c`**

Create `src/fontlist.h`:

```c
// The font picker's list: font faces, as a scan found them, grouped into families by name (ignoring
// case), each family writing its Regular face, else the first face seen. The bundled fonts sort
// first, then every family by name. Pure: no SDL, no globals; memory comes from alloc.h.
#ifndef FONTLIST_H
#define FONTLIST_H

#include <stdbool.h>

typedef struct FontList FontList;

FontList *fontlist_create(void);
void fontlist_free(FontList *list);
bool fontlist_add(FontList *list, const char *path, int face, const char *family, const char *style, bool bundled);
void fontlist_finish(FontList *list);
int fontlist_count(const FontList *list);
const char *fontlist_family(const FontList *list, int index);
const char *fontlist_path(const FontList *list, int index);
int fontlist_face(const FontList *list, int index);
int fontlist_find(const FontList *list, const char *path, int face);

#endif
```

Create `src/fontlist.c`:

```c
#include <stdlib.h>
#include <string.h>
#include "fontlist.h"
#include "alloc.h"

typedef struct {
    char *name;
    char *path;       // The face it writes
    int face;
    bool regular;     // That face is its Regular
    bool bundled;     // One of its faces is a bundled font
} Family;

typedef struct {
    char *path;
    int face;
    int family;       // Its family's index, before the sort
} Face;

struct FontList {
    Family *families;
    int family_count;
    int family_capacity;
    Face *faces;
    int face_count;
    int face_capacity;
    int *order;       // After fontlist_finish(): the families in sorted order
};

// A function to lower-case an ASCII letter
static int lower(char c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : (unsigned char) c;
}

// A function to compare two names without regard to ASCII case
static int compare_names(const char *a, const char *b)
{
    while (*a != '\0' && lower(*a) == lower(*b)) {
        a++;
        b++;
    }
    return lower(*a) - lower(*b);
}

// A function to make an empty list; NULL when out of memory
FontList *fontlist_create(void)
{
    return alloc_calloc(1, sizeof(FontList));
}

// A function to free the list
void fontlist_free(FontList *list)
{
    if (list == NULL)
        return;
    for (int i = 0; i < list->family_count; i++) {
        alloc_free(list->families[i].name);
        alloc_free(list->families[i].path);
    }
    for (int i = 0; i < list->face_count; i++)
        alloc_free(list->faces[i].path);
    alloc_free(list->families);
    alloc_free(list->faces);
    alloc_free(list->order);
    alloc_free(list);
}

// A function to make room for one more item in an array
static bool grow(void **items, int *capacity, int count, size_t size)
{
    if (count < *capacity)
        return true;
    int bigger = *capacity ? *capacity * 2 : 32;
    void *more = alloc_realloc(*items, (size_t) bigger * size);
    if (more == NULL)
        return false;
    *items = more;
    *capacity = bigger;
    return true;
}

// A function to add a face: to its family, found by name, or to a new one. A Regular face becomes
// the one its family writes, in place of any other. False when out of memory, with nothing added.
bool fontlist_add(FontList *list, const char *path, int face, const char *family, const char *style, bool bundled)
{
    bool regular = compare_names(style, "Regular") == 0;
    int index = -1;
    for (int i = 0; i < list->family_count && index < 0; i++) {
        if (compare_names(list->families[i].name, family) == 0)
            index = i;
    }
    if (!grow((void**) &list->faces, &list->face_capacity, list->face_count, sizeof(Face)))
        return false;
    char *face_path = alloc_strdup(path);
    char *family_path = alloc_strdup(path);
    char *name = index < 0 ? alloc_strdup(family) : NULL;
    bool ok = face_path != NULL && family_path != NULL && (index >= 0 || name != NULL);
    if (ok && index < 0)
        ok = grow((void**) &list->families, &list->family_capacity, list->family_count, sizeof(Family));
    if (!ok) {
        alloc_free(face_path);
        alloc_free(family_path);
        alloc_free(name);
        return false;
    }
    if (index < 0) {
        index = list->family_count++;
        list->families[index] = (Family) { .name = name, .path = family_path, .face = face, .regular = regular,
                                           .bundled = bundled };
    }
    else {
        Family *f = &list->families[index];
        f->bundled = f->bundled || bundled;
        if (regular && !f->regular) {
            alloc_free(f->path);
            f->path = family_path;
            f->face = face;
            f->regular = true;
        }
        else
            alloc_free(family_path);
    }
    list->faces[list->face_count++] = (Face) { .path = face_path, .face = face, .family = index };
    return true;
}

// The list being sorted, for compare_families (qsort has no context argument)
static const FontList *sorting = NULL;

// A function to order two families: bundled first, then by name
static int compare_families(const void *a, const void *b)
{
    const Family *x = &sorting->families[*(const int*) a];
    const Family *y = &sorting->families[*(const int*) b];
    if (x->bundled != y->bundled)
        return x->bundled ? -1 : 1;
    int by_name = compare_names(x->name, y->name);
    return by_name != 0 ? by_name : *(const int*) a - *(const int*) b;
}

// A function to put the families in order once every face is added. Out of memory, they keep the
// order they were found in.
void fontlist_finish(FontList *list)
{
    alloc_free(list->order);
    list->order = alloc_calloc((size_t) (list->family_count > 0 ? list->family_count : 1), sizeof(int));
    if (list->order == NULL)
        return;
    for (int i = 0; i < list->family_count; i++)
        list->order[i] = i;
    sorting = list;
    qsort(list->order, (size_t) list->family_count, sizeof(int), compare_families);
    sorting = NULL;
}

// A function to count the families
int fontlist_count(const FontList *list)
{
    return list->family_count;
}

// A function to find a family by its place in the sorted list
static const Family *family_at(const FontList *list, int index)
{
    if (index < 0 || index >= list->family_count)
        return NULL;
    return &list->families[list->order != NULL ? list->order[index] : index];
}

// A function to get a family's name
const char *fontlist_family(const FontList *list, int index)
{
    const Family *f = family_at(list, index);
    return f != NULL ? f->name : NULL;
}

// A function to get the file a family writes
const char *fontlist_path(const FontList *list, int index)
{
    const Family *f = family_at(list, index);
    return f != NULL ? f->path : NULL;
}

// A function to get the face within that file
int fontlist_face(const FontList *list, int index)
{
    const Family *f = family_at(list, index);
    return f != NULL ? f->face : 0;
}

// A function to find the family that holds a face, by its place in the sorted list; -1 for none
int fontlist_find(const FontList *list, const char *path, int face)
{
    int family = -1;
    for (int i = 0; i < list->face_count && family < 0; i++) {
        if (list->faces[i].face == face && strcmp(list->faces[i].path, path) == 0)
            family = list->faces[i].family;
    }
    for (int i = 0; family >= 0 && i < list->family_count; i++) {
        if ((list->order != NULL ? list->order[i] : i) == family)
            return i;
    }
    return -1;
}
```

- [ ] **Step 4: Write `fontscan.h` and `fontscan.c`**

Create `src/fontscan.h`:

```c
// Finding the installed font files for the font picker, on a thread of their own: on Windows the
// Fonts registry keys (the machine's and the user's), on Linux the system and user font folders,
// searched folder by folder; and the bundled fonts' folder first on both. The thread only lists
// files: SDL_ttf's one FreeType library must not open fonts on two threads at once, so the main
// thread reads the faces (settings_pickers.c).
#ifndef FONTSCAN_H
#define FONTSCAN_H

#include <stdbool.h>

typedef struct FontScan FontScan;

FontScan *fontscan_start(const char *bundled_folder);
bool fontscan_done(FontScan *scan);
int fontscan_count(const FontScan *scan);
const char *fontscan_file(const FontScan *scan, int index);
bool fontscan_bundled(const FontScan *scan, int index);
void fontscan_free(FontScan *scan);

#endif
```

Create `src/fontscan.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "fontscan.h"
#include "fileio.h"
#include "alloc.h"
#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_DEPTH 8          // Font folders nest (/usr/share/fonts/truetype/dejavu); no deeper than this
#define PATH_BYTES 2048

struct FontScan {
    SDL_Thread *thread;
    SDL_atomic_t done;
    char *bundled_folder;
    char **files;
    bool *bundled;
    int count;
    int capacity;
};

// A function to lower-case an ASCII letter
static char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char) (c - 'A' + 'a') : c;
}

// A function to tell a font file by its extension: TrueType or OpenType, single or a collection
static bool is_font_file(const char *name)
{
    static const char *const extensions[] = { ".ttf", ".otf", ".ttc", ".otc" };
    size_t length = strlen(name);
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        if (length <= 4)
            continue;
        size_t k = 0;
        while (k < 4 && lower(name[length - 4 + k]) == extensions[i][k])
            k++;
        if (k == 4)
            return true;
    }
    return false;
}

// A function to add a file to the list; a file already listed (by path) is left as it is
static void add_file(FontScan *scan, const char *path, bool bundled)
{
    for (int i = 0; i < scan->count; i++) {
        if (strcmp(scan->files[i], path) == 0)
            return;
    }
    if (scan->count == scan->capacity) {
        int capacity = scan->capacity ? scan->capacity * 2 : 256;
        char **files = alloc_realloc(scan->files, (size_t) capacity * sizeof(char*));
        if (files == NULL)
            return;
        scan->files = files;
        bool *flags = alloc_realloc(scan->bundled, (size_t) capacity * sizeof(bool));
        if (flags == NULL)
            return;
        scan->bundled = flags;
        scan->capacity = capacity;
    }
    char *copy = alloc_strdup(path);
    if (copy == NULL)
        return;
    scan->files[scan->count] = copy;
    scan->bundled[scan->count] = bundled;
    scan->count++;
}

// A function to list the font files in a folder and the folders under it. A folder that cannot be
// listed is skipped, and so is anything hidden.
static void scan_folder(FontScan *scan, const char *folder, bool bundled, int depth)
{
    FileioEntry *entries = NULL;
    int count = fileio_list(folder, &entries);
    char path[PATH_BYTES];
    for (int i = 0; i < count; i++) {
        if (entries[i].hidden)
            continue;
        size_t length = strlen(folder);
        bool slash = length > 0 && (folder[length - 1] == '/' || folder[length - 1] == '\\');
        snprintf(path, sizeof(path), "%s%s%s", folder, slash ? "" : "/", entries[i].name);
        if (entries[i].is_dir && depth < MAX_DEPTH)
            scan_folder(scan, path, bundled, depth + 1);
        else if (!entries[i].is_dir && is_font_file(entries[i].name))
            add_file(scan, path, bundled);
    }
    fileio_free_list(entries, count > 0 ? count : 0);
}

#ifdef _WIN32
// A function to turn a UTF-16 string into UTF-8, into a buffer; false when it does not fit
static bool to_utf8(const wchar_t *wide, char *out, int size)
{
    return WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, size, NULL, NULL) > 0;
}

// A function to list the fonts one Fonts registry key names. A value's data is a file: a bare name
// is in Windows' own Fonts folder, and a full path (a font installed for one user) is used as it is.
static void scan_registry(FontScan *scan, HKEY root, const char *fonts_folder)
{
    HKEY key;
    if (RegOpenKeyExW(root, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return;
    wchar_t name[512];
    wchar_t data[1024];
    char file[PATH_BYTES];
    char path[PATH_BYTES];
    for (DWORD i = 0;; i++) {
        DWORD name_size = (DWORD) (sizeof(name) / sizeof(name[0]));
        DWORD data_size = (DWORD) sizeof(data) - sizeof(wchar_t);
        DWORD type = 0;
        LONG result = RegEnumValueW(key, i, name, &name_size, NULL, &type, (LPBYTE) data, &data_size);
        if (result == ERROR_NO_MORE_ITEMS)
            break;
        if (result != ERROR_SUCCESS || type != REG_SZ)
            continue;
        data[data_size / sizeof(wchar_t)] = L'\0';
        if (!to_utf8(data, file, (int) sizeof(file)) || !is_font_file(file))
            continue;
        bool full = (file[0] != '\0' && file[1] == ':') || (file[0] == '\\' && file[1] == '\\');
        if (full)
            snprintf(path, sizeof(path), "%s", file);
        else
            snprintf(path, sizeof(path), "%s\\%s", fonts_folder, file);
        add_file(scan, path, false);
    }
    RegCloseKey(key);
}
#endif

// A function run on the scan's thread: list every font file. It logs nothing (output_log() is the
// main thread's) and opens no font (SDL_ttf's FreeType library is the main thread's too).
static int scan_thread(void *data)
{
    FontScan *scan = data;
    if (scan->bundled_folder != NULL)
        scan_folder(scan, scan->bundled_folder, true, 0);
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_FONT_DIRS (colon-separated) stands in
    // for the system's font folders
    const char *dirs = getenv("STREAMFLEX_TEST_FONT_DIRS");
    if (dirs != NULL) {
        char copy[PATH_BYTES];
        snprintf(copy, sizeof(copy), "%s", dirs);
        for (char *dir = strtok(copy, ":"); dir != NULL; dir = strtok(NULL, ":"))
            scan_folder(scan, dir, false, 0);
        SDL_AtomicSet(&scan->done, 1);
        return 0;
    }
#endif
#ifdef _WIN32
    char windows[MAX_PATH + 1];
    UINT length = GetWindowsDirectoryA(windows, (UINT) sizeof(windows));
    char fonts_folder[PATH_BYTES];
    snprintf(fonts_folder, sizeof(fonts_folder), "%s\\Fonts", length > 0 && length < sizeof(windows) ? windows : "C:\\Windows");
    scan_registry(scan, HKEY_LOCAL_MACHINE, fonts_folder);
    scan_registry(scan, HKEY_CURRENT_USER, fonts_folder);
#else
    scan_folder(scan, "/usr/share/fonts", false, 0);
    scan_folder(scan, "/usr/local/share/fonts", false, 0);
    const char *home = getenv("HOME");
    if (home != NULL) {
        char path[PATH_BYTES];
        snprintf(path, sizeof(path), "%s/.local/share/fonts", home);
        scan_folder(scan, path, false, 0);
        snprintf(path, sizeof(path), "%s/.fonts", home);
        scan_folder(scan, path, false, 0);
    }
#endif
    SDL_AtomicSet(&scan->done, 1);
    return 0;
}

// A function to start listing the font files on a thread; NULL when it cannot start
FontScan *fontscan_start(const char *bundled_folder)
{
    FontScan *scan = alloc_calloc(1, sizeof(FontScan));
    if (scan == NULL)
        return NULL;
    scan->bundled_folder = bundled_folder != NULL ? alloc_strdup(bundled_folder) : NULL;
    SDL_AtomicSet(&scan->done, 0);
    scan->thread = SDL_CreateThread(scan_thread, "Font scan", scan);
    if (scan->thread == NULL) {
        fontscan_free(scan);
        return NULL;
    }
    return scan;
}

// A function to tell whether the list is complete
bool fontscan_done(FontScan *scan)
{
    return SDL_AtomicGet(&scan->done) != 0;
}

// A function to count the files found; only once the scan is done
int fontscan_count(const FontScan *scan)
{
    return scan->count;
}

// A function to get a file found
const char *fontscan_file(const FontScan *scan, int index)
{
    return scan->files[index];
}

// A function to tell whether a file is one of the bundled fonts
bool fontscan_bundled(const FontScan *scan, int index)
{
    return scan->bundled[index];
}

// A function to free the scan, waiting for its thread
void fontscan_free(FontScan *scan)
{
    if (scan == NULL)
        return;
    if (scan->thread != NULL)
        SDL_WaitThread(scan->thread, NULL);
    for (int i = 0; i < scan->count; i++)
        alloc_free(scan->files[i]);
    alloc_free(scan->files);
    alloc_free(scan->bundled);
    alloc_free(scan->bundled_folder);
    alloc_free(scan);
}
```

In `src/CMakeLists.txt`:
- add `fontlist.c`, `fontlist.h`, `fontscan.c` and `fontscan.h` to `SOURCES`, after `colorpick.h`;
- add `advapi32` to the Windows link list, after `uuid` (the registry's API).

- [ ] **Step 5: Add the font picker to the pickers**

In `src/settings_pickers.h`, add `void (*quiet)(const SettingsEvent *event);   // Log and store a change, without refreshing` to `PickerHost` after `event`, and declare:

```c
void pickers_tick(void);
void pickers_quit(void);
```

In `src/settings_pickers.c`:
- add `#include "fontlist.h"`, `#include "fontscan.h"` and `#include "image.h"`, and `extern TextInfo title_info;` and `extern Clock *clk;` after the other externs, with `#include "clock.h"`;
- add `PICKER_FONT` to `PickerKind`;
- add the font state after the colour picker's:

```c
#define FONT_SAMPLE_SIZE_RATIO 1.0F    // A family's row is drawn at the row font's size
#define FONT_FILES_PER_FRAME 6         // Files read per frame while the list loads
#define FONT_CACHE_SIZE 32             // Families drawn in their own face, kept while on show

static FontScan *font_scan = NULL;     // The listing thread, until its files are read
static FontList *fonts = NULL;         // The families, kept for the session once read
static int font_files_read = 0;
static int font_files_skipped = 0;
static Uint32 font_scan_start = 0;
static bool fonts_ready = false;
static struct {
    char *path;
    int face;
    SDL_Texture *texture;   // The family's name drawn in its own face
    int w;
    int h;
    Uint32 used;
} font_cache[FONT_CACHE_SIZE];
static Uint32 font_clock = 0;
```

Add the scan's reading and the picker's filling:

```c
// A function to find the bundled fonts' folder: beside the executable, else where the packages put it
static char *bundled_fonts_folder(void)
{
    char *font = find_default_font(FILENAME_DEFAULT_FONT);
    if (font == NULL)
        return NULL;
    size_t length = strlen(font);
    while (length > 0 && font[length - 1] != '/' && font[length - 1] != '\\')
        length--;
    font[length > 0 ? length - 1 : 0] = '\0';
    return font;
}

// A function to read one file's faces into the font list, on the main thread: every face with a
// family name, and a glyph for "Aa0" (symbol and emoji fonts have none), is added
static void read_font_file(const char *path, bool bundled)
{
    TTF_Font *font = TTF_OpenFontIndex(path, 12, 0);
    if (font == NULL) {
        font_files_skipped++;
        return;
    }
    long faces = TTF_FontFaces(font);
    TTF_CloseFont(font);
    for (long i = 0; i < faces && i < 64; i++) {
        TTF_Font *face = TTF_OpenFontIndex(path, 12, i);
        if (face == NULL)
            continue;
        const char *family = TTF_FontFaceFamilyName(face);
        const char *style = TTF_FontFaceStyleName(face);
        if (family != NULL && TTF_GlyphIsProvided(face, 'A') && TTF_GlyphIsProvided(face, 'a') && TTF_GlyphIsProvided(face, '0'))
            fontlist_add(fonts, path, (int) i, family, style != NULL ? style : "", bundled);
        TTF_CloseFont(face);
    }
}

// A function to encode a family's face as the list picker's value: "<face>|<path>"
static void font_value(const char *path, int face, char *out, size_t size)
{
    snprintf(out, size, "%d|%s", face, path);
}

// A function to name the font a setting uses now, into `out`: the file the titles or the clock
// opened; for a clock that is off, the one it would open. "" when there is none.
static void font_in_use(const SettingSlot *s, char *out, size_t size, int *face)
{
    const char *path = NULL;
    char *clock_default = NULL;
    *face = 0;
    if (s->def->id == SET_ID_TITLE_FONT) {
        path = title_info.font_path;
        *face = title_info.font_face;
    }
    else if (clk != NULL) {
        path = clk->text_info.font_path;
        *face = clk->text_info.font_face;
    }
    else if (config.clock_font_path != NULL) {
        path = config.clock_font_path;
        *face = config.clock_font_face;
    }
    else
        path = clock_default = find_default_font(FILENAME_DEFAULT_CLOCK_FONT);
    snprintf(out, size, "%s", path != NULL ? path : "");
    free(clock_default);
}

// A function to fill the font picker from the font list, the cursor on the font in use
static void fill_fonts(void)
{
    char value[LISTPICK_TEXT_MAX];
    for (int i = 0; i < fontlist_count(fonts); i++) {
        font_value(fontlist_path(fonts, i), fontlist_face(fonts, i), value, sizeof(value));
        listpick_add(list, fontlist_family(fonts, i), value, true, NULL);
    }
    char path[LISTPICK_TEXT_MAX];
    int face = 0;
    font_in_use(slot, path, sizeof(path), &face);
    int family = fontlist_find(fonts, path, face);
    if (family >= 0)
        font_value(fontlist_path(fonts, family), fontlist_face(fonts, family), value, sizeof(value));
    else
        font_value(path, face, value, sizeof(value));
    // A font in use that the scan did not find (a file outside the font folders) is pinned by name
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *base = backslash != NULL && (slash == NULL || backslash > slash) ? backslash + 1
                     : slash != NULL ? slash + 1 : path;
    char custom[LISTPICK_TEXT_MAX + 16];
    snprintf(custom, sizeof(custom), "Custom: %s", base);
    listpick_select(list, value, custom);
}

// A function to read more of the font files each frame while the list loads, then fill the picker
void pickers_tick(void)
{
    if (font_scan == NULL || !fontscan_done(font_scan))
        return;
    int total = fontscan_count(font_scan);
    for (int n = 0; n < FONT_FILES_PER_FRAME && font_files_read < total; n++, font_files_read++)
        read_font_file(fontscan_file(font_scan, font_files_read), fontscan_bundled(font_scan, font_files_read));
    if (font_files_read < total)
        return;
    fontlist_finish(fonts);
    log_debug("Fonts: found %i families in %i files, skipped %i (%u ms)", fontlist_count(fonts), total,
        font_files_skipped, SDL_GetTicks() - font_scan_start);
    fontscan_free(font_scan);
    font_scan = NULL;
    fonts_ready = true;
    if (kind == PICKER_FONT)
        fill_fonts();
}

// A function to open the font picker: the list at once when it was read before, else the scan
static void open_fonts_picker(void)
{
    list = listpick_create();
    if (list == NULL) {
        log_error("Settings: the font picker cannot open: out of memory");
        return;
    }
    kind = PICKER_FONT;
    list_first = 0;
    if (fonts_ready) {
        fill_fonts();
        return;
    }
    if (font_scan != NULL)
        return;
    fonts = fontlist_create();
    char *folder = bundled_fonts_folder();
    font_scan_start = SDL_GetTicks();
    font_files_read = 0;
    font_files_skipped = 0;
    font_scan = fonts != NULL ? fontscan_start(folder) : NULL;
    free(folder);
    if (font_scan == NULL)
        log_error("Settings: the fonts cannot be listed: out of memory, or no thread");
}
```

The per-frame file budget is what bounds each frame's time, and a font that is slow to open costs one frame.

In `pickers_open()`, add the font branch before the list types:

```c
    else if (s->def->type == SET_TYPE_FONT)
        open_fonts_picker();
```

`PICKER_FONT` shares the list picker's keys. In `pickers_command()`, `if (kind == PICKER_LIST)` becomes `if (kind == PICKER_LIST || kind == PICKER_FONT)`. While the list loads, `listpick_command()` on an empty list does nothing but cancel on Back.

In `list_command()`'s `LISTPICK_CHOSEN` branch, choose a font's two settings. Its face is stored quietly first, then its file with the refresh that opens it:

```c
    else if (result == LISTPICK_CHOSEN && kind == PICKER_FONT) {
        const char *chosen = listpick_chosen(list);
        const char *bar = strchr(chosen, '|');
        int face = atoi(chosen);
        SettingValue font = original;
        snprintf(font.text, sizeof(font.text), "%s", bar != NULL ? bar + 1 : chosen);
        SettingId face_id = slot->def->id == SET_ID_TITLE_FONT ? SET_ID_TITLE_FONT_FACE : SET_ID_CLOCK_FONT_FACE;
        SettingSlot *face_slot = settings_slot(host.model, face_id, -1);
        SettingValue face_value = face_slot->value;
        face_value.inherit = face == 0;
        face_value.number = face;
        SettingsEvent quiet = settings_choose_value(host.model, face_slot, &face_value);
        host.quiet(&quiet);
        if (quiet.kind != SETTINGS_EVENT_NONE && strcmp(font.text, original.text) == 0) {
            // The same file, another face: the font's own value did not change, so refresh here
            SettingSlot *s = slot;
            close_picker();
            host.apply(s, true);
            return;
        }
        choose(font);
    }
```

  Put this branch first, and make the existing `LISTPICK_CHOSEN` branch `else if (result == LISTPICK_CHOSEN)`. `choose()` hands the font's change to the pages, which store it and run *the title font* (or *the clock*) group once.

Draw the font picker, each family in its own face:

```c
// A function to get a family's name drawn in its own face, from the cache; NULL when the face cannot
// be opened (the row then draws in the settings' font)
static SDL_Texture *font_sample(const char *path, int face, const char *name, int *w, int *h)
{
    font_clock++;
    int oldest = 0;
    for (int i = 0; i < FONT_CACHE_SIZE; i++) {
        if (font_cache[i].path != NULL && font_cache[i].face == face && strcmp(font_cache[i].path, path) == 0) {
            font_cache[i].used = font_clock;
            *w = font_cache[i].w;
            *h = font_cache[i].h;
            return font_cache[i].texture;
        }
        if (font_cache[i].used < font_cache[oldest].used)
            oldest = i;
    }
    if (font_cache[oldest].texture != NULL)
        SDL_DestroyTexture(font_cache[oldest].texture);
    free(font_cache[oldest].path);
    memset(&font_cache[oldest], 0, sizeof(font_cache[oldest]));
    TTF_Font *font = TTF_OpenFontIndex(path, TTF_FontHeight(host.font_row) * 7 / 10, face);
    if (font == NULL)
        return NULL;
    SDL_Color white = { 0xFF, 0xFF, 0xFF, 0xFF };
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, name, white);
    TTF_CloseFont(font);
    if (surface == NULL)
        return NULL;
    font_cache[oldest].texture = SDL_CreateTextureFromSurface(renderer, surface);
    font_cache[oldest].w = surface->w;
    font_cache[oldest].h = surface->h;
    SDL_FreeSurface(surface);
    font_cache[oldest].path = strdup(path);
    font_cache[oldest].face = face;
    font_cache[oldest].used = font_clock;
    *w = font_cache[oldest].w;
    *h = font_cache[oldest].h;
    return font_cache[oldest].texture;
}

// A function to empty the family samples' cache
static void clear_font_cache(void)
{
    for (int i = 0; i < FONT_CACHE_SIZE; i++) {
        if (font_cache[i].texture != NULL)
            SDL_DestroyTexture(font_cache[i].texture);
        free(font_cache[i].path);
    }
    memset(font_cache, 0, sizeof(font_cache));
}

// A function to draw the font picker: "Loading fonts... (N)" while the files are read, then each
// family's row, its name drawn in its own face over an empty row's highlight
static void draw_fonts(int x, int top, int bottom)
{
    if (!fonts_ready) {
        char text[64];
        snprintf(text, sizeof(text), "Loading fonts\xE2\x80\xA6 (%i)", font_files_read);
        host.text(host.font_row, text, x + host.margin / 2, top, host.column_width - host.margin, 255, false);
        return;
    }
    int count = listpick_count(list);
    int cursor = listpick_cursor(list);
    list_page = (bottom - top) / host.row_height > 1 ? (bottom - top) / host.row_height : 1;
    if (cursor < list_first)
        list_first = cursor;
    if (cursor >= list_first + list_page)
        list_first = cursor - list_page + 1;
    int y = top;
    for (int i = list_first; i < count && y + host.row_height <= bottom; i++) {
        const ListPickRow *row = listpick_row(list, i);
        SettingsRow shown;
        memset(&shown, 0, sizeof(shown));
        shown.kind = SETTINGS_ROW_ACTION;
        shown.enabled = true;
        const char *bar = strchr(row->value, '|');
        int w = 0;
        int h = 0;
        SDL_Texture *sample = !row->custom && bar != NULL ? font_sample(bar + 1, atoi(row->value), row->label, &w, &h) : NULL;
        if (sample == NULL)
            copy_string(shown.label, row->label, sizeof(shown.label));
        host.row(&shown, i == cursor, x, y, host.column_width, 0);
        if (sample != NULL) {
            int room = host.column_width - host.margin;
            SDL_Rect rect = { x + host.margin / 2, y + (host.row_height - h) / 2, w < room ? w : room, h };
            SDL_Rect from = { 0, 0, rect.w, h };
            SDL_RenderCopy(renderer, sample, &from, &rect);
        }
        y += host.row_height;
    }
}
```

In `pickers_draw()`, add `else if (kind == PICKER_FONT) draw_fonts(x, top, bottom);`. In `close_picker()`, call `clear_font_cache();`: the samples are drawn again next time.

Add the session's end:

```c
// A function to let go of the font list and its scan at quit: they are kept while the launcher runs
void pickers_quit(void)
{
    close_picker();
    fontscan_free(font_scan);
    font_scan = NULL;
    fontlist_free(fonts);
    fonts = NULL;
    fonts_ready = false;
}
```

In `pickers_hint()`, a font picker still loading says so: `if (kind == PICKER_FONT && !fonts_ready) return "Back cancels";`.

- [ ] **Step 6: Wire the quiet store, the tick and the quit**

In `src/settings_screen.c`:
- add, above `settings_open()`:

```c
// A function to log and store a change without refreshing: the font picker's face, which the
// font's own change refreshes with it
static void handle_quiet(const SettingsEvent *event)
{
    if (event->kind != SETTINGS_EVENT_CHANGED)
        return;
    log_change(event->slot, &event->before);
    apply_slot(event->slot, false);
}
```

- add `.quiet = handle_quiet,` to the `PickerHost` in `settings_open()`;
- call `pickers_tick();` at the top of `settings_draw()`, after `poll_decode(false);`.

In `src/launcher.c`'s `cleanup()`, call `pickers_quit();` right after `settings_close_now();`, with `#include "settings_pickers.h"`. The font scan's thread is waited for there, before SDL quits.

- [ ] **Step 7: See the tests and the harness pass**

Reconfigure, build and run the unit tests: `100% tests passed`, with `fontlist` and `alloc` among them. Run the **Linux unit tests** (label `t10`): `warnings outside src/external: 0`.

Run the headless harness (labels `t10`, `t10-fedora`) and the leak pass (`t10-leaks`, `t10-fedora-leaks`). Expected: `0 failed`. The two font checks pass, and the leak pass shows the font list, the scan and the samples freed at quit.

- [ ] **Step 8: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/fontlist.h src/fontlist.c src/fontscan.h src/fontscan.c src/settings_pickers.h src/settings_pickers.c src/settings_screen.c src/launcher.c src/CMakeLists.txt tests/test_fontlist.c tests/test_alloc.c tests/CMakeLists.txt tests/headless/checks/59-settings-fonts.sh
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: the font picker: installed fonts by family, each in its own face"
```

### Task 11: List mode in `inidoc`

Settings follow inih's rule that the last key wins. `[Hotkeys]` ignores its key names, though, and `[Gamepad]` allows a label twice: every line is one binding. So `inidoc` gains a list mode that addresses lines, not keys. It can:
- list a section's key lines in order (leaving out the settings kept in the same section);
- find a line by its text;
- set a line;
- add a line after the section's last key;
- remove a line.

Comments, order and line endings survive, as in 3a. A continuation line (indented, after a key) is never listed. It moves with its key: removing a key removes its continuation lines, as `inidoc_remove()` does. Giving a line another key is refused when continuation lines follow it: they would start setting that other key.

**Files:**
- Modify: `src/inidoc.h`, `src/inidoc.c`
- Test: `tests/test_inidoc.c`, `tests/test_alloc.c`

**Interfaces:**
- Consumes: 3a's `inidoc` internals (`Line`, `read_line()`, `insert_line()`, `remove_line()`, `remove_continuations()`, `end_of_key()`, `add_section()`, `count_keys()`, `find_header()`, `same_section()`, `inidoc_check()`, `too_long_in_line()`).
- Produces:

```c
typedef struct {
    int line;           // Its index in the document, for inidoc_list_set() and inidoc_list_remove()
    const char *key;
    const char *value;  // As inih reads it
    const char *text;   // The whole line as written, without its line ending
} IniDocItem;
int inidoc_list(const IniDoc *doc, const char *section, const char *const *skip, IniDocItem *items, int max);
int inidoc_find_line(const IniDoc *doc, const char *section, const char *text);
bool inidoc_list_set(IniDoc *doc, int line, const char *key, const char *value);
bool inidoc_list_add(IniDoc *doc, const char *section, const char *key, const char *value);
bool inidoc_list_remove(IniDoc *doc, int line);
```

  An item's pointers stay valid until the document next changes.

- [ ] **Step 1: Write the failing tests**

Add to `tests/test_inidoc.c`, above `main()`:

```c
static const char *const GAMEPAD_SETTINGS[] = { "Enabled", "DeviceIndex", "ControllerMappingsFile", NULL };

// A function to parse a text, failing the check when it does not parse
static IniDoc *parse(const char *text)
{
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK(doc != NULL);
    return doc;
}

// A function to write a document out for comparing (a static buffer)
static const char *written(const IniDoc *doc)
{
    static char text[4096];
    char *out = inidoc_serialize(doc, NULL);
    snprintf(text, sizeof(text), "%s", out != NULL ? out : "");
    free(out);
    return text;
}

// A function to test listing a section's lines: in order, duplicates kept, settings and
// continuations left out
static void test_list(void)
{
    IniDoc *doc = parse("[Gamepad]\nEnabled=true\nButtonA=:select ; confirm\n  :back\nButtonA=:up\n; a comment\n"
                        "DeviceIndex=0\n\n[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey1=#40000045;:settings\n[Main]\n"
                        "Entry1=One;apps;:quit\n");
    IniDocItem items[8];
    int count = inidoc_list(doc, "Gamepad", GAMEPAD_SETTINGS, items, 8);
    CHECK_INT(count, 2);
    CHECK_STR(items[0].key, "ButtonA");
    CHECK_STR(items[0].value, ":select");
    CHECK_STR(items[0].text, "ButtonA=:select ; confirm");
    CHECK_STR(items[1].value, ":up");
    count = inidoc_list(doc, "Hotkeys", NULL, items, 8);
    CHECK_INT(count, 2);                                    // One key name, two bindings: inih reads both
    CHECK_STR(items[1].value, "#40000045;:settings");
    CHECK_INT(inidoc_list(doc, "Hotkeys", NULL, items, 1), 2);   // The count, though only one fits
    CHECK_INT(inidoc_list(doc, "Nowhere", NULL, items, 8), 0);
    CHECK_INT(inidoc_find_line(doc, "Hotkeys", "Hotkey1=#40000045;:settings"), items[1].line);
    CHECK_INT(inidoc_find_line(doc, "Hotkeys", "Hotkey1=#40000045;:quit"), -1);
    CHECK_INT(inidoc_find_line(doc, "Gamepad", "  :back"), -1);  // A continuation is not a line of the list
    inidoc_free(doc);
}

// A function to test setting a line: the same key keeps its spacing and comment, and another key
// is written in its place; only the line asked for changes, though another has the same key
static void test_list_set(void)
{
    IniDoc *doc = parse("[Gamepad]\nButtonA = :select ; confirm\nButtonA=:up\n");
    IniDocItem items[4];
    inidoc_list(doc, "Gamepad", GAMEPAD_SETTINGS, items, 4);
    CHECK(inidoc_list_set(doc, items[1].line, "ButtonA", ":down"));
    CHECK_STR(written(doc), "[Gamepad]\nButtonA = :select ; confirm\nButtonA=:down\n");
    CHECK(inidoc_list_set(doc, items[0].line, "ButtonA", ":home"));
    CHECK_STR(written(doc), "[Gamepad]\nButtonA = :home ; confirm\nButtonA=:down\n");
    CHECK(inidoc_list_set(doc, items[0].line, "ButtonB", ":back"));
    CHECK_STR(written(doc), "[Gamepad]\nButtonB=:back ; confirm\nButtonA=:down\n");
    inidoc_free(doc);

    // Refused: not a key line; a value config.ini cannot hold; another key over continuation lines
    doc = parse("[Hotkeys]\n; keys\nHotkey1=#4000003A;:quit\n  #40000045;:home\n");
    CHECK(!inidoc_list_set(doc, 1, "Hotkey1", "#4000003A;:up"));
    CHECK_STR(inidoc_why(doc), "the line is not a key");
    CHECK(!inidoc_list_set(doc, 2, "Hotkey1", " ;x"));
    CHECK(!inidoc_list_set(doc, 2, "Hotkey9", "#4000003A;:up"));
    CHECK_STR(inidoc_why(doc), "it would change how other lines read");
    CHECK(inidoc_list_set(doc, 2, "Hotkey1", "#4000003A;:up"));   // The same key: its continuation stays its own
    CHECK_STR(written(doc), "[Hotkeys]\n; keys\nHotkey1=#4000003A;:up\n  #40000045;:home\n");
    inidoc_free(doc);
}

// A function to test adding a line: after the section's last key (before a comment or blank line
// that follows it), under the header of a section with no keys, or in a new section at the end
static void test_list_add(void)
{
    IniDoc *doc = parse("[Hotkeys]\r\nHotkey1=#4000003A;:quit\r\n\r\n; next\r\n[Main]\r\nEntry1=One;apps;:quit\r\n");
    CHECK(inidoc_list_add(doc, "Hotkeys", "Hotkey2", "#40000045;:home"));
    CHECK_STR(written(doc), "[Hotkeys]\r\nHotkey1=#4000003A;:quit\r\nHotkey2=#40000045;:home\r\n\r\n; next\r\n"
                            "[Main]\r\nEntry1=One;apps;:quit\r\n");
    inidoc_free(doc);

    doc = parse("[Gamepad]\n; none yet\n");
    CHECK(inidoc_list_add(doc, "Gamepad", "ButtonA", ":select"));
    CHECK(inidoc_list_add(doc, "Gamepad", "ButtonA", ":up"));        // A label twice is allowed
    CHECK_STR(written(doc), "[Gamepad]\nButtonA=:select\nButtonA=:up\n; none yet\n");
    inidoc_free(doc);

    doc = parse("[Main]\nEntry1=One;apps;:quit");
    CHECK(inidoc_list_add(doc, "Hotkeys", "Hotkey1", "#4000003A;:quit"));
    CHECK_STR(written(doc), "[Main]\nEntry1=One;apps;:quit\n\n[Hotkeys]\nHotkey1=#4000003A;:quit");
    CHECK(!inidoc_list_add(doc, "Hotkeys", "Hotkey2", "x\ny"));
    inidoc_free(doc);
}

// A function to test removing a line: only it, with its continuation lines, and the file's last
// line keeping its missing line ending
static void test_list_remove(void)
{
    IniDoc *doc = parse("[Gamepad]\nButtonA=:select\n  :back\n; keep\nButtonA=:up");
    IniDocItem items[4];
    inidoc_list(doc, "Gamepad", GAMEPAD_SETTINGS, items, 4);
    CHECK(inidoc_list_remove(doc, items[0].line));
    CHECK_STR(written(doc), "[Gamepad]\n; keep\nButtonA=:up");
    inidoc_list(doc, "Gamepad", GAMEPAD_SETTINGS, items, 4);
    CHECK(inidoc_list_remove(doc, items[0].line));
    CHECK_STR(written(doc), "[Gamepad]\n; keep");
    CHECK(!inidoc_list_remove(doc, 1));                   // A comment is not a key line
    CHECK(!inidoc_list_remove(doc, 7));
    inidoc_free(doc);
}
```

and call them from `main()`: `test_list(); test_list_set(); test_list_add(); test_list_remove();`. `test_inidoc.c` already includes `<stdio.h>`, `<stdlib.h>` and `<string.h>`.

In `tests/test_alloc.c`, add above `main()`:

```c
// A function to prove that list mode's add and set fail cleanly: refused, saying "out of memory",
// with the document as it was
static void prove_list_mode(void)
{
    static const char *const text = "[Hotkeys]\nHotkey1=#4000003A;:quit\n\n[Main]\nEntry1=One;apps;:quit\n";
    for (int n = 1;; n++) {
        IniDoc *doc = inidoc_parse(text, strlen(text));
        IniDocItem items[2];
        inidoc_list(doc, "Hotkeys", NULL, items, 2);
        arm(n);
        bool set = inidoc_list_set(doc, items[0].line, "Hotkey1", "#40000045;:home");
        bool added = set && inidoc_list_add(doc, "Hotkeys", "Hotkey2", "#4000003A;:up");
        disarm();
        char *out = text_of(doc);
        if (!failed)
            CHECK_RUN(set && added && strstr(out, "Hotkey1=#40000045;:home\nHotkey2=#4000003A;:up\n") != NULL, n);
        else
            CHECK_RUN(strcmp(inidoc_why(doc), "out of memory") == 0, n);
        alloc_free(out);
        inidoc_free(doc);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("list mode");
}
```

and call `prove_list_mode();` after `prove_set(...)`'s last call.

- [ ] **Step 2: Run the tests to see them fail**

Build (Global Constraints). Expected: `'IniDocItem': undeclared identifier` and `implicit declaration of function 'inidoc_list'` (or MSVC's wording).

- [ ] **Step 3: Declare list mode**

In `src/inidoc.h`, after the `IniDocPlacement` enum, add:

```c
// A line of a list section ([Hotkeys], [Gamepad]), where every key line counts and a key may repeat
typedef struct {
    int line;           // Its index in the document, for inidoc_list_set() and inidoc_list_remove()
    const char *key;
    const char *value;  // As inih reads it
    const char *text;   // The whole line as written, without its line ending
} IniDocItem;
```

and after `inidoc_check_in`'s declaration:

```c
int inidoc_list(const IniDoc *doc, const char *section, const char *const *skip, IniDocItem *items, int max);
int inidoc_find_line(const IniDoc *doc, const char *section, const char *text);
bool inidoc_list_set(IniDoc *doc, int line, const char *key, const char *value);
bool inidoc_list_add(IniDoc *doc, const char *section, const char *key, const char *value);
bool inidoc_list_remove(IniDoc *doc, int line);
```

- [ ] **Step 4: Write list mode**

In `src/inidoc.c`, add `static const char *const NOT_A_KEY = "the line is not a key";` with the other reasons, and before `inidoc_free()`:

```c
// A function to tell whether a key is one a list leaves out (the settings its section also holds)
static bool skipped(const char *key, const char *const *skip)
{
    for (int i = 0; skip != NULL && skip[i] != NULL; i++) {
        if (strcmp(skip[i], key) == 0)
            return true;
    }
    return false;
}

// A function to list a section's key lines in order, every one (a key may repeat), leaving out the
// keys in `skip` (NULL-terminated; NULL for none). It fills at most `max` items and returns how many
// there are. Continuation lines are not listed: they move with their key.
int inidoc_list(const IniDoc *doc, const char *section, const char *const *skip, IniDocItem *items, int max)
{
    int count = 0;
    for (int i = 0; i < doc->count; i++) {
        const Line *line = &doc->lines[i];
        if (line->kind != LINE_KEY || !same_section(section_name(doc, line->section), section) || skipped(line->name, skip))
            continue;
        if (count < max)
            items[count] = (IniDocItem) { .line = i, .key = line->name, .value = line->value, .text = line->text };
        count++;
    }
    return count;
}

// A function to find a section's key line by its whole text; -1 when no line reads so
int inidoc_find_line(const IniDoc *doc, const char *section, const char *text)
{
    size_t length = strlen(text);
    for (int i = 0; i < doc->count; i++) {
        const Line *line = &doc->lines[i];
        if (line->kind == LINE_KEY && line->length == length && memcmp(line->text, text, length) == 0 &&
            same_section(section_name(doc, line->section), section))
            return i;
    }
    return -1;
}

// A function to tell whether a key line has continuation lines after it
static bool has_continuations(const IniDoc *doc, int i)
{
    return end_of_key(doc, i) != i;
}

// A function to set one line of a list section. The same key keeps the line's spacing and trailing
// comment; another key is written as key=value with the comment kept. The line must read back as
// written and stay a key of its own; another key over continuation lines is refused, since they
// would start setting it.
bool inidoc_list_set(IniDoc *doc, int line, const char *key, const char *value)
{
    doc->why = "";
    if (line < 0 || line >= doc->count || doc->lines[line].kind != LINE_KEY)
        return refuse(doc, NOT_A_KEY);
    const char *reason = inidoc_check(key, value);
    if (reason != NULL)
        return refuse(doc, reason);
    Line *old = &doc->lines[line];
    bool same_key = strcmp(old->name, key) == 0;
    if (!same_key && has_continuations(doc, line))
        return refuse(doc, NO_SAFE_PLACE);
    size_t tail = old->value_start + old->value_length;
    size_t head = same_key ? old->value_start : strlen(key) + 1;
    size_t value_length = strlen(value);
    size_t new_length = head + value_length + (old->length - tail);
    if (new_length > INIDOC_MAX_LINE)
        return refuse(doc, memchr(old->text + tail, ';', old->length - tail) != NULL ? TOO_LONG_WITH_COMMENT : TOO_LONG_WITH_SPACING);
    Line updated = *old;   // Keeps the line ending; read_line() replaces the name and value
    updated.text = alloc_malloc(new_length + 1);
    if (updated.text == NULL)
        return refuse(doc, OUT_OF_MEMORY);
    if (same_key)
        memcpy(updated.text, old->text, head);
    else {
        memcpy(updated.text, key, head - 1);
        updated.text[head - 1] = '=';
    }
    memcpy(updated.text + head, value, value_length);
    memcpy(updated.text + head + value_length, old->text + tail, old->length - tail);
    updated.text[new_length] = '\0';
    updated.length = new_length;
    if (!read_line(&updated)) {
        alloc_free(updated.text);
        return refuse(doc, OUT_OF_MEMORY);
    }
    Line before = *old;
    *old = updated;
    classify_all(doc);
    const Line *now = &doc->lines[line];
    if (now->kind != LINE_KEY || strcmp(now->name, key) != 0 || strcmp(now->value, value) != 0) {
        free_line(&doc->lines[line]);
        doc->lines[line] = before;
        classify_all(doc);
        return refuse(doc, NO_SAFE_PLACE);
    }
    free_line(&before);
    return true;
}

// A function to prove a list line just inserted at `at`: exactly one more key, reading as written,
// with no line after it that inih would read as its value
static bool list_line_proven(const IniDoc *doc, int at, const char *key, const char *value, int keys_before)
{
    const Line *line = &doc->lines[at];
    return count_keys(doc) == keys_before + 1 && line->kind == LINE_KEY && strcmp(line->name, key) == 0 &&
           strcmp(line->value, value) == 0 && end_of_key(doc, at) == at;
}

// A function to add a line to a list section: after its last key and that key's continuation lines,
// else (no key yet) under its header, else at the end of the section; a missing section is added at
// the end of the file. Refused only when no place is safe.
bool inidoc_list_add(IniDoc *doc, const char *section, const char *key, const char *value)
{
    doc->why = "";
    const char *reason = inidoc_check(key, value);
    if (reason != NULL)
        return refuse(doc, reason);
    size_t size = strlen(key) + strlen(value) + 2;
    char *text = alloc_malloc(size);
    if (text == NULL)
        return refuse(doc, OUT_OF_MEMORY);
    snprintf(text, size, "%s=%s", key, value);
    int header = find_header(doc, section);
    bool ok = false;
    if (header < 0)
        ok = add_section(doc, text, section, key, value);
    else {
        int after_last_key = -1;
        for (int i = 0; i < doc->count; i++) {
            const Line *line = &doc->lines[i];
            if (line->kind == LINE_KEY && same_section(section_name(doc, line->section), section))
                after_last_key = end_of_key(doc, i) + 1;
        }
        int section_end = header + 1;
        while (section_end < doc->count && doc->lines[section_end].kind != LINE_SECTION)
            section_end++;
        int places[2] = { after_last_key >= 0 ? after_last_key : header + 1, section_end };
        for (int i = 0; i < 2 && !ok && doc->why != OUT_OF_MEMORY; i++) {
            if (i > 0 && places[i] == places[0])
                break;
            int keys_before = count_keys(doc);
            if (!insert_line(doc, places[i], text))
                refuse(doc, OUT_OF_MEMORY);
            else if (list_line_proven(doc, places[i], key, value, keys_before))
                ok = true;
            else {
                remove_line(doc, places[i]);
                refuse(doc, NO_SAFE_PLACE);
            }
        }
    }
    alloc_free(text);
    if (ok)
        doc->why = "";
    return ok;
}

// A function to remove one line of a list section, with its continuation lines, so none is left for
// inih to read as the value of the key before it
bool inidoc_list_remove(IniDoc *doc, int line)
{
    if (line < 0 || line >= doc->count || doc->lines[line].kind != LINE_KEY || doc->lines[line].name[0] == '\0')
        return false;
    remove_continuations(doc, line);
    remove_line(doc, line);
    return true;
}
```

`add_section()` proves a new section's line with `inidoc_get()`, which finds the last line with that key. In a section it has just made, that is the new line. The empty-key rule is `inidoc_remove()`'s own: nothing continues a key with an empty name.

- [ ] **Step 5: Run the tests to see them pass**

Build and run the unit tests: `100% tests passed`, with `inidoc` and `alloc` among them. Run the **Linux unit tests** (label `t11`): `warnings outside src/external: 0`.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/inidoc.h src/inidoc.c tests/test_inidoc.c tests/test_alloc.c
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: list mode in inidoc, for [Hotkeys] and [Gamepad]"
```

### Task 12: The save writes list edits, and keeps a hand edit made meanwhile

`config_save` gains list edits. The save reads the file fresh, as 3a's does, and each edited binding carries the line's text as it was when settings opened:
- a change or removal finds that line in the fresh file;
- when a hand edit changed or removed that line meanwhile, a change is written as a new line and a removal is skipped, each with a note the screen logs;
- additions are always written.

A new hotkey's `HotkeyN` is one above the highest N in the fresh file, so it never reuses a number a hand edit took.

**Files:**
- Modify: `src/config_save.h`, `src/config_save.c`
- Test: `tests/test_config_save.c`, `tests/test_alloc.c`

**Interfaces:**
- Consumes: Task 11's `inidoc_list()`, `inidoc_find_line()`, `inidoc_list_set()`, `inidoc_list_add()`, `inidoc_list_remove()`.
- Produces:

```c
typedef enum { CONFIG_LIST_SET, CONFIG_LIST_ADD, CONFIG_LIST_REMOVE } ConfigListOp;
typedef struct {
    ConfigListOp op;
    const char *section;
    const char *original;   // SET, REMOVE: the line as settings read it
    const char *key;        // SET, ADD: the key, or with `numbered` its stem ("Hotkey")
    bool numbered;          // Number the key one above the highest <key>N in the section
    const char *value;      // SET, ADD
} ConfigListEdit;
bool config_save_all(const char *loaded, const char *system_prefix, const char *user_config,
                     const ConfigEdit *edits, int count, const ConfigListEdit *lists, int list_count,
                     ConfigSaveResult *result);
// ConfigSaveResult gains: char notes[1024];   // One line per list edit the save could not make as asked
```

  `config_save()` stays, as `config_save_all()` with no list edits.

- [ ] **Step 1: Write the failing tests**

Add to `tests/test_config_save.c`, above `main()`, using its `reset()` and `holds()` helpers:

```c
// A function to test list edits: a set, a removal and two additions. The additions are numbered
// after the highest HotkeyN the file holds once the removal is made.
static void test_list_edits(void)
{
    reset(CONFIG, "[General]\nDefaultMenu=Main\n\n[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey4=#4000003B;:home\n"
                  "; end\n\n[Main]\nEntry1=One;apps;:quit\n");
    ConfigListEdit lists[] = {
        { CONFIG_LIST_SET, "Hotkeys", "Hotkey1=#4000003A;:quit", "Hotkey", true, "#4000003A;:settings" },
        { CONFIG_LIST_REMOVE, "Hotkeys", "Hotkey4=#4000003B;:home", NULL, false, NULL },
        { CONFIG_LIST_ADD, "Hotkeys", NULL, "Hotkey", true, "#4000003C;:back" },
        { CONFIG_LIST_ADD, "Hotkeys", NULL, "Hotkey", true, "#4000003D;:up" }
    };
    ConfigSaveResult result;
    CHECK(config_save_all(CONFIG, NULL, NULL, NULL, 0, lists, 4, &result));
    CHECK(holds(CONFIG, "[General]\nDefaultMenu=Main\n\n[Hotkeys]\nHotkey1=#4000003A;:settings\nHotkey2=#4000003C;:back\n"
                        "Hotkey3=#4000003D;:up\n; end\n\n[Main]\nEntry1=One;apps;:quit\n"));
    CHECK_STR(result.notes, "");
}

// A function to test list edits after a hand edit made while settings were open (Review Focus 4).
// Settings read Hotkey1, Hotkey2 and Hotkey3; by the save, a hand edit changed Hotkey2, removed
// Hotkey3 and added Hotkey7. The change is written as a new line and the removal skipped, each with
// a note, and every new line is numbered after 7.
static void test_list_edits_after_hand_edit(void)
{
    reset(CONFIG, "[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey2=#4000003B;:back ; changed by hand\n"
                  "Hotkey7=#40000040;:sleep\n");
    ConfigListEdit lists[] = {
        { CONFIG_LIST_SET, "Hotkeys", "Hotkey2=#4000003B;:home", "Hotkey", true, "#4000003B;:up" },
        { CONFIG_LIST_REMOVE, "Hotkeys", "Hotkey3=#4000003C;:quit", NULL, false, NULL },
        { CONFIG_LIST_ADD, "Hotkeys", NULL, "Hotkey", true, "#4000003D;:down" }
    };
    ConfigSaveResult result;
    CHECK(config_save_all(CONFIG, NULL, NULL, NULL, 0, lists, 3, &result));
    CHECK(holds(CONFIG, "[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey2=#4000003B;:back ; changed by hand\n"
                        "Hotkey7=#40000040;:sleep\nHotkey8=#4000003B;:up\nHotkey9=#4000003D;:down\n"));
    CHECK(strstr(result.notes, "'Hotkey2=#4000003B;:home' changed meanwhile, so its change is written as a new line") != NULL);
    CHECK(strstr(result.notes, "'Hotkey3=#4000003C;:quit' is not there any more, so its removal is skipped") != NULL);

    // A gamepad control keeps its label, with no number; a line that cannot be written fails the
    // whole save, and nothing is written
    reset(CONFIG, "[Gamepad]\nButtonA=:select\n");
    ConfigListEdit pad[] = {
        { CONFIG_LIST_SET, "Gamepad", "ButtonA=:select", "ButtonB", false, ":back" },
        { CONFIG_LIST_ADD, "Gamepad", NULL, "ButtonA", false, "; not a command" }
    };
    CHECK(!config_save_all(CONFIG, NULL, NULL, NULL, 0, pad, 2, &result));
    CHECK(strstr(result.why, "cannot be written") != NULL);
    CHECK(holds(CONFIG, "[Gamepad]\nButtonA=:select\n"));
}
```

and call both from `main()`.

A `SET` of a hotkey passes the stem `"Hotkey"` with `numbered`. A line that is found keeps its own key (`Hotkey1`). A line that has gone becomes a new, numbered one.

In `tests/test_alloc.c`, add after `prove_config_save()`:

```c
// A function to prove that a save with list edits fails cleanly: refused with a reason and the
// config as it was, or done in full
static void prove_config_save_lists(void)
{
    static const char *const before = "[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey2=#4000003B;:home\n";
    static const char *const after = "[Hotkeys]\nHotkey1=#4000003A;:settings\nHotkey2=#4000003C;:back\n";
    ConfigListEdit lists[] = {
        { CONFIG_LIST_SET, "Hotkeys", "Hotkey1=#4000003A;:quit", "Hotkey", true, "#4000003A;:settings" },
        { CONFIG_LIST_REMOVE, "Hotkeys", "Hotkey2=#4000003B;:home", NULL, false, NULL },
        { CONFIG_LIST_ADD, "Hotkeys", NULL, "Hotkey", true, "#4000003C;:back" }
    };
    CHECK(fileio_make_dirs(DIR));
    for (int n = 1;; n++) {
        fileio_remove(CONFIG ".tmp");
        fileio_remove(CONFIG ".bak.tmp");
        CHECK(fileio_write_all(CONFIG, before, strlen(before)));
        ConfigSaveResult result;
        arm(n);
        bool ok = config_save_all(CONFIG, NULL, NULL, NULL, 0, lists, 3, &result);
        disarm();
        if (ok)
            CHECK_RUN(holds(CONFIG, after), n);
        else
            CHECK_RUN(failed && result.why[0] != '\0' && holds(CONFIG, before), n);
        CHECK_RUN(!fileio_exists(CONFIG ".tmp"), n);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("saving list edits");
}
```

and call `prove_config_save_lists();` after `prove_config_save();`.

- [ ] **Step 2: Run the tests to see them fail**

Build. Expected: `'ConfigListEdit': undeclared identifier` and `implicit declaration of function 'config_save_all'`.

- [ ] **Step 3: Declare the list edits**

In `src/config_save.h`, add after `ConfigEdit`:

```c
typedef enum {
    CONFIG_LIST_SET,       // Change the line that read `original` when settings opened
    CONFIG_LIST_ADD,       // Add a line after the section's last
    CONFIG_LIST_REMOVE     // Remove the line that read `original`
} ConfigListOp;

// One edit to a list section ([Hotkeys], [Gamepad]), whose lines are found by their text, not their key
typedef struct {
    ConfigListOp op;
    const char *section;
    const char *original;   // SET, REMOVE: the line as settings read it
    const char *key;        // SET, ADD: the key, or with `numbered` its stem ("Hotkey")
    bool numbered;          // Number the key one above the highest <key>N in the section
    const char *value;      // SET, ADD
} ConfigListEdit;
```

Add `char notes[1024];                  // One line per list edit that could not be made as asked; "" when none` to `ConfigSaveResult`, and declare after `config_save()`:

```c
bool config_save_all(const char *loaded, const char *system_prefix, const char *user_config,
                     const ConfigEdit *edits, int count, const ConfigListEdit *lists, int list_count,
                     ConfigSaveResult *result);
```

- [ ] **Step 4: Write the list edits**

In `src/config_save.c`, add before `config_save()`:

```c
// A function to add a note to the result, one per line
static void add_note(ConfigSaveResult *result, const char *format, const char *line)
{
    size_t used = strlen(result->notes);
    if (used > 0 && used + 1 < sizeof(result->notes))
        result->notes[used++] = '\n';
    snprintf(result->notes + used, sizeof(result->notes) - used, format, line);
}

// A function to number a key one above the highest <stem>N the section holds (Hotkey1 and Hotkey4
// give Hotkey5), into `out`; false when out of memory
static bool next_key(const IniDoc *doc, const char *section, const char *stem, char *out, size_t size)
{
    int count = inidoc_list(doc, section, NULL, NULL, 0);
    IniDocItem *items = alloc_calloc((size_t) (count > 0 ? count : 1), sizeof(IniDocItem));
    if (items == NULL)
        return false;
    inidoc_list(doc, section, NULL, items, count);
    size_t stem_length = strlen(stem);
    long highest = 0;
    for (int i = 0; i < count; i++) {
        const char *digits = items[i].key + stem_length;
        if (strncmp(items[i].key, stem, stem_length) != 0 || *digits == '\0' || strspn(digits, "0123456789") != strlen(digits))
            continue;
        long n = strtol(digits, NULL, 10);
        if (n > highest)
            highest = n;
    }
    alloc_free(items);
    snprintf(out, size, "%s%ld", stem, highest + 1);
    return true;
}

// A function to add one list line: its key numbered when asked; false (with the reason) when it
// cannot be written
static bool add_list_line(IniDoc *doc, const ConfigListEdit *edit, ConfigSaveResult *result)
{
    char key[128];
    if (!edit->numbered)
        snprintf(key, sizeof(key), "%s", edit->key);
    else if (!next_key(doc, edit->section, edit->key, key, sizeof(key))) {
        snprintf(result->why, sizeof(result->why), "out of memory");
        return false;
    }
    if (inidoc_list_add(doc, edit->section, key, edit->value))
        return true;
    snprintf(result->why, sizeof(result->why), "the %s value in [%s] cannot be written: %s", key, edit->section, inidoc_why(doc));
    return false;
}

// A function to apply the list edits to the fresh file: changes and removals first, found by the line
// each had when settings opened, then the additions. A line a hand edit changed or removed meanwhile
// takes its change as a new line, and its removal is skipped; both say so in the notes.
static bool apply_list_edits(IniDoc *doc, const ConfigListEdit *lists, int count, ConfigSaveResult *result)
{
    for (int i = 0; i < count; i++) {
        const ConfigListEdit *edit = &lists[i];
        if (edit->op == CONFIG_LIST_ADD)
            continue;
        int line = inidoc_find_line(doc, edit->section, edit->original);
        if (edit->op == CONFIG_LIST_REMOVE) {
            if (line >= 0)
                inidoc_list_remove(doc, line);
            else
                add_note(result, "'%s' is not there any more, so its removal is skipped", edit->original);
            continue;
        }
        if (line < 0) {
            add_note(result, "'%s' changed meanwhile, so its change is written as a new line", edit->original);
            if (!add_list_line(doc, edit, result))
                return false;
            continue;
        }
        const char *key = edit->key;
        char numbered[128];
        if (edit->numbered) {
            // A set keeps the line's own key: its number is already the line's
            int count_items = inidoc_list(doc, edit->section, NULL, NULL, 0);
            IniDocItem *items = alloc_calloc((size_t) (count_items > 0 ? count_items : 1), sizeof(IniDocItem));
            if (items == NULL) {
                snprintf(result->why, sizeof(result->why), "out of memory");
                return false;
            }
            inidoc_list(doc, edit->section, NULL, items, count_items);
            for (int k = 0; k < count_items; k++) {
                if (items[k].line == line)
                    snprintf(numbered, sizeof(numbered), "%s", items[k].key);
            }
            alloc_free(items);
            key = numbered;
        }
        if (!inidoc_list_set(doc, line, key, edit->value)) {
            snprintf(result->why, sizeof(result->why), "the %s value in [%s] cannot be written: %s", key,
                edit->section, inidoc_why(doc));
            return false;
        }
    }
    for (int i = 0; i < count; i++) {
        if (lists[i].op == CONFIG_LIST_ADD && !add_list_line(doc, &lists[i], result))
            return false;
    }
    return true;
}
```

Rename `config_save()` to `config_save_all()`, with the two list parameters after `count`. Where it applies the edits, apply the lists after them:

```c
    if (!apply_edits(doc, edits, count, result) || !apply_list_edits(doc, lists, list_count, result)) {
        inidoc_free(doc);
        return false;
    }
```

and add the old name back as a wrapper:

```c
// A function to save the settings screen's changes to single keys only (see config_save_all)
bool config_save(const char *loaded, const char *system_prefix, const char *user_config,
                 const ConfigEdit *edits, int count, ConfigSaveResult *result)
{
    return config_save_all(loaded, system_prefix, user_config, edits, count, NULL, 0, result);
}
```

A `SET`'s `numbered` flag matters only when its line has gone and its change becomes a new line. That new line is numbered after the highest in the fresh file (`Hotkey8` in the test), as any new hotkey is.

- [ ] **Step 5: Run the tests to see them pass**

Build and run the unit tests: `100% tests passed`, with `config_save` and `alloc` among them. Run the **Linux unit tests** (label `t12`): `warnings outside src/external: 0`.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/config_save.h src/config_save.c tests/test_config_save.c tests/test_alloc.c
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: the save writes list edits, and keeps a hand edit made meanwhile"
```

### Task 13: The bindings model — lists, the safety floor, confirmation and capture (`bindings.c`)

A pure model of the keyboard's hotkeys and the gamepad's controls as the file's lines hold them. It provides:
- **the lists:** add, change, remove and discard;
- **the list edits the save writes:** `HotkeyN` numbered by the save;
- **the safety floor:**
  - the arrows, OK and Back always keep their built-in meaning, so a hotkey on Left, Right, Return or Backspace is refused;
  - no change may leave a navigation command or `:settings` with no key or button at all;
  - binding Up, Down or a Menu key to anything else takes that key's navigation away, so it must be confirmed within 10 s or it reverts;
- **the capture state machine:** it ignores the press that started it, and that key's repeats and release, and gives up after 5 s.

The floor counts each command's working bindings: the keyboard's built-in keys, and the gamepad's default controls, as `util.c` adds them.

**Files:**
- Create: `src/bindings.h`, `src/bindings.c`
- Modify: `tests/CMakeLists.txt` (`test_bindings`; `bindings.c` joins `test_alloc`)
- Test: `tests/test_bindings.c`, `tests/test_alloc.c`

**Interfaces:**
- Consumes: `IniDocItem` (`inidoc.h`) and `ConfigListEdit` (`config_save.h`), both pure; `launcher_config.h`'s gamepad label names.
- Produces:

```c
#define BINDINGS_COMMAND_MAX 1024
#define BINDINGS_VALUE_MAX (BINDINGS_COMMAND_MAX + 16)
#define BINDINGS_LABELS 25
#define BINDINGS_CAPTURE_MS 5000
#define BINDINGS_CONFIRM_MS 10000
#define BIND_KEY_UNKNOWN 0          // SDL keycodes by value (bindings.c is pure; settings_pickers.c checks them)
#define BIND_KEY_BACKSPACE 0x08
#define BIND_KEY_RETURN 0x0D
#define BIND_KEY_RIGHT 0x4000004F
#define BIND_KEY_LEFT 0x40000050
#define BIND_KEY_DOWN 0x40000051
#define BIND_KEY_UP 0x40000052
#define BIND_KEY_F1 0x4000003A
#define BIND_KEY_F12 0x40000045
#define BIND_KEY_F13 0x40000068
#define BIND_KEY_F24 0x40000073
#define BIND_KEY_APPLICATION 0x40000065
#define BIND_KEY_MENU 0x40000076
typedef enum { BINDINGS_KEYBOARD, BINDINGS_GAMEPAD } BindingsDevice;
typedef struct {
    int code;                            // Keyboard: the keycode; gamepad: the label's index
    char command[BINDINGS_COMMAND_MAX];
    char key[64];                        // The line's key ("Hotkey3", "ButtonA"); "" for a new binding
    char original[256];                  // The line as settings read it; "" for a new binding
    bool removed;
} Binding;
typedef struct Bindings Bindings;
Bindings *bindings_create(bool windows, bool gamepad_on);
void bindings_free(Bindings *bindings);
bool bindings_load(Bindings *bindings, BindingsDevice device, const IniDocItem *items, int count);
int bindings_count(const Bindings *bindings, BindingsDevice device);
const Binding *bindings_at(const Bindings *bindings, BindingsDevice device, int index);
const char *bindings_label(int index);
int bindings_label_index(const char *label);
const char *bindings_refuse_key(const Bindings *bindings, BindingsDevice device, int code, const char *command);
const char *bindings_refuse_change(const Bindings *bindings, BindingsDevice device, int index, int code,
                                   const char *command, bool remove);
bool bindings_takes_navigation(BindingsDevice device, int code, const char *command);
int bindings_set(Bindings *bindings, BindingsDevice device, int index, int code, const char *command);
void bindings_remove(Bindings *bindings, BindingsDevice device, int index);
bool bindings_changed(const Bindings *bindings);
void bindings_discard(Bindings *bindings);
int bindings_edits(const Bindings *bindings, BindingsDevice device, ConfigListEdit *edits,
                   char (*values)[BINDINGS_VALUE_MAX], int max);
typedef enum { CAPTURE_IDLE, CAPTURE_LISTENING, CAPTURE_CAPTURED, CAPTURE_TIMED_OUT } CaptureState;
typedef struct { CaptureState state; unsigned int started; int starting_code; bool starting_held; int code; } Capture;
void capture_begin(Capture *capture, unsigned int now, int starting_code);
bool capture_press(Capture *capture, unsigned int now, int code, bool repeat);    // true: captured
void capture_release(Capture *capture, int code);
bool capture_expired(Capture *capture, unsigned int now);                         // true: timed out now
typedef struct { bool active; unsigned int started; int code; } Probation;
void probation_begin(Probation *probation, unsigned int now, int code);
bool probation_press(Probation *probation, int code);                             // true: confirmed
bool probation_expired(Probation *probation, unsigned int now);                   // true: revert now
```

  A keyboard binding's line value is `#<HEX>;<command>`, the hex uppercase, as the sample config writes it. A gamepad binding's key is its label, and its value is the command.

- [ ] **Step 1: Write the failing tests**

Create `tests/test_bindings.c`:

```c
#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "bindings.h"

// A function to load a device's lines from text, as settings do from the fresh file
static Bindings *loaded(bool windows, const char *hotkeys, const char *gamepad)
{
    Bindings *b = bindings_create(windows, true);
    IniDocItem items[16];
    int count = 0;
    char text[2048];
    snprintf(text, sizeof(text), "[Hotkeys]\n%s\n[Gamepad]\nEnabled=true\n%s\n", hotkeys, gamepad);
    IniDoc *doc = inidoc_parse(text, strlen(text));
    static const char *const skip[] = { "Enabled", "DeviceIndex", "ControllerMappingsFile", NULL };
    count = inidoc_list(doc, "Hotkeys", NULL, items, 16);
    CHECK(bindings_load(b, BINDINGS_KEYBOARD, items, count));
    count = inidoc_list(doc, "Gamepad", skip, items, 16);
    CHECK(bindings_load(b, BINDINGS_GAMEPAD, items, count));
    inidoc_free(doc);
    return b;
}

// A function to test loading lines: codes, commands, keys and the lines as read; lines the launcher
// would not read are left out
static void test_load(void)
{
    Bindings *b = loaded(false, "Hotkey1=#4000003A;:quit\nHotkey2=nonsense\nHotkey3=#4000003B;kodi --standalone",
                         "ButtonA=:select\nButtonNope=:quit\nButtonA=:up");
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 2);
    const Binding *first = bindings_at(b, BINDINGS_KEYBOARD, 0);
    CHECK_INT(first->code, 0x4000003A);
    CHECK_STR(first->command, ":quit");
    CHECK_STR(first->key, "Hotkey1");
    CHECK_STR(first->original, "Hotkey1=#4000003A;:quit");
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 1)->command, "kodi --standalone");
    CHECK_INT(bindings_count(b, BINDINGS_GAMEPAD), 2);                // An unknown label is left out
    CHECK_INT(bindings_at(b, BINDINGS_GAMEPAD, 0)->code, bindings_label_index("ButtonA"));
    CHECK_STR(bindings_label(bindings_at(b, BINDINGS_GAMEPAD, 1)->code), "ButtonA");
    CHECK_STR(bindings_label(0), "LStickX-");
    CHECK_STR(bindings_label(24), "ButtonDPadRight");
    CHECK_INT(bindings_label_index("RTrigger"), 9);
    CHECK_INT(bindings_label_index("Nope"), -1);
    CHECK(!bindings_changed(b));
    bindings_free(b);
}

// A function to test the keys the floor refuses outright
static void test_refused_keys(void)
{
    Bindings *b = loaded(false, "", "");
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_UNKNOWN, ":quit") != NULL);
    CHECK_STR(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_LEFT, ":quit"),
              "The arrows, OK and Back keep their own meaning, so a hotkey on them would never run");
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_RETURN, ":quit") != NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_BACKSPACE, ":quit") != NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_UP, ":quit") == NULL);          // Allowed, with a confirmation
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, 0x4000003E, ":exit") == NULL);          // Linux: :exit logs, harmless
    CHECK(bindings_refuse_key(b, BINDINGS_GAMEPAD, -1, ":quit") != NULL);
    bindings_free(b);

    // On Windows the exit hotkey must be a function key, F1 to F24, but not F12
    b = loaded(true, "", "");
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, 0x4000003E, ":exit") == NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_F12, ":exit") != NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, 0x61, ":exit") != NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_F24, ":exit") == NULL);
    bindings_free(b);
}

// A function to test the floor: the last way to a navigation command or :settings stays
static void test_floor(void)
{
    // Keyboard: Up's own key moves up, so a hotkey for :up can go; taking Up over leaves none
    Bindings *b = loaded(false, "Hotkey1=#4000003A;:up", "");
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, 0, 0, NULL, true) == NULL);
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false) == NULL);   // F1 still goes up
    bindings_free(b);
    b = loaded(false, "", "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false),
              "That would leave no key for Up");
    // Both Menu keys taken leaves no key for Settings; one of them is fine
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_APPLICATION, ":quit", false) == NULL);
    bindings_set(b, BINDINGS_KEYBOARD, -1, BIND_KEY_APPLICATION, ":quit");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_MENU, ":home", false),
              "That would leave no key for Settings");
    bindings_free(b);

    // Gamepad: the built-in Up, Down and Settings controls come back when nothing else has them
    b = loaded(false, "", "ButtonDPadUp=:up\nButtonA=:select\nButtonB=:back");
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 0, 0, NULL, true) == NULL);              // The D-pad's own default
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, 1, 0, NULL, true),
              "That would leave no button for OK");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, 2, bindings_label_index("ButtonB"), ":home", false),
              "That would leave no button for Back");
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 1, bindings_label_index("ButtonX"), ":select", false) == NULL);
    bindings_free(b);

    // A command that had no binding to begin with is not the floor's to keep
    b = loaded(false, "", "ButtonA=:quit");
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 0, 0, NULL, true) == NULL);
    bindings_free(b);

    // The gamepad off: its list has no floor
    b = bindings_create(false, false);
    IniDoc *doc = inidoc_parse("[Gamepad]\nButtonA=:select\n", 26);
    IniDocItem items[2];
    CHECK(bindings_load(b, BINDINGS_GAMEPAD, items, inidoc_list(doc, "Gamepad", NULL, items, 2)));
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 0, 0, NULL, true) == NULL);
    inidoc_free(doc);
    bindings_free(b);
}

// A function to test which keyboard bindings take a key's navigation away, needing a confirmation
static void test_takes_navigation(void)
{
    CHECK(bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_UP, ":quit"));
    CHECK(!bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_UP, ":up"));
    CHECK(bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_DOWN, ":select"));
    CHECK(bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_MENU, ":home"));
    CHECK(!bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_APPLICATION, ":settings"));
    CHECK(!bindings_takes_navigation(BINDINGS_KEYBOARD, 0x4000003E, ":quit"));
    CHECK(!bindings_takes_navigation(BINDINGS_GAMEPAD, bindings_label_index("ButtonDPadUp"), ":quit"));
}

// A function to test the edits a save gets: a change, a removal and an addition; a new binding that
// was removed again writes nothing; Discard puts every list back
static void test_edits(void)
{
    Bindings *b = loaded(false, "Hotkey1=#4000003A;:quit\nHotkey2=#4000003B;:home", "ButtonA=:select");
    bindings_set(b, BINDINGS_KEYBOARD, 0, 0x4000003A, ":settings");
    bindings_remove(b, BINDINGS_KEYBOARD, 1);
    int added = bindings_set(b, BINDINGS_KEYBOARD, -1, 0x4000003C, ":back");
    CHECK_INT(added, 2);
    int gone = bindings_set(b, BINDINGS_KEYBOARD, -1, 0x4000003D, ":up");
    bindings_remove(b, BINDINGS_KEYBOARD, gone);
    CHECK(bindings_changed(b));
    ConfigListEdit edits[8];
    char values[8][BINDINGS_VALUE_MAX];
    int count = bindings_edits(b, BINDINGS_KEYBOARD, edits, values, 8);
    CHECK_INT(count, 3);
    CHECK_INT(edits[0].op, CONFIG_LIST_SET);
    CHECK_STR(edits[0].section, "Hotkeys");
    CHECK_STR(edits[0].original, "Hotkey1=#4000003A;:quit");
    CHECK_STR(edits[0].key, "Hotkey");
    CHECK(edits[0].numbered);
    CHECK_STR(edits[0].value, "#4000003A;:settings");
    CHECK_INT(edits[1].op, CONFIG_LIST_REMOVE);
    CHECK_STR(edits[1].original, "Hotkey2=#4000003B;:home");
    CHECK_INT(edits[2].op, CONFIG_LIST_ADD);
    CHECK_STR(edits[2].value, "#4000003C;:back");
    CHECK_INT(bindings_edits(b, BINDINGS_GAMEPAD, edits, values, 8), 0);

    // A gamepad change keeps its label as the key, with no number
    bindings_set(b, BINDINGS_GAMEPAD, 0, bindings_label_index("ButtonX"), ":select");
    count = bindings_edits(b, BINDINGS_GAMEPAD, edits, values, 8);
    CHECK_INT(count, 1);
    CHECK_STR(edits[0].section, "Gamepad");
    CHECK_STR(edits[0].key, "ButtonX");
    CHECK(!edits[0].numbered);
    CHECK_STR(edits[0].value, ":select");

    bindings_discard(b);
    CHECK(!bindings_changed(b));
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 2);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 0)->command, ":quit");
    CHECK(!bindings_at(b, BINDINGS_KEYBOARD, 1)->removed);
    CHECK_INT(bindings_edits(b, BINDINGS_KEYBOARD, edits, values, 8), 0);
    bindings_free(b);
}

// A function to test the capture: the starting key's repeats and release are ignored (Review Focus
// 5), the same key pressed again after its release counts, and 5 s with nothing gives up
static void test_capture(void)
{
    Capture c;
    capture_begin(&c, 1000, BIND_KEY_RETURN);
    CHECK_INT(c.state, CAPTURE_LISTENING);
    CHECK(!capture_press(&c, 1100, BIND_KEY_RETURN, true));      // Its auto-repeats
    CHECK(!capture_press(&c, 1200, BIND_KEY_RETURN, true));
    capture_release(&c, BIND_KEY_RETURN);                          // Its release
    CHECK_INT(c.state, CAPTURE_LISTENING);
    CHECK(capture_press(&c, 1300, BIND_KEY_RETURN, false));       // A new press of it counts
    CHECK_INT(c.code, BIND_KEY_RETURN);
    CHECK_INT(c.state, CAPTURE_CAPTURED);

    capture_begin(&c, 1000, BIND_KEY_RETURN);
    CHECK(capture_press(&c, 1500, 0x4000003E, false));             // Another key while it is held counts
    CHECK_INT(c.code, 0x4000003E);

    capture_begin(&c, 1000, BIND_KEY_RETURN);
    CHECK(!capture_expired(&c, 5999));
    CHECK(capture_expired(&c, 6000));
    CHECK_INT(c.state, CAPTURE_TIMED_OUT);
    CHECK(!capture_press(&c, 6100, 0x4000003E, false));           // Nothing after it gave up
    CHECK(!capture_expired(&c, 7000));                              // Timing out is said once
}

// A function to test the 10 s confirmation: the new key confirms it; any other key does not; 10 s
// with nothing reverts it, once
static void test_probation(void)
{
    Probation p;
    probation_begin(&p, 1000, BIND_KEY_UP);
    CHECK(!probation_press(&p, BIND_KEY_DOWN));
    CHECK(!probation_expired(&p, 10999));
    CHECK(probation_press(&p, BIND_KEY_UP));
    CHECK(!p.active);
    CHECK(!probation_expired(&p, 20000));                           // Confirmed: nothing to revert

    probation_begin(&p, 1000, BIND_KEY_UP);
    CHECK(probation_expired(&p, 11000));
    CHECK(!probation_expired(&p, 12000));
}

int main(void)
{
    test_load();
    test_refused_keys();
    test_floor();
    test_takes_navigation();
    test_edits();
    test_capture();
    test_probation();
    return check_report();
}
```

`test_bindings.c` needs `#include <stdio.h>` for `snprintf`, and `#include "inidoc.h"`.

Add to `tests/CMakeLists.txt`, after `test_fontlist`:

```cmake
# Unit tests for the bindings model: lists, the safety floor, confirmation and capture (pure)
add_executable(test_bindings test_bindings.c "${PROJECT_SOURCE_DIR}/src/bindings.c" "${PROJECT_SOURCE_DIR}/src/inidoc.c"
  "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_bindings PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME bindings COMMAND test_bindings)
```

and add `"${PROJECT_SOURCE_DIR}/src/bindings.c"` to `test_alloc`'s sources. In `tests/test_alloc.c`, add `#include "bindings.h"` and, above `main()`:

```c
// A function to prove that loading and changing bindings fails cleanly: a list that could not be
// loaded is empty, a binding that could not be added is not there, and nothing is left allocated
static void prove_bindings(void)
{
    static const char *const text = "[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey2=#4000003B;:home\n";
    IniDoc *doc = inidoc_parse(text, strlen(text));
    IniDocItem items[4];
    int count = inidoc_list(doc, "Hotkeys", NULL, items, 4);
    for (int n = 1;; n++) {
        arm(n);
        Bindings *b = bindings_create(false, true);
        bool ok = b != NULL && bindings_load(b, BINDINGS_KEYBOARD, items, count) &&
                  bindings_set(b, BINDINGS_KEYBOARD, -1, 0x4000003C, ":back") == 2;
        disarm();
        CHECK_RUN(failed || ok, n);
        if (b != NULL && !ok)
            CHECK_RUN(bindings_count(b, BINDINGS_KEYBOARD) <= 2, n);
        bindings_free(b);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    inidoc_free(doc);
    report("the bindings");
}
```

and call `prove_bindings();` after `prove_fontlist();`.

- [ ] **Step 2: Run the tests to see them fail**

Reconfigure (Global Constraints). Expected: the configure fails with `Cannot find source file: .../src/bindings.c`.

- [ ] **Step 3: Write `bindings.h`**

Create `src/bindings.h` with the declarations under **Interfaces**, above, and this header comment:

```c
// The keyboard's hotkeys and the gamepad's controls as the settings screen edits them: loaded from
// the file's lines ([Hotkeys], [Gamepad]), changed, removed and added, and turned into the list edits
// the save writes. The safety floor refuses a change that would leave a navigation command or
// :settings with no key or button, and says which keyboard changes take a key's navigation away (to
// be confirmed within 10 s). Capture and that confirmation are state machines driven by the caller's
// clock. Pure: no SDL, no globals; memory comes from alloc.h.
```

It includes `<stdbool.h>`, `"inidoc.h"` and `"config_save.h"`, and is guarded by `#ifndef BINDINGS_H`.

- [ ] **Step 4: Write `bindings.c`**

Create `src/bindings.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bindings.h"
#include "alloc.h"
#include <launcher_config.h>

// The gamepad's control labels, in util.c's table's order (its add_gamepad_control() info)
static const char *const LABELS[BINDINGS_LABELS] = {
    SETTING_GAMEPAD_LSTICK_XM, SETTING_GAMEPAD_LSTICK_XP, SETTING_GAMEPAD_LSTICK_YM, SETTING_GAMEPAD_LSTICK_YP,
    SETTING_GAMEPAD_RSTICK_XM, SETTING_GAMEPAD_RSTICK_XP, SETTING_GAMEPAD_RSTICK_YM, SETTING_GAMEPAD_RSTICK_YP,
    SETTING_GAMEPAD_LTRIGGER, SETTING_GAMEPAD_RTRIGGER, SETTING_GAMEPAD_BUTTON_A, SETTING_GAMEPAD_BUTTON_B,
    SETTING_GAMEPAD_BUTTON_X, SETTING_GAMEPAD_BUTTON_Y, SETTING_GAMEPAD_BUTTON_BACK, SETTING_GAMEPAD_BUTTON_GUIDE,
    SETTING_GAMEPAD_BUTTON_START, SETTING_GAMEPAD_BUTTON_LEFT_STICK, SETTING_GAMEPAD_BUTTON_RIGHT_STICK,
    SETTING_GAMEPAD_BUTTON_LEFT_SHOULDER, SETTING_GAMEPAD_BUTTON_RIGHT_SHOULDER, SETTING_GAMEPAD_BUTTON_DPAD_UP,
    SETTING_GAMEPAD_BUTTON_DPAD_DOWN, SETTING_GAMEPAD_BUTTON_DPAD_LEFT, SETTING_GAMEPAD_BUTTON_DPAD_RIGHT
};

// The commands the floor keeps a way to, and what the screen calls them
static const char *const FLOOR[] = { ":left", ":right", ":up", ":down", ":select", ":back", ":settings" };
static const char *const FLOOR_NAMES[] = { "Left", "Right", "Up", "Down", "OK", "Back", "Settings" };
#define FLOOR_COUNT 7

typedef struct {
    Binding *items;
    int count;
    int capacity;
    Binding *entry;       // The list as loaded: Discard's target
    int entry_count;
} List;

struct Bindings {
    bool windows;         // The exit hotkey's rule applies
    bool gamepad_on;      // The gamepad's list has a floor only while the gamepad runs
    List lists[2];
};

// A function to make an empty model; NULL when out of memory
Bindings *bindings_create(bool windows, bool gamepad_on)
{
    Bindings *b = alloc_calloc(1, sizeof(Bindings));
    if (b != NULL) {
        b->windows = windows;
        b->gamepad_on = gamepad_on;
    }
    return b;
}

// A function to free the model
void bindings_free(Bindings *b)
{
    if (b == NULL)
        return;
    for (int d = 0; d < 2; d++) {
        alloc_free(b->lists[d].items);
        alloc_free(b->lists[d].entry);
    }
    alloc_free(b);
}

// A function to get a gamepad label's name by its index; "" out of range
const char *bindings_label(int index)
{
    return index >= 0 && index < BINDINGS_LABELS ? LABELS[index] : "";
}

// A function to find a gamepad label's index; -1 for none
int bindings_label_index(const char *label)
{
    for (int i = 0; i < BINDINGS_LABELS; i++) {
        if (strcmp(LABELS[i], label) == 0)
            return i;
    }
    return -1;
}

// A function to make room for one more binding
static bool grow(List *list)
{
    if (list->count < list->capacity)
        return true;
    int capacity = list->capacity ? list->capacity * 2 : 8;
    Binding *items = alloc_realloc(list->items, (size_t) capacity * sizeof(Binding));
    if (items == NULL)
        return false;
    list->items = items;
    list->capacity = capacity;
    return true;
}

// A function to read a hotkey's value, "#<hex>;<command>", as add_hotkey() does; false for a line
// the launcher would not read
static bool read_hotkey(const char *value, int *code, char *command, size_t size)
{
    const char *semicolon = strchr(value, ';');
    if (value[0] != '#' || semicolon == NULL || semicolon[1] == '\0')
        return false;
    *code = (int) strtol(value + 1, NULL, 16);
    snprintf(command, size, "%s", semicolon + 1);
    return true;
}

// A function to load a device's bindings from its section's lines (inidoc_list()), in order. Lines
// the launcher would not read (no keycode, an unknown label, no command) are left out: settings
// never edit them. False when out of memory, with the list empty.
bool bindings_load(Bindings *b, BindingsDevice device, const IniDocItem *items, int count)
{
    List *list = &b->lists[device];
    list->count = 0;
    for (int i = 0; i < count; i++) {
        Binding binding;
        memset(&binding, 0, sizeof(binding));
        if (device == BINDINGS_KEYBOARD) {
            if (!read_hotkey(items[i].value, &binding.code, binding.command, sizeof(binding.command)))
                continue;
        }
        else {
            binding.code = bindings_label_index(items[i].key);
            if (binding.code < 0 || items[i].value[0] == '\0')
                continue;
            snprintf(binding.command, sizeof(binding.command), "%s", items[i].value);
        }
        snprintf(binding.key, sizeof(binding.key), "%s", items[i].key);
        snprintf(binding.original, sizeof(binding.original), "%s", items[i].text);
        if (!grow(list)) {
            list->count = 0;
            return false;
        }
        list->items[list->count++] = binding;
    }
    alloc_free(list->entry);
    list->entry = alloc_calloc((size_t) (list->count > 0 ? list->count : 1), sizeof(Binding));
    if (list->entry == NULL) {
        list->count = 0;
        return false;
    }
    memcpy(list->entry, list->items, (size_t) list->count * sizeof(Binding));
    list->entry_count = list->count;
    return true;
}

// A function to count a device's bindings, removed ones included (their rows go; their places stay)
int bindings_count(const Bindings *b, BindingsDevice device)
{
    return b->lists[device].count;
}

// A function to get a binding; NULL out of range
const Binding *bindings_at(const Bindings *b, BindingsDevice device, int index)
{
    const List *list = &b->lists[device];
    return index >= 0 && index < list->count ? &list->items[index] : NULL;
}

// A function to find a command among the floor's; -1 for another
static int floor_index(const char *command)
{
    for (int i = 0; i < FLOOR_COUNT; i++) {
        if (strcmp(FLOOR[i], command) == 0)
            return i;
    }
    return -1;
}

// A function to tell whether a keyboard key is one the dispatcher always takes first
static bool dispatcher_key(int code)
{
    return code == BIND_KEY_LEFT || code == BIND_KEY_RIGHT || code == BIND_KEY_RETURN || code == BIND_KEY_BACKSPACE;
}

// A function to say why a key or button cannot be bound to a command at all; NULL when it can
const char *bindings_refuse_key(const Bindings *b, BindingsDevice device, int code, const char *command)
{
    if (device == BINDINGS_GAMEPAD)
        return code < 0 || code >= BINDINGS_LABELS ? "This button has no name StreamFlex can store" : NULL;
    if (code == BIND_KEY_UNKNOWN)
        return "This key has no code StreamFlex can store (a CEC remote's OK and Back arrive this way on Linux)";
    if (dispatcher_key(code))
        return "The arrows, OK and Back keep their own meaning, so a hotkey on them would never run";
    bool function_key = (code >= BIND_KEY_F1 && code <= BIND_KEY_F12) || (code >= BIND_KEY_F13 && code <= BIND_KEY_F24);
    if (b->windows && strcmp(command, ":exit") == 0 && (!function_key || code == BIND_KEY_F12))
        return "The exit hotkey must be F1 to F24, but not F12";
    return NULL;
}

// A function to count each floor command's ways in on the keyboard: the built-in keys (Up, Down and
// the Menu keys give way to a hotkey on them), then every hotkey the dispatcher lets run
static void keyboard_counts(const Binding *items, int count, int *counts)
{
    bool taken_up = false, taken_down = false, taken_application = false, taken_menu = false;
    for (int i = 0; i < count; i++) {
        if (items[i].removed)
            continue;
        taken_up = taken_up || items[i].code == BIND_KEY_UP;
        taken_down = taken_down || items[i].code == BIND_KEY_DOWN;
        taken_application = taken_application || items[i].code == BIND_KEY_APPLICATION;
        taken_menu = taken_menu || items[i].code == BIND_KEY_MENU;
    }
    counts[0] = counts[1] = counts[4] = counts[5] = 1;   // Left, Right, Return, Backspace
    counts[2] = taken_up ? 0 : 1;
    counts[3] = taken_down ? 0 : 1;
    counts[6] = (taken_application ? 0 : 1) + (taken_menu ? 0 : 1);
    for (int i = 0; i < count; i++) {
        int f = floor_index(items[i].command);
        if (!items[i].removed && f >= 0 && !dispatcher_key(items[i].code))
            counts[f]++;
    }
}

// A function to count each floor command's ways in on the gamepad: its controls, then the defaults
// util.c adds for Up, Down and Settings to the labels the config leaves free, when nothing else has them
static void gamepad_counts(const Binding *items, int count, int *counts)
{
    static const struct {
        int floor;
        const char *labels[2];
    } defaults[] = {
        { 2, { SETTING_GAMEPAD_BUTTON_DPAD_UP, SETTING_GAMEPAD_LSTICK_YM } },
        { 3, { SETTING_GAMEPAD_BUTTON_DPAD_DOWN, SETTING_GAMEPAD_LSTICK_YP } },
        { 6, { SETTING_GAMEPAD_BUTTON_START, NULL } }
    };
    for (int f = 0; f < FLOOR_COUNT; f++)
        counts[f] = 0;
    for (int i = 0; i < count; i++) {
        int f = floor_index(items[i].command);
        if (!items[i].removed && f >= 0)
            counts[f]++;
    }
    for (size_t d = 0; d < sizeof(defaults) / sizeof(defaults[0]); d++) {
        if (counts[defaults[d].floor] > 0)
            continue;
        for (int k = 0; k < 2 && defaults[d].labels[k] != NULL; k++) {
            int label = bindings_label_index(defaults[d].labels[k]);
            bool free_label = true;
            for (int i = 0; i < count && free_label; i++)
                free_label = items[i].removed || items[i].code != label;
            if (free_label)
                counts[defaults[d].floor]++;
        }
    }
}

// A function to say why a change would leave a floor command with no way in, which it had before;
// NULL when it would not. `index` -1 is a new binding; `remove` removes the one at `index`.
const char *bindings_refuse_change(const Bindings *b, BindingsDevice device, int index, int code,
                                   const char *command, bool remove)
{
    static char why[64];
    const List *list = &b->lists[device];
    if (device == BINDINGS_GAMEPAD && !b->gamepad_on)
        return NULL;
    Binding *after = alloc_calloc((size_t) list->count + 1, sizeof(Binding));
    if (after == NULL)
        return "out of memory";
    memcpy(after, list->items, (size_t) list->count * sizeof(Binding));
    int count = list->count;
    if (index < 0)
        index = count++;
    if (remove)
        after[index].removed = true;
    else {
        after[index].code = code;
        snprintf(after[index].command, sizeof(after[index].command), "%s", command);
        after[index].removed = false;
    }
    int before_counts[FLOOR_COUNT];
    int after_counts[FLOOR_COUNT];
    if (device == BINDINGS_KEYBOARD) {
        keyboard_counts(list->items, list->count, before_counts);
        keyboard_counts(after, count, after_counts);
    }
    else {
        gamepad_counts(list->items, list->count, before_counts);
        gamepad_counts(after, count, after_counts);
    }
    alloc_free(after);
    for (int f = 0; f < FLOOR_COUNT; f++) {
        if (before_counts[f] > 0 && after_counts[f] == 0) {
            snprintf(why, sizeof(why), "That would leave no %s for %s", device == BINDINGS_KEYBOARD ? "key" : "button",
                FLOOR_NAMES[f]);
            return why;
        }
    }
    return NULL;
}

// A function to tell whether a keyboard binding takes a key's own navigation away: Up, Down or a
// Menu key bound to anything but its own command
bool bindings_takes_navigation(BindingsDevice device, int code, const char *command)
{
    if (device != BINDINGS_KEYBOARD)
        return false;
    if (code == BIND_KEY_UP)
        return strcmp(command, ":up") != 0;
    if (code == BIND_KEY_DOWN)
        return strcmp(command, ":down") != 0;
    if (code == BIND_KEY_APPLICATION || code == BIND_KEY_MENU)
        return strcmp(command, ":settings") != 0;
    return false;
}

// A function to set a binding's key or button and command, or add one (index -1); returns its index,
// -1 when out of memory
int bindings_set(Bindings *b, BindingsDevice device, int index, int code, const char *command)
{
    List *list = &b->lists[device];
    if (index < 0) {
        if (!grow(list))
            return -1;
        index = list->count++;
        memset(&list->items[index], 0, sizeof(Binding));
    }
    Binding *binding = &list->items[index];
    binding->code = code;
    snprintf(binding->command, sizeof(binding->command), "%s", command);
    binding->removed = false;
    return index;
}

// A function to remove a binding; its place stays, so the indexes of the others do not move
void bindings_remove(Bindings *b, BindingsDevice device, int index)
{
    List *list = &b->lists[device];
    if (index >= 0 && index < list->count)
        list->items[index].removed = true;
}

// A function to tell whether a binding differs from how it was loaded (a new one always does)
static bool binding_changed(const List *list, int i)
{
    if (i >= list->entry_count)
        return !list->items[i].removed;
    const Binding *now = &list->items[i];
    const Binding *was = &list->entry[i];
    return now->removed || now->code != was->code || strcmp(now->command, was->command) != 0;
}

// A function to tell whether any binding changed
bool bindings_changed(const Bindings *b)
{
    for (int d = 0; d < 2; d++) {
        for (int i = 0; i < b->lists[d].count; i++) {
            if (binding_changed(&b->lists[d], i))
                return true;
        }
    }
    return false;
}

// A function to put every list back as it was loaded
void bindings_discard(Bindings *b)
{
    for (int d = 0; d < 2; d++) {
        List *list = &b->lists[d];
        memcpy(list->items, list->entry, (size_t) list->entry_count * sizeof(Binding));
        list->count = list->entry_count;
    }
}

// A function to write a binding's line value: "#<HEX>;<command>" for a hotkey, the command for a control
static void line_value(BindingsDevice device, const Binding *binding, char *out, size_t size)
{
    if (device == BINDINGS_KEYBOARD)
        snprintf(out, size, "#%X;%s", (unsigned int) binding->code, binding->command);
    else
        snprintf(out, size, "%s", binding->command);
}

// A function to turn a device's changes into the save's list edits, in order: a changed binding
// sets its line, a removed one removes it, a new one is added (a hotkey numbered by the save); a
// new one removed again writes nothing. `values` holds each edit's value text. Returns how many.
int bindings_edits(const Bindings *b, BindingsDevice device, ConfigListEdit *edits,
                   char (*values)[BINDINGS_VALUE_MAX], int max)
{
    const List *list = &b->lists[device];
    const char *section = device == BINDINGS_KEYBOARD ? "Hotkeys" : "Gamepad";
    int n = 0;
    for (int i = 0; i < list->count && n < max; i++) {
        const Binding *binding = &list->items[i];
        bool loaded_line = i < list->entry_count;
        if (!binding_changed(list, i) || (!loaded_line && binding->removed))
            continue;
        ConfigListEdit *edit = &edits[n];
        memset(edit, 0, sizeof(*edit));
        edit->section = section;
        edit->original = loaded_line ? list->entry[i].original : NULL;
        edit->key = device == BINDINGS_KEYBOARD ? "Hotkey" : bindings_label(binding->code);
        edit->numbered = device == BINDINGS_KEYBOARD;
        line_value(device, binding, values[n], BINDINGS_VALUE_MAX);
        edit->value = values[n];
        edit->op = !loaded_line ? CONFIG_LIST_ADD : binding->removed ? CONFIG_LIST_REMOVE : CONFIG_LIST_SET;
        n++;
    }
    return n;
}

// A function to start a capture: the key or button that started it (OK) is held, so its repeats and
// its release are ignored until it is let go
void capture_begin(Capture *capture, unsigned int now, int starting_code)
{
    capture->state = CAPTURE_LISTENING;
    capture->started = now;
    capture->starting_code = starting_code;
    capture->starting_held = true;
    capture->code = BIND_KEY_UNKNOWN;
}

// A function to take a press while capturing; true when it is the one captured
bool capture_press(Capture *capture, unsigned int now, int code, bool repeat)
{
    if (capture->state != CAPTURE_LISTENING || now - capture->started >= BINDINGS_CAPTURE_MS)
        return false;
    if (code == capture->starting_code && (capture->starting_held || repeat))
        return false;
    capture->code = code;
    capture->state = CAPTURE_CAPTURED;
    return true;
}

// A function to take a release while capturing: the starting key's lets its next press count
void capture_release(Capture *capture, int code)
{
    if (code == capture->starting_code)
        capture->starting_held = false;
}

// A function to tell, once, that 5 s passed with nothing captured
bool capture_expired(Capture *capture, unsigned int now)
{
    if (capture->state != CAPTURE_LISTENING || now - capture->started < BINDINGS_CAPTURE_MS)
        return false;
    capture->state = CAPTURE_TIMED_OUT;
    return true;
}

// A function to start the 10 s in which a key that took navigation away must be pressed again
void probation_begin(Probation *probation, unsigned int now, int code)
{
    probation->active = true;
    probation->started = now;
    probation->code = code;
}

// A function to take a press during the 10 s; true when it is the new key, which keeps the change
bool probation_press(Probation *probation, int code)
{
    if (!probation->active || code != probation->code)
        return false;
    probation->active = false;
    return true;
}

// A function to tell, once, that the 10 s passed unconfirmed: the change reverts
bool probation_expired(Probation *probation, unsigned int now)
{
    if (!probation->active || now - probation->started < BINDINGS_CONFIRM_MS)
        return false;
    probation->active = false;
    return true;
}
```

Add `bindings.c` and `bindings.h` to `src/CMakeLists.txt`'s `SOURCES`, after `fontscan.h`.

`bindings_refuse_change()` returns its reason in a static buffer, valid until its next call. That suits the settings model, which copies the reason into its notice at once. `bindings.h` also declares `Binding.original` as 256 bytes, more than `INIDOC_MAX_LINE` (199), so a line always fits.

- [ ] **Step 5: Run the tests to see them pass**

Reconfigure, build and run the unit tests: `100% tests passed`, with `bindings` and `alloc` among them. Run the **Linux unit tests** (label `t13`): `warnings outside src/external: 0`.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/bindings.h src/bindings.c src/CMakeLists.txt tests/test_bindings.c tests/test_alloc.c tests/CMakeLists.txt
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: the bindings model: lists, the safety floor, confirmation and capture"
```

### Task 14: The binding pages, capture and the 10 s confirmation, live

The Controls page gains *Keyboard ›*. The Keyboard and Gamepad pages list each binding as *key → command*, with *Add binding* at the top. OK on a binding opens its page:
- *Key*, which captures;
- *Command ›*, the command picker;
- *Remove*, greyed with its reason when the floor refuses it.

Capture: *Press the key or button… (5 s)*, then a confirm page with *Keep*, *Try again* and *Cancel*. A change that takes Up, Down or a Menu key's navigation away is applied, and must be confirmed by pressing that key again within 10 s, or it reverts.

The in-memory lists are rebuilt from the model after every change, and *Discard* rebuilds them as settings opened. `util.c`'s static tail pointers go. The save writes the list edits.

**Files:**
- Modify: `src/settings.h`, `src/settings.c` (the binding pages, rows, actions and events; `settings_set_bindings()`, `settings_captured()`, `settings_capture_ended()`, `settings_bind_command()`, `settings_revert_binding()`)
- Modify: `src/settings_pickers.h`, `src/settings_pickers.c` (capture and the confirmation; the binding's command picker; `apply_bindings()`)
- Modify: `src/settings_screen.h`, `src/settings_screen.c` (bindings loaded at open, applied, saved; the raw key and pad routes)
- Modify: `src/util.h`, `src/util.c` (`add_hotkey()` and `add_gamepad_control()` without static tails; `clear_hotkeys()`, `clear_gamepad_controls()`; the label table shared)
- Modify: `src/launcher.c` (`handle_keypress()`; `SDL_KEYUP`; `gamepad_pressed_label()`; the pad hook's button; `cleanup()` frees the lists through the clears)
- Modify: `src/platform/platform.h`, `src/platform/win32.c` (`clear_exit_hotkey()`)
- Test: `tests/test_settings.c`
- Create: `tests/headless/checks/62-settings-bindings.sh`, `tests/headless/fixtures/f62-keys.ini`, `f62-up.ini`, `f62-pad.ini`

**Interfaces:**
- Consumes: Task 13's `Bindings`, `Capture`, `Probation` and floor functions; Task 12's `config_save_all()` and `ConfigListEdit`; Task 9's command picker rows.
- Produces:

```c
// settings.h
// SettingsPage gains SETTINGS_PAGE_KEYBOARD, SETTINGS_PAGE_BINDING, SETTINGS_PAGE_CAPTURE, SETTINGS_PAGE_CONFIRM
// SettingsRowKind gains SETTINGS_ROW_BINDING; SettingsRow gains int binding;
// SettingsAction gains SETTINGS_ACTION_ADD_BINDING, _CAPTURE, _BIND_COMMAND, _REMOVE_BINDING, _KEEP, _TRY_AGAIN, _CANCEL
// SettingsEventKind gains SETTINGS_EVENT_CAPTURE, SETTINGS_EVENT_PICK_COMMAND, SETTINGS_EVENT_BINDINGS
// SettingsEvent gains: int device; int code; bool confirm;   // BINDINGS: confirm = start the 10 s for `code`
typedef void (*SettingsKeyNamer)(int device, int code, char *out, size_t size);
void settings_set_bindings(SettingsState *state, Bindings *bindings, SettingsKeyNamer namer);
SettingsEvent settings_captured(SettingsState *state, int code);
void settings_capture_ended(SettingsState *state, const char *why);
SettingsEvent settings_bind_command(SettingsState *state, const char *command);
SettingsEvent settings_revert_binding(SettingsState *state);
const char *settings_binding_command(const SettingsState *state);   // The command the binding page shows
// settings_screen.h
bool settings_raw_key(int code, bool repeat);  // A key while settings are open: true when capture or the 10 s took it
void settings_raw_release(int code);
bool settings_raw_pad(int label);              // Each frame: the gamepad label held, -1 for none; true when taken
// util.h
void clear_hotkeys(void);
void clear_gamepad_controls(void);
void add_gamepad_control(const char *label, const char *cmd);   // No longer static
int gamepad_label_count(void);
const struct gamepad_info *gamepad_label_info(int index);
// launcher.h
int gamepad_pressed_label(void);               // The first control held on any open pad; -1 for none
// platform.h (_WIN32)
void clear_exit_hotkey(void);
```

- [ ] **Step 1: Write the failing tests**

In `tests/test_settings.c`, add `#include "bindings.h"` and `#include "inidoc.h"`, and above `main()`:

```c
// A function to name keys in tests: "#<HEX>" for a key, the label for a pad's control
static void test_namer(int device, int code, char *out, size_t size)
{
    if (device == BINDINGS_KEYBOARD)
        snprintf(out, size, "#%X", (unsigned int) code);
    else
        snprintf(out, size, "%s", bindings_label(code));
}

// A function to give a model bindings loaded from hotkey lines
static Bindings *with_bindings(SettingsState *state, const char *hotkeys)
{
    char text[512];
    snprintf(text, sizeof(text), "[Hotkeys]\n%s\n", hotkeys);
    IniDoc *doc = inidoc_parse(text, strlen(text));
    IniDocItem items[8];
    Bindings *b = bindings_create(false, true);
    CHECK(bindings_load(b, BINDINGS_KEYBOARD, items, inidoc_list(doc, "Hotkeys", NULL, items, 8)));
    inidoc_free(doc);
    settings_set_bindings(state, b, test_namer);
    return b;
}

// A function to test adding a hotkey: Add binding, capture, Keep, then the command, which commits it
static void test_add_binding(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    Bindings *b = with_bindings(state, "Hotkey1=#4000003A;:quit");
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].label, "Add binding");
    CHECK_INT(rows[1].kind, SETTINGS_ROW_BINDING);
    CHECK_STR(rows[1].label, "#4000003A");
    CHECK_STR(rows[1].value, "Quit StreamFlex");
    CHECK_INT(rows[count - 1].kind, SETTINGS_ROW_NOTE);

    // Add binding: the page of a new binding, its key first
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    SettingsEvent event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_CAPTURE);
    CHECK_INT(event.device, BINDINGS_KEYBOARD);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_CAPTURE);

    // A refused key ends the capture with its reason; a good one asks to keep it
    settings_capture_ended(state, NULL);
    settings_command(state, SETTINGS_OK);
    event = settings_captured(state, BIND_KEY_LEFT);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK(strstr(settings_notice(state), "keep their own meaning") != NULL);
    settings_command(state, SETTINGS_OK);
    settings_captured(state, 0x4000003E);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_CONFIRM);
    CHECK_STR(row_labelled(state, rows, "Keep")->label, "Keep");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);   // Keep
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK_STR(row_labelled(state, rows, "Key")->value, "#4000003E");

    // The command commits the new binding
    cursor_to(state, "Command");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_PICK_COMMAND);
    event = settings_bind_command(state, ":home");
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK(!event.confirm);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 2);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 1)->command, ":home");
    CHECK(settings_any_changed(state));
    settings_free(state);
    bindings_free(b);
}

// A function to test a change that takes Up's navigation away: allowed while another key goes up,
// flagged for the 10 s, and reverted when it is not confirmed
static void test_navigation_confirm(void)
{
    SettingsState *state = open_model();
    Bindings *b = with_bindings(state, "Hotkey1=#4000003A;:up");
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    settings_command(state, SETTINGS_OK);                    // Add binding
    settings_command(state, SETTINGS_OK);                    // Key
    settings_captured(state, BIND_KEY_UP);
    settings_command(state, SETTINGS_OK);                    // Keep
    SettingsEvent event = settings_bind_command(state, ":quit");
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK(event.confirm);
    CHECK_INT(event.code, BIND_KEY_UP);
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 2);
    event = settings_revert_binding(state);
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK(bindings_at(b, BINDINGS_KEYBOARD, 1)->removed);
    CHECK(!settings_any_changed(state));
    settings_free(state);
    bindings_free(b);
}

// A function to test Remove: greyed with the floor's reason for the last way to a command, and
// allowed otherwise
static void test_remove_binding(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    Bindings *b = with_bindings(state, "Hotkey1=#4000003A;:quit");
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    settings_command(state, SETTINGS_DOWN);                  // The binding
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    const SettingsRow *remove = row_labelled(state, rows, "Remove");
    CHECK(remove != NULL && remove->enabled);
    cursor_to(state, "Remove");
    SettingsEvent event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);
    CHECK(bindings_at(b, BINDINGS_KEYBOARD, 0)->removed);
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 2);   // Add binding and the note

    // Discard brings it back
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);
    cursor_to(state, "Discard changes");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_DISCARD);
    CHECK(!bindings_at(b, BINDINGS_KEYBOARD, 0)->removed);
    settings_free(state);
    bindings_free(b);
}
```

and call them from `main()`: `test_add_binding(); test_navigation_confirm(); test_remove_binding();`.

In `tests/CMakeLists.txt`, `test_settings` now links `bindings.c` and `inidoc.c` as well: `settings.c` uses the bindings model, and the tests parse hotkey lines. Its sources become `test_settings.c settings.c layout.c alloc.c bindings.c inidoc.c`. `test_alloc` already links `inidoc.c`, and Task 13 added `bindings.c` to it.

Create the fixtures. `tests/headless/fixtures/f62-keys.ini` has no hotkeys:

```ini
[General]
DefaultMenu=Main

[Main]
Entry1=One;apps;:quit
```

`tests/headless/fixtures/f62-up.ini` has F1 for `:up`, so Up itself may be taken over:

```ini
[General]
DefaultMenu=Main

[Hotkeys]
Hotkey1=#4000003A;:up

[Main]
Entry1=One;apps;:quit
```

`tests/headless/fixtures/f62-pad.ini`:

```ini
[General]
DefaultMenu=Main

[Gamepad]
Enabled=true

[Main]
Entry1=One;apps;:quit
```

The Controls page now starts with *Keyboard*, so Task 6's page tour must walk it and reach *Gamepad* with one more `Down`. In `tests/headless/checks/55-settings-pages.sh`, replace the Controls line of `tour()`:

```bash
    keys="$keys Return Return $(walk) BackSpace BackSpace Down"   # Controls, Gamepad
```

with:

```bash
    keys="$keys Return Return $(walk) BackSpace Down Return $(walk) BackSpace BackSpace Down"   # Controls, Keyboard, Gamepad
```

and add `"Controls${ARROW}Keyboard"` to the check's list of pages that must open. The walk presses no OK, so it opens no binding page and starts no capture.

Create `tests/headless/checks/62-settings-bindings.sh`:

```bash
# Key and gamepad bindings (3b): capture, Keep, the command, the save; a held starting key; the
# 10 s confirmation, reverted and kept; a pad's button captured through the virtual pad

TO_KEYBOARD="Menu Down Down Down Down Down Down Down Down Return Return"   # Controls, then Keyboard
QUIT="Down Down Down Down Down Down Down Down Down Return"                 # Quit StreamFlex in the command picker
SAVE="BackSpace BackSpace BackSpace"

# F5 captured for Quit StreamFlex: saved as the first hotkey in a new [Hotkeys]
cfg=$(writable_config f62-keys)
CFG=$cfg run_keys f62-bind $TO_KEYBOARD Return Return F5 Return Down Return $QUIT $SAVE
ok=1
grep -q 'Settings: capture got F5 (#4000003E)' "$out/f62-bind.log" \
    && sed -n '/^\[Hotkeys\]/,/^\[/p' "$cfg" | grep -qx 'Hotkey1=#4000003E;:quit' \
    && grep -q 'Settings: the bindings now hold 1 hotkey' "$out/f62-bind.log" && ran_clean f62-bind && ok=0
result "bindings: a captured key bound to a command saves as a hotkey (exit $(cat "$out/f62-bind.code"))" $ok
diff "$FX/f62-keys.ini" "$cfg" | sed 's/^/      /'

# The OK that starts a capture held for 2 s: its repeats and its release are not captured, and the
# key pressed after it is (Review Focus 5)
hold_return_then_f6() { xdotool keydown Return; sleep 2; xdotool keyup Return; sleep 0.5; xdotool key F6; sleep 1; }
cfg=$(writable_config f62-keys)
CFG=$cfg run_keys f62-held $TO_KEYBOARD Return +hold_return_then_f6 Return Down Return $QUIT $SAVE
ok=1
[ "$(grep -c 'Key Return (#D) detected' "$out/f62-held.log")" -gt 2 ] \
    && grep -q 'Settings: capture got F6 (#4000003F)' "$out/f62-held.log" \
    && ! grep -q 'Settings: capture got Return' "$out/f62-held.log" \
    && grep -qx 'Hotkey1=#4000003F;:quit' "$cfg" && ran_clean f62-held && ok=0
result "bindings: a held OK starts one capture, and the key after it is the one bound (exit $(cat "$out/f62-held.code"))" $ok

# Up taken for Quit StreamFlex (F1 still goes up): unconfirmed for 10 s, it goes back, and nothing is saved
wait_revert() { sleep 11; }
cfg=$(writable_config f62-up)
CFG=$cfg run_keys f62-revert $TO_KEYBOARD Return Return Up Return Down Return $QUIT +wait_revert $SAVE
ok=1
grep -q 'Settings: press Up again within 10 s to keep it' "$out/f62-revert.log" \
    && grep -q 'Settings: the binding went back: Up was not pressed again within 10 s' "$out/f62-revert.log" \
    && grep -q 'Settings: nothing changed' "$out/f62-revert.log" && cmp -s "$FX/f62-up.ini" "$cfg" \
    && ran_clean f62-revert && ok=0
result "bindings: taking Up's navigation away reverts after 10 s unconfirmed (exit $(cat "$out/f62-revert.code"))" $ok

# The same, confirmed by pressing Up within the 10 s: kept and saved
cfg=$(writable_config f62-up)
CFG=$cfg run_keys f62-confirm $TO_KEYBOARD Return Return Up Return Down Return $QUIT Up $SAVE
ok=1
grep -q 'Settings: kept Up for :quit' "$out/f62-confirm.log" && grep -qx 'Hotkey2=#40000052;:quit' "$cfg" \
    && ran_clean f62-confirm && ok=0
result "bindings: taking Up's navigation away is kept when Up is pressed again in time (exit $(cat "$out/f62-confirm.code"))" $ok

# A pad's button: the virtual pad holds B while the file names it (STREAMFLEX_TEST_PAD_BUTTON)
press_b() { : > /tmp/pad-b; sleep 1; rm -f /tmp/pad-b; sleep 1; }
rm -f /tmp/pad-b
cfg=$(writable_config f62-pad)
STREAMFLEX_TEST_PAD=/tmp/pad-b STREAMFLEX_TEST_PAD_BUTTON=b WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-pad Menu Down Down Down Down Down Down Down Down Return Down Return Down Down Down Return Return \
    +press_b Return Down Return $QUIT BackSpace BackSpace BackSpace BackSpace
ok=1
grep -q 'Settings: capture got ButtonB' "$out/f62-pad.log" && sed -n '/^\[Gamepad\]/,/^\[/p' "$cfg" | grep -qx 'ButtonB=:quit' \
    && ran_clean f62-pad && ok=0
result "bindings: a gamepad button captured from the pad saves as a control (exit $(cat "$out/f62-pad.code"))" $ok
```

- [ ] **Step 2: Run the tests to see them fail**

Build `test_settings`. Expected: `'SETTINGS_PAGE_KEYBOARD': undeclared identifier` and the same for `settings_set_bindings`.

- [ ] **Step 3: Share the gamepad labels, and drop the static tails**

In `src/util.c`:
- Move `add_gamepad_control()`'s `info` table to file scope as `static const struct gamepad_info GAMEPAD_INFO[] = { ... };`, unchanged and in the same order. It is the order `bindings.c`'s `LABELS` repeats.
- Add below the table:

```c
// A function to count the gamepad's control labels
int gamepad_label_count(void)
{
    return (int) (sizeof(GAMEPAD_INFO) / sizeof(GAMEPAD_INFO[0]));
}

// A function to get a gamepad control label's type and SDL index
const struct gamepad_info *gamepad_label_info(int index)
{
    return index >= 0 && index < gamepad_label_count() ? &GAMEPAD_INFO[index] : NULL;
}
```

- Replace `add_hotkey()`'s list code (the `static Hotkey *current_hotkey` and the `if (current_hotkey == NULL)` block) with an append that walks to the end:

```c
    // Add to the end of the list, found each time: settings rebuild the list, so no tail pointer lives on
    Hotkey *hotkey = malloc(sizeof(Hotkey));
    hotkey->keycode = code;
    hotkey->cmd = strdup(cmd);
    hotkey->next = NULL;
    if (hotkeys == NULL)
        hotkeys = hotkey;
    else {
        Hotkey *last = hotkeys;
        while (last->next != NULL)
            last = last->next;
        last->next = hotkey;
    }
```

- Make `add_gamepad_control()` public: remove `static` and its prototype at the top. Replace its `static GamepadControl *current_gamepad_control` block the same way, finding the table entry in `GAMEPAD_INFO`:

```c
    GamepadControl *control = malloc(sizeof(GamepadControl));
    *control = (GamepadControl) {
        .type     = GAMEPAD_INFO[i].type,
        .index    = GAMEPAD_INFO[i].index,
        .label    = GAMEPAD_INFO[i].label,
        .repeat   = 0,
        .next     = NULL
    };
    control->cmd = strdup(cmd);
    if (gamepad_controls == NULL)
        gamepad_controls = control;
    else {
        GamepadControl *last = gamepad_controls;
        while (last->next != NULL)
            last = last->next;
        last->next = control;
    }
```

- Add:

```c
// A function to free every hotkey: at quit, and when settings rebuild the list
void clear_hotkeys(void)
{
    while (hotkeys != NULL) {
        Hotkey *next = hotkeys->next;
        free(hotkeys->cmd);
        free(hotkeys);
        hotkeys = next;
    }
}

// A function to free every gamepad control: at quit, and when settings rebuild the list
void clear_gamepad_controls(void)
{
    while (gamepad_controls != NULL) {
        GamepadControl *next = gamepad_controls->next;
        free(gamepad_controls->cmd);
        free(gamepad_controls);
        gamepad_controls = next;
    }
}
```

In `src/util.h`, declare `clear_hotkeys`, `clear_gamepad_controls`, `add_gamepad_control`, `gamepad_label_count` and `gamepad_label_info`. `struct gamepad_info` is already there.

In `src/launcher.c`'s `cleanup()`, replace the two free loops for the hotkeys and gamepad controls with `clear_hotkeys();` and `clear_gamepad_controls();`.

In `src/platform/win32.c`, add below `register_exit_hotkey()`:

```c
// A function to let go of the exit hotkey, before settings bind it again
void clear_exit_hotkey()
{
    if (exit_hotkey)
        UnregisterHotKey(wm_info.info.win.window, 1);
    exit_hotkey = 0;
}
```

and declare `void clear_exit_hotkey(void);` in `platform.h`'s `_WIN32` block.

- [ ] **Step 4: The binding pages in the model**

In `src/settings.h`:
- Add `#include "bindings.h"`.
- Add the pages `SETTINGS_PAGE_KEYBOARD`, `SETTINGS_PAGE_BINDING`, `SETTINGS_PAGE_CAPTURE` and `SETTINGS_PAGE_CONFIRM` before `SETTINGS_PAGE_SAVE_FAILED`.
- Add the row kind `SETTINGS_ROW_BINDING` (`// OK opens a binding's page`).
- Add the field `int binding;                 // BINDING rows: the binding's index` to `SettingsRow`.
- Add the actions `SETTINGS_ACTION_ADD_BINDING`, `SETTINGS_ACTION_CAPTURE`, `SETTINGS_ACTION_BIND_COMMAND`, `SETTINGS_ACTION_REMOVE_BINDING`, `SETTINGS_ACTION_KEEP`, `SETTINGS_ACTION_TRY_AGAIN` and `SETTINGS_ACTION_CANCEL`.
- Add the events:

```c
    SETTINGS_EVENT_CAPTURE,      // Start capturing a key or button for the binding page's device
    SETTINGS_EVENT_PICK_COMMAND, // Open the command picker for the binding page's binding
    SETTINGS_EVENT_BINDINGS,     // The binding lists changed: apply them; with `confirm`, start the 10 s for `code`
```

- Add to `SettingsEvent`:

```c
    int device;                  // CAPTURE: BindingsDevice
    int code;                    // BINDINGS with confirm: the key whose navigation was taken
    bool confirm;
```

- Declare the functions from **Interfaces**, with `typedef void (*SettingsKeyNamer)(int device, int code, char *out, size_t size);`.

In `src/settings.c`:
- `PageRef` gains `int device;` and `int binding;` (-1 for a new binding).
- `SettingsState` gains:

```c
    Bindings *bindings;          // The key and gamepad bindings, while settings are open; NULL: no binding rows
    SettingsKeyNamer namer;
    struct {                     // The binding the binding page edits
        int device;
        int index;               // -1: a new one, not yet in the list
        int code;                // -1 until a key is kept
        char command[BINDINGS_COMMAND_MAX];
        int captured;            // The key the confirm page asks about
    } pending;
    struct {                     // What the 10 s would put back
        bool active;
        int device;
        int index;
        bool added;
        Binding before;
    } undo;
```

Add the binding pages' functions:

```c
// A function to give the model its bindings, and a way to name keys and buttons
void settings_set_bindings(SettingsState *state, Bindings *bindings, SettingsKeyNamer namer)
{
    state->bindings = bindings;
    state->namer = namer;
}

// A function to name a key or button through the screen's namer
static void key_name(const SettingsState *state, int device, int code, char *out, size_t size)
{
    if (state->namer != NULL)
        state->namer(device, code, out, size);
    else
        snprintf(out, size, "#%X", (unsigned int) code);
}

// A function to add a device's binding rows: Add binding, then every binding that is not removed
static int binding_rows(SettingsState *state, int device, SettingsRow *rows, int n, int max)
{
    char text[BINDINGS_COMMAND_MAX];
    SettingsRow add = action_row("Add binding", SETTINGS_ACTION_ADD_BINDING, true);
    n = add_row(rows, n, max, add);
    for (int i = 0; i < bindings_count(state->bindings, (BindingsDevice) device); i++) {
        const Binding *binding = bindings_at(state->bindings, (BindingsDevice) device, i);
        if (binding->removed)
            continue;
        key_name(state, device, binding->code, text, sizeof(text));
        SettingsRow row = new_row(SETTINGS_ROW_BINDING, text);
        setting_command_label(binding->command, row.value, sizeof(row.value));
        row.binding = i;
        n = add_row(rows, n, max, row);
    }
    return n;
}
```

In `settings_rows()`:
- the `SETTINGS_PAGE_CONTROLS` case lists the Keyboard first, when there are bindings:

```c
        case SETTINGS_PAGE_CONTROLS:
            if (state->bindings != NULL) {
                int keys = 0;
                for (int i = 0; i < bindings_count(state->bindings, BINDINGS_KEYBOARD); i++)
                    keys += bindings_at(state->bindings, BINDINGS_KEYBOARD, i)->removed ? 0 : 1;
                snprintf(text, sizeof(text), keys == 1 ? "%d hotkey" : "%d hotkeys", keys);
                n = add_row(rows, n, max, link_row("Keyboard", text, SETTINGS_PAGE_KEYBOARD, -1));
            }
            n = add_row(rows, n, max, link_row("Gamepad", on_off(state, SET_ID_GAMEPAD_ENABLED), SETTINGS_PAGE_GAMEPAD, -1));
            break;
```

- the `SETTINGS_PAGE_GAMEPAD` case adds its bindings after its note, when there are bindings:

```c
            if (state->bindings != NULL) {
                n = binding_rows(state, BINDINGS_GAMEPAD, rows, n, max);
                n = add_row(rows, n, max, note_row(BUILT_IN_NOTE));
            }
```

- add the new pages:

```c
        case SETTINGS_PAGE_KEYBOARD:
            n = binding_rows(state, BINDINGS_KEYBOARD, rows, n, max);
            n = add_row(rows, n, max, note_row(KEYBOARD_NOTE));
            break;
        case SETTINGS_PAGE_BINDING: {
            char name[64];
            if (state->pending.code >= 0)
                key_name(state, state->pending.device, state->pending.code, name, sizeof(name));
            else
                snprintf(name, sizeof(name), "Choose" ELLIPSIS);
            SettingsRow key = action_row("Key", SETTINGS_ACTION_CAPTURE, true);
            snprintf(key.value, sizeof(key.value), "%s", name);
            n = add_row(rows, n, max, key);
            SettingsRow command = action_row("Command", SETTINGS_ACTION_BIND_COMMAND, true);
            command.kind = SETTINGS_ROW_ACTION;
            if (state->pending.command[0] != '\0')
                setting_command_label(state->pending.command, command.value, sizeof(command.value));
            else
                snprintf(command.value, sizeof(command.value), "None");
            n = add_row(rows, n, max, command);
            if (state->pending.index < 0)
                n = add_row(rows, n, max, action_row("Cancel", SETTINGS_ACTION_CANCEL, true));
            else {
                const char *why = bindings_refuse_change(state->bindings, (BindingsDevice) state->pending.device,
                                                         state->pending.index, 0, NULL, true);
                n = add_row(rows, n, max, greyed(action_row("Remove", SETTINGS_ACTION_REMOVE_BINDING, true), why != NULL, why));
            }
            break;
        }
        case SETTINGS_PAGE_CAPTURE:
            n = add_row(rows, n, max, note_row("Press the key or button" ELLIPSIS));
            break;
        case SETTINGS_PAGE_CONFIRM: {
            char name[64];
            key_name(state, state->pending.device, state->pending.captured, name, sizeof(name));
            snprintf(state->confirm_note, sizeof(state->confirm_note), "Captured: %s", name);
            n = add_row(rows, n, max, note_row(state->confirm_note));
            n = add_row(rows, n, max, action_row("Keep", SETTINGS_ACTION_KEEP, true));
            n = add_row(rows, n, max, action_row("Try again", SETTINGS_ACTION_TRY_AGAIN, true));
            n = add_row(rows, n, max, action_row("Cancel", SETTINGS_ACTION_CANCEL, true));
            break;
        }
```

  with `char confirm_note[96];` in `SettingsState`, and the notes:

```c
static const char *const KEYBOARD_NOTE = "The arrows, OK (Enter) and Back (Backspace) always keep their own meaning.";
static const char *const BUILT_IN_NOTE = "Up, Down and Settings have built-in buttons while nothing else is bound to them.";
```

The command row's label is `Command`, and its value shows › (drawn for an action row whose action is `SETTINGS_ACTION_BIND_COMMAND`: Task 14's `draw_row()` edit below).

Add the binding page's logic:

```c
// A function to commit the binding page's binding once it has a key and a command: refused with the
// floor's reason, or set (added when new); a change that takes a key's navigation away asks for the
// 10 s confirmation. Back to the list either way unless refused.
static SettingsEvent commit_binding(SettingsState *state)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_MOVED;
    BindingsDevice device = (BindingsDevice) state->pending.device;
    const char *why = bindings_refuse_key(state->bindings, device, state->pending.code, state->pending.command);
    if (why == NULL)
        why = bindings_refuse_change(state->bindings, device, state->pending.index, state->pending.code,
                                     state->pending.command, false);
    if (why != NULL) {
        snprintf(state->notice, sizeof(state->notice), "%s", why);
        return event;
    }
    bool added = state->pending.index < 0;
    if (!added)
        state->undo.before = *bindings_at(state->bindings, device, state->pending.index);
    int index = bindings_set(state->bindings, device, state->pending.index, state->pending.code, state->pending.command);
    if (index < 0) {
        snprintf(state->notice, sizeof(state->notice), "out of memory");
        return event;
    }
    event.kind = SETTINGS_EVENT_BINDINGS;
    event.confirm = bindings_takes_navigation(device, state->pending.code, state->pending.command);
    event.code = state->pending.code;
    event.device = (int) device;
    state->undo.active = event.confirm;
    state->undo.device = device;
    state->undo.index = index;
    state->undo.added = added;
    state->depth--;   // Back to the list
    return event;
}

// A function to take a captured key or button: a refused one ends the capture with its reason; any
// other goes to the confirm page
SettingsEvent settings_captured(SettingsState *state, int code)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_MOVED;
    if (state->stack[state->depth].page != SETTINGS_PAGE_CAPTURE)
        return event;
    state->depth--;
    const char *why = bindings_refuse_key(state->bindings, (BindingsDevice) state->pending.device, code, "");
    if (why != NULL) {
        snprintf(state->notice, sizeof(state->notice), "%s", why);
        fix_cursor(state);
        return event;
    }
    state->pending.captured = code;
    push_page(state, SETTINGS_PAGE_CONFIRM, -1);
    return event;
}

// A function to end a capture that caught nothing (a timeout), with the reason for the caption
void settings_capture_ended(SettingsState *state, const char *why)
{
    if (state->stack[state->depth].page != SETTINGS_PAGE_CAPTURE)
        return;
    state->depth--;
    snprintf(state->notice, sizeof(state->notice), "%s", why != NULL ? why : "");
    fix_cursor(state);
}

// A function to set the binding page's command, chosen in the command picker; with a key already, it
// commits the binding
SettingsEvent settings_bind_command(SettingsState *state, const char *command)
{
    snprintf(state->pending.command, sizeof(state->pending.command), "%s", command);
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_MOVED;
    if (state->pending.code >= 0 && command[0] != '\0')
        event = commit_binding(state);
    fix_cursor(state);
    return event;
}

// A function to put back the change the 10 s were for, unconfirmed
SettingsEvent settings_revert_binding(SettingsState *state)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    if (!state->undo.active)
        return event;
    BindingsDevice device = (BindingsDevice) state->undo.device;
    if (state->undo.added)
        bindings_remove(state->bindings, device, state->undo.index);
    else
        bindings_set(state->bindings, device, state->undo.index, state->undo.before.code, state->undo.before.command);
    state->undo.active = false;
    event.kind = SETTINGS_EVENT_BINDINGS;
    fix_cursor(state);
    return event;
}

// A function to get the command the binding page shows, for the command picker's cursor
const char *settings_binding_command(const SettingsState *state)
{
    return state->pending.command;
}
```

In `settings_command()`'s `SETTINGS_OK` case, add the binding rows and actions before the existing `ACTION` branches:

```c
            else if (row->kind == SETTINGS_ROW_BINDING) {
                const Binding *binding = bindings_at(state->bindings, (BindingsDevice) top->device, row->binding);
                state->pending.device = top->device;
                state->pending.index = row->binding;
                state->pending.code = binding->code;
                snprintf(state->pending.command, sizeof(state->pending.command), "%s", binding->command);
                push_page(state, SETTINGS_PAGE_BINDING, -1);
                event.kind = SETTINGS_EVENT_MOVED;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_ADD_BINDING) {
                state->pending.device = top->device;
                state->pending.index = -1;
                state->pending.code = -1;
                state->pending.command[0] = '\0';
                push_page(state, SETTINGS_PAGE_BINDING, -1);
                event.kind = SETTINGS_EVENT_MOVED;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && (row->action == SETTINGS_ACTION_CAPTURE || row->action == SETTINGS_ACTION_TRY_AGAIN)) {
                if (row->action == SETTINGS_ACTION_TRY_AGAIN)
                    state->depth--;
                push_page(state, SETTINGS_PAGE_CAPTURE, -1);
                event.kind = SETTINGS_EVENT_CAPTURE;
                event.device = state->pending.device;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_BIND_COMMAND)
                event.kind = SETTINGS_EVENT_PICK_COMMAND;
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_KEEP) {
                state->pending.code = state->pending.captured;
                state->depth--;
                event.kind = SETTINGS_EVENT_MOVED;
                if (state->pending.command[0] != '\0')
                    event = commit_binding(state);
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_CANCEL) {
                state->depth--;
                event.kind = SETTINGS_EVENT_MOVED;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_REMOVE_BINDING) {
                bindings_remove(state->bindings, (BindingsDevice) state->pending.device, state->pending.index);
                state->depth--;
                event.kind = SETTINGS_EVENT_BINDINGS;
            }
```

`push_page()` must record the device: in `push_page()`, set `top->device = page == SETTINGS_PAGE_KEYBOARD ? BINDINGS_KEYBOARD : page == SETTINGS_PAGE_GAMEPAD ? BINDINGS_GAMEPAD : state->stack[state->depth - 1].device;`. The binding pages then inherit the device of the list they came from.

In the `DISCARD` branch, add `if (state->bindings != NULL) bindings_discard(state->bindings);` and `state->undo.active = false;`. In `settings_any_changed()`, add `|| (state->bindings != NULL && bindings_changed(state->bindings))` to its result. In `settings_path()`, name the pages `Keyboard`, `Binding`, `Press a key` and `Keep it?`.

`SettingsRow.binding` is `0` by default. Only `SETTINGS_ROW_BINDING` rows read it.

- [ ] **Step 5: Capture, the 10 s and live bindings on screen**

In `src/settings_pickers.c`, add `#include "bindings.h"` and `#include "util.h"` (and `platform/platform.h` on Windows), and:

```c
// The keys bindings.c knows by value, checked against SDL's own
SDL_COMPILE_TIME_ASSERT(key_return, BIND_KEY_RETURN == SDLK_RETURN);
SDL_COMPILE_TIME_ASSERT(key_backspace, BIND_KEY_BACKSPACE == SDLK_BACKSPACE);
SDL_COMPILE_TIME_ASSERT(key_left, BIND_KEY_LEFT == SDLK_LEFT);
SDL_COMPILE_TIME_ASSERT(key_right, BIND_KEY_RIGHT == SDLK_RIGHT);
SDL_COMPILE_TIME_ASSERT(key_up, BIND_KEY_UP == SDLK_UP);
SDL_COMPILE_TIME_ASSERT(key_down, BIND_KEY_DOWN == SDLK_DOWN);
SDL_COMPILE_TIME_ASSERT(key_application, BIND_KEY_APPLICATION == SDLK_APPLICATION);
SDL_COMPILE_TIME_ASSERT(key_menu, BIND_KEY_MENU == SDLK_MENU);
SDL_COMPILE_TIME_ASSERT(key_f1, BIND_KEY_F1 == SDLK_F1);
SDL_COMPILE_TIME_ASSERT(key_f12, BIND_KEY_F12 == SDLK_F12);
SDL_COMPILE_TIME_ASSERT(key_f13, BIND_KEY_F13 == SDLK_F13);
SDL_COMPILE_TIME_ASSERT(key_f24, BIND_KEY_F24 == SDLK_F24);

static Capture capture;
static int capture_device = -1;         // The device being captured from; -1 when no capture runs
static int pad_held = -1;               // The pad's control held last frame, for its releases
static Probation probation;
static int probation_device = BINDINGS_KEYBOARD;
static char probation_command[BINDINGS_COMMAND_MAX];
static bool picking_command = false;    // The list picker chooses a binding's command

// A function to name a key or button for the screen: a key by SDL's name ("Menu" for both of the
// Menu key's codes), a pad's control by its label; "#<HEX>" for a key SDL cannot name
void pickers_key_name(int device, int code, char *out, size_t size)
{
    if (device == BINDINGS_GAMEPAD) {
        snprintf(out, size, "%s", bindings_label(code));
        return;
    }
    if (code == BIND_KEY_APPLICATION || code == BIND_KEY_MENU) {
        snprintf(out, size, "Menu");
        return;
    }
    const char *name = SDL_GetKeyName((SDL_Keycode) code);
    if (name != NULL && name[0] != '\0')
        snprintf(out, size, "%s", name);
    else
        snprintf(out, size, "#%X", (unsigned int) code);
}

// A function to start capturing for a device: the key or pad control held now started it
void pickers_capture(int device)
{
    capture_device = device;
    capture_begin(&capture, SDL_GetTicks(), device == BINDINGS_KEYBOARD ? BIND_KEY_RETURN : gamepad_pressed_label());
    pad_held = device == BINDINGS_GAMEPAD ? gamepad_pressed_label() : -1;
    log_debug("Settings: capturing a %s", device == BINDINGS_KEYBOARD ? "key" : "button");
}

// A function to hand a capture's end to the pages: the key caught, or nothing
static void captured(int code)
{
    char name[64];
    pickers_key_name(capture_device, code, name, sizeof(name));
    if (capture_device == BINDINGS_KEYBOARD)
        log_debug("Settings: capture got %s (#%X)", name, (unsigned int) code);
    else
        log_debug("Settings: capture got %s", name);
    capture_device = -1;
    SettingsEvent event = settings_captured(host.model, code);
    host.event(&event);
}

// A function to start the 10 s for a change that took a key's navigation away
void pickers_probation(int device, int code, const char *command)
{
    char name[64];
    probation_begin(&probation, SDL_GetTicks(), code);
    probation_device = device;
    snprintf(probation_command, sizeof(probation_command), "%s", command);
    pickers_key_name(device, code, name, sizeof(name));
    log_debug("Settings: press %s again within 10 s to keep it", name);
}

// A function to take a key while settings are open: during a keyboard capture every key is the
// capture's; during the 10 s, the new key confirms it. True when the key was taken.
bool pickers_raw_key(int code, bool repeat)
{
    char name[64];
    if (capture_device == BINDINGS_KEYBOARD) {
        if (capture_press(&capture, SDL_GetTicks(), code, repeat))
            captured(code);
        return true;
    }
    if (capture_device == BINDINGS_GAMEPAD)
        return true;   // A pad's capture: keys wait
    if (probation_device == BINDINGS_KEYBOARD && !repeat && probation_press(&probation, code)) {
        pickers_key_name(BINDINGS_KEYBOARD, code, name, sizeof(name));
        log_debug("Settings: kept %s for %s", name, probation_command);
        return true;
    }
    return false;
}

// A function to take a key's release during a capture
void pickers_raw_release(int code)
{
    if (capture_device == BINDINGS_KEYBOARD)
        capture_release(&capture, code);
}

// A function to take the pad's state each frame: during a pad capture a newly held control is the
// capture's; during a pad's 10 s, its new control confirms it. True when the pad's input was taken.
bool pickers_raw_pad(int label)
{
    int was = pad_held;
    pad_held = label;
    if (capture_device == BINDINGS_GAMEPAD) {
        if (was >= 0 && was != label)
            capture_release(&capture, was);
        if (label >= 0 && label != was && capture_press(&capture, SDL_GetTicks(), label, false))
            captured(label);
        return true;
    }
    if (capture_device == BINDINGS_KEYBOARD)
        return true;
    if (probation.active && probation_device == BINDINGS_GAMEPAD) {
        if (label >= 0 && label != was && probation_press(&probation, label))
            log_debug("Settings: kept %s for %s", bindings_label(label), probation_command);
        return true;
    }
    return false;
}

// A function to rebuild the launcher's hotkeys and gamepad controls from the bindings, as they now are:
// the exit hotkey let go and taken again on Windows, the gamepad's defaults added again
void apply_bindings(const Bindings *bindings)
{
#ifdef _WIN32
    clear_exit_hotkey();
#endif
    clear_hotkeys();
    clear_gamepad_controls();
    int keys = 0;
    int controls = 0;
    char code[16];
    for (int i = 0; i < bindings_count(bindings, BINDINGS_KEYBOARD); i++) {
        const Binding *b = bindings_at(bindings, BINDINGS_KEYBOARD, i);
        if (b->removed)
            continue;
        snprintf(code, sizeof(code), "#%X", (unsigned int) b->code);
        add_hotkey(code, b->command);
        keys++;
    }
    for (int i = 0; i < bindings_count(bindings, BINDINGS_GAMEPAD); i++) {
        const Binding *b = bindings_at(bindings, BINDINGS_GAMEPAD, i);
        if (b->removed)
            continue;
        add_gamepad_control(bindings_label(b->code), b->command);
        controls++;
    }
    if (gamepad_running())
        add_default_gamepad_controls();
#ifdef _WIN32
    if (has_exit_hotkey())
        register_exit_hotkey();
#endif
    log_debug(keys == 1 ? "Settings: the bindings now hold %i hotkey and %i controls" : "Settings: the bindings now hold %i hotkeys and %i controls",
        keys, controls);
}

// A function to open the command picker for the binding page's binding
void pickers_open_binding_command(void)
{
    close_picker();
    picking_command = true;
    list = listpick_create();
    if (list == NULL)
        return;
    fill_commands();
    char custom[LISTPICK_TEXT_MAX + 16];
    const char *now = settings_binding_command(host.model);
    snprintf(custom, sizeof(custom), "Custom: %s", now);
    listpick_select(list, now, custom);
    kind = PICKER_LIST;
    list_first = 0;
}
```

In `list_command()`, a binding's command goes to the model, not to a slot. At the top of the `LISTPICK_CHOSEN` handling, add:

```c
    if (result == LISTPICK_CHOSEN && picking_command) {
        char chosen[LISTPICK_TEXT_MAX];
        snprintf(chosen, sizeof(chosen), "%s", listpick_chosen(list));
        close_picker();
        SettingsEvent event = settings_bind_command(host.model, chosen);
        host.event(&event);
        return;
    }
```

In `close_picker()`, set `picking_command = false;`. In `pickers_path()`, `slot` is NULL while picking a binding's command, and the path is the page's.

In `pickers_tick()`, before the font scan's work, run the clocks:

```c
    if (capture_device >= 0 && capture_expired(&capture, SDL_GetTicks())) {
        capture_device = -1;
        log_debug("Settings: the capture caught nothing in 5 s");
        settings_capture_ended(host.model, "Nothing was pressed in 5 s");
    }
    if (probation_expired(&probation, SDL_GetTicks())) {
        char name[64];
        pickers_key_name(probation_device, probation.code, name, sizeof(name));
        log_debug("Settings: the binding went back: %s was not pressed again within 10 s", name);
        SettingsEvent event = settings_revert_binding(host.model);
        host.event(&event);
    }
```

In `pickers_note()`, when nothing else is said, count the capture and the 10 s down:

```c
    if (note[0] == '\0' && capture_device >= 0)
        snprintf(note, sizeof(note), "Press the key or button" "\xE2\x80\xA6" " (%u s)",
            (BINDINGS_CAPTURE_MS - (SDL_GetTicks() - capture.started) + 999) / 1000);
    else if (note[0] == '\0' && probation.active) {
        char name[64];
        pickers_key_name(probation_device, probation.code, name, sizeof(name));
        snprintf(note, sizeof(note), "Press %s again within %u s to keep it", name,
            (BINDINGS_CONFIRM_MS - (SDL_GetTicks() - probation.started) + 999) / 1000);
    }
```

`pickers_note()` is now read every frame. `draw_caption()` asks the pickers first whenever a capture or the 10 s are running: its condition becomes `pickers_active() || pickers_busy()`, with `bool pickers_busy(void) { return capture_device >= 0 || probation.active; }`. In `pickers_end()`, reset `capture_device = -1;` and `probation.active = false;`.

Declare in `settings_pickers.h`: `pickers_key_name`, `pickers_capture`, `pickers_probation`, `pickers_raw_key`, `pickers_raw_release`, `pickers_raw_pad`, `pickers_busy`, `apply_bindings` and `pickers_open_binding_command`.

In `src/settings_screen.c`:
- Add `static Bindings *bindings = NULL;` with the file's statics.
- In `settings_open()`, after `list_pads();`, load the bindings from the file as it is now:

```c
    // The bindings come from the file's lines, which the save finds again by their text
    bindings = bindings_create(
#ifdef _WIN32
        true,
#else
        false,
#endif
        gamepad_running());
    size_t length = 0;
    char *text = fileio_read_all(config.config_path, &length);
    IniDoc *doc = text != NULL ? inidoc_parse(text, length) : NULL;
    alloc_free(text);
    if (bindings != NULL && doc != NULL) {
        static const char *const skip[] = { SETTING_GAMEPAD_ENABLED, SETTING_GAMEPAD_DEVICE, SETTING_GAMEPAD_MAPPINGS_FILE, NULL };
        int count = inidoc_list(doc, "Hotkeys", NULL, NULL, 0);
        IniDocItem *items = calloc((size_t) (count > 0 ? count : 1), sizeof(IniDocItem));
        if (items != NULL) {
            inidoc_list(doc, "Hotkeys", NULL, items, count);
            bindings_load(bindings, BINDINGS_KEYBOARD, items, count);
            free(items);
        }
        count = inidoc_list(doc, "Gamepad", skip, NULL, 0);
        items = calloc((size_t) (count > 0 ? count : 1), sizeof(IniDocItem));
        if (items != NULL) {
            inidoc_list(doc, "Gamepad", skip, items, count);
            bindings_load(bindings, BINDINGS_GAMEPAD, items, count);
            free(items);
        }
        settings_set_bindings(model, bindings, pickers_key_name);
    }
    else
        log_error("Settings: the bindings could not be read from %s", config.config_path);
    inidoc_free(doc);
```

- In `free_screen()`, after `settings_free(model);`, `bindings_free(bindings); bindings = NULL;`.
- In `handle_event()`, add:

```c
        case SETTINGS_EVENT_CAPTURE:
            pickers_capture(event->device);
            return;
        case SETTINGS_EVENT_PICK_COMMAND:
            pickers_open_binding_command();
            return;
        case SETTINGS_EVENT_BINDINGS:
            apply_bindings(bindings);
            if (event->confirm)
                pickers_probation(event->device, event->code, settings_binding_command(model));
            break;
```

  In the `DISCARD` case, after `apply_all();`, add `if (bindings != NULL) apply_bindings(bindings);`.
- In `save_changes()`, add the list edits for both devices and save them with the rest:

```c
    ConfigListEdit lists[2 * BINDINGS_MAX_EDITS];
    static char list_values[2 * BINDINGS_MAX_EDITS][BINDINGS_VALUE_MAX];
    int list_count = 0;
    if (bindings != NULL) {
        list_count = bindings_edits(bindings, BINDINGS_KEYBOARD, lists, list_values, BINDINGS_MAX_EDITS);
        list_count += bindings_edits(bindings, BINDINGS_GAMEPAD, lists + list_count, list_values + list_count, BINDINGS_MAX_EDITS);
    }
```

  with `#define BINDINGS_MAX_EDITS 64` at the top of the file. Call `config_save_all(..., edits, n, lists, list_count, &result)` in both branches, in place of `config_save(...)`. After a save that succeeded, log each note:

```c
        for (char *line = result.notes; line[0] != '\0';) {
            char *end = strchr(line, '\n');
            if (end != NULL)
                *end = '\0';
            log_debug("Settings: %s", line);
            if (end == NULL)
                break;
            line = end + 1;
        }
```

  The success line's count, `Settings saved %i change(s)`, becomes `n + list_count`.
- In `draw_row()`, show › on the Command row: in the value's formatting, treat `row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_BIND_COMMAND` and `row->kind == SETTINGS_ROW_BINDING` like `SETTINGS_ROW_LINK`.
- Add the routes the launcher calls:

```c
// A function to take a key before anything else does while settings are open: a capture's, or the
// 10 s's; true when it was taken
bool settings_raw_key(int code, bool repeat)
{
    return model != NULL && pickers_raw_key(code, repeat);
}

// A function to take a key's release while settings are open
void settings_raw_release(int code)
{
    if (model != NULL)
        pickers_raw_release(code);
}

// A function to take the pad's state each frame while settings are open; true when it was taken
bool settings_raw_pad(int label)
{
    return model != NULL && pickers_raw_pad(label);
}
```

  and declare the three in `settings_screen.h`.

- [ ] **Step 6: Route raw keys and the pad in the launcher**

In `src/launcher.c`:
- In `handle_keypress()`, right after the debug log line, let a capture or the 10 s take the key:

```c
    // A capture takes every key, and the 10 s the key that confirms them, before any other meaning
    if (settings_is_open() && settings_raw_key(key->sym, repeat))
        return;
```

- In the event loop, add the key's release:

```c
                case SDL_KEYUP:
                    if (settings_is_open())
                        settings_raw_release(event.key.keysym.sym);
                    break;
```

- Add above `poll_gamepad()`:

```c
// A function to find the first control held on any open pad, as a gamepad label's index (util.c's
// table's order, which bindings.c shares); -1 for none
int gamepad_pressed_label()
{
    for (int i = 0; i < gamepad_label_count(); i++) {
        const struct gamepad_info *control = gamepad_label_info(i);
        for (Gamepad *pad = gamepads; pad != NULL; pad = pad->next) {
            if (pad->controller == NULL)
                continue;
            bool held = control->type == TYPE_BUTTON
                        ? SDL_GameControllerGetButton(pad->controller, (SDL_GameControllerButton) control->index) != 0
                        : (control->type == TYPE_AXIS_POS ? 1 : -1) * SDL_GameControllerGetAxis(pad->controller, (SDL_GameControllerAxis) control->index) > GAMEPAD_DEADZONE;
            if (held)
                return i;
        }
    }
    return -1;
}
```

  and declare it in `launcher.h`.
- In the main loop, a capture or the 10 s take the pad before its controls run:

```c
            if (gamepads != NULL && !(settings_is_open() && settings_raw_pad(gamepad_pressed_label())))
                poll_gamepad();
```

- In `test_pad_update()`, hold the button `STREAMFLEX_TEST_PAD_BUTTON` names, else Start:

```c
    const char *name = getenv("STREAMFLEX_TEST_PAD_BUTTON");
    SDL_GameControllerButton button = name != NULL ? SDL_GameControllerGetButtonFromString(name) : SDL_CONTROLLER_BUTTON_START;
    if (test_pad != NULL && button != SDL_CONTROLLER_BUTTON_INVALID)
        SDL_JoystickSetVirtualButton(test_pad, button, file_exists(held) ? SDL_PRESSED : SDL_RELEASED);
```

- [ ] **Step 7: See the tests and the harness pass**

Build and run the unit tests: `100% tests passed`. Run the **Linux unit tests** (label `t14`): `warnings outside src/external: 0`. The MSVC job must also accept the `SDL_COMPILE_TIME_ASSERT`s. They are compile-time, so the Windows build is their check: build Release locally (Global Constraints).

Run the headless harness (labels `t14`, `t14-fedora`) and the leak pass (`t14-leaks`, `t14-fedora-leaks`). Expected: `0 failed`. The five binding checks pass, and `f50-hotkey`, `f50-heldhotkey` and the pad checks still pass on the rebuilt lists.

- [ ] **Step 8: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/settings.h src/settings.c src/settings_pickers.h src/settings_pickers.c src/settings_screen.h src/settings_screen.c src/util.h src/util.c src/launcher.h src/launcher.c src/platform/platform.h src/platform/win32.c tests/test_settings.c tests/CMakeLists.txt tests/headless/checks/55-settings-pages.sh tests/headless/checks/62-settings-bindings.sh tests/headless/fixtures/f62-keys.ini tests/headless/fixtures/f62-up.ini tests/headless/fixtures/f62-pad.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: key and gamepad bindings: capture, confirmation and the safety floor"
```

### Task 15: The mappings file, chosen in the folder browser

*Mappings file ›* opens 3a's folder browser in a new mode, `BROWSER_FILE`. That mode lists every file that is not hidden, beside the folders, and opens at the file the setting names, highlighted. Choosing one sets `ControllerMappingsFile`, which applies at next start, since SDL can add mappings but not remove them. The caption says so when it changes.

**Files:**
- Modify: `src/browser.h`, `src/browser.c` (`BROWSER_FILE`, `BROWSER_ROW_FILE`)
- Modify: `src/settings.c` (`settings_choose()`'s notice for `SET_FLAG_NEXT_START`)
- Modify: `src/settings_screen.c` (`open_browser()`, `preview_highlighted()`, `draw_browser_rows()`)
- Test: `tests/test_browser.c`, `tests/test_settings.c`
- Create: `tests/headless/checks/63-settings-mappings.sh`, `tests/headless/fixtures/f63-mappings.ini`

**Interfaces:**
- Consumes: 3a's browser (`browser_open()`, `load_folder()`, `browser_parent()`).
- Produces: `BrowserMode` gains `BROWSER_FILE`, and `BrowserRowKind` gains `BROWSER_ROW_FILE`. `settings_choose()` leaves the notice "This applies at next start" for a setting flagged `SET_FLAG_NEXT_START`.

- [ ] **Step 1: Write the failing tests**

Add to `tests/test_browser.c`, above `main()`, over its pretend `/home/me/Pictures`. That folder holds the folders `Autumn/` and `Birthdays/`, and the files `beach.JPG`, `.hidden.png`, `notes.txt`, `zebra.png` and `apple.webp`.

```c
// A function to test file mode: every file that is not hidden, after the folders, whatever its kind;
// opened at a file, its folder with it highlighted; OK on a file chooses it
static void test_file_mode(void)
{
    Browser *browser = browser_open(BROWSER_FILE, "/home/me/Pictures/notes.txt", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK(browser != NULL);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures");
    CHECK_INT(browser_row_count(browser), 6);
    CHECK_INT(browser_row(browser, 0)->kind, BROWSER_ROW_FOLDER);
    CHECK_STR(browser_row(browser, 0)->name, "Autumn");
    CHECK_STR(browser_row(browser, 1)->name, "Birthdays");
    CHECK_INT(browser_row(browser, 2)->kind, BROWSER_ROW_FILE);
    CHECK_STR(browser_row(browser, 2)->name, "apple.webp");
    CHECK_STR(browser_row(browser, 4)->name, "notes.txt");        // Not an image, and listed
    CHECK_STR(browser_row(browser, 5)->name, "zebra.png");        // .hidden.png is left out
    CHECK_INT(browser_cursor(browser), 4);
    CHECK_INT(browser_command(browser, BROWSER_DOWN, 5), BROWSER_MOVED);
    CHECK_INT(browser_command(browser, BROWSER_OK, 5), BROWSER_CHOSEN);
    CHECK_STR(browser_chosen(browser), "/home/me/Pictures/zebra.png");
    browser_free(browser);
}
```

and call it from `main()`.

Add to `tests/test_settings.c`:

```c
// A function to test that a setting that applies at next start says so when it changes
static void test_next_start(void)
{
    SettingsState *state = open_model();
    SettingSlot *slot = settings_slot(state, SET_ID_GAMEPAD_MAPPINGS, -1);
    CHECK_INT(settings_choose(state, slot, "/pads/new.txt").kind, SETTINGS_EVENT_CHANGED);
    CHECK_STR(settings_notice(state), "This applies at next start");
    CHECK_INT(settings_choose(state, settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1), "/a.png").kind, SETTINGS_EVENT_CHANGED);
    CHECK_STR(settings_notice(state), "");
    settings_free(state);
}
```

and call it from `main()`.

Create `tests/headless/fixtures/f63-mappings.ini`:

```ini
[General]
DefaultMenu=Main

[Gamepad]
Enabled=true
ControllerMappingsFile=/home/tester/pads/old.txt

[Main]
Entry1=One;apps;:quit
```

Create `tests/headless/checks/63-settings-mappings.sh`:

```bash
# The mappings file (3b): chosen in the folder browser's file mode, saved, and marked as applying at
# next start

rm -rf "$TESTER_HOME/pads"
mkdir -p "$TESTER_HOME/pads"
printf '# no mappings\n' > "$TESTER_HOME/pads/old.txt"
printf '# no mappings either\n' > "$TESTER_HOME/pads/new.txt"
chown -R tester:tester "$TESTER_HOME/pads"

# Controls > Gamepad > Mappings file: the browser opens on old.txt; Up to new.txt, OK chooses it
cfg=$(writable_config f63-mappings)
CFG=$cfg run_keys f63-mappings Menu Down Down Down Down Down Down Down Down Return Down Return Down Down Return Up Return \
    BackSpace BackSpace BackSpace
ok=1
grep -qx 'ControllerMappingsFile=/home/tester/pads/new.txt' "$cfg" \
    && grep -q 'Settings: the note under the preview says This applies at next start' "$out/f63-mappings.log" \
    && grep -q 'Settings: browsing /home/tester/pads' "$out/f63-mappings.log" && ran_clean f63-mappings && ok=0
result "mappings: a file chosen in the browser saves, and applies at next start (exit $(cat "$out/f63-mappings.code"))" $ok
```

`Down Down` from *On* reaches *Mappings file*: *Device* is between them, and the gamepad is on.

- [ ] **Step 2: Run the tests to see them fail**

Build. Expected: `'BROWSER_FILE': undeclared identifier`.

- [ ] **Step 3: Add file mode**

In `src/browser.h`:
- add `BROWSER_FILE     // Choosing one file of any kind` to `BrowserMode`;
- add `BROWSER_ROW_FILE` to `BrowserRowKind`.

In `src/browser.c`'s `load_folder()`, gather files in file mode where it gathers images. Replace `else if (browser_is_image_file(&entries[i]))` with:

```c
        else if (browser->mode == BROWSER_FILE || browser_is_image_file(&entries[i]))
```

and in the loop that adds them, give a file mode row its own kind and let it be chosen:

```c
    for (int i = 0; ok && i < image_count; i++) {
        char *path = join_path(copy, images[i]->name);
        bool choosable = browser->mode == BROWSER_IMAGE || browser->mode == BROWSER_FILE;
        const char *why = path != NULL && choosable ? why_not(browser, path) : NULL;
        ok = add_row(rows, &row_count, browser->mode == BROWSER_FILE ? BROWSER_ROW_FILE : BROWSER_ROW_IMAGE,
                     images[i]->name, path, choosable && why == NULL, why);
    }
```

In `browser_open()`, open a file's folder with the file highlighted, as image mode does for an image:

```c
        if ((mode == BROWSER_IMAGE && browser_is_image(folder)) || mode == BROWSER_FILE) {
```

A file mode start is always a file (the setting's value), so its parent is listed.

- [ ] **Step 4: Open it for the mappings file, and say it applies at next start**

In `src/settings_screen.c`'s `open_browser()`, choose the mode by setting:

```c
    BrowserMode mode = slot->def->id == SET_ID_BACKGROUND_IMAGE ? BROWSER_IMAGE
                     : slot->def->id == SET_ID_GAMEPAD_MAPPINGS ? BROWSER_FILE : BROWSER_FOLDER;
```

In `preview_highlighted()`, a file row previews nothing: the existing code sets `path` only for image and folder rows, so a file row leaves `path` empty, and the preview shows the real background. In `draw_browser_rows()`, a `BROWSER_ROW_FILE` draws as an action row, as an image does, which the existing `opens ? LINK : ACTION` already gives.

In `src/settings.c`'s `settings_choose()`, before `return event;` at the end, say when a change waits for the next start:

```c
    if (slot->def->flags & SET_FLAG_NEXT_START)
        snprintf(state->notice, sizeof(state->notice), "This applies at next start");
```

- [ ] **Step 5: See the tests and the harness pass**

Build and run the unit tests: `100% tests passed`. Run the **Linux unit tests** (label `t15`): `warnings outside src/external: 0`. Run the headless harness (labels `t15`, `t15-fedora`) and the leak pass (`t15-leaks`, `t15-fedora-leaks`): `0 failed`.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/browser.h src/browser.c src/settings.c src/settings_screen.c tests/test_browser.c tests/test_settings.c tests/headless/checks/63-settings-mappings.sh tests/headless/fixtures/f63-mappings.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: the gamepad's mappings file, chosen in the folder browser"
```

### Task 16: Documentation, the hands-on checks, and the finish

This task documents everything 3b added, and writes the hands-on checklists for sf-test. It then runs the whole verification: the Windows build, the unit tests, both harness images with their leak passes, and the whole-branch review. Last, it hands the build to sf-test.

**Files:**
- Modify: `docs/configuration.md`, `CHANGELOG.md`, `CONTRIBUTING.md`
- Create: `tests/qa/checklists/windows-3b-hands-on.md`, `tests/qa/checklists/ubuntu-3b-hands-on.md`
- Modify: `tests/qa/README.md` (lists the new checklists)

**Interfaces:**
- Consumes: everything above.
- Produces: the documentation and the checklists; the verification record in the SDD workspace (`.superpowers/sdd/2026-09-30-settings-all/`).

- [ ] **Step 1: Document every page, picker and binding in `docs/configuration.md`**

In **The Settings Screen** (lines 43-86):
- Rewrite *What it changes* (lines 62-67) as one sub-section per top-level row, in the screen's order: General, Background, Menus, Titles, Highlight, Scroll indicators, Clock, Screensaver, Controls. Each lists its rows with the steps from Global Constraints' exact values, and says which rows are greyed until a switch is on.
- Add **### The pickers**:
  - the colour picker: the 24 swatches by name, *Custom #RRGGBB*, the hex editor's keys, and Back's putting the colour back;
  - the contrast warning (advisory, 3:1, against the background colour or an image's mean luminance with the overlay);
  - the command picker's rows, with `:exit` on Windows only;
  - the font picker: installed fonts by family, the bundled fonts first, *Loading fonts… (N)* on the first opening, the Regular style written, and `FontFace=` for a face in a collection;
  - the list pickers for *Default menu* and *Device*.
- Add **### Key and button bindings**:
  - the Keyboard and Gamepad pages, and a binding's page;
  - capture (5 s; the starting key's repeats ignored; unknown keys refused, with the CEC note);
  - the safety floor (the arrows, OK and Back keep their meaning; the last way to a navigation command or to Settings stays);
  - the 10 s confirmation when Up, Down or the Menu key is taken;
  - that a new hotkey is numbered one above the highest `HotkeyN`.
- In **### Saving**, add: bindings are saved line by line, and a line changed or removed by hand while settings were open is kept, with the log saying so.

In **## Settings**:
- `FPSLimit` (line 118-119): "The minimum is 10" stays, and is now true.
- Titles `Font` (line 278) and Clock `Font` (line 553): add "`FontFace` picks a face inside a font collection (`.ttc`), counted from 0. Absent means 0. A config moved to another machine whose font is not there falls back to the bundled font, as its image paths do."
- Add a `FontFace` entry under both Titles and Clock.
- `ControllerMappingsFile` (line 671): add "Applies at next start: SDL can add mappings while running, but not take one back."
- `IconSpacing`, `Margin` and the opacities: say that a percentage may have up to two decimals (`12.5%`).

In **### Special Commands**, `:exit` (line 485): "Windows only, as a hotkey. Anywhere else (a Linux hotkey, or a menu entry on either platform) StreamFlex logs that it does nothing."

In `docs/compilation.md`, line 13 already reads `SDL ≥ 2.0.18` (Task 4).

- [ ] **Step 2: Record 3b in `CHANGELOG.md`**

Under `## [Unreleased]`, keeping its existing `### Changed` lines, add:

```markdown
### Added
- **Every setting on the settings screen.** Nine pages mirror `config.ini`: General, Background, Menus, Titles, Highlight, Scroll indicators, Clock, Screensaver and Controls. Every change shows at once, and Back saves only what changed. A row that depends on a switch that is off is greyed, and says why.
- **A colour picker:** 24 named swatches, a hex editor for any other colour, and a warning when a title or clock colour stands out too little from the background.
- **A font picker:** the installed fonts by family, each drawn in its own face. A face inside a font collection is written as the new `FontFace` key.
- **A command picker** for the startup and quit commands and for bindings: the special commands, every submenu, and every command the menus already run.
- **Key and gamepad bindings:** add, change and remove hotkeys and gamepad controls from the remote. Capture a key or button, and keep it. The arrows, OK and Back always keep their meaning. Taking Up, Down or the Menu key over must be confirmed within 10 seconds, or it goes back.

### Changed
- **The Linux SDL2 minimum is now 2.0.18,** for switching VSync without a restart.
- **The built-in highlight fill is one step less opaque** (alpha `0x3F`, not `0x40`), which is what `FillOpacity=25%` has always given.

### Fixed
- **`FPSLimit=10` works.** The documented minimum was refused, and anything up to 10 left VSync on.
- **A negative `[Clock] FontSize` is refused with a log line.** It used to wrap to a huge size.
- **`:exit` outside a Windows hotkey says why it does nothing,** where it used to do nothing silently.
- **Gamepads plugged in after another was removed are tracked correctly,** and the clock's background render is handed over safely.
```

- [ ] **Step 3: Update `CONTRIBUTING.md`'s project structure and tests**

In **## Project Structure** (lines 44-46), add the new modules to the settings screen's list: `settings_pickers.c`, `listpick.c`, `colorpick.c`, `fontlist.c`, `fontscan.c`, `bindings.c`, `config_fields.c`, `derive.c`. Line 11's pure modules gain `derive`, `listpick`, `colorpick`, `fontlist` and `bindings`. Name the new unit tests: `test_derive`, `test_listpick`, `test_colorpick`, `test_fontlist` and `test_bindings`.

- [ ] **Step 4: Write the hands-on checklists for sf-test**

Create `tests/qa/checklists/windows-3b-hands-on.md` and `tests/qa/checklists/ubuntu-3b-hands-on.md`. Follow the 3a checklists' form: their *How to read and run each step* conventions (inputs, frames, the log's marks, verdicts) apply unchanged, and each file says so in its first section. The steps, the same on both unless marked:
1. **B0. Setup and record (no verdict):** as 3a's W0/U0 (the build, the guest, the GPU, the baseline config copy), with the pad attached.
2. **B1. Rebind a key.**
   - Controls › Keyboard › Add binding › Key: press F5, then Keep.
   - Command ›: pick *Quit StreamFlex*.
   - Back out to save.

   Expected: the log's `Settings: capture got F5 (#4000003E)`; the file's `[Hotkeys]` gains `HotkeyN=#4000003E;:quit` and nothing else changes. After a restart, F5 quits.
3. **B2. Rebind a pad button.** The same, on Controls › Gamepad, pressing the pad's **Y**, bound to *Home*. Expected: `[Gamepad]` gains `ButtonY=:home`. After a restart, Y goes home.
4. **B3. The safety floor.**
   - Try to bind Backspace. Expected: refused, with the caption's reason.
   - Bind Up to *Quit StreamFlex* while nothing else goes up. Expected: refused, *That would leave no key for Up*.
5. **B4. Pick a system font.**
   - Titles › Font: the list loads (*Loading fonts… (N)*, then the families, each in its own face).
   - Pick one that is not bundled: on Windows *Segoe UI*, on Ubuntu *Ubuntu* or *DejaVu Serif*.

   Expected: the titles change at once; `[Titles] Font=` names the system file, with `FontFace=` only for a collection's later face. **Frame** B4.
6. **B5. A colour by hex.** Titles › Colour › Custom: type `#FFD700`, then Keep. Expected: the titles turn gold in the preview and after the save. Then choose Black: the caption warns of low contrast against the black background.
7. **B6. Turn the clock on.** Clock › Show: On, with Show date On. Expected: the clock appears at once and the menu moves below it. **Frame** B6.
8. **B7. Restart and compare.** Quit, and start again with `-d`. Expected:
   - every change from B1-B6 is in force;
   - `config.ini` differs from the baseline only in those lines (`Compare-Object` on Windows, `diff` on Ubuntu);
   - its comments and order are unchanged.
9. **B8 (Windows only). The exit hotkey.** Bind F9 to `:exit` (it is in the command picker on Windows). Start Notepad from a menu entry, press F9, and Notepad closes. Binding F12 to `:exit` is refused with the reason.
10. **B9. VSync live.** General › VSync Off, FPS limit 30. Expected: the log's `Frame timing: FPS limit 30, 33 ms a frame`, with no restart. Record the renderer's own VSync support: the log's `Video:` line.

Each checklist ends with a results table of step, verdict and notes, as the 3a checklists do. Add both files to `tests/qa/README.md`'s list.

- [ ] **Step 5: Run the whole verification, in parallel where the host allows**

Check host memory, and announce the batch to qa-harness, qa-test and sf-test (Global Constraints). Then run:
1. The Windows build and unit tests (Global Constraints): `100% tests passed`, and the MSVC Release build with no warning.
2. The **Linux unit tests** (label `final-unit`): `warnings outside src/external: 0` and `100% tests passed`.
3. The headless harness on Debian and Fedora (labels `final`, `final-fedora`): `0 failed` each.
4. The leak pass on both (labels `final-leaks`, `final-fedora-leaks`): `0 failed`, with no `LEAK` line.
5. `scrollfail` on Debian (label `final-scrollfail`): `0 failed`.

Each run's `N failed` line and its PASS count go into the SDD workspace's `progress.md`. A failure is fixed with a failing check first, then every run is repeated in full. Never trim a run: parallelise instead.

- [ ] **Step 6: Commit, and hand the build to sf-test**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add docs/configuration.md CHANGELOG.md CONTRIBUTING.md tests/qa/checklists/windows-3b-hands-on.md tests/qa/checklists/ubuntu-3b-hands-on.md tests/qa/README.md
git -C C:/Users/jscha/source/repos/streamflex commit -m "docs: every setting on the settings screen, the pickers and bindings"
```

Push the branch and open the PR (not armed: the hands-on comes first). Once CI's Windows zip and Debian package are built:
1. Download them.
2. Record their SHA-256 (`Get-FileHash -Algorithm SHA256`).
3. Send sf-test, with `SendMessage`:
   - the PR's head commit;
   - the artifacts' paths and checksums;
   - the two checklists' paths, in the order Windows then Ubuntu;
   - what it must not touch: the repo, the PR, and any file outside its guests.

Every finding sf-test returns is fixed on the branch with a failing check first, re-verified (Step 5's runs), and handed back to sf-test to check again. When every step passes, the PR is ready for the final whole-branch review and the user's merge.
