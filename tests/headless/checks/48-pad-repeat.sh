# A held pad control repeats in wall time, whatever the frame rate: 500 ms after its press, then every
# 25 ms or every frame, whichever is longer. Counted in frames, as it was, a fast machine repeated a short
# tap and a slow one repeated late; the fast side is check 62's taps on the harness's own unpaced frames.
# Here each frame is held up 40 ms (STREAMFLEX_TEST_FRAME_SLEEP_MS), a slow machine at about 25 fps:
# D-pad Right (bound in f48-pad.ini) held 0.8 s repeats, where the old frame count's 31st frame came at
# least 1.2 s after the press, and tapped it presses once. Each run waits for its frame report (2 s of
# frames), which says the frames really were that slow. Each hold is timed from the launcher's press
# line, not from the file: the runs start at 'Gamepad connected', before the menu is drawn and the loop
# polls the pad, so a slow start (Fedora's leak pass on GitHub) took the press late and the 0.8 s
# timed from the file ended before the repeat. wait_line sees the line up to 0.2 s late, so the hold
# lasts 0.8-1.0 s from the press, still short of the old 1.2 s, and the tap 0.1-0.3 s, short of 0.5 s.
p48_hold() {
    : > /tmp/pad-right
    wait_line 'Gamepad ButtonDPadRight detected' "$2" && sleep 0.8
    rm -f /tmp/pad-right; sleep 1
}
p48_tap() {
    : > /tmp/pad-right
    wait_line 'Gamepad ButtonDPadRight detected' "$2" && sleep 0.1
    rm -f /tmp/pad-right; sleep 1
}
# A function to tell whether a run's frame report shows 26 frames a second or fewer
p48_slow() {
    local report frames span
    report=$(grep -o 'Test hook: [0-9]* frames in [0-9]* ms' "$out/$1.log" | head -1)
    read -r frames span <<< "$(sed 's/Test hook: \([0-9]*\) frames in \([0-9]*\) ms/\1 \2/' <<< "$report")"
    [ -n "$report" ] && [ $((frames * 1000)) -le $((26 * span)) ]
}
p48_count() { grep -c "$2" "$out/$1.log"; }
rm -f /tmp/pad-right
for p48_run in hold tap; do
    STREAMFLEX_TEST_FRAME_SLEEP_MS=40 STREAMFLEX_TEST_FRAME_REPORT_MS=2000 UNTIL=' of them paced' \
        STREAMFLEX_TEST_PAD=/tmp/pad-right STREAMFLEX_TEST_PAD_BUTTON=dpright WAIT_FOR='Gamepad connected' \
        CFG=$FX/f48-pad.ini run_keys "f48-slow$p48_run" "+p48_$p48_run"
    rm -f /tmp/pad-right
done
ok=1
p48_slow f48-slowhold && [ "$(p48_count f48-slowhold 'Gamepad ButtonDPadRight detected')" = 1 ] \
    && [ "$(p48_count f48-slowhold 'Test hook: pad ButtonDPadRight repeated')" -ge 1 ] && ran_clean f48-slowhold && ok=0
result "pad: at 25 fps a D-pad held 0.8 s repeats, 500 ms after its press (pressed $(p48_count f48-slowhold 'Gamepad ButtonDPadRight detected') times, repeated $(p48_count f48-slowhold 'Test hook: pad ButtonDPadRight repeated') times; exit $(cat "$out/f48-slowhold.code"))" $ok
ok=1
p48_slow f48-slowtap && [ "$(p48_count f48-slowtap 'Gamepad ButtonDPadRight detected')" = 1 ] \
    && [ "$(p48_count f48-slowtap 'Test hook: pad ButtonDPadRight repeated')" = 0 ] && ran_clean f48-slowtap && ok=0
result "pad: at 25 fps a D-pad tapped, let go 0.1-0.3 s after its press, presses once (pressed $(p48_count f48-slowtap 'Gamepad ButtonDPadRight detected') times, repeated $(p48_count f48-slowtap 'Test hook: pad ButtonDPadRight repeated') times; exit $(cat "$out/f48-slowtap.code"))" $ok
grep -h 'Test hook: [0-9]* frames in' "$out/f48-slowhold.log" "$out/f48-slowtap.log" | sed 's/^/      /'

# A control held through a gap in polling does not repeat the moment polling resumes: A selects the
# first button, which launches `sleep 1`; the launcher does not poll the pad until the launch's 3 s
# timeout, and A is held on through it. Its hold is timed again from the resume, which the test hook
# logs, and A is let go 0.1 s after that line (wait_line sees it up to 0.2 s late, so 0.1-0.3 s after
# the resume), short of the next repeat, 500 ms after the resume. Timed from the press, as it was, it
# would have repeated at once, launching again.
p48_launch() {
    : > /tmp/pad-a
    wait_line 'Test hook: pad ButtonA held through a gap in polling' "$2" && sleep 0.1
    rm -f /tmp/pad-a; sleep 1
}
rm -f /tmp/pad-a
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$FX/f48-pad.ini \
    run_keys f48-launchhold +p48_launch
rm -f /tmp/pad-a
ok=1
[ "$(p48_count f48-launchhold 'Gamepad ButtonA detected')" = 1 ] \
    && grep -q 'Test hook: pad ButtonA held through a gap in polling, timed from now' "$out/f48-launchhold.log" \
    && [ "$(p48_count f48-launchhold 'Test hook: pad ButtonA repeated')" = 0 ] && ran_clean f48-launchhold && ok=0
result "pad: A held through a launch's timeout is timed again from the resume, and does not repeat at once (pressed $(p48_count f48-launchhold 'Gamepad ButtonA detected') times, repeated $(p48_count f48-launchhold 'Test hook: pad ButtonA repeated') times; exit $(cat "$out/f48-launchhold.code"))" $ok
grep -E 'Test hook: pad ButtonA|Gamepad ButtonA' "$out/f48-launchhold.log" | sed 's/^/      /'
