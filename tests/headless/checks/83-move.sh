# Moving tiles (3c, spec "Move"): OK on Move picks a tile up and the preview outlines it; the arrows move it
# through Main's 2 x 2 grid while the preview lays the menu out again; OK drops it, Back puts it back; and
# Move to menu, which a submenu tile cannot use to go into its own menu or a menu inside it. Each saved
# config.ini is read line by line.
# Not timing-sensitive: every key waits for its frame, and every result is read from the log's lines or the saved file, never from a clock

E83_TILES="Menu Down Down Return Down Return Down Down Down Return"   # Main's Tiles page
E83_MOVE="Down Down Down Down Down Down Down Down Up Up Up"          # A tile's page: Move, three above Remove
E83_MOVE_TO="Down Down Down Down Down Down Down Down Up Up"          # Move to menu, two above Remove
e83_lines() { grep -v '^$' "$1" | tr '\n' '|'; }

# One picked up, Right, Right (the row's end: it stays), Down, dropped: Two, Three, Games, One, each line
# keeping its key; the second Right moves nothing, as the launcher's own highlight stops at a row's end
cfg=$(writable_config f83-move)
CFG=$cfg run_keys f83-move $E83_TILES Down Return $E83_MOVE Return Right Right Down Return Menu
ok=1
log=$out/f83-move.log
[ "$(sed -n '/^\[Main\]/,/^$/p' "$cfg" | grep -v '^$' | tr '\n' '|')" = '[Main]|Entry2=Two;apps;:home|Entry3=Three;apps;:home|Entry4=Games;games;:submenu Games|Entry1=One;apps;:home|' ] \
    && grep -q "Menu 'Main': tile 0 outlined" "$log" && grep -q "Menu 'Main': tile 1 outlined" "$log" \
    && grep -q "Menu 'Main': tile 3 outlined" "$log" && ! grep -q "Menu 'Main': tile 2 outlined" "$log" \
    && grep -q 'Settings: no tile outlined' "$log" \
    && grep -q "Settings: the key hint reads Arrows move it $(printf '\xC2\xB7') OK drops it $(printf '\xC2\xB7') Back puts it back" "$log" \
    && in_range "$log" "Menu 'Main': tile 0 outlined" 'Settings: no tile outlined' "Menus: the launcher's menus were made again" \
    && ran_clean f83-move && ok=0
result "move: a tile picked up, moved right to its row's end and down, and dropped, saves in its place (exit $(cat "$out/f83-move.code"))" $ok
grep -E "outlined|Settings: (the key hint|the menus changed)" "$log" | sed 's/^/      /'

# Two picked up, Down a row (two columns): One, Three, Games, Two
cfg=$(writable_config f83-move)
CFG=$cfg run_keys f83-down $E83_TILES Down Down Return $E83_MOVE Return Down Return Menu
ok=1
log=$out/f83-down.log
[ "$(sed -n '/^\[Main\]/,/^$/p' "$cfg" | grep -v '^$' | tr '\n' '|')" = '[Main]|Entry1=One;apps;:home|Entry3=Three;apps;:home|Entry4=Games;games;:submenu Games|Entry2=Two;apps;:home|' ] \
    && grep -q "Menu 'Main': tile 3 outlined" "$log" && grep -q "Menu 'Main': tile 3 selected" "$log" \
    && ran_clean f83-down && ok=0
result "move: Down moves a tile a row of the grid (exit $(cat "$out/f83-down.code"))" $ok

# Three picked up, Left (the row's start: it stays), Up (the first), then Back: put back where it was, and
# nothing saved
cfg=$(writable_config f83-move)
CFG=$cfg run_keys f83-cancel $E83_TILES Down Down Down Return $E83_MOVE Return Left Up BackSpace Menu
ok=1
log=$out/f83-cancel.log
grep -q "Menu 'Main': tile 0 outlined" "$log" && grep -q 'Settings: no tile outlined' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f83-move.ini" "$cfg" && ran_clean f83-cancel && ok=0
result "move: Back puts a moved tile back, and nothing is saved (exit $(cat "$out/f83-cancel.code"))" $ok

# Two moved to Arcade: at its end, with a new line there
cfg=$(writable_config f83-move)
CFG=$cfg run_keys f83-moveto $E83_TILES Down Down Return $E83_MOVE_TO Return Down Return Menu
ok=1
log=$out/f83-moveto.log
[ "$(e83_lines "$cfg")" = '[General]|DefaultMenu=Main|[Layout]|Rows=2|Columns=2|[Main]|Entry1=One;apps;:home|Entry3=Three;apps;:home|Entry4=Games;games;:submenu Games|[Games]|Entry1=Arcade;games;:submenu Arcade|[Arcade]|Entry1=Pong;games;:back|Entry2=Two;apps;:home|' ] \
    && grep -q "Settings: the cursor's row reads Arcade" "$log" && ran_clean f83-moveto && ok=0
result "move: Move to menu puts the tile at the other menu's end (exit $(cat "$out/f83-moveto.code"))" $ok

# The Games tile cannot go into Games, nor into Arcade, inside it: both rows say why, OK does nothing
cfg=$(writable_config f83-move)
CFG=$cfg run_keys f83-refused $E83_TILES Down Down Down Down Return $E83_MOVE_TO Return Down Return Menu
ok=1
log=$out/f83-refused.log
grep -q 'Settings: the note under the preview says This tile opens that menu' "$log" \
    && grep -q 'Settings: the note under the preview says That menu is inside the one this tile opens' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f83-move.ini" "$cfg" && ran_clean f83-refused && ok=0
result "move: a submenu tile cannot move into its own menu or one inside it, and says why (exit $(cat "$out/f83-refused.code"))" $ok
