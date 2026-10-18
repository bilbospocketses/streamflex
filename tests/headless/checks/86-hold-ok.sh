# OK in the launcher's menus (3c Task 14, plan ruling 1): it acts as it is let go, not as it is pressed.
# A tap of Return, and of a hotkey bound to :select, selects on its release; in an empty menu it selects
# nothing; inside settings OK still acts on its press. The holds are 86-hold-ok-timing.sh's.
# Not timing-sensitive: a tap's press and release come in one xdotool call, 800 ms short of a hold, and
# the press inside settings is read in the log before its release is sent

P86_ARROW=$(printf ' \xE2\x80\xBA ')

# Return tapped on Bye: its release selects it, and StreamFlex quits
CFG=$FX/f86-hold.ini run_keys f86-tap Right Return
log=$out/f86-tap.log
ok=1
precedes "$log" 'Key Return (#D) detected' 'Hold OK: let go after' \
    && grep -qE '^Hold OK: let go after [0-9]+ ms, so it selects$' "$log" \
    && grep -qx 'Title: Bye' "$log" && grep -q '^Quitting program' "$log" && ! grep -q 'Settings opened' "$log" \
    && ran_clean f86-tap && ok=0
result "hold-ok: Return tapped selects as it is let go (exit $(cat "$out/f86-tap.code"))" $ok
grep -E '^(Key Return|Hold OK|Title:)' "$log" | sed 's/^/      /'

# A hotkey bound to :select (F6) is OK too
CFG=$FX/f86-hold.ini run_keys f86-hotkey Right F6
log=$out/f86-hotkey.log
ok=1
precedes "$log" 'Key F6 (#4000003F) detected' 'Hold OK: let go after' \
    && grep -qx 'Title: Bye' "$log" && ran_clean f86-hotkey && ok=0
result "hold-ok: a hotkey bound to :select selects as it is let go (exit $(cat "$out/f86-hotkey.code"))" $ok

# An empty menu: OK let go there selects nothing, and says so
CFG=$FX/f86-hold.ini run_keys f86-empty Return Return BackSpace
log=$out/f86-empty.log
ok=1
in_range "$log" "Loading menu 'Kids'" "Loading menu 'Main'" "Menu 'Kids' is empty: there is no tile to select" \
    && ! grep -q 'Settings opened' "$log" && ran_clean f86-empty && ok=0
result "hold-ok: OK let go in an empty menu selects nothing (exit $(cat "$out/f86-empty.code"))" $ok

# Inside settings OK acts as it is pressed: the General page opens while Return is still held
p86_press_in_settings() {
    xdotool keydown Return
    wait_line "Settings: page Settings${P86_ARROW}General" "$2" && echo 'pressed yes' >> "$out/$1.seen"
    xdotool keyup Return; sleep 1
}
rm -f "$out/f86-inside.seen"
CFG=$FX/f86-hold.ini run_keys f86-inside Menu +p86_press_in_settings BackSpace BackSpace
ok=1
grep -qx 'pressed yes' "$out/f86-inside.seen" 2> /dev/null && ! grep -q '^Hold OK:' "$out/f86-inside.log" \
    && ran_clean f86-inside && ok=0
result "hold-ok: inside settings OK acts as it is pressed (exit $(cat "$out/f86-inside.code"))" $ok
