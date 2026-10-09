# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/).

This project started from complexlogic's Flex Launcher at v2.2 and is developed independently. The original release history up to v2.2 is in [`CHANGELOG`](CHANGELOG), which is frozen.

## [Unreleased]

### Changed
- CI checks that added lines and commit messages use American spelling.
- CI pulls the test images' bases, and the Linux, Raspberry Pi and Arch Linux build containers, through mirror.gcr.io.

### Fixed
- The headless harness waits for the launcher to draw a frame after each key it presses. On a slow CI runner, several keys could be handled with no frame between them, so a check whose line only a drawn frame logs failed at random (seen in `63-settings-mappings.sh`).

## [0.4.0] - 2026-10-09

Every setting is now on the settings screen, with color, font and command pickers, key and gamepad bindings, and an offer to restart when a change needs one. Checked by hand on Windows 11 and Ubuntu 26.04. A config written for 0.3.x reads and saves as before; on Linux, StreamFlex now needs SDL2 2.0.18 or later.

### Added
- **Every setting on the settings screen.** Nine pages mirror `config.ini`: General, Background, Menus, Titles, Highlight, Scroll indicators, Clock, Screensaver and Controls. Every change shows at once, and Back saves only what changed. A row that depends on a switch that is off is grayed, and says why.
- **A color picker:** 24 named swatches, a hex editor for any other color, and a warning when a title or clock color stands out too little from the background.
- **A font picker:** the installed fonts by family, each drawn in its own face. A face inside a font collection is written as the new `FontFace` key.
- **A command picker** for the startup and quit commands and for bindings: the special commands, every submenu, and every command the menus already run.
- **Key and gamepad bindings:** add, change and remove hotkeys and gamepad controls from the remote. Capture a key or button, and keep it. The arrows, OK and Back always keep their meaning. Taking Up, Down or the Menu key over must be confirmed within 10 seconds, or it goes back. While a change waits for that, no binding's page opens.
- **Restart now?** After a save that wrote a setting that applies at next start (the gamepad's mappings file), settings offer to restart StreamFlex: Yes, under the cursor, or No (Back too). A restart runs no `QuitCmd`, and the restarted copy no `StartupCmd`. The gamepad's mappings file is chosen in the folder browser, which lists every file for it.
- A gamepad capture needs a pad to capture from: with the gamepad off, or on with no pad connected, *Key* says why (*Turn the gamepad on to capture a button*, *No gamepad is connected*) once for each press, instead of waiting 5 seconds for a press that cannot come.
- A held D-pad or stick keeps repeating on *FPS limit* or *VSync* when a step changes the frame timing under it, at its usual pace.
- On Windows, the copy a restart starts comes to the front with the keyboard: the copy before it hands it the foreground. Its command line is the original one with `--restarted` after one space.
- The font picker starts on the bundled font, with no *Custom* row, when the config names it by a relative path, as the Windows config does (`.\assets\fonts\...`). OK on that row changes nothing, so the file keeps its relative path. When the configured font file is gone, the picker starts on the bundled font the titles fell back to, and OK there writes that font, so the dead path leaves the config.
- The font picker names a family whose face has no glyph for U+2019 or U+2013 (a symbol font such as Linux's D050000L, or a face with no Latin punctuation) in the settings' font, not in its own face. Chosen, it still sets the titles' face.
- Choosing a family in the font picker writes its regular face when that face is named *Book*, *Roman* or *Normal* (DejaVu's and URW's fonts on Linux), not its bold or oblique one; with no regular face, its upright, normal-weight one.
- On Linux, a restarted StreamFlex keeps its process name, `streamflex`, so `pgrep`, `pkill` and `ps` find it: starting again through `/proc/self/exe` had named it `exe`.
- The hex editor's, the first page's and the folder browser's key hints fit the settings column at 1280 x 720 and 1280 x 800: *Arrows edit the digits · OK keeps · Back returns*, *Left and right change · OK opens · Back saves*, and *Left and right page · OK chooses · Back goes up*.
- A row's value may use the room its label leaves, so a binding's command reads in full beside a short key name (*F9*, *Close the app on show*).
- The debug log's list of hotkeys (`-d`) includes the Windows exit hotkey (marked as not registered, with Windows' reason, when Windows refuses it), and any later `:exit` binding marked as not used (only the first is the exit hotkey), and the frame timing line says when VSync is off with no FPS limit set, so frames keep the display's rate.
- The debug log (`-d`) says when a binding's line was changed or removed by hand while settings were open, and adds, for a change written as a new line, that the first line on a key or button is the one that runs.
- Headless tests for every page, every picker, the bindings (check 62), the mappings file (63) and the restart (64). The harness sends function keys through `tests/headless/key.py`, since Fedora's keymap makes `xdotool` hold Alt down for them. `tests/headless/run-shards.ps1` runs the four passes in shards on 8 containers, and `merge.py` adds each pass's shards up into one result.
- CI also runs the unit tests built with AddressSanitizer and UBSan, in the Debian headless image, as part of the required `build-and-test` gate.

### Changed
- **The Linux SDL2 minimum is now 2.0.18,** for switching VSync without a restart.
- **The built-in highlight fill is one step less opaque** (alpha `0x3F`, not `0x40`), which is what `FillOpacity=25%` has always given.
- **Invalid config values are logged and ignored,** as `Invalid <key> value '<value>' in [<section>], ignoring it`, and the default is kept. `IconSpacing=40px` is no longer read as 40.
- A percentage setting may have up to two decimals (`12.5%`).
- **American spelling throughout:** the settings screen, its notes and the log say *Color*, *Gray*, *Vertical center*, *grayed* and *canceled*, and so do the docs, the code and its file names (`colorpick.c`). The config's keys and values were American already (`Color`, `VCenter`, `Mode=Color`), so a config file reads and saves exactly as before.
- CI runs each headless pass in two shards, one job each, merged per pass by `merge.py`.
- The configuration guide's Transparent section says that icons are kept clear of the chroma key color, and that text and configured colors are not, so those are what the key must avoid.
- The research for a later sub-project, a 10-foot overlay for the DRM streaming sites in a real browser, is in `design/research/overlay/`.

### Fixed
- **`FPSLimit=10` works.** The documented minimum was refused, and anything up to 10 left VSync on.
- **A negative `[Clock] FontSize` is refused with a log line.** It used to wrap to a huge size.
- **`:exit` outside a Windows hotkey says why it does nothing,** where it used to do nothing silently.
- **Gamepads plugged in after another was removed are tracked correctly,** and the clock's background render is handed over safely.
- **A renderer that refuses VSync no longer runs the main loop uncapped,** and an `FPSLimit` is honored even when the renderer will not turn VSync off: StreamFlex paces each frame itself, and logs that it does.
- **A clock whose background thread cannot start renders in place** instead of freezing.
- **After a launched application exits, each gamepad is reopened at its current device index.** It used to be reopened at a stale one, and could get another pad.
- Repeating `DefaultMenu`, `StartupCmd`, `QuitCmd`, `Font` or `ControllerMappingsFile` in the config no longer leaks the earlier value.
- A BSD build compiles: the call that keeps a restarted StreamFlex's process name (`prctl`) and its header are Linux-only.
- The docs: the gamepad is on by default (the README and the docs home page said it was off), `PauseSlideshow` follows the background's `Mode` (not `BackgroundMode`), and `SlideshowTransitionTime`'s built-in default is 1.5 seconds, at most 3. With more than 62 menus, the Menus page lists 61 and a note.

## [0.3.1] - 2026-09-29

Fixes for what the hands-on check of 0.3.0 found on Windows 11 and Ubuntu 26.04: holes in icons in Transparent mode on Windows, a relative config path in settings' messages, and false errors while choosing a background. A config written for 0.3.0 works unchanged.

### Fixed
- **Transparent mode on Windows no longer punches holes in icons.** Windows shows through every pixel of the window that is exactly the chroma key color (`#010101` unless `ChromaKeyColor` says otherwise), so icon art containing that color turned partly see-through, and so did dark pixels that scaling blended onto it; the built-in Plex icon had a diagonal line and specks of it. Every icon, PNG or SVG, now has its opaque pixels within one step of the key lifted two steps off it as it loads (near-black becomes `#030303`), whatever the background mode, so no opaque pixel of an icon is within one step of the key, and dark art that was near it can no longer be averaged onto it when the icon is scaled. The built-in Plex, Hulu, Spotify, Twitch and Amazon Music icons are lifted the same way (none of them blends onto the key when scaled), and the icon library's check refuses brand art that is not.
- Settings and their messages name the config file by its full path. Started from its own folder, StreamFlex showed `Couldn't save to .\config.ini: ...` on screen and logged `.\config.ini`; it now says `C:\StreamFlex\config.ini`. When the full path differs from the path the file was found by (`.\config.ini`, or on Linux a link given with `-c`), the debug log's `Config file found:` line gives both: `.\config.ini (C:\StreamFlex\config.ini)`.
- Choosing *Image* or *Slideshow* in settings before picking an image or folder no longer logs errors blaming the config file (on Linux they also reached the terminal). The preview shows the color until one is chosen, and the debug log says so. A config that sets such a mode without an image or folder still reports it at startup.
- The debug log (`-d`) names every folder the settings' folder browser moves into, not only the one it opens in.
- The configuration guide and the hands-on checklists name the Menu key by its keycodes: SDL logs `#40000065` as `Application` or `Menu` depending on its version.

## [0.3.0] - 2026-09-29

The second part of the overhaul: a settings screen, opened with the remote's Menu key, a gamepad's Start or `:settings`, that changes the background, each menu's grid and the title size with a live preview, and saves only what changed into your config. A config written for 0.2.0 still works. Visible differences: the gamepad is on unless the config turns it off, titles scale with each menu's buttons (the same size as before on 256 px buttons), and the Windows zip no longer carries a Visual C++ runtime DLL.

### Added
- **A settings screen.** Press the Menu key on the remote, Start on a gamepad, or run the new `:settings` command, and change the background, each menu's grid and the title size from the remote, with a live preview beside the settings. A folder browser chooses a background image or slideshow folder, previewing each image as you move over it. Leaving saves only what changed into your config file, keeping its comments and layout, and keeps the previous version as `config.ini.bak`; nothing is written when nothing changed, and a save that fails says why and leaves the file as it was. On Linux, a read-only packaged config is saved as your own `~/.config/streamflex/config.ini`.
- The default config's System menu has a Settings tile, and its gamepad section maps Start to `:settings`. A config that maps nothing to `:settings` gets Start for it anyway, unless it uses Start for something else.
- The Menu key opens settings as either key SDL reports for it: a keyboard's context-menu key (`Application`, `#40000065`) or a remote's Menu button (`Menu`, `#40000076`). Holding it, or Start, or a hotkey bound to `:settings`, opens or closes settings once instead of flicking them at the repeat rate. Settings don't open while an application is launching or running.
- In the settings screen:
  - The Menus list's preview follows the cursor once it has rested for 300 ms, so moving down a long list no longer pauses at every row to lay out each menu passed.
  - The Menus page lists up to 62 menus; with more, a note counts the rest, whose grids are set in `config.ini`. A page longer than the screen keeps room for its note, and scrolls to its end to show it.
  - A *Couldn't save* message too long for the column, such as one naming a long path, is cut in the middle to fit, so what failed, why, and the *Try again* and *Leave without saving* rows all stay on screen.
  - The folder browser keeps a `C:/...` path's forward slashes, so the current image is highlighted and the save writes one style. A path too long for the config file is shown and refused with the reason, as is one that would not fit beside its line's comment, rather than failing the save. A Pictures folder on a network share is not opened unasked. On Linux, Pictures is the folder your desktop names in `user-dirs.dirs`, in any language (`~/Bilder`), and the drives in `/media/<user>` are listed.
  - The debug log (`-d`) says why an image cannot be opened (`Unsupported image format`, say), and the log says when a save could not keep the file's permissions.
  - On Linux, a `~/.config/streamflex/config.ini` that exists but cannot be read fails the save with the reason, instead of being replaced without a backup.
  - An install without its bundled font opens settings in the title font (or else the clock's) and logs which. Before, the Menu key did nothing. When no font opens, settings stay closed and the log says why as `Settings cannot open: ...`.
- The debug log (`-d`) names SDL's video driver and renderer at startup: `Video: SDL's x11 driver, the opengl renderer`.
- Headless tests: CI runs the launcher under a virtual display, drives it with key presses, and checks what it logs, saves and draws on screen. They run on two images, as the `Headless (Debian)` and `Headless (Fedora)` legs of one job; Fedora 44's SDL2 is sdl2-compat over SDL3, so the launcher is also tested where SDL3 stands in for SDL2. Both legs are part of the required `build-and-test` gate, and each runs all its passes even when one fails.
- The headless tests run every check a second time under LeakSanitizer, with no suppressions, and a leak in any run fails the job. A compiler warning outside `src/external/` fails it too.

### Changed
- **The gamepad is on by default.** A config with no `Enabled` line in `[Gamepad]` now has it on, and the default config says `Enabled=true`. Set `Enabled=false` to turn it off.
- **Titles scale with each menu's buttons.** `FontSize` and `Padding` take a percentage of the button size, and a config that doesn't set them now gets `14%` and `8%`: the same as before on 256 px buttons, smaller in a dense grid, larger on large buttons, and never below 2% of the screen height. A plain number still means a fixed size.
- The default config's `OversizeMode` is `Truncate`, so every title in a menu is the same size.
- **The Windows zip no longer ships `vcruntime140.dll`, or any other DLL.** StreamFlex is now built with the static C runtime, like the libraries it links, so it needs no Visual C++ runtime DLL and imports only Windows's own. Before, the link mixed three C runtimes (warning LNK4098).
- CI runs on `ubuntu-24.04` instead of `ubuntu-latest`, which GitHub moves to Ubuntu 26 from October 19, 2026. Every job now names its runner image (Windows was already `windows-2022` and Raspberry Pi `ubuntu-24.04-arm`), so a new image arrives in its own deliberate change rather than under a build that had not changed.
- **SVG icons draw more of what they ask for.** The SVG code StreamFlex builds in (nanosvg) is updated to its latest version: shapes styled by class from a `<style>` block get their colors, `paint-order` is followed, the last dash of a dashed outline gets its proper corners, and neither a malformed `rgb()` color nor a gradient whose numbers work out to not-a-number reads past the end of its data any more.

### Fixed
- The docs caught up with the icon library and the font fix. The configuration guide lists SVG among the supported image formats, says a relative `Font` path is also looked for next to the executable and that library icons don't depend on the working directory, and names the entry's middle field `icon` throughout. The README, the docs home page and the default config's comments point to the library; `CONTRIBUTING.md` counts the Icon library check in the required gate and points to the library's own tests; `SECURITY.md` includes the library manifest in scope. The setup guide's contents also named a section "Maintaining Controls" instead of "Maintaining Contrast".
- **If the scroll arrows cannot be drawn, StreamFlex turns them off and carries on.** Before, it freed their memory twice, once on turning them off and again at exit, which could crash on quit or corrupt memory.
- **A config without `HPadding` or `VPadding` gets the documented 30 px.** Before, an unset padding was -1: the highlight hugged the button with no padding, and an outline was drawn with a width of -1. The shipped config sets both, so it looks the same.
- **Back works after an entry opens the menu it is already in.** Before, that menu became its own Back target, so Back stayed where it was and the way back was lost until a restart.
- **`MaxButtons` and `IconSize` refuse junk, as `Rows` and `Columns` already did, and the log says so.** `MaxButtons=7x` used to be read as 7 and `IconSize=200px` as 200; both are now ignored. `IconSize` takes a whole number from 32 to 1024, in `[Layout]` or in a menu.
- **Huge `Rows`, `Columns`, `IconSpacing` or `VPadding` values no longer overflow the layout arithmetic.** `IconSpacing` is capped at the screen width, with a log line, and no more rows or columns are tried than 32 px buttons could fill. Before, `Columns=999999` with a screen-wide gap overflowed an `int` and laid out a nonsense grid.
- The debug log shows every menu's grid and button size, including menus that are never opened. Before, a menu's layout appeared only once it was loaded.
- A menu whose grid is reduced to fit the screen is reported in the log once, not every time it is opened.
- **Titles stay readable in dense grids.** Shrink mode made a long title smaller with no limit, down to a few pixels, and leaked a font each time it went all the way down. It now stops at 2% of the screen height and truncates the rest.
- **Titles on large buttons grow with them.** The title size was the same in every menu, so 1024 px buttons had titles sized for 256 px ones.
- **Title padding follows the button size.** It was sized for 256 px buttons whatever the grid.
- **`OversizeMode=Truncate` works.** The parser only knew `Truncated`, so the documented spelling was ignored.
- **StreamFlex starts on a display that reports no refresh rate**, as Xvfb, some VMs and remote desktops do. It divided by zero before drawing anything; it now uses 60 Hz and logs it.
- **File paths with non-ASCII characters work on Windows**, such as a config, log, icon library or slideshow folder under a user folder named `José`, and a command or shortcut whose path has such a character launches.
- **A menu section that appears twice keeps all its entries.** Its later entries and grid settings went to the menu read in between.
- **An empty menu entry (`Entry2=`) after another entry no longer crashes StreamFlex at startup.** It is skipped.
- `Mode=Slideshow` without a `SlideshowDirectory` handed the file system a null path at startup, which could crash StreamFlex; it now falls back to the color background. A slideshow folder with a single image no longer rewrites the `Image` setting.
- **A slideshow folder with no images, or only one, no longer corrupts memory at startup.** The fade speed was written into the slideshow after it had been freed.
- **A slideshow folder whose files all fail to load no longer hangs StreamFlex at startup.** It falls back to the color background.
- **A running slideshow that runs out of images falls back safely.** With one image left, its loader thread made a texture off the main thread; with none, it freed the slideshow and then wrote into it. The fall-back now happens on the main thread.
- **A `QuitCmd` is no longer freed twice when StreamFlex quits**, which could crash it on the way out.
- Invalid `Mode`, `Color`, `SlideshowImageDuration` and `SlideshowTransitionTime` values in `[Background]`, and invalid `FontSize` and `Padding` values, are logged; they were ignored without a word. The background `Color` refuses a value with a stray character (`#12345G`) instead of half-reading it.
- The configuration guide said comments could not follow a value. They can, after a space and a semicolon (`Columns=4 ; four across`), and the guide now says so.
- **A Linux slideshow takes upper-case extensions** (`DSC_0001.JPG`). Slideshows on both platforms leave out hidden files: on Linux a name starting with a dot, such as the `._` files macOS leaves on a share, and on Windows a file with the hidden or system attribute. The slideshow and the settings screen now use one rule, so the Folder row's count matches what the slideshow shows.
- **A slideshow could stop changing images for good** when its loader finished before StreamFlex marked it as loading. The loader's flags are now set before it starts.
- On a file system that does not say what kind each entry is (some CIFS, NFS and older XFS mounts), a link in a slideshow folder onto a network mount that is down, or a network mount that is down on a folder inside it, no longer holds up startup.
- **Quitting while the slideshow was reading its next image leaked that image**, and with the clock on, its font leaked at quit. Both are freed.
- **A title cut to fit never runs wider than its button.** Truncate, and Shrink at its smallest size, cut by the average letter's width, so a title of wide letters could reach 43 px past its button; the cut is now measured. When a smaller font fails to open in Shrink mode, the title is measured in the font it is drawn in, and a title size whose font cannot be opened lays the menu out for the fixed size it falls back to, rather than overlapping the row below.
- **A title with `Shadows=true` keeps room for its shadow**, below it and beside it. In a grid limited by the screen's height its buttons are 2-3 px smaller, and a cut title that much narrower.
- **A menu entry that is refused says why in the log:** one without a title, an icon and a command, or one whose command is `:select`. Both were dropped without a word.
- A config file that cannot be opened stops StreamFlex with a log line naming the file and the reason, not only "Could not open config file".
- **A log line longer than 500 bytes, such as one naming a long path, no longer reads past the log's buffer.** It is cut to fit.
- **On Linux, the clock no longer cuts `LANG` short for every application StreamFlex launches** (`en_US.UTF-8` became `en`), and with `LANG` unset it no longer reads freed memory at startup.
- **On Linux, StreamFlex no longer crashes at startup when `HOME` is not set**, as for a system service with no user or a launch through `env -i`. It takes the home folder from the user database, as a login shell does, for its log, its config search and the settings' save; with no usable home there either, it logs to stderr.

## [0.2.0] - 2026-09-27

The first part of the overhaul. Menus can be grids of several rows, buttons grow to fill the screen, and an entry can name an icon from a built-in library instead of giving a file path. A config written for 0.1.3 still works. One visible difference: a menu that doesn't set `IconSize` now gets buttons that grow to fill the screen, where it used to get 256 px.

### Added
- **A built-in icon library.** The app icons of 33 streaming and media services, and 36 generic icons for system actions, kinds of media, general use and devices, all on one rounded-square outline. An entry names one instead of a path: `Entry1=Netflix;netflix;...`. The docs site has a gallery of every icon and its name.
- **Menus can show several rows of buttons.** `Rows` sets how many rows are visible at once. With two or more, a menu is a grid: Left and Right stop at the end of a row, Up and Down move between rows, and the grid scrolls one row at a time. A single row still scrolls sideways.
- **Buttons are sized to fill the grid.** Choose its shape with `Rows` and `Columns`, and the buttons scale to fit the screen, titles included.
- **Each menu can have its own layout.** `Rows`, `Columns` and `IconSize` in a menu's section override the `[Layout]` settings for that menu.
- The `:up` and `:down` special commands. The Up and Down arrow keys also move between rows; a hotkey already bound to Up or Down keeps working.
- Gamepads get Up and Down by default. When the config maps nothing to `:up` or `:down`, the D-pad and the left stick's vertical axis run them, unless the config uses those controls for something else.
- Unit tests for the layout logic, run by CTest in CI.

### Changed
- **The default config uses library icons.** The seven Numix icons are gone. A config that still points at one of their old files gets the matching library icon, and the log suggests its name.
- **`MaxButtons` is now `Columns`.** The old name still works; if both are set, `Columns` wins.
- **`IconSize` is now the largest a button may grow**, not a fixed size. Menus that set it look the same as before. A config with no `IconSize` line now gets buttons that grow to fill the screen, where it used to get 256 px.
- **A single row with more buttons than fit now slides one button at a time**, instead of flipping to the next page.
- **If `IconSpacing` is too large for a row to fit at `IconSize`, the buttons now shrink to fit.** Before, the spacing was reduced instead. The spacing is kept as set, and the button follows the grid.
- **With the clock on, a menu is kept below it.** A row set high with `VCenter` (for example `25%` with the time and date shown) moves down just far enough to clear the clock, where before it could overlap it.
- SVG icons are drawn at the button's size, so they stay sharp at any size.

### Fixed
- On Windows, the default config's icons no longer depend on the folder StreamFlex was started from.
- A font named by a relative path (the Windows default config uses `.\assets\fonts\...`) is now found next to the executable when StreamFlex is started from another folder, such as a shortcut's "Start in" folder. Before, it logged "Could not initialize font from config file" and fell back to the default font.
- The README, the setup guide and the `OversizeMode` and Scroll Indicators docs still described the single-row layout, and the setup guide said SVG menu icons were not supported. They now describe grids, and recommend SVG icons.
- Titles on very small buttons no longer write outside their memory. When fewer than three characters of a title fitted, truncating it walked back past the start of the text, which could crash the launcher or corrupt memory. It becomes `...` instead, and a one- or two-character title is left as it is. Small buttons were rare before grids; a dense grid on a small screen makes them ordinary.

## [0.1.3] - 2026-09-27

On Linux the app icon now comes in the small sizes that menus and panels use. Everything else here is about the icon's source and tooling; the launcher behaves exactly as in 0.1.2.

### Added
- The app icon's source is in the repository under `branding/icon/`. `build-svg.py` holds its geometry and colors and writes the two vector masters, and `build-icon.ps1` renders, packs, installs and verifies all four icon files. Run on the committed source, it reproduces the 0.1.2 icon byte for byte, so a later icon change can be regenerated rather than redrawn.
- Three icon tools sit beside it. `review-sheet.py` lays every icon size out on light and dark backgrounds, with pixel zooms of the small sizes, for judging a design by eye. `render-check.py` renders the icon in Chromium and Inkscape and measures the difference, a portability check for design changes. `palette.py` re-derives the icon's colors from the logo banner.
- On Windows, `build-icon.ps1` also checks that the icon reads correctly through WIC (Explorer's decoder) and GDI+, and that `rc.exe` compiles it.
- The full-resolution logo original (2816×1536) is in the repository as `branding/logo/streamflex-logo.jpg`. The README and docs banner is a downscale of it.
- The Windows build instructions in the compilation guide now reproduce the CI build: Visual Studio 2022, vcpkg pinned to the version CI uses, and CI's generator and library settings. The `build` directory is ignored by git.

### Fixed
- **Linux: the app icon now comes in 16, 24 and 32 px sizes**, drawn from the icon's simplified small-size design. Until now only the 48 px and scalable icons were installed, so menus, panels and taskbars at small sizes shrank the detailed artwork, and the design made for small sizes only reached Windows.

## [0.1.2] - 2026-09-27

StreamFlex's own app icon. The launcher behaves exactly as in 0.1.1.

### Changed
- **New app icon**, drawn from the StreamFlex logo. It shows the logo's teal ring and rising ribbon arrow around an app-window glyph, on a dark-teal rounded plate. The icon is platform-neutral, so unlike the banner it has no penguin. It replaces the icon inherited from the original project everywhere:
  - The Windows executable (`config/streamflex.ico`): ten sizes from 16 to 256 px, all with alpha. The small sizes use a simplified, bolder drawing so they stay legible.
  - The Linux desktop entry: `streamflex.svg` for the scalable icon and a 48×48 `streamflex.png`, installed into the hicolor theme.
  - The documentation site's favicon.

## [0.1.1] - 2026-09-27

First release under the name StreamFlex. The launcher behaves exactly as in 0.1.0; what changes is its name, and with it the executable, package and directory names listed below. The new app icon follows in 0.1.2.

### Changed
- **The project is renamed from Flex Launcher to StreamFlex**, with a new logo. Everything named after the project follows, so an existing install does not carry over:
  - The executable is `streamflex` (`streamflex.exe` on Windows), and the packages are `streamflex_<version>_amd64.deb`, `streamflex_<version>_arm64.deb`, `streamflex-<version>-1-x86_64.pkg.tar.zst` and `streamflex-<version>-win64.zip`. The Debian/Arch package name is `streamflex`; the `flex-launcher` package is not replaced or removed by it.
  - On Linux the user config directory is now `~/.config/streamflex/` (was `~/.config/flex-launcher/`), the debug log is written to `~/.local/share/streamflex/`, and the packaged defaults install to `/usr/share/streamflex/`. Move an existing config with `mv ~/.config/flex-launcher ~/.config/streamflex` and replace `flex-launcher` with `streamflex` in any paths inside it.
  - The repository is `bilbospocketses/streamflex` and the documentation site is https://bilbospocketses.github.io/streamflex/.
  - The Debian packages carry no epoch (`0.1.1`, not `1:0.1.1`). The epoch on v0.1.0 existed to sort the `flex-launcher` package above the original project's 2.2; the new `streamflex` package name has no earlier versions to outrank.
- The Debian and Raspberry Pi packages list each dependency once. The `Depends` field combined a hand-written list with the one `dpkg-shlibdeps` computes from the binary, so every library appeared twice; it is now the computed list alone.
- CI builds a pull request once per commit instead of twice. The build workflow's `push` trigger now fires only for `master` and `v*` tags, leaving PR branches to the `pull_request` trigger.
- The docs site's download page takes its version and file links from the latest published release, read at build time, and the `Release` job rebuilds the site once a release is published. Previously the version was bumped by hand in the release PR and deployed when that PR merged, so the download links pointed at a release that did not exist yet until the tag was pushed and published. `launcher_version` in `docs/_config.yml` is gone.

### Fixed
- The documentation site no longer reports visits to the original author's Google Analytics property. A `docs/_includes/head-custom-google-analytics.html` override inherited from the original project hardcoded its measurement ID; it is removed, so the theme's default include applies and stays inactive unless `google_analytics` is set in `docs/_config.yml`.
- `CONTRIBUTING.md` described three build targets where there are four (Arch Linux was missing from the build-and-test gate description) and misdescribed the `config/` and `assets/` directories.
- The compilation guide's dependency list now includes inih, and getopt on Windows.

## [0.1.0] - 2026-09-26

First release of the independent project. Versioning restarts at 0.1.0; the project stays below 1.0 until its major overhaul is in place. There are no changes to the launcher's behavior or configuration compared with the original v2.2 — this release is about licensing, packaging, and the build.

### Added
- Arch Linux package (`.pkg.tar.zst`), built with makepkg from the existing PKGBUILD template.
- Every release package now ships with Sigstore build provenance attestations, so a download can be verified with `gh attestation verify <file> -R bilbospocketses/flex-launcher`.
- The packages include the license text: `LICENSE.txt` in the Windows zip, and `/usr/share/licenses/flex-launcher/LICENSE` in the Linux packages.
- Documentation site at https://bilbospocketses.github.io/flex-launcher/, built from `docs/` and deployed by the `pages.yml` workflow.
- `SECURITY.md` with a private vulnerability reporting flow, `CONTRIBUTING.md` including a release procedure, and `.github/CODEOWNERS`.
- CI: a `build-and-test` gate (the required status check for `master`) that passes only when the Windows, Debian, Raspberry Pi, and Arch builds all succeed; a single `Release` job that dry-runs on every pull request; OpenSSF Scorecard; CodeQL; Dependabot for GitHub Actions with auto-merge.

### Changed
- **License: GNU General Public License v3.0**, replacing the Unlicense. The original project's code was public domain, which permits relicensing; the original author is credited in the README.
- **Linux packages now need Debian 12 / Ubuntu 22.04 / Raspberry Pi OS 12 (Bookworm) or newer.** They are built on Debian 12 instead of Debian 11, which is past end of life.
- The Debian packages' dependencies, including the minimum glibc, are now computed from the built binary by `dpkg-shlibdeps`. Previously they declared `libc6 (>= 2.31)` while the binary needed glibc 2.34, so the package would install on systems where it could not run.
- The Debian packages carry epoch 1 (`1:0.1.0`), so apt treats them as newer than the original project's 2.2 packages despite the lower version number.
- Package metadata points at this project: homepage `https://github.com/bilbospocketses/flex-launcher`, maintainer `bilbospocketses`. The README, docs site, and the default `config.ini` link to this project's releases and documentation.
- The Windows zip ships this changelog as `CHANGELOG.txt` instead of the original project's frozen one.
- The repository is a standalone project, detached from complexlogic/flex-launcher's fork network.
- The Raspberry Pi package is built in a Debian Bookworm container on a native arm64 runner, replacing the QEMU chroot into a 2022 Raspberry Pi OS image.
- Build workflows: every action pinned to a commit SHA and updated to its current release, read-only token by default, and releases published with the repository's own token.

### Fixed
- Windows build. The previous vcpkg pin fetched `getopt-win32` from a GitHub archive that now returns 404; vcpkg is now pinned to release 2026.07.29.
- Debian and Raspberry Pi builds, which failed at dependency install because the Debian 11 security mirror now returns 404.
