# Key and gamepad bindings (3b): capture, Keep, the command, the save; a held starting key; the
# 10 s confirmation, reverted and kept; a pad's button captured through the virtual pad

TO_KEYBOARD="Menu Down Down Down Down Down Down Down Down Return Return"   # Controls, then Keyboard
QUIT="Down Down Down Down Down Down Down Down Down Return"                 # Quit StreamFlex in the command picker
SAVE="BackSpace BackSpace BackSpace"

# F5 captured for Quit StreamFlex: saved as the first hotkey in a new [Hotkeys]
cfg=$(writable_config f62-keys)
CFG=$cfg run_keys f62-bind $TO_KEYBOARD Return Return F5 Return Down Return $QUIT $SAVE
ok=1
grep -q 'Settings: capture got F5 (#4000003E)' "$out/f62-bind.log" \
    && sed -n '/^\[Hotkeys\]/,/^\[/p' "$cfg" | grep -qx 'Hotkey1=#4000003E;:quit' \
    && grep -q 'Settings: the bindings now hold 1 hotkey' "$out/f62-bind.log" && ran_clean f62-bind && ok=0
result "bindings: a captured key bound to a command saves as a hotkey (exit $(cat "$out/f62-bind.code"))" $ok
diff "$FX/f62-keys.ini" "$cfg" | sed 's/^/      /'

# The OK that starts a capture held for 2 s: its repeats and its release are not captured, and the
# key pressed after it is (Review Focus 5)
hold_return_then_f6() { xdotool keydown Return; sleep 2; xdotool keyup Return; sleep 0.5; xdotool key F6; sleep 1; }
cfg=$(writable_config f62-keys)
CFG=$cfg run_keys f62-held $TO_KEYBOARD Return +hold_return_then_f6 Return Down Return $QUIT $SAVE
ok=1
[ "$(grep -c 'Key Return (#D) detected' "$out/f62-held.log")" -gt 2 ] \
    && grep -q 'Settings: capture got F6 (#4000003F)' "$out/f62-held.log" \
    && ! grep -q 'Settings: capture got Return' "$out/f62-held.log" \
    && grep -qx 'Hotkey1=#4000003F;:quit' "$cfg" && ran_clean f62-held && ok=0
result "bindings: a held OK starts one capture, and the key after it is the one bound (exit $(cat "$out/f62-held.code"))" $ok

# Up taken for Quit StreamFlex (F1 still goes up): unconfirmed for 10 s, it goes back, and nothing is saved
wait_revert() { sleep 11; }
cfg=$(writable_config f62-up)
CFG=$cfg run_keys f62-revert $TO_KEYBOARD Return Return Up Return Down Return $QUIT +wait_revert $SAVE
ok=1
grep -q 'Settings: press Up again within 10 s to keep it' "$out/f62-revert.log" \
    && grep -q 'Settings: the binding went back: Up was not pressed again within 10 s' "$out/f62-revert.log" \
    && grep -q 'Settings: nothing changed' "$out/f62-revert.log" && cmp -s "$FX/f62-up.ini" "$cfg" \
    && ran_clean f62-revert && ok=0
result "bindings: taking Up's navigation away reverts after 10 s unconfirmed (exit $(cat "$out/f62-revert.code"))" $ok

# The same, confirmed by pressing Up within the 10 s: kept and saved
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
B62_ELLIPSIS=$(printf '\xE2\x80\xA6')
RIGHT_MARK=$(printf '\xE2\x80\xBA')
TO_GAMEPAD="Menu Down Down Down Down Down Down Down Down Return Down Return"   # Controls, then Gamepad

# The pages as f62-bind's run drew them: the confirm page was on show as the capture ended (logged
# by the frame, since no key moved there), and the binding's Command row carries the › marker
ok=1
grep -qxF "Settings: page Settings${B62_ARROW}Controls${B62_ARROW}Keyboard${B62_ARROW}Binding${B62_ARROW}Keep it?" "$out/f62-bind.log" \
    && grep -qF "Settings: the cursor's row reads Command: None $RIGHT_MARK" "$out/f62-bind.log" && ok=0
result "bindings: the confirm page's path is logged as the capture ends, and Command shows its marker" $ok

# The countdowns in the caption: the capture's from 5 s (f62-bind's run), the confirmation's from 10 s
# down to 1 (f62-revert's run)
ok=1
grep -qF "Settings: the note under the preview says Press the key or button$B62_ELLIPSIS (5 s)" "$out/f62-bind.log" \
    && grep -qF 'Settings: the note under the preview says Press Up again within 10 s to keep it' "$out/f62-revert.log" \
    && grep -qF 'Settings: the note under the preview says Press Up again within 1 s to keep it' "$out/f62-revert.log" && ok=0
result "bindings: the caption counts the capture and the 10 s down" $ok

# The 10 s end on their own, as the frames go by: f62-revert's change went back before the next key
# (its first Back) came
ok=1
precedes "$out/f62-revert.log" 'Settings: the binding went back' 'Key Backspace (#8) detected' && ok=0
result "bindings: the 10 s end on the clock, not on the next key" $ok

# A capture that catches nothing ends after 5 s with the reason, counting down to 1 s; then an OK held
# through a capture, past the key captured, does not keep it on the confirm page with its repeats; nor
# does the captured key held down (F7, a hotkey for :select here) with its own
b62_wait_capture() { sleep 6; }
b62_hold_ok_press_f6() { xdotool keydown Return; sleep 0.3; xdotool key F6; sleep 1.5; xdotool keyup Return; sleep 0.5; }
b62_hold_f7() { xdotool keydown F7; sleep 1.5; xdotool keyup F7; sleep 0.5; }
CFG=$FX/f62-select.ini run_keys f62-capture $TO_KEYBOARD Return Return +b62_wait_capture +b62_hold_ok_press_f6 BackSpace \
    Return +b62_hold_f7 BackSpace BackSpace BackSpace BackSpace BackSpace
log=$out/f62-capture.log
ok=1
grep -q 'Settings: the capture caught nothing in 5 s' "$log" \
    && grep -qF 'Settings: the note under the preview says Nothing was pressed in 5 s' "$log" \
    && grep -qF "Settings: the note under the preview says Press the key or button$B62_ELLIPSIS (1 s)" "$log" \
    && grep -q 'Settings: capture got F6 (#4000003F)' "$log" && grep -q 'Settings: capture got F7 (#40000040)' "$log" \
    && ! grep -q "Settings: the cursor's row reads Key: F6" "$log" && ! grep -q "Settings: the cursor's row reads Key: F7" "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-capture && ok=0
result "bindings: a capture times out with its reason, and held keys do not keep what it caught (exit $(cat "$out/f62-capture.code"))" $ok
grep -E "Settings: (capture|the capture|the cursor's row reads Key)" "$log" | sed 's/^/      /'

# The confirmation's key held down from before the change began: its repeats do not keep it, and while
# the 10 s run no binding's page opens (OK on Add binding does nothing); it goes back at 10 s. Up, held
# from the command picker, moves its cursor from Quit StreamFlex to Settings, which the pad's OK
# chooses: a key pressed there would end Up's repeats (a keyboard repeats only the key pressed last),
# and the repeats during the 10 s are what this proves, so the check counts them too.
b62_hold_up_choose() {
    xdotool keydown Up; sleep 0.1
    : > /tmp/pad-a; sleep 0.3; rm -f /tmp/pad-a
    sleep 1.6; xdotool keyup Up; sleep 0.5
}
B62_TO_QUIT="Down Down Down Down Down Down Down Down Down"
rm -f /tmp/pad-a
cfg=$(writable_config f62-uppad)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-norepeat $TO_KEYBOARD Return Return Up Return Down Return $B62_TO_QUIT +b62_hold_up_choose Return \
    +wait_revert $SAVE
log=$out/f62-norepeat.log
ok=1
grep -q 'Settings: press Up again within 10 s to keep it' "$log" \
    && grep -q 'Settings: the binding went back: Up was not pressed again within 10 s' "$log" \
    && [ "$(sed -n '/Settings: press Up again within 10 s/,/Settings: the binding went back/p' "$log" \
            | grep -c 'Key Up (#40000052) detected')" -gt 2 ] \
    && ! grep -q 'Settings: kept Up' "$log" \
    && ! sed -n '/Settings: press Up again within 10 s/,/Settings: the binding went back/p' "$log" \
         | grep -qF "Settings: page Settings${B62_ARROW}Controls${B62_ARROW}Keyboard${B62_ARROW}Binding" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f62-uppad.ini" "$cfg" && ran_clean f62-norepeat && ok=0
result "bindings: a held key's repeats do not confirm, and no binding opens while the 10 s run (exit $(cat "$out/f62-norepeat.code"))" $ok
rm -f /tmp/pad-a

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

# The pad's OK pressed on Remove rebuilds the controls the launcher is reading the pad through: it
# stops reading them (no use after free), and the held button keeps its count, so it presses once
b62_press_a_briefly() { : > /tmp/pad-a; sleep 0.3; rm -f /tmp/pad-a; sleep 1; }
rm -f /tmp/pad-a
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padok $TO_GAMEPAD Down Down Down Down Down Return Down Down +b62_press_a_briefly $SAVE
ok=1
[ "$(grep -c 'Gamepad ButtonA detected' "$out/f62-padok.log")" = 1 ] && ! grep -q '^ButtonY' "$cfg" \
    && grep -qx 'ButtonA=:select' "$cfg" \
    && grep -q 'Settings: the bindings now hold 0 hotkeys and 1 controls' "$out/f62-padok.log" \
    && ! sed -n '/Settings: the bindings now hold 0 hotkeys and 1 controls/,$p' "$out/f62-padok.log" | grep -q '^ButtonY ' \
    && grep -qF "Settings: the cursor's row reads ButtonY: Quit StreamFlex $RIGHT_MARK" "$out/f62-padok.log" \
    && ran_clean f62-padok && ok=0
result "bindings: the pad's OK on Remove rebuilds its controls safely, pressing once (exit $(cat "$out/f62-padok.code"))" $ok
echo "      ButtonA came $(grep -c 'Gamepad ButtonA detected' "$out/f62-padok.log") times"

# A pad's button captured while held (A, OK here) does not press on the confirm page until let go;
# pressed again once let go, it does (Keep)
b62_hold_a() { : > /tmp/pad-a; sleep 1.5; rm -f /tmp/pad-a; sleep 1; }
b62_tap_a_keep() { : > /tmp/pad-a; sleep 0.3; rm -f /tmp/pad-a; sleep 1; }
rm -f /tmp/pad-a
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padheld $TO_GAMEPAD Down Down Down Return Return +b62_hold_a +b62_tap_a_keep \
    BackSpace BackSpace BackSpace BackSpace
log=$out/f62-padheld.log
ok=1
grep -q 'Settings: capture got ButtonA' "$log" && [ "$(grep -c 'Gamepad ButtonA detected' "$log")" = 1 ] \
    && grep -q "Settings: the cursor's row reads Key: ButtonA" "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-padheld && ok=0
result "bindings: a pad's button held through its capture does not press until let go (exit $(cat "$out/f62-padheld.code"))" $ok
rm -f /tmp/pad-a

# The gamepad switched on while settings are open brings its floor with it: Remove of OK's only
# button is greyed, and says why
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

# A [Hotkeys] line with no name is listed; its Remove is greyed with why, and does nothing; its command
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

# The pad's OK (A) starting a pad capture, held: it is not caught while held, the keys wait while the
# pad captures (two Backs do nothing), and A pressed again once let go is caught
b62_hold_a_start() { : > /tmp/pad-a; sleep 0.6; rm -f /tmp/pad-a; sleep 0.4; }
b62_tap_a() { : > /tmp/pad-a; sleep 0.3; rm -f /tmp/pad-a; sleep 0.7; }
rm -f /tmp/pad-a
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padstart $TO_GAMEPAD Down Down Down Return +b62_hold_a_start BackSpace BackSpace +b62_tap_a \
    BackSpace BackSpace BackSpace BackSpace BackSpace
log=$out/f62-padstart.log
ok=1
grep -q 'Settings: capture got ButtonA' "$log" && precedes "$log" ' (#8) detected' 'Settings: capture got ButtonA' \
    && ! sed -n '/Settings: capturing a button/,/Settings: capture got ButtonA/p' "$log" \
         | grep -qxF "Settings: page Settings${B62_ARROW}Controls${B62_ARROW}Gamepad" \
    && [ "$(grep -c 'Gamepad ButtonA detected' "$log")" = 1 ] \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-padstart && ok=0
result "bindings: the pad's OK starts a pad capture, which keeps the keys waiting (exit $(cat "$out/f62-padstart.code"))" $ok
rm -f /tmp/pad-a

# The pad's OK starting a keyboard capture: a key pressed and let go before it is not taken for the
# capture's starting key (F5 is caught), and the pad waits while the keyboard captures (A presses once)
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padkey $TO_KEYBOARD Return F5 +b62_tap_a +b62_tap_a F5 BackSpace BackSpace BackSpace BackSpace BackSpace
log=$out/f62-padkey.log
ok=1
grep -q 'Settings: capturing a key' "$log" && grep -q 'Settings: capture got F5 (#4000003E)' "$log" \
    && [ "$(grep -c 'Gamepad ButtonA detected' "$log")" = 1 ] \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-padkey && ok=0
result "bindings: the pad's OK starts a keyboard capture, which keeps the pad waiting (exit $(cat "$out/f62-padkey.code"))" $ok
rm -f /tmp/pad-a

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

# The OK that starts a capture held on past the capture's 5 s: the capture ends with nothing, and OK's
# repeats after that wait until it is let go, so they do not start another capture on the Key row
b62_hold_ok_timeout() { xdotool keydown Return; sleep 7; xdotool keyup Return; sleep 0.5; }
CFG=$FX/f62-keys.ini run_keys f62-holdok $TO_KEYBOARD Return +b62_hold_ok_timeout Menu
log=$out/f62-holdok.log
ok=1
[ "$(grep -c 'Settings: capturing a key' "$log")" = 1 ] && grep -q 'Settings: the capture caught nothing in 5 s' "$log" \
    && [ "$(sed -n '/Settings: the capture caught nothing in 5 s/,$p' "$log" | grep -c 'Key Return (#D) detected')" -gt 2 ] \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-holdok && ok=0
result "bindings: the OK held past a capture's 5 s does not start another (exit $(cat "$out/f62-holdok.code"))" $ok
echo "      captures started: $(grep -c 'Settings: capturing a key' "$log")"

# The 10 s over while the launcher is held up (stopped here, as a long frame would hold it): Up, pressed
# in that time and read on the first frame after, comes too late, since the clocks run before a frame's
# keys; the change goes back
b62_stop_up() { kill -STOP "$2"; sleep 11; xdotool key Up; sleep 0.5; kill -CONT "$2"; sleep 2; }
cfg=$(writable_config f62-up)
CFG=$cfg run_keys f62-stop $TO_KEYBOARD Return Return Up Return Down Return $QUIT +b62_stop_up $SAVE
log=$out/f62-stop.log
ok=1
grep -q 'Settings: the binding went back: Up was not pressed again within 10 s' "$log" && ! grep -q 'Settings: kept Up' "$log" \
    && sed -n '/Settings: press Up again within 10 s/,$p' "$log" | grep -q 'Key Up (#40000052) detected' \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f62-up.ini" "$cfg" && ran_clean f62-stop && ok=0
result "bindings: a key read after the 10 s ended does not keep the change (exit $(cat "$out/f62-stop.code"))" $ok

# A pad's OK tapped for one frame starts a pad capture and is let go on the next: the capture sees it
# let go, so the same button tapped again is the one caught
b62_tap_frame() { : > /tmp/pad-a; sleep 0.5; rm -f /tmp/pad-a; sleep 0.7; }
rm -f /tmp/pad-a
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a STREAMFLEX_TEST_PAD_FRAMES=1 WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padtap $TO_GAMEPAD Down Down Down Return +b62_tap_frame +b62_tap_frame BackSpace BackSpace Menu
log=$out/f62-padtap.log
ok=1
grep -q 'Settings: capturing a button' "$log" && grep -q 'Settings: capture got ButtonA' "$log" \
    && ! grep -q 'Settings: the capture caught nothing' "$log" && [ "$(grep -c 'Gamepad ButtonA detected' "$log")" = 1 ] \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-padtap && ok=0
result "bindings: a pad's OK tapped for one frame is caught when tapped again (exit $(cat "$out/f62-padtap.code"))" $ok
rm -f /tmp/pad-a

# The Menu key captured is named Menu (SDL calls this code of it Application), on the confirm page too
CFG=$FX/f62-keys.ini run_keys f62-menu $TO_KEYBOARD Return Return Menu BackSpace BackSpace Menu
ok=1
grep -q 'Settings: capture got Menu (#40000065)' "$out/f62-menu.log" \
    && grep -q "Settings: the cursor's row reads Keep" "$out/f62-menu.log" \
    && grep -q 'Settings: nothing changed' "$out/f62-menu.log" && ran_clean f62-menu && ok=0
result "bindings: the Menu key is named Menu (exit $(cat "$out/f62-menu.code"))" $ok

# The pad's OK held on a binding's Command row: its first press opens the command picker, and its
# repeat chooses there, which rebuilds the controls the launcher is reading the pad through, on a
# repeat this time: the held count is taken down before the command runs, so nothing freed is written
b62_hold_a_repeat() { : > /tmp/pad-a; sleep 2; rm -f /tmp/pad-a; sleep 6.5; }
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

# The countdowns stay between 1 s and their whole time (5 s, 10 s): the runs that waited out a capture
# or the 10 s counted down, and no frame showed 0 s (a frame's own time can pass the end the clocks
# have not acted on yet) or more than the whole time
ok=0
for name in f62-revert f62-norepeat f62-capture f62-holdok; do
    counts=$(sed -n -e 's/.*the note under the preview says Press .* again within \([0-9]*\) s to keep it$/10 \1/p' \
                    -e 's/.*the note under the preview says Press the key or button.* (\([0-9]*\) s)$/5 \1/p' "$out/$name.log")
    [ -n "$counts" ] || { echo "      no countdown in $name"; ok=1; }
    echo "$counts" | awk -v n="$name" 'NF == 2 && ($2 < 1 || $2 > $1) { print "      " n ": " $2 " s of " $1; bad = 1 }
                                       END { exit bad }' || ok=1
done
result "bindings: the countdowns never show 0 s, nor more than their whole time" $ok

# --- Fix round 1 ---

# The 10 s over while the launcher is held up, and the pad's OK pressed on Add binding in that time:
# the clocks run before the pad's first frame after, so the change has gone back and the binding page
# opens (the pad is not told to keep the last change first)
b62_stop_pad_a() { kill -STOP "$2"; sleep 11; : > /tmp/pad-a; kill -CONT "$2"; sleep 0.4; rm -f /tmp/pad-a; sleep 1.5; }
rm -f /tmp/pad-a
cfg=$(writable_config f62-uppad)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padstop $TO_KEYBOARD Return Return Up Return Down Return $QUIT +b62_stop_pad_a \
    BackSpace BackSpace BackSpace BackSpace
log=$out/f62-padstop.log
ok=1
grep -q 'Settings: the binding went back: Up was not pressed again within 10 s' "$log" \
    && sed -n '/Settings: the binding went back/,$p' "$log" \
       | grep -qxF "Settings: page Settings${B62_ARROW}Controls${B62_ARROW}Keyboard${B62_ARROW}Binding" \
    && ! grep -q 'keep the last change first' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f62-uppad.ini" "$cfg" && ran_clean f62-padstop && ok=0
result "bindings: the pad read after the 10 s ended finds the change gone back (exit $(cat "$out/f62-padstop.code"))" $ok
rm -f /tmp/pad-a

# The pad's OK that starts a keyboard capture, held on past the key caught: it waits until let go, so
# its repeat does not Keep what the capture caught
b62_hold_a_capture_f5() { : > /tmp/pad-a; sleep 0.5; xdotool key F5; sleep 1.5; rm -f /tmp/pad-a; sleep 0.5; }
rm -f /tmp/pad-a
cfg=$(writable_config f62-padok)
STREAMFLEX_TEST_PAD=/tmp/pad-a STREAMFLEX_TEST_PAD_BUTTON=a WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-padholdkey $TO_KEYBOARD Return +b62_hold_a_capture_f5 BackSpace BackSpace Menu
log=$out/f62-padholdkey.log
ok=1
grep -q 'Settings: capturing a key' "$log" && grep -q 'Settings: capture got F5 (#4000003E)' "$log" \
    && [ "$(grep -c 'Gamepad ButtonA detected' "$log")" = 1 ] \
    && ! grep -q "Settings: the cursor's row reads Key: F5" "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-padholdkey && ok=0
result "bindings: the pad's OK held through a keyboard capture does not Keep what it caught (exit $(cat "$out/f62-padholdkey.code"))" $ok
rm -f /tmp/pad-a

# The OK (Return) that starts a pad capture, held on past the button caught: its repeats wait until it
# is let go, so they do not Keep what the capture caught
b62_hold_return_pad_b() {
    xdotool keydown Return; sleep 0.3
    : > /tmp/pad-b; sleep 0.4; rm -f /tmp/pad-b
    sleep 1.5; xdotool keyup Return; sleep 0.5
}
rm -f /tmp/pad-b
cfg=$(writable_config f62-pad)
STREAMFLEX_TEST_PAD=/tmp/pad-b STREAMFLEX_TEST_PAD_BUTTON=b WAIT_FOR='Gamepad connected' CFG=$cfg \
    run_keys f62-keyholdpad $TO_GAMEPAD Down Down Down Return +b62_hold_return_pad_b BackSpace BackSpace Menu
log=$out/f62-keyholdpad.log
ok=1
grep -q 'Settings: capturing a button' "$log" && grep -q 'Settings: capture got ButtonB' "$log" \
    && [ "$(sed -n '/Settings: capture got ButtonB/,$p' "$log" | grep -c 'Key Return (#D) detected')" -gt 2 ] \
    && ! grep -q "Settings: the cursor's row reads Key: ButtonB" "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-keyholdpad && ok=0
result "bindings: OK held through a pad capture does not Keep what it caught (exit $(cat "$out/f62-keyholdpad.code"))" $ok
rm -f /tmp/pad-b

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
