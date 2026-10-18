# Editing a menu's tiles in settings (3c, spec "Pages and flows"): a special command added after a tile
# through its page's Add after this tile, its title typed on the on-screen keys and shown in the preview as
# typed; one added after the third of four tiles; a submenu made with a name typed on a real keyboard, at
# the end through the Tiles page's Add; a submenu removed with its tiles, and the confirm page's other
# choice; a submenu renamed, with every :submenu line, the default menu and the bindings that named it, and
# renamed again in the same visit; a
# title config.ini cannot hold; Discard; and saves after a hand edit. Each saved config.ini is read line by
# line (its lines but the blank ones, in order).
# Not timing-sensitive: every key waits for its frame, xdotool type's keys reach the launcher in order, and every result is read from the log's lines or the saved file, never from a clock

ARROW=$(printf ' \xE2\x80\xBA ')
# The top page, Menus, the first menu, its Tiles row (after Rows, Columns and Largest button)
E82_TILES="Menu Down Down Return Down Return Down Down Down Return"
E82_LAST="Down Down Down Down Down Down Down Down"   # The last row of a page: a tile's Remove, Add's Special
E82_AFTER="$E82_LAST Up"                              # A tile's Add after this tile, one above Remove
e82_type() { xdotool type --delay 60 "$1"; }
e82_type_kids() { e82_type Kids; }
e82_type_play() { e82_type Play; }
e82_type_semicolon() { e82_type ';x'; }
e82_type_x() { e82_type x; }
# A function to give the lines of a config but its blank ones, joined by '|', for a line-by-line comparison
e82_lines() { grep -v '^$' "$1" | tr '\n' '|'; }

# Games' page, Add after this tile, Special, Sleep: after Games; then its title on the on-screen keys:
# Right x5 to y, OK, then Down, Down, Down, Right x4 to Done. The preview follows the title as typed, and
# the new line is numbered above the section's highest and written before Bye's.
cfg=$(writable_config f82-tiles)
CFG=$cfg run_keys f82-special $E82_TILES Down Return $E82_AFTER Return $E82_LAST Return Down Down Down Return \
    Return Return Right Right Right Right Right Return Down Down Down Right Right Right Right Return Menu
ok=1
log=$out/f82-special.log
[ "$(e82_lines "$cfg")" = '[General]|DefaultMenu=Main|[Main]|Entry1=Games;games;:submenu Games|Entry3=Sleepy;sleep;:sleep|Entry2=Bye;quit;:quit|[Games]|Rows=2|Entry1=Retro;emulators;:back|Entry2=Arcade;games;:submenu Arcade|[Arcade]|Entry1=Pong;games;:back|' ] \
    && grep -qF "Settings: page Settings${ARROW}Menus${ARROW}Main${ARROW}Tiles${ARROW}Games${ARROW}Add${ARROW}Special" "$log" \
    && grep -q "Menu 'Main': tile 1 selected" "$log" \
    && in_range "$log" "Settings: opened the keyboard for Title for Sleep" "Settings: the keyboard finished with 'Sleepy'" \
        "Settings: the preview shows the title 'Sleepy'" \
    && grep -q 'Settings: the menus were written to ' "$log" && ran_clean f82-special && ok=0
result "tiles: a special command added after a tile from its page, its title typed on screen, saves (exit $(cat "$out/f82-special.code"))" $ok
grep -E "Settings: (page|the keyboard (reads|finished)|the preview shows the title|the menus)|tile [0-9]+ selected" "$log" | sed 's/^/      /'

# Four tiles: Three's page, Add after this tile, Special, Home: after Three, before Four, numbered above
# the section's highest (user ruling 2026-10-16: a tile's page adds after that tile)
cfg=$(writable_config f82-four)
CFG=$cfg run_keys f82-after $E82_TILES Down Down Down Return $E82_AFTER Return $E82_LAST Return Return Menu
ok=1
log=$out/f82-after.log
[ "$(e82_lines "$cfg")" = '[General]|DefaultMenu=Main|[Main]|Entry1=One;apps;:home|Entry2=Two;apps;:home|Entry3=Three;apps;:home|Entry5=Home;home;:home|Entry4=Four;apps;:home|' ] \
    && grep -qF "Settings: page Settings${ARROW}Menus${ARROW}Main${ARROW}Tiles${ARROW}Three${ARROW}Add" "$log" \
    && grep -q "Menu 'Main': tile 3 selected" "$log" && ran_clean f82-after && ok=0
result "tiles: Add after this tile puts a new tile after the third of four (exit $(cat "$out/f82-after.code"))" $ok
grep -E "Settings: (page|the menus)|tile [0-9]+ selected" "$log" | sed 's/^/      /'

# Add, Submenu: Kids typed on a real keyboard (Enter is Done once typed), at the end; then Games removed with
# its tiles from the confirm page, which offers nothing else (no other tile opens Games). Arcade stays.
cfg=$(writable_config f82-tiles)
CFG=$cfg run_keys f82-submenu $E82_TILES Return $E82_LAST Up Return +e82_type_kids Return Up Up Return \
    $E82_LAST Return Return Menu
ok=1
log=$out/f82-submenu.log
[ "$(e82_lines "$cfg")" = '[General]|DefaultMenu=Main|[Main]|Entry2=Bye;quit;:quit|Entry3=Kids;folder;:submenu Kids|[Arcade]|Entry1=Pong;games;:back|[Kids]|' ] \
    && grep -q "Settings: opened the keyboard for Name for the new submenu" "$log" \
    && grep -q "Settings: the cursor's row reads Remove Games and its 2 tiles" "$log" \
    && ! grep -q "Settings: the cursor's row reads Remove only this tile" "$log" \
    && grep -q "Menus: 'Games' is gone" "$log" && grep -q "Menus: 'Kids' is new" "$log" && ran_clean f82-submenu && ok=0
result "tiles: a submenu made with a typed name, and one removed with its tiles, save (exit $(cat "$out/f82-submenu.code"))" $ok
grep -E "Settings: (opened the keyboard|the cursor's row reads Remove)|Menus: '" "$log" | sed 's/^/      /'

# Arcade opened from two menus: its tile's confirm page offers both, and Remove only this tile leaves the
# menu and its other opener
cfg=$(writable_config f82-two)
CFG=$cfg run_keys f82-two $E82_TILES Down Return $E82_LAST Return Down Return Menu
ok=1
log=$out/f82-two.log
[ "$(e82_lines "$cfg")" = '[General]|DefaultMenu=Main|[Main]|Entry2=Bye;quit;:quit|[Games]|Entry1=Arcade;games;:submenu Arcade|[Arcade]|Entry1=Pong;games;:back|' ] \
    && grep -q "Settings: the cursor's row reads Remove Arcade and its 1 tile" "$log" \
    && grep -q "Settings: the cursor's row reads Remove only this tile" "$log" && ran_clean f82-two && ok=0
result "tiles: a submenu opened twice offers Remove only this tile, which keeps the menu (exit $(cat "$out/f82-two.code"))" $ok

# Games renamed Play through its tile's title (settings opened over Games, the default menu): five Backspaces
# on the on-screen keys (Down x3 to the bottom row, Right x3 to the backspace key), then Play typed on a real
# keyboard. Its section, the tile that opens it, DefaultMenu, the hotkey and the gamepad button follow.
cfg=$(writable_config f82-rename)
CFG=$cfg run_keys f82-rename Menu Down Down Return Down Down Return Down Down Down Return Down Return Return \
    Down Down Down Right Right Right Return Return Return Return Return +e82_type_play Return Menu
ok=1
log=$out/f82-rename.log
[ "$(e82_lines "$cfg")" = '[General]|DefaultMenu=Play|[Hotkeys]|Hotkey1=#4000003A;:submenu Play|[Gamepad]|ButtonX=:submenu Play|[Play]|Entry1=Retro;emulators;:back|Entry2=Main;apps;:submenu Main|[Main]|Entry1=Play;games;:submenu Play|Entry2=Bye;quit;:quit|' ] \
    && grep -q "Settings: the keyboard reads '' (cursor 0)" "$log" \
    && grep -q "Menus: 'Games' is now 'Play'" "$log" \
    && grep -q 'Settings: the bindings and the Startup and Quit commands follow the renamed menu' "$log" \
    && ! grep -q "Settings: the preview shows the title" "$log" && ran_clean f82-rename && ok=0
result "tiles: a submenu renamed through its tile renames every :submenu line, DefaultMenu and the bindings (exit $(cat "$out/f82-rename.code"))" $ok
grep -E "Settings: (the keyboard (reads|finished)|the bindings)|Menus: '" "$log" | sed 's/^/      /'

# The same submenu renamed twice in one visit: Games to Play as above, then its Title again, x typed on a
# real keyboard after Play. The second rename starts from the live name Play, not the section as read, and
# every :submenu line, DefaultMenu and the bindings end on Playx (Task 4 review's CARRY)
cfg=$(writable_config f82-rename)
CFG=$cfg run_keys f82-rename-twice Menu Down Down Return Down Down Return Down Down Down Return Down Return Return \
    Down Down Down Right Right Right Return Return Return Return Return +e82_type_play Return Return +e82_type_x Return \
    Menu
ok=1
log=$out/f82-rename-twice.log
[ "$(e82_lines "$cfg")" = '[General]|DefaultMenu=Playx|[Hotkeys]|Hotkey1=#4000003A;:submenu Playx|[Gamepad]|ButtonX=:submenu Playx|[Playx]|Entry1=Retro;emulators;:back|Entry2=Main;apps;:submenu Main|[Main]|Entry1=Playx;games;:submenu Playx|Entry2=Bye;quit;:quit|' ] \
    && grep -q "Menus: 'Games' is now 'Play'" "$log" && grep -q "Menus: 'Play' is now 'Playx'" "$log" \
    && [ "$(grep -c '^Settings: the bindings and the Startup and Quit commands follow the renamed menu$' "$log")" = 2 ] \
    && ran_clean f82-rename-twice && ok=0
result "tiles: a submenu renamed twice in one visit ends on its second name everywhere (exit $(cat "$out/f82-rename-twice.code"))" $ok
grep -E "Settings: (the keyboard finished|the bindings)|Menus: '" "$log" | sed 's/^/      /'

# A title with ';' keeps Done off with the reason; Esc asks, Esc again discards; nothing is saved
cfg=$(writable_config f82-tiles)
CFG=$cfg run_keys f82-semicolon $E82_TILES Down Down Return Return +e82_type_semicolon Return Escape Escape Menu
ok=1
log=$out/f82-semicolon.log
grep -q "Settings: the keyboard reads 'Bye;x' (cursor 5)" "$log" \
    && grep -q 'Settings: the keyboard says A title can.t contain ;' "$log" \
    && grep -q 'Settings: Done is off: A title can.t contain ;' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f82-tiles.ini" "$cfg" && ran_clean f82-semicolon && ok=0
result "tiles: a title with ';' keeps Done off with the reason, and nothing is saved (exit $(cat "$out/f82-semicolon.code"))" $ok

# Discard after adding Home: the menus go back as they were read, the launcher with them, and nothing is
# written (Discard changes is the top page's last row)
cfg=$(writable_config f82-tiles)
CFG=$cfg run_keys f82-discard $E82_TILES Return $E82_LAST Return Return BackSpace BackSpace BackSpace \
    $E82_LAST Down Down Down Down Return Menu
ok=1
log=$out/f82-discard.log
grep -q 'Settings: discarded the changes' "$log" && grep -q 'Settings: the menus went back to how they were read' "$log" \
    && sed -n '/Settings: discarded the changes/,$p' "$log" | grep -q "Menus: the launcher's menus were made again from the menus model" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f82-tiles.ini" "$cfg" && ran_clean f82-discard && ok=0
result "tiles: Discard puts the menus back and writes nothing (exit $(cat "$out/f82-discard.code"))" $ok

# A hand edit meanwhile (Review Focus 1): Bye removed in settings while config.ini's Bye line is changed by
# hand, then Home added at the end through the Tiles page's Add. The removal is skipped with 3b's note, the
# hand's line stays, and Home is written after the section's last line
# (written over in place, as check 62's hand edits are, so the file keeps its owner and inode)
e82_hand_edit() {
    sed 's/^Entry2=Bye;quit;:quit$/Entry2=Ciao;quit;:quit/' "$e82_cfg" > "$out/f82-hand.edit"
    cat "$out/f82-hand.edit" > "$e82_cfg"
    sleep 0.5
}
e82_cfg=$(writable_config f82-tiles)
CFG=$e82_cfg run_keys f82-hand $E82_TILES Down Down Return $E82_LAST Return +e82_hand_edit Up Return $E82_LAST \
    Return Return Menu
ok=1
log=$out/f82-hand.log
[ "$(e82_lines "$e82_cfg")" = '[General]|DefaultMenu=Main|[Main]|Entry1=Games;games;:submenu Games|Entry2=Ciao;quit;:quit|Entry3=Home;home;:home|[Games]|Rows=2|Entry1=Retro;emulators;:back|Entry2=Arcade;games;:submenu Arcade|[Arcade]|Entry1=Pong;games;:back|' ] \
    && grep -q "Settings: not saved as asked: 'Entry2=Bye;quit;:quit' is not there any more, so its removal is skipped" "$log" \
    && ran_clean f82-hand && ok=0
result "tiles: a save after a hand edit skips what is gone, with a note, and writes the rest (exit $(cat "$out/f82-hand.code"))" $ok
grep -E 'Settings: (not saved as asked|saved|the menus were written)' "$log" | sed 's/^/      /'

# A tile's title changed in settings ("x" typed after Bye) while the hand changes its line: the change is
# written as a new line beside the hand's, and the note says so with no words on keys and buttons, which
# are the bindings' (Fable review M6)
e82_cfg=$(writable_config f82-tiles)
CFG=$e82_cfg run_keys f82-meanwhile $E82_TILES Down Down Return Return +e82_hand_edit +e82_type_x Return Menu
ok=1
log=$out/f82-meanwhile.log
sed -n '/^\[Main\]/,/^$/p' "$e82_cfg" | grep -qx 'Entry2=Ciao;quit;:quit' \
    && sed -n '/^\[Main\]/,/^$/p' "$e82_cfg" | grep -qE '^Entry[0-9]+=Byex;quit;:quit$' \
    && grep -qx "Settings: not saved as asked: 'Entry2=Bye;quit;:quit' changed meanwhile, so its change is written as a new line" "$log" \
    && ran_clean f82-meanwhile && ok=0
result "tiles: a tile changed in settings and by hand is written as a new line, its note without the bindings' words (exit $(cat "$out/f82-meanwhile.code"))" $ok
grep -E 'Settings: (not saved as asked|the menus were written)' "$log" | sed 's/^/      /'
