# Shadowed titles: a title's shadow is drawn 1/40 of its line height below it, at least 2 px (2 px
# for a 36 pt title), and the layout keeps that room under every title. A height-limited grid of
# 4 rows so gives each of its buttons 2 px, the same config without shadows having none to give.

# A function to read the button size from a run's "1 x 4 grid" line
grid_px() { grep -m1 -oE "Menu 'Main': 1 x 4 grid, [0-9]+ px buttons" "$out/$1.log" | grep -oE '[0-9]+ px' | grep -oE '[0-9]+'; }

run_quick f45-noshadow
run_quick f45-shadow
plain=$(grid_px f45-noshadow)
shadowed=$(grid_px f45-shadow)
ok=1
ran_clean f45-noshadow && ran_clean f45-shadow && [ -n "$plain" ] && [ "$shadowed" = "$((plain - 2))" ] && ok=0
result "shadowed titles keep room for their shadows: 4 rows of ${plain:-?} px buttons become ${shadowed:-?} px (exit $(cat "$out/f45-shadow.code"))" $ok

# Titles too long for their buttons, shadowed: each is cut to its button less the shadow's reach,
# so title and shadow together fit the button. The debug log names any title (measured with its
# shadow) wider than its button.
run_quick f45-shadowlong
ok=1
ran_clean f45-shadowlong && grep -qE "Menu 'Main': 4 x 1 grid, [0-9]+ px buttons, 36 pt titles" "$out/f45-shadowlong.log" \
    && ! grep -q 'px wide, over its' "$out/f45-shadowlong.log" && ok=0
result "a shadowed title cut to fit keeps its shadow inside its button (exit $(cat "$out/f45-shadowlong.code"))" $ok
grep 'px wide, over its' "$out/f45-shadowlong.log" | sed 's/^/      /'

# A title of only a zero-width space (U+200B), as a config that hides one entry's title might hold,
# shadowed and not: SDL_ttf measures it zero wide and renders no surface for it, and its button is
# drawn with no title, the menu's other titles as ever, where StreamFlex read the surface it never got
f45_blank() {
    ok=1
    ran_clean "$1" && grep -q '^Text .* is zero wide: it draws nothing$' "$out/$1.log" \
        && grep -qx "Menu 'Main': rendered its buttons at [0-9]* px" "$out/$1.log" && ok=0
    result "a title of only a zero-width space draws nothing, $2 (exit $(cat "$out/$1.code"))" $ok
    grep -E '^(Text |Menu .Main.: rendered)' "$out/$1.log" | sed 's/^/      /'
}
run_quick f45-blank
f45_blank f45-blank shadowed
run_quick f45-blank-plain
f45_blank f45-blank-plain 'with no shadow'
