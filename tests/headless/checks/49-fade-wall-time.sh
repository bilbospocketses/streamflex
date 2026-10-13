# The slideshow's fade and the screensaver's dim run in wall time, whatever the frame rate: each ends
# its time after it began, to the frame. Counted in frames, as they were, each frame added the share of
# the fade that a 16 ms display frame holds, so a slow machine stretched them: at 25 fps the 3 s fade
# took about 7.5 s and the 1.5 s dim about 3.75 s. Here each frame is held up 40 ms
# (STREAMFLEX_TEST_FRAME_SLEEP_MS), as in check 48, and the test hook logs when each began and ended.
# Each must end no sooner than its time, and no later than one frame (from the run's frame report)
# and a 200 ms margin for a frame the runner stalls. Each run's frame report must show 26 frames a
# second or fewer, so the low rate is real.
# Timing-sensitive (run.sh <label> [leaks] [K/N] timing): the fade's and the dim's durations, and the
# frame rate, are what it reads, and a loaded box stretches them.
# A function to give a run's frame time in whole ms, rounded up, from its frame report; nothing when
# there is no report, or it shows more than 26 frames a second
p49_frame() {
    local report frames span
    report=$(grep -o 'Test hook: [0-9]* frames in [0-9]* ms' "$out/$1.log" | head -1)
    read -r frames span <<< "$(sed 's/Test hook: \([0-9]*\) frames in \([0-9]*\) ms/\1 \2/' <<< "$report")"
    [ -n "$report" ] && [ "$frames" -gt 0 ] && [ $((frames * 1000)) -le $((26 * span)) ] \
        && echo $(((span + frames - 1) / frames))
}
# A function to give how long a run's fade or dim (WHAT: "slideshow's fade", "screensaver's dim") took,
# in ms, from the test hook's line as it ended; nothing when it never ended
p49_took() { sed -n "s/^Test hook: the $2 ended at tick [0-9]*, \([0-9]*\) ms after it began\$/\1/p" "$out/$1.log" | head -1; }
# A function to tell whether a run's WHAT took from TIME ms to TIME ms plus a frame and 200 ms
p49_on_time() {
    local frame took
    frame=$(p49_frame "$1") took=$(p49_took "$1" "$2")
    [ -n "$frame" ] && [ -n "$took" ] && [ "$took" -ge "$3" ] && [ "$took" -le $(($3 + frame + 200)) ]
}

# The slideshow: f60-running.ini changes its picture 5 s in, with a 3 s fade
STREAMFLEX_TEST_FRAME_SLEEP_MS=40 STREAMFLEX_TEST_FRAME_REPORT_MS=2000 \
    UNTIL="Test hook: the slideshow's fade ended" CFG=$FX/f60-running.ini run_keys f49-slowfade
ok=1
p49_on_time f49-slowfade "slideshow's fade" 3000 && grep -q "Test hook: the slideshow's fade began at tick [0-9]*, to take 3000 ms" "$out/f49-slowfade.log" \
    && ran_clean f49-slowfade && ok=0
result "fade: at 25 fps a 3 s slideshow fade ends 3 s after it began, within a frame (took $(p49_took f49-slowfade "slideshow's fade") ms, frames of $(p49_frame f49-slowfade) ms; exit $(cat "$out/f49-slowfade.code"))" $ok
grep -E "Test hook: ([0-9]* frames in|the slideshow's fade)" "$out/f49-slowfade.log" | sed 's/^/      /'

# The screensaver: f50-screensaver.ini starts it after 3 s idle, and it dims over 1.5 s
STREAMFLEX_TEST_FRAME_SLEEP_MS=40 STREAMFLEX_TEST_FRAME_REPORT_MS=2000 \
    UNTIL="Test hook: the screensaver's dim ended" CFG=$FX/f50-screensaver.ini run_keys f49-slowdim
ok=1
p49_on_time f49-slowdim "screensaver's dim" 1500 && grep -q "Test hook: the screensaver's dim began at tick [0-9]*, to take 1500 ms" "$out/f49-slowdim.log" \
    && ran_clean f49-slowdim && ok=0
result "dim: at 25 fps the screensaver's 1.5 s dim ends 1.5 s after it began, within a frame (took $(p49_took f49-slowdim "screensaver's dim") ms, frames of $(p49_frame f49-slowdim) ms; exit $(cat "$out/f49-slowdim.code"))" $ok
grep -E "Test hook: ([0-9]* frames in|the screensaver's dim)" "$out/f49-slowdim.log" | sed 's/^/      /'
