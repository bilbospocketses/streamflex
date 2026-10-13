# Key and gamepad bindings (3b): capture, Keep, the command, the save; a held starting key; the
# 10 s confirmation, reverted and kept; a pad's button captured through the virtual pad

TO_KEYBOARD="Menu Down Down Down Down Down Down Down Down Return Return"   # Controls, then Keyboard
QUIT="Down Down Down Down Down Down Down Down Down Return"                 # Quit StreamFlex in the command picker
SAVE="BackSpace BackSpace BackSpace"

# A function for a +key: wait out the 10 s a change unconfirmed is kept for. The cases that time it
# (Up taken and reverted, f62-revert, and those after it) are in 62-settings-bindings-timing.sh.
wait_revert() { sleep 11; }

# Up taken for Quit StreamFlex (F1 still goes up), confirmed by pressing Up within the 10 s: kept and saved
cfg=$(writable_config f62-up)
CFG=$cfg run_keys f62-confirm $TO_KEYBOARD Return Return Up Return Down Return $QUIT Up $SAVE
ok=1
grep -q 'Settings: kept Up for :quit' "$out/f62-confirm.log" && grep -qx 'Hotkey2=#40000052;:quit' "$cfg" \
    && ran_clean f62-confirm && ok=0
result "bindings: taking Up's navigation away is kept when Up is pressed again in time (exit $(cat "$out/f62-confirm.code"))" $ok

# A pad's button: the virtual pad holds B while the file names it (STREAMFLEX_TEST_PAD_BUTTON)
press_b() { : > /tmp/pad-b; sleep 1; rm -f /tmp/pad-b; sleep 1; }
rm -f /tmp/pad-b
cfg=$(writable_config f62-pad)
STREAMFLEX_TEST_PAD=/tmp/pad-b STREAMFLEX_TEST_PAD_BUTTON=b WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-pad Menu Down Down Down Down Down Down Down Down Return Down Return Down Down Down Return Return \
    +press_b Return Down Return $QUIT BackSpace BackSpace BackSpace BackSpace
ok=1
grep -q 'Settings: capture got ButtonB' "$out/f62-pad.log" && sed -n '/^\[Gamepad\]/,/^\[/p' "$cfg" | grep -qx 'ButtonB=:quit' \
    && ran_clean f62-pad && ok=0
result "bindings: a gamepad button captured from the pad saves as a control (exit $(cat "$out/f62-pad.code"))" $ok

# --- Beyond the brief's checks: the branches they leave unexercised ---

B62_ARROW=$(printf ' \xE2\x80\xBA ')
RIGHT_MARK=$(printf '\xE2\x80\xBA')
TO_GAMEPAD="Menu Down Down Down Down Down Down Down Down Return Down Return"   # Controls, then Gamepad
# A pad control first repeats 500 ms after its press, so a hold that must outlast that waits for the
# test hook's line saying settings took it for that long: during a capture, or waiting to be let go
# after one
B62_HELD_LET_GO='Test hook: pad ButtonA held while settings took it (waiting to be let go)'

# Confirmed, the change is live as settings close: Up on the main screen now quits, so the Down sent
# after it finds no launcher (the harness's own stop would log "Quitting program" too); and the
# binding pages open again once it is kept
cfg=$(writable_config f62-up)
CFG=$cfg run_keys f62-live $TO_KEYBOARD Return Return Up Return Down Return $QUIT Up Return BackSpace $SAVE Up Down
log=$out/f62-live.log
ok=1
grep -q 'Settings: kept Up for :quit' "$log" \
    && sed -n '/Settings: kept Up for :quit/,$p' "$log" \
       | grep -qF "Settings: page Settings${B62_ARROW}Controls${B62_ARROW}Keyboard${B62_ARROW}Binding" \
    && grep -qx 'Hotkey2=#40000052;:quit' "$cfg" && sed -n '/Settings closed/,$p' "$log" | grep -q 'Quitting program' \
    && sed -n '/Settings closed/,$p' "$log" | grep -q 'Key Up (#40000052) detected' \
    && ! sed -n '/Settings closed/,$p' "$log" | grep -q 'Key Down (#40000051) detected' \
    && ran_clean f62-live && ok=0
result "bindings: a kept change is live, and the pages open again (exit $(cat "$out/f62-live.code"))" $ok

# Settings closed within the 10 s: the change goes back first, and nothing is saved
cfg=$(writable_config f62-up)
CFG=$cfg run_keys f62-close $TO_KEYBOARD Return Return Up Return Down Return $QUIT $SAVE
ok=1
grep -q 'Settings: the binding went back: Up was not pressed again before settings closed' "$out/f62-close.log" \
    && grep -q 'Settings: nothing changed' "$out/f62-close.log" && cmp -s "$FX/f62-up.ini" "$cfg" \
    && ran_clean f62-close && ok=0
result "bindings: closing settings within the 10 s puts the change back (exit $(cat "$out/f62-close.code"))" $ok

# Discard within the 10 s puts the lists back, live, and ends the 10 s: nothing goes back again later
cfg=$(writable_config f62-up)
CFG=$cfg run_keys f62-discard $TO_KEYBOARD Return Return Up Return Down Return $QUIT BackSpace BackSpace Down Return \
    +wait_revert Menu
log=$out/f62-discard.log
ok=1
grep -q 'Settings: discarded the changes' "$log" \
    && sed -n '/Settings: discarded the changes/,$p' "$log" | grep -q 'Settings: the bindings now hold 1 hotkey and' \
    && ! grep -q 'Settings: the binding went back' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f62-up.ini" "$cfg" && ran_clean f62-discard && ok=0
result "bindings: Discard within the 10 s puts the lists back live and ends the 10 s (exit $(cat "$out/f62-discard.code"))" $ok

# A gamepad control added is live once settings close (B quits), and the rebuilt controls keep the
# gamepad's defaults (Start opens settings)
rm -f /tmp/pad-b
cfg=$(writable_config f62-pad)
STREAMFLEX_TEST_PAD=/tmp/pad-b STREAMFLEX_TEST_PAD_BUTTON=b WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padlive $TO_GAMEPAD Down Down Down Return Return +press_b Return Down Return $QUIT \
    BackSpace BackSpace BackSpace +press_b
log=$out/f62-padlive.log
ok=1
sed -n '/Settings: the bindings now hold 0 hotkeys and 1 controls/,$p' "$log" | grep -qE '^ButtonStart +:settings$' \
    && sed -n '/Settings: the bindings now hold 0 hotkeys and 1 controls/,$p' "$log" | grep -qE '^ButtonB +:quit$' \
    && sed -n '/Settings closed/,$p' "$log" | grep -q 'Gamepad ButtonB detected' \
    && sed -n '/Settings closed/,$p' "$log" | grep -q 'Quitting program' && ran_clean f62-padlive && ok=0
result "bindings: a new control is live, beside the gamepad's defaults (exit $(cat "$out/f62-padlive.code"))" $ok
rm -f /tmp/pad-b

# The gamepad switched on while settings are open brings its floor with it: Remove of OK's only
# button is grayed, and says why
cfg=$(writable_config f62-gpoff)
CFG=$cfg run_keys f62-gpon $TO_GAMEPAD Right Down Down Down Down Return Down Down Menu
ok=1
grep -q 'Settings: the note under the preview says That would leave no button for OK' "$out/f62-gpon.log" \
    && grep -qx 'Enabled=true' "$cfg" && grep -qx 'ButtonA=:select' "$cfg" && ran_clean f62-gpon && ok=0
result "bindings: the gamepad switched on brings its floor (exit $(cat "$out/f62-gpon.code"))" $ok

# A command is read by its first word while settings are open, as on the main screen and by the
# floor: F5, bound to ":down now", moves the cursor down
CFG=$FX/f62-word.ini run_keys f62-word Menu F5 Menu
ok=1
grep -q "Settings: the cursor's row reads Background: " "$out/f62-word.log" && ! grep -q 'Settings: ignoring' "$out/f62-word.log" \
    && grep -q 'Settings: nothing changed' "$out/f62-word.log" && ran_clean f62-word && ok=0
result "bindings: a command is read by its first word while settings are open (exit $(cat "$out/f62-word.code"))" $ok

# A [Hotkeys] line with no name is listed; its Remove is grayed with why, and does nothing; its command
# changes and saves in its own line
cfg=$(writable_config f62-empty)
CFG=$cfg run_keys f62-empty $TO_KEYBOARD Down Return Down Down Return Up Return Down Down Return $SAVE
ok=1
grep -q 'Settings: the note under the preview says This line has no name in config.ini' "$out/f62-empty.log" \
    && grep -qx '=#4000003C;:quit' "$cfg" && ! grep -q ':home' "$cfg" && grep -q 'Settings saved 1 change(s)' "$out/f62-empty.log" \
    && ran_clean f62-empty && ok=0
result "bindings: a hotkey line with no name keeps its line, and its command saves (exit $(cat "$out/f62-empty.code"))" $ok
diff "$FX/f62-empty.ini" "$cfg" | sed 's/^/      /'

# Two hotkeys' lines changed by hand while settings are open: the save writes each change as a new
# line, and the log says so for each, without claiming either took effect over the line already there
b62_hand_edit() {
    sed 's/^\(Hotkey[12]=#4000003[EF];:home\)$/\1 ; edited by hand/' "$cfg" > "$out/f62-note.edit"
    cat "$out/f62-note.edit" > "$cfg"
    sleep 0.5
}
cfg=$(writable_config f62-note)
CFG=$cfg run_keys f62-note $TO_KEYBOARD Down Return Down +b62_hand_edit Return Down Down Return \
    Down Return Down Return Down Down Return $SAVE
ok=1
grep -qF "Settings: not saved as asked: 'Hotkey1=#4000003E;:home' changed meanwhile, so its change is written as a new line" \
    "$out/f62-note.log" \
    && grep -qF "Settings: not saved as asked: 'Hotkey2=#4000003F;:home' changed meanwhile, so its change is written as a new line" \
       "$out/f62-note.log" \
    && [ "$(grep -c 'where two lines bind one key or button, the first in the file is the one that runs' "$out/f62-note.log")" = 2 ] \
    && grep -qx 'Hotkey1=#4000003E;:home ; edited by hand' "$cfg" && grep -qx 'Hotkey2=#4000003F;:home ; edited by hand' "$cfg" \
    && grep -qx 'Hotkey3=#4000003E;:quit' "$cfg" && grep -qx 'Hotkey4=#4000003F;:quit' "$cfg" \
    && ran_clean f62-note && ok=0
result "bindings: a line changed by hand meanwhile is saved beside it, and the log says which runs (exit $(cat "$out/f62-note.code"))" $ok
grep 'Settings: not saved as asked' "$out/f62-note.log" | sed 's/^/      /'

# The exit hotkeys after a live rebuild: the launcher's list follows the model's order, so the first
# :exit left (F6) is first, which is the one Windows registers (the model's registered_exit())
cfg=$(writable_config f62-exit)
CFG=$cfg run_keys f62-exit $TO_KEYBOARD Down Return Down Down Return $SAVE
log=$out/f62-exit.log
ok=1
sed -n '/Settings: the bindings now hold 2 hotkeys/,$p' "$log" | grep -m1 'Hotkey 0 Keycode' | grep -q 'Keycode: 4000003F$' \
    && sed -n '/Settings: the bindings now hold 2 hotkeys/,$p' "$log" | grep -m1 'Hotkey 1 Keycode' | grep -q 'Keycode: 40000040$' \
    && ! grep -q '^Hotkey1=' "$cfg" && grep -qx 'Hotkey2=#4000003F;:exit' "$cfg" && ran_clean f62-exit && ok=0
result "bindings: the rebuilt hotkeys keep the model's order, the first :exit first (exit $(cat "$out/f62-exit.code"))" $ok

# A binding's row gives its command the room its key's short name leaves, so F5's :exit reads in
# full, not cut at half the row: on this display (1920 x 1080), and on a second at 1280 x 800. The
# harness build logs the value's width beside its room as the cursor reaches the row.
for b62_size in 1920x1080 1280x800; do
    name=f62-exitrow$b62_size
    if [ "$b62_size" = 1920x1080 ]; then
        # shellcheck disable=SC2086
        CFG=$FX/f62-exit.ini run_keys "$name" $TO_KEYBOARD Down Menu
    else
        # shellcheck disable=SC2086
        CFG=$FX/f62-exit.ini run_keys_at "$b62_size" "$name" $TO_KEYBOARD Down Menu
    fi
    log=$out/$name.log
    line=$(awk -v row="Settings: the cursor's row reads F5: Close the app on show $RIGHT_MARK" '
        index($0, row) == 1 { on = 1; next }
        on && /^Test hook: the cursor.s row.s value is / { print; exit }' "$log")
    read -r b62_wide b62_room <<< "$(sed 's/.* is \([0-9]*\) px wide, with \([0-9]*\) px to draw in/\1 \2/' <<< "$line")"
    ok=1
    grep -qx "Resolution: *$b62_size" "$log" && [ -n "${b62_room:-}" ] && [ "$b62_wide" -le "$b62_room" ] \
        && grep -q 'Settings: nothing changed' "$log" && ran_clean "$name" && ok=0
    result "bindings: at $b62_size a binding's row draws its command uncut beside a short key name (exit $(cat "$out/$name.code"))" $ok
    echo "      ${line:-the width of the F5 row was never logged}"
done
# A long label still leaves its value the half of the row it always had: at 1280 x 800, Block the OS
# screensaver's value gets half the column, whose width the key hint's line gives
CFG=$FX/f62-exit.ini run_keys_at 1280x800 f62-longrow Menu Return Down Down Down Down Menu
log=$out/f62-longrow.log
line=$(awk -v row="Settings: the cursor's row reads Block the OS screensaver: " '
    index($0, row) == 1 { on = 1; next }
    on && /^Test hook: the cursor.s row.s value is / { print; exit }' "$log")
b62_room=$(sed 's/.* with \([0-9]*\) px to draw in/\1/' <<< "$line")
b62_column=$(grep -o 'Test hook: the key hint is [0-9]* px wide, in a column [0-9]* px wide' "$log" | tail -1 \
             | sed 's/.* in a column \([0-9]*\) px wide/\1/')
ok=1
[ -n "$line" ] && [ -n "$b62_column" ] && [ "$b62_room" = $((b62_column / 2)) ] \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-longrow && ok=0
result "bindings: a long label still leaves its value half the row (exit $(cat "$out/f62-longrow.code"))" $ok
echo "      ${line:-the width of the row was never logged}; the column: ${b62_column:-not logged}"

# The binding's command picker cannot open out of memory: it says so, and the keys stay with the page
CFG=$FX/f62-keys.ini STREAMFLEX_TEST_FAIL=list run_keys f62-cmdfail $TO_KEYBOARD Return Down Return Up Menu
log=$out/f62-cmdfail.log
ok=1
grep -q 'Settings: the list cannot open: out of memory' "$log" && ! grep -q 'Settings: the command picker lists' "$log" \
    && sed -n '/the list cannot open/,$p' "$log" | grep -q "Settings: the cursor's row reads Key: " \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-cmdfail && ok=0
result "bindings: the binding's command picker out of memory does not open (exit $(cat "$out/f62-cmdfail.code"))" $ok

# A pad plugged in while the binding's command picker is open leaves it alone
b62_plug_in() { : > /tmp/pad-plug; wait_line 'Test hook: pad plugged in' "$2"; sleep 1; }
rm -f /tmp/pad-plug
STREAMFLEX_TEST_PAD_PLUG=/tmp/pad-plug CFG=$FX/f62-keys.ini \
    run_keys f62-cmdplug $TO_KEYBOARD Return Down Return +b62_plug_in Down BackSpace Menu
log=$out/f62-cmdplug.log
ok=1
grep -q 'Settings: opened the command picker for the binding' "$log" \
    && sed -n '/Test hook: pad plugged in/,$p' "$log" | grep -q 'Gamepad connected' \
    && ! grep -q 'Settings: the pads changed' "$log" \
    && [ "$(grep -c 'Settings: closed the command picker for the binding' "$log")" = 1 ] \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-cmdplug && ok=0
result "bindings: a pad plugged in leaves the binding's command picker alone (exit $(cat "$out/f62-cmdplug.code"))" $ok
rm -f /tmp/pad-plug

# Bindings that cannot be read (the file gone since the launcher read it, or out of memory) leave the
# Keyboard page and the gamepad's bindings out, and say so
b62_remove_config() { rm -f "$cfg"; sleep 0.5; }
cfg=$(writable_config f62-up)
CFG=$cfg run_keys f62-unread +b62_remove_config Menu Down Down Down Down Down Down Down Down Return Return Menu
STREAMFLEX_TEST_FAIL=bindings CFG=$FX/f62-up.ini \
    run_keys f62-loadfail Menu Down Down Down Down Down Down Down Down Return Return Menu
for name in f62-unread f62-loadfail; do
    ok=1
    grep -q 'Settings: the bindings could not be read from ' "$out/$name.log" \
        && grep -qF "Settings: page Settings${B62_ARROW}Controls${B62_ARROW}Gamepad" "$out/$name.log" \
        && ! grep -qF "${B62_ARROW}Keyboard" "$out/$name.log" && ran_clean "$name" && ok=0
    result "bindings: unreadable bindings leave their pages out ($name, exit $(cat "$out/$name.code"))" $ok
done

# A hotkey for :select (F7) as the OK that starts a capture, held: its repeats are not caught, and the
# key after it is (F8); tapped, it starts a capture whose next F7, once it was let go, is caught
b62_hold_f7_then_f8() { xdotool keydown F7; sleep 1.5; xdotool keyup F7; sleep 0.3; xdotool key F8; sleep 1; }
CFG=$FX/f62-select.ini run_keys f62-hotkeyok $TO_KEYBOARD Return +b62_hold_f7_then_f8 BackSpace F7 F7 \
    BackSpace BackSpace BackSpace BackSpace BackSpace
log=$out/f62-hotkeyok.log
ok=1
grep -q 'Settings: capture got F8 (#40000041)' "$log" && [ "$(grep -c 'Settings: capture got' "$log")" = 2 ] \
    && precedes "$log" 'Settings: capture got F8' 'Settings: capture got F7 (#40000040)' \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-hotkeyok && ok=0
result "bindings: a hotkey for OK starts a capture as OK does (exit $(cat "$out/f62-hotkeyok.code"))" $ok
grep 'Settings: capture got' "$log" | sed 's/^/      /'

# A stick pushed is captured as its direction (the virtual pad's right stick to the right)
cfg=$(writable_config f62-pad)
STREAMFLEX_TEST_PAD=/tmp/pad-b STREAMFLEX_TEST_PAD_BUTTON=rightx WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padaxis $TO_GAMEPAD Down Down Down Return Return +press_b Return Down Return $QUIT BackSpace BackSpace BackSpace
ok=1
grep -q 'Settings: capture got RStickX+' "$out/f62-padaxis.log" && grep -qx 'RStickX+=:quit' "$cfg" \
    && ran_clean f62-padaxis && ok=0
result "bindings: a stick pushed is captured as its direction (exit $(cat "$out/f62-padaxis.code"))" $ok
grep 'Settings: capture got' "$out/f62-padaxis.log" | sed 's/^/      /'
rm -f /tmp/pad-b

# A binding changed while the gamepad is off: its defaults are not added to the rebuilt controls
cfg=$(writable_config f62-gpoff)
CFG=$cfg run_keys f62-gpoffapply $TO_KEYBOARD Return Return F5 Return Down Return $QUIT Menu
log=$out/f62-gpoffapply.log
ok=1
sed -n '/Settings: the bindings now hold 1 hotkey and 1 controls/,$p' "$log" | grep -qE '^ButtonA +:select$' \
    && ! sed -n '/Settings: the bindings now hold 1 hotkey and 1 controls/,$p' "$log" | grep -q '^ButtonStart ' \
    && grep -qx 'Hotkey1=#4000003E;:quit' "$cfg" && ran_clean f62-gpoffapply && ok=0
result "bindings: with the gamepad off, the rebuilt controls leave its defaults out (exit $(cat "$out/f62-gpoffapply.code"))" $ok

# The Menu key captured is named Menu (SDL calls this code of it Application), on the confirm page too
CFG=$FX/f62-keys.ini run_keys f62-menu $TO_KEYBOARD Return Return Menu BackSpace BackSpace Menu
ok=1
grep -q 'Settings: capture got Menu (#40000065)' "$out/f62-menu.log" \
    && grep -q "Settings: the cursor's row reads Keep" "$out/f62-menu.log" \
    && grep -q 'Settings: nothing changed' "$out/f62-menu.log" && ran_clean f62-menu && ok=0
result "bindings: the Menu key is named Menu (exit $(cat "$out/f62-menu.code"))" $ok

# The pad's OK held on a binding's Command row: its first press opens the command picker, and its
# repeat chooses there, which rebuilds the controls the launcher is reading the pad through, on a
# repeat this time: the next repeat is set before the command runs, so nothing freed is written.
# The repeat comes 500 ms after the press, however fast or slowly the frames run, and OK is held until
# the rebuild's line is logged. The repeats after it open the binding and start a capture, which runs
# out in 5 s, as before.
b62_hold_a_repeat() {
    : > /tmp/pad-a; wait_line 'Settings: the bindings now hold 0 hotkeys and 2 controls' "$2"
    rm -f /tmp/pad-a; sleep 6.5
}
rm -f /tmp/pad-a
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padrepeat $TO_GAMEPAD Down Down Down Down Down Return Down +b62_hold_a_repeat Menu
log=$out/f62-padrepeat.log
ok=1
[ "$(grep -c 'Gamepad ButtonA detected' "$log")" = 1 ] && grep -q 'Settings: opened the command picker for the binding' "$log" \
    && sed -n '/Settings: opened the command picker for the binding/,$p' "$log" \
       | grep -q 'Settings: the bindings now hold 0 hotkeys and 2 controls' \
    && ran_clean f62-padrepeat && ok=0
result "bindings: the pad's OK repeating into a rebuild of its controls writes nothing freed (exit $(cat "$out/f62-padrepeat.code"))" $ok
rm -f /tmp/pad-a

# --- Fix round 1 ---

# The pad's OK that starts a keyboard capture, held on past the key caught and past its repeat's delay:
# it waits until let go, so its repeat does not Keep what the capture caught
b62_hold_a_capture_f5() {
    : > /tmp/pad-a; wait_line 'Settings: capturing a key' "$2"; xdotool key F5
    wait_line "$B62_HELD_LET_GO" "$2"; rm -f /tmp/pad-a; sleep 0.5
}
rm -f /tmp/pad-a
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padholdkey $TO_KEYBOARD Return +b62_hold_a_capture_f5 BackSpace BackSpace Menu
log=$out/f62-padholdkey.log
ok=1
grep -q 'Settings: capturing a key' "$log" && precedes "$log" 'Settings: capture got F5 (#4000003E)' "$B62_HELD_LET_GO" \
    && [ "$(grep -c 'Gamepad ButtonA detected' "$log")" = 1 ] \
    && ! grep -q "Settings: the cursor's row reads Key: F5" "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-padholdkey && ok=0
result "bindings: the pad's OK held through a keyboard capture does not Keep what it caught (exit $(cat "$out/f62-padholdkey.code"))" $ok
rm -f /tmp/pad-a

# The rebuilt lists out of memory (STREAMFLEX_TEST_FAIL=apply): each hotkey and control that cannot be
# added is logged and left out, and the launcher runs on
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_FAIL=apply CFG=$cfg run_keys f62-applyfail $TO_KEYBOARD Return Return F5 Return Down Return $QUIT Menu
log=$out/f62-applyfail.log
ok=1
grep -q 'Test hook: apply fails' "$log" && grep -q 'Could not add the hotkey #4000003E: out of memory' "$log" \
    && grep -q 'Could not add the gamepad control ButtonA: out of memory' "$log" \
    && grep -qx 'Hotkey1=#4000003E;:quit' "$cfg" && ran_clean f62-applyfail && ok=0
result "bindings: hotkeys and controls that cannot be added out of memory are left out (exit $(cat "$out/f62-applyfail.code"))" $ok

# A hotkey removed in settings whose line was removed by hand meanwhile: the save skips the removal and
# says so, without the words about two lines on one key, which only a change written as a new line needs
b62_remove_line() { sed '/^Hotkey1=/d' "$cfg" > "$out/f62-skipnote.edit"; cat "$out/f62-skipnote.edit" > "$cfg"; sleep 0.5; }
cfg=$(writable_config f62-note)
CFG=$cfg run_keys f62-skipnote $TO_KEYBOARD Down Return Down Down +b62_remove_line Return $SAVE
ok=1
grep -qxF "Settings: not saved as asked: 'Hotkey1=#4000003E;:home' is not there any more, so its removal is skipped" \
    "$out/f62-skipnote.log" \
    && ! grep -q 'Hotkey1=' "$cfg" && grep -qx 'Hotkey2=#4000003F;:home' "$cfg" && ran_clean f62-skipnote && ok=0
result "bindings: a removal skipped is logged without the two-lines note (exit $(cat "$out/f62-skipnote.code"))" $ok
grep 'Settings: not saved as asked' "$out/f62-skipnote.log" | sed 's/^/      /'

# Both kinds of note in one save: Hotkey1 changed in settings and by hand, Hotkey2 removed in settings
# and by hand. Each line carries the words its own kind needs, and only those: the two-lines note
# goes by the kind the save gives each note, so it reaches the change's line and not the removal's.
b62_mixed_edit() {
    sed -e 's/^\(Hotkey1=#4000003E;:home\)$/\1 ; edited by hand/' -e '/^Hotkey2=/d' "$cfg" > "$out/f62-mixnote.edit"
    cat "$out/f62-mixnote.edit" > "$cfg"
    sleep 0.5
}
cfg=$(writable_config f62-note)
CFG=$cfg run_keys f62-mixnote $TO_KEYBOARD Down Return Down Return Down Down Return Down Return Down Down Return \
    +b62_mixed_edit $SAVE
ok=1
grep -qxF "Settings: not saved as asked: 'Hotkey1=#4000003E;:home' changed meanwhile, so its change is written as a new line; where two lines bind one key or button, the first in the file is the one that runs" \
    "$out/f62-mixnote.log" \
    && grep -qxF "Settings: not saved as asked: 'Hotkey2=#4000003F;:home' is not there any more, so its removal is skipped" \
       "$out/f62-mixnote.log" \
    && grep -qx 'Hotkey1=#4000003E;:home ; edited by hand' "$cfg" && grep -qx 'Hotkey2=#4000003E;:quit' "$cfg" \
    && ran_clean f62-mixnote && ok=0
result "bindings: a change and a removal both made by hand meanwhile each get their own words (exit $(cat "$out/f62-mixnote.code"))" $ok
grep 'Settings: not saved as asked' "$out/f62-mixnote.log" | sed 's/^/      /'

# --- Final review fix wave ---

# A pad's capture with no pad to capture from is refused with the reason, and starts nothing: the
# gamepad off, then on with no pad connected (no virtual pad). The binding page stays as it was, and
# its command picker still opens.
for name in f62-capoff f62-capnopad; do
    case $name in
        f62-capoff) fixture=f62-padoff why='Turn the gamepad on to capture a button' ;;
        *) fixture=f62-pad why='No gamepad is connected' ;;
    esac
    CFG=$FX/$fixture.ini run_keys $name $TO_GAMEPAD Down Down Down Return Return Down Return BackSpace Menu
    log=$out/$name.log
    ok=1
    grep -qxF "Settings: the capture cannot start: $why" "$log" \
        && grep -qxF "Settings: the note under the preview says $why" "$log" \
        && ! grep -q 'Settings: capturing a' "$log" \
        && sed -n '/Settings: the capture cannot start/,$p' "$log" | grep -q 'Settings: opened the command picker for the binding' \
        && grep -q 'Settings: nothing changed' "$log" && ran_clean $name && ok=0
    result "bindings: a pad's capture with no pad to capture from says why, and starts nothing ($name, exit $(cat "$out/$name.code"))" $ok
done

# An SDL2-era config's media keys still mean what they meant (spec section 1, item 2; plan Task 1.4).
# #40000105 is SDL2's AudioPlay; a remote's play/pause key sends SDL3's MediaPlayPause (#4000010F)
# now, and a play key MediaPlay (#40000106): either is read as #40000105, runs its hotkey, and the log
# names it by the code the file holds. The hotkey quits, so the Down sent after it finds no launcher:
# the harness's own stop would log "Quitting program" too, and a play key that ran nothing would leave
# the launcher there to read the Down
CFG=$FX/f62-media.ini run_keys f62-media XF86AudioPlay Down
ok=1
grep -qx 'Key MediaPlayPause (#40000105) detected' "$out/f62-media.log" && grep -q '^Quitting program' "$out/f62-media.log" \
    && ! grep -q '^Key Down' "$out/f62-media.log" && ran_clean f62-media && ok=0
result "keys: an SDL2-era config's play key still runs its hotkey (exit $(cat "$out/f62-media.code"))" $ok
grep -E '^Key ' "$out/f62-media.log" | sed 's/^/      /'

# A media key captured is saved as the code SDL2 gave it, so what an SDL3 StreamFlex writes an SDL2-era
# one reads the same: next track is #40000102 (SDL2's AudioNext), not SDL3's #4000010B
cfg=$(writable_config f62-keys)
CFG=$cfg run_keys f62-mediabind $TO_KEYBOARD Return Return XF86AudioNext Return Down Return $QUIT $SAVE
ok=1
grep -q 'Settings: capture got MediaTrackNext (#40000102)' "$out/f62-mediabind.log" \
    && sed -n '/^\[Hotkeys\]/,/^\[/p' "$cfg" | grep -qx 'Hotkey1=#40000102;:quit' && ran_clean f62-mediabind && ok=0
result "keys: a captured media key saves as SDL2's code (exit $(cat "$out/f62-mediabind.code"))" $ok
grep -h -E 'Settings: capture got|^Hotkey' "$out/f62-mediabind.log" "$cfg" | sed 's/^/      /'
