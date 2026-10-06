# The SDL3 port, Vulkan first: design

**Date:** 2026-10-06
**Status:** approved in brainstorming, awaiting spec review
**Covers:** the SDL3 port sub-project (todo item 7's order: after 3b, before the 3c plan). StreamFlex moves from SDL2, SDL2_image and SDL2_ttf to SDL3, SDL3_image and SDL3_ttf, and asks for Vulkan first on every platform.
**Ships as:** **v0.5.0**, on its own. 3c moves to **v0.6.0**.
**Builds on:** `feat/settings-all` (3b, v0.4.0) as it merges. **The implementation plan is written after 3b merges**, because it names exact functions and lines that 3b is still changing.

## Context

StreamFlex is C on SDL2. SDL2 provides the window, the renderer, input, gamepads and threads; SDL2_image (with libjpeg-turbo, libwebp and libpng) loads images; SDL2_ttf (FreeType, plus HarfBuzz on distro builds) draws text. Measured at `e13f2f6` (3b, Task 13), about 535 lines across 22 files under `src/` use SDL, most of them in `launcher.c` (231), `settings_screen.c` (76), `image.c` (67) and `util.c` (27). The rest of the stack is inih, getopt on Windows, vendored nanosvg, and a thin OS layer in `src/platform/`.

The renderer asks for nothing in particular today (`launcher.c:362`, `SDL_CreateRenderer(window, -1, flags)`), and SDL2 picks **Direct3D 9 on Windows** and **OpenGL on Linux**. SDL2's renderer has no Vulkan backend. SDL3's has `vulkan`, `gpu`, `direct3d12` and `direct3d11`.

The user's binding decisions (todo § Binding decisions, 2026-09-30) set the direction:
- **Bleeding edge.** StreamFlex targets current technology and does not support OS releases that lack it. A user on an older Debian, Ubuntu, RHEL or Raspberry Pi OS upgrades. **There is no backport** (user, 2026-10-06): no bundled SDL3 for older releases and no SDL2 branch kept alive.
- **SDL3 as its own sub-project,** after 3b ships and before the 3c plan. 3b finishes on SDL2; 3c is planned and built on SDL3.
- **Vulkan first everywhere,** as a renderer preference list: Windows `"vulkan,direct3d12,direct3d11,opengl"`, Linux and Pi `"vulkan,opengl"`.

## Decisions

| Question | Decision |
|---|---|
| Scope | **A straight port, plus the Vulkan preference list.** Behaviour stays as on SDL2. New SDL3 features stay with the sub-projects that own them: real per-pixel transparency (`SDL_WINDOW_TRANSPARENT`) with sub-project 4, pad types and button labels with item 26, and the text-input rework with 3c. |
| The one addition | **Recovering a lost GPU device.** On `SDL_EVENT_RENDER_DEVICE_RESET`, run the full reload that 3b's Task 4 built for a config change. The new renderers bring this failure with them (a driver update, a GPU reset), so it counts as part of the port. |
| How to port | **Directly, on one branch, subsystem by subsystem.** SDL's migration tools (`build-scripts/rename_symbols.py`, `rename_headers.py`, `rename_macros.py` and `SDL_migration.cocci`, per SDL's `docs/README-migration.md`) do the mechanical renames; each subsystem is then ported by hand with its own tests. No compatibility header (SDL2 idioms would stay forever) and no sdl2-compat stepping stone (it reaches neither the preference list nor the new APIs). |
| Release | **v0.5.0, alone.** Its notes open with the changed system requirements. 3c becomes v0.6.0. A port regression shows before 3c's features can hide it, and the hands-on check tests one change. |
| Platforms | **Debian trixie, Raspberry Pi OS trixie, Ubuntu 25.10, Arch, and Windows,** or newer. The `.deb` is built on trixie and installs on newer Debian and Ubuntu releases. **v0.4.0 is the last release for Debian bookworm and Ubuntu 24.04 LTS**, neither of which packages SDL3; it stays downloadable. |
| Harness | **Every check on Vulkan** (Mesa's lavapipe), plus a small subset that proves the automatic fallback to OpenGL. |
| Choosing another renderer | **The standard `SDL_RENDER_DRIVER` environment variable,** honoured over the preference list. No config key and no settings row. |

## 1. What the port must get right

These are the traps that compile cleanly and then misbehave; the renames are not listed.

1. **Return values flip.** SDL3 functions that returned a negative error code now return `bool`, true on success (`README-migration.md`). Every `if (SDL_X(...) < 0)` and `!= 0` test inverts silently. The plan walks every call site in the port's diff; a test or a mutant proves each guarded failure branch still fires.
2. **A saved key code keeps its meaning.** `config.ini` stores hotkeys as hex SDL key codes (`#4000003A` is F1), and Windows `:exit` registration depends on the F1-F11 and F13-F24 codes (`keycode_convert.h`). SDL3 makes `SDL_Keycode` a `Uint32` with `SDLK_*` as defines, keeps the scancode mask, and changes how layouts and modifiers shape a key event's keycode (`SDL_HINT_KEYCODE_OPTIONS`). StreamFlex must read the same key from the same code as on SDL2. A unit test pins every bindable code; if a value moved, a translation table keeps old configs meaning what they meant, and the writer keeps writing codes that SDL2-era files understand.
3. **Pixel positions.** SDL3 draws with float rectangles (`SDL_RenderTexture` takes `SDL_FRect`). The layout stays integer; rectangles convert at the draw call only, so nothing lands half a pixel off. The harness's exact-pixel checks are the proof.
4. **Windows Transparent mode under the new renderers.** Transparent is a chroma key, `SetLayeredWindowAttributes(..., LWA_COLORKEY)` (`platform/win32.c:116-119`), proven only with D3D9. Vulkan and D3D12 swapchains on a layered window may ignore it. This is the largest risk, and the plan's first task is a spike (§ 3).
5. **Text, images and the window handle.**
   - SDL3_ttf's render calls take UTF-8 with explicit lengths and float point sizes.
   - `SDL_RWops` becomes `SDL_IOStream`.
   - `SDL_SysWMinfo` is gone. The HWND comes from the window property `SDL_PROP_WINDOW_WIN32_HWND_POINTER` (today `launcher.c:206`, `platform/win32.c:26`).
6. **Text input is off by default in SDL3** ("no longer automatically enabled when initializing video"). That suits 3c's rule that no OS keyboard may appear. The port keeps it off and calls `SDL_StartTextInput` nowhere; 3c owns the rest.
7. **Gamepads.** `SDL_GameController` becomes `SDL_Gamepad`, whose buttons are named by position (south, east, west, north). StreamFlex's own names in `config.ini` (`ButtonA`, `LTrigger` and the rest) and their meaning do not change; the mapping between them and SDL's buttons lives in the code. The `ControllerMappingsFile` format does not change (`SDL_AddGamepadMappingsFromFile`).
8. **Hints that left.** `SDL_HINT_RENDER_SCALE_QUALITY` (`launcher.c:235`) is removed. SDL3 creates textures with `SDL_SCALEMODE_LINEAR` by default; the port sets it explicitly on each texture anyway, so scaled icons stay as today whatever the default becomes. Every other hint StreamFlex sets (`launcher.c:233-236`) is checked against SDL3's list and its replacement named.

## 2. Build, packaging and CI

**Version floors** come from Debian trixie, the oldest supported platform: **SDL3 3.2.10, SDL3_image 3.2.4, SDL3_ttf 3.2.2.** They replace `MIN_SDL_VERSION "2.0.18"`, `MIN_SDL_IMAGE_VERSION "2.0.5"` and `MIN_SDL_TTF_VERSION "2.0.15"` (`CMakeLists.txt:34-36`). Ubuntu 25.10 carries 3.2.20 / 3.2.4 / 3.2.2 and Ubuntu 26.04 LTS 3.4.2 / 3.4.0 / 3.2.2, both above the floors; Ubuntu 24.04 LTS carries none.

**Finding the libraries.** SDL3 ships CMake config files on every platform, Debian's `-dev` packages included. Linux's pkg-config branch (`CMakeLists.txt:50-52`) and Windows' `find_package` branch (`:58-60`) become one: `find_package(SDL3 3.2.10 CONFIG REQUIRED)` and the same for `SDL3_image` and `SDL3_ttf`. inih and getopt stay as they are.

**CI builds** (`.github/workflows/build.yml`):

| Build | Today | After the port |
|---|---|---|
| Debian amd64 | `debian:bookworm`, `libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev` | `debian:trixie`, `libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev` |
| Raspberry Pi arm64 | `debian:bookworm` on `ubuntu-24.04-arm`, `-DRPI=1` | `debian:trixie`, same packages, `-DRPI=1` |
| Arch | `sdl2 sdl2_image sdl2_ttf` | `sdl3 sdl3_image sdl3_ttf` |
| Windows (vcpkg, `x64-windows-static`) | `sdl2`, `sdl2-image[libjpeg-turbo,libwebp]`, `sdl2-ttf` | `sdl3[vulkan]`, `sdl3-image[jpeg,png,webp]`, `sdl3-ttf` |

- **`vulkan` is not a default feature of vcpkg's `sdl3` port.** Without it the Windows build silently has no Vulkan renderer and the list starts at D3D12. A unit test catches that (§ 4).
- SDL3_image in vcpkg enables no image format by default, so all three are named. HarfBuzz stays off on Windows, as now.
- `VCPKG_COMMITTISH` moves to a commit carrying the SDL3 ports (vcpkg had `sdl3` 3.4.18, `sdl3-image` 3.4.8 and `sdl3-ttf` 3.2.2 on 2026-10-06).
- **Both ends of the range are built.** Debian and the Pi build against the floor (3.2.x); Arch and Windows build against the newest (3.4.x). Code that only compiles on 3.4 fails CI.

**Packages.**
- The `.deb` depends on `libsdl3-0`, `libsdl3-image-0` and `libsdl3-ttf-0`. The note at `CMakeLists.txt:194-196`, which explains where SDL2's version floor came from (the `SDL_RenderSetVSync` symbol), is rewritten for the symbol that sets SDL3's.
- The Arch package depends on `sdl3`, `sdl3_image` and `sdl3_ttf`.
- The Windows zip stays static, with no DLLs beside the exe.
- Nothing is bundled on Linux. nanosvg stays vendored, even though SDL3_image can load SVG.

**Harness images.** `tests/headless/Dockerfile` moves from bookworm to trixie and adds `mesa-vulkan-drivers` (lavapipe). `Dockerfile.fedora` drops sdl2-compat for native SDL3 and adds lavapipe.

**Documents.** The README's requirements, the docs site's download and install pages, and the v0.5.0 CHANGELOG all state *Debian trixie, Raspberry Pi OS trixie, Ubuntu 25.10, or newer*. The v0.5.0 notes say v0.4.0 is the last release for Debian bookworm and Ubuntu 24.04 LTS.

## 3. The renderer

**Creation.** `SDL_CreateRenderer(window, list)`, with the list `"vulkan,direct3d12,direct3d11,opengl"` on Windows and `"vulkan,opengl"` on Linux and the Pi. SDL takes the first that works. There is no `software` at the end, which matches today's accelerated-only request; if nothing works, the existing error path reports it and exits as today.

**Override.** When `SDL_RENDER_DRIVER` is set in the environment, StreamFlex passes no list and SDL uses the variable. The harness's fallback checks rely on it, and it is the answer to *Vulkan misbehaves on my GPU*.

**Logging.** The log states the renderer chosen, from `SDL_GetRendererName`: `Renderer: vulkan`. When that is not the list's first entry, it says so: `Renderer: opengl (vulkan was unavailable)`. When the variable chose it: `Renderer: opengl (from SDL_RENDER_DRIVER)`. The harness reads these lines. `debug.c:339`'s renderer dump moves from `SDL_GetRendererInfo` to SDL3's renderer properties (name, maximum texture size, VSync).

**VSync.** The live VSync setting (`launcher.c:270-320`) moves to `SDL_SetRenderVSync` and `SDL_GetRenderVSync`. The rule that greys the VSync row with its reason when the renderer cannot do VSync stays, re-based on SDL3's answer, and is checked per renderer.

**Device loss.** On `SDL_EVENT_RENDER_DEVICE_RESET` (and on `SDL_EVENT_RENDER_TARGETS_RESET`, which affects only render targets), StreamFlex runs the full reload a config change runs: every texture is rebuilt, and the current menu and screen state are kept. The log says `Renderer: device lost; reloading`. StreamFlex handles neither event today.

**The Transparent spike (the plan's first task).**
- **Where:** the user's Windows machine and its real GPU. The qa-harness Windows guest has no GPU and would test only the D3D fallbacks.
- **What:** with `Background=Transparent`, confirm the desktop shows through and the key colour is cut out, on each of `vulkan`, `direct3d12`, `direct3d11` and `opengl`, forcing each with `SDL_RENDER_DRIVER`; then repeat with Transparent switched on live from the settings screen.
- **The spike's build is throwaway:** the smallest SDL3 build that opens the window, draws the background and applies the chroma key.
- **If all four work,** nothing changes. **If any fails,** the measured table goes to the user with the options before the rest of the plan is written. Changing renderer on the fly is real work and a behaviour change, so it is the user's decision, not a ruling.

## 4. Testing

**Unit tests (CTest)** are ported, and build against both ends of the version range. New:
1. **The key-code table.** Every code a user can bind, keyboard and pad, reads as the same key or button as on SDL2, including the F-key set behind Windows `:exit`.
2. **An old config still means the same.** 3b's bindings fixtures, written for SDL2-era files, load to exactly the same bindings.
3. **The built SDL has Vulkan.** `SDL_GetRenderDriver` lists `vulkan`. It needs no GPU, so it runs on Windows (catching a missing `sdl3[vulkan]`) and on Linux.
4. **Failure branches after the return-value flip.** Each guarded SDL call's failure branch is proven by a test or a mutant.

**Headless harness** (Debian trixie and Fedora 44, both with lavapipe):
- **Every existing check runs on Vulkan,** and the run asserts `Renderer: vulkan`.
- **Automatic fallback.** A subset hides the Vulkan driver (`VK_DRIVER_FILES` pointed at nothing) so that SDL falls back on its own. It asserts `Renderer: opengl (vulkan was unavailable)`, then runs the drawing checks for the background, titles, the settings screen and the pickers.
- **Override.** One check sets `SDL_RENDER_DRIVER=opengl` and asserts it beat the list.
- **Device loss.** A test hook posts `SDL_EVENT_RENDER_DEVICE_RESET`; the check asserts the reload ran and the screen redraws pixel-correct.
- **Gamepads.** The `STREAMFLEX_TEST_PAD` hook ports to SDL3's `SDL_AttachVirtualJoystick`, and every pad check passes unchanged.
- **Leaks.** The LeakSanitizer leg's Mesa handling extends to lavapipe. Anything newly suppressed is listed with its reason.
- **Exact-pixel checks.** lavapipe may draw edges differently from llvmpipe. A check that needs a tolerance gets a ledger ruling with the measured difference. Nothing is loosened silently, and a behaviour change goes to the user.

**The mutant rule continues:** every branch the brief's tests leave unexercised gets a mutant-proven check, with an unmutated control.

**Hands-on (v0.5.0):**

| Where | What |
|---|---|
| The user's Windows machine, real GPU | The log reads `vulkan`; Transparent (per the spike); pads; the Windows `:exit` hotkey; VSync on and off; each fallback forced with `SDL_RENDER_DRIVER`; device loss if a driver reinstall is practical. |
| qa-harness, Ubuntu 26.04 guest | The full Ubuntu checklist (`tests/qa/checklists/ubuntu-hands-on.md`) on SDL3, installing the trixie-built `.deb`, and recording which renderer the guest chose. |
| Fedora 44 | Blocked on qa-harness M3 (todo item 25); runs when it unblocks. |
| Raspberry Pi | **Not covered by hand:** the test-device list has no Pi. The arm64 package is proven only by CI building it and inspecting its metadata, and the release notes claim no more. |

qa-harness has StreamFlex queued after ws-scrcpy-web, so the hands-on can go through it.

## Out of scope

- Real per-pixel transparency (`SDL_WINDOW_TRANSPARENT`): sub-project 4.
- Pad types and button labels that match the pad in hand: todo item 26.
- Text input and the no-OS-keyboard rule: 3c. Its pre-plan research changes from *the SDL2 controls at the 2.0.18 floor* to *the SDL3 controls at the 3.2.10 floor*.
- A renderer setting in `config.ini` or on the settings screen.
- SDL's GPU API (`SDL_gpu`) and the `gpu` renderer.
- SDL3_image's own SVG loader in place of nanosvg.
- Any support for Debian bookworm, Ubuntu 24.04 LTS, or older releases.

## Follow-on edits elsewhere

- The StreamFlex todo (item 7) gives 3c's release as v0.5.0; that becomes v0.6.0. Its 3c research line moves from SDL2 to SDL3, as above.
- The 3c spec (`design/specs/2026-09-29-menu-editing-design.md`) names no release. Its text-input line is re-read against SDL3 when 3c's research runs, as the todo already requires.
