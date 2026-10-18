# The installed apps (3c, spec "Installed apps"): the first Add > App of a settings visit starts listing them
# on a thread of their own (Task 7's scan), here over the fixture applications folders (the harness build's
# STREAMFLEX_TEST_APP_DIRS stands in for the XDG folders, the Flatpak exports and Snap's). The log gives the
# count and the time, and the hook build each app in the list's order; a scan that fails or finds nothing
# says why; a folder that is not there holds no apps, and one that cannot be read fails the scan; closing
# settings while it runs stops it and waits for its thread; and a thread that cannot start leaves settings
# working. The list on screen, "Finding apps... (N)" included, is check 82's.
# Not timing-sensitive: every wait ends on a log line (20 s) or a key's frame (60 s); the scan held by
# STREAMFLEX_TEST_APP_SCAN=delay:600000 is stopped by the close however late the close comes, and a close
# that waited it out would miss its frame's 60 s; no result reads a duration

A81=$FX/apps
A81_DIRS=$A81/user:$A81/system:$A81/flatpak:$A81/system
A81_CFG=$FX/f81-apps.ini
A81_ARROW=$(printf ' \xE2\x80\xBA ')
# The top page, Menus, Main, its Tiles row, Add, App: the app list, whose first opening starts the scan
A81_APP="Menu Down Down Return Down Return Down Down Down Return Return Return"
a81_found() { wait_line 'Apps: found' "$2"; }
a81_none() { wait_line 'Apps: none listed' "$2"; }

# A function to list a run's apps as the hook build logged them, joined with |
a81_apps() { grep -o '^Test hook: app [0-9]* is .*' "$1" | sed 's/^Test hook: app [0-9]* is //' | tr '\n' '|'; }

# Every rule at once, in German: the user's Kodi by its German name; the system's Firefox hidden by the
# user's copy (Hidden=true, the same desktop file ID, in an earlier folder); GIMP by its de_DE name;
# NoDisplay, a Link, an uninstaller, a readme, a hidden file and a text file left out; a subfolder's VLC
# and the Flatpak's VLC both listed (two installs: two commands); the system folder named twice adds
# nothing the second time, every file's ID having been read
CFG=$A81_CFG STREAMFLEX_TEST_APP_DIRS=$A81_DIRS LC_ALL= LC_MESSAGES= LANG=de_DE.UTF-8 UNTIL='Settings closed' \
    run_keys f81-apps $A81_APP +a81_found Menu
log=$out/f81-apps.log
a81_list=$(a81_apps "$log")
a81_want="GIMP Bildbearbeitung, run by $A81/system/gimp.desktop|Kodi Mediencenter, run by $A81/user/kodi.desktop|"
a81_want+="Steam, run by $A81/system/steam.desktop|VLC media player, run by $A81/flatpak/org.videolan.VLC.desktop|"
a81_want+="VLC media player, run by $A81/system/sub/vlc.desktop|"
ok=1
grep -qE '^Apps: found 5 apps \([0-9]+ ms\)$' "$log" && precedes "$log" 'Apps: listing the installed apps' 'Apps: found 5 apps' \
    && precedes "$log" "Settings: page Settings${A81_ARROW}Menus${A81_ARROW}Main${A81_ARROW}Tiles${A81_ARROW}Add" 'Apps: listing the installed apps' \
    && [ "$a81_list" = "$a81_want" ] && ran_clean f81-apps && ok=0
result "apps: settings list every folder's apps by the rules, in the system's language (exit $(cat "$out/f81-apps.code"))" $ok
echo "      ${a81_list:-no apps listed}"
grep -E '^Apps:' "$log" | sed 's/^/      /'

# A folder with no .desktop files, and one that is not there: no apps, and that is no failure
CFG=$A81_CFG STREAMFLEX_TEST_APP_DIRS=$A81/empty:$TESTER_HOME/a81-not-there UNTIL='Settings closed' \
    run_keys f81-empty $A81_APP +a81_none Menu
ok=1
grep -qE '^Apps: none listed: No apps were found \([0-9]+ ms\)$' "$out/f81-empty.log" \
    && ! grep -q '^Test hook: app ' "$out/f81-empty.log" && ran_clean f81-empty && ok=0
result "apps: folders with no apps say none were found (exit $(cat "$out/f81-empty.code"))" $ok

# A folder that is there but cannot be read, beside one that is not there: the scan fails, and says
# which folder and why; settings still close as usual
rm -rf "$TESTER_HOME/a81-locked"
mkdir -p "$TESTER_HOME/a81-locked"
cp "$A81/system/steam.desktop" "$TESTER_HOME/a81-locked/"
chmod 000 "$TESTER_HOME/a81-locked"
CFG=$A81_CFG STREAMFLEX_TEST_APP_DIRS=$TESTER_HOME/a81-locked:$TESTER_HOME/a81-not-there UNTIL='Settings closed' \
    run_keys f81-locked $A81_APP +a81_none Menu
chmod 755 "$TESTER_HOME/a81-locked"
rm -rf "$TESTER_HOME/a81-locked"
ok=1
grep -qE "^Apps: none listed: The apps could not be listed: $TESTER_HOME/a81-locked: .+ \([0-9]+ ms\)$" "$out/f81-locked.log" \
    && grep -q '^Settings closed$' "$out/f81-locked.log" && ran_clean f81-locked && ok=0
result "apps: a folder that cannot be read fails the scan, saying which and why (exit $(cat "$out/f81-locked.code"))" $ok
grep -E '^Apps:' "$out/f81-locked.log" | sed 's/^/      /'

# The hook's two outcomes over folders that hold apps: a failure, and nothing found
CFG=$A81_CFG STREAMFLEX_TEST_APP_DIRS=$A81_DIRS STREAMFLEX_TEST_APP_SCAN=fail UNTIL='Settings closed' \
    run_keys f81-fail $A81_APP +a81_none Menu
CFG=$A81_CFG STREAMFLEX_TEST_APP_DIRS=$A81_DIRS STREAMFLEX_TEST_APP_SCAN=empty UNTIL='Settings closed' \
    run_keys f81-hookempty $A81_APP +a81_none Menu
ok=1
grep -qE '^Apps: none listed: The apps could not be listed: the test hook failed it \([0-9]+ ms\)$' "$out/f81-fail.log" \
    && grep -qE '^Apps: none listed: No apps were found \([0-9]+ ms\)$' "$out/f81-hookempty.log" \
    && ! grep -q '^Test hook: app ' "$out/f81-fail.log" "$out/f81-hookempty.log" \
    && ran_clean f81-fail && ran_clean f81-hookempty && ok=0
result "apps: a scan that fails or finds nothing says so, and lists nothing (exits $(cat "$out/f81-fail.code"), $(cat "$out/f81-hookempty.code"))" $ok

# Settings closed while the scan runs (held for ten minutes; Review Focus 4): the close stops it and waits
# for its thread before the list it fills is freed, the leak pass finds nothing left, and no verdict is logged
CFG=$A81_CFG STREAMFLEX_TEST_APP_DIRS=$A81_DIRS STREAMFLEX_TEST_APP_SCAN=delay:600000 UNTIL='Settings closed' \
    run_keys f81-closed $A81_APP Menu
log=$out/f81-closed.log
ok=1
grep -qE '^Apps: stopped the app scan as settings closed \([0-9]+ ms\)$' "$log" \
    && precedes "$log" 'Apps: listing the installed apps' 'Apps: stopped the app scan as settings closed' \
    && precedes "$log" 'Apps: stopped the app scan as settings closed' 'Settings closed' \
    && ! grep -qE '^Apps: (found|none listed)' "$log" && ran_clean f81-closed && ok=0
result "apps: closing settings mid-scan stops the scan and waits for it (exit $(cat "$out/f81-closed.code"))" $ok
grep -E '^(Apps:|Settings closed)' "$log" | sed 's/^/      /'

# A thread that cannot start: settings say why, and work as before
CFG=$A81_CFG STREAMFLEX_TEST_APP_DIRS=$A81_DIRS STREAMFLEX_TEST_FAIL=appthread UNTIL='Settings closed' \
    run_keys f81-nothread $A81_APP Menu
log=$out/f81-nothread.log
ok=1
grep -q '^Test hook: appthread fails$' "$log" \
    && grep -q '^Apps: the apps cannot be listed: out of memory, or no thread$' "$log" \
    && ! grep -q '^Apps: listing the installed apps' "$log" && grep -q '^Settings closed$' "$log" \
    && ran_clean f81-nothread && ok=0
result "apps: a scan thread that cannot start leaves settings working (exit $(cat "$out/f81-nothread.code"))" $ok
