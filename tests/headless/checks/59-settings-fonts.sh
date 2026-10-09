# The font picker (3b), over a fixture font folder (the harness build's STREAMFLEX_TEST_FONT_DIRS
# stands in for the system's): the bundled fonts are found, a file SDL_ttf cannot open is skipped,
# a family is chosen, and its file is saved with no FontFace for a first face

rm -rf "$TESTER_HOME/fonts"
mkdir -p "$TESTER_HOME/fonts/deeper"
cp /work/assets/fonts/Roboto-Regular.ttf "$TESTER_HOME/fonts/deeper/"
printf 'not a font\n' > "$TESTER_HOME/fonts/broken.ttf"
chown -R tester:tester "$TESTER_HOME/fonts"

# Titles > Font: the list loads, the cursor starts on the font in use (Open Sans, the fifth family),
# and four Ups choose DejaVu Sans, the first
wait_fonts() { wait_line 'Fonts: found' "$2"; sleep 1; }
cfg=$(writable_config f60-color)
STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg UNTIL='Settings saved' \
    run_keys f59-fonts Menu Down Down Down Return Down Down Return +wait_fonts Up Up Up Up Return BackSpace BackSpace
log=$out/f59-fonts.log
ok=1
grep -qE '^Fonts: found 7 families in 9 files, skipped 1 \([0-9]+ ms\)$' "$log" \
    && grep -qE '^Font=/work/build/assets/fonts/DejaVuSans.ttf$' "$cfg" && ! grep -q '^FontFace=' "$cfg" \
    && grep -q 'Titles: opened /work/build/assets/fonts/DejaVuSans.ttf (face 0)' "$log" \
    && ran_clean f59-fonts && ok=0
result "fonts: the picker lists the fonts by family and saves the one chosen (exit $(cat "$out/f59-fonts.code"))" $ok
grep -E '^(Fonts:|Titles: opened)' "$log" | sed 's/^/      /'

# A config whose font file has gone: the picker still opens, the titles use the bundled font, the
# log gives SDL's own reason (the file is opened all the same: only something there that is not a
# regular file is refused before the open), and the config's value stays as it was
mkdir -p "$TESTER_HOME/cfg"
cfg=$TESTER_HOME/cfg/f59-gone.ini
printf '[General]\nDefaultMenu=Main\n\n[Titles]\nFont=%s/fonts/gone.ttf\n\n[Main]\nEntry1=One;apps;:quit\n' "$TESTER_HOME" > "$cfg"
chown tester:tester "$cfg"
STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg run_keys f59-gone Menu Down Down Down Return Down Down Return +wait_fonts Menu
ok=1
grep -q "Could not open the font $TESTER_HOME/fonts/gone.ttf (face 0), using the default font" "$out/f59-gone.log" \
    && grep -A1 -F "Could not open the font $TESTER_HOME/fonts/gone.ttf (face 0)" "$out/f59-gone.log" \
        | grep -qF "Couldn't open $TESTER_HOME/fonts/gone.ttf" \
    && grep -q 'Fonts: found' "$out/f59-gone.log" && grep -q "Font=$TESTER_HOME/fonts/gone.ttf" "$cfg" \
    && grep -q 'Settings: nothing changed' "$out/f59-gone.log" && ran_clean f59-gone && ok=0
result "fonts: a font file that is gone falls back, and the picker still opens (exit $(cat "$out/f59-gone.code"))" $ok

# ---------------------------------------------------------------------------------------------------
# Beyond the plan's two: each check below proves a branch the two leave unexercised. The debug log
# says what the font picker shows: the list's rows on show and the family under its cursor, each
# family it draws in its own face (or cannot), "Loading fonts... (N)" while the files are read, and
# what it lets go of at quit.

TITLES_FONT="Menu Down Down Down Return Down Down Return"
CLOCK_FONT="Menu Down Down Down Down Down Down Return Down Down Down Down Return"
BUNDLED=/work/build/assets/fonts
F59=$TESTER_HOME/f59
f59_count() { grep -c -- "$1" "$2"; }

# A function to list the font picker's cursor reads in a log from its first line holding START,
# joined with |
f59_reads() {
    awk -v start="$2" 'index($0, start) { on = 1 } on' "$1" | grep -o "Settings: the list's cursor reads .*" \
        | sed 's/.*reads //' | tr '\n' '|'
}

# A function to print a key's value in one section of an ini file
f59_key() {
    s="[$2]" k="$3=" awk 'BEGIN { s = ENVIRON["s"]; k = ENVIRON["k"] }
        $0 == s { on = 1; next } /^\[/ { on = 0 } on && index($0, k) == 1 { print substr($0, length(k) + 1) }' "$1"
}

# A function to write a config for the test user from a printf format and its arguments; prints its path
f59_config() {
    local name=$1 format=$2; shift 2
    mkdir -p "$TESTER_HOME/cfg"
    # shellcheck disable=SC2059
    printf "$format" "$@" > "$TESTER_HOME/cfg/$name.ini"
    chown tester:tester "$TESTER_HOME/cfg/$name.ini"
    echo "$TESTER_HOME/cfg/$name.ini"
}
F59_MAIN='\n[Main]\nEntry1=One;apps;:quit\n'

# The fixture fonts, from the bundled ones: Qoboto (Roboto renamed) and Open Sanz (Open Sans
# renamed) as faces 0 and 1 of one collection, 40 more families for the samples' cache, and a
# folder for each of the scan's rules
rm -rf "$F59"
mkdir -p "$F59/ttc" "$F59/many" "$F59/elsewhere" "$F59/ext" "$F59/hidden" "$F59/depth/1/2/3/4/5/6/7/8/9" \
         "$F59/twice" "$F59/faces"
python3 "$HERE/make_fonts.py" rename /work/assets/fonts/Roboto-Regular.ttf "$F59/qoboto.ttf" Roboto Qoboto
python3 "$HERE/make_fonts.py" rename /work/assets/fonts/OpenSans-Regular.ttf "$F59/opensanz.ttf" 'Open Sans' 'Open Sanz'
python3 "$HERE/make_fonts.py" ttc "$F59/ttc/pair.ttc" "$F59/qoboto.ttf" "$F59/opensanz.ttf"
for i in $(seq -w 0 39); do
    python3 "$HERE/make_fonts.py" rename /work/assets/fonts/Roboto-Regular.ttf "$F59/many/rob0$i.ttf" Roboto "Rob0$i"
done
cp /work/assets/fonts/Inter-Regular.ttf "$F59/elsewhere/Zed.ttf"
cp /work/assets/fonts/DejaVuSans.ttf "$F59/ext/a.TTF"
cp /work/assets/fonts/FreeSans.ttf "$F59/ext/b.otf"
cp "$F59/ttc/pair.ttc" "$F59/ext/c.ttc"
cp "$F59/ttc/pair.ttc" "$F59/ext/d.OTC"
printf 'not a font\n' > "$F59/ext/notes.txt"
cp /work/assets/fonts/Inter-Regular.ttf "$F59/hidden/h.ttf"
cp /work/assets/fonts/Roboto-Regular.ttf "$F59/hidden/.hidden.ttf"
printf 'x\n' > "$F59/hidden/x"
cp /work/assets/fonts/Inter-Regular.ttf "$F59/depth/1/2/3/4/5/6/7/8/deep8.ttf"
cp /work/assets/fonts/NotoSans-Regular.ttf "$F59/depth/1/2/3/4/5/6/7/8/9/deep9.ttf"
cp /work/assets/fonts/Roboto-Regular.ttf "$F59/twice/r.ttf"
python3 "$HERE/make_fonts.py" blank "$F59/faces/full.ttf" Blank Aa0
python3 "$HERE/make_fonts.py" blank "$F59/faces/star.ttf" Starry "$(printf '\xe2\x98\x85')"
python3 "$HERE/make_fonts.py" blank "$F59/faces/noupper.ttf" NoUpper a0
python3 "$HERE/make_fonts.py" blank "$F59/faces/nolower.ttf" NoLower A0
python3 "$HERE/make_fonts.py" blank "$F59/faces/nodigit.ttf" NoDigit Aa
python3 "$HERE/make_fonts.py" blank "$F59/faces/nameless.ttf" - Aa0
mkdir -p "$F59/collections" "$F59/fakedir/1/2/3/4/5/6/7/8/fake.ttf" "$F59/pipe"
cp /work/assets/fonts/Inter-Regular.ttf "$F59/pipe/p.ttf"
mkfifo "$F59/pipe/x.ttf"
python3 "$HERE/make_fonts.py" blank "$F59/half.ttf" Half Aa0
python3 "$HERE/make_fonts.py" ttc "$F59/collections/half.ttc" "$F59/half.ttf" -
python3 "$HERE/make_fonts.py" blank "$F59/many.ttf" Many Aa0
python3 "$HERE/make_fonts.py" blank "$F59/last.ttf" Last Aa0
# shellcheck disable=SC2046
python3 "$HERE/make_fonts.py" ttc "$F59/collections/many.ttc" $(for i in $(seq 64); do echo "$F59/many.ttf"; done) "$F59/last.ttf"
chown -R tester:tester "$F59"
PAIR=$F59/ttc/pair.ttc

# Quitting lets go of the font list kept for the session. LeakSanitizer cannot see a list a static
# still points to, so the log says so.
ok=1
sed -n '/Quitting program/,$p' "$out/f59-fonts.log" | grep -q 'Fonts: let go of the font list at quit' && ok=0
result "fonts: quitting lets go of the font list kept for the session" $ok

# A face inside a collection: Open Sanz is face 1 of pair.ttc, after every bundled family (Open Sanz
# then Qoboto, by name). Choosing it writes the collection and FontFace=1, which the titles open;
# the picker opened again shows at once, with no second listing, its cursor on the face in use.
cfg=$(writable_config f60-color)
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DIRS=$F59/ttc CFG=$cfg UNTIL='Settings saved' \
    run_keys f59-ttc $TITLES_FONT +wait_fonts Down Down Down Return Return BackSpace BackSpace BackSpace
log=$out/f59-ttc.log
reads=$(f59_reads "$log" 'Settings: opened the picker for [Titles] Font')
ok=1
grep -qE '^Fonts: found 9 families in 8 files, skipped 0 \(' "$log" && [ "$(f59_count 'Fonts: listing the font files' "$log")" = 1 ] \
    && [ "$reads" = 'Open Sans|Roboto|Source Sans Pro|Open Sanz|Open Sanz|' ] \
    && [ "$(f59_key "$cfg" Titles Font)" = "$PAIR" ] && [ "$(f59_key "$cfg" Titles FontFace)" = 1 ] \
    && grep -q "Titles: opened $PAIR (face 1)" "$log" && ran_clean f59-ttc && ok=0
result "fonts: a collection's second face is saved with FontFace=1 and opened (exit $(cat "$out/f59-ttc.code"))" $ok
echo "      the cursor read: ${reads:-nothing}"
grep -E '^(Fonts:|Titles: opened|Settings: \[Titles\])' "$log" | sed 's/^/      /'

# The same file's other face: from Open Sanz (face 1) to Qoboto (face 0) the font's own value stays,
# FontFace goes, and the title font opens again once, at face 0. Choosing the family in use again
# changes nothing.
cfg=$(f59_config f59-face "[General]\nDefaultMenu=Main\n\n[Titles]\nFont=%s\nFontFace=1\n$F59_MAIN" "$PAIR")
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DIRS=$F59/ttc CFG=$cfg UNTIL='Settings saved' \
    run_keys f59-face $TITLES_FONT +wait_fonts Down Return Return Return BackSpace BackSpace
log=$out/f59-face.log
reads=$(f59_reads "$log" 'Settings: opened the picker for [Titles] Font')
ok=1
[ "$reads" = 'Open Sanz|Qoboto|Qoboto|' ] && grep -q 'Settings: \[Titles\] FontFace 1 -> ' "$log" \
    && ! grep -q 'Settings: \[Titles\] Font .* -> ' "$log" && grep -q 'Settings: \[Titles\] Font is unchanged' "$log" \
    && [ "$(sed -n '/FontFace 1 -> /,$p' "$log" | f59_count 'Settings: refreshed the title font' -)" = 1 ] \
    && grep -q "Titles: opened $PAIR (face 0)" "$log" \
    && [ "$(f59_key "$cfg" Titles Font)" = "$PAIR" ] && [ -z "$(f59_key "$cfg" Titles FontFace)" ] \
    && ran_clean f59-face && ok=0
result "fonts: another face of the same file removes FontFace and opens the titles again (exit $(cat "$out/f59-face.code"))" $ok
echo "      the cursor read: ${reads:-nothing}"
grep -E '^(Titles: opened|Settings: \[Titles\]|Settings: refreshed)' "$log" | sed 's/^/      /'

# The clock's font: its picker starts on the clock's own font (Source Sans Pro), and Open Sanz saves
# into [Clock], with FontFace=1 there and none in [Titles]; the clock starts again
cfg=$(f59_config f59-clock "[General]\nDefaultMenu=Main\n\n[Clock]\nEnabled=true\n$F59_MAIN")
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DIRS=$F59/ttc CFG=$cfg UNTIL='Settings saved' \
    run_keys f59-clock $CLOCK_FONT +wait_fonts Down Return BackSpace BackSpace
log=$out/f59-clock.log
reads=$(f59_reads "$log" 'Settings: opened the picker for [Clock] Font')
ok=1
[ "$reads" = 'Source Sans Pro|Open Sanz|' ] && [ "$(f59_key "$cfg" Clock Font)" = "$PAIR" ] \
    && [ "$(f59_key "$cfg" Clock FontFace)" = 1 ] && [ -z "$(f59_key "$cfg" Titles FontFace)" ] \
    && sed -n '/Settings: \[Clock\] Font /,$p' "$log" | grep -q 'Settings: refreshed the clock' \
    && ran_clean f59-clock && ok=0
result "fonts: the clock's picker starts on its font and saves into [Clock] (exit $(cat "$out/f59-clock.code"))" $ok
echo "      the cursor read: ${reads:-nothing}"

# A font in use that no scan finds (a file outside the font folders) is pinned by its file's name,
# drawn in the settings' font, and choosing it changes nothing
cfg=$(f59_config f59-custom "[General]\nDefaultMenu=Main\n\n[Titles]\nFont=%s\n$F59_MAIN" "$F59/elsewhere/Zed.ttf")
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg run_keys f59-custom $TITLES_FONT +wait_fonts Return Menu
log=$out/f59-custom.log
reads=$(f59_reads "$log" 'Settings: opened the picker for [Titles] Font')
ok=1
[ "$reads" = 'Custom: Zed.ttf|' ] && grep -q 'Settings: \[Titles\] Font is unchanged' "$log" \
    && ! grep -q 'the font picker drew Custom' "$log" && grep -q 'Settings: the font picker drew Inter in its own face' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f59-custom && ok=0
result "fonts: a font no scan finds is pinned by name and kept (exit $(cat "$out/f59-custom.code"))" $ok
echo "      the cursor read: ${reads:-nothing}"

# A font in use that is a face of a family, though not the face the family writes (the fixture's
# copy of Roboto; the bundled one, listed first, is Roboto's): the cursor starts on that family
cfg=$(f59_config f59-member "[General]\nDefaultMenu=Main\n\n[Titles]\nFont=%s\n$F59_MAIN" \
      "$TESTER_HOME/fonts/deeper/Roboto-Regular.ttf")
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg run_keys f59-member $TITLES_FONT +wait_fonts Menu
log=$out/f59-member.log
reads=$(f59_reads "$log" 'Settings: opened the picker for [Titles] Font')
ok=1
grep -q "Titles: opened $TESTER_HOME/fonts/deeper/Roboto-Regular.ttf (face 0)\|Title font: $TESTER_HOME/fonts/deeper/Roboto-Regular.ttf (face 0)" "$log" \
    && [ "$reads" = 'Roboto|' ] && grep -q 'Settings: nothing changed' "$log" && ran_clean f59-member && ok=0
result "fonts: a font in use that its family does not write starts the cursor on its family (exit $(cat "$out/f59-member.code"))" $ok
echo "      the cursor read: ${reads:-nothing}"

# The bundled font named by a relative path, as the Windows config names it (.\assets\fonts\...):
# the cursor starts on its family, with no Custom row (7 rows, the fixture folder's 7 families),
# whether the path opened from the working folder (StreamFlex started in its own folder, as the
# Windows shortcut starts it) or beside the executable (started from a folder without it, where
# the loader joins it to the executable's folder). The file's value stays as it was.
for p59_where in /work/build:./assets/fonts/OpenSans-Regular.ttf "$TESTER_HOME:/work/build/./assets/fonts/OpenSans-Regular.ttf"; do
    IFS=: read -r p59_dir p59_opened <<< "$p59_where"
    name=f59-rel$([ "$p59_dir" = /work/build ] && echo work || echo exe)
    cfg=$(f59_config "$name" "[General]\nDefaultMenu=Main\n\n[Titles]\nFont=./assets/fonts/OpenSans-Regular.ttf\n$F59_MAIN")
    # shellcheck disable=SC2086
    ( cd "$p59_dir" && STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg run_keys "$name" $TITLES_FONT +wait_fonts Menu )
    log=$out/$name.log
    reads=$(f59_reads "$log" 'Settings: opened the picker for [Titles] Font')
    ok=1
    grep -qxF "Title font: $p59_opened (face 0)" "$log" && [ "$reads" = 'Open Sans|' ] \
        && grep -q "Settings: the list shows rows 0 to [0-9]* of 7$" "$log" \
        && [ "$(f59_key "$cfg" Titles Font)" = ./assets/fonts/OpenSans-Regular.ttf ] \
        && grep -q 'Settings: nothing changed' "$log" && ran_clean "$name" && ok=0
    result "fonts: the bundled font by a relative path, opened from $p59_dir, starts the cursor on its family (exit $(cat "$out/$name.code"))" $ok
    echo "      the cursor read: ${reads:-nothing}; $(grep -o 'Title font: .*' "$log"); $(grep -o 'Settings: the list shows rows .*' "$log" | head -1)"
done

# OK on the row the cursor starts on, the font in use, changes nothing, though the file names it by
# a relative path and the row by its full one: no write, and the file's value stays as it was
cfg=$(f59_config f59-relok "[General]\nDefaultMenu=Main\n\n[Titles]\nFont=./assets/fonts/OpenSans-Regular.ttf\n$F59_MAIN")
# shellcheck disable=SC2086
( cd /work/build && STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg run_keys f59-relok $TITLES_FONT +wait_fonts Return Menu )
log=$out/f59-relok.log
ok=1
grep -q 'Settings: \[Titles\] Font is unchanged' "$log" && ! grep -q 'Settings: \[Titles\] Font .* -> ' "$log" \
    && grep -q 'Settings: nothing changed' "$log" \
    && [ "$(f59_key "$cfg" Titles Font)" = ./assets/fonts/OpenSans-Regular.ttf ] && ran_clean f59-relok && ok=0
result "fonts: OK on the font in use, named by a relative path, changes nothing (exit $(cat "$out/f59-relok.code"))" $ok
grep -E 'Settings: (\[Titles\]|nothing changed|saved)' "$log" | sed 's/^/      /'

# The same, started from a folder without it: the file is found beside the executable, so it is
# there and not taken as gone, and OK still changes nothing
cfg=$(f59_config f59-relokexe "[General]\nDefaultMenu=Main\n\n[Titles]\nFont=./assets/fonts/OpenSans-Regular.ttf\n$F59_MAIN")
# shellcheck disable=SC2086
( cd "$TESTER_HOME" && STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg run_keys f59-relokexe $TITLES_FONT +wait_fonts Return Menu )
log=$out/f59-relokexe.log
ok=1
grep -qxF 'Title font: /work/build/./assets/fonts/OpenSans-Regular.ttf (face 0)' "$log" \
    && grep -q 'Settings: \[Titles\] Font is unchanged' "$log" && ! grep -q 'Settings: \[Titles\] Font .* -> ' "$log" \
    && grep -q 'Settings: nothing changed' "$log" \
    && [ "$(f59_key "$cfg" Titles Font)" = ./assets/fonts/OpenSans-Regular.ttf ] && ran_clean f59-relokexe && ok=0
result "fonts: OK on the font in use, found beside the executable, changes nothing (exit $(cat "$out/f59-relokexe.code"))" $ok
grep -E 'Settings: (\[Titles\]|nothing changed|saved)' "$log" | sed 's/^/      /'

# The configured file gone: the cursor starts on the bundled font the titles fell back to, and OK
# there writes that font, so the dead path leaves the file
cfg=$(f59_config f59-goneok "[General]\nDefaultMenu=Main\n\n[Titles]\nFont=$TESTER_HOME/fonts/gone.ttf\n$F59_MAIN")
chown tester:tester "$TESTER_HOME/cfg"   # The save writes its backup beside the file
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg UNTIL='Settings saved' \
    run_keys f59-goneok $TITLES_FONT +wait_fonts Return BackSpace BackSpace
log=$out/f59-goneok.log
reads=$(f59_reads "$log" 'Settings: opened the picker for [Titles] Font')
ok=1
grep -qF "Settings: [Titles] Font $TESTER_HOME/fonts/gone.ttf -> $BUNDLED/OpenSans-Regular.ttf" "$log" \
    && [ "${reads%%|*}" = 'Open Sans' ] && [ "$(f59_key "$cfg" Titles Font)" = "$BUNDLED/OpenSans-Regular.ttf" ] \
    && ! grep -q '^FontFace=' "$cfg" && ran_clean f59-goneok && ok=0
result "fonts: OK on the bundled font a gone file fell back to writes it (exit $(cat "$out/f59-goneok.code"))" $ok
grep -E 'Settings: (\[Titles\]|nothing changed|saved)' "$log" | sed 's/^/      /'

# With no Font in the file the titles use the bundled font, which is not a file gone: OK on its
# row, where the cursor starts, changes nothing and writes no Font
cfg=$(f59_config f59-noneok "[General]\nDefaultMenu=Main\n$F59_MAIN")
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg run_keys f59-noneok $TITLES_FONT +wait_fonts Return Menu
log=$out/f59-noneok.log
reads=$(f59_reads "$log" 'Settings: opened the picker for [Titles] Font')
ok=1
grep -q 'Settings: \[Titles\] Font is unchanged' "$log" && ! grep -q 'Settings: \[Titles\] Font .* -> ' "$log" \
    && [ "${reads%%|*}" = 'Open Sans' ] && grep -q 'Settings: nothing changed' "$log" && ! grep -q '^Font=' "$cfg" \
    && ran_clean f59-noneok && ok=0
result "fonts: OK on the bundled font, with no Font in the file, changes nothing (exit $(cat "$out/f59-noneok.code"))" $ok
grep -E 'Settings: (\[Titles\]|nothing changed|saved)' "$log" | sed 's/^/      /'

# A family whose regular face is named Book, as DejaVu's is: DejaVu Sanz (DejaVu Sans renamed),
# its Bold in the first folder read and its Book in the second, so the Bold is found first.
# Choosing the family writes the Book file. It is the eighth row, after the 7 bundled families.
rm -rf "$TESTER_HOME/f59-book"
mkdir -p "$TESTER_HOME/f59-book/bold" "$TESTER_HOME/f59-book/book"
python3 "$HERE/make_fonts.py" rename /work/assets/fonts/DejaVuSans.ttf "$TESTER_HOME/f59-book/book/sanz.ttf" Sans Sanz
python3 "$HERE/make_fonts.py" rename "$TESTER_HOME/f59-book/book/sanz.ttf" "$TESTER_HOME/f59-book/bold/sanz-bold.ttf" Book Bold
chown -R tester:tester "$TESTER_HOME/f59-book"
cfg=$(writable_config f60-color)
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/f59-book/bold:$TESTER_HOME/f59-book/book CFG=$cfg UNTIL='Settings saved' \
    run_keys f59-book $TITLES_FONT +wait_fonts Down Down Down Return BackSpace BackSpace
log=$out/f59-book.log
reads=$(f59_reads "$log" 'Settings: opened the picker for [Titles] Font')
ok=1
grep -qE '^Fonts: found 8 families in 9 files, skipped 0 \(' "$log" && [ "$reads" = 'Open Sans|Roboto|Source Sans Pro|DejaVu Sanz|' ] \
    && [ "$(f59_key "$cfg" Titles Font)" = "$TESTER_HOME/f59-book/book/sanz.ttf" ] && [ -z "$(f59_key "$cfg" Titles FontFace)" ] \
    && grep -q "Titles: opened $TESTER_HOME/f59-book/book/sanz.ttf (face 0)" "$log" && ran_clean f59-book && ok=0
result "fonts: a family whose regular face is named Book writes that face, not its Bold found first (exit $(cat "$out/f59-book.code"))" $ok
echo "      the cursor read: ${reads:-nothing}; $(grep -E '^(Fonts: found|Settings: \[Titles\] Font)' "$log" | tr '\n' ';')"

# A symbol font, as Linux's D050000L is: Dingy has glyphs for its own name and "Aa0" (as a dingbat
# font mapping ASCII does) but none for U+2019 or U+2013, so its row's name is drawn in the
# settings' font, as are Halfa's, with the en dash alone, and Halfb's, with the quote alone. Typo,
# with both, draws its name in its own face (the control), as the bundled fonts do. Chosen, Dingy
# still gives the titles its own face. Dingy is the eighth row.
rm -rf "$TESTER_HOME/f59-sym"
mkdir -p "$TESTER_HOME/f59-sym"
python3 "$HERE/make_fonts.py" blank "$TESTER_HOME/f59-sym/dingy.ttf" Dingy DingyAa0
python3 "$HERE/make_fonts.py" blank "$TESTER_HOME/f59-sym/halfa.ttf" Halfa "HalfaA0$(printf '\xe2\x80\x93')"
python3 "$HERE/make_fonts.py" blank "$TESTER_HOME/f59-sym/halfb.ttf" Halfb "HalfbA0$(printf '\xe2\x80\x99')"
python3 "$HERE/make_fonts.py" blank "$TESTER_HOME/f59-sym/typo.ttf" Typo "TypoAa0$(printf '\xe2\x80\x99\xe2\x80\x93')"
chown -R tester:tester "$TESTER_HOME/f59-sym"
cfg=$(writable_config f60-color)
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/f59-sym CFG=$cfg UNTIL='Settings saved' \
    run_keys f59-symbols $TITLES_FONT +wait_fonts Down Down Down Return BackSpace BackSpace
log=$out/f59-symbols.log
ok=1
grep -qE '^Fonts: found 11 families in 11 files, skipped 0 \(' "$log" \
    && [ "$(f59_count "Settings: the font picker draws Dingy in the settings' font: its face is symbols" "$log")" = 1 ] \
    && grep -q "Settings: the font picker draws Halfa in the settings' font" "$log" \
    && grep -q "Settings: the font picker draws Halfb in the settings' font" "$log" \
    && ! grep -qE 'Settings: the font picker drew (Dingy|Halfa|Halfb)' "$log" \
    && grep -q 'Settings: the font picker drew Typo in its own face' "$log" \
    && grep -q 'Settings: the font picker drew Open Sans in its own face' "$log" \
    && grep -q "Titles: opened $TESTER_HOME/f59-sym/dingy.ttf (face 0)" "$log" \
    && [ "$(f59_key "$cfg" Titles Font)" = "$TESTER_HOME/f59-sym/dingy.ttf" ] && ran_clean f59-symbols && ok=0
result "fonts: a symbol font's row is named in the settings' font, and its face still sets the titles (exit $(cat "$out/f59-symbols.code"))" $ok
grep -E 'font picker (drew|draws) (Dingy|Halfa|Halfb|Typo)|Titles: opened' "$log" | sed 's/^/      /'

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

# The families' samples: each is drawn once in its own face and kept while the picker is open; the
# picker closed lets them go; a face that no longer opens is drawn in the settings' font, and tried
# once. Paging through 47 families overruns the 32 kept, so the first page is drawn again on the
# way back up.
f59_lose_rob000() { rm -f "$F59/many/rob000.ttf"; }
# shellcheck disable=SC2086
STREAMFLEX_TEST_FONT_DIRS=$F59/many CFG=$FX/f60-color.ini run_keys f59-samples $TITLES_FONT +wait_fonts BackSpace \
    +f59_lose_rob000 Return Right Right Right Right Right Left Left Left Left Left Left Menu
log=$out/f59-samples.log
drew=$(f59_count 'Settings: the font picker drew DejaVu Sans in its own face' "$log")
lost=$(f59_count 'Settings: the font picker could not draw Rob000 in its own face' "$log")
ok=1
grep -qE '^Fonts: found 47 families in 47 files' "$log" && [ "$drew" = 3 ] && [ "$lost" = 2 ] \
    && [ "$(f59_count 'Settings: the font picker drew Rob000 in its own face' "$log")" = 1 ] \
    && grep -q 'Settings: the list shows rows [0-9]* to 46 of 47' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f59-samples && ok=0
result "fonts: each family is drawn in its own face once, from a cache of 32 (exit $(cat "$out/f59-samples.code"))" $ok
echo "      DejaVu Sans drawn $drew times, Rob000 not drawn $lost times"

# The scan's rules, a folder each (STREAMFLEX_TEST_FONT_DIRS, and the bundled seven):
# - ext: .ttf, .otf, .ttc and .otc in any case, and no other file;
# - hidden: no hidden file, and none named shorter than an extension (x);
# - depth: folders 8 deep and no deeper;
# - twice: a folder named twice, once with a slash at its end, listed once; an empty or missing one
#   skipped;
# - faces: the faces read, where a font with no glyph for any one of "Aa0" (a symbol font) or no
#   family name opens but lists no family, and one with all of them does (Blank);
# - collections: a collection whose second face does not open lists its first (Half), and one of
#   65 faces is read to its 64th (Many) and no further (Last);
# - fakedir: a folder named like a font file, 8 deep, is not a file;
# - pipe: a pipe named like a font file (x.ttf, which nothing writes) is not a file, so it is never
#   read: a read of it would hold the font list for good
for scan in 'ext:ext:9 families in 11 files' 'hidden:hidden:7 families in 8 files' \
            'depth:depth:7 families in 8 files' 'twice:twice:7 families in 8 files' \
            'faces:faces:8 families in 13 files' 'collections:collections:9 families in 9 files' \
            'fakedir:fakedir:7 families in 7 files' 'pipe:pipe:7 families in 8 files'; do
    IFS=: read -r name dirs found <<< "$scan"
    case $name in
        twice) dirs="$F59/twice/:$F59/twice::/nonexistent" ;;
        *) dirs=$F59/$dirs ;;
    esac
    # shellcheck disable=SC2086
    STREAMFLEX_TEST_FONT_DIRS=$dirs CFG=$FX/f60-color.ini run_keys "f59-$name" $TITLES_FONT +wait_fonts Menu
    log=$out/f59-$name.log
    ok=1
    grep -qE "^Fonts: found $found, skipped 0 \\(" "$log" && ran_clean "f59-$name" && ok=0
    result "fonts: the scan's $name rule finds $found (exit $(cat "$out/f59-$name.code"))" $ok
    grep '^Fonts: found' "$log" | sed 's/^/      /'
done

# Without STREAMFLEX_TEST_FONT_DIRS the scan reads the system's folders and the user's two: the
# files there, plus the bundled seven
mkdir -p "$TESTER_HOME/.fonts" "$TESTER_HOME/.local/share/fonts"
cp /work/assets/fonts/Inter-Regular.ttf "$TESTER_HOME/.fonts/h.ttf"
cp /work/assets/fonts/Inter-Regular.ttf "$TESTER_HOME/.local/share/fonts/l.ttf"
chown -R tester:tester "$TESTER_HOME/.fonts" "$TESTER_HOME/.local"
system=0
for root in /usr/share/fonts /usr/local/share/fonts "$TESTER_HOME/.local/share/fonts" "$TESTER_HOME/.fonts"; do
    [ -d "$root" ] || continue
    n=$(cd "$root" && find -L . -mindepth 1 -maxdepth 9 ! -type d ! -path '*/.*' \
        \( -iname '*.ttf' -o -iname '*.otf' -o -iname '*.ttc' -o -iname '*.otc' \) | wc -l)
    system=$((system + n))
done
# shellcheck disable=SC2086
CFG=$FX/f60-color.ini run_keys f59-system $TITLES_FONT +wait_fonts Menu
ok=1
grep -qE "^Fonts: found [0-9]+ families in $((system + 7)) files" "$out/f59-system.log" && ran_clean f59-system && ok=0
result "fonts: with no test folders the scan reads the system's and the user's fonts (exit $(cat "$out/f59-system.code"))" $ok
echo "      expected $((system + 7)) files; $(grep '^Fonts: found' "$out/f59-system.log")"
rm -rf "$TESTER_HOME/.fonts" "$TESTER_HOME/.local/share/fonts"

# An install without its bundled title font lists no bundled folder, and its titles' own font (the
# file its config names, outside the folders listed) is pinned
rm -rf /opt/sf59-nobundle
mkdir -p /opt/sf59-nobundle
cp /work/build/streamflex /opt/sf59-nobundle/ && cp -r /work/build/assets /opt/sf59-nobundle/
rm -f /opt/sf59-nobundle/assets/fonts/OpenSans-Regular.ttf
cfg=$(f59_config f59-nobundle "[General]\nDefaultMenu=Main\n\n[Titles]\nFont=$BUNDLED/DejaVuSans.ttf\n$F59_MAIN")
# shellcheck disable=SC2086
exe=/opt/sf59-nobundle/streamflex STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg \
    run_keys f59-nobundle $TITLES_FONT +wait_fonts Menu
log=$out/f59-nobundle.log
reads=$(f59_reads "$log" 'Settings: opened the picker for [Titles] Font')
ok=1
grep -qE '^Fonts: found 1 families in 2 files, skipped 1 \(' "$log" && [ "$reads" = 'Custom: DejaVuSans.ttf|' ] \
    && ran_clean f59-nobundle && ok=0
result "fonts: an install with no bundled folder lists the others and pins its own font (exit $(cat "$out/f59-nobundle.code"))" $ok
echo "      the cursor read: ${reads:-nothing}"

# Pipes that nothing writes, as each font file StreamFlex opens: an open of any would wait for good,
# so none is opened. Each goes the way a missing font goes there, with its path and the reason in the
# log, and the launcher never waits on it:
# - pipeabs: the titles' font, by its full path: the titles use the bundled font;
# - piperel: the titles' font, by a path the launcher's own folder holds (the second path tried);
# - pipedefault: a bundled font with no font configured, the clock's: the launcher stops, as
#   without it. The clock's is used because it opens after the window: a stop before the window
#   leaves SDL3's display data unfreed (sdl2-compat's X11, Fedora), which the leak pass counts;
# - pipesettings: the titles' bundled font, which settings take as missing: they open in the titles'
#   font;
# - pipeswap: the titles' font becomes a pipe while the launcher runs, so settings, with no bundled
#   font to use, cannot open, as when that file has gone.
rm -rf /opt/sf59-pipes /opt/sf59-pipebundle "$F59/pipes"
mkdir -p /opt/sf59-pipes/f59rel /opt/sf59-pipebundle "$F59/pipes"
cp /work/build/streamflex /opt/sf59-pipes/ && cp -r /work/build/assets /opt/sf59-pipes/
cp /work/build/streamflex /opt/sf59-pipebundle/ && cp -r /work/build/assets /opt/sf59-pipebundle/
rm -f /opt/sf59-pipebundle/assets/fonts/OpenSans-Regular.ttf /opt/sf59-pipebundle/assets/fonts/SourceSansPro-Regular.ttf
mkfifo /opt/sf59-pipes/f59rel/x.ttf /opt/sf59-pipebundle/assets/fonts/OpenSans-Regular.ttf \
       /opt/sf59-pipebundle/assets/fonts/SourceSansPro-Regular.ttf "$F59/pipes/x.ttf"
cp /work/assets/fonts/Inter-Regular.ttf "$F59/pipes/t.ttf"
chown -R tester:tester /opt/sf59-pipes /opt/sf59-pipebundle "$F59/pipes"
PIPED_BUNDLE=/opt/sf59-pipebundle/assets/fonts/OpenSans-Regular.ttf
# A function to tell whether a log has the line FIRST with the line SECOND straight after it
f59_then() { grep -A1 -xF -- "$2" "$1" | sed -n 2p | grep -qxF -- "$3"; }

cfg=$(f59_config f59-pipeabs "[General]\nDefaultMenu=Main\nStartupCmd=:quit\n\n[Titles]\nFont=$F59/pipes/x.ttf\n$F59_MAIN")
CFG=$cfg run_quick f59-pipeabs
log=$out/f59-pipeabs.log
ok=1
f59_then "$log" "Could not open the font $F59/pipes/x.ttf (face 0), using the default font" 'not a regular file' \
    && grep -qxF 'Title font: /work/build/assets/fonts/OpenSans-Regular.ttf (face 0)' "$log" && ran_clean f59-pipeabs && ok=0
result "fonts: a pipe as the titles' font is never opened, and the titles use the bundled font (exit $(cat "$out/f59-pipeabs.code"))" $ok

cfg=$(f59_config f59-piperel "[General]\nDefaultMenu=Main\nStartupCmd=:quit\n\n[Titles]\nFont=f59rel/x.ttf\n$F59_MAIN")
exe=/opt/sf59-pipes/streamflex CFG=$cfg run_quick f59-piperel
log=$out/f59-piperel.log
ok=1
f59_then "$log" 'Could not open the font f59rel/x.ttf (face 0), using the default font' 'not a regular file' \
    && grep -qxF 'Title font: /opt/sf59-pipes/assets/fonts/OpenSans-Regular.ttf (face 0)' "$log" && ran_clean f59-piperel && ok=0
result "fonts: a pipe as the titles' font in the launcher's folder is never opened (exit $(cat "$out/f59-piperel.code"))" $ok

cfg=$(f59_config f59-pipedefault "[General]\nDefaultMenu=Main\nStartupCmd=:quit\n\n[Titles]\nFont=$BUNDLED/DejaVuSans.ttf\n\n[Clock]\nEnabled=true\n$F59_MAIN")
STREAMFLEX_TEST_NO_MESSAGE_BOX=1 exe=/opt/sf59-pipebundle/streamflex CFG=$cfg run_quick f59-pipedefault
log=$out/f59-pipedefault.log
ok=1
f59_then "$log" 'Could not open the default font /opt/sf59-pipebundle/assets/fonts/SourceSansPro-Regular.ttf' 'not a regular file' \
    && grep -qxF 'Could not load default font' "$log" && ran_clean f59-pipedefault 1 && ok=0
result "fonts: a pipe as the clock's bundled font is never opened, and the launcher stops as without it (exit $(cat "$out/f59-pipedefault.code"))" $ok

cfg=$(f59_config f59-pipesettings "[General]\nDefaultMenu=Main\n\n[Titles]\nFont=$BUNDLED/DejaVuSans.ttf\n$F59_MAIN")
exe=/opt/sf59-pipebundle/streamflex CFG=$cfg UNTIL="Settings opened over menu 'Main'" run_keys f59-pipesettings Menu
log=$out/f59-pipesettings.log
ok=1
f59_then "$log" "Settings: the font $PIPED_BUNDLE is not a regular file" \
        "Settings: the font OpenSans-Regular.ttf is missing, so they use $BUNDLED/DejaVuSans.ttf" \
    && grep -q "Settings opened over menu 'Main'" "$log" && ran_clean f59-pipesettings && ok=0
result "fonts: settings take a pipe as the bundled font as missing, and open in the titles' font (exit $(cat "$out/f59-pipesettings.code"))" $ok
grep 'Settings' "$log" | head -4 | sed 's/^/      /'

# A function for a +key: the titles' font file becomes a pipe. Settings must refuse it before the
# run ends (UNTIL): an open that waited on it would be cut short by stop_run's TERM, and then fail
# as if it had been refused.
f59_swap() { rm -f "$F59/pipes/t.ttf" && mkfifo "$F59/pipes/t.ttf" && chown tester:tester "$F59/pipes/t.ttf"; }
cfg=$(f59_config f59-pipeswap "[General]\nDefaultMenu=Main\n\n[Titles]\nFont=$F59/pipes/t.ttf\n$F59_MAIN")
exe=/opt/sf59-pipebundle/streamflex CFG=$cfg UNTIL='Settings cannot open' run_keys f59-pipeswap +f59_swap Menu
log=$out/f59-pipeswap.log
ok=1
grep -qxF "Title font: $F59/pipes/t.ttf (face 0)" "$log" \
    && f59_then "$log" "Settings cannot open: could not open the font $F59/pipes/t.ttf" 'not a regular file' \
    && ! grep -q "Settings opened over" "$log" && ran_clean f59-pipeswap && ok=0
result "fonts: settings never open the titles' font once it is a pipe (exit $(cat "$out/f59-pipeswap.code"))" $ok
grep 'Settings' "$log" | head -4 | sed 's/^/      /'

# A font list that cannot be made, take its rows or pin the font in use does not open, whether its
# files were read with the picker open (it closes) or before (it refuses); the keys go back to the
# page. A font list that cannot be started, or whose faces run out of memory as they are read, is
# not kept: opening the picker again starts anew.
for fail in list:f60 rows:f60 select:custom; do
    IFS=: read -r step fixture <<< "$fail"
    name=f59-fail$step
    if [ "$fixture" = custom ]; then cfg=$TESTER_HOME/cfg/f59-custom.ini; else cfg=$FX/f60-color.ini; fi
    # shellcheck disable=SC2086
    STREAMFLEX_TEST_FAIL=$step STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$cfg \
        run_keys "$name" $TITLES_FONT +wait_fonts Return Down Menu
    log=$out/$name.log
    ok=1
    [ "$(f59_count 'Settings: the list cannot open: out of memory' "$log")" = 2 ] \
        && [ "$(f59_count 'Settings: opened the picker for \[Titles\] Font' "$log")" = 1 ] \
        && sed -n '/the list cannot open/,$p' "$log" | grep -q 'Settings: closed the picker for \[Titles\] Font' \
        && sed -n '/the list cannot open/,$p' "$log" | grep -q "Settings: the cursor's row reads Colour" \
        && grep -q 'Settings: nothing changed' "$log" && ran_clean "$name" && ok=0
    result "fonts: a font list whose $step step runs out of memory does not open (exit $(cat "$out/$name.code"))" $ok
done
for step in fontlist fontscan fontfolder fontthread; do
    name=f59-fail$step
    # shellcheck disable=SC2086
    STREAMFLEX_TEST_FAIL=$step STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$FX/f60-color.ini \
        run_keys "$name" $TITLES_FONT Return Down Menu
    log=$out/$name.log
    ok=1
    [ "$(f59_count 'Settings: the fonts cannot be listed: out of memory, or no thread' "$log")" = 2 ] \
        && ! grep -q 'Settings: opened the picker' "$log" && ! grep -q 'Fonts: listing' "$log" \
        && sed -n '/the fonts cannot be listed/,$p' "$log" | grep -q "Settings: the cursor's row reads Colour" \
        && grep -q 'Settings: nothing changed' "$log" && ran_clean "$name" && ok=0
    result "fonts: a font list whose $step step fails does not open (exit $(cat "$out/$name.code"))" $ok
done
# Functions to wait for the first and the second font list let go
f59_lost() {
    local i
    for i in $(seq 100); do
        [ "$(grep -c 'so the list was let go' "$LOG" 2> /dev/null)" -ge "$2" ] && break
        running "$1" || break
        sleep 0.2
    done
    sleep 1
}
f59_lost1() { f59_lost "$2" 1; }
f59_lost2() { f59_lost "$2" 2; }
# A font list whose faces (faces) or whose files (fontadd, on the listing's own thread) run out of
# memory is let go, and listed and read anew at the next opening
for fail in 'faces:reading the faces' 'fontadd:listing the font files'; do
    IFS=: read -r step what <<< "$fail"
    name=f59-fail$step
    # shellcheck disable=SC2086
    STREAMFLEX_TEST_FAIL=$step STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$FX/f60-color.ini \
        run_keys "$name" $TITLES_FONT +f59_lost1 Return +f59_lost2 Down Menu
    log=$out/$name.log
    ok=1
    [ "$(f59_count 'Fonts: listing the font files' "$log")" = 2 ] \
        && [ "$(f59_count "Fonts: out of memory while $what, so the list was let go" "$log")" = 2 ] \
        && [ "$(f59_count 'Settings: the list cannot open: out of memory' "$log")" = 2 ] \
        && ! grep -q 'Fonts: found' "$log" \
        && sed -n '/the list cannot open/,$p' "$log" | grep -q "Settings: the cursor's row reads Colour" \
        && grep -q 'Settings: nothing changed' "$log" && ran_clean "$name" && ok=0
    result "fonts: a font list that runs out of memory $what is let go, and read anew (exit $(cat "$out/$name.code"))" $ok
done

# A sample that cannot be kept (out of memory for its place in the cache) is not drawn: its row
# reads in the settings' font, and nothing is drawn or logged in its own face. The hook, tried each
# frame, says it fails once.
# shellcheck disable=SC2086
STREAMFLEX_TEST_FAIL=sample STREAMFLEX_TEST_FONT_DIRS=$TESTER_HOME/fonts CFG=$FX/f60-color.ini \
    run_keys f59-failsample $TITLES_FONT +wait_fonts Down Menu
log=$out/f59-failsample.log
reads=$(f59_reads "$log" 'Settings: opened the picker for [Titles] Font')
ok=1
[ "$(f59_count 'Test hook: sample fails' "$log")" = 1 ] && [ "$reads" = 'Open Sans|Roboto|' ] \
    && ! grep -q 'the font picker drew' "$log" && ! grep -q 'the font picker could not draw' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f59-failsample && ok=0
result "fonts: a sample that cannot be kept is not drawn (exit $(cat "$out/f59-failsample.code"))" $ok
echo "      the cursor read: ${reads:-nothing}; $(grep -c 'the font picker drew' "$log") samples drawn"
