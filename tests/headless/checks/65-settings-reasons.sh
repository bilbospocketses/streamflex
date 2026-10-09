# The settings screen's side of the reasons the pure modules give: the folder browser leaves a place
# on a network share closed until asked, refuses a path its key's line cannot hold beside the line's
# comment, and says why when it cannot open or cannot go into a folder; a save that could not keep
# the file's permissions says so. STREAMFLEX_TEST_FAIL (the harness's build only, see test_fail())
# makes one step fail as no real run can: out of memory, or the permissions not kept.

# A Pictures folder on a network share (anything in /mnt counts) is not opened unasked: with no
# image chosen yet, the browser starts in Home instead
mkdir -p /mnt/nas
cp "$TESTER_HOME/Pictures/red.png" /mnt/nas/
chmod -R a+rX /mnt/nas
XDG_PICTURES_DIR=/mnt/nas CFG=$FX/f60-color.ini run_keys f65-network Menu Down Return Right Down Return Menu
ok=1
grep -qE 'Settings: browsing /home/tester$' "$out/f65-network.log" && ! grep -q 'Settings: browsing /mnt' "$out/f65-network.log" \
    && grep -q 'Settings: nothing changed' "$out/f65-network.log" && ran_clean f65-network && ok=0
result "settings: the browser does not open a Pictures folder on a network share unasked (exit $(cat "$out/f65-network.code"))" $ok
grep 'Settings: browsing' "$out/f65-network.log" | sed 's/^/      /'

# A picture whose path fits on a line of its own, but not in place of the one on Image's line,
# which keeps a comment: the browser refuses it with the save's own reason, before it can be chosen
long=$TESTER_HOME/$(printf 'folder%.0s' $(seq 17))
wide=b$(printf 'x%.0s' $(seq 31)).png
comment='; chosen in the settings screen, which keeps this comment here'
mkdir -p "$long" "$TESTER_HOME/cfg"
cp "$TESTER_HOME/Pictures/red.png" "$long/a.png"
cp "$TESTER_HOME/Pictures/green.png" "$long/$wide"
cfg=$TESTER_HOME/cfg/f65-comment.ini
printf '[General]\nDefaultMenu=Main\n\n[Background]\nMode=Image\nImage=%s %s\n\n[Main]\nEntry1=One;apps;:quit\n' \
    "$long/a.png" "$comment" > "$cfg"
cp "$cfg" /tmp/f65-comment.ini
chown -R tester:tester "$long" "$TESTER_HOME/cfg"
fits=$(( ${#long} + 1 + ${#wide} + 6 ))                        # Image=<path>
with=$(( fits + 1 + ${#comment} ))                               # Image=<path> <comment>
now=$(( ${#long} + 6 + 6 + 1 + ${#comment} ))                    # Image=<long>/a.png <comment>
CFG=$cfg run_keys f65-comment Menu Down Return Down Return Down Return Menu
ok=1
[ "$fits" -le 199 ] && [ "$with" -gt 199 ] && [ "$now" -le 199 ] \
    && grep -qF 'Settings: it is too long for one line of config.ini (199 bytes at most) with its comment' "$out/f65-comment.log" \
    && ! grep -q 'Settings: chose' "$out/f65-comment.log" && grep -q 'Settings: nothing changed' "$out/f65-comment.log" \
    && cmp -s /tmp/f65-comment.ini "$cfg" && ran_clean f65-comment && ok=0
result "settings: a path too long for its line beside the line's comment is refused in the browser (lines of $now, $fits and $with bytes; exit $(cat "$out/f65-comment.code"))" $ok
grep -E 'Settings: (browsing|it is|chose|Couldn)' "$out/f65-comment.log" | sed 's/^/      /'

# The browser's places run out of memory: it says so and opens at the image all the same
STREAMFLEX_TEST_FAIL=places CFG=$FX/f60-broken.ini run_keys f65-noplaces Menu Down Return Down Return Menu
ok=1
grep -q 'Settings: the folder browser has no places: out of memory' "$out/f65-noplaces.log" \
    && grep -q 'Settings: browsing /home/tester/broken' "$out/f65-noplaces.log" \
    && grep -q 'Settings: nothing changed' "$out/f65-noplaces.log" && ran_clean f65-noplaces && ok=0
result "settings: a browser whose places run out of memory says so and still opens (exit $(cat "$out/f65-noplaces.code"))" $ok

# The browser itself runs out of memory as it opens: the log says why, and settings carry on
STREAMFLEX_TEST_FAIL=browser CFG=$FX/f60-broken.ini run_keys f65-nobrowser Menu Down Return Down Return Menu
ok=1
grep -q 'Settings: the folder browser cannot open: out of memory' "$out/f65-nobrowser.log" \
    && ! grep -q 'Settings: browsing' "$out/f65-nobrowser.log" \
    && grep -q 'Settings: nothing changed' "$out/f65-nobrowser.log" && ran_clean f65-nobrowser && ok=0
result "settings: a browser that cannot open says why (exit $(cat "$out/f65-nobrowser.code"))" $ok

# OK on a folder runs out of memory in the browser: the note gives the browser's own reason
rm -rf "$TESTER_HOME/folders"
mkdir -p "$TESTER_HOME/folders/inner"
chown -R tester:tester "$TESTER_HOME/folders"
STREAMFLEX_TEST_FAIL=command run_keys f65-command Menu Down Return Down Return Down Return Menu
ok=1
grep -q "Settings: Can't open inner: out of memory" "$out/f65-command.log" \
    && grep -q 'Settings: nothing changed' "$out/f65-command.log" && ran_clean f65-command && ok=0
result "settings: a folder the browser runs out of memory opening says so (exit $(cat "$out/f65-command.code"))" $ok
grep -E "Settings: (browsing|Can't)" "$out/f65-command.log" | sed 's/^/      /'

# A save that cannot keep the file's permissions succeeds, and the log says what was lost
cfg=$(writable_config f60-color)
STREAMFLEX_TEST_FAIL=keep CFG=$cfg run_keys f65-keep Menu Down Return Down Right BackSpace BackSpace
ok=1
grep -qx 'Color=#1E1E1E' "$cfg" \
    && grep -qF "Settings saved to $cfg, but the file's permissions could not be kept" "$out/f65-keep.log" \
    && ran_clean f65-keep && ok=0
result "settings: a save that could not keep the file's permissions says so (exit $(cat "$out/f65-keep.code"))" $ok
