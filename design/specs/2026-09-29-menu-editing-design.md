# Editing menus from the remote (3c): design

**Date:** 2026-09-29
**Status:** approved in brainstorming, awaiting spec review. **Amended 2026-10-15** for SDL3 (the port shipped in v0.5.0): real-keyboard typing comes from key events, and no OS keyboard may ever appear (see **No OS keyboard, ever**).
**Covers:** sub-project 3c of the overhaul (todo item 7): adding, removing, reordering and renaming the user's tiles and submenus, their icons, and the on-screen keyboard.
**Ships as:** v0.6.0, built on SDL3 (floor 3.2.10).
**Builds on:**
- `design/specs/2026-09-27-settings-screen-design.md` (3a);
- `design/specs/2026-09-29-settings-all-settings-design.md` (3b), whose list picker, `inidoc` list mode and `settings_pickers.c` 3c reuses.

**The implementation plan is written after the SDL3 port (v0.5.0)**, against SDL3.

## Context

A menu is a `config.ini` section, and its lines are entries: `Key=title;icon;command`. The parser ignores the key name. The icon is a library icon name (one of 69 in `assets/icons/library/icons.ini`, each with a `title` and a `group`) or an image path. The command is a program, a `.lnk` (Windows), a `.desktop` file (Linux), a script, or a special command such as `:submenu Games`. Today every menu is written by hand.

3c lets the user build their launcher from the couch. **Its scope is the tiles users add themselves:** apps such as Edge, Chrome, Kodi, Steam or Calculator, scripts, special commands and submenus. The streaming services arrive later as built-in tiles from the D-pad overlay sub-project, which users do not install, and 3c's add flow can take that source then.

**An on-screen keyboard is unavoidable** (the overlay's login fields need one too), so 3c builds it. This lifts 3b's "no free text" rule once 3c lands.

## Decisions

| Question | Decision |
|---|---|
| Scope | The tiles users add themselves, their submenus, their order, their names and their icons. |
| On-screen keyboard | **Built in 3c:** StreamFlex's own, drawn in SDL. Its layouts are data tables the overlay sub-project can reuse. |
| Where apps come from | **The installed-apps list, plus browsing.** Windows lists the shell's AppsFolder; Linux reads `.desktop` files. |
| Where editing happens | **In settings** (*Menus › <menu> › Tiles ›*), plus a shortcut: **holding OK on a tile** opens that tile's page. |
| Icons | **A library match, the app's own icon on a library plate, the library grid, or an image file.** |
| Back tiles | **New submenus get none:** Back and the pad's B button already go back. Existing Back tiles are untouched. |
| Removing a submenu tile | **A confirm page:** *Remove Games and its N tiles*, or *Remove only this tile*. The second option appears only when another menu also opens Games. |
| Keyboard layout | **QWERTY by default, with an ABC grid one press away.** The choice is remembered. |
| OS on-screen keyboards | **Never.** StreamFlex's keyboard is the only one that appears, on every OS and desktop. StreamFlex never starts SDL text input. |
| Real-keyboard typing | **From key events only** (user, 2026-10-15). Layout, Shift, Caps Lock and AltGr work; dead-key accents and CJK input methods do not compose. The *àé* page covers European accents. |
| Architecture | **3b's list picker and `inidoc` list mode, plus new pure models:** `osk.c`, `applist.c`, `iconmatch.c` and `menuedit.c`. The platform scans run on background threads. |

## Pages and flows

**Settings › Menus › *<menu>*** gains **Tiles ›** under 3a's grid rows. The Tiles page has **Add ›** at the top, then one row per tile in menu order (a submenu tile shows ›).

- **Add ›** offers:
  - ***App ›*:** the installed-apps list, alphabetical. Up from the top row, or typing on a real keyboard, opens the on-screen keyboard to filter it.
  - ***Browse for a program ›*:** 3a's folder browser, showing `.exe` and `.lnk` on Windows, and executables, `.desktop` files and scripts on Linux. The title starts as the file's name without its extension.
  - ***Submenu ›*:** a name through the keyboard. This creates `[Name]` and a tile that runs `:submenu Name`, with no Back tile.
  - ***Special ›*:** Home, Back, Settings, Sleep, Restart, Shut down and Quit, each with its library icon.
  - A new tile goes after the highlighted tile, or at the end.
- **A tile's page:**
  - *Title*: OK opens the keyboard.
  - *Icon ›*: see **Icons**.
  - *Command ›*: the app list, a browse, the special commands, or *Type…* (the keyboard) for a `:fork` line or a path. `:select` is never offered, because the parser forbids it in entries.
  - *Move*
  - *Move to menu ›*
  - *Remove*
- **Move:** OK picks the tile up. The preview outlines it, the arrows move it through the grid while the preview re-lays the menu live, OK drops it, and Back cancels.
- **Remove:** a normal tile goes at once; *Discard changes* is the undo, as everywhere in settings. A submenu tile gets the confirm page (see **Decisions**).
- **Renaming a submenu tile renames the submenu:** the `[Section]`, every entry whose command is exactly `:submenu <Old>`, and `DefaultMenu` when it names it. A name that clashes with another section is refused with a reason.
- **The hold-OK shortcut:**
  - In the normal launcher, holding OK (Return, or the pad's A) on a tile for 0.8 s opens settings at that tile's page. This fires on the first press only, and never on key repeat.
  - Back from that page goes to the Tiles page, then up through settings as usual. Closing settings lands on that tile.
- **Guard rails:**
  - The default menu cannot be removed.
  - A menu's last tile can be removed; an empty menu shows *Empty: Add ›*.
  - A menu's `Rows`, `Columns` and `IconSize` lines are never touched by tile edits.

## The on-screen keyboard (`src/osk.c`, drawn by `settings_pickers.c`)

- **Where it appears:** across the bottom 40% of the screen, over the preview. The text field is above it with its label (*Title for Kodi*), and the preview updates as the user types.
- **Layouts** are data tables:
  - **QWERTY** (the default): the number row, three letter rows, and a bottom row with *ABC*, *?123*, *àé*, *Shift*, *Space*, *⌫*, *◀ ▶* (the text cursor) and *Done*.
  - **ABC:** a 7-column alphabetical grid, with the same bottom row.
  - **?123:** digits and symbols.
  - **àé:** à á â ä ã å æ ç è é ê ë ì í î ï ñ ò ó ô ö õ ø œ ù ú û ü ý ÿ ß.
  - Shift is one-shot; pressing it twice turns on caps lock.
  - The choice is remembered as `[General] KeyboardLayout=QWERTY|ABC`, which is also a row on the General page.
- **Moving:**
  - Left and Right step between keys, wrapping within a row.
  - Up and Down go to the key in the next row nearest the current key's center.
  - OK types the highlighted key.
- **Shortcuts:**
  - The pad's X is backspace, Y is space, and Start is Done.
  - Back cancels. With no change it cancels at once; after a change, the first Back shows *Back again to discard*.
- **A real keyboard types into the field, from key events:** a printing key inserts its character, Enter is Done, Esc cancels and Backspace deletes. The arrows keep moving the on-screen highlight, because remotes send the same arrow keys.
  - The character is `SDL_GetKeyFromScancode(event.key.scancode, event.key.mod & (SDL_KMOD_SHIFT | SDL_KMOD_CAPS | SDL_KMOD_MODE), false)`: the layout's own character with Shift, Caps Lock and AltGr applied. `event.key.key` is not used for this, because SDL3 gives it unshifted.
  - A keycode with `SDLK_SCANCODE_MASK` set, a control character below U+0020, U+007F, or U+FFFD (what SDL gives on Windows for a key that types several code points) inserts nothing.
  - Key repeat inserts again, as a held key does in any text field.
  - **Limits:** a dead key types its own character and does not combine with the next letter, and a CJK input method is bypassed, so a Japanese or Chinese layout types Latin letters. The *àé* page covers European accents. Non-Latin layouts that need no input method (Greek, Cyrillic) type their own letters.
- **The field's text** is drawn as titles are, one texture re-rendered on each change, not with SDL3_ttf's text engine (`TTF_CreateRendererTextEngine`): typing changes it at most a few times a second.
- **The text** is UTF-8, with the cursor on character boundaries.
  - Each field has a maximum length and a validator, and the caption says why *Done* is disabled.
  - A title cannot contain `;`, which would break `title;icon;command`.
  - Every value must pass `inidoc_value_ok()`.
- **Uses:**
  - tile titles and submenu names;
  - filtering the app list and the icon grid;
  - the *Type…* row for commands.

  Once 3c lands, 3b's command picker and path rows gain a *Type…* row as well.

### No OS keyboard, ever

A TV launcher driven by a remote must never have the system's own on-screen keyboard cover it. **The rule: StreamFlex never calls `SDL_StartTextInput` or `SDL_StartTextInputWithProperties`, anywhere.** It has no `SDL_EVENT_TEXT_INPUT` or `SDL_EVENT_TEXT_EDITING` handler, since neither event arrives without them.

Why only that works, from SDL 3.2.10's and 3.4.18's sources (research, 2026-10-15):
- **SDL3 never starts text input by itself** (SDL2 did on desktop). The only callers are `SDL_StartTextInput*`, and the restart on keyboard focus runs only while text input is already active.
- **Wayland:** starting text input always calls `zwp_text_input_v3_enable`, which is what makes GNOME's and KDE's screen keyboards appear. No hint gates it.
- **`SDL_HINT_ENABLE_SCREEN_KEYBOARD=0` does not help:** it only skips SDL's own show and hide, which Windows and Wayland do not have, so it cannot stop the compositor's keyboard. No `SDL_PROP_TEXTINPUT_*` property suppresses one either.
- **Windows:** starting text input associates an IMM context with the window. SDL 3.4.18 detaches it right after creating the window (`SDL_windowswindow.c`, "Ensure that the IME isn't active on the new window until explicitly requested"); 3.2.10 does not. The Windows zip is built with vcpkg's SDL 3.4.18, but a build against 3.2.x would keep Windows' default context, and whether the touch keyboard then shows is OS behavior the source cannot settle. So **`make_window()` calls `SDL_StopTextInput()` on each window it creates**, on every platform. `make_window()` makes every window, including the ones `recreate_window()` rebuilds: on 3.4 it repeats what SDL did, and on 3.2 it detaches the context.
- **Key events arrive without text input** on Windows, X11 and Wayland, which is what real-keyboard typing reads. StreamFlex does not set `SDL_HINT_WINDOWS_RAW_KEYBOARD`.

## Installed apps (`src/applist.c`, with platform scans)

`applist.c` holds the pure list: sorting, deduplication and filtering. Each platform scans on a background thread, once per settings visit. Until a scan finishes, the list shows *Finding apps… (N)*.

- **Windows:** the shell's **AppsFolder** (COM, `FOLDERID_AppsFolder`), which lists what the Start menu shows, both desktop and Store apps.
  - Each app gives its display name and its app ID.
  - The command written is `explorer.exe shell:AppsFolder\<app ID>`, which launches both kinds.
  - Uninstallers, and help or readme shortcuts, are filtered out.
- **Linux:** the `.desktop` files in every `$XDG_DATA_DIRS/applications`, in `~/.local/share/applications`, in the Flatpak exports (`/var/lib/flatpak/exports/share/applications` and `~/.local/share/flatpak/exports/share/applications`), and in `/var/lib/snapd/desktop/applications`.
  - Files with `NoDisplay=true`, `Hidden=true`, or a `Type` other than `Application` are skipped.
  - The title is `Name[<locale>]` from `LANG`, falling back to `Name`.
  - The command written is the `.desktop` path, which StreamFlex already launches.

## Icons

*Icon ›* lists the choices in this order, preselecting the first that applies:

1. **A library match** (`src/iconmatch.c`, pure).
   - The app's name is normalized: lower case, no punctuation, and vendor words (Microsoft, Google, Mozilla, Apple, Valve and so on) dropped.
   - It is then compared with each library icon's name and `title`.
   - The manifest gains optional `match =` aliases (for example, `edge` gets "microsoft edge"). They are generated by `branding/library/build-library.py` from its tables, since the manifest itself is never hand-edited.
2. **The app's own icon, on a plate.**
   - **Windows:** `IShellItemImageFactory::GetImage` at 256 px, for any AppsFolder item.
   - **Linux:** the `.desktop` `Icon=` key. An absolute path is used as it is. A name is looked up in the hicolor theme (the largest PNG, or the scalable SVG), then `/usr/share/pixmaps`, then the Flatpak export icon folders.
   - The icon is centered on the library's standard square plate and saved as a PNG in an `icons/` folder beside `config.ini` (on Linux, `~/.config/streamflex/icons/`), named from the app's name. The tile's icon is that path.
   - StreamFlex only ever writes these files. A file is deleted only by *Discard* in the visit that created it, when nothing uses it.
3. **The library grid:** every icon with its title, grouped by the manifest's `group` and filtered by typing.
4. ***Browse for an image ›*:** 3a's folder browser.

With no match and no icon of its own, an app gets the library's generic `apps` icon. The preview shows each highlighted icon on the tile.

## Saving and applying (`src/menuedit.c`, pure)

- **The menu model:** the menus as plain data (section name, entry key, title, icon and command, in order), with operations for add, remove, move, move to menu, create submenu, rename submenu and remove submenu.
  - The SDL side rebuilds the live `Menu` and `Entry` lists from it after each change: the textures are marked stale and the layout is recomputed.
  - The snapshot and *Discard* use the model too. Discard also deletes any plated icon files created in that visit that nothing uses.
- **Writing** goes through 3b's `inidoc` list mode, since a menu section is a list. The layout keys `Rows`, `Columns` and `IconSize` are excluded by the parser's own rule: they are settings only when their value holds no `;`.
  - **A new entry** is written as `EntryN=title;icon;command`, where N is one above the highest `Entry<number>` in the section. Other key names are kept as they are.
  - **Move:** `inidoc_list_move(line, before)` moves the line within its section. Comments stay where they were.
  - **Move to menu:** removes the line from one section and adds it to the other.
  - **New submenu:** `[Name]` is added at the end of the file. A valid name:
    - is not a reserved section (General, Background, Layout, Titles, Highlight, Scroll Indicators, Clock, Screensaver, Hotkeys, Gamepad);
    - contains no `]`;
    - does not already exist.
  - **Remove submenu:** `inidoc_remove_section()` removes the header and every line up to the next header.
  - **Rename submenu:** renames the header, rewrites each entry whose command is exactly `:submenu <Old>`, and updates `DefaultMenu` when it matches.
- **Saving** keeps 3a's and 3b's rule of reading the file fresh.
  - A line edit finds its line by its original text, as 3b's bindings do.
  - A section operation matches by the section's name.
  - When a hand edit has removed or renamed the target meanwhile, that operation is skipped with a log line, and the save still succeeds.
- **Leaving settings:**
  - When the menu settings were opened from has been removed, closing goes to the default menu.
  - The hold-OK shortcut returns to its tile.

## Errors and limits

- An app scan that fails or finds nothing leaves *Browse for a program ›* and the other Add options working, and the caption says why.
- An icon that cannot be extracted or plated falls back to the library match, or else to `apps`.
- A plated icon that cannot be written (for example, a read-only folder) keeps the tile's previous icon, and the caption says why.
- **Debug output (`-d`):**
  - every menu edit;
  - the scans' counts and times;
  - each icon's source and path;
  - each save step.

## Testing

### Unit tests (CTest, no SDL: Windows, Debian and Pi CI)

- **`test_osk`:**
  - every layout;
  - nearest-key movement between rows of different widths;
  - one-shot shift and caps lock;
  - UTF-8 insert and delete at the cursor;
  - inserting a key's character: printing keys insert, and scancode-masked keys, control characters, U+007F and U+FFFD insert nothing;
  - each field's validator.
- **`test_applist`:**
  - `.desktop` parsing: NoDisplay, Hidden, Type, localized names;
  - deduplication;
  - the uninstaller and readme filter;

  all with a fake scan.
- **`test_iconmatch`:** normalization, vendor words, aliases, and the fallback.
- **`test_menuedit`:**
  - every operation;
  - EntryN numbering;
  - rename rewriting `:submenu` lines and `DefaultMenu`;
  - the name rules;
  - Discard.
- **`test_inidoc`** (extended): `list_move`, `remove_section`, and a save after a hand edit.
- **Windows only:** enumerating AppsFolder and getting one item's image, proving the COM path on the runner.

### Headless harness (Debian and Fedora)

A fixture applications folder holds fake `.desktop` files and icons. With xdotool, the harness checks:
- adding an app;
- typing a title on the on-screen keyboard, and on a real keyboard with Shift (`xdotool type` with mixed case and symbols);
- moving a tile, and moving one to another menu;
- creating and renaming a submenu;
- removing a submenu with its tiles;
- the hold-OK shortcut (`keydown Return`, a 1 s wait, `keyup`);
- *Discard*.

`config.ini` is checked line by line after each save, and everything runs under ASan and the leak pass. A source check fails the build if `SDL_StartTextInput` appears anywhere under `src/`.

### Hands-on (qa-harness, before the 3c merge)

- **Windows:** add Calculator (a Store app) and Edge.
- **Ubuntu:** add Firefox and a Flatpak app.
- **Both:** rename one app with the on-screen keyboard and one with a real keyboard; make a submenu and move tiles into it; restart, and confirm.
- **No OS keyboard, one row per OS,** each with the system's keyboard forced on: open a title field, type, and confirm only StreamFlex's keyboard appears.
  - Windows: the touch keyboard set to show when no keyboard is attached, in tablet posture.
  - Ubuntu GNOME (Wayland): the screen keyboard turned on in Accessibility.
  - KDE Plasma (Wayland): a virtual keyboard (Maliit) selected.

All of this runs in qa-harness, never by hand.

## Documentation

- **`docs/configuration.md`:** editing menus, the on-screen keyboard, where plated icons are kept, and `KeyboardLayout`.
- **`CHANGELOG.md`:** *Added:* menu editing and the on-screen keyboard.
- **`CONTRIBUTING.md`:** the new modules and tests.
- **`branding/library/`:** the `match =` aliases.

## Out of scope

- The built-in streaming tiles (the overlay sub-project).
- Android apps (todo item 8).
- 16:9 tiles (todo item 20).
- Mouse use, and dragging.
- Typing into other applications' text fields: that is the overlay's keyboard, which can reuse `osk.c`'s layout tables.
