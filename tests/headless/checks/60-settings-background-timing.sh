# The Background page's timing-sensitive cases, from 60-settings-background.sh: OK during a decode the
# harness build holds for 4 s, and a mode stepped inside a slideshow's 3 s fade. f60-slideshow's run is
# not timing-sensitive; it is here because the Folder row's result reads its log beside f60-running's.
# Timing-sensitive: a key must land inside a 4 s decode or a 3 s fade

# OK while the highlighted image is still decoding: the test hook (STREAMFLEX_TEST_DECODE_DELAY_MS,
# in the harness's build only) makes each decode take 4 s, so OK lands while blue.png, the image
# highlighted first, is still decoding and green.png waits behind it. OK waits for both, and
# chooses green, the one highlighted, only once green's decode is done.
cfg=$(writable_config f60-color)
STREAMFLEX_TEST_DECODE_DELAY_MS=4000 CFG=$cfg UNTIL='Settings saved' \
    run_keys f60-slowdecode Menu Down Return Right Down Return Down Return BackSpace BackSpace
ok=1
grep -qx 'Image=/home/tester/Pictures/green.png' "$cfg" \
    && grep -q 'Settings: OK waited for the decode of /home/tester/Pictures/green.png' "$out/f60-slowdecode.log" \
    && precedes "$out/f60-slowdecode.log" 'Settings: the preview shows /home/tester/Pictures/green.png' \
        'Settings: chose /home/tester/Pictures/green.png' \
    && ran_clean f60-slowdecode && ok=0
result "settings: OK during a slow decode waits for it and chooses the highlighted image (exit $(cat "$out/f60-slowdecode.code"))" $ok
grep -E 'Settings: (OK waited|chose|the preview shows)' "$out/f60-slowdecode.log" | sed 's/^/      /'

# The same with a broken image: OK on b.png while it is still decoding waits, finds it cannot be
# opened, and refuses it. Without the wait it would be chosen and saved.
STREAMFLEX_TEST_DECODE_DELAY_MS=4000 CFG=$FX/f60-broken.ini UNTIL='Settings: nothing changed' \
    run_keys f60-slowbroken Menu Down Return Down Return Down Return Menu
ok=1
grep -q 'Settings: OK waited for the decode of /home/tester/broken/b.png' "$out/f60-slowbroken.log" \
    && grep -q 'Settings: This image cannot be opened: /home/tester/broken/b.png' "$out/f60-slowbroken.log" \
    && ! grep -q 'Settings: chose' "$out/f60-slowbroken.log" && ran_clean f60-slowbroken && ok=0
result "settings: OK on a broken image during its decode waits, then refuses it (exit $(cat "$out/f60-slowbroken.code"))" $ok

# Slideshow: Mode to Slideshow, open the browser on the Folder row, use Pictures
cfg=$(writable_config f60-color)
CFG=$cfg run_keys f60-slideshow Menu Down Return Right Right Down Return Return BackSpace BackSpace
ok=1
grep -qx 'Mode=Slideshow' "$cfg" && grep -qx 'SlideshowDirectory=/home/tester/Pictures' "$cfg" \
    && grep -q 'Found 3 images in directory /home/tester/Pictures' "$out/f60-slideshow.log" \
    && ran_clean f60-slideshow && ok=0
result "settings: a slideshow folder chosen in the folder browser is saved (exit $(cat "$out/f60-slideshow.code"))" $ok

# Stepping the mode through a running slideshow: its first change is due at 5 s and fades for 3 s.
# The keys start when the log says the fade has begun, so settings open inside it and the first
# step (to Transparent) lands in it about 1 s later, however slowly the launcher started: the log
# must say the fade in progress was dropped. The keys that reach the Background page come 150 ms
# apart: a second apart, as run_keys sends them, the step came 3 s in, as the fade ended.
open_background() { xdotool key --delay 150 Menu Down Return; sleep 0.5; }
run_after_line f60-running 'Slideshow: fading in the next image' +open_background Right Left Right Left BackSpace BackSpace
ok=1
ran_clean f60-running && grep -q 'Settings: nothing changed' "$out/f60-running.log" \
    && precedes "$out/f60-running.log" 'Slideshow: fading in the next image' 'Settings: [Background] Mode Slideshow -> Transparent' \
    && in_range "$out/f60-running.log" 'Settings: [Background] Mode Slideshow -> Transparent' \
        'Background set up' 'Slideshow: dropped the fade in progress' \
    && ok=0
result "settings: switching modes while a slideshow runs frees its fade cleanly (exit $(cat "$out/f60-running.code"))" $ok
grep -m3 -E 'AddressSanitizer|runtime error' "$out/f60-running.err" | sed 's/^/      /'

# The Folder row shows the folder's name and its image count: counted when the page opens (the
# running slideshow's Pictures) and again when a folder is chosen in the browser
DOT=$(printf '\xC2\xB7')
ok=1
grep -q "Settings: the Folder row shows Pictures $DOT 3 images" "$out/f60-running.log" \
    && sed -n '/Settings: chose \/home\/tester\/Pictures/,$p' "$out/f60-slideshow.log" \
        | grep -q "Settings: the Folder row shows Pictures $DOT 3 images" && ok=0
result "settings: the Folder row shows the folder's name and its image count" $ok
