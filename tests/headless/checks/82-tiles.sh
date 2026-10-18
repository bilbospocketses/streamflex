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

# --- Task 10b: the app list, the program browser, and a tile's Command ---

# A fixture applications folder for the app scan (Task 7's STREAMFLEX_TEST_APP_DIRS), with three apps, each
# naming an icon <id>82; and a data folder whose hicolor theme holds zed82's alone (an SVG), which the own
# icon's lookup reads first through XDG_DATA_DIRS
rm -rf "$TESTER_HOME/apps82" "$TESTER_HOME/apps82share"; mkdir -p "$TESTER_HOME/apps82"
for e82_app in 'alpha:Alpha Tool' 'kodi:Kodi' 'zed:Zed Editor'; do
    printf '[Desktop Entry]\nType=Application\nName=%s\nExec=/bin/true\nIcon=%s82\n' "${e82_app#*:}" "${e82_app%%:*}" \
        > "$TESTER_HOME/apps82/${e82_app%%:*}.desktop"
done
E82_ZED_SVG=$TESTER_HOME/apps82share/icons/hicolor/scalable/apps/zed82.svg
mkdir -p "$(dirname "$E82_ZED_SVG")"
printf '<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128"><rect x="8" y="8" width="112" height="112" rx="24" fill="#2864C8"/></svg>\n' \
    > "$E82_ZED_SVG"
chown -R tester:tester "$TESTER_HOME/apps82" "$TESTER_HOME/apps82share"
E82_ZED_PNG=$TESTER_HOME/.config/streamflex/icons/zed-editor.png
# The rows come the frame after Task 7's scan has its list: wait for them, not for the scan's verdict
e82_apps_found() { wait_line 'Settings: the app list shows 3 of 3 apps' "$2"; }
e82_type_z() { e82_type z; }

# Add, App: Up from the top row opens the filter's keyboard (Back leaves it); Kodi, the second app, added at
# the end with its library icon. Then Games' page, Add after this tile, App (the visit's scan is done: no
# second listing), z typed on a real keyboard (it opens the filter with that z), Enter, and Zed Editor, the
# one app left, added after Games with its own icon, which no library icon matches: plated and written as
# a PNG in the icons folder as it is added (user ruling 2026-10-16). New lines are numbered in the menu's
# order.
rm -rf "$TESTER_HOME/.config/streamflex/icons"
cfg=$(writable_config f82-tiles)
STREAMFLEX_TEST_APP_DIRS=$TESTER_HOME/apps82 XDG_DATA_DIRS=$TESTER_HOME/apps82share:/usr/local/share:/usr/share \
    CFG=$cfg run_keys f82-app $E82_TILES Return Return +e82_apps_found Up BackSpace Down Return Up Up Return \
    $E82_AFTER Return Return +e82_type_z Return Return Menu
ok=1
log=$out/f82-app.log
[ "$(e82_lines "$cfg")" = "[General]|DefaultMenu=Main|[Main]|Entry1=Games;games;:submenu Games|Entry3=Zed Editor;$E82_ZED_PNG;/home/tester/apps82/zed.desktop|Entry2=Bye;quit;:quit|Entry4=Kodi;kodi;/home/tester/apps82/kodi.desktop|[Games]|Rows=2|Entry1=Retro;emulators;:back|Entry2=Arcade;games;:submenu Arcade|[Arcade]|Entry1=Pong;games;:back|" ] \
    && [ "$(grep -c '^Apps: listing the installed apps$' "$log")" = 1 ] && grep -qE '^Apps: found 3 apps \([0-9]+ ms\)$' "$log" \
    && [ "$(grep -c 'Settings: opened the keyboard for Filter the apps' "$log")" = 2 ] \
    && grep -q "Settings: 'z' (scancode Z) opens the keyboard on the filter" "$log" \
    && grep -q 'Settings: the app list shows 1 of 3 apps for z' "$log" \
    && grep -q 'Settings: chose the app Kodi' "$log" && grep -q 'Settings: chose the app Zed Editor' "$log" \
    && grep -qx "Menus: the new tile 'Kodi' takes the library icon kodi, its match" "$log" \
    && grep -qx "Menus: the new tile 'Zed Editor' takes its own icon from $E82_ZED_SVG, saved to $E82_ZED_PNG" "$log" \
    && [ -f "$E82_ZED_PNG" ] && ran_clean f82-app && ok=0
result "tiles: apps added from the app list, found once a visit and filtered by typing, each with its icon, save (exit $(cat "$out/f82-app.code"))" $ok
grep -E '^Apps:|^Menus: the new tile|Settings: (the app list|opened the keyboard|chose the app)' "$log" | sed 's/^/      /'
rm -rf "$TESTER_HOME/.config/streamflex/icons"

# A scan that fails says why where the list would be, and the Add page's other rows still work
cfg=$(writable_config f82-tiles)
STREAMFLEX_TEST_APP_SCAN=fail CFG=$cfg run_keys f82-scanfail $E82_TILES Return Return BackSpace $E82_LAST Return \
    Return Menu
ok=1
log=$out/f82-scanfail.log
grep -q '^Apps: none listed: ' "$log" && sed -n '/^\[Main\]/,/^$/p' "$cfg" | grep -qx 'Entry3=Home;home;:home' \
    && ran_clean f82-scanfail && ok=0
result "tiles: a failed app scan says why, and Special still adds (exit $(cat "$out/f82-scanfail.code"))" $ok
grep -E '^Apps:|Settings: the note under the preview' "$log" | sed 's/^/      /'

# (Settings closed while the scan runs, Review Focus 4, is check 81's f81-closed: the one scan is Task 7's.)

# A tile's Command, Browse for a program: it opens at the tile's own program, lists programs alone (the
# README without its execute bit is left out), and run-kodi.sh becomes the command. Then the Tiles page's
# Add, Browse for a program, which opens on the program last chosen: Up to old-tool.sh, added at the end,
# titled by its file's name, with the generic icon (no library match, and a script names no icon of its own).
rm -rf "$TESTER_HOME/progs82"; mkdir -p "$TESTER_HOME/progs82"
printf '#!/bin/sh\nexit 0\n' > "$TESTER_HOME/progs82/old-tool.sh"
printf '#!/bin/sh\nexit 0\n' > "$TESTER_HOME/progs82/run-kodi.sh"
printf 'not a program\n' > "$TESTER_HOME/progs82/readme.txt"
chmod 755 "$TESTER_HOME/progs82/old-tool.sh" "$TESTER_HOME/progs82/run-kodi.sh"
chmod 644 "$TESTER_HOME/progs82/readme.txt"
chown -R tester:tester "$TESTER_HOME/progs82"
cfg=$(writable_config f82-progs)
CFG=$cfg run_keys f82-browse $E82_TILES Down Return Down Return Down Down Return Down Return BackSpace Up Return \
    Down Return Up Return Menu
ok=1
log=$out/f82-browse.log
[ "$(e82_lines "$cfg")" = '[General]|DefaultMenu=Main|[Main]|Entry1=Tool;apps;/home/tester/progs82/run-kodi.sh|Entry2=Bye;quit;:quit|Entry3=old-tool;apps;/home/tester/progs82/old-tool.sh|' ] \
    && grep -q 'Settings: the program browser leaves out readme.txt' "$log" \
    && grep -qx "Menus: the new tile 'old-tool' takes the generic icon apps" "$log" \
    && grep -q "Settings: the list's cursor reads Custom: /home/tester/progs82/old-tool.sh" "$log" \
    && grep -q 'Settings: chose /home/tester/progs82/run-kodi.sh' "$log" \
    && grep -q 'Settings: chose /home/tester/progs82/old-tool.sh' "$log" && ran_clean f82-browse && ok=0
result "tiles: Browse for a program lists programs alone, sets a command and adds a tile (exit $(cat "$out/f82-browse.code"))" $ok
grep -E 'Settings: (the program browser|chose|browsing|opened the list)' "$log" | sed 's/^/      /'

# A tile's Command, Type...: the keyboard opens on its command; " now" typed on a real keyboard after it
cfg=$(writable_config f82-tiles)
e82_type_now() { e82_type ' now'; }
CFG=$cfg run_keys f82-command $E82_TILES Down Down Return Down Return Right Right Right Return +e82_type_now Return Menu
ok=1
log=$out/f82-command.log
sed -n '/^\[Main\]/,/^$/p' "$cfg" | grep -qx 'Entry2=Bye;quit;:quit now' \
    && grep -q "Settings: opened the keyboard for Command for Bye" "$log" \
    && grep -q "Settings: the keyboard reads ':quit now' (cursor 9)" "$log" && ran_clean f82-command && ok=0
result "tiles: a tile's command typed on the keyboard saves (exit $(cat "$out/f82-command.code"))" $ok
