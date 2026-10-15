# Native Wayland's timing-sensitive case, from 67-wayland.sh: a launched Wayland client takes the
# keyboard, and the launcher gets it back within 2 s of the client's end (spec sections 5 and 9). It
# needs focus, which Weston gives only on Weston 15 with --fake-seat (Weston 14's headless backend has
# no seat), so it runs on Weston 15 only; this file starts its own Weston, as 67-wayland.sh's may run
# in the other set.
# Timing-sensitive: the launcher must log "Application finished" within 2 s of the client's kill
T67_XDG=/tmp/t67-xdg
rm -rf "$T67_XDG" /tmp/w67-child /tmp/t67-pad /tmp/w67-client.pid
mkdir -p "$T67_XDG" /tmp/w67-child
chown tester:tester "$T67_XDG" /tmp/w67-child
chmod 700 "$T67_XDG"
t67_major=$(weston --version 2> /dev/null | sed -n 's/^weston \([0-9][0-9]*\)\..*/\1/p')
t67_weston=

# A function to start a headless Weston 15 as the test user on the socket NAME, 1920 x 1080 at
# scale 1, with its fake seat, as 67-wayland.sh's w67_start does; its PID goes in t67_weston, its
# log in weston-NAME.log. Fails when its socket never appears within 30 s, or Weston exits first.
t67_start() {
    local socket=$1 i
    rm -f "$T67_XDG/$socket" "$T67_XDG/$socket.lock"
    setpriv --reuid=tester --regid=tester --init-groups -- env HOME="$TESTER_HOME" XDG_RUNTIME_DIR="$T67_XDG" \
        weston --backend=headless --renderer=pixman --shell=kiosk --socket="$socket" --width=1920 \
        --height=1080 --scale=1 --idle-time=0 --no-config --fake-seat \
        > "$out/weston-$socket.log" 2>&1 &
    t67_weston=$!
    for i in $(seq 300); do
        [ -S "$T67_XDG/$socket" ] && return 0
        running "$t67_weston" || break
        sleep 0.1
    done
    [ -S "$T67_XDG/$socket" ]
}

# A function to end the Weston that t67_start started
t67_stop() {
    [ -n "$t67_weston" ] || return 0
    kill "$t67_weston" 2> /dev/null
    wait "$t67_weston" 2> /dev/null
    t67_weston=
}

# A function to run one of run.sh's run functions (its name and arguments follow SOCKET) with the
# launcher on the Wayland socket SOCKET beside Xvfb, as 67-wayland.sh's w67_on does
t67_on() {
    local socket=$1 word at=()
    shift
    for word in "${TESTER[@]}"; do
        at+=("$word")
        [ "$word" = DISPLAY=:99 ] && at+=("XDG_RUNTIME_DIR=$T67_XDG" "WAYLAND_DISPLAY=$socket")
    done
    ( TESTER=("${at[@]}"); "$@" )
}

# Functions for +keys (see run_keys), called with the run's NAME and PID: press A, then wait for the
# launched client to take the keyboard; kill that client by its PID and give the launcher 2 s to log
# "Application finished" (written to NAME.seen as "finished yes" or "finished no")
t67_launch() {
    rm -f /tmp/w67-client.pid
    : > /tmp/t67-pad
    wait_line 'Gamepad ButtonA detected' "$2"
    rm -f /tmp/t67-pad
    wait_line 'Application detected' "$2"
    sleep 1
}
t67_kill() {
    local name=$1 pid i
    pid=$(cat /tmp/w67-client.pid 2> /dev/null)
    if [ -z "$pid" ] || ! kill -TERM "$pid" 2> /dev/null; then
        echo "finished no: no client to kill (PID '$pid')" >> "$out/$name.seen"
        return
    fi
    for i in $(seq 20); do
        grep -qx 'Application finished' "$LOG" && { echo "finished yes" >> "$out/$name.seen"; return; }
        sleep 0.1
    done
    echo "finished no" >> "$out/$name.seen"
}

if [ "${t67_major:-0}" -ge 15 ]; then
    # A launch: the client takes the keyboard as it maps, and the launcher gets it back when the
    # client is killed, within 2 s
    ok=1
    if t67_start wayland-sft; then
        rm -f /tmp/t67-pad
        STREAMFLEX_TEST_PAD=/tmp/t67-pad STREAMFLEX_TEST_PAD_BUTTON=a STREAMFLEX_TEST_PAD_FRAMES=5 \
            WAIT_FOR='Gained keyboard focus' CFG=$FX/f67-wayland.ini t67_on wayland-sft run_keys f67-launch +t67_launch +t67_kill
        log=$out/f67-launch.log
        sed -n '/^Application detected$/,$p' "$log" > "$out/f67-launch.after"
        precedes "$log" 'Lost keyboard focus' 'Application detected' \
            && precedes "$out/f67-launch.after" 'Gained keyboard focus' 'Application finished' \
            && grep -qx 'finished yes' "$out/f67-launch.seen" && ran_clean f67-launch && ok=0
    else
        tail -20 "$out/weston-wayland-sft.log" 2> /dev/null | sed 's/^/      /'
    fi
    result "wayland: a launched Wayland client takes the keyboard, and its end gives it back within 2 s (exit $(cat "$out/f67-launch.code" 2> /dev/null))" $ok
    grep -E '^(Lost|Gained) keyboard focus$|^Application (detected|finished)$' "$out/f67-launch.log" 2> /dev/null | sed 's/^/      /'
    sed 's/^/      /' "$out/f67-launch.seen" 2> /dev/null
    t67_stop
else
    echo "      wayland: weston ${t67_major:-not found} gives no client the keyboard, so the launch case runs on Weston 15 only"
fi
