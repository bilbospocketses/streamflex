# The desktop's media keys (v0.6.0). At every start the log says which of next and previous track KDE
# Plasma or GNOME holds, and while it holds either and [Desktop Keys] Ask is not no, settings open on the
# Media keys prompt: Turn them off saves the shortcuts to [Desktop Keys] and only then clears them, Not now
# changes nothing, and Don't ask again saves Ask=no. Controls > Desktop media keys gives them back: the saved
# ones, else the desktop's defaults. The harness build's STREAMFLEX_TEST_MEDIAKEYS names a file that stands
# in for the desktop (src/platform/mediakeys_unix.c): its first line plasma, gnome or none, then each key's
# value, a line each, then any fail= lines, which make a step fail. util.c's [Desktop Keys] reader needs SDL,
# so this file is its only test: a config holding the section never loads it as a menu, and the Ask=no and
# saved-shortcut cases start from a section written into the config before the launch.

F68_ARROW=$(printf ' \xE2\x80\xBA ')
F68_LINK=$(printf ' \xE2\x80\xBA')        # After the value of a row OK opens
F68_DOT=$(printf ' \xC2\xB7 ')
F68_DESKTOP=/tmp/f68-desktop              # The file standing in for the desktop
F68_PROMPT="Settings: page Settings${F68_ARROW}Media keys"
F68_FAILED="Settings: page Settings${F68_ARROW}Media keys${F68_ARROW}Couldn't change shortcuts"
F68_TO_ROW="Menu Down Down Down Down Down Down Down Down Return Down Down"   # Controls, then its Desktop media keys row
F68_NOTE="uses the next-track and previous-track keys for its own media controls, so StreamFlex never receives them."
F68_NOTE_END="shortcuts for them off? Other music players will stop responding to these keys until you turn them back on in Settings${F68_ARROW}Controls${F68_ARROW}Desktop media keys."
F68_PLASMA=(plasma 16777347,0,0,0 16777346,0,0,0)   # Plasma 6's own shortcuts: Media Next and Media Previous
F68_OWN_NEXT='16777347,0,0,0|117440594,0,0,0'      # Next track with a second shortcut of the user's own: no default
F68_GNOME=(gnome "['']" "['']" "['XF86AudioNext', '<Ctrl>XF86AudioNext']" "['XF86AudioPrev', '<Ctrl>XF86AudioPrev']")

# A function to stand the desktop in: the file's lines, one an argument, which the test user can write
f68_desktop() { printf '%s\n' "$@" > "$F68_DESKTOP"; chmod 666 "$F68_DESKTOP"; }

# A function to tell whether the file standing in for the desktop holds exactly these lines
f68_desktop_is() { printf '%s\n' "$@" > "$out/f68-want"; cmp -s "$out/f68-want" "$F68_DESKTOP"; }

# A function to write the config NAME.ini, which the test user can write: one menu, then the lines given
# (a [Desktop Keys] section, written before the launch), with the StartupCmd F68_STARTUP when that is set;
# prints its path, and keeps a copy as NAME.before
f68_config() {
    local name=$1 cfg=$TESTER_HOME/cfg/$1.ini; shift
    mkdir -p "$TESTER_HOME/cfg"
    rm -f "$cfg" "$cfg.bak" "$cfg.tmp" "$cfg.bak.tmp"
    { printf '%s\n' '[General]' 'DefaultMenu=Main'
      [ -z "${F68_STARTUP:-}" ] || printf 'StartupCmd=%s\n' "$F68_STARTUP"
      printf '%s\n' '' '[Main]' 'Entry1=One;apps;:quit' "$@"; } > "$cfg"
    cp "$cfg" "$out/$name.before"
    chown -R tester:tester "$TESTER_HOME/cfg"
    chmod 644 "$cfg"
    echo "$cfg"
}

# A function to give a config's [Desktop Keys] section, its header and lines
f68_section() { sed -n '/^\[Desktop Keys\]/,/^\[/p' "$1"; }

# A function to tell whether a config holds no saved shortcuts: no Desktop line and no value line
f68_nothing_saved() { ! grep -qE '^(Desktop|PlasmaNext|PlasmaPrevious|GnomeNext|GnomePrevious|GnomeNextStatic|GnomePreviousStatic)=' "$1"; }

# A function to list the values the Controls row Desktop media keys read under the cursor, in order, joined by |
f68_row_values() {
    sed -n "s/^Settings: the cursor's row reads Desktop media keys: \(.*\)$F68_LINK\$/\1/p" "$out/$1.log" | paste -sd'|'
}

# A function to tell whether a run's log loaded one menu, Main, and none for [Desktop Keys]
f68_one_menu() {
    [ "$(grep -c '^Menu Name: ' "$out/$1.log")" = 1 ] && grep -qx 'Menu Name: Main' "$out/$1.log" \
        && ! grep -qE "^Menu Name: Desktop Keys|Menu 'Desktop Keys'" "$out/$1.log"
}

# A function to show a run's media-key lines and pages under its result
f68_show() { grep -E '^(Media keys|Settings: (page|the note|the cursor|nothing changed)|Settings (opened|closed)|Couldn)' "$out/$1.log" | sed 's/^/      /'; }

# The prompt at start: Plasma holds both keys and the config has no [Desktop Keys]. It says why, its Back
# closes settings (the key hint says so), and Not now closes them with the config and the desktop untouched
f68_desktop "${F68_PLASMA[@]}"
cfg=$(f68_config f68-prompt)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg WAIT_FOR=$F68_PROMPT UNTIL='Settings closed' run_keys f68-prompt Down Return
log=$out/f68-prompt.log
ok=1
grep -qx 'Media keys: KDE Plasma holds next track and previous track' "$log" \
    && precedes "$log" 'Media keys: KDE Plasma holds' "Settings opened over menu 'Main'" \
    && grep -qxF "$F68_PROMPT" "$log" \
    && grep -qxF "Settings: the note says KDE Plasma $F68_NOTE Turn Plasma's $F68_NOTE_END" "$log" \
    && grep -qxF "Settings: the key hint reads Left and right change${F68_DOT}OK opens${F68_DOT}Back closes" "$log" \
    && in_range "$log" 'Settings opened' 'Settings closed' 'Settings: nothing changed' \
    && cmp -s "$out/f68-prompt.before" "$cfg" && [ ! -e "$cfg.bak" ] && f68_desktop_is "${F68_PLASMA[@]}" \
    && ! grep -q '^Media keys: saved' "$log" && ran_clean f68-prompt && ok=0
result "media keys: the prompt opens at start while Plasma holds them, and Not now changes nothing (exit $(cat "$out/f68-prompt.code"))" $ok
f68_show f68-prompt

# [Desktop Keys] Ask=no, written before the launch: no prompt, though Plasma holds both keys, and the section
# is read as no menu
f68_desktop "${F68_PLASMA[@]}"
cfg=$(f68_config f68-askno '' '[Desktop Keys]' 'Ask=no')
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Begin program loop' run_keys f68-askno
log=$out/f68-askno.log
ok=1
grep -qx 'Media keys: KDE Plasma holds next track and previous track' "$log" && ! grep -q 'Settings opened' "$log" \
    && f68_one_menu f68-askno && ran_clean f68-askno && ok=0
result "media keys: Ask=no written by hand opens no prompt, and [Desktop Keys] is no menu (exit $(cat "$out/f68-askno.code"))" $ok
grep -E '^(Media keys|Menu Name)' "$log" | sed 's/^/      /'

# No desktop that holds media keys: nothing is logged and nothing opens. A desktop that cannot be read is
# logged, and nothing opens either
f68_desktop none
cfg=$(f68_config f68-none)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Begin program loop' run_keys f68-none
f68_desktop "${F68_PLASMA[@]}" fail=read
cfg=$(f68_config f68-readfail)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Begin program loop' run_keys f68-readfail
ok=1
! grep -q '^Media keys' "$out/f68-none.log" && ! grep -q 'Settings opened' "$out/f68-none.log" \
    && grep -qx "Media keys: could not read Plasma's shortcuts (the test file fails read)" "$out/f68-readfail.log" \
    && [ "$(grep -c '^Media keys' "$out/f68-readfail.log")" = 1 ] && ! grep -q 'Settings opened' "$out/f68-readfail.log" \
    && ran_clean f68-none && ran_clean f68-readfail && ok=0
result "media keys: no desktop logs nothing, a failed read is logged, and neither opens the prompt (exit $(cat "$out/f68-none.code") and $(cat "$out/f68-readfail.code"))" $ok
grep '^Media keys' "$out/f68-readfail.log" | sed 's/^/      /'

# Don't ask again saves Ask=no and closes settings, the desktop untouched; the next start asks nothing
f68_desktop "${F68_PLASMA[@]}"
cfg=$(f68_config f68-never)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg WAIT_FOR=$F68_PROMPT UNTIL='Settings closed' run_keys f68-never Down Down Return
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Begin program loop' run_keys f68-never-next
ok=1
f68_section "$cfg" | grep -qx 'Ask=no' && f68_nothing_saved "$cfg" \
    && grep -qxF "Media keys: will not ask again (saved Ask=no to $cfg)" "$out/f68-never.log" \
    && precedes "$out/f68-never.log" 'Media keys: will not ask again' 'Settings closed' \
    && f68_desktop_is "${F68_PLASMA[@]}" && cmp -s "$out/f68-never.before" "$cfg.bak" \
    && grep -q '^Media keys: KDE Plasma holds' "$out/f68-never-next.log" && ! grep -q 'Settings opened' "$out/f68-never-next.log" \
    && f68_one_menu f68-never-next && ran_clean f68-never && ran_clean f68-never-next && ok=0
result "media keys: Don't ask again saves Ask=no, and the next start asks nothing (exit $(cat "$out/f68-never.code") and $(cat "$out/f68-never-next.code"))" $ok
f68_show f68-never
f68_section "$cfg" | sed 's/^/      /'

# Turn them off on the prompt: the shortcuts are saved to [Desktop Keys] first, then cleared, and settings
# close; the config's old copy is kept as .bak
f68_desktop "${F68_PLASMA[@]}"
cfg=$(f68_config f68-off)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg WAIT_FOR=$F68_PROMPT UNTIL='Settings closed' run_keys f68-off Return
log=$out/f68-off.log
section=$(f68_section "$cfg")
ok=1
grep -qx 'Desktop=plasma' <<< "$section" && grep -qx 'PlasmaNext=16777347,0,0,0' <<< "$section" \
    && grep -qx 'PlasmaPrevious=16777346,0,0,0' <<< "$section" && ! grep -q '^Ask=' <<< "$section" \
    && cmp -s "$out/f68-off.before" "$cfg.bak" \
    && precedes "$log" "Media keys: saved Plasma's shortcuts to [Desktop Keys]" "Media keys: turned off Plasma's next- and previous-track shortcuts" \
    && precedes "$log" "Media keys: turned off Plasma's next- and previous-track shortcuts" 'Settings closed' \
    && grep -qx "Media keys: turned off Plasma's next- and previous-track shortcuts" "$log" \
    && f68_desktop_is plasma '' '' && ran_clean f68-off && ok=0
result "media keys: Turn them off saves the shortcuts, then clears them, then closes settings (exit $(cat "$out/f68-off.code"))" $ok
f68_show f68-off
sed 's/^/      /' <<< "$section"

# A clear that fails: the saved values are removed again, the desktop is as it was, and the failure page says
# why; its OK goes back to the prompt, whose Not now closes settings
f68_desktop "${F68_PLASMA[@]}" fail=clear
cfg=$(f68_config f68-failclear)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg WAIT_FOR=$F68_PROMPT UNTIL='Settings closed' run_keys f68-failclear Return Return Down Return
log=$out/f68-failclear.log
ok=1
f68_nothing_saved "$cfg" && f68_desktop_is "${F68_PLASMA[@]}" fail=clear \
    && precedes "$log" "Media keys: saved Plasma's shortcuts" "Media keys: could not change Plasma's shortcuts (the test file fails clear), so nothing was changed" \
    && grep -qx "Media keys: could not change Plasma's shortcuts (the test file fails clear), so nothing was changed" "$log" \
    && grep -qxF "$F68_FAILED" "$log" \
    && grep -qxF "Settings: the note says Couldn't change Plasma's shortcuts: the test file fails clear. Nothing was changed." "$log" \
    && [ "$(grep -cxF "$F68_PROMPT" "$log")" = 2 ] && ! grep -q '^Media keys: turned off' "$log" \
    && ran_clean f68-failclear && ok=0
result "media keys: a clear that fails removes the saved values, and says why (exit $(cat "$out/f68-failclear.code"))" $ok
f68_show f68-failclear

# The second key's clear fails once the first is cleared: the first is put back, so the desktop ends as it
# started, and nothing stays saved
f68_desktop "${F68_PLASMA[@]}" fail=clear-second
cfg=$(f68_config f68-failsecond)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg WAIT_FOR=$F68_PROMPT UNTIL="$F68_FAILED" run_keys f68-failsecond Return
ok=1
f68_nothing_saved "$cfg" && f68_desktop_is "${F68_PLASMA[@]}" fail=clear-second \
    && grep -qx "Media keys: could not change Plasma's shortcuts (the test file fails clear-second), so nothing was changed" "$out/f68-failsecond.log" \
    && ran_clean f68-failsecond && ok=0
result "media keys: a clear that fails halfway leaves the desktop as it started (exit $(cat "$out/f68-failsecond.code"))" $ok
f68_show f68-failsecond

# The same, but the first key cannot be put back: the log and the failure page say some of the shortcuts may
# have changed, never that nothing was, and the saved shortcuts stay in [Desktop Keys], the only way back for
# the key left cleared; OK goes back to the prompt, whose Not now closes settings
f68_desktop "${F68_PLASMA[@]}" fail=clear-second fail=put-back
cfg=$(f68_config f68-clearputback)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg WAIT_FOR=$F68_PROMPT UNTIL='Settings closed' \
    run_keys f68-clearputback Return Return Down Return
log=$out/f68-clearputback.log
section=$(f68_section "$cfg")
ok=1
f68_desktop_is plasma '' 16777346,0,0,0 fail=clear-second fail=put-back \
    && grep -qx 'Desktop=plasma' <<< "$section" && grep -qx 'PlasmaNext=16777347,0,0,0' <<< "$section" \
    && grep -qx 'PlasmaPrevious=16777346,0,0,0' <<< "$section" \
    && grep -qxF "Media keys: could not change Plasma's shortcuts (the test file fails clear-second; nextmedia could not be put back), so some of them may have changed" "$log" \
    && grep -qxF 'Media keys: the saved shortcuts stay in [Desktop Keys], for Give them back' "$log" \
    && grep -qxF "Settings: the note says Couldn't change Plasma's shortcuts: the test file fails clear-second; nextmedia could not be put back. Some of them may have changed." "$log" \
    && grep -qxF "$F68_FAILED" "$log" && ! grep -qiF 'nothing was changed' "$log" \
    && ! grep -q '^Media keys: turned off' "$log" && ran_clean f68-clearputback && ok=0
result "media keys: a clear whose put-back fails says so, and keeps the saved shortcuts (exit $(cat "$out/f68-clearputback.code"))" $ok
f68_show f68-clearputback
sed 's/^/      /' <<< "$section"

# A save that fails (a config the launcher cannot write) clears nothing and shows the Couldn't save page.
# Its Try again, once the config can be written, turns the keys off: it makes the media-key choice again,
# not the settings' own save (which would find nothing changed and only close)
f68_writable() { chmod 644 "$f68_cfg"; sleep 1; }
f68_desktop "${F68_PLASMA[@]}"
f68_cfg=$(f68_config f68-failsave)
chmod 444 "$f68_cfg"
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$f68_cfg WAIT_FOR=$F68_PROMPT UNTIL='Settings closed' \
    run_keys f68-failsave Return +f68_writable Return
log=$out/f68-failsave.log
ok=1
grep -qF "Couldn't save to $f68_cfg: permission denied" "$log" \
    && grep -qxF "Settings: page Settings${F68_ARROW}Media keys${F68_ARROW}Couldn't save" "$log" \
    && precedes "$log" "Couldn't save to $f68_cfg" "Media keys: saved Plasma's shortcuts" \
    && [ "$(grep -c "^Media keys: saved Plasma's shortcuts" "$log")" = 1 ] \
    && precedes "$log" "Media keys: turned off Plasma's" 'Settings closed' && ! grep -q 'Settings: nothing changed' "$log" \
    && f68_section "$f68_cfg" | grep -qx 'Desktop=plasma' && f68_desktop_is plasma '' '' && ran_clean f68-failsave && ok=0
result "media keys: a failed save clears nothing, and its Try again turns them off (exit $(cat "$out/f68-failsave.code"))" $ok
f68_show f68-failsave

# Controls > Desktop media keys, Ask=no written by hand: the row reads On while Plasma holds them; Turn them off
# there saves and clears them and stays, the row then reads Off; Give them back gives the saved ones back and
# forgets them, and the row reads On again. Ask=no stays. The row never says whether a copy is saved, so three
# things prove one was: next track has a shortcut of the user's own, which the defaults would not give back;
# the log says the saved ones came back; and the .bak that Give them back's save kept is the config Turn them
# off wrote, holding them
f68_desktop plasma "$F68_OWN_NEXT" 16777346,0,0,0
cfg=$(f68_config f68-controls '' '[Desktop Keys]' 'Ask=no')
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Settings closed' \
    run_keys f68-controls $F68_TO_ROW Return Down Return BackSpace Return Return BackSpace BackSpace BackSpace
log=$out/f68-controls.log
section=$(f68_section "$cfg.bak")
ok=1
[ "$(f68_row_values f68-controls)" = 'On|Off|On' ] \
    && grep -qxF "Settings: page Settings${F68_ARROW}Controls${F68_ARROW}Desktop media keys" "$log" \
    && ! grep -qxF "$F68_PROMPT" "$log" \
    && precedes "$log" "Media keys: saved Plasma's shortcuts" "Media keys: turned off Plasma's" \
    && precedes "$log" "Media keys: turned off Plasma's" 'Media keys: gave KDE Plasma back its saved shortcuts' \
    && grep -qx 'Media keys: gave KDE Plasma back its saved shortcuts' "$log" \
    && grep -qx 'Ask=no' <<< "$section" && grep -qx 'Desktop=plasma' <<< "$section" \
    && grep -qxF "PlasmaNext=$F68_OWN_NEXT" <<< "$section" && grep -qx 'PlasmaPrevious=16777346,0,0,0' <<< "$section" \
    && f68_desktop_is plasma "$F68_OWN_NEXT" 16777346,0,0,0 && f68_section "$cfg" | grep -qx 'Ask=no' \
    && f68_nothing_saved "$cfg" && ran_clean f68-controls && ok=0
result "media keys: Controls turns them off and gives the saved ones back, the row On, then Off, then On (exit $(cat "$out/f68-controls.code"))" $ok
echo "      the row read: $(f68_row_values f68-controls)"
f68_show f68-controls
sed 's/^/      /' <<< "$section"

# Shortcuts saved before the launch, by hand: the row reads Off while the desktop holds none, and Give them
# back gives exactly them back, then forgets them (Ask=no stays); [Desktop Keys] is no menu. What proves the
# save was read: the log says the saved ones came back, next track ends with its shortcut of the user's own,
# which the defaults would not give, and the .bak that Give them back's save kept is the config as written
f68_desktop plasma '' ''
cfg=$(f68_config f68-givesaved '' '[Desktop Keys]' 'Ask=no' 'Desktop=plasma' "PlasmaNext=$F68_OWN_NEXT" \
      'PlasmaPrevious=16777346,0,0,0')
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Settings closed' \
    run_keys f68-givesaved $F68_TO_ROW Return Return BackSpace BackSpace BackSpace
ok=1
[ "$(f68_row_values f68-givesaved)" = 'Off|On' ] \
    && grep -qx 'Media keys: gave KDE Plasma back its saved shortcuts' "$out/f68-givesaved.log" \
    && f68_desktop_is plasma "$F68_OWN_NEXT" 16777346,0,0,0 && cmp -s "$out/f68-givesaved.before" "$cfg.bak" \
    && f68_section "$cfg" | grep -qx 'Ask=no' && f68_nothing_saved "$cfg" && f68_one_menu f68-givesaved \
    && ran_clean f68-givesaved && ok=0
result "media keys: shortcuts saved before the launch read Off, and come back as saved (exit $(cat "$out/f68-givesaved.code"))" $ok
echo "      the row read: $(f68_row_values f68-givesaved)"
f68_show f68-givesaved

# Nothing saved: the row reads Off while the desktop holds none, and Give them back gives the defaults
f68_desktop plasma '' ''
cfg=$(f68_config f68-givedefault '' '[Desktop Keys]' 'Ask=no')
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Settings closed' \
    run_keys f68-givedefault $F68_TO_ROW Return Return BackSpace BackSpace BackSpace
ok=1
[ "$(f68_row_values f68-givedefault)" = 'Off|On' ] \
    && grep -qx 'Media keys: gave KDE Plasma back its default shortcuts' "$out/f68-givedefault.log" \
    && f68_desktop_is "${F68_PLASMA[@]}" && f68_section "$cfg" | grep -qx 'Ask=no' && ran_clean f68-givedefault && ok=0
result "media keys: with nothing saved the row reads Off, and Give them back gives the defaults (exit $(cat "$out/f68-givedefault.code"))" $ok
echo "      the row read: $(f68_row_values f68-givedefault)"
f68_show f68-givedefault

# A save written by hand that lacks PlasmaNext is read as nothing saved: the row reads Off, and Give them back
# gives the defaults, never an empty next-track key; the partial save is removed with it. The .bak that Give
# them back's save kept is the config as written: the partial save was still there, and was not used
f68_desktop plasma '' ''
cfg=$(f68_config f68-missing '' '[Desktop Keys]' 'Ask=no' 'Desktop=plasma' 'PlasmaPrevious=16777346,0,0,0')
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Settings closed' \
    run_keys f68-missing $F68_TO_ROW Return Return BackSpace BackSpace BackSpace
ok=1
[ "$(f68_row_values f68-missing)" = 'Off|On' ] \
    && grep -qx 'No PlasmaNext value in \[Desktop Keys\], so no saved KDE Plasma shortcuts are read' "$out/f68-missing.log" \
    && grep -qx 'Media keys: gave KDE Plasma back its default shortcuts' "$out/f68-missing.log" \
    && f68_desktop_is "${F68_PLASMA[@]}" && cmp -s "$out/f68-missing.before" "$cfg.bak" \
    && f68_nothing_saved "$cfg" && f68_section "$cfg" | grep -qx 'Ask=no' && ran_clean f68-missing && ok=0
result "media keys: a save missing PlasmaNext reads as nothing saved, and gives the defaults back (exit $(cat "$out/f68-missing.code"))" $ok
echo "      the row read: $(f68_row_values f68-missing)"
f68_show f68-missing

# A save written by hand whose PlasmaPrevious is not key sequences is read as nothing saved, as the desktop
# would refuse it: the row reads Off, and Give them back gives the defaults. Its PlasmaNext, a valid shortcut of
# the user's own, would end on the desktop had the save been used, so both keys ending as the defaults proves it
# was not; and the .bak that Give them back's save kept is the config as written, the save still there
f68_desktop plasma '' ''
cfg=$(f68_config f68-unparsable '' '[Desktop Keys]' 'Ask=no' 'Desktop=plasma' "PlasmaNext=$F68_OWN_NEXT" 'PlasmaPrevious=abc')
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Settings closed' \
    run_keys f68-unparsable $F68_TO_ROW Return Return BackSpace BackSpace BackSpace
ok=1
[ "$(f68_row_values f68-unparsable)" = 'Off|On' ] \
    && grep -qxF "Invalid PlasmaPrevious value 'abc' in [Desktop Keys], so no saved KDE Plasma shortcuts are read" "$out/f68-unparsable.log" \
    && grep -qx 'Media keys: gave KDE Plasma back its default shortcuts' "$out/f68-unparsable.log" \
    && f68_desktop_is "${F68_PLASMA[@]}" && cmp -s "$out/f68-unparsable.before" "$cfg.bak" \
    && f68_nothing_saved "$cfg" && f68_section "$cfg" | grep -qx 'Ask=no' && ran_clean f68-unparsable && ok=0
result "media keys: a saved PlasmaPrevious that is not key sequences reads as nothing saved (exit $(cat "$out/f68-unparsable.code"))" $ok
echo "      the row read: $(f68_row_values f68-unparsable)"
f68_show f68-unparsable

# Give them back that fails on the second key, once the first is set: the first is put back, so the desktop
# ends exactly as it started, the saved shortcuts stay, and the failure page says nothing was changed. Then,
# the desktop able to take them, Give them back again gives the saved ones: the log says so, and next track
# ends with its shortcut of the user's own, which the defaults would not give; so the failure kept the save,
# in the config (the .bak that save kept is the config as written) and in what settings hold
F68_SAVED=('[Desktop Keys]' 'Ask=no' 'Desktop=plasma' "PlasmaNext=$F68_OWN_NEXT" 'PlasmaPrevious=16777346,0,0,0')
F68_GIVE_FAILED="Settings: page Settings${F68_ARROW}Controls${F68_ARROW}Desktop media keys${F68_ARROW}Couldn't change shortcuts"
# A function, run once the failure page's OK is back on the page, to note the desktop as it started and the
# config untouched, then to let the desktop take the shortcuts
f68_heal() {
    f68_desktop_is plasma '' '' fail=write-second && cmp -s "$out/$1.before" "$cfg" && [ ! -e "$cfg.bak" ] \
        && touch "$out/$1.as-failed"
    f68_desktop plasma '' ''
}
f68_desktop plasma '' '' fail=write-second
rm -f "$out/f68-failwrite.as-failed"
cfg=$(f68_config f68-failwrite '' "${F68_SAVED[@]}")
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Settings closed' \
    run_keys f68-failwrite $F68_TO_ROW Return Return Return +f68_heal Return BackSpace BackSpace BackSpace
log=$out/f68-failwrite.log
ok=1
[ -e "$out/f68-failwrite.as-failed" ] \
    && grep -qxF "Media keys: could not change Plasma's shortcuts (the test file fails write-second), so nothing was changed" "$log" \
    && grep -qxF "$F68_GIVE_FAILED" "$log" \
    && grep -qxF "Settings: the note says Couldn't change Plasma's shortcuts: the test file fails write-second. Nothing was changed." "$log" \
    && precedes "$log" "Media keys: could not change Plasma's shortcuts" 'Media keys: gave KDE Plasma back its saved shortcuts' \
    && [ "$(grep -c '^Media keys: gave' "$log")" = 1 ] \
    && grep -qx 'Media keys: gave KDE Plasma back its saved shortcuts' "$log" \
    && f68_desktop_is plasma "$F68_OWN_NEXT" 16777346,0,0,0 && cmp -s "$out/f68-failwrite.before" "$cfg.bak" \
    && f68_nothing_saved "$cfg" && f68_section "$cfg" | grep -qx 'Ask=no' \
    && [ "$(f68_row_values f68-failwrite)" = 'Off|On' ] && ran_clean f68-failwrite && ok=0
result "media keys: Give them back that fails on the second key leaves the desktop and the save as they were, and the save still comes back (exit $(cat "$out/f68-failwrite.code"))" $ok
echo "      the row read: $(f68_row_values f68-failwrite)"
f68_show f68-failwrite

# The same, but the first key cannot be put back: the log and the failure page say some of the shortcuts may
# have changed, never that nothing was; the row reads On, as the desktop now holds next track. The key left
# set holds the saved shortcut of the user's own, which the defaults would not give: the save was used
f68_desktop plasma '' '' fail=write-second fail=put-back
cfg=$(f68_config f68-putback '' "${F68_SAVED[@]}")
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Settings closed' \
    run_keys f68-putback $F68_TO_ROW Return Return Return BackSpace BackSpace BackSpace
log=$out/f68-putback.log
ok=1
f68_desktop_is plasma "$F68_OWN_NEXT" '' fail=write-second fail=put-back && cmp -s "$out/f68-putback.before" "$cfg" \
    && grep -qxF "Media keys: could not change Plasma's shortcuts (the test file fails write-second; nextmedia could not be put back), so some of them may have changed" "$log" \
    && grep -qxF "$F68_GIVE_FAILED" "$log" \
    && grep -qxF "Settings: the note says Couldn't change Plasma's shortcuts: the test file fails write-second; nextmedia could not be put back. Some of them may have changed." "$log" \
    && ! grep -qiF 'nothing was changed' "$log" \
    && [ "$(f68_row_values f68-putback)" = 'Off|On' ] && ran_clean f68-putback && ok=0
result "media keys: a Give them back whose put-back fails says some shortcuts may have changed (exit $(cat "$out/f68-putback.code"))" $ok
echo "      the row read: $(f68_row_values f68-putback)"
f68_show f68-putback

# A desktop that cannot be read as settings open: the Controls row shows grayed, reading Unknown, with the
# read's reason under the preview, and its OK opens nothing
f68_desktop "${F68_PLASMA[@]}" fail=read
cfg=$(f68_config f68-unreadable)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Settings closed' \
    run_keys f68-unreadable $F68_TO_ROW Return BackSpace BackSpace
log=$out/f68-unreadable.log
ok=1
[ "$(f68_row_values f68-unreadable)" = 'Unknown' ] \
    && grep -qxF "Settings: the note under the preview says Couldn't read Plasma's shortcuts: the test file fails read" "$log" \
    && grep -qxF "Settings: page Settings${F68_ARROW}Controls" "$log" \
    && ! grep -qF "Settings: page Settings${F68_ARROW}Controls${F68_ARROW}Desktop media keys" "$log" \
    && f68_desktop_is "${F68_PLASMA[@]}" fail=read && ran_clean f68-unreadable && ok=0
result "media keys: a desktop that cannot be read shows the Controls row grayed, with why (exit $(cat "$out/f68-unreadable.code"))" $ok
echo "      the row read: $(f68_row_values f68-unreadable)"
f68_show f68-unreadable
grep -F 'Settings: the note under the preview says' "$log" | sed 's/^/      /'

# GNOME: the prompt at start, and Turn them off saves its four keys' gsettings text as it came, then
# clears them
f68_desktop "${F68_GNOME[@]}"
cfg=$(f68_config f68-gnome)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg WAIT_FOR=$F68_PROMPT UNTIL='Settings closed' run_keys f68-gnome Return
log=$out/f68-gnome.log
section=$(f68_section "$cfg")
ok=1
grep -qx 'Media keys: GNOME holds next track and previous track' "$log" \
    && grep -qxF "Settings: the note says GNOME $F68_NOTE Turn GNOME's $F68_NOTE_END" "$log" \
    && precedes "$log" "Media keys: saved GNOME's shortcuts to [Desktop Keys]" "Media keys: turned off GNOME's next- and previous-track shortcuts" \
    && grep -qx 'Desktop=gnome' <<< "$section" && grep -qxF "GnomeNext=['']" <<< "$section" \
    && grep -qxF "GnomePrevious=['']" <<< "$section" \
    && grep -qxF "GnomeNextStatic=['XF86AudioNext', '<Ctrl>XF86AudioNext']" <<< "$section" \
    && grep -qxF "GnomePreviousStatic=['XF86AudioPrev', '<Ctrl>XF86AudioPrev']" <<< "$section" \
    && f68_desktop_is gnome '@as []' '@as []' '@as []' '@as []' && ran_clean f68-gnome && ok=0
result "media keys: GNOME's prompt opens at start, and Turn them off saves its keys, then clears them (exit $(cat "$out/f68-gnome.code"))" $ok
f68_show f68-gnome
sed 's/^/      /' <<< "$section"

# The StartupCmd waits for the prompt (user, 2026-10-28: the keys are turned off before the app starts): it
# runs once, only after the prompt has closed, whether Not now or Turn them off closed it; with no prompt it
# runs at start as before. Here it adds a line to a marker file, as check 64's does
F68_MARKS=$TESTER_HOME/r68-marks
F68_STARTUP_CMD=":fork echo started >> $F68_MARKS/startup.txt"
F68_RUNS_NOW='StartupCmd: the Media keys prompt closed, so it runs now'

# A function to empty the marker's folder before a run
f68_clear_marks() { rm -rf "$F68_MARKS"; mkdir -p "$F68_MARKS"; chown tester:tester "$F68_MARKS"; }

# A function to count the marker's lines, waiting up to 5 s for the file when WAIT is set: :fork's command can
# write after the launcher has gone on
f68_marks() {
    local file=$F68_MARKS/startup.txt i
    if [ -n "${1:-}" ]; then
        for i in $(seq 25); do [ -s "$file" ] && break; sleep 0.2; done
        sleep 0.5
    fi
    [ -f "$file" ] && wc -l < "$file" || echo 0
}

# A function for a +key, called with the run's NAME: keep how many times the StartupCmd has run while the
# prompt is still on show (NAME.early), after a second for a :fork that was started to write
f68_early() { sleep 1; f68_marks > "$out/$1.early"; }

# A function to tell whether a run held its StartupCmd back until settings closed, and ran it once then
f68_held_back() {
    local log=$out/$1.log
    [ "$(cat "$out/$1.early")" = 0 ] && [ "$2" = 1 ] \
        && precedes "$log" "$F68_PROMPT" 'StartupCmd: waits for the Media keys prompt to close' \
        && precedes "$log" 'Settings closed' "$F68_RUNS_NOW" && [ "$(grep -cxF "$F68_RUNS_NOW" "$log")" = 1 ]
}

# Not now: the StartupCmd runs once settings close
f68_desktop "${F68_PLASMA[@]}"
f68_clear_marks
cfg=$(F68_STARTUP=$F68_STARTUP_CMD f68_config f68-startup)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg WAIT_FOR=$F68_PROMPT UNTIL="$F68_RUNS_NOW" \
    run_keys f68-startup +f68_early Down Return
marks=$(f68_marks wait)
ok=1
f68_held_back f68-startup "$marks" && in_range "$out/f68-startup.log" 'Settings opened' 'Settings closed' 'Settings: nothing changed' \
    && f68_desktop_is "${F68_PLASMA[@]}" && ran_clean f68-startup && ok=0
result "media keys: the StartupCmd waits for the prompt, and runs once after Not now (exit $(cat "$out/f68-startup.code"))" $ok
echo "      the StartupCmd ran $(cat "$out/f68-startup.early" 2> /dev/null) times with the prompt open, $marks in all"
grep -E '^(StartupCmd|Settings: page|Settings (opened|closed))' "$out/f68-startup.log" | sed 's/^/      /'

# Turn them off: the keys are off before the StartupCmd runs
f68_desktop "${F68_PLASMA[@]}"
f68_clear_marks
cfg=$(F68_STARTUP=$F68_STARTUP_CMD f68_config f68-startup-off)
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg WAIT_FOR=$F68_PROMPT UNTIL="$F68_RUNS_NOW" \
    run_keys f68-startup-off +f68_early Return
marks=$(f68_marks wait)
ok=1
f68_held_back f68-startup-off "$marks" \
    && precedes "$out/f68-startup-off.log" "Media keys: turned off Plasma's" 'Settings closed' \
    && f68_desktop_is plasma '' '' && ran_clean f68-startup-off && ok=0
result "media keys: the StartupCmd runs once after Turn them off, with the keys off (exit $(cat "$out/f68-startup-off.code"))" $ok
echo "      the StartupCmd ran $(cat "$out/f68-startup-off.early" 2> /dev/null) times with the prompt open, $marks in all"
grep -E '^(StartupCmd|Media keys|Settings (opened|closed))' "$out/f68-startup-off.log" | sed 's/^/      /'

# No prompt (Ask=no): the StartupCmd runs at start, as it always has
f68_desktop "${F68_PLASMA[@]}"
f68_clear_marks
cfg=$(F68_STARTUP=$F68_STARTUP_CMD f68_config f68-startup-askno '' '[Desktop Keys]' 'Ask=no')
STREAMFLEX_TEST_MEDIAKEYS=$F68_DESKTOP CFG=$cfg UNTIL='Begin program loop' run_keys f68-startup-askno
marks=$(f68_marks wait)
ok=1
[ "$marks" = 1 ] && ! grep -q '^StartupCmd: \(waits\|the Media keys\)' "$out/f68-startup-askno.log" \
    && ! grep -q 'Settings opened' "$out/f68-startup-askno.log" \
    && ran_clean f68-startup-askno && ok=0
result "media keys: with no prompt the StartupCmd runs at start, as before (exit $(cat "$out/f68-startup-askno.code"))" $ok
echo "      the StartupCmd ran $marks times"
rm -rf "$F68_MARKS"
rm -f "$F68_DESKTOP" "$out/f68-want"
