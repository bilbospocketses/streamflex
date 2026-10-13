# The font picker's timing-sensitive cases, from 59-settings-fonts.sh: keys and a quit that must land
# while the harness build holds the font listing back. The fixture folder and the helpers they use
# are made here as there.
# Timing-sensitive: keys and a quit must land inside a font listing held 8 s or 5 s

rm -rf "$TESTER_HOME/fonts"
mkdir -p "$TESTER_HOME/fonts/deeper"
cp /work/assets/fonts/Roboto-Regular.ttf "$TESTER_HOME/fonts/deeper/"
printf 'not a font\n' > "$TESTER_HOME/fonts/broken.ttf"
chown -R tester:tester "$TESTER_HOME/fonts"

wait_fonts() { wait_line 'Fonts: found' "$2"; sleep 1; }

TITLES_FONT="Menu Down Down Down Return Down Down Return"

f59_count() { grep -c -- "$1" "$2"; }

# While the files are listed (STREAMFLEX_TEST_FONT_DELAY_MS holds the listing back) the picker says
# Loading fonts... (0), Back is its only key, and opening it again lists nothing twice. The files are
# read with the picker closed, and it then opens at once on the font in use.
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DELAY_MS=8000 STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$FX/f60-color.ini \
    run_keys f59-loading $TITLES_FONT Up Return BackSpace Return BackSpace +wait_fonts Return Menu
log=$out/f59-loading.log
LOADING=$(printf 'Loading fonts\xE2\x80\xA6 (0)')
ok=1
[ "$(grep -cF "Settings: the font picker reads $LOADING" "$log")" = 2 ] \
    && grep -qx 'Settings: the key hint reads Back cancels' "$log" \
    && [ "$(f59_count 'Fonts: listing the font files' "$log")" = 1 ] \
    && [ "$(f59_count 'Settings: opened the picker for \[Titles\] Font' "$log")" = 3 ] \
    && [ "$(f59_count 'Settings: closed the picker for \[Titles\] Font' "$log")" -ge 2 ] \
    && sed -n '/Fonts: found/,$p' "$log" | grep -q "Settings: the list's cursor reads Open Sans" \
    && ! sed -n '/Fonts: found/,$p' "$log" | grep -q 'Settings: the font picker reads Loading' \
    && ! grep -q 'Settings: \[Titles\]' "$log" && grep -q 'Settings: nothing changed' "$log" && ran_clean f59-loading && ok=0
result "fonts: the picker loading says so, takes only Back, and lists the files once (exit $(cat "$out/f59-loading.code"))" $ok
grep -E 'Fonts:|font picker reads|key hint reads|picker for \[Titles\]' "$log" | sed 's/^/      /'

# Quitting while the files are still listed waits for the listing's thread, then exits cleanly
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DELAY_MS=5000 STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$FX/f60-color.ini \
    run_keys f59-quitscan $TITLES_FONT
log=$out/f59-quitscan.log
ok=1
grep -q 'Fonts: listing the font files' "$log" && ! grep -q 'Fonts: found' "$log" \
    && sed -n '/Quitting program/,$p' "$log" | grep -q 'Fonts: waited for the font scan at quit' \
    && ran_clean f59-quitscan && ok=0
result "fonts: quitting while the fonts are listed waits for the listing (exit $(cat "$out/f59-quitscan.code"))" $ok
