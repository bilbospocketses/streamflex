# The menus as 3c shows and rebuilds them (plan rulings 2 and 5): a section with no lines is a menu of
# its own, shown empty with a note, and the launcher's menus follow the settings' menus model as it
# changes. The Tiles pages that change the model come later; STREAMFLEX_TEST_MENU_EDIT makes one change
# as settings open, so the rebuild is watched now. Nothing here saves the menus model: it is written only
# by the Tiles pages' save, so each config that opens settings must come back unchanged, but for the one
# run that saves a menu's grid (f80-hand-grid).
# Not timing-sensitive: every key waits for its frame, and every result is read from the log's lines and their order, never from a clock

# A header alone ([Kids]) and a section of only a comment ([Notes]) are menus, listed where their sections
# are, though the parser saw no line of either. Kids opens with its note and no highlight, OK there
# selects nothing, Back leaves it, and Main's next tile still runs.
run_keys f80-empty Return Return BackSpace Right Return
ok=1
log=$out/f80-empty.log
[ "$(grep -o '^Menu Name: .*' "$log" | tr '\n' ,)" = "Menu Name: Kids,Menu Name: Main,Menu Name: Notes," ] \
    && grep -A1 '^Menu Name: Kids' "$log" | grep -qx 'Number of Entries: 0' \
    && grep -q "Menu 'Notes' has no lines, so it is shown empty" "$log" \
    && in_range "$log" "Loading menu 'Kids'" "Loading menu 'Main'" "Menu 'Kids' is empty, so it shows the note: This menu is empty. Hold OK to add tiles." \
    && in_range "$log" "Loading menu 'Kids'" "Loading menu 'Main'" "Empty menu note: rendered" \
    && in_range "$log" "Loading menu 'Kids'" "Loading menu 'Main'" "Menu 'Kids' is empty: there is no tile to select" \
    && grep -qx 'Title: Bye' "$log" && ran_clean f80-empty && ok=0
result "menus live: a section with no lines is an empty menu in its place, shown with its note, and Back leaves it (exit $(cat "$out/f80-empty.code"))" $ok
grep -E "^Menu Name|Loading menu|is empty|Empty menu note|^Title:" "$log" | sed 's/^/      /'

# DefaultMenu may name a menu with no lines: StreamFlex starts on it, with its note, where it stopped before
run_quick f80-default-empty
ok=1
log=$out/f80-default-empty.log
grep -q "Loading menu 'Kids'" "$log" \
    && grep -q "Menu 'Kids' is empty, so it shows the note: This menu is empty. Hold OK to add tiles." "$log" \
    && ! grep -qE "Default menu .* not found|Could not load default menu" "$log" && ran_clean f80-default-empty && ok=0
result "menus live: a default menu with no lines is where StreamFlex starts (exit $(cat "$out/f80-default-empty.code"))" $ok

# A submenu removed while settings are open over it (Review Focus 3): the launcher's menus lose it and the
# tile that opened it, the settings follow over the one menu left, and closing lands on the default menu,
# where the tile left is Bye
cfg=$(writable_config f80-remove)
STREAMFLEX_TEST_MENU_EDIT='remove-menu Games' CFG=$cfg run_keys f80-remove Return Menu BackSpace Return
ok=1
log=$out/f80-remove.log
grep -q "Settings opened over menu 'Games'" "$log" \
    && grep -q "Test hook: the menu edit 'remove-menu Games' was made" "$log" \
    && grep -q "Menus: 'Games' is gone" "$log" \
    && grep -q "Menus: the launcher's menus were made again from the menus model: 1 menu(s), 1 kept, 0 new, 1 gone" "$log" \
    && grep -q "Settings: the menus changed, so their settings were made again over 1 menu(s)" "$log" \
    && sed -n '/^Settings closed/,$p' "$log" | grep -qx 'Title: Bye' \
    && cmp -s "$FX/f80-remove.ini" "$cfg" && ran_clean f80-remove && ok=0
result "menus live: a submenu removed under settings goes with its tile, and closing lands on the default menu (exit $(cat "$out/f80-remove.code"))" $ok
grep -E "Loading menu|Menus:|Settings (opened|closed)|Test hook: the menu edit|^Title:" "$log" | sed 's/^/      /'

# The default menu renamed under settings: its menu keeps its place under the new name, :home finds it, and
# no line looks for the old name
cfg=$(writable_config f80-rename)
STREAMFLEX_TEST_MENU_EDIT='rename-menu Main=Home' CFG=$cfg run_keys f80-rename Menu BackSpace Return Home Right Return
ok=1
log=$out/f80-rename.log
grep -q "Test hook: the menu edit 'rename-menu Main=Home' was made" "$log" \
    && grep -q "Menus: 'Main' is now 'Home'" "$log" \
    && sed -n '/^Key Home/,$p' "$log" | grep -q "^Loading menu 'Home'" \
    && sed -n '/^Key Home/,$p' "$log" | grep -qx 'Title: Bye' \
    && ! grep -q "Menu 'Main' not found" "$log" \
    && cmp -s "$FX/f80-rename.ini" "$cfg" && ran_clean f80-rename && ok=0
result "menus live: the default menu renamed under settings stays the default under its new name (exit $(cat "$out/f80-rename.code"))" $ok
grep -E "Loading menu|Menus:|Key Home|^Title:" "$log" | sed 's/^/      /'

# A tile added under settings, a change that leaves the menus as they were: the menu's entries are made
# again from the model (its icon, a library name, pointed at its file), its buttons rendered again, the
# settings model kept, and the new tile runs
cfg=$(writable_config f80-add)
STREAMFLEX_TEST_MENU_EDIT='add-tile Main=Bye;quit;:quit' CFG=$cfg run_keys f80-add Menu BackSpace Right Return
ok=1
log=$out/f80-add.log
grep -q "Test hook: the menu edit 'add-tile Main=Bye;quit;:quit' was made" "$log" \
    && grep -q "Menus: the launcher's menus were made again from the menus model: 1 menu(s), 1 kept, 0 new, 0 gone" "$log" \
    && in_range "$log" "Settings opened" "Settings closed" "Menu 'Main': rendered its buttons" \
    && ! grep -q "Settings: the menus changed" "$log" && ! grep -q 'no library icon named' "$log" \
    && sed -n '/^Settings closed/,$p' "$log" | grep -qx 'Title: Bye' \
    && cmp -s "$FX/f80-add.ini" "$cfg" && ran_clean f80-add && ok=0
result "menus live: a tile added under settings is in its menu at once, and runs (exit $(cat "$out/f80-add.code"))" $ok
grep -E "Menus:|rendered its buttons|Settings (opened|closed)|^Title:" "$log" | sed 's/^/      /'

# Keys before the first header, and keys under an empty [] header, are in no section (inih reads both as
# one named ""): they make no menu, the log says so once with both counted, and settings open finds the
# launcher's menus and the menus model in step, so nothing is rebuilt
run_keys f80-stray Menu BackSpace Return
ok=1
log=$out/f80-stray.log
[ "$(grep -c '^Config: 2 entry line(s) before the first \[section\] or under an empty \[\] are in no menu, so they are ignored$' "$log")" = 1 ] \
    && [ "$(grep -o '^Menu Name: .*' "$log" | tr '\n' ,)" = "Menu Name: Main," ] \
    && grep -q "Settings opened over menu 'Main'" "$log" \
    && ! grep -q "Menus: config.ini's menus are not the launcher's" "$log" \
    && ! grep -q "Menus: the launcher's menus were made again" "$log" \
    && sed -n '/^Settings closed/,$p' "$log" | grep -qx 'Title: Bye' && ran_clean f80-stray && ok=0
result "menus live: keys before the first section make no menu, and settings open rebuilds nothing (exit $(cat "$out/f80-stray.code"))" $ok
grep -E "^Config:|^Menu Name|Menus:|Settings (opened|closed)|^Title:" "$log" | sed 's/^/      /'

# A menu config.ini gained by hand while StreamFlex runs: settings open reads it (the catch-up makes its
# menu, from its section), and the next change keeps that menu, never freeing it and making it again
# (make_menu() gives it the section it was read from, which menu_for() finds it by; Discard's restored
# menus take the same path)
f80_hand_add() { printf '\n[Kids]\nEntry1=Cartoons;apps;:home\n' >> "$cfg"; }
cfg=$(writable_config f80-caught-up)
STREAMFLEX_TEST_MENU_EDIT='add-tile Main=Two;apps;:home' CFG=$cfg run_keys f80-caught-up +f80_hand_add Menu BackSpace Return
ok=1
log=$out/f80-caught-up.log
grep -q "Menus: config.ini's menus are not the launcher's" "$log" \
    && grep -q "Test hook: the menu edit 'add-tile Main=Two;apps;:home' was made" "$log" \
    && [ "$(grep -c "^Menus: 'Kids' is new$" "$log")" = 1 ] && ! grep -q "^Menus: 'Kids' is gone$" "$log" \
    && precedes "$log" "Menus: the launcher's menus were made again from the menus model: 2 menu(s), 1 kept, 1 new, 0 gone" \
                       "Menus: the launcher's menus were made again from the menus model: 2 menu(s), 2 kept, 0 new, 0 gone" \
    && sed -n '/^Settings closed/,$p' "$log" | grep -qx 'Title: Bye' && ran_clean f80-caught-up && ok=0
result "menus live: a menu read by settings open is kept by the next change, not made again (exit $(cat "$out/f80-caught-up.code"))" $ok
grep -E "Menus:|Test hook: the menu edit|Settings (opened|closed)|^Title:" "$log" | sed 's/^/      /'

# A menu config.ini gained by hand with a grid of its own (Rows=2): the catch-up reads its grid from its
# section as StreamFlex reads it at start, so the preview shows it 2 rows high, and its Rows row starts
# at 2, so stepping it down to All menus is a change the save writes (the line goes)
f80_hand_grid() { printf '\n[Kids]\nRows=2\nEntry1=Cartoons;apps;:home\n' >> "$cfg"; }
cfg=$(writable_config f80-hand-grid)
CFG=$cfg run_keys f80-hand-grid +f80_hand_grid Menu Down Down Return Down Down Return Left Left BackSpace BackSpace BackSpace
ok=1
log=$out/f80-hand-grid.log
grep -q "^Menus: 'Kids' is new$" "$log" \
    && grep -qE "^Menu 'Kids': [0-9]+ x 2 grid" "$log" \
    && grep -q 'Settings: \[Kids\] Rows 2 -> 1' "$log" \
    && sed -n '/^\[Kids\]/,$p' "$cfg" | grep -qx 'Entry1=Cartoons;apps;:home' \
    && ! sed -n '/^\[Kids\]/,$p' "$cfg" | grep -q '^Rows=' && ran_clean f80-hand-grid && ok=0
result "menus live: a menu read by settings open has its section's grid, and its grid's change saves (exit $(cat "$out/f80-hand-grid.code"))" $ok
grep -E "Menus:|^Menu 'Kids': |Settings: \[Kids\]|Settings (opened|closed)" "$log" | sed 's/^/      /'

# Every menu removed from config.ini by hand while StreamFlex runs: the launcher cannot show no menu, so
# settings open says so and keeps the launcher's menus (no catch-up, no rebuild), and Main's tile still runs
f80_hand_empty() { printf '[General]\nDefaultMenu=Main\n' > "$cfg"; }
cfg=$(writable_config f80-no-menus)
CFG=$cfg run_keys f80-no-menus +f80_hand_empty Menu BackSpace Return
ok=1
log=$out/f80-no-menus.log
grep -q "has no menus now (edited by hand since StreamFlex started), so the launcher keeps its menus, and they cannot be edited" "$log" \
    && ! grep -q "Menus: config.ini's menus are not the launcher's" "$log" \
    && ! grep -q "Menus: the launcher's menus were made again" "$log" \
    && sed -n '/^Settings closed/,$p' "$log" | grep -qx 'Title: Bye' && ran_clean f80-no-menus && ok=0
result "menus live: a config.ini left with no menu by hand keeps the launcher's menus at settings open (exit $(cat "$out/f80-no-menus.code"))" $ok
grep -E "Settings: .*has no menus|Menus:|Settings (opened|closed)|^Title:" "$log" | sed 's/^/      /'
