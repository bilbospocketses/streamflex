# In-app settings screen (3a) — design

**Date:** 2026-09-27
**Status:** approved in brainstorming, awaiting spec review
**Covers:** sub-project 3a of the overhaul: the settings screen, the config writer and live resizing from todo item 7; title sizing, items 18 and 19; and the 0 Hz refresh rate, item 22

## Context

StreamFlex is a 10-foot launcher: it runs full screen on a TV and is driven by a remote or a gamepad alone. Every screen must work with the arrows, OK and Back, with no mouse and no typing (binding decision). The look is ours, in the spirit of Windows Media Center, not a copy of Google TV (binding decision).

The overhaul has four sub-projects. Sub-projects 1 (grids and button sizing) and 2 (the icon library) were released in v0.2.0. Sub-project 4 (fullscreen polish) fits in anywhere. Sub-project 3, the settings screen, was split by the user on 2026-09-27 into three parts, each with its own spec, plan and build, in this order:

1. **3a** (this document): the config writer, the settings screen and its basic controls, applying changes live, and the first settings: the background, each menu's grid, and title sizing.
2. **3b:** every other setting in `config.ini`, including the harder controls: a color picker, a font picker, and "press the button you want" for key and gamepad bindings.
3. **3c:** editing menus: adding, removing and reordering apps and submenus, and picking icons from the library.

The user chose the full scope ("all settings, plus editing menus"); the split only orders it.

## What exists today

- **Config.** `config.ini` is read once at startup by inih (`parse_config_file()` and `config_handler()` in `src/util.c`) into the global `Config`, and nothing writes it. `main()` frees the file's path right after parsing.
- **Validation.** `validate_settings()` converts some values in place (percentages to px, the title padding clamped against `IconSize`), so it cannot run a second time.
- **One-time setup.** The background (color, image, slideshow, transparent), the overlay, the clock, the highlight, the scroll indicators and the screensaver are all set up once in `main()`. Only the grid is recomputed, by `apply_layout()` on every menu load.
- **Input.** The built-in keys (arrows, Return, Backspace), `[Hotkeys]` and the gamepad controls all end in `execute_command()` and its special commands (`:left`, `:select`, `:back`, `:home` and so on).
- **Titles.** One font at one size, `config.title_font_size`, for the whole launcher; `geo.font_height` feeds every menu's `title_block`. Shrink mode (`render_text()`, `src/image.c:346-364`) reopens the font one point smaller at a time with no floor, and leaks the last font it opened when it reaches 0.
- **Where the config is found.** Linux searches `./`, the executable's folder, `~/.config/streamflex/`, then `/usr/share/streamflex/`, where the packages install it read-only. Windows searches `./` and the executable's folder.
- **Windows file access is ANSI.** `fopen()` and `FindFirstFileA()` (`src/platform/win32.c:183-203`) take paths in the system code page, while SDL hands out UTF-8 (`SDL_GetBasePath()`, image loading). A path with non-ASCII characters probably fails today; this was read from the code, not tested.
- **Refresh rate.** `init_sdl()` and `create_window()` divide by `display_mode.refresh_rate` (`src/launcher.c:218`, `:239`, `:245`), which SDL may report as 0 (item 22). Xvfb always does.
- **SDL_ttf.** The Linux minimum is 2.0.15 (`CMakeLists.txt:30`), so `TTF_SetFontSize()` (2.0.18) is not available: each font size needs its own `TTF_OpenFont()`.
- **Tests.** CTest runs `test_layout`, `test_utf8`, `test_library` and `test_library_svg` on Windows, Debian and the Pi. A headless harness (Xvfb, `xdotool`, ASan and UBSan in `debian:bookworm`) exists only in scratch, and patches the source around item 22.

## Decisions

| Question | Decision |
|---|---|
| How much the screen changes | Everything in `config.ini`, including menus, delivered as 3a, 3b and 3c. 3a covers the background, each menu's grid and title sizing. |
| How to open it | A new special command, `:settings`. When nothing maps it, the gamepad's Start button and the keyboard's context-menu key open it, by the same fallback rule Up and Down got in sub-project 1. |
| When changes apply | **Live**, as they are made. Leaving settings with Back writes the visit's changes in one save. A *Discard changes* row restores everything as it was on entry. |
| The layout | **A narrow settings column on the left and a large live preview on the right.** In the user's words: "the left column only as wide as is comfortable to fit the menus, so that the preview on the right can be large enough to get a good feel for the changes being made". Approved from mockups. |
| Choosing an image or a folder | A **folder browser** in the left column. The preview shows the highlighted image as the background. |
| Colors in 3a | Preset colors, stepped with Left and Right. The full color picker is 3b's. |
| Title sizing | **Titles scale with the button**, per menu, with a readable minimum. This reverses sub-project 1's rule that "the title size stays global, for readability at 10 feet": that rule is what made item 19. |
| How edits reach the launcher and the file | **A settings table** (approach 1): the screen edits typed values in `Config` and refreshes only what they affect; on save, each changed value is formatted back into the file. |
| The file writer | Edits `config.ini` line by line, so comments, blank lines, order and line endings survive. Reads the file fresh at save time, keeps a `.bak`, and replaces the file safely. |
| Headless tests | The harness moves into the repo and runs in CI. |
| Item 22 | Fixed in 3a, because the harness in CI runs under Xvfb, which reports 0 Hz. |

## The screen

### Opening it

- `:settings` is dispatched in `execute_command()` beside the other special commands, so an entry, a hotkey or a gamepad control can use it. The sample config's System menu gains a *Settings* tile (`Settings;settings;:settings`).
- **Gamepad default.** If no gamepad control maps `:settings`, the Start button opens it, unless the config assigns Start to something else (`add_default_controls()`, as for `:up` and `:down`).
- **Keyboard default.** The context-menu key (`SDLK_APPLICATION`, labeled Menu on most remotes; `xdotool key Menu` sends it) becomes a built-in key that opens settings, unless `[Hotkeys]` binds that key, in which case the hotkey keeps it (as for Up and Down).
- Settings open over whichever menu is showing, and close back to it.

### Layout

- **Left column:** "Settings", the page path under it (*Settings › Menus › Games*), the rows, and a one-line key hint at the bottom. Its width is fixed while settings are open, so the preview never jumps between pages: the widest label and value among the table's rows, kept between 20% and 32% of the screen width. Longer text, such as a file name in the browser, is cut with "…".
- **Preview:** the rest of the screen, 16:9, centered vertically, with a thin outline. A caption under it names what it shows: *Preview: Games · 6 × 3, 190 px buttons*, adding *reduced to 9 columns to fit* when `layout_compute()` reduced the grid.
- **The look is fixed and ours.** The screen always uses the bundled default font (`FILENAME_DEFAULT_FONT`), sized from the screen height, and its own colors. A decorative title font or an unusual highlight color in someone's config never makes settings unreadable.
- Approved mockups: `C:/Users/jscha/ClaudeScratch/streamflex-settings-brainstorm/.superpowers/brainstorm/576075-1790561805/content/` (scratch; `layout-c-v2.html` is the approved layout, `title-sizing.html` the title comparison).

### Keys

- **Up / Down** move between rows and stop at the ends.
- **Left / Right** step the highlighted row's value. Holding a gamepad direction repeats with the existing timing (`GAMEPAD_REPEAT_DELAY`, `GAMEPAD_REPEAT_INTERVAL`); the keyboard repeats as the OS does.
- **OK** opens a row marked ›, or runs an action row (*Discard changes*, *Use this folder*, *Try again*).
- **Back** goes up one level. At the top level it saves and closes.
- **`:home`** acts as Back at the top level (save and close), then goes to the default menu.

### The preview

- It is the launcher's own scene, drawn by the same code as the normal screen: background (a slideshow keeps running), overlay, clock, the menu, its highlight and scroll indicators.
- It shows the menu settings were opened from. Under *Menus › Games* it switches to Games, loaded without touching its back link or its remembered position, and switches back when that page is left.
- Transparent mode shows a checkerboard where the desktop would show through; the preview is a texture and cannot be see-through.
- **How it is drawn.** `draw_screen()` is split into `draw_scene()` (everything above) and presenting. While settings are open, the scene is drawn into a target texture the size of the screen, which is then drawn scaled into the preview area. If `SDL_RenderTargetSupported()` is false, the scene is drawn full screen and the column is drawn over it on a translucent backing instead.

### Saving, discarding and failures

- **On opening**, the screen takes a snapshot of every value in the settings table.
- **Back at the top level:**
  - with nothing changed, nothing is written;
  - with changes, only the changed keys are written (see **The config writer**), then the screen closes back to its menu.
- ***Discard changes*** puts every value back to the snapshot, refreshes everything, and stays in settings. The row is grayed while there is nothing to discard.
- **A failed save** replaces the rows with *Couldn't save to <path>: <reason>* and two rows, *Try again* and *Leave without saving*. Leaving without saving keeps the changes on screen until the launcher restarts. Back returns to the settings rows.

### While settings are open

- `:left`, `:right`, `:up`, `:down`, `:select`, `:back` and `:home` go to the screen, whichever key, hotkey or gamepad control sent them. The built-in keys count as their commands: the arrows, Return and Backspace.
- `:settings` saves and closes, like Back at the top level, so the key that opened settings also closes them.
- Every other command is ignored. The sample config's `Esc` = `:quit` hotkey therefore cannot throw unsaved changes away, and no app can be launched from behind the screen.
- The screensaver does not start.
- `MouseSelect` does nothing.

## Pages and settings

### Top level

*Background ›*, *Menus ›*, *Titles ›*, a divider, then *Discard changes*. Each row shows a summary of its current value: *Slideshow*, *3 menus*, *Medium*.

### Background

*Mode* steps through *Color*, *Image*, *Slideshow* and *Transparent*, written as `[Background] Mode=Color|Image|Slideshow|Transparent`. The rows under it depend on the mode:

- **Color:** *Color* steps through ten presets: *Black* `#000000` (the built-in default), *Charcoal* `#1E1E1E`, *Graphite* `#33383D`, *Slate* `#2E3440`, *Midnight* `#121A2E`, *Navy* `#0B1F3A`, *Teal* `#07606C` (the app icon's), *Forest* `#1E3B2F`, *Plum* `#3B1F3A` and *Burgundy* `#4A1520`. A color in the file that is not a preset appears as *Custom #1A2B3C* before *Black*, until the user steps off it. Written as `Color=#RRGGBB`.
- **Image:** *Image ›* opens the folder browser. The row shows the file name. Written as `Image=<absolute path>`.
- **Slideshow:**
  - *Folder ›* opens the folder browser; the row shows the folder name and its image count. Written as `SlideshowDirectory=<absolute path>`.
  - *Change every*: 5 s to 60 min in steps (5, 10, 15, 30 s; 1, 2, 5, 10, 30, 60 min), the existing limits. Written as `SlideshowImageDuration=<seconds>`.
  - *Fade*: 0 to 3 s in 0.5 s steps, the existing limit. Written as `SlideshowTransitionTime=<seconds>`.
- **Transparent:** a note that the desktop shows through, and that Linux needs a compositor, with the docs page named. The see-through color (`ChromaKeyColor`) stays as configured; 3b can expose it.
- **An incomplete mode**, such as *Image* with no image chosen yet, previews as the color background. Leaving the Background page with the mode still incomplete puts *Mode* back to its value on entry, and the caption says why.

### Folder browser

- **Where it opens:** the folder of the current image or slideshow folder; else the user's Pictures folder (Windows: the Pictures known folder; Linux: `XDG_PICTURES_DIR`, else `~/Pictures`); else the home folder.
- **Places.** Going up from a drive's root (or from `/`) shows *Places*: Pictures, Home, then the drives on Windows, or `/`, `/media/*` and `/mnt/*` on Linux.
- **The list:** folders first, then images with the slideshow's extensions (`.jpg`, `.jpeg`, `.png`, `.webp`, matched without regard to case), each group sorted by name without regard to case. Hidden files and folders are skipped. Left and Right move a page at a time.
- **Choosing an image** (Image mode): highlighting an image shows it as the preview's background, decoded on a background thread so moving through the list never stalls; until it is ready the previous background stays. OK chooses it and returns to the Background page. An image that cannot be decoded says so in the caption and cannot be chosen.
- **Choosing a folder** (Slideshow mode): the first row inside every folder is *Use this folder (N images)*, disabled with a reason when the folder holds fewer than two images. Highlighting a folder previews its first image.
- **Paths are written as absolute paths.** A path the config file cannot hold (see **Values it refuses**) is shown but cannot be chosen, and the caption says why.

### Menus

- *All menus* edits `[Layout]`. Then one row per menu, in file order, showing *All menus* or its own grid (*3 × 6*).
- **Each menu's page** has three rows:
  - *Rows*, 1 to 10 (`Rows=`);
  - *Columns*, 1 to 12 (`Columns=`);
  - *Largest button*: *Fill* (no `IconSize`), then 64, 96, 128, 160, 192, 256, 320, 384, 512, 768 and 1024 px (`IconSize=`). A value already in the file that is not in the list (such as 200) appears in its sorted place.
- **Inheriting.** On a menu's own page, each row's lowest step is *All menus (n)*, which removes that key from the menu's section so the menu follows `[Layout]` again. On the *All menus* page, *Fill* removes `[Layout] IconSize`; Rows and Columns always hold a number.
- **`MaxButtons`.** When `[Layout]` has `MaxButtons` and no `Columns`, a change to Columns edits the `MaxButtons` line in place, because the parser treats it as `Columns`.
- A grid larger than the screen allows is reduced by `layout_compute()` as today; the preview caption says so.

### Titles

- *Size* steps through *Small*, *Medium* and *Large*, written as `[Titles] FontSize=11%`, `14%` and `17%`.
- A fixed size already in the file (`FontSize=36`) appears as *Fixed 36* before *Small*, until the user steps off it.
- The other title settings (`Enabled`, colors, shadows, `OversizeMode`) are 3b's.

## Title sizing (items 18 and 19)

### The rule

- **`FontSize` accepts a percentage** of the menu's button size, as well as a plain number (a fixed size, as today). **`TitlePadding` also accepts a percentage** of the button, as well as a plain number of px.
- **With a percentage**, a menu's title size is that share of its button size, but never below the **readable minimum: 2% of the screen height** (22 at 1080p). Sizes are SDL_ttf point sizes, which SDL_ttf renders at one pixel per point. The padding scales the same way.
- A fixed `TitlePadding` larger than half the menu's button is capped at half, the limit `validate_settings()` applied against `IconSize` until now.
- **Every title in a menu is the same size.** With the default `OversizeMode=Truncate`, a title too long for its button is cut with "…" at the menu's size.
- **Shrink mode** shrinks a long title towards the readable minimum and no further, then cuts it with "…". It works out the size from the measured width in one step instead of reopening the font a point at a time, which also removes the leak in `render_text()`.
- **Fixed sizes** (`FontSize=36`) keep today's meaning: one size in every menu. Shrink mode's minimum applies to them too.
- **New built-in defaults:** `FontSize=14%` and `TitlePadding=8%`. At the old default button of 256 px they give 36 and 20, today's values. The sample config ships the percentage forms.

### How it is computed

- The title block depends on the button size, and the button size depends on the title block, so `layout_compute()` solves the two together. `LayoutParams.title_block` becomes a small title-sizing description: the size (a percentage or a fixed size), the padding (a percentage or fixed px), the readable minimum, and the font's height per point (measured once at startup from the title font, `TTF_FontHeight()` at a reference size).
- The title block is piecewise linear in the button size (a percentage above the minimum, a constant at it), so the largest fitting button has a closed form in each piece. After rounding the font size to a whole point, the result is checked with exact integers and the button is reduced a pixel at a time if rounding made it overflow.
- `LayoutGeometry` gains the menu's title size and padding. `apply_layout()` uses them for the highlight's height and title placement in place of `geo.font_height` and `config.title_padding`.
- **Fonts.** Each title size in use opens its own `TTF_Font`, kept in a small cache keyed by size. When settings close, sizes no menu uses any more are closed.

### Existing configs

- A config with `FontSize` and `TitlePadding` as plain numbers looks as it does today, except that Shrink mode no longer goes below the readable minimum.
- A config **without** a `FontSize` or `TitlePadding` line now gets titles that scale with the button. At 256 px buttons nothing changes; on smaller or larger buttons the titles follow. This goes in the CHANGELOG under *Changed*.

## The config writer

### The in-memory file (`src/inidoc.c`)

- `config.ini` is held as a list of lines. Every line keeps its exact text.
- **It reads a file the way inih does.** `=` or `:` separates a key from its value; `;` or `#` at the start of a line begins a comment; `;` after whitespace begins a trailing comment; names are matched exactly, including case, as `MATCH` does (`src/util.h:16`). When a section or a key appears more than once, the last one counts, because that is the value the parser ends up with, and it is the one edited.
- **Setting a key that exists** replaces only the value text on its line. The key, the spacing around the separator and any trailing comment stay.
- **Setting a key that does not exist:** in a menu section it goes directly under the section header, so layout keys sit above the entries; in any other section it goes after the section's last key, before any trailing blank or comment lines.
- **A missing section** is added at the end of the file.
- **Removing a key** removes its line.
- New lines use the file's own line ending (LF or CRLF, taken from its first line ending). A UTF-8 BOM is kept.
- **Values it refuses.** A value inih would read back differently is refused: one containing `;` after whitespace (it would become a comment), one with leading or trailing whitespace (it would be trimmed), or one containing a line break. The screen never offers such a value; the browser shows the path and says why it cannot be chosen.

### Saving

1. **Read fresh.** The file is read again from disk at save time, and only the changed keys are applied to that text. A hand edit made while the launcher ran (over SSH, say) therefore survives, unless it touched the same key.
2. **Backup.** The current file is copied to `config.ini.bak.tmp` and moved over `config.ini.bak` the same way the file itself is replaced (step 4), so an earlier backup survives until the new one is ready, and a hidden backup does not block the save. A path too long for the backup's name fails the save with "the path is too long".
3. **Write.** The new text goes into `config.ini.tmp` in the same folder, and is flushed to the disk before the file is closed.
4. **Replace.**
   - **Linux:** `rename()`. The new file takes the old one's permission bits. When `config.ini` is a symbolic link, the file it points to is the one written, and the link stays.
   - **Windows:** `MoveFileExW()` with `MOVEFILE_REPLACE_EXISTING` and `MOVEFILE_WRITE_THROUGH`, retried for about a second when the file is held open, as antivirus scanners do. The file keeps its hidden and system attributes.
5. On any failure, the temporary file is removed and the screen shows the failure (see **Saving, discarding and failures**).

### Where it writes

- **Normally:** to the file the launcher loaded, including one given with `-c`.
- **Linux, when the loaded file is the packaged system copy** (under `PATH_CONFIG_SYSTEM`) and cannot be written: the first save writes `~/.config/streamflex/config.ini`, creating the folder, starting from the loaded file's text. When that file already exists (a second save in the same session, or one the user made meanwhile), the save reads it fresh and applies the changes to it instead. The launcher already searches that folder before the system copy.
- **Otherwise, when the file cannot be written**, the save fails with its reason. This includes a file in the working folder or beside the executable (either would still shadow `~/.config` on the next start), and on Windows a copy in a protected folder such as Program Files.

### UTF-8 paths on Windows

- A small platform helper opens files and lists folders through the wide-character APIs (`_wfopen()`, `FindFirstFileW()`), converting paths from and to UTF-8.
- The config reader, the log, the library manifest, the slideshow scan, the writer and the browser all use it. `library.c`'s `fopen()` calls go through it as well.
- Linux keeps plain `fopen()`; its paths are already bytes.

## Architecture

The pure modules below take plain C types only: no SDL types, no rendering and no globals, so the tests build them without SDL, as `layout.c` and `library.c` are built today. The APIs are a design intent; the implementation plan may change names.

### `src/inidoc.c` / `src/inidoc.h` (new, pure)

```c
typedef struct IniDoc IniDoc;
typedef enum { INIDOC_AFTER_LAST_KEY, INIDOC_UNDER_HEADER } IniDocPlacement;

IniDoc     *inidoc_parse(const char *text, size_t length);
char       *inidoc_serialize(const IniDoc *doc, size_t *length);     // caller frees
const char *inidoc_get(const IniDoc *doc, const char *section, const char *key); // as inih reads it; NULL if absent
bool        inidoc_set(IniDoc *doc, const char *section, const char *key, const char *value,
                       IniDocPlacement placement);                    // false if the value is refused
bool        inidoc_remove(IniDoc *doc, const char *section, const char *key);
bool        inidoc_value_ok(const char *value);
void        inidoc_free(IniDoc *doc);
```

### `src/settings.c` / `src/settings.h` (new, pure)

- **The table.** One `SettingDef` per setting: label, section (or "the menu being edited"), key, type, limits or step list, whether it can inherit, and its refresh group (layout, titles or background). 3b extends 3a by adding rows to this table.
- **Types:** count, choice (a mode setting's names), button size cap, preset color, image path, folder path, seconds, and title size.
- **Values.** A small `SettingValue` (a number, a string or *inherit*). Per type: `setting_parse()` (text to value), `setting_format()` (value to text) and `setting_step()` (Left or Right from a value, keeping a custom entry value in its sorted place).
- **One text-to-value path.** `config_handler()` reads the table's keys through `setting_parse()` instead of its own branches, so the startup parser and the screen cannot disagree about what a line means. Behavior for invalid values stays as today: ignored, with a log line. So does sub-project 1's reserved-name rule: in a menu section, `Rows`, `Columns` and `IconSize` are settings only when the value holds no `;`, and are otherwise read as entries.
- **Pages.** The page stack, the cursor, which rows are enabled, and which settings changed since the snapshot.
- **Colors** are a plain `{r, g, b, a}` struct here; the SDL side copies them into `SDL_Color`.

### `src/browser.c` / `src/browser.h` (new, pure)

The folder browser's model: its mode (image or folder), current folder, rows (*Places*, *Use this folder*, folders, images), cursor, paging, going up and down, and the chosen path. It lists folders through a function pointer, so the tests pass a fake listing and the launcher passes the platform's.

### `src/settings_screen.c` / `src/settings_screen.h` (new, SDL)

- Opens and closes the screen, takes the snapshot, routes commands to the pages, and draws the column, the preview and the caption.
- Binds each `SettingDef` to its field in `Config` (or in a `Menu`'s overrides) through small getter and setter functions, and calls the refresh group's function after each change.
- Saves through `inidoc` and the platform's safe replace, and runs *Discard changes*.
- Decodes the browser's highlighted image on a background thread for the preview.

### Changes elsewhere

- **`launcher.c`:**
  - keeps the loaded config's path;
  - `draw_screen()` splits into `draw_scene()` and presenting, and the main loop draws the settings screen instead while it is open;
  - `:settings` in `execute_command()`, and the command routing and screensaver rule of **While settings are open**;
  - the keyboard's context-menu key as a built-in key;
  - **`reload_background()`**, new: stops the slideshow thread and frees the slideshow, destroys the background textures, sets the new mode up the way `main()` does, sets the draw color, and on Windows turns the layered-window transparency on or off. `main()` calls it too, so startup and settings share one path;
  - **`reload_titles()`**, new: marks every menu's textures as stale and re-lays out the current menu;
  - `init_sdl()`: when SDL reports a refresh rate of 0 or less, use 60 Hz and log it once, and use that value at all three divisions (item 22).
- **`util.c`:** the table's keys are parsed through `settings.c`; `validate_settings()` stops clamping the title padding against `IconSize` (the layout now sizes it); `add_default_gamepad_controls()` adds Start for `:settings`.
- **`image.c`:** Shrink mode as described in **The rule**; the title font comes from the size cache.
- **`layout.c`:** the title sizing in `layout_compute()`.
- **Platform (`win32.c`, `unix.c`, `platform.h`):** listing a folder (wide APIs on Windows), the drives and places, the Pictures folder, the UTF-8 file helper, the safe replace, and on Windows turning transparency off (`make_window_transparent()` can already turn it on).

## Errors and limits

- A setting whose value in the file is invalid is ignored with a log line, as today. The screen shows the value the launcher actually uses.
- A background image or slideshow folder that disappears after it was chosen is handled as at startup today: the launcher logs it and falls back to the color background.
- The settings screen never writes an invalid value: every value it can produce comes from the table's limits or step lists, and `inidoc_set()` refuses what the parser would misread.
- **Debug output** (`-d`): every change (setting, old value, new value), every refresh, the save's target path and backup path, and each step of a failed save.

## Testing

### Unit tests (CTest, no SDL; Windows, Debian and Pi CI jobs)

- **`test_inidoc`:**
  - loading and serializing without edits is byte-identical, for fixtures with LF, CRLF, a BOM, comments, trailing comments, `:` separators, repeated sections and keys, no final newline, and the shipped sample config;
  - setting an existing key changes only its value text, and keeps a trailing comment;
  - each placement rule: a new key in a menu section, in another section, and a new section;
  - removing a key; refused values; exact-case matching.
- **`test_settings`:**
  - each type survives text → value → text, through `setting_parse()` and `setting_format()`;
  - step limits, the *All menus* step, *Fill*, and a custom value (such as `IconSize=200`) in its sorted place;
  - page navigation, the changed set, and Discard restoring the snapshot.
- **`test_browser`**, with a fake listing: sort order, folders before images, the image filter, *Places*, going up from a root, the two-image rule, paging, and a path that cannot be chosen.
- **`test_layout`** gains title sizing: a percentage size, the readable minimum, percentage padding, a fixed size unchanged, the button and title solved together (checked with exact integers after rounding), item 19's cases at both ends (a dense grid at 1280 × 720, and 1024 px buttons at 5120 × 2160), and item 18's case.
- **Windows only:** a test that holds `config.ini` open without sharing for 300 ms from another thread and checks that the save still succeeds, proving the retry.

### Headless tests in CI (new)

- The harness moves from scratch into `tests/headless/`: its `Dockerfile` (CI's `debian:bookworm` build dependencies plus `xvfb`, `xdotool` and Mesa), `run.sh`, and fixtures. Its item 22 patch is removed, since the real code is fixed. Its `scrollfail` fault injection (item 11's texture failure) stays as it is: `run.sh` applies it to its own copy of the source, never to the repo's.
- **A new CI job** on `ubuntu-24.04` builds the harness image and runs it with `--security-opt seccomp=unconfined` (its `setarch -R` needs it), as it runs locally. `build-and-test` requires it.
- **Settings checks:**
  - open settings with `xdotool key Menu`, change a grid's Columns, back out, and check that exactly the expected line of `config.ini` changed, that `config.ini.bak` holds the old file, and that the log shows the new grid;
  - *Discard changes*, then Back: the file is untouched;
  - a save to a read-only file shows the failure rows;
  - with the config loaded from the system path and read-only, the save goes to `~/.config/streamflex/config.ini`;
  - the existing fixtures keep running.

### Hands-on (qa-harness, as for sub-projects 1 and 2)

CI's Windows zip in the qa-harness Windows 11 guest, driven by keys and a virtual Xbox pad: open settings with Start; step through the background modes; browse to an image in a folder with a non-ASCII name (proving the UTF-8 fix); change a grid and the title size and watch the preview; save; restart; confirm everything stuck and the rest of the file is unchanged.

## Documentation

- **`docs/configuration.md`:** the settings screen; `:settings` and its defaults (Start, the Menu key); `FontSize` and `TitlePadding` as percentages and the readable minimum; Shrink mode's minimum; where saves go (including `~/.config` on Linux) and `config.ini.bak`.
- **`config/config.ini.in`:** `FontSize=14%`, `TitlePadding=8%`, and a *Settings* tile in the System menu.
- **`CHANGELOG.md`:**
  - *Added:* the settings screen and `:settings`.
  - *Changed:* titles scale with the button when `FontSize` is not a fixed number.
  - *Fixed:* items 18 and 19, the Shrink-mode font leak, non-ASCII paths on Windows, and the crash on a display that reports 0 Hz (item 22).
- **`CONTRIBUTING.md`:** the new modules, the new tests, and the headless job.

## Out of scope

- **3b:** every other setting, the color and font pickers, key and gamepad bindings, the background overlay, and the see-through color.
- **3c:** editing menus.
- **Sub-project 4:** making Transparent mode work without the tinkering.
- **Not planned:** bundled background images; a per-user config location on Windows (`%APPDATA%`); mouse use in settings.
