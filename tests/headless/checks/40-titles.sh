# Items 18 and 19: titles scale with the button by default, stop at a readable minimum, and a
# fixed FontSize or Padding still means what it did

# A dense 8 x 4 grid: 14% of these buttons would be unreadable, so titles stop at 22 pt (2% of 1080)
run_quick f40-dense
ok=1
grep -qE "Menu 'Main': 8 x 4 grid, [0-9]+ px buttons, 22 pt titles" "$out/f40-dense.log" \
    && ran_clean f40-dense && ok=0
result "items 18-19: a dense grid's titles stop at the readable minimum (exit $(cat "$out/f40-dense.code"))" $ok
grep "Menu 'Main':" "$out/f40-dense.log" | sed 's/^/      /'

# Three large buttons: the titles grow with them (round(556 * 0.14) = 78)
run_quick f40-large
ok=1
grep -q "Menu 'Main': 3 x 1 grid, 556 px buttons, 78 pt titles" "$out/f40-large.log" \
    && ran_clean f40-large && ok=0
result "item 19: large buttons get titles to match (exit $(cat "$out/f40-large.code"))" $ok
grep "Menu 'Main':" "$out/f40-large.log" | sed 's/^/      /'

# FontSize=36 and Padding=20 keep today's fixed titles
run_quick f40-fixed
ok=1
grep -q "Menu 'Main': 4 x 1 grid, 256 px buttons, 36 pt titles" "$out/f40-fixed.log" \
    && grep -A10 'Titles ===' "$out/f40-fixed.log" | grep -qE 'FontSize:\s+36$' \
    && grep -A10 'Titles ===' "$out/f40-fixed.log" | grep -qE '^Padding:\s+20$' \
    && ran_clean f40-fixed && ok=0
result "items 18-19: a fixed FontSize and Padding still mean what they did (exit $(cat "$out/f40-fixed.code"))" $ok

# Titles off: nothing under the buttons, and no font sized for them
run_quick f40-notitles
ok=1
ran_clean f40-notitles && grep -q "Menu 'Main': 4 x 1 grid, [0-9]* px buttons, no titles" "$out/f40-notitles.log" && ok=0
result "titles turned off with a percentage FontSize (exit $(cat "$out/f40-notitles.code"))" $ok

# OversizeMode=Truncate (the documented spelling) parses, and so does the old Truncated
for f in f40-truncate f40-truncated; do
    run_quick $f
    ok=1
    grep -A10 'Titles ===' "$out/$f.log" | grep -qE 'OversizeMode:\s+Truncate$' && ran_clean $f && ok=0
    result "$(grep -o 'OversizeMode=[A-Za-z]*' "$FX/$f.ini") parses as Truncate (exit $(cat "$out/$f.code"))" $ok
done

# Shrink mode on long titles in a dense grid: the titles are already at the 22 pt minimum, so
# Shrink cuts them without opening a smaller font, and without a crash
run_quick f40-shrink
ok=1
ran_clean f40-shrink && grep -A10 'Titles ===' "$out/f40-shrink.log" | grep -qE 'OversizeMode:\s+Shrink$' \
    && grep -qE "Menu 'Main': 8 x 4 grid, [0-9]+ px buttons, 22 pt titles" "$out/f40-shrink.log" && ok=0
result "item 19: Shrink mode on long titles in a dense grid (exit $(cat "$out/f40-shrink.code"))" $ok

# Shrink mode under large buttons: at 78 pt every title here is wider than its 556 px button, so
# render_text opens smaller fonts. One title fits at its first smaller size, one needs a step down
# (the scratch set resized), and one stops at the 22 pt minimum and is cut. This finds a
# use-after-free or a double close; the leak pass (run.sh leaks) finds a font left open. The cut
# title fits its button: the debug log names any title wider than its button.
run_quick f40-shrink-large
ok=1
ran_clean f40-shrink-large && grep -A10 'Titles ===' "$out/f40-shrink-large.log" | grep -qE 'OversizeMode:\s+Shrink$' \
    && grep -q "Menu 'Main': 3 x 1 grid, 556 px buttons, 78 pt titles" "$out/f40-shrink-large.log" \
    && ! grep -q 'px wide, over its' "$out/f40-shrink-large.log" && ok=0
result "item 19: Shrink mode on long titles under large buttons steps down and stops at the minimum (exit $(cat "$out/f40-shrink-large.code"))" $ok
grep 'px wide, over its' "$out/f40-shrink-large.log" | sed 's/^/      /'

# A FontSize that is neither a size nor a percentage is refused with a log line
run_quick f40-junk
ok=1
grep -q "Invalid FontSize value '12pt'" "$out/f40-junk.log" && grep -q "Invalid Padding value '20px'" "$out/f40-junk.log" \
    && ran_clean f40-junk && ok=0
result "FontSize=12pt and Padding=20px are refused with a log line (exit $(cat "$out/f40-junk.code"))" $ok

# Every font is closed at quit, before SDL_ttf is: the fixed title font, the cached title sizes and
# the clock's. The leak pass sees a clock font left open, since the clock that held it is freed.
run_quick f40-clock
ok=1
ran_clean f40-clock && grep -A2 'Clock ===' "$out/f40-clock.log" | grep -qE 'Enabled:\s+true$' && ok=0
result "the title and clock fonts are closed at quit (exit $(cat "$out/f40-clock.code"))" $ok

# Shrink mode when a step down fails part-way (the harness build's
# STREAMFLEX_TEST_FAIL_SHRINK_STEP fails every one): the title is cut at the menu's size,
# measured in that font rather than in the one just closed. The debug log names each title wider
# than its button: none may be, least of all Emulation Station Retro, the title whose step down
# fails (822 px in its 556 px button when it was measured in the font just closed).
STREAMFLEX_TEST_FAIL_SHRINK_STEP=1 CFG=$FX/f40-shrink-large.ini run_quick f40-shrinkfail
ok=1
ran_clean f40-shrinkfail && grep -q 'Test hook: the step down to [0-9]* pt fails' "$out/f40-shrinkfail.log" \
    && grep -q "Menu 'Main': 3 x 1 grid, 556 px buttons, 78 pt titles" "$out/f40-shrinkfail.log" \
    && ! grep -q "the title 'Emulation Station Retro' is [0-9]* px wide, over its" "$out/f40-shrinkfail.log" \
    && ! grep -q 'px wide, over its' "$out/f40-shrinkfail.log" && ok=0
result "item 19: Shrink mode cuts a title to fit when a smaller font fails to open (exit $(cat "$out/f40-shrinkfail.code"))" $ok
grep -E 'Test hook|px wide, over its' "$out/f40-shrinkfail.log" | sed 's/^/      /'

# A menu's title size whose font cannot be opened (the harness build's STREAMFLEX_TEST_FAIL_TITLE_SIZE
# fails 22 pt, the dense grid's size): its titles use the fixed FontSize, 36, and the menu is laid
# out for that font's height, so the room kept for them is the room they take
STREAMFLEX_TEST_FAIL_TITLE_SIZE=22 CFG=$FX/f40-dense.ini run_quick f40-failsize
ok=1
ran_clean f40-failsize && grep -q 'Could not open the title font at 22 pt' "$out/f40-failsize.log" \
    && grep -q "Menu 'Main': its titles use the fixed 36 pt font instead" "$out/f40-failsize.log" \
    && grep -qE "Menu 'Main': [0-9]+ x [0-9]+ grid, [0-9]+ px buttons, 36 pt titles" "$out/f40-failsize.log" \
    && ! grep -q "Menu 'Main': .*, 22 pt titles" "$out/f40-failsize.log" && ok=0
result "item 19: a title size that cannot be opened falls back to the fixed size, laid out for it (exit $(cat "$out/f40-failsize.code"))" $ok
grep "Menu 'Main':" "$out/f40-failsize.log" | sed 's/^/      /'
