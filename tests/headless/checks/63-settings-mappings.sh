# The mappings file (3b): chosen in the folder browser's file mode, saved, and marked as applying at
# next start

rm -rf "$TESTER_HOME/pads"
mkdir -p "$TESTER_HOME/pads"
printf '# no mappings\n' > "$TESTER_HOME/pads/old.txt"
printf '# no mappings either\n' > "$TESTER_HOME/pads/new.txt"
chown -R tester:tester "$TESTER_HOME/pads"

# Controls > Gamepad > Mappings file: the browser opens on old.txt; Up to new.txt, OK chooses it
cfg=$(writable_config f63-mappings)
CFG=$cfg run_keys f63-mappings Menu Down Down Down Down Down Down Down Down Return Down Return Down Down Return Up Return \
    BackSpace BackSpace BackSpace
ok=1
grep -qx 'ControllerMappingsFile=/home/tester/pads/new.txt' "$cfg" \
    && grep -q 'Settings: the note under the preview says This applies at next start' "$out/f63-mappings.log" \
    && grep -q 'Settings: browsing /home/tester/pads' "$out/f63-mappings.log" && ran_clean f63-mappings && ok=0
result "mappings: a file chosen in the browser saves, and applies at next start (exit $(cat "$out/f63-mappings.code"))" $ok
