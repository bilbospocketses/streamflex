# Each feature starts, stops and frees what it holds; the frame timing is worked out from VSync and
# FPSLimit without changing either. The leak pass (run.sh leaks) proves the stops free everything.

# FPSLimit=10 is the documented minimum: with VSync off it sets the frame time, and VSync stays off
run_quick f42-fps10
ok=1
grep -q 'Frame timing: FPS limit 10, 100 ms a frame' "$out/f42-fps10.log" \
    && grep -q 'Frame timing: VSync wanted off, the renderer gives off' "$out/f42-fps10.log" \
    && grep -A4 'General ===' "$out/f42-fps10.log" | grep -qE '^VSync:\s+false$' \
    && ran_clean f42-fps10 && ok=0
result "FPSLimit=10 with VSync off sets 100 ms frames, and leaves VSync as written (exit $(cat "$out/f42-fps10.code"))" $ok

# A renderer that refuses VSync (a test hook stands in for one: the harness's renderers never refuse
# it) is paced by the frame delay instead, and the refusal is logged once
STREAMFLEX_TEST_VSYNC_REFUSED=1 run_quick f42-vsync
ok=1
grep -q 'Frame timing: VSync wanted on, the renderer gives off' "$out/f42-vsync.log" \
    && [ "$(grep -c 'The renderer refused VSync' "$out/f42-vsync.log")" = 1 ] \
    && ran_clean f42-vsync && ok=0
result "a renderer that refuses VSync is logged once, and VSync counts as off (exit $(cat "$out/f42-vsync.code"))" $ok
grep 'VSync' "$out/f42-vsync.log" | sed 's/^/      /'

# A renderer that will not turn VSync off (the same hook, "off") keeps presenting at the display's
# rate, so FPSLimit=10 holds only if present_frame() still paces each frame to 100 ms: every frame
# counted is paced, and there are no more of them than 10 a second allows
STREAMFLEX_TEST_VSYNC_REFUSED=off STREAMFLEX_TEST_FRAME_REPORT_MS=3000 UNTIL='Test hook: ' run_keys f42-fpsstuck
report=$(grep -o 'Test hook: [0-9]* frames in [0-9]* ms, [0-9]* of them paced' "$out/f42-fpsstuck.log" | head -1)
read -r frames span paced <<< "$(sed 's/Test hook: \([0-9]*\) frames in \([0-9]*\) ms, \([0-9]*\) of them paced/\1 \2 \3/' <<< "$report")"
ok=1
grep -q 'Frame timing: VSync wanted off, the renderer gives on' "$out/f42-fpsstuck.log" \
    && grep -q 'The renderer would not turn VSync off' "$out/f42-fpsstuck.log" \
    && [ -n "$report" ] && [ "$paced" = "$frames" ] && [ "$frames" -le $((span / 100 + 2)) ] \
    && ran_clean f42-fpsstuck && ok=0
result "a renderer that keeps VSync on is still paced to FPSLimit=10 (${report:-no frame report}; exit $(cat "$out/f42-fpsstuck.code"))" $ok

# Most of the VSync watch's checks run on a small second display (P42_SIZE), where lavapipe draws far
# above the display's rate; the full-size check below sets its own rate. A function to say why a run's
# frames could not test the watch, or nothing when they could: with none of them paced, its frame report
# must show presents at twice the display's rate or more. A check asks it only once its own assertions
# failed, and adds what it says to the FAIL line, so a future slowdown reads as a broken premise rather
# than as a watch that never fired. It never fails a check by itself: the report counts from the first
# frame, so a stall as the launcher starts (LeakSanitizer's, say) can make it read slow in a run whose
# watch did all that was asked.
P42_SIZE=640x360
p42_premise() {
    local log=$1 report frames span paced rate
    report=$(grep -o 'Test hook: [0-9]* frames in [0-9]* ms, [0-9]* of them paced' "$log" | head -1)
    read -r frames span paced <<< "$(sed 's/Test hook: \([0-9]*\) frames in \([0-9]*\) ms, \([0-9]*\) of them paced/\1 \2 \3/' <<< "$report")"
    rate=$(sed -n 's/^Refresh rate: *\([0-9]*\) Hz$/\1/p' "$log" | head -1)
    if [ -z "$report" ] || [ -z "$rate" ]; then
        echo "premise unknown: no frame report or no refresh rate in the log"
    elif [ "$paced" = 0 ] && [ $((frames * 1000)) -lt $((2 * rate * span)) ]; then
        echo "premise broken: presents not fast enough to test the VSync watch ($frames frames in $span ms, none paced, under twice the display's $rate Hz)"
    fi
}

# A renderer that claims VSync and does not deliver it (the harness's, as f42-all below says) is found
# again whenever the VSync wish changes. In settings, VSync off, then an FPS limit of 30, turn the wish
# off (VSync off alone keeps it: frames still keep the display's rate); the limit back to Off turns it
# on again, and the renderer is watched afresh, so it is found, and said, a second time. Settings then
# close, putting both back, and the home screen runs a while.
p42_home_a_while() { sleep 2; }
STREAMFLEX_TEST_FRAME_REPORT_MS=2000 CFG=$FX/f55-frame.ini run_keys_at $P42_SIZE f42-vsyncagain \
    Menu Return Down Down Down Down Down Left Down Right Left Menu +p42_home_a_while
log=$out/f42-vsyncagain.log
undelivered_line='^Frame timing: the renderer claims VSync but does not deliver it'
again=$(sed -n '/^Frame timing: VSync wanted off, the renderer gives off$/,$p' "$log" | grep -c "$undelivered_line")
ok=1
grep -qx "Resolution: *$P42_SIZE" "$log" \
    && grep -q '^Frame timing: VSync wanted off, the renderer gives off$' "$log" && [ "$(grep -c "$undelivered_line" "$log")" = 2 ] \
    && [ "$again" = 1 ] && ran_clean f42-vsyncagain && ok=0
premise=
[ $ok = 0 ] || premise=$(p42_premise "$log")
result "VSync wanted off and on again is watched afresh, and found undelivered again (said $(grep -c "$undelivered_line" "$log") times, $again after the wish changed; exit $(cat "$out/f42-vsyncagain.code"))${premise:+; $premise}" $ok
grep -E '^(Resolution:|Test hook: [0-9]* frames)' "$log" | sed 's/^/      /'
grep -E '^(Settings: \[General\] (VSync|FPSLimit)|Frame timing:)' "$log" | sed 's/^/      /'

# The case the watch once missed: presents at one to two times the display's rate, at full size (the
# default 1920 x 1080 display). Only a present under half a frame counted as early, so none of them did;
# a present now counts as early under three quarters of a frame (vsyncwatch.c), so the band's upper part,
# above 4/3 of the display's rate, is found, said once, and paced. lavapipe draws f55-frame.ini's one
# button far faster than the band (200 fps and more, measured), so each frame is held up 8 ms as it is
# shown (STREAMFLEX_TEST_FRAME_SLEEP_MS): with the draw, its presents come 8.33-12.5 ms apart at 60 Hz,
# which three quarters of a frame finds and half a frame never does. The premise is a check of its own,
# read from the test hook's measure of the run that found it: its mean gap between half and three
# quarters of the display's frame (80-120 fps at 60 Hz). A run outside the band fails it, with the gaps.
STREAMFLEX_TEST_FRAME_SLEEP_MS=8 STREAMFLEX_TEST_FRAME_REPORT_MS=4000 CFG=$FX/f55-frame.ini UNTIL=' of them paced' \
    run_keys f42-fullsize
log=$out/f42-fullsize.log
report=$(grep -o 'Test hook: [0-9]* frames in [0-9]* ms, [0-9]* of them paced' "$log" | head -1)
read -r frames span paced <<< "$(sed 's/Test hook: \([0-9]*\) frames in \([0-9]*\) ms, \([0-9]*\) of them paced/\1 \2 \3/' <<< "$report")"
undelivered=$(grep -c '^Frame timing: the renderer claims VSync but does not deliver it, so each frame is paced to 16 ms$' "$log")
ok=1
grep -qx 'Resolution: *1920x1080' "$log" \
    && [ -n "$report" ] && [ "$undelivered" = 1 ] && [ "$paced" -gt 0 ] && ran_clean f42-fullsize && ok=0
result "at full size, VSync claimed but not delivered is found, said once and paced (${report:-no frame report}; said $undelivered times)" $ok
run=$(grep -o "Test hook: the VSync watch's run of [0-9]* early presents came [0-9]* to [0-9]* us apart, [0-9]* us on average" "$log" | head -1)
read -r gap_min gap_max gap_mean <<< "$(sed 's/.* came \([0-9]*\) to \([0-9]*\) us apart, \([0-9]*\) us on average/\1 \2 \3/' <<< "$run")"
rate=$(sed -n 's/^Refresh rate: *\([0-9]*\) Hz$/\1/p' "$log" | head -1)
ok=1
[ -n "$run" ] && [ -n "$rate" ] && [ $((gap_mean * 2 * rate)) -gt 1000000 ] && [ $((gap_mean * 4 * rate)) -lt 3000000 ] && ok=0
if [ -n "$run" ]; then measured="gaps $gap_min to $gap_max us, $gap_mean us on average, at ${rate:-an unknown} Hz"
else measured='no measure of the run in the log: the watch never fired'; fi
result "at full size, the run that found it came between half and three quarters of a frame apart on average ($measured)" $ok
grep -E "^(Resolution:|Frame timing:|Test hook: the VSync watch's run)" "$log" | sed 's/^/      /'

# Every feature running at once, the screensaver on and off again, the virtual pad attached, and a
# clean quit: every feature's stop runs at quit. On the small display, for the VSync watch below.
wake() { xdotool key Right; sleep 1; }
rm -f /tmp/pad-none
STREAMFLEX_TEST_PAD=/tmp/pad-none STREAMFLEX_TEST_FRAME_REPORT_MS=2000 UNTIL='Screensaver off' \
    WAIT_FOR='Screensaver on' run_keys_at $P42_SIZE f42-all +wake
log=$out/f42-all.log

# VSync wanted, and claimed by the harness's renderer, which does not deliver it: SDL3's Vulkan
# renderer takes VSync, and lavapipe's swapchain under Xvfb ignores it and presents as fast as it can
# draw. present_frame() finds that out by measurement, says so once, and from then on paces each
# frame to the display's 16 ms itself, so the frames keep about the display's rate. The frames before
# it is found are not paced: at least the run of 60 early presents that finds it (vsyncwatch.h), and
# no more than 240 here.
report=$(grep -o 'Test hook: [0-9]* frames in [0-9]* ms, [0-9]* of them paced' "$log" | head -1)
read -r frames span paced <<< "$(sed 's/Test hook: \([0-9]*\) frames in \([0-9]*\) ms, \([0-9]*\) of them paced/\1 \2 \3/' <<< "$report")"
undelivered=$(grep -c '^Frame timing: the renderer claims VSync but does not deliver it, so each frame is paced to 16 ms$' "$log")
ok=1
grep -qx "Resolution: *$P42_SIZE" "$log" \
    && [ -n "$report" ] && [ "$undelivered" = 1 ] && [ "$paced" -gt 0 ] && [ "$paced" -le $((span / 16 + 2)) ] \
    && [ $((frames - paced)) -le 240 ] && ok=0
premise=
[ $ok = 0 ] || premise=$(p42_premise "$log")
result "with VSync claimed but not delivered, frames are paced to the display's rate, and it is said once (${report:-no frame report}; said $undelivered times)${premise:+; $premise}" $ok
grep '^Resolution:' "$log" | sed 's/^/      /'
stops=$(sed -n '/Quitting program/,$p' "$log")
all_stopped=0
for feature in Clock Screensaver 'Scroll indicators' Highlight Overlay Gamepad; do
    grep -q "^$feature stopped" <<< "$stops" || { all_stopped=1; echo "      no '$feature stopped' after 'Quitting program'"; }
done
ok=1
grep -q 'Overlay started' "$log" && grep -q 'Highlight started' "$log" && grep -q 'Scroll indicators started' "$log" \
    && grep -q 'Clock started' "$log" && grep -q 'Screensaver started' "$log" && grep -q 'Gamepad started' "$log" \
    && grep -q 'Gamepad connected with device index 0' "$log" \
    && grep -q 'Frame timing: VSync wanted on, the renderer gives on' "$log" \
    && [ $all_stopped = 0 ] && ran_clean f42-all && ok=0
result "every feature starts, and all six stop at quit (exit $(cat "$out/f42-all.code"))" $ok
grep -E '(started|stopped|Screensaver o)' "$log" | sed 's/^/      /'

# The virtual pad is attached as the gamepad starts, before it lists the pads present, as a pad
# plugged in before the launcher is: the listing connects it, and its connect event is still
# queued. It is connected, and opened, once.
connects=$(grep -c 'Gamepad connected with device index 0' "$log")
mappings=$(grep -c 'Gamepad Mapping:' "$log")
ok=1
[ "$connects" = 1 ] && [ "$mappings" = 1 ] && ran_clean f42-all && ok=0
result "a pad present when the gamepad starts is connected and opened once (connected $connects times, opened $mappings times)" $ok

# A pad whose device index is not its instance id: attach A, attach B, detach A, attach C, so C
# arrives at B's old device index with an instance id of its own. It is added, not taken for B.
settle() { sleep 1; }
STREAMFLEX_TEST_PAD_SWAP=1 run_after_line f42-padswap 'Test hook: return' +settle
log=$out/f42-padswap.log
c=$(grep -o 'Test hook: pad C attached at device index [0-9]*, instance id [0-9]*' "$log" | head -1)
c_index=$(sed 's/.*device index \([0-9]*\),.*/\1/' <<< "$c")
c_id=${c##* }
ok=1
[ -n "$c" ] && [ "$c_index" != "$c_id" ] && grep -q 'Gamepad disconnected' "$log" \
    && grep -q "Gamepad connected with device index $c_index, instance id $c_id" "$log" \
    && ran_clean f42-padswap && ok=0
result "a pad at another pad's old device index is added by its instance id (exit $(cat "$out/f42-padswap.code"))" $ok
grep -E 'Test hook: pad|Gamepad (connected|disconnected)' "$log" | sed 's/^/      /'

# The same run then launches and returns (pre_launch(), then post_launch()): every pad is closed and
# opened again, each by its instance id at its current device index, although B has moved from
# index 1 to index 0 since it was added
b=$(grep -o 'Test hook: pad B attached at device index [0-9]*, instance id [0-9]*' "$log" | head -1)
b_id=${b##* }
reopened=$(sed -n '/Test hook: return/,$p' "$log")
b_opens=$(grep -c "^Gamepad opened at device index [0-9]*, instance id $b_id\$" <<< "$reopened")
c_opens=$(grep -c "^Gamepad opened at device index [0-9]*, instance id $c_id\$" <<< "$reopened")
ok=1
[ -n "$b" ] && [ -n "$c" ] && [ "$b_opens" = 1 ] && [ "$c_opens" = 1 ] && ran_clean f42-padswap && ok=0
result "after a launch and return, each pad opens again as itself (B opened $b_opens times, C $c_opens times)" $ok
grep -E 'Test hook: (launch|return)|Gamepad (opened|with instance)' "$log" | sed 's/^/      /'

# The clock's render thread fails to start (a test hook; the delay hook makes it render every
# second): each render falls back to the main thread, so the clock keeps rendering
let_it_render() { sleep 4; }
STREAMFLEX_TEST_CLOCK_THREAD_FAIL=1 STREAMFLEX_TEST_CLOCK_DELAY_MS=0 CFG=$FX/f42-all.ini run_keys f42-clockfail +let_it_render
fallbacks=$(grep -c "Could not start the clock's render thread" "$out/f42-clockfail.log")
ok=1
[ "$fallbacks" -ge 2 ] && ran_clean f42-clockfail && ok=0
result "a clock thread that fails to start renders on the main thread, and again next time ($fallbacks fallbacks; exit $(cat "$out/f42-clockfail.code"))" $ok
