# The Icon page (3c Task 13, spec "Icons"): the choices in the spec's order with the first that applies
# preselected; an app's own icon found through its .desktop file's Icon= in a hicolor theme, plated, and
# written as a PNG under ~/.config/streamflex/icons only when it is chosen; the library match; the generic
# icon for an app with neither; the library grid filtered on the keyboard; an image chosen in the folder
# browser; a read-only icons folder; an icon that cannot be plated; Discard deleting the plated file
# nothing uses; and its own icon chosen again once the app's icon changed, written as a new file rather
# than the old one kept. STREAMFLEX_TEST_OPEN_TILE opens settings at a tile's page, where Down and OK open
# Icon.
# Not timing-sensitive: every key waits for its frame, the filter's letters are typed into the keyboard
# that opened a frame before, and every result is read from the log, config.ini and the PNG file

I85_SHARE=/work/tests/headless/fixtures/icons85/share
I85_DATA=$I85_SHARE:/usr/local/share:/usr/share
I85_ICONS=$TESTER_HOME/.config/streamflex/icons
I85_PNG=$I85_ICONS/sf85-tool.png
I85_SVG=$I85_SHARE/icons/hicolor/scalable/apps/sf85-tool.svg
I85_DESKTOP=$I85_SHARE/applications
I85_BACK="BackSpace BackSpace BackSpace BackSpace BackSpace"   # The tile's page up to the top, then saved and closed

# A function to start each run with no icons folder, so a file it writes is its own
i85_clean() { chmod -R u+w "$TESTER_HOME/.config/streamflex" 2> /dev/null; rm -rf "$TESTER_HOME/.config/streamflex"; }

# A function to read a PNG's width, height, color type and its first pixel's alpha: "512 512 6 0". The
# first pixel of the first row reads as stored whatever the row's filter: each filter adds only what lies
# left of it or above it, which is nothing there.
i85_png() {
    python3 - "$1" <<'EOF'
import struct, sys, zlib
data = open(sys.argv[1], 'rb').read()
width, height, depth, kind = struct.unpack('>IIBB', data[16:26])
at, idat = 8, b''
while at < len(data):
    length, name = struct.unpack('>I4s', data[at:at + 8])
    if name == b'IDAT':
        idat += data[at + 8:at + 8 + length]
    at += 12 + length
raw = zlib.decompress(idat)
print(width, height, kind, raw[4] if kind == 6 and depth == 8 else 'none')
EOF
}

# SF85 Tool, an app with no library match: its own icon comes first and is highlighted, shown on the tile
# as it is; chosen, it is written as a 512 px PNG with clear corners, and saved as the tile's icon. Its
# .desktop file says NoDisplay=true, which keeps it out of the app list but not its icon from its tile.
# Chosen again from the Icon page opened once more (the tile's page keeps its cursor on Icon), the tile
# already shows that file, so it keeps it and nothing more is written (the controller's ruling, 2026-10-16).
i85_clean
cfg=$(writable_config f85-icons)
XDG_DATA_DIRS=$I85_DATA STREAMFLEX_TEST_OPEN_TILE=Main:0 CFG=$cfg run_keys f85-own Menu Down Return Return Return Return \
    $I85_BACK
log=$out/f85-own.log
ok=1
grep -qx "Menus: 'SF85 Tool' in 'Main': no library match" "$log" \
    && grep -qx "Menus: 'SF85 Tool' in 'Main': its own icon is $I85_SVG" "$log" \
    && grep -q "^Settings: the cursor's row reads Its own icon" "$log" \
    && grep -qx "Menus: the preview shows its own icon on 'SF85 Tool'" "$log" \
    && precedes "$log" "Menus: the preview shows its own icon on 'SF85 Tool'" "Menus: saved the own icon of 'SF85 Tool' to $I85_PNG" \
    && grep -qx "Menus: 'SF85 Tool' in 'Main' now has the icon $I85_PNG, from its own icon" "$log" \
    && [ "$(grep -c '^Menus: saved the own icon of ' "$log")" = 1 ] \
    && precedes "$log" "Menus: saved the own icon of 'SF85 Tool' to $I85_PNG" "Menus: 'SF85 Tool' in 'Main' keeps its own icon $I85_PNG, written already" \
    && [ "$(ls -A "$I85_ICONS")" = sf85-tool.png ] \
    && grep -qx "Entry1=SF85 Tool;$I85_PNG;$I85_DESKTOP/sf85-tool.desktop" "$cfg" \
    && [ "$(i85_png "$I85_PNG" 2> /dev/null)" = "512 512 6 0" ] && ran_clean f85-own && ok=0
result "icons: an app's own icon is offered first, shown on its tile, written as a 512 px PNG when chosen, and kept, not written again, when chosen again (exit $(cat "$out/f85-own.code"))" $ok
echo "      $(i85_png "$I85_PNG" 2>&1)"
grep -E "^Menus:|the cursor's row reads" "$log" | sed 's/^/      /'

# Kodi, which the library matches: the match comes before its own icon and is highlighted; chosen, no
# file is written
i85_clean
cfg=$(writable_config f85-icons)
XDG_DATA_DIRS=$I85_DATA STREAMFLEX_TEST_OPEN_TILE=Main:1 CFG=$cfg run_keys f85-match Menu Down Return Return $I85_BACK
log=$out/f85-match.log
ok=1
grep -qx "Menus: 'Kodi' in 'Main': the library match is kodi" "$log" \
    && grep -qx "Menus: 'Kodi' in 'Main': its own icon is $I85_SVG" "$log" \
    && grep -qx "Settings: the cursor's row reads Kodi: Library match" "$log" \
    && grep -qx "Menus: the preview shows the library icon kodi on 'Kodi'" "$log" \
    && grep -qx "Menus: 'Kodi' in 'Main' now has the icon kodi, from the library match" "$log" \
    && grep -qx "Entry2=Kodi;kodi;$I85_DESKTOP/kodi85.desktop" "$cfg" \
    && [ -z "$(ls -A "$I85_ICONS" 2> /dev/null)" ] && ran_clean f85-match && ok=0
result "icons: a library match comes first, and choosing it writes no file (exit $(cat "$out/f85-match.code"))" $ok

# An icons folder that cannot be written: choosing its own icon keeps the tile's icon, the caption says
# why, and nothing is saved
i85_clean
mkdir -p "$I85_ICONS"
chown -R tester:tester "$TESTER_HOME/.config"
chmod 555 "$I85_ICONS"
cfg=$(writable_config f85-icons)
XDG_DATA_DIRS=$I85_DATA STREAMFLEX_TEST_OPEN_TILE=Main:0 CFG=$cfg run_keys f85-readonly Menu Down Return Return \
    BackSpace $I85_BACK
log=$out/f85-readonly.log
ok=1
grep -q "^Menus: Couldn't save its icon to $I85_PNG: " "$log" \
    && grep -q "^Settings: the note under the preview says Couldn't save its icon to $I85_PNG" "$log" \
    && ! grep -q "now has the icon" "$log" && [ ! -e "$I85_PNG" ] \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f85-icons.ini" "$cfg" && ran_clean f85-readonly && ok=0
result "icons: an icons folder that cannot be written keeps the tile's icon, and the caption says why (exit $(cat "$out/f85-readonly.code"))" $ok
grep -E "^Menus: Couldn't|the note under the preview says Couldn't" "$log" | sed 's/^/      /'
i85_clean

# An own icon that cannot be plated (the test hook): it is not offered, and with no match either the
# generic apps comes first; chosen, it is the tile's
cfg=$(writable_config f85-icons)
XDG_DATA_DIRS=$I85_DATA STREAMFLEX_TEST_FAIL=plate STREAMFLEX_TEST_OPEN_TILE=Main:0 CFG=$cfg \
    run_keys f85-plate Menu Down Return Return $I85_BACK
log=$out/f85-plate.log
ok=1
grep -qx 'Test hook: plate fails' "$log" \
    && grep -qx "Menus: 'SF85 Tool' in 'Main': its own icon cannot be used: $I85_SVG could not be put on a plate: the test hook failed it" "$log" \
    && grep -qx "Menus: 'SF85 Tool' in 'Main': no match and no icon of its own, so the generic apps is offered" "$log" \
    && grep -qx "Settings: the cursor's row reads Apps: Generic icon" "$log" \
    && grep -qx "Menus: 'SF85 Tool' in 'Main' now has the icon apps, from the library's generic icon" "$log" \
    && grep -qx "Entry1=SF85 Tool;apps;$I85_DESKTOP/sf85-tool.desktop" "$cfg" \
    && [ ! -e "$I85_PNG" ] && ran_clean f85-plate && ok=0
result "icons: an own icon that cannot be plated gives way to the generic icon (exit $(cat "$out/f85-plate.code"))" $ok

# The library grid on a tile that is no app and whose icon is an image, so the grid opens on its first
# icon: Up from that first row opens the keyboard over the grid, "kod" typed on the real keyboard filters
# it as each letter comes, Enter (Done, once typed) keeps the filter on the grid still open under it, and OK
# chooses Kodi, which the preview shows on the tile
i85_type_kod() { xdotool type --delay 150 kod; sleep 1; }
cfg=$(writable_config f85-icons)
XDG_DATA_DIRS=$I85_DATA STREAMFLEX_TEST_OPEN_TILE=Main:3 CFG=$cfg \
    run_keys f85-grid Menu Down Return Down Return Up +i85_type_kod Return Return $I85_BACK
log=$out/f85-grid.log
ok=1
grep -qx "Menus: 'SF85 Pics' in 'Main': no match and no icon of its own, so the generic apps is offered" "$log" \
    && grep -qE '^Settings: opened the library grid: [0-9]+ icons in [0-9]+ groups$' "$log" \
    && grep -qx 'Settings: the keyboard filters the library grid' "$log" \
    && grep -qE "^Settings: the library grid shows [0-9]+ icons for 'kod'$" "$log" \
    && grep -qx "Menus: the preview shows the library icon kodi on 'SF85 Pics'" "$log" \
    && grep -qx 'Settings: chose the library icon kodi' "$log" \
    && grep -qx "Menus: 'SF85 Pics' in 'Main' now has the icon kodi, from the library" "$log" \
    && grep -qx 'Entry4=SF85 Pics;kodi;:home' "$cfg" && ran_clean f85-grid && ok=0
result "icons: the library grid, filtered on the keyboard, chooses a tile's icon (exit $(cat "$out/f85-grid.code"))" $ok
grep -E "^Settings: (opened the library grid|the keyboard filters|the library grid|chose the library)|^Menus:" "$log" | sed 's/^/      /'

# Browse for an image: the browser opens at the tile's image (red.png, in Pictures), Up highlights
# green.png, which the preview shows on the tile, and OK makes it the tile's icon
cfg=$(writable_config f85-icons)
XDG_DATA_DIRS=$I85_DATA STREAMFLEX_TEST_OPEN_TILE=Main:3 CFG=$cfg \
    run_keys f85-browse Menu Down Return Down Down Return Up Return $I85_BACK
log=$out/f85-browse.log
ok=1
grep -qx "Settings: browsing $TESTER_HOME/Pictures" "$log" \
    && grep -qx "Menus: the preview shows $TESTER_HOME/Pictures/green.png on 'SF85 Pics'" "$log" \
    && grep -qx "Menus: 'SF85 Pics' in 'Main' now has the icon $TESTER_HOME/Pictures/green.png, from an image file" "$log" \
    && grep -qx "Entry4=SF85 Pics;$TESTER_HOME/Pictures/green.png;:home" "$cfg" && ran_clean f85-browse && ok=0
result "icons: an image chosen in the folder browser is the tile's icon, shown on it while highlighted (exit $(cat "$out/f85-browse.code"))" $ok

# Discard after its own icon was chosen: the PNG written in this visit, which nothing uses now, is
# deleted, and nothing is saved. Four Backs climb from the tile's page to the top page (Tiles, the menu,
# Menus, the top), its cursor on Menus; seven Downs reach Discard changes, its last row (General,
# Background, Menus, Titles, Highlight, Scroll indicators, Clock, Screensaver, Controls, a divider, Discard
# changes, as master's f50-discard counts them; no 3c task adds a top-page row).
i85_clean
cfg=$(writable_config f85-icons)
XDG_DATA_DIRS=$I85_DATA STREAMFLEX_TEST_OPEN_TILE=Main:0 CFG=$cfg run_keys f85-discard Menu Down Return Return \
    BackSpace BackSpace BackSpace BackSpace Down Down Down Down Down Down Down Return BackSpace
log=$out/f85-discard.log
ok=1
grep -qx "Menus: saved the own icon of 'SF85 Tool' to $I85_PNG" "$log" \
    && grep -q 'Settings: discarded the changes' "$log" && [ ! -e "$I85_PNG" ] \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f85-icons.ini" "$cfg" && ran_clean f85-discard && ok=0
result "icons: Discard deletes the icon file the visit wrote and nothing uses (exit $(cat "$out/f85-discard.code"))" $ok
i85_clean

# SF85 Swap, whose own icon is /tmp/sf85-swap.svg: chosen, it is written as sf85-swap.png. Then the app's
# icon changes (the SVG written again in blue) and the Icon page is opened once more: its own icon is now
# another picture than the tile's file holds, so choosing it writes a new file, sf85-swap-2.png, which the
# tile takes; the old file is kept only when it holds the same plated picture (Opus re-review P3)
I85_SWAP_SVG=/tmp/sf85-swap.svg
i85_swap_svg() {
    printf '<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128"><rect x="8" y="8" width="112" height="112" rx="24" fill="%s"/></svg>\n' "$1" > "$I85_SWAP_SVG"
}
i85_swap_blue() { i85_swap_svg '#2850C8'; }
i85_clean
i85_swap_svg '#C83228'
cfg=$(writable_config f85-icons)
XDG_DATA_DIRS=$I85_DATA STREAMFLEX_TEST_OPEN_TILE=Main:4 CFG=$cfg run_keys f85-changed Menu Down Return Return \
    +i85_swap_blue Return Return $I85_BACK
log=$out/f85-changed.log
ok=1
grep -qx "Menus: 'SF85 Swap' in 'Main': its own icon is $I85_SWAP_SVG" "$log" \
    && precedes "$log" "Menus: saved the own icon of 'SF85 Swap' to $I85_ICONS/sf85-swap.png" \
        "Menus: saved the own icon of 'SF85 Swap' to $I85_ICONS/sf85-swap-2.png" \
    && grep -qx "Menus: 'SF85 Swap' in 'Main' now has the icon $I85_ICONS/sf85-swap-2.png, from its own icon" "$log" \
    && ! grep -q "^Menus: 'SF85 Swap' in 'Main' keeps its own icon " "$log" \
    && [ -f "$I85_ICONS/sf85-swap.png" ] && [ "$(ls -A "$I85_ICONS" | wc -l)" = 2 ] \
    && grep -qx "Entry5=SF85 Swap;$I85_ICONS/sf85-swap-2.png;$I85_DESKTOP/sf85-swap.desktop" "$cfg" \
    && ran_clean f85-changed && ok=0
result "icons: its own icon chosen again once the app's icon changed is written as a new file, not the old one kept (exit $(cat "$out/f85-changed.code"))" $ok
grep -E "^Menus:" "$log" | sed 's/^/      /'
rm -f "$I85_SWAP_SVG"
i85_clean
