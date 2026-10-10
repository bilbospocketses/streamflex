# The harness's own helpers (run.sh), checked before anything relies on them: a launcher that
# starts slowly still gets its keys, the frame line each key waits for follows that frame's own
# lines, a launcher that ignores TERM is killed and fails its check, one that exits without
# logging does not hold the run up, a zombie is not running whatever its name, a quick run that has to be stopped fails, a log range must be closed to count, the log
# helpers take their strings as written, a pixel probe must have points to read, the checks run
# with nullglob off, and a listed leak is a failure

# Every shard runs this file (run.sh <label> [leaks] K/N): a shard is a run of its own, so it
# proves the helpers it relies on itself.

# Stand-in launchers, run as the test user as the real one is
mkdir -p /tmp/harness
cat > /tmp/harness/slow-start << EOF
#!/bin/sh
sleep 6
exec /work/build/streamflex "\$@"
EOF
cat > /tmp/harness/ignores-term << EOF
#!/bin/sh
mkdir -p "$(dirname "$LOG")"
echo "Loading menu 'Stand-in'" >> "$LOG"
trap '' TERM
exec sleep 60
EOF
cat > /tmp/harness/quits-on-term << EOF
#!/bin/sh
# The leak pass preloads ASan into what it runs. This shell is not the launcher, and bash (sh on
# some systems) leaks when a trap ends it, so it starts again without the preload.
[ -z "\${LD_PRELOAD:-}" ] || exec env -u LD_PRELOAD "\$0" "\$@"
trap 'exit 0' TERM
while :; do sleep 0.2; done
EOF
printf '#!/bin/sh\nexit 0\n' > /tmp/harness/exits-at-once
printf '#!/bin/sh\nuntil [ -e /tmp/harness/cue ]; do sleep 0.1; done\n' > /tmp/harness/exits-on-cue
cp /tmp/harness/exits-on-cue '/tmp/harness/exits on cue'
chmod 755 /tmp/harness/*

# A launcher that takes 6 s to start: the keys wait for its first menu, so Back still comes
# after Games has opened twice (the keys of item 13 in 10-grid.sh)
CFG=$FX/f13-selfsub.ini exe=/tmp/harness/slow-start run_keys h-slow Return Return BackSpace
ok=1
[ "$(grep -o "Loading menu '[^']*'" "$out/h-slow.log" | tail -1)" = "Loading menu 'Main'" ] \
    && [ "$(grep -c "Loading menu 'Games'" "$out/h-slow.log")" = 2 ] && ran_clean h-slow && ok=0
result "harness: a launcher that starts 6 s late still gets its keys (exit $(cat "$out/h-slow.code"))" $ok

# The line run_keys waits for after each key comes after the frame it names, not as the key is
# handled: Menu and each Down move settings' cursor to a new row, and the first frame after the key
# logs that row, so each of those keys' lines must have a cursor-row line, and then a frame line,
# before the next key. A hook logged ahead of its frame would let a slow launcher take the next key
# before the frame, and every other check would still pass.
CFG=$FX/f13-selfsub.ini run_keys h-order Menu Down Down Down BackSpace
ok=1
awk '
    /^Key (Menu|Down) \(#[0-9A-F]+\) detected$/ { if (open) bad = 1; open = 1; row = 0; keys++; next }
    /^Key / { if (open) bad = 1; open = 0; next }
    open && /^Settings: the cursor.s row reads / { row = 1 }
    open && /^Test hook: a frame was drawn after a key$/ { if (!row) bad = 1; open = 0; drawn++ }
    END { exit !(keys == 4 && drawn == 4 && !bad) }
' "$out/h-order.log" && ran_clean h-order && ok=0
result "harness: the frame line each key waits for follows the lines that frame drew (exit $(cat "$out/h-order.code"))" $ok
grep -E "^Key |the cursor's row reads|^Test hook: a frame was drawn" "$out/h-order.log" | sed 's/^/      /'

# A launcher that ignores TERM is killed 10 s later (exit 137), and its check fails on the exit
# code alone: its sanitizers are quiet
CFG=none exe=/tmp/harness/ignores-term run_keys h-kill
ok=1
[ "$(cat "$out/h-kill.code")" = 137 ] && sanitizer_clean h-kill && ! ran_clean h-kill && ok=0
result "harness: a launcher that ignores TERM is killed, and fails its check (exit $(cat "$out/h-kill.code"))" $ok

# A launcher that exits without logging its first menu: the wait ends when it does, not 20 s
# later, and the missing line fails the check though it exited 0
SECONDS=0
CFG=none exe=/tmp/harness/exits-at-once run_keys h-gone
took=$SECONDS
ok=1
[ "$took" -lt 5 ] && ! ran_clean h-gone && grep -q "never logged 'Loading menu'" "$out/h-gone.code" && ok=0
result "harness: a launcher that exits without logging ends the wait at once, and fails (exit $(cat "$out/h-gone.code"))" $ok
echo "      the wait took $took s"

# A zombie (a process that has exited but not been waited for) is not running, also when its
# name has a space: /proc/PID/stat shows the name in brackets, before the state. The stand-ins
# are children of a sleep, which never waits for them, so they stay zombies until it ends; they
# exit on a cue given once their parent has become the sleep.
rm -f /tmp/harness/cue /tmp/harness/plain.pid /tmp/harness/spaced.pid
bash -c '/tmp/harness/exits-on-cue & echo $! > /tmp/harness/plain.pid
         "/tmp/harness/exits on cue" & echo $! > /tmp/harness/spaced.pid
         exec sleep 10' &
zombies=$!
for i in $(seq 25); do [ "$(cat "/proc/$zombies/comm" 2> /dev/null)" = sleep ] && break; sleep 0.2; done
: > /tmp/harness/cue
for i in $(seq 25); do
    grep -qs ') Z ' "/proc/$(cat /tmp/harness/plain.pid)/stat" \
        && grep -qs '(exits on cue) Z ' "/proc/$(cat /tmp/harness/spaced.pid)/stat" && break
    sleep 0.2
done
ok=1
grep -qs ') Z ' "/proc/$(cat /tmp/harness/plain.pid)/stat" \
    && grep -qs '(exits on cue) Z ' "/proc/$(cat /tmp/harness/spaced.pid)/stat" \
    && ! running "$(cat /tmp/harness/plain.pid)" && ! running "$(cat /tmp/harness/spaced.pid)" && ok=0
result "harness: a zombie is not running, with a space in its name or without" $ok
kill "$zombies"; wait "$zombies" 2> /dev/null

# A quick run (a StartupCmd that quits) that has to be stopped fails its check, though it exits
# 0 on TERM as the launcher does; one that quits by itself passes. QUICK_LIMIT shortens the 30 s.
QUICK_LIMIT=2 CFG=none exe=/tmp/harness/quits-on-term run_quick h-hang
CFG=none exe=/tmp/harness/exits-at-once run_quick h-quits
ok=1
[ "$(cat "$out/h-hang.code")" = "0, and did not quit by itself within 2 s" ] && ! ran_clean h-hang \
    && ran_clean h-quits && ok=0
result "harness: a quick run that has to be stopped fails, one that quits passes (exit $(cat "$out/h-hang.code"))" $ok

# A log range counts only once its end line comes: a launcher that stopped logging inside
# settings cannot pass a check on the lines inside them
printf 'Settings opened\nScreensaver off\n' > "$out/h-range-open.log"
printf 'Settings opened\nScreensaver off\nSettings closed\n' > "$out/h-range-closed.log"
ok=1
! in_range "$out/h-range-open.log" 'Settings opened' 'Settings closed' 'Screensaver off' \
    && in_range "$out/h-range-closed.log" 'Settings opened' 'Settings closed' 'Screensaver off' && ok=0
result "harness: a log range with no end line does not count" $ok

# The log helpers match their strings as written, backslashes included, and precedes tells the
# order of two lines
printf 'Settings opened\nImage=C:\\temp\\new.png\nSettings closed\n' > "$out/h-strings.log"
ok=1
in_range "$out/h-strings.log" 'Settings opened' 'Settings closed' 'C:\temp\new.png' \
    && precedes "$out/h-strings.log" 'C:\temp\new.png' 'Settings closed' \
    && ! precedes "$out/h-strings.log" 'Settings closed' 'C:\temp\new.png' && ok=0
result "harness: the log helpers match a backslash as written, and precedes tells the order" $ok

# A pixel probe says "no" when it has no points to read (any screen would pass it) and when the
# log never said where the preview is (it would read the screen's corner)
printf 'Settings opened\n' > "$out/h-look.log"
rm -f "$out/h-look.seen" "$out/h-look.pixels"
LOG=$out/h-look.log look h-look $$ nothing 'Settings opened'
LOG=$out/h-look.log look h-look $$ unplaced 'Settings opened' 30,30=0,0,0
ok=1
grep -qx 'nothing no' "$out/h-look.seen" && grep -qx 'unplaced no' "$out/h-look.seen" && ok=0
result "harness: a pixel probe with no points, or no preview to place them in, says no" $ok
sed 's/^/      /' "$out/h-look.seen"

# The check files run with bash's own glob rules. run.sh turns nullglob on only to find them
# and the .err files; left on, a pattern matching nothing would vanish, and `grep x "$d"/*.log`
# over an empty folder would read its input instead of failing.
ok=1
shopt -q nullglob || ok=0
result "harness: the checks run with nullglob off" $ok

# Each leak the leak pass lists counts as a failure, so the listing and the verdict cannot
# disagree, even for a run whose check never looked at its sanitizers. A folder with no .err
# files lists nothing, and says nothing about a missing *.err.
mkdir -p /tmp/harness/leaky /tmp/harness/no-errs
printf '==1==ERROR: LeakSanitizer: detected memory leaks\nSUMMARY: AddressSanitizer: 8 byte(s) leaked in 1 allocation(s).\n' \
    > /tmp/harness/leaky/h-leak.err
printf 'nothing leaked\n' > /tmp/harness/leaky/h-clean.err
real_out=$out saved=$failures
failures=0
out=/tmp/harness/leaky list_leaks > "$real_out/h-leaks.txt" 2>&1
out=/tmp/harness/no-errs list_leaks > "$real_out/h-no-errs.txt" 2>&1
counted=$failures
failures=$saved
ok=1
[ "$counted" = 1 ] && [ ! -s "$out/h-no-errs.txt" ] \
    && [ "$(cat "$out/h-leaks.txt")" = "LEAK  h-leak: SUMMARY: AddressSanitizer: 8 byte(s) leaked in 1 allocation(s)." ] \
    && ok=0
result "harness: a listed leak counts as a failure (counted $counted), and no .err files list nothing" $ok
sed 's/^/      /' "$out/h-leaks.txt" "$out/h-no-errs.txt"
