# The settings screen's timing-sensitive cases, from 50-settings.sh: keys held against the X server's
# repeat delay, keys sent faster than the Menus preview's rest, and a key that must land inside an
# application's launch
# Timing-sensitive (run.sh <label> [leaks] [K/N] timing): a loaded box could stretch the holds and
# the gaps these cases time, or move a key past the window it must land in.

# A held Menu key opens settings once, and they stay open: the keyboard's repeats of it do nothing.
# The same for a remote's Menu button, SDLK_MENU (X's XF86MenuKB, where Menu is SDLK_APPLICATION).
# The key must have come more than once, or nothing was repeated and the check proves nothing. Its
# lines are matched by keycode alone: SDL names #40000065 Application or Menu, by version.
hold_menu() { xdotool keydown Menu; sleep 2; xdotool keyup Menu; sleep 1; }
hold_menukb() { xdotool keydown XF86MenuKB; sleep 2; xdotool keyup XF86MenuKB; sleep 1; }
for held in held:Menu:40000065 heldkb:MenuKB:40000076; do
    IFS=: read -r name key code <<< "$held"
    CFG=$FX/f50-keys.ini run_keys "f50-$name" "+hold_${key,,}"
    n=$(grep -c "^Key .* (#$code) detected$" "$out/f50-$name.log")
    ok=1
    [ "$n" -gt 1 ] && [ "$(grep -c 'Settings opened' "$out/f50-$name.log")" = 1 ] \
        && ! grep -q 'Settings closed' "$out/f50-$name.log" && ran_clean "f50-$name" && ok=0
    result "settings: a held $key key (#$code) opens them once, and they stay open (exit $(cat "$out/f50-$name.code"))" $ok
    echo "      the key came $n times; settings opened $(grep -c 'Settings opened' "$out/f50-$name.log") times"
done

# The same for a hotkey the config binds to :settings (F1 here): held, it opens them once
hold_f1() { xdotool keydown F1; sleep 2; xdotool keyup F1; sleep 1; }
CFG=$FX/f50-hotkeyset.ini run_keys f50-heldhotkey +hold_f1
n=$(grep -c 'Key F1 (#4000003A) detected' "$out/f50-heldhotkey.log")
ok=1
[ "$n" -gt 1 ] && [ "$(grep -c 'Settings opened' "$out/f50-heldhotkey.log")" = 1 ] \
    && ! grep -q 'Settings closed' "$out/f50-heldhotkey.log" && ran_clean f50-heldhotkey && ok=0
result "settings: a held hotkey bound to :settings opens them once, and they stay open (exit $(cat "$out/f50-heldhotkey.code"))" $ok
echo "      the key came $n times; settings opened $(grep -c 'Settings opened' "$out/f50-heldhotkey.log") times"

# Moving quickly down the Menus list (five menus of twelve 556 px SVG icons) does not lay out, or
# rasterize the icons of, each menu on the way: the preview follows once the cursor rests, on D.
# Going up to C and back to D shows D again without rendering its buttons a second time. The Downs
# come 50 ms apart, far inside the 300 ms rest, so a loaded host cannot stretch a gap past it.
fast_downs() { xdotool key --delay 50 Down Down Down Down Down; sleep 2; }
revisit() { xdotool key Up; sleep 1; xdotool key Down; sleep 1; }
CFG=$FX/f50-menus.ini run_keys f50-menus Menu Down Down Return +fast_downs +revisit Menu
log=$out/f50-menus.log
ok=1
in_range "$log" 'Settings opened' 'Settings closed' "Settings: the preview shows menu 'D'" \
    && ! sed -n "/Settings opened/,/Settings: the preview shows menu 'D'/p" "$log" | grep -qE "Loading menu '[ABC]'" \
    && [ "$(grep -c "Menu 'D': rendered its buttons" "$log")" = 1 ] && ran_clean f50-menus && ok=0
result "settings: the Menus list's preview waits for the cursor to rest, and never renders a menu twice (exit $(cat "$out/f50-menus.code"))" $ok
grep -E "Loading menu|rendered its buttons|the preview shows menu|kept the screen waiting" "$log" | sed 's/^/      /'

# Settings do not open while an application is launching (sleep 6, which never takes the screen,
# so the launch lasts ApplicationTimeout, 3 s): the Menu key is refused with a log line, and
# opens them once the launch is over. Had the first press opened them, the second would close them.
wait_launch() { sleep 3; }
CFG=$FX/f50-launch.ini run_keys f50-launch Return Menu +wait_launch Menu
ok=1
precedes "$out/f50-launch.log" 'Settings: not opened while an application is launching or running' 'Settings opened' \
    && [ "$(grep -c 'Settings opened' "$out/f50-launch.log")" = 1 ] && ! grep -q 'Settings closed' "$out/f50-launch.log" \
    && ran_clean f50-launch && ok=0
result "settings: they do not open while an application is launching (exit $(cat "$out/f50-launch.code"))" $ok
grep -E 'Settings( opened|: not opened| closed)' "$out/f50-launch.log" | sed 's/^/      /'
