# The on-screen keyboard (3c, spec "The on-screen keyboard" and "No OS keyboard, ever"), through the ways in
# 3b's pages gain: the command picker's Type... (Startup command, Quit command, a binding's command) and a
# path row's Type a path... (Mappings file). Typing on its keys with the remote, on a real keyboard with
# Shift, and under a German layout with AltGr and a dead key; the keypad's digits and operators; the pad's
# X, Y and Start; Back asking first after a change; Done off with the reason; the layout kept as
# KeyboardLayout, and read back from the file; the settings key and a new screen size with the keyboard
# open; a keyboard that cannot open; and SDL's text input stopped on the window. The Tiles pages' ways in
# (titles, names, the app list's filter) are check 82's.
# Not timing-sensitive: every key waits for its frame, xdotool type's keys reach the launcher in order, and every result is read from the log's lines or the saved file, never from a clock

ARROW=$(printf ' \xE2\x80\xBA ')
K84_DOT=$(printf '\xC2\xB7')
# General, then Startup command (the tenth row) or Quit command (the eleventh), then the picker's last row, Type...
K84_STARTUP="Menu Return Down Down Down Down Down Down Down Down Down Return Right Right Right Return"
K84_QUIT="Menu Return Down Down Down Down Down Down Down Down Down Down Return Right Right Right Return"
k84_type_mixed() { xdotool type --delay 60 'Kodi Box #2!'; }
k84_type_semicolon() { xdotool type --delay 60 'a ;b'; }
k84_type_z() { xdotool type --delay 60 'z'; }
k84_type_new() { xdotool type --delay 60 'new.txt'; }
k84_type_gone() { xdotool type --delay 60 'gone.txt'; }
k84_altgr_q() { xdotool keydown ISO_Level3_Shift; xdotool key q; xdotool keyup ISO_Level3_Shift; }
k84_press() { : > /tmp/pad-k84; sleep 1; rm -f /tmp/pad-k84; sleep 1; }
# The keypad's keys by their keycodes with no modifier held (key.py), so Num Lock's state never matters
k84_keypad() {
    local k
    for k in KP_1 KP_0 KP_Decimal KP_Divide KP_Multiply KP_Subtract KP_Add KP_9; do
        python3 "$HERE/key.py" key "$k"; sleep 0.1
    done
}

# On its own keys: o (Right x8 from q), k (Down), then Down, Down, Right, Right to Done. The keyboard covers
# the bottom 40% (y = 648 of 1080), its hint is the keyboard's, and the window's text input is off. The
# column keeps the page under it: the cursor's row, Startup command, still has its highlight (x 40, y 790)
k84_column() { look "$1" "$2" column 'Settings: the keyboard is drawn at' @40,790=49,59,67; }
cfg=$(writable_config f84-keys)
CFG=$cfg run_keys f84-osk $K84_STARTUP +k84_column Right Right Right Right Right Right Right Right Return Down Return \
    Down Down Right Right Return Menu
ok=1
log=$out/f84-osk.log
grep -qx 'StartupCmd=ok' "$cfg" && grep -q 'Window: SDL text input is off for the window' "$log" \
    && grep -qx 'column yes' "$out/f84-osk.seen" \
    && grep -q "Settings: opened the keyboard for Command for Startup command (QWERTY)" "$log" \
    && grep -q "Settings: the keyboard reads 'o' (cursor 1)" "$log" \
    && grep -q "Settings: the keyboard reads 'ok' (cursor 2)" "$log" \
    && grep -q "Settings: the keyboard's highlight is on Done" "$log" \
    && grep -q "Settings: the keyboard finished with 'ok'" "$log" \
    && grep -qE 'Settings: the keyboard is drawn at [0-9]+,648, [0-9]+ x [0-9]+' "$log" \
    && grep -qF "Settings: the key hint reads OK types $K84_DOT Back cancels" "$log" \
    && grep -qF "Settings: page Settings${ARROW}General${ARROW}Command for Startup command" "$log" \
    && grep -q 'Settings: the keyboard says Type a command' "$log" \
    && grep -qF 'Settings: the note under the preview says Type a command' "$log" \
    && grep -qF "Settings: the keyboard says X deletes $K84_DOT Y types a space $K84_DOT Start is Done" "$log" \
    && ! grep -q ' of 0 on show$' "$log" \
    && grep -q 'Settings: \[General\] StartupCmd (none) -> ok' "$log" && ran_clean f84-osk && ok=0
result "keyboard: a command typed on the on-screen keys saves, the keyboard over the bottom 40% (exit $(cat "$out/f84-osk.code"))" $ok
grep -E "Settings: (opened the keyboard|the keyboard|the key hint)|Window: SDL text input" "$log" | sed 's/^/      /'

# On a real keyboard, with Shift: mixed case and symbols, each the layout's own; Enter is Done once typed
cfg=$(writable_config f84-keys)
CFG=$cfg run_keys f84-real $K84_QUIT +k84_type_mixed Return Menu
ok=1
log=$out/f84-real.log
grep -qxF 'QuitCmd=Kodi Box #2!' "$cfg" \
    && grep -q "Settings: the keyboard typed 'K' (scancode K)" "$log" \
    && grep -q "Settings: the keyboard typed '#' (scancode 3)" "$log" \
    && grep -q "Settings: the keyboard typed '!' (scancode 1)" "$log" \
    && grep -q "Settings: the keyboard finished with 'Kodi Box #2!'" "$log" && ran_clean f84-real && ok=0
result "keyboard: a real keyboard types mixed case and symbols with Shift, and Enter is Done (exit $(cat "$out/f84-real.code"))" $ok
grep -E "Settings: the keyboard (typed|finished)" "$log" | sed 's/^/      /'

# A German layout (Review Focus 5): the Y key types z, AltGr+Q types @, the dead circumflex types its own ^,
# Shift+2 types ". The layout is set on the X server once the launcher is connected (k84_german): Xvfb puts
# its keymap back to US whenever its last client leaves, so a layout set before the run would be gone by
# the time the launcher started. The run's end puts it back the same way; the setxkbmap after it is a guard.
# The dead circumflex goes through key.py, by the keycode of the key left of 1: German puts dead_circumflex
# on the a-umlaut key's AltGr level too, at a lower keycode, which xdotool would press instead.
k84_german() { setxkbmap -layout de; sleep 1; }
k84_dead() { python3 "$HERE/key.py" key dead_circumflex; sleep 1; }
cfg=$(writable_config f84-keys)
CFG=$cfg run_keys f84-german +k84_german $K84_STARTUP z +k84_altgr_q +k84_dead shift+2 Return Menu
setxkbmap -layout us
ok=1
log=$out/f84-german.log
grep -q "Settings: the keyboard typed 'z' (scancode Y)" "$log" \
    && grep -q "Settings: the keyboard typed '@' (scancode Q)" "$log" \
    && grep -q "Settings: the keyboard typed '\"' (scancode 2)" "$log" && ran_clean f84-german && ok=0
result "keyboard: a German layout types its own letters and AltGr's (exit $(cat "$out/f84-german.code"))" $ok
# OPEN (3c Task 12, reported to the controller): SDL 3.2.10 and 3.4.18 both give the dead circumflex as a
# scancode-masked keycode (0x40000035), which types nothing under the real-keyboard rule, so this fails
# until the plan rules on dead keys
ok=1
grep -qxF 'StartupCmd=z@^"' "$cfg" && grep -q "Settings: the keyboard typed '^' " "$log" && ok=0
result "keyboard: under a German layout a dead key types its own character (exit $(cat "$out/f84-german.code"))" $ok
grep -E "Settings: the keyboard (typed|finished)|^Key \` " "$log" | sed 's/^/      /'

# The keypad (user ruling 2026-10-16, "Type the digit"): its digits, point and operators type their
# character as the top row's keys do, Backspace then deletes (a printing key was typed), and Keypad Enter
# is Done
cfg=$(writable_config f84-keys)
CFG=$cfg run_keys f84-keypad $K84_QUIT +k84_keypad BackSpace KP_Enter Menu
ok=1
log=$out/f84-keypad.log
grep -qxF 'QuitCmd=10./*-+' "$cfg" \
    && grep -qF "Settings: the keyboard reads '10./*-+9' (cursor 8)" "$log" \
    && sed -n "/the keyboard reads '10.\/\*-+9' (cursor 8)/,\$p" "$log" | grep -qF "Settings: the keyboard reads '10./*-+' (cursor 7)" \
    && grep -qF "Settings: the keyboard typed '1' (scancode Keypad 1)" "$log" \
    && grep -qF "Settings: the keyboard typed '0' (scancode Keypad 0)" "$log" \
    && grep -qF "Settings: the keyboard typed '.' (scancode Keypad .)" "$log" \
    && grep -qF "Settings: the keyboard typed '/' (scancode Keypad /)" "$log" \
    && grep -qF "Settings: the keyboard typed '*' (scancode Keypad *)" "$log" \
    && grep -qF "Settings: the keyboard typed '-' (scancode Keypad -)" "$log" \
    && grep -qF "Settings: the keyboard typed '+' (scancode Keypad +)" "$log" \
    && grep -qF "Settings: the keyboard finished with '10./*-+'" "$log" && ran_clean f84-keypad && ok=0
result "keyboard: the keypad types its digits, point and operators, Backspace deletes, and Keypad Enter is Done (exit $(cat "$out/f84-keypad.code"))" $ok
grep -E "Settings: the keyboard (typed|finished)" "$log" | sed 's/^/      /'

# Held keys on a real keyboard: a held letter types again as it repeats; a held Esc asks once ("Back again
# to discard") and its repeats never discard; a held Backspace deletes again, held longer than the letter
# was, so the field ends empty, and Esc then closes it at once (no change left). Only the first repeat of
# each matters, which comes 660 ms into a hold of 1.5 s or more.
k84_hold() { xdotool keydown "$3"; sleep "$4"; xdotool keyup "$3"; sleep 1; }
k84_hold_a() { k84_hold "$1" "$2" a 1.5; }
k84_hold_escape() { k84_hold "$1" "$2" Escape 2; }
k84_hold_backspace() { k84_hold "$1" "$2" BackSpace 3.5; }
cfg=$(writable_config f84-keys)
CFG=$cfg run_keys f84-held $K84_QUIT +k84_hold_a +k84_hold_escape +k84_hold_backspace Escape Menu
ok=1
log=$out/f84-held.log
grep -q "Settings: the keyboard reads 'aa' (cursor 2)" "$log" \
    && sed -n '/Settings: the keyboard says Back again to discard/,$p' "$log" | grep -q "Settings: the keyboard reads '' (cursor 0)" \
    && ! sed -n "/Settings: the keyboard says Back again to discard/,/Settings: the keyboard reads '' (cursor 0)/p" "$log" \
       | grep -q 'Settings: the keyboard was canceled' \
    && sed -n "/Settings: the keyboard says Back again to discard/,\$p" "$log" | grep -q 'Settings: the keyboard was canceled' \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f84-keys.ini" "$cfg" && ran_clean f84-held && ok=0
result "keyboard: a held letter types again, a held Esc asks once and never discards, a held Backspace deletes again (exit $(cat "$out/f84-held.code"))" $ok

# Back with a change asks first ("Back again to discard"), and a second Back cancels: nothing saved
cfg=$(writable_config f84-keys)
CFG=$cfg run_keys f84-back $K84_STARTUP Return BackSpace BackSpace Menu
ok=1
log=$out/f84-back.log
grep -q "Settings: the keyboard reads 'q' (cursor 1)" "$log" \
    && grep -q 'Settings: the keyboard says Back again to discard' "$log" \
    && grep -q 'Settings: the keyboard was canceled' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f84-keys.ini" "$cfg" && ran_clean f84-back && ok=0
result "keyboard: Back after a change asks first, and Back again discards it (exit $(cat "$out/f84-back.code"))" $ok

# Done is off for a value config.ini cannot hold, the reason in the note; Esc is Back, asking first
cfg=$(writable_config f84-keys)
CFG=$cfg run_keys f84-done $K84_QUIT +k84_type_semicolon Return Escape Escape Menu
ok=1
log=$out/f84-done.log
k84_why="config.ini can't hold this command: it has a semicolon after a space, which config.ini would read as a comment"
grep -qF "Settings: the keyboard says $k84_why" "$log" && grep -qF "Settings: Done is off: $k84_why" "$log" \
    && grep -q 'Settings: the keyboard says Back again to discard' "$log" \
    && grep -q 'Settings: the keyboard was canceled' "$log" && ! grep -q 'Settings: the keyboard finished' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f84-keys.ini" "$cfg" && ran_clean f84-done && ok=0
result "keyboard: Done is off with the reason for a ';' after a space, and Esc asks before it discards (exit $(cat "$out/f84-done.code"))" $ok

# The ABC key switches the layout; the keyboard opens in it next time, and KeyboardLayout=ABC is saved
cfg=$(writable_config f84-keys)
CFG=$cfg run_keys f84-layout $K84_STARTUP Down Down Down Left Left Return BackSpace Return Right Right Right Return \
    BackSpace Menu
ok=1
log=$out/f84-layout.log
grep -qx 'KeyboardLayout=ABC' "$cfg" && grep -q 'Settings: \[General\] KeyboardLayout QWERTY -> ABC' "$log" \
    && grep -q "Settings: opened the keyboard for Command for Startup command (ABC)" "$log" \
    && ! grep -q '^StartupCmd' "$cfg" && ran_clean f84-layout && ok=0
result "keyboard: the ABC key switches the layout, which is kept as KeyboardLayout (exit $(cat "$out/f84-layout.code"))" $ok

# The pad (STREAMFLEX_TEST_PAD holds the button STREAMFLEX_TEST_PAD_BUTTON names while the file is there):
# Start is Done, and a Start held past Done never reaches :settings, which would close settings unsaved by
# this run's Menu; X deletes; Y types a space. OK on the q key types q; z comes from the real keyboard.
rm -f /tmp/pad-k84
cfg=$(writable_config f84-pad)
STREAMFLEX_TEST_PAD=/tmp/pad-k84 STREAMFLEX_TEST_PAD_BUTTON=start WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f84-padstart $K84_STARTUP Return +k84_press Menu
ok=1
log=$out/f84-padstart.log
grep -qx 'StartupCmd=q' "$cfg" && grep -q "Settings: the pad's ButtonStart is Done" "$log" \
    && grep -q "Settings: the keyboard finished with 'q'" "$log" && ! grep -q 'Gamepad ButtonStart detected' "$log" \
    && ran_clean f84-padstart && ok=0
result "keyboard: the pad's Start is Done, and never reaches :settings (exit $(cat "$out/f84-padstart.code"))" $ok

rm -f /tmp/pad-k84
cfg=$(writable_config f84-pad)
STREAMFLEX_TEST_PAD=/tmp/pad-k84 STREAMFLEX_TEST_PAD_BUTTON=x WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f84-padx $K84_STARTUP Return Return +k84_press +k84_type_z Return Menu
ok=1
log=$out/f84-padx.log
grep -qx 'StartupCmd=qz' "$cfg" && grep -q "Settings: the pad's ButtonX deletes" "$log" \
    && grep -q "Settings: the keyboard reads 'qq' (cursor 2)" "$log" && ran_clean f84-padx && ok=0
result "keyboard: the pad's X deletes (exit $(cat "$out/f84-padx.code"))" $ok

rm -f /tmp/pad-k84
cfg=$(writable_config f84-pad)
STREAMFLEX_TEST_PAD=/tmp/pad-k84 STREAMFLEX_TEST_PAD_BUTTON=y WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f84-pady $K84_STARTUP Return +k84_press +k84_type_z Return Menu
ok=1
log=$out/f84-pady.log
grep -qx 'StartupCmd=q z' "$cfg" && grep -q "Settings: the pad's ButtonY types a space" "$log" && ran_clean f84-pady && ok=0
result "keyboard: the pad's Y types a space (exit $(cat "$out/f84-pady.code"))" $ok
rm -f /tmp/pad-k84

# A path row's Type a path...: the browser's last row opens the keyboard on the folder on show, and a file
# typed there is chosen as one chosen in the list
rm -rf "$TESTER_HOME/k84"; mkdir -p "$TESTER_HOME/k84"
printf '# no mappings\n' > "$TESTER_HOME/k84/old.txt"; printf '# no mappings\n' > "$TESTER_HOME/k84/new.txt"
chown -R tester:tester "$TESTER_HOME/k84"
cfg=$(writable_config f84-paths)
CFG=$cfg run_keys f84-path Menu Down Down Down Down Down Down Down Down Return Down Return Down Down Return \
    Right Right Right Return +k84_type_new Return Menu BackSpace
ok=1
log=$out/f84-path.log
grep -qx 'ControllerMappingsFile=/home/tester/k84/new.txt' "$cfg" \
    && grep -q "Settings: opened the keyboard for Path for Mappings file" "$log" \
    && grep -q "Settings: the keyboard reads '/home/tester/k84/' (cursor 17)" "$log" \
    && grep -q 'Settings: chose /home/tester/k84/new.txt' "$log" && ran_clean f84-path && ok=0
result "keyboard: Type a path... in a path row's browser types the path, which saves (exit $(cat "$out/f84-path.code"))" $ok

# A typed path that is not there: Done is on while it is typed (the validator reads no disk), Done refuses
# it, and the keyboard reopens on the same text with the reason, the browser under it; Esc then cancels at
# once (no change), and nothing is saved
cfg=$(writable_config f84-paths)
CFG=$cfg run_keys f84-pathgone Menu Down Down Down Down Down Down Down Down Return Down Return Down Down Return \
    Right Right Right Return +k84_type_gone Return Escape Menu
ok=1
log=$out/f84-pathgone.log
! grep -q 'Settings: Done is off: No file is there' "$log" \
    && grep -qx 'Settings: the typed path /home/tester/k84/gone.txt is refused: No file is there' "$log" \
    && [ "$(grep -c 'Settings: opened the keyboard for Path for Mappings file' "$log")" = 2 ] \
    && grep -q "Settings: the keyboard reads '/home/tester/k84/gone.txt' (cursor 25)" "$log" \
    && grep -q 'Settings: the keyboard says No file is there' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f84-paths.ini" "$cfg" && ran_clean f84-pathgone && ok=0
result "keyboard: a typed path that is not there is refused at Done, the keyboard reopening on it with why (exit $(cat "$out/f84-pathgone.code"))" $ok

# A keyboard that cannot open (out of memory) says so, and the keys stay with the page
cfg=$(writable_config f84-keys)
STREAMFLEX_TEST_FAIL=keyboard CFG=$cfg run_keys f84-nomem $K84_STARTUP Down Menu
ok=1
log=$out/f84-nomem.log
grep -q 'Settings: the keyboard cannot open: out of memory' "$log" \
    && sed -n '/the keyboard cannot open/,$p' "$log" | grep -q "Settings: the cursor's row reads Quit command" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f84-nomem && ok=0
result "keyboard: a keyboard that cannot open says so, and the page keeps the keys (exit $(cat "$out/f84-nomem.code"))" $ok

# KeyboardLayout=ABC in the file: the keyboard opens in it, the debug output says so, and Keypad Enter is
# Done at once (nothing typed); the command it hands back is the one the row has, so nothing changes
cfg=$(writable_config f84-abc)
CFG=$cfg run_keys f84-abc $K84_STARTUP KP_Enter Menu
ok=1
log=$out/f84-abc.log
grep -qE '^KeyboardLayout: +ABC$' "$log" \
    && grep -q "Settings: opened the keyboard for Command for Startup command (ABC)" "$log" \
    && grep -q "Settings: the keyboard reads ':left' (cursor 5)" "$log" \
    && grep -q "Settings: the keyboard finished with ':left'" "$log" \
    && grep -q 'Settings: \[General\] StartupCmd is unchanged' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f84-abc.ini" "$cfg" && ran_clean f84-abc && ok=0
result "keyboard: KeyboardLayout=ABC opens the keyboard in ABC, and the same command handed back changes nothing (exit $(cat "$out/f84-abc.code"))" $ok

# The settings key (Menu) with the keyboard open cancels it, then closes settings: the text is not kept
cfg=$(writable_config f84-keys)
CFG=$cfg run_keys f84-menu $K84_STARTUP Return Menu
ok=1
log=$out/f84-menu.log
grep -q "Settings: the keyboard reads 'q' (cursor 1)" "$log" \
    && grep -q 'Settings: the keyboard was canceled as settings close' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f84-keys.ini" "$cfg" && ran_clean f84-menu && ok=0
result "keyboard: the settings key cancels the keyboard and closes settings, keeping nothing (exit $(cat "$out/f84-menu.code"))" $ok

# A new screen size closes the keyboard as Back at its start would (it was measured where it opened),
# and the page under it stays (STREAMFLEX_TEST_PIXEL_SIZE stands in for the window's new size)
k84_to_720() { echo 1280x720 > /tmp/k84-size; wait_line 'Settings: re-measured for 1280x720 px' "$2"; }
rm -f /tmp/k84-size
cfg=$(writable_config f84-keys)
STREAMFLEX_TEST_PIXEL_SIZE=/tmp/k84-size CFG=$cfg run_keys f84-resize $K84_STARTUP Return +k84_to_720 Menu
rm -f /tmp/k84-size
ok=1
log=$out/f84-resize.log
in_range "$log" 'Screen: 1280x720 px, was 1920x1080; reloading' 'Settings: re-measured for 1280x720 px' \
        'Settings: the keyboard was canceled' \
    && sed -n '/^Settings: re-measured for /,$p' "$log" | grep -qxF "Settings: page Settings${ARROW}General" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f84-keys.ini" "$cfg" && ran_clean f84-resize && ok=0
result "keyboard: a new screen size closes the keyboard, and the page under it stays (exit $(cat "$out/f84-resize.code"))" $ok

# A binding's command picker ends with Type... too: its command is weighed as a hotkey's line holds it
# (#4000003A; before it), so 180 characters, which StartupCmd= could hold, keep Done off; the keyboard
# canceled, a command typed again goes to the binding page
k84_type_long() { xdotool type --delay 20 "$(printf 'a%.0s' $(seq 180))"; }
K84_BINDING="Menu Down Down Down Down Down Down Down Down Return Return Return Down Return Right Right Right Return"
cfg=$(writable_config f84-keys)
CFG=$cfg run_keys f84-binding $K84_BINDING +k84_type_long Return Escape Escape Return Right Right Right Return \
    +k84_type_z Return BackSpace BackSpace Menu
ok=1
log=$out/f84-binding.log
[ "$(grep -c 'Settings: opened the keyboard for Command for the binding (QWERTY)' "$log")" = 2 ] \
    && grep -qF "Settings: Done is off: config.ini can't hold this command: it is too long for one line of config.ini (199 bytes at most)" "$log" \
    && grep -q "Settings: the keyboard finished with 'z'" "$log" \
    && sed -n "/Settings: the keyboard finished with 'z'/,\$p" "$log" | grep -q "Settings: the cursor's row reads Command: z" \
    && ran_clean f84-binding && ok=0
result "keyboard: Type... in a binding's command picker weighs it as a hotkey's line, and hands it to the binding (exit $(cat "$out/f84-binding.code"))" $ok
grep -E "Settings: (opened the keyboard|Done is off|the keyboard finished|the cursor's row reads Command)" "$log" | sed 's/^/      /'

# A typed path config.ini cannot hold keeps Done off as it is typed, with the reason
k84_type_semicolon_path() { xdotool type --delay 60 'a ;b'; }
cfg=$(writable_config f84-paths)
CFG=$cfg run_keys f84-pathsemi Menu Down Down Down Down Down Down Down Down Return Down Return Down Down Return \
    Right Right Right Return +k84_type_semicolon_path Return Escape Escape Menu
ok=1
log=$out/f84-pathsemi.log
k84_path_why="config.ini can't hold this path: it has a semicolon after a space, which config.ini would read as a comment"
grep -qF "Settings: the keyboard says $k84_path_why" "$log" && grep -qF "Settings: Done is off: $k84_path_why" "$log" \
    && grep -q 'Settings: the keyboard was canceled' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f84-paths.ini" "$cfg" && ran_clean f84-pathsemi && ok=0
result "keyboard: a typed path config.ini cannot hold keeps Done off with the reason (exit $(cat "$out/f84-pathsemi.code"))" $ok

# At /, Type a path... opens the keyboard on / (no second separator after a folder that ends in one); at the
# places, it opens empty, and Done stays off until a path is typed
cfg=$(writable_config f84-paths)
CFG=$cfg run_keys f84-pathplaces Menu Down Down Down Down Down Down Down Down Return Down Return Down Down Return \
    BackSpace BackSpace BackSpace Right Right Right Right Return Escape BackSpace Right Right Right Return Escape \
    BackSpace Menu
ok=1
log=$out/f84-pathplaces.log
grep -q "Settings: the keyboard reads '/' (cursor 1)" "$log" \
    && grep -q 'Settings: browsing the places' "$log" \
    && sed -n '/Settings: browsing the places/,$p' "$log" | grep -q 'Settings: opened the keyboard for Path for Mappings file' \
    && grep -q "Settings: the keyboard reads '' (cursor 0)" "$log" \
    && grep -q 'Settings: the keyboard says Type a path' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f84-paths.ini" "$cfg" && ran_clean f84-pathplaces && ok=0
result "keyboard: Type a path... at / opens on /, and at the places empty, Done off until a path is typed (exit $(cat "$out/f84-pathplaces.code"))" $ok
