# Fallback fonts (spec section 4): a character the title font lacks draws from a fallback font, found by
# fontconfig, with the bundled DejaVu Sans last. FONTCONFIG_FILE names one of run.sh's fixture files, so
# the candidates are known: fcbox's one font, sf-box.ttf, draws 中 as a filled square, and fcempty has
# none. The real-font run reads the image's own fonts instead.

# A function for a +key (run_keys), with the run's NAME and PID: once the menu is drawn, count the
# title color's exact pixels inside the box they span (Color=#FF00FF, which nothing else on the screen
# has) into NAME.box, trying for up to 10 s to see a filled glyph
f72_shot() {
    local name=$1 pid=$2 shot=/tmp/f72.xwd i
    command xdotool search --name '^StreamFlex$' windowmove %@ 0 0 windowsize %@ 1920 1080 > /dev/null 2>&1
    wait_line 'Begin program loop' "$pid" || return 0
    for i in $(seq 25); do
        sleep 0.4
        { xwd -root -silent -out "$shot" && python3 "$HERE/pixels.py" "$shot" count 255,0,255 inside 255,0,255; } \
            > "$out/$name.box" 2>&1
        f72_filled "$name" && return 0
    done
}

# A function to tell whether NAME.box shows a filled glyph: at least 100 pixels, filling 90 % of their
# box. Open Sans's own box for a character it lacks is an outline 1.8 px thick at 36 pt, which leaves
# most of its box empty.
f72_filled() {
    local x0 y0 x1 y1 n
    read -r x0 y0 x1 y1 n < <(sed -n 's/^box \([0-9]*\),\([0-9]*\) to \([0-9]*\),\([0-9]*\): \([0-9]*\) pixels.*/\1 \2 \3 \4 \5/p' "$out/$1.box" 2> /dev/null)
    [ -n "${n:-}" ] && [ "$n" -ge 100 ] && [ $((n * 10)) -ge $(( (x1 - x0 + 1) * (y1 - y0 + 1) * 9 )) ]
}

# sf-box has 中: the title falls back to it, logs that once, draws it filled, and at quit the log says
# what the sets' module let go of
FONTCONFIG_FILE=$TESTER_HOME/fcbox/fonts.conf run_keys f72-box +f72_shot
log=$out/f72-box.log
ok=1
grep -qxF 'Fonts: 1 fallback candidates from fontconfig' "$log" \
    && [ "$(grep -cxF "Fonts: the 36 pt titles fall back to $TESTER_HOME/fcbox/fonts/sf-box.ttf (face 0), first for U+4E2D" "$log")" = 1 ] \
    && ! grep -q 'Fonts: no installed font draws' "$log" && f72_filled f72-box \
    && sed -n '/Quitting program/,$p' "$log" | grep -qE '^Fonts: let go of 2 fallback candidates and [0-9]+ probe fonts at quit$' \
    && ran_clean f72-box && ok=0
result "fallback fonts: a title's 中 falls back to the fixture's box font and draws filled (exit $(cat "$out/f72-box.code"))" $ok
grep '^Fonts:' "$log" | sed 's/^/      /'
sed 's/^/      /' "$out/f72-box.box" 2> /dev/null

# No font has 中: the title keeps Open Sans's hollow box, and the log says so once (not a debug line)
FONTCONFIG_FILE=$TESTER_HOME/fcempty/fonts.conf CFG=$FX/f72-box.ini run_keys f72-empty +f72_shot
log=$out/f72-empty.log
ok=1
grep -qxF 'Fonts: 0 fallback candidates from fontconfig' "$log" \
    && [ "$(grep -cxF 'Fonts: no installed font draws U+4E2D; it shows as a box' "$log")" = 1 ] \
    && ! grep -q ' fall back to ' "$log" && ! f72_filled f72-empty && ran_clean f72-empty && ok=0
result "fallback fonts: a character no font has keeps the hollow box and is logged once (exit $(cat "$out/f72-empty.code"))" $ok
grep '^Fonts:' "$log" | sed 's/^/      /'
sed 's/^/      /' "$out/f72-empty.box" 2> /dev/null

# On the image's own fonts: which color emoji font it has is detected, not named by distribution.
# Debian's is a CBDT bitmap font, skipped as one that cannot be scaled; Fedora's is a COLRv1 font,
# skipped as drawing nothing at SDL_ttf 3.2.2. 😀 then falls back to DejaVu Sans, in monochrome, and 中
# to Noto Sans CJK: Debian's Regular collection, face 0, and Fedora's variable one at its Regular named
# instance, 4.
run_quick f72-real
log=$out/f72-real.log
cbdt=/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf
colr=/usr/share/fonts/google-noto-color-emoji-fonts/Noto-COLRv1.ttf
cjk=/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc
cjkvf=/usr/share/fonts/google-noto-sans-cjk-vf-fonts/NotoSansCJK-VF.ttc
skip='' han=''
if [ -f "$cbdt" ]; then skip="Fonts: skipped $cbdt: a bitmap font cannot be scaled to the text"
elif [ -f "$colr" ]; then skip="Fonts: skipped $colr: it draws nothing for U+1F579"; fi
if [ -f "$cjk" ]; then han="Fonts: the 36 pt titles fall back to $cjk (face 0), first for U+4E2D"
elif [ -f "$cjkvf" ]; then han="Fonts: the 36 pt titles fall back to $cjkvf (face 0, named instance 4), first for U+4E2D"; fi
emoji=$(grep -m1 -E '^Fonts: the 36 pt titles fall back to [^ ]*/DejaVuSans\.ttf \(face 0\), first for U\+1F600$' "$log")
ok=1
[ -n "$skip" ] && [ -n "$han" ] && [ -n "$emoji" ] && [ "$(grep -cxF "$skip" "$log")" = 1 ] \
    && precedes "$log" "$skip" "$emoji" && grep -qxF "$han" "$log" && ran_clean f72-real && ok=0
result "fallback fonts: on the image's fonts the color emoji font is skipped, 😀 falls back to DejaVu Sans and 中 to Noto Sans CJK (exit $(cat "$out/f72-real.code"))" $ok
echo "      expected: ${skip:-no color emoji font found}; ${han:-no Noto Sans CJK found}"
grep '^Fonts:' "$log" | sed 's/^/      /'

# Shrink mode over box glyphs: each title keeps one scratch set, opened at its smaller size with its
# fallbacks and resized step by step, so every size measured carries the fallback the drawn one does.
# 90-titles-fit.sh then finds no title wider than its button in this run either.
FONTCONFIG_FILE=$TESTER_HOME/fcbox/fonts.conf run_quick f72-shrink
log=$out/f72-shrink.log
smaller=$(grep -E '^Fonts: the [0-9]+ pt titles fall back to .*/sf-box\.ttf \(face 0\), first for U\+' "$log" | grep -vc '^Fonts: the 78 pt ')
ok=1
grep -q "Menu 'Main': 3 x 1 grid, 556 px buttons, 78 pt titles" "$log" \
    && grep -q "Fonts: the 78 pt titles fall back to $TESTER_HOME/fcbox/fonts/sf-box.ttf (face 0), first for U+4E2D" "$log" \
    && [ "${smaller:-0}" -ge 1 ] && ! grep -q 'px wide, over its' "$log" && ran_clean f72-shrink && ok=0
result "fallback fonts: Shrink measures CJK titles with their fallback at every size and fits them (exit $(cat "$out/f72-shrink.code"))" $ok
grep -E '^Fonts:|px wide, over its' "$log" | sed 's/^/      /'

# Shrink when every step down fails (the harness build's STREAMFLEX_TEST_FAIL_SHRINK_STEP): the scratch
# set is closed, its font before its fallback, and the title is cut at the menu's size
FONTCONFIG_FILE=$TESTER_HOME/fcbox/fonts.conf STREAMFLEX_TEST_FAIL_SHRINK_STEP=1 CFG=$FX/f72-shrink.ini run_quick f72-shrinkfail
log=$out/f72-shrinkfail.log
ok=1
grep -q 'Test hook: the step down to [0-9]* pt fails' "$log" && ! grep -q 'px wide, over its' "$log" \
    && ran_clean f72-shrinkfail && ok=0
result "fallback fonts: a failed step down closes the scratch set and still cuts the title to fit (exit $(cat "$out/f72-shrinkfail.code"))" $ok
grep -E 'Test hook|px wide, over its' "$log" | sed 's/^/      /'
