# Native Wayland (spec section 5). StreamFlex sets no video driver: SDL 3.2.10 and later prefer
# Wayland only when the compositor offers wp_fifo_manager_v1, and otherwise use X11. A Weston
# headless compositor runs beside Xvfb so that the choice is a real one. Weston 15 (Fedora 44)
# advertises fifo-v1 with every backend, and Weston 14 (Debian trixie) has none, so each image runs
# one branch of SDL's rule; the Weston version is read, never the distribution. Read in Weston's
# source:
# - the headless output's mode is --width and --height times --scale;
# - Weston 14's headless backend has no seat, and Weston 15's has one with a keyboard only under
#   --fake-seat, so focus (which a launched app's detection reads) is checked on Weston 15 only;
# - the kiosk shell makes each window fullscreen, activates it as it maps, and activates the one
#   below when it closes.
# Weston has no keys to press, so the key-driven checks stay on Xvfb; the harness's virtual pad
# drives these runs once the window holds the keyboard. The launch case, whose result reads a 2 s
# bound, is in 67-wayland-timing.sh.
# Not timing-sensitive: every wait ends on a log line or a file (Weston's socket, polled for 30 s; the launcher's lines, 20 s), the 1 s sleeps are floors after a log line, and no case reads a duration
W67_XDG=/tmp/w67-xdg
rm -rf "$W67_XDG" /tmp/w67-child /tmp/w67-app /tmp/w67-pad /tmp/w67-client.pid
mkdir -p "$W67_XDG" /tmp/w67-child
chown tester:tester "$W67_XDG" /tmp/w67-child
chmod 700 "$W67_XDG"
w67_major=$(weston --version 2> /dev/null | sed -n 's/^weston \([0-9][0-9]*\)\..*/\1/p')
w67_weston=

# A function to start a headless Weston as the test user on the socket NAME, with an output of
# WIDTH x HEIGHT logical pixels at SCALE (its mode is WIDTH*SCALE x HEIGHT*SCALE), and the fake
# seat from Weston 15 on; its PID goes in w67_weston, its log in weston-NAME.log. Fails when its
# socket never appears within 30 s, or Weston exits first.
w67_start() {
    local socket=$1 width=$2 height=$3 scale=$4 seat=() i
    [ "${w67_major:-0}" -ge 15 ] && seat=(--fake-seat)
    rm -f "$W67_XDG/$socket" "$W67_XDG/$socket.lock"
    setpriv --reuid=tester --regid=tester --init-groups -- env HOME="$TESTER_HOME" XDG_RUNTIME_DIR="$W67_XDG" \
        weston --backend=headless --renderer=pixman --shell=kiosk --socket="$socket" --width="$width" \
        --height="$height" --scale="$scale" --idle-time=0 --no-config "${seat[@]}" \
        > "$out/weston-$socket.log" 2>&1 &
    w67_weston=$!
    for i in $(seq 300); do
        [ -S "$W67_XDG/$socket" ] && return 0
        running "$w67_weston" || break
        sleep 0.1
    done
    [ -S "$W67_XDG/$socket" ]
}

# A function to end the Weston that w67_start started
w67_stop() {
    [ -n "$w67_weston" ] || return 0
    kill "$w67_weston" 2> /dev/null
    wait "$w67_weston" 2> /dev/null
    w67_weston=
}

# A function to run one of run.sh's run functions (its name and arguments follow SOCKET) with the
# launcher on the Wayland socket SOCKET beside Xvfb: XDG_RUNTIME_DIR and WAYLAND_DISPLAY join its
# environment after DISPLAY, in a subshell, as run_keys_at swaps the display
w67_on() {
    local socket=$1 word at=()
    shift
    for word in "${TESTER[@]}"; do
        at+=("$word")
        [ "$word" = DISPLAY=:99 ] && at+=("XDG_RUNTIME_DIR=$W67_XDG" "WAYLAND_DISPLAY=$socket")
    done
    ( TESTER=("${at[@]}"); "$@" )
}

# A function to start the launcher as a restart's fresh copy starts (--restarted), wait for it to
# gain the keyboard, and end it; its files are kept under NAME as run_keys keeps them. The leak
# pass retries it as run_quick and run_keys are retried (with_tracer_retry).
w67_restarted() { with_tracer_retry "$1" w67_restarted_once "$1"; }
w67_restarted_once() {
    local name=$1 pid code
    rm -f "$LOG"
    "${TESTER[@]}" "$exe" --restarted -c "$FX/f67-wayland.ini" -d > "$out/$name.out" 2> "$out/$name.err" &
    pid=$!
    wait_line 'Gained keyboard focus' "$pid"
    stop_run "$pid"; code=$?
    echo "$code" > "$out/$name.code"
    cp "$LOG" "$out/$name.log" 2> /dev/null || : > "$out/$name.log"
}

# A function for +keys (see run_keys), called with the run's NAME and PID: hold the D-pad's right
# until the launcher has read it
w67_walk() { : > /tmp/w67-pad; wait_line 'Gamepad ButtonDPadRight detected' "$2"; rm -f /tmp/w67-pad; sleep 1; }

ok=1
command -v weston > /dev/null && w67_start wayland-sf 1920 1080 1 && ok=0
result "wayland: Weston starts headless beside Xvfb (weston ${w67_major:-not found})" $ok
[ $ok = 0 ] || tail -20 "$out/weston-wayland-sf.log" 2> /dev/null | sed 's/^/      /'
if [ $ok = 0 ] && [ "$w67_major" -lt 15 ]; then
    # Weston 14: no fifo-v1, so SDL keeps to X11 and the log says why; the variable still wins
    CFG=$FX/f67-quick.ini w67_on wayland-sf run_quick f67-default
    log=$out/f67-default.log
    ok=1
    grep -q "^Video: SDL's x11 driver, the " "$log" \
        && grep -qx 'Video: x11, though a Wayland compositor is running: it lacks fifo-v1, which SDL needs before it prefers Wayland' "$log" \
        && ran_clean f67-default && ok=0
    result "wayland: with no fifo-v1 SDL keeps to x11, and the log says why (exit $(cat "$out/f67-default.code"))" $ok
    grep -E '^(Video|Renderer|Screen):' "$log" | sed 's/^/      /'

    SDL_VIDEO_DRIVER=wayland CFG=$FX/f67-quick.ini w67_on wayland-sf run_quick f67-forced
    log=$out/f67-forced.log
    ok=1
    grep -q "^Video: SDL's wayland driver, the " "$log" \
        && grep -qx 'Video: the driver list comes from SDL_VIDEO_DRIVER=wayland' "$log" \
        && ! grep -q 'it lacks fifo-v1' "$log" && ran_clean f67-forced && ok=0
    result "wayland: SDL_VIDEO_DRIVER=wayland gives wayland, and the log names the variable (exit $(cat "$out/f67-forced.code"))" $ok
    grep -E '^(Video|Renderer|Screen):' "$log" | sed 's/^/      /'
elif [ $ok = 0 ]; then
    # Weston 15: fifo-v1, so SDL chooses Wayland with no override, at the compositor's size
    CFG=$FX/f67-quick.ini w67_on wayland-sf run_quick f67-default
    log=$out/f67-default.log
    ok=1
    grep -q "^Video: SDL's wayland driver, the " "$log" && ! grep -q '^Video: x11, though' "$log" \
        && ! grep -q '^Video: the driver list comes from' "$log" \
        && grep -qx 'Screen: 1920x1080 px, pixel density 1.00, display scale 1.00' "$log" \
        && ran_clean f67-default && ok=0
    result "wayland: with fifo-v1 and no override SDL chooses wayland, at the compositor's size (exit $(cat "$out/f67-default.code"))" $ok
    grep -E '^(Video|Renderer|Screen):' "$log" | sed 's/^/      /'

    SDL_VIDEO_DRIVER=x11 CFG=$FX/f67-quick.ini w67_on wayland-sf run_quick f67-x11
    log=$out/f67-x11.log
    ok=1
    grep -q "^Video: SDL's x11 driver, the " "$log" \
        && grep -qx 'Video: the driver list comes from SDL_VIDEO_DRIVER=x11' "$log" && ran_clean f67-x11 && ok=0
    result "wayland: SDL_VIDEO_DRIVER=x11 gives x11, and the log names the variable (exit $(cat "$out/f67-x11.code"))" $ok

    # A pad drives the menu: SDL drops pad events for a window without the keyboard
    rm -f /tmp/w67-pad
    STREAMFLEX_TEST_PAD=/tmp/w67-pad STREAMFLEX_TEST_PAD_BUTTON=dpright STREAMFLEX_TEST_PAD_FRAMES=5 \
        WAIT_FOR='Gained keyboard focus' CFG=$FX/f67-wayland.ini w67_on wayland-sf run_keys f67-walk +w67_walk
    ok=1
    grep -q "^Video: SDL's wayland driver, the " "$out/f67-walk.log" \
        && grep -qx 'Gamepad ButtonDPadRight detected' "$out/f67-walk.log" && ran_clean f67-walk && ok=0
    result "wayland: a pad drives the menu on Wayland (exit $(cat "$out/f67-walk.code"))" $ok

    # A restart's fresh copy takes the keyboard as its window maps
    w67_on wayland-sf w67_restarted f67-restart
    ok=1
    precedes "$out/f67-restart.log" 'Restarted, so the window is brought to the front' 'Gained keyboard focus' \
        && ran_clean f67-restart && ok=0
    result "wayland: a restart's fresh copy takes the keyboard on Wayland (exit $(cat "$out/f67-restart.code"))" $ok

    # Scale 2: the window asks for native pixels, so it gets the output's mode, not half of it
    w67_stop
    ok=1
    if w67_start wayland-sf2 1920 1080 2; then
        CFG=$FX/f67-quick.ini w67_on wayland-sf2 run_quick f67-scale
        grep -qx 'Screen: 3840x2160 px, pixel density 2.00, display scale 2.00' "$out/f67-scale.log" \
            && grep -qx 'Resolution:    3840x2160' "$out/f67-scale.log" && ran_clean f67-scale && ok=0
        grep -E '^(Video|Screen|Resolution):' "$out/f67-scale.log" | sed 's/^/      /'
    fi
    result "wayland: at scale 2 the window gets the output's native pixels (exit $(cat "$out/f67-scale.code" 2> /dev/null))" $ok
    w67_stop
    w67_start wayland-sf 1920 1080 1
fi

# The app id is streamflex whatever the program file is called (SDL_SetAppMetadata), not SDL's
# fallback, the file's own name: a copy named sf-renamed, with the assets beside it as the build
# has them, on Wayland on both images, with libwayland's protocol trace in its stderr
if [ -n "$w67_weston" ]; then
    mkdir -p /tmp/w67-app
    cp "$exe" /tmp/w67-app/sf-renamed
    cp -r /work/build/assets /tmp/w67-app/
    chown -R tester:tester /tmp/w67-app
    ( exe=/tmp/w67-app/sf-renamed; SDL_VIDEO_DRIVER=wayland WAYLAND_DEBUG=1 CFG=$FX/f67-quick.ini \
        w67_on wayland-sf run_quick f67-appid )
    ok=1
    grep -qF 'set_app_id("streamflex")' "$out/f67-appid.err" && ! grep -qF 'set_app_id("sf-renamed")' "$out/f67-appid.err" \
        && ran_clean f67-appid && ok=0
    result "wayland: the app id is streamflex whatever the program file is called (exit $(cat "$out/f67-appid.code"))" $ok
    grep -oF 'set_app_id("' "$out/f67-appid.err" > /dev/null || echo "      no set_app_id in the protocol trace"
fi
w67_stop
