# Key and gamepad bindings (3b), the second half of the timing-sensitive cases from
# 62-settings-bindings.sh: pad taps that must end before the pad's 500 ms repeat or be seen in a frame,
# and the launcher stopped past the 10 s confirmation. The first half is 62-settings-bindings-timing.sh;
# the cases are in two files so the timing set's two shards can share them.
# Timing-sensitive: pad taps against the 500 ms repeat or one frame, and the launcher stopped past the 10 s

TO_KEYBOARD="Menu Down Down Down Down Down Down Down Down Return Return"   # Controls, then Keyboard
QUIT="Down Down Down Down Down Down Down Down Down Return"                 # Quit StreamFlex in the command picker
SAVE="BackSpace BackSpace BackSpace"

# --- Beyond the brief's checks: the branches they leave unexercised ---

B62_ARROW=$(printf ' \xE2\x80\xBA ')
RIGHT_MARK=$(printf '\xE2\x80\xBA')
TO_GAMEPAD="Menu Down Down Down Down Down Down Down Down Return Down Return"   # Controls, then Gamepad
# A pad control first repeats 500 ms after its press, so a hold that must outlast that waits for the
# test hook's line saying settings took it for that long: during a capture, or waiting to be let go
# after one
B62_HELD_CAPTURE='Test hook: pad ButtonA held while settings took it (the capture)'
B62_HELD_LET_GO='Test hook: pad ButtonA held while settings took it (waiting to be let go)'

# The pad's OK pressed on Remove rebuilds the controls the launcher is reading the pad through: it
# stops reading them (no use after free), and the held button keeps its press, so it presses once
b62_press_a_briefly() { : > /tmp/pad-a; sleep 0.3; rm -f /tmp/pad-a; sleep 1; }
rm -f /tmp/pad-a
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padok $TO_GAMEPAD Down Down Down Down Down Return Down Down +b62_press_a_briefly $SAVE
ok=1
[ "$(grep -c 'Gamepad ButtonA detected' "$out/f62-padok.log")" = 1 ] && ! grep -q '^ButtonY' "$cfg" \
    && grep -qx 'ButtonA=:select' "$cfg" \
    && grep -q 'Settings: the bindings now hold 0 hotkeys and 1 controls' "$out/f62-padok.log" \
    && ! sed -n '/Settings: the bindings now hold 0 hotkeys and 1 controls/,$p' "$out/f62-padok.log" | grep -q '^ButtonY ' \
    && grep -qF "Settings: the cursor's row reads ButtonY: Quit StreamFlex $RIGHT_MARK" "$out/f62-padok.log" \
    && ran_clean f62-padok && ok=0
result "bindings: the pad's OK on Remove rebuilds its controls safely, pressing once (exit $(cat "$out/f62-padok.code"))" $ok
echo "      ButtonA came $(grep -c 'Gamepad ButtonA detected' "$out/f62-padok.log") times"

# A pad's button captured while held (A, OK here) does not press on the confirm page until let go,
# held past its repeat's delay; pressed again once let go, it does (Keep)
b62_hold_a() { : > /tmp/pad-a; wait_line "$B62_HELD_LET_GO" "$2"; rm -f /tmp/pad-a; sleep 1; }
b62_tap_a_keep() { : > /tmp/pad-a; sleep 0.3; rm -f /tmp/pad-a; sleep 1; }
rm -f /tmp/pad-a
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padheld $TO_GAMEPAD Down Down Down Return Return +b62_hold_a +b62_tap_a_keep \
    BackSpace BackSpace BackSpace BackSpace
log=$out/f62-padheld.log
ok=1
grep -q 'Settings: capture got ButtonA' "$log" && grep -qF "$B62_HELD_LET_GO" "$log" \
    && [ "$(grep -c 'Gamepad ButtonA detected' "$log")" = 1 ] \
    && grep -q "Settings: the cursor's row reads Key: ButtonA" "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-padheld && ok=0
result "bindings: a pad's button held through its capture does not press until let go (exit $(cat "$out/f62-padheld.code"))" $ok
rm -f /tmp/pad-a

# The pad's OK (A) starting a pad capture, held past its repeat's delay: it is not caught while held,
# the keys wait while the pad captures (two Backs do nothing), and A pressed again once let go is
# caught. The capture listens for 30 s here (STREAMFLEX_TEST_CAPTURE_SECONDS), so a slow runner's 31
# frames fit inside it; the tap ends it, so it takes no longer.
b62_hold_a_start() { : > /tmp/pad-a; wait_line "$B62_HELD_CAPTURE" "$2"; rm -f /tmp/pad-a; sleep 0.4; }
b62_tap_a() { : > /tmp/pad-a; sleep 0.3; rm -f /tmp/pad-a; sleep 0.7; }
rm -f /tmp/pad-a
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a STREAMFLEX_TEST_CAPTURE_SECONDS=30 WAIT_FOR='Gamepad connected' \
    CFG=$cfg run_keys f62-padstart $TO_GAMEPAD Down Down Down Return +b62_hold_a_start BackSpace BackSpace +b62_tap_a \
    BackSpace BackSpace BackSpace BackSpace BackSpace
log=$out/f62-padstart.log
ok=1
grep -q 'Test hook: the capture listens for 30 s' "$log" \
    && precedes "$log" "$B62_HELD_CAPTURE" 'Settings: capture got ButtonA' \
    && precedes "$log" ' (#8) detected' 'Settings: capture got ButtonA' \
    && ! sed -n '/Settings: capturing a button/,/Settings: capture got ButtonA/p' "$log" \
         | grep -qxF "Settings: page Settings${B62_ARROW}Controls${B62_ARROW}Gamepad" \
    && [ "$(grep -c 'Gamepad ButtonA detected' "$log")" = 1 ] \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-padstart && ok=0
result "bindings: the pad's OK starts a pad capture, which keeps the keys waiting (exit $(cat "$out/f62-padstart.code"))" $ok
rm -f /tmp/pad-a

# The pad's OK starting a keyboard capture: a key pressed and let go before it is not taken for the
# capture's starting key (F5 is caught), and the pad waits while the keyboard captures (A presses once)
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padkey $TO_KEYBOARD Return F5 +b62_tap_a +b62_tap_a F5 BackSpace BackSpace BackSpace BackSpace BackSpace
log=$out/f62-padkey.log
ok=1
grep -q 'Settings: capturing a key' "$log" && grep -q 'Settings: capture got F5 (#4000003E)' "$log" \
    && [ "$(grep -c 'Gamepad ButtonA detected' "$log")" = 1 ] \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-padkey && ok=0
result "bindings: the pad's OK starts a keyboard capture, which keeps the pad waiting (exit $(cat "$out/f62-padkey.code"))" $ok
rm -f /tmp/pad-a

# The 10 s over while the launcher is held up (stopped here, as a long frame would hold it): Up, pressed
# in that time and read on the first frame after, comes too late, since the clocks run before a frame's
# keys; the change goes back. Up is sent with no wait for its frame (frame_nowait): the launcher is
# stopped until after it, and the next key waits for that frame before its own.
b62_stop_up() { kill -STOP "$2"; sleep 11; frame_nowait=yes xdotool key Up; sleep 0.5; kill -CONT "$2"; sleep 2; }
cfg=$(writable_config f62-up)
CFG=$cfg run_keys f62-stop $TO_KEYBOARD Return Return Up Return Down Return $QUIT +b62_stop_up $SAVE
log=$out/f62-stop.log
ok=1
grep -q 'Settings: the binding went back: Up was not pressed again within 10 s' "$log" && ! grep -q 'Settings: kept Up' "$log" \
    && sed -n '/Settings: press Up again within 10 s/,$p' "$log" | grep -q 'Key Up (#40000052) detected' \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f62-up.ini" "$cfg" && ran_clean f62-stop && ok=0
result "bindings: a key read after the 10 s ended does not keep the change (exit $(cat "$out/f62-stop.code"))" $ok

# A pad's OK tapped for one frame starts a pad capture and is let go on the next: the capture sees it
# let go, so the same button tapped again is the one caught
b62_tap_frame() { : > /tmp/pad-a; sleep 0.5; rm -f /tmp/pad-a; sleep 0.7; }
rm -f /tmp/pad-a
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a STREAMFLEX_TEST_PAD_FRAMES=1 WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padtap $TO_GAMEPAD Down Down Down Return +b62_tap_frame +b62_tap_frame BackSpace BackSpace Menu
log=$out/f62-padtap.log
ok=1
grep -q 'Settings: capturing a button' "$log" && grep -q 'Settings: capture got ButtonA' "$log" \
    && ! grep -q 'Settings: the capture caught nothing' "$log" && [ "$(grep -c 'Gamepad ButtonA detected' "$log")" = 1 ] \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-padtap && ok=0
result "bindings: a pad's OK tapped for one frame is caught when tapped again (exit $(cat "$out/f62-padtap.code"))" $ok
rm -f /tmp/pad-a

# --- Fix round 1 ---

# The 10 s over while the launcher is held up, and the pad's OK pressed on Add binding in that time:
# the clocks run before the pad's first frame after, so the change has gone back and the binding page
# opens (the pad is not told to keep the last change first). OK is let go once the launcher has
# logged this press, not a time after the resume: a slow first frame after it would read the pad
# only once the file had gone. The press is the next "Gamepad ButtonA detected" line, counted before
# it, as an earlier press would have logged one too; it is let go 0.1-0.3 s after it (the wait polls
# every 0.2 s), short of the repeat's 500 ms.
# A function to wait, as wait_line does (up to 20 s, or until the launcher PID exits), for more than
# BEFORE lines holding LINE in the launcher's log; fails when they never come
b62_wait_next() {
    local line=$1 before=$2 pid=$3 i n
    for i in $(seq 100); do
        n=$(grep -cF -- "$line" "$LOG" 2> /dev/null)
        [ "${n:-0}" -gt "$before" ] && return 0
        running "$pid" || break
        sleep 0.2
    done
    n=$(grep -cF -- "$line" "$LOG" 2> /dev/null)
    [ "${n:-0}" -gt "$before" ]
}
b62_stop_pad_a() {
    local before
    kill -STOP "$2"; sleep 11
    before=$(grep -cF 'Gamepad ButtonA detected' "$LOG" 2> /dev/null)
    : > /tmp/pad-a; kill -CONT "$2"
    b62_wait_next 'Gamepad ButtonA detected' "${before:-0}" "$2"; sleep 0.1
    rm -f /tmp/pad-a; sleep 1.5
}
rm -f /tmp/pad-a
cfg=$(writable_config f62-uppad)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padstop $TO_KEYBOARD Return Return Up Return Down Return $QUIT +b62_stop_pad_a \
    BackSpace BackSpace BackSpace BackSpace
log=$out/f62-padstop.log
ok=1
grep -q 'Settings: the binding went back: Up was not pressed again within 10 s' "$log" \
    && sed -n '/Settings: the binding went back/,$p' "$log" \
       | grep -qxF "Settings: page Settings${B62_ARROW}Controls${B62_ARROW}Keyboard${B62_ARROW}Binding" \
    && ! grep -q 'keep the last change first' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f62-uppad.ini" "$cfg" && ran_clean f62-padstop && ok=0
result "bindings: the pad read after the 10 s ended finds the change gone back (exit $(cat "$out/f62-padstop.code"))" $ok
rm -f /tmp/pad-a

# The OK (Return) that starts a pad capture, held on past the button caught: its repeats wait until it
# is let go, so they do not Keep what the capture caught
b62_hold_return_pad_b() {
    xdotool keydown Return; sleep 0.3
    : > /tmp/pad-b; sleep 0.4; rm -f /tmp/pad-b
    sleep 1.5; xdotool keyup Return; sleep 0.5
}
rm -f /tmp/pad-b
cfg=$(writable_config f62-pad)
STREAMFLEX_TEST_PAD=/tmp/pad-b STREAMFLEX_TEST_PAD_BUTTON=b WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-keyholdpad $TO_GAMEPAD Down Down Down Return +b62_hold_return_pad_b BackSpace BackSpace Menu
log=$out/f62-keyholdpad.log
ok=1
grep -q 'Settings: capturing a button' "$log" && grep -q 'Settings: capture got ButtonB' "$log" \
    && [ "$(sed -n '/Settings: capture got ButtonB/,$p' "$log" | grep -c 'Key Return (#D) detected')" -gt 2 ] \
    && ! grep -q "Settings: the cursor's row reads Key: ButtonB" "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-keyholdpad && ok=0
result "bindings: OK held through a pad capture does not Keep what it caught (exit $(cat "$out/f62-keyholdpad.code"))" $ok
rm -f /tmp/pad-b
