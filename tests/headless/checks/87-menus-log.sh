# The menus' debug lines (3c Task 15, spec "Errors and limits"): every menu edit, tile by tile, and each
# step the save is asked to take, here for an edit the test hook makes as settings open (STREAMFLEX_TEST_MENU_EDIT)
# and saved as they close; and an app scan that fails or finds nothing saying why where the app list
# would be, with Add's other choices working. The scans' counts and times are check 81's, each icon's
# source and path check 85's.
# Not timing-sensitive: every key waits for its frame, the app list waits for the scan's verdict line,
# and every result is read from the log and config.ini

# A tile added: the edit's line, then the save's step for its menu, and the line in config.ini
cfg=$(writable_config f87-menus)
STREAMFLEX_TEST_MENU_EDIT='add-tile Main=Bye;quit;:quit' CFG=$cfg run_keys f87-add Menu BackSpace
log=$out/f87-add.log
ok=1
grep -qx "Menus: 'Main': tile 1 added: Bye;quit;:quit" "$log" \
    && precedes "$log" "Menus: 'Main': tile 1 added" "Menus: saving: [Main]'s tiles are written again, 2 of them" \
    && precedes "$log" "Menus: saving: [Main]'s tiles are written again" 'Settings saved' \
    && grep -qx 'Entry2=Bye;quit;:quit' "$cfg" && ran_clean f87-add && ok=0
result "menus log: a tile added is logged, then the save's step for its menu (exit $(cat "$out/f87-add.code"))" $ok
grep -E '^(Menus:|Settings saved)' "$log" | sed 's/^/      /'

# A submenu renamed: its opener's tile is now another, and the save renames the section and writes
# Main's tiles again
cfg=$(writable_config f87-menus)
STREAMFLEX_TEST_MENU_EDIT='rename-menu Games=Play' CFG=$cfg run_keys f87-rename Menu BackSpace
log=$out/f87-rename.log
ok=1
grep -qx "Menus: 'Games' is now 'Play'" "$log" \
    && grep -qx "Menus: 'Main': tile 0 is now Play;games;:submenu Play, was Games;games;:submenu Games" "$log" \
    && grep -qxF 'Menus: saving: [Games] is renamed [Play]' "$log" \
    && grep -qxF "Menus: saving: [Main]'s tiles are written again, 1 of them" "$log" \
    && grep -qxF '[Play]' "$cfg" && ran_clean f87-rename && ok=0
result "menus log: a submenu renamed is logged, with the save's steps (exit $(cat "$out/f87-rename.code"))" $ok

# A submenu removed with its tiles: its opener is logged removed, and the save removes the section
cfg=$(writable_config f87-menus)
STREAMFLEX_TEST_MENU_EDIT='remove-menu Games' CFG=$cfg run_keys f87-remove Menu BackSpace
log=$out/f87-remove.log
ok=1
grep -qx "Menus: 'Games' is gone" "$log" && grep -qx "Menus: 'Main': tile 0 removed: Games" "$log" \
    && grep -qxF 'Menus: saving: [Games] is removed' "$log" && ! grep -qxF '[Games]' "$cfg" && ran_clean f87-remove && ok=0
result "menus log: a submenu removed is logged, with the save's step (exit $(cat "$out/f87-remove.code"))" $ok

# An app scan that fails, or finds nothing (I4): settings open at Main's Tiles page; Add, App (whose first
# opening starts the scan); once its verdict is in, the app list says why where its rows would be, and the
# caption too; Back to the Add page, and Browse for a program, the row below App, still opens the folder
# browser; Menu closes settings from it. Each run is named literally, so the smoke lint can cite it.
i87_scanned() { wait_line 'Apps: none listed' "$2"; }

i87_why='The apps could not be listed: the test hook failed it'
STREAMFLEX_TEST_APP_SCAN=fail STREAMFLEX_TEST_OPEN_TILE=Main:-1 CFG=$FX/f87-menus.ini \
    run_keys f87-scanfail Menu Return Return +i87_scanned BackSpace Down Return Menu
log=$out/f87-scanfail.log
ok=1
grep -qE "^Apps: none listed: $i87_why \([0-9]+ ms\)$" "$log" \
    && grep -qxF "Settings: the note under the preview says $i87_why" "$log" \
    && grep -q '^Settings: browsing ' "$log" && ran_clean f87-scanfail && ok=0
result "menus log: an app scan that fails says why where the app list would be, and Browse for a program still works (exit $(cat "$out/f87-scanfail.code"))" $ok

i87_why='No apps were found'
STREAMFLEX_TEST_APP_SCAN=empty STREAMFLEX_TEST_OPEN_TILE=Main:-1 CFG=$FX/f87-menus.ini \
    run_keys f87-scanempty Menu Return Return +i87_scanned BackSpace Down Return Menu
log=$out/f87-scanempty.log
ok=1
grep -qE "^Apps: none listed: $i87_why \([0-9]+ ms\)$" "$log" \
    && grep -qxF "Settings: the note under the preview says $i87_why" "$log" \
    && grep -q '^Settings: browsing ' "$log" && ran_clean f87-scanempty && ok=0
result "menus log: an app scan that finds nothing says why where the app list would be, and Browse for a program still works (exit $(cat "$out/f87-scanempty.code"))" $ok
