# The restart prompt (3b, Task 15b): a save that wrote a setting that applies at next start (the
# mappings file) asks "Restart StreamFlex now to apply the mappings file?", with Yes under the
# cursor. Yes restarts StreamFlex in place (exec: the same PID), with no QuitCmd and no StartupCmd
# in the fresh copy; No, or Back, closes settings as before. The harness build's
# STREAMFLEX_TEST_RESTART_SELF stands in for /proc/self/exe, and STREAMFLEX_TEST_FAIL=restart for
# the memory the arguments are kept in, to reach the ways a restart cannot go ahead.

F64_ARROW=$(printf ' \xE2\x80\xBA ')
TO_MAPPINGS="Menu Down Down Down Down Down Down Down Down Return Down Return Down Down Return"   # Controls > Gamepad > Mappings file
F64_SAVE="BackSpace BackSpace BackSpace"                                                       # Gamepad, Controls, then save

# The folder the mappings browser opens in (on old.txt, with new.txt just above it), and the
# folder StartupCmd and QuitCmd each add a line to a file in
rm -rf "$TESTER_HOME/r64" "$TESTER_HOME/r64-marks"
mkdir -p "$TESTER_HOME/r64" "$TESTER_HOME/r64-marks"
printf '# no mappings\n' > "$TESTER_HOME/r64/old.txt"
printf '# no mappings either\n' > "$TESTER_HOME/r64/new.txt"
chown -R tester:tester "$TESTER_HOME/r64" "$TESTER_HOME/r64-marks"

# A function to empty the markers' folder before a run
f64_clear_marks() { rm -f "$TESTER_HOME/r64-marks/startup.txt" "$TESTER_HOME/r64-marks/quit.txt"; }

# A function to count a marker's lines, waiting up to 5 s for the file when WAIT is set: a QuitCmd
# runs through :fork, so its line can come after the launcher has gone
f64_marks() {
    local file=$TESTER_HOME/r64-marks/$1.txt i
    if [ -n "${2:-}" ]; then
        for i in $(seq 25); do [ -s "$file" ] && break; sleep 0.2; done
        sleep 0.5
    fi
    [ -f "$file" ] && wc -l < "$file" || echo 0
}

# A function to wait up to 20 s for the launcher's log to hold N lines holding LINE. The launcher's
# PID runs on through a restart, whose program takes its place.
f64_wait_count() {
    local line=$1 n=$2 pid=$3 i c
    for i in $(seq 100); do
        c=$(grep -cF -- "$line" "$LOG" 2> /dev/null)
        [ "${c:-0}" -ge "$n" ] && return 0
        running "$pid" || break
        sleep 0.2
    done
    c=$(grep -cF -- "$line" "$LOG" 2> /dev/null)
    [ "${c:-0}" -ge "$n" ]
}

# A function for a +key, called with the run's NAME and PID: wait for the Nth start-up's program
# loop, then keep the command line the PID runs now and how many of its files are the log
f64_fresh() {
    local name=$1 pid=$2 n=$3
    if f64_wait_count 'Begin program loop' "$n" "$pid"; then
        tr '\0' ' ' < "/proc/$pid/cmdline" > "$out/$name.cmdline$n"
        # As the launcher's own user: root in the container may not read another user's fds
        setpriv --reuid=tester --regid=tester --init-groups -- find "/proc/$pid/fd" -lname "$LOG" | wc -l \
            > "$out/$name.logfds$n"
    fi
    sleep 1
}
f64_fresh2() { f64_fresh "$1" "$2" 2; }
f64_fresh3() { f64_fresh "$1" "$2" 3; }

# A function to print the lines of a log after its Nth line holding START that hold NEEDLE
f64_after() {
    s=$3 n=$4 awk -v want="$2" 'index($0, ENVIRON["s"]) { seen++ } seen >= want && index($0, ENVIRON["n"])' "$1"
}

# Yes: StreamFlex restarts in place and loads the new mappings file; then a second restart from the
# fresh copy, which already has --restarted, back to the old file
f64_clear_marks
cfg=$(writable_config f64-restart)
CFG=$cfg run_keys f64-yes $TO_MAPPINGS Up Return $F64_SAVE Return +f64_fresh2 \
    $TO_MAPPINGS Down Return $F64_SAVE Return +f64_fresh3
log=$out/f64-yes.log
startups=$(f64_marks startup)
quits=$(f64_marks quit wait)
ok=1
grep -q 'Settings: asking to restart StreamFlex to apply the mappings file$' "$log" \
    && grep -qF "Settings: page Settings${F64_ARROW}Restart?" "$log" \
    && in_range "$log" 'Settings: asking to restart StreamFlex to apply the mappings file' \
        'Restarting StreamFlex to apply the mappings file' "Settings: the cursor's row reads Yes: " && ok=0
result "restart: a save that wrote the mappings file asks to restart, with Yes under the cursor (exit $(cat "$out/f64-yes.code"))" $ok
ok=1
args="-c $cfg -d"
precedes "$log" 'Gamepad mappings loaded from /home/tester/r64/old.txt (' 'Restarting StreamFlex to apply the mappings file' \
    && precedes "$log" 'Restarting StreamFlex to apply the mappings file' 'Gamepad mappings loaded from /home/tester/r64/new.txt (' \
    && grep -q 'Restart: the program is /proc/self/exe$' "$log" \
    && [ "$(cat "$out/f64-yes.cmdline2" 2> /dev/null)" = "/work/build/streamflex $args --restarted " ] \
    && [ "$(cat "$out/f64-yes.logfds2" 2> /dev/null)" = 1 ] && ran_clean f64-yes && ok=0
result "restart: Yes starts StreamFlex again in the same process, which loads the new mappings file (exit $(cat "$out/f64-yes.code"))" $ok
echo "      command line after the restart: $(cat "$out/f64-yes.cmdline2" 2> /dev/null)"
echo "      its files open on the log: $(cat "$out/f64-yes.logfds2" 2> /dev/null), then $(cat "$out/f64-yes.logfds3" 2> /dev/null)"
ok=1
[ "$(cat "$out/f64-yes.cmdline3" 2> /dev/null)" = "/work/build/streamflex $args --restarted " ] \
    && [ "$(cat "$out/f64-yes.logfds3" 2> /dev/null)" = 1 ] \
    && [ "$(grep -c 'Restarting StreamFlex to apply the mappings file' "$log")" = 2 ] \
    && [ "$(grep -c 'Begin program loop' "$log")" = 3 ] \
    && f64_after "$log" 2 'Restarting StreamFlex' 'Gamepad mappings loaded from /home/tester/r64/old.txt (' | grep -q . \
    && grep -qx 'ControllerMappingsFile=/home/tester/r64/old.txt' "$cfg" && ran_clean f64-yes && ok=0
result "restart: a restarted copy restarts again with --restarted once, back to the old mappings file (exit $(cat "$out/f64-yes.code"))" $ok
ok=1
[ "$startups" = 1 ] && [ "$quits" = 1 ] \
    && [ "$(grep -c 'Restarted, so the StartupCmd does not run again' "$log")" = 2 ] && ran_clean f64-yes && ok=0
result "restart: neither restart runs QuitCmd, nor either fresh copy StartupCmd ($startups StartupCmd, $quits QuitCmd)" $ok

# No, after closing with the settings key: settings close as before, saved, with no restart
cfg=$(writable_config f64-restart)
CFG=$cfg UNTIL='Settings closed' run_keys f64-no $TO_MAPPINGS Up Return BackSpace BackSpace Menu Down Return
log=$out/f64-no.log
ok=1
grep -q 'Settings: asking to restart StreamFlex to apply the mappings file$' "$log" \
    && precedes "$log" 'Settings: no restart now, so the mappings file waits for the next start' 'Settings closed' \
    && ! grep -q 'Restarting StreamFlex' "$log" && [ "$(grep -c 'Begin program loop' "$log")" = 1 ] \
    && grep -qx 'ControllerMappingsFile=/home/tester/r64/new.txt' "$cfg" && ran_clean f64-no && ok=0
result "restart: closed with the settings key, No closes settings with no restart (exit $(cat "$out/f64-no.code"))" $ok

# Back on the prompt is No
cfg=$(writable_config f64-restart)
CFG=$cfg UNTIL='Settings closed' run_keys f64-back $TO_MAPPINGS Up Return $F64_SAVE BackSpace
log=$out/f64-back.log
ok=1
grep -q 'Settings: asking to restart StreamFlex to apply the mappings file$' "$log" \
    && precedes "$log" 'Settings: no restart now, so the mappings file waits for the next start' 'Settings closed' \
    && ! grep -q 'Restarting StreamFlex' "$log" && [ "$(grep -c 'Begin program loop' "$log")" = 1 ] \
    && grep -qx 'ControllerMappingsFile=/home/tester/r64/new.txt' "$cfg" && ran_clean f64-back && ok=0
result "restart: Back on the prompt closes settings with no restart (exit $(cat "$out/f64-back.code"))" $ok

# A save with no setting that waits for the next start (Wrap around) asks nothing
cfg=$(writable_config f64-restart)
CFG=$cfg UNTIL='Settings closed' run_keys f64-plain Menu Return Down Right BackSpace BackSpace
log=$out/f64-plain.log
ok=1
grep -q 'Settings saved 1 change(s)' "$log" && grep -qx 'WrapEntries=true' "$cfg" \
    && ! grep -q 'Settings: asking to restart' "$log" && ! grep -q 'Restart?' "$log" \
    && grep -q 'Settings closed' "$log" && ran_clean f64-plain && ok=0
result "restart: a save with no setting for the next start asks nothing (exit $(cat "$out/f64-plain.code"))" $ok

# /proc/self/exe missing: the restart goes by argv[0], a path here
cfg=$(writable_config f64-restart)
STREAMFLEX_TEST_RESTART_SELF=/nonexistent CFG=$cfg run_keys f64-argv $TO_MAPPINGS Up Return $F64_SAVE Return +f64_fresh2
log=$out/f64-argv.log
ok=1
grep -q 'Restart: the program is /work/build/streamflex$' "$log" \
    && precedes "$log" 'Restarting StreamFlex to apply the mappings file' 'Gamepad mappings loaded from /home/tester/r64/new.txt (' \
    && [ "$(cat "$out/f64-argv.cmdline2" 2> /dev/null)" = "/work/build/streamflex -c $cfg -d --restarted " ] \
    && ran_clean f64-argv && ok=0
result "restart: with no /proc/self/exe it restarts by argv[0]'s path (exit $(cat "$out/f64-argv.code"))" $ok

# /proc/self/exe missing and argv[0] a bare name (StreamFlex started through the PATH): the
# restart finds it in the PATH, past a folder and a file it may not run that have its name
mkdir -p "$TESTER_HOME/r64-bin" "$TESTER_HOME/r64-dir/streamflex" "$TESTER_HOME/r64-noexec"
ln -sf /work/build/streamflex "$TESTER_HOME/r64-bin/streamflex"
printf '#!/bin/sh\n' > "$TESTER_HOME/r64-noexec/streamflex"
chmod 644 "$TESTER_HOME/r64-noexec/streamflex"
chown -R tester:tester "$TESTER_HOME/r64-bin" "$TESTER_HOME/r64-dir" "$TESTER_HOME/r64-noexec"
F64_PATH=$TESTER_HOME/r64-dir:$TESTER_HOME/r64-noexec:$TESTER_HOME/r64-bin:$PATH
cfg=$(writable_config f64-restart)
exe=streamflex PATH=$F64_PATH STREAMFLEX_TEST_RESTART_SELF=/nonexistent CFG=$cfg \
    run_keys f64-path $TO_MAPPINGS Up Return $F64_SAVE Return +f64_fresh2
log=$out/f64-path.log
ok=1
grep -q 'Restart: the program is /home/tester/r64-bin/streamflex$' "$log" \
    && precedes "$log" 'Restarting StreamFlex to apply the mappings file' 'Gamepad mappings loaded from /home/tester/r64/new.txt (' \
    && [ "$(cat "$out/f64-path.cmdline2" 2> /dev/null)" = "streamflex -c $cfg -d --restarted " ] \
    && ran_clean f64-path && ok=0
result "restart: with no /proc/self/exe and a bare argv[0], it restarts by the PATH (exit $(cat "$out/f64-path.code"))" $ok

# The same with the PATH's copy gone before Yes: nowhere to restart from, so the reason is logged,
# nothing is torn down, and settings close with StreamFlex running on
f64_unlink() { rm -f "$TESTER_HOME/r64-bin/streamflex"; }
ln -sf /work/build/streamflex "$TESTER_HOME/r64-bin/streamflex"
cfg=$(writable_config f64-restart)
exe=streamflex PATH=$F64_PATH STREAMFLEX_TEST_RESTART_SELF=/nonexistent CFG=$cfg UNTIL='Settings closed' \
    run_keys f64-lost $TO_MAPPINGS Up Return $F64_SAVE +f64_unlink Return
log=$out/f64-lost.log
ok=1
precedes "$log" 'Cannot restart StreamFlex: the program is neither at /nonexistent nor found as streamflex' 'Settings closed' \
    && ! grep -q 'Restarting StreamFlex' "$log" && [ "$(grep -c 'Begin program loop' "$log")" = 1 ] \
    && ran_clean f64-lost && ok=0
result "restart: a program that cannot be found is logged, and StreamFlex runs on (exit $(cat "$out/f64-lost.code"))" $ok
grep -E 'restart' "$log" | sed 's/^/      /'

# A program found that will not start (a file that is not one): after the teardown, the reason is
# logged and StreamFlex ends with an error, with no QuitCmd
printf 'not a program\n' > "$TESTER_HOME/r64-notaprogram"
chmod 755 "$TESTER_HOME/r64-notaprogram"
f64_clear_marks
cfg=$(writable_config f64-restart)
STREAMFLEX_TEST_RESTART_SELF=$TESTER_HOME/r64-notaprogram CFG=$cfg UNTIL='Could not restart StreamFlex' \
    run_keys f64-exec $TO_MAPPINGS Up Return $F64_SAVE Return
log=$out/f64-exec.log
sleep 1
ok=1
grep -q 'Restart: the program is /home/tester/r64-notaprogram$' "$log" \
    && precedes "$log" 'Restarting StreamFlex to apply the mappings file' \
        'Could not restart StreamFlex: /home/tester/r64-notaprogram did not start: Exec format error' \
    && [ "$(grep -c 'Begin program loop' "$log")" = 1 ] && [ "$(f64_marks quit)" = 0 ] \
    && ran_clean f64-exec 1 && ok=0
result "restart: a program that will not start is logged after the teardown, and StreamFlex exits 1 (exit $(cat "$out/f64-exec.code"))" $ok

# No memory to keep the arguments in when StreamFlex started: the restart says so and goes no further
cfg=$(writable_config f64-restart)
STREAMFLEX_TEST_FAIL=restart CFG=$cfg UNTIL='Settings closed' run_keys f64-oom $TO_MAPPINGS Up Return $F64_SAVE Return
log=$out/f64-oom.log
ok=1
precedes "$log" 'Cannot restart StreamFlex: there was no memory to keep its arguments when it started' 'Settings closed' \
    && ! grep -q 'Restarting StreamFlex' "$log" && ran_clean f64-oom && ok=0
result "restart: with no memory for its arguments, the restart is logged and goes no further (exit $(cat "$out/f64-oom.code"))" $ok

# --help lists the internal flag
"${TESTER[@]}" "$exe" --help > "$out/f64-help.out" 2> "$out/f64-help.err"
echo $? > "$out/f64-help.code"
ok=1
grep -qF -- '--restarted  Internal: StreamFlex restarting itself, which runs no StartupCmd.' "$out/f64-help.out" \
    && ran_clean f64-help && ok=0
result "restart: --help names --restarted as internal (exit $(cat "$out/f64-help.code"))" $ok
