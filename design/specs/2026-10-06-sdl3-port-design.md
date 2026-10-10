# The SDL3 port, Vulkan first: design

**Date:** 2026-10-06
**Status:** approved in brainstorming, awaiting spec review
**Amended:** 2026-10-10, scope extended by the user: fallback fonts, native Wayland, display scaling, a gamepad battery glyph, and startup timing with a measurement gate. Device loss also handles `SDL_EVENT_RENDER_DEVICE_LOST`, and v0.4.1 is the last release for Debian bookworm and Ubuntu 24.04 LTS.
**Covers:** the SDL3 port sub-project (todo item 7's order: after 3b, before the 3c plan). StreamFlex moves from SDL2, SDL2_image and SDL2_ttf to SDL3, SDL3_image and SDL3_ttf, asks for Vulkan first on every platform, and recovers a lost GPU device. It also takes five SDL3 capabilities: fallback fonts, native Wayland on Linux, sharp drawing at any display scale, a low-battery glyph for gamepads, and startup timing.
**Ships as:** **v0.5.0**, on its own. 3c moves to **v0.6.0**.
**Builds on:** 3b (`feat/settings-all`), which shipped as v0.4.0 and has merged; v0.4.1 has shipped since. **The implementation plan is written next**; it names exact functions and lines on master.

## Context

StreamFlex is C on SDL2. SDL2 provides the window, the renderer, input, gamepads and threads; SDL2_image (with libjpeg-turbo, libwebp and libpng) loads images; SDL2_ttf (FreeType, plus HarfBuzz on distro builds) draws text. Measured at `e13f2f6` (3b, Task 13), about 535 lines across 22 files under `src/` use SDL, most of them in `launcher.c` (231), `settings_screen.c` (76), `image.c` (67) and `util.c` (27). The rest of the stack is inih, getopt on Windows, vendored nanosvg, and a thin OS layer in `src/platform/`.

The renderer asks for nothing in particular today (`launcher.c:362`, `SDL_CreateRenderer(window, -1, flags)`), and SDL2 picks **Direct3D 9 on Windows** and **OpenGL on Linux**. SDL2's renderer has no Vulkan backend. SDL3's has `vulkan`, `gpu`, `direct3d12` and `direct3d11`.

The user's binding decisions (todo § Binding decisions, 2026-09-30) set the direction:
- **Bleeding edge.** StreamFlex targets current technology and does not support OS releases that lack it. A user on an older Debian, Ubuntu, RHEL or Raspberry Pi OS upgrades. **There is no backport** (user, 2026-10-06): no bundled SDL3 for older releases and no SDL2 branch kept alive.
- **SDL3 as its own sub-project,** after 3b ships and before the 3c plan. 3b finishes on SDL2; 3c is planned and built on SDL3.
- **Vulkan first everywhere,** as a renderer preference list: Windows `"vulkan,direct3d12,direct3d11,opengl"`, Linux and Pi `"vulkan,opengl"`.
- **Five additions in the port** (user, 2026-10-10): fallback fonts, native Wayland, display scaling, gamepad battery, and startup timing with a measurement gate. The port is no longer a straight port.

## Decisions

| Question | Decision |
|---|---|
| Scope | **The port, the Vulkan preference list and device-loss recovery, plus five additions:** fallback fonts (§ 4), native Wayland (§ 5), display scaling (§ 6), a gamepad battery glyph (§ 7), and startup timing with a measurement gate (§ 8). Everything else behaves as on SDL2. Other new SDL3 features stay with the sub-projects that own them: real per-pixel transparency (`SDL_WINDOW_TRANSPARENT`) with sub-project 4, pad types and button labels with item 26, and the text-input rework with 3c. |
| Device loss | **Recovering a lost GPU device.** On `SDL_EVENT_RENDER_DEVICE_RESET`, run the full reload that 3b's Task 4 built for a config change. On `SDL_EVENT_RENDER_DEVICE_LOST`, create a new renderer first (after a launched app ends, and with retries scheduled in the main loop), then run the same reload (§ 3). The new renderers bring this failure with them (a driver update, a GPU reset), so it counts as part of the port. |
| How to port | **Directly, on one branch, subsystem by subsystem.** SDL's migration tools (`build-scripts/rename_symbols.py`, `rename_headers.py`, `rename_macros.py` and `SDL_migration.cocci`, per SDL's `docs/README-migration.md`) do the mechanical renames; each subsystem is then ported by hand with its own tests. No compatibility header (SDL2 idioms would stay forever) and no sdl2-compat stepping stone (it reaches neither the preference list nor the new APIs). |
| Release | **v0.5.0, alone.** Its notes open with the changed system requirements. 3c becomes v0.6.0. A port regression shows before 3c's features can hide it, and the hands-on check tests one sub-project. |
| Platforms | **Debian trixie, Raspberry Pi OS trixie, Ubuntu 25.10, Arch, and Windows,** or newer. The `.deb` is built on trixie and installs on newer Debian and Ubuntu releases. **v0.4.1 is the last release for Debian bookworm and Ubuntu 24.04 LTS**, neither of which packages SDL3; it stays downloadable. |
| Harness | **Every check on Vulkan** (Mesa's lavapipe), plus a small subset that proves the automatic fallback to OpenGL, and a Weston leg that runs both branches of SDL's Wayland rule (§ 5). |
| Choosing another renderer | **The standard `SDL_RENDER_DRIVER` environment variable,** honored over the preference list. No config key and no settings row. |
| Fallback fonts | **Text draws characters the chosen font lacks from fallback fonts** (`TTF_AddFallbackFont`), found by fontconfig on Linux and from a fixed list of Windows' own fonts on Windows, with the bundled DejaVu Sans last. Titles and the settings screen, not the clock. Linux draws emoji in monochrome, Windows in color. HarfBuzz is on for every build, Windows included, and Linux links fontconfig. SDL3_ttf's text engine is not used. No config key and no settings row. |
| Video driver on Linux | **SDL's own choice, with no hint.** SDL 3.2.10 prefers Wayland only when the compositor offers `wp_fifo_manager_v1`, and otherwise uses X11. On every target desktop that means native Wayland, except Debian trixie's Plasma 6.3, which stays on XWayland. The standard `SDL_VIDEO_DRIVER` environment variable (or SDL2's `SDL_VIDEODRIVER`) overrides it. No config key and no settings row. The Ubuntu 26.04 hands-on gates it (§ 5). |
| Display scaling | **Sharp at native pixels; the OS scale is logged, not applied.** The window asks for native pixels (`SDL_WINDOW_HIGH_PIXEL_DENSITY`), the screen size comes from the window's pixel size, the Windows manifest declares per-monitor v2, and a change of pixel size runs the full reload. The layout is already sized as fractions of the screen, and pixel settings stay pixels. |
| Gamepad battery | **A low-battery glyph, only while a pad runs on its battery at or below 40 %.** One glyph in the top corner opposite the clock, for the lowest pad on battery, and nothing otherwise. No config key and no settings row. SDL's default enhanced reports for DualShock 4 and DualSense pads stay on. |
| Startup timing | **Measured first.** `Timing:` log lines split read, decode and texture time. Before the rest of the plan is written, a throwaway timing patch on master (SDL2) measures on the user's Windows machine and the Ubuntu qa-harness guest; parallel icon decode on SDL threads is built only if the measurement trips a gate whose thresholds the user ratifies (§ 8). `SDL_AsyncIO` is not used. |

## 1. What the port must get right

These are the traps that compile cleanly and then misbehave; the renames are not listed.

1. **Return values flip.** SDL3 functions that returned a negative error code now return `bool`, true on success (`README-migration.md`). Every `if (SDL_X(...) < 0)` and `!= 0` test inverts silently. The plan walks every call site in the port's diff; a test or a mutant proves each guarded failure branch still fires.
2. **A saved key code keeps its meaning.** `config.ini` stores hotkeys as hex SDL key codes (`#4000003A` is F1), and Windows `:exit` registration depends on the F1-F11 and F13-F24 codes (`keycode_convert.h`). SDL3 makes `SDL_Keycode` a `Uint32` with `SDLK_*` as defines, keeps the scancode mask, and changes how layouts and modifiers shape a key event's keycode (`SDL_HINT_KEYCODE_OPTIONS`). StreamFlex must read the same key from the same code as on SDL2. A unit test pins every bindable code; if a value moved, a translation table keeps old configs meaning what they meant, and the writer keeps writing codes that SDL2-era files understand.
3. **Pixel positions.** SDL3 draws with float rectangles (`SDL_RenderTexture` takes `SDL_FRect`). The layout stays integer; rectangles convert at the draw call only, so nothing lands half a pixel off. The harness's exact-pixel checks are the proof.
4. **Windows Transparent mode under the new renderers.** Transparent is a chroma key, `SetLayeredWindowAttributes(..., LWA_COLORKEY)` (`platform/win32.c:116-119`), proven only with D3D9. Vulkan and D3D12 swapchains on a layered window may ignore it. This is the largest risk, and a spike gates the rest of the plan (§ 3).
5. **Text, images and the window handle.**
   - SDL3_ttf's render calls take UTF-8 with explicit lengths and float point sizes.
   - `TTF_OpenFontIndex` is gone: the face number is the property `TTF_PROP_FONT_CREATE_FACE_NUMBER` to `TTF_OpenFontWithProperties`.
   - `TTF_GlyphIsProvided` becomes `TTF_FontHasGlyph`, which follows a font's fallbacks (§ 4). The font picker's glyph checks therefore stay on fonts with none.
   - `SDL_RWops` becomes `SDL_IOStream`.
   - `SDL_SysWMinfo` is gone. The HWND comes from the window property `SDL_PROP_WINDOW_WIN32_HWND_POINTER` (today `launcher.c:206`, `platform/win32.c:26`).
6. **Text input is off by default in SDL3** ("no longer automatically enabled when initializing video"). That suits 3c's rule that no OS keyboard may appear. The port keeps it off and calls `SDL_StartTextInput` nowhere; 3c owns the rest.
7. **Gamepads.** `SDL_GameController` becomes `SDL_Gamepad`, whose buttons are named by position (south, east, west, north). StreamFlex's own names in `config.ini` (`ButtonA`, `LTrigger` and the rest) and their meaning do not change; the mapping between them and SDL's buttons lives in the code. The `ControllerMappingsFile` format does not change (`SDL_AddGamepadMappingsFromFile`).
8. **Hints that left.** `SDL_HINT_RENDER_SCALE_QUALITY` (`launcher.c:235`) is removed. SDL3 creates textures with `SDL_SCALEMODE_LINEAR` by default; the port sets it explicitly on each texture anyway, so scaled icons stay as today whatever the default becomes. Every other hint StreamFlex sets (`launcher.c:233-236`) is checked against SDL3's list and its replacement named.
9. **The screen size is not the desktop mode.** Display id 0 is invalid in SDL3, so today's `SDL_GetDesktopDisplayMode(0, ...)` (`launcher.c:252-254`) fails, and on Wayland the desktop mode is in scaled units: a 3840x2160 output at 150 % reports 2560x1440. Ported as is, StreamFlex would lay that screen out in its top-left two thirds. The geometry comes from the window's pixel size instead (§ 6).

## 2. Build, packaging and CI

**Version floors** come from Debian trixie, the oldest supported platform: **SDL3 3.2.10, SDL3_image 3.2.4, SDL3_ttf 3.2.2.** They replace `MIN_SDL_VERSION "2.0.18"`, `MIN_SDL_IMAGE_VERSION "2.0.5"` and `MIN_SDL_TTF_VERSION "2.0.15"` (`CMakeLists.txt:34-36`). Ubuntu 25.10 carries 3.2.20 / 3.2.4 / 3.2.2 and Ubuntu 26.04 LTS 3.4.2 / 3.4.0 / 3.2.2, both above the floors; Ubuntu 24.04 LTS carries none.

**Finding the libraries.** SDL3 ships CMake config files on every platform, Debian's `-dev` packages included. Linux's pkg-config branch (`CMakeLists.txt:50-52`) and Windows' `find_package` branch (`:58-60`) become one: `find_package(SDL3 3.2.10 CONFIG REQUIRED)` and the same for `SDL3_image` and `SDL3_ttf`. On Linux, StreamFlex also links fontconfig, `find_package(Fontconfig REQUIRED)`, for fallback fonts (§ 4). inih and getopt stay as they are.

**CI builds** (`.github/workflows/build.yml`):

| Build | Today | After the port |
|---|---|---|
| Debian amd64 | `debian:bookworm`, `libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev` | `debian:trixie`, `libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev libfontconfig-dev` |
| Raspberry Pi arm64 | `debian:bookworm` on `ubuntu-24.04-arm`, `-DRPI=1` | `debian:trixie`, same packages, `-DRPI=1` |
| Arch | `sdl2 sdl2_image sdl2_ttf` | `sdl3 sdl3_image sdl3_ttf fontconfig` |
| Windows (vcpkg, `x64-windows-static`) | `sdl2`, `sdl2-image[libjpeg-turbo,libwebp]`, `sdl2-ttf` | `sdl3[vulkan]`, `sdl3-image[jpeg,png,webp]`, `sdl3-ttf[harfbuzz]` with the baseline patch in an overlay port |

- **`vulkan` is not a default feature of vcpkg's `sdl3` port.** Without it the Windows build silently has no Vulkan renderer and the list starts at D3D12. A unit test catches that (§ 9).
- SDL3_image in vcpkg enables no image format by default, so all three are named.
- **HarfBuzz is on for every build, Windows included** (`sdl3-ttf[harfbuzz]`). Every Linux distribution already builds SDL3_ttf with it. On Windows it makes Arabic, Hebrew, Indic scripts, emoji sequences and kerning shape as on Linux; without it, fallback Arabic draws as isolated letters in the wrong order. It is a C++ library linked into a C program, so the first CI run proves MSVC picks up its runtime. Its cost in the static zip is unmeasured; the plan measures it and the release notes state it. A unit test catches a build without it (§ 9).
- **An overlay port carries SDL3_ttf's baseline fix, on Windows only.** At 3.2.2 each fallback glyph sits on its own font's ascent (upstream #629): `pos->y = y + F26Dot6(pos->font->ascent) - pos->y_offset;` in `src/SDL_ttf.c`. The one-line change to `F26Dot6(font->ascent)` puts every glyph on the primary font's baseline. Measured at 48 pt over OpenSans, it brings Segoe UI Emoji down 17 px and Yu Gothic 9 px, and keeps the text at the font's height. The patch rides in a vcpkg overlay port, is sent upstream, and is dropped when an SDL3_ttf release carries it. **Linux uses the distribution's unpatched SDL3_ttf,** so on Linux a fallback with a different ascent sits slightly off the baseline (Noto Sans CJK measured 4 px low at 48 pt) and can make a text a few pixels taller than the font's height.
- `VCPKG_COMMITTISH` moves to a commit carrying the SDL3 ports (vcpkg had `sdl3` 3.4.18, `sdl3-image` 3.4.8 and `sdl3-ttf` 3.2.2 on 2026-10-06).
- **Both ends of the range are built.** Debian and the Pi build against the floor (3.2.x); Arch and Windows build against the newest (3.4.x). Code that only compiles on 3.4 fails CI.

**Packages.**
- The `.deb` depends on `libsdl3-0`, `libsdl3-image-0` and `libsdl3-ttf-0`, and gains `libfontconfig1` through `shlibdeps`. The note at `CMakeLists.txt:194-196`, which explains where SDL2's version floor came from (the `SDL_RenderSetVSync` symbol), is rewritten for the symbol that sets SDL3's.
- The Arch package depends on `sdl3`, `sdl3_image`, `sdl3_ttf` and `fontconfig`.
- The Windows zip stays static, with no DLLs beside the exe.
- Nothing is bundled on Linux. nanosvg stays vendored, even though SDL3_image can load SVG.

**Harness images.**
- `tests/headless/Dockerfile` moves from bookworm to trixie and adds `mesa-vulkan-drivers` (lavapipe), `libfontconfig-dev`, `fonts-noto-cjk`, `fonts-noto-color-emoji` and `weston` (14.0.2, without fifo-v1).
- `Dockerfile.fedora` drops sdl2-compat for native SDL3 and adds lavapipe, `fontconfig-devel`, `google-noto-sans-cjk-vf-fonts`, `google-noto-color-emoji-fonts` and `weston` (15.0.1, with fifo-v1).
- `make_fonts.py` gains a `box` mode: a font whose glyph for each listed character is a filled square, for the fallback checks (§ 9).

**Documents.** The README's requirements, the docs site's download and install pages, and the v0.5.0 CHANGELOG all state *Debian trixie, Raspberry Pi OS trixie, Ubuntu 25.10, or newer*. The v0.5.0 notes say v0.4.1 is the last release for Debian bookworm and Ubuntu 24.04 LTS, and state the known limits:
- Linux draws emoji in monochrome.
- On Linux a fallback glyph can sit slightly off the baseline.
- A title mixing left-to-right and right-to-left text is not reordered.
- A gamepad battery shows only where the pad and the platform report one; on Linux, only through SDL's HIDAPI (§ 7).
- The Windows zip's size change from HarfBuzz.

## 3. The renderer

**Creation.** `SDL_CreateRenderer(window, list)`, with the list `"vulkan,direct3d12,direct3d11,opengl"` on Windows and `"vulkan,opengl"` on Linux and the Pi. SDL takes the first that works. There is no `software` at the end, which matches today's accelerated-only request; if nothing works, the existing error path reports it and exits as today.

**Override.** When `SDL_RENDER_DRIVER` is set in the environment, StreamFlex passes no list and SDL uses the variable. The harness's fallback checks rely on it, and it is the answer to *Vulkan misbehaves on my GPU*.

**Logging.** The log states the renderer chosen, from `SDL_GetRendererName`: `Renderer: vulkan`. When that is not the list's first entry, it says so: `Renderer: opengl (vulkan was unavailable)`. When the variable chose it: `Renderer: opengl (from SDL_RENDER_DRIVER)`. The harness reads these lines. `debug.c:339`'s renderer dump moves from `SDL_GetRendererInfo` to SDL3's renderer properties (name, maximum texture size, VSync).

**VSync.** The live VSync setting (`launcher.c:270-320`) moves to `SDL_SetRenderVSync` and `SDL_GetRenderVSync`. The rule that grays the VSync row with its reason when the renderer cannot do VSync stays, re-based on SDL3's answer, and is checked per renderer.

**Device loss.** SDL 3.2.10 has three render events (`include/SDL3/SDL_events.h`, since 3.2.0). StreamFlex handles none of them today.
- **`SDL_EVENT_RENDER_TARGETS_RESET`** ("The render targets have been reset and their contents need to be updated") and **`SDL_EVENT_RENDER_DEVICE_RESET`** ("The device has been reset and all textures need to be recreated"): StreamFlex runs the full reload a config change runs. Every texture is rebuilt, and the current menu and screen state are kept. The log says `Renderer: device lost; reloading`.
- **`SDL_EVENT_RENDER_DEVICE_LOST`** ("The device has been lost and can't be recovered."): at 3.2.10 the Vulkan, Direct3D 12 and Direct3D 11 renderers try to rebuild their device after a loss, and send `DEVICE_RESET` when that works and `DEVICE_LOST` when it does not (`recovered ? SDL_EVENT_RENDER_DEVICE_RESET : SDL_EVENT_RENDER_DEVICE_LOST` in `src/render/vulkan/SDL_render_vulkan.c:2528`, `direct3d12/SDL_render_d3d12.c:1475` and `direct3d11/SDL_render_d3d11.c:1100`). The OpenGL renderer sends neither. SDL has already tried once by the time `DEVICE_LOST` arrives, and the renderer is unusable. StreamFlex drops every texture, destroys the renderer, logs `Renderer: device lost and not recovered; creating a new renderer`, and then:
  - **While an app is launching or running,** it does nothing more yet. A GPU reset is often caused by the game StreamFlex launched, a second try under it would likely fail the same way, and nothing is drawn while it runs. The new renderer is created in `post_launch()`, as a deferred size change is (§ 6).
  - **Otherwise it creates the new renderer with retries:** up to 5 attempts, 2 s apart (about 8 s in all), each logging `Renderer: creating a new renderer, attempt 2 of 5`. The attempts are scheduled inside the main loop, not a blocking wait: events keep being pumped between them, so Windows never marks the window "Not responding" and the quit paths stay live, and nothing is drawn until a renderer exists. Only when all five fail does the startup error path report it and exit.
  - **The new renderer is made as at startup,** by the renderer half of `create_window()`: the list (or `SDL_RENDER_DRIVER`), so a GPU that no longer offers the first choice falls down the list; `SDL_CreateRenderer`; the blend mode; the draw color; the VSync setting and the VSync check that grays the row per renderer; and the `Video:` and `Renderer:` log lines. Then the HWND is read again from `SDL_PROP_WINDOW_WIN32_HWND_POINTER`.
  - **OpenGL replaces the native window.** At 3.2.10 the OpenGL renderer calls `SDL_RecreateWindow` when the window lacks `SDL_WINDOW_OPENGL` (`src/render/opengl/SDL_render_gl.c:1637-1645`), and StreamFlex's window is created with no graphics flag; the Vulkan, Direct3D 12 and Direct3D 11 renderers never recreate it. Everything in `platform/win32.c` uses the HWND read once at startup (the layered style and color key, the foreground calls, `RegisterHotKey`), so when the HWND has changed, StreamFlex re-applies Transparent and re-registers every hotkey it had registered. At startup the problem does not arise, because the HWND is read after the renderer exists.
  - Then the full reload runs.
- **One reload, geometry first.** A target reset, a device reset or loss, and a change of the window's pixel size (§ 6) can arrive in one batch of events. Each handler only marks a reload pending, with its reason; one reload runs after the events are drained, before drawing. The reload recomputes the screen geometry first, calling `SDL_SyncWindow` and then `SDL_GetWindowSizeInPixels` (§ 6), then rebuilds the textures in the order Discard uses (`REFRESH_ORDER`). 3b's config-change reload never had to recompute geometry, because the screen size never changed; the plan checks this explicitly. While an app is launching or running, a target or device reset still rebuilds the textures, but when a size change is pending it keeps `geo` as it is, so a game's temporary resolution never becomes the layout: the geometry is recomputed once, in `post_launch()`.

**The Transparent spike (a gate before the rest of the plan is written).** The plan has two gates, both run on the user's Windows machine before the remaining plan tasks are written: this spike, and the timing measurement (§ 8), which also runs on the Ubuntu qa-harness guest.
- **Where:** the user's Windows machine and its real GPU. The qa-harness Windows guest has no GPU and would test only the D3D fallbacks.
- **What:** with `Background=Transparent`, confirm the desktop shows through and the key color is cut out, on each of `vulkan`, `direct3d12`, `direct3d11` and `opengl`, forcing each with `SDL_RENDER_DRIVER`; then repeat with Transparent switched on live from the settings screen. The table records the display scale it ran at.
- **The spike's build is throwaway:** the smallest SDL3 build that opens the window, draws the background and applies the chroma key.
- **If all four work,** nothing changes. **If any fails,** the measured table goes to the user with the options before the rest of the plan is written. Changing renderer on the fly is real work and a behavior change, so it is the user's decision, not a ruling.

## 4. Fallback fonts

A title, a menu name or a settings value in a script the chosen font lacks draws in a font that has it, instead of as boxes: CJK, Hangul, Indic, Thai, Arabic, Hebrew, symbols and emoji. Today the bundled OpenSans covers Latin, Greek and Cyrillic, and anything else draws as its hollow `.notdef` box. `TTF_AddFallbackFont` is in SDL3_ttf 3.2.2, which is both the floor and the newest release, so nothing needs a run-time check.

**Font sets.** Each font StreamFlex opens for titles or the settings screen becomes a set: the font itself and the fallbacks attached to it, at its size, all owned by the set (a new `src/fontset.c`). Before a text is first measured, each character the font itself lacks gets its *provider*, the first usable candidate that has it, attached to the set. A config whose text the font covers builds no candidate list, opens no fallback and pays nothing. SDL3_ttf measures and wraps through fallbacks (`TTF_GetStringSize`, `TTF_MeasureString` and the `_Wrapped` calls), so Shrink, truncation and the settings rows' widths stay right without new layout code.

**Candidates.**
- **Linux:** fontconfig's sort for `sans-serif:style=Regular` (`FcFontSort`, which adds the user's language). It follows the desktop's own choices, including fonts the user installed. It carries each font's character set, so no font is opened to ask what it covers. It reports a variable font's Regular named instance, so Fedora's Noto Sans CJK VF, whose default instance is Thin, draws at regular weight: fontconfig's index is `(instance << 16) | face`, which StreamFlex passes as `TTF_PROP_FONT_CREATE_FACE_NUMBER` to `TTF_OpenFontWithProperties`, and SDL3_ttf 3.2.2 hands it to `FT_Open_Face` unchanged. Measured: 0.4-2.7 ms per sort with a warm cache.
- **Windows:** a fixed list of Windows' own families, found by their `Fonts` registry value names (the keys `fontscan.c` already reads), never by file name, because file names drift between builds:
  1. Segoe UI;
  2. Segoe UI Emoji, before Symbol, so emoji draw in color;
  3. Segoe UI Symbol;
  4. Yu Gothic, Microsoft YaHei, Microsoft JhengHei and Malgun Gothic, with the user's preferred locale (`SDL_GetPreferredLocales`) choosing which comes first: `ja` Yu Gothic, `ko` Malgun Gothic, `zh` with TW, HK or MO Microsoft JhengHei, anything else Microsoft YaHei. YaHei stays in the list behind Yu Gothic, which lacks simplified Han;
  5. Nirmala UI, Leelawadee UI, Ebrima, Gadugi, Myanmar Text, Javanese Text, Microsoft Himalaya, Mongolian Baiti, Sylfaen and Segoe UI Historic.

  StreamFlex learns each Windows candidate's coverage by opening it once, at 12 pt with no fallbacks, when the walk first reaches it.
- **Both:** the bundled `DejaVuSans.ttf` comes last. It covers Greek, Cyrillic, Hebrew, Arabic, symbols and some emoji, in monochrome. On both platforms the primary's own file also gets a 12 pt probe with no fallbacks, because `TTF_FontHasGlyph` on the primary follows its fallbacks (`get_char_index_fallback` in SDL3_ttf) and cannot say what the primary itself lacks.

**Rejected candidates.** A candidate is skipped for the session, with a log line, if it cannot be scaled (`TTF_FontIsScalable`) or draws no ink for the character it was picked for (`TTF_RenderGlyph_Blended`). At 3.2.2 that rules out Noto Color Emoji, a CBDT bitmap font that draws 128 px tall at any size, and Fedora's Noto COLRv1 emoji, which draw nothing. So **Linux draws emoji in monochrome, from DejaVu Sans, and Windows in color, from Segoe UI Emoji.** Default-ignorable code points (ZWJ, ZWNJ, variation selectors, soft hyphen and the like) are never searched for. A character no font has still draws as the primary's box, and the log says so.

**Order is kept.** A set's fallbacks stay in candidate order. A provider that ranks after every attached fallback is appended; one that ranks earlier rebuilds the chain (`TTF_ClearFallbackFonts`, then each in order). Without this, a ★ that attached Segoe UI Symbol first would make a later emoji draw in monochrome, because `TTF_FontHasGlyph` already answers yes. A set remembers the code points it has resolved, so the settings screen, which measures its rows every frame, pays one lookup per character.

**Closing order (upstream #549).** At 3.2.2, adding or removing a fallback does not flush the font's cache of glyph positions, which point into the fallback. Closing a fallback before the font that used it, then measuring again, reads freed memory: upstream #549, reported at SDL 3.2.10 and SDL3_ttf 3.2.2, and fixed only after 3.2.2. A set therefore closes its font first (that close flushes and clears its fallbacks), then the fallbacks, and never closes a fallback while its font lives. No fallback is shared between sets or made with `TTF_CopyFont`, which copies no fallbacks and shares one file stream between copies.

**Where it plugs in.**
- **Titles:** `title_font()`'s cache holds sets instead of fonts, and `fixed_title_font` becomes a set. `render_text()` prepares the set before its first measure.
- **Shrink** stops opening a font per step. Each text keeps one scratch set and resizes it, fallbacks first (`TTF_SetFontSize` resizes only the font it is given), so every size it measures carries the same fallbacks as the size it draws.
- **The line-height probe** stays primary-only: its height is the primary's.
- **Settings:** the three settings fonts become sets, prepared in the helpers that measure and draw text.
- **Untouched:** the font picker's samples keep fresh fonts with no fallbacks, so their glyph checks test the face itself. The clock keeps a single font, because its strings are ASCII (StreamFlex never calls `setlocale`) and it draws on its own thread.
- **Closing:** every close (`title_fonts_free`, `title_fonts_keep`, `reload_title_font`, `cleanup`) closes a set font first. `fontset_quit()` runs before `TTF_Quit` and calls `FcFini()`, so the LeakSanitizer leg sees no fontconfig cache.

**Cost.** Each fallback is opened per size, and only when a text needs it. Measured per face: Segoe UI Emoji 8.1 MB and 45-52 ms to open, Noto Sans CJK 2.4 MB, Microsoft YaHei 0.8 MB. A menu with emoji titles at three sizes costs about 25 MB on Windows. The 16-size title cache stays.

**Device loss** does not touch fonts: they are FreeType state, and the reload re-renders the title textures from the same sets.

**Known limits at 3.2.2,** stated in the release notes:
- On Linux, a fallback with a different ascent sits a little high or low (§ 2). The Windows overlay patch fixes it there.
- SDL_ttf has no bidirectional algorithm, so a title mixing left-to-right and right-to-left text is not reordered, and it is shaped wrong when the primary font itself covers the right-to-left script.

**Log lines** (debug unless noted):
- `Fonts: the 36 pt titles fall back to /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc (face 0), first for U+6E38`, once per set and fallback. The role reads `titles`, `settings rows`, `settings headers` or `settings notes`; a named instance reads `(face 0, named instance 4)`.
- `Fonts: skipped /usr/share/fonts/truetype/noto/NotoColorEmoji.ttf: a bitmap font cannot be scaled to the text` and `Fonts: skipped /usr/share/fonts/google-noto-color-emoji-fonts/Noto-COLRv1.ttf: it draws nothing for U+1F600`, once per file.
- `Fonts: no installed font draws U+1F579; it shows as a box`, **not debug**, once per code point per session, because the user can act on it.
- `Fonts: 156 fallback candidates from fontconfig` or `Fonts: 15 fallback candidates in C:\Windows\Fonts`, once, when the list is built.

**No config key and no settings row.** The candidates are the system's own. A user who wants one particular font for every title already has `Font=`.

**SDL3_ttf's text engine is not used.** `TTF_CreateRendererTextEngine` would draw each title glyph by glyph every frame instead of copying one texture; its texts must live on the renderer's thread, which the clock's thread is not; its atlases are textures a device reset loses, and it handles no render event; and its 3.2.2 defects are fixed only after 3.2.2. 3c's text editing may want it, and 3c's research looks again at the SDL_ttf release current then.

## 5. Native Wayland

**The driver: SDL's own choice, with no hint.** `init_sdl()` sets no `SDL_HINT_VIDEO_DRIVER`. At 3.2.10 SDL's bootstrap list (`src/video/SDL_video.c:79`) tries, in order:
1. Wayland, if the compositor advertises `wp_fifo_manager_v1` (`Wayland_preferred_bootstrap`, `src/video/wayland/SDL_waylandvideo.c`), "so we don't regress GPU-bound performance and frame-pacing by default due to swapchain starvation";
2. otherwise X11, meaning XWayland or an X11 session;
3. otherwise Wayland without fifo-v1: a Wayland session with no X server.

3.4.x keeps the rule. Fifo-v1 arrived in Mutter 48 and KWin 6.4, both checked in their source. Per target desktop:

| Desktop | Compositor | fifo-v1 | v0.4 (SDL2) | v0.5.0 (SDL3) |
|---|---|---|---|---|
| Debian trixie GNOME | Mutter 48.7 | yes | x11 | **wayland** |
| Debian trixie Plasma | KWin 6.3.6 | no | x11 | x11 (XWayland) |
| Ubuntu 25.10 GNOME | Mutter 49.0 | yes | x11 | **wayland** |
| Kubuntu 25.10 | KWin 6.4.5 | yes | x11 | **wayland** |
| Ubuntu / Kubuntu 26.04 | Mutter 50.1 / KWin 6.6 | yes | x11 | **wayland** |
| Fedora 44 Workstation / KDE | Mutter 50.5 / KWin 6.6.4 | yes | wayland (sdl2-compat) | **wayland** |
| Any X11 session | none | n/a | x11 | x11 |

Classic SDL2 tries X11 first, so Ubuntu and Debian run StreamFlex on XWayland today. Fedora's SDL2 is sdl2-compat over SDL3, which already runs it on native Wayland. The environment variable `SDL_VIDEO_DRIVER` (or SDL2's `SDL_VIDEODRIVER`, which SDL3 still reads) outranks any hint and stays the user's override, as `SDL_RENDER_DRIVER` is for the renderer.

`init_sdl()` also calls `SDL_SetAppMetadata(PROJECT_NAME, PROJECT_VERSION, "streamflex")` before `SDL_Init`, so the Wayland `app_id` (and the X11 `WM_CLASS`) matches `streamflex.desktop` however the program was started. Today it matches only because SDL falls back to the executable's name.

**The gate.** The Ubuntu 26.04 hands-on runs smoke module 13, the launch rows and the restart rows on the default driver (§ 9). If a Wayland-only failure shows, the measured table goes to the user, and the fallback is one line on Linux, `SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11,wayland")`, at normal priority so the environment still wins. Taking it is the user's decision at that point, not a ruling.

**Focus needs no new code.** Read in Mutter's and KWin's source: both give a new window the keyboard when it maps (Mutter unless the non-default `focus-new-windows=strict` is set, KWin unless focus-stealing prevention is Extreme), and give it back to the window below when it closes. So the focus-lost and focus-gained logic that detects a launched app's start and end works as on X11.
- A launched app needs no xdg-activation token, and StreamFlex hands it none: SDL 3.2.10 has no public way to mint one for another process, and a pad press carries no Wayland serial to mint it with.
- A restart's fresh window takes focus at map. `raise_restarted_window()` stays, log line included, and is harmless (SDL 3.4 raises on every show anyway).
- StreamFlex adds no `SDL_RaiseWindow` call on an app's exit: on GNOME a raise from a covered window becomes a "StreamFlex is ready" notice, not a raise.

**Other behavior.**
- Fullscreen goes to `xdg_toplevel_set_fullscreen`, and the compositor puts the window above its panels.
- The screensaver inhibit tries D-Bus first on both drivers, so on GNOME and Plasma it does not depend on the driver.
- On GNOME, SDL loads libdecor, because Mutter has no xdg-decoration; a fullscreen window draws no frame. SDL's default stays, the GNOME hands-on records whether `libdecor` is mapped, and the hint is set only if that shows a cost or a bug.
- Scaled outputs: § 6.
- Transparent on Linux is a picom shader for X11 compositors and never worked in a Wayland session, so nothing changes.
- **Unverified:** whether Vulkan FIFO presentation blocks `SDL_RenderPresent` while a launched app covers StreamFlex. It would show as a late `Application finished`. The Wayland leg's 2 s bound and a smoke row (an app left in front for two minutes) measure it, and a delay goes to the user before any fix is designed.

**Log lines.**
- The startup line keeps its text: `Video: SDL's wayland driver, the vulkan renderer`.
- When the driver list came from the environment: `Video: the driver list comes from SDL_VIDEO_DRIVER=x11`, naming whichever variable is set (the new name is checked first, as SDL does).
- When SDL chose x11 while `WAYLAND_DISPLAY` is set and nothing overrode it: `Video: x11, though a Wayland compositor is running: it lacks fifo-v1, which SDL needs before it prefers Wayland`.

The note is a pure function of the driver name and the three environment values, and is unit-tested.

## 6. Screen size and display scaling

This one section covers the geometry change that both native Wayland and display scaling need.

**One source: the window's pixel size.** `init_sdl()` stops reading the desktop mode for the screen size. Once `create_window()` has made the window, it calls `SDL_SyncWindow` and sets `geo.screen_width` and `geo.screen_height` from `SDL_GetWindowSizeInPixels`, and from them `geo.screen_margin`, `geo.title_min_size` and `title_info.min_size`, with today's arithmetic and rounding (a pure function, unit-tested).
- **Why the window and not the renderer.** The window's pixel size exists as soon as the window does, so it does not depend on a renderer that device loss may just have replaced (§ 3). It is also the size `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` reports, and the size every renderer in the list draws to: `SDL_GetRenderOutputSize` answers from the renderer's own output, or from the same window call when a renderer has none (`src/render/SDL_render.c`), and the swapchain follows the window's pixel size.
- **One cross-check.** Once the renderer exists, `SDL_GetRenderOutputSize` is read once, and a difference from the window's size is logged as an error. `SDL_GetCurrentRenderOutputSize` is never used: it follows the render target, which is the settings preview while that draws.
- **Order in `main()`.** Today `init_sdl_ttf()` and `refresh_effective()` run before `create_window()` and read `geo`. The rule is that nothing reads `geo` before it is set from the window; the plan picks between moving `create_window()` up and moving the two steps down after reading master's `main()`.
- **The refresh rate** is all the desktop mode still gives: `SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window))->refresh_rate`, a float in SDL3. The call returns a pointer that can be NULL; NULL or a 0 refresh rate keeps today's 60 Hz fallback. Never display 0.
- **No logical presentation and no render scale** (`SDL_SetRenderLogicalPresentation`, `SDL_SetRenderScale`). One layout pixel is one output pixel, so § 1 item 3's integer rule stays exact: the conversion to `SDL_FRect` is a cast of a whole number, never a multiplication by 1.5.
- **`Resolution:` keeps its exact format,** `Resolution:    %ix%i`, because four harness checks match it with `grep -qx`. It now prints the window's pixel size. Smoke rows 1.25, 13.5 and 13.12 keep their claims.

**The window asks for native pixels.** `create_window()` creates the window fullscreen (`SDL_WINDOW_FULLSCREEN` with no fullscreen mode set, which is SDL3's desktop fullscreen) and with `SDL_WINDOW_HIGH_PIXEL_DENSITY`. The flag changes nothing on Windows or X11. On a scaled Wayland output it gets a buffer at the output's native pixels (through `wp_fractional_scale_v1`, which Mutter and KWin offer), instead of one at the scaled-down size that the compositor blows up blurry. No config key: a blurry launcher is never what a user wants. Some fractional scale and mode pairs round the buffer one pixel off the mode; the layout uses whatever size it gets.

**The Windows manifest says per-monitor v2.** `config/streamflex.manifest.in` changes `<dpiAwareness>` from `permonitor` (per-monitor v1) to `PerMonitorV2`. SDL3 asks for v2 by itself when its hint is unset (`src/video/windows/SDL_windowsvideo.c:480-481`), but a process's awareness is set once and the manifest sets it first, so today's manifest would keep the process at v1. Keeping the manifest, rather than deleting the element, keeps the process aware before `SDL_Init`. That v1 misbehaves under SDL3 is unverified; the change aligns StreamFlex with the mode SDL3 is built for. The log says `DPI awareness: per-monitor v2` (or `per-monitor v1`, `system`, `unaware`).

**A change of pixel size runs the full reload.** On `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`, StreamFlex reads the window's pixel size again and compares it with `geo`:
- **The same size does nothing.** That covers the event SDL sends when the window is created, and a Wayland scale change that keeps the native size.
- **A new size, nothing launched:** the reload of § 3 runs, geometry first, and also re-derives the percentages of the screen (`refresh_effective()`, `calculate_layout_area()`). The log says `Screen: 1280x720 px, was 1920x1080; reloading`.
- **A new size while an app is launching or running:** no reload yet, and the log says `Screen: 1280x720 px, was 1920x1080; reloading when the app is done`. `post_launch()` checks again and reloads only if the size is still new, so a game that changes the resolution and puts it back costs nothing: `Screen: 1920x1080 px again; nothing to reload`. A target or device reset meanwhile keeps `geo` (§ 3).
- **Settings open:** the reload also re-measures the settings screen (a new `settings_reflow()`: its fonts, text cache, preview and layout), keeping the page and its unsaved edits. An open picker or folder browser, measured when it opened, closes as Back closes it.

This covers a resolution change, a TV switching mode, a monitor hot-plug, and a compositor sending its fractional scale after the first configure. On SDL2, StreamFlex never handled a size change at all.

On `SDL_EVENT_WINDOW_DISPLAY_CHANGED`, the refresh rate is read from the new display and the frame timing is worked out again (`apply_frame_timing()`). The log says `Screen: now on display <id>, <rate> Hz`.

**The OS's scale is logged, never applied.** The startup log says `Screen: 3840x2160 px, pixel density 1.00, display scale 1.50` (from `SDL_GetWindowSizeInPixels`, `SDL_GetWindowPixelDensity` and `SDL_GetWindowDisplayScale`). `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` with no change of pixel size logs `Screen: display scale 2.00, was 1.50; nothing to redo`.
- **Why not apply it.** Pixel and point settings keep meaning pixels and points, as `configuration.md` documents. The layout is already sized as fractions of the screen, a TV's OS scale is set for a desk, not a couch, and scaling the settings would make a row that says `30 px` draw 45.
- **X11 at the floor.** At 3.2.10 the X11 driver never delivers a live scale change (`src/video/x11/SDL_x11settings.c` returns early for every setting; 3.4 fixes it), so on Debian trixie's SDL the logged scale is the one at startup.
- **Windows.** Whether Windows reports a live change of the Scale setting is unverified. Nothing depends on it.

Every call in this section exists at the 3.2.10 floor.

## 7. Gamepad battery

SDL3 gives each open gamepad a power state and a percent (`SDL_GetGamepadPowerInfo`), a connection state (`SDL_GetGamepadConnectionState`: wired, wireless or unknown), and a change event (`SDL_EVENT_JOYSTICK_BATTERY_UPDATED`), all since 3.2.0. SDL2 offered five coarse levels, which StreamFlex never read.

**What the user sees.** One small battery glyph, built from filled rectangles, in the top corner opposite the clock's alignment, only while a connected pad runs on its battery at or below 40 %: amber, and red at or below 15 %. It hides again above 45 %, so a pad that reports in steps of ten does not flicker at the edge. Nothing is drawn for a charging, charged or wired pad, an unknown state, or no pad. With several pads it shows the lowest pad on battery. There are no digits: an Xbox pad reports four steps (10, 40, 70, 100), a Switch Pro five and a PlayStation pad ten, so a number would claim more than SDL knows.
- **On battery only.** A wired Xbox pad on XInput or RawInput reports `CHARGING, 100`, and some drivers send `UNKNOWN` with a percent; keying on `SDL_POWERSTATE_ON_BATTERY` alone sidesteps both.
- **Placement.** The clock owns one top corner, and the glyph takes the other, at the clock's margin (or the margin the clock would have, when it is off), so the two line up. It is drawn with the scene, after the clock and under the screensaver's dim layer, and never in the settings preview.
- **No texture,** so nothing to rebuild on device loss and nothing stale after a renderer change. Its fixed red and amber are far from the shipped chroma key, and the draw nudges a color that lands near a user-set key, by `chroma.h`'s rule.
- **No config key and no settings row.** It appears only when something is true and actionable.

**Where in `src/`.**
- A new pure module, `src/padpower.c` (no SDL, like `layout.c` and `chroma.c`), holds StreamFlex's own power and connection enums, kept equal to SDL's by `COMPILE_CHECK`s in `launcher.c`, and two functions: `padpower_level()` (the 40/45/15 rule, `ON_BATTERY` only, a percent of -1 or out of range hidden) and `padpower_pick()` (the lowest pad on battery).
- Each `Gamepad` gains its power state, percent and connection. `open_controller()` reads them once and logs them. A battery event updates the matching pad by instance id, taking `state` and `percent` from the event itself (`event.jbattery`, an `SDL_JoyBatteryEvent`, both fields at 3.2.10), never by asking `SDL_GetGamepadPowerInfo` again; otherwise the test hook's pushed event would prove nothing, since the virtual pad reads unknown.
- Closing the pads for a launch resets them to unknown, so the glyph returns with the first report after `post_launch()`.
- A new `draw_pad_battery()` runs from `draw_scene()` after the clock, never for the preview.

**What reports a battery** (read from SDL's drivers, not from pad hardware):
- **Windows:** Xbox pads through XInput or RawInput (the Xbox Wireless Adapter; a wired pad reads as charging at 100 %), Bluetooth Xbox pads through SDL's HIDAPI driver, and DualSense, DualShock 4 and Switch Pro pads through HIDAPI. Which backend takes a Bluetooth Xbox pad is unverified (SDL's hint text and its code disagree); both give the same four steps.
- **Linux:** the evdev path carries no battery at all. A pad shows one only when SDL's HIDAPI can open its `hidraw` node, which needs udev rules (Steam's, or `game-devices-udev`) that the stock systemd rules do not grant for gamepads. Otherwise StreamFlex shows nothing.
- **SDL's virtual joystick** reports no battery and no connection, at 3.2.10 or later.

**DualShock 4 and DualSense: SDL's default stays.** SDL3 turns on enhanced reports for these pads by default (`SDL_HINT_JOYSTICK_ENHANCED_REPORTS`, default `"1"`; SDL2 left them off), and over Bluetooth only enhanced reports carry a battery. StreamFlex sets no hint. A known effect of that default, per SDL's own hint text: once switched, a PlayStation pad stays in enhanced mode until it is power-cycled, and a launched app that reads it through DirectInput without SDL may misread it until then. This was not observed on hardware; a smoke row records any effect.

**Log lines** (debug):
- On open: `Gamepad power: instance id 3, wireless, on battery, 40%`. The connection reads `unknown`, `wired` or `wireless`; the state `unknown`, `on battery`, `no battery`, `charging`, `charged` or `error`; the percent `40%`, or `no percent` for -1.
- On a battery event that changes what StreamFlex holds: the same line, with `Gamepad power changed:` in place of `Gamepad power:`.
- On the glyph's edge or level: `Battery indicator: shown, low, 40% at 24,24 36x18`, `Battery indicator: shown, critical, 10% at 24,24 36x18` and `Battery indicator: hidden`. The rectangle lets a check sample its pixels.
- `Gamepad opened at device index N, instance id N` (smoke rows 1.26 and 1.27) is unchanged.

**Test hook.** `STREAMFLEX_TEST_PAD_POWER=<file>`, in test-hook builds only, is read once a frame like `STREAMFLEX_TEST_PAD_PLUG`. Its one line is `<state> <percent>` (`battery 40`, `charging 80`, `unknown -1`), with an optional pad letter in front (`B battery 40`) for the second pad. When the contents change, StreamFlex pushes an `SDL_EVENT_JOYSTICK_BATTERY_UPDATED` for its virtual pad with `SDL_PushEvent`. The handler reads the pushed event's own fields, as it does a real one, so this drives the real event handler, the draw and the log, and it proves StreamFlex's handling and the glyph's levels, colors and corner. It does not prove that SDL's drivers read a real pad; only hands-on hardware can.

Item 26 stays separate. It shares only the per-pad fields filled in `open_controller()`, and logs its pad type on its own line.

## 8. Startup timing

Today every icon of the shown menu is read, decoded and made a texture on the main thread before the first frame (`render_buttons`), and the first slideshow image the same, and no log line measures any of it. Measured on a fast desktop with a warm cache, through the same image libraries as a proxy: reading is about 5 % of an icon's cost and decoding most of the rest; the default four-icon menu takes roughly 8 to 10 ms, and a 24-icon menu about 55 ms. Couch hardware is slower by an amount not measured.

**What ships: the timing lines.** `load_icon` and `load_texture_measured` read with `SDL_LoadFile` and decode with `IMG_LoadTyped_IO` from memory, passing the file's extension as the type as `IMG_Load` does (never `IMG_Load_IO`, which passes none), so the parts can be timed apart with `SDL_GetPerformanceCounter`. The `fileio_not_a_file` guard stays in front. Logged with `-d`, in whole milliseconds:
- `Timing: menu 'Main': 4 icons at 256 px in 9 ms (read 1, decode 6, texture 2), 4 titles in 3 ms`, from `render_buttons`, as a new line beside `Menu '%s': rendered its buttons at %i px`, which stays byte-identical (`50-settings.sh` greps it).
- `Timing: background: Slideshow image in 31 ms (read 1, decode 24, luminance 4, texture 2)`, and the same shape for a single image.
- `Timing: startup: first menu shown 94 ms after the window opened`, at `Begin program loop`, counted from `ticks.program_start`.

A missing file now fails in `SDL_LoadFile` rather than `IMG_Load`. Whether its message reads the same is unverified, so smoke row 5.53 and `30-backgrounds.sh`'s check of SDL's own reason are re-run.

**Image formats are primed at startup, whatever the gate decides.** SDL_image sets up each format lazily on its first load, without synchronization (below), and that can race today: the slideshow thread and the settings preview's decode thread call `IMG_Load` beside the main thread's loads. So StreamFlex primes PNG, JPEG and WebP once at startup on the main thread, decoding three tiny embedded images through `SDL_IOFromConstMem`, before any thread that decodes is started.

**The gate.** Like the Transparent spike, the measurement is a gate that runs before the remaining plan tasks are written (§ 3).
- **What runs:** master (SDL2) with a throwaway timing patch that prints the same `Timing:` lines with SDL2 calls (`SDL_LoadFile`, `IMG_LoadTyped_RW` from memory). It is never merged. The decision it informs turns on read and decode cost, and those come from the same libpng, libjpeg-turbo, libwebp and nanosvg under SDL2 and SDL3. Only the texture share depends on the renderer (SDL2's Direct3D 9 and OpenGL against SDL3's Vulkan-first list), so that share is read as indicative only.
- **Where:** the user's Windows machine, with the user's real config and background, and the slowest Linux target, the Ubuntu qa-harness guest, whose emulated CPU is a repeatable worst case. The guest run waits on the qa-harness queue. The lines go into the plan's ledger.
- **The shipped lines** are still written in the port, in SDL3 calls, as above.

Parallel decode is built only if either holds:
1. the default menu's `Timing: menu` line exceeds 250 ms, or `Timing: startup` exceeds 500 ms;
2. any menu open or Back exceeds 50 ms, StreamFlex's own bar for a slow key (`SLOW_KEY_MS`).

These thresholds are proposed; the user ratifies or replaces them before the measurement is read, and any later change is the user's call. If neither trips, nothing more is built.

**If it trips: parallel decode on SDL threads** (a new `src/iconload.c`). `render_buttons` hands its icons to a small pool of SDL threads, `clamp(SDL_GetNumLogicalCPUCores() - 1, 1, 4)` of them, which read, decode and run the chroma pass. The main thread joins them, creates every texture (`SDL_CreateTextureFromSurface` is main-thread only) and renders the titles, so the grid still appears all at once, only sooner. A reload or a menu change waits for a batch in flight, and a stale result is freed. Any parallel decode must handle two hazards:
- **SDL_image 3.2.4 initializes each format lazily** (`IMG_InitPNG` and its JPEG and WebP siblings) behind an unsynchronized counter, so two threads racing a format's first load can see half-written function pointers. The startup priming above already covers it, and a TSan run on the Debian leg confirms it before release.
- **nanosvg's rasterizer is one global** (`image.c`) with scratch buffers inside, shared with the highlight and the scroll arrows. Each worker gets its own.

**Why not `SDL_AsyncIO`.** It is present at 3.2.10, but it overlaps only the read, which is the small part; it opens files synchronously, so a FIFO would still block; and it has no cancel. On a default Debian install it probably runs on SDL's own thread pool, because Debian lists `liburing2` only as a suggestion (per its package page; not checked beyond that). Decode and texture creation are the cost.

## 9. Testing

**Unit tests (CTest)** are ported, and build against both ends of the version range. New:
1. **The key-code table.** Every code a user can bind, keyboard and pad, reads as the same key or button as on SDL2, including the F-key set behind Windows `:exit`.
2. **An old config still means the same.** 3b's bindings fixtures, written for SDL2-era files, load to exactly the same bindings.
3. **The built SDL has Vulkan.** `SDL_GetRenderDriver` lists `vulkan`. It needs no GPU, so it runs on Windows (catching a missing `sdl3[vulkan]`) and on Linux.
4. **Failure branches after the return-value flip.** Each guarded SDL call's failure branch is proven by a test or a mutant.
5. **The built SDL_ttf has HarfBuzz.** `TTF_GetHarfBuzzVersion`'s major is above 0, catching a Windows build without `[harfbuzz]`.
6. **The Windows candidate order follows the locale.** `ja-JP`, `ko-KR`, `zh-TW`, `zh-HK`, `zh-CN` and `en-US` each give the documented first CJK family (pure, built everywhere).
7. **A font set keeps candidate order and closes its font first.** Inserting a provider appends or rebuilds as § 4 says, and the default-ignorable table answers yes for U+200D, U+FE0F and U+E0100 and no for U+0041 and U+1F600 (pure). With real SDL_ttf and the bundled fonts, confined to `DejaVuSans.ttf` through a test seam, OpenSans plus ★ attaches DejaVu Sans and measures wider than with no fallbacks. Closing, reopening and measuring the same string again under ASan is the #549 path, and a mutant that closes the fallbacks first must fail it.
8. **The driver note.** `wayland` gives no note; `x11` with `WAYLAND_DISPLAY` set and no override gives the fifo-v1 note; `x11` without `WAYLAND_DISPLAY` gives none; `SDL_VIDEO_DRIVER` or `SDL_VIDEODRIVER` gives the override note naming that variable.
9. **The screen geometry from a pixel size.** The margin and the title minimum come out exactly as today: 54 and 22 for 1920x1080, 108 and 43 for 3840x2160, 36 and 14 for 1280x720.
10. **The reload decision.** The same size never reloads, a new size reloads, a new size during an app waits, and a size that came back reloads nothing.
11. **The manifest asks for per-monitor v2,** in the source on every platform and, on Windows, in the built exe (`mt.exe -inputresource:`).
12. **The battery table** (`test_padpower`). `padpower_level` and `padpower_pick` give the glyph for every state and percent SDL's drivers send, including the ones that look like a level and are not (`UNKNOWN` with a percent, a wired Xbox pad's `CHARGING, 100`), and the 40/45 hysteresis.

If parallel decode is built, its job table gets its own tests: one job claimed at a time, a stale result discarded, a wait that returns only when every job has posted.

**Headless harness** (Debian trixie and Fedora 44, both with lavapipe):
- **Every existing check runs on Vulkan,** and the run asserts `Renderer: vulkan`. The Xvfb runs also assert `Video: SDL's x11 driver` and `Screen: 1920x1080 px, pixel density 1.00, display scale 1.00`, and that no startup logs a `Screen: ... reloading` line. For that, the harness drops its forced `xdotool ... windowsize 1920 1080` (`run.sh`, `look()`, `58-settings-pickers.sh`), or limits it to 1920x1080 displays: SDL3 creates a fullscreen window at the display's bounds (`src/video/SDL_video.c:2473-2483`), and forcing 1920x1080 onto a display of another size (the 3840x480 picker run, `run_keys_at`) would fire the size-change reload.
- **Automatic fallback.** A subset hides the Vulkan driver (`VK_DRIVER_FILES` pointed at nothing) so that SDL falls back on its own. It asserts `Renderer: opengl (vulkan was unavailable)`, then runs the drawing checks for the background, titles, the settings screen and the pickers.
- **Override.** One check sets `SDL_RENDER_DRIVER=opengl` and asserts it beat the list.
- **Device loss.** A test hook posts `SDL_EVENT_RENDER_DEVICE_RESET`; the check asserts the reload ran and the screen redraws pixel-correct. A second check posts `SDL_EVENT_RENDER_DEVICE_LOST` and asserts `Renderer: device lost and not recovered; creating a new renderer`, a fresh `Renderer: vulkan` line and the same redraw. A third makes the recreation skip Vulkan through a test hook, and asserts `Renderer: opengl (vulkan was unavailable)`, an unchanged `Screen:` line and a pixel-correct redraw. A fourth posts `DEVICE_LOST` while a launched app runs, and asserts that no new renderer is made before `Application finished`. A fifth makes the first two attempts fail, and asserts `Renderer: creating a new renderer, attempt 3 of 5` and the redraw. Run with the fallback-font fixture, a fallback title is lit again after the reload.
- **Gamepads.** The `STREAMFLEX_TEST_PAD` hook ports to SDL3's `SDL_AttachVirtualJoystick`, and every pad check passes unchanged.
- **Fallback fonts.** `FONTCONFIG_FILE` names a fixture whose only folder holds `sf-box.ttf`, a font of filled boxes. A title `中` draws filled and logs `Fonts: the <N> pt titles fall back to <dir>/sf-box.ttf (face 0), first for U+4E2D`; with the folder empty it draws the hollow box and logs that no font has it. On the real fonts, Debian logs its CBDT emoji font and Fedora its COLRv1 one as skipped, then a fallback to DejaVu Sans. A CJK title logs `NotoSansCJK-Regular.ttc (face 0)` on Debian and `NotoSansCJK-VF.ttc (face 0, named instance 4)` on Fedora. A long CJK title in Shrink mode still fits its button (`90-titles-fit.sh`), a CJK menu name falls back on the settings screen, and no existing run logs a fallback, which proves its exact-pixel checks are untouched.
- **Scale.** With `SDL_VIDEO_X11_SCALING_FACTOR=2`, the log says `display scale 2.00` and the home grid's pixels match a run without it, which says `display scale 1.00`. The runs pin `GDK_SCALE`, `Xft.dpi` and the variable unset except where a check sets one.
- **Size change.** After Main is drawn, the window is resized to 1280x720. The log shows `Screen: 1280x720 px, was 1920x1080; reloading`, Main draws whole in 1280x720, and with settings open the page and its highlighted row survive. With a picker open, the picker closes as Back would and the page underneath stays. Whether SDL3's X11 driver reports a resize of a fullscreen window with no window manager is unverified; if it does not, a test hook posts `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` with a forced size, which proves the reload but not SDL's event.
- **A Wayland leg.** A Weston headless compositor runs beside Xvfb, so SDL's choice is a real one. Fedora 44's Weston 15 has fifo-v1 and Debian trixie's Weston 14 does not (per Weston's release notes; its source was not read), so both branches of SDL's rule run.
  - **Fedora:** no override gives `Video: SDL's wayland driver`, and `SDL_VIDEO_DRIVER=x11` gives x11 with the override line. The `Screen:` line equals the headless output, and a pad-driven walk logs as usual. A launch of a second Wayland client, killed by PID, logs `Lost keyboard focus`, `Application detected`, `Gained keyboard focus` and `Application finished` in order, within 2 s of the kill. A restart logs `Restarted, so the window is brought to the front`, then `Gained keyboard focus`. If Weston's headless output takes a scale of 2, the leg also asserts `Screen: 3840x2160 px, pixel density 2.00, display scale 2.00` and `Resolution:    3840x2160`, the only automated proof of native pixels on Wayland.
  - **Debian:** no override gives x11 and the fifo-v1 note; `SDL_VIDEO_DRIVER=wayland` gives wayland.
  - Key-driven checks stay on Xvfb: Weston offers no way to press keys. Weston's exact flags are unverified and are settled when the check is written.
- **Gamepad battery** (`66-pad-power.sh`). The virtual pad with no power file logs `Gamepad power: instance id N, unknown, unknown, no percent` and draws no glyph. Through `STREAMFLEX_TEST_PAD_POWER`, `battery 40` draws the amber glyph at the logged rectangle, `battery 10` the red one, and `battery 100` hides it; `charging 20`, `charged 100` and `unknown -1` draw nothing; 44 after 40 stays shown and 46 hides. With two pads the lower is drawn; unplugging the pad hides it; the glyph takes the corner opposite the clock for either alignment; the settings preview never draws it. Mutants drop the `ON_BATTERY` guard, the hysteresis and the opposite-corner rule.
- **Timing.** A 48-entry fixture (`f60-icons.ini`) asserts each `Timing:` line appears once per menu render, every figure is a whole number, and read, decode and texture add up to no more than the total plus rounding. No absolute time is asserted: the sanitizer build and lavapipe distort it.
- **Leaks.** The LeakSanitizer leg's Mesa handling extends to lavapipe. Anything newly suppressed is listed with its reason.
- **Exact-pixel checks.** lavapipe may draw edges differently from llvmpipe. A check that needs a tolerance gets a ledger ruling with the measured difference. Nothing is loosened silently, and a behavior change goes to the user.

**The mutant rule continues:** every branch the brief's tests leave unexercised gets a mutant-proven check, with an unmutated control.

**Hands-on (v0.5.0):**

| Where | What |
|---|---|
| The user's Windows machine, real GPU | The log reads `vulkan`; Transparent (per the spike); pads; the Windows `:exit` hotkey; VSync on and off; each fallback forced with `SDL_RENDER_DRIVER`; device loss if a driver reinstall is practical. CJK, emoji (in color), Arabic and Hindi titles. A 150 % display scale: the native `Resolution:`, `DPI awareness: per-monitor v2`, a frame identical to 100 %, a live scale change, and a resolution change during a game. A wireless pad's battery glyph, with whatever pads the user has. Before the rest of the plan is written: the timing measurement on master with the throwaway patch and the user's real config, a gate (§ 8). |
| qa-harness, Ubuntu 26.04 guest | The full Ubuntu checklist (`tests/qa/checklists/ubuntu-hands-on.md`) on SDL3, installing the trixie-built `.deb`, and recording which renderer the guest chose. On the default driver: smoke module 13, the launch rows and the restart rows, which gate native Wayland (§ 5), recording whether libdecor is loaded. A fractional GNOME output (125 % and 150 %) and 200 %. CJK and emoji titles (emoji in monochrome). |
| Fedora 44 | Blocked on qa-harness M3 (todo item 25); runs when it unblocks, adding the CJK title at regular weight (named instance 4) and a fractional KDE output on Fedora KDE. |
| Raspberry Pi | **Not covered by hand:** the test-device list has no Pi. The arm64 package is proven only by CI building it and inspecting its metadata, and the release notes claim no more. |

The timing measurement on the slowest Linux target, the Ubuntu qa-harness guest, is the other half of that gate (§ 8). It is recorded in the plan's ledger and waits on the qa-harness queue. qa-harness has StreamFlex queued after ws-scrcpy-web, so the hands-on can go through it.

**Smoke rows.** StreamFlex's rule is that every user-visible change adds rows to `tests/qa/smoke-test.md` (being written on `feat/smoke-doc`, not yet merged), so the plan carries a smoke-rows task; the ids are the smoke doc's. One claim per row:
- **Fallback fonts:** a CJK title draws glyphs, not boxes (`[Win]`, naming YaHei or Yu Gothic and Malgun Gothic; `[Ubuntu]` `[Kubuntu]`, Noto Sans CJK; `[Fedora]` `[Fedora-KDE]`, at regular weight); an emoji title draws in color on Windows, and in monochrome on Linux with the skipped color font logged; on Windows, Arabic draws joined right to left and Hindi's i-matra sits before its consonant; a character no font has shows a box and the log says so; a CJK menu name draws on the settings screen; a long CJK title fits its button in Shrink mode; a Latin-only config logs no fallback.
- **Native Wayland:** with no driver forced, the log names `wayland` (row 13.1 amended) and shows no fifo-v1 note; the cursor is hidden; the Menu key opens settings; closing an app left in front for two minutes returns focus within 2 s; a settings restart takes focus, with no GNOME "is ready" notice and no KDE attention highlight; `InhibitOSScreensaver` holds an inhibit on GNOME and on KDE.
- **Display scaling:** at 150 % on Windows, `Resolution:` is the native mode, the scale logs as 1.50, the process is per-monitor v2, and the frame matches 100 %; under Wayland at 200 % and at fractional 150 % on GNOME and KDE, `Resolution:` is the output's mode and a 1 px outline is 1 px on screen; the driver SDL picks unforced at 150 % is recorded; a resolution change reloads at the new size, waits for a running app, and keeps the settings page and its edits; a resolution change with a picker open closes the picker as Back would; a live scale change on Windows redraws nothing; moving to a second display reloads at its size. Rows 1.25, 13.5 and 13.12 note that `Resolution:` is now the window's pixel size.
- **Gamepad battery:** a pad that reports nothing shows nothing; an Xbox pad on its last step shows the red glyph, and on its cable shows none; a Bluetooth Xbox pad reports a percent; a DualSense or DualShock 4 on USB reports charging and shows none; a low Bluetooth DualSense shows the glyph, recording any effect on a DirectInput app afterward; the glyph returns after an app exits and follows the clock's side; on Linux without `hidraw` access nothing shows and nothing breaks.
- **Startup timing:** the first menu's `Timing:` lines appear in the log, with whole-number figures.

## Out of scope

- Real per-pixel transparency (`SDL_WINDOW_TRANSPARENT`): sub-project 4.
- Pad types and button labels that match the pad in hand: todo item 26. (The low-battery glyph is in scope, § 7.)
- Text input and the no-OS-keyboard rule: 3c. Its pre-plan research changes from *the SDL2 controls at the 2.0.18 floor* to *the SDL3 controls at the 3.2.10 floor*.
- A renderer setting in `config.ini` or on the settings screen.
- SDL's GPU API (`SDL_gpu`) and the `gpu` renderer.
- SDL3_image's own SVG loader in place of nanosvg.
- SDL3_ttf's text engine (`TTF_CreateRendererTextEngine`). 3c's research looks again.
- Applying the OS display scale to StreamFlex's fixed pixel and point sizes.
- `SDL_AsyncIO`, and parallel icon decode unless the timing gate trips (§ 8).
- A texture cache that keeps icon textures across a title-size change or a reload.
- HDR detection: todo item 24.
- Any support for Debian bookworm, Ubuntu 24.04 LTS, or older releases.

## Follow-on edits elsewhere

- The StreamFlex todo (item 7) gives 3c's release as v0.5.0; that becomes v0.6.0. Its 3c research line moves from SDL2 to SDL3, as above.
- The 3c spec (`design/specs/2026-09-29-menu-editing-design.md`) names no release. Its text-input line is re-read against SDL3 when 3c's research runs, as the todo already requires.
