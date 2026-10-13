# The settings pages' timing-sensitive case, from 55-settings-pages.sh: the clock switched off while
# a render is in flight
# Timing-sensitive (run.sh <label> [leaks] [K/N] timing): the key must land inside a render the
# harness build holds for 3 s, which a loaded box could move it past.

# The clock switched off while its render thread is at work (the harness build's
# STREAMFLEX_TEST_CLOCK_DELAY_MS makes each render take 3 s, and one every second): stopping waits
# for the thread and frees what it made, and Enabled=false is saved. The wait is looked for while
# settings are open: quitting with the clock still running logs the same line.
render_then_off() {
    local before i
    before=$(grep -c 'Clock: rendering on its thread' "$LOG")
    for i in $(seq 50); do
        [ "$(grep -c 'Clock: rendering on its thread' "$LOG")" -gt "$before" ] && break
        sleep 0.2
    done
    xdotool key Left
    sleep 4
}
cfg=$(writable_config f55-clock)
STREAMFLEX_TEST_CLOCK_DELAY_MS=3000 CFG=$cfg UNTIL='Settings saved' \
    run_keys f55-clock Menu Down Down Down Down Down Down Return +render_then_off BackSpace BackSpace
ok=1
in_range "$out/f55-clock.log" 'Settings opened' 'Settings closed' 'Clock stopped (it waited for a render in progress)' \
    && sed -n '/^\[Clock\]/,/^\[/p' "$cfg" | grep -qx 'Enabled=false' && ran_clean f55-clock && ok=0
result "settings: the clock switched off mid-render waits for it and frees it (exit $(cat "$out/f55-clock.code"))" $ok
