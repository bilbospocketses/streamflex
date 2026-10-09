# Built-in icon library — design

**Date:** 2026-09-27
**Status:** approved in brainstorming, awaiting spec review
**Covers:** todo item 6 (built-in icon library: generic defaults plus the major streaming services)

## Context

StreamFlex is a full-desktop HTPC: it brings back the living-room PC that Windows Media Center used to be. It runs full screen and is driven by a remote or a gamepad alone. The plan is for its app tiles to open the real Android apps, running in a virtualized AOSP container (todo item 8, not designed yet). Whether a service has a Windows app does not matter.

**The interface is StreamFlex's own.** It does not copy Google TV or any other 10-foot interface. It keeps the square icons the original project used, because they sit well together.

This is sub-project 2 of the overhaul:

1. **Grid and sizing**: done (PRs #23, #24, #25). Buttons are square and sized by each menu's grid, from 32 px up to most of the screen. `load_icon()` draws an `.svg` at the button's exact size.
2. **Built-in icon library** (this document): item 6.
3. **In-app settings screen**: item 7, with the config writer and live resizing. It will offer this library as an icon picker.
4. **Fullscreen polish**: item 7. It can land at any point.

## What exists today

- **Icons are file paths.** An entry is `Title;icon;command`, and the icon field is a path (`util.c`, the entry parser). If a sibling `<name>_selected.<ext>` exists, it is shown while the button is highlighted (`selected_path()`).
- **Seven shipped icons.** `assets/icons/` holds seven Numix PNGs: `kodi`, `plex`, `steam`, `retroarch`, `system`, `restart` and `sleep`. The default config points at them through the `ICONS_PREFIX` build variable.
- **Where the default config looks on Windows.** It uses `./assets/icons/...`, which resolves against the folder the launcher was *started from*. A shortcut with a different "Start in" folder therefore loses every icon. Fonts do not have this problem: `load_font()` looks next to the executable first (`<exe>/assets/fonts`), then in the system path on Linux.
- **A missing icon file** makes `load_icon()` return `NULL`, and the button draws with no image.
- **Parsing and tests.** `config.ini` is parsed with inih. CTest runs `test_layout` and `test_utf8` on Windows, Debian and Raspberry Pi.

## Decisions

| Question | Decision |
|---|---|
| Look of the set | **Each service's own app icon**, cut to one shared rounded-square outline. Our generic icons use the same outline. |
| Tile shape | **Square**, like today's buttons. A 16:9 tile option is a separate, later item (todo item 20). |
| How brand art reaches users | **Bundled** in every package, in its own folder, with a notice that it belongs to its owners and is not GPL. |
| Services covered | The 33 listed under [The brand set](#the-brand-set). |
| Generic icons covered | The 36 listed under [The generic set](#the-generic-set). |
| How an entry names an icon | A **bare name** in the existing icon field (`Entry1=Netflix;netflix;...`). Anything that is not a bare name stays a file path. |
| Generic icon colors | **A color per kind**: every media kind has its own hue, system actions share a slate, general-purpose icons share StreamFlex teal, and devices share a steel blue. |
| Structure | Icon files plus a manifest, `icons.ini`, which a new pure module, `src/library.c`, reads. |
| Existing Numix icons | Removed. The library replaces them. |

## The library on disk

```
assets/icons/library/
  icons.ini                          the manifest
  generic/<name>.svg                 our icons, GPL-3.0
  generic/LICENSE-material-symbols.txt
  brands/<name>.png                  owners' artwork
  brands/NOTICE.md                   generated from icons.ini
```

### Manifest

`icons.ini` has one section per icon, and the section name is the icon's name:

```ini
[netflix]
title   = Netflix
group   = video
file    = brands/netflix.png
owner   = Netflix, Inc.
android = com.netflix.ninja
source  = <URL of the listing or press kit the art came from>
sha256  = <checksum of brands/netflix.png>

[movies]
title = Movies
group = media
file  = generic/movies.svg
```

| Key | Required | Meaning |
|---|---|---|
| `title` | yes | Display name for the gallery and the future icon picker. |
| `group` | yes | One of `video`, `free-tv`, `servers`, `music`, `games`, `web` (brands), or `system`, `media`, `general`, `devices` (generic). |
| `file` | yes | Path relative to the library folder. It must not be absolute or contain `..`. |
| `owner` | brands | The legal owner, as written in `NOTICE.md`. |
| `android` | brands, if one exists | The Android package that item 8 will launch. |
| `source` | brands | Where the art came from, so it can be re-fetched or proven. |
| `sha256` | brands | The checksum of the committed PNG, so a swapped file is caught. |

**Names** are 1-32 characters from `a-z`, `0-9` and `-`, and are unique across generic and brand icons. The launcher ignores unknown keys, so later sub-projects can add fields.

### Licensing

- `generic/` is GPL-3.0, like the rest of the repository. Its glyphs come from Material Symbols (Apache-2.0, compatible with GPL-3.0), whose license text ships beside them.
- `brands/NOTICE.md` says:
  - each icon is its owner's trademark and artwork;
  - it is included only to identify the service;
  - it is not covered by the GPL;
  - it will be removed if an owner asks.

  It is generated from the manifest's `owner` fields, so it cannot drift from the files.
- Removing a brand icon means deleting its file and its manifest section. A config that still names it falls back as described in [Errors](#errors-and-limits).

## The generic set

36 icons, generated by `branding/library/build-generic.py`, in the same style as `branding/icon/build-svg.py`:

| Group | Icons | Plate color |
|---|---|---|
| `system` | power, restart, sleep, quit, settings, back, lock, user | one slate |
| `media` | movies, tv-shows, live-tv, music, photos, games, emulators, audiobooks, podcasts, radio, kids, sports, news | a hue each |
| `general` | apps, web, folder, favorites, home, search, info, download, terminal, desktop | StreamFlex teal |
| `devices` | display, audio, bluetooth, network, gamepad | one steel blue |

- **Plate.** A rounded square filling the canvas (`viewBox 0 0 512 512`), with a corner radius of 22% of its side. That matches the app icon's plate (54 of 244). It is a flat fill: no gradients, filters, masks or text, so nanosvg draws it the same everywhere.
- **Color.** Chosen on a comparison screen with the real glyphs (2026-09-27). **Every plate has the same CIE lightness, L\* = 52**, so no plate looks heavier than another, and white contrast is 4.18:1 on all of them.
  - **Media plates are vivid.** They sit at hue slots `h = 25 + k·360/13` (k = 0-12). Each one's chroma is `floor(0.85 × the largest chroma that hue can reach in sRGB at L* = 52)`.

    Equal chroma was rejected. Blue and cyan leave the sRGB gamut early, which would have held all 13 hues to C = 30, a dusty palette.
  - **Slots, in order:**

    | k | Icon | Hex |
    |---|---|---|
    | 0 | movies | `#E7364B` |
    | 1 | news | `#C85D21` |
    | 2 | kids | `#9E7522` |
    | 3 | sports | `#7A8223` |
    | 4 | music | `#328E23` |
    | 5 | podcasts | `#308B69` |
    | 6 | audiobooks | `#338984` |
    | 7 | photos | `#358696` |
    | 8 | tv-shows | `#3784A9` |
    | 9 | live-tv | `#3A7EC9` |
    | 10 | games | `#7B68EA` |
    | 11 | emulators | `#CF37C7` |
    | 12 | radio | `#DF3784` |

    The generator computes these colors rather than hard-coding them, and its tests pin these values.
  - **Other plates:**
    - `system`: slate, LCh(42, 6, 260), `#5C646D`;
    - `general`: StreamFlex teal, the app icon's `TEAL_BG` `#07606C`;
    - `devices`: steel blue, LCh(46, 18, 250), `#4D7189`.
  - **Contrast rule.** Every plate must give white a contrast ratio of at least 3:1, and the generator asserts this.
- **Glyphs.**
  - **Source.** Material Symbols Rounded, filled (npm `@material-symbols/svg-500`, version 0.47.5, Apache-2.0). Each glyph is one path on a `0 -960 960 960` view box.
  - **Vendored.** The 36 source files are copied into `branding/library/glyphs/` with the license, so building needs no network.
  - **Placement.** Each is drawn in white through a `transform`, with the 960-unit em box scaled to 60% of the plate side, centered, and multiplied by a per-glyph optical trim factor. Each trim starts at 1.0 and is tuned on the review sheet, because equal-area glyphs do not look equal.
  - **Mapping, verified to exist in that package** (our name → glyph):
    - power → `power_settings_new`, restart → `restart_alt`, sleep → `bedtime`, quit → `logout`;
    - settings → `settings`, back → `arrow_back`, lock → `lock`, user → `switch_account`;
    - movies → `movie`, tv-shows → `tv`, live-tv → `live_tv`, music → `music_note`;
    - photos → `photo`, games → `sports_esports`, emulators → `joystick`, audiobooks → `headphones`;
    - podcasts → `podcasts`, radio → `radio`, kids → `toys`, sports → `sports_soccer`;
    - news → `newspaper`, apps → `apps`, web → `language`, folder → `folder`;
    - favorites → `favorite`, home → `home`, search → `search`, info → `info`;
    - download → `download`, terminal → `terminal`, desktop → `desktop_windows`, display → `display_settings`;
    - audio → `volume_up`, bluetooth → `bluetooth`, network → `wifi`, gamepad → `gamepad`.

    The files are `rounded/<glyph>-fill.svg`.
- **One source of truth.** The generator holds the glyph, color and trim tables and writes every SVG. Re-running it must reproduce the committed files byte for byte.

## The brand set

33 services:

| Group | Services |
|---|---|
| `video` | netflix, prime-video, disney-plus, max, hulu, apple-tv, paramount-plus, peacock, crunchyroll |
| `free-tv` | youtube, youtube-tv, pluto-tv, tubi, twitch |
| `servers` | plex, jellyfin, emby, kodi, vlc, audiobookshelf |
| `music` | spotify, apple-music, amazon-music, pandora, audible |
| `games` | steam, retroarch, es-de, moonlight, geforce-now, xbox, playnite |
| `web` | chrome |

- **Art source.**
  - For a service with an Android app, the source is the high-resolution icon on its Google Play listing. Play has required full-bleed square art since 2019, because Android cuts each icon to its own shape.
  - For a service without one, the source is the icon in the owner's press kit, website or repository. Playnite is Windows-only, and Steam on the desktop means Big Picture.

  Their tiles launch desktop programs, as entries do today, so `android` is left out.
- **Research pass.** Before any art is committed, the first implementation task confirms, for each service:
  - its Android package (the TV app where one exists: Netflix's TV app is `com.netflix.ninja`, not the phone app);
  - its owner;
  - its source.

  The results go into `icons.ini`.
- **Resolution.** The target is 1024 px wherever the owner publishes art that large, and the minimum is 512 px. Art larger than 1024 is stored at 1024. The default four-column strip on a 4K screen gives buttons of about 850 px, where 512 px art is visibly soft. The review sheet marks which icons are 512-only.
- **Import.** `branding/library/import-brand.py <name> <source image> --source <url>` does the following:
  - It checks that the image is square, at least 512 px and sRGB.
  - It cuts the image to the shared 22% rounded-square outline with an anti-aliased edge, and leaves everything outside it transparent.
  - It writes `brands/<name>.png` and updates `source` and `sha256` in the manifest.
  - It never upscales, and it never recolors or redraws the art. Only the outline changes, which is what Android launchers themselves do.
  - An image that is not full-bleed (transparent pixels inside the outline, such as an old round logo) is refused and listed for handling by hand.
- **Later.** Once item 8 can install apps, the launcher could read an app's own icon from the container. The bundled icon would then be the fallback. That is not part of this design.

## Runtime

### `src/library.c` / `src/library.h` (new, pure)

No SDL. It uses inih and stdio only, so it can be unit-tested like `layout.c`.

```c
bool        library_is_name(const char *field);      // 1-32 of [a-z0-9-]
int         library_load(const char *root);          // parse root/icons.ini; returns the icon count, or -1
const char *library_lookup(const char *name);        // absolute file path, or NULL
const char *library_legacy_name(const char *path);   // "kodi" for .../kodi.png etc. (the seven old
                                                     // shipped files; system.png -> "settings"), else NULL
void        library_free(void);
```

`library_legacy_name()` only matches the file name. The caller checks that the path does not exist before using it, so the module stays free of filesystem checks beyond `library_load()`.

`library_load()` skips the following, logging each:
- a section with no `file`;
- a `file` that is absolute or contains `..`;
- a file that does not exist;
- a duplicate name (the first one wins).

### Finding the library

The search order is the same as for the bundled font. The first folder that contains `icons.ini` wins:

1. `<executable folder>/assets/icons/library`
2. On Linux, `@CMAKE_INSTALL_PREFIX@/share/streamflex/assets/icons/library`. On Windows, `.\assets\icons\library`.

Looking next to the executable first is what makes the default config independent of the start folder.

### Resolving entries

The library is loaded once, after the config is read and before any menu is rendered. Every entry whose icon field passes `library_is_name()` then has its `icon_path` replaced by `library_lookup()`'s absolute path. Everything downstream is unchanged: `render_buttons()`, SVG drawing at the button's size, and the grid.

- Library icons have no `_selected` variant, so the normal icon stays while the button is highlighted.
- A field that is not a bare name is a file path and behaves exactly as today. A real file whose name looks like a bare name is written `./kodi`.

### Debug log

`-d` logs:
- the library folder and the number of icons loaded;
- each entry's resolved icon path, in `debug_menu_entries()`.

## Errors and limits

| Case | Behavior |
|---|---|
| Unknown name (`netflx`) | Logs `Entry 'Netflix' in menu 'Main': no library icon named 'netflx'` and uses the library's `apps` icon. |
| No library found, or `icons.ini` unreadable | Logs one error. Every bare name falls back as above. If `apps` is missing too, the button has no image, as a missing file does today. |
| Old shipped icon path | See [Existing configs](#existing-configs). |
| Brand art bigger than the button | Scaled down by the GPU, as PNGs are today. |
| Button bigger than the brand art | Scaled up. The art is 1024 px where the owner publishes it, 512 px at least. |

## Existing configs

- **Paths keep working.** Nothing about a file-path icon changes.
- **The seven removed Numix files.** A config written against an older default may name `.../assets/icons/kodi.png` (or `plex`, `steam`, `retroarch`, `system`, `restart` or `sleep`). After an upgrade, that file no longer exists. When an icon path does not exist **and** its file name is one of those seven, the launcher uses the library equivalent (`system.png` becomes `settings`, the others keep their names). It logs a one-line note suggesting the bare name. This keeps upgraded installs from losing their icons, and costs one small lookup table.
- **The default config** (`config/config.ini.in`) switches to bare names:
  - Main: `kodi`, `plex`, `steam` or `retroarch` for Games (as `ICON_GAMES` decides today: `retroarch` on Raspberry Pi, `steam` elsewhere), and `settings` for System.
  - System menu: `power`, `restart`, `sleep`.

  `ICONS_PREFIX` is removed from CMake and the template.

## Packaging

- `install(DIRECTORY assets ...)` already installs the whole folder, so the library reaches the Windows zip, both `.deb` packages and the Arch package without new rules. The seven Numix PNGs are deleted from `assets/icons/`.
- **Size.** The generic SVGs are a few KB each. The brand PNGs total roughly 5-15 MB, depending on how many reach 1024 px.
- The Material Symbols license and `brands/NOTICE.md` ship inside the library folder.

## Documentation

- **Gallery page.** A new page, `docs/icons.md` ("Icon library"), shows every icon with its name and title, grouped by `group`, with the brand notice above the brand section. `pages.yml` builds it on each deploy with `branding/library/build-gallery.py` and copies the images into the site. The page and the copied images are gitignored, so the repository holds one copy of each icon.
- **`docs/configuration.md`.** The Entry format says that the icon field takes a library name or a path, and links to the gallery.
- **`docs/setup.md`.** "Selecting Menu Icons" puts the library first and the user's own files second.
- **`README.md`.** The Numix credit is replaced by a Material Symbols credit and a pointer to the brand notice.
- **`branding/library/README.md`.** Explains the generator, the importer, the checker and the review sheet.
- **CHANGELOG `[Unreleased]`:**
  - Added: the library.
  - Changed: the default config uses it, and the Numix icons are removed.
  - Fixed: the Windows default icons no longer depend on the start folder.

## Testing

- **Unit tests** (`tests/test_library.c`, CTest, on every CI platform):
  - `library_is_name()`: valid names pass. Uppercase letters, a dot, a slash, an empty string and 33 characters fail.
  - `library_load()` on fixture manifests covers:
    - a good manifest;
    - a duplicate name (the first wins);
    - a missing `file`;
    - an absolute or `..` path, which is rejected;
    - a missing file, which is skipped;
    - unknown keys, which are ignored.
  - `library_lookup()`: a hit returns the absolute path, and a miss returns `NULL`.
  - The old-Numix-path mapping: each of the seven maps as specified, and any other file name does not.
- **SVG test** (CTest). Every file in `generic/` is parsed with nanosvg, the launcher's own parser. Each must produce at least one shape and the 512×512 view box.
- **Library check** (`branding/library/check-library.py`, in CI). It confirms that:
  - every listed file exists, and every file in `generic/` and `brands/` is listed;
  - names are valid and unique, and groups are known;
  - brand PNGs are square RGBA, at least 512 px and at most 1024 px, transparent outside the rounded outline, and match their `sha256`;
  - `NOTICE.md` equals what the manifest generates;
  - re-running `build-generic.py` reproduces the committed SVGs;
  - every plate passes the 3:1 contrast rule.
- **Fixture runs** (local runner):
  - a config using bare names, one unknown name and one old Numix path shows each resolved path, the fallback and the migration note in the log;
  - on Windows, starting the launcher from a different folder still finds every library icon.
- **Review by eye.** `review-sheet.py` is extended to show all 69 icons at 32, 48, 64, 128 and 256 px on light, gray, dark and photo backgrounds. The user signs off on this before merge.
- **Hands-on (qa-harness).** The CI Windows zip runs with the default config, checking that the library icons draw sharp at 1080p and at the largest mode the guest offers, and that an unknown name shows the Apps icon.

## Out of scope

- Launching Android apps. That is item 8; this design only records each app's package name.
- The in-app icon picker. That is sub-project 3; it will read `icons.ini`.
- 16:9 tiles. That is todo item 20.
- A user folder that overrides library icons. It was considered and not chosen.
- `_selected` variants for library icons.
- Title legibility on small buttons. That is todo items 18 and 19.
