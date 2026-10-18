# The hold-OK shortcut (3c Task 14, spec "The hold-OK shortcut"): Return and the pad's A held past 0.8 s
# open settings at the tile's page, the key's repeats doing nothing in the menu or in settings; Back walks
# up to the top and closing lands on the tile; an empty menu's hold opens its Tiles page; a press let go
# before 0.8 s selects; a second OK during a press is not counted, nor are its repeats in the settings the
# first one's hold opened; a hold canceled by a pad pulled out or by the window losing the keyboard (the
# pad's A, and Return, whose release SDL sends just before it says so) opens nothing, and the next OK
# selects; settings opened by Menu during a hold (the pad's A, or Return) cancel it, so closing them
# with OK still down opens nothing, and its release does nothing; and settings opened by the pad's Y
# (:settings) during Return's hold cancel it, and Return's repeats, going on inside them, press nothing
# Timing-sensitive: 1 s and 0.5 s key and button holds against the 800 ms hold and the X server's key
# repeat, and keys that must come inside the hold's 800 ms

T86_ARROW=$(printf ' \xE2\x80\xBA ')
T86_TILES="Settings${T86_ARROW}Menus${T86_ARROW}Main${T86_ARROW}Tiles"
t86_hold_return() { xdotool keydown Return; sleep 1; xdotool keyup Return; sleep 0.5; }
t86_short_return() { xdotool keydown Return; sleep 0.5; xdotool keyup Return; sleep 1; }

# Return held 1 s on Bye: settings open at Bye's page, its repeats (it came more than once) do nothing,
# and its release does nothing; four Backs walk up to the top, the fifth closes, and Return then selects
# Bye, the tile the hold was on
CFG=$FX/f86-hold.ini run_keys f86-hold Right +t86_hold_return BackSpace BackSpace BackSpace BackSpace BackSpace Return
log=$out/f86-hold.log
ok=1
grep -qE "^Hold OK: held [0-9]+ ms, so settings open at tile 1 of menu 'Main'$" "$log" \
    && grep -qx "Hold OK: settings opened at a tile's page of menu 'Main'" "$log" \
    && grep -qF "Settings: page $T86_TILES$T86_ARROW" "$log" \
    && [ "$(grep -c 'Key Return (#D) detected' "$log")" -gt 2 ] \
    && [ "$(sed -n '/^Hold OK: settings opened at/,/^Key Backspace (#8) detected/p' "$log" | grep -c '^Settings: page ')" = 0 ] \
    && grep -qx 'Hold OK: let go after the hold opened settings, so it does nothing' "$log" \
    && grep -qx "Settings: page $T86_TILES" "$log" \
    && ! sed -n '1,/^Settings closed/p' "$log" | grep -q '^Selected Entry' \
    && sed -n '/^Settings closed/,$p' "$log" | grep -qx 'Title: Bye' && ran_clean f86-hold && ok=0
result "hold-ok: Return held on a tile opens its page, Back walks up, and closing lands on it (exit $(cat "$out/f86-hold.code"))" $ok
grep -E '^(Hold OK|Settings: page|Settings (opened|closed)|Title:)' "$log" | sed 's/^/      /'
echo "      Return came $(grep -c 'Key Return (#D) detected' "$log") times"

# Return held 0.5 s, under the hold: its release selects
CFG=$FX/f86-hold.ini run_keys f86-short Right +t86_short_return
log=$out/f86-short.log
ok=1
grep -qE '^Hold OK: let go after [0-9]+ ms, so it selects$' "$log" && grep -qx 'Title: Bye' "$log" \
    && ! grep -q 'Settings opened' "$log" && ran_clean f86-short && ok=0
result "hold-ok: Return let go under 0.8 s selects (exit $(cat "$out/f86-short.code"))" $ok

# The empty menu's hold opens its Tiles page; three Backs walk up to the top, the fourth closes
CFG=$FX/f86-hold.ini run_keys f86-holdempty Return +t86_hold_return BackSpace BackSpace BackSpace BackSpace
log=$out/f86-holdempty.log
ok=1
grep -qE "^Hold OK: held [0-9]+ ms, so settings open at tile -1 of menu 'Kids'$" "$log" \
    && grep -qx "Hold OK: settings opened at the Tiles page of menu 'Kids'" "$log" \
    && grep -qx "Settings: page Settings${T86_ARROW}Menus${T86_ARROW}Kids${T86_ARROW}Tiles" "$log" \
    && grep -q '^Settings closed' "$log" && ran_clean f86-holdempty && ok=0
result "hold-ok: OK held in an empty menu opens its Tiles page (exit $(cat "$out/f86-holdempty.code"))" $ok

# A second OK (F6) pressed while Return is held is not counted: Return's release selects once
t86_second() { xdotool keydown Return; sleep 0.1; xdotool key F6; xdotool keyup Return; sleep 1; }
CFG=$FX/f86-hold.ini run_keys f86-second Right +t86_second
log=$out/f86-second.log
ok=1
grep -qx 'Hold OK: a second OK while the first is held does nothing' "$log" \
    && [ "$(grep -c '^Selected Entry' "$log")" = 1 ] && grep -qx 'Title: Bye' "$log" && ran_clean f86-second && ok=0
result "hold-ok: a second OK while one is held is not counted (exit $(cat "$out/f86-second.code"))" $ok

# The pad's A held 1 s on Bye: settings open at its page, and A neither repeats into them nor acts as it
# is let go. The hold is timed from the launcher's press line, as check 48's are.
t86_hold_a() {
    : > /tmp/pad-a
    wait_line 'Gamepad ButtonA detected' "$2" && sleep 1
    rm -f /tmp/pad-a; sleep 0.5
}
rm -f /tmp/pad-a
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' \
    CFG=$FX/f86-hold.ini run_keys f86-padhold Right +t86_hold_a
log=$out/f86-padhold.log
ok=1
grep -qE "^Hold OK: held [0-9]+ ms, so settings open at tile 1 of menu 'Main'$" "$log" \
    && ! grep -q 'Test hook: pad ButtonA repeated' "$log" \
    && grep -qx 'Hold OK: let go after the hold opened settings, so it does nothing' "$log" \
    && ! grep -q '^Selected Entry' "$log" && ran_clean f86-padhold && ok=0
result "hold-ok: the pad's A held opens the tile's page, and never repeats (exit $(cat "$out/f86-padhold.code"))" $ok
rm -f /tmp/pad-a

# The pad's A tapped for one frame: its release selects
t86_tap_a() { : > /tmp/pad-a; wait_line 'Gamepad ButtonA detected' "$2"; rm -f /tmp/pad-a; sleep 1; }
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a STREAMFLEX_TEST_PAD_FRAMES=1 WAIT_FOR='Gamepad connected' \
    CFG=$FX/f86-hold.ini run_keys f86-padtap Right +t86_tap_a
log=$out/f86-padtap.log
ok=1
grep -qE '^Hold OK: let go after [0-9]+ ms, so it selects$' "$log" && grep -qx 'Title: Bye' "$log" \
    && ran_clean f86-padtap && ok=0
result "hold-ok: the pad's A tapped selects as it is let go (exit $(cat "$out/f86-padtap.code"))" $ok
rm -f /tmp/pad-a

# A second OK held (F6, its keyboard repeats coming) while Return is held past the hold: settings open at
# Bye's page, and F6's repeats never press OK there (no page opens, no keyboard opens on its Title)
t86_second_held() {
    xdotool keydown Return; sleep 0.1; xdotool keydown F6; sleep 1.5
    xdotool keyup F6; xdotool keyup Return; sleep 0.5
}
CFG=$FX/f86-hold.ini run_keys f86-secondheld Right +t86_second_held BackSpace BackSpace BackSpace BackSpace BackSpace
log=$out/f86-secondheld.log
ok=1
grep -qx 'Hold OK: a second OK while the first is held does nothing' "$log" \
    && grep -qE "^Hold OK: held [0-9]+ ms, so settings open at tile 1 of menu 'Main'$" "$log" \
    && [ "$(grep -c 'Key F6 (#4000003F) detected' "$log")" -gt 2 ] \
    && [ "$(sed -n '/^Hold OK: settings opened at/,/^Key Backspace (#8) detected/p' "$log" | grep -c '^Settings: page ')" = 0 ] \
    && ! grep -q 'Settings: opened the keyboard' "$log" && ! grep -q '^Selected Entry' "$log" \
    && ran_clean f86-secondheld && ok=0
result "hold-ok: a second OK held during a hold never repeats into the settings it opened (exit $(cat "$out/f86-secondheld.code"))" $ok
echo "      F6 came $(grep -c 'Key F6 (#4000003F) detected' "$log") times"

# The pad's A held on Bye, its pad pulled out 5 frames later with A still down (STREAMFLEX_TEST_PAD_UNPLUG):
# the hold is canceled, so nothing opens when 0.8 s have gone, and Return, the next OK, selects Bye
t86_unplug() { : > /tmp/pad-u; wait_line 'Test hook: the pad is pulled out' "$2" && sleep 1.2; }
rm -f /tmp/pad-u
STREAMFLEX_TEST_PAD=/tmp/pad-u STREAMFLEX_TEST_PAD_BUTTON=a STREAMFLEX_TEST_PAD_UNPLUG=5 WAIT_FOR='Gamepad connected' \
    CFG=$FX/f86-hold.ini run_keys f86-unplug Right +t86_unplug Return
log=$out/f86-unplug.log
ok=1
grep -qx 'Hold OK: a pad was pulled out, so the hold is canceled' "$log" \
    && ! grep -qE '^Hold OK: held [0-9]+ ms' "$log" && ! grep -q 'Settings opened' "$log" \
    && precedes "$log" 'Hold OK: a pad was pulled out' 'Hold OK: let go after' \
    && [ "$(grep -c '^Selected Entry' "$log")" = 1 ] && grep -qx 'Title: Bye' "$log" && ran_clean f86-unplug && ok=0
result "hold-ok: a pad pulled out while its A holds OK cancels the hold, and the next OK selects (exit $(cat "$out/f86-unplug.code"))" $ok
rm -f /tmp/pad-u

# The pad's A held on Bye, the window losing the keyboard 5 frames later (STREAMFLEX_TEST_UNFOCUS): the hold
# is canceled, A let go later does nothing, and Return then selects Bye
t86_unfocus() { : > /tmp/pad-f; wait_line 'Test hook: the window gets the keyboard back' "$2" && sleep 1.2; rm -f /tmp/pad-f; sleep 0.5; }
rm -f /tmp/pad-f
STREAMFLEX_TEST_PAD=/tmp/pad-f STREAMFLEX_TEST_PAD_BUTTON=a STREAMFLEX_TEST_UNFOCUS=5 WAIT_FOR='Gamepad connected' \
    CFG=$FX/f86-hold.ini run_keys f86-unfocus Right +t86_unfocus Return
log=$out/f86-unfocus.log
ok=1
grep -qx 'Hold OK: the window lost the keyboard, so the hold is canceled' "$log" \
    && ! grep -qE '^Hold OK: held [0-9]+ ms' "$log" && ! grep -q 'Settings opened' "$log" \
    && precedes "$log" 'Test hook: the window gets the keyboard back' 'Selected Entry' \
    && [ "$(grep -c '^Selected Entry' "$log")" = 1 ] && grep -qx 'Title: Bye' "$log" && ran_clean f86-unfocus && ok=0
result "hold-ok: the window losing the keyboard while the pad holds OK cancels the hold (exit $(cat "$out/f86-unfocus.code"))" $ok
rm -f /tmp/pad-f

# Return held on Bye, then the pad's A (a second OK, not counted), and 5 frames later the window loses the
# keyboard: SDL's release of Return, sent just before its focus event, cancels the hold rather than
# selecting; the real release later does nothing; Return tapped then selects Bye
t86_unfocuskey() {
    xdotool keydown Return; sleep 0.2; : > /tmp/pad-k
    wait_line 'Test hook: the window gets the keyboard back' "$2" && sleep 1.2
    rm -f /tmp/pad-k; xdotool keyup Return; sleep 0.5
}
rm -f /tmp/pad-k
STREAMFLEX_TEST_PAD=/tmp/pad-k STREAMFLEX_TEST_PAD_BUTTON=a STREAMFLEX_TEST_UNFOCUS=5 WAIT_FOR='Gamepad connected' \
    CFG=$FX/f86-hold.ini run_keys f86-unfocuskey Right +t86_unfocuskey Return
log=$out/f86-unfocuskey.log
ok=1
grep -qx 'Hold OK: a second OK while the first is held does nothing' "$log" \
    && grep -qx 'Hold OK: the window lost the keyboard, so the hold is canceled' "$log" \
    && precedes "$log" 'Test hook: the window loses the keyboard' 'Hold OK: the window lost the keyboard' \
    && ! grep -qE '^Hold OK: held [0-9]+ ms' "$log" && ! grep -q 'Settings opened' "$log" \
    && precedes "$log" 'Test hook: the window gets the keyboard back' 'Selected Entry' \
    && [ "$(grep -c '^Selected Entry' "$log")" = 1 ] && grep -qx 'Title: Bye' "$log" && ran_clean f86-unfocuskey && ok=0
result "hold-ok: the release SDL sends as the window loses the keyboard cancels a held Return, not selects (exit $(cat "$out/f86-unfocuskey.code"))" $ok
rm -f /tmp/pad-k

# The pad's A held on Bye, settings opened by Menu inside the hold's 0.8 s, which cancels the hold (user
# ruling 2026-10-16); a setting changed and Discard, which makes the pad's controls again; Menu closes with
# A still down, and nothing opens on that frame or after; A let go then does nothing
T86_DOWNS="Down Down Down Down Down Down Down Down Down Down Down Down"
t86_hold_a_start() { : > /tmp/pad-r; wait_line 'Gamepad ButtonA detected' "$2"; }
t86_let_go_a() { sleep 1.5; rm -f /tmp/pad-r; sleep 1; }
rm -f /tmp/pad-r
STREAMFLEX_TEST_PAD=/tmp/pad-r STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$FX/f86-hold.ini \
    run_keys f86-rebuilt Right +t86_hold_a_start Menu Return Down Right BackSpace $T86_DOWNS Return Menu +t86_let_go_a
log=$out/f86-rebuilt.log
ok=1
grep -qx 'Hold OK: settings opened another way, so the hold is canceled' "$log" \
    && grep -q 'Settings: discarded the changes' "$log" && grep -q '^Settings: the bindings now hold ' "$log" \
    && ! grep -q '^Hold OK: the pad' "$log" \
    && ! grep -qE '^Hold OK: held [0-9]+ ms' "$log" && [ "$(grep -c 'Settings opened' "$log")" = 1 ] \
    && ! grep -q '^Selected Entry' "$log" && ran_clean f86-rebuilt && ok=0
result "hold-ok: settings opened by Menu during the pad's hold cancel it, and closing them opens nothing (exit $(cat "$out/f86-rebuilt.code"))" $ok
grep -E '^(Hold OK|Settings (opened|closed|: discarded))' "$log" | sed 's/^/      /'
rm -f /tmp/pad-r

# Return held on Bye, settings opened by Menu inside the hold's 0.8 s, which cancels the hold; Menu closes
# them with Return still down, and nothing opens on that frame or after (no page past the top: not the
# hold's tile page); Return let go then selects nothing. Return does not repeat here: the X server repeats
# only the last key pressed, so Menu ends Return's repeat (f86-padopened has the repeats).
t86_keyopened() {
    xdotool keydown Return; sleep 0.2; xdotool key Menu; sleep 1
    xdotool key Menu; sleep 1.5; xdotool keyup Return; sleep 0.5
}
CFG=$FX/f86-hold.ini run_keys f86-keyopened Right +t86_keyopened
log=$out/f86-keyopened.log
ok=1
grep -qx 'Hold OK: settings opened another way, so the hold is canceled' "$log" \
    && ! grep -qF "Settings: page Settings${T86_ARROW}" "$log" \
    && ! grep -qE '^Hold OK: held [0-9]+ ms' "$log" && [ "$(grep -c 'Settings opened' "$log")" = 1 ] \
    && grep -q '^Settings closed' "$log" && ! grep -q '^Selected Entry' "$log" && ran_clean f86-keyopened && ok=0
result "hold-ok: settings opened by Menu during Return's hold cancel it, and closing them opens nothing (exit $(cat "$out/f86-keyopened.code"))" $ok
grep -E '^(Hold OK|Settings (opened|closed))' "$log" | sed 's/^/      /'

# Return held on Bye, settings opened by the pad's Y (bound to :settings) inside the hold's 0.8 s, which
# cancels the hold. No key is pressed after Return, so the X server keeps repeating it inside settings, and
# its repeats press nothing there (on the top page they would open General); Y let go does nothing; Menu
# closes settings (and ends Return's repeat), nothing opens on that frame or after, and Return let go then
# selects nothing
t86_padopened() {
    xdotool keydown Return; sleep 0.2; : > /tmp/pad-o
    wait_line 'Hold OK: settings opened another way' "$2" && sleep 1
    rm -f /tmp/pad-o; xdotool key Menu; sleep 1.5; xdotool keyup Return; sleep 0.5
}
rm -f /tmp/pad-o
STREAMFLEX_TEST_PAD=/tmp/pad-o STREAMFLEX_TEST_PAD_BUTTON=y WAIT_FOR='Gamepad connected' CFG=$FX/f86-hold.ini \
    run_keys f86-padopened Right +t86_padopened
log=$out/f86-padopened.log
ok=1
precedes "$log" 'Gamepad ButtonY detected' 'Hold OK: settings opened another way, so the hold is canceled' \
    && grep -qx 'Hold OK: settings opened another way, so the hold is canceled' "$log" \
    && [ "$(sed -n '/^Hold OK: settings opened another way/,/^Settings closed/p' "$log" | grep -c 'Key Return (#D) detected')" -gt 2 ] \
    && ! grep -qF "Settings: page Settings${T86_ARROW}" "$log" \
    && ! grep -qE '^Hold OK: held [0-9]+ ms' "$log" && [ "$(grep -c 'Settings opened' "$log")" = 1 ] \
    && grep -q '^Settings closed' "$log" && ! grep -q '^Selected Entry' "$log" && ran_clean f86-padopened && ok=0
result "hold-ok: settings opened by a pad button during Return's hold cancel it, and Return's repeats press nothing in them (exit $(cat "$out/f86-padopened.code"))" $ok
grep -E '^(Hold OK|Gamepad ButtonY|Settings (opened|closed))' "$log" | sed 's/^/      /'
echo "      Return came $(sed -n '/^Hold OK: settings opened another way/,/^Settings closed/p' "$log" | grep -c 'Key Return (#D) detected') times inside settings"
rm -f /tmp/pad-o
