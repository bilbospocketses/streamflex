# Key and gamepad bindings (3b), the first half of the timing-sensitive cases from
# 62-settings-bindings.sh: keys held against the keyboard's repeat, and the 5 s capture and the 10 s
# confirmation waited out. The second half, the pad's taps and the launcher stopped, is
# 62-settings-bindings-pad-timing.sh. f62-bind's run is not timing-sensitive; it is here because two
# results read its log beside f62-revert's.
# Timing-sensitive: key holds against the repeat, and the 5 s capture and the 10 s confirmation waited out

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

# --- Final review fix wave ---

# OK held on the Key row with the gamepad off: the refusal comes once for the press, its repeats wait
# until it is let go (as a key held as a capture ends does), and OK pressed again refuses once more
b62_hold_return_refused() { xdotool keydown Return; sleep 2; xdotool keyup Return; sleep 0.5; }
CFG=$FX/f62-padoff.ini run_keys f62-capheld $TO_GAMEPAD Down Down Down Return +b62_hold_return_refused Return Menu
log=$out/f62-capheld.log
ok=1
[ "$(grep -c 'Settings: the capture cannot start: Turn the gamepad on to capture a button' "$log")" = 2 ] \
    && [ "$(sed -n '/Settings: the capture cannot start/,$p' "$log" | grep -c 'Key Return (#D) detected')" -gt 3 ] \
    && ! grep -q 'Settings: capturing a' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f62-capheld && ok=0
result "bindings: a held OK refused a pad's capture once, and a new press refuses again (exit $(cat "$out/f62-capheld.code"))" $ok
echo "      refusals: $(grep -c 'Settings: the capture cannot start' "$log")"
