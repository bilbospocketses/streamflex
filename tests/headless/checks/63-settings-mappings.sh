# The mappings file (3b): chosen in the folder browser's file mode, saved, and marked as applying at
# next start

# a.txt sorts first among the files and pics/ before them all, so a browser that did not open with
# old.txt highlighted would not reach new.txt with one Up. pics/ holds an image, which file mode
# must not preview.
rm -rf "$TESTER_HOME/pads"
mkdir -p "$TESTER_HOME/pads/pics"
printf '# no mappings\n' > "$TESTER_HOME/pads/old.txt"
printf '# no mappings either\n' > "$TESTER_HOME/pads/new.txt"
printf '# nor here\n' > "$TESTER_HOME/pads/a.txt"
cp "$TESTER_HOME/Pictures/red.png" "$TESTER_HOME/pads/pics/"
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

# Moving onto a folder in file mode previews nothing: Up three times to pics/, then OK enters it, which
# proves the cursor was there, a second after it arrived
cfg=$(writable_config f63-mappings)
CFG=$cfg run_keys f63-folder Menu Down Down Down Down Down Down Down Down Return Down Return Down Down Return Up Up Up \
    Return Menu
ok=1
grep -q 'Settings: browsing /home/tester/pads/pics' "$out/f63-folder.log" \
    && ! grep -q 'Settings: the preview shows' "$out/f63-folder.log" \
    && grep -q 'Settings: nothing changed' "$out/f63-folder.log" && ran_clean f63-folder && ok=0
result "mappings: a folder highlighted in file mode previews nothing (exit $(cat "$out/f63-folder.code"))" $ok
grep -E 'Settings: (browsing|the preview shows)' "$out/f63-folder.log" | sed 's/^/      /'

# An image that failed to decode in the Image browser (~/mixed/broken.png, highlighted as it opens)
# can still be chosen as the mappings file in the same settings session: only an image must open
cfg=$(writable_config f63-broken)
CFG=$cfg run_keys f63-broken Menu Down Return Down Return Down Return BackSpace \
    Down Down Down Down Down Down Down Return Down Return Down Down Return Up Return BackSpace BackSpace BackSpace
ok=1
grep -q 'Settings: the caption says This image cannot be opened for /home/tester/mixed/broken.png' "$out/f63-broken.log" \
    && grep -qx 'Image=/home/tester/mixed/red.png' "$cfg" \
    && grep -qx 'ControllerMappingsFile=/home/tester/mixed/broken.png' "$cfg" \
    && ! grep -q 'Settings: This image cannot be opened: ' "$out/f63-broken.log" && ran_clean f63-broken && ok=0
result "mappings: an image that would not decode can still be chosen as the mappings file (exit $(cat "$out/f63-broken.code"))" $ok
grep -E 'Settings: (browsing|chose|This image|the caption)' "$out/f63-broken.log" | sed 's/^/      /'
