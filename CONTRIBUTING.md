# Contributing to StreamFlex

StreamFlex is a customizable application launcher and desktop replacement for Windows and Linux, written in C with SDL3.

This repository is an independent project. It started from complexlogic's Flex Launcher at v2.2 and has been developed separately since; it does not track, sync with, or send changes back to that project. Changes land here through the process below.

## Building

Build instructions for Windows (Visual Studio + vcpkg) and Linux (CMake + distro packages) are in [`docs/compilation.md`](docs/compilation.md). The CI workflow in [`.github/workflows/build.yml`](.github/workflows/build.yml) is the authoritative, always-exercised recipe for all four targets: Windows, Debian, Raspberry Pi, and Arch Linux. After building, run the unit tests with `ctest --test-dir build -C Release --output-on-failure`. CI also builds and runs them with AddressSanitizer and UBSan in the Debian headless image (the `Unit tests (ASan, UBSan)` job), which catches a read before an array or of a freed slot that a Release build can pass. `tests/unit.sh` runs that job's build in either harness image: `docker run --rm --security-opt seccomp=unconfined -v "$PWD:/src:ro" streamflex-test bash /src/tests/unit.sh [<ctest -R regex>]`; its header gives `UNIT_TARGETS`, for a build of some targets only, and `MUTANT_FILE` with `MUTANT_SED`, for a unit mutant that leaves the repository untouched. The icon library's tooling has Python tests of its own, and its README says how to run them and `check-library.py`; see [`branding/library/README.md`](branding/library/README.md).

The unit tests reach failures no real run gives through two seams, both for tests only. The pure modules (fileio, inidoc, config_save, settings, browser, listpick, fontlist and bindings) allocate through `alloc.h`, and `alloc_set_hooks()` puts a test's allocator in its place, which `test_alloc` uses to make every allocation fail in turn. The pure modules derive and colorpick allocate nothing. Each pure module builds without SDL and has its own unit test (`test_derive`, `test_listpick`, `test_colorpick`, `test_fontlist` and `test_bindings` among them). `fileio_set_fault()` makes one fileio step fail: `FILEIO_FAULT_LIST_READ` fails a listing's read after a given number of entries with a given error, `FILEIO_FAULT_KEEP` makes a replace unable to keep the old file's permissions or attributes, and `FILEIO_FAULT_NO_KIND` (Linux) makes a listing give no entry's kind, as some file systems give none. On Linux, `fileio_set_mount_table()` points fileio at a pretend `/proc/self/mounts`.

The headless checks run in Docker, as the `headless` jobs in `build.yml` do (each pass in two shards): build an image from `tests/headless`, then run `run.sh` in it with the repository mounted read-only at `/src` and a folder for the results at `/out`, with the seccomp profile off (the harness turns address randomization off with `setarch -R`, which ASan needs and Docker's default profile refuses):

    docker build -t streamflex-test tests/headless
    docker run --rm --security-opt seccomp=unconfined -v "$PWD:/src:ro" -v "$PWD/headless-out:/out" streamflex-test bash /src/tests/headless/run.sh local

There are two images and one `run.sh`. `Dockerfile` is Debian trixie with SDL3 at the port's floor; `Dockerfile.fedora` is Fedora 44 with its own newer SDL3, whose Mesa has llvmpipe but no softpipe. The launcher draws with Vulkan on Mesa's lavapipe in both. What differs between them is detected, never branched on by name, and `run.sh` prints the Mesa driver it found (`Mesa: <driver>, from <path>`). Both images also hold Weston, which check 67 starts headless beside Xvfb so that SDL's choice of video driver is a real one: Debian's Weston 14 has no fifo-v1 and Fedora's Weston 15 has it, so each image runs one branch of SDL's rule, and the check reads Weston's version, never the distribution. For the Fedora pass, build its image and give its tag to the same `docker run`:

    docker build -t streamflex-test-fedora -f tests/headless/Dockerfile.fedora tests/headless

A second argument to `run.sh` picks the mode:

- `run.sh <label>` runs every check in `checks/`, in name order, starting with `00-harness.sh`, which checks the harness's own helpers. The launcher is built with ASan and UBSan, and a compiler warning outside `src/external/` fails the run. Some checks also read the screen: `xwd` takes a screenshot and `pixels.py` reads chosen points, such as the settings preview's color, image and Transparent checkerboard. The checks press keys with `xdotool`, except the function keys: `run.sh`'s `xdotool` function sends those through `key.py`, by keycode with no modifier, because Fedora's keymap makes `xdotool` hold Alt down for every F key, which a key capture would catch.
- `run.sh <label> scrollfail` runs item 11 only, with the scroll arrow's texture forced to fail, a path no config or input can reach.
- `run.sh <label> leaks` runs every check again with LeakSanitizer on. Mesa's software drivers (OpenGL's and lavapipe), the Vulkan loader and libX11 are preloaded into the launcher so the blocks they hold at exit stay reachable, and only the two leaks in third-party libraries listed below are suppressed (on Debian): a run that leaks anything else fails its check, and each leak is also listed at the end as a `LEAK` line that counts in `N failed`.
  - On Fedora, SDL3 loads libX11 at run time and unloads it at `SDL_Quit`, so the blocks libX11 keeps from `SDL_Init` (its input method and resource database) lose their only pointers and read as leaks: under sdl2-compat only when StreamFlex stopped on a fatal error before any window existed (OpenGL's libGL kept libX11 loaded once there was one), and on every run once the renderer is Vulkan. The leak pass preloads libX11 for that reason, and with it a fatal stop before the window is clean on Fedora 44 (measured). Checks 05 and 59 still stop on a font the clock looks for, after the window opens, as they were written before the preload.
  - Debian's leak pass suppresses two leaks in third-party libraries, each by one function in that library's code, never a whole library. The image installs the debug symbols of Mesa's GLX library and of SDL3 (`libglx-mesa0-dbgsym`, `libsdl3-0-dbgsym`, from Debian's debug archive) so LeakSanitizer can name those functions, and names its suppressions file in `LEAK_SUPPRESSIONS`, which `run.sh` reads in the leak pass. Fedora's image names none, so its leak pass suppresses nothing. The file, `tests/headless/lsan-debian.supp`, gives each entry's stack and reason:
    - `leak:VULKAN_CreateRenderer`: SDL 3.2.10 leaks its Vulkan renderer's data (10,600 bytes) when Vulkan fails and it falls back to OpenGL, the `gl-*` runs. Fixed upstream in libsdl-org/SDL 5622eef5a (3.4.0 and later); no 3.2.x release has it.
    - `leak:__glXInitialize`: Mesa 25.0.7 keeps its GLX state for the X display (about 108 KB), made as SDL chooses an OpenGL visual, past exit, on every OpenGL run. Fedora's SDL 3.4.18 with Mesa 26.2.4 is clean.
  - LeakSanitizer sometimes cannot start its tracer as a run exits: the run's stderr says `WARNING: ptrace appears to be blocked (is seccomp enabled?). LeakSanitizer may hang.` and `Child exited with signal 96.`, and no leak is listed, because no leak check ran. A busy host brings it on (the containers run with seccomp off, so ptrace is not blocked). `run.sh` runs such a run once more, by itself (`with_tracer_retry`, which `run_quick` and `run_keys` go through): only when its stderr has those two lines and no sanitizer's report, and the run was otherwise whole (no line missing, no frame missed, no hang). Nothing else is ever retried, and the warning is never suppressed. Before the retry it puts back what the run could change (the test user's home, the files at the top of `/tmp`, and the run's own files in the output folder), so a +key function must do the same when called again from the same state. The console says `LSan tracer flake in <run>: retried`; the first try's files are kept in `flake/<run>/`, and its stderr as `<run>.flake.err`, where the leak listing still finds any leak. A retry that meets the flake again fails: its exit code file says so, its check fails, and it is listed at the end as a `LEAK` line. Only then is it worth running the set again by hand.

A pass of about 260 checks takes about 50 minutes on one container, mostly waiting on the launcher, so it can also run in shards, each a run of its own in a container of its own. `run.sh <label> K/N` (or `run.sh <label> leaks K/N`) runs shard K of N. The build, Xvfb, the fixtures and the check files with a `# Every shard runs this file` line run in every shard: `00-harness.sh`, whose helpers each shard relies on, and `90-titles-fit.sh` and `95-every-run.sh`, which read the logs of their own shard's runs. Each other check file runs in exactly one shard, planned from the time it took (the table in `run.sh`; a file not in it counts as 60 seconds) so the shards take about as long. A shard's console says which check file each result belongs to (its `SHARD` lines), and a shard alone is not the pass: `merge.py` adds a pass's shard consoles up into its one result, printing every result line, the `PASS`, `FAIL` and `LEAK` counts and `N failed`. It also fails, with an `INCOMPLETE` line for each problem and exit code 2, unless the consoles are one complete run: shards 1 to N of one tree, each present once and run to its end, every check file the shards declared run in exactly one of them, and no case in two. Run by hand, each shard's console is saved for the merge by redirecting it, into a folder made first:

    mkdir -p headless-out
    docker run --rm --security-opt seccomp=unconfined -v "$PWD:/src:ro" -v "$PWD/headless-out:/out" streamflex-test bash /src/tests/headless/run.sh local 1/2 > headless-out/local-s1.console.log
    docker run --rm --security-opt seccomp=unconfined -v "$PWD:/src:ro" -v "$PWD/headless-out:/out" streamflex-test bash /src/tests/headless/run.sh local 2/2 > headless-out/local-s2.console.log
    python tests/headless/merge.py --leg local headless-out/local-s1.console.log headless-out/local-s2.console.log

A check's `PASS` or `FAIL` text must not repeat one in another check file: the merge reads the same text from two shards as one case run twice, and fails the pass.

Some checks hold only on a quiet box. A check file whose results a loaded box could change has a line `# Timing-sensitive` near its top: its results read wall time, frame rates, frame gaps or durations, or depend on a hold or a wait that scheduling latency can cut short or stretch, such as a pad tapped for 0.3 s that must end before the 500 ms repeat, an 11 s sleep that must outlast a 10 s window, or a key that must land inside a 3 s fade. The test for the mark: would this result change on a loaded box? Keys that `run_keys` waits for frame by frame, holds that end on a log line, and bounded waits for a line or a file do not need it. A file whose cases are mostly steady keeps its few timing-sensitive ones in a file of their own beside it, with the same number and `-timing` (`50-settings-timing.sh` beside `50-settings.sh`), so that only they need the quiet box; a case that reads another run's log goes where that run is.

A check file says which it is when it is written, near its top, in one line with its reason. A timing-sensitive one has `# Timing-sensitive: <why a loaded box could change its results>`, as every marked file does (49's reads `# Timing-sensitive: reads the fade's and the dim's durations, and a frame rate`); a new one for the rest set has `# Not timing-sensitive: <why not>`. A new case added to an existing file follows the same test: a timing-sensitive case goes in the file's `-timing.sh` file (made for it if there is none, and added to `timing_pinned`), never in a file the rest set runs. A `-timing.sh` file too long for one shard of the timing set is split in two, as check 62's is (`62-settings-bindings-timing.sh` and `62-settings-bindings-pad-timing.sh`), so the two shards can share it. Reviewers check the line and its reason.

Then:

- `run.sh <label> [leaks] [K/N] timing` runs only the marked files, and `run.sh <label> [leaks] [K/N] rest` only the others. Both run the files every shard runs. With neither, `run.sh` runs every check file, as it always has, and as CI does: GitHub's runners are not shared, so CI keeps the full set.
- A shard of a selection names it in its header, declares only the files it runs and lists each one it leaves out (`SHARD LEFT OUT`), and `merge.py` merges it as a set of its own.
- `run.sh` lists the marked files in `timing_pinned` and refuses to run when the marks differ from that list. A mark lost would move its file into the rest set, which can run beside other work, and no check would fail for it; so adding, removing or renaming a timing-sensitive file changes that list too.
- `merge.py --cover --leg <name> --timing <consoles> --rest <consoles>` proves that one pass's two sets, of one tree, ran every check file exactly once between them (the files every shard runs, in both), and no case twice. It prints their results as the pass's one result, and fails as a merge does.

On Windows, `tests/headless/run-shards.ps1` runs a set of passes this way. It builds the images, then starts the Debian, Fedora and both leak passes (`-Legs plain,fedora,leaks,fedora-leaks` picks some) in N shards each (`-Shards`, 2 by default) as containers named `sfdev-<label>-s<K>`, each on 3 CPUs of its own with `-m 2g --memory-swap 2g`, at most 8 at once and only while 8 GB of host RAM is free. It saves each shard's console as `<label>-s<K>.console.log`, merges each pass into `<label>.merged.log`, and prints a line for each pass and one for the set, exiting non-zero unless every pass is complete with `0 failed`. In 2 shards, the four passes run on 8 containers in about half the time one container per pass takes:

    pwsh tests/headless/run-shards.ps1 -Prefix local

With `-Select timing` or `-Select rest` it runs one selection's set, and each pass is good only when its merge says it ran that selection. The timing set needs a quiet box; the rest can run beside other work. A pass's two sets are then checked as one:

    pwsh tests/headless/run-shards.ps1 -Prefix quiet -Select timing
    pwsh tests/headless/run-shards.ps1 -Prefix busy -Select rest
    python tests/headless/merge.py --cover --leg local --timing headless-out/quiet-s1.console.log headless-out/quiet-s2.console.log --rest headless-out/busy-s1.console.log headless-out/busy-s2.console.log

The harness builds with `-DSTREAMFLEX_TEST_HOOKS`, which nothing else defines. It adds test-only hooks, environment variables a check sets to force what no config or input can reach; normal builds contain none of them:

- `STREAMFLEX_TEST_APP=<file>`: an app takes the screen while `<file>` exists, as a launched one does (launching, then a lost keyboard), and gives it back when the file goes; the focus events are pushed, so the real handlers run. Checks 47 and 48 set it.
- `STREAMFLEX_TEST_CAPTURE_SECONDS=<s>`: a binding's capture listens that long instead of 5 s, so a pad control held through it can run past its repeat's delay with time to spare. Check 62 sets it, for one run.
- `STREAMFLEX_TEST_CLOCK_DELAY_MS=<ms>`: each clock render on the clock's thread waits that long first, and the clock renders every second and logs each render it starts on its thread, so a check can act while one is in flight. Checks 42 and 55 set it.
- `STREAMFLEX_TEST_CLOCK_THREAD_FAIL`: the clock's render thread fails to start, so each render runs on the main thread instead. Check 42 sets it.
- `STREAMFLEX_TEST_DECODE_DELAY_MS=<ms>`: the settings preview waits that long before decoding an image, so a check can press a key during the decode. Check 60 sets it.
- `STREAMFLEX_TEST_DISPLAY_EVENT=<file>`: once `<file>` exists, `SDL_EVENT_WINDOW_DISPLAY_CHANGED` is pushed once for the window's display, as a move to another display sends, and `Test hook: the window moved to a display` is logged as it is pushed, the line check 47 reads from. Check 47 sets it.
- `STREAMFLEX_TEST_FAIL=<step>`: that step runs as if memory had run out: the settings screen's `places`, `browser`, `command` and `bindings`; the pickers' `list`, `rows`, `select`, `pads` and `apply`; the font picker's `fontlist`, `fontscan`, `fontfolder`, `faces` and `sample`; and `restart`, the copy of the arguments a restart starts with, made as StreamFlex starts. With `keep` the saved file's permissions cannot be kept, with `fontthread` the font list's thread does not start, and with `fontadd` that thread cannot add a file. Check 65 sets `places`, `browser`, `command` and `keep`; check 62 `bindings`, `list` and `apply`; check 58 `list`, `rows`, `select` and `pads`; check 59 `list`, `rows`, `select` and the font picker's steps; check 64 `restart`.
- `STREAMFLEX_TEST_FAIL_SHRINK_STEP`: every step down in Shrink mode fails to open its font. Check 40 sets it.
- `STREAMFLEX_TEST_FAIL_TITLE_SIZE=<pt>`: the title font fails to open at that size. Check 40 sets it.
- `STREAMFLEX_TEST_FONT_DELAY_MS=<ms>`: the font list's thread waits that long after reading the bundled folder, as a slow disk would, so a check can act while the list loads. Check 59 sets it.
- `STREAMFLEX_TEST_FONT_DIRS=<dir>[:<dir>...]`: the font list reads these folders, after the bundled one, in place of the system's and the user's. Check 59 sets it.
- `STREAMFLEX_TEST_FRAME_PERIOD_US=<us>`: each present is held until that long after the last one started, so presents start that far apart however long the frame took to draw (as long as the draw and the last present took less). Timed from the start, the present's own time cancels out: the gap between two presents' returns is the period plus the change in the present's time from one to the next. Timed from the last return, as it first was, the gap carried the whole present on top, about 3 ms in a loaded full set. Unlike the sleep below, the gap carries neither the draw's time nor its jitter. Check 42 uses it to hold its full-size run's presents inside the VSync watch's band, between half and three quarters of the display's frame apart, so the hold must stay before `SDL_RenderPresent`. Unset, it does nothing.
- `STREAMFLEX_TEST_FRAME_REPORT_MS=<ms>`: once that long has passed, the log says how many frames were shown and how many of them `present_frame()` paced: waited out the frame's time itself, for an FPS limit or for a renderer that does not deliver the VSync wanted. Checks 42, 48 and 49 set it.
- `STREAMFLEX_TEST_FRAME_SLEEP_MS=<ms>`: each frame is held up that long as it is shown, as on a slow machine, so a check can run the launcher at a low frame rate. Unset, it does nothing. Checks 48 and 49 set it.
- `STREAMFLEX_TEST_NO_BASE_PATH`: SDL gives no base path, as `SDL_GetBasePath()` can fail, so the launcher's folder is unknown and a bundled font is looked for only where the packages install it. Check 05 sets it.
- `STREAMFLEX_TEST_NO_LUMINANCE`: an image's brightness cannot be measured, as when its conversion fails, so the contrast warning has no reading for it. Check 58 sets it.
- `STREAMFLEX_TEST_NO_MESSAGE_BOX`: a fatal error quits without its message box, which some SDLs show and wait on. Checks 05, 25, 44, 48 and 59 set it.
- `STREAMFLEX_TEST_NO_RENDER_TARGETS`: settings draw as they would on a renderer without render targets. Checks 55 and 60 set it.
- `STREAMFLEX_TEST_PAD=<file>`: attaches a virtual gamepad, since Xvfb has none, and holds its Start button while `<file>` exists. It does nothing when the gamepad is turned off. Checks 42, 48, 50, 55, 58, 62 and 67 set it.
- `STREAMFLEX_TEST_PAD_BUTTON=<name>`: the virtual gamepad holds this button, by SDL's name for it (`b`), instead of Start; an axis's name (`rightx`) pushes that axis to its positive end. Checks 48, 55, 62 and 67 set it.
- `STREAMFLEX_TEST_PAD_FRAMES=<n>`: each time the virtual gamepad's file appears, its button is held for `n` frames however long the file stays: a tap shorter than a script can time. Checks 62 and 67 set it.
- `STREAMFLEX_TEST_PAD_PLUG=<file>`: another virtual gamepad is plugged in while `<file>` exists and pulled out when it goes, as a pad is plugged in or out by hand at any moment. Checks 55, 58 and 62 set it.
- `STREAMFLEX_TEST_PAD_SWAP`: virtual gamepads are attached and detached, one step a frame, so one arrives at another's old device index with an instance id of its own; then a launch and a return close every pad and open each again. Check 42 sets it.
- `STREAMFLEX_TEST_PIXEL_SIZE=<file>`: while `<file>` exists, the `WxH` it holds is the window's size in pixels in place of SDL's, and each change of it pushes `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`: SDL's X11 driver may not report a resize of a fullscreen window with no window manager. Checks 47 and 48 set it.
- `STREAMFLEX_TEST_RELOAD_FONTS`: the title and clock fonts are opened a second time as they start, as a reload does, so the leak pass shows whether the font each replaces is closed. Check 41 sets it.
- `STREAMFLEX_TEST_RENDER_EVENT=<file>`: each time what `<file>` holds changes, the render events it names are pushed in one batch, in its order: `targets` (`SDL_EVENT_RENDER_TARGETS_RESET`), `reset` (`SDL_EVENT_RENDER_DEVICE_RESET`) and `lost` (`SDL_EVENT_RENDER_DEVICE_LOST`), which no software renderer sends. Each is logged as it is pushed (`Test hook: pushed the render event '<word>'`), the line check 95 reads to tell a run that reloads on purpose. A size `STREAMFLEX_TEST_PIXEL_SIZE`'s file took since that hook read it in the same pass is pushed first, so a size and a reset written together always arrive in one batch. Check 48 sets it.
- `STREAMFLEX_TEST_RENDERER_FAIL=<n>`: after a lost device, the first `n` attempts at a new renderer fail. Check 48 sets it.
- `STREAMFLEX_TEST_RENDERER_SKIP=<name>`: after a lost device, the new renderer's list leaves `<name>` out, as a GPU that no longer offers it would; the `Renderer:` line still words itself from the full list. Check 48 sets it.
- `STREAMFLEX_TEST_RESTART_SELF=<path>`: a restart (Linux) looks for the program at `<path>` in place of `/proc/self/exe`: a missing path makes it go by `argv[0]`, and a file that is not a program makes its `execv` fail after the teardown. Check 64 sets it.
- `STREAMFLEX_TEST_SLIDESHOW_HOLD=<file>`: the slideshow's loader thread waits before its read until `<file>` exists (a minute at most), as a slow disk would hold it, so a check can step the mode or quit while it loads and let it go when it has looked. Check 60 sets it.
- `STREAMFLEX_TEST_VSYNC_REFUSED[=off]`: the renderer reports the VSync it was asked for as refused; with `off`, it reports VSync kept on when it is not wanted. Check 42 sets it.
- `STREAMFLEX_TEST_WINDOWS_PLAN`: every window is planned as on Windows, so a Transparent background gets Windows' see-through window, one pixel row short of the screen, on the transparent list's renderers, and its recreation and its fallbacks run on Xvfb; what only Windows does around them (the taskbar, the foreground, the clear to nothing) stays Windows'. Checks 48 and 60 set it.

The hook build also logs how many paragraphs the settings screen measured while it was open, so a check can catch a note measured again in every frame. It logs each repeat of a held gamepad control (`Test hook: pad <label> repeated`), each control still held when polling resumes after a gap and timed again from then (`Test hook: pad <label> held through a gap in polling, timed from now`), and when settings have taken a held control for as long as its repeat waits, 500 ms in wall time (`Test hook: pad <label> held while settings took it (the capture)` or `(waiting to be let go)`), so a check that holds a pad control past its repeat waits for that line, not for a time of its own. It logs when the slideshow's fade and the screensaver's dim begin and end, by SDL's ticks (`Test hook: the slideshow's fade began at tick <t>, to take <ms> ms`, then `Test hook: the slideshow's fade ended at tick <t>, <ms> ms after it began`, and the same two lines for `the screensaver's dim`), so a check can time them. When the VSync watch finds VSync not delivered, it logs how far apart the run of early presents that found it came (`Test hook: the VSync watch's run of <n> early presents came <min> to <max> us apart, <mean> us on average`), so check 42 can tell that its full-size run's presents came between half and three quarters of the display's frame apart. It logs each change of the window's pixel size the launcher handles, with the size it found (`Test hook: a size event found the window <w>x<h> px`), so check 47 can wait for one that changes nothing, such as a size put back while an app is in front. It logs each rest of the settings' Mode row's cursor, with the Mode it rested on (`Test hook: the Mode row's cursor rested on <Mode>`), so check 60 can tell when the rest that recreates the window on Windows came, where on Linux no Mode changes the window. It logs each key hint's width beside the column's (`Test hook: the key hint is <w> px wide, in a column <c> px wide`), and the width of the value on the cursor's row beside the room it has (`Test hook: the cursor's row's value is <w> px wide, with <r> px to draw in`), so a check can tell text that fits from text cut with "…". It logs the first frame shown after a key (`Test hook: a frame was drawn after a key`; a key ignored while a lost device's new renderer is owed, `Key ignored: there is no renderer yet`, counts too, and its frame is the first drawn once there is one), and `run_keys` waits for that line after every key it presses (a key written `!KEY` is pressed with no wait, for a key whose frame can come only after a later step lets the launcher go, and the next key waits for that frame before its own), so a slow runner cannot handle two keys with no frame between them and skip a line only a drawn frame logs. The helpers checks share (`run_keys`, `run_keys_at` for a second X display of another size, `ran_clean`, `in_range`, `precedes`, `look`, `stop_run` and others) live in `run.sh`.

Each run prints one `PASS` or `FAIL` line per check, then `N failed`, and keeps each check's output and log, and the launcher's exit code, under `headless-out/<label>/` (which the harness leaves out of the source it builds). Locally a pass on one container takes about 50 minutes, the leak pass the longest, and the four passes through `run-shards.ps1` in 2 shards each, on 8 containers, about 25 to 30 minutes; scrollfail, its build included, takes about 6 seconds on 6 CPUs. On GitHub the passes run in 2 shards as well: for Debian and for Fedora, the plain pass and the leak pass each run as two Headless jobs, one per shard (`run.sh <label> K/2` or `run.sh <label> leaks K/2`), each keeping its console, and scrollfail runs in the job of each distro's plain shard 1, after the shard. On GitHub (run 37801014279) the plain shards took about 26 minutes and the leak shards about 27 to 30, all in parallel. A merge job for each distro and pass then runs `merge.py` over that pass's two consoles, and fails as it does locally. `build-and-test` fails unless every shard and every merge passed, or, on a master or tag push, all were skipped because the PR they came from passed them on the same files.

## Project Structure

```
src/                 Launcher core (launcher.c, layout.c, library.c, image.c, clock.c, chroma.c, util.c, utf8.c, debug.c, alloc.c, config_fields.c, derive.c, keycodes.c, winplan.c, vsyncwatch.c, videodriver.c, padpower.c) and the settings screen (settings_screen.c, settings.c, settings_pickers.c, settings_fonts.c, listpick.c, colorpick.c, fontlist.c, fontscan.c, bindings.c, browser.c, inidoc.c, config_save.c, fileio.c)
                     test_hooks.c: the harness's STREAMFLEX_TEST_FAIL hook, built into every build but compiled to no code unless STREAMFLEX_TEST_HOOKS is defined; the other hooks sit inline in clock.c, fontscan.c, image.c, launcher.c, settings_screen.c and platform/unix.c
src/platform/        Windows and Linux platform layers
src/external/        Vendored third-party sources (nanosvg): where each came from, and every local change, in its README
config/              Default config template, packaging and platform templates (PKGBUILD, .desktop, manifest, icon)
assets/              Icons and fonts
branding/icon/       Source for the app icon; regenerate with build-icon.ps1 (see its README)
branding/logo/       The full-resolution logo original; the docs banner is a downscale of it
branding/library/    Tools for the icon library: generator, brand importer, checks, gallery (see its README)
design/              Design specs and implementation plans (not published; docs/ is the site)
tests/               Unit tests (CTest); run them with ctest after building
tests/headless/      Headless checks: the launcher under Xvfb, driven by key presses, and beside it on a headless Weston for check 67 (see run.sh); CI runs them on Debian and Fedora
docs/                Documentation site (GitHub Pages / Jekyll)
```

Vendored code in `src/external/` is taken from the latest upstream and evaluated before it lands. A CodeQL alert in it is patched only where the patch changes nothing, and each patch carries a `// streamflex:` comment and is listed in [`src/external/README.md`](src/external/README.md), with the upstream commit, so the next update can diff our copy against it and re-apply them; that README also gives the update steps. Compiler warnings in vendored code are not patched but silenced. The launcher and `test_library_svg` include the folder as a system header, so GCC reports nothing from it and MSVC treats it as external (`/external:I` with `/external:W0`). Neither reaches C4702, which MSVC's back end raises in nanosvg's implementation, so `image.c` and `test_library_svg.c` also wrap the implementation's include in `#pragma warning(push, 0)` with `#pragma warning(disable: 4702)`. The headless harness does not count warnings from `src/external/` either.

## Branch Strategy

`master` is **PR-gated**. Direct pushes are blocked by a branch ruleset; every change goes branch → PR → required checks green → squash-merge.

**Required status checks** (all must be green before merge, and the branch must be up to date with `master`):
- `build-and-test` — the gate job in `build.yml`; passes only when the `Promote` job, the Windows, Debian, Raspberry Pi, and Arch Linux builds, the `Icon library` check, the `American spelling` check, the `Unit tests (ASan, UBSan)` job, the eight `Headless (<distro>, <pass> K/2)` shard jobs of the `headless` job and the four `Headless (<distro>, <pass>)` merges of the `headless_merge` job all succeed. On a push to `master` or a `v*` tag, the sanitized unit tests, the shards and the merges are skipped, and `build-and-test` accepts that, only when `Promote` found that the commit is a merged PR's squash commit with the same files as the PR's head, whose own `build-and-test` passed (`.github/scripts/promote-check.sh`). Every other run runs them.
- `CodeQL` — code scanning via CodeQL default setup (C/C++ and GitHub Actions). It is required as the single `CodeQL` result rather than the per-language `Analyze (...)` jobs, so PRs where those jobs don't run are not blocked forever.
- `Scorecard analysis` — OpenSSF supply-chain scoring from `scorecard.yml`.

**American spelling.** Every English word in the repository uses American spelling: code, comments, docs and commit messages. The `American spelling` job runs the central gate, [`bilbospocketses/american-spelling`](https://github.com/bilbospocketses/american-spelling), pinned by commit in `build.yml`, over the lines a PR adds and its commit messages, and fails on a British spelling it finds. Its exceptions live in that repository's `allow.txt`, never in a file here; a new one is a PR there, a new tag, then a bump of the pin.

**Merge method:** squash only. Rebase merges are disallowed because they skip GitHub's signature on the merged commit.

**Signed commits required.** Commits to `master` and `v*` tags must be signed.

**Workflow file edits:** every action in `.github/workflows/*.yml` must be pinned to a full commit SHA (not an annotated-tag object SHA) with a precise version comment such as `# v7.0.1`, never a bare `# v7`. The repository enforces SHA pinning and only allows an explicit list of third-party actions; a new third-party action has to be added to that allowlist before its workflow can run.

## Changelog

Record changes in `CHANGELOG.md`. The older `CHANGELOG` file holds the original project's release history up to v2.2 and is frozen; don't add to it.

## Releasing

Versions follow [Semantic Versioning](https://semver.org/) with three parts (`0.1.0`). The project stays below 1.0 until the major overhaul is in place.

1. In one PR, set `VERSION` in `CMakeLists.txt` to the new version, and rename `## [Unreleased]` in `CHANGELOG.md` to `## [x.y.z] - YYYY-MM-DD` with a fresh empty `## [Unreleased]` above it. The `Release` job's dry run on that PR checks that every package carries the new version and that the notes extract.
2. After it merges, tag the merge commit with a signed annotated tag and push it:
   ```bash
   git tag -s vx.y.z -m "vx.y.z"
   git push origin vx.y.z
   ```
3. The `Release` job builds all four packages, attests them, and publishes the GitHub release with the CHANGELOG section as its notes. It then dispatches the `Docs site` workflow, which rebuilds the download page from the new release. The docs site never carries a version by hand, so there is nothing to bump there.

`v*` tags can't be deleted or moved, so a failed release can't be retried under the same number: fix the problem and release the next patch version.

## Commit Messages

Use conventional-commit-style prefixes: `feat:`, `fix:`, `refactor:`, `docs:`, `style:`, `chore:`, `build:`, `ci:`, `test:`. Keep the subject short and imperative, wrap the body at 72 columns, and reference issue numbers when applicable.

## Pull Requests

- Keep PRs focused on one concern.
- Update `CHANGELOG.md` under `[Unreleased]` for any user-visible change.
- Update the relevant page in `docs/` when user-facing behavior or configuration changes.

## Smoke Tests

`tests/qa/smoke-test.md` is the hands-on check every release runs, one observable claim per row, and `tests/qa/automation-coverage.md` says which rows the unit tests and the headless harness already prove. A PR that changes code a user can see (`src/`, `assets/`, `config/`, `CMakeLists.txt`, `vcpkg.json`) updates the rows that cover the change, or says why it needs none in its body:

    <!-- smoke: none -- <why this change has no smoke impact> -->

A marker with no reason does not count. The `smoke-coverage` check enforces both, and editing the PR body re-runs it.

Before pushing a change to the doc, the register or a results file, run:

    python tests/qa/check-smoke.py lint

Row ids are never renumbered or reused: retire a row by moving its id to the doc's **Retired ids** list. A row's tag is part of its claim, so change it by retiring the row and adding a new one. After adding or moving register lines, `lint --write` regenerates the summary.

## Reporting Bugs

Open an issue with:

- Expected vs actual behavior
- OS and version (Windows 10/11, Linux distro, Raspberry Pi OS)
- Your `config.ini` (or the relevant section of it)
- Output from running with debug logging enabled (see the README's Debugging section)

## Reporting Security Issues

Do **not** file a public issue. See [`SECURITY.md`](SECURITY.md) for the private reporting flow.

## License

The project is released under the [GNU General Public License v3.0](LICENSE). By contributing you agree to license your contributions under the same terms.
