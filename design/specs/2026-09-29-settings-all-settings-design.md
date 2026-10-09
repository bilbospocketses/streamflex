# Every setting on the settings screen (3b): design

**Date:** 2026-09-29
**Status:** approved in brainstorming, awaiting spec review
**Covers:** sub-project 3b of the overhaul (todo item 7): every `config.ini` setting that 3a left file-only, the color picker, the font picker, and key and gamepad bindings.
**Builds on:** `design/specs/2026-09-27-settings-screen-design.md` (3a) as built on `feat/settings-screen`. **The implementation plan is written after 3a merges**, because it names exact functions and lines that 3a's fix wave is still changing.

## Context

3a built the settings screen: a narrow column of rows on the left, a large live preview on the right, a settings table (`SettingDef`, `src/settings.c`), a line-preserving config writer (`src/inidoc.c`), a folder browser, and live refresh groups (layout, titles, background). It covers the background, each menu's grid and the title size. 3b makes everything else in `config.ini` editable from the remote. 3c (menu editing) follows. The binding decisions still hold: arrows, OK and Back only, with no typing and no mouse; changes apply live and Back saves them; the look is ours.

A read-only map of every remaining setting (its parse, its consumers, and what applying it live takes) was made on 2026-09-29 against `feat/settings-screen`. Its findings shaped the design below.
- **59 fixed keys** remain: General 11, Background 4, Layout 2, Titles 8, Highlight 9, Scroll Indicators 5, Clock 13, Screensaver 4, Gamepad 3. On top of those come the `[Hotkeys]` and `[Gamepad]` control lists.
- **Most cannot change live today.**
  - `validate_settings()` overwrites configured values in place: percentages become px, opacity is baked into alpha, and paddings are clamped. Its `< 0` sentinels are spent after one run, so it cannot run twice.
  - The overlay, highlight, scroll indicators, clock, screensaver and gamepad are set up only when enabled at startup, so enabling one live would dereference NULL (`launcher.c:849`, `:1021`, `:1035`, `:1368`).
  - The clock captures its font and formats once, in `init_clock()`.
  - The renderer's VSync flag is fixed at creation.
- **Existing bugs on this path:**
  - `FPSLimit` needs a value above 10, while the docs say "at least 10" (`util.c:178`, `launcher.c:254`, `docs/configuration.md:117`);
  - a negative `[Clock] FontSize` passes the check and wraps to a huge unsigned size (`util.c:420-422`);
  - `:exit` is a silent no-op on Linux: `execute_command()` has no branch for it, and only Windows routes it (`util.c:832`).

## Decisions

| Question | Decision |
|---|---|
| Settings holding free text (commands, the mappings file, fonts) | **Chosen from lists.** Commands come from a command picker, the mappings file from 3a's folder browser, and fonts from the font picker. A value in the file that matches nothing appears as *Custom: …* and is kept until the user steps off it, as 3a does for colors. `TimeFormat` and `DateFormat` are already choices (`24hr`/`12hr`/`Auto`, `Big`/`Little`/`Auto`) and need nothing new. |
| The color picker | **A grid of preset swatches, plus a hex editor.** Opacity stays its own row, and the caption gives an advisory contrast warning. |
| How the pages are organized | **One page per config section.** Nine top-level rows mirror `config.ini` and the docs. |
| Hotkeys and gamepad controls | **Full add, change and remove,** with a safety floor so navigation can never be lost. |
| The font picker | **The installed system fonts, listed by family,** found with no new dependencies, each drawn in its own face. |
| What the font picker writes | **The face's file path,** as today, plus a new optional `FontFace=` for a face inside a collection. Startup stays instant. A config moved to another machine falls back to the bundled font, as its image paths and commands already do. |
| Binding capture | **Capture, then confirm, with a safety floor** (see **Bindings**). |
| Architecture | **The settings table plus a shared list-picker model.** The color picker has its own small model. The pickers' drawing and capture go in a new `settings_pickers.c`, so `settings_screen.c` (1,114 lines after 3a) keeps the pages. |
| Settings that cannot apply live | **The Linux SDL2 minimum rises from 2.0.14 to 2.0.18**, so VSync switches live (`SDL_RenderSetVSync()`). **`ControllerMappingsFile` alone applies at next start**, because SDL can add mappings but not remove them. Its row and the caption say so. |

## Pages and rows

The top level: *General ›*, *Background ›*, *Menus ›*, *Titles ›*, *Highlight ›*, *Scroll indicators ›*, *Clock ›*, *Screensaver ›*, *Controls ›*, a divider, then *Discard changes*. Each row shows a summary of its page, as in 3a.

Rows marked › open a picker; the rest step with Left and Right, with the limits below. **A row that depends on a switch that is off is grayed, not hidden,** so the column never jumps. For example, *Shadow color* is grayed while *Shadows* is off. Every value the file holds that is not in a row's step list appears in its sorted place, or as *Custom*, as 3a's rows do.

- **General:**
  - *Default menu* steps through the menus, in file order.
  - *Wrap around* (`WrapEntries`), *Reset on Back*, *Mouse select*, *Block the OS screensaver* (`InhibitOSScreensaver`) and *VSync* are On/Off.
  - *After launching an app* (`OnLaunch`) steps through its existing modes.
  - *App timeout* steps through 3, 5, 10, 15, 20 and 30 s.
  - *FPS limit* steps through Off, 30, 60, 75, 120, 144, 165 and 240.
  - *Startup command ›* and *Quit command ›* open the command picker.
- **Background:**
  - 3a's rows stay.
  - *Overlay* is On/Off.
  - *Overlay color ›* opens the color picker.
  - *Overlay opacity* runs 0-100% in steps of 5.
  - *See-through color ›* (`ChromaKeyColor`) is shown in Transparent mode only.
- **Menus:** 3a's rows stay. The *All menus* page adds *Icon spacing* (0-10% of the screen width, in steps of 1) and *Vertical center* (25-75%, in steps of 5).
- **Titles:**
  - *Size* is 3a's row.
  - *Show titles* and *Shadows* are On/Off.
  - *Font ›* opens the font picker.
  - *Color ›* and *Shadow color ›* open the color picker.
  - *Opacity* runs 0-100% in steps of 5.
  - *Too long* is Truncate or Shrink.
  - *Padding* runs 0-20% in steps of 2, with 3a's percentage meaning.
- **Highlight:**
  - *Show* is On/Off.
  - *Fill color ›* and *Outline color ›* open the color picker.
  - *Fill opacity* and *Outline opacity* run 0-100% in steps of 5.
  - *Outline size* runs 0-10 px.
  - *Corner radius* runs 0-100 in steps of 5.
  - *Vertical padding* and *Horizontal padding* run 0-100 px in steps of 5.
- **Scroll indicators:**
  - *Show* is On/Off.
  - *Fill color ›* and *Outline color ›* open the color picker.
  - *Outline size* runs 0-10 px.
  - *Opacity* runs 0-100% in steps of 5.
- **Clock:**
  - *Show*, *Show date*, *Shadows* and *Weekday* are On/Off.
  - *Alignment* steps through its modes.
  - *Font ›* opens the font picker.
  - *Size* runs 20-120 in steps of 5.
  - *Color ›* and *Shadow color ›* open the color picker.
  - *Opacity* runs 0-100% in steps of 5.
  - *Margin* runs 0-10% in steps of 1.
  - *Time* shows as *14:05*, *2:05 PM* or *Auto*.
  - *Date* shows as *Sep 28*, *28 Sep* or *Auto*.
- **Screensaver:**
  - *On* and *Pause slideshow* are On/Off.
  - *Idle time* steps through 3, 5, 10, 15 and 30 s, then 1, 2, 5, 10 and 15 min.
  - *Dim level* (`Intensity`) runs 10-100% in steps of 10.
  - While this page is open, the preview shows the dim level.
- **Controls:**
  - *Keyboard ›* opens the hotkey list.
  - *Gamepad ›* opens a page with:
    - *On*;
    - *Device*: *Any*, then each connected pad by name;
    - *Mappings file ›*, which opens the folder browser, and is marked "applies at next start";
    - the control list.

**The command picker** lists:
- *None*;
- the navigation commands (`:left`, `:right`, `:up`, `:down`, `:select`, `:back`, `:home`);
- `:settings`, `:quit`, `:shutdown`, `:restart` and `:sleep`;
- *Open submenu: <name>* for every menu;
- on Windows only, `:exit`;
- then every command the menus' entries run, each labeled with its entry's title and deduplicated.

## Applying every setting live

### Configured and effective values

- **`Config` holds exactly what the file says,** and nothing converts it in place.
- A new pure function, `derive_settings(const Config *, screen size) → Effective`, computes what the launcher draws with:
  - the px values of percentages;
  - the alpha bytes of opacities;
  - the clamped paddings, outline sizes and corner radius.
- It runs at startup and after any change, as often as needed.
- A clamp limits only the effective value, so raising a padding after a clamp restores it. The `-1` sentinels go.
- `validate_settings()` keeps only the checks that belong to parsing, and everything it converted moves into `derive_settings()`.

### A start, stop and reload for each feature

- The overlay, highlight, scroll indicators, clock, screensaver and gamepad each get functions that are safe to call repeatedly: `start` sets the feature up, `stop` frees everything it holds, and `reload` is stop then start when running.
- **Startup calls the same `start`** as the screen does, as 3a did with `reload_background()`.
- Turning a feature on starts it, turning it off stops it, and changing one of its values reloads it.
- No code path touches a feature's state unless it is running.

### Refresh groups

3a's groups were layout, titles and background. 3b's are:

| Group | Refreshes |
|---|---|
| layout | the current menu's layout, as 3a |
| titles | the titles' textures, as 3a |
| title font | closes the size cache and the fixed font, reopens the font, measures its height per point again, then refreshes the titles |
| background | as 3a, now including the overlay texture |
| highlight | the highlight's texture |
| scroll indicators | the scroll indicators' texture |
| clock | the clock, restarted; then the layout area and the layout, since the clock's size moves the buttons |
| screensaver | the screensaver |
| gamepad | the gamepad |
| none | nothing: for values read on every event, such as *Wrap around*, *App timeout* and *Idle time* |

Each `SettingDef` names its group.

### Special cases

- **VSync:** `SDL_RenderSetVSync()`, possible because the Linux minimum rises to SDL 2.0.18. `CMakeLists.txt` and the packaging are updated, and the CHANGELOG says so.
- **FPS limit:** recomputes the frame timing, with the existing rule that an out-of-range limit uses VSync.
- **Block the OS screensaver:** `SDL_DisableScreenSaver()` or `SDL_EnableScreenSaver()`.
- **Gamepad On:** `SDL_InitSubSystem()` or `SDL_QuitSubSystem()` for the game controller subsystem, the repeat timing, and the default controls.
- **Device:** closes the open pad and connects the chosen one, by listing the pads present (`SDL_NumJoysticks()`) rather than waiting for connect events.
- **Default menu:** points `:home` at the new menu.
- **Mappings file:** applies at next start (see **Decisions**).
- **The clock re-renders at once after a change,** rather than at the next minute.

### Discard

*Discard changes* restores the configured values and the binding lists from the snapshot, then runs every group's reload.

## The pickers

Each picker is a pure model (no SDL types, no rendering and no globals, as 3a's modules are), unit-tested without SDL. `settings_pickers.c` draws them and runs capture.

### The shared list picker (`src/listpick.c`)

- The model holds the rows (each with a label, an optional value, whether it is enabled, and the reason when it is not), a cursor, paging with Left and Right, and a pinned *Custom: …* row for a file value that matches nothing.
- OK chooses and Back cancels.
- It serves the command picker, the font picker and the *Default menu* and *Device* lists.

### The color picker (`src/colorpick.c`)

- OK on a color row opens a **6 × 4 grid of named swatches**, moved with all four arrows. The swatches are:
  - 3a's ten presets: Black `#000000`, Charcoal `#1E1E1E`, Graphite `#33383D`, Slate `#2E3440`, Midnight `#121A2E`, Navy `#0B1F3A`, Teal `#07606C`, Forest `#1E3B2F`, Plum `#3B1F3A` and Burgundy `#4A1520`;
  - four neutrals: White `#FFFFFF`, Light gray `#C8C8C8`, Gray `#808080` and Dark gray `#4A4A4A`;
  - ten accents: Red `#D03030`, Orange `#E07020`, Amber `#F0B000`, Yellow `#F0E040`, Lime `#80C040`, Green `#30A050`, Cyan `#20B0C0`, Blue `#3070D0`, Indigo `#5048C0` and Pink `#D04890`.
- The preview shows the highlighted swatch as it moves, and the current color is marked.
- **The last row, *Custom #RRGGBB*,** opens the hex editor. Left and Right choose a digit, Up and Down step it through 0-F with wrapping, OK keeps the color, and Back cancels.
- Colors stay `#RRGGBB` in the file, as `hex_to_color()` reads them.
- **The contrast warning** is advisory: a title or clock color below 3:1 contrast (WCAG) against what lies behind it puts a warning in the caption. What lies behind it is:
  - the background color composited with the overlay;
  - for an image or slideshow, the image's mean luminance, measured when it is decoded.

### The font picker (`src/fontlist.c`, with platform listing)

- **Finding the fonts:** on a background thread the first time the picker opens, then kept for the session. Until it finishes, the picker shows *Loading fonts… (N)*.
  - **Windows:** the `Fonts` registry keys under HKLM and HKCU. Their value names are display names, so each file is still opened for its family.
  - **Linux and the Pi:** a scan of `/usr/share/fonts`, `/usr/local/share/fonts`, `~/.local/share/fonts` and `~/.fonts`.
  - Each file is opened with `TTF_OpenFontIndex()` for every face (`TTF_FontFaces()`) to read its family and style (`TTF_FontFaceFamilyName()`, `TTF_FontFaceStyleName()`).
  - There are no new dependencies.
- **What it lists:**
  - one row per family, drawn in its own face, loaded only for the rows on screen from a small cache;
  - the bundled font first;
  - files SDL_ttf cannot open, and symbol and emoji fonts (no glyph for the sample text), are skipped.
- **Choosing:** the Regular style when the family has one, otherwise its first style. The row writes that face's file path, and `FontFace=<index>` when the face's index is not 0, removing `FontFace` otherwise.
- `load_font()` gains the face index. A missing file falls back to the bundled font with a log line, as today, and the file's value is left as it is.

### Bindings

- **The Keyboard and Gamepad pages** list each binding as *key → command*, with *Add binding* at the top. OK on a binding opens its page: *Key* (capture), *Command ›* (the command picker), and *Remove*.
- **Capture:**
  - *Press the key or button… (5 s)*. The press that started the capture, and its release, are ignored.
  - A key SDL reports as unknown is refused with a reason. CEC remotes' OK and Back arrive as unknown keys on Linux, because SDL's evdev table has no mapping for `KEY_OK` and `KEY_EXIT`.
  - A timeout returns to the binding unchanged.
  - The captured key is shown on a confirm page: *Keep*, *Try again*, *Cancel*.
- **The safety floor:**
  - The arrows, OK (Return) and Back (Backspace) always keep their built-in meaning. A hotkey on Left, Right, Return or Backspace is refused with a reason, since the dispatcher checks those keys first and such a hotkey would never run.
  - The last binding of each navigation command, and of `:settings`, can be neither removed nor rebound to another command.
  - Binding Up, Down or the Menu key to a non-navigation command takes that key's navigation away, so it must be confirmed with the new key within 10 s, or it reverts.
- **Storage is unchanged:**
  - hotkeys stay `HotkeyN=#<hex keycode>;<command>`, with N one above the highest in the file;
  - gamepad controls stay `<Label>=<command>` with the 25 existing labels;
  - SDL's key names are for display only.
- **The Menu key** is both `0x40000065` (`SDLK_APPLICATION`, which SDL names "Application") and `0x40000076` (`SDLK_MENU`). 3a's fix wave (Batch C) makes both open settings, and 3b's capture shows either as *Menu*.

## The config writer and the parser

- **One parse path.** Every remaining key moves into the settings table and is read through `setting_parse()`, as 3a did for its keys, and `config_handler()`'s branches for them go. This fixes the three bugs:
  - `FPSLimit` accepts 10 and above, matching the docs;
  - a negative `[Clock] FontSize` is refused with a log line;
  - `:exit` on Linux logs that it is Windows-only.

  Invalid values otherwise keep today's behavior: they are ignored, with a log line.
- **List mode in `inidoc`.** Settings follow inih's rule that the last key wins, but `[Hotkeys]` ignores its key names and `[Gamepad]` allows a label twice. `inidoc` gains:
  - `inidoc_list(section)`: the section's lines in order, excluding `[Gamepad]`'s `Enabled`, `DeviceIndex` and `ControllerMappingsFile`;
  - `inidoc_list_set(line, key, value)`;
  - `inidoc_list_add(section, key, value)`, which adds after the section's last list line;
  - `inidoc_list_remove(line)`.

  Comments, order and line endings survive, as in 3a.
- **Saving lists** keeps 3a's rule of reading the file fresh. Each edited binding remembers its original line text:
  - a change or removal finds that line in the fresh file;
  - when a hand edit changed or removed that line meanwhile, a change is written as a new line and a removal is skipped, each with a log line;
  - additions are always written.
- **The binding lists in memory** gain replace and remove functions. `add_hotkey()`'s function-local static tail pointer goes. Snapshot and *Discard* cover both lists.
- **Nothing else changes in the writer:** only changed keys are touched, a value set back to its default is written explicitly, and 3a's refused-value rules apply.

## Errors and limits

- A font, image or mappings file that disappears later is handled as at startup: a log line and the fallback, with the file's value unchanged.
- The font scan skips unreadable files and folders. An empty result leaves only the bundled font.
- Capture never binds an unknown key, and never takes navigation away without the confirmation described in **Bindings**.
- **Debug output** (`-d`):
  - every change;
  - every refresh group run;
  - each feature's start and stop;
  - the font scan's count and time;
  - each capture, and its result.

## Testing

### Unit tests (CTest, no SDL: Windows, Debian and Pi CI)

- **`test_settings`** (extended): every new key text → value → text; the step lists and limits; the grayed-row rules; the three bug fixes.
- **`test_derive`** (new): `derive_settings()` gives the same result twice; a value clamped and then raised is restored; every percentage and opacity conversion is checked.
- **`test_listpick`**, **`test_colorpick`** and **`test_fontlist`** (new):
  - the list picker's paging and its *Custom* row;
  - the color grid's movement, the hex digits stepping and wrapping, and *Custom*;
  - grouping into families, choosing the Regular style, and collections, with a fake listing.
- **`test_bindings`** (new): the safety floor, refusing unknown keys, `HotkeyN` numbering, and the 10 s revert's state machine.
- **`test_inidoc`** (extended): list mode, and a list save after a hand edit.

### Headless harness (`tests/headless`, Debian and Fedora 44 in CI)

- Every page is opened and every row stepped once.
- Each feature is switched off and on and has a value changed, under ASan. **The leak pass proves that stopping a feature frees everything.**
- A binding is captured with `xdotool`, confirmed and saved, and the file's line is checked. The 10 s revert is checked too.
- A color is entered through the hex editor, and a pixel check confirms the preview shows it.
- The font picker is run against a fixture fonts folder.

### Hands-on (qa-harness, before the 3b merge)

The Windows 11 and Ubuntu guests, with a remote-style key driver and the virtual pad:
- rebind a key and a pad button;
- pick a system font, and a color by hex;
- turn the clock on;
- restart, and confirm that everything stuck and the rest of the file is unchanged.

## Documentation

- **`docs/configuration.md`:**
  - every page;
  - the pickers;
  - capture and the safety floor;
  - `FontFace=`;
  - the SDL 2.0.18 minimum;
  - the mappings file applying at next start;
  - the corrected `FPSLimit` minimum;
  - `:exit` being Windows-only.
- **`CHANGELOG.md`:**
  - *Added:* every setting on the settings screen, the color and font pickers, and bindings;
  - *Changed:* the Linux SDL2 minimum is now 2.0.18;
  - *Fixed:* the three bugs.
- **`CONTRIBUTING.md`:** the new modules and tests.

## Out of scope

- **3c:** editing menus.
- **Sub-project 4:** fullscreen polish, and Transparent mode without the tinkering.
- **Not planned:** free-text entry (no on-screen keyboard); storing fonts by family name; mouse use in settings.
