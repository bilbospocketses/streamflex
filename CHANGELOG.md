# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/).

This project started from complexlogic's Flex Launcher at v2.2 and is developed independently. The original release history up to v2.2 is in [`CHANGELOG`](CHANGELOG), which is frozen.

## [Unreleased]

## [0.6.0] - 2026-10-18

v0.4.1 is the last release built on SDL2. From v0.5.0, StreamFlex is built on SDL3 and needs Debian trixie, Raspberry Pi OS trixie or Ubuntu 25.10 (or a newer release of each), Arch Linux, or Windows. Debian bookworm, Ubuntu 24.04 LTS and older releases package no SDL3, so users there stay on v0.4.1, which stays downloadable; there is no backport.

### Added
- **Editing menus from the remote.** Settings › Menus › *<menu>* › Tiles adds, removes, moves and renames a menu's tiles and submenus, and chooses their icons, with no config file to edit. *Add* takes an installed app, a program browsed to, a new submenu or a special command (Home, Back, Settings, Sleep, Restart, Shut down, Quit), at the end of the menu, or, from a tile's page, right after that tile. A tile's page changes its title, icon and command, moves it through the grid as the arrows move the highlight, with the preview following, or to another menu, and removes it. Renaming a submenu renames its section and every tile, hotkey, gamepad control, `DefaultMenu`, startup and quit command that opens it; removing one asks first, and the default menu cannot be removed. The save keeps the settings screen's rules: only what changed is written, comments and order stay, a hand edit made meanwhile is kept, and *Discard changes* puts every tile back.
- **An on-screen keyboard.** StreamFlex's own keyboard types titles, menu names, commands, paths and filters with the remote's arrows and OK, or a gamepad (X deletes, Y types a space, Start is Done). It has a QWERTY layout and an alphabetical one, remembered as `[General] KeyboardLayout` and set on the General page too, with a page of digits and symbols and one of accents. A real keyboard types into it in its own layout, with Shift, Caps Lock and AltGr; Enter and Backspace stay the remote's OK and Back until something is typed on it, then are Done and delete. The command picker gains *Type…*, and the image, folder and mappings-file browsers *Type a path…*, where a typed path is looked for when it is done, and the keyboard opens again on it with the reason when it is not there. No operating system's on-screen keyboard ever appears: StreamFlex never turns on text input, so Windows' touch keyboard, GNOME's screen keyboard and KDE's virtual keyboard stay hidden.
- **The installed apps list.** *Add › App* lists the installed apps: on Windows the Start menu's, desktop and Store apps alike (a tile runs `explorer.exe shell:AppsFolder\<app ID>`, with the argument in double quotes when the ID holds a space or a comma); on Linux the `.desktop` files of the system, the user, Flatpak and Snap, named in the system's language, without what the desktop's own menu hides, apps that run in a terminal, uninstallers or readmes. It is found on a thread of its own the first time it opens in a visit, with a count as it goes, and is filtered by typing.
- **Icons for tiles.** A tile's *Icon* page offers the library icon matching its title (the library's manifest gains `match =` aliases), the app's own icon on the library's teal plate, saved as a 512 px PNG in an `icons` folder (beside `config.ini` on Windows, `~/.config/streamflex/icons` on Linux), the whole library in a grid by group with a typed filter, or an image file, and the preview shows each on the tile. A new app's tile gets the library's icon when there is one, else its own icon, plated and saved as it is added, else the generic one.
- **The hold-OK shortcut.** Holding OK (Return, or the pad's A) on a tile for 0.8 s opens settings at that tile's page, and closing them lands back on the tile; in an empty menu it opens the menu's Tiles page. Opening settings another way while OK is held cancels the hold.
- **More library icons.** The icon library gains 19 generic icons: notes, calculator, clock, weather, calendar, mail, chat, documents, code, cloud, archive, store, camera, paint, security, backup, tools, accessibility and remote, with `match =` aliases where an app's name differs from the icon's. It holds 88: 55 generic and 33 brand.
- **A Fedora 44 download.** Each release now carries `streamflex-<version>-fedora44-x86_64.tar.gz`, built on Fedora 44 against its release repositories, beside the Windows, Debian, Raspberry Pi and Arch packages. It is a tarball, not an `.rpm`: it lists the packages it needs in `usr/share/streamflex/REQUIRES-fedora44.txt`, which `dnf install` takes, and unpacks onto `/` with the `.deb`'s layout (the README gives both commands). dnf does not track its files, so it is upgraded by unpacking the new tarball over the old one and removed by hand. CI installs every build in a fresh Fedora 44 from that list alone, and fails if a library the program needs is left out.
- **The desktop's media keys, on KDE Plasma and GNOME.** Both desktops take the next-track and previous-track keys for their own media controls, so StreamFlex never received them. At every start, while the desktop holds either key, settings now open on a *Media keys* prompt, and it comes before the app a `StartupCmd` launches, which waits until the prompt closes. *Turn them off* saves the desktop's shortcuts to a `[Desktop Keys]` section of `config.ini` first, then turns them off; *Not now* changes nothing and asks again at the next start; *Don't ask again* saves `Ask=no` there. Settings › Controls gains a *Desktop media keys* row, reading *On* while the desktop holds either key and *Off* while it holds neither, whose page gives them back (the saved ones, else the desktop's defaults) or turns them off again. Plasma's shortcuts are changed through kglobalaccel over D-Bus, so StreamFlex on Linux now needs libdbus-1 (`libdbus-1-3` on Debian and Ubuntu); GNOME's through `gsettings`. Windows has no prompt and no row: nothing there takes the keys.

### Changed
- CI no longer stops a job for taking long. A slow Fedora mirror made a headless shard's test image take 29 minutes to build, and the job's 60-minute limit then killed the shard five seconds after its last passing check. No job has a time limit now. In the Build and Smoke coverage workflows, every step that builds, installs, downloads or tests, on Linux and on Windows, runs under a watchdog that stops it only when, for 20 minutes, it has printed no line and shown no other sign of work: on Linux, the runner receiving 4 MiB; on Windows, where the runner's own background traffic would hide a hang, the step's processes using 60 s of CPU or doing 4 MiB of I/O. It then prints what was running, its last output and the process tree. Each step's duration is reported as a notice.
- **OK selects a tile as it is let go** in the launcher's menus, so that holding it can open the tile's page; a quick press feels the same. Return, hotkeys and gamepad controls bound to `:select` all work this way; inside settings OK still acts as it is pressed, and a mouse click still selects at once.
- **A menu with no entries is shown.** A section with no entries (a header alone, or only comments) is a menu: it shows *This menu is empty. Hold OK to add tiles.* in the launcher and in the settings preview, and `DefaultMenu` may name one. Before, such a menu could not be shown, and a `DefaultMenu` naming it stopped StreamFlex at start.
- **Entry lines in no section are ignored.** Lines before the first `[section]`, or under an empty `[]` header, used to make a menu with no name; now they are in no menu, and the log says how many were ignored.
- The headless harness gains checks 80 to 87 for menu editing, and the mutant driver runs unit mutants through `tests/unit.sh` and judges every mutant against its unmutated control.
- The smoke doc gains module 18, *Menu editing*, and retires row 6.17 for 18.6.
- The smoke doc gains rows 1.36 to 1.53, the Fedora tarball's install on Fedora Workstation and Fedora KDE, and its Fedora pre-flight installs the tarball.

### Known limits
- A real keyboard's dead key does not combine with the next letter: on Linux it types nothing (the keyboard's *?123* page has `^`), and on Windows it types its own accent and the next key types on its own. An input method is bypassed, so a Japanese or Chinese layout types Latin letters; the keyboard's accents page covers the European accents.
- A config saved by v0.6.0 on Linux can hold a `[Desktop Keys]` section, which an older StreamFlex reads as a menu named *Desktop Keys*.
- On KDE Plasma, changing the media-key shortcuts is proven only with kglobalaccel running inside KWin (Plasma 6 on Wayland); the standalone kglobalaccel daemon is unmeasured.

## [0.5.1] - 2026-10-16

v0.4.1 is the last release built on SDL2. From v0.5.0, StreamFlex is built on SDL3 and needs Debian trixie, Raspberry Pi OS trixie or Ubuntu 25.10 (or a newer release of each), Arch Linux, or Windows. Debian bookworm, Ubuntu 24.04 LTS and older releases package no SDL3, so users there stay on v0.4.1, which stays downloadable; there is no backport.

### Fixed
- On Windows, after a Transparent window fell back to the Color background (no renderer could draw it), stepping *Mode* to another mode and back to *Transparent* within 300 ms tries Transparent again, as stepping slowly does. Before, the quick step left the row reading *Transparent* on an opaque window, and logged nothing.
- On Windows, a picker or folder browser opened within 300 ms of stepping *Mode* to or from *Transparent* stays open. Before, the window was recreated 300 ms after the step wherever the cursor had gone, and the recreation closed the picker (a color picker put its color back). Leaving the *Mode* row before that now leaves the window as it is until settings close, or until *Mode* is stepped and rests again.
- On Windows, a Transparent window whose renderer is lost three times falls back to the Color background, with the reason in the log (`Video: no renderer can draw a transparent window (the renderer was lost 3 times on this window); using an opaque background`), instead of making a new renderer every time. A driver that kept failing the new renderer's frames would have reloaded every icon and title over and over. Transparent is tried again once another mode is chosen and then *Transparent* again. An opaque window still gets a new renderer every time.

## [0.5.0] - 2026-10-15

v0.4.1 is the last release built on SDL2. From v0.5.0, StreamFlex is built on SDL3 and needs Debian trixie, Raspberry Pi OS trixie or Ubuntu 25.10 (or a newer release of each), Arch Linux, or Windows. Debian bookworm, Ubuntu 24.04 LTS and older releases package no SDL3, so users there stay on v0.4.1, which stays downloadable; there is no backport.

StreamFlex now runs on SDL3 and draws with Vulkan first. Text the title font cannot draw comes from fallback fonts, Linux desktops with a current compositor run StreamFlex as a native Wayland client, the screen stays sharp at any display scale and follows a change of resolution, a gamepad low on its battery shows a small glyph, and on Windows the Transparent background is see-through pixel by pixel, with soft edges. A config written for 0.4.x reads and saves as before.

### Added
- **Vulkan first.** StreamFlex asks SDL for `vulkan,direct3d12,direct3d11,opengl` on Windows and `vulkan,opengl` on Linux and the Raspberry Pi, and SDL takes the first that works; on SDL2 it drew with Direct3D 9 on Windows and OpenGL on Linux. The debug log names the renderer (`Renderer: vulkan`) and says when it is not the first choice (`Renderer: opengl (vulkan was unavailable)`). The `SDL_RENDER_DRIVER` environment variable chooses another renderer (`Renderer: opengl (from SDL_RENDER_DRIVER)`); there is no setting for it.
- **A lost graphics device is recovered.** When the GPU is reset or lost (a driver update, a GPU reset), StreamFlex rebuilds everything it draws and keeps the menu and the settings screen as they were. When the renderer itself is lost, StreamFlex makes a new one, up to five tries 2 seconds apart while the window keeps responding (keys and gamepads do nothing until the new renderer exists); when the loss comes while a launched app runs, it waits for the app to end first.
- **Fallback fonts.** A title, menu name or settings value in a script the font lacks (Chinese, Japanese, Korean, Indic, Thai, Arabic, Hebrew, symbols and emoji) is drawn from a font that has it, instead of as boxes: on Linux from the fonts fontconfig offers, on Windows from Windows' own fonts (Segoe UI, Segoe UI Emoji, Yu Gothic, Microsoft YaHei, Malgun Gothic, Nirmala UI and others, the CJK font first chosen by the preferred language), and from the bundled DejaVu Sans last. The clock keeps its one font. A character no installed font has still shows as a box, and the log says so once, even without `-d`: `Fonts: no installed font draws U+1F579; it shows as a box`. A config whose text the font covers opens no other font. There is no setting.
- **Native Wayland.** SDL now chooses the video driver: Wayland when the compositor offers `wp_fifo_manager_v1` (GNOME 48 and later, KDE Plasma 6.4 and later), X11 otherwise. Ubuntu and Kubuntu 25.10 and later, Fedora, and Debian trixie's GNOME run StreamFlex as a native Wayland client; Debian trixie's Plasma 6.3 stays on XWayland, and the debug log says why. The `SDL_VIDEO_DRIVER` environment variable (or SDL2's `SDL_VIDEODRIVER`) still forces a driver. The Wayland app id and the X11 window class are `streamflex`, matching `streamflex.desktop`, however StreamFlex was started.
- **Sharp at any display scale.** On a scaled Wayland output StreamFlex draws at the output's native pixels, and on Windows it is per-monitor DPI aware (v2). The OS scale is logged (`Screen: 3840x2160 px, pixel density 1.00, display scale 1.50`) but not applied: a setting in pixels or points keeps meaning pixels and points, and the layout is already sized as a share of the screen.
- **A change of screen size is handled.** A resolution change, a TV switching mode, a monitor plugged in or a move to another display lays StreamFlex out again at the new size, keeping the menu, and the settings screen with its page and unsaved changes; an open picker or folder browser closes as Back would. A change while a launched app runs waits until the app ends, so a game that switches the resolution and back costs nothing. A new display mode with another refresh rate (a TV switching from 4K at 60 Hz to 1080p at 120 Hz) also sets the frame timing to the new rate. On SDL2, StreamFlex never handled a size change.
- **A low-battery glyph for gamepads.** While a connected pad runs on its battery at 40 % or below, a small battery glyph shows in the top corner opposite the clock: amber, and red at 15 % or below. It hides again above 45 %, and shows nothing for a pad that is charging, on its cable, or reports no battery. With several pads it shows the lowest. There is no setting.
- **Startup timing in the debug log** (`-d`). `Timing:` lines give the time a menu's icons and titles took, split into reading, decoding and making textures, the time of a background image, and the time from the window opening to the first menu shown.
- **A menu's icons are decoded on several threads at once,** one fewer than the machine's logical cores, from one up to four, and the menu still appears whole. The debug log says how many: `Icons: decoding on 3 threads`.

### Changed
- **StreamFlex is built on SDL3** (SDL3 3.2.10, SDL3_image 3.2.4 and SDL3_ttf 3.2.2 or later), and the Linux packages on Debian trixie. The `.deb` depends on `libsdl3-0`, `libsdl3-image0`, `libsdl3-ttf0` and `libfontconfig1`, the Arch package on `sdl3`, `sdl3_image`, `sdl3_ttf` and `fontconfig`. The Windows zip is still one static executable, with no DLLs.
- **Transparent mode on Windows is per-pixel alpha.** The background is truly see-through, and the soft edges of icons, titles and the highlight blend into the desktop with no key color, so the chroma key's holes in dark art cannot occur. `ChromaKeyColor` is ignored on Windows (it is still read and saved, so a config shared with Linux keeps it), and the settings screen shows no *See-through color* row there. The window is one pixel row short of the display and marked fullscreen, so the taskbar never draws over icons or titles; a taskbar behind the window shows through the see-through background, and the missing row shows the desktop. It draws with Vulkan, or with Direct3D 11 where Vulkan is missing or offers no premultiplied alpha. Switching to or from Transparent in settings recreates the window, keeping the page and its edits. A Transparent window no renderer can draw (with `SDL_RENDER_DRIVER` set to `direct3d12`, `direct3d` or `opengl`, for example) falls back to the Color background instead of exiting. The see-through area now takes mouse clicks. Transparent on Linux is unchanged: the chroma key, with a compositor's shader on X11.
- **A background overlay now tints the desktop in Transparent mode on Windows,** at its opacity. Under the chroma key an overlay made the Transparent background fully opaque, because no pixel stayed the key color. The screensaver's dim does the same: it darkens the desktop at its dim level instead of turning it black.
- **Text is shaped with HarfBuzz on Windows too,** as every Linux build already was: Arabic joins and runs right to left, Indic scripts reorder, emoji sequences combine, and kerning applies.
- **Nintendo pads use SDL3's positional buttons.** `ButtonA` is the bottom button on every pad, which on a Switch Pro is the one labeled B, so a config written for v0.4 that binds `ButtonA` answers to that button on a Nintendo pad; Xbox and PlayStation pads keep the buttons they had. Buttons that follow a Nintendo pad's labels come in a later release.
- **PlayStation pads run in SDL's enhanced mode.** SDL3 switches DualShock 4 and DualSense pads to their enhanced reports, which over Bluetooth are the only ones that carry the battery, and StreamFlex keeps SDL's default. Once switched, a pad stays so until it is powered off, and a launched app that reads it through DirectInput without SDL may misread it until then.
- **The Windows manifest declares per-monitor v2 DPI awareness** (it was per-monitor v1), and the debug log says which one the process has: `DPI awareness: per-monitor v2`.
- **The debug log's `Resolution:` is the window's size in pixels,** which a fullscreen window shares with the display; on Windows in Transparent mode it is one row less.
- **The slideshow's fade, the screensaver's dim and a held gamepad control's repeat are timed by the clock, not counted in frames,** so they take their set time however many frames a second the machine draws; on SDL2 a machine drawing fewer frames a second than its display shows stretched them. Two effects of the change: a fade still running when an app launches ends on the first frame after the app returns (before, it paused while nothing was drawn), and changing *Fade* (`SlideshowTransitionTime`) in settings during a fade moves it at once to the new time's share, and a new time of 0 ends it (before, a change to 0 kept the old rate).
- The configuration guide's Exit Hotkey section says what happens when another program holds the key: Windows refuses it, and StreamFlex keeps trying until the key is free.
- A CI run on master or on a release tag no longer runs the sanitized unit tests and the headless shards again when the PR it came from already passed them on the same files, so cutting a release takes about 40 minutes instead of about 75.
- The hands-on checks are one smoke doc, `tests/qa/smoke-test.md`, with a row per observable claim on five desktops, and a coverage register saying which rows the unit tests and the headless harness already prove. The smoke doc replaces the four per-release checklists. From v0.6.0, each release records its verdicts in a results file; this release has none.
- A new required CI check, `smoke-coverage`, fails a PR that changes code a user can see unless it updates the smoke doc or says in its body why it needs no change, and keeps the smoke doc, the register and the results files well-formed.
- CI's American spelling check uses v1.0.3 of the word list.

### Fixed
- An icon file named `.svg` that holds no SVG, such as a text file, is refused, and under the log's `Could not load image` line, which names the file, the reason reads `could not open SVG image.` nanosvg read it as an image of size zero, and StreamFlex divided by that size to scale it, which is undefined behavior.
- A title of only zero-width characters, such as a zero-width space (U+200B) used to hide one entry's title, no longer crashes StreamFlex as it draws the menu: the button shows no title. SDL_ttf renders no image for a text with no width, and StreamFlex read the one it never got.
- When SDL cannot give the folder StreamFlex's executable is in, StreamFlex no longer crashes: it looks for its bundled font only where the packages install it (on Windows, `assets\fonts` under the folder it was started in), and on Windows the log goes to stderr instead of to a file beside the executable. Both read that folder's path without checking there was one.

### Known limits
- Linux draws emoji in monochrome, from a font such as DejaVu Sans: SDL3_ttf 3.2.2 cannot scale the color emoji fonts Linux ships (Noto Color Emoji is a bitmap font, and Fedora's COLRv1 emoji draw nothing), so StreamFlex skips them. Windows draws emoji in color, from Segoe UI Emoji.
- On Linux, a character drawn from a fallback font can sit slightly above or below the line (Noto Sans CJK measured 4 px low at 48 pt), and can make a title a few pixels taller than its font's height. The Windows build carries a fix for it (SDL_ttf issue #629); Linux uses the distribution's SDL3_ttf.
- On Linux with SDL 3.4.18 or older, a CJK Extension B or later character (U+20000 and up) at the end of a title can draw as the replacement character, U+FFFD: SDL_ttf's fallback code finds the end of the text with SDL's `SDL_StepUTF8`, which reads such a character as invalid at those versions. The Windows build carries SDL's fix for it (libsdl-org/SDL `57f22e6e43`); Linux uses the distribution's SDL3.
- A title mixing left-to-right and right-to-left text is not reordered: SDL_ttf has no bidirectional algorithm. Such a title is also shaped wrong when the title font itself covers the right-to-left script.
- A gamepad's battery shows only where the pad and the platform report one. On Linux that is only through SDL's HIDAPI, which needs udev rules giving the user the pad's `hidraw` node (Steam's, or `game-devices-udev`); without them, no glyph shows.
- A hotkey that a v0.4 config binds to a key SDL3 no longer sends (SDL2's Mail, WWW, Calculator and the like) is kept in the config but never fires, and the settings screen shows it by its code, such as `#40000109` for Mail.
- With VSync on, a display that runs faster than the refresh rate it reports (one that says 60 Hz, or reports none, which StreamFlex takes as 60 Hz, and runs at 85 to 120 Hz) is paced to the rate it reports: StreamFlex shows about 60 frames a second there, with some judder, and the log says `Frame timing: the renderer claims VSync but does not deliver it`. Nothing is timed in frames, so only smoothness suffers.
- HarfBuzz makes the Windows zip 0.4 MB larger; the zip is 5.9 MB.
- Transparent on Windows may not work on every machine: SDL notes that per-pixel transparency "does not work on all hardware configurations", such as laptops whose display is driven by one GPU while another one renders. Before the port, a test program showed it working on one machine, whose display its graphics card drives; this release's Transparent mode was not run on a real graphics card.
- On Windows, Transparent draws with Direct3D 11 where Vulkan is missing or offers no premultiplied alpha for a window.
- The Raspberry Pi package is built, and its metadata checked, in CI; it was not checked by hand on a Pi.

## [0.4.1] - 2026-10-10

On Windows, an exit hotkey that another program held when StreamFlex started now starts working once that program lets go of the key, with no restart. Checked by hand on Windows 11. CI now checks for American spelling and pulls its images through mirror.gcr.io, and the headless tests no longer fail at random on a slow runner.

### Changed
- CI checks that added lines and commit messages use American spelling.
- CI pulls the test images' bases, and the Linux, Raspberry Pi and Arch Linux build containers, through mirror.gcr.io.

### Fixed
- The headless harness waits for the launcher to draw a frame after each key it presses. On a slow CI runner, several keys could be handled with no frame between them, so a check whose line only a drawn frame logs failed at random (seen in `63-settings-mappings.sh`).
- On Windows, an exit hotkey that Windows refused because another program held its key is tried again: every 5 seconds, when StreamFlex comes back to the front, and when settings close. Once the key is free it is registered, and the log says so once (`Registered the exit hotkey with Windows: the key is free now`), so the hotkey works without a restart. It had stayed dead until StreamFlex restarted or the bindings changed.

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
