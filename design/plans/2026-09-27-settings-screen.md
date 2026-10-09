# In-app settings screen (3a) — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A settings screen inside StreamFlex, driven by a remote alone, that changes the background, each menu's grid and the title size live, and saves only the changed lines back into the user's own `config.ini`.

**Architecture:**
- Four new pure modules (no SDL, unit-tested like `layout.c`):
  - `fileio.c`: UTF-8 file access on every platform, and the safe replace;
  - `inidoc.c`: `config.ini` as editable lines, read the way inih reads it;
  - `settings.c`: the settings table, value conversions and the page model;
  - `browser.c`: the folder browser's model.

  A fifth, `config_save.c`, joins `inidoc` and `fileio` into the save.
- `layout.c` learns to size titles with the button.
- `settings_screen.c` is the SDL side. It draws a narrow settings column and a live preview. The preview is the launcher's own scene drawn into a target texture.
- `launcher.c` is refactored so the background can be reloaded at any time, and so the scene can be drawn into that texture.
- A headless harness (Xvfb, `xdotool`, ASan, UBSan) moves into the repo and CI. It drives the screen with key presses and checks the saved file.

**Tech Stack:** C (C99, as the codebase is), SDL2 (floor 2.0.14), SDL2_image (2.0.5), SDL2_ttf (2.0.15), inih, CMake 3.18+ and CTest, Docker (headless checks), GitHub Actions.

**Spec:** `design/specs/2026-09-27-settings-screen-design.md`. Read it first; this plan implements it.

## Spec corrections found while planning

Each of these was a wrong or loose assumption about the code. The plan follows the right-hand side.

1. **The title padding key is `[Titles] Padding`, not `TitlePadding`** (`SETTING_TITLE_PADDING` is `"Padding"`). Wherever the spec says `TitlePadding`, read `Padding`.
2. **inih reads at most 199 bytes of a line.** It reads into a 200-byte buffer (`INI_MAX_LINE`), and splits a longer line in two. So "values it refuses" includes any line longer than 199 bytes (`INIDOC_MAX_LINE`). A deep path can hit that limit: the browser shows such a path and says why it cannot be chosen.
3. **inih treats an indented line after a key as the key's continuation** (`INI_ALLOW_MULTILINE`), not as a new key, and keeps doing so across blank, comment and unreadable lines until the next key or section header. Each continuation is another handler call, so the last one is the value the launcher keeps. `inidoc` follows suit: changing a key replaces its continuation lines along with its value, removing a key removes them too, and a new key is never placed where it would gain a continuation (such an insert is refused, leaving the file unchanged). *(Amended during Task 3's review: the plan first said continuation lines were left as they are, which let an edit read back correctly while the launcher read something else.)*
4. **`OversizeMode=Truncate` has never parsed.**
   - The parser's name is `Truncated` (`mode_settings` in `src/util.c`), while the docs and the spec say `Truncate`.
   - The sample config ships `OversizeMode=Shrink`, which gives one menu several title sizes. That is exactly what the approved title rule ("every title in a menu is the same size") replaces.
   - So the parser accepts both names, and the sample config ships `Truncate`.
   - This changes the sample config. The user approved it on 2026-09-28.
5. **The exact-integer title fit is a binary search** over the button size, not a closed form followed by pixel steps. It gives the same largest fitting button, is simpler, and is exact by construction.
6. **A value on entry stays in its row's steps for the whole visit.** The spec says a custom colour or fixed title size appears "until the user steps off it". Keeping it reachable means the user can always step back to it without Discard, and the rule is the same for every type.
7. **The title font cache is bounded (16 sizes) and evicts the oldest,** instead of pruning when settings close. Memory stays bounded either way, and nothing depends on which menus happen to be loaded.
8. **`init_slideshow()` overwrote `config.background_image`** when a slideshow folder held one image. With the new `background_shown` (what is on screen, as opposed to what the config chose), no fallback rewrites a setting any more. The settings screen needs that: it must show the user's choice, not the fallback.
9. **`make_window_transparent()` had no prototype.** `platform.h` declares `set_window_transparent()`, which does not exist, so `launcher.c` has been calling an undeclared function. `platform.h` now declares `make_window_transparent()` and the new `make_window_opaque()`.
10. **Root ignores file permissions,** and both the CI Debian job and the Docker harness run as root. So the harness runs the launcher as an unprivileged user (`tester`). The unit tests skip their read-only checks when run as root, and say so.
11. **Scorecard checks Dockerfiles.** The harness image's `FROM` is pinned by digest, and Dependabot gains a `docker` entry for `tests/headless`.
12. **Grid summaries read columns × rows** (`6 × 3`), matching the launcher's log (`6 x 3 grid`), not the mockup's `3 × 6`.
13. **The preview keeps the screen's own shape,** not a fixed 16:9. The preview is the whole screen scaled down, so a 16:10 or ultra-wide display would be distorted if it were forced into 16:9. On a 16:9 screen the two are the same.
14. **The caption says "(reduced to fit the screen)"** when `layout_compute()` shrank a grid, instead of naming the new column count. The grid it shows is already the reduced one (`9 × 3`), so the count would say the same thing twice.
15. **A menu's own page has a fourth row, a note** (`MENU_NOTE`: "The lowest step, All menus, follows the shared grid."), below the spec's three. It is text only and the cursor skips it, so the three setting rows behave exactly as the spec says. It explains the step the spec gives each row, which otherwise reads as a value.

## Existing bugs fixed on the way

Planning turned up three bugs in code this branch works next to. The user chose, on 2026-09-28, to fix them in this branch rather than file them. Writing the fixes turned up two more in the same functions, a hang and a crash, and they are fixed with them. Each fix lives in the task that already touches its code, with a harness check that fails before it and passes after, except the first, which only Windows can show.

| Bug | What happens today | Fixed in |
|---|---|---|
| `start_process()` (`src/platform/win32.c`) launches through `ShellExecuteExA` | On Windows, a command or file path with a character outside the system code page fails to launch. Task 2 moves every other path to UTF-8. | Task 2, Step 7 |
| `load_next_slideshow_background()` (`src/image.c`) falls back on the slideshow thread | When a running slideshow's folder has one loadable image left, the loader thread creates a texture (SDL textures belong to the main thread). When it has none, the thread frees the slideshow and then writes into it, a use-after-free. | Task 9, Steps 10-14 |
| The same function's loop | At startup, a slideshow folder whose files all fail to load (two files that only look like pictures, say) hangs the launcher forever: the loop's stop test waits for an index of -1 it never reaches, and its attempt counter only counts converted images. | Task 9, Steps 10-14 |
| `config_handler()` (`src/util.c`) keeps its menu cursor on the last menu read | When a menu's section appears a second time after another menu, its later entries and grid keys land in the menu between. | Task 1, Steps 15-19 |
| The same function's entry parsing | An empty entry (`Entry2=`) in a menu that already has an entry frees the entry before it and then dereferences NULL, so the launcher crashes at startup. | Task 1, Steps 15-19 |

Three smaller faults in the same functions are fixed with them: `start_process()` never closed the process handle it asked for, a refused entry leaked its strings, and the slideshow's alpha conversion used a surface without checking that it was created.

## Global Constraints

- **Every repo command uses an absolute path:** `git -C C:/Users/jscha/source/repos/streamflex ...`, and files under `C:/Users/jscha/source/repos/streamflex/`. Never `cd` into the repo.
- **Branch:** work on `feat/settings-screen`, cut only after the PR carrying this spec and plan has merged:

  ```
  pwsh C:/Users/jscha/.claude/scripts/git-new-branch.ps1 -Repo C:/Users/jscha/source/repos/streamflex -Branch feat/settings-screen
  ```

  Stage named files only; never `git add -A`. Commit messages are conventional (`feat:`, `fix:`, `test:`, `docs:`, `ci:`) with no AI attribution. The PR is squash-merged.
- **The pure modules must not include SDL, `launcher.h` or any SDL-using header:** `fileio.c`, `inidoc.c`, `config_save.c`, `settings.c`, `browser.c` and `layout.c`. They may include `<launcher_config.h>` (macros only). Each test executable links them without SDL.
- **Use no SDL API newer than 2.0.14,** no SDL_image API newer than 2.0.5, and no SDL_ttf API newer than 2.0.15. In particular, no `TTF_SetFontSize()` (2.0.18): every font size is its own `TTF_OpenFont()`. `SDL_RenderTargetSupported`, `SDL_SetRenderTarget`, `SDL_AtomicGet`/`SDL_AtomicSet` and `TTF_RenderUTF8_Blended_Wrapped` are all older.
- **Keep the codebase's style:**
  - four-space indents and braces on the same line;
  - one comment line above each function, in the existing voice ("// A function to ...");
  - explicit casts where `int` and `unsigned int` mix, so `-DEXTRA_WARNINGS=ON` gains no new warnings;
  - `snprintf`, never `sprintf` or `strcpy` into fixed buffers.
- **Every local Windows build command starts with the CMake PATH line,** because shell state does not persist between tool calls:

  ```powershell
  $env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
  ```

  - **Build:** `cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release`.
  - **Tests:** `ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure`.
  - **Check the output for `100% tests passed`, not only the exit code.** A missing PATH line makes `ctest` a CommandNotFound, which sets no exit code.
- **The headless harness** needs Docker Desktop running. It never needs the user's display. Run it with:

  ```powershell
  docker build -t streamflex-test C:/Users/jscha/source/repos/streamflex/tests/headless
  docker run --rm --security-opt seccomp=unconfined -v C:/Users/jscha/source/repos/streamflex:/src:ro -v C:/Users/jscha/ClaudeScratch/streamflex-headless-out:/out streamflex-test bash /src/tests/headless/run.sh local
  ```

  - It prints one `PASS` or `FAIL` line per check, then `N failed`, and exits non-zero when anything failed.
  - Each check's config, log, stdout and stderr are kept in `C:/Users/jscha/ClaudeScratch/streamflex-headless-out/local/`.
  - A run takes about a minute, plus the image build the first time.
- **Never launch a fixture on the user's desktop.** Everything a running launcher must prove goes through the headless harness. The one exception is the hands-on step in Task 13, which runs in the qa-harness project's Windows guest.
- **Exact values from the spec:**
  - `FontSize` presets: Small `11%`, Medium `14%`, Large `17%`.
  - The built-in defaults become `FontSize=14%` and `Padding=8%`.
  - The readable minimum is 2% of the screen height (22 at 1080p).
  - Rows 1-10; Columns 1-12.
  - *Largest button* steps: *Fill*, then 64, 96, 128, 160, 192, 256, 320, 384, 512, 768 and 1024 px.
  - *Change every* steps: 5, 10, 15 and 30 s, then 1, 2, 5, 10, 30 and 60 min.
  - *Fade* steps: 0 to 3 s in 0.5 s steps.
  - **Colour presets:** Black `#000000`, Charcoal `#1E1E1E`, Graphite `#33383D`, Slate `#2E3440`, Midnight `#121A2E`, Navy `#0B1F3A`, Teal `#07606C`, Forest `#1E3B2F`, Plum `#3B1F3A`, Burgundy `#4A1520`.
  - The settings column is 20% to 32% of the screen width.
  - The Windows replace retries for about a second: 10 attempts, 100 ms apart.
- **The menu key** is `SDLK_APPLICATION`: `xdotool key Menu` sends it, and so do Windows' `VK_APPS` and most HTPC remotes' Menu button.

## Review Focus

These five input classes are the most likely to bite someone using this, and nothing else in the plan exercises them. Each line names its test and the task that adds it.

1. **A deep path that does not fit on one config line.** A Pictures folder three network shares deep easily makes `Image=<path>` longer than inih's 199 bytes. Such a line would be silently misread on the next start. The browser must show that image or folder, refuse it and say why, and `inidoc_set` must refuse the line.
   - Task 3: `test_inidoc` checks lines of exactly 199 and 200 bytes.
   - Task 8: `test_browser` checks that a long path's rows are disabled with the reason.
2. **`config.ini` deleted, renamed or replaced on disk while settings are open** (over SSH, or by a sync tool). The save re-reads the file. It must keep any other line that changed meanwhile, and a vanished file must turn into the failure rows, never a crash or an empty config.
   - Task 4: `test_config_save`, *a hand edit made meanwhile survives*, and *a missing file fails with a reason*.
3. **A menu with no valid entries in the Menus list.** The preview cannot load it. The screen must keep previewing the menu it came from, still let the user change that menu's grid, and save it.
   - Task 10: harness fixture `f50-empty`.
4. **Opening settings or switching the background mode while a slideshow is mid-transition,** with its loader thread running. `reload_background()` must wait for the thread and free the half-finished transition, never touch freed memory.
   - Task 11: harness check *stepping the mode through a running slideshow*, whose timing lands the key presses inside the first transition.
5. **Titles turned off while `FontSize` is a percentage.** There is no title block, the highlight is only as tall as the button, and nothing divides by a font that was never used.
   - Task 5: `test_layout` checks titles off.
   - Task 7: harness fixture `f40-notitles`.

## File structure

| File | Status | Responsibility |
|---|---|---|
| `tests/headless/Dockerfile` | new | The harness image: CI's Debian build dependencies, Xvfb, xdotool, Mesa, python3, a `tester` user |
| `tests/headless/run.sh` | new | Builds with ASan and UBSan, starts Xvfb, provides the run helpers, sources `checks/*.sh`, exits non-zero on any failure |
| `tests/headless/checks/*.sh` | new | One file per area: grid items (10), refresh rate (20), menu parsing (25), backgrounds (30), titles (40), settings (50, 60) |
| `tests/headless/fixtures/*.ini` | new | The fixture configs |
| `tests/headless/make_images.py` | new | Writes small solid-colour PNGs for the background checks |
| `.gitattributes` | new | Keeps the harness scripts LF on a Windows checkout |
| `src/fileio.h`, `src/fileio.c` | new | UTF-8 file access on every platform, the safe replace, folder listing, places; a UTF-16 copy for other Windows calls |
| `src/inidoc.h`, `src/inidoc.c` | new | `config.ini` as editable lines |
| `src/config_save.h`, `src/config_save.c` | new | The save: fresh read, edits, backup, temp file, replace, the Linux fallback |
| `src/settings.h`, `src/settings.c` | new | The settings table, parse/format/step/describe, and the page model |
| `src/browser.h`, `src/browser.c` | new | The folder browser's model |
| `src/settings_screen.h`, `src/settings_screen.c` | new | The SDL settings screen |
| `src/layout.h`, `src/layout.c` | modify | Title sizing with the button; title size and padding parsers |
| `src/launcher.h`, `src/launcher.c` | modify | Item 22; the reloadable background; the slideshow's fall-back on the main thread; the scene/present split; config path; `:settings`; routing; title fields |
| `src/util.c`, `src/util.h` | modify | Menu sections read twice; empty entries; table keys parsed through `settings.c`; `Truncate`; percentage `FontSize`/`Padding`; defaults for Start |
| `src/image.h`, `src/image.c` | modify | The slideshow loader touches only the slideshow; title font cache; Shrink with a floor and no leak; the default font's path |
| `src/debug.c` | modify | Log file through `fileio`; title settings and sizes in the debug output |
| `src/library.c` | modify | Manifest and probe through `fileio` |
| `src/platform/platform.h`, `win32.c` | modify | `make_window_opaque`; UTF-8 existence checks, slideshow scan and launching |
| `src/CMakeLists.txt` | modify | New sources; `shell32`/`ole32` on Windows |
| `tests/CMakeLists.txt` | modify | `test_fileio`, `test_inidoc`, `test_config_save`, `test_settings`, `test_browser`; `test_library` links `fileio.c` |
| `tests/test_*.c` | new/modify | The unit tests named above; `test_layout.c` gains title sizing |
| `config/config_settings.cmake`, `config/launcher_config.h.in`, `config/config.ini.in` | modify | New defaults; the sample config's titles and Settings tile |
| `.github/workflows/build.yml`, `.github/dependabot.yml` | modify | The headless job, required by `build-and-test`; Docker digest updates |
| `docs/configuration.md`, `CHANGELOG.md`, `CONTRIBUTING.md` | modify | Documentation |

---

### Task 1: The headless harness in the repo and in CI, the 0 Hz refresh rate (item 22), and two menu parsing bugs

The harness exists today only in `C:/Users/jscha/ClaudeScratch/streamflex-docker/` (scratch), where it patches its copy of the source so that Xvfb's 0 Hz refresh rate does not crash the launcher. This task moves it into the repo **without** that patch. The first run is therefore RED on item 22. The fix makes it GREEN, and CI then runs the harness on every PR.

With the harness in place, Steps 15-19 fix two bugs in how `config_handler()` reads menu sections (see **Existing bugs fixed on the way**), each pinned by a check that fails first. They are a separate commit.

**Files:**
- Create: `tests/headless/Dockerfile`, `tests/headless/run.sh`, `tests/headless/checks/10-grid.sh`, `tests/headless/checks/20-refresh.sh`, `tests/headless/checks/25-menus.sh`
- Create: `tests/headless/fixtures/f11-scroll.ini`, `f12-padding.ini`, `f13-selfsub.ini`, `f14-junk.ini`, `f15-limits.ini`, `f16-debuglayout.ini`, `f17-once.ini`, `f22-refresh.ini`, `f25-split.ini`, `f25-empty.ini`
- Create: `.gitattributes`
- Modify: `src/launcher.h` (a constant), `src/launcher.c:215-218` (`init_sdl`)
- Modify: `src/util.c:417-487` (`config_handler()`'s menu branch)
- Modify: `.github/workflows/build.yml` (new `headless` job; `build-and-test` needs it), `.github/dependabot.yml`

**Interfaces:**
- Produces for later tasks:
  - **Helpers in `run.sh`:**
    - `run_quick NAME` and `run_keys NAME KEY...` run `$FX/NAME.ini`, or the file named by `CFG` when it is set. `CFG=none` runs with no `-c`.
    - `sanitizer_clean NAME`, and `result "description" STATUS` (0 = pass).
  - **Variables:** `$FX` (fixtures), `$out` (this run's output folder), `$LOG` (the launcher's log), `$TESTER_HOME` (`/home/tester`), `$exe`.
  - **Output files:** each run leaves `$out/NAME.{out,err,code,log}`.
  - **Adding checks:** any later check file dropped into `tests/headless/checks/` runs in name order.

- [ ] **Step 1: Create `.gitattributes`** so the scripts keep LF endings on a Windows checkout (bash refuses `\r`):

```gitattributes
# The headless harness runs in Linux; its scripts must keep LF line endings on a Windows checkout
tests/headless/*.sh text eol=lf
tests/headless/checks/*.sh text eol=lf
tests/headless/*.py text eol=lf
tests/headless/Dockerfile text eol=lf
```

- [ ] **Step 2: Resolve the Debian image's digest,** for the pinned `FROM` (Scorecard flags an unpinned base image):

```powershell
docker pull debian:bookworm
docker inspect --format '{{index .RepoDigests 0}}' debian:bookworm
```

Expected: one line, `debian@sha256:<64 hex digits>`. Use that digest in Step 3.

- [ ] **Step 3: Create `tests/headless/Dockerfile`** (put the real digest from Step 2 in place of `<digest>`):

```dockerfile
# The StreamFlex headless test image: CI's Debian build dependencies, plus a virtual X display
# (Xvfb) with Mesa's software OpenGL so the accelerated renderer works without a screen, and
# xdotool to press keys. The launcher runs as `tester`, because root ignores file permissions.
# See run.sh for how it is used.
FROM debian:bookworm@sha256:<digest>
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential file git cmake pkg-config \
      libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libinih-dev \
      xvfb xauth xdotool libgl1-mesa-dri \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --create-home tester
```

- [ ] **Step 4: Create `tests/headless/run.sh`:**

```bash
#!/bin/bash
# StreamFlex headless checks. Builds the launcher with ASan and UBSan, starts a virtual X display,
# runs fixture configs (some with key presses) and reads the debug log and the files they write.
# Runs inside the image built from this folder's Dockerfile, with the repo mounted read-only at
# /src and an output folder at /out:
#   run.sh <label>              every check in checks/, in name order
#   run.sh <label> scrollfail   item 11 only: the scroll arrow's texture is forced to fail,
#                               a path no config or input can reach
# Prints PASS or FAIL per check and exits non-zero when any failed. Every run's output, log and
# exit code are kept in /out/<label>.
set -u
label=$1
fault=${2:-}
HERE=/src/tests/headless
FX=$HERE/fixtures
out=/out/$label
rm -rf "$out"; mkdir -p "$out"

# Build a copy of the source, without the Windows build tree (vcpkg, several GB) or .git
rm -rf /work; mkdir -p /work
tar -C /src --exclude=./build --exclude=./.git -cf - . | tar -C /work -xf -

if [ "$fault" = scrollfail ]; then
    sed -i 's|^    if (scroll->texture == NULL)|    SDL_DestroyTexture(scroll->texture); scroll->texture = NULL; /* FAULT */\n&|' /work/src/image.c
    grep -q 'FAULT' /work/src/image.c || { echo "FAULT NOT INJECTED"; exit 2; }
fi

flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake -S /work -B /work/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="$flags" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" > "$out/configure.log" 2>&1 \
    || { echo "CONFIGURE FAILED"; tail -20 "$out/configure.log"; exit 2; }
cmake --build /work/build -j"$(nproc)" > "$out/build.log" 2>&1 \
    || { echo "BUILD FAILED"; tail -30 "$out/build.log"; exit 2; }
grep -iE 'warning:' "$out/build.log" | sed 's/^/BUILD WARNING: /'

# Xvfb reports a refresh rate of 0, which the launcher must survive (item 22). -ac lets the
# unprivileged test user connect to it.
Xvfb :99 -screen 0 1920x1080x24 -ac > /dev/null 2>&1 &
export DISPLAY=:99
for i in $(seq 100); do [ -S /tmp/.X11-unix/X99 ] && break; sleep 0.2; done
[ -S /tmp/.X11-unix/X99 ] || { echo "Xvfb DID NOT START"; exit 2; }
sleep 1

# The launcher runs as `tester`: root ignores file permissions, and the settings checks need a
# config it cannot write. setpriv, env and setarch each exec the next, so the launcher keeps the
# PID the shell sees. setarch -R turns address randomization off, because GCC 12's ASan crashes
# at random when the kernel randomizes 32 bits of mmap (WSL2, and GitHub's ubuntu-24.04
# runners); it needs the container started with --security-opt seccomp=unconfined. Mesa's
# softpipe has no JIT; llvmpipe's JIT made ASan runs crash at random.
exe=/work/build/streamflex
TESTER_HOME=/home/tester
LOG=$TESTER_HOME/.local/share/streamflex/streamflex.log
TESTER=(setpriv --reuid=tester --regid=tester --init-groups --
        env HOME=$TESTER_HOME DISPLAY=:99 ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1
            GALLIUM_DRIVER=softpipe setarch "$(uname -m)" -R)

# A function to give the launcher its config: the fixture NAME.ini, the file CFG names, or
# none at all with CFG=none (the launcher then searches for one)
config_args() {
    local cfg=${CFG:-$FX/$1.ini}
    [ "$cfg" = none ] || printf '%s\n' -c "$cfg"
}

# A config whose StartupCmd quits by itself. One that hangs gets TERM after 30 s, and KILL 5 s
# later: SDL turns TERM into a quit event, which a launcher stuck in a loop never reads.
run_quick() {
    local name=$1
    local args; mapfile -t args < <(config_args "$name")
    rm -f "$LOG"
    timeout -k 5 -s TERM 30 "${TESTER[@]}" "$exe" "${args[@]}" -d > "$out/$name.out" 2> "$out/$name.err"
    echo $? > "$out/$name.code"
    cp "$LOG" "$out/$name.log" 2> /dev/null || : > "$out/$name.log"
}

# A config left running: send the keys a second apart, then quit it with SIGTERM (SDL turns
# that into a quit event)
run_keys() {
    local name=$1; shift
    local args; mapfile -t args < <(config_args "$name")
    rm -f "$LOG"
    "${TESTER[@]}" "$exe" "${args[@]}" -d > "$out/$name.out" 2> "$out/$name.err" &
    local pid=$!
    sleep 4
    for k in "$@"; do xdotool key "$k"; sleep 1; done
    kill -TERM "$pid" 2> /dev/null; wait "$pid"; echo $? > "$out/$name.code"
    cp "$LOG" "$out/$name.log" 2> /dev/null || : > "$out/$name.log"
}

sanitizer_clean() { ! grep -qE 'AddressSanitizer|runtime error|LeakSanitizer' "$out/$1.err"; }

failures=0
result() {
    if [ "$2" = 0 ]; then echo "PASS  $1"; else echo "FAIL  $1"; failures=$((failures + 1)); fi
}

if [ "$fault" = scrollfail ]; then
    run_quick f11-scroll
    ok=1
    [ "$(cat "$out/f11-scroll.code")" = 0 ] && sanitizer_clean f11-scroll \
        && grep -q 'Could not render scroll indicator' "$out/f11-scroll.log" && ok=0
    result "item 11: a failed scroll arrow disables the arrows and exits cleanly (exit $(cat "$out/f11-scroll.code"))" $ok
    grep -m3 -E 'AddressSanitizer|double-free|runtime error' "$out/f11-scroll.err" | sed 's/^/      /'
else
    for check in "$HERE"/checks/*.sh; do
        . "$check"
    done
fi
echo "$failures failed"
[ "$failures" = 0 ]
```

- [ ] **Step 5: Create `tests/headless/checks/10-grid.sh`.** These are the scratch harness's checks for items 12-17, unchanged except that each one now counts through `result`:

```bash
# Items 12-17: the grid review's follow-ups (#32)

# Item 12: no HPadding/VPadding in the config means the documented 30, and the outline keeps its size
run_quick f12-padding
hl=$(grep -A8 'Highlight ===' "$out/f12-padding.log")
ok=1
echo "$hl" | grep -qE 'VPadding:\s+30$' && echo "$hl" | grep -qE 'HPadding:\s+30$' \
    && echo "$hl" | grep -qE 'OutlineSize:\s+5$' && sanitizer_clean f12-padding && ok=0
result "item 12: unset padding is 30 and OutlineSize stays 5" $ok

# Item 13: Main -> Games -> Games (opened from itself) -> Back must return to Main
run_keys f13-selfsub Return Return BackSpace
last=$(grep -o "Loading menu '[^']*'" "$out/f13-selfsub.log" | tail -1)
ok=1
[ "$last" = "Loading menu 'Main'" ] && sanitizer_clean f13-selfsub && ok=0
result "item 13: Back after a menu opens itself returns to Main" $ok

# Item 14: junk MaxButtons and IconSize are logged and ignored
run_quick f14-junk
ok=1
grep -q "Invalid MaxButtons value '7x'" "$out/f14-junk.log" \
    && grep -q "Invalid IconSize value '200px'" "$out/f14-junk.log" \
    && grep -A3 'Layout ===' "$out/f14-junk.log" | grep -qE 'Columns:\s+4$' \
    && grep -q "Invalid IconSize value '2000' in menu 'Main'" "$out/f14-junk.log" \
    && sanitizer_clean f14-junk && ok=0
result "item 14: MaxButtons=7x and IconSize=200px are rejected with a log line" $ok

# Item 15: huge Rows, Columns and IconSpacing stay inside int and the grid still comes out
run_quick f15-limits
ok=1
[ "$(cat "$out/f15-limits.code")" = 0 ] && sanitizer_clean f15-limits \
    && grep -q "Menu 'Main': [0-9]* x [0-9]* grid" "$out/f15-limits.log" && ok=0
result "item 15: Rows/Columns=999999 and IconSpacing=2000000000 lay out without overflow" $ok

# Item 16: the debug log shows the grid of a menu that is never opened
run_quick f16-debuglayout
ok=1
grep -A4 'Menu Name: Games' "$out/f16-debuglayout.log" | grep -qE 'Layout: 6 x 3 grid, [0-9]+ px buttons' \
    && sanitizer_clean f16-debuglayout && ok=0
result "item 16: the debug log shows Games' 6 x 3 grid without opening it" $ok

# Item 17: a reduced grid is reported once, though the menu loads twice
run_keys f17-once
n=$(grep -c "Menu 'Big': not enough screen space" "$out/f17-once.err")
ok=1
[ "$n" = 1 ] && sanitizer_clean f17-once && ok=0
result "item 17: the reduction is logged once across two loads (logged $n times)" $ok
```

- [ ] **Step 6: Create `tests/headless/checks/20-refresh.sh`:**

```bash
# Item 22: Xvfb reports a refresh rate of 0, as some VMs and remote desktops do. The launcher
# must start, use 60 Hz and say so, instead of dividing by zero.
run_quick f22-refresh
ok=1
[ "$(cat "$out/f22-refresh.code")" = 0 ] && sanitizer_clean f22-refresh \
    && grep -q 'The display reports no refresh rate, using 60 Hz' "$out/f22-refresh.log" && ok=0
result "item 22: a display that reports 0 Hz starts at 60 Hz (exit $(cat "$out/f22-refresh.code"))" $ok
grep -m2 -E 'runtime error|AddressSanitizer' "$out/f22-refresh.err" | sed 's/^/      /'
```

- [ ] **Step 7: Create the fixtures.** Each of the first seven is byte-for-byte the scratch harness's file of the same name. `tests/headless/fixtures/f11-scroll.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Scroll Indicators]
Enabled=true

[Main]
Entry1=One;apps;:quit
Entry2=Two;apps;:quit
Entry3=Three;apps;:quit
Entry4=Four;apps;:quit
Entry5=Five;apps;:quit
Entry6=Six;apps;:quit
```

`f12-padding.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Highlight]
OutlineSize=5

[Main]
Entry1=One;apps;:quit
```

`f13-selfsub.ini`:

```ini
[General]
DefaultMenu=Main

[Main]
Entry1=Games;games;:submenu Games
Entry2=Quit;power;:quit

[Games]
Entry1=Again;games;:submenu Games
Entry2=Other;apps;:quit
```

`f14-junk.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Layout]
MaxButtons=7x
IconSize=200px

[Main]
Entry1=One;apps;:quit
IconSize=2000
```

`f15-limits.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Layout]
Rows=999999
Columns=999999
IconSpacing=2000000000

[Main]
Entry1=One;apps;:quit
Entry2=Two;apps;:quit
Entry3=Three;apps;:quit
```

`f16-debuglayout.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Main]
Entry1=Games;games;:submenu Games

[Games]
Rows=3
Columns=6
Entry1=One;apps;:quit
```

`f17-once.ini`:

```ini
[General]
DefaultMenu=Big
StartupCmd=:submenu Big

[Layout]
Columns=200

[Big]
Entry1=One;apps;:quit
Entry2=Two;apps;:quit
Entry3=Three;apps;:quit
```

`f22-refresh.ini` (new):

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Main]
Entry1=One;apps;:quit
```

- [ ] **Step 8: Run the harness and watch item 22 fail**

Run the two `docker` commands under **Global Constraints** (build, then run with label `local`).

Expected: the item 22 line reads `FAIL  item 22: a display that reports 0 Hz starts at 60 Hz (exit 1)`, and its stderr shows `launcher.c:218:27: runtime error: division by zero`. The other checks fail too, for the same reason. The last line is `N failed`, with N > 0.

- [ ] **Step 9: Fix the refresh rate.** In `src/launcher.h`, under `// Launcher parameters`, add:

```c
#define DEFAULT_REFRESH_RATE 60
```

In `src/launcher.c`, `init_sdl()`, replace:

```c
    SDL_GetDesktopDisplayMode(0, &display_mode);
    geo.screen_width = display_mode.w;
    geo.screen_height = display_mode.h;
    refresh_period = 1000 / (Uint32) display_mode.refresh_rate;
```

with:

```c
    SDL_GetDesktopDisplayMode(0, &display_mode);
    geo.screen_width = display_mode.w;
    geo.screen_height = display_mode.h;

    // SDL reports 0 when the display does not say (Xvfb, some VMs and remote desktops), and
    // every timing below divides by the rate: create_window() reads it again from display_mode
    if (display_mode.refresh_rate <= 0) {
        log_debug("The display reports no refresh rate, using %i Hz", DEFAULT_REFRESH_RATE);
        display_mode.refresh_rate = DEFAULT_REFRESH_RATE;
    }
    refresh_period = 1000 / (Uint32) display_mode.refresh_rate;
```

`create_window()`'s two other divisions (`:239` and `:245`) read the same `display_mode.refresh_rate`, so this one change covers all three.

- [ ] **Step 10: Run the harness again**

Expected: every line is `PASS` (items 12, 13, 14, 15, 16, 17 and 22), then `0 failed`, and the exit code is 0. Then run it with the fault:

```powershell
docker run --rm --security-opt seccomp=unconfined -v C:/Users/jscha/source/repos/streamflex:/src:ro -v C:/Users/jscha/ClaudeScratch/streamflex-headless-out:/out streamflex-test bash /src/tests/headless/run.sh local-scrollfail scrollfail
```

Expected: `PASS  item 11: a failed scroll arrow disables the arrows and exits cleanly (exit 0)`, then `0 failed`.

- [ ] **Step 11: Add the CI job.** In `.github/workflows/build.yml`, after the `library_check` job and before the `build-and-test` comment, add:

```yaml
  # What only a running launcher shows: the settings screen, key handling and startup paths,
  # driven by key presses under a virtual display (tests/headless). It runs in its own container
  # because the harness turns off address randomization for ASan (setarch -R), which Docker's
  # default seccomp profile refuses.
  headless:
    name: Headless
    runs-on: ubuntu-24.04
    steps:
      - name: Checkout Git repository
        uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1
        with:
          persist-credentials: false

      - name: Build the test image
        run: docker build -t streamflex-test tests/headless

      - name: Run the headless checks
        run: |
          mkdir -p "$RUNNER_TEMP/headless"
          docker run --rm --security-opt seccomp=unconfined \
            -v "$GITHUB_WORKSPACE:/src:ro" -v "$RUNNER_TEMP/headless:/out" \
            streamflex-test bash /src/tests/headless/run.sh ci
          docker run --rm --security-opt seccomp=unconfined \
            -v "$GITHUB_WORKSPACE:/src:ro" -v "$RUNNER_TEMP/headless:/out" \
            streamflex-test bash /src/tests/headless/run.sh ci-scrollfail scrollfail

      - uses: actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a # v7.0.1
        name: Upload the logs of a failed run
        if: failure()
        with:
          name: Headless logs
          path: ${{ runner.temp }}/headless
```

In `build-and-test`, change `needs: [build_windows, build_linux, build_rpi, build_arch, library_check]` to:

```yaml
    needs: [build_windows, build_linux, build_rpi, build_arch, library_check, headless]
```

and its comment's first sentence to: `# Single required status check for the master branch ruleset. It passes only when every platform build, the icon library check and the headless checks succeeded;`.

- [ ] **Step 12: Let Dependabot bump the image digest.** In `.github/dependabot.yml`, add under `updates:`, after the `github-actions` entry:

```yaml
  # The headless test image's base (tests/headless/Dockerfile) is pinned by digest
  - package-ecosystem: docker
    directory: /tests/headless
    schedule:
      interval: weekly
    open-pull-requests-limit: 2
```

Then change the file's leading comment line `# github-actions maintains the SHA-pinned action refs in .github/workflows/.` to:

```yaml
# github-actions maintains the SHA-pinned action refs in .github/workflows/, and docker the
# headless test image's pinned base in tests/headless/Dockerfile.
```

- [ ] **Step 13: Build and unit-test on Windows** (the launcher change must still compile there):

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: the build succeeds with no new warnings, and the output has `100% tests passed, 0 tests failed out of 4`.

- [ ] **Step 14: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add .gitattributes tests/headless src/launcher.h src/launcher.c .github/workflows/build.yml .github/dependabot.yml
git -C C:/Users/jscha/source/repos/streamflex commit -m "fix: survive a display that reports 0 Hz; run the headless harness in CI (item 22)"
```

- [ ] **Step 15: Write the menu parsing checks.** Create `tests/headless/checks/25-menus.sh`:

```bash
# How menu sections are read. A section can appear twice with another menu between (configs
# assembled from pieces do this), and an entry can be left empty.

# The second [Main] block's entry and Rows belong to Main, not to Games, the menu read before it
run_quick f25-split
ok=1
grep -A2 'Menu Name: Main' "$out/f25-split.log" | grep -qE 'Number of Entries: 2$' \
    && grep -A2 'Menu Name: Main' "$out/f25-split.log" | grep -qE 'Rows 2, ' \
    && grep -A2 'Menu Name: Games' "$out/f25-split.log" | grep -qE 'Number of Entries: 1$' \
    && grep -A2 'Menu Name: Games' "$out/f25-split.log" | grep -qE 'Rows 0, ' \
    && sanitizer_clean f25-split && ok=0
result "a menu section that appears twice keeps its own entries and grid" $ok

# Entry2= is skipped with a log line; the entries either side of it stay
run_quick f25-empty
ok=1
[ "$(cat "$out/f25-empty.code")" = 0 ] && grep -q "Menu 'Main': 'Entry2' is empty, ignoring it" "$out/f25-empty.log" \
    && grep -A1 'Menu Name: Main' "$out/f25-empty.log" | grep -qE 'Number of Entries: 2$' \
    && sanitizer_clean f25-empty && ok=0
result "an empty entry is skipped instead of crashing (exit $(cat "$out/f25-empty.code"))" $ok
grep -m2 -E 'runtime error|AddressSanitizer' "$out/f25-empty.err" | sed 's/^/      /'
```

Create `tests/headless/fixtures/f25-split.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Main]
Entry1=One;apps;:quit

[Games]
Entry1=Solo;games;:quit

[Main]
Entry2=Two;apps;:quit
Rows=2
```

and `tests/headless/fixtures/f25-empty.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Main]
Entry1=One;apps;:quit
Entry2=
Entry3=Three;apps;:quit
```

- [ ] **Step 16: Run the harness and watch both fail**

Expected:
- `FAIL  a menu section that appears twice keeps its own entries and grid`: the log shows Main with 1 entry and Games with 2, and Games has `Rows 2`.
- `FAIL  an empty entry is skipped instead of crashing (exit 1)`, with `AddressSanitizer: SEGV` in `config_handler` under it: `Entry2=` freed `Entry1` and then wrote through a NULL `previous_entry`.
- Every other line still passes.

- [ ] **Step 17: Fix the menu branch.** In `src/util.c`, `config_handler()`, replace the start of the menu branch:

```c
    // Parse menus/entries
    else {
        Entry *previous_entry = NULL;

        // Check if menu struct exists for current section
        if (config.first_menu == NULL) {
            config.first_menu = create_menu(section, &config.num_menus);
            menu = config.first_menu;
        }
        else {
            bool menu_exists = false;
            for (Menu *tmp = config.first_menu; tmp != NULL;
            tmp = tmp->next) {
                if (MATCH(tmp->name,section)) {
                    menu_exists = true;
                    break;
                }
            }

        // Create menu if it doesn't already exist
            if (menu_exists == false) {
                menu->next = create_menu(section, &config.num_menus);
                menu = menu->next;
            }
        }
```

with:

```c
    // Parse menus/entries
    else {
        Entry *previous_entry = NULL;

        // Point the menu and entry cursors at this section's menu, adding it to the end of the list
        // when it is new. A section can appear twice with another menu between, so the cursors move
        // back to it, and to its last entry, rather than staying on the last menu read.
        Menu *section_menu = NULL;
        Menu *last_menu = NULL;
        for (Menu *tmp = config.first_menu; tmp != NULL; tmp = tmp->next) {
            if (section_menu == NULL && MATCH(tmp->name, section))
                section_menu = tmp;
            last_menu = tmp;
        }
        if (section_menu == NULL) {
            section_menu = create_menu(section, &config.num_menus);
            if (last_menu == NULL)
                config.first_menu = section_menu;
            else
                last_menu->next = section_menu;
        }
        if (section_menu != menu) {
            menu = section_menu;
            entry = menu->first_entry;
            while (entry != NULL && entry->next != NULL)
                entry = entry->next;
        }
```

The new menu goes on the end of the list, not after `menu`: once the cursor can move back to an earlier menu, `menu->next = ...` would cut off every menu after it.

Further down, after `token = strtok(string, delimiter);`, add:

```c
        if (token == NULL) {
            log_error("Menu '%s': '%s' is empty, ignoring it", section, name);
            return 0;
        }
```

(the `if (token != NULL)` block that follows is left as it is). Then in `// Delete entry if parse failed to find 3 valid tokens`, free the refused entry's strings before the entry itself. Replace:

```c
        if (i != 3 || MATCH(":select", entry->cmd)) {
            if (menu->num_entries == 0) {
```

with:

```c
        if (i != 3 || MATCH(":select", entry->cmd)) {
            free(entry->title);
            free(entry->icon_path);
            free(entry->cmd);
            if (menu->num_entries == 0) {
```

(`entry` is the refused one in both branches below it, and it came from `calloc`, so any field never set is NULL.)

- [ ] **Step 18: Run the harness again**

Expected: both `25-menus` lines pass, every other line still passes, `0 failed`. Then the Windows build and unit tests as in Step 13: `100% tests passed, 0 tests failed out of 4`, and no new warnings.

- [ ] **Step 19: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/util.c tests/headless/checks/25-menus.sh tests/headless/fixtures/f25-split.ini tests/headless/fixtures/f25-empty.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "fix: a menu section read twice keeps its entries, and an empty entry no longer crashes"
```

---

### Task 2: UTF-8 file access on every platform (`fileio`)

On Windows, `fopen()` and the `A` APIs read a path in the system code page, while SDL hands out UTF-8. This task adds one small pure module that takes UTF-8 paths everywhere and uses the wide APIs on Windows, and moves the launcher's file access onto it. The later tasks' writer and browser build on it. It also moves `start_process()` from `ShellExecuteExA` to `ShellExecuteExW`, so a command with a non-ASCII path launches (see **Existing bugs fixed on the way**).

**Files:**
- Create: `src/fileio.h`, `src/fileio.c`, `tests/test_fileio.c`
- Modify: `src/CMakeLists.txt`, `tests/CMakeLists.txt`
- Modify: `src/util.c:122` (config file), `src/debug.c:36` (log file), `src/library.c:141,174` (manifest, probe)
- Modify: `src/platform/win32.c:29-47` (`file_exists`, `directory_exists`), `:136-178` (`start_process`), `:180-206` (`scan_slideshow_directory`)

**Interfaces:**
- Produces (every later task uses these exact names):

```c
typedef struct {
    char *name;   // UTF-8 name, without its folder
    bool is_dir;
    bool hidden;  // Windows: the hidden or system attribute; elsewhere: the name starts with '.'
} FileioEntry;

FILE *fileio_open(const char *path, const char *mode);
bool fileio_exists(const char *path);            // a file or folder that can be read
bool fileio_is_dir(const char *path);
bool fileio_is_writable(const char *path);       // an existing file that can be opened for writing
char *fileio_read_all(const char *path, size_t *length);   // NUL-terminated; caller frees
bool fileio_write_all(const char *path, const char *data, size_t length);  // flushed to the disk
bool fileio_copy(const char *from, const char *to);
bool fileio_replace(const char *from, const char *to);     // Windows: retries while the file is held
bool fileio_remove(const char *path);
bool fileio_make_dirs(const char *path);
bool fileio_real_path(const char *path, char *out, size_t size);  // on Linux, follows symbolic links; on Windows, the path as given; false (out empty) when it does not fit
int fileio_list(const char *folder, FileioEntry **entries); // count, or -1
void fileio_free_list(FileioEntry *entries, int count);
const char *fileio_last_error(void);             // why the last call failed, in a few words
#ifdef _WIN32
wchar_t *fileio_wide(const char *text);          // Windows: a new UTF-16 copy, or NULL; caller frees
#endif
```

- [ ] **Step 1: Write the failing test.** Create `tests/test_fileio.c`:

```c
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "fileio.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

// A folder and a file with non-ASCII names ("fileio-fixture-éß", "café.txt"), relative to the
// folder CTest runs the test in
#define DIR "fileio-fixture-\xC3\xA9\xC3\x9F"
#define CAFE "caf\xC3\xA9.txt"

// A function to make a file read-only or writable again, for the permission checks. The path
// is UTF-8, so Windows needs the wide API.
static void set_read_only(const char *path, bool read_only)
{
#ifdef _WIN32
    wchar_t wide[512];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 512);
    SetFileAttributesW(wide, read_only ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL);
#else
    chmod(path, read_only ? 0444 : 0644);
#endif
}

// A function to test that files and folders with non-ASCII names can be made, written, read and listed
static void test_non_ascii_round_trip(void)
{
    CHECK(fileio_make_dirs(DIR "/deeper/still"));
    CHECK(fileio_is_dir(DIR "/deeper/still"));
    CHECK(fileio_write_all(DIR "/" CAFE, "one\r\ntwo\n", 9));
    CHECK(fileio_exists(DIR "/" CAFE));
    CHECK(!fileio_is_dir(DIR "/" CAFE));

    size_t length = 0;
    char *text = fileio_read_all(DIR "/" CAFE, &length);
    CHECK(text != NULL && length == 9 && memcmp(text, "one\r\ntwo\n", 9) == 0 && text[9] == '\0');
    free(text);

    FILE *file = fileio_open(DIR "/" CAFE, "rb");
    CHECK(file != NULL);
    if (file != NULL)
        fclose(file);

    FileioEntry *entries = NULL;
    int count = fileio_list(DIR, &entries);
    bool found_file = false, found_dir = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, CAFE) == 0 && !entries[i].is_dir)
            found_file = true;
        if (strcmp(entries[i].name, "deeper") == 0 && entries[i].is_dir)
            found_dir = true;
        CHECK(strcmp(entries[i].name, ".") != 0 && strcmp(entries[i].name, "..") != 0);
    }
    CHECK(found_file);
    CHECK(found_dir);
    fileio_free_list(entries, count);
}

// A function to test copy, replace and remove
static void test_copy_replace_remove(void)
{
    CHECK(fileio_write_all(DIR "/a.ini", "old", 3));
    CHECK(fileio_copy(DIR "/a.ini", DIR "/a.ini.bak"));
    CHECK(fileio_write_all(DIR "/a.ini.tmp", "new", 3));
    CHECK(fileio_replace(DIR "/a.ini.tmp", DIR "/a.ini"));
    CHECK(!fileio_exists(DIR "/a.ini.tmp"));
    char *text = fileio_read_all(DIR "/a.ini", NULL);
    CHECK(text != NULL && strcmp(text, "new") == 0);
    free(text);
    text = fileio_read_all(DIR "/a.ini.bak", NULL);
    CHECK(text != NULL && strcmp(text, "old") == 0);
    free(text);
    CHECK(fileio_remove(DIR "/a.ini.bak"));
    CHECK(!fileio_exists(DIR "/a.ini.bak"));
}

// A function to test that failures return an error and say why
static void test_failures(void)
{
    FileioEntry *entries = NULL;
    CHECK(fileio_list(DIR "/missing", &entries) == -1);
    CHECK(fileio_last_error()[0] != '\0');
    CHECK(fileio_read_all(DIR "/missing.ini", NULL) == NULL);
    CHECK(strstr(fileio_last_error(), "not found") != NULL);
    CHECK(!fileio_exists(DIR "/missing.ini"));
}

// A function to test the writable check. Root can write anything, so there it is skipped.
static void test_writable(void)
{
    set_read_only(DIR "/locked.ini", false);   // A run stopped halfway may have left it read-only
    CHECK(fileio_write_all(DIR "/locked.ini", "x", 1));
    CHECK(fileio_is_writable(DIR "/locked.ini"));
#ifndef _WIN32
    if (geteuid() == 0) {
        printf("skipped the read-only check: running as root\n");
        return;
    }
#endif
    set_read_only(DIR "/locked.ini", true);
    CHECK(!fileio_is_writable(DIR "/locked.ini"));
    CHECK(strstr(fileio_last_error(), "permission denied") != NULL);
    set_read_only(DIR "/locked.ini", false);
}

// A function to test that a name starting with '.' counts as hidden outside Windows
static void test_hidden(void)
{
#ifndef _WIN32
    CHECK(fileio_write_all(DIR "/.hidden", "x", 1));
    FileioEntry *entries = NULL;
    int count = fileio_list(DIR, &entries);
    bool hidden = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, ".hidden") == 0)
            hidden = entries[i].hidden;
    }
    CHECK(hidden);
    fileio_free_list(entries, count);
#endif
}

#ifdef _WIN32
// A function run on a thread: keep the file open, as an antivirus scan does, then let it go
static DWORD WINAPI hold_file(LPVOID handle)
{
    Sleep(300);
    CloseHandle((HANDLE) handle);
    return 0;
}

// A function to test that a replace waits out another program holding the file open
static void test_replace_waits_for_a_held_file(void)
{
    CHECK(fileio_write_all(DIR "/held.ini", "old", 3));
    CHECK(fileio_write_all(DIR "/held.ini.tmp", "new", 3));
    wchar_t wide[512];
    MultiByteToWideChar(CP_UTF8, 0, DIR "/held.ini", -1, wide, 512);
    HANDLE handle = CreateFileW(wide, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(handle != INVALID_HANDLE_VALUE);
    HANDLE thread = CreateThread(NULL, 0, hold_file, handle, 0, NULL);
    CHECK(fileio_replace(DIR "/held.ini.tmp", DIR "/held.ini"));
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    char *text = fileio_read_all(DIR "/held.ini", NULL);
    CHECK(text != NULL && strcmp(text, "new") == 0);
    free(text);
}

// A function to test the UTF-16 copy that start_process() launches commands with
static void test_wide(void)
{
    wchar_t *wide = fileio_wide("caf\xC3\xA9 \"x\"");
    CHECK(wide != NULL && wcscmp(wide, L"caf\x00e9 \"x\"") == 0);   // An escape: MSVC reads the source as cp1252
    free(wide);
    CHECK(fileio_wide("bad \xC3") == NULL);   // A lead byte with nothing after it
    CHECK(strstr(fileio_last_error(), "UTF-8") != NULL);
}
#endif

int main(void)
{
    test_non_ascii_round_trip();
    test_copy_replace_remove();
    test_failures();
    test_writable();
    test_hidden();
#ifdef _WIN32
    test_replace_waits_for_a_held_file();
    test_wide();
#endif
    return check_report();
}
```

- [ ] **Step 2: Register the test.** In `tests/CMakeLists.txt`, after the `test_utf8` block, add:

```cmake
# Unit tests for UTF-8 file access (non-ASCII names, the replace and its retry on Windows)
add_executable(test_fileio test_fileio.c "${PROJECT_SOURCE_DIR}/src/fileio.c")
target_include_directories(test_fileio PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME fileio COMMAND test_fileio)
```

- [ ] **Step 3: Run it to make sure it fails**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_fileio
```

Expected: the build fails, because `fileio.h` does not exist.

- [ ] **Step 4: Create `src/fileio.h`:**

```c
// File access with UTF-8 paths on every platform. Windows' narrow file APIs read a path in the
// system code page, so there every call converts it to UTF-16 and uses the wide API. Pure: no
// SDL, no launcher headers, so tests/test_fileio.c builds it on its own.
#ifndef FILEIO_H
#define FILEIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define FILEIO_REPLACE_ATTEMPTS 10   // Windows: how often a held file is retried...
#define FILEIO_REPLACE_WAIT_MS 100   // ...and how long apart: about a second in all

typedef struct {
    char *name;   // UTF-8 name of the file or folder, without its folder
    bool is_dir;
    bool hidden;  // Windows: the hidden or system attribute; elsewhere: the name starts with '.'
} FileioEntry;

FILE *fileio_open(const char *path, const char *mode);
bool fileio_exists(const char *path);
bool fileio_is_dir(const char *path);
bool fileio_is_writable(const char *path);
char *fileio_read_all(const char *path, size_t *length);
bool fileio_write_all(const char *path, const char *data, size_t length);
bool fileio_copy(const char *from, const char *to);
bool fileio_replace(const char *from, const char *to);
bool fileio_remove(const char *path);
bool fileio_make_dirs(const char *path);
bool fileio_real_path(const char *path, char *out, size_t size);
int fileio_list(const char *folder, FileioEntry **entries);
void fileio_free_list(FileioEntry *entries, int count);
const char *fileio_last_error(void);

#ifdef _WIN32
#include <wchar.h>
wchar_t *fileio_wide(const char *text);   // For other Windows calls that take a path or command
#endif

#endif
```

- [ ] **Step 5: Create `src/fileio.c`:**

```c
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "fileio.h"
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

static char last_error[160] = "";

// A function to remember why the last call failed
static void set_error(const char *reason)
{
    snprintf(last_error, sizeof(last_error), "%s", reason);
}

// A function to tell the caller why the last call failed
const char *fileio_last_error(void)
{
    return last_error;
}

// A function to describe a C library error in a few words
static void set_errno_error(int code)
{
    switch (code) {
        case EACCES:
        case EPERM:
            set_error("permission denied");
            break;
#ifdef EROFS
        case EROFS:
            set_error("the file system is read-only");
            break;
#endif
        case ENOSPC:
            set_error("the disk is full");
            break;
        case ENOENT:
        case ENOTDIR:
            set_error("not found");
            break;
        default:
            set_error(strerror(code));
    }
}

#ifdef _WIN32
// A function to describe a Windows error code in a few words
static void set_windows_error(DWORD code)
{
    char text[64];
    switch (code) {
        case ERROR_ACCESS_DENIED:
            set_error("permission denied");
            break;
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            set_error("the file is in use by another program");
            break;
        case ERROR_DISK_FULL:
        case ERROR_HANDLE_DISK_FULL:
            set_error("the disk is full");
            break;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            set_error("not found");
            break;
        case ERROR_WRITE_PROTECT:
            set_error("the disk is write-protected");
            break;
        default:
            snprintf(text, sizeof(text), "Windows error %lu", (unsigned long) code);
            set_error(text);
    }
}

// A function to convert a UTF-8 string to a new UTF-16 one
static wchar_t *to_wide(const char *text)
{
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (count <= 0) {
        set_error("the path is not valid UTF-8");
        return NULL;
    }
    wchar_t *wide = malloc((size_t) count * sizeof(wchar_t));
    if (wide != NULL)
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, count);
    return wide;
}

// A function to convert a UTF-16 string to a new UTF-8 one
static char *to_utf8(const wchar_t *wide)
{
    int count = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (count <= 0)
        return NULL;
    char *text = malloc((size_t) count);
    if (text != NULL)
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, text, count, NULL, NULL);
    return text;
}

// A function to give other Windows code a UTF-16 copy of a UTF-8 string, such as a command to launch
wchar_t *fileio_wide(const char *text)
{
    return to_wide(text);
}
#endif

// A function to open a file whose path is UTF-8
FILE *fileio_open(const char *path, const char *mode)
{
#ifdef _WIN32
    wchar_t *wide_path = to_wide(path);
    wchar_t *wide_mode = to_wide(mode);
    FILE *file = NULL;
    if (wide_path != NULL && wide_mode != NULL) {
        file = _wfopen(wide_path, wide_mode);
        if (file == NULL)
            set_errno_error(errno);
    }
    free(wide_path);
    free(wide_mode);
    return file;
#else
    FILE *file = fopen(path, mode);
    if (file == NULL)
        set_errno_error(errno);
    return file;
#endif
}

// A function to tell whether a file or folder exists and can be read
bool fileio_exists(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    bool exists = wide != NULL && _waccess(wide, 4) == 0;
    free(wide);
    return exists;
#else
    return access(path, R_OK) == 0;
#endif
}

// A function to tell whether a path is a folder
bool fileio_is_dir(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    DWORD attributes = wide != NULL ? GetFileAttributesW(wide) : INVALID_FILE_ATTRIBUTES;
    free(wide);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

// A function to tell whether an existing file can be opened for writing
bool fileio_is_writable(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    if (wide == NULL)
        return false;
    HANDLE handle = CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD code = GetLastError();

        // Another program holding it is a moment's wait, which fileio_replace() handles
        if (code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION)
            return true;
        set_windows_error(code);
        return false;
    }
    CloseHandle(handle);
    return true;
#else
    if (access(path, W_OK) == 0)
        return true;
    set_errno_error(errno);
    return false;
#endif
}

// A function to read a whole file into a new NUL-terminated buffer
char *fileio_read_all(const char *path, size_t *length)
{
    FILE *file = fileio_open(path, "rb");
    if (file == NULL)
        return NULL;
    size_t capacity = 4096;
    size_t used = 0;
    char *buffer = malloc(capacity + 1);
    while (buffer != NULL) {
        used += fread(buffer + used, 1, capacity - used, file);
        if (used < capacity)
            break;
        capacity *= 2;
        char *bigger = realloc(buffer, capacity + 1);
        if (bigger == NULL) {
            free(buffer);
            buffer = NULL;
        }
        else
            buffer = bigger;
    }
    bool failed = ferror(file) != 0;
    fclose(file);
    if (buffer == NULL || failed) {
        free(buffer);
        set_error(failed ? "the file could not be read" : "out of memory");
        return NULL;
    }
    buffer[used] = '\0';
    if (length != NULL)
        *length = used;
    return buffer;
}

// A function to write a whole file and flush it to the disk before closing it
bool fileio_write_all(const char *path, const char *data, size_t length)
{
    FILE *file = fileio_open(path, "wb");
    if (file == NULL)
        return false;
    bool ok = fwrite(data, 1, length, file) == length && fflush(file) == 0;
#ifdef _WIN32
    ok = ok && _commit(_fileno(file)) == 0;
#else
    ok = ok && fsync(fileno(file)) == 0;
#endif
    if (!ok)
        set_errno_error(errno);
    if (fclose(file) != 0 && ok) {
        set_errno_error(errno);
        ok = false;
    }
    return ok;
}

// A function to copy a file (config files are small, so it goes through memory)
bool fileio_copy(const char *from, const char *to)
{
    size_t length = 0;
    char *data = fileio_read_all(from, &length);
    if (data == NULL)
        return false;
    bool ok = fileio_write_all(to, data, length);
    free(data);
    return ok;
}

// A function to put one file in place of another in a single step. On Windows, antivirus
// scanners and indexers open a file that has just changed, and the replace is refused while
// they hold it, so it is tried again for about a second.
bool fileio_replace(const char *from, const char *to)
{
#ifdef _WIN32
    wchar_t *wide_from = to_wide(from);
    wchar_t *wide_to = to_wide(to);
    bool ok = false;
    for (int attempt = 0; wide_from != NULL && wide_to != NULL && attempt < FILEIO_REPLACE_ATTEMPTS; attempt++) {
        if (MoveFileExW(wide_from, wide_to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            ok = true;
            break;
        }
        DWORD code = GetLastError();
        set_windows_error(code);
        if (code != ERROR_SHARING_VIOLATION && code != ERROR_LOCK_VIOLATION && code != ERROR_ACCESS_DENIED)
            break;
        Sleep(FILEIO_REPLACE_WAIT_MS);
    }
    free(wide_from);
    free(wide_to);
    return ok;
#else
    // The new file takes the old one's permission bits
    struct stat info;
    if (stat(to, &info) == 0)
        chmod(from, info.st_mode & 07777);
    if (rename(from, to) != 0) {
        set_errno_error(errno);
        return false;
    }
    return true;
#endif
}

// A function to delete a file
bool fileio_remove(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    bool ok = wide != NULL && DeleteFileW(wide);
    if (wide != NULL && !ok)
        set_windows_error(GetLastError());
    free(wide);
    return ok;
#else
    if (remove(path) == 0)
        return true;
    set_errno_error(errno);
    return false;
#endif
}

// A function to make one folder, which may exist already
static bool make_dir(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    bool ok = wide != NULL && (CreateDirectoryW(wide, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    if (wide != NULL && !ok)
        set_windows_error(GetLastError());
    free(wide);
    return ok;
#else
    if (mkdir(path, 0755) == 0 || errno == EEXIST)
        return true;
    set_errno_error(errno);
    return false;
#endif
}

// A function to make a folder and every folder above it that is missing
bool fileio_make_dirs(const char *path)
{
    size_t length = strlen(path);
    char *buffer = malloc(length + 1);
    if (buffer == NULL)
        return false;
    memcpy(buffer, path, length + 1);

    // Make each parent in turn: cut the path at each separator, skipping a leading one ("/")
    // and a drive's ("C:\")
    for (size_t i = 1; i < length; i++) {
        if ((buffer[i] == '/' || buffer[i] == '\\') && buffer[i - 1] != ':') {
            char separator = buffer[i];
            buffer[i] = '\0';
            if (!make_dir(buffer)) {
                free(buffer);
                return false;
            }
            buffer[i] = separator;
        }
    }
    bool ok = make_dir(buffer);
    free(buffer);
    return ok;
}

// A function to find the file a path really names: on Linux a symbolic link is followed, so a
// save writes the file it points to and leaves the link in place
bool fileio_real_path(const char *path, char *out, size_t size)
{
#ifdef _WIN32
    snprintf(out, size, "%s", path);
    return true;
#else
    char resolved[PATH_MAX];
    if (realpath(path, resolved) == NULL) {
        snprintf(out, size, "%s", path);
        return false;
    }
    snprintf(out, size, "%s", resolved);
    return true;
#endif
}

// A function to add one entry to a growing list; the list takes over `name`
static void add_entry(FileioEntry **entries, int *count, int *capacity, char *name, bool is_dir, bool hidden)
{
    if (name == NULL)
        return;
    if (*count == *capacity) {
        int grown = *capacity ? *capacity * 2 : 32;
        FileioEntry *bigger = realloc(*entries, (size_t) grown * sizeof(FileioEntry));
        if (bigger == NULL) {
            free(name);
            return;
        }
        *entries = bigger;
        *capacity = grown;
    }
    (*entries)[*count] = (FileioEntry) { .name = name, .is_dir = is_dir, .hidden = hidden };
    (*count)++;
}

// A function to list a folder's files and folders, without "." and ".."
int fileio_list(const char *folder, FileioEntry **entries)
{
    int count = 0;
    int capacity = 0;
    *entries = NULL;
#ifdef _WIN32
    size_t length = strlen(folder);
    char *pattern = malloc(length + 3);
    if (pattern == NULL)
        return -1;
    bool separator = length > 0 && (folder[length - 1] == '\\' || folder[length - 1] == '/');
    snprintf(pattern, length + 3, "%s%s*", folder, separator ? "" : "\\");
    wchar_t *wide = to_wide(pattern);
    free(pattern);
    if (wide == NULL)
        return -1;
    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW(wide, &data);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND)
            return 0;
        set_windows_error(code);
        return -1;
    }
    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
            continue;
        add_entry(entries, &count, &capacity, to_utf8(data.cFileName),
                  (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
                  (data.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0);
    } while (FindNextFileW(handle, &data));
    FindClose(handle);
#else
    DIR *dir = opendir(folder);
    if (dir == NULL) {
        set_errno_error(errno);
        return -1;
    }
    size_t folder_length = strlen(folder);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // d_type is DT_UNKNOWN on some file systems, and a link to a folder should open like one
        size_t size = folder_length + strlen(entry->d_name) + 2;
        char *full = malloc(size);
        struct stat info;
        bool is_dir = false;
        if (full != NULL) {
            snprintf(full, size, "%s/%s", folder, entry->d_name);
            is_dir = stat(full, &info) == 0 && S_ISDIR(info.st_mode);
            free(full);
        }
        add_entry(entries, &count, &capacity, strdup(entry->d_name), is_dir, entry->d_name[0] == '.');
    }
    closedir(dir);
#endif
    return count;
}

// A function to free a list from fileio_list
void fileio_free_list(FileioEntry *entries, int count)
{
    for (int i = 0; i < count; i++)
        free(entries[i].name);
    free(entries);
}
```

- [ ] **Step 6: Run the test to verify it passes**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_fileio
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release -R fileio --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`. The test's own line reads `N checks, 0 failed`. On Windows the held-file check is included, and takes about 300 ms.

- [ ] **Step 7: Move the launcher onto it.** In `src/CMakeLists.txt`, add `fileio.c` and `fileio.h` to `SOURCES` after `library.h`. In `tests/CMakeLists.txt`, add the file to `test_library`'s sources, since `library.c` now uses it:

```cmake
add_executable(test_library test_library.c "${PROJECT_SOURCE_DIR}/src/library.c" "${PROJECT_SOURCE_DIR}/src/fileio.c")
```

In `src/util.c`, add `#include "fileio.h"` after `#include "library.h"`, and in `parse_config_file()` replace `FILE *file = fopen(config_file_path, "r");` with:

```c
    FILE *file = fileio_open(config_file_path, "r");
```

In `src/debug.c`, add `#include "fileio.h"` after `#include "debug.h"`, and in `init_log()` replace `log_file = fopen(log_file_path, "wb");` with:

```c
    log_file = fileio_open(log_file_path, "wb");
```

In `src/library.c`, add `#include "fileio.h"` after its `#include "library.h"`, and replace `FILE *file = fopen(manifest, "r");` with `FILE *file = fileio_open(manifest, "r");`, and `FILE *probe = fopen(path, "rb");` with `FILE *probe = fileio_open(path, "rb");`.

In `src/platform/win32.c`, add `#include "../fileio.h"` after `#include "../debug.h"`, and replace `file_exists()` and `directory_exists()` with:

```c
// A function to determine if a file exists on the filesystem
bool file_exists(const char *path)
{
    return fileio_exists(path);
}

// A function to determine if a directory exists on the filesystem
bool directory_exists(const char *path)
{
    return fileio_is_dir(path);
}
```

Replace `scan_slideshow_directory()` with:

```c
// A function to tell an image file by its extension, whatever its case
static bool has_image_extension(const char *name)
{
    size_t length = strlen(name);
    for (size_t i = 0; i < NUM_IMAGE_EXTENSIONS; i++) {
        size_t extension_length = strlen(extensions[i]);
        if (length > extension_length && SDL_strcasecmp(name + length - extension_length, extensions[i]) == 0)
            return true;
    }
    return false;
}

// A function to scan the slideshow directory for image files
void scan_slideshow_directory(Slideshow *slideshow, const char *directory)
{
    FileioEntry *entries = NULL;
    int count = fileio_list(directory, &entries);
    char file_output[MAX_PATH_CHARS + 1];
    for (int i = 0; i < count; i++) {
        if (entries[i].is_dir || !has_image_extension(entries[i].name))
            continue;
        join_paths(file_output, sizeof(file_output), 2, directory, entries[i].name);
        char **grown = realloc(slideshow->images, (size_t) (slideshow->num_images + 1) * sizeof(char*));
        if (grown == NULL)
            break;
        slideshow->images = grown;
        slideshow->images[slideshow->num_images] = strdup(file_output);
        slideshow->num_images++;
    }
    fileio_free_list(entries, count);
}
```

Replace `start_process()` with the version below. `parse_command()` stays as it is: it splits on `"` and spaces, which never occur inside a UTF-8 multi-byte character, so it is safe on UTF-8 bytes.

```c
// A function to launch an application. The command is UTF-8, as every string from the config is,
// so it goes to Windows as UTF-16: the ANSI call misread any character outside the system code page.
bool start_process(char *cmd, bool application)
{
    bool ret = false;
    char file[MAX_PATH_CHARS + 1];
    char *params = NULL;
    int cmd_show = application ? SW_SHOWMAXIMIZED : SW_HIDE;

    // Parse command into file and parameters strings
    parse_command(cmd, file, sizeof(file), &params);

    wchar_t *wide_file = fileio_wide(file);
    wchar_t *wide_params = params != NULL ? fileio_wide(params) : NULL;
    BOOL successful = FALSE;
    if (wide_file == NULL || (params != NULL && wide_params == NULL))
        log_error("Could not launch '%s': %s", file, fileio_last_error());
    else {
        // Set up info struct
        SHELLEXECUTEINFOW info = {
            .cbSize = sizeof(SHELLEXECUTEINFOW),
            .fMask = SEE_MASK_NOCLOSEPROCESS,
            .hwnd = NULL,
            .lpVerb = L"open",
            .lpFile = wide_file,
            .lpParameters = wide_params,
            .lpDirectory = NULL,
            .nShow = cmd_show,
            .lpIDList = NULL,
            .lpClass = NULL,
        };
        successful = ShellExecuteExW(&info);

        // Nothing waits on the process, so the handle SEE_MASK_NOCLOSEPROCESS asked for is closed
        if (successful && info.hProcess != NULL)
            CloseHandle(info.hProcess);
    }
    free(wide_file);
    free(wide_params);

    if (!application)
        ret = true;
    else {
        // Go down in the window stack so the launched application can take focus
        if (successful) {
            HWND hwnd = wm_info.info.win.window;
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOREDRAW | SWP_NOSIZE | SWP_NOMOVE);
            ret = true;
        }
        else {
            log_debug("Failed to launch command");
            ret = false;
        }
    }
    free(params);
    return ret;
}
```

Nothing but a Windows desktop can show a launch, so the harness cannot pin this; `test_wide` pins the conversion, and Task 13's hands-on check launches a non-ASCII path.

- [ ] **Step 8: Build and run every test**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 5`, and no new warnings in the build output. Then run the headless harness (Global Constraints). Expected: `0 failed`; the Linux side goes through `fopen` and `opendir` exactly as before.

- [ ] **Step 9: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/fileio.h src/fileio.c tests/test_fileio.c src/CMakeLists.txt tests/CMakeLists.txt src/util.c src/debug.c src/library.c src/platform/win32.c
git -C C:/Users/jscha/source/repos/streamflex commit -m "fix: open files and launch commands by their UTF-8 path on Windows"
```

---

### Task 3: `config.ini` as editable lines (`inidoc`)

A pure module that holds a config file as its lines, reads each line exactly as inih does, and changes one key's value without touching any other byte. Everything later that writes `config.ini` goes through it.

**The inih rules it must mirror** (inih's defaults, which Debian's package and vcpkg's port both build with):
- **Comments.** `;` or `#` at the start of a line (after whitespace) makes a comment. `;` after whitespace inside a line starts a trailing comment (`INI_ALLOW_INLINE_COMMENTS`, prefix `;`). A `;` straight after a non-space character is part of the value.
- **Sections.** `[name]` is a section. The name is everything between the brackets, spaces included (`[ Games ]` is ` Games `).
- **Keys.** `name=value` and `name:value` are keys. The name is right-trimmed, and the value is trimmed at both ends after the trailing comment is cut off.
- **Continuations.** An indented line, after a key and with no section header in between, is that key's continuation (`INI_ALLOW_MULTILINE`), not a key of its own.
- **The BOM.** A UTF-8 BOM at the very start is skipped (`INI_ALLOW_BOM`).
- **Line length.** A line is read into a 200-byte buffer (`INI_MAX_LINE`), so only 199 bytes of it count.

**Files:**
- Create: `src/inidoc.h`, `src/inidoc.c`, `tests/test_inidoc.c`
- Modify: `tests/check.h` (a `CHECK_STR` helper), `tests/CMakeLists.txt`, `src/CMakeLists.txt`

**Interfaces:**
- Consumes: `fileio_read_all()` (Task 2), in the test only, to read the shipped sample config.
- Produces:

```c
#define INIDOC_MAX_LINE 199
typedef enum { INIDOC_AFTER_LAST_KEY, INIDOC_UNDER_HEADER } IniDocPlacement;
typedef struct IniDoc IniDoc;
IniDoc *inidoc_parse(const char *text, size_t length);          // NULL only when out of memory
char *inidoc_serialize(const IniDoc *doc, size_t *length);       // caller frees
const char *inidoc_get(const IniDoc *doc, const char *section, const char *key);  // NULL if absent
bool inidoc_set(IniDoc *doc, const char *section, const char *key, const char *value, IniDocPlacement placement);
bool inidoc_remove(IniDoc *doc, const char *section, const char *key);  // every occurrence; false if none
const char *inidoc_check(const char *key, const char *value);   // NULL, or why `key=value` cannot be written
void inidoc_free(IniDoc *doc);
```

- [ ] **Step 1: Add `CHECK_STR` to `tests/check.h`.** Add `#include <string.h>` after `#include <stdio.h>`, and after `CHECK_INT`:

```c
#define CHECK_STR(actual, expected) do { \
    const char *actual_ = (actual); \
    const char *expected_ = (expected); \
    check_count++; \
    if (actual_ == NULL || strcmp(actual_, expected_) != 0) { \
        check_failures++; \
        fprintf(stderr, "%s:%d: %s is \"%s\", expected \"%s\"\n", __FILE__, __LINE__, #actual, \
            actual_ != NULL ? actual_ : "(null)", expected_); \
    } \
} while (0)
```

- [ ] **Step 2: Write the failing test.** Create `tests/test_inidoc.c`:

```c
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "inidoc.h"
#include "fileio.h"

// A function to parse a file's text, write it back out and give the result (caller frees)
static char *round_trip(const char *text, size_t length, size_t *out_length)
{
    IniDoc *doc = inidoc_parse(text, length);
    char *out = inidoc_serialize(doc, out_length);
    inidoc_free(doc);
    return out;
}

// A function to test that a file read and written back without edits is byte-identical
static void test_round_trip_is_identical(void)
{
    static const char *const texts[] = {
        "[General]\nDefaultMenu=Main\n",
        "[General]\r\nDefaultMenu=Main\r\n\r\n; comment\r\n",
        "\xEF\xBB\xBF[General]\nDefaultMenu=Main\n",
        "[General]\nDefaultMenu=Main",
        "# hash comment\n; semicolon comment\n[Layout]\nColumns = 4 ; four across\nRows: 2\n",
        "[Main]\nEntry1=A;apps;:quit\n[Main]\nEntry1=B;apps;:quit\n",
        "[Layout]\nRows=1\n  continued\n",
        "[Caf\xC3\xA9]\nEntry1=Th\xC3\xA9;apps;:quit\n",
        "",
        "\n\n",
        "[Broken\nNoSeparator\n"
    };
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
        size_t length = 0;
        char *out = round_trip(texts[i], strlen(texts[i]), &length);
        CHECK(out != NULL && length == strlen(texts[i]) && memcmp(out, texts[i], length) == 0);
        free(out);
    }

    // The sample config the build generates, exactly as shipped
    size_t sample_length = 0;
    char *sample = fileio_read_all(SAMPLE_CONFIG, &sample_length);
    CHECK(sample != NULL);
    if (sample != NULL) {
        size_t length = 0;
        char *out = round_trip(sample, sample_length, &length);
        CHECK(out != NULL && length == sample_length && memcmp(out, sample, length) == 0);
        free(out);
        free(sample);
    }
}

// A function to test that a key is found, and its value read, the way inih reads it
static void test_get_reads_like_inih(void)
{
    const char *text =
        "Loose=before any section\n"
        "[Background]\n"
        "Mode = Slideshow ; the photos\n"
        "Semi=;x\n"
        "Spaced= ;x\n"
        "[ Games ]\n"
        "Rows: 3\n"
        "  Columns=6\n"
        "[Layout]\n"
        "Rows=1\n"
        "Rows=2\n";
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK_STR(inidoc_get(doc, "", "Loose"), "before any section");
    CHECK_STR(inidoc_get(doc, "Background", "Mode"), "Slideshow");
    CHECK_STR(inidoc_get(doc, "Background", "Semi"), ";x");       // No space before ';': not a comment
    CHECK_STR(inidoc_get(doc, "Background", "Spaced"), "");       // A space before ';': a comment
    CHECK_STR(inidoc_get(doc, " Games ", "Rows"), "3");           // The name keeps its spaces, as inih's does
    CHECK(inidoc_get(doc, "Games", "Rows") == NULL);
    CHECK(inidoc_get(doc, " Games ", "Columns") == NULL);         // Indented after a key: a continuation
    CHECK_STR(inidoc_get(doc, "Layout", "Rows"), "2");            // The last one wins, as in the parser
    CHECK(inidoc_get(doc, "layout", "Rows") == NULL);             // Names match exactly, case included
    inidoc_free(doc);
}

// A function to apply one set or remove (value NULL) and compare the whole file with what is expected
static void check_edit(int line, const char *text, const char *section, const char *key, const char *value,
                       IniDocPlacement placement, bool expected_ok, const char *expected)
{
    IniDoc *doc = inidoc_parse(text, strlen(text));
    bool ok = value != NULL ? inidoc_set(doc, section, key, value, placement) : inidoc_remove(doc, section, key);
    char *out = inidoc_serialize(doc, NULL);
    check_count++;
    if (ok != expected_ok || out == NULL || strcmp(out, expected) != 0) {
        check_failures++;
        fprintf(stderr, "test_inidoc.c:%d: edit gave %s and\n[%s]\nexpected %s and\n[%s]\n", line,
            ok ? "true" : "false", out != NULL ? out : "(null)", expected_ok ? "true" : "false", expected);
    }
    free(out);
    inidoc_free(doc);
}

// A function to test every way a value is set or removed
static void test_edits(void)
{
    // An existing key: only the value changes; spacing and the trailing comment stay
    check_edit(__LINE__, "[Layout]\nIconSize = 256 ; cap\n", "Layout", "IconSize", "128", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nIconSize = 128 ; cap\n");

    // An empty value before a comment: the value goes straight after '=', so the comment stays a comment
    check_edit(__LINE__, "[Background]\nImage=   ; pick one\n", "Background", "Image", "/pics/a.png", INIDOC_AFTER_LAST_KEY, true,
               "[Background]\nImage=/pics/a.png   ; pick one\n");

    // A repeated key: the last one, which the parser uses, is the one edited
    check_edit(__LINE__, "[Layout]\nRows=1\nRows=2\n", "Layout", "Rows", "3", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=1\nRows=3\n");

    // A new key after the section's last key, before its trailing blank and comment lines
    check_edit(__LINE__, "[Layout]\nRows=1\nColumns=4\n\n; the menus\n[Main]\nEntry1=A;apps;:quit\n",
               "Layout", "IconSize", "256", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=1\nColumns=4\nIconSize=256\n\n; the menus\n[Main]\nEntry1=A;apps;:quit\n");

    // A new key in a menu section goes directly under its header, above the entries
    check_edit(__LINE__, "[Games]\nEntry1=A;apps;:quit\n", "Games", "Rows", "3", INIDOC_UNDER_HEADER, true,
               "[Games]\nRows=3\nEntry1=A;apps;:quit\n");

    // An indented key under the header would become the new key's continuation, so the new key goes after it
    check_edit(__LINE__, "[Games]\n  Rows=3\n", "Games", "Columns", "6", INIDOC_UNDER_HEADER, true,
               "[Games]\n  Rows=3\nColumns=6\n");

    // A continuation stays with its key
    check_edit(__LINE__, "[Layout]\nRows=1\n  more\n", "Layout", "Columns", "4", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=1\n  more\nColumns=4\n");

    // A missing section is added at the end after a blank line, in the file's own line endings, and a
    // file without a final newline still has none
    check_edit(__LINE__, "[General]\r\nDefaultMenu=Main", "Layout", "Columns", "5", INIDOC_AFTER_LAST_KEY, true,
               "[General]\r\nDefaultMenu=Main\r\n\r\n[Layout]\r\nColumns=5");
    check_edit(__LINE__, "[General]\nDefaultMenu=Main\n", "Layout", "Rows", "2", INIDOC_AFTER_LAST_KEY, true,
               "[General]\nDefaultMenu=Main\n\n[Layout]\nRows=2\n");

    // The BOM is kept
    check_edit(__LINE__, "\xEF\xBB\xBF[General]\nX=1\n", "General", "X", "2", INIDOC_AFTER_LAST_KEY, true,
               "\xEF\xBB\xBF[General]\nX=2\n");

    // Removing takes every occurrence, so the key is truly gone; the last line keeps "no newline"
    check_edit(__LINE__, "[Games]\nRows=3\nRows=2\nEntry1=A;apps;:quit\n", "Games", "Rows", NULL, INIDOC_UNDER_HEADER, true,
               "[Games]\nEntry1=A;apps;:quit\n");
    check_edit(__LINE__, "[Games]\nEntry1=A;apps;:quit\nRows=3", "Games", "Rows", NULL, INIDOC_UNDER_HEADER, true,
               "[Games]\nEntry1=A;apps;:quit");
    check_edit(__LINE__, "[Games]\nEntry1=A;apps;:quit\n", "Games", "Rows", NULL, INIDOC_UNDER_HEADER, false,
               "[Games]\nEntry1=A;apps;:quit\n");

    // Values the parser would read back differently are refused, and the file is left alone
    check_edit(__LINE__, "[Background]\n", "Background", "Image", "/a ;b", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
    check_edit(__LINE__, "[Background]\n", "Background", "Image", " /a", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
    check_edit(__LINE__, "[Background]\n", "Background", "Image", "/a\n", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
    check_edit(__LINE__, "[Background]\n", "Background", "Image", ";x", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
}

// A function to test inih's 199-byte line limit: a longer line would be split in two
static void test_line_limit(void)
{
    char value[256];
    memset(value, 'a', 193);
    value[193] = '\0';                                   // "Image=" + 193 bytes = 199
    CHECK(inidoc_check("Image", value) == NULL);
    IniDoc *doc = inidoc_parse("[Background]\n", 13);
    CHECK(inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    value[193] = 'a';
    value[194] = '\0';                                   // 200 bytes
    CHECK(inidoc_check("Image", value) != NULL);
    CHECK(!inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    CHECK(strlen(inidoc_get(doc, "Background", "Image")) == 193);
    inidoc_free(doc);

    // An existing line's trailing comment counts too
    const char *commented = "[Background]\nImage=x ; a long comment that takes up the room on this line\n";
    doc = inidoc_parse(commented, strlen(commented));
    value[150] = '\0';
    CHECK(!inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_get(doc, "Background", "Image"), "x");
    inidoc_free(doc);
}

// A function to test the reasons given for a value that cannot be written
static void test_check(void)
{
    CHECK(inidoc_check("Image", "/home/me/Pictures/a b.png") == NULL);
    CHECK(inidoc_check("Image", "C:\\Pics\\a;b.png") == NULL);   // ';' after a non-space is fine
    CHECK(inidoc_check("Image", "/a ;b") != NULL);
    CHECK(inidoc_check("Image", "\t/a") != NULL);
    CHECK(inidoc_check("Image", "/a ") != NULL);
    CHECK(inidoc_check("Image", "/a\r") != NULL);
}

int main(void)
{
    test_round_trip_is_identical();
    test_get_reads_like_inih();
    test_edits();
    test_line_limit();
    test_check();
    return check_report();
}
```

- [ ] **Step 3: Register the test.** In `tests/CMakeLists.txt`, after the `test_fileio` block:

```cmake
# Unit tests for editing config.ini line by line; the shipped sample config must survive a round trip
add_executable(test_inidoc test_inidoc.c "${PROJECT_SOURCE_DIR}/src/inidoc.c" "${PROJECT_SOURCE_DIR}/src/fileio.c")
target_include_directories(test_inidoc PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(test_inidoc PRIVATE SAMPLE_CONFIG="${PROJECT_BINARY_DIR}/config.ini")
add_test(NAME inidoc COMMAND test_inidoc)
```

- [ ] **Step 4: Run it to make sure it fails**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_inidoc
```

Expected: the build fails, because `inidoc.h` does not exist.

- [ ] **Step 5: Create `src/inidoc.h`:**

```c
// config.ini held as its lines, for changing single settings without disturbing anything else:
// comments, blank lines, order, spacing and line endings all survive. Each line is read the way
// inih reads it, so a key found here is the key the launcher reads. Pure: no SDL, no globals.
#ifndef INIDOC_H
#define INIDOC_H

#include <stdbool.h>
#include <stddef.h>

// inih reads a line into a 200-byte buffer, so a line longer than 199 bytes is split in two
#define INIDOC_MAX_LINE 199

typedef enum {
    INIDOC_AFTER_LAST_KEY,  // After the section's last key, before any trailing blank or comment lines
    INIDOC_UNDER_HEADER     // Directly under the section header (menus: above the entries)
} IniDocPlacement;

typedef struct IniDoc IniDoc;

IniDoc *inidoc_parse(const char *text, size_t length);
char *inidoc_serialize(const IniDoc *doc, size_t *length);
const char *inidoc_get(const IniDoc *doc, const char *section, const char *key);
bool inidoc_set(IniDoc *doc, const char *section, const char *key, const char *value, IniDocPlacement placement);
bool inidoc_remove(IniDoc *doc, const char *section, const char *key);
const char *inidoc_check(const char *key, const char *value);
void inidoc_free(IniDoc *doc);

#endif
```

- [ ] **Step 6: Create `src/inidoc.c`:**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inidoc.h"

typedef enum {
    LINE_OTHER,          // Blank, a comment, or a line inih cannot read
    LINE_SECTION,
    LINE_KEY,
    LINE_CONTINUATION    // Indented after a key: inih reads it as more of that key's value
} LineKind;

typedef struct {
    char *text;          // The line as written, without its line ending
    const char *eol;     // "\n", "\r\n", or "" for a last line with no ending
    LineKind kind;
    char *name;          // SECTION: the section's name; KEY: the key's name, as inih reads them
    char *value;         // KEY: the value, as inih reads it
    size_t value_start;  // KEY: where the value starts in `text`; just after the separator when empty
    size_t value_length; // KEY: how many bytes of `text` the value spans
    int section;         // Index of the SECTION line this one sits under; -1 before the first
} Line;

struct IniDoc {
    Line *lines;
    int count;
    int capacity;
    bool bom;            // The file started with a UTF-8 byte order mark
    const char *eol;     // The ending new lines get: the file's first one, else "\n"
};

static const char *const EOL_LF = "\n";
static const char *const EOL_CRLF = "\r\n";
static const char *const EOL_NONE = "";

// A function to tell whitespace as inih does (isspace in the C locale)
static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// A function to copy part of a string into a new one
static char *copy_span(const char *start, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, start, length);
        copy[length] = '\0';
    }
    return copy;
}

// A function to find, as inih's find_chars_or_comment() does, the first of `chars` from `start`,
// or a ';' that follows whitespace (a trailing comment), or the end of the text
static size_t find_chars_or_comment(const char *text, size_t start, const char *chars)
{
    bool was_space = false;
    size_t i = start;
    while (text[i] != '\0' && (chars == NULL || strchr(chars, text[i]) == NULL) && !(was_space && text[i] == ';')) {
        was_space = is_space(text[i]);
        i++;
    }
    return i;
}

// A function to name the section a line sits under; lines before the first header are in ""
static const char *section_name(const IniDoc *doc, int section)
{
    return section < 0 ? "" : doc->lines[section].name;
}

// A function to read every line the way inih's ini_parse_stream() does. It runs again after
// each edit: config files are small, and reading them whole keeps every index honest.
static void classify_all(IniDoc *doc)
{
    int section = -1;
    bool after_key = false;   // inih's prev_name: a key has been read since the last header
    for (int i = 0; i < doc->count; i++) {
        Line *line = &doc->lines[i];
        free(line->name);
        free(line->value);
        line->name = NULL;
        line->value = NULL;
        line->kind = LINE_OTHER;
        line->section = section;
        const char *text = line->text;
        size_t start = 0;
        while (is_space(text[start]))
            start++;
        if (text[start] == '\0' || text[start] == ';' || text[start] == '#')
            continue;
        if (after_key && start > 0) {
            line->kind = LINE_CONTINUATION;
            continue;
        }
        if (text[start] == '[') {
            size_t end = find_chars_or_comment(text, start + 1, "]");
            if (text[end] == ']') {
                line->kind = LINE_SECTION;
                line->name = copy_span(text + start + 1, end - start - 1);
                line->section = i;
                section = i;
                after_key = false;
            }
            continue;
        }
        size_t end = find_chars_or_comment(text, start, "=:");
        if (text[end] != '=' && text[end] != ':')
            continue;
        size_t name_end = end;
        while (name_end > start && is_space(text[name_end - 1]))
            name_end--;
        size_t value_begin = end + 1;
        size_t value_end = find_chars_or_comment(text, value_begin, NULL);
        size_t first = value_begin;
        while (first < value_end && is_space(text[first]))
            first++;
        size_t last = value_end;
        while (last > first && is_space(text[last - 1]))
            last--;
        line->kind = LINE_KEY;
        line->name = copy_span(text + start, name_end - start);
        line->value = copy_span(text + first, last - first);
        line->value_start = first == last ? value_begin : first;
        line->value_length = last - first;
        after_key = true;
    }
}

// A function to make room for one more line
static bool grow(IniDoc *doc)
{
    if (doc->count < doc->capacity)
        return true;
    int capacity = doc->capacity ? doc->capacity * 2 : 16;
    Line *lines = realloc(doc->lines, (size_t) capacity * sizeof(Line));
    if (lines == NULL)
        return false;
    doc->lines = lines;
    doc->capacity = capacity;
    return true;
}

// A function to add a line read from the file
static bool append_raw(IniDoc *doc, const char *text, size_t length, const char *eol)
{
    if (!grow(doc))
        return false;
    Line *line = &doc->lines[doc->count];
    memset(line, 0, sizeof(*line));
    line->text = copy_span(text, length);
    line->eol = eol;
    if (line->text == NULL)
        return false;
    doc->count++;
    return true;
}

// A function to insert a new line at an index. A new last line takes over "no line ending" from
// the old last line, so a file that did not end with a newline still does not.
static bool insert_line(IniDoc *doc, int at, const char *text)
{
    if (!grow(doc))
        return false;
    char *copy = copy_span(text, strlen(text));
    if (copy == NULL)
        return false;
    memmove(&doc->lines[at + 1], &doc->lines[at], (size_t) (doc->count - at) * sizeof(Line));
    Line *line = &doc->lines[at];
    memset(line, 0, sizeof(*line));
    line->text = copy;
    line->eol = doc->eol;
    doc->count++;
    if (at == doc->count - 1 && at > 0 && doc->lines[at - 1].eol[0] == '\0') {
        doc->lines[at - 1].eol = doc->eol;
        line->eol = EOL_NONE;
    }
    classify_all(doc);
    return true;
}

// A function to delete a line. When the deleted line was the last and had no line ending, the
// new last line loses its ending too.
static void remove_line(IniDoc *doc, int at)
{
    bool was_open_end = at == doc->count - 1 && doc->lines[at].eol[0] == '\0';
    free(doc->lines[at].text);
    free(doc->lines[at].name);
    free(doc->lines[at].value);
    memmove(&doc->lines[at], &doc->lines[at + 1], (size_t) (doc->count - at - 1) * sizeof(Line));
    doc->count--;
    if (was_open_end && doc->count > 0)
        doc->lines[doc->count - 1].eol = EOL_NONE;
    classify_all(doc);
}

// A function to find the last line setting a key in a section: the one the parser ends up with
static int find_key(const IniDoc *doc, const char *section, const char *key)
{
    for (int i = doc->count - 1; i >= 0; i--) {
        const Line *line = &doc->lines[i];
        if (line->kind == LINE_KEY && strcmp(line->name, key) == 0 && strcmp(section_name(doc, line->section), section) == 0)
            return i;
    }
    return -1;
}

// A function to find the last header of a section
static int find_header(const IniDoc *doc, const char *section)
{
    for (int i = doc->count - 1; i >= 0; i--) {
        if (doc->lines[i].kind == LINE_SECTION && strcmp(doc->lines[i].name, section) == 0)
            return i;
    }
    return -1;
}

// A function to find a key's last line: the key itself, or its last continuation line
static int end_of_key(const IniDoc *doc, int i)
{
    while (i + 1 < doc->count && doc->lines[i + 1].kind == LINE_CONTINUATION)
        i++;
    return i;
}

// A function to count the keys, to prove an insert made exactly one more
static int count_keys(const IniDoc *doc)
{
    int keys = 0;
    for (int i = 0; i < doc->count; i++) {
        if (doc->lines[i].kind == LINE_KEY)
            keys++;
    }
    return keys;
}

// A function to read a file's text into lines
IniDoc *inidoc_parse(const char *text, size_t length)
{
    IniDoc *doc = calloc(1, sizeof(IniDoc));
    if (doc == NULL)
        return NULL;
    doc->eol = EOL_LF;
    bool eol_found = false;
    size_t i = 0;
    if (length >= 3 && (unsigned char) text[0] == 0xEF && (unsigned char) text[1] == 0xBB && (unsigned char) text[2] == 0xBF) {
        doc->bom = true;
        i = 3;
    }
    while (i < length) {
        size_t end = i;
        while (end < length && text[end] != '\n')
            end++;
        const char *eol = EOL_NONE;
        size_t text_end = end;
        if (end < length) {
            eol = EOL_LF;
            if (end > i && text[end - 1] == '\r') {
                eol = EOL_CRLF;
                text_end = end - 1;
            }
            if (!eol_found) {
                doc->eol = eol;
                eol_found = true;
            }
        }
        if (!append_raw(doc, text + i, text_end - i, eol)) {
            inidoc_free(doc);
            return NULL;
        }
        i = end < length ? end + 1 : end;
    }
    classify_all(doc);
    return doc;
}

// A function to write the lines back out as one text
char *inidoc_serialize(const IniDoc *doc, size_t *length)
{
    size_t total = doc->bom ? 3 : 0;
    for (int i = 0; i < doc->count; i++)
        total += strlen(doc->lines[i].text) + strlen(doc->lines[i].eol);
    char *out = malloc(total + 1);
    if (out == NULL)
        return NULL;
    size_t used = 0;
    if (doc->bom) {
        memcpy(out, "\xEF\xBB\xBF", 3);
        used = 3;
    }
    for (int i = 0; i < doc->count; i++) {
        size_t text_length = strlen(doc->lines[i].text);
        size_t eol_length = strlen(doc->lines[i].eol);
        memcpy(out + used, doc->lines[i].text, text_length);
        used += text_length;
        memcpy(out + used, doc->lines[i].eol, eol_length);
        used += eol_length;
    }
    out[used] = '\0';
    if (length != NULL)
        *length = used;
    return out;
}

// A function to read a key's value, as the parser would get it
const char *inidoc_get(const IniDoc *doc, const char *section, const char *key)
{
    int i = find_key(doc, section, key);
    return i < 0 ? NULL : doc->lines[i].value;
}

// A function to say why `key=value` cannot be written so that inih reads it back unchanged,
// or NULL when it can
const char *inidoc_check(const char *key, const char *value)
{
    size_t length = strlen(value);
    if (strchr(value, '\n') != NULL || strchr(value, '\r') != NULL)
        return "it contains a line break";
    if (length > 0 && (is_space(value[0]) || is_space(value[length - 1])))
        return "it starts or ends with a space, which config.ini would drop";
    if (value[0] == ';')
        return "it starts with a semicolon, which config.ini would read as a comment";
    for (size_t i = 1; i < length; i++) {
        if (value[i] == ';' && is_space(value[i - 1]))
            return "it has a semicolon after a space, which config.ini would read as a comment";
    }
    if (strlen(key) + 1 + length > INIDOC_MAX_LINE)
        return "it is too long for one line of config.ini (199 bytes at most)";
    return NULL;
}

// A function to put a new value into an existing key's line, keeping everything around it
static bool replace_value(IniDoc *doc, int i, const char *section, const char *key, const char *value)
{
    Line *line = &doc->lines[i];
    size_t text_length = strlen(line->text);
    size_t head = line->value_start;
    size_t tail = line->value_start + line->value_length;
    size_t value_length = strlen(value);
    size_t new_length = head + value_length + (text_length - tail);
    if (new_length > INIDOC_MAX_LINE)
        return false;
    char *text = malloc(new_length + 1);
    if (text == NULL)
        return false;
    memcpy(text, line->text, head);
    memcpy(text + head, value, value_length);
    memcpy(text + head + value_length, line->text + tail, text_length - tail + 1);
    char *old = line->text;
    line->text = text;
    classify_all(doc);

    // Prove the line reads back as intended; if not, put the old line back
    if (find_key(doc, section, key) != i || strcmp(doc->lines[i].value, value) != 0) {
        doc->lines[i].text = old;
        free(text);
        classify_all(doc);
        return false;
    }
    free(old);
    return true;
}

// A function to set a key's value: in its line when it exists, else on a new line placed as asked
bool inidoc_set(IniDoc *doc, const char *section, const char *key, const char *value, IniDocPlacement placement)
{
    if (inidoc_check(key, value) != NULL)
        return false;
    int existing = find_key(doc, section, key);
    if (existing >= 0)
        return replace_value(doc, existing, section, key, value);

    size_t size = strlen(key) + strlen(value) + 2;
    char *text = malloc(size);
    if (text == NULL)
        return false;
    snprintf(text, size, "%s=%s", key, value);
    int keys_before = count_keys(doc);
    int header = find_header(doc, section);
    int at;
    if (header < 0) {
        // A missing section goes at the end, after a blank line
        size_t header_size = strlen(section) + 3;
        char *header_text = malloc(header_size);
        bool ok = header_text != NULL;
        if (ok) {
            snprintf(header_text, header_size, "[%s]", section);
            if (doc->count > 0 && doc->lines[doc->count - 1].text[0] != '\0')
                ok = insert_line(doc, doc->count, "");
            ok = ok && insert_line(doc, doc->count, header_text);
        }
        free(header_text);
        if (!ok) {
            free(text);
            return false;
        }
        at = doc->count;
    }
    else {
        // Under the header, unless the line there is indented: it would become the new key's continuation
        at = header + 1;
        const char *next = at < doc->count ? doc->lines[at].text : "";
        if (placement == INIDOC_AFTER_LAST_KEY || is_space(next[0])) {
            for (int i = 0; i < doc->count; i++) {
                const Line *line = &doc->lines[i];
                if (line->kind == LINE_KEY && strcmp(section_name(doc, line->section), section) == 0)
                    at = end_of_key(doc, i) + 1;
            }
        }
    }
    bool ok = insert_line(doc, at, text);
    free(text);

    // Prove the file now holds exactly one more key, reading as intended
    const char *read_back = ok ? inidoc_get(doc, section, key) : NULL;
    if (ok && (count_keys(doc) != keys_before + 1 || read_back == NULL || strcmp(read_back, value) != 0)) {
        remove_line(doc, at);
        return false;
    }
    return ok;
}

// A function to remove every line that sets a key in a section, with any continuation lines
bool inidoc_remove(IniDoc *doc, const char *section, const char *key)
{
    bool removed = false;
    int i;
    while ((i = find_key(doc, section, key)) >= 0) {
        for (int k = end_of_key(doc, i); k >= i; k--)
            remove_line(doc, k);
        removed = true;
    }
    return removed;
}

// A function to free a document
void inidoc_free(IniDoc *doc)
{
    if (doc == NULL)
        return;
    for (int i = 0; i < doc->count; i++) {
        free(doc->lines[i].text);
        free(doc->lines[i].name);
        free(doc->lines[i].value);
    }
    free(doc->lines);
    free(doc);
}
```

In `src/CMakeLists.txt`, add `inidoc.c` and `inidoc.h` to `SOURCES` after `fileio.h`.

- [ ] **Step 7: Run the test to verify it passes**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_inidoc
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release -R inidoc --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`. A failing edit prints the whole file it produced next to the one expected; compare them byte by byte (line endings included).

- [ ] **Step 8: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/inidoc.h src/inidoc.c tests/test_inidoc.c tests/check.h tests/CMakeLists.txt src/CMakeLists.txt
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: edit config.ini line by line, reading it as inih does"
```

---

### Task 4: The save (`config_save`)

Joins `inidoc` and `fileio` into the one operation the settings screen calls:
1. read the file fresh;
2. apply only the changed keys;
3. keep a `.bak`;
4. write a `.tmp` and swap it in.

On Linux it also falls back to `~/.config/streamflex/config.ini` when the loaded file is the read-only system copy.

**Files:**
- Create: `src/config_save.h`, `src/config_save.c`, `tests/test_config_save.c`
- Modify: `tests/CMakeLists.txt`, `src/CMakeLists.txt`

**Interfaces:**
- Consumes: `inidoc_*` (Task 3), `fileio_*` (Task 2).
- Produces:

```c
#define CONFIG_SAVE_PATH_MAX 1024
typedef struct {
    const char *section;
    const char *key;
    const char *alias;          // An older name the parser reads as `key` (MaxButtons); NULL if none
    const char *value;          // NULL removes the key
    IniDocPlacement placement;
} ConfigEdit;
typedef struct {
    char path[CONFIG_SAVE_PATH_MAX];    // The file written (or that could not be)
    char backup[CONFIG_SAVE_PATH_MAX];  // Its backup; "" when there was no file to back up
    char why[512];                      // Why the save failed, in a few words
} ConfigSaveResult;
bool config_save(const char *loaded, const char *system_prefix, const char *user_config,
                 const ConfigEdit *edits, int count, ConfigSaveResult *result);
```

- [ ] **Step 1: Write the failing test.** Create `tests/test_config_save.c`:

```c
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "config_save.h"
#include "fileio.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define DIR "config-save-fixture"
#define CONFIG DIR "/config.ini"

static const char *const ORIGINAL =
    "; my launcher\n"
    "[Layout]\n"
    "Rows=1\n"
    "MaxButtons=4 ; the old name\n"
    "\n"
    "[Games]\n"
    "IconSize=128\n"
    "Entry1=One;apps;:quit\n";

// A function to make a file read-only or writable again
static void set_read_only(const char *path, bool read_only)
{
#ifdef _WIN32
    SetFileAttributesA(path, read_only ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL);
#else
    chmod(path, read_only ? 0444 : 0644);
#endif
}

// A function to tell whether read-only files can be tested: root writes them anyway
static bool can_test_read_only(void)
{
#ifndef _WIN32
    if (geteuid() == 0) {
        printf("skipped a read-only check: running as root\n");
        return false;
    }
#endif
    return true;
}

// A function to start a check from a known file, with no backup or temporary file lying about
static void reset(const char *path, const char *text)
{
    char other[256];
    set_read_only(path, false);
    fileio_remove(path);
    snprintf(other, sizeof(other), "%s.bak", path);
    fileio_remove(other);
    snprintf(other, sizeof(other), "%s.tmp", path);
    fileio_remove(other);
    CHECK(fileio_make_dirs(DIR));
    if (text != NULL)
        CHECK(fileio_write_all(path, text, strlen(text)));
}

// A function to test whether a file holds exactly a text
static bool holds(const char *path, const char *text)
{
    char *content = fileio_read_all(path, NULL);
    bool same = content != NULL && strcmp(content, text) == 0;
    free(content);
    return same;
}

// A function to test the edits: a changed value, an alias edited in place, a removed key and a
// new key under a menu header, with the old file kept as .bak and no .tmp left behind
static void test_saves_only_the_edits(void)
{
    reset(CONFIG, ORIGINAL);
    ConfigEdit edits[] = {
        { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY },
        { "Layout", "Columns", "MaxButtons", "5", INIDOC_AFTER_LAST_KEY },
        { "Games", "IconSize", NULL, NULL, INIDOC_UNDER_HEADER },
        { "Games", "Rows", NULL, "3", INIDOC_UNDER_HEADER }
    };
    ConfigSaveResult result;
    CHECK(config_save(CONFIG, NULL, NULL, edits, 4, &result));
    CHECK(holds(CONFIG,
        "; my launcher\n"
        "[Layout]\n"
        "Rows=2\n"
        "MaxButtons=5 ; the old name\n"
        "\n"
        "[Games]\n"
        "Rows=3\n"
        "Entry1=One;apps;:quit\n"));
    CHECK(strstr(result.path, "config-save-fixture") != NULL);
    CHECK(strstr(result.backup, "config.ini.bak") != NULL);
    CHECK(holds(CONFIG ".bak", ORIGINAL));
    CHECK(!fileio_exists(CONFIG ".tmp"));
}

// A function to test that a change made on disk meanwhile, by hand, survives the save
static void test_keeps_a_hand_edit_made_meanwhile(void)
{
    reset(CONFIG, ORIGINAL);
    const char *edited_meanwhile =
        "; my launcher\n"
        "[Layout]\n"
        "Rows=1\n"
        "MaxButtons=4 ; the old name\n"
        "\n"
        "[Games]\n"
        "Columns=6\n"
        "IconSize=128\n"
        "Entry1=One;apps;:quit\n";
    CHECK(fileio_write_all(CONFIG, edited_meanwhile, strlen(edited_meanwhile)));
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    char *content = fileio_read_all(CONFIG, NULL);
    CHECK(content != NULL && strstr(content, "Columns=6\n") != NULL && strstr(content, "Rows=2\n") != NULL);
    free(content);
}

// A function to test that a value config.ini cannot hold fails the save and leaves the file alone
static void test_refused_value_changes_nothing(void)
{
    reset(CONFIG, ORIGINAL);
    ConfigEdit edit = { "Background", "Image", NULL, "/a ;b.png", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "comment") != NULL);
    CHECK(holds(CONFIG, ORIGINAL));
    CHECK(!fileio_exists(CONFIG ".bak"));
    CHECK(!fileio_exists(CONFIG ".tmp"));
}

// A function to test that a read-only file fails with the reason and is left alone
static void test_read_only_fails(void)
{
    if (!can_test_read_only())
        return;
    reset(CONFIG, ORIGINAL);
    set_read_only(CONFIG, true);
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "permission denied") != NULL);
    CHECK(holds(CONFIG, ORIGINAL));
    set_read_only(CONFIG, false);
}

// A function to test the fallback: a read-only config under the system prefix is saved as the
// user's own copy, and the system copy is left alone
static void test_system_copy_falls_back_to_the_user_config(void)
{
    if (!can_test_read_only())
        return;
    const char *system_config = DIR "/system/config.ini";
    const char *user_config = DIR "/home/.config/streamflex/config.ini";
    CHECK(fileio_make_dirs(DIR "/system"));
    reset(system_config, ORIGINAL);
    reset(user_config, NULL);
    set_read_only(system_config, true);
    char prefix[CONFIG_SAVE_PATH_MAX];
    CHECK(fileio_real_path(DIR "/system", prefix, sizeof(prefix) - 1));
    strcat(prefix, "/");
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(config_save(system_config, prefix, user_config, &edit, 1, &result));
    CHECK_STR(result.path, user_config);
    CHECK_STR(result.backup, "");
    CHECK(holds(system_config, ORIGINAL));
    char *content = fileio_read_all(user_config, NULL);
    CHECK(content != NULL && strstr(content, "Rows=2\n") != NULL && strstr(content, "; my launcher\n") != NULL);
    free(content);
    set_read_only(system_config, false);

    // Without a system prefix there is no fallback
    set_read_only(system_config, true);
    CHECK(!config_save(system_config, NULL, NULL, &edit, 1, &result));
    set_read_only(system_config, false);
}

// A function to test that a config that has vanished fails with a reason
static void test_missing_file_fails(void)
{
    reset(CONFIG, NULL);
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "not found") != NULL);
    CHECK(!fileio_exists(CONFIG));
}

#ifndef _WIN32
// A function to test that a symbolic link is followed: the file it points to changes, the link stays
static void test_follows_a_symbolic_link(void)
{
    reset(DIR "/real.ini", ORIGINAL);
    remove(DIR "/link.ini");
    CHECK(symlink("real.ini", DIR "/link.ini") == 0);
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(config_save(DIR "/link.ini", NULL, NULL, &edit, 1, &result));
    struct stat info;
    CHECK(lstat(DIR "/link.ini", &info) == 0 && S_ISLNK(info.st_mode));
    char *content = fileio_read_all(DIR "/real.ini", NULL);
    CHECK(content != NULL && strstr(content, "Rows=2\n") != NULL);
    free(content);
}
#endif

int main(void)
{
    test_saves_only_the_edits();
    test_keeps_a_hand_edit_made_meanwhile();
    test_refused_value_changes_nothing();
    test_read_only_fails();
    test_system_copy_falls_back_to_the_user_config();
    test_missing_file_fails();
#ifndef _WIN32
    test_follows_a_symbolic_link();
#endif
    return check_report();
}
```

- [ ] **Step 2: Register the test.** In `tests/CMakeLists.txt`, after the `test_inidoc` block:

```cmake
# Unit tests for the settings screen's save: fresh read, edits, backup, replace and the Linux fallback
add_executable(test_config_save test_config_save.c "${PROJECT_SOURCE_DIR}/src/config_save.c"
  "${PROJECT_SOURCE_DIR}/src/inidoc.c" "${PROJECT_SOURCE_DIR}/src/fileio.c")
target_include_directories(test_config_save PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME config_save COMMAND test_config_save)
```

- [ ] **Step 3: Run it to make sure it fails**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_config_save
```

Expected: the build fails, because `config_save.h` does not exist.

- [ ] **Step 4: Create `src/config_save.h`:**

```c
// Saving the settings screen's changes into config.ini: only the changed keys, applied to the
// file as it is on disk at that moment, with the old file kept as .bak and the new one swapped in
// whole. Pure: no SDL.
#ifndef CONFIG_SAVE_H
#define CONFIG_SAVE_H

#include <stdbool.h>
#include "inidoc.h"

#define CONFIG_SAVE_PATH_MAX 1024

typedef struct {
    const char *section;
    const char *key;
    const char *alias;          // An older name the parser reads as `key` (MaxButtons for Columns),
                                // edited in its place when only it is present; NULL if none
    const char *value;          // NULL removes the key
    IniDocPlacement placement;  // Where a new key goes
} ConfigEdit;

typedef struct {
    char path[CONFIG_SAVE_PATH_MAX];    // The file written, or that could not be
    char backup[CONFIG_SAVE_PATH_MAX];  // Its backup; "" when there was no file to back up
    char why[512];                      // Why the save failed, in a few words
} ConfigSaveResult;

bool config_save(const char *loaded, const char *system_prefix, const char *user_config,
                 const ConfigEdit *edits, int count, ConfigSaveResult *result);

#endif
```

- [ ] **Step 5: Create `src/config_save.c`:**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config_save.h"
#include "fileio.h"

// A function to tell whether a path lies under a folder
static bool starts_with(const char *path, const char *prefix)
{
    return strncmp(path, prefix, strlen(prefix)) == 0;
}

// A function to cut a file's path down to its folder, in place
static void cut_to_folder(char *path)
{
    size_t length = strlen(path);
    while (length > 0 && path[length - 1] != '/' && path[length - 1] != '\\')
        length--;
    if (length > 1)
        length--;
    path[length] = '\0';
}

// A function to apply the edits to a config's text; false (with the reason) when one cannot be written
static bool apply_edits(IniDoc *doc, const ConfigEdit *edits, int count, ConfigSaveResult *result)
{
    for (int i = 0; i < count; i++) {
        const ConfigEdit *edit = &edits[i];
        const char *key = edit->key;
        if (edit->alias != NULL && inidoc_get(doc, edit->section, key) == NULL &&
        inidoc_get(doc, edit->section, edit->alias) != NULL)
            key = edit->alias;
        if (edit->value == NULL)
            inidoc_remove(doc, edit->section, key);
        else if (!inidoc_set(doc, edit->section, key, edit->value, edit->placement)) {
            const char *reason = inidoc_check(key, edit->value);
            snprintf(result->why, sizeof(result->why), "the %s value in [%s] cannot be written: %s",
                key, edit->section, reason != NULL ? reason : "it would change how other lines read");
            return false;
        }
    }
    return true;
}

// A function to save the settings screen's changes. `loaded` is the file the launcher read.
// When it cannot be written but lies under `system_prefix` (the packaged copy on Linux), the
// save goes to `user_config` instead, which the launcher searches first; pass NULL for both
// where there is no such fallback (Windows).
bool config_save(const char *loaded, const char *system_prefix, const char *user_config,
                 const ConfigEdit *edits, int count, ConfigSaveResult *result)
{
    memset(result, 0, sizeof(*result));
    char source[CONFIG_SAVE_PATH_MAX];
    if (!fileio_real_path(loaded, source, sizeof(source))) {
        snprintf(result->path, sizeof(result->path), "%s", loaded);
        snprintf(result->why, sizeof(result->why), "%s", fileio_last_error());
        return false;
    }
    snprintf(result->path, sizeof(result->path), "%s", source);

    if (!fileio_is_writable(source)) {
        if (system_prefix == NULL || user_config == NULL || !starts_with(source, system_prefix)) {
            snprintf(result->why, sizeof(result->why), "%s", fileio_last_error());
            return false;
        }
        snprintf(result->path, sizeof(result->path), "%s", user_config);
        char folder[CONFIG_SAVE_PATH_MAX];
        snprintf(folder, sizeof(folder), "%s", user_config);
        cut_to_folder(folder);
        if (!fileio_make_dirs(folder)) {
            snprintf(result->why, sizeof(result->why), "could not make the folder %s: %s", folder, fileio_last_error());
            return false;
        }
    }

    // Read the file as it is on disk now, so a change made meanwhile by hand survives
    size_t length = 0;
    char *text = fileio_read_all(source, &length);
    if (text == NULL) {
        snprintf(result->why, sizeof(result->why), "could not read %s: %s", source, fileio_last_error());
        return false;
    }
    IniDoc *doc = inidoc_parse(text, length);
    free(text);
    if (doc == NULL) {
        snprintf(result->why, sizeof(result->why), "out of memory");
        return false;
    }
    if (!apply_edits(doc, edits, count, result)) {
        inidoc_free(doc);
        return false;
    }
    char *output = inidoc_serialize(doc, &length);
    inidoc_free(doc);
    if (output == NULL) {
        snprintf(result->why, sizeof(result->why), "out of memory");
        return false;
    }

    // Keep the file as it was, then write the new one beside it and swap it in whole
    bool ok = true;
    if (fileio_exists(result->path)) {
        snprintf(result->backup, sizeof(result->backup), "%s.bak", result->path);
        if (!fileio_copy(result->path, result->backup)) {
            snprintf(result->why, sizeof(result->why), "could not write the backup %s: %s", result->backup, fileio_last_error());
            result->backup[0] = '\0';
            ok = false;
        }
    }
    char temporary[CONFIG_SAVE_PATH_MAX + 4];
    snprintf(temporary, sizeof(temporary), "%s.tmp", result->path);
    if (ok && !fileio_write_all(temporary, output, length)) {
        snprintf(result->why, sizeof(result->why), "could not write %s: %s", temporary, fileio_last_error());
        ok = false;
    }
    else if (ok && !fileio_replace(temporary, result->path)) {
        snprintf(result->why, sizeof(result->why), "could not replace the file: %s", fileio_last_error());
        ok = false;
    }
    if (!ok)
        fileio_remove(temporary);
    free(output);
    return ok;
}
```

In `src/CMakeLists.txt`, add `config_save.c` and `config_save.h` to `SOURCES` after `inidoc.h`.

- [ ] **Step 6: Run the test to verify it passes**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_config_save
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release -R config_save --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`. On Windows every check runs. On a Linux machine as root, two lines say `skipped a read-only check: running as root`; the headless harness covers those cases in Task 10.

- [ ] **Step 7: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/config_save.h src/config_save.c tests/test_config_save.c tests/CMakeLists.txt src/CMakeLists.txt
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: save settings into config.ini with a backup and a safe replace"
```

---

### Task 5: Titles that scale with the button, in the layout (items 18 and 19)

`layout_compute()` learns to size a menu's titles from its button size, and to solve the two together: the title block under a button grows with the button, and the button must leave room for it. It stays pure, so the rule is tested here without SDL. Task 7 wires it into the launcher.

**The rule**, for a button of `b` px:
- **Title size** (points; SDL_ttf draws one pixel per point):
  - with a percentage `FontSize` of `p`, it is `max(round(b·p/100), minimum)`;
  - with a fixed `FontSize`, it is 0 here, and the fixed font's line height arrives as `title_block`.
- **Line height** for a percentage size `s`: `ceil(s · line_pm / 1000) + 1`, where `line_pm` is the font's line height per point measured at 1000 pt. The `+ 1` covers rounding in the font.
- **Padding:**
  - with a percentage `Padding` of `q`, it is `round(b·q/100)`;
  - with a fixed one, it is `min(padding, b/2)`: the limit `validate_settings()` applied against `IconSize` until now.
- **Title block** = `title_block` (the fixed font's line height, or 0) + line height (0 for a fixed size) + padding.
- **The largest button** is the largest `b` for which `rows · (b + block(b)) + (rows − 1) · spacing + 2 · vpad ≤ height`, capped as before by the width and `IconSize`. `block(b)` never shrinks as `b` grows, so a binary search over whole pixels finds it exactly.

**Files:**
- Modify: `src/layout.h`, `src/layout.c`, `tests/test_layout.c`

**Interfaces:**
- Produces (Task 7 and the settings table rely on these):

```c
#define LAYOUT_MAX_TITLE_POINTS 512
#define LAYOUT_MAX_TITLE_PERCENT 100
#define LAYOUT_MAX_PADDING_PERCENT 50

// LayoutParams gains, after vpad (existing positional initialisers leave them 0):
    int title_padding;     // A fixed title Padding in px, capped at half the button; 0 without titles
    int title_padding_pct; // Padding as a percentage of the button; 0 = fixed (title_padding)
    int title_size_pct;    // FontSize as a percentage of the button; 0 = fixed (its line height is title_block)
    int title_min_size;    // The readable minimum point size for a percentage FontSize
    int title_line_pm;     // The title font's line height per point, in thousandths, for a percentage FontSize
// and title_block's meaning narrows to: the fixed FontSize's line height, without the padding.

// LayoutGeometry gains:
    int title_size;        // Title point size for a percentage FontSize; 0 = the fixed FontSize
    int title_padding;     // Space between the button and its title, in px
    int title_block;       // Padding plus the title's line height: all that sits under the button

int layout_title_size(const LayoutParams *params, int button);
int layout_title_padding(const LayoutParams *params, int button);
int layout_title_block(const LayoutParams *params, int button);
bool layout_parse_title_size(const char *value, int *size, bool *percent);
bool layout_parse_title_padding(const char *value, int *padding, bool *percent);
```

- [ ] **Step 1: Write the failing tests.** In `tests/test_layout.c`, after `test_compute_clock_band()`, add:

```c
// Titles that scale: FontSize 14% and Padding 8% of the button, never below 22 pt, with a font
// whose line height per point is exactly 1, so a title's line is its size + 1
static LayoutParams scaled(int rows, int columns, int icon_cap)
{
    LayoutParams p = params(rows, columns, icon_cap);
    p.title_block = 0;
    p.title_size_pct = 14;
    p.title_padding_pct = 8;
    p.title_min_size = 22;
    p.title_line_pm = 1000;
    return p;
}

// A function to test FontSize and Padding values: a percentage of the button, or a fixed size
static void test_parse_title_values(void)
{
    int n = -7;
    bool percent = false;
    CHECK(layout_parse_title_size("14%", &n, &percent));
    CHECK_INT(n, 14);
    CHECK(percent);
    CHECK(layout_parse_title_size("36", &n, &percent));
    CHECK_INT(n, 36);
    CHECK(!percent);
    CHECK(layout_parse_title_size("100%", &n, &percent));
    CHECK(!layout_parse_title_size("0%", &n, &percent));
    CHECK(!layout_parse_title_size("101%", &n, &percent));
    CHECK(!layout_parse_title_size("0", &n, &percent));
    CHECK(!layout_parse_title_size("513", &n, &percent));
    CHECK(!layout_parse_title_size("14 %", &n, &percent));
    CHECK(!layout_parse_title_size("%", &n, &percent));
    CHECK(!layout_parse_title_size("12pt", &n, &percent));
    CHECK(!layout_parse_title_size("", &n, &percent));
    CHECK(!layout_parse_title_size(NULL, &n, &percent));
    CHECK_INT(n, 100); // A rejected value leaves the output alone

    CHECK(layout_parse_title_padding("8%", &n, &percent));
    CHECK_INT(n, 8);
    CHECK(percent);
    CHECK(layout_parse_title_padding("0%", &n, &percent));
    CHECK_INT(n, 0);
    CHECK(layout_parse_title_padding("20", &n, &percent));
    CHECK_INT(n, 20);
    CHECK(!percent);
    CHECK(layout_parse_title_padding("0", &n, &percent));
    CHECK(!layout_parse_title_padding("51%", &n, &percent));
    CHECK(!layout_parse_title_padding("1025", &n, &percent));
    CHECK(!layout_parse_title_padding("-1", &n, &percent));
    CHECK(!layout_parse_title_padding("20px", &n, &percent));
}

// A function to test scaled titles on a width-limited strip
static void test_titles_scale_on_a_strip(void)
{
    LayoutParams p = scaled(1, 4, 0);
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, NULL, 0), 0);
    CHECK_INT(g.button, 393);       // (1920 - 3*96 - 2*30) / 4, as with fixed titles
    CHECK_INT(g.title_size, 55);    // round(393 * 0.14)
    CHECK_INT(g.title_padding, 31); // round(393 * 0.08)
    CHECK_INT(g.title_block, 87);   // 55 + 1 + 31
    CHECK_INT(g.y_advance, 576);    // 393 + 87 + 96
    CHECK_INT(g.y_origin, 300);     // 540 - (393 + 87) / 2
}

// A function to test that the button and its growing title block are solved together: 3 rows
// have 720 px, so b + block(b) <= 240, which 196 meets exactly (27 pt, 28 px line, 16 px padding)
// and 197 misses (28 pt, 29 px, 16 px: 242)
static void test_titles_solved_with_the_button(void)
{
    LayoutParams p = scaled(3, 6, 0);
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 18, &g, NULL, 0), 0);
    CHECK_INT(g.button, 196);
    CHECK_INT(g.title_size, 27);
    CHECK_INT(g.title_padding, 16);
    CHECK_INT(g.title_block, 44);
    CHECK_INT(g.x_origin, 132);     // (1920 - (6*196 + 5*96)) / 2
    CHECK_INT(g.y_origin, 84);      // the 912 px block fills the area: 54 + 30
    CHECK_INT(g.y_advance, 336);    // 196 + 44 + 96
}

// A function to test item 19's dense end: 8 x 4 buttons are small enough that 14% would be
// unreadable, so titles stop at the 22 pt minimum and the buttons shrink to make room
static void test_titles_stop_at_the_minimum(void)
{
    LayoutParams p = scaled(4, 8, 0);
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 32, &g, NULL, 0), 0);
    CHECK_INT(g.button, 123);       // 4 rows have 624 px: 123 + 23 + 10 = 156 fits, 124 needs 157
    CHECK_INT(g.title_size, 22);    // round(123 * 0.14) is 17, below the minimum
    CHECK_INT(g.title_block, 33);
}

// A function to test item 19's large end: 1024 px buttons at 5120 x 2160 get titles to match
static void test_titles_grow_with_large_buttons(void)
{
    LayoutArea screen_5k = { 0, 108, 5120, 1944, 1080 };
    LayoutParams p = scaled(1, 4, 1024);
    p.spacing = 256;
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &screen_5k, 4, &g, NULL, 0), 0);
    CHECK_INT(g.button, 1024);
    CHECK_INT(g.title_size, 143);   // round(1024 * 0.14); a fixed 36 looked tiny here
    CHECK_INT(g.title_padding, 82); // item 18: the padding follows the button too
}

// A function to test fixed titles: a fixed FontSize's line height arrives as title_block, a
// fixed Padding is capped at half the button (item 18), and a percentage Padding still scales
static void test_fixed_titles(void)
{
    LayoutArea narrow = { 0, 0, 300, 972, 486 };
    LayoutParams p = params(1, 6, 0);
    LayoutGeometry g;
    p.title_block = 30;
    p.title_padding = 100;
    CHECK_INT(layout_compute(&p, &narrow, 6, &g, NULL, 0), 0);
    CHECK_INT(g.button, 72);        // Two columns fit, as in test_compute_reduces_overflowing_axis
    CHECK_INT(g.title_size, 0);
    CHECK_INT(g.title_padding, 36); // 100 px capped at half of 72
    CHECK_INT(g.title_block, 66);

    p = params(1, 4, 0);
    p.title_block = 37;
    p.title_padding_pct = 8;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, NULL, 0), 0);
    CHECK_INT(g.button, 393);
    CHECK_INT(g.title_padding, 31);
    CHECK_INT(g.title_block, 68);
}

// A function to test titles turned off: nothing sits under the buttons
static void test_no_titles(void)
{
    LayoutParams p = params(1, 4, 0);
    LayoutGeometry g;
    p.title_block = 0;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, NULL, 0), 0);
    CHECK_INT(g.title_block, 0);
    CHECK_INT(g.title_padding, 0);
    CHECK_INT(g.y_origin, 344);     // 540 - 393 / 2
    CHECK_INT(g.y_advance, 489);    // 393 + 96
}
```

And in `main()`, after `test_compute_clock_band();`:

```c
    test_parse_title_values();
    test_titles_scale_on_a_strip();
    test_titles_solved_with_the_button();
    test_titles_stop_at_the_minimum();
    test_titles_grow_with_large_buttons();
    test_fixed_titles();
    test_no_titles();
```

- [ ] **Step 2: Run the tests to make sure they fail**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_layout
```

Expected: the build fails: `layout_parse_title_size` is undeclared, and `LayoutParams` has no member `title_size_pct`.

- [ ] **Step 3: Extend `src/layout.h`.** After `#define LAYOUT_MAX_BUTTON 1024 ...`, add:

```c
#define LAYOUT_MAX_TITLE_POINTS 512   // Largest fixed FontSize
#define LAYOUT_MAX_TITLE_PERCENT 100  // Largest FontSize percentage
#define LAYOUT_MAX_PADDING_PERCENT 50 // Largest title Padding percentage
```

Replace the `LayoutParams` struct with:

```c
// Everything layout_compute needs to size a menu's buttons
typedef struct {
    int rows;
    int columns;
    int icon_cap;          // Largest allowed button size; 0 = no cap
    int spacing;           // Gap between buttons in px, across and down
    int title_block;       // A fixed FontSize's line height below each icon; 0 for a percentage or without titles
    int hpad;              // Requested highlight padding; negative counts as 0
    int vpad;
    int title_padding;     // A fixed title Padding in px, capped at half the button; 0 without titles
    int title_padding_pct; // Padding as a percentage of the button; 0 = fixed (title_padding)
    int title_size_pct;    // FontSize as a percentage of the button; 0 = fixed (its line height is title_block)
    int title_min_size;    // The readable minimum point size for a percentage FontSize
    int title_line_pm;     // The title font's line height per point, in thousandths, for a percentage FontSize
} LayoutParams;
```

In `LayoutGeometry`, after `int vpad;`, add:

```c
    int title_size;  // Title point size for a percentage FontSize; 0 = the fixed FontSize
    int title_padding; // Space between the button and its title, in px
    int title_block; // Padding plus the title's line height: all that sits under the button
```

After the `layout_parse_icon_size` prototype, add:

```c
bool layout_parse_title_size(const char *value, int *size, bool *percent);
bool layout_parse_title_padding(const char *value, int *padding, bool *percent);
int layout_title_size(const LayoutParams *params, int button);
int layout_title_padding(const LayoutParams *params, int button);
int layout_title_block(const LayoutParams *params, int button);
```

- [ ] **Step 4: Implement in `src/layout.c`.** After `layout_parse_icon_size()`, add:

```c
// A function to read "N" or "N%": a whole number from min_number to max_number, or a percentage
// from min_percent to max_percent
static bool parse_number_or_percent(const char *value, int min_number, int max_number,
                                    int min_percent, int max_percent, int *number, bool *percent)
{
    if (value == NULL)
        return false;
    size_t length = strlen(value);
    bool is_percent = length > 1 && value[length - 1] == '%';
    size_t digits = is_percent ? length - 1 : length;
    if (digits == 0 || digits > 4)
        return false;
    int n = 0;
    for (size_t i = 0; i < digits; i++) {
        if (!isdigit((unsigned char) value[i]))
            return false;
        n = n * 10 + (value[i] - '0');
    }
    if (is_percent ? (n < min_percent || n > max_percent) : (n < min_number || n > max_number))
        return false;
    *number = n;
    *percent = is_percent;
    return true;
}

// A function to read a FontSize: a percentage of the button ("14%") or a fixed point size ("36")
bool layout_parse_title_size(const char *value, int *size, bool *percent)
{
    return parse_number_or_percent(value, 1, LAYOUT_MAX_TITLE_POINTS, 1, LAYOUT_MAX_TITLE_PERCENT, size, percent);
}

// A function to read a title Padding: a percentage of the button ("8%") or a number of px ("20")
bool layout_parse_title_padding(const char *value, int *padding, bool *percent)
{
    return parse_number_or_percent(value, 0, LAYOUT_MAX_BUTTON, 0, LAYOUT_MAX_PADDING_PERCENT, padding, percent);
}
```

After `max_int()`, add:

```c
// A function to take a whole percentage of a size, rounded to the nearest px
static int percent_of(int size, int percent)
{
    return (size * percent + 50) / 100;
}

// A function to size a menu's titles for its button: a percentage FontSize follows the button but
// never drops below the readable minimum; a fixed one is 0 here (its height is title_block)
int layout_title_size(const LayoutParams *params, int button)
{
    if (params->title_size_pct <= 0)
        return 0;
    return max_int(percent_of(button, params->title_size_pct), params->title_min_size);
}

// A function to find the space between a button and its title: a percentage of the button, or
// a fixed number of px capped at half the button
int layout_title_padding(const LayoutParams *params, int button)
{
    if (params->title_padding_pct > 0)
        return percent_of(button, params->title_padding_pct);
    return min_int(max_int(params->title_padding, 0), max_int(button, 0) / 2);
}

// A function to find everything under a button: its padding and its title's line height. A
// percentage size's line is measured per point at a large size; one px more covers rounding.
int layout_title_block(const LayoutParams *params, int button)
{
    int size = layout_title_size(params, button);
    int line = size > 0 ? (size * params->title_line_pm + 999) / 1000 + 1 : 0;
    return max_int(params->title_block, 0) + line + layout_title_padding(params, button);
}

// A function to find the largest button for which `rows` rows fit the area's height, each with
// its title block under it. The block grows with the button, so this searches instead of dividing.
// It returns -1 when not even a 0 px button fits.
static int fit_height(const LayoutParams *params, const LayoutArea *area, int rows, int spacing, int vpad)
{
    int room = area->h - (rows - 1) * spacing - 2 * vpad;
    if (room < 0 || rows * layout_title_block(params, 0) > room)
        return -1;
    int low = 0;
    int high = room / rows;
    while (low < high) {
        int middle = low + (high - low + 1) / 2;
        if (rows * (middle + layout_title_block(params, middle)) <= room)
            low = middle;
        else
            high = middle - 1;
    }
    return low;
}
```

In `layout_compute()`, replace:

```c
        height_fit = fit(area->h, g.rows, spacing, g.vpad, params->title_block);
```

with:

```c
        height_fit = fit_height(params, area, g.rows, spacing, g.vpad);
```

After `g.button = min_int(cap, min_int(width_fit, height_fit));` and the `if (g.button < LAYOUT_MIN_BUTTON)` block, add:

```c
    g.title_size = layout_title_size(params, g.button);
    g.title_padding = layout_title_padding(params, g.button);
    g.title_block = layout_title_block(params, g.button);
```

Then replace the two remaining uses of `params->title_block` with `g.title_block`:

```c
    int block_h = used_rows * (g.button + g.title_block) + (used_rows - 1) * spacing;
    g.x_advance = g.button + spacing;
    g.y_advance = g.button + g.title_block + spacing;
```

`fit()` keeps its `extra` parameter; the width passes 0, as before.

- [ ] **Step 5: Run the tests to verify they pass**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_layout
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release -R layout --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`. Every existing layout check still passes unchanged: their `title_block` of 60 with the new fields at 0 is a constant block, which the search finds exactly as the old division did.

- [ ] **Step 6: Commit.** The launcher does not use the new fields until Task 7; it still builds, because its initializers name their fields.

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/layout.h src/layout.c tests/test_layout.c
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: size titles with the button in the layout (items 18, 19)"
```

---

### Task 6: The settings table and the page model (`settings`)

The pure heart of the screen. It covers:
- **The table:** every setting 3a can change.
- **Per type:** how a value is read from and written to `config.ini`, how Left and Right step it, and how it is described on screen.
- **The pages:** what the remote moves through (rows, the cursor, the page path, Discard, the "incomplete mode" rule, and the save-failed page).

The startup parser switches to this table's parse for the keys it covers, so the parser and the screen cannot disagree about what a line means. `FontSize` joins in Task 7, together with the config fields it needs.

**Files:**
- Create: `src/settings.h`, `src/settings.c`, `tests/test_settings.c`
- Modify: `src/util.c` (`config_handler`: `[Layout]`, `[Background]`, menu sections), `src/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `layout_parse_count()`, `layout_parse_icon_size()`, `layout_parse_title_size()` (Task 5); the `SETTING_*` key names from `<launcher_config.h>`.
- Produces: everything in `settings.h` below. `settings_screen.c` (Tasks 10, 11) uses exactly these names.

- [ ] **Step 1: Create `src/settings.h`** (the test needs the types to compile):

```c
// The settings screen's model: the table of settings it can change, how each one's value is read
// from and written to config.ini, stepped with Left and Right and described on screen, and the
// pages the remote moves through. Pure: no SDL, no globals, so tests/test_settings.c builds it on
// its own. settings_screen.c draws it and applies what it changes.
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#define SETTING_TEXT_MAX 1024  // Longest path a setting holds
#define SETTINGS_MAX_ROWS 64   // Most rows one page shows
#define SETTINGS_MAX_DEPTH 8   // Deepest the pages go

typedef enum {
    SET_TYPE_COUNT,       // A whole number: Rows, Columns
    SET_TYPE_CHOICE,      // One of the background modes
    SET_TYPE_ICON_SIZE,   // IconSize: Fill (no key), or px
    SET_TYPE_COLOR,       // #RRGGBB, stepped through the presets
    SET_TYPE_PATH,        // A file or folder, chosen in the browser, never stepped
    SET_TYPE_SECONDS,     // Whole seconds: SlideshowImageDuration
    SET_TYPE_MILLIS,      // Seconds with decimals in the file, ms here: SlideshowTransitionTime
    SET_TYPE_TITLE_SIZE   // FontSize: a percentage of the button, or a fixed size
} SettingType;

typedef enum {
    SET_REFRESH_NONE,        // The launcher reads the value when it next needs it
    SET_REFRESH_LAYOUT,      // Lay the menu on show out again
    SET_REFRESH_TITLES,      // Render every menu's titles again
    SET_REFRESH_BACKGROUND   // Set the background up again
} SettingRefresh;

typedef enum {
    SET_ID_BACKGROUND_MODE,
    SET_ID_BACKGROUND_COLOR,
    SET_ID_BACKGROUND_IMAGE,
    SET_ID_SLIDESHOW_DIRECTORY,
    SET_ID_SLIDESHOW_DURATION,
    SET_ID_SLIDESHOW_FADE,
    SET_ID_LAYOUT_ROWS,
    SET_ID_LAYOUT_COLUMNS,
    SET_ID_LAYOUT_ICON_SIZE,
    SET_ID_TITLE_SIZE,
    SET_ID_MENU_ROWS,        // The per-menu settings come last: one of each for every menu
    SET_ID_MENU_COLUMNS,
    SET_ID_MENU_ICON_SIZE,
    SET_ID_COUNT
} SettingId;

#define SET_ID_GLOBAL_COUNT SET_ID_MENU_ROWS
#define SET_ID_PER_MENU_COUNT (SET_ID_COUNT - SET_ID_MENU_ROWS)

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} SettingColor;

typedef struct {
    bool inherit;                // The key is absent: a menu follows [Layout], or IconSize is Fill
    int number;                  // COUNT, CHOICE (index), ICON_SIZE (px), SECONDS (s), MILLIS (ms), TITLE_SIZE
    bool percent;                // TITLE_SIZE: `number` is a percentage of the button
    SettingColor color;          // COLOR
    char text[SETTING_TEXT_MAX]; // PATH; "" when none is chosen
} SettingValue;

typedef struct {
    SettingId id;
    const char *label;           // The row's label on screen
    const char *section;         // NULL: the section of the menu being edited
    const char *key;
    const char *alias;           // An older key the parser reads as this one (MaxButtons); NULL if none
    SettingType type;
    int min;                     // COUNT, SECONDS, MILLIS: the limits; CHOICE: the first and last index
    int max;
    bool can_inherit;            // The lowest step removes the key
    SettingRefresh refresh;
} SettingDef;

typedef struct {
    const SettingDef *def;
    int menu;                    // The menu's index for a per-menu setting; -1 otherwise
    SettingValue value;          // What the launcher shows now
    SettingValue entry;          // What it was when settings opened: Discard's target, and the changed test
} SettingSlot;

typedef enum {
    SETTINGS_PAGE_TOP,
    SETTINGS_PAGE_BACKGROUND,
    SETTINGS_PAGE_MENUS,
    SETTINGS_PAGE_MENU,          // One menu's grid, or All menus ([Layout])
    SETTINGS_PAGE_TITLES,
    SETTINGS_PAGE_SAVE_FAILED
} SettingsPage;

typedef enum {
    SETTINGS_ROW_SETTING,        // Left and Right step its value
    SETTINGS_ROW_LINK,           // OK opens another page
    SETTINGS_ROW_BROWSE,         // OK opens the folder browser for its path
    SETTINGS_ROW_ACTION,         // OK does something
    SETTINGS_ROW_DIVIDER,
    SETTINGS_ROW_NOTE            // Text only
} SettingsRowKind;

typedef enum {
    SETTINGS_ACTION_NONE,
    SETTINGS_ACTION_DISCARD,
    SETTINGS_ACTION_RETRY,
    SETTINGS_ACTION_LEAVE
} SettingsAction;

typedef struct {
    SettingsRowKind kind;
    char label[128];
    char value[256];             // What the row shows on the right
    const char *note;            // NOTE rows: the text, owned by the model
    SettingSlot *slot;           // SETTING and BROWSE rows
    SettingsPage target;         // LINK rows
    int menu;                    // LINK rows to a menu's page: its index; -1 for All menus
    SettingsAction action;       // ACTION rows
    bool enabled;                // False: shown greyed, and the cursor skips it
} SettingsRow;

typedef enum {
    SETTINGS_UP,
    SETTINGS_DOWN,
    SETTINGS_LEFT,
    SETTINGS_RIGHT,
    SETTINGS_OK,
    SETTINGS_BACK,
    SETTINGS_HOME,               // Save and close, then go to the default menu
    SETTINGS_CLOSE               // Save and close (the key that opened settings)
} SettingsCommand;

typedef enum {
    SETTINGS_EVENT_NONE,
    SETTINGS_EVENT_MOVED,        // The cursor or the page changed: redraw
    SETTINGS_EVENT_CHANGED,      // `slot`'s value changed from `before`: apply it
    SETTINGS_EVENT_BROWSE,       // Open the folder browser for `slot`
    SETTINGS_EVENT_DISCARD,      // Every value went back to its entry: apply them all
    SETTINGS_EVENT_CLOSE,        // Save and close; apply `slot` first when it is set
    SETTINGS_EVENT_CLOSE_HOME,   // The same, then go to the default menu
    SETTINGS_EVENT_RETRY,        // Try the failed save again
    SETTINGS_EVENT_LEAVE         // Close without saving
} SettingsEventKind;

typedef struct {
    SettingsEventKind kind;
    SettingSlot *slot;
    SettingValue before;
} SettingsEvent;

typedef struct SettingsState SettingsState;

const SettingDef *setting_def(SettingId id);
bool setting_parse(const SettingDef *def, const char *text, SettingValue *value);
void setting_format(const SettingDef *def, const SettingValue *value, char *out, size_t size);
bool setting_equal(const SettingDef *def, const SettingValue *a, const SettingValue *b);
SettingValue setting_step(const SettingDef *def, const SettingValue *current, const SettingValue *entry, int direction);
void setting_describe(const SettingDef *def, const SettingValue *value, const SettingValue *inherited, char *out, size_t size);

SettingsState *settings_create(const char *const *menu_names, int menu_count);
void settings_free(SettingsState *state);
SettingSlot *settings_slot(SettingsState *state, SettingId id, int menu);
int settings_slot_count(const SettingsState *state);
SettingSlot *settings_slot_at(SettingsState *state, int index);
void settings_set_entry(SettingsState *state, SettingId id, int menu, const SettingValue *value);
bool settings_changed(const SettingSlot *slot);
bool settings_any_changed(const SettingsState *state);
int settings_rows(SettingsState *state, SettingsRow *rows, int max);
int settings_cursor(const SettingsState *state);
SettingsPage settings_page(const SettingsState *state);
void settings_path(const SettingsState *state, char *out, size_t size);
int settings_preview_menu(SettingsState *state);
const char *settings_notice(const SettingsState *state);
SettingsEvent settings_command(SettingsState *state, SettingsCommand command);
SettingsEvent settings_choose(SettingsState *state, SettingSlot *slot, const char *path);
void settings_show_save_failed(SettingsState *state, const char *message);

#endif
```

- [ ] **Step 2: Write the failing test.** Create `tests/test_settings.c`:

```c
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "settings.h"

#define ARROW " \xE2\x80\xBA "   // " › " between pages in the page path
#define TIMES "\xC3\x97"         // "×"
#define ELLIPSIS "\xE2\x80\xA6"  // "…"

// A function to parse a value, failing the check when the parser refuses it
static SettingValue parsed(SettingId id, const char *text)
{
    SettingValue value;
    memset(&value, 0, sizeof(value));
    CHECK(setting_parse(setting_def(id), text, &value));
    return value;
}

// A function to write a value back out as config.ini would hold it
static const char *formatted(SettingId id, const SettingValue *value)
{
    static char text[SETTING_TEXT_MAX];
    setting_format(setting_def(id), value, text, sizeof(text));
    return text;
}

// A function to describe a value as the screen shows it
static const char *described(SettingId id, const SettingValue *value, const SettingValue *inherited)
{
    static char text[256];
    setting_describe(setting_def(id), value, inherited, text, sizeof(text));
    return text;
}

// A function to step a value several times one way
static SettingValue stepped(SettingId id, SettingValue value, const SettingValue *entry, int direction, int times)
{
    for (int i = 0; i < times; i++)
        value = setting_step(setting_def(id), &value, entry, direction);
    return value;
}

// A function to test that every type reads what the parser reads and writes it back unchanged
static void test_round_trips(void)
{
    static const struct { SettingId id; const char *text; } cases[] = {
        { SET_ID_LAYOUT_ROWS, "3" },
        { SET_ID_LAYOUT_COLUMNS, "12" },
        { SET_ID_LAYOUT_ROWS, "999999" },
        { SET_ID_LAYOUT_ICON_SIZE, "256" },
        { SET_ID_MENU_ICON_SIZE, "1024" },
        { SET_ID_BACKGROUND_MODE, "Slideshow" },
        { SET_ID_BACKGROUND_COLOR, "#1A2B3C" },
        { SET_ID_BACKGROUND_IMAGE, "C:\\My Pictures\\sunset.jpg" },
        { SET_ID_SLIDESHOW_DIRECTORY, "/home/me/Pictures" },
        { SET_ID_SLIDESHOW_DURATION, "30" },
        { SET_ID_SLIDESHOW_DURATION, "3600" },
        { SET_ID_SLIDESHOW_FADE, "0" },
        { SET_ID_SLIDESHOW_FADE, "0.5" },
        { SET_ID_SLIDESHOW_FADE, "1" },
        { SET_ID_SLIDESHOW_FADE, "1.5" },
        { SET_ID_SLIDESHOW_FADE, "2.5" },
        { SET_ID_SLIDESHOW_FADE, "3" },
        { SET_ID_SLIDESHOW_FADE, "1.25" },
        { SET_ID_SLIDESHOW_FADE, "0.123" },
        { SET_ID_TITLE_SIZE, "14%" },
        { SET_ID_TITLE_SIZE, "36" }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        SettingValue value = parsed(cases[i].id, cases[i].text);
        CHECK_STR(formatted(cases[i].id, &value), cases[i].text);
    }

    // What the parser reads from files written other ways
    SettingValue value = parsed(SET_ID_BACKGROUND_COLOR, "#1a2b3c");
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#1A2B3C");
    value = parsed(SET_ID_BACKGROUND_IMAGE, "\"C:\\My Pictures\\a.png\"");     // Quotes dropped, as clean_path does
    CHECK_STR(value.text, "C:\\My Pictures\\a.png");
    value = parsed(SET_ID_SLIDESHOW_DURATION, "30s");                          // atoi, as before
    CHECK_INT(value.number, 30);
    value = parsed(SET_ID_SLIDESHOW_FADE, "0.7");
    CHECK_INT(value.number, 700);
    value = parsed(SET_ID_SLIDESHOW_FADE, "1.2346");
    CHECK_INT(value.number, 1235);                                             // Rounded to the ms

    // Following the default writes nothing: the key is removed
    SettingValue inherit;
    memset(&inherit, 0, sizeof(inherit));
    inherit.inherit = true;
    CHECK_STR(formatted(SET_ID_MENU_ROWS, &inherit), "");
}

// A function to test the values the parser refuses
static void test_rejects(void)
{
    SettingValue value;
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ROWS), "0", &value));
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ROWS), "3x", &value));
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ICON_SIZE), "31", &value));
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ICON_SIZE), "200px", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_MODE), "color", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_COLOR), "#12345G", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_COLOR), "123456", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_COLOR), "#1234567", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_IMAGE), "", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_DURATION), "4", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_DURATION), "3601", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "-1", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "3.5", &value));
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_SIZE), "0%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_SIZE), "abc", &value));
}

// A function to test how Left and Right step each type
static void test_steps(void)
{
    // Rows stop at their ends, and a value from the file past the last step stays reachable
    SettingValue one = parsed(SET_ID_LAYOUT_ROWS, "1");
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, one, &one, -1, 1).number, 1);
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, one, &one, 1, 1).number, 2);
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, one, &one, 1, 20).number, 10);
    SettingValue big = parsed(SET_ID_LAYOUT_ROWS, "25");
    SettingValue ten = stepped(SET_ID_LAYOUT_ROWS, big, &big, -1, 1);
    CHECK_INT(ten.number, 10);
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, ten, &big, 1, 1).number, 25);

    // A menu's lowest step follows All menus
    SettingValue three = parsed(SET_ID_MENU_ROWS, "3");
    SettingValue value = stepped(SET_ID_MENU_ROWS, three, &three, -1, 3);
    CHECK(value.inherit);
    value = stepped(SET_ID_MENU_ROWS, value, &three, 1, 1);
    CHECK(!value.inherit);
    CHECK_INT(value.number, 1);

    // IconSize: Fill, then the steps, with a value from the file in its sorted place
    SettingValue fill;
    memset(&fill, 0, sizeof(fill));
    fill.inherit = true;
    CHECK_INT(stepped(SET_ID_LAYOUT_ICON_SIZE, fill, &fill, 1, 1).number, 64);
    SettingValue odd = parsed(SET_ID_LAYOUT_ICON_SIZE, "200");
    SettingValue below = parsed(SET_ID_LAYOUT_ICON_SIZE, "192");
    CHECK_INT(stepped(SET_ID_LAYOUT_ICON_SIZE, below, &odd, 1, 1).number, 200);
    CHECK_INT(stepped(SET_ID_LAYOUT_ICON_SIZE, odd, &odd, 1, 1).number, 256);

    // Colours: a custom colour from the file first, then the presets in order
    SettingValue custom = parsed(SET_ID_BACKGROUND_COLOR, "#123456");
    value = stepped(SET_ID_BACKGROUND_COLOR, custom, &custom, 1, 1);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#000000");
    value = stepped(SET_ID_BACKGROUND_COLOR, value, &custom, 1, 1);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#1E1E1E");
    value = stepped(SET_ID_BACKGROUND_COLOR, value, &custom, -1, 2);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#123456");
    SettingValue black = parsed(SET_ID_BACKGROUND_COLOR, "#000000");
    value = stepped(SET_ID_BACKGROUND_COLOR, black, &black, -1, 1);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#000000");
    value = stepped(SET_ID_BACKGROUND_COLOR, black, &black, 1, 20);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#4A1520");

    // Title size: a fixed size from the file, then Small, Medium and Large
    SettingValue fixed = parsed(SET_ID_TITLE_SIZE, "36");
    value = stepped(SET_ID_TITLE_SIZE, fixed, &fixed, 1, 1);
    CHECK_STR(formatted(SET_ID_TITLE_SIZE, &value), "11%");
    value = stepped(SET_ID_TITLE_SIZE, value, &fixed, 1, 5);
    CHECK_STR(formatted(SET_ID_TITLE_SIZE, &value), "17%");
    value = stepped(SET_ID_TITLE_SIZE, value, &fixed, -1, 5);
    CHECK_STR(formatted(SET_ID_TITLE_SIZE, &value), "36");

    // Durations and fades follow their step lists
    SettingValue thirty = parsed(SET_ID_SLIDESHOW_DURATION, "30");
    CHECK_INT(stepped(SET_ID_SLIDESHOW_DURATION, thirty, &thirty, 1, 1).number, 60);
    SettingValue fade = parsed(SET_ID_SLIDESHOW_FADE, "1.5");
    CHECK_INT(stepped(SET_ID_SLIDESHOW_FADE, fade, &fade, 1, 1).number, 2000);

    // The mode stops at both ends; a path never steps
    SettingValue mode = parsed(SET_ID_BACKGROUND_MODE, "Color");
    CHECK_INT(stepped(SET_ID_BACKGROUND_MODE, mode, &mode, -1, 1).number, 0);
    CHECK_INT(stepped(SET_ID_BACKGROUND_MODE, mode, &mode, 1, 9).number, 3);
    SettingValue path = parsed(SET_ID_BACKGROUND_IMAGE, "/a.png");
    SettingValue same = stepped(SET_ID_BACKGROUND_IMAGE, path, &path, 1, 1);
    CHECK_STR(same.text, "/a.png");
}

// A function to test how values are described on screen
static void test_descriptions(void)
{
    SettingValue inherit;
    memset(&inherit, 0, sizeof(inherit));
    inherit.inherit = true;
    SettingValue four = parsed(SET_ID_LAYOUT_ROWS, "4");
    CHECK_STR(described(SET_ID_MENU_ROWS, &inherit, &four), "All menus (4)");
    SettingValue value = parsed(SET_ID_MENU_ROWS, "3");
    CHECK_STR(described(SET_ID_MENU_ROWS, &value, &four), "3");
    CHECK_STR(described(SET_ID_LAYOUT_ICON_SIZE, &inherit, NULL), "Fill");
    CHECK_STR(described(SET_ID_MENU_ICON_SIZE, &inherit, &inherit), "All menus (Fill)");
    SettingValue cap = parsed(SET_ID_LAYOUT_ICON_SIZE, "256");
    CHECK_STR(described(SET_ID_MENU_ICON_SIZE, &inherit, &cap), "All menus (256 px)");
    CHECK_STR(described(SET_ID_LAYOUT_ICON_SIZE, &cap, NULL), "256 px");
    value = parsed(SET_ID_BACKGROUND_MODE, "Color");
    CHECK_STR(described(SET_ID_BACKGROUND_MODE, &value, NULL), "Colour");
    value = parsed(SET_ID_BACKGROUND_COLOR, "#1E1E1E");
    CHECK_STR(described(SET_ID_BACKGROUND_COLOR, &value, NULL), "Charcoal");
    value = parsed(SET_ID_BACKGROUND_COLOR, "#123456");
    CHECK_STR(described(SET_ID_BACKGROUND_COLOR, &value, NULL), "Custom #123456");
    value = parsed(SET_ID_BACKGROUND_IMAGE, "C:\\Pics\\sunset.jpg");
    CHECK_STR(described(SET_ID_BACKGROUND_IMAGE, &value, NULL), "sunset.jpg");
    value = parsed(SET_ID_SLIDESHOW_DIRECTORY, "/home/me/Pictures/");
    CHECK_STR(described(SET_ID_SLIDESHOW_DIRECTORY, &value, NULL), "Pictures");
    memset(&value, 0, sizeof(value));
    CHECK_STR(described(SET_ID_BACKGROUND_IMAGE, &value, NULL), "Choose" ELLIPSIS);
    value = parsed(SET_ID_SLIDESHOW_DURATION, "30");
    CHECK_STR(described(SET_ID_SLIDESHOW_DURATION, &value, NULL), "30 s");
    value = parsed(SET_ID_SLIDESHOW_DURATION, "120");
    CHECK_STR(described(SET_ID_SLIDESHOW_DURATION, &value, NULL), "2 min");
    value = parsed(SET_ID_SLIDESHOW_DURATION, "90");
    CHECK_STR(described(SET_ID_SLIDESHOW_DURATION, &value, NULL), "90 s");
    value = parsed(SET_ID_SLIDESHOW_FADE, "1.5");
    CHECK_STR(described(SET_ID_SLIDESHOW_FADE, &value, NULL), "1.5 s");
    value = parsed(SET_ID_TITLE_SIZE, "11%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Small");
    value = parsed(SET_ID_TITLE_SIZE, "14%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Medium");
    value = parsed(SET_ID_TITLE_SIZE, "17%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Large");
    value = parsed(SET_ID_TITLE_SIZE, "12%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "12%");
    value = parsed(SET_ID_TITLE_SIZE, "36");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Fixed 36");
}

// A function to open a model over two menus, with the values a typical config gives
static SettingsState *open_model(void)
{
    static const char *const names[] = { "Main", "Games" };
    SettingsState *state = settings_create(names, 2);
    SettingValue value;
    value = parsed(SET_ID_BACKGROUND_MODE, "Color");
    settings_set_entry(state, SET_ID_BACKGROUND_MODE, -1, &value);
    value = parsed(SET_ID_BACKGROUND_COLOR, "#000000");
    settings_set_entry(state, SET_ID_BACKGROUND_COLOR, -1, &value);
    memset(&value, 0, sizeof(value));
    settings_set_entry(state, SET_ID_BACKGROUND_IMAGE, -1, &value);
    settings_set_entry(state, SET_ID_SLIDESHOW_DIRECTORY, -1, &value);
    value = parsed(SET_ID_SLIDESHOW_DURATION, "30");
    settings_set_entry(state, SET_ID_SLIDESHOW_DURATION, -1, &value);
    value = parsed(SET_ID_SLIDESHOW_FADE, "1.5");
    settings_set_entry(state, SET_ID_SLIDESHOW_FADE, -1, &value);
    value = parsed(SET_ID_LAYOUT_ROWS, "1");
    settings_set_entry(state, SET_ID_LAYOUT_ROWS, -1, &value);
    value = parsed(SET_ID_LAYOUT_COLUMNS, "4");
    settings_set_entry(state, SET_ID_LAYOUT_COLUMNS, -1, &value);
    memset(&value, 0, sizeof(value));
    value.inherit = true;
    settings_set_entry(state, SET_ID_LAYOUT_ICON_SIZE, -1, &value);
    value = parsed(SET_ID_TITLE_SIZE, "14%");
    settings_set_entry(state, SET_ID_TITLE_SIZE, -1, &value);
    value = parsed(SET_ID_MENU_ROWS, "3");
    settings_set_entry(state, SET_ID_MENU_ROWS, 1, &value);
    value = parsed(SET_ID_MENU_COLUMNS, "6");
    settings_set_entry(state, SET_ID_MENU_COLUMNS, 1, &value);
    return state;
}

// A function to test the top level, the Menus page, one menu's page and Discard
static void test_top_and_menus(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 5);
    CHECK_STR(rows[0].label, "Background");
    CHECK_STR(rows[0].value, "Colour");
    CHECK_STR(rows[1].label, "Menus");
    CHECK_STR(rows[1].value, "2 menus");
    CHECK_STR(rows[2].label, "Titles");
    CHECK_STR(rows[2].value, "Medium");
    CHECK_INT(rows[3].kind, SETTINGS_ROW_DIVIDER);
    CHECK_STR(rows[4].label, "Discard changes");
    CHECK(!rows[4].enabled);
    CHECK_INT(settings_cursor(state), 0);
    CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_NONE);   // Discard is greyed
    CHECK_INT(settings_cursor(state), 2);
    settings_command(state, SETTINGS_UP);

    // Menus: All menus, a divider, then each menu with its grid as columns x rows
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENUS);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus");
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 4);
    CHECK_STR(rows[0].label, "All menus");
    CHECK_STR(rows[0].value, "4 " TIMES " 1");
    CHECK_STR(rows[2].label, "Main");
    CHECK_STR(rows[2].value, "All menus");
    CHECK_STR(rows[3].label, "Games");
    CHECK_STR(rows[3].value, "6 " TIMES " 3");
    CHECK_INT(settings_preview_menu(state), -1);
    settings_command(state, SETTINGS_DOWN);                  // Over the divider, onto Main
    CHECK_INT(settings_cursor(state), 2);
    CHECK_INT(settings_preview_menu(state), 0);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_preview_menu(state), 1);

    // Games' page: its own rows and columns, and IconSize following All menus
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENU);
    CHECK_INT(settings_preview_menu(state), 1);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Games");
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].value, "3");
    CHECK_STR(rows[1].value, "6");
    CHECK_STR(rows[2].value, "All menus (Fill)");

    // Rows 3 -> 2 -> 1 -> All menus (1)
    SettingsEvent event = settings_command(state, SETTINGS_LEFT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK(event.slot == settings_slot(state, SET_ID_MENU_ROWS, 1));
    CHECK_INT(event.before.number, 3);
    settings_command(state, SETTINGS_LEFT);
    settings_command(state, SETTINGS_LEFT);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].value, "All menus (1)");
    CHECK(settings_changed(settings_slot(state, SET_ID_MENU_ROWS, 1)));
    CHECK_INT(settings_command(state, SETTINGS_LEFT).kind, SETTINGS_EVENT_NONE);

    // Back at the top, Discard is offered; it puts every value back and greys out again
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    CHECK_INT(settings_cursor(state), 1);                    // Where it was
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK(rows[4].enabled);
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_cursor(state), 4);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_DISCARD);
    CHECK_INT(settings_slot(state, SET_ID_MENU_ROWS, 1)->value.number, 3);
    CHECK(!settings_any_changed(state));
    CHECK_INT(settings_cursor(state), 2);

    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_CLOSE);
    CHECK_INT(settings_command(state, SETTINGS_HOME).kind, SETTINGS_EVENT_CLOSE_HOME);
    CHECK_INT(settings_command(state, SETTINGS_CLOSE).kind, SETTINGS_EVENT_CLOSE);
    settings_free(state);
}

// A function to test the Background page: its rows per mode, and the incomplete-mode rule
static void test_background_page(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BACKGROUND);
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 2);
    CHECK_STR(rows[0].value, "Colour");
    CHECK_STR(rows[1].label, "Colour");
    CHECK_STR(rows[1].value, "Black");

    // Image with none chosen: Back puts the mode back, and says why
    settings_command(state, SETTINGS_RIGHT);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 2);
    CHECK_INT(rows[1].kind, SETTINGS_ROW_BROWSE);
    CHECK_STR(rows[1].value, "Choose" ELLIPSIS);
    SettingsEvent event = settings_command(state, SETTINGS_BACK);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK(event.slot == settings_slot(state, SET_ID_BACKGROUND_MODE, -1));
    CHECK_INT(event.slot->value.number, 0);
    CHECK(strstr(settings_notice(state), "No image was chosen") != NULL);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    CHECK(!settings_any_changed(state));

    // Image chosen in the browser: Back keeps it
    settings_command(state, SETTINGS_OK);
    settings_command(state, SETTINGS_RIGHT);
    settings_command(state, SETTINGS_DOWN);
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_BROWSE);
    CHECK(event.slot == settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1));
    SettingSlot *image = event.slot;
    CHECK_INT(settings_choose(state, image, "/pics/a.png").kind, SETTINGS_EVENT_CHANGED);
    CHECK_INT(settings_choose(state, image, "/pics/a.png").kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_slot(state, SET_ID_BACKGROUND_MODE, -1)->value.number, 1);

    // Slideshow: a folder, how long each image shows, and the fade
    settings_command(state, SETTINGS_OK);
    settings_command(state, SETTINGS_RIGHT);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 4);
    CHECK_STR(rows[1].label, "Folder");
    CHECK_STR(rows[2].value, "30 s");
    CHECK_STR(rows[3].value, "1.5 s");

    // Transparent: a note in place of rows
    settings_command(state, SETTINGS_RIGHT);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 2);
    CHECK_INT(rows[1].kind, SETTINGS_ROW_NOTE);
    CHECK(rows[1].note != NULL && strstr(rows[1].note, "compositor") != NULL);

    // Closing from the page with Slideshow and no folder puts back the mode the page opened with
    settings_command(state, SETTINGS_LEFT);
    event = settings_command(state, SETTINGS_CLOSE);
    CHECK_INT(event.kind, SETTINGS_EVENT_CLOSE);
    CHECK(event.slot == settings_slot(state, SET_ID_BACKGROUND_MODE, -1));
    CHECK_INT(event.slot->value.number, 1);
    settings_free(state);
}

// A function to test the page a failed save shows
static void test_save_failed_page(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    settings_show_save_failed(state, "Couldn't save to /x/config.ini: permission denied");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_SAVE_FAILED);
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 3);
    CHECK_INT(rows[0].kind, SETTINGS_ROW_NOTE);
    CHECK(strstr(rows[0].note, "permission denied") != NULL);
    CHECK_STR(rows[1].label, "Try again");
    CHECK_STR(rows[2].label, "Leave without saving");
    CHECK_INT(settings_cursor(state), 1);
    CHECK_INT(settings_command(state, SETTINGS_HOME).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_CLOSE).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_RETRY);

    // A retry that fails again stays on the same page, with the new reason
    settings_show_save_failed(state, "Couldn't save to /x/config.ini: the disk is full");
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK(strstr(rows[0].note, "disk is full") != NULL);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_LEAVE);
    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    settings_free(state);
}

int main(void)
{
    test_round_trips();
    test_rejects();
    test_steps();
    test_descriptions();
    test_top_and_menus();
    test_background_page();
    test_save_failed_page();
    return check_report();
}
```

- [ ] **Step 3: Register the test.** In `tests/CMakeLists.txt`, after the `test_config_save` block:

```cmake
# Unit tests for the settings table and the settings screen's pages; settings.c reads counts and
# sizes through layout.c's parsers
add_executable(test_settings test_settings.c "${PROJECT_SOURCE_DIR}/src/settings.c" "${PROJECT_SOURCE_DIR}/src/layout.c")
target_include_directories(test_settings PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME settings COMMAND test_settings)
```

- [ ] **Step 4: Run it to make sure it fails**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_settings
```

Expected: the build fails at the link step: `setting_def`, `setting_parse` and the rest are unresolved.

- [ ] **Step 5: Create `src/settings.c`:**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "settings.h"
#include "layout.h"
#include <launcher_config.h>

#define ARROW " \xE2\x80\xBA "   // " › " between the pages in the page path
#define TIMES "\xC3\x97"         // "×"
#define ELLIPSIS "\xE2\x80\xA6"  // "…"
#define LENGTH(array) ((int) (sizeof(array) / sizeof((array)[0])))

// The background modes, in ModeBackground's order (launcher.h): the file's names and the screen's
static const char *const MODE_NAMES[] = { "Color", "Image", "Slideshow", "Transparent" };
static const char *const MODE_LABELS[] = { "Colour", "Image", "Slideshow", "Transparent" };
#define MODE_IMAGE 1
#define MODE_SLIDESHOW 2

static const struct {
    const char *name;
    SettingColor color;
} PRESETS[] = {
    { "Black",    { 0x00, 0x00, 0x00 } },
    { "Charcoal", { 0x1E, 0x1E, 0x1E } },
    { "Graphite", { 0x33, 0x38, 0x3D } },
    { "Slate",    { 0x2E, 0x34, 0x40 } },
    { "Midnight", { 0x12, 0x1A, 0x2E } },
    { "Navy",     { 0x0B, 0x1F, 0x3A } },
    { "Teal",     { 0x07, 0x60, 0x6C } },
    { "Forest",   { 0x1E, 0x3B, 0x2F } },
    { "Plum",     { 0x3B, 0x1F, 0x3A } },
    { "Burgundy", { 0x4A, 0x15, 0x20 } }
};

static const int ICON_STEPS[] = { 64, 96, 128, 160, 192, 256, 320, 384, 512, 768, 1024 };
static const int SECOND_STEPS[] = { 5, 10, 15, 30, 60, 120, 300, 600, 1800, 3600 };
static const int MILLI_STEPS[] = { 0, 500, 1000, 1500, 2000, 2500, 3000 };
static const int TITLE_STEPS[] = { 11, 14, 17 };  // Small, Medium, Large

static const char *const TRANSPARENT_NOTE =
    "The desktop shows through. On Linux this needs a compositor: see Transparent Backgrounds in the configuration docs.";
static const char *const MENU_NOTE = "The lowest step, All menus, follows the shared grid.";

static const SettingDef DEFS[SET_ID_COUNT] = {
    { SET_ID_BACKGROUND_MODE, "Mode", "Background", SETTING_BACKGROUND_MODE, NULL, SET_TYPE_CHOICE, 0, 3, false, SET_REFRESH_BACKGROUND },
    { SET_ID_BACKGROUND_COLOR, "Colour", "Background", SETTING_BACKGROUND_COLOR, NULL, SET_TYPE_COLOR, 0, 0, false, SET_REFRESH_BACKGROUND },
    { SET_ID_BACKGROUND_IMAGE, "Image", "Background", SETTING_BACKGROUND_IMAGE, NULL, SET_TYPE_PATH, 0, 0, false, SET_REFRESH_BACKGROUND },
    { SET_ID_SLIDESHOW_DIRECTORY, "Folder", "Background", SETTING_SLIDESHOW_DIRECTORY, NULL, SET_TYPE_PATH, 0, 0, false, SET_REFRESH_BACKGROUND },
    { SET_ID_SLIDESHOW_DURATION, "Change every", "Background", SETTING_SLIDESHOW_IMAGE_DURATION, NULL, SET_TYPE_SECONDS, 5, 3600, false, SET_REFRESH_NONE },
    { SET_ID_SLIDESHOW_FADE, "Fade", "Background", SETTING_SLIDESHOW_TRANSITION_TIME, NULL, SET_TYPE_MILLIS, 0, 3000, false, SET_REFRESH_NONE },
    { SET_ID_LAYOUT_ROWS, "Rows", "Layout", SETTING_ROWS, NULL, SET_TYPE_COUNT, 1, 10, false, SET_REFRESH_LAYOUT },
    { SET_ID_LAYOUT_COLUMNS, "Columns", "Layout", SETTING_COLUMNS, SETTING_MAX_BUTTONS, SET_TYPE_COUNT, 1, 12, false, SET_REFRESH_LAYOUT },
    { SET_ID_LAYOUT_ICON_SIZE, "Largest button", "Layout", SETTING_ICON_SIZE, NULL, SET_TYPE_ICON_SIZE, 0, 0, true, SET_REFRESH_LAYOUT },
    { SET_ID_TITLE_SIZE, "Size", "Titles", SETTING_TITLE_FONT_SIZE, NULL, SET_TYPE_TITLE_SIZE, 0, 0, false, SET_REFRESH_TITLES },
    { SET_ID_MENU_ROWS, "Rows", NULL, SETTING_ROWS, NULL, SET_TYPE_COUNT, 1, 10, true, SET_REFRESH_LAYOUT },
    { SET_ID_MENU_COLUMNS, "Columns", NULL, SETTING_COLUMNS, NULL, SET_TYPE_COUNT, 1, 12, true, SET_REFRESH_LAYOUT },
    { SET_ID_MENU_ICON_SIZE, "Largest button", NULL, SETTING_ICON_SIZE, NULL, SET_TYPE_ICON_SIZE, 0, 0, true, SET_REFRESH_LAYOUT }
};

// A function to get a setting's row in the table
const SettingDef *setting_def(SettingId id)
{
    return &DEFS[id];
}

// A function to read "#RRGGBB", strictly: six hex digits
static bool parse_hex_color(const char *text, SettingColor *color)
{
    if (text[0] != '#' || strlen(text) != 7)
        return false;
    unsigned int rgb = 0;
    for (int i = 1; i < 7; i++) {
        char c = text[i];
        unsigned int digit;
        if (c >= '0' && c <= '9')
            digit = (unsigned int) (c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = (unsigned int) (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            digit = (unsigned int) (c - 'A' + 10);
        else
            return false;
        rgb = rgb * 16 + digit;
    }
    color->r = (unsigned char) (rgb >> 16);
    color->g = (unsigned char) ((rgb >> 8) & 0xFF);
    color->b = (unsigned char) (rgb & 0xFF);
    return true;
}

// A function to read a setting's value from config.ini's text, as the launcher has always read it
bool setting_parse(const SettingDef *def, const char *text, SettingValue *value)
{
    SettingValue v;
    memset(&v, 0, sizeof(v));
    switch (def->type) {
        case SET_TYPE_COUNT:
            if (!layout_parse_count(text, &v.number))
                return false;
            break;
        case SET_TYPE_ICON_SIZE:
            if (!layout_parse_icon_size(text, &v.number))
                return false;
            break;
        case SET_TYPE_CHOICE:
            for (v.number = 0; v.number < LENGTH(MODE_NAMES) && strcmp(MODE_NAMES[v.number], text) != 0; v.number++);
            if (v.number == LENGTH(MODE_NAMES))
                return false;
            break;
        case SET_TYPE_COLOR:
            if (!parse_hex_color(text, &v.color))
                return false;
            break;
        case SET_TYPE_PATH: {
            // Quotes round a path are dropped, as clean_path() does
            size_t length = strlen(text);
            if (length == 0 || length >= SETTING_TEXT_MAX)
                return false;
            if (length >= 3 && text[0] == '"' && text[length - 1] == '"') {
                memcpy(v.text, text + 1, length - 2);
                v.text[length - 2] = '\0';
            }
            else
                memcpy(v.text, text, length + 1);
            break;
        }
        case SET_TYPE_SECONDS:
            v.number = atoi(text);
            if (v.number < def->min || v.number > def->max)
                return false;
            break;
        case SET_TYPE_MILLIS: {
            double seconds = atof(text);
            if (seconds < 0.0 || seconds * 1000.0 > (double) def->max + 0.5)
                return false;
            v.number = (int) (seconds * 1000.0 + 0.5);
            break;
        }
        case SET_TYPE_TITLE_SIZE:
            if (!layout_parse_title_size(text, &v.number, &v.percent))
                return false;
            break;
    }
    *value = v;
    return true;
}

// A function to write ms as seconds with only the decimals it needs: 1500 is "1.5", 3000 is "3"
static void format_millis(int ms, char *out, size_t size)
{
    int whole = ms / 1000;
    int part = ms % 1000;
    if (part == 0)
        snprintf(out, size, "%d", whole);
    else if (part % 100 == 0)
        snprintf(out, size, "%d.%d", whole, part / 100);
    else if (part % 10 == 0)
        snprintf(out, size, "%d.%02d", whole, part / 10);
    else
        snprintf(out, size, "%d.%03d", whole, part);
}

// A function to write a value as config.ini holds it; "" for a value that removes the key
void setting_format(const SettingDef *def, const SettingValue *value, char *out, size_t size)
{
    if (size == 0)
        return;
    out[0] = '\0';
    if (value->inherit)
        return;
    switch (def->type) {
        case SET_TYPE_COUNT:
        case SET_TYPE_ICON_SIZE:
        case SET_TYPE_SECONDS:
            snprintf(out, size, "%d", value->number);
            break;
        case SET_TYPE_CHOICE:
            if (value->number >= 0 && value->number < LENGTH(MODE_NAMES))
                snprintf(out, size, "%s", MODE_NAMES[value->number]);
            break;
        case SET_TYPE_COLOR:
            snprintf(out, size, "#%02X%02X%02X", value->color.r, value->color.g, value->color.b);
            break;
        case SET_TYPE_PATH:
            snprintf(out, size, "%s", value->text);
            break;
        case SET_TYPE_MILLIS:
            format_millis(value->number, out, size);
            break;
        case SET_TYPE_TITLE_SIZE:
            if (value->percent)
                snprintf(out, size, "%d%%", value->number);
            else
                snprintf(out, size, "%d", value->number);
            break;
    }
}

// A function to compare two values of a setting
bool setting_equal(const SettingDef *def, const SettingValue *a, const SettingValue *b)
{
    if (a->inherit || b->inherit)
        return a->inherit == b->inherit;
    switch (def->type) {
        case SET_TYPE_COLOR:
            return a->color.r == b->color.r && a->color.g == b->color.g && a->color.b == b->color.b;
        case SET_TYPE_PATH:
            return strcmp(a->text, b->text) == 0;
        case SET_TYPE_TITLE_SIZE:
            return a->percent == b->percent && a->number == b->number;
        default:
            return a->number == b->number;
    }
}

// One step a row can take; a SettingValue without its path buffer
typedef struct {
    bool inherit;
    int number;
    bool percent;
    SettingColor color;
} Candidate;

#define MAX_CANDIDATES 48

// A function to find a colour among the presets; -1 when it is not one
static int preset_index(SettingColor color)
{
    for (int i = 0; i < LENGTH(PRESETS); i++) {
        if (PRESETS[i].color.r == color.r && PRESETS[i].color.g == color.g && PRESETS[i].color.b == color.b)
            return i;
    }
    return -1;
}

// A function to give a step its place: following the default first, then a custom colour or a
// fixed title size, then the rest in order
static long sort_key(const SettingDef *def, const Candidate *c)
{
    if (c->inherit)
        return LONG_MIN;
    if (def->type == SET_TYPE_COLOR)
        return preset_index(c->color);
    if (def->type == SET_TYPE_TITLE_SIZE)
        return c->percent ? c->number : -1;
    return c->number;
}

// A function to compare two steps
static bool same_candidate(const SettingDef *def, const Candidate *a, const Candidate *b)
{
    if (a->inherit || b->inherit)
        return a->inherit == b->inherit;
    if (def->type == SET_TYPE_COLOR)
        return a->color.r == b->color.r && a->color.g == b->color.g && a->color.b == b->color.b;
    if (def->type == SET_TYPE_TITLE_SIZE)
        return a->percent == b->percent && a->number == b->number;
    return a->number == b->number;
}

// A function to add a step to a list once
static void add_candidate(const SettingDef *def, Candidate *list, int *count, Candidate c)
{
    for (int i = 0; i < *count; i++) {
        if (same_candidate(def, &list[i], &c))
            return;
    }
    if (*count < MAX_CANDIDATES)
        list[(*count)++] = c;
}

// A function to make a step from a number
static Candidate number_candidate(int number, bool percent)
{
    Candidate c;
    memset(&c, 0, sizeof(c));
    c.number = number;
    c.percent = percent;
    return c;
}

// A function to make a step from a value
static Candidate value_candidate(const SettingValue *value)
{
    Candidate c;
    memset(&c, 0, sizeof(c));
    c.inherit = value->inherit;
    c.number = value->number;
    c.percent = value->percent;
    c.color = value->color;
    return c;
}

// A function to list a row's steps in order. The value on entry and the current value are always
// among them, in their sorted place, so the user can step back to what the file said.
static int build_candidates(const SettingDef *def, const SettingValue *current, const SettingValue *entry, Candidate *list)
{
    int count = 0;
    if (def->can_inherit) {
        Candidate inherit = number_candidate(0, false);
        inherit.inherit = true;
        add_candidate(def, list, &count, inherit);
    }
    switch (def->type) {
        case SET_TYPE_COUNT:
        case SET_TYPE_CHOICE:
            for (int n = def->min; n <= def->max; n++)
                add_candidate(def, list, &count, number_candidate(n, false));
            break;
        case SET_TYPE_ICON_SIZE:
            for (int i = 0; i < LENGTH(ICON_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(ICON_STEPS[i], false));
            break;
        case SET_TYPE_SECONDS:
            for (int i = 0; i < LENGTH(SECOND_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(SECOND_STEPS[i], false));
            break;
        case SET_TYPE_MILLIS:
            for (int i = 0; i < LENGTH(MILLI_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(MILLI_STEPS[i], false));
            break;
        case SET_TYPE_TITLE_SIZE:
            for (int i = 0; i < LENGTH(TITLE_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(TITLE_STEPS[i], true));
            break;
        case SET_TYPE_COLOR:
            for (int i = 0; i < LENGTH(PRESETS); i++) {
                Candidate c = number_candidate(0, false);
                c.color = PRESETS[i].color;
                add_candidate(def, list, &count, c);
            }
            break;
        case SET_TYPE_PATH:
            break;
    }
    add_candidate(def, list, &count, value_candidate(entry));
    add_candidate(def, list, &count, value_candidate(current));

    // Put them in order: an insertion sort, since a row has a few dozen steps at most
    for (int i = 1; i < count; i++) {
        Candidate c = list[i];
        long key = sort_key(def, &c);
        int j = i - 1;
        while (j >= 0 && sort_key(def, &list[j]) > key) {
            list[j + 1] = list[j];
            j--;
        }
        list[j + 1] = c;
    }
    return count;
}

// A function to step a value one place left (direction < 0) or right; it stops at the ends
SettingValue setting_step(const SettingDef *def, const SettingValue *current, const SettingValue *entry, int direction)
{
    SettingValue result = *current;
    if (def->type == SET_TYPE_PATH || direction == 0)
        return result;
    Candidate list[MAX_CANDIDATES];
    int count = build_candidates(def, current, entry, list);
    Candidate now = value_candidate(current);
    int index = -1;
    for (int i = 0; i < count && index < 0; i++) {
        if (same_candidate(def, &list[i], &now))
            index = i;
    }
    int next = index + (direction > 0 ? 1 : -1);
    if (index < 0 || next < 0 || next >= count)
        return result;
    result.inherit = list[next].inherit;
    result.number = list[next].number;
    result.percent = list[next].percent;
    result.color = list[next].color;
    return result;
}

// A function to find the last name in a path, ignoring a trailing separator
static void base_name(const char *path, char *out, size_t size)
{
    size_t length = strlen(path);
    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\'))
        length--;
    size_t start = length;
    while (start > 0 && path[start - 1] != '/' && path[start - 1] != '\\')
        start--;
    snprintf(out, size, "%.*s", (int) (length - start), path + start);
}

// A function to describe a value as the screen shows it. `inherited` is the value a per-menu
// setting follows (from [Layout]); NULL for the others.
void setting_describe(const SettingDef *def, const SettingValue *value, const SettingValue *inherited, char *out, size_t size)
{
    char text[64];
    switch (def->type) {
        case SET_TYPE_COUNT:
            if (value->inherit)
                snprintf(out, size, "All menus (%d)", inherited != NULL ? inherited->number : 0);
            else
                snprintf(out, size, "%d", value->number);
            break;
        case SET_TYPE_ICON_SIZE:
            if (!value->inherit)
                snprintf(out, size, "%d px", value->number);
            else if (def->section != NULL)
                snprintf(out, size, "Fill");
            else if (inherited == NULL || inherited->inherit)
                snprintf(out, size, "All menus (Fill)");
            else
                snprintf(out, size, "All menus (%d px)", inherited->number);
            break;
        case SET_TYPE_CHOICE:
            snprintf(out, size, "%s", value->number >= 0 && value->number < LENGTH(MODE_LABELS) ? MODE_LABELS[value->number] : "?");
            break;
        case SET_TYPE_COLOR: {
            int preset = preset_index(value->color);
            if (preset >= 0)
                snprintf(out, size, "%s", PRESETS[preset].name);
            else
                snprintf(out, size, "Custom #%02X%02X%02X", value->color.r, value->color.g, value->color.b);
            break;
        }
        case SET_TYPE_PATH:
            if (value->text[0] == '\0')
                snprintf(out, size, "Choose" ELLIPSIS);
            else
                base_name(value->text, out, size);
            break;
        case SET_TYPE_SECONDS:
            if (value->number >= 60 && value->number % 60 == 0)
                snprintf(out, size, "%d min", value->number / 60);
            else
                snprintf(out, size, "%d s", value->number);
            break;
        case SET_TYPE_MILLIS:
            format_millis(value->number, text, sizeof(text));
            snprintf(out, size, "%s s", text);
            break;
        case SET_TYPE_TITLE_SIZE:
            if (!value->percent)
                snprintf(out, size, "Fixed %d", value->number);
            else if (value->number == TITLE_STEPS[0])
                snprintf(out, size, "Small");
            else if (value->number == TITLE_STEPS[1])
                snprintf(out, size, "Medium");
            else if (value->number == TITLE_STEPS[2])
                snprintf(out, size, "Large");
            else
                snprintf(out, size, "%d%%", value->number);
            break;
    }
}

// One page on the stack
typedef struct {
    SettingsPage page;
    int menu;          // MENU pages: the menu's index, -1 for All menus
    int cursor;
    int entry_mode;    // The background mode when the page opened, for the incomplete-mode rule
} PageRef;

struct SettingsState {
    SettingSlot *slots;
    int slot_count;
    char **names;
    int menu_count;
    PageRef stack[SETTINGS_MAX_DEPTH];
    int depth;
    char notice[256];
    char failure[1400];
};

// A function to start the model over the launcher's menus; the caller then sets every entry value
SettingsState *settings_create(const char *const *menu_names, int menu_count)
{
    SettingsState *state = calloc(1, sizeof(SettingsState));
    if (state == NULL)
        return NULL;
    state->menu_count = menu_count;
    state->slot_count = SET_ID_GLOBAL_COUNT + menu_count * SET_ID_PER_MENU_COUNT;
    state->slots = calloc((size_t) state->slot_count, sizeof(SettingSlot));
    state->names = calloc((size_t) (menu_count > 0 ? menu_count : 1), sizeof(char*));
    if (state->slots == NULL || state->names == NULL) {
        settings_free(state);
        return NULL;
    }
    for (int i = 0; i < menu_count; i++)
        state->names[i] = strdup(menu_names[i]);
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        state->slots[id].def = &DEFS[id];
        state->slots[id].menu = -1;
    }
    for (int m = 0; m < menu_count; m++) {
        for (int k = 0; k < SET_ID_PER_MENU_COUNT; k++) {
            SettingSlot *slot = &state->slots[SET_ID_GLOBAL_COUNT + m * SET_ID_PER_MENU_COUNT + k];
            slot->def = &DEFS[SET_ID_MENU_ROWS + k];
            slot->menu = m;
            slot->value.inherit = true;
            slot->entry.inherit = true;
        }
    }
    state->stack[0].page = SETTINGS_PAGE_TOP;
    state->stack[0].menu = -1;
    return state;
}

// A function to free the model
void settings_free(SettingsState *state)
{
    if (state == NULL)
        return;
    for (int i = 0; state->names != NULL && i < state->menu_count; i++)
        free(state->names[i]);
    free(state->names);
    free(state->slots);
    free(state);
}

// A function to find a setting's slot; `menu` matters only for the per-menu settings
SettingSlot *settings_slot(SettingsState *state, SettingId id, int menu)
{
    if (id < SET_ID_GLOBAL_COUNT)
        return &state->slots[id];
    if (id >= SET_ID_COUNT || menu < 0 || menu >= state->menu_count)
        return NULL;
    return &state->slots[SET_ID_GLOBAL_COUNT + menu * SET_ID_PER_MENU_COUNT + (id - SET_ID_MENU_ROWS)];
}

// A function to count the slots, for walking them all
int settings_slot_count(const SettingsState *state)
{
    return state->slot_count;
}

// A function to get a slot by its place
SettingSlot *settings_slot_at(SettingsState *state, int index)
{
    return index >= 0 && index < state->slot_count ? &state->slots[index] : NULL;
}

// A function to set a setting's value as it is when settings open
void settings_set_entry(SettingsState *state, SettingId id, int menu, const SettingValue *value)
{
    SettingSlot *slot = settings_slot(state, id, menu);
    if (slot != NULL) {
        slot->value = *value;
        slot->entry = *value;
    }
}

// A function to tell whether a setting differs from what it was when settings opened
bool settings_changed(const SettingSlot *slot)
{
    return !setting_equal(slot->def, &slot->value, &slot->entry);
}

// A function to tell whether anything has changed
bool settings_any_changed(const SettingsState *state)
{
    for (int i = 0; i < state->slot_count; i++) {
        if (settings_changed(&state->slots[i]))
            return true;
    }
    return false;
}

// A function to get the value a per-menu setting follows when it inherits
static const SettingValue *inherited_value(SettingsState *state, const SettingSlot *slot)
{
    switch (slot->def->id) {
        case SET_ID_MENU_ROWS:
            return &settings_slot(state, SET_ID_LAYOUT_ROWS, -1)->value;
        case SET_ID_MENU_COLUMNS:
            return &settings_slot(state, SET_ID_LAYOUT_COLUMNS, -1)->value;
        case SET_ID_MENU_ICON_SIZE:
            return &settings_slot(state, SET_ID_LAYOUT_ICON_SIZE, -1)->value;
        default:
            return NULL;
    }
}

// A function to start a row of a kind
static SettingsRow new_row(SettingsRowKind kind, const char *label)
{
    SettingsRow row;
    memset(&row, 0, sizeof(row));
    row.kind = kind;
    row.menu = -1;
    row.enabled = true;
    snprintf(row.label, sizeof(row.label), "%s", label);
    return row;
}

// A function to make a setting's row: stepped, or opening the browser for a path
static SettingsRow setting_row(SettingsState *state, SettingSlot *slot)
{
    SettingsRow row = new_row(slot->def->type == SET_TYPE_PATH ? SETTINGS_ROW_BROWSE : SETTINGS_ROW_SETTING, slot->def->label);
    row.slot = slot;
    setting_describe(slot->def, &slot->value, inherited_value(state, slot), row.value, sizeof(row.value));
    return row;
}

// A function to make a row that opens a page
static SettingsRow link_row(const char *label, const char *value, SettingsPage target, int menu)
{
    SettingsRow row = new_row(SETTINGS_ROW_LINK, label);
    snprintf(row.value, sizeof(row.value), "%s", value);
    row.target = target;
    row.menu = menu;
    return row;
}

// A function to make a row that does something
static SettingsRow action_row(const char *label, SettingsAction action, bool enabled)
{
    SettingsRow row = new_row(SETTINGS_ROW_ACTION, label);
    row.action = action;
    row.enabled = enabled;
    return row;
}

// A function to make a row of text
static SettingsRow note_row(const char *text)
{
    SettingsRow row = new_row(SETTINGS_ROW_NOTE, "");
    row.note = text;
    return row;
}

// A function to summarise a menu's grid (columns x rows), or say it follows All menus
static void grid_summary(SettingsState *state, int menu, char *out, size_t size)
{
    const SettingValue *rows = &settings_slot(state, SET_ID_LAYOUT_ROWS, -1)->value;
    const SettingValue *columns = &settings_slot(state, SET_ID_LAYOUT_COLUMNS, -1)->value;
    if (menu >= 0) {
        const SettingValue *menu_rows = &settings_slot(state, SET_ID_MENU_ROWS, menu)->value;
        const SettingValue *menu_columns = &settings_slot(state, SET_ID_MENU_COLUMNS, menu)->value;
        const SettingValue *menu_icon = &settings_slot(state, SET_ID_MENU_ICON_SIZE, menu)->value;
        if (menu_rows->inherit && menu_columns->inherit && menu_icon->inherit) {
            snprintf(out, size, "All menus");
            return;
        }
        if (!menu_rows->inherit)
            rows = menu_rows;
        if (!menu_columns->inherit)
            columns = menu_columns;
    }
    snprintf(out, size, "%d " TIMES " %d", columns->number, rows->number);
}

// A function to add a row when there is room
static int add_row(SettingsRow *rows, int count, int max, SettingsRow row)
{
    if (count < max)
        rows[count] = row;
    return count + 1;
}

// A function to list the rows of the page on show
int settings_rows(SettingsState *state, SettingsRow *rows, int max)
{
    const PageRef *top = &state->stack[state->depth];
    char text[256];
    int n = 0;
    switch (top->page) {
        case SETTINGS_PAGE_TOP: {
            SettingSlot *mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1);
            setting_describe(mode->def, &mode->value, NULL, text, sizeof(text));
            n = add_row(rows, n, max, link_row("Background", text, SETTINGS_PAGE_BACKGROUND, -1));
            snprintf(text, sizeof(text), state->menu_count == 1 ? "%d menu" : "%d menus", state->menu_count);
            n = add_row(rows, n, max, link_row("Menus", text, SETTINGS_PAGE_MENUS, -1));
            SettingSlot *size = settings_slot(state, SET_ID_TITLE_SIZE, -1);
            setting_describe(size->def, &size->value, NULL, text, sizeof(text));
            n = add_row(rows, n, max, link_row("Titles", text, SETTINGS_PAGE_TITLES, -1));
            n = add_row(rows, n, max, new_row(SETTINGS_ROW_DIVIDER, ""));
            n = add_row(rows, n, max, action_row("Discard changes", SETTINGS_ACTION_DISCARD, settings_any_changed(state)));
            break;
        }
        case SETTINGS_PAGE_BACKGROUND: {
            SettingSlot *mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1);
            n = add_row(rows, n, max, setting_row(state, mode));
            if (mode->value.number == 0)
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_BACKGROUND_COLOR, -1)));
            else if (mode->value.number == MODE_IMAGE)
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1)));
            else if (mode->value.number == MODE_SLIDESHOW) {
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_SLIDESHOW_DIRECTORY, -1)));
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_SLIDESHOW_DURATION, -1)));
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_SLIDESHOW_FADE, -1)));
            }
            else
                n = add_row(rows, n, max, note_row(TRANSPARENT_NOTE));
            break;
        }
        case SETTINGS_PAGE_MENUS:
            grid_summary(state, -1, text, sizeof(text));
            n = add_row(rows, n, max, link_row("All menus", text, SETTINGS_PAGE_MENU, -1));
            n = add_row(rows, n, max, new_row(SETTINGS_ROW_DIVIDER, ""));
            for (int m = 0; m < state->menu_count; m++) {
                grid_summary(state, m, text, sizeof(text));
                n = add_row(rows, n, max, link_row(state->names[m], text, SETTINGS_PAGE_MENU, m));
            }
            break;
        case SETTINGS_PAGE_MENU: {
            int m = top->menu;
            n = add_row(rows, n, max, setting_row(state, settings_slot(state, m < 0 ? SET_ID_LAYOUT_ROWS : SET_ID_MENU_ROWS, m)));
            n = add_row(rows, n, max, setting_row(state, settings_slot(state, m < 0 ? SET_ID_LAYOUT_COLUMNS : SET_ID_MENU_COLUMNS, m)));
            n = add_row(rows, n, max, setting_row(state, settings_slot(state, m < 0 ? SET_ID_LAYOUT_ICON_SIZE : SET_ID_MENU_ICON_SIZE, m)));
            if (m >= 0)
                n = add_row(rows, n, max, note_row(MENU_NOTE));
            break;
        }
        case SETTINGS_PAGE_TITLES:
            n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_TITLE_SIZE, -1)));
            break;
        case SETTINGS_PAGE_SAVE_FAILED:
            n = add_row(rows, n, max, note_row(state->failure));
            n = add_row(rows, n, max, action_row("Try again", SETTINGS_ACTION_RETRY, true));
            n = add_row(rows, n, max, action_row("Leave without saving", SETTINGS_ACTION_LEAVE, true));
            break;
    }
    return n < max ? n : max;
}

// A function to tell whether the cursor may rest on a row
static bool selectable(const SettingsRow *row)
{
    return row->enabled && row->kind != SETTINGS_ROW_DIVIDER && row->kind != SETTINGS_ROW_NOTE;
}

// A function to keep the cursor on a row it may rest on, since the rows under it can change
static void fix_cursor(SettingsState *state)
{
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    PageRef *top = &state->stack[state->depth];
    if (top->cursor >= count)
        top->cursor = count - 1;
    if (top->cursor < 0)
        top->cursor = 0;
    if (count == 0 || selectable(&rows[top->cursor]))
        return;
    for (int i = top->cursor; i < count; i++) {
        if (selectable(&rows[i])) {
            top->cursor = i;
            return;
        }
    }
    for (int i = top->cursor; i >= 0; i--) {
        if (selectable(&rows[i])) {
            top->cursor = i;
            return;
        }
    }
}

// A function to open a page on top of the one on show
static void push_page(SettingsState *state, SettingsPage page, int menu)
{
    if (state->depth + 1 >= SETTINGS_MAX_DEPTH)
        return;
    state->depth++;
    PageRef *top = &state->stack[state->depth];
    memset(top, 0, sizeof(*top));
    top->page = page;
    top->menu = menu;
    top->entry_mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1)->value.number;
    fix_cursor(state);
}

// A function to apply the incomplete-mode rule when the Background page is left: Image with no
// image, or Slideshow with no folder, goes back to the mode the page opened with
static SettingsEvent leave_background(SettingsState *state)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    SettingSlot *mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1);
    const char *missing = NULL;
    if (mode->value.number == MODE_IMAGE && settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1)->value.text[0] == '\0')
        missing = "No image was chosen";
    else if (mode->value.number == MODE_SLIDESHOW && settings_slot(state, SET_ID_SLIDESHOW_DIRECTORY, -1)->value.text[0] == '\0')
        missing = "No folder was chosen";
    if (missing == NULL)
        return event;
    event.before = mode->value;
    mode->value.number = state->stack[state->depth].entry_mode;
    snprintf(state->notice, sizeof(state->notice), "%s, so Mode went back to %s", missing, MODE_LABELS[mode->value.number]);
    event.kind = SETTINGS_EVENT_CHANGED;
    event.slot = mode;
    return event;
}

// A function to act on one key of the remote
SettingsEvent settings_command(SettingsState *state, SettingsCommand command)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    state->notice[0] = '\0';
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    PageRef *top = &state->stack[state->depth];
    SettingsRow *row = top->cursor >= 0 && top->cursor < count ? &rows[top->cursor] : NULL;
    bool failure_page = top->page == SETTINGS_PAGE_SAVE_FAILED;

    switch (command) {
        case SETTINGS_UP:
        case SETTINGS_DOWN: {
            int step = command == SETTINGS_UP ? -1 : 1;
            for (int i = top->cursor + step; i >= 0 && i < count; i += step) {
                if (selectable(&rows[i])) {
                    top->cursor = i;
                    event.kind = SETTINGS_EVENT_MOVED;
                    break;
                }
            }
            break;
        }
        case SETTINGS_LEFT:
        case SETTINGS_RIGHT:
            if (row != NULL && row->kind == SETTINGS_ROW_SETTING) {
                SettingSlot *slot = row->slot;
                SettingValue next = setting_step(slot->def, &slot->value, &slot->entry, command == SETTINGS_RIGHT ? 1 : -1);
                if (!setting_equal(slot->def, &next, &slot->value)) {
                    event.before = slot->value;
                    slot->value = next;
                    event.kind = SETTINGS_EVENT_CHANGED;
                    event.slot = slot;
                }
            }
            break;
        case SETTINGS_OK:
            if (row == NULL || !selectable(row))
                break;
            if (row->kind == SETTINGS_ROW_LINK) {
                push_page(state, row->target, row->menu);
                event.kind = SETTINGS_EVENT_MOVED;
            }
            else if (row->kind == SETTINGS_ROW_BROWSE) {
                event.kind = SETTINGS_EVENT_BROWSE;
                event.slot = row->slot;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_DISCARD) {
                for (int i = 0; i < state->slot_count; i++)
                    state->slots[i].value = state->slots[i].entry;
                event.kind = SETTINGS_EVENT_DISCARD;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_RETRY)
                event.kind = SETTINGS_EVENT_RETRY;
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_LEAVE)
                event.kind = SETTINGS_EVENT_LEAVE;
            break;
        case SETTINGS_BACK:
            if (state->depth == 0) {
                event.kind = SETTINGS_EVENT_CLOSE;
                break;
            }
            if (top->page == SETTINGS_PAGE_BACKGROUND)
                event = leave_background(state);
            state->depth--;
            if (event.kind == SETTINGS_EVENT_NONE)
                event.kind = SETTINGS_EVENT_MOVED;
            break;
        case SETTINGS_HOME:
        case SETTINGS_CLOSE:
            if (failure_page)
                break;
            if (top->page == SETTINGS_PAGE_BACKGROUND)
                event = leave_background(state);
            event.kind = command == SETTINGS_HOME ? SETTINGS_EVENT_CLOSE_HOME : SETTINGS_EVENT_CLOSE;
            break;
    }
    fix_cursor(state);
    return event;
}

// A function to set a path chosen in the folder browser
SettingsEvent settings_choose(SettingsState *state, SettingSlot *slot, const char *path)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    state->notice[0] = '\0';
    if (strlen(path) >= SETTING_TEXT_MAX || strcmp(slot->value.text, path) == 0)
        return event;
    event.before = slot->value;
    snprintf(slot->value.text, SETTING_TEXT_MAX, "%s", path);
    event.kind = SETTINGS_EVENT_CHANGED;
    event.slot = slot;
    return event;
}

// A function to show why a save failed, with Try again and Leave without saving
void settings_show_save_failed(SettingsState *state, const char *message)
{
    snprintf(state->failure, sizeof(state->failure), "%s", message);
    if (state->stack[state->depth].page != SETTINGS_PAGE_SAVE_FAILED)
        push_page(state, SETTINGS_PAGE_SAVE_FAILED, -1);
    else
        fix_cursor(state);
}

// A function to get the cursor on the page on show
int settings_cursor(const SettingsState *state)
{
    return state->stack[state->depth].cursor;
}

// A function to get the page on show
SettingsPage settings_page(const SettingsState *state)
{
    return state->stack[state->depth].page;
}

// A function to write the path of pages to the one on show: "Settings › Menus › Games"
void settings_path(const SettingsState *state, char *out, size_t size)
{
    snprintf(out, size, "Settings");
    for (int i = 1; i <= state->depth; i++) {
        const PageRef *page = &state->stack[i];
        const char *name = "";
        switch (page->page) {
            case SETTINGS_PAGE_BACKGROUND: name = "Background"; break;
            case SETTINGS_PAGE_MENUS: name = "Menus"; break;
            case SETTINGS_PAGE_MENU: name = page->menu < 0 ? "All menus" : state->names[page->menu]; break;
            case SETTINGS_PAGE_TITLES: name = "Titles"; break;
            case SETTINGS_PAGE_SAVE_FAILED: name = "Couldn't save"; break;
            case SETTINGS_PAGE_TOP: break;
        }
        size_t used = strlen(out);
        snprintf(out + used, size - used, "%s%s", ARROW, name);
    }
}

// A function to say which menu the preview should show: the one a MENU page edits, or the one
// highlighted on the Menus page; -1 for the menu settings were opened from
int settings_preview_menu(SettingsState *state)
{
    const PageRef *top = &state->stack[state->depth];
    if (top->page == SETTINGS_PAGE_MENU)
        return top->menu;
    if (top->page == SETTINGS_PAGE_MENUS) {
        SettingsRow rows[SETTINGS_MAX_ROWS];
        int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
        if (top->cursor >= 0 && top->cursor < count && rows[top->cursor].kind == SETTINGS_ROW_LINK)
            return rows[top->cursor].menu;
    }
    return -1;
}

// A function to get the note the last command left for the caption, or ""
const char *settings_notice(const SettingsState *state)
{
    return state->notice;
}
```

In `src/CMakeLists.txt`, add `settings.c` and `settings.h` to `SOURCES` after `config_save.h`.

- [ ] **Step 6: Run the test to verify it passes**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_settings
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release -R settings --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 7: Read the table's keys through `setting_parse()` at startup.** In `src/util.c`, add `#include "settings.h"` after `#include "fileio.h"`. Replace the start of the `[Layout]` branch, from its `else if (MATCH(section, "Layout")) {` line through the `IconSize` block's closing brace, with the code below. It handles `Columns`, `MaxButtons`, `Rows` and `IconSize`; the `IconSpacing` and `VCenter` branches after it and the section's closing brace stay as they are.

```c
    else if (MATCH(section, "Layout")) {
        SettingValue parsed;
        if (MATCH(name, SETTING_MAX_BUTTONS)) {
            if (!setting_parse(setting_def(SET_ID_LAYOUT_COLUMNS), value, &parsed))
                log_error("Invalid %s value '%s' in [Layout], ignoring it", SETTING_MAX_BUTTONS, value);
            else if (!columns_set)
                config.max_buttons = (unsigned int) parsed.number;
        }
        else if (MATCH(name, SETTING_COLUMNS)) {
            if (setting_parse(setting_def(SET_ID_LAYOUT_COLUMNS), value, &parsed)) {
                config.max_buttons = (unsigned int) parsed.number;
                columns_set = true;
            }
            else
                log_error("Invalid %s value '%s' in [Layout], ignoring it", SETTING_COLUMNS, value);
        }
        else if (MATCH(name, SETTING_ROWS)) {
            if (setting_parse(setting_def(SET_ID_LAYOUT_ROWS), value, &parsed))
                config.rows = (unsigned int) parsed.number;
            else
                log_error("Invalid %s value '%s' in [Layout], ignoring it", SETTING_ROWS, value);
        }
        else if (MATCH(name, SETTING_ICON_SIZE)) {
            if (setting_parse(setting_def(SET_ID_LAYOUT_ICON_SIZE), value, &parsed))
                config.icon_size = (Uint16) parsed.number;
            else
                log_error("Invalid %s value '%s' in [Layout] (use a whole number from %i to %i), ignoring it",
                    SETTING_ICON_SIZE, value, MIN_ICON_SIZE, MAX_ICON_SIZE);
        }
```

(the `IconSpacing` and `VCenter` branches that follow are unchanged).

Replace the `[Background]` branch's `Mode`, `Color`, `Image`, `SlideshowDirectory`, `SlideshowImageDuration` and `SlideshowTransitionTime` handling with a lookup through the table. Keep `ChromaKeyColor`, `Overlay`, `OverlayColor` and `OverlayOpacity` as they are:

```c
    else if (MATCH(section, "Background")) {
        SettingId id = SET_ID_COUNT;
        if (MATCH(name, SETTING_BACKGROUND_MODE))
            id = SET_ID_BACKGROUND_MODE;
        else if (MATCH(name, SETTING_BACKGROUND_COLOR))
            id = SET_ID_BACKGROUND_COLOR;
        else if (MATCH(name, SETTING_BACKGROUND_IMAGE))
            id = SET_ID_BACKGROUND_IMAGE;
        else if (MATCH(name, SETTING_SLIDESHOW_DIRECTORY))
            id = SET_ID_SLIDESHOW_DIRECTORY;
        else if (MATCH(name, SETTING_SLIDESHOW_IMAGE_DURATION))
            id = SET_ID_SLIDESHOW_DURATION;
        else if (MATCH(name, SETTING_SLIDESHOW_TRANSITION_TIME))
            id = SET_ID_SLIDESHOW_FADE;
        if (id != SET_ID_COUNT) {
            SettingValue parsed;
            if (setting_parse(setting_def(id), value, &parsed))
                store_background_setting(id, &parsed);
            else
                log_error("Invalid %s value '%s' in [Background], ignoring it", name, value);
        }
        else if (MATCH(name, SETTING_CHROMA_KEY_COLOR))
            hex_to_color(value, &config.chroma_key_color);
        else if (MATCH(name, SETTING_BACKGROUND_OVERLAY))
            convert_bool(value, &config.background_overlay);
        else if (MATCH(name, SETTING_BACKGROUND_OVERLAY_COLOR))
            hex_to_color(value, &config.background_overlay_color);
        else if (MATCH(name, SETTING_BACKGROUND_OVERLAY_OPACITY)) {
            if (is_percent(value))
                copy_string(config.background_overlay_opacity, value, sizeof(config.background_overlay_opacity));
        }
    }
```

Add the helper above `config_handler()` (and its prototype with the other `static` prototypes at the top of the file):

```c
// A function to store a [Background] setting read through the settings table
static void store_background_setting(SettingId id, const SettingValue *value)
{
    switch (id) {
        case SET_ID_BACKGROUND_MODE:
            config.background_mode = (ModeBackground) value->number;
            break;
        case SET_ID_BACKGROUND_COLOR:
            config.background_color.r = value->color.r;
            config.background_color.g = value->color.g;
            config.background_color.b = value->color.b;
            break;
        case SET_ID_BACKGROUND_IMAGE:
            free(config.background_image);
            config.background_image = strdup(value->text);
            break;
        case SET_ID_SLIDESHOW_DIRECTORY:
            free(config.slideshow_directory);
            config.slideshow_directory = strdup(value->text);
            break;
        case SET_ID_SLIDESHOW_DURATION:
            config.slideshow_image_duration = (Uint32) value->number * 1000;
            break;
        case SET_ID_SLIDESHOW_FADE:
            config.slideshow_transition_time = (Uint32) value->number;
            break;
        default:
            break;
    }
}
```

In the menu branch, replace the per-menu layout block:

```c
        if (layout_key && strchr(value, ';') == NULL) {
            int count = 0;
            bool valid = MATCH(name, SETTING_ICON_SIZE) ? layout_parse_icon_size(value, &count)
                                                        : layout_parse_count(value, &count);
            if (!valid)
                log_error("Invalid %s value '%s' in menu '%s', ignoring it", name, value, section);
            else if (MATCH(name, SETTING_ROWS))
                menu->overrides.rows = count;
            else if (MATCH(name, SETTING_COLUMNS))
                menu->overrides.columns = count;
            else
                menu->overrides.icon_cap = count;
            return 0;
        }
```

with:

```c
        if (layout_key && strchr(value, ';') == NULL) {
            SettingId id = MATCH(name, SETTING_ROWS) ? SET_ID_MENU_ROWS
                         : MATCH(name, SETTING_COLUMNS) ? SET_ID_MENU_COLUMNS : SET_ID_MENU_ICON_SIZE;
            SettingValue parsed;
            if (!setting_parse(setting_def(id), value, &parsed))
                log_error("Invalid %s value '%s' in menu '%s', ignoring it", name, value, section);
            else if (id == SET_ID_MENU_ROWS)
                menu->overrides.rows = parsed.number;
            else if (id == SET_ID_MENU_COLUMNS)
                menu->overrides.columns = parsed.number;
            else
                menu->overrides.icon_cap = parsed.number;
            return 0;
        }
```

Two differences from before, both deliberate:
- an invalid `Mode`, `Color`, `SlideshowImageDuration` or `SlideshowTransitionTime` now gets a log line, where it was silently ignored;
- a `Color` with a stray character (`#12345G`) is now refused, where it was half-read.

- [ ] **Step 8: Build, run every unit test and the harness**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 8` (layout, utf8, library, library_svg, fileio, inidoc, config_save, settings). Then the headless harness: `0 failed`. Items 14, 15 and 16 read `[Layout]` and menu keys through the new path, and their exact log lines are unchanged.

- [ ] **Step 9: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/settings.h src/settings.c tests/test_settings.c src/util.c src/CMakeLists.txt tests/CMakeLists.txt
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: the settings table and the settings screen's pages"
```

---

### Task 7: Titles that scale, in the launcher (items 18 and 19)

Wires Task 5's rule into the running launcher:
- `FontSize` and `Padding` accept percentages, and default to `14%` and `8%`.
- Each menu renders its titles in a font of its own size, from a small cache.
- Shrink mode stops at the readable minimum and then cuts, which also ends the font leak.
- The highlight's height follows each menu's title block.
- `OversizeMode=Truncate` finally parses, and the sample config ships it.

**Files:**
- Modify: `config/config_settings.cmake`, `config/launcher_config.h.in`, `config/config.ini.in`
- Modify: `src/launcher.h`, `src/launcher.c`, `src/util.c`, `src/image.h`, `src/image.c`, `src/debug.c`
- Create: `tests/headless/checks/40-titles.sh`; fixtures `f40-dense.ini`, `f40-large.ini`, `f40-fixed.ini`, `f40-notitles.ini`, `f40-truncate.ini`, `f40-truncated.ini`, `f40-shrink.ini`, `f40-junk.ini`

**Interfaces:**
- Consumes: `layout_parse_title_padding()`, the new `LayoutParams`/`LayoutGeometry` fields (Task 5); `setting_parse(setting_def(SET_ID_TITLE_SIZE), ...)` (Task 6).
- Produces:
  - `Config.title_font_size_pct` (0 = the fixed `title_font_size`) and `Config.title_padding_pct` (0 = the fixed `title_padding`);
  - `Geometry.title_min_size` and `Geometry.title_line_pm`;
  - `TTF_Font *title_font(int size)` and `void title_fonts_free(void)` (image.c);
  - `void describe_titles(const LayoutGeometry *geometry, char *out, size_t size)` (launcher.c).

  Task 10's `reload_titles()` builds on these.

- [ ] **Step 1: Write the failing headless checks.** Create `tests/headless/checks/40-titles.sh`:

```bash
# Items 18 and 19: titles scale with the button by default, stop at a readable minimum, and a
# fixed FontSize or Padding still means what it did

# A dense 8 x 4 grid: 14% of these buttons would be unreadable, so titles stop at 22 pt (2% of 1080)
run_quick f40-dense
ok=1
grep -qE "Menu 'Main': 8 x 4 grid, [0-9]+ px buttons, 22 pt titles" "$out/f40-dense.log" \
    && sanitizer_clean f40-dense && ok=0
result "items 18-19: a dense grid's titles stop at the readable minimum" $ok
grep "Menu 'Main':" "$out/f40-dense.log" | sed 's/^/      /'

# Three large buttons: the titles grow with them (round(556 * 0.14) = 78)
run_quick f40-large
ok=1
grep -q "Menu 'Main': 3 x 1 grid, 556 px buttons, 78 pt titles" "$out/f40-large.log" \
    && sanitizer_clean f40-large && ok=0
result "item 19: large buttons get titles to match" $ok
grep "Menu 'Main':" "$out/f40-large.log" | sed 's/^/      /'

# FontSize=36 and Padding=20 keep today's fixed titles
run_quick f40-fixed
ok=1
grep -q "Menu 'Main': 4 x 1 grid, 256 px buttons, 36 pt titles" "$out/f40-fixed.log" \
    && grep -A10 'Titles ===' "$out/f40-fixed.log" | grep -qE 'FontSize:\s+36$' \
    && sanitizer_clean f40-fixed && ok=0
result "items 18-19: a fixed FontSize and Padding still mean what they did" $ok

# Titles off: nothing under the buttons, and no font sized for them
run_quick f40-notitles
ok=1
[ "$(cat "$out/f40-notitles.code")" = 0 ] && grep -q "Menu 'Main': 4 x 1 grid, [0-9]* px buttons, no titles" "$out/f40-notitles.log" \
    && sanitizer_clean f40-notitles && ok=0
result "titles turned off with a percentage FontSize" $ok

# OversizeMode=Truncate (the documented spelling) parses, and so does the old Truncated
for f in f40-truncate f40-truncated; do
    run_quick $f
    ok=1
    grep -A10 'Titles ===' "$out/$f.log" | grep -qE 'OversizeMode:\s+Truncate$' && sanitizer_clean $f && ok=0
    result "OversizeMode $(grep -o 'OversizeMode=[A-Za-z]*' "$FX/$f.ini") parses as Truncate" $ok
done

# Shrink mode on long titles in a dense grid: stops at the minimum and cuts the rest, without a
# crash (the harness runs with detect_leaks=0, so the font leak's fix is checked by reading)
run_quick f40-shrink
ok=1
[ "$(cat "$out/f40-shrink.code")" = 0 ] && sanitizer_clean f40-shrink && ok=0
result "item 19: Shrink mode on long titles in a dense grid" $ok

# A FontSize that is neither a size nor a percentage is refused with a log line
run_quick f40-junk
ok=1
grep -q "Invalid FontSize value '12pt'" "$out/f40-junk.log" && grep -q "Invalid Padding value '20px'" "$out/f40-junk.log" \
    && sanitizer_clean f40-junk && ok=0
result "FontSize=12pt and Padding=20px are refused with a log line" $ok
```

Create the fixtures in `tests/headless/fixtures/`. `f40-dense.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Layout]
Rows=4
Columns=8

[Main]
Entry1=One;apps;:quit
Entry2=Two;apps;:quit
Entry3=Three;apps;:quit
```

`f40-large.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Layout]
Rows=1
Columns=3

[Main]
Entry1=One;apps;:quit
Entry2=Two;apps;:quit
Entry3=Three;apps;:quit
```

`f40-fixed.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Layout]
IconSize=256

[Titles]
FontSize=36
Padding=20

[Main]
Entry1=One;apps;:quit
```

`f40-notitles.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Titles]
Enabled=false

[Main]
Entry1=One;apps;:quit
```

`f40-truncate.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Titles]
OversizeMode=Truncate

[Main]
Entry1=One;apps;:quit
```

`f40-truncated.ini`: the same, with `OversizeMode=Truncated`. `f40-shrink.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Layout]
Rows=4
Columns=8

[Titles]
OversizeMode=Shrink

[Main]
Entry1=Kodi Media Center;apps;:quit
Entry2=Steam Big Picture;apps;:quit
Entry3=Emulation Station;apps;:quit
Entry4=Plex;apps;:quit
```

`f40-junk.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Titles]
FontSize=12pt
Padding=20px

[Main]
Entry1=One;apps;:quit
```

- [ ] **Step 2: Run the harness and watch them fail**

Expected: every new `40-titles` line fails. The log lines have no `pt titles` yet, `OversizeMode=Truncate` is not parsed, and `12pt` is read as 12 without complaint. The earlier checks still pass.

- [ ] **Step 3: The defaults.** In `config/config_settings.cmake`:
- after `set(DEFAULT_FONT_SIZE 36)`, add `set(DEFAULT_FONT_SIZE_PERCENT 14)`;
- replace `set(DEFAULT_TITLE_OVERSIZE_MODE "Shrink")` with `set(DEFAULT_TITLE_OVERSIZE_MODE "Truncate")`;
- replace `set(DEFAULT_TITLE_PADDING 20)` with `set(DEFAULT_TITLE_PADDING_PERCENT 8)`.

In `config/launcher_config.h.in`:
- after `#define DEFAULT_FONT_SIZE @DEFAULT_FONT_SIZE@`, add `#define DEFAULT_FONT_SIZE_PERCENT @DEFAULT_FONT_SIZE_PERCENT@`;
- replace `#define DEFAULT_TITLE_PADDING @DEFAULT_TITLE_PADDING@` with `#define DEFAULT_TITLE_PADDING_PERCENT @DEFAULT_TITLE_PADDING_PERCENT@`.

In `config/config.ini.in`, replace the two lines:

```
@SETTING_TITLE_FONT_SIZE@=@DEFAULT_FONT_SIZE@
```

```
@SETTING_TITLE_PADDING@=@DEFAULT_TITLE_PADDING@
```

with:

```
@SETTING_TITLE_FONT_SIZE@=@DEFAULT_FONT_SIZE_PERCENT@%
```

```
@SETTING_TITLE_PADDING@=@DEFAULT_TITLE_PADDING_PERCENT@%
```

- [ ] **Step 4: The config fields and geometry.** In `src/launcher.h`, under `// Launcher parameters`, add:

```c
#define TITLE_MIN_SIZE 0.02F       // The readable minimum title size: 2% of the screen height
#define TITLE_MEASURE_SIZE 1000    // Point size a title font's line height is measured at
```

In `Geometry`, replace `int font_height;` with:

```c
    int font_height;    // The fixed FontSize's line height
    int title_min_size; // The readable minimum title size, in points
    int title_line_pm;  // The title font's line height per point, in thousandths
```

In `Highlight`, after `int vpad;`, add `int title_block;`. In `Config`, after `unsigned int title_font_size;`, add:

```c
    int title_font_size_pct;    // FontSize as a percentage of the button; 0 = the fixed title_font_size
```

and after `int title_padding;`, add:

```c
    int title_padding_pct;      // Padding as a percentage of the button; 0 = the fixed title_padding
```

At the end of the file, after `compute_menu_layout`'s prototype, add:

```c
void describe_titles(const LayoutGeometry *geometry, char *out, size_t size);
```

- [ ] **Step 5: Parse `FontSize`, `Padding` and `Truncate`.** In `src/util.c`, in `mode_settings`, replace `{"Truncated", "Shrink", "None", NULL, NULL},          // OversizeMode` with:

```c
    {"Truncate", "Shrink", "None", NULL, NULL},           // OversizeMode ("Truncated" is read too)
```

In the `[Titles]` branch, replace:

```c
        else if (MATCH(name, SETTING_TITLE_FONT_SIZE))
            config.title_font_size = (unsigned int) atoi(value);
```

with:

```c
        else if (MATCH(name, SETTING_TITLE_FONT_SIZE)) {
            SettingValue parsed;
            if (!setting_parse(setting_def(SET_ID_TITLE_SIZE), value, &parsed))
                log_error("Invalid %s value '%s' in [Titles] (use a percentage of the button such as 14%%, or a size such as 36), ignoring it",
                    SETTING_TITLE_FONT_SIZE, value);
            else if (parsed.percent)
                config.title_font_size_pct = parsed.number;
            else {
                config.title_font_size = (unsigned int) parsed.number;
                config.title_font_size_pct = 0;
            }
        }
```

Replace:

```c
        else if (MATCH(name, SETTING_TITLE_OVERSIZE_MODE))
            parse_mode_setting(MODE_SETTING_OVERSIZE, value, (int*) &config.title_oversize_mode);
        else if (MATCH(name, SETTING_TITLE_PADDING)) {
            int title_padding = atoi(value);
            if (title_padding >= 0)
                config.title_padding = title_padding;
        }
```

with:

```c
        else if (MATCH(name, SETTING_TITLE_OVERSIZE_MODE)) {
            // "Truncated" was the only spelling the parser knew; the docs have always said "Truncate"
            if (MATCH(value, "Truncated"))
                config.title_oversize_mode = OVERSIZE_TRUNCATE;
            else
                parse_mode_setting(MODE_SETTING_OVERSIZE, value, (int*) &config.title_oversize_mode);
        }
        else if (MATCH(name, SETTING_TITLE_PADDING)) {
            int padding;
            bool percent;
            if (!layout_parse_title_padding(value, &padding, &percent))
                log_error("Invalid %s value '%s' in [Titles] (use a percentage of the button such as 8%%, or px such as 20), ignoring it",
                    SETTING_TITLE_PADDING, value);
            else if (percent) {
                config.title_padding_pct = padding;
                config.title_padding = 0;
            }
            else {
                config.title_padding = padding;
                config.title_padding_pct = 0;
            }
        }
```

In `validate_settings()`, replace:

```c
    if (!config.titles_enabled)
        config.title_padding = 0;
```

with:

```c
    if (!config.titles_enabled) {
        config.title_padding = 0;
        config.title_padding_pct = 0;
    }
```

and delete the whole title padding block that follows `// Make sure title padding is in valid range. IconSize is an optional cap now, so the range` (through its closing brace): the layout now caps a fixed padding at half each menu's button.

- [ ] **Step 6: The title font cache and Shrink with a floor.** In `src/image.h`, add `int min_size;` to `TextInfo` after `int max_width;`, with the comment `// Shrink mode stops here: the readable minimum`, and these prototypes:

```c
TTF_Font *title_font(int size);
void title_fonts_free(void);
```

In `src/image.c`, after `quit_svg()`, add:

```c
#define MAX_TITLE_FONTS 16

// Title fonts by point size, for menus whose titles scale with their buttons
static struct {
    int size;
    TTF_Font *font;
} title_fonts[MAX_TITLE_FONTS];
static int title_font_count = 0;

// A function to get the title font at a point size, opening it the first time. SDL_ttf 2.0.15
// cannot resize an open font, so each size is its own; the cache keeps MAX_TITLE_FONTS of them
// and closes the oldest to make room.
TTF_Font *title_font(int size)
{
    for (int i = 0; i < title_font_count; i++) {
        if (title_fonts[i].size == size)
            return title_fonts[i].font;
    }
    TTF_Font *font = TTF_OpenFont(config.title_font_path, size);
    if (font == NULL) {
        log_error("Could not open the title font at %i pt\n%s", size, TTF_GetError());
        return NULL;
    }
    if (title_font_count == MAX_TITLE_FONTS) {
        TTF_CloseFont(title_fonts[0].font);
        memmove(&title_fonts[0], &title_fonts[1], (MAX_TITLE_FONTS - 1) * sizeof(title_fonts[0]));
        title_font_count--;
    }
    title_fonts[title_font_count].size = size;
    title_fonts[title_font_count].font = font;
    title_font_count++;
    return font;
}

// A function to close every cached title font
void title_fonts_free(void)
{
    for (int i = 0; i < title_font_count; i++)
        TTF_CloseFont(title_fonts[i].font);
    title_font_count = 0;
}
```

In `render_text()`, replace everything from `        // Shrink mode:` through the line `        output_font = info->font;` (the old shrink loop, and the two `if` statements after it) with:

```c
        // Shrink mode: work out the size that fits from the measured width, never going below
        // the readable minimum, then cut whatever still does not fit
        else if (info->oversize_mode == OVERSIZE_SHRINK) {
            int size = info->font_size * info->max_width / w;
            if (size < info->min_size)
                size = info->min_size;
            if (size > 0 && size < info->font_size) {
                reduced_font = TTF_OpenFont(*info->font_path, size);

                // The font's own rounding can leave it a pixel or two too wide: step down to the minimum
                while (reduced_font != NULL) {
                    TTF_SizeUTF8(reduced_font, text_buffer, &w, &h);
                    if (w <= info->max_width || size <= info->min_size)
                        break;
                    TTF_CloseFont(reduced_font);
                    reduced_font = TTF_OpenFont(*info->font_path, --size);
                }
            }
            if (w > info->max_width) {
                utf8_truncate(text_buffer, w, info->max_width);
                TTF_SizeUTF8(reduced_font != NULL ? reduced_font : info->font, text_buffer, &w, &h);
            }
        }
    }
    output_font = reduced_font != NULL ? reduced_font : info->font;
```

`render_text()`'s clean-up still closes `reduced_font` when it is set, so every font it opens is closed: the leak item 19 noted is gone.

- [ ] **Step 7: Per-menu titles in the launcher.** In `src/launcher.c`, add the defaults. Replace `.title_font_size = DEFAULT_FONT_SIZE,` with:

```c
    .title_font_size                  = DEFAULT_FONT_SIZE,
    .title_font_size_pct              = DEFAULT_FONT_SIZE_PERCENT,
```

and replace `.title_padding = -1,` with:

```c
    .title_padding                    = 0,
    .title_padding_pct                = DEFAULT_TITLE_PADDING_PERCENT,
```

Add a global after `TextInfo title_info;`:

```c
static TTF_Font *fixed_title_font = NULL; // The title font at the fixed FontSize
```

In `init_sdl()`, after `geo.screen_margin = ...;`, add:

```c
    geo.title_min_size = (int) (TITLE_MIN_SIZE * (float) geo.screen_height + 0.5F);
```

In `init_sdl_ttf()`, add `.min_size = geo.title_min_size,` to the `TextInfo` initializer after `.max_width = 0, ...`. Then replace:

```c
    int error = load_font(&title_info, FILENAME_DEFAULT_FONT);
    if (error)
        log_fatal("Could not load title font");
    geo.font_height = config.titles_enabled ? TTF_FontHeight(title_info.font) : 0;
```

with:

```c
    int error = load_font(&title_info, FILENAME_DEFAULT_FONT);
    if (error)
        log_fatal("Could not load title font");
    fixed_title_font = title_info.font;
    geo.font_height = config.titles_enabled ? TTF_FontHeight(title_info.font) : 0;

    // A percentage FontSize sizes each menu's titles from its buttons, so measure the font's line
    // height per point once, at a large size, for the layout to reserve room for any size
    TTF_Font *probe = TTF_OpenFont(config.title_font_path, TITLE_MEASURE_SIZE);
    geo.title_line_pm = probe != NULL ? TTF_FontHeight(probe) * 1000 / TITLE_MEASURE_SIZE : 1500;
    if (probe != NULL)
        TTF_CloseFont(probe);
```

In `cleanup()`, before `TTF_Quit();`, add `title_fonts_free();`.

Replace `compute_menu_layout()`'s `LayoutParams params = { ... };` with:

```c
    bool titles = config.titles_enabled;
    LayoutParams params = {
        .rows              = effective.rows,
        .columns           = effective.columns,
        .icon_cap          = effective.icon_cap,
        .spacing           = config.icon_spacing,
        .title_block       = titles && config.title_font_size_pct == 0 ? geo.font_height : 0,
        .hpad              = config.highlight_hpadding,
        .vpad              = config.highlight_vpadding,
        .title_padding     = titles ? config.title_padding : 0,
        .title_padding_pct = titles ? config.title_padding_pct : 0,
        .title_size_pct    = titles ? config.title_font_size_pct : 0,
        .title_min_size    = geo.title_min_size,
        .title_line_pm     = geo.title_line_pm
    };
```

After `compute_menu_layout()`, add:

```c
// A function to describe a menu's titles for the log: "36 pt titles", or "no titles"
void describe_titles(const LayoutGeometry *geometry, char *out, size_t size)
{
    if (!config.titles_enabled)
        snprintf(out, size, "no titles");
    else
        snprintf(out, size, "%i pt titles", geometry->title_size > 0 ? geometry->title_size : (int) config.title_font_size);
}
```

In `apply_layout()`, replace:

```c
    log_debug("Menu '%s': %i x %i grid, %i px buttons", menu->name, layout.columns, layout.rows, layout.button);

    if (menu->rendered_size != layout.button) {
        render_buttons(menu, layout.button);
        menu->rendered_size = layout.button;
    }
    if (config.highlight && (highlight->button != layout.button ||
    highlight->hpad != layout.hpad || highlight->vpad != layout.vpad)) {
        if (highlight->texture != NULL)
            SDL_DestroyTexture(highlight->texture);
        int button_height = layout.button + config.title_padding + geo.font_height;
```

with:

```c
    char titles[32];
    describe_titles(&layout, titles, sizeof(titles));
    log_debug("Menu '%s': %i x %i grid, %i px buttons, %s", menu->name, layout.columns, layout.rows, layout.button, titles);

    if (menu->rendered_size != layout.button) {
        render_buttons(menu, &layout);
        menu->rendered_size = layout.button;
    }
    if (config.highlight && (highlight->button != layout.button || highlight->hpad != layout.hpad ||
    highlight->vpad != layout.vpad || highlight->title_block != layout.title_block)) {
        if (highlight->texture != NULL)
            SDL_DestroyTexture(highlight->texture);
        int button_height = layout.button + layout.title_block;
```

and after `highlight->vpad = layout.vpad;` add `highlight->title_block = layout.title_block;`. In `main()`, add `.title_block = 0` to the `Highlight` initializer.

Replace `render_buttons()` (its prototype at the top of the file too, as `static void render_buttons(Menu *menu, const LayoutGeometry *geometry);`) with:

```c
// A function to render all buttons (icon and title) of a menu for its layout: the icons at the
// button size, and the titles in the menu's own title size
static void render_buttons(Menu *menu, const LayoutGeometry *geometry)
{
    int size = geometry->button;
    title_info.max_width = size;
    title_info.font = fixed_title_font;
    title_info.font_size = (int) config.title_font_size;
    if (geometry->title_size > 0) {
        TTF_Font *font = title_font(geometry->title_size);
        if (font != NULL) {
            title_info.font = font;
            title_info.font_size = geometry->title_size;
        }
    }
    int line_height = TTF_FontHeight(title_info.font);
    for (unsigned int i = 0; i < menu->num_entries; i++) {
        Entry *entry = menu->items[i];
        if (entry->icon != NULL)
            SDL_DestroyTexture(entry->icon);
        if (entry->icon_selected != NULL)
            SDL_DestroyTexture(entry->icon_selected);
        entry->icon = load_icon(entry->icon_path, size);
        entry->icon_selected = entry->icon_selected_path != NULL ? load_icon(entry->icon_selected_path, size) : NULL;
        if (config.titles_enabled) {
            int h;
            if (entry->title_texture != NULL)
                SDL_DestroyTexture(entry->title_texture);
            entry->title_texture = render_text_texture(entry->title, &title_info, &entry->text_rect, &h);
            entry->title_offset = (config.title_oversize_mode == OVERSIZE_SHRINK && h != line_height)
                                  ? (line_height - h) / 2 : 0;
        }
    }
}
```

In `place_entries()`, replace `entry->text_rect.y = y + layout.button + entry->title_offset + config.title_padding;` with:

```c
        entry->text_rect.y = y + layout.button + entry->title_offset + layout.title_padding;
```

- [ ] **Step 8: The debug output.** In `src/debug.c`, `debug_settings()`, replace `DEBUG_INT(SETTING_TITLE_FONT_SIZE, config.title_font_size);` with:

```c
    char title_value[40];
    if (config.title_font_size_pct)
        snprintf(title_value, sizeof(title_value), "%i%% of the button", config.title_font_size_pct);
    else
        snprintf(title_value, sizeof(title_value), "%u", config.title_font_size);
    DEBUG_STR(SETTING_TITLE_FONT_SIZE, title_value);
```

and `DEBUG_INT(SETTING_TITLE_PADDING, config.title_padding);` with:

```c
    if (config.title_padding_pct)
        snprintf(title_value, sizeof(title_value), "%i%% of the button", config.title_padding_pct);
    else
        snprintf(title_value, sizeof(title_value), "%i", config.title_padding);
    DEBUG_STR(SETTING_TITLE_PADDING, title_value);
```

In `debug_menu_entries()`, replace `log_debug("Layout: %i x %i grid, %i px buttons", geometry.columns, geometry.rows, geometry.button);` with:

```c
            char titles[32];
            describe_titles(&geometry, titles, sizeof(titles));
            log_debug("Layout: %i x %i grid, %i px buttons, %s", geometry.columns, geometry.rows, geometry.button, titles);
```

- [ ] **Step 9: Build and run everything**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: no new warnings, and `100% tests passed, 0 tests failed out of 8`. `test_inidoc` round-trips the regenerated sample config, which now holds `FontSize=14%`, `Padding=8%` and `OversizeMode=Truncate`. Then run the headless harness. Expected: every `40-titles` line passes, the earlier checks still pass, and the last line is `0 failed`.

- [ ] **Step 10: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add config/config_settings.cmake config/launcher_config.h.in config/config.ini.in src/launcher.h src/launcher.c src/util.c src/image.h src/image.c src/debug.c tests/headless/checks/40-titles.sh tests/headless/fixtures/f40-dense.ini tests/headless/fixtures/f40-large.ini tests/headless/fixtures/f40-fixed.ini tests/headless/fixtures/f40-notitles.ini tests/headless/fixtures/f40-truncate.ini tests/headless/fixtures/f40-truncated.ini tests/headless/fixtures/f40-shrink.ini tests/headless/fixtures/f40-junk.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: titles scale with each menu's buttons, with a readable minimum (items 18, 19)"
```

---

### Task 8: The folder browser's model (`browser`) and the starting places

The pure model behind the Image and Folder rows:
- **Rows:** folders first, then images, each sorted by name ignoring case; hidden files are skipped.
- **Movement:** a cursor that moves a row or a page at a time; going into folders and back up to *Places*.
- **Folder mode:** *Use this folder (N images)*, which needs two or more images.
- **Refusals:** a path that `config.ini` cannot hold is shown but cannot be chosen.

It reads folders through a function it is given, so the tests hand it a pretend file system. Paths in either style (`/home/me`, `C:\Users\me`, `\\server\share`) work on any platform, so the same tests run everywhere. The platform side adds `fileio_places()`, which lists Pictures, Home and the drives, or `/`, `/media/*` and `/mnt/*`.

**Files:**
- Create: `src/browser.h`, `src/browser.c`, `tests/test_browser.c`
- Modify: `src/fileio.h`, `src/fileio.c` (places), `tests/test_fileio.c`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `FileioEntry`, `fileio_free_list()`, `fileio_is_dir()`, `fileio_list()` (Task 2).
- Produces:

```c
#define BROWSER_PATH_MAX 1024
typedef enum { BROWSER_IMAGE, BROWSER_FOLDER } BrowserMode;
typedef enum { BROWSER_ROW_PLACE, BROWSER_ROW_USE_FOLDER, BROWSER_ROW_FOLDER, BROWSER_ROW_IMAGE } BrowserRowKind;
typedef struct {
    BrowserRowKind kind;
    char *name;          // What the row shows
    char *path;          // The full path it stands for (the folder on show for USE_FOLDER)
    bool enabled;        // False: shown, but OK does nothing
    const char *why;     // Why a row is disabled, when it says; NULL otherwise
    int image_count;     // USE_FOLDER: the images in the folder on show
} BrowserRow;
typedef struct { const char *label; const char *path; } BrowserPlace;
typedef int (*BrowserList)(const char *folder, FileioEntry **entries, void *context);
typedef const char *(*BrowserCheck)(const char *path, void *context);   // NULL = the path can be saved
typedef enum { BROWSER_UP, BROWSER_DOWN, BROWSER_PAGE_UP, BROWSER_PAGE_DOWN, BROWSER_OK, BROWSER_BACK } BrowserCommand;
typedef enum { BROWSER_NONE, BROWSER_MOVED, BROWSER_CHOSEN, BROWSER_CLOSED } BrowserResult;
typedef struct Browser Browser;
Browser *browser_open(BrowserMode mode, const char *start, const BrowserPlace *places, int place_count,
                      BrowserList list, BrowserCheck check, void *context);
void browser_free(Browser *browser);
BrowserResult browser_command(Browser *browser, BrowserCommand command, int page_rows);
int browser_row_count(const Browser *browser);
const BrowserRow *browser_row(const Browser *browser, int index);
int browser_cursor(const Browser *browser);
const char *browser_folder(const Browser *browser);   // NULL while the places are shown
const char *browser_chosen(const Browser *browser);   // after BROWSER_CHOSEN
bool browser_first_image(const Browser *browser, const char *folder, char *out, size_t size);
bool browser_is_image(const char *name);
bool browser_parent(const char *path, char *out, size_t size);

// fileio.h gains:
typedef struct { char *label; char *path; } FileioPlace;
int fileio_places(FileioPlace **places);           // only folders that exist; Pictures first, then Home
void fileio_free_places(FileioPlace *places, int count);
```

- [ ] **Step 1: Write the failing test.** Create `tests/test_browser.c`:

```c
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "browser.h"

// A pretend file system: each folder's names, '|' between them; a trailing '/' marks a folder
static const struct {
    const char *folder;
    const char *names;
} FAKE[] = {
    { "/", "home/|media/" },
    { "/home", "me/" },
    { "/home/me", "Pictures/|Music/|.cache/" },
    { "/home/me/Pictures", "Autumn/|beach.JPG|.hidden.png|notes.txt|zebra.png|apple.webp|Birthdays/" },
    { "/home/me/Pictures/Autumn", "a.jpg|b.jpeg" },
    { "/home/me/Pictures/Birthdays", "one.png" },
    { "/media", "usb/" },
    { "/media/usb", "" },
    { "C:\\", "Users/" },
    { "C:\\Users", "me/" },
    { "C:\\Users\\me", "Pictures/" },
    { "C:\\Users\\me\\Pictures", "trip.png|x.jpg" }
};

// A function to list a pretend folder, the way fileio_list() lists a real one
static int fake_list(const char *folder, FileioEntry **entries, void *context)
{
    (void) context;
    for (size_t i = 0; i < sizeof(FAKE) / sizeof(FAKE[0]); i++) {
        if (strcmp(FAKE[i].folder, folder) != 0)
            continue;
        int count = 0;
        *entries = calloc(16, sizeof(FileioEntry));
        const char *p = FAKE[i].names;
        while (*p != '\0') {
            const char *end = strchr(p, '|');
            size_t length = end != NULL ? (size_t) (end - p) : strlen(p);
            bool is_dir = length > 0 && p[length - 1] == '/';
            size_t name_length = is_dir ? length - 1 : length;
            char *name = malloc(name_length + 1);
            memcpy(name, p, name_length);
            name[name_length] = '\0';
            (*entries)[count] = (FileioEntry) { .name = name, .is_dir = is_dir, .hidden = name[0] == '.' };
            count++;
            p += length;
            if (*p == '|')
                p++;
        }
        return count;
    }
    return -1;
}

// A function to refuse paths with "zebra" or ending in "Autumn", standing in for config.ini's limits
static const char *fake_check(const char *path, void *context)
{
    (void) context;
    size_t length = strlen(path);
    if (strstr(path, "zebra") != NULL || (length >= 6 && strcmp(path + length - 6, "Autumn") == 0))
        return "it is too long for one line of config.ini";
    return NULL;
}

static const BrowserPlace PLACES[] = {
    { "Pictures", "/home/me/Pictures" },
    { "Home", "/home/me" },
    { "/", "/" }
};

// A function to test the rows of a folder: folders first, then images, each sorted without regard
// to case, with hidden files and other files left out, and the cursor on the image it opened at
static void test_rows_and_start(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures/zebra.png", PLACES, 3, fake_list, NULL, NULL);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures");
    CHECK_INT(browser_row_count(browser), 5);
    CHECK_STR(browser_row(browser, 0)->name, "Autumn");
    CHECK_INT(browser_row(browser, 0)->kind, BROWSER_ROW_FOLDER);
    CHECK_STR(browser_row(browser, 1)->name, "Birthdays");
    CHECK_STR(browser_row(browser, 2)->name, "apple.webp");
    CHECK_STR(browser_row(browser, 3)->name, "beach.JPG");
    CHECK_STR(browser_row(browser, 4)->name, "zebra.png");
    CHECK_STR(browser_row(browser, 4)->path, "/home/me/Pictures/zebra.png");
    CHECK_INT(browser_cursor(browser), 4);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_CHOSEN);
    CHECK_STR(browser_chosen(browser), "/home/me/Pictures/zebra.png");
    browser_free(browser);
}

// A function to test going into folders and back up to the places, and out
static void test_into_and_out_of_folders(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures/zebra.png", PLACES, 3, fake_list, NULL, NULL);
    for (int i = 0; i < 4; i++)
        browser_command(browser, BROWSER_UP, 10);
    CHECK_INT(browser_cursor(browser), 0);
    CHECK_INT(browser_command(browser, BROWSER_UP, 10), BROWSER_NONE);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_MOVED);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures/Autumn");
    CHECK_INT(browser_row_count(browser), 2);

    // Back comes out a folder at a time, with the cursor on the folder it came out of
    CHECK_INT(browser_command(browser, BROWSER_BACK, 10), BROWSER_MOVED);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures");
    CHECK_INT(browser_cursor(browser), 0);
    browser_command(browser, BROWSER_BACK, 10);
    CHECK_STR(browser_folder(browser), "/home/me");
    CHECK_STR(browser_row(browser, browser_cursor(browser))->name, "Pictures");
    CHECK_INT(browser_row_count(browser), 2);                  // .cache is hidden
    browser_command(browser, BROWSER_BACK, 10);
    CHECK_STR(browser_folder(browser), "/home");
    browser_command(browser, BROWSER_BACK, 10);
    CHECK_STR(browser_folder(browser), "/");

    // Above the root: the places, with the cursor on the one it came from; Back again closes
    browser_command(browser, BROWSER_BACK, 10);
    CHECK(browser_folder(browser) == NULL);
    CHECK_INT(browser_row_count(browser), 3);
    CHECK_INT(browser_row(browser, 0)->kind, BROWSER_ROW_PLACE);
    CHECK_INT(browser_cursor(browser), 2);
    CHECK_INT(browser_command(browser, BROWSER_BACK, 10), BROWSER_CLOSED);
    browser_free(browser);
}

// A function to test the folder mode: Use this folder needs two images, and images cannot be chosen
static void test_folder_mode(void)
{
    Browser *browser = browser_open(BROWSER_FOLDER, "/home/me/Pictures/Autumn", PLACES, 3, fake_list, NULL, NULL);
    CHECK_INT(browser_row(browser, 0)->kind, BROWSER_ROW_USE_FOLDER);
    CHECK_INT(browser_row(browser, 0)->image_count, 2);
    CHECK(browser_row(browser, 0)->enabled);
    CHECK(!browser_row(browser, 1)->enabled);                  // An image, shown but not chosen
    CHECK_INT(browser_cursor(browser), 0);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_CHOSEN);
    CHECK_STR(browser_chosen(browser), "/home/me/Pictures/Autumn");
    browser_free(browser);

    browser = browser_open(BROWSER_FOLDER, "/home/me/Pictures/Birthdays", PLACES, 3, fake_list, NULL, NULL);
    CHECK(!browser_row(browser, 0)->enabled);
    CHECK(browser_row(browser, 0)->why != NULL && strstr(browser_row(browser, 0)->why, "2 or more") != NULL);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_NONE);
    browser_command(browser, BROWSER_DOWN, 10);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_NONE);
    browser_free(browser);
}

// A function to test where the browser opens when there is no start, or it is gone
static void test_start_places(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "", PLACES, 3, fake_list, NULL, NULL);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures");
    browser_free(browser);
    browser = browser_open(BROWSER_IMAGE, "/gone/away.png", PLACES, 3, fake_list, NULL, NULL);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures");
    browser_free(browser);
    static const BrowserPlace gone[] = { { "Gone", "/gone" } };
    browser = browser_open(BROWSER_IMAGE, "", gone, 1, fake_list, NULL, NULL);
    CHECK(browser_folder(browser) == NULL);
    CHECK_INT(browser_row_count(browser), 1);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_NONE);   // It cannot be listed
    browser_free(browser);
}

// A function to test moving a page at a time
static void test_paging(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures", PLACES, 3, fake_list, NULL, NULL);
    CHECK_INT(browser_cursor(browser), 0);
    CHECK_INT(browser_command(browser, BROWSER_PAGE_DOWN, 2), BROWSER_MOVED);
    CHECK_INT(browser_cursor(browser), 2);
    browser_command(browser, BROWSER_PAGE_DOWN, 2);
    CHECK_INT(browser_cursor(browser), 4);
    CHECK_INT(browser_command(browser, BROWSER_PAGE_DOWN, 2), BROWSER_NONE);
    browser_command(browser, BROWSER_PAGE_UP, 3);
    CHECK_INT(browser_cursor(browser), 1);
    browser_free(browser);
}

// A function to test that a path config.ini cannot hold is shown, but refused with the reason
static void test_refused_paths(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures/zebra.png", PLACES, 3, fake_list, fake_check, NULL);
    const BrowserRow *row = browser_row(browser, browser_cursor(browser));
    CHECK_STR(row->name, "zebra.png");
    CHECK(!row->enabled);
    CHECK(row->why != NULL && strstr(row->why, "too long") != NULL);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_NONE);
    browser_free(browser);
    browser = browser_open(BROWSER_FOLDER, "/home/me/Pictures/Autumn", PLACES, 3, fake_list, fake_check, NULL);
    CHECK(!browser_row(browser, 0)->enabled);
    CHECK(strstr(browser_row(browser, 0)->why, "too long") != NULL);
    browser_free(browser);
}

// A function to test parent folders in both path styles
static void test_parent(void)
{
    char out[BROWSER_PATH_MAX];
    CHECK(!browser_parent("/", out, sizeof(out)));
    CHECK(browser_parent("/home", out, sizeof(out)));
    CHECK_STR(out, "/");
    CHECK(browser_parent("/home/me/", out, sizeof(out)));
    CHECK_STR(out, "/home");
    CHECK(!browser_parent("C:\\", out, sizeof(out)));
    CHECK(!browser_parent("C:", out, sizeof(out)));
    CHECK(browser_parent("C:\\Users", out, sizeof(out)));
    CHECK_STR(out, "C:\\");
    CHECK(browser_parent("C:\\Users\\me\\", out, sizeof(out)));
    CHECK_STR(out, "C:\\Users");
    CHECK(!browser_parent("\\\\server\\share", out, sizeof(out)));
    CHECK(browser_parent("\\\\server\\share\\dir", out, sizeof(out)));
    CHECK_STR(out, "\\\\server\\share");
    CHECK(!browser_parent("relative", out, sizeof(out)));
}

// A function to test finding a folder's first image, for previewing a folder
static void test_first_image(void)
{
    Browser *browser = browser_open(BROWSER_FOLDER, "/home/me/Pictures", PLACES, 3, fake_list, NULL, NULL);
    char out[BROWSER_PATH_MAX];
    CHECK(browser_first_image(browser, "/home/me/Pictures", out, sizeof(out)));
    CHECK_STR(out, "/home/me/Pictures/apple.webp");
    CHECK(browser_first_image(browser, "/home/me/Pictures/Autumn", out, sizeof(out)));
    CHECK_STR(out, "/home/me/Pictures/Autumn/a.jpg");
    CHECK(!browser_first_image(browser, "/media/usb", out, sizeof(out)));
    browser_free(browser);
}

// A function to test Windows paths: joined with backslashes, and up to the drive's root
static void test_windows_paths(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "C:\\Users\\me\\Pictures\\trip.png", PLACES, 3, fake_list, NULL, NULL);
    CHECK_STR(browser_row(browser, browser_cursor(browser))->path, "C:\\Users\\me\\Pictures\\trip.png");
    browser_command(browser, BROWSER_BACK, 10);
    CHECK_STR(browser_folder(browser), "C:\\Users\\me");
    browser_command(browser, BROWSER_BACK, 10);
    browser_command(browser, BROWSER_BACK, 10);
    CHECK_STR(browser_folder(browser), "C:\\");
    CHECK_STR(browser_row(browser, 0)->path, "C:\\Users");
    browser_free(browser);
}

// A function to test telling images by their extension, whatever its case
static void test_is_image(void)
{
    CHECK(browser_is_image("a.jpg"));
    CHECK(browser_is_image("a.JPEG"));
    CHECK(browser_is_image("a.Png"));
    CHECK(browser_is_image("a.webp"));
    CHECK(!browser_is_image("a.gif"));
    CHECK(!browser_is_image(".png"));
    CHECK(!browser_is_image("png"));
}

int main(void)
{
    test_rows_and_start();
    test_into_and_out_of_folders();
    test_folder_mode();
    test_start_places();
    test_paging();
    test_refused_paths();
    test_parent();
    test_first_image();
    test_windows_paths();
    test_is_image();
    return check_report();
}
```

In `tests/test_fileio.c`, add before `main()`:

```c
// A function to test the starting places: at least one, and every one a folder that exists
static void test_places(void)
{
    FileioPlace *places = NULL;
    int count = fileio_places(&places);
    CHECK(count >= 1);
    for (int i = 0; i < count; i++) {
        CHECK(places[i].label != NULL && places[i].label[0] != '\0');
        CHECK(fileio_is_dir(places[i].path));
    }
    fileio_free_places(places, count);
}
```

and call `test_places();` in `main()` before `return check_report();`.

- [ ] **Step 2: Register the test, and link the Windows shell libraries.** In `tests/CMakeLists.txt`, after the `test_utf8` block and before the `test_fileio` block, add the function below. It must come before every call to it: CMake reads the file top to bottom, and `test_fileio`, `test_inidoc` and `test_config_save` sit above `link_inih()`'s definition.

```cmake
# fileio.c finds the Windows places (Pictures, drives) through the shell
function(link_fileio target)
  if (WIN32)
    target_link_libraries(${target} shell32 ole32 uuid)
  endif ()
endfunction()
```

Call `link_fileio(<target>)` after each existing target that compiles `fileio.c`: `test_fileio`, `test_library`, `test_inidoc` and `test_config_save`. Then add:

```cmake
# Unit tests for the folder browser's model, over a pretend file system
add_executable(test_browser test_browser.c "${PROJECT_SOURCE_DIR}/src/browser.c" "${PROJECT_SOURCE_DIR}/src/fileio.c")
target_include_directories(test_browser PRIVATE "${PROJECT_SOURCE_DIR}/src")
link_fileio(test_browser)
add_test(NAME browser COMMAND test_browser)
```

In `src/CMakeLists.txt`, add `browser.c` and `browser.h` to `SOURCES` after `settings.h`, and add `shell32 ole32 uuid` to the Windows `target_link_libraries` list after `PowrProf`.

- [ ] **Step 3: Run it to make sure it fails**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_browser
```

Expected: the build fails, because `browser.h` does not exist.

- [ ] **Step 4: Add the places to `fileio`.** In `src/fileio.h`, after the `#ifdef _WIN32` block and before the include guard's closing `#endif` (the places are for every platform, so not inside the Windows block):

```c
typedef struct {
    char *label;  // What the browser shows: "Pictures", "Home", "C:", "/", a mount's name
    char *path;
} FileioPlace;

int fileio_places(FileioPlace **places);
void fileio_free_places(FileioPlace *places, int count);
```

In `src/fileio.c`, add `#include <shlobj.h>` and `#include <knownfolders.h>` after `#include <windows.h>`, and at the end of the file:

```c
// A function to add a starting place when its folder exists
static void add_place(FileioPlace **places, int *count, const char *label, const char *path)
{
    if (path == NULL || !fileio_is_dir(path))
        return;
    FileioPlace *grown = realloc(*places, (size_t) (*count + 1) * sizeof(FileioPlace));
    if (grown == NULL)
        return;
    *places = grown;
    (*places)[*count].label = strdup(label);
    (*places)[*count].path = strdup(path);
    (*count)++;
}

#ifndef _WIN32
// A function to add each folder inside a folder as a place: mounted drives under /media and /mnt
static void add_places_under(FileioPlace **places, int *count, const char *folder)
{
    FileioEntry *entries = NULL;
    int found = fileio_list(folder, &entries);
    for (int i = 0; i < found; i++) {
        if (!entries[i].is_dir || entries[i].hidden)
            continue;
        size_t size = strlen(folder) + strlen(entries[i].name) + 2;
        char *path = malloc(size);
        if (path != NULL) {
            snprintf(path, size, "%s/%s", folder, entries[i].name);
            add_place(places, count, entries[i].name, path);
            free(path);
        }
    }
    fileio_free_list(entries, found);
}
#endif

// A function to list where the folder browser can start: Pictures first, then Home, then the
// drives (Windows) or the file system's root and its mounted drives (elsewhere)
int fileio_places(FileioPlace **places)
{
    int count = 0;
    *places = NULL;
#ifdef _WIN32
    PWSTR wide = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Pictures, 0, NULL, &wide))) {
        char *path = to_utf8(wide);
        add_place(places, &count, "Pictures", path);
        free(path);
    }
    CoTaskMemFree(wide);
    wide = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Profile, 0, NULL, &wide))) {
        char *path = to_utf8(wide);
        add_place(places, &count, "Home", path);
        free(path);
    }
    CoTaskMemFree(wide);

    // An empty card reader or DVD drive must not pop up "There is no disk in the drive"
    UINT old_mode = SetErrorMode(SEM_FAILCRITICALERRORS);
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (drives & (1u << i)) {
            char path[4] = { (char) ('A' + i), ':', '\\', '\0' };
            char label[3] = { (char) ('A' + i), ':', '\0' };
            add_place(places, &count, label, path);
        }
    }
    SetErrorMode(old_mode);
#else
    const char *home = getenv("HOME");
    const char *pictures = getenv("XDG_PICTURES_DIR");
    char buffer[4096];
    if (pictures == NULL && home != NULL) {
        snprintf(buffer, sizeof(buffer), "%s/Pictures", home);
        pictures = buffer;
    }
    add_place(places, &count, "Pictures", pictures);
    add_place(places, &count, "Home", home);
    add_place(places, &count, "/", "/");
    add_places_under(places, &count, "/media");
    add_places_under(places, &count, "/mnt");
#endif
    return count;
}

// A function to free a list from fileio_places
void fileio_free_places(FileioPlace *places, int count)
{
    for (int i = 0; i < count; i++) {
        free(places[i].label);
        free(places[i].path);
    }
    free(places);
}
```

- [ ] **Step 5: Create `src/browser.h`:**

```c
// The folder browser behind the settings screen's Image and Folder rows: a folder's rows, a cursor,
// and moving between folders and the places it can start from. It lists folders through a function
// it is given, so the tests hand it a pretend file system; paths in either style ("/home/me",
// "C:\Users\me", "\\server\share") work on any platform. Pure: no SDL, no globals.
#ifndef BROWSER_H
#define BROWSER_H

#include <stdbool.h>
#include <stddef.h>
#include "fileio.h"

#define BROWSER_PATH_MAX 1024

typedef enum {
    BROWSER_IMAGE,   // Choosing one image
    BROWSER_FOLDER   // Choosing a folder of images (a slideshow)
} BrowserMode;

typedef enum {
    BROWSER_ROW_PLACE,
    BROWSER_ROW_USE_FOLDER,
    BROWSER_ROW_FOLDER,
    BROWSER_ROW_IMAGE
} BrowserRowKind;

typedef struct {
    BrowserRowKind kind;
    char *name;          // What the row shows
    char *path;          // The full path it stands for (the folder on show for USE_FOLDER)
    bool enabled;        // False: shown, but OK does nothing
    const char *why;     // Why a row is disabled, when it says; NULL otherwise
    int image_count;     // USE_FOLDER: the images in the folder on show
} BrowserRow;

typedef struct {
    const char *label;
    const char *path;
} BrowserPlace;

typedef int (*BrowserList)(const char *folder, FileioEntry **entries, void *context);
typedef const char *(*BrowserCheck)(const char *path, void *context);

typedef enum {
    BROWSER_UP,
    BROWSER_DOWN,
    BROWSER_PAGE_UP,
    BROWSER_PAGE_DOWN,
    BROWSER_OK,
    BROWSER_BACK
} BrowserCommand;

typedef enum {
    BROWSER_NONE,
    BROWSER_MOVED,
    BROWSER_CHOSEN,
    BROWSER_CLOSED
} BrowserResult;

typedef struct Browser Browser;

Browser *browser_open(BrowserMode mode, const char *start, const BrowserPlace *places, int place_count,
                      BrowserList list, BrowserCheck check, void *context);
void browser_free(Browser *browser);
BrowserResult browser_command(Browser *browser, BrowserCommand command, int page_rows);
int browser_row_count(const Browser *browser);
const BrowserRow *browser_row(const Browser *browser, int index);
int browser_cursor(const Browser *browser);
const char *browser_folder(const Browser *browser);
const char *browser_chosen(const Browser *browser);
bool browser_first_image(const Browser *browser, const char *folder, char *out, size_t size);
bool browser_is_image(const char *name);
bool browser_parent(const char *path, char *out, size_t size);

#endif
```

- [ ] **Step 6: Create `src/browser.c`:**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "browser.h"

static const char *const IMAGE_EXTENSIONS[] = { ".jpg", ".jpeg", ".png", ".webp" };
static const char *const TOO_FEW = "a slideshow needs 2 or more images";

struct Browser {
    BrowserMode mode;
    BrowserList list;
    BrowserCheck check;
    void *context;
    BrowserPlace *places;   // Copies of the places given
    int place_count;
    char *folder;           // The folder on show; NULL while the places are shown
    BrowserRow *rows;
    int row_count;
    int cursor;
    char chosen[BROWSER_PATH_MAX];
};

// A function to tell a path separator, in either style
static bool is_separator(char c)
{
    return c == '/' || c == '\\';
}

// A function to lower-case an ASCII letter, for sorting and extensions
static int lower(char c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : (unsigned char) c;
}

// A function to compare names without regard to ASCII case, then exactly
static int compare_names(const char *a, const char *b)
{
    const char *x = a;
    const char *y = b;
    while (*x != '\0' && lower(*x) == lower(*y)) {
        x++;
        y++;
    }
    int difference = lower(*x) - lower(*y);
    return difference != 0 ? difference : strcmp(a, b);
}

// A function to sort entries by name, for qsort
static int compare_entries(const void *a, const void *b)
{
    const FileioEntry *x = *(const FileioEntry *const *) a;
    const FileioEntry *y = *(const FileioEntry *const *) b;
    return compare_names(x->name, y->name);
}

// A function to tell an image file by its extension, whatever its case
bool browser_is_image(const char *name)
{
    size_t length = strlen(name);
    for (size_t i = 0; i < sizeof(IMAGE_EXTENSIONS) / sizeof(IMAGE_EXTENSIONS[0]); i++) {
        size_t extension_length = strlen(IMAGE_EXTENSIONS[i]);
        if (length <= extension_length)
            continue;
        const char *tail = name + length - extension_length;
        size_t k = 0;
        while (k < extension_length && lower(tail[k]) == IMAGE_EXTENSIONS[i][k])
            k++;
        if (k == extension_length)
            return true;
    }
    return false;
}

// A function to tell a root, which has no parent: "/", "C:", "C:\" or "\\server\share"
static bool is_root(const char *path, size_t length)
{
    if (length == 1 && is_separator(path[0]))
        return true;
    if ((length == 2 || length == 3) && path[1] == ':' && (length == 2 || is_separator(path[2])))
        return true;
    if (length >= 2 && is_separator(path[0]) && is_separator(path[1])) {
        int separators = 0;
        for (size_t i = 2; i < length; i++) {
            if (is_separator(path[i]))
                separators++;
        }
        return separators <= 1;
    }
    return false;
}

// A function to find a path's parent folder; false for a root or a bare name
bool browser_parent(const char *path, char *out, size_t size)
{
    size_t length = strlen(path);
    while (length > 1 && is_separator(path[length - 1]) && !is_root(path, length))
        length--;
    if (length == 0 || is_root(path, length))
        return false;
    size_t cut = length;
    while (cut > 0 && !is_separator(path[cut - 1]))
        cut--;
    if (cut == 0)
        return false;

    // Keep the separator when the parent is a root ("/", "C:\"), drop it otherwise
    if (!is_root(path, cut))
        cut--;
    if (cut + 1 > size)
        return false;
    memcpy(out, path, cut);
    out[cut] = '\0';
    return true;
}

// A function to join a folder and a name with the folder's own kind of separator
static bool join_path(const char *folder, const char *name, char *out, size_t size)
{
    size_t length = strlen(folder);
    bool windows = strchr(folder, '\\') != NULL || (length >= 2 && folder[1] == ':');
    const char *between = length > 0 && is_separator(folder[length - 1]) ? "" : (windows ? "\\" : "/");
    int written = snprintf(out, size, "%s%s%s", folder, between, name);
    return written > 0 && (size_t) written < size;
}

// A function to free the rows on show
static void free_rows(Browser *browser)
{
    for (int i = 0; i < browser->row_count; i++) {
        free(browser->rows[i].name);
        free(browser->rows[i].path);
    }
    free(browser->rows);
    browser->rows = NULL;
    browser->row_count = 0;
}

// A function to add a row
static void add_row(Browser *browser, int capacity, BrowserRowKind kind, const char *name, const char *path,
                    bool enabled, const char *why)
{
    if (browser->row_count >= capacity)
        return;
    BrowserRow *row = &browser->rows[browser->row_count++];
    row->kind = kind;
    row->name = strdup(name);
    row->path = strdup(path);
    row->enabled = enabled;
    row->why = why;
    row->image_count = 0;
}

// A function to put the cursor on the row for a path, or on the first row
static void select_path(Browser *browser, const char *path)
{
    browser->cursor = 0;
    for (int i = 0; path != NULL && i < browser->row_count; i++) {
        if (strcmp(browser->rows[i].path, path) == 0) {
            browser->cursor = i;
            return;
        }
    }
}

// A function to show the places, with the cursor on `selected` when it is one of them
static void load_places(Browser *browser, const char *selected)
{
    free_rows(browser);
    free(browser->folder);
    browser->folder = NULL;
    browser->rows = calloc((size_t) (browser->place_count > 0 ? browser->place_count : 1), sizeof(BrowserRow));
    for (int i = 0; browser->rows != NULL && i < browser->place_count; i++)
        add_row(browser, browser->place_count, BROWSER_ROW_PLACE, browser->places[i].label, browser->places[i].path, true, NULL);
    select_path(browser, selected);
}

// A function to show a folder: folders first, then images, each sorted by name, hidden files left
// out; in folder mode "Use this folder" comes first. False when the folder cannot be listed.
static bool load_folder(Browser *browser, const char *folder, const char *selected)
{
    FileioEntry *entries = NULL;
    int count = browser->list(folder, &entries, browser->context);
    if (count < 0)
        return false;
    FileioEntry **folders = calloc((size_t) (count > 0 ? count : 1), sizeof(FileioEntry*));
    FileioEntry **images = calloc((size_t) (count > 0 ? count : 1), sizeof(FileioEntry*));
    int folder_count = 0;
    int image_count = 0;
    for (int i = 0; folders != NULL && images != NULL && i < count; i++) {
        if (entries[i].hidden)
            continue;
        if (entries[i].is_dir)
            folders[folder_count++] = &entries[i];
        else if (browser_is_image(entries[i].name))
            images[image_count++] = &entries[i];
    }
    qsort(folders, (size_t) folder_count, sizeof(FileioEntry*), compare_entries);
    qsort(images, (size_t) image_count, sizeof(FileioEntry*), compare_entries);

    char *copy = strdup(folder);   // `folder` may be a row's path, which is about to be freed
    free_rows(browser);
    free(browser->folder);
    browser->folder = copy;
    int capacity = folder_count + image_count + 1;
    browser->rows = calloc((size_t) capacity, sizeof(BrowserRow));
    char path[BROWSER_PATH_MAX];
    if (browser->rows != NULL && browser->mode == BROWSER_FOLDER) {
        const char *why = image_count < 2 ? TOO_FEW : (browser->check != NULL ? browser->check(copy, browser->context) : NULL);
        add_row(browser, capacity, BROWSER_ROW_USE_FOLDER, "Use this folder", copy, why == NULL, why);
        browser->rows[browser->row_count - 1].image_count = image_count;
    }
    for (int i = 0; browser->rows != NULL && i < folder_count; i++) {
        if (join_path(copy, folders[i]->name, path, sizeof(path)))
            add_row(browser, capacity, BROWSER_ROW_FOLDER, folders[i]->name, path, true, NULL);
    }
    for (int i = 0; browser->rows != NULL && i < image_count; i++) {
        if (!join_path(copy, images[i]->name, path, sizeof(path)))
            continue;
        const char *why = NULL;
        if (browser->mode == BROWSER_IMAGE && browser->check != NULL)
            why = browser->check(path, browser->context);
        add_row(browser, capacity, BROWSER_ROW_IMAGE, images[i]->name, path, browser->mode == BROWSER_IMAGE && why == NULL, why);
    }
    free(folders);
    free(images);
    fileio_free_list(entries, count);
    select_path(browser, selected);
    return true;
}

// A function to open the browser: at `start` (an image opens its folder with the image highlighted),
// else at the first place that can be listed (Pictures, then Home, ...), else at the places
Browser *browser_open(BrowserMode mode, const char *start, const BrowserPlace *places, int place_count,
                      BrowserList list, BrowserCheck check, void *context)
{
    Browser *browser = calloc(1, sizeof(Browser));
    if (browser == NULL)
        return NULL;
    browser->mode = mode;
    browser->list = list;
    browser->check = check;
    browser->context = context;
    browser->places = calloc((size_t) (place_count > 0 ? place_count : 1), sizeof(BrowserPlace));
    for (int i = 0; browser->places != NULL && i < place_count; i++) {
        browser->places[i].label = strdup(places[i].label);
        browser->places[i].path = strdup(places[i].path);
        browser->place_count++;
    }
    bool opened = false;
    if (start != NULL && start[0] != '\0') {
        char parent[BROWSER_PATH_MAX];
        if (mode == BROWSER_IMAGE && browser_is_image(start)) {
            if (browser_parent(start, parent, sizeof(parent)))
                opened = load_folder(browser, parent, start);
        }
        else
            opened = load_folder(browser, start, NULL);
    }
    for (int i = 0; !opened && i < browser->place_count; i++)
        opened = load_folder(browser, browser->places[i].path, NULL);
    if (!opened)
        load_places(browser, NULL);
    return browser;
}

// A function to free the browser
void browser_free(Browser *browser)
{
    if (browser == NULL)
        return;
    free_rows(browser);
    free(browser->folder);
    for (int i = 0; i < browser->place_count; i++) {
        free((char*) browser->places[i].label);
        free((char*) browser->places[i].path);
    }
    free(browser->places);
    free(browser);
}

// A function to act on one key: move, page, open a folder or place, choose, or go back up
BrowserResult browser_command(Browser *browser, BrowserCommand command, int page_rows)
{
    int last = browser->row_count - 1;
    int before = browser->cursor;
    if (page_rows < 1)
        page_rows = 1;
    switch (command) {
        case BROWSER_UP:
            if (browser->cursor > 0)
                browser->cursor--;
            break;
        case BROWSER_DOWN:
            if (browser->cursor < last)
                browser->cursor++;
            break;
        case BROWSER_PAGE_UP:
            browser->cursor = browser->cursor - page_rows < 0 ? 0 : browser->cursor - page_rows;
            break;
        case BROWSER_PAGE_DOWN:
            browser->cursor = browser->cursor + page_rows > last ? (last > 0 ? last : 0) : browser->cursor + page_rows;
            break;
        case BROWSER_OK: {
            if (browser->cursor < 0 || browser->cursor > last)
                return BROWSER_NONE;
            const BrowserRow *row = &browser->rows[browser->cursor];
            if (row->kind == BROWSER_ROW_PLACE || row->kind == BROWSER_ROW_FOLDER) {
                char path[BROWSER_PATH_MAX];
                snprintf(path, sizeof(path), "%s", row->path);
                return load_folder(browser, path, NULL) ? BROWSER_MOVED : BROWSER_NONE;
            }
            if (!row->enabled)
                return BROWSER_NONE;
            snprintf(browser->chosen, sizeof(browser->chosen), "%s", row->path);
            return BROWSER_CHOSEN;
        }
        case BROWSER_BACK: {
            if (browser->folder == NULL)
                return BROWSER_CLOSED;
            char from[BROWSER_PATH_MAX];
            char parent[BROWSER_PATH_MAX];
            snprintf(from, sizeof(from), "%s", browser->folder);
            if (!browser_parent(from, parent, sizeof(parent)) || !load_folder(browser, parent, from))
                load_places(browser, from);
            return BROWSER_MOVED;
        }
    }
    return browser->cursor != before ? BROWSER_MOVED : BROWSER_NONE;
}

// A function to count the rows on show
int browser_row_count(const Browser *browser)
{
    return browser->row_count;
}

// A function to get a row on show
const BrowserRow *browser_row(const Browser *browser, int index)
{
    return index >= 0 && index < browser->row_count ? &browser->rows[index] : NULL;
}

// A function to get the cursor
int browser_cursor(const Browser *browser)
{
    return browser->cursor;
}

// A function to get the folder on show; NULL while the places are shown
const char *browser_folder(const Browser *browser)
{
    return browser->folder;
}

// A function to get the path just chosen
const char *browser_chosen(const Browser *browser)
{
    return browser->chosen;
}

// A function to find a folder's first image by name, for previewing a folder
bool browser_first_image(const Browser *browser, const char *folder, char *out, size_t size)
{
    FileioEntry *entries = NULL;
    int count = browser->list(folder, &entries, browser->context);
    const FileioEntry *first = NULL;
    for (int i = 0; i < count; i++) {
        if (entries[i].hidden || entries[i].is_dir || !browser_is_image(entries[i].name))
            continue;
        if (first == NULL || compare_names(entries[i].name, first->name) < 0)
            first = &entries[i];
    }
    bool found = first != NULL && join_path(folder, first->name, out, size);
    fileio_free_list(entries, count > 0 ? count : 0);
    return found;
}
```

- [ ] **Step 7: Run the tests to verify they pass**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 9`. `test_fileio` now also lists this machine's places: Pictures, Home and the drives here, and Home, `/` and any mounts in CI's container.

- [ ] **Step 8: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/browser.h src/browser.c tests/test_browser.c src/fileio.h src/fileio.c tests/test_fileio.c src/CMakeLists.txt tests/CMakeLists.txt
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: the folder browser's model and the places it starts from"
```

---

### Task 9: A background that can be reloaded, and the scene apart from the frame

The settings screen needs three things the launcher cannot do yet:
- **Set the background up again at any time.** The code for it lives inline in `main()`, and when it falls back it rewrites `config.background_mode` (and even `config.background_image`).
- **Draw the scene into a texture.** `draw_screen()` draws, presents and waits in one function.
- **Remember the config file's path.** It is freed right after parsing.

This task refactors all three without changing what the launcher does:
- A new `background_shown` holds what is on screen, while `config.background_mode` keeps what the config chose.
- `main()` sets the background up through the same `reload_background()` the screen will call.
- Pinned by new harness checks of every background startup path, including a crash they found (`Mode=Slideshow` with no `SlideshowDirectory` passed `NULL` to `directory_exists()`).

Steps 10-14 then fix the slideshow loader, as a separate commit (see **Existing bugs fixed on the way**). It runs on its own thread, yet when it runs out of images it falls back there: it makes a texture, frees the slideshow and then writes into it. At startup it can also loop forever. After the fix the loader only loads and reports, and the main thread falls back.

**Files:**
- Modify: `src/launcher.h`, `src/launcher.c`, `src/image.c`, `src/platform/platform.h`, `src/platform/win32.c`
- Modify: `tests/headless/Dockerfile` (python3), `tests/headless/run.sh` (the test pictures)
- Create: `tests/headless/make_images.py`, `tests/headless/checks/30-backgrounds.sh`; fixtures `f30-image.ini`, `f30-missing.ini`, `f30-slideshow.ini`, `f30-one.ini`, `f30-empty.ini`, `f30-nodir.ini`, `f30-broken.ini`, `f30-mixed.ini`, `f30-vanish.ini`

**Interfaces:**
- Produces (Tasks 10 and 11 call these; declare them in `launcher.h`):

```c
extern ModeBackground background_shown;  // what is on screen; config.background_mode is what was chosen
void draw_scene(void);                   // the background, overlay, arrows, clock, highlight and buttons
void present_frame(void);                // present, and without VSync wait out the frame
void reload_background(void);            // set the background up for config.background_mode
void update_slideshow_timing(void);      // the fade speed, after SlideshowTransitionTime changes
void reload_titles(void);                // every menu's titles again; the menu on show now
void refresh_layout(void);               // lay the menu on show out again
int show_menu(Menu *menu);               // show a menu, keeping its back link and position (0 = shown)
void show_home(void);                    // what :home does
// Config gains: char *config_path;      // the file the settings were read from
// Slideshow gains: bool only_one;        // set by the loader: the only image that loads is the one on show
// platform.h: void make_window_transparent(void); void make_window_opaque(void);
```

- Harness: `run.sh` leaves `/home/tester/Pictures` (`blue.png`, `green.png`, `red.png`), `/home/tester/one` (one image), `/home/tester/empty`, `/home/tester/broken` (two files that only look like pictures) and `/home/tester/mixed` (one picture and one of those) for the checks to use.

- [ ] **Step 1: Give the harness pictures.** In `tests/headless/Dockerfile`, add `python3` to the `apt-get install` line after `libgl1-mesa-dri`. Create `tests/headless/make_images.py`:

```python
"""Write three small solid-colour PNGs for the headless background checks.

Usage: python3 make_images.py <folder>
"""
import os
import struct
import sys
import zlib


def png(path, width, height, rgb):
    """Write a solid RGB PNG, with no library beyond the standard one."""
    raw = b"".join(b"\x00" + bytes(rgb) * width for _ in range(height))

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw)))
        f.write(chunk(b"IEND", b""))


folder = sys.argv[1]
os.makedirs(folder, exist_ok=True)
for name, rgb in (("blue", (40, 70, 160)), ("green", (40, 140, 70)), ("red", (170, 40, 40))):
    png(os.path.join(folder, name + ".png"), 64, 36, rgb)
```

In `tests/headless/run.sh`, after the `LOG=...` line, add:

```bash
# Pictures for the background checks: three in Pictures, one on its own, and an empty folder
python3 "$HERE/make_images.py" "$TESTER_HOME/Pictures"
mkdir -p "$TESTER_HOME/one" "$TESTER_HOME/empty"
cp "$TESTER_HOME/Pictures/red.png" "$TESTER_HOME/one/"
chown -R tester:tester "$TESTER_HOME"
```

- [ ] **Step 2: Write the checks.** Create `tests/headless/checks/30-backgrounds.sh`:

```bash
# The background's startup paths, which now all go through reload_background(): an image, a
# missing image, a slideshow, a slideshow folder with one image, one with none, and none at all.
# The setting is kept as chosen even when the launcher falls back to the colour.

run_quick f30-image
ok=1
[ "$(cat "$out/f30-image.code")" = 0 ] && ! grep -q "Couldn't load background image" "$out/f30-image.log" \
    && sanitizer_clean f30-image && ok=0
result "an image background loads" $ok

run_quick f30-missing
ok=1
[ "$(cat "$out/f30-missing.code")" = 0 ] && grep -q "Couldn't load background image" "$out/f30-missing.log" \
    && grep -A2 'Background ===' "$out/f30-missing.log" | grep -qE 'Mode:\s+Image$' \
    && sanitizer_clean f30-missing && ok=0
result "a missing image falls back to the colour, and the setting stays Image" $ok

run_quick f30-slideshow
ok=1
[ "$(cat "$out/f30-slideshow.code")" = 0 ] && grep -q "Found 3 images in directory /home/tester/Pictures" "$out/f30-slideshow.log" \
    && sanitizer_clean f30-slideshow && ok=0
result "a slideshow finds its three images" $ok

run_quick f30-one
ok=1
[ "$(cat "$out/f30-one.code")" = 0 ] && grep -q "Only one image found" "$out/f30-one.log" \
    && grep -A4 'Background ===' "$out/f30-one.log" | grep -qE 'Image:\s+\(null\)$' \
    && sanitizer_clean f30-one && ok=0
result "a one-image slideshow shows the image without rewriting the Image setting" $ok

run_quick f30-empty
ok=1
[ "$(cat "$out/f30-empty.code")" = 0 ] && grep -q "No images found in slideshow directory" "$out/f30-empty.log" \
    && sanitizer_clean f30-empty && ok=0
result "an empty slideshow folder falls back to the colour" $ok

run_quick f30-nodir
ok=1
[ "$(cat "$out/f30-nodir.code")" = 0 ] && grep -q "does not exist" "$out/f30-nodir.log" \
    && sanitizer_clean f30-nodir && ok=0
result "Mode=Slideshow with no SlideshowDirectory falls back instead of crashing (exit $(cat "$out/f30-nodir.code"))" $ok
```

Create the fixtures. `f30-image.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Background]
Mode=Image
Image=/home/tester/Pictures/red.png

[Main]
Entry1=One;apps;:quit
```

- `f30-missing.ini`: the same, with `Image=/home/tester/Pictures/missing.png`.
- `f30-slideshow.ini`: the same `[General]` and `[Main]`, with:

  ```ini
  [Background]
  Mode=Slideshow
  SlideshowDirectory=/home/tester/Pictures
  ```

- `f30-one.ini`: as `f30-slideshow.ini`, with `SlideshowDirectory=/home/tester/one`.
- `f30-empty.ini`: as `f30-slideshow.ini`, with `SlideshowDirectory=/home/tester/empty`.
- `f30-nodir.ini`: as `f30-slideshow.ini`, without the `SlideshowDirectory` line.

- [ ] **Step 3: Run the harness and watch it fail**

Expected:
- `f30-one` fails: today's code copies the image's path into `Image`.
- `f30-nodir` fails: the launcher crashes in `directory_exists(NULL)` (ASan `SEGV`).
- `f30-missing` fails on its `Mode: Image` half: today's fallback rewrites the setting to `Color` before the debug output is written. Its log line already matches; the `Mode: Image` half is what pins the refactor.

- [ ] **Step 4: What is shown, apart from what was chosen.** In `src/launcher.h`, add `char *config_path; // The file the settings were read from` to `Config` after `char *exe_path;`, and at the end of the file:

```c
#define SCMD_SETTINGS ":settings"

extern ModeBackground background_shown;
void draw_scene(void);
void present_frame(void);
void reload_background(void);
void update_slideshow_timing(void);
void reload_titles(void);
void refresh_layout(void);
int show_menu(Menu *menu);
void show_home(void);
```

(put `#define SCMD_SETTINGS` with the other special commands, after `SCMD_SLEEP`, rather than at the end). In `src/launcher.c`, after `Menu *current_menu = NULL;`, add:

```c
ModeBackground background_shown       = BACKGROUND_COLOR; // What is on screen: the colour when the chosen background failed
```

In `set_draw_color()`, replace both `config.background_mode` with `background_shown`.

Replace `quit_slideshow()` with:

```c
// A function to free the slideshow, if there is one
void quit_slideshow()
{
    if (slideshow == NULL)
        return;
    for (int i = 0; i < slideshow->num_images; i++)
        free(slideshow->images[i]);
    free(slideshow->images);
    free(slideshow->order);
    free(slideshow);
    slideshow = NULL;
}
```

Replace `init_slideshow()` with:

```c
// A function to scan the slideshow folder. What is shown falls back to the colour, or to a single
// image, when the folder is missing or holds fewer than two images; the settings are left alone.
static void init_slideshow()
{
    if (config.slideshow_directory == NULL || !directory_exists(config.slideshow_directory)) {
        log_error("Slideshow directory '%s' does not exist, "
            "Switching to color background mode",
            config.slideshow_directory != NULL ? config.slideshow_directory : "(none)"
        );
        background_shown = BACKGROUND_COLOR;
        return;
    }
    slideshow = malloc(sizeof(Slideshow));
    *slideshow = (Slideshow) {
        .i = -1,
        .num_images = 0,
        .transition_surface = NULL,
        .transition_texture = NULL,
        .transition_alpha = 0.f,
        .transition_change_rate = 0.f,
        .images = NULL,
        .order = NULL
    };
    scan_slideshow_directory(slideshow, config.slideshow_directory);
    if (!slideshow->num_images) {
        log_error("No images found in slideshow directory '%s', "
            "Changing background mode to color",
            config.slideshow_directory
        );
        background_shown = BACKGROUND_COLOR;
        quit_slideshow();
    }
    else if (slideshow->num_images == 1) {
        log_error("Only one image found in slideshow directory %s, showing it as a single image",
            config.slideshow_directory
        );
        background_texture = load_texture_from_file(slideshow->images[0]);
        background_shown = background_texture != NULL ? BACKGROUND_IMAGE : BACKGROUND_COLOR;
        quit_slideshow();
    }
    else {
        slideshow->order = malloc(sizeof(int) * (size_t) slideshow->num_images);
        random_array(slideshow->order, slideshow->num_images);
        if (config.debug)
            debug_slideshow(slideshow);
    }
}
```

Leave `load_next_slideshow_background()` in `src/image.c` alone in this step: Step 12 replaces it whole.

- [ ] **Step 5: `reload_background()` and the timing.** In `src/launcher.c`, add after `resume_slideshow()`:

```c
// A function to work out the slideshow's fade speed from its fade time and the frame rate
void update_slideshow_timing()
{
    if (slideshow != NULL && config.slideshow_transition_time > 0)
        slideshow->transition_change_rate = 255.0f / ((float) config.slideshow_transition_time / (float) refresh_period);
}

// A function to set the background up for config.background_mode: at startup, and whenever the
// settings screen changes it. What is shown (background_shown) falls back to the colour when an
// image or slideshow cannot be used; the setting itself stays as it was chosen.
void reload_background()
{
    // Stop the slideshow: wait for an image being loaded on its thread, then free it all
    if (Slideshowhread != NULL) {
        SDL_WaitThread(Slideshowhread, NULL);
        Slideshowhread = NULL;
    }
    if (slideshow != NULL) {
        if (slideshow->transition_surface != NULL)
            SDL_FreeSurface(slideshow->transition_surface);
        if (slideshow->transition_texture != NULL)
            SDL_DestroyTexture(slideshow->transition_texture);
        quit_slideshow();
    }
    state.slideshow_transition = false;
    state.slideshow_background_rendering = false;
    state.slideshow_background_ready = false;
    if (background_texture != NULL) {
        SDL_DestroyTexture(background_texture);
        background_texture = NULL;
    }

    background_shown = config.background_mode;
    if (config.background_mode == BACKGROUND_IMAGE) {
        if (config.background_image == NULL)
            log_error("Background 'Image' setting not specified in config file");
        else
            background_texture = load_texture_from_file(config.background_image);
        if (background_texture == NULL) {
            log_error("Couldn't load background image, defaulting to color background");
            background_shown = BACKGROUND_COLOR;
        }
    }
    else if (config.background_mode == BACKGROUND_SLIDESHOW) {
        init_slideshow();
        if (background_shown == BACKGROUND_SLIDESHOW) {
            SDL_Surface *surface = load_next_slideshow_background(slideshow, false);

            // With one loadable image it made its own texture as well; this one replaces it
            if (surface != NULL) {
                if (background_texture != NULL)
                    SDL_DestroyTexture(background_texture);
                background_texture = load_texture(surface);
            }
            ticks.slideshow_load = ticks.main;
        }
    }
    update_slideshow_timing();
#ifdef _WIN32
    if (background_shown == BACKGROUND_TRANSPARENT)
        make_window_transparent();
    else
        make_window_opaque();
#endif
    set_draw_color();
    log_debug("Background set up: %s", get_mode_setting(MODE_SETTING_BACKGROUND, (int) background_shown));
}
```

In `create_window()`, delete:

```c
    if (slideshow != NULL)
        slideshow->transition_change_rate = 255.0f / ((float) config.slideshow_transition_time / (float) refresh_period);
```

and, inside its `#ifdef _WIN32` block, delete:

```c
    if (config.background_mode == BACKGROUND_TRANSPARENT)
        make_window_transparent();
```

(keep the `SDL_VERSION` and `SDL_GetWindowWMInfo` lines).

In `main()`:
- delete the block `// Initialize slideshow` with its `if (config.background_mode == BACKGROUND_SLIDESHOW) init_slideshow();`;
- replace the two blocks `// Render background` and `// Render first slideshow image`, from `if (config.background_mode == BACKGROUND_IMAGE) {` through the slideshow block's closing brace, with:

  ```c
      // Set the background up
      reload_background();
  ```

- replace the `free(config_file_path);` straight after `parse_config_file(config_file_path);` with:

  ```c
      config.config_path = config_file_path;   // The settings screen saves here
  ```

Replace every other test of `config.background_mode` that decides what is drawn or animated with `background_shown`:
- `update_screensaver()`, both places;
- `post_launch()`, both places;
- the main loop's `if (config.background_mode == BACKGROUND_SLIDESHOW) update_slideshow();`;
- `draw_screen()`, both places (moved into `draw_scene()` in Step 6).

In `cleanup()`, replace:

```c
    if (config.background_mode == BACKGROUND_SLIDESHOW)
        quit_slideshow();
```

with `quit_slideshow();`, and add `free(config.config_path);` after `free(config.exe_path);`.

- [ ] **Step 6: The scene apart from presenting.** Replace `draw_screen()` with three functions. The body of `draw_scene()` is `draw_screen()`'s old body from `// Draw background` through `// Draw the visible buttons`' loop, unchanged except for `background_shown` and the new first two lines:

```c
// A function to draw the launcher's scene: the background, its overlay, the scroll indicators,
// the clock, the highlight and the visible buttons. The settings screen draws it into its preview.
void draw_scene()
{
    set_draw_color();
    SDL_RenderClear(renderer);
    if (background_shown == BACKGROUND_IMAGE || background_shown == BACKGROUND_SLIDESHOW)
        SDL_RenderCopy(renderer, background_texture, NULL, NULL);
    if (background_shown == BACKGROUND_SLIDESHOW && state.slideshow_transition)
        SDL_RenderCopy(renderer, slideshow->transition_texture, NULL, NULL);

    // Draw background overlay
    if (config.background_overlay)
        SDL_RenderCopy(renderer, background_overlay, NULL, NULL);

    // ... the scroll indicators, clock, highlight and visible buttons, exactly as in draw_screen() ...
}

// A function to show the frame, and without VSync wait out the rest of its time
void present_frame()
{
    SDL_RenderPresent(renderer);
    if (!config.vsync) {
        Uint32 elapsed = SDL_GetTicks() - ticks.main;
        if (elapsed < refresh_period)
            SDL_Delay(refresh_period - elapsed);
    }
}

// A function to update the screen: the scene and the screensaver's dimming, or a blank screen
// while an application is launching
static void draw_screen()
{
    if (!(state.application_launching && config.on_launch == ON_LAUNCH_BLANK)) {
        draw_scene();
        if (state.screensaver_active)
            SDL_RenderCopy(renderer, screensaver->texture, NULL, NULL);
    }
    else {
        SDL_RenderClear(renderer);
        SDL_RenderFillRect(renderer, NULL);
    }
    present_frame();
}
```

The `...` comment stands for code moved verbatim, not for code to write: cut the scroll indicator, clock, highlight and button blocks out of the old `draw_screen()` and paste them there. `draw_scene()` must not contain the screensaver or the blank-screen branch.

- [ ] **Step 7: The functions the settings screen calls.** After `load_back_menu()`, add:

```c
// A function to render every menu's titles again after the title size changed: the menu on show
// now, the others when they are next opened
void reload_titles()
{
    for (Menu *menu = config.first_menu; menu != NULL; menu = menu->next)
        menu->rendered_size = 0;
    apply_layout(current_menu);
}

// A function to lay the menu on show out again after its grid changed
void refresh_layout()
{
    apply_layout(current_menu);
}

// A function to show a menu without changing its back link or remembered position
int show_menu(Menu *menu)
{
    return load_menu(menu, false, false);
}

// A function to go to the default menu, as :home does
void show_home()
{
    load_menu(default_menu, false, true);
}
```

In `src/platform/platform.h`, replace `void set_window_transparent(void);` with:

```c
void make_window_transparent(void);
void make_window_opaque(void);
```

In `src/platform/win32.c`, after `make_window_transparent()`, add:

```c
// A function to make the window solid again after a transparent background
void make_window_opaque()
{
    HWND hwnd = wm_info.info.win.window;
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
}
```

- [ ] **Step 8: Build and run everything**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: no new warnings (MSVC no longer sees an undeclared `make_window_transparent`), and `100% tests passed, 0 tests failed out of 9`. Then rebuild the harness image (the Dockerfile changed) and run it. Expected: every `30-backgrounds` line passes, every earlier check still passes, `0 failed`.

- [ ] **Step 9: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/launcher.h src/launcher.c src/platform/platform.h src/platform/win32.c tests/headless/Dockerfile tests/headless/run.sh tests/headless/make_images.py tests/headless/checks/30-backgrounds.sh tests/headless/fixtures/f30-image.ini tests/headless/fixtures/f30-missing.ini tests/headless/fixtures/f30-slideshow.ini tests/headless/fixtures/f30-one.ini tests/headless/fixtures/f30-empty.ini tests/headless/fixtures/f30-nodir.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "refactor: reload the background at any time, and draw the scene apart from the frame"
```

- [ ] **Step 10: Write the slideshow loader checks.** In `tests/headless/run.sh`, in the block Step 1 added, insert before `chown -R tester:tester "$TESTER_HOME"`:

```bash
# Slideshow folders that fail: two files that only look like pictures, and one picture beside one
mkdir -p "$TESTER_HOME/broken" "$TESTER_HOME/mixed"
printf 'not a picture\n' > "$TESTER_HOME/broken/a.png"
printf 'not a picture\n' > "$TESTER_HOME/broken/b.png"
cp "$TESTER_HOME/Pictures/red.png" "$TESTER_HOME/mixed/"
printf 'not a picture\n' > "$TESTER_HOME/mixed/broken.png"
```

Append to `tests/headless/checks/30-backgrounds.sh`:

```bash
# The slideshow loader. It runs on its own thread, so when its folder stops giving it two pictures
# it only reports, and the main thread falls back: to the one picture that still loads, or to the
# colour. The Mode setting stays Slideshow.

# A function to run a slideshow fixture that keeps running: wait until its first picture is up,
# run the rest of the arguments as a command (which may take the pictures away), then give the
# loader time to try the next picture (the fixtures change every 5 s, the shortest allowed)
# before quitting it. A launcher that hangs is killed.
run_slideshow() {
    local name=$1; shift
    local pid i
    rm -f "$LOG"
    "${TESTER[@]}" "$exe" -c "$FX/$name.ini" -d > "$out/$name.out" 2> "$out/$name.err" &
    pid=$!
    for i in $(seq 100); do grep -q 'Background set up: Slideshow' "$LOG" 2> /dev/null && break; sleep 0.2; done
    "$@"
    sleep 8
    kill -TERM "$pid" 2> /dev/null
    for i in $(seq 50); do kill -0 "$pid" 2> /dev/null || break; sleep 0.2; done
    kill -KILL "$pid" 2> /dev/null
    wait "$pid"; echo $? > "$out/$name.code"
    cp "$LOG" "$out/$name.log" 2> /dev/null || : > "$out/$name.log"
}

# At startup: no file in the folder loads
run_quick f30-broken
ok=1
[ "$(cat "$out/f30-broken.code")" = 0 ] \
    && grep -q "Could not load any image from slideshow directory /home/tester/broken" "$out/f30-broken.log" \
    && grep -q "Background set up: Color" "$out/f30-broken.log" \
    && sanitizer_clean f30-broken && ok=0
result "a slideshow whose files all fail to load falls back to the colour instead of hanging (exit $(cat "$out/f30-broken.code"))" $ok

# While running: the only picture that loads is the one on show
run_slideshow f30-mixed true
ok=1
[ "$(cat "$out/f30-mixed.code")" = 0 ] \
    && grep -q "Could only load one image from slideshow directory /home/tester/mixed, showing it as a single image" "$out/f30-mixed.log" \
    && sanitizer_clean f30-mixed && ok=0
result "a running slideshow left with one picture shows it as a single image (exit $(cat "$out/f30-mixed.code"))" $ok

# While running: the pictures vanish (a network share dropping, say)
mkdir -p "$TESTER_HOME/vanish"
cp "$TESTER_HOME/Pictures/blue.png" "$TESTER_HOME/Pictures/green.png" "$TESTER_HOME/vanish/"
chown -R tester:tester "$TESTER_HOME/vanish"
run_slideshow f30-vanish rm -f "$TESTER_HOME/vanish/blue.png" "$TESTER_HOME/vanish/green.png"
ok=1
[ "$(cat "$out/f30-vanish.code")" = 0 ] \
    && grep -q "Could not load any image from slideshow directory /home/tester/vanish" "$out/f30-vanish.log" \
    && sanitizer_clean f30-vanish && ok=0
result "a running slideshow whose pictures vanish falls back to the colour (exit $(cat "$out/f30-vanish.code"))" $ok
grep -m2 -E 'runtime error|AddressSanitizer' "$out/f30-vanish.err" | sed 's/^/      /'
```

Create the fixtures. `f30-broken.ini` is `f30-slideshow.ini` with `SlideshowDirectory=/home/tester/broken`. `f30-mixed.ini` keeps running, so it has no `StartupCmd`:

```ini
[General]
DefaultMenu=Main

[Background]
Mode=Slideshow
SlideshowDirectory=/home/tester/mixed
SlideshowImageDuration=5
SlideshowTransitionTime=0

[Main]
Entry1=One;apps;:quit
```

`f30-vanish.ini` is the same with `SlideshowDirectory=/home/tester/vanish`.

`f30-mixed` does not depend on the random order: whichever of its two files comes first, the loader's next attempt fails on `broken.png` and comes back round to `red.png`, the image on show.

- [ ] **Step 11: Rebuild the harness image, run it and watch the three fail**

Expected:
- `f30-broken` fails with exit 137, after about 35 s: the startup load loops forever, so `timeout`'s TERM at 30 s is only queued as a quit event, and its KILL 5 s later ends it.
- `f30-vanish` fails with a non-zero exit, and `AddressSanitizer: heap-use-after-free` under it: the loader thread freed the slideshow and then stored its result in it.
- `f30-mixed` fails on its log line: today's reads "Changing background to single image mode", logged from the loader thread after it made a texture there. The harness cannot see which thread made a texture; this check pins the fall-back's behaviour, and the task's review checks the thread rule by reading.
- Every other line still passes.

- [ ] **Step 12: The loader only loads; the main thread falls back.** In `src/launcher.h`, add to `Slideshow`, after `SDL_Texture *transition_texture;`:

```c
    bool only_one;   // Set by the loader: the only image that loads is the one already on show
```

In `src/image.c`, replace `load_next_slideshow_background()` whole with:

```c
// A function to load the next slideshow image that loads. It also runs on the slideshow thread, so
// it touches nothing but the slideshow: textures, the draw colour and what is shown belong to the
// main thread. It returns NULL when no image in the folder loads, and sets slideshow->only_one when
// the only one that does is the image already on show; the main thread falls back from either.
SDL_Surface *load_next_slideshow_background(Slideshow *slideshow, bool transition)
{
    SDL_Surface *surface = NULL;
    int initial_index = slideshow->i;

    // Try each image once at most, starting after the one on show (i is -1 before the first)
    for (int attempts = 0; surface == NULL && attempts < slideshow->num_images; attempts++) {
        slideshow->i = (slideshow->i + 1) % slideshow->num_images;
        surface = IMG_Load(slideshow->images[slideshow->order[slideshow->i]]);

        // If the loaded image has no alpha channel (e.g. JPEG), create one
        // so that we can have transparency for the background transition
        if (surface != NULL && surface->format->format == SDL_PIXELFORMAT_RGB24 && transition) {
            SDL_Surface *tmp = SDL_CreateRGBSurfaceWithFormat(0,
                                   surface->w,
                                   surface->h,
                                   32,
                                   SDL_PIXELFORMAT_ARGB8888
                                );
            if (tmp != NULL) {
                Uint32 color = SDL_MapRGBA(tmp->format, 0, 0, 0, 0xFF);
                SDL_FillRect(tmp, NULL, color);
                SDL_BlitSurface(surface, NULL, tmp, NULL);
                SDL_FreeSurface(surface);
                surface = tmp;
            }
        }
    }
    slideshow->only_one = surface != NULL && slideshow->i == initial_index;
    return surface;
}
```

In `src/launcher.c`:
- Add `.only_one = false` to `init_slideshow()`'s compound literal, after `.order = NULL` (which gains a comma).
- Add `static void fall_back_from_slideshow(SDL_Surface *surface);` after `static void resume_slideshow(void);` at the top.
- After `quit_slideshow()`, add:

  ```c
  // A function to stop a slideshow that can no longer show two images, on the main thread: show the
  // one image that still loads (surface, the same image as the one on show), or the colour when none
  // does. The Mode setting stays Slideshow, so the folder is tried again when the background is next
  // set up.
  static void fall_back_from_slideshow(SDL_Surface *surface)
  {
      if (surface != NULL) {
          log_error("Could only load one image from slideshow directory %s, showing it as a single image",
              config.slideshow_directory
          );
          SDL_FreeSurface(surface);
          background_shown = BACKGROUND_IMAGE;
      }
      else {
          log_error("Could not load any image from slideshow directory %s, showing the background color",
              config.slideshow_directory
          );
          if (background_texture != NULL) {
              SDL_DestroyTexture(background_texture);
              background_texture = NULL;
          }
          background_shown = BACKGROUND_COLOR;
      }
      if (slideshow->transition_texture != NULL)
          SDL_DestroyTexture(slideshow->transition_texture);
      quit_slideshow();
      set_draw_color();
  }
  ```

- In `update_slideshow()`, replace:

  ```c
          else if (state.slideshow_background_ready) {
              SDL_WaitThread(Slideshowhread, NULL);
              Slideshowhread = NULL;
              if (config.slideshow_transition_time > 0) {
  ```

  with:

  ```c
          else if (state.slideshow_background_ready) {
              SDL_WaitThread(Slideshowhread, NULL);
              Slideshowhread = NULL;

              // The loader found no image that loads, or only the one on show: stop the slideshow
              if (slideshow->transition_surface == NULL || slideshow->only_one) {
                  state.slideshow_background_ready = false;
                  fall_back_from_slideshow(slideshow->transition_surface);
                  return;
              }
              if (config.slideshow_transition_time > 0) {
  ```

- In `reload_background()`, replace:

  ```c
              SDL_Surface *surface = load_next_slideshow_background(slideshow, false);

              // With one loadable image it made its own texture as well; this one replaces it
              if (surface != NULL) {
                  if (background_texture != NULL)
                      SDL_DestroyTexture(background_texture);
                  background_texture = load_texture(surface);
              }
              ticks.slideshow_load = ticks.main;
  ```

  with:

  ```c
              SDL_Surface *surface = load_next_slideshow_background(slideshow, false);
              if (surface != NULL) {
                  background_texture = load_texture(surface);
                  ticks.slideshow_load = ticks.main;
              }
              else
                  fall_back_from_slideshow(NULL);
  ```

  At startup `only_one` is never set (nothing is on show yet), so only the no-image case can happen here.

`image.c` no longer calls `quit_slideshow()` or `set_draw_color()`, and no longer reads `config.slideshow_directory`.

- [ ] **Step 13: Build and run everything**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: no new warnings, and `100% tests passed, 0 tests failed out of 9`. Then run the harness. Expected: the three new lines pass, every earlier line still passes, `0 failed`.

- [ ] **Step 14: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/launcher.h src/launcher.c src/image.c tests/headless/run.sh tests/headless/checks/30-backgrounds.sh tests/headless/fixtures/f30-broken.ini tests/headless/fixtures/f30-mixed.ini tests/headless/fixtures/f30-vanish.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "fix: fall back from a failing slideshow on the main thread, and stop its startup loop hanging"
```

---

### Task 10: The settings screen: grids, titles, Discard and the save

The SDL side of the screen:
- **Opening:** `:settings` opens it (with the Start button and the Menu key as defaults). It draws the column and the live preview.
- **Input:** every navigation key, hotkey and gamepad command is routed to it; everything else is ignored while it is open.
- **Changes** go into the running launcher as they are made.
- **Closing:** Back at the top saves and closes, Discard undoes, and a failed save shows its rows.

This task covers every page except the folder browser. The Background page's Mode, Colour, Change every and Fade rows all work. OK on its Image and Folder rows does nothing until Task 11 adds the browser.

**Files:**
- Create: `src/settings_screen.h`, `src/settings_screen.c`
- Modify: `src/launcher.c` (routing, keys, the main loop, clean-up), `src/util.c` (Start for `:settings`), `src/image.h`, `src/image.c` (`find_default_font`), `src/CMakeLists.txt`
- Create: `tests/headless/checks/50-settings.sh`; fixtures `f50-grid.ini`, `f50-hotkey.ini`, `f50-empty.ini`, `f50-pad.ini`, `f50-pad-taken.ini`

**Interfaces:**
- Consumes: the whole of `settings.h` (Task 6); `config_save()` (Task 4); `draw_scene()`, `present_frame()`, `reload_background()`, `update_slideshow_timing()`, `reload_titles()`, `refresh_layout()`, `show_menu()`, `show_home()`, `Config.config_path` (Task 9); `describe_titles()`, `compute_menu_layout()` (Task 7 and before).
- Produces:

```c
bool settings_is_open(void);
void settings_open(void);
void settings_handle_command(const char *command);   // a special command, while settings are open
void settings_draw(void);                             // one frame, presented
void settings_close_now(void);                        // at quit: free everything, save nothing
char *find_default_font(const char *font);            // image.c: the bundled font's path; caller frees
```

- [ ] **Step 1: Write the failing headless checks.** Create `tests/headless/checks/50-settings.sh`:

```bash
# The settings screen, driven by key presses: Menu opens it, the arrows, Return and BackSpace
# move through it, and Back at the top saves. Each check starts from a fresh copy of its fixture
# that the test user can write.

# A function to give the test user a copy of a fixture it can write; prints its path
writable_config() {
    mkdir -p "$TESTER_HOME/cfg"
    rm -f "$TESTER_HOME/cfg/$1.ini" "$TESTER_HOME/cfg/$1.ini.bak" "$TESTER_HOME/cfg/$1.ini.tmp"
    cp "$FX/$1.ini" "$TESTER_HOME/cfg/$1.ini"
    chown -R tester:tester "$TESTER_HOME/cfg"
    chmod 644 "$TESTER_HOME/cfg/$1.ini"
    echo "$TESTER_HOME/cfg/$1.ini"
}

# A function to count the lines that differ between two files (a changed line counts twice)
changed_lines() { diff "$1" "$2" | grep -c '^[<>]'; }

# The keys that open settings, go to All menus, step Columns up once and back out, saving
ALL_MENUS_COLUMNS_UP="Menu Down Return Return Down Right BackSpace BackSpace BackSpace"

# A grid change saves exactly one line, keeps its trailing comment, and leaves the old file as .bak
cfg=$(writable_config f50-grid)
CFG=$cfg run_keys f50-grid $ALL_MENUS_COLUMNS_UP
ok=1
[ "$(changed_lines "$FX/f50-grid.ini" "$cfg")" = 2 ] && grep -qx 'Columns=5 ; four across' "$cfg" \
    && cmp -s "$FX/f50-grid.ini" "$cfg.bak" && [ ! -e "$cfg.tmp" ] \
    && grep -q "Settings opened over menu 'Main'" "$out/f50-grid.log" \
    && grep -q 'Settings: \[Layout\] Columns 4 -> 5' "$out/f50-grid.log" \
    && grep -q "Settings saved 1 change(s) to $cfg (backup: $cfg.bak)" "$out/f50-grid.log" \
    && grep -q 'Key Application' "$out/f50-grid.log" && sanitizer_clean f50-grid && ok=0
result "settings: a grid change saves one line, keeps its comment, and keeps a backup" $ok
diff "$FX/f50-grid.ini" "$cfg" | sed 's/^/      /'

# Stepping a menu's Rows down to "All menus" removes its line
cfg=$(writable_config f50-grid)
CFG=$cfg run_keys f50-inherit Menu Down Return Down Down Return Left Left Left BackSpace BackSpace BackSpace
ok=1
[ "$(changed_lines "$FX/f50-grid.ini" "$cfg")" = 1 ] && ! sed -n '/^\[Games\]/,$p' "$cfg" | grep -q '^Rows=' \
    && grep -q 'Settings: \[Games\] Rows 1 -> (none)' "$out/f50-inherit.log" && sanitizer_clean f50-inherit && ok=0
result "settings: a menu's Rows set to All menus removes the line" $ok

# Discard puts everything back, and closing with nothing changed writes nothing
cfg=$(writable_config f50-grid)
CFG=$cfg run_keys f50-discard Menu Down Return Return Down Right BackSpace BackSpace Down Down Return BackSpace
ok=1
cmp -s "$FX/f50-grid.ini" "$cfg" && [ ! -e "$cfg.bak" ] \
    && grep -q 'Settings: discarded the changes' "$out/f50-discard.log" \
    && grep -q 'Settings: nothing changed' "$out/f50-discard.log" && sanitizer_clean f50-discard && ok=0
result "settings: Discard, then Back, leaves the file untouched" $ok

# A config the launcher cannot write: the failure rows, then Leave without saving
cfg=$(writable_config f50-grid)
chmod 444 "$cfg"
CFG=$cfg run_keys f50-readonly $ALL_MENUS_COLUMNS_UP Down Return
ok=1
cmp -s "$FX/f50-grid.ini" "$cfg" && grep -q "Couldn't save to $cfg: permission denied" "$out/f50-readonly.log" \
    && grep -q 'Settings: leaving without saving' "$out/f50-readonly.log" && sanitizer_clean f50-readonly && ok=0
result "settings: a read-only config shows why, and Leave without saving closes" $ok

# The packaged system config is read-only: the first save becomes the user's own copy
rm -rf /opt/sf "$TESTER_HOME/.config/streamflex" /usr/local/share/streamflex
mkdir -p /opt/sf /usr/local/share/streamflex
cp /work/build/streamflex /opt/sf/ && cp -r /work/build/assets /opt/sf/
cp "$FX/f50-grid.ini" /usr/local/share/streamflex/config.ini
exe=/opt/sf/streamflex CFG=none run_keys f50-system $ALL_MENUS_COLUMNS_UP
user_cfg=$TESTER_HOME/.config/streamflex/config.ini
ok=1
cmp -s "$FX/f50-grid.ini" /usr/local/share/streamflex/config.ini && grep -qx 'Columns=5 ; four across' "$user_cfg" \
    && grep -q "Settings saved 1 change(s) to $user_cfg (backup: none)" "$out/f50-system.log" \
    && sanitizer_clean f50-system && ok=0
result "settings: a read-only system config is saved as ~/.config/streamflex/config.ini" $ok
rm -rf "$TESTER_HOME/.config/streamflex"

# While settings are open, the Esc=:quit hotkey is ignored, and the Menu key closes them again
run_keys f50-hotkey Menu Escape Menu
ok=1
grep -q "Settings: ignoring ':quit' while settings are open" "$out/f50-hotkey.log" \
    && grep -q 'Settings closed' "$out/f50-hotkey.log" && sanitizer_clean f50-hotkey && ok=0
result "settings: a :quit hotkey is ignored while they are open, and Menu closes them" $ok

# A menu with no entries cannot be previewed: the preview stays put, and its grid still saves
cfg=$(writable_config f50-empty)
CFG=$cfg run_keys f50-empty Menu Down Return Down Down Return Right BackSpace BackSpace BackSpace
ok=1
[ "$(cat "$out/f50-empty.code")" = 0 ] && sed -n '/^\[Empty\]/,$p' "$cfg" | grep -qx 'Rows=3' \
    && sanitizer_clean f50-empty && ok=0
result "settings: a menu with no entries can have its grid changed" $ok

# The gamepad's Start button opens settings when nothing else does, and keeps a mapping of its own
run_quick f50-pad
run_quick f50-pad-taken
ok=1
grep -A12 'Gamepad ===' "$out/f50-pad.log" | grep -qE 'ButtonStart\s+:settings$' \
    && grep -A12 'Gamepad ===' "$out/f50-pad-taken.log" | grep -qE 'ButtonStart\s+:quit$' \
    && ! grep -A12 'Gamepad ===' "$out/f50-pad-taken.log" | grep -q ':settings' && ok=0
result "settings: Start opens them by default, unless the config maps Start itself" $ok
```

Create the fixtures. `f50-grid.ini`:

```ini
; A settings fixture: its comments, blank lines and order must survive a save
[General]
DefaultMenu=Main

[Layout]
Rows=1
Columns=4 ; four across
IconSize=256

[Main]
Entry1=Games;games;:submenu Games
Entry2=Quit;power;:quit

[Games]
Rows=3
Columns=6
Entry1=One;apps;:quit
```

`f50-hotkey.ini`:

```ini
[General]
DefaultMenu=Main

[Hotkeys]
Hotkey1=#1B;:quit

[Main]
Entry1=One;apps;:quit
```

`f50-empty.ini`:

```ini
[General]
DefaultMenu=Main

[Main]
Entry1=One;apps;:quit

[Empty]
Rows=2
```

`f50-pad.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Gamepad]
Enabled=true

[Main]
Entry1=One;apps;:quit
```

`f50-pad-taken.ini`: the same, with `ButtonStart=:quit` added under `Enabled=true`.

- [ ] **Step 2: Run the harness and watch them fail**

Expected: every `settings:` line fails, because the Menu key does nothing yet, and `Key Application` is logged but no settings line follows. Every earlier check still passes.

- [ ] **Step 3: Find the default font from anywhere.** In `src/image.c`, add before `load_font()`:

```c
// A function to find a bundled font: next to the executable, else where the packages install it
char *find_default_font(const char *font)
{
    const char *prefixes[2];
    char fonts_exe_buffer[MAX_PATH_CHARS + 1];
    prefixes[0] = join_paths(fonts_exe_buffer, sizeof(fonts_exe_buffer), 3, config.exe_path, PATH_ASSETS_EXE, PATH_FONTS_EXE);
#ifdef __unix__
    prefixes[1] = PATH_FONTS_SYSTEM;
#else
    prefixes[1] = PATH_FONTS_RELATIVE;
#endif
    return find_file(font, 2, prefixes);
}
```

In `load_font()`, replace the lines from `const char *prefixes[2];` through `char *default_font_path = find_file(default_font, 2, prefixes);` with:

```c
        char *default_font_path = find_default_font(default_font);
```

Add `char *find_default_font(const char *font);` to `src/image.h`.

- [ ] **Step 4: Create `src/settings_screen.h`:**

```c
// The settings screen (settings_screen.c): opened over the menu on show with :settings
#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H

#include <stdbool.h>

bool settings_is_open(void);
void settings_open(void);
void settings_handle_command(const char *command);
void settings_draw(void);
void settings_close_now(void);

#endif
```

- [ ] **Step 5: Create `src/settings_screen.c`:**

```c
// The settings screen: a narrow column of settings on the left and a live preview of the launcher
// on the right, driven by the remote alone. settings.c holds the pages and the values; this file
// draws them, puts each change into the running launcher, and saves on the way out.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "launcher.h"
#include <launcher_config.h>
#include "settings.h"
#include "settings_screen.h"
#include "config_save.h"
#include "image.h"
#include "util.h"
#include "debug.h"

extern Config config;
extern Geometry geo;
extern SDL_Renderer *renderer;
extern Menu *current_menu;
extern LayoutGeometry layout;

#define MARGIN_RATIO 0.03F         // Of the screen height
#define HEADER_FONT_RATIO 0.045F
#define ROW_FONT_RATIO 0.028F
#define SMALL_FONT_RATIO 0.02F
#define ROWS_TOP_RATIO 0.17F
#define ROW_HEIGHT_RATIO 1.6F      // Of the row font's line height
#define MIN_COLUMN_RATIO 0.20F     // Of the screen width
#define MAX_COLUMN_RATIO 0.32F
#define TEXT_CACHE_SIZE 96
#define ALPHA_VALUE 180            // A row's value
#define ALPHA_DIM 110              // Greyed rows, the page path and the key hint
#define ALPHA_FILL 40              // The highlighted row
#define ALPHA_OUTLINE 220
#define ALPHA_DIVIDER 46
#define ALPHA_FRAME 77             // The preview's outline
#define LEFT_ARROW "\xE2\x80\xB9"  // "‹"
#define RIGHT_ARROW "\xE2\x80\xBA" // "›"

static const SDL_Color BACKDROP = { 0x0B, 0x16, 0x20, 0xFF };
static const SDL_Color WHITE = { 0xFF, 0xFF, 0xFF, 0xFF };

// A line of text already rendered, kept while it is still being drawn
typedef struct {
    TTF_Font *font;
    char *text;
    SDL_Texture *texture;
    int w;
    int h;
    Uint32 used;
} CachedText;

static SettingsState *model = NULL;   // NULL while settings are closed
static Menu **menus = NULL;           // The launcher's menus, in the model's order
static int menu_count = 0;
static Menu *origin = NULL;           // The menu settings opened over
static bool go_home = false;          // Go to the default menu after closing (:home)
static SDL_Texture *preview = NULL;   // The scene at full size; NULL when the renderer has no targets
static TTF_Font *font_header = NULL;
static TTF_Font *font_row = NULL;
static TTF_Font *font_small = NULL;
static CachedText text_cache[TEXT_CACHE_SIZE];
static Uint32 text_clock = 0;
static int margin = 0;
static int column_width = 0;
static int row_height = 0;
static int first_row = 0;             // The first row shown when a list is longer than the column
static SDL_Rect preview_rect;

// A function to return the smaller of two ints
static int min_int(int a, int b)
{
    return a < b ? a : b;
}

// A function to return the larger of two ints
static int max_int(int a, int b)
{
    return a > b ? a : b;
}

// A function to tell whether settings are open
bool settings_is_open(void)
{
    return model != NULL;
}

// A function to measure a line of text
static int text_width(TTF_Font *font, const char *text)
{
    int w = 0;
    int h = 0;
    if (text[0] != '\0')
        TTF_SizeUTF8(font, text, &w, &h);
    return w;
}

// A function to get a line of text as a texture, from the cache when it was drawn recently
static CachedText *cached_text(TTF_Font *font, const char *text)
{
    text_clock++;
    int oldest = 0;
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        CachedText *entry = &text_cache[i];
        if (entry->texture != NULL && entry->font == font && strcmp(entry->text, text) == 0) {
            entry->used = text_clock;
            return entry;
        }
        if (entry->used < text_cache[oldest].used)
            oldest = i;
    }
    CachedText *entry = &text_cache[oldest];
    if (entry->texture != NULL) {
        SDL_DestroyTexture(entry->texture);
        free(entry->text);
        memset(entry, 0, sizeof(*entry));
    }
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, WHITE);
    if (surface == NULL)
        return NULL;
    entry->texture = SDL_CreateTextureFromSurface(renderer, surface);
    entry->w = surface->w;
    entry->h = surface->h;
    SDL_FreeSurface(surface);
    if (entry->texture == NULL)
        return NULL;
    entry->font = font;
    entry->text = strdup(text);
    entry->used = text_clock;
    return entry;
}

// A function to empty the text cache
static void clear_text_cache(void)
{
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (text_cache[i].texture != NULL)
            SDL_DestroyTexture(text_cache[i].texture);
        free(text_cache[i].text);
    }
    memset(text_cache, 0, sizeof(text_cache));
}

// A function to draw a line of text cut with "..." to fit; `right` puts its right edge at x
static void draw_text(TTF_Font *font, const char *text, int x, int y, int max_width, Uint8 alpha, bool right)
{
    if (text == NULL || text[0] == '\0' || max_width <= 0)
        return;
    char buffer[SETTING_TEXT_MAX + 16];
    copy_string(buffer, text, sizeof(buffer));
    int w = text_width(font, buffer);
    if (w > max_width)
        utf8_truncate(buffer, w, max_width);
    CachedText *line = cached_text(font, buffer);
    if (line == NULL)
        return;
    SDL_SetTextureAlphaMod(line->texture, alpha);
    SDL_Rect rect = { right ? x - line->w : x, y, line->w, line->h };
    SDL_RenderCopy(renderer, line->texture, NULL, &rect);
}

// A function to draw a paragraph wrapped to a width; returns its height
static int draw_wrapped(TTF_Font *font, const char *text, int x, int y, int width, Uint8 alpha)
{
    if (text == NULL || text[0] == '\0' || width <= 0)
        return 0;
    SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(font, text, WHITE, (Uint32) width);
    if (surface == NULL)
        return 0;
    int h = surface->h;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = { x, y, surface->w, surface->h };
    SDL_FreeSurface(surface);
    if (texture != NULL) {
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        SDL_DestroyTexture(texture);
    }
    return h;
}

// A function to name the section a setting lives in: its own, or its menu's
static const char *section_of(const SettingSlot *slot)
{
    return slot->def->section != NULL ? slot->def->section : menus[slot->menu]->name;
}

// A function to read a setting's value from the running launcher
static SettingValue read_value(SettingId id, int menu_index)
{
    SettingValue value;
    memset(&value, 0, sizeof(value));
    Menu *menu = menu_index >= 0 ? menus[menu_index] : NULL;
    switch (id) {
        case SET_ID_BACKGROUND_MODE:
            value.number = (int) config.background_mode;
            break;
        case SET_ID_BACKGROUND_COLOR:
            value.color.r = config.background_color.r;
            value.color.g = config.background_color.g;
            value.color.b = config.background_color.b;
            break;
        case SET_ID_BACKGROUND_IMAGE:
            copy_string(value.text, config.background_image != NULL ? config.background_image : "", sizeof(value.text));
            break;
        case SET_ID_SLIDESHOW_DIRECTORY:
            copy_string(value.text, config.slideshow_directory != NULL ? config.slideshow_directory : "", sizeof(value.text));
            break;
        case SET_ID_SLIDESHOW_DURATION:
            value.number = (int) (config.slideshow_image_duration / 1000);
            break;
        case SET_ID_SLIDESHOW_FADE:
            value.number = (int) config.slideshow_transition_time;
            break;
        case SET_ID_LAYOUT_ROWS:
            value.number = (int) config.rows;
            break;
        case SET_ID_LAYOUT_COLUMNS:
            value.number = (int) config.max_buttons;
            break;
        case SET_ID_LAYOUT_ICON_SIZE:
            value.inherit = config.icon_size == 0;
            value.number = config.icon_size;
            break;
        case SET_ID_TITLE_SIZE:
            value.percent = config.title_font_size_pct > 0;
            value.number = value.percent ? config.title_font_size_pct : (int) config.title_font_size;
            break;
        case SET_ID_MENU_ROWS:
            value.inherit = menu->overrides.rows == 0;
            value.number = menu->overrides.rows;
            break;
        case SET_ID_MENU_COLUMNS:
            value.inherit = menu->overrides.columns == 0;
            value.number = menu->overrides.columns;
            break;
        case SET_ID_MENU_ICON_SIZE:
            value.inherit = menu->overrides.icon_cap == 0;
            value.number = menu->overrides.icon_cap;
            break;
        case SET_ID_COUNT:
            break;
    }
    return value;
}

// A function to replace a config path with a copy of a new one; "" leaves it unset
static void replace_path(char **path, const char *text)
{
    free(*path);
    *path = text[0] != '\0' ? strdup(text) : NULL;
}

// A function to put a setting's value into the running launcher, then refresh what it affects
static void apply_slot(const SettingSlot *slot, bool refresh)
{
    const SettingValue *value = &slot->value;
    Menu *menu = slot->menu >= 0 ? menus[slot->menu] : NULL;
    switch (slot->def->id) {
        case SET_ID_BACKGROUND_MODE:
            config.background_mode = (ModeBackground) value->number;
            break;
        case SET_ID_BACKGROUND_COLOR:
            config.background_color.r = value->color.r;
            config.background_color.g = value->color.g;
            config.background_color.b = value->color.b;
            break;
        case SET_ID_BACKGROUND_IMAGE:
            replace_path(&config.background_image, value->text);
            break;
        case SET_ID_SLIDESHOW_DIRECTORY:
            replace_path(&config.slideshow_directory, value->text);
            break;
        case SET_ID_SLIDESHOW_DURATION:
            config.slideshow_image_duration = (Uint32) value->number * 1000;
            break;
        case SET_ID_SLIDESHOW_FADE:
            config.slideshow_transition_time = (Uint32) value->number;
            update_slideshow_timing();
            break;
        case SET_ID_LAYOUT_ROWS:
            config.rows = (unsigned int) value->number;
            break;
        case SET_ID_LAYOUT_COLUMNS:
            config.max_buttons = (unsigned int) value->number;
            break;
        case SET_ID_LAYOUT_ICON_SIZE:
            config.icon_size = value->inherit ? 0 : (Uint16) value->number;
            break;
        case SET_ID_TITLE_SIZE:
            if (value->percent)
                config.title_font_size_pct = value->number;
            else {
                config.title_font_size_pct = 0;
                config.title_font_size = (unsigned int) value->number;
            }
            break;
        case SET_ID_MENU_ROWS:
            menu->overrides.rows = value->inherit ? 0 : value->number;
            break;
        case SET_ID_MENU_COLUMNS:
            menu->overrides.columns = value->inherit ? 0 : value->number;
            break;
        case SET_ID_MENU_ICON_SIZE:
            menu->overrides.icon_cap = value->inherit ? 0 : value->number;
            break;
        case SET_ID_COUNT:
            break;
    }
    if (!refresh)
        return;
    switch (slot->def->refresh) {
        case SET_REFRESH_LAYOUT:
            refresh_layout();
            break;
        case SET_REFRESH_TITLES:
            reload_titles();
            break;
        case SET_REFRESH_BACKGROUND:
            reload_background();
            break;
        case SET_REFRESH_NONE:
            break;
    }
}

// A function to put every value into the launcher (after Discard), refreshing everything once
static void apply_all(void)
{
    for (int i = 0; i < settings_slot_count(model); i++)
        apply_slot(settings_slot_at(model, i), false);
    reload_background();
    reload_titles();
}

// A function to log a change for -d: "[Section] Key old -> new"
static void log_change(const SettingSlot *slot, const SettingValue *before)
{
    char old_text[SETTING_TEXT_MAX];
    char new_text[SETTING_TEXT_MAX];
    setting_format(slot->def, before, old_text, sizeof(old_text));
    setting_format(slot->def, &slot->value, new_text, sizeof(new_text));
    log_debug("Settings: [%s] %s %s -> %s", section_of(slot), slot->def->key,
        old_text[0] != '\0' ? old_text : "(none)", new_text[0] != '\0' ? new_text : "(none)");
}

// A function to open the screen's own fonts: the bundled default, sized from the screen height
static bool open_fonts(void)
{
    char *path = find_default_font(FILENAME_DEFAULT_FONT);
    if (path == NULL) {
        log_error("Settings: could not find the font %s", FILENAME_DEFAULT_FONT);
        return false;
    }
    float height = (float) geo.screen_height;
    font_header = TTF_OpenFont(path, max_int(8, (int) (HEADER_FONT_RATIO * height)));
    font_row = TTF_OpenFont(path, max_int(8, (int) (ROW_FONT_RATIO * height)));
    font_small = TTF_OpenFont(path, max_int(8, (int) (SMALL_FONT_RATIO * height)));
    free(path);
    if (font_header == NULL || font_row == NULL || font_small == NULL) {
        log_error("Settings: could not open the font %s\n%s", FILENAME_DEFAULT_FONT, TTF_GetError());
        return false;
    }
    return true;
}

// A function to size the column and the preview for this screen. The column is as wide as its
// widest row needs, within limits, and keeps that width while settings are open, so the preview
// never jumps. The preview keeps the screen's shape.
static void measure_layout(void)
{
    static const char *const labels[] = {
        "Background", "Menus", "Titles", "Discard changes", "Mode", "Colour", "Image", "Folder",
        "Change every", "Fade", "Rows", "Columns", "Largest button", "Size", "All menus", "Try again",
        "Leave without saving", "Use this folder"
    };
    static const char *const values[] = {
        LEFT_ARROW " Transparent " RIGHT_ARROW, LEFT_ARROW " Custom #000000 " RIGHT_ARROW,
        LEFT_ARROW " All menus (1024 px) " RIGHT_ARROW, LEFT_ARROW " Fixed 512 " RIGHT_ARROW,
        "12 \xC3\x97 10 " RIGHT_ARROW
    };
    int w = geo.screen_width;
    int h = geo.screen_height;
    margin = (int) (MARGIN_RATIO * (float) h);
    row_height = (int) (ROW_HEIGHT_RATIO * (float) TTF_FontHeight(font_row));
    int widest_label = 0;
    int widest_value = 0;
    for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++)
        widest_label = max_int(widest_label, text_width(font_row, labels[i]));
    for (int i = 0; i < menu_count; i++)
        widest_label = max_int(widest_label, text_width(font_row, menus[i]->name));
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        widest_value = max_int(widest_value, text_width(font_row, values[i]));
    column_width = widest_label + widest_value + 3 * margin;
    column_width = max_int(column_width, (int) (MIN_COLUMN_RATIO * (float) w));
    column_width = min_int(column_width, (int) (MAX_COLUMN_RATIO * (float) w));

    int x = 2 * margin + column_width;
    int available_w = w - x - margin;
    int caption = 2 * TTF_FontHeight(font_small);
    int available_h = h - 2 * margin - caption;
    int preview_w = available_w;
    int preview_h = preview_w * h / w;
    if (preview_h > available_h) {
        preview_h = available_h;
        preview_w = preview_h * w / h;
    }
    preview_rect.x = x + (available_w - preview_w) / 2;
    preview_rect.y = (h - preview_h - caption) / 2;
    preview_rect.w = preview_w;
    preview_rect.h = preview_h;
}

// A function to show in the preview the menu the page is about: a menu being edited or
// highlighted, else the one settings opened over. A menu with no entries cannot be shown.
static void follow_preview(void)
{
    int index = settings_preview_menu(model);
    Menu *want = index >= 0 && index < menu_count ? menus[index] : origin;
    if (want->num_entries == 0)
        want = origin;
    if (want != current_menu)
        show_menu(want);
}

// A function to free what the screen holds while open
static void free_screen(void)
{
    if (preview != NULL)
        SDL_DestroyTexture(preview);
    preview = NULL;
    clear_text_cache();
    if (font_header != NULL)
        TTF_CloseFont(font_header);
    if (font_row != NULL)
        TTF_CloseFont(font_row);
    if (font_small != NULL)
        TTF_CloseFont(font_small);
    font_header = NULL;
    font_row = NULL;
    font_small = NULL;
    settings_free(model);
    model = NULL;
    free(menus);
    menus = NULL;
    menu_count = 0;
}

// A function to close settings, back to the menu they opened over (or the default menu for :home)
static void close_settings(void)
{
    if (origin != NULL && current_menu != origin)
        show_menu(origin);
    free_screen();
    log_debug("Settings closed");
    if (go_home)
        show_home();
}

// A function to save every changed setting into config.ini; on failure, show why
static bool save_changes(void)
{
    int count = settings_slot_count(model);
    ConfigEdit *edits = calloc((size_t) count, sizeof(ConfigEdit));
    char (*values)[SETTING_TEXT_MAX] = calloc((size_t) count, SETTING_TEXT_MAX);
    if (edits == NULL || values == NULL) {
        free(edits);
        free(values);
        settings_show_save_failed(model, "Couldn't save: out of memory");
        return false;
    }
    int n = 0;
    for (int i = 0; i < count; i++) {
        SettingSlot *slot = settings_slot_at(model, i);
        if (!settings_changed(slot))
            continue;
        setting_format(slot->def, &slot->value, values[n], SETTING_TEXT_MAX);
        edits[n].section = section_of(slot);
        edits[n].key = slot->def->key;
        edits[n].alias = slot->def->alias;
        edits[n].value = slot->value.inherit ? NULL : values[n];
        edits[n].placement = slot->def->section != NULL ? INIDOC_AFTER_LAST_KEY : INIDOC_UNDER_HEADER;
        n++;
    }
    ConfigSaveResult result;
    bool ok;
#ifdef __unix__
    // The packaged config cannot be written; the user's own goes where the launcher looks first
    char user_config[MAX_PATH_CHARS + 1];
    const char *home = getenv("HOME");
    if (home != NULL)
        join_paths(user_config, sizeof(user_config), 4, home, ".config", EXECUTABLE_TITLE, FILENAME_DEFAULT_CONFIG);
    ok = config_save(config.config_path, PATH_CONFIG_SYSTEM, home != NULL ? user_config : NULL, edits, n, &result);
#else
    ok = config_save(config.config_path, NULL, NULL, edits, n, &result);
#endif
    free(edits);
    free(values);
    if (ok) {
        log_debug("Settings saved %i change(s) to %s (backup: %s)", n, result.path,
            result.backup[0] != '\0' ? result.backup : "none");
        if (strcmp(result.path, config.config_path) != 0) {
            free(config.config_path);
            config.config_path = strdup(result.path);
        }
        return true;
    }
    char message[CONFIG_SAVE_PATH_MAX + 600];
    snprintf(message, sizeof(message), "Couldn't save to %s: %s", result.path, result.why);
    log_error("%s", message);
    settings_show_save_failed(model, message);
    return false;
}

// A function to save and close; nothing is written when nothing changed
static void save_and_close(void)
{
    if (!settings_any_changed(model)) {
        log_debug("Settings: nothing changed");
        close_settings();
    }
    else if (save_changes())
        close_settings();
}

// A function to act on what a key did in the model
static void handle_event(const SettingsEvent *event)
{
    switch (event->kind) {
        case SETTINGS_EVENT_CHANGED:
            log_change(event->slot, &event->before);
            apply_slot(event->slot, true);
            break;
        case SETTINGS_EVENT_DISCARD:
            log_debug("Settings: discarded the changes");
            apply_all();
            break;
        case SETTINGS_EVENT_BROWSE:
            // The folder browser is not built yet: the Image and Folder rows do nothing on OK
            break;
        case SETTINGS_EVENT_CLOSE:
        case SETTINGS_EVENT_CLOSE_HOME:
            if (event->slot != NULL) {
                log_change(event->slot, &event->before);
                apply_slot(event->slot, true);
            }
            go_home = event->kind == SETTINGS_EVENT_CLOSE_HOME;
            save_and_close();
            return;
        case SETTINGS_EVENT_RETRY:
            save_and_close();
            return;
        case SETTINGS_EVENT_LEAVE:
            log_debug("Settings: leaving without saving");
            close_settings();
            return;
        case SETTINGS_EVENT_MOVED:
        case SETTINGS_EVENT_NONE:
            break;
    }
    follow_preview();
}

// A function to turn a special command into one of the screen's keys
static bool to_settings_command(const char *command, SettingsCommand *out)
{
    static const struct {
        const char *name;
        SettingsCommand command;
    } keys[] = {
        { SCMD_UP, SETTINGS_UP }, { SCMD_DOWN, SETTINGS_DOWN }, { SCMD_LEFT, SETTINGS_LEFT },
        { SCMD_RIGHT, SETTINGS_RIGHT }, { SCMD_SELECT, SETTINGS_OK }, { SCMD_BACK, SETTINGS_BACK },
        { SCMD_HOME, SETTINGS_HOME }, { SCMD_SETTINGS, SETTINGS_CLOSE }
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (MATCH(command, keys[i].name)) {
            *out = keys[i].command;
            return true;
        }
    }
    return false;
}

// A function to act on a special command while settings are open: the remote's keys move through
// them, and every other command waits until they close
void settings_handle_command(const char *command)
{
    SettingsCommand key;
    if (model == NULL)
        return;
    if (!to_settings_command(command, &key)) {
        log_debug("Settings: ignoring '%s' while settings are open", command);
        return;
    }
    SettingsEvent event = settings_command(model, key);
    handle_event(&event);
}

// A function to draw one row; returns the height it took
static int draw_row(const SettingsRow *row, bool highlighted, int x, int y, int width)
{
    int pad = margin / 2;
    int text_y = y + (row_height - TTF_FontHeight(font_row)) / 2;
    if (row->kind == SETTINGS_ROW_DIVIDER) {
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_DIVIDER);
        SDL_RenderDrawLine(renderer, x, y + row_height / 2, x + width, y + row_height / 2);
        return row_height;
    }
    if (row->kind == SETTINGS_ROW_NOTE) {
        int h = draw_wrapped(font_small, row->note, x + pad, y, width - 2 * pad, ALPHA_VALUE);
        return max_int(row_height, h + row_height / 2);
    }
    if (highlighted) {
        SDL_Rect box = { x, y, width, row_height - 2 };
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_FILL);
        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_OUTLINE);
        SDL_RenderDrawRect(renderer, &box);
    }
    char value[320];
    if (row->kind == SETTINGS_ROW_SETTING && highlighted)
        snprintf(value, sizeof(value), LEFT_ARROW " %s " RIGHT_ARROW, row->value);
    else if (row->kind == SETTINGS_ROW_LINK || row->kind == SETTINGS_ROW_BROWSE)
        snprintf(value, sizeof(value), "%s " RIGHT_ARROW, row->value);
    else
        snprintf(value, sizeof(value), "%s", row->value);
    int value_width = min_int(text_width(font_row, value), width / 2);
    draw_text(font_row, row->label, x + pad, text_y, width - value_width - 3 * pad, row->enabled ? 255 : ALPHA_DIM, false);
    draw_text(font_row, value, x + width - pad, text_y, width / 2, highlighted ? 255 : ALPHA_VALUE, true);
    return row_height;
}

// A function to draw the model's rows between two heights, scrolled to keep the cursor in view
static void draw_model_rows(int x, int top, int bottom)
{
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(model, rows, SETTINGS_MAX_ROWS);
    int cursor = settings_cursor(model);
    int visible = max_int(1, (bottom - top) / row_height);
    if (cursor < first_row)
        first_row = cursor;
    if (cursor >= first_row + visible)
        first_row = cursor - visible + 1;
    first_row = max_int(0, min_int(first_row, count - visible));
    int y = top;
    for (int i = first_row; i < count && y + row_height <= bottom; i++)
        y += draw_row(&rows[i], i == cursor, x, y, column_width);
}

// A function to draw the column: the title, the page path, the rows and the key hint
static void draw_column(void)
{
    char path[512];
    int x = margin;
    draw_text(font_header, "Settings", x, margin, column_width, 255, false);
    settings_path(model, path, sizeof(path));
    draw_text(font_small, path, x, margin + TTF_FontHeight(font_header), column_width, ALPHA_DIM, false);
    int top = (int) (ROWS_TOP_RATIO * (float) geo.screen_height);
    int hint_y = geo.screen_height - margin - TTF_FontHeight(font_small);
    draw_model_rows(x, top, hint_y - margin);
    const char *hint = settings_page(model) == SETTINGS_PAGE_TOP
                       ? "Left and right change \xC2\xB7 OK opens \xC2\xB7 Back saves and closes"
                       : "Left and right change \xC2\xB7 OK opens \xC2\xB7 Back goes back";
    draw_text(font_small, hint, x, hint_y, column_width, ALPHA_DIM, false);
}

// A function to draw the caption under the preview: which menu, its grid and titles, and any note
static void draw_caption(void)
{
    char caption[512];
    char titles[32];
    char why[256];
    LayoutGeometry geometry;
    describe_titles(&layout, titles, sizeof(titles));
    bool reduced = compute_menu_layout(current_menu, &geometry, why, sizeof(why)) == 0 && why[0] != '\0';
    snprintf(caption, sizeof(caption), "Preview: %s \xC2\xB7 %i \xC3\x97 %i, %i px buttons, %s%s", current_menu->name,
        layout.columns, layout.rows, layout.button, titles, reduced ? " (reduced to fit the screen)" : "");
    int y = preview_rect.y + preview_rect.h + margin / 2;
    draw_text(font_small, caption, preview_rect.x, y, preview_rect.w, ALPHA_VALUE, false);
    draw_text(font_small, settings_notice(model), preview_rect.x, y + TTF_FontHeight(font_small), preview_rect.w, 255, false);
}

// A function to draw one frame of the screen and present it
void settings_draw(void)
{
    if (model == NULL)
        return;
    if (preview != NULL) {
        SDL_SetRenderTarget(renderer, preview);
        draw_scene();
        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, BACKDROP.r, BACKDROP.g, BACKDROP.b, BACKDROP.a);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, preview, NULL, &preview_rect);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_FRAME);
        SDL_RenderDrawRect(renderer, &preview_rect);
        draw_caption();
    }
    else {
        // No render targets: the scene fills the screen and the column sits on a dark backing
        draw_scene();
        SDL_Rect backing = { 0, 0, column_width + 2 * margin, geo.screen_height };
        SDL_SetRenderDrawColor(renderer, BACKDROP.r, BACKDROP.g, BACKDROP.b, 220);
        SDL_RenderFillRect(renderer, &backing);
    }
    draw_column();
    present_frame();
}

// A function to open settings over the menu on show
void settings_open(void)
{
    if (model != NULL || current_menu == NULL)
        return;
    menu_count = (int) config.num_menus;
    menus = calloc((size_t) (menu_count > 0 ? menu_count : 1), sizeof(Menu*));
    const char **names = calloc((size_t) (menu_count > 0 ? menu_count : 1), sizeof(char*));
    int found = 0;
    for (Menu *menu = config.first_menu; menus != NULL && names != NULL && menu != NULL && found < menu_count; menu = menu->next) {
        menus[found] = menu;
        names[found] = menu->name;
        found++;
    }
    menu_count = found;
    model = menus != NULL && names != NULL ? settings_create(names, menu_count) : NULL;
    free(names);
    if (model == NULL) {
        log_error("Settings: out of memory");
        free_screen();
        return;
    }
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        SettingValue value = read_value((SettingId) id, -1);
        settings_set_entry(model, (SettingId) id, -1, &value);
    }
    for (int m = 0; m < menu_count; m++) {
        for (int id = SET_ID_MENU_ROWS; id < SET_ID_COUNT; id++) {
            SettingValue value = read_value((SettingId) id, m);
            settings_set_entry(model, (SettingId) id, m, &value);
        }
    }
    if (!open_fonts()) {
        free_screen();
        return;
    }
    origin = current_menu;
    go_home = false;
    first_row = 0;
    measure_layout();
    if (SDL_RenderTargetSupported(renderer)) {
        preview = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, geo.screen_width, geo.screen_height);
        if (preview == NULL)
            log_error("Settings: no preview texture, the menu is drawn behind the settings instead\n%s", SDL_GetError());
        else
            SDL_SetTextureBlendMode(preview, SDL_BLENDMODE_NONE);
    }
    log_debug("Settings opened over menu '%s'", origin->name);
}

// A function to let go of settings at quit, saving nothing
void settings_close_now(void)
{
    if (model != NULL)
        free_screen();
}
```

In `src/CMakeLists.txt`, add `settings_screen.c` and `settings_screen.h` to `SOURCES` after `browser.h`.

- [ ] **Step 6: Route input to the screen.** In `src/launcher.c`, add `#include "settings_screen.h"` after `#include "clock.h"`.

At the top of `execute_command()`, before `char *cmd = strdup(command);`, add:

```c
    // While settings are open the remote's keys belong to them, and everything else waits
    if (settings_is_open()) {
        settings_handle_command(command);
        return;
    }
```

and in its special command chain, after the `SCMD_SLEEP` branch, add:

```c
        else if (!strcmp(special_command, SCMD_SETTINGS))
            settings_open();
```

In `handle_keypress()`, after the debug log line, add:

```c
    // While settings are open, the built-in keys are their commands; other keys run their hotkey,
    // which execute_command() hands to settings (and settings ignore unless it is one of theirs)
    if (settings_is_open()) {
        const char *command = NULL;
        switch (key->sym) {
            case SDLK_LEFT:
                command = SCMD_LEFT;
                break;
            case SDLK_RIGHT:
                command = SCMD_RIGHT;
                break;
            case SDLK_UP:
                command = SCMD_UP;
                break;
            case SDLK_DOWN:
                command = SCMD_DOWN;
                break;
            case SDLK_RETURN:
                command = SCMD_SELECT;
                break;
            case SDLK_BACKSPACE:
                command = SCMD_BACK;
                break;
            default:
                if (key->sym == SDLK_APPLICATION && !hotkey_bound(SDLK_APPLICATION))
                    command = SCMD_SETTINGS;
                break;
        }
        if (command != NULL) {
            settings_handle_command(command);
            return;
        }
    }
```

In the same function's key chain, before the final `//Check hotkeys` `else`, add:

```c
    // The Menu key (the context-menu key on a keyboard) opens settings, unless a hotkey has it
    else if (key->sym == SDLK_APPLICATION && !hotkey_bound(SDLK_APPLICATION))
        execute_command(SCMD_SETTINGS);
```

In the main loop:
- change `if (config.mouse_select && event.button.button == SDL_BUTTON_LEFT) {` to `if (config.mouse_select && !settings_is_open() && event.button.button == SDL_BUTTON_LEFT) {`;
- change `if (config.screensaver_enabled)` (in the post-event updates) to `if (config.screensaver_enabled && !settings_is_open())`;
- replace:

  ```c
          if (state.application_running)
              SDL_Delay(APPLICATION_WAIT_PERIOD);
          else
              draw_screen();
  ```

  with:

  ```c
          if (settings_is_open())
              settings_draw();
          else if (state.application_running)
              SDL_Delay(APPLICATION_WAIT_PERIOD);
          else
              draw_screen();
  ```

In `cleanup()`, add `settings_close_now();` as its first line, before the threads are waited on.

- [ ] **Step 7: Start opens settings by default.** In `src/util.c`, replace `add_default_gamepad_controls()` and its comment with:

```c
// A function to give Up, Down and :settings default gamepad controls. Configs written before
// grids existed map nothing to :up or :down, and a grid is unusable without them; and any config
// written before the settings screen needs a way to open it.
void add_default_gamepad_controls()
{
    static const char *const up[] = { SETTING_GAMEPAD_BUTTON_DPAD_UP, SETTING_GAMEPAD_LSTICK_YM };
    static const char *const down[] = { SETTING_GAMEPAD_BUTTON_DPAD_DOWN, SETTING_GAMEPAD_LSTICK_YP };
    static const char *const settings[] = { SETTING_GAMEPAD_BUTTON_START };
    add_default_controls(SCMD_UP, up, sizeof(up) / sizeof(up[0]));
    add_default_controls(SCMD_DOWN, down, sizeof(down) / sizeof(down[0]));
    add_default_controls(SCMD_SETTINGS, settings, sizeof(settings) / sizeof(settings[0]));
}
```

- [ ] **Step 8: Build and run everything**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: no new warnings, and `100% tests passed, 0 tests failed out of 9`. Then run the headless harness. Expected: every `settings:` line passes, every earlier check still passes, and the last line is `0 failed`.

If a key sequence misses its row (the check fails and the log shows the wrong `Settings: [...]` line), compare the key list with the rows each page lists in Task 6's tests: the top page is Background, Menus, Titles, a divider, then Discard; the Menus page is All menus, a divider, then each menu in file order.

- [ ] **Step 9: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/settings_screen.h src/settings_screen.c src/launcher.c src/util.c src/image.h src/image.c src/CMakeLists.txt tests/headless/checks/50-settings.sh tests/headless/fixtures/f50-grid.ini tests/headless/fixtures/f50-hotkey.ini tests/headless/fixtures/f50-empty.ini tests/headless/fixtures/f50-pad.ini tests/headless/fixtures/f50-pad-taken.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: the settings screen, with grids, titles, Discard and the save"
```

---

### Task 11: The Background page: the folder browser, the image preview and Transparent

This task finishes the Background page:
- **The browser.** OK on *Image* or *Folder* opens it in the column, driven by the same keys: Left and Right page, OK opens or chooses, Back goes up.
- **The preview** shows the highlighted image, or a highlighted folder's first image, as the background. It is decoded on a thread, so moving through a long folder never stalls.
- **Refused paths.** A path `config.ini` cannot hold, or an image that failed to decode, cannot be chosen, and the caption says why.
- **Transparent** shows a checkerboard in the preview.

**Files:**
- Modify: `src/settings_screen.c`, `src/launcher.h`, `src/launcher.c`
- Create: `tests/headless/checks/60-settings-background.sh`; fixtures `f60-color.ini`, `f60-running.ini`

**Interfaces:**
- Consumes: `browser_*` and `fileio_places()` (Task 8); `inidoc_check()` (Task 3); `settings_choose()` (Task 6); the Task 10 screen.
- Produces: `extern SDL_Texture *background_override;` (launcher.c, the browsed image; NULL otherwise); `draw_scene(bool preview)` replaces `draw_scene(void)`.

- [ ] **Step 1: Write the failing headless checks.** Create `tests/headless/checks/60-settings-background.sh`:

```bash
# The Background page: a preset colour, an image and a slideshow folder chosen in the folder
# browser, the incomplete-mode rule, and switching modes while a slideshow is running.
# writable_config and changed_lines come from 50-settings.sh, which runs first.

# Colour: step from Black to Charcoal
cfg=$(writable_config f60-color)
CFG=$cfg run_keys f60-color Menu Return Down Right BackSpace BackSpace
ok=1
[ "$(changed_lines "$FX/f60-color.ini" "$cfg")" = 2 ] && grep -qx 'Color=#1E1E1E' "$cfg" \
    && grep -q 'Settings: \[Background\] Color #000000 -> #1E1E1E' "$out/f60-color.log" && sanitizer_clean f60-color && ok=0
result "settings: a preset colour is saved" $ok

# Image: Mode to Image, open the browser (it starts in Pictures), take the second image
cfg=$(writable_config f60-color)
CFG=$cfg run_keys f60-image Menu Return Right Down Return Down Return BackSpace BackSpace
ok=1
grep -qx 'Mode=Image' "$cfg" && grep -qx 'Image=/home/tester/Pictures/green.png' "$cfg" \
    && grep -q 'Settings saved 2 change(s)' "$out/f60-image.log" && sanitizer_clean f60-image && ok=0
result "settings: an image chosen in the folder browser is saved" $ok
diff "$FX/f60-color.ini" "$cfg" | sed 's/^/      /'

# Image with none chosen: leaving the page puts Colour back, so nothing is saved
cfg=$(writable_config f60-color)
CFG=$cfg run_keys f60-incomplete Menu Return Right BackSpace BackSpace
ok=1
cmp -s "$FX/f60-color.ini" "$cfg" && grep -q 'Settings: \[Background\] Mode Image -> Color' "$out/f60-incomplete.log" \
    && grep -q 'Settings: nothing changed' "$out/f60-incomplete.log" && sanitizer_clean f60-incomplete && ok=0
result "settings: Image with no image chosen goes back to Colour and saves nothing" $ok

# Slideshow: Mode to Slideshow, open the browser on the Folder row, use Pictures
cfg=$(writable_config f60-color)
CFG=$cfg run_keys f60-slideshow Menu Return Right Right Down Return Return BackSpace BackSpace
ok=1
grep -qx 'Mode=Slideshow' "$cfg" && grep -qx 'SlideshowDirectory=/home/tester/Pictures' "$cfg" \
    && grep -q 'Found 3 images in directory /home/tester/Pictures' "$out/f60-slideshow.log" \
    && sanitizer_clean f60-slideshow && ok=0
result "settings: a slideshow folder chosen in the folder browser is saved" $ok

# Stepping the mode through a running slideshow: its first change is due at 5 s and fades for 3 s,
# so these steps land while an image is loading on its thread and while it fades in
run_keys f60-running Menu Return Right Left Right Left BackSpace BackSpace
ok=1
[ "$(cat "$out/f60-running.code")" = 0 ] && grep -q 'Settings: nothing changed' "$out/f60-running.log" \
    && sanitizer_clean f60-running && ok=0
result "settings: switching modes while a slideshow runs frees it cleanly" $ok
grep -m3 -E 'AddressSanitizer|runtime error' "$out/f60-running.err" | sed 's/^/      /'
```

`tests/headless/fixtures/f60-color.ini`:

```ini
[General]
DefaultMenu=Main

[Background]
Mode=Color
Color=#000000

[Main]
Entry1=One;apps;:quit
```

`tests/headless/fixtures/f60-running.ini`:

```ini
[General]
DefaultMenu=Main

[Background]
Mode=Slideshow
SlideshowDirectory=/home/tester/Pictures
SlideshowImageDuration=5
SlideshowTransitionTime=3

[Main]
Entry1=One;apps;:quit
```

- [ ] **Step 2: Run the harness and watch them fail**

Expected:
- **Fail:** the colour check can already pass (the Colour row works since Task 10), but the image, slideshow and incomplete checks fail. OK on the Image or Folder row does nothing, so no image is chosen.
- **Incomplete-mode check:** it fails on its `Mode Image -> Color` line only if the rule misfires; read its log before assuming.
- **Running-slideshow check:** it should already pass. It pins Task 9's `reload_background()` against a live slideshow thread.

- [ ] **Step 3: The browsed image and the checkerboard in the scene.** In `src/launcher.h`, change `void draw_scene(void);` to `void draw_scene(bool preview);` and add `extern SDL_Texture *background_override;`. In `src/launcher.c`, after `SDL_Texture *background_overlay = NULL;`, add:

```c
SDL_Texture *background_override      = NULL; // The image being browsed in settings, shown in their preview
```

Before `draw_scene()`, add:

```c
// A function to fill the screen with a grey checkerboard. In the settings preview it stands for a
// transparent background: a texture cannot show the desktop through.
static void draw_checkerboard()
{
    int square = geo.screen_height / 18 > 8 ? geo.screen_height / 18 : 8;
    for (int y = 0; y < geo.screen_height; y += square) {
        for (int x = 0; x < geo.screen_width; x += square) {
            Uint8 shade = ((x / square) + (y / square)) % 2 == 0 ? 0x55 : 0x88;
            SDL_Rect cell = { x, y, square, square };
            SDL_SetRenderDrawColor(renderer, shade, shade, shade, 0xFF);
            SDL_RenderFillRect(renderer, &cell);
        }
    }
}
```

Change `draw_scene()`'s signature to `void draw_scene(bool preview)`, its comment's last sentence to `The settings screen draws it into its preview (preview true), where a transparent background shows as a checkerboard and an image being browsed replaces the background.`, and its background lines to:

```c
    set_draw_color();
    SDL_RenderClear(renderer);
    if (preview && background_override != NULL)
        SDL_RenderCopy(renderer, background_override, NULL, NULL);
    else if (preview && background_shown == BACKGROUND_TRANSPARENT)
        draw_checkerboard();
    else {
        if (background_shown == BACKGROUND_IMAGE || background_shown == BACKGROUND_SLIDESHOW)
            SDL_RenderCopy(renderer, background_texture, NULL, NULL);
        if (background_shown == BACKGROUND_SLIDESHOW && state.slideshow_transition)
            SDL_RenderCopy(renderer, slideshow->transition_texture, NULL, NULL);
    }
```

In `draw_screen()`, call `draw_scene(false)`. In `cleanup()`, after `settings_close_now();`, add:

```c
    if (background_override != NULL)
        SDL_DestroyTexture(background_override);
```

- [ ] **Step 4: The browser in the screen.** In `src/settings_screen.c`:

Add after `#include <SDL_ttf.h>`: `#include <SDL_image.h>`; and after `#include "config_save.h"`: `#include "browser.h"`, `#include "fileio.h"`, `#include "inidoc.h"`. Add `extern SDL_Texture *background_override;` to the externs.

After the other statics, add:

```c
static Browser *browser = NULL;          // The folder browser, while it is open
static SettingSlot *browser_slot = NULL; // The Image or Folder setting it chooses for
static int browser_first = 0;            // Its first row on show
static int browser_page = 1;             // How many of its rows fit: Left and Right move this far
static char browser_note[128] = "";      // Why the last OK did nothing, for the caption

// The preview's image, decoded on its own thread so moving through a folder never stalls
static SDL_Thread *decode_thread = NULL;
static SDL_atomic_t decode_done;
static char decode_path[BROWSER_PATH_MAX];  // What the thread is decoding
static SDL_Surface *decode_surface = NULL;  // Its result; NULL when it failed
static char wanted_path[BROWSER_PATH_MAX];  // What the preview should show; "" = the real background
static char shown_path[BROWSER_PATH_MAX];   // What background_override holds
static char broken_path[BROWSER_PATH_MAX];  // The last image that could not be decoded
```

Add these functions before `free_screen()`:

```c
// A function run on its own thread: decode one image for the preview
static int decode_image(void *data)
{
    UNUSED(data);
    decode_surface = IMG_Load(decode_path);
    SDL_AtomicSet(&decode_done, 1);
    return 0;
}

// A function to start decoding the wanted image, unless another is being decoded already
static void start_decode(void)
{
    if (decode_thread != NULL || wanted_path[0] == '\0' || strcmp(wanted_path, shown_path) == 0)
        return;
    copy_string(decode_path, wanted_path, sizeof(decode_path));
    SDL_AtomicSet(&decode_done, 0);
    decode_thread = SDL_CreateThread(decode_image, "Preview image", NULL);
}

// A function to ask for an image as the preview's background; "" goes back to the real one
static void want_preview_image(const char *path)
{
    copy_string(wanted_path, path, sizeof(wanted_path));
    if (wanted_path[0] == '\0' && background_override != NULL) {
        SDL_DestroyTexture(background_override);
        background_override = NULL;
        shown_path[0] = '\0';
    }
    start_decode();
}

// A function to pick up a finished decode: show it if it is still wanted, then start the next
static void poll_decode(void)
{
    if (decode_thread == NULL || !SDL_AtomicGet(&decode_done))
        return;
    SDL_WaitThread(decode_thread, NULL);
    decode_thread = NULL;
    if (decode_surface == NULL) {
        copy_string(broken_path, decode_path, sizeof(broken_path));
        log_debug("Settings: could not open %s", decode_path);
    }
    else if (strcmp(decode_path, wanted_path) == 0) {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, decode_surface);
        if (texture != NULL) {
            if (background_override != NULL)
                SDL_DestroyTexture(background_override);
            background_override = texture;
            copy_string(shown_path, decode_path, sizeof(shown_path));
        }
    }
    if (decode_surface != NULL)
        SDL_FreeSurface(decode_surface);
    decode_surface = NULL;
    start_decode();
}

// A function to wait out a decode in flight and drop the preview's image
static void stop_decoding(void)
{
    if (decode_thread != NULL) {
        SDL_WaitThread(decode_thread, NULL);
        decode_thread = NULL;
    }
    if (decode_surface != NULL)
        SDL_FreeSurface(decode_surface);
    decode_surface = NULL;
    wanted_path[0] = '\0';
    shown_path[0] = '\0';
    if (background_override != NULL)
        SDL_DestroyTexture(background_override);
    background_override = NULL;
}

// A function to list a folder for the browser
static int list_folder(const char *folder, FileioEntry **entries, void *context)
{
    UNUSED(context);
    return fileio_list(folder, entries);
}

// A function to tell the browser whether config.ini can hold a path for its setting
static const char *check_path(const char *path, void *context)
{
    const SettingSlot *slot = context;
    return inidoc_check(slot->def->key, path);
}

// A function to preview what the browser's cursor is on: an image, or a folder's first image
static void preview_highlighted(void)
{
    const BrowserRow *row = browser_row(browser, browser_cursor(browser));
    char path[BROWSER_PATH_MAX] = "";
    if (row != NULL && row->kind == BROWSER_ROW_IMAGE)
        copy_string(path, row->path, sizeof(path));
    else if (row != NULL && (row->kind == BROWSER_ROW_FOLDER || row->kind == BROWSER_ROW_USE_FOLDER))
        browser_first_image(browser, row->path, path, sizeof(path));
    want_preview_image(path);
}

// A function to open the folder browser for an Image or Folder setting, at its current path
static void open_browser(SettingSlot *slot)
{
    FileioPlace *places = NULL;
    int count = fileio_places(&places);
    BrowserPlace *list = calloc((size_t) (count > 0 ? count : 1), sizeof(BrowserPlace));
    for (int i = 0; list != NULL && i < count; i++) {
        list[i].label = places[i].label;
        list[i].path = places[i].path;
    }
    BrowserMode mode = slot->def->id == SET_ID_BACKGROUND_IMAGE ? BROWSER_IMAGE : BROWSER_FOLDER;
    browser = list != NULL ? browser_open(mode, slot->value.text, list, count, list_folder, check_path, slot) : NULL;
    free(list);
    fileio_free_places(places, count);
    if (browser == NULL)
        return;
    browser_slot = slot;
    browser_first = 0;
    browser_note[0] = '\0';
    log_debug("Settings: browsing %s", browser_folder(browser) != NULL ? browser_folder(browser) : "the places");
    preview_highlighted();
}

// A function to close the folder browser, back to the Background page
static void close_browser(void)
{
    browser_free(browser);
    browser = NULL;
    browser_slot = NULL;
    want_preview_image("");
}
```

In `free_screen()`, add as its first lines:

```c
    if (browser != NULL)
        close_browser();
    stop_decoding();
```

In `handle_event()`, replace the `SETTINGS_EVENT_BROWSE` case with:

```c
        case SETTINGS_EVENT_BROWSE:
            open_browser(event->slot);
            return;
```

Before `settings_handle_command()`, add:

```c
// A function to act on a key while the folder browser is open: move, page, open, choose or go back
static void handle_browser_command(const char *command)
{
    BrowserCommand key;
    browser_note[0] = '\0';
    if (MATCH(command, SCMD_UP))
        key = BROWSER_UP;
    else if (MATCH(command, SCMD_DOWN))
        key = BROWSER_DOWN;
    else if (MATCH(command, SCMD_LEFT))
        key = BROWSER_PAGE_UP;
    else if (MATCH(command, SCMD_RIGHT))
        key = BROWSER_PAGE_DOWN;
    else if (MATCH(command, SCMD_SELECT))
        key = BROWSER_OK;
    else if (MATCH(command, SCMD_BACK))
        key = BROWSER_BACK;
    else if (MATCH(command, SCMD_HOME) || MATCH(command, SCMD_SETTINGS)) {
        // Leave the browser without choosing, then close settings as the pages would
        close_browser();
        SettingsEvent event = settings_command(model, MATCH(command, SCMD_HOME) ? SETTINGS_HOME : SETTINGS_CLOSE);
        handle_event(&event);
        return;
    }
    else {
        log_debug("Settings: ignoring '%s' while settings are open", command);
        return;
    }

    BrowserResult result = browser_command(browser, key, browser_page);
    if (result == BROWSER_CLOSED) {
        close_browser();
        return;
    }
    if (result == BROWSER_CHOSEN) {
        char chosen[BROWSER_PATH_MAX];
        copy_string(chosen, browser_chosen(browser), sizeof(chosen));
        if (strcmp(chosen, broken_path) == 0) {
            snprintf(browser_note, sizeof(browser_note), "This image cannot be opened");
            return;
        }
        SettingSlot *slot = browser_slot;
        close_browser();
        log_debug("Settings: chose %s", chosen);
        SettingsEvent event = settings_choose(model, slot, chosen);
        handle_event(&event);
        return;
    }
    if (key == BROWSER_OK && result == BROWSER_NONE) {
        const BrowserRow *row = browser_row(browser, browser_cursor(browser));
        if (row != NULL && row->why != NULL)
            snprintf(browser_note, sizeof(browser_note), "%s", row->why);
    }
    preview_highlighted();
}
```

In `settings_handle_command()`, after `if (model == NULL) return;`, add:

```c
    if (browser != NULL) {
        handle_browser_command(command);
        return;
    }
```

Add a row drawer for the browser before `draw_column()`:

```c
// A function to draw the browser's rows, scrolled to keep the cursor in view
static void draw_browser_rows(int x, int top, int bottom)
{
    int count = browser_row_count(browser);
    int cursor = browser_cursor(browser);
    browser_page = max_int(1, (bottom - top) / row_height);
    if (cursor < browser_first)
        browser_first = cursor;
    if (cursor >= browser_first + browser_page)
        browser_first = cursor - browser_page + 1;
    browser_first = max_int(0, min_int(browser_first, count - browser_page));
    int y = top;
    for (int i = browser_first; i < count && y + row_height <= bottom; i++) {
        const BrowserRow *row = browser_row(browser, i);
        bool opens = row->kind == BROWSER_ROW_PLACE || row->kind == BROWSER_ROW_FOLDER;
        SettingsRow shown;
        memset(&shown, 0, sizeof(shown));
        shown.kind = opens ? SETTINGS_ROW_LINK : SETTINGS_ROW_ACTION;
        shown.enabled = opens || row->enabled;
        copy_string(shown.label, row->name, sizeof(shown.label));
        if (row->kind == BROWSER_ROW_USE_FOLDER)
            snprintf(shown.value, sizeof(shown.value), row->image_count == 1 ? "%i image" : "%i images", row->image_count);
        y += draw_row(&shown, i == cursor, x, y, column_width);
    }
}
```

In `draw_column()`, replace:

```c
    settings_path(model, path, sizeof(path));
    draw_text(font_small, path, x, margin + TTF_FontHeight(font_header), column_width, ALPHA_DIM, false);
    int top = (int) (ROWS_TOP_RATIO * (float) geo.screen_height);
    int hint_y = geo.screen_height - margin - TTF_FontHeight(font_small);
    draw_model_rows(x, top, hint_y - margin);
    const char *hint = settings_page(model) == SETTINGS_PAGE_TOP
                       ? "Left and right change \xC2\xB7 OK opens \xC2\xB7 Back saves and closes"
                       : "Left and right change \xC2\xB7 OK opens \xC2\xB7 Back goes back";
```

with:

```c
    if (browser != NULL)
        copy_string(path, browser_folder(browser) != NULL ? browser_folder(browser) : "Places", sizeof(path));
    else
        settings_path(model, path, sizeof(path));
    draw_text(font_small, path, x, margin + TTF_FontHeight(font_header), column_width, ALPHA_DIM, false);
    int top = (int) (ROWS_TOP_RATIO * (float) geo.screen_height);
    int hint_y = geo.screen_height - margin - TTF_FontHeight(font_small);
    if (browser != NULL)
        draw_browser_rows(x, top, hint_y - margin);
    else
        draw_model_rows(x, top, hint_y - margin);
    const char *hint = browser != NULL ? "Left and right page \xC2\xB7 OK opens or chooses \xC2\xB7 Back goes up"
                     : settings_page(model) == SETTINGS_PAGE_TOP
                       ? "Left and right change \xC2\xB7 OK opens \xC2\xB7 Back saves and closes"
                       : "Left and right change \xC2\xB7 OK opens \xC2\xB7 Back goes back";
```

In `draw_caption()`, replace its last line:

```c
    draw_text(font_small, settings_notice(model), preview_rect.x, y + TTF_FontHeight(font_small), preview_rect.w, 255, false);
```

with:

```c
    const char *note = settings_notice(model);
    if (browser != NULL) {
        const BrowserRow *row = browser_row(browser, browser_cursor(browser));
        note = browser_note[0] != '\0' ? browser_note : (row != NULL && row->why != NULL ? row->why : "");
    }
    draw_text(font_small, note, preview_rect.x, y + TTF_FontHeight(font_small), preview_rect.w, 255, false);
```

In `settings_draw()`, add `poll_decode();` as the first line after the `model == NULL` check, and change both `draw_scene();` calls to `draw_scene(true);`.

- [ ] **Step 5: Build and run everything**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: no new warnings, and `100% tests passed, 0 tests failed out of 9`. Then run the headless harness. Expected: every `60-settings-background` line passes, every earlier line still passes, and the last line is `0 failed`.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add src/settings_screen.c src/launcher.h src/launcher.c tests/headless/checks/60-settings-background.sh tests/headless/fixtures/f60-color.ini tests/headless/fixtures/f60-running.ini
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: choose a background image or slideshow folder with the remote"
```

---

### Task 12: Documentation, the sample config and the CHANGELOG

**Files:**
- Modify: `docs/configuration.md`, `config/config.ini.in`, `CHANGELOG.md`, `CONTRIBUTING.md`

- [ ] **Step 1: The sample config.** In `config/config.ini.in`:
- Add a Settings tile to the System menu, after `Entry3=Sleep;sleep;:sleep`:

  ```
  Entry4=Settings;settings;:settings
  ```

- In `[Gamepad]`, replace `#@SETTING_GAMEPAD_BUTTON_START@=` with:

  ```
  @SETTING_GAMEPAD_BUTTON_START@=:settings
  ```

- [ ] **Step 2: `docs/configuration.md`.**

In the Table of Contents, insert `2. [The Settings Screen](#the-settings-screen)` after `1. [Overview](#overview)`, and renumber the entries after it (3 to 9).

In **Overview**, replace the sentence pair `A line can be commented out by using the # character at the beginning of the line, which will cause the line to be ignored by the program. In-line comments are not allowable.` with:

```markdown
A line that starts with `#` or `;` is a comment, and is ignored. A comment can also follow a value, after a space and a semicolon: `Columns=4 ; four across`. The [settings screen](#the-settings-screen) keeps every comment when it saves.
```

Insert this section between **Overview** and **Settings**:

```markdown
## The Settings Screen
The settings screen changes the background, each menu's grid and the title size from the remote, and shows each change in a preview as you make it. It saves your changes into your config file when you leave.

### Opening it
- Press the **Menu** key on the remote (the context-menu key on a keyboard), or **Start** on a gamepad. Either works unless your config gives that key or button something else to do.
- Or run the `:settings` [special command](#special-commands) from a menu entry, a hotkey or a gamepad control. The default config's System menu has a Settings tile.

### Using it
The settings are in a column on the left; the rest of the screen is a live preview of your launcher.
- **Up and Down** move between rows.
- **Left and Right** change the highlighted value.
- **OK** opens a row marked ›.
- **Back** goes back a page. On the first page, it saves your changes and closes settings.
- **Menu** (or Start) closes settings from any page, saving your changes.

While settings are open, other hotkeys and commands wait until they close.

### What it changes
- **Background:** a colour, an image, a slideshow of a folder of images, or transparent. For an image or a folder, a folder browser starts in your Pictures folder and can reach your home folder and your drives; the preview shows each image as you move over it. A slideshow folder needs at least two images.
- **Menus:** the grid of every menu (*All menus*, which is the `[Layout]` section) and of each menu on its own: rows, columns and the largest a button may grow. On a menu's own page, the lowest step, *All menus*, makes that menu follow the shared grid again.
- **Titles:** Small, Medium or Large. Titles scale with each menu's buttons; see [FontSize](#fontsize).

### Saving
- Only the settings you changed are written. Everything else in your config file stays as it was: comments, blank lines and order included.
- The previous version is kept beside it as `config.ini.bak`.
- **Discard changes**, on the first page, puts everything back as it was when you opened settings.
- **On Linux**, the config installed with the package (in `/usr/share/streamflex`) cannot be changed. Your first save writes your own copy to `~/.config/streamflex/config.ini`, which StreamFlex reads from then on.
- If the file cannot be written, settings say why, and offer to try again or to leave without saving.
```

In **Titles**, replace the `FontSize` section's text and default with:

```markdown
##### FontSize
Defines the size of the menu entry titles, in one of two ways:
- **A percentage** of the button size, such as `14%`. Each menu's titles follow its buttons, so a dense grid gets smaller titles and a row of large buttons gets larger ones. Titles never get smaller than 2% of the screen height, so they stay readable from the couch. The settings screen's Small, Medium and Large are `11%`, `14%` and `17%`.
- **A fixed size**, such as `36`: the same size in every menu, as in earlier versions.

Default: 14%
```

Replace the `OversizeMode` section's list and default with:

```markdown
- Truncate: Truncates the title at the maximum width and adds "..." to the end. ("Truncated" is accepted too.)
- Shrink: Shrinks an oversized title to a smaller size than `FontSize` so that it fits, but never below 2% of the screen height. A title that still does not fit is truncated.
- None: No action is taken to limit the width of titles. Overlaps with other titles may occur, and it is the user's responsibility to manually handle any such case.

Default: Truncate
```

Replace the `Padding` section's text and default with:

```markdown
##### Padding
Defines the vertical spacing between an icon and its title: a percentage of the button size, such as `8%`, or a number of pixels. A number of pixels is capped at half the button's size.

Default: 8%
```

In **Special Commands**, add after the `:sleep` section's footnote (before `### Desktop Files (Linux Only)`):

```markdown
#### :settings
Opens the [settings screen](#the-settings-screen). Running it again while settings are open saves your changes and closes them.
```

In **Gamepad Controls**, after the paragraph starting `Up and down have defaults of their own.`, add:

```markdown
Start has a default too: if your config maps nothing to `:settings`, Start opens the [settings screen](#the-settings-screen), unless your config already uses Start for something else.
```

- [ ] **Step 3: `CHANGELOG.md`.** Under `## [Unreleased]`, add an `### Added` section above `### Changed`:

```markdown
### Added
- **A settings screen.** Press the Menu key on the remote, Start on a gamepad, or run the new `:settings` command, and change the background, each menu's grid and the title size from the remote, with a live preview beside the settings. Leaving saves only what changed into your config file, keeping its comments and layout, and keeps the previous version as `config.ini.bak`. On Linux, a read-only packaged config is saved as your own `~/.config/streamflex/config.ini`.
- The default config's System menu has a Settings tile, and its gamepad section maps Start to `:settings`.
- Headless tests: CI runs the launcher under a virtual display, drives it with key presses, and checks what it logs and saves.
```

Add to `### Changed`:

```markdown
- **Titles scale with each menu's buttons.** `FontSize` and `Padding` take a percentage of the button size, and a config that doesn't set them now gets `14%` and `8%`: the same as before on 256 px buttons, smaller in a dense grid, larger on large buttons, and never below 2% of the screen height. A plain number still means a fixed size.
- The default config's `OversizeMode` is `Truncate`, so every title in a menu is the same size.
```

Add to `### Fixed`:

```markdown
- **Titles stay readable in dense grids.** Shrink mode made a long title smaller with no limit, down to a few pixels, and leaked a font each time it went all the way down. It now stops at 2% of the screen height and truncates the rest.
- **Title padding follows the button size.** It was sized for 256 px buttons whatever the grid.
- **`OversizeMode=Truncate` works.** The parser only knew `Truncated`, so the documented spelling was ignored.
- **StreamFlex starts on a display that reports no refresh rate**, as Xvfb, some VMs and remote desktops do. It divided by zero before drawing anything; it now uses 60 Hz and logs it.
- **File paths with non-ASCII characters work on Windows**, such as a config, log, icon library or slideshow folder under a user folder named `José`.
- `Mode=Slideshow` without a `SlideshowDirectory` crashed at startup; it now falls back to the colour background. A slideshow folder with a single image no longer rewrites the `Image` setting.
- Invalid `Mode`, `Color`, `SlideshowImageDuration`, `SlideshowTransitionTime`, `FontSize` and `Padding` values are logged; they were ignored without a word. `Color` refuses a value with a stray character (`#12345G`) instead of half-reading it.
```

- [ ] **Step 4: `CONTRIBUTING.md`.**
- Replace the `src/` line of **Project Structure** with:

  ```
  src/                 Launcher core (launcher.c, layout.c, library.c, image.c, clock.c, util.c, utf8.c, debug.c) and the settings screen (settings_screen.c, settings.c, browser.c, inidoc.c, config_save.c, fileio.c)
  ```

- Add after the `tests/` line:

  ```
  tests/headless/      Headless checks: the launcher under Xvfb, driven by key presses (see run.sh); CI runs them
  ```

- In the `build-and-test` bullet, replace `builds and the \`Icon library\` check all succeed.` with `builds, the \`Icon library\` check and the headless checks all succeed.`

- [ ] **Step 5: Build, test, and read the docs page**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 9`. `test_inidoc` round-trips the regenerated sample config with its new Settings tile and Start mapping.

The new section links only to anchors that already exist (`#special-commands`, `#fontsize`) or that it creates (`#the-settings-screen`). Do not link to `#settings-1`: several headings are named "Settings", and adding `:settings` changes which one gets that id.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add docs/configuration.md config/config.ini.in CHANGELOG.md CONTRIBUTING.md
git -C C:/Users/jscha/source/repos/streamflex commit -m "docs: the settings screen, titles that scale, and :settings"
```

---

### Task 13: Final verification, the hands-on check, and the review

**Files:** none new. This task proves the branch, then hands it on.

- [ ] **Step 1: The full local run**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
docker build -t streamflex-test C:/Users/jscha/source/repos/streamflex/tests/headless
docker run --rm --security-opt seccomp=unconfined -v C:/Users/jscha/source/repos/streamflex:/src:ro -v C:/Users/jscha/ClaudeScratch/streamflex-headless-out:/out streamflex-test bash /src/tests/headless/run.sh final
docker run --rm --security-opt seccomp=unconfined -v C:/Users/jscha/source/repos/streamflex:/src:ro -v C:/Users/jscha/ClaudeScratch/streamflex-headless-out:/out streamflex-test bash /src/tests/headless/run.sh final-scrollfail scrollfail
```

Expected: `100% tests passed, 0 tests failed out of 9`; both harness runs end `0 failed`, with no `BUILD WARNING:` lines naming a file this branch added.

- [ ] **Step 2: No new warnings under `EXTRA_WARNINGS`.** Build the branch and `master` with `-Wall -Wextra -Wpedantic -Wconversion` in the harness image, and compare the warnings in the files this branch touched:

```powershell
docker run --rm -v C:/Users/jscha/source/repos/streamflex:/src:ro streamflex-test bash -c "tar -C /src --exclude=./build --exclude=./.git -cf - . | (mkdir -p /w && tar -C /w -xf -) && cmake -S /w -B /w/b -DEXTRA_WARNINGS=ON > /dev/null && cmake --build /w/b 2>&1 | grep -E 'warning:' | sed -E 's/:[0-9]+:[0-9]+:/:/' | sort -u" > C:/Users/jscha/ClaudeScratch/streamflex-headless-out/warnings-branch.txt
git -C C:/Users/jscha/source/repos/streamflex worktree add C:/Users/jscha/ClaudeScratch/streamflex-master-wt master
docker run --rm -v C:/Users/jscha/ClaudeScratch/streamflex-master-wt:/src:ro streamflex-test bash -c "tar -C /src --exclude=./build --exclude=./.git -cf - . | (mkdir -p /w && tar -C /w -xf -) && cmake -S /w -B /w/b -DEXTRA_WARNINGS=ON > /dev/null && cmake --build /w/b 2>&1 | grep -E 'warning:' | sed -E 's/:[0-9]+:[0-9]+:/:/' | sort -u" > C:/Users/jscha/ClaudeScratch/streamflex-headless-out/warnings-master.txt
git -C C:/Users/jscha/source/repos/streamflex worktree remove C:/Users/jscha/ClaudeScratch/streamflex-master-wt
Compare-Object (Get-Content C:/Users/jscha/ClaudeScratch/streamflex-headless-out/warnings-master.txt) (Get-Content C:/Users/jscha/ClaudeScratch/streamflex-headless-out/warnings-branch.txt) | Where-Object SideIndicator -eq '=>'
```

(Line numbers are stripped, so moved code does not show as new.) Expected: no output, which means no warning the branch introduced. Fix any that appear, and commit the fix as `fix: warnings under EXTRA_WARNINGS`.

- [ ] **Step 3: Push and open the PR; let CI run.** Push `feat/settings-screen` and open a PR with a body that follows the repo's PR template. Wait for every check, including the new **Headless** job and `build-and-test`, and read their results from the API, not from a notification.

- [ ] **Step 4: The hands-on check, through the qa-harness project,** as for PRs #24 and #27. Hand this checklist to that project's session. Its Windows 11 guest runs this PR's CI Windows zip, driven by SendInput keys and the ViGEm virtual Xbox pad. Ask for a frame for each step and the `streamflex.log`:
  1. Start StreamFlex with the shipped config. Press **Start** on the pad: settings open, with the column on the left and the menu in the preview.
  2. **Background:** step Mode through Colour (step the colour), Image, Slideshow and Transparent (checkerboard in the preview), then back to Colour.
  3. **Image:** choose Mode Image, open the browser, and go into a folder with a non-ASCII name (make `C:\Users\<user>\Pictures\Été` with two images first). Move over the images and watch the preview follow; choose one.
  4. **Menus:** open All menus and step Columns from 4 to 6; watch the preview's buttons shrink and their titles follow. Open System and step its Rows.
  5. **Titles:** step Size from Medium to Large and back to Small, watching the preview.
  6. Back out to the top level and press Back: settings close. Quit StreamFlex, and diff `config.ini` against the zip's original. Expect only the changed lines, a `config.ini.bak` next to it, and every comment intact.
  7. Start StreamFlex again: the background, grids and titles are as saved.
  8. Make `config.ini` read-only, change a setting and press Back: the failure rows say "permission denied"; *Leave without saving* closes.
  9. **Launching a non-ASCII path** (Task 2's `start_process()` fix, which only Windows can show). Make the config writable again, create `C:\Users\Public\Été\café.txt`, and add `Entry9=Café;apps;"C:\Users\Public\Été\café.txt"` to `[Main]` in `config.ini`, saved as UTF-8. Start StreamFlex and press Café: Notepad opens the file, and the log has no `Failed to launch command`.

- [ ] **Step 5: A whole-branch review** on the most capable model, as for sub-projects 1 and 2, before the PR is armed. Every Critical or Important finding is fixed on the branch before merging. Minors become todo entries only at the user's word.

- [ ] **Step 6: Merge.** Squash-merge once CI, the hands-on check and the review are all clean. Arming auto-merge seals the branch: nothing more is pushed to it.

---

## Self-review

Checked against the spec after writing:
- **Spec coverage.** Every spec section maps to a task:
  - the screen and keys: Task 10;
  - the preview: Tasks 9 to 11;
  - saving, discarding and failures: Tasks 4 and 10;
  - while settings are open: Task 10;
  - the Background page and the browser: Tasks 8 and 11;
  - Menus and Titles: Tasks 6 and 10;
  - title sizing: Tasks 5 and 7;
  - the writer: Tasks 3 and 4;
  - UTF-8 paths: Task 2;
  - item 22 and the harness in CI: Task 1;
  - errors and debug output: Tasks 6, 7, 9 and 10;
  - documentation: Task 12;
  - hands-on testing: Task 13.

  The deviations from the spec's letter are listed under **Spec corrections found while planning**, each with its reason.
- **Existing bugs.** The five under **Existing bugs fixed on the way** are fixed in Tasks 1, 2 and 9, each pinned by a check: harness checks `25-menus` and the three new `30-backgrounds` lines, `test_wide`, and hands-on step 9 in Task 13.
- **Placeholders.** None, apart from two deliberate interim states:
  - Task 9 Step 6 moves existing drawing code verbatim; its `...` names the blocks to move.
  - Task 10's `SETTINGS_EVENT_BROWSE` case does nothing until Task 11 replaces it.
- **Type consistency.** The functions, structs and constants each task consumes are named exactly as the task that produces them declares them, and each task's **Interfaces** block lists them.
- **Review Focus.** All five lines are pinned by named tests in their owning tasks.

