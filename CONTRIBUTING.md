# Contributing to StreamFlex

StreamFlex is a customizable application launcher and desktop replacement for Windows and Linux, written in C with SDL2.

This repository is an independent project. It started from complexlogic's Flex Launcher at v2.2 and has been developed separately since; it does not track, sync with, or send changes back to that project. Changes land here through the process below.

## Building

Build instructions for Windows (Visual Studio + vcpkg) and Linux (CMake + distro packages) are in [`docs/compilation.md`](docs/compilation.md). The CI workflow in [`.github/workflows/build.yml`](.github/workflows/build.yml) is the authoritative, always-exercised recipe for all four targets: Windows, Debian, Raspberry Pi, and Arch Linux. After building, run the unit tests with `ctest --test-dir build -C Release --output-on-failure`. The icon library's tooling has Python tests of its own, and its README says how to run them and `check-library.py`; see [`branding/library/README.md`](branding/library/README.md).

The unit tests reach failures no real run gives through two seams, both for tests only. The pure modules (fileio, inidoc, config_save, settings, browser, listpick, fontlist and bindings) allocate through `alloc.h`, and `alloc_set_hooks()` puts a test's allocator in its place, which `test_alloc` uses to make every allocation fail in turn. The pure modules derive and colourpick allocate nothing. Each pure module builds without SDL and has its own unit test (`test_derive`, `test_listpick`, `test_colourpick`, `test_fontlist` and `test_bindings` among them). `fileio_set_fault()` makes one fileio step fail: `FILEIO_FAULT_LIST_READ` fails a listing's read after a given number of entries with a given error, `FILEIO_FAULT_KEEP` makes a replace unable to keep the old file's permissions or attributes, and `FILEIO_FAULT_NO_KIND` (Linux) makes a listing give no entry's kind, as some file systems give none. On Linux, `fileio_set_mount_table()` points fileio at a pretend `/proc/self/mounts`.

The headless checks run in Docker, as the `Headless (Debian)` and `Headless (Fedora)` jobs in `build.yml` do: build an image from `tests/headless`, then run `run.sh` in it with the repository mounted read-only at `/src` and a folder for the results at `/out`, with the seccomp profile off (the harness turns address randomization off with `setarch -R`, which ASan needs and Docker's default profile refuses):

    docker build -t streamflex-test tests/headless
    docker run --rm --security-opt seccomp=unconfined -v "$PWD:/src:ro" -v "$PWD/headless-out:/out" streamflex-test bash /src/tests/headless/run.sh local

There are two images and one `run.sh`. `Dockerfile` is Debian with SDL2 itself; `Dockerfile.fedora` is Fedora 44, whose SDL2 is sdl2-compat over SDL3 and whose Mesa has llvmpipe but no softpipe. What differs between them is detected, never branched on by name, and `run.sh` prints the Mesa driver it found (`Mesa: <driver>, from <path>`). For the Fedora pass, build its image and give its tag to the same `docker run`:

    docker build -t streamflex-test-fedora -f tests/headless/Dockerfile.fedora tests/headless

A second argument to `run.sh` picks the mode:

- `run.sh <label>` runs every check in `checks/`, in name order, starting with `00-harness.sh`, which checks the harness's own helpers. The launcher is built with ASan and UBSan, and a compiler warning outside `src/external/` fails the run. Some checks also read the screen: `xwd` takes a screenshot and `pixels.py` reads chosen points, such as the settings preview's colour, image and Transparent checkerboard. The checks press keys with `xdotool`, except the function keys: `run.sh`'s `xdotool` function sends those through `key.py`, by keycode with no modifier, because Fedora's keymap makes `xdotool` hold Alt down for every F key, which a key capture would catch.
- `run.sh <label> scrollfail` runs item 11 only, with the scroll arrow's texture forced to fail, a path no config or input can reach.
- `run.sh <label> leaks` runs every check again with LeakSanitizer on. Mesa's software driver is preloaded into the launcher so the blocks it holds at exit stay reachable, and there is no suppressions file: a run that leaks fails its check, and each leak is also listed at the end as a `LEAK` line that counts in `N failed`.
  - On Fedora, SDL3 (under sdl2-compat) leaks its X11 display data when StreamFlex stops on a fatal error before any window exists, and the leak pass counts that as a leak. A check of a fatal stop must therefore stop after the window opens: check 59's run of a pipe as a bundled font uses the clock's, which opens after the window, for that reason.
  - LeakSanitizer sometimes cannot start its tracer: the run's stderr says `WARNING: ptrace appears to be blocked (is seccomp enabled?). LeakSanitizer may hang.` and `Child exited with signal 96.`, and no leak is listed. `run.sh` takes any `LeakSanitizer` line in a run's stderr as a sanitizer's report, so that run's check fails although nothing leaked. This is a known flake, not a leak: run the set again.

A pass of about 260 checks takes about 50 minutes on one container, mostly waiting on the launcher, so it can also run in shards, each a run of its own in a container of its own. `run.sh <label> K/N` (or `run.sh <label> leaks K/N`) runs shard K of N. The build, Xvfb, the fixtures and the check files with a `# Every shard runs this file` line run in every shard: `00-harness.sh`, whose helpers each shard relies on, and `90-titles-fit.sh`, which reads the logs of its own shard's runs. Each other check file runs in exactly one shard, planned from the time it took (the table in `run.sh`; a file not in it counts as 60 seconds) so the shards take about as long. A shard's console says which check file each result belongs to (its `SHARD` lines), and a shard alone is not the pass: `merge.py` adds a pass's shard consoles up into its one result, printing every result line, the `PASS`, `FAIL` and `LEAK` counts and `N failed`. It also fails, with an `INCOMPLETE` line for each problem and exit code 2, unless the consoles are one complete run: shards 1 to N of one tree, each present once and run to its end, every check file the shards declared run in exactly one of them, and no case in two. Run by hand, each shard's console is saved for the merge by redirecting it, into a folder made first:

    mkdir -p headless-out
    docker run --rm --security-opt seccomp=unconfined -v "$PWD:/src:ro" -v "$PWD/headless-out:/out" streamflex-test bash /src/tests/headless/run.sh local 1/2 > headless-out/local-s1.console.log
    docker run --rm --security-opt seccomp=unconfined -v "$PWD:/src:ro" -v "$PWD/headless-out:/out" streamflex-test bash /src/tests/headless/run.sh local 2/2 > headless-out/local-s2.console.log
    python tests/headless/merge.py --leg local headless-out/local-s1.console.log headless-out/local-s2.console.log

A check's `PASS` or `FAIL` text must not repeat one in another check file: the merge reads the same text from two shards as one case run twice, and fails the pass.

On Windows, `tests/headless/run-shards.ps1` runs a set of passes this way. It builds the images, then starts the Debian, Fedora and both leak passes (`-Legs plain,fedora,leaks,fedora-leaks` picks some) in N shards each (`-Shards`, 2 by default) as containers named `sfdev-<label>-s<K>`, each on 3 CPUs of its own with `-m 2g --memory-swap 2g`, at most 8 at once and only while 8 GB of host RAM is free. It saves each shard's console as `<label>-s<K>.console.log`, merges each pass into `<label>.merged.log`, and prints a line for each pass and one for the set, exiting non-zero unless every pass is complete with `0 failed`. In 2 shards, the four passes run on 8 containers in about half the time one container per pass takes:

    pwsh tests/headless/run-shards.ps1 -Prefix local

The harness builds with `-DSTREAMFLEX_TEST_HOOKS`, which nothing else defines. It adds test-only hooks, environment variables a check sets to force what no config or input can reach; normal builds contain none of them:

- `STREAMFLEX_TEST_CLOCK_DELAY_MS=<ms>`: each clock render on the clock's thread waits that long first, and the clock renders every second and logs each render it starts on its thread, so a check can act while one is in flight. Checks 42 and 55 set it.
- `STREAMFLEX_TEST_CLOCK_THREAD_FAIL`: the clock's render thread fails to start, so each render runs on the main thread instead. Check 42 sets it.
- `STREAMFLEX_TEST_DECODE_DELAY_MS=<ms>`: the settings preview waits that long before decoding an image, so a check can press a key during the decode. Check 60 sets it.
- `STREAMFLEX_TEST_FAIL=<step>`: that step runs as if memory had run out: the settings screen's `places`, `browser`, `command` and `bindings`; the pickers' `list`, `rows`, `select`, `pads` and `apply`; the font picker's `fontlist`, `fontscan`, `fontfolder`, `faces` and `sample`; and `restart`, the copy of the arguments a restart starts with, made as StreamFlex starts. With `keep` the saved file's permissions cannot be kept, with `fontthread` the font list's thread does not start, and with `fontadd` that thread cannot add a file. Check 65 sets `places`, `browser`, `command` and `keep`; check 62 `bindings`, `list` and `apply`; check 58 `list`, `rows`, `select` and `pads`; check 59 `list`, `rows`, `select` and the font picker's steps; check 64 `restart`.
- `STREAMFLEX_TEST_FAIL_SHRINK_STEP`: every step down in Shrink mode fails to open its font. Check 40 sets it.
- `STREAMFLEX_TEST_FAIL_TITLE_SIZE=<pt>`: the title font fails to open at that size. Check 40 sets it.
- `STREAMFLEX_TEST_FONT_DELAY_MS=<ms>`: the font list's thread waits that long after reading the bundled folder, as a slow disk would, so a check can act while the list loads. Check 59 sets it.
- `STREAMFLEX_TEST_FONT_DIRS=<dir>[:<dir>...]`: the font list reads these folders, after the bundled one, in place of the system's and the user's. Check 59 sets it.
- `STREAMFLEX_TEST_FRAME_REPORT_MS=<ms>`: once that long has passed, the log says how many frames were shown and how many of them waited out the FPS limit. Check 42 sets it.
- `STREAMFLEX_TEST_NO_LUMINANCE`: an image's brightness cannot be measured, as when its conversion fails, so the contrast warning has no reading for it. Check 58 sets it.
- `STREAMFLEX_TEST_NO_MESSAGE_BOX`: a fatal error quits without its message box, which some SDLs show and wait on. Checks 25 and 59 set it.
- `STREAMFLEX_TEST_NO_RENDER_TARGETS`: settings draw as they would on a renderer without render targets. Checks 55 and 60 set it.
- `STREAMFLEX_TEST_PAD=<file>`: attaches a virtual gamepad, since Xvfb has none, and holds its Start button while `<file>` exists. It does nothing when the gamepad is turned off. Checks 42, 50, 55, 58 and 62 set it.
- `STREAMFLEX_TEST_PAD_BUTTON=<name>`: the virtual gamepad holds this button, by SDL's name for it (`b`), instead of Start; an axis's name (`rightx`) pushes that axis to its positive end. Check 62 sets it.
- `STREAMFLEX_TEST_PAD_FRAMES=<n>`: each time the virtual gamepad's file appears, its button is held for `n` frames however long the file stays: a tap shorter than a script can time. Check 62 sets it.
- `STREAMFLEX_TEST_PAD_PLUG=<file>`: another virtual gamepad is plugged in while `<file>` exists and pulled out when it goes, as a pad is plugged in or out by hand at any moment. Checks 55, 58 and 62 set it.
- `STREAMFLEX_TEST_PAD_SWAP`: virtual gamepads are attached and detached, one step a frame, so one arrives at another's old device index with an instance id of its own; then a launch and a return close every pad and open each again. Check 42 sets it.
- `STREAMFLEX_TEST_RELOAD_FONTS`: the title and clock fonts are opened a second time as they start, as a reload does, so the leak pass shows whether the font each replaces is closed. Check 41 sets it.
- `STREAMFLEX_TEST_RESTART_SELF=<path>`: a restart (Linux) looks for the program at `<path>` in place of `/proc/self/exe`: a missing path makes it go by `argv[0]`, and a file that is not a program makes its `execv` fail after the teardown. Check 64 sets it.
- `STREAMFLEX_TEST_SLIDESHOW_HOLD=<file>`: the slideshow's loader thread waits before its read until `<file>` exists (a minute at most), as a slow disk would hold it, so a check can step the mode or quit while it loads and let it go when it has looked. Check 60 sets it.
- `STREAMFLEX_TEST_VSYNC_REFUSED[=off]`: the renderer reports the VSync it was asked for as refused; with `off`, it reports VSync kept on when it is not wanted. Check 42 sets it.

The hook build also logs how many paragraphs the settings screen measured while it was open, so a check can catch a note measured again in every frame. The helpers checks share (`run_keys`, `ran_clean`, `in_range`, `precedes`, `look`, `stop_run` and others) live in `run.sh`.

Each run prints one `PASS` or `FAIL` line per check, then `N failed`, and keeps each check's output and log, and the launcher's exit code, under `headless-out/<label>/` (which the harness leaves out of the source it builds). Locally a pass on one container takes about 50 minutes, the leak pass the longest, and the four passes through `run-shards.ps1` in 2 shards each, on 8 containers, about 25 to 30 minutes; scrollfail, its build included, takes about 6 seconds on 6 CPUs. On GitHub each Headless job, which runs all three, takes about 16 minutes.

## Project Structure

```
src/                 Launcher core (launcher.c, layout.c, library.c, image.c, clock.c, chroma.c, util.c, utf8.c, debug.c, alloc.c, config_fields.c, derive.c) and the settings screen (settings_screen.c, settings.c, settings_pickers.c, settings_fonts.c, listpick.c, colourpick.c, fontlist.c, fontscan.c, bindings.c, browser.c, inidoc.c, config_save.c, fileio.c)
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
tests/headless/      Headless checks: the launcher under Xvfb, driven by key presses (see run.sh); CI runs them on Debian and Fedora
docs/                Documentation site (GitHub Pages / Jekyll)
```

Vendored code in `src/external/` is taken from the latest upstream and evaluated before it lands. A CodeQL alert in it is patched only where the patch changes nothing, and each patch carries a `// streamflex:` comment and is listed in [`src/external/README.md`](src/external/README.md), with the upstream commit, so the next update can diff our copy against it and re-apply them; that README also gives the update steps. Compiler warnings in vendored code are not patched but silenced. The launcher and `test_library_svg` include the folder as a system header, so GCC reports nothing from it and MSVC treats it as external (`/external:I` with `/external:W0`). Neither reaches C4702, which MSVC's back end raises in nanosvg's implementation, so `image.c` and `test_library_svg.c` also wrap the implementation's include in `#pragma warning(push, 0)` with `#pragma warning(disable: 4702)`. The headless harness does not count warnings from `src/external/` either.

## Branch Strategy

`master` is **PR-gated**. Direct pushes are blocked by a branch ruleset; every change goes branch → PR → required checks green → squash-merge.

**Required status checks** (all must be green before merge, and the branch must be up to date with `master`):
- `build-and-test` — the gate job in `build.yml`; passes only when the Windows, Debian, Raspberry Pi, and Arch Linux builds, the `Icon library` check and the `Headless (Debian)` and `Headless (Fedora)` checks (two legs of the `headless` job) all succeed.
- `CodeQL` — code scanning via CodeQL default setup (C/C++ and GitHub Actions). It is required as the single `CodeQL` result rather than the per-language `Analyze (...)` jobs, so PRs where those jobs don't run are not blocked forever.
- `Scorecard analysis` — OpenSSF supply-chain scoring from `scorecard.yml`.

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
- Update the relevant page in `docs/` when user-facing behaviour or configuration changes.

## Reporting Bugs

Open an issue with:

- Expected vs actual behaviour
- OS and version (Windows 10/11, Linux distro, Raspberry Pi OS)
- Your `config.ini` (or the relevant section of it)
- Output from running with debug logging enabled (see the README's Debugging section)

## Reporting Security Issues

Do **not** file a public issue. See [`SECURITY.md`](SECURITY.md) for the private reporting flow.

## License

The project is released under the [GNU General Public License v3.0](LICENSE). By contributing you agree to license your contributions under the same terms.
