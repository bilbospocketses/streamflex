# What SDL3's calls return, and the branches that follow from it (spec section 1; plan Tasks 1.2a
# and 1.3).
#
# SDL_GetBasePath() can return NULL, as SDL2's could: the launcher's folder is then unknown, and the
# bundled font is looked for only where the packages install it, never through a NULL path (Task
# 1.2a; the user's ruling, 2026-10-11). The titles' font is named by its full path, so the one bundled
# font looked for is the clock's, which opens after the window (a fatal stop before it would meet the
# Fedora leak CONTRIBUTING.md describes). This image installs no fonts there, so the launcher stops as
# with no bundled font, exit 1, and no sanitizer reports anything. The control, the same config with
# SDL's base path, finds the font beside the launcher and quits by itself.
STREAMFLEX_TEST_NO_BASE_PATH=1 STREAMFLEX_TEST_NO_MESSAGE_BOX=1 run_quick f05-basepath
CFG=$FX/f05-basepath.ini run_quick f05-basepath-control
ok=1
grep -qx 'Could not load default font' "$out/f05-basepath.log" && ran_clean f05-basepath 1 \
    && grep -q '^Quitting program' "$out/f05-basepath-control.log" \
    && ! grep -q 'Could not load default font' "$out/f05-basepath-control.log" && ran_clean f05-basepath-control && ok=0
result "SDL with no base path: the bundled font is not looked for beside the launcher, and nothing crashes (exit $(cat "$out/f05-basepath.code"), $(cat "$out/f05-basepath-control.code"))" $ok
grep -h -E '^(Could not|Quitting)' "$out/f05-basepath.log" "$out/f05-basepath-control.log" | sed 's/^/      /'
