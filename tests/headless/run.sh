#!/bin/bash
# StreamFlex headless checks. Builds the launcher with ASan and UBSan, starts a virtual X display,
# runs fixture configs (some with key presses) and reads the debug log, the files they write and
# the screen. Runs inside the image built from this folder's Dockerfile (Debian) or
# Dockerfile.fedora (whose SDL2 is sdl2-compat over SDL3), with the repo mounted read-only at
# /src and an output folder at /out. No step branches on the distribution: what differs between
# the images is detected.
#   run.sh <label>              every check in checks/, in name order
#   run.sh <label> scrollfail   item 11 only: the scroll arrow's texture is forced to fail,
#                               a path no config or input can reach
#   run.sh <label> leaks        every check again, with LeakSanitizer on: a run that leaks fails
#                               its check, and each leak is listed at the end as a failure too
# The build defines STREAMFLEX_TEST_HOOKS, which only this harness does: each hook it enables sits
# under #ifdef STREAMFLEX_TEST_HOOKS in src/, and a STREAMFLEX_TEST_* environment variable drives it.
# Prints PASS or FAIL per check and exits non-zero when any failed. Every run's output, log and
# exit code are kept in /out/<label>.
set -u
label=${1:-}
fault=${2:-}
case $label in
    '' | . | .. | */*) echo "usage: run.sh <label> [scrollfail|leaks], where <label> names a folder in /out"; exit 2 ;;
esac
case $fault in
    '' | scrollfail | leaks) ;;
    *) echo "unknown mode '$fault'"; exit 2 ;;
esac
HERE=/src/tests/headless
FX=$HERE/fixtures
out=/out/$label
rm -rf "$out"; mkdir -p "$out"

# Build a copy of the source, without the Windows build tree (vcpkg, several GB), .git, or the
# output of an earlier run that CONTRIBUTING's command keeps in the repo
rm -rf /work; mkdir -p /work
tar -C /src --exclude=./build --exclude=./.git --exclude=./headless-out -cf - . | tar -C /work -xf -

if [ "$fault" = scrollfail ]; then
    sed -i 's|^    if (scroll->texture == NULL)|    SDL_DestroyTexture(scroll->texture); scroll->texture = NULL; /* FAULT */\n&|' /work/src/image.c
    grep -q 'FAULT' /work/src/image.c || { echo "FAULT NOT INJECTED"; exit 2; }
fi

flags="-fsanitize=address,undefined -fno-omit-frame-pointer -DSTREAMFLEX_TEST_HOOKS"
cmake -S /work -B /work/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="$flags" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" > "$out/configure.log" 2>&1 \
    || { echo "CONFIGURE FAILED"; tail -20 "$out/configure.log"; exit 2; }
cmake --build /work/build -j"$(nproc)" > "$out/build.log" 2>&1 \
    || { echo "BUILD FAILED"; tail -30 "$out/build.log"; exit 2; }

failures=0
result() {
    if [ "$2" = 0 ]; then echo "PASS  $1"; else echo "FAIL  $1"; failures=$((failures + 1)); fi
}

# A compiler warning in our code fails the run; the bundled libraries in src/external are not
# ours. make's own warnings are left out: a busy host can step the container's clock, and make
# then warns of a clock skew, which says nothing about the code.
warnings=$(grep -iE 'warning:' "$out/build.log" | grep -vE '^g?make(\[[0-9]+\])?: ')
ours=$(printf '%s\n' "$warnings" | grep -v '^/work/src/external/' | sed '/^$/d')
printf '%s\n' "$warnings" | sed '/^$/d; s/^/BUILD WARNING: /'
ok=1; [ -z "$ours" ] && ok=0
result "the build has no compiler warnings outside src/external" $ok

# Xvfb reports a refresh rate of 0, which the launcher must survive (item 22). -ac lets the
# unprivileged test user connect to it.
Xvfb :99 -screen 0 1920x1080x24 -ac > /dev/null 2>&1 &
export DISPLAY=:99
for i in $(seq 100); do xdotool getdisplaygeometry > /dev/null 2>&1 && break; sleep 0.2; done
xdotool getdisplaygeometry > /dev/null 2>&1 || { echo "Xvfb DID NOT START"; exit 2; }

# A function standing in for xdotool. A function key pressed, let go or both (key, keydown or keyup
# with one F key) goes through key.py, by its keycode with no modifier held: Fedora's keymap makes
# xdotool hold Alt down for every F key, which a key capture would catch (key.py says why). Anything
# else goes to xdotool itself.
xdotool() {
    if [ $# = 2 ] && [[ $1 =~ ^key(down|up)?$ ]] && [[ $2 =~ ^F[0-9]+$ ]]; then
        python3 "$HERE/key.py" "$1" "$2"
    else
        command xdotool "$@"
    fi
}

# The launcher runs as `tester`: root ignores file permissions, and the settings checks need a
# config it cannot write. setpriv, env and setarch each exec the next, so the launcher keeps the
# PID the shell sees. setarch -R turns address randomization off, because GCC 12's ASan crashes
# at random when the kernel randomizes 32 bits of mmap (WSL2, and GitHub's ubuntu-24.04
# runners); it needs the container started with --security-opt seccomp=unconfined.
exe=/work/build/streamflex
TESTER_HOME=/home/tester
LOG=$TESTER_HOME/.local/share/streamflex/streamflex.log

# Pictures for the background checks: three in Pictures, one on its own, and an empty folder.
# Pictures also holds a file that is not a picture and a hidden real picture, which every scan
# must leave out, so its "3 images" proves the rule. ~/upper has only upper-case extensions (its
# .JPG holds a PNG: SDL_image goes by what a file holds, the scans by its name).
python3 "$HERE/make_images.py" "$TESTER_HOME/Pictures"
printf 'not a picture\n' > "$TESTER_HOME/Pictures/notes.txt"
cp "$TESTER_HOME/Pictures/red.png" "$TESTER_HOME/Pictures/.hidden.png"
mkdir -p "$TESTER_HOME/upper"
cp "$TESTER_HOME/Pictures/blue.png" "$TESTER_HOME/upper/BLUE.PNG"
cp "$TESTER_HOME/Pictures/green.png" "$TESTER_HOME/upper/GREEN.JPG"
mkdir -p "$TESTER_HOME/one" "$TESTER_HOME/empty"
cp "$TESTER_HOME/Pictures/red.png" "$TESTER_HOME/one/"
# Slideshow folders that fail: two files that only look like pictures, and one picture beside one
mkdir -p "$TESTER_HOME/broken" "$TESTER_HOME/mixed"
printf 'not a picture\n' > "$TESTER_HOME/broken/a.png"
printf 'not a picture\n' > "$TESTER_HOME/broken/b.png"
cp "$TESTER_HOME/Pictures/red.png" "$TESTER_HOME/mixed/"
printf 'not a picture\n' > "$TESTER_HOME/mixed/broken.png"
chown -R tester:tester "$TESTER_HOME"

# Mesa's software driver, found from Mesa's GL library for X, libGLX_mesa: before Mesa 24.2 it is
# dri/swrast_dri.so beside that library, which loads it at run time; from 24.2 every driver is
# in libgallium, which the library links (swrast_dri.so is then a stub). softpipe has no JIT, and
# llvmpipe's JIT made ASan runs crash at random, so softpipe is used when the driver has it. A
# Mesa built without it (Fedora's) gets llvmpipe, the only software driver it has.
mesa_glx=$(ldconfig -p | awk '$1 == "libGLX_mesa.so.0" { print $NF; exit }')
mesa_driver=$(ldd "$mesa_glx" 2> /dev/null | awk '$1 ~ /^libgallium/ { print $3; exit }')
[ -n "$mesa_driver" ] || mesa_driver=$(dirname "$mesa_glx")/dri/swrast_dri.so
mesa_driver=$(readlink -f "$mesa_driver")
[ -n "$mesa_glx" ] && [ -f "$mesa_driver" ] || { echo "NO MESA DRIVER FOUND (libGLX_mesa: '$mesa_glx')"; exit 2; }
gallium=llvmpipe
grep -qa softpipe "$mesa_driver" && gallium=softpipe
echo "Mesa: $gallium, from $mesa_driver"

# The leak pass preloads Mesa's driver into the launcher. Unpreloaded, libGL unloads it at exit,
# before LeakSanitizer looks, so the few blocks the driver still holds (from context creation and
# its first flush) lose their only pointers and read as leaks, with stacks in an unknown module.
# A libgallium driver, which libGLX_mesa links, was clean without the preload (Fedora 44); it is
# preloaded all the same, so both kinds of Mesa take one path. ASan must come first in the
# preload list. Only the launcher gets the preload (the env after setarch): an ASan runtime in
# setarch starts before randomization is off, and crashes. Whole stacks
# (fast_unwind_on_malloc=0) reach our code through libraries built without frame pointers; that
# is slower, so only this pass does it. It keeps setarch -R: the ASan build crashes at random
# without it whether or not leaks are looked for.
asan_options=detect_leaks=0
preload=()
if [ "$fault" = leaks ]; then
    asan_options=detect_leaks=1:fast_unwind_on_malloc=0
    # The runtime the launcher links: gcc's libasan.so can be a linker script, which ld.so
    # cannot preload
    libasan=$(ldd "$exe" | awk '$1 ~ /^libasan\.so/ { print $3; exit }')
    [ -f "$libasan" ] || { echo "NO ASAN RUNTIME TO PRELOAD"; exit 2; }
    preload=(env "LD_PRELOAD=$libasan $mesa_driver")
fi
TESTER=(setpriv --reuid=tester --regid=tester --init-groups --
        env HOME=$TESTER_HOME DISPLAY=:99 ASAN_OPTIONS=$asan_options UBSAN_OPTIONS=print_stacktrace=1
            "GALLIUM_DRIVER=$gallium" setarch "$(uname -m)" -R "${preload[@]}")

# A function to give the launcher its config: the fixture NAME.ini, the file CFG names, or
# none at all with CFG=none (the launcher then searches for one)
config_args() {
    local cfg=${CFG:-$FX/$1.ini}
    [ "$cfg" = none ] || printf '%s\n' -c "$cfg"
}

# A function to give the test user a copy of a fixture it can write; prints its path
writable_config() {
    mkdir -p "$TESTER_HOME/cfg"
    rm -f "$TESTER_HOME/cfg/$1.ini" "$TESTER_HOME/cfg/$1.ini.bak" "$TESTER_HOME/cfg/$1.ini.tmp" "$TESTER_HOME/cfg/$1.ini.bak.tmp"
    cp "$FX/$1.ini" "$TESTER_HOME/cfg/$1.ini"
    chown -R tester:tester "$TESTER_HOME/cfg"
    chmod 644 "$TESTER_HOME/cfg/$1.ini"
    echo "$TESTER_HOME/cfg/$1.ini"
}

# A function to tell whether the process PID is still running (an exited child stays a zombie
# until it is waited for, and kill -0 still finds a zombie). The state comes after the process's
# name in brackets, which may hold spaces, so it is read after the last ") ".
running() {
    local stat
    read -r stat 2> /dev/null < "/proc/$1/stat" || return 1
    stat=${stat##*") "}
    [ "${stat%% *}" != Z ]
}

# A function to end the launcher PID's run and return its exit code: TERM (SDL turns it into a
# quit event), then KILL 10 s later for a launcher stuck in a loop that never reads it (exit
# 137). Every run ends through here, so every run has the same escalation.
stop_run() {
    local pid=$1 i
    kill -TERM "$pid" 2> /dev/null
    for i in $(seq 50); do running "$pid" || break; sleep 0.2; done
    kill -KILL "$pid" 2> /dev/null
    # A killed run's "Killed" notice goes nowhere: its exit code, 137, says so
    wait "$pid" 2> /dev/null
}

# A function to run a config whose StartupCmd quits by itself. One still running after 30 s
# (QUICK_LIMIT, which only the harness's own checks shorten) is ended by stop_run, and "did not
# quit by itself" goes into NAME.code beside the exit code, so its check fails even when TERM
# made it exit 0.
run_quick() {
    local name=$1 limit=${QUICK_LIMIT:-30} pid code hung="" i
    local args; mapfile -t args < <(config_args "$name")
    rm -f "$LOG"
    "${TESTER[@]}" "$exe" "${args[@]}" -d > "$out/$name.out" 2> "$out/$name.err" &
    pid=$!
    for i in $(seq $((limit * 5))); do running "$pid" || break; sleep 0.2; done
    running "$pid" && hung=yes
    stop_run "$pid"; code=$?
    [ -z "$hung" ] || code="$code, and did not quit by itself within $limit s"
    echo "$code" > "$out/$name.code"
    cp "$LOG" "$out/$name.log" 2> /dev/null || : > "$out/$name.log"
}

# A function to wait up to 20 s for a line in the launcher's log. It fails when the line never
# comes, or when the launcher (PID) exits without writing it.
wait_line() {
    local line=$1 pid=$2 i
    for i in $(seq 100); do
        grep -qF -- "$line" "$LOG" 2> /dev/null && return 0
        running "$pid" || break
        sleep 0.2
    done
    grep -qF -- "$line" "$LOG" 2> /dev/null
}

# A function to run a config that keeps running, and drive it. It starts the launcher, waits for
# a line in its log (WAIT_FOR, by default the first "Loading menu"), sends the keys a second
# apart, waits for the line UNTIL when that is set, then ends it with stop_run (TERM, and KILL
# 10 s later). A key written +name calls the function `name` with the run's name and PID
# instead: that is how a check does something at a moment the log chooses. Every wait is
# bounded; a line that never came is written into NAME.code beside the exit code, so the check's
# exit code test fails. run_after_line and run_slideshow use this too.
run_keys() {
    local name=$1; shift
    local args; mapfile -t args < <(config_args "$name")
    local start=${WAIT_FOR:-Loading menu} missing="" pid code k
    rm -f "$LOG" "$out/$name.seen" "$out/$name.pixels"
    "${TESTER[@]}" "$exe" "${args[@]}" -d > "$out/$name.out" 2> "$out/$name.err" &
    pid=$!
    if wait_line "$start" "$pid"; then
        for k in "$@"; do
            case $k in
                +*) "${k#+}" "$name" "$pid" ;;
                *) xdotool key "$k"; sleep 1 ;;
            esac
        done
        [ -z "${UNTIL:-}" ] || wait_line "$UNTIL" "$pid" || missing=$UNTIL
    else
        missing=$start
    fi
    stop_run "$pid"; code=$?
    [ -z "$missing" ] || code="$code, and never logged '$missing'"
    echo "$code" > "$out/$name.code"
    cp "$LOG" "$out/$name.log" 2> /dev/null || : > "$out/$name.log"
}

# A function to run a config until its log shows LINE, then send the keys as run_keys does
run_after_line() {
    local name=$1 line=$2; shift 2
    WAIT_FOR=$line run_keys "$name" "$@"
}

# A function to run a slideshow fixture: once its first picture is up, run the rest of the
# arguments as a command (which may take the pictures away), then wait for the loader's verdict,
# the log line UNTIL (the fixtures change every 5 s, the shortest allowed), and quit
run_slideshow() {
    local name=$1 until=$2; shift 2
    slideshow_command=("$@")
    WAIT_FOR='Background set up: Slideshow' UNTIL=$until run_keys "$name" +run_slideshow_command
}
run_slideshow_command() { "${slideshow_command[@]}"; }

# A function to tell whether the sanitizers reported nothing for the run NAME
sanitizer_clean() { ! grep -qE 'AddressSanitizer|runtime error|LeakSanitizer' "$out/$1.err"; }

# A function to tell whether the run NAME exited as expected (0 unless CODE is given) with its
# sanitizers quiet. A launcher that had to be killed exits 137, so it fails here on its own.
ran_clean() { [ "$(cat "$out/$1.code")" = "${2:-0}" ] && sanitizer_clean "$1"; }

# A function to tell whether a log has the line START, a line END after it, and a line holding
# NEEDLE between the two (all fixed strings). A range whose END never comes does not count, so
# a launcher that stopped logging halfway cannot pass by running to the end of the file. The
# strings reach awk through its environment: awk -v would read a backslash in them as an escape.
in_range() {
    s=$2 e=$3 n=$4 awk '
        !open && index($0, ENVIRON["s"]) { open = 1; next }
        open && index($0, ENVIRON["n"]) { found = 1 }
        open && index($0, ENVIRON["e"]) { closed = 1; exit }
        END { exit !(closed && found) }
    ' "$1"
}

# A function to tell whether a log has a line holding FIRST before its first line holding
# SECOND (fixed strings, passed to awk as in_range passes them)
precedes() {
    a=$2 b=$3 awk '
        index($0, ENVIRON["b"]) { ok = seen; done = 1; exit }
        index($0, ENVIRON["a"]) { seen = 1 }
        END { exit !(done && ok) }
    ' "$1"
}

# A function to count the lines that differ between two files (a changed line counts twice)
changed_lines() { diff "$1" "$2" | grep -c '^[<>]'; }

# A function to map a point of the launcher's scene (the full 1920 x 1080 screen it would draw)
# to the screen, inside the settings preview, whose place the live log gives. It fails when the
# log has not said where the preview is.
preview_point() {
    local rect px py pw ph
    rect=$(grep -o 'Settings: the preview is at [0-9]*,[0-9]*, [0-9]* x [0-9]*' "$LOG" | tail -1)
    read -r px py pw ph <<< "$(sed 's/.* at \([0-9]*\),\([0-9]*\), \([0-9]*\) x \([0-9]*\)/\1 \2 \3 \4/' <<< "$rect")"
    [ -n "${ph:-}" ] || return 1
    echo "$((px + $1 * pw / 1920)),$((py + $2 * ph / 1080))"
}

# A function to wait up to 10 s for the screen to show the colours asked for, each x,y=r,g,b:
# it takes a screenshot, reads the points and tries again until they match. Every reading, and
# any error taking the screenshot, is kept in NAME.pixels under TAG, and the last screenshot in
# NAME-TAG.xwd when they never match.
screen_shows() {
    local name=$1 tag=$2; shift 2
    local shot=/tmp/screen.xwd i
    for i in $(seq 50); do
        if { echo "$tag:"; xwd -root -silent -out "$shot" && python3 "$HERE/pixels.py" "$shot" "$@"; } \
            >> "$out/$name.pixels" 2>&1; then
            return 0
        fi
        sleep 0.2
    done
    cp "$shot" "$out/$name-$tag.xwd" 2> /dev/null
    return 1
}

# A function for a +key (see run_keys), called with the run's NAME and PID: wait for the log line
# LINE, then for the settings preview to show each colour asked for, written sx,sy=r,g,b with the
# point in the launcher's scene (or @x,y=r,g,b, a point of the screen). Writes "TAG yes" or
# "TAG no" to NAME.seen for the check to read. A preview whose place was never logged is "no",
# and so is a probe with no points, which any screen would pass.
look() {
    local name=$1 pid=$2 tag=$3 line=$4; shift 4
    local asked=() p rest point placed=yes seen=no
    if [ $# = 0 ]; then
        echo "$tag: no points to read" >> "$out/$name.pixels"
        echo "$tag no" >> "$out/$name.seen"
        return
    fi
    # Xvfb has no window manager to carry out the launcher's fullscreen request, so its window
    # stays 1 x 1 and nothing it draws reaches the screen: give it the screen, as one would
    xdotool search --name '^StreamFlex$' windowmove %@ 0 0 windowsize %@ 1920 1080 > /dev/null 2>&1
    if wait_line "$line" "$pid"; then
        for p in "$@"; do
            rest=${p#*,}
            case $p in
                @*) asked+=("${p#@}") ;;
                *) point=$(preview_point "${p%%,*}" "${rest%%=*}") || placed=no
                   asked+=("$point=${p#*=}") ;;
            esac
        done
        [ "$placed" = yes ] && screen_shows "$name" "$tag" "${asked[@]}" && seen=yes
        [ "$placed" = yes ] || echo "$tag: the log never said where the preview is" >> "$out/$name.pixels"
    fi
    echo "$tag $seen" >> "$out/$name.seen"
}

# A function to list each run in the output folder whose LeakSanitizer found a leak. Each one
# counts as a failure, so the list cannot disagree with the verdict, even for a run whose check
# never looked at its sanitizers. nullglob is on only while the .err files are collected.
list_leaks() {
    local err errs restore
    restore=$(shopt -p nullglob); shopt -s nullglob
    errs=("$out"/*.err)
    eval "$restore"
    for err in "${errs[@]}"; do
        grep -q 'ERROR: LeakSanitizer' "$err" || continue
        echo "LEAK  $(basename "$err" .err): $(grep -m1 '^SUMMARY' "$err")"
        failures=$((failures + 1))
    done
}

if [ "$fault" = scrollfail ]; then
    run_quick f11-scroll
    ok=1
    ran_clean f11-scroll \
        && grep -q 'Could not render scroll indicator, so the scroll indicators were not started' "$out/f11-scroll.log" \
        && ! grep -q 'Scroll indicators started' "$out/f11-scroll.log" && ok=0
    result "item 11: a failed scroll arrow leaves the arrows not started, and exits cleanly (exit $(cat "$out/f11-scroll.code"))" $ok
    grep -m3 -E 'AddressSanitizer|double-free|runtime error' "$out/f11-scroll.err" | sed 's/^/      /'
else
    # A check file that does not parse would stop part-way through when sourced, and the checks
    # after the error would be missing without a word, so each is parsed first. nullglob is on
    # only while they are collected: the checks run with bash's own glob rules.
    restore=$(shopt -p nullglob); shopt -s nullglob
    checks=("$HERE"/checks/*.sh)
    eval "$restore"
    [ "${#checks[@]}" -gt 0 ] || { echo "NO CHECKS FOUND in $HERE/checks"; exit 2; }
    for check in "${checks[@]}"; do
        if bash -n "$check" 2> "$out/parse.err"; then
            . "$check"
        else
            result "$(basename "$check") parses" 1
            sed 's/^/      /' "$out/parse.err"
        fi
    done
    [ "$fault" != leaks ] || list_leaks
fi
# The total counts a leaking run twice on purpose: once as its check's FAIL and once as its LEAK
# line (22 leaking runs read "44 failed"). The LEAK count is what makes a leak in a run whose
# check never looked at its sanitizers fail the pass; do not bring the total down to the FAILs.
echo "$failures failed"
[ "$failures" = 0 ]
