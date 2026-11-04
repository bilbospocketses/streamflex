# The clock's automatic formats read the region from LANG (get_region(), platform/unix.c). With LANG
# unset it must not read another caller's string (Fedora's Mesa had freed the one it went on from),
# and with LANG set it must leave the process's own LANG whole for what the launcher starts.

# LANG unset, the clock on
( unset LANG; run_quick f46-clock )
ok=1
ran_clean f46-clock && grep -A2 'Clock ===' "$out/f46-clock.log" | grep -qE 'Enabled:\s+true$' && ok=0
result "the clock starts with LANG unset (exit $(cat "$out/f46-clock.code"))" $ok
grep -m3 -E 'AddressSanitizer|runtime error' "$out/f46-clock.err" | sed 's/^/      /'

# LANG set, the clock on: the startup command writes the LANG it was given to ~/lang.txt
lang_written() { wait_for 'the startup command to write ~/lang.txt' "$2" test -s "$TESTER_HOME/lang.txt"; }
rm -f "$TESTER_HOME/lang.txt"
LANG=en_US.UTF-8 run_keys f46-lang +lang_written
ok=1
[ "$(cat "$TESTER_HOME/lang.txt" 2> /dev/null)" = en_US.UTF-8 ] && ran_clean f46-lang && ok=0
result "reading the region leaves LANG whole for what the launcher starts (it gave '$(cat "$TESTER_HOME/lang.txt" 2> /dev/null)'; exit $(cat "$out/f46-lang.code"))" $ok
