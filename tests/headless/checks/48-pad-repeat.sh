# A held pad control repeats in wall time, whatever the frame rate: 500 ms after its press, then every
# 25 ms or every frame, whichever is longer. Counted in frames, as it was, a fast machine repeated a short
# tap and a slow one repeated late; the fast side is check 62's taps on the harness's own unpaced frames.
# Here each frame is held up 40 ms (STREAMFLEX_TEST_FRAME_SLEEP_MS), a slow machine at about 25 fps:
# a D-pad Right held 0.8 s repeats (31 frames of 40 ms would take 1.24 s), and one tapped for 0.3 s
# presses once. The frame report says the frames really were that slow.
p48_hold() { : > /tmp/pad-right; sleep 0.8; rm -f /tmp/pad-right; sleep 1; }
p48_tap() { : > /tmp/pad-right; sleep 0.3; rm -f /tmp/pad-right; sleep 1; }
# A function to tell whether a run's frame report shows 26 frames a second or fewer
p48_slow() {
    local report frames span
    report=$(grep -o 'Test hook: [0-9]* frames in [0-9]* ms' "$out/$1.log" | head -1)
    read -r frames span <<< "$(sed 's/Test hook: \([0-9]*\) frames in \([0-9]*\) ms/\1 \2/' <<< "$report")"
    [ -n "$report" ] && [ $((frames * 1000)) -le $((26 * span)) ]
}
rm -f /tmp/pad-right
for p48_run in hold tap; do
    STREAMFLEX_TEST_FRAME_SLEEP_MS=40 STREAMFLEX_TEST_FRAME_REPORT_MS=2000 STREAMFLEX_TEST_PAD=/tmp/pad-right \
        STREAMFLEX_TEST_PAD_BUTTON=dpright WAIT_FOR='Gamepad connected' CFG=$FX/f47-grid.ini \
        run_keys "f48-slow$p48_run" "+p48_$p48_run"
    rm -f /tmp/pad-right
done
p48_count() { grep -c "$2" "$out/$1.log"; }
ok=1
p48_slow f48-slowhold && [ "$(p48_count f48-slowhold 'Gamepad ButtonDPadRight detected')" = 1 ] \
    && [ "$(p48_count f48-slowhold 'Test hook: pad ButtonDPadRight repeated')" -ge 1 ] && ran_clean f48-slowhold && ok=0
result "pad: at 25 fps a D-pad held 0.8 s repeats, 500 ms after its press (pressed $(p48_count f48-slowhold 'Gamepad ButtonDPadRight detected') times, repeated $(p48_count f48-slowhold 'Test hook: pad ButtonDPadRight repeated') times; exit $(cat "$out/f48-slowhold.code"))" $ok
ok=1
p48_slow f48-slowtap && [ "$(p48_count f48-slowtap 'Gamepad ButtonDPadRight detected')" = 1 ] \
    && [ "$(p48_count f48-slowtap 'Test hook: pad ButtonDPadRight repeated')" = 0 ] && ran_clean f48-slowtap && ok=0
result "pad: at 25 fps a D-pad tapped for 0.3 s presses once (pressed $(p48_count f48-slowtap 'Gamepad ButtonDPadRight detected') times, repeated $(p48_count f48-slowtap 'Test hook: pad ButtonDPadRight repeated') times; exit $(cat "$out/f48-slowtap.code"))" $ok
grep -h 'Test hook: [0-9]* frames in' "$out/f48-slowhold.log" "$out/f48-slowtap.log" | sed 's/^/      /'
