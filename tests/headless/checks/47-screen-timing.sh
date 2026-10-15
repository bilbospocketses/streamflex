# The screen's timing-sensitive case, from 47-screen.sh: a size change held back by a launch the
# launcher gives up on when its ApplicationTimeout runs out
# Timing-sensitive: the size change must land inside the launch's 3 s timeout

# An app that never takes the screen (sleep 6, which opens no window: 50-settings-timing.sh's launch)
# is given up on after its ApplicationTimeout of 3 s; a size change it held back is read again then,
# and reloads. The test hook (STREAMFLEX_TEST_PIXEL_SIZE) forces the size: whether SDL itself reports
# a resize is 47-screen.sh's question, and SDL's event would take this case no further.
T47_SIZE=/tmp/t47-size
t47_to_720_later() {
    echo 1280x720 > "$T47_SIZE"
    wait_line 'Screen: 1280x720 px, was 1920x1080; reloading when the app is done' "$2"
}
t47_at() { grep -nxF -- "$2" "$out/$1.log" | head -1 | cut -d: -f1; }   # The line number of an exact line; empty when absent
rm -f "$T47_SIZE"
CFG=$FX/f50-launch.ini STREAMFLEX_TEST_PIXEL_SIZE=$T47_SIZE UNTIL='Reload: done at 1280x720 px' run_keys f47-timeout Return +t47_to_720_later
t47_waits=$(t47_at f47-timeout 'Screen: 1280x720 px, was 1920x1080; reloading when the app is done')
t47_again=$(t47_at f47-timeout 'Screen: 1280x720 px, was 1920x1080; reloading')
ok=1
[ -n "$t47_waits" ] && [ -n "$t47_again" ] && [ "$t47_waits" -lt "$t47_again" ] \
    && grep -qx "Reload: done at 1280x720 px, for the window's size" "$out/f47-timeout.log" && ran_clean f47-timeout && ok=0
result "screen: a size change held back by a launch that timed out reloads then (exit $(cat "$out/f47-timeout.code"))" $ok
grep -E '^(Screen|Reload|Test hook: the window)' "$out/f47-timeout.log" | sed 's/^/      /'
