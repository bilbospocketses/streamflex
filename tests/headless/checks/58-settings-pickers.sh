# The pickers (3b): a colour typed in the hex editor and shown in the preview, the command picker,
# the default menu's list, and the contrast warning

# Background > Colour > Custom: #000000 becomes #102030 one digit at a time, the preview shows it,
# and it saves
shows_hex() { look "$1" "$2" hex 'Settings: previewing #102030' 30,30=16,32,48; }
hex_keys="Down Down Down Down Return Up Right Right Up Up Right Right Up Up Up"
cfg=$(writable_config f60-colour)
CFG=$cfg run_keys f58-hex Menu Down Return Down Return $hex_keys +shows_hex Return BackSpace BackSpace
ok=1
grep -qx 'Color=#102030' "$cfg" && grep -q 'Settings: \[Background\] Color #000000 -> #102030' "$out/f58-hex.log" \
    && grep -qx 'hex yes' "$out/f58-hex.seen" && ran_clean f58-hex && ok=0
result "pickers: a colour typed in the hex editor shows in the preview and saves (exit $(cat "$out/f58-hex.code"))" $ok

# Back in the colour picker puts the colour back: nothing is saved
CFG=$FX/f60-colour.ini run_keys f58-colourback Menu Down Return Down Return Right Right BackSpace BackSpace BackSpace
ok=1
grep -q 'Settings: previewing #33383D' "$out/f58-colourback.log" && grep -q 'Settings: nothing changed' "$out/f58-colourback.log" \
    && ran_clean f58-colourback && ok=0
result "pickers: Back in the colour picker puts the colour back (exit $(cat "$out/f58-colourback.code"))" $ok

# General > Startup command: the command picker lists None, the special commands, the submenus and
# the entries' commands; Quit StreamFlex is the tenth row. General > Default menu: Games from its list.
cfg=$(writable_config f58-pickers)
CFG=$cfg run_keys f58-command Menu Return Down Down Down Down Down Down Down Down Down Return \
    Down Down Down Down Down Down Down Down Down Return Up Up Up Up Up Up Up Up Up Return Down Return BackSpace BackSpace
ok=1
grep -qx 'StartupCmd=:quit' "$cfg" && grep -qx 'DefaultMenu=Games' "$cfg" \
    && grep -q 'Settings: the command picker lists 16 rows' "$out/f58-command.log" \
    && ran_clean f58-command && ok=0
result "pickers: a command and a default menu chosen from their lists save (exit $(cat "$out/f58-command.code"))" $ok
grep -E 'Settings: (the command picker|\[General\])' "$out/f58-command.log" | sed 's/^/      /'

# Titles > Colour: black titles on the black background are 1:1, and the caption says so
CFG=$FX/f60-colour.ini run_keys f58-contrast Menu Down Down Down Return Down Down Down Return Menu
ok=1
grep -q 'Settings: the note under the preview says White #FFFFFF' "$out/f58-contrast.log" \
    && ! grep -q 'Low contrast' "$out/f58-contrast.log" && ran_clean f58-contrast && ok=0
result "pickers: white titles on black raise no contrast warning (exit $(cat "$out/f58-contrast.code"))" $ok
CFG=$FX/f60-colour.ini run_keys f58-lowcontrast Menu Down Down Down Return Down Down Down Return Up Up Up Left Left Left Left Menu
ok=1
grep -q 'Settings: the note under the preview says Black #000000 · Low contrast: 1.0:1 against the background' "$out/f58-lowcontrast.log" \
    && ran_clean f58-lowcontrast && ok=0
result "pickers: black titles on black warn of low contrast (exit $(cat "$out/f58-lowcontrast.code"))" $ok

# ---------------------------------------------------------------------------------------------------
# Beyond the plan's five: each check below proves a branch the five leave unexercised. The debug log
# says what a picker shows: its path ("page ..."), its key hint, the list's rows on show and the row
# under its cursor, the colour picker's Custom row and where its swatches are drawn.
ARROW=$(printf ' \xE2\x80\xBA ')
LEFT_MARK=$(printf '\xE2\x80\xB9')
RIGHT_MARK=$(printf '\xE2\x80\xBA')
P58_DOT=$(printf '\xC2\xB7')
P58_LOW='Low contrast: 2.5:1 against the background'
p58_downs() { local i; for i in $(seq "$1"); do printf 'Down '; done; }

# The background colour's own picker is not a title's or the clock's colour: it warns of nothing,
# though every colour it previews is the background it is previewed on
ok=1
grep -q 'Settings: the note under the preview says Black #000000' "$out/f58-hex.log" && ! grep -q 'Low contrast' "$out/f58-hex.log" \
    && ! grep -q 'Low contrast' "$out/f58-colourback.log" && ok=0
result "pickers: the background colour's picker warns of no contrast" $ok

# The hex editor: the settings key leaves it without choosing, so the colour typed is not kept
# (nothing is saved); Back leaves the editor for the grid, where the Custom row keeps the colour typed
# while the cursor is on it and shows the colour the picker opened with while it is not. In the
# editor the note calls a colour Custom, even one a swatch has (#000000 as it opens).
cfg=$(writable_config f60-colour)
# shellcheck disable=SC2046
CFG=$cfg run_keys f58-hexhome Menu Down Return Down Return $(p58_downs 4) Return Up BackSpace Up Down Return Up Menu
log=$out/f58-hexhome.log
custom=$(grep -o 'Settings: the Custom row reads #[0-9A-F]*' "$log" | sed 's/.* //' | tr '\n' ' ')
ok=1
[ "$custom" = '#000000 #100000 #000000 #100000 #200000 ' ] && grep -q 'Settings: previewing #200000' "$log" \
    && grep -q 'Settings: the note under the preview says Custom #000000' "$log" \
    && grep -q 'Settings: the note under the preview says Custom #200000' "$log" \
    && grep -qF "Settings: page Settings${ARROW}Background${ARROW}Colour" "$log" \
    && grep -qF "Settings: the key hint reads Arrows edit the digits $P58_DOT OK keeps $P58_DOT Back returns" "$log" \
    && grep -qF "Settings: the key hint reads Arrows move $P58_DOT OK chooses $P58_DOT Back cancels" "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f60-colour.ini" "$cfg" && ran_clean f58-hexhome && ok=0
result "pickers: the settings key leaves the hex editor keeping nothing, and the Custom row follows the cursor (exit $(cat "$out/f58-hexhome.code"))" $ok
echo "      the Custom row read: ${custom:-nothing}"

# The preview is applied only when the colour it shows changes: in the hex editor, Right, Right and
# Left choose a digit and leave the colour as it is, and so do opening the editor and leaving it
# (Up on the second digit then shows #010000, which proves the digit moved). What was last previewed
# is forgotten as the picker opens again: Teal, the last colour previewed before Back, is previewed
# again by the first Down in the picker opened anew.
# shellcheck disable=SC2046
CFG=$FX/f60-colour.ini run_keys f58-digits Menu Down Return Down Return $(p58_downs 4) Return Right Right Left Up \
    BackSpace Up Up Up BackSpace Return Down BackSpace BackSpace BackSpace
log=$out/f58-digits.log
previews=$(grep -o 'Settings: previewing #[0-9A-F]*' "$log" | sed 's/.* //' | tr '\n' ' ')
ok=1
[ "$previews" = '#07606C #808080 #80C040 #000000 #010000 #80C040 #808080 #07606C #07606C ' ] \
    && [ "$(grep -c 'Settings: the colour picker put \[Background\] Color back' "$log")" = 2 ] \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f58-digits && ok=0
result "pickers: a key that leaves the previewed colour as it is applies nothing (exit $(cat "$out/f58-digits.code"))" $ok
echo "      previewed: ${previews:-nothing}"

# The swatches as drawn: Red in its place, the colour the picker opened with (White) marked in its
# middle with its opposite and outlined, as the cursor is on it, and the cell of a swatch the cursor
# is not on left without an outline
p58_swatches() {
    local name=$1 pid=$2 line c ox oy seen=no
    xdotool search --name '^StreamFlex$' windowmove %@ 0 0 windowsize %@ 1920 1080 > /dev/null 2>&1
    if wait_line 'Settings: the colour picker draws' "$pid"; then
        line=$(grep -o 'Settings: the colour picker draws [0-9]* px cells from [0-9]*,[0-9]*' "$LOG" | tail -1)
        read -r c ox oy <<< "$(sed 's/.* draws \([0-9]*\) px cells from \([0-9]*\),\([0-9]*\)/\1 \2 \3/' <<< "$line")"
        [ -n "${oy:-}" ] && screen_shows "$name" swatches \
            "$((ox + 2 * c + c / 2)),$((oy + 2 * c + c / 2))=208,48,48" \
            "$((ox + 4 * c + c / 2)),$((oy + c + c / 2))=0,0,0" \
            "$((ox + 4 * c + 5)),$((oy + c + 5))=255,255,255" \
            "$((ox + 4 * c)),$((oy + c + c / 2))=231,232,233" \
            "$((ox + 2 * c)),$((oy + 2 * c + c / 2))=11,22,32" && seen=yes
    fi
    echo "swatches $seen" >> "$out/$name.seen"
}
CFG=$FX/f60-colour.ini run_keys f58-swatches Menu Down Down Down Return Down Down Down Return +p58_swatches Menu
ok=1
grep -qx 'swatches yes' "$out/f58-swatches.seen" && ran_clean f58-swatches && ok=0
result "pickers: the swatches are drawn in their colours, the current one marked and the cursor's outlined (exit $(cat "$out/f58-swatches.code"))" $ok
sed 's/^/      /' "$out/f58-swatches.pixels" 2> /dev/null | tail -6

# On a short, wide screen (a second X display, 3840 x 480, where the column is a fifth of the width
# and so the swatches would be large) the swatches shrink so that they, the Custom row and the hex
# editor all end above the key hint. The log gives where the editor's row would end and the bottom.
Xvfb :98 -screen 0 3840x480x24 -ac > /dev/null 2>&1 &
p58_xvfb=$!
for i in $(seq 100); do DISPLAY=:98 xdotool getdisplaygeometry > /dev/null 2>&1 && break; sleep 0.2; done
p58_short=()
for word in "${TESTER[@]}"; do
    case $word in
        DISPLAY=:99) p58_short+=(DISPLAY=:98) ;;
        *) p58_short+=("$word") ;;
    esac
done
# shellcheck disable=SC2046
( export DISPLAY=:98; TESTER=("${p58_short[@]}")
  CFG=$FX/f60-colour.ini run_keys f58-short Menu Down Return Down Return $(p58_downs 4) Return )
kill "$p58_xvfb" 2> /dev/null; wait "$p58_xvfb" 2> /dev/null
log=$out/f58-short.log
line=$(grep -o 'Settings: the colour picker draws [0-9]* px cells from [0-9]*,[0-9]*, down to [0-9]* of [0-9]*' "$log" | tail -1)
read -r p58_end p58_bottom <<< "$(sed 's/.* down to \([0-9]*\) of \([0-9]*\)/\1 \2/' <<< "$line")"
ok=1
[ -n "${p58_bottom:-}" ] && [ "$p58_end" -le "$p58_bottom" ] \
    && grep -qF "Settings: the key hint reads Arrows edit the digits $P58_DOT" "$log" \
    && ran_clean f58-short && ok=0
result "pickers: on a short, wide screen the swatches end above the key hint (exit $(cat "$out/f58-short.code"))" $ok
echo "      ${line:-the cells of the colour picker were never logged}"

# The hex editor's one-line key hint fits the column, uncut, on the smallest screens: 1280 x 720,
# the smallest the specs lay the screens out for, and 1280 x 800, whose taller text (a share of the
# height) in the same column (a share of the width) fits closer still. Each runs on a second X
# display of its size; the harness build logs each hint's width beside the column's.
for p58_size in 1280x720 1280x800; do
    name=f58-hint$p58_size
    # shellcheck disable=SC2046
    CFG=$FX/f60-colour.ini run_keys_at "$p58_size" "$name" Menu Down Return Down Return $(p58_downs 4) Return
    log=$out/$name.log
    line=$(grep -o 'Test hook: the key hint is [0-9]* px wide, in a column [0-9]* px wide' "$log" | tail -1)
    read -r p58_wide p58_column <<< "$(sed 's/.* is \([0-9]*\) px wide, in a column \([0-9]*\) px wide/\1 \2/' <<< "$line")"
    ok=1
    grep -qx "Resolution: *$p58_size" "$log" && grep -q 'Settings: the note under the preview says Custom #000000' "$log" \
        && [ -n "${p58_column:-}" ] && [ "$p58_wide" -le "$p58_column" ] && ran_clean "$name" && ok=0
    result "pickers: at $p58_size the hex editor's key hint fits the column (exit $(cat "$out/$name.code"))" $ok
    echo "      $(grep -o 'Settings: the key hint reads .*' "$log" | tail -1): ${line:-no width logged}"
    # The same run's first hint, the top page's, as settings open on it
    line=$(grep -o 'Test hook: the key hint is [0-9]* px wide, in a column [0-9]* px wide' "$log" | head -1)
    read -r p58_wide p58_column <<< "$(sed 's/.* is \([0-9]*\) px wide, in a column \([0-9]*\) px wide/\1 \2/' <<< "$line")"
    ok=1
    grep -qx "Resolution: *$p58_size" "$log" && grep -m1 'Settings: the key hint reads ' "$log" | grep -qF 'OK opens' \
        && [ -n "${p58_column:-}" ] && [ "$p58_wide" -le "$p58_column" ] && ran_clean "$name" && ok=0
    result "pickers: at $p58_size the top page's key hint fits the column (exit $(cat "$out/$name.code"))" $ok
    echo "      $(grep -o 'Settings: the key hint reads .*' "$log" | head -1): ${line:-no width logged}"
    # The folder browser's, as it opens from Background > Image (Mode turned to Image first): the
    # first hint logged after it says where it is browsing
    name=f58-browse$p58_size
    CFG=$FX/f60-colour.ini run_keys_at "$p58_size" "$name" Menu Down Return Right Down Return
    log=$out/$name.log
    p58_browsing=$(sed -n '/^Settings: browsing /,$p' "$log")
    line=$(grep -o 'Test hook: the key hint is [0-9]* px wide, in a column [0-9]* px wide' <<< "$p58_browsing" | head -1)
    read -r p58_wide p58_column <<< "$(sed 's/.* is \([0-9]*\) px wide, in a column \([0-9]*\) px wide/\1 \2/' <<< "$line")"
    ok=1
    grep -qx "Resolution: *$p58_size" "$log" && grep -m1 'Settings: the key hint reads ' <<< "$p58_browsing" | grep -qF 'Back goes up' \
        && [ -n "${p58_column:-}" ] && [ "$p58_wide" -le "$p58_column" ] && ran_clean "$name" && ok=0
    result "pickers: at $p58_size the folder browser's key hint fits the column (exit $(cat "$out/$name.code"))" $ok
    echo "      $(grep -o 'Settings: the key hint reads .*' <<< "$p58_browsing" | head -1): ${line:-no width logged}"
done

# Home (a hotkey for :home in these fixtures) and the settings key close the picker before they
# close settings: with a save that fails (a read-only config), the failure page that follows takes
# the keys, and Leave without saving closes. A list picker, then the colour picker, whose previewed
# colour goes back first.
cfg=$(writable_config f58-custom)
chmod 444 "$cfg"
# shellcheck disable=SC2046
CFG=$cfg run_keys f58-listhome Menu Return Right $(p58_downs 9) Return Down Home Down Return
ok=1
grep -q "Couldn't save to $cfg" "$out/f58-listhome.log" && grep -q 'Settings: leaving without saving' "$out/f58-listhome.log" \
    && cmp -s "$FX/f58-custom.ini" "$cfg" && ran_clean f58-listhome && ok=0
result "pickers: Home closes a list picker before it closes settings (exit $(cat "$out/f58-listhome.code"))" $ok
cfg=$(writable_config f58-home)
chmod 444 "$cfg"
CFG=$cfg run_keys f58-colourhome Menu Down Return Down Right Return Right Home Down Return
log=$out/f58-colourhome.log
ok=1
grep -q 'Settings: previewing #33383D' "$log" && grep -q 'Settings: the colour picker put \[Background\] Color back' "$log" \
    && grep -q "Couldn't save to $cfg" "$log" && grep -q 'Settings: leaving without saving' "$log" \
    && cmp -s "$FX/f58-home.ini" "$cfg" && ran_clean f58-colourhome && ok=0
result "pickers: Home puts the previewed colour back and closes the colour picker before settings (exit $(cat "$out/f58-colourhome.code"))" $ok

# A command in the file that no row gives is pinned first as Custom, and choosing it keeps it; a
# :quit hotkey is ignored while a picker is open; Back leaves a list without choosing; Home from a
# list goes to the default menu (settings were opened over Games)
CFG=$FX/f58-custom.ini run_keys f58-custom Return Menu Return $(p58_downs 10) Return Escape Return Return Down BackSpace Return Home
log=$out/f58-custom.log
ok=1
grep -q "Settings: the list's cursor reads Custom: :fork true" "$log" \
    && grep -q "Settings: ignoring ':quit' while a picker is open" "$log" \
    && grep -q 'Settings: \[General\] QuitCmd is unchanged' "$log" && ! grep -q 'Settings: \[General\] QuitCmd .* -> ' "$log" \
    && grep -q 'Settings: nothing changed' "$log" \
    && sed -n '/Settings: nothing changed/,$p' "$log" | grep -q "Loading menu 'Main'" && ran_clean f58-custom && ok=0
result "pickers: a custom command is kept, a hotkey ignored, Back chooses nothing, and Home goes home (exit $(cat "$out/f58-custom.code"))" $ok
grep -E "Settings: (the list's cursor|ignoring|\[General\])|Loading menu" "$log" | sed 's/^/      /'

# Left and Right page through a list: two Rights reach the last row (the entry Kodi's command, by its
# title), two Lefts come back to the first. The rows on show follow the cursor down and back up.
# Startup command takes Kodi's command, then None, which removes its key; Quit command takes :left.
cfg=$(writable_config f58-pickers)
# shellcheck disable=SC2046
CFG=$cfg run_keys f58-page Menu Return $(p58_downs 9) Return Right Right Return Return Left Left Return \
    Down Return Right Right Left Left Down Return BackSpace BackSpace
log=$out/f58-page.log
shown=$(sed -n "/Settings: page Settings${ARROW}General${ARROW}Quit command/,\$p" "$log" \
        | grep -o 'Settings: the list shows rows [0-9]* to [0-9]* of 16' | sed 's/.*rows //; s/ of 16//' | tr '\n' '|')
ok=1
grep -q 'Settings: \[General\] StartupCmd (none) -> kodi --standalone' "$log" \
    && grep -q 'Settings: \[General\] StartupCmd kodi --standalone -> (none)' "$log" \
    && ! grep -q '^StartupCmd' "$cfg" && grep -qx 'QuitCmd=:left' "$cfg" \
    && grep -q "Settings: the list's cursor reads Kodi" "$log" \
    && grep -qE '(^|\|)[1-9][0-9]* to 15\|' <<< "$shown" && grep -qE '\|0 to [0-9]+\|$' <<< "$shown" \
    && grep -qF "Settings: the key hint reads Left and right page $P58_DOT OK chooses $P58_DOT Back cancels" "$log" \
    && grep -qF "Settings: page Settings${ARROW}General${ARROW}Quit command" "$log" \
    && ran_clean f58-page && ok=0
result "pickers: Left and Right page through a list, whose rows follow the cursor (exit $(cat "$out/f58-page.code"))" $ok
echo "      Quit command's rows on show: ${shown:-none}"

# The Device list names the pads present, and a pad plugged in while it is open joins it at once,
# with the cursor kept on the pad it was on: the second pad is then one Down away, and chosen
p58_plug_in() { : > /tmp/pad-plug; wait_line 'Test hook: pad plugged in' "$2"; sleep 1; }
p58_pull_out() { rm -f /tmp/pad-plug; wait_line 'Test hook: pad unplugged' "$2"; sleep 1; }
rm -f /tmp/pad-plug /tmp/pad-none
cfg=$(writable_config f55-frame)
# shellcheck disable=SC2046
STREAMFLEX_TEST_PAD=/tmp/pad-none STREAMFLEX_TEST_PAD_PLUG=/tmp/pad-plug CFG=$cfg WAIT_FOR='Gamepad connected' \
    run_keys f58-device Menu $(p58_downs 8) Return Down Return Down Return Down +p58_plug_in Down Return BackSpace BackSpace BackSpace
ok=1
grep -qx 'DeviceIndex=1' "$cfg" && grep -q 'Settings: the pads changed, so the list was made again' "$out/f58-device.log" \
    && ! grep -q 'Settings: the command picker lists' "$out/f58-device.log" && ran_clean f58-device && ok=0
result "pickers: a pad plugged in while the Device list is open joins it, the cursor staying put (exit $(cat "$out/f58-device.code"))" $ok
grep -E "Test hook: pad|Settings: (the pads|the list's cursor|\[Gamepad\])" "$out/f58-device.log" | sed 's/^/      /'
rm -f /tmp/pad-plug

# A device index in the file with no pad at it is pinned as the Device row names it; a pad pulled
# out from under the cursor leaves the list, and the cursor goes back to the file's value, which OK
# then keeps
: > /tmp/pad-plug
chmod 666 /tmp/pad-plug
cfg=$(writable_config f58-device3)
# shellcheck disable=SC2046
STREAMFLEX_TEST_PAD_PLUG=/tmp/pad-plug CFG=$cfg WAIT_FOR='Test hook: pad plugged in' \
    run_keys f58-unplug Menu $(p58_downs 8) Return Down Return Down Return Down Down +p58_pull_out Return BackSpace BackSpace BackSpace
log=$out/f58-unplug.log
ok=1
grep -q "Settings: the list's cursor reads Pad 3 (not connected)" "$log" \
    && sed -n '/Test hook: pad unplugged/,$p' "$log" | grep -q 'Settings: the pads changed, so the list was made again' \
    && grep -q 'Settings: \[Gamepad\] DeviceIndex is unchanged' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f58-device3.ini" "$cfg" && ran_clean f58-unplug && ok=0
result "pickers: a pad pulled out leaves the Device list, and the file's own index is pinned (exit $(cat "$out/f58-unplug.code"))" $ok
grep -E "Test hook: pad|Settings: (the pads|the list's cursor|\[Gamepad\])" "$log" | sed 's/^/      /'
rm -f /tmp/pad-plug

# A list made again keeps the file's pinned index while the cursor stays on another row that is
# still there: with the cursor on Any as the pad is pulled out, Up reaches Pad 3 (not connected)
# again, and OK keeps it
: > /tmp/pad-plug
chmod 666 /tmp/pad-plug
cfg=$(writable_config f58-device3)
# shellcheck disable=SC2046
STREAMFLEX_TEST_PAD_PLUG=/tmp/pad-plug CFG=$cfg WAIT_FOR='Test hook: pad plugged in' \
    run_keys f58-keeppin Menu $(p58_downs 8) Return Down Return Down Return Down +p58_pull_out Up Return BackSpace BackSpace BackSpace
log=$out/f58-keeppin.log
cursor=$(sed -n '/Test hook: pad unplugged/,$p' "$log" | grep -o "Settings: the list's cursor reads .*" | sed 's/.*reads //' | tr '\n' '|')
ok=1
sed -n '/Test hook: pad unplugged/,$p' "$log" | grep -q 'Settings: the pads changed, so the list was made again' \
    && [ "$cursor" = 'Any|Pad 3 (not connected)|' ] \
    && grep -q 'Settings: \[Gamepad\] DeviceIndex is unchanged' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f58-device3.ini" "$cfg" && ran_clean f58-keeppin && ok=0
result "pickers: a Device list made again keeps the file's pinned index beside the cursor's row (exit $(cat "$out/f58-keeppin.code"))" $ok
echo "      the cursor read, from the pull: ${cursor:-nothing}"
rm -f /tmp/pad-plug

# Out of memory (STREAMFLEX_TEST_FAIL, the harness's build only): a list that cannot be made again
# when the pads change closes, and the keys go back to the page
# shellcheck disable=SC2046
STREAMFLEX_TEST_FAIL=pads STREAMFLEX_TEST_PAD_PLUG=/tmp/pad-plug CFG=$FX/f55-frame.ini \
    run_keys f58-padfail Menu $(p58_downs 8) Return Down Return Down Return +p58_plug_in Down Menu
log=$out/f58-padfail.log
ok=1
grep -q 'Settings: the pads changed, and the list could not be made again: out of memory' "$log" \
    && sed -n '/the list could not be made again/,$p' "$log" | grep -q "Settings: the cursor's row reads Mappings file" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f58-padfail && ok=0
result "pickers: a Device list that cannot be made again closes (exit $(cat "$out/f58-padfail.code"))" $ok
rm -f /tmp/pad-plug

# Only a Device list is made again when the pads change: a pad plugged in while General > Startup
# command's list is open leaves that list as it is, its cursor on None until Down moves it
# shellcheck disable=SC2046
STREAMFLEX_TEST_PAD_PLUG=/tmp/pad-plug CFG=$FX/f58-pickers.ini \
    run_keys f58-cmdplug Menu Return $(p58_downs 9) Return +p58_plug_in Down BackSpace Menu
log=$out/f58-cmdplug.log
cursor=$(sed -n '/Settings: opened the picker for \[General\] StartupCmd/,$p' "$log" \
         | grep -o "Settings: the list's cursor reads .*" | sed 's/.*reads //' | tr '\n' '|')
ok=1
sed -n '/Test hook: pad plugged in/,$p' "$log" | grep -q 'Gamepad connected' \
    && ! grep -q 'Settings: the pads changed' "$log" && [ "$cursor" = 'None|Left|' ] \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f58-cmdplug && ok=0
result "pickers: a pad plugged in leaves an open command list alone (exit $(cat "$out/f58-cmdplug.code"))" $ok
echo "      the cursor read: ${cursor:-nothing}"
rm -f /tmp/pad-plug

# A list that cannot be made, cannot take its rows, or cannot pin the file's value does not open:
# it says why, and the keys stay with the page
for fail in list:f58-pickers:9:'Quit command' rows:f58-pickers:9:'Quit command' select:f58-custom:10:'Startup command'; do
    IFS=: read -r step fixture downs next <<< "$fail"
    name=f58-fail$step
    # shellcheck disable=SC2046
    STREAMFLEX_TEST_FAIL=$step CFG=$FX/$fixture.ini run_keys "$name" Menu Return $(p58_downs "$downs") Return \
        $([ "$step" = select ] && echo Up || echo Down) Menu
    log=$out/$name.log
    ok=1
    grep -q 'Settings: the list cannot open: out of memory' "$log" && ! grep -q 'Settings: opened the picker' "$log" \
        && ! grep -q 'Settings: the command picker lists' "$log" \
        && sed -n '/the list cannot open/,$p' "$log" | grep -q "Settings: the cursor's row reads $next" \
        && grep -q 'Settings: nothing changed' "$log" && ran_clean "$name" && ok=0
    result "pickers: a list whose $step step runs out of memory does not open (exit $(cat "$out/$name.code"))" $ok
done

# Quitting with a list open closes it as settings close. LeakSanitizer cannot see a list left open,
# which a static still points to, so the log says the picker closed.
# shellcheck disable=SC2046
CFG=$FX/f58-pickers.ini run_keys f58-quitopen Menu Return $(p58_downs 9) Return
ok=1
grep -q 'Settings: opened the picker for \[General\] StartupCmd' "$out/f58-quitopen.log" \
    && sed -n '/Quitting program/,$p' "$out/f58-quitopen.log" | grep -q 'Settings: closed the picker for \[General\] StartupCmd' \
    && ran_clean f58-quitopen && ok=0
result "pickers: quitting with a list open closes it cleanly (exit $(cat "$out/f58-quitopen.code"))" $ok

# The contrast warning under an image, measured as it loads (blue.png, luminance 0.074): black
# titles and a black clock read 2.5:1, on the row as well as in the picker
CFG=$FX/f58-image.ini run_keys f58-image Menu Down Down Down Return Down Down Down BackSpace \
    Down Down Down Return $(p58_downs 6) Menu
log=$out/f58-image.log
ok=1
grep -q 'Background: the image on show has a mean luminance of 0.074' "$log" \
    && sed -n "/Settings: page Settings${ARROW}Titles\$/,/Settings: page Settings\$/p" "$log" \
       | grep -qF "Settings: the note under the preview says $P58_LOW" \
    && sed -n "/Settings: page Settings${ARROW}Clock\$/,/Settings closed/p" "$log" \
       | grep -qF "Settings: the note under the preview says $P58_LOW" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f58-image && ok=0
result "pickers: black titles and clock over a blue image warn of 2.5:1 (exit $(cat "$out/f58-image.code"))" $ok
grep -E 'Background: the image|Low contrast' "$log" | sed 's/^/      /'

# An image that could not be measured (STREAMFLEX_TEST_NO_LUMINANCE, the harness's build only)
# warns of nothing
STREAMFLEX_TEST_NO_LUMINANCE=1 CFG=$FX/f58-image.ini run_keys f58-nolum Menu Down Down Down Return Down Down Down Menu
log=$out/f58-nolum.log
ok=1
grep -q 'Background: the image on show could not be measured' "$log" \
    && grep -qF "Settings: the cursor's row reads Colour: $LEFT_MARK Black $RIGHT_MARK" "$log" \
    && ! grep -q 'Low contrast' "$log" && ran_clean f58-nolum && ok=0
result "pickers: an image that could not be measured warns of nothing (exit $(cat "$out/f58-nolum.code"))" $ok

# A single image in a slideshow folder is measured too (red.png, 0.102)
CFG=$FX/f30-one.ini run_quick f58-one
ok=1
grep -q 'Background: the image on show has a mean luminance of 0.102' "$out/f58-one.log" && ran_clean f58-one && ok=0
result "pickers: a slideshow folder's only image is measured (exit $(cat "$out/f58-one.code"))" $ok

# The overlay lies over the background: white titles on black under a 50% white overlay read 1.9:1.
# A transparent background warns of nothing.
cfg=$(writable_config f58-overlay)
CFG=$cfg run_keys f58-overlay Menu Down Down Down Return Down Down Down BackSpace Up Up Return Right Right Right \
    BackSpace Down Down Return Down Down Down Menu
log=$out/f58-overlay.log
ok=1
sed -n '/Settings: \[Background\] Mode Color -> Image/q; p' "$log" \
       | grep -qF 'Settings: the note under the preview says Low contrast: 1.9:1 against the background' \
    && sed -n '/Settings: \[Background\] Mode Slideshow -> Transparent/,$p' "$log" \
       | grep -qF "Settings: the cursor's row reads Colour: $LEFT_MARK White $RIGHT_MARK" \
    && ! sed -n '/Settings: \[Background\] Mode Slideshow -> Transparent/,$p' "$log" | grep -q 'Low contrast' \
    && grep -qx 'Mode=Transparent' "$cfg" && ran_clean f58-overlay && ok=0
result "pickers: the overlay counts in the contrast, and a transparent background warns of nothing (exit $(cat "$out/f58-overlay.code"))" $ok
grep -E 'Low contrast|Mode .* -> ' "$log" | sed 's/^/      /'

# A slideshow measures each image on its loader thread, and the main thread takes the measure with
# the image, whether it cuts to it or fades it in. ~/duo holds blue.png and green.png, which take
# turns; with black titles, only blue's 2.5:1 warns, and the warning follows the image live.
rm -rf "$TESTER_HOME/duo"
mkdir -p "$TESTER_HOME/duo"
cp "$TESTER_HOME/Pictures/blue.png" "$TESTER_HOME/Pictures/green.png" "$TESTER_HOME/duo/"
chown -R tester:tester "$TESTER_HOME/duo"
p58_measured() {
    local i
    for i in $(seq 100); do
        [ "$(grep -c 'Background: the image on show has a mean luminance of' "$LOG" 2> /dev/null)" -ge 2 ] && return
        sleep 0.2
    done
}
p58_rest() { sleep 11; }
p58_values() { grep -o 'Background: the image on show has a mean luminance of [0-9.]*' "$1" | sed 's/.* //'; }
WAIT_FOR='Background set up: Slideshow' CFG=$FX/f58-slide.ini \
    run_keys f58-slide +p58_measured Menu Down Down Down Return Down Down Down +p58_rest Menu
log=$out/f58-slide.log
values=$(p58_values "$log")
lowest=$(sort -n <<< "$values" | head -1)
ok=1
[ "$(grep -c . <<< "$values")" -ge 3 ] && [ "$(sort -u <<< "$values" | grep -c .)" = 2 ] \
    && ! grep -q 'could not be measured' "$log" \
    && awk -v low="$lowest" '
        /Background: the image on show has a mean luminance of/ { last = $NF }
        /the note under the preview says Low contrast/ {
            if (index($0, "Low contrast: 2.5:1") && last == low) warned++; else wrong++
        }
        END { exit !(warned >= 1 && wrong == 0) }' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f58-slide && ok=0
result "pickers: a slideshow's images are measured as they change, and the warning follows them (exit $(cat "$out/f58-slide.code"))" $ok
echo "      measured: $(tr '\n' ' ' <<< "$values")"
WAIT_FOR='Background set up: Slideshow' CFG=$FX/f58-fade.ini run_keys f58-fade +p58_measured
log=$out/f58-fade.log
values=$(p58_values "$log")
ok=1
[ "$(sort -u <<< "$values" | grep -c .)" = 2 ] \
    && sed -n '/Slideshow: fading in the next image/,$p' "$log" | grep -q 'Background: the image on show has a mean luminance of' \
    && ran_clean f58-fade && ok=0
result "pickers: an image faded in is measured as it takes over (exit $(cat "$out/f58-fade.code"))" $ok
echo "      measured: $(tr '\n' ' ' <<< "$values")"
