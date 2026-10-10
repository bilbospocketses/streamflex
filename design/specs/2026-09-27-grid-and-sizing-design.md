# Grid layout and button sizing — design

**Date:** 2026-09-27
**Status:** approved in brainstorming, awaiting spec review
**Covers:** todo items 4 (multiple rows of buttons) and 5 (button resizing)

## Context

StreamFlex is a 10-foot launcher: it runs full screen on a TV and is driven by a remote or a gamepad alone. There is no mouse and no keyboard to rely on. Every behavior below must work with the D-pad, OK and Back.

This is sub-project 1 of the overhaul, which is split into four parts, each with its own spec, plan and build:

1. **Grid and sizing** (this document): items 4 and 5.
2. **Built-in icon library**: item 6.
3. **In-app settings screen**: item 7's settings screen, including the config writer that preserves the user's file. It will also edit the grid shape live, from the remote.
4. **Fullscreen polish**: item 7's per-platform checks (no startup flash, no taskbar bleed, multi-monitor, Transparent mode without tinkering). It can land at any point.

Order: 1, 2, 3, with 4 wherever it fits.

## What exists today

- **Layout.** Every menu is one row. `calculate_button_geometry()` places every icon at the same `geo.y_margin`. The global `IconSize` is one button size for the whole launcher, and `MaxButtons` is the number of buttons per screen.
- **Navigation.** It is left/right only. Moving past the last visible button replaces the whole row with the next page. The state lives in `Menu.page`, `Menu.highlight_position` and `Menu.root_entry`. Keyboard: Left, Right, Enter and Backspace are built in, and they win over hotkeys. Gamepad: controls come only from the config file; the sample maps the D-pad and left stick X to `:left` and `:right`, and leaves Up and Down unmapped.
- **Config.** `config.ini` is read by inih and never written. Inside a menu section, every key's value is parsed as an entry (`Title;icon;command`), and the key name is ignored.
- **Textures.**
  - Icons load at native size, and the GPU scales them with linear filtering.
  - The highlight is rendered once, at `IconSize`.
  - Titles are truncated to `IconSize` width.
  - Scroll indicators are one arrow texture, 11% of the screen height, drawn in the bottom corners.
- **Clock.** It sits in a top corner, inset by `ClockMargin`, which is at most 10% of the screen height.
- **Tests.** There are none. The repo has no CTest and no test directory.

## Decisions

| Question | Decision |
|---|---|
| What "resizing" means | The size follows the grid: the user picks rows and columns, and the buttons scale to fill. The grid can differ per menu, and it can be changed live later (sub-project 3). There are no featured or mixed-size tiles: every button in a menu is the same size. |
| Too many buttons for one screen | A grid scrolls **one row** at a time (Google TV style). |
| Left/Right at a row edge, in a grid | The highlight **stops** there. Up and Down move between rows. |
| A menu with one row | It is a **strip** that slides **one button** at a time. This keeps every existing single-row config fully reachable with Left/Right alone. |
| Existing `MaxButtons` and `IconSize` | `MaxButtons` becomes the column count and stays accepted as an alias of `Columns`. `IconSize` becomes an optional **cap** on the button size. |
| Structure | Layout and navigation move into a new pure module, `src/layout.c`, with unit tests. |

## Configuration

```ini
[Layout]
Rows=1          ; new: rows visible at once (default 1)
Columns=4       ; new name for MaxButtons (default 4); MaxButtons still accepted
IconSize=256    ; the LARGEST a button may grow; leave it out to let buttons fill
IconSpacing=5%  ; gap between buttons, across and down (percent of screen width)
VCenter=50%     ; vertical center of the whole grid block

[Games]
Rows=3          ; per-menu overrides: Rows, Columns, IconSize
Columns=6
Entry1=...
```

- Every new key is optional. A config that uses none of them lays out as today, apart from the two changes listed under **Existing configs**.
- **Precedence.** `MaxButtons` is an alias only in `[Layout]`. When `[Layout]` has both, `Columns` wins.
- **Reserved names in menu sections.** Inside a menu section, `Rows`, `Columns` and `IconSize` are settings **only when their value is a number**. A value shaped like an entry (containing `;`) is still parsed as an entry, with a warning in the log. No existing button can be lost to the new rule.

## Behavior

### Sizing

- The button size is the largest square for which `Columns` buttons fit across and `Rows` buttons fit down the usable area. Each button's height includes its title block (title padding plus font height, when titles are on), and the `IconSpacing` gap applies between columns and between rows.
- The result is capped by the menu's `IconSize`, which falls back to `[Layout]`'s. With no `IconSize` anywhere, it is capped only by `MAX_ICON_SIZE`, 1024.
- The size comes from the grid's **shape**, not its entry count. A `3×6` menu with four entries does not balloon.
- **Usable area.**
  - **Width:** the full screen width, as today's single row has. Left and right margins would shrink today's default 4 × 256 px row at 1280×720 (to 239 px).
  - **Height:** the screen height minus the 5% `SCREEN_MARGIN` at top and bottom, where a grid's scroll arrows live. When the clock is enabled, the top edge also moves down to below the clock's bottom, so a grid never runs under the time or date.

### Placement

- The grid block is centered horizontally using its **configured** column count, so columns stay put as it scrolls, and a partial last row lines up left under them.
- **Exception:** a menu whose buttons fill less than one row is centered, exactly like today's single row.
- The **occupied** rows are centered vertically on `VCenter`. `VCenter` keeps its existing limits of 25% to 75%. The block is then clamped so that it stays inside the usable area: a tall grid with `VCenter=25%` moves down rather than spilling into the margin or under the clock.

### Strip (`Rows=1`)

- Left and Right move along the strip.
- At the visible edge the strip slides by one button.
- With `WrapEntries`, Right on the last button selects the first, and Left on the first selects the last. The strip scrolls to show the selection.
- `:up` and `:down` do nothing.

### Grid (`Rows` of 2 or more)

- Buttons fill left to right, then top to bottom.
- Left and Right move within the row and stop at its edges. With `WrapEntries`, they wrap **within the same row** instead: Right on the row's last button selects its first, and Left mirrors it. They never cross rows.
- Up and Down move one row, keeping the column. On the top or bottom visible row, the grid scrolls one row when more rows exist in that direction.
- Down into a shorter last row lands on that row's last button.
- With `WrapEntries`:
  - Down on the last row selects the same column on the first row.
  - Up on the first row selects the same column on the last row, or that row's last button if it is shorter.
  - The grid scrolls to show the selection.

### Memory and live re-layout

- Each menu remembers its selected button and scroll position. `:back` restores them, and `ResetOnBack` still resets them.
- Every menu load resolves the menu's effective settings and recomputes its layout.
- If a menu's shape or size changes (sub-project 3), the selected button stays selected, and the scroll position is adjusted so that it stays on screen.

## Architecture

### `src/layout.c` / `src/layout.h` (new, pure)

This module holds plain integers only: no SDL types, no rendering and no globals. The tests can therefore build without SDL. The API below is a design intent; the implementation plan may adjust names.

```c
typedef enum { LAYOUT_UP, LAYOUT_DOWN, LAYOUT_LEFT, LAYOUT_RIGHT } LayoutDirection;

typedef struct {
    int rows, columns;      // effective shape
    int icon_cap;           // IconSize cap; 0 = no cap (MAX_ICON_SIZE)
    int spacing;            // px, used across and down
    int title_block;        // title padding + font height, or 0 with titles off
    int hpad, vpad;         // highlight padding (may be reduced to fit)
} LayoutParams;

typedef struct {
    int x, y, w, h;         // usable area (margins and clock band excluded)
    int vcenter;            // px, clamped to the VCenter limits
} LayoutArea;

typedef struct {
    int rows, columns;      // after any reduction to fit
    int button;             // square button size
    int x_advance, y_advance;
    int x_origin, y_origin; // top-left of the first visible slot
    int hpad, vpad;         // after any reduction
} LayoutGeometry;

typedef struct {
    int selected;           // entry index
    int first;              // strip: first visible entry; grid: first visible row
} LayoutPosition;

int  layout_compute(const LayoutParams *p, const LayoutArea *a, int entry_count,
                    LayoutGeometry *out, char *why, size_t why_len);
LayoutPosition layout_move(const LayoutGeometry *g, int entry_count, LayoutPosition pos,
                           LayoutDirection dir, bool wrap);
LayoutPosition layout_clamp(const LayoutGeometry *g, int entry_count, LayoutPosition pos);
bool layout_slot(const LayoutGeometry *g, LayoutPosition pos, int index, int *x, int *y);
bool layout_can_scroll(const LayoutGeometry *g, int entry_count, LayoutPosition pos,
                       LayoutDirection dir);

typedef struct { int rows, columns, icon_cap; } LayoutOverrides;   // 0 = not set
LayoutOverrides layout_resolve(LayoutOverrides menu, LayoutOverrides global,
                               LayoutOverrides builtin);
```

- **`layout_compute`** returns 0 on success. If the requested shape cannot fit even at `MIN_ICON_SIZE` (32), it reduces only the axis that overflows (columns when too wide, rows when too tall). It writes what it did to `why` for the log, and still returns 0. It returns non-zero only when not even one button fits.
- **`layout_compute` also fits the highlight.** Highlight padding is capped at half the gap between buttons:
  - horizontally always, which is today's rule, moved here from `validate_settings()`;
  - vertically only for grids of two or more rows, since a single row has no row gap and today's strips must not change.

  A negative (unset) padding counts as 0.
- **Edge rules.** Every rule in **Behavior** lives in `layout_move` and `layout_clamp`, and nowhere else.

### Changes elsewhere

- **`Menu`** (`launcher.h`):
  - gains `Entry **items`, built once after parsing (the linked list stays for parsing);
  - gains the overrides `{rows, columns, icon_cap}`, where 0 means inherit;
  - gains a `LayoutPosition`;
  - gains the button size its textures were last rendered at.

  `page`, `highlight_position` and `root_entry` are removed. `last_selected_entry` becomes part of the `LayoutPosition`.
- **`Geometry`** keeps its screen-wide fields and gains `vcenter` (the `VCenter` setting in px). The per-row fields (`x_margin`, `y_margin`, `x_advance`, `num_buttons`) move into the current menu's `LayoutGeometry`.
- **Parser** (`util.c`):
  - `[Layout]` gains `Rows` and `Columns` (with `MaxButtons` as an alias);
  - menu sections apply the reserved-name rule above;
  - `validate_settings()` keeps the global checks (fonts, colors, limits), and the width-fitting logic moves into `layout_compute`.
- **`launcher.c`:**
  - **`load_menu`** resolves the effective settings and calls `layout_compute`. If the button size changed, it re-renders the menu's textures:
    - SVG icons are rasterized at the button size with `rasterize_svg_from_file`, which `image.h` has declared but never defined until now;
    - titles are rendered with `max_width` set to the button size;
    - the highlight texture is re-rendered.

    It then places the visible entries.
  - **`move_left`, `move_right`, `move_up`, `move_down`** call `layout_move`, then re-place the visible entries and the highlight.
  - **`draw_screen`** draws only the visible entries.
  - **Removed:** `calculate_button_geometry` and the page arithmetic, plus `advance_entries` if nothing else uses it.

### Scroll indicators

- A **strip** keeps today's left and right arrows in the bottom corners, unchanged.
- A **grid** draws the same arrow texture rotated to point up or down (`SDL_RenderCopyEx`):
  - sized to the height of `SCREEN_MARGIN`;
  - centered horizontally in the top and bottom margins;
  - shown only when `layout_can_scroll` says more rows exist that way.

  It takes no space from the buttons, and it cannot collide with the clock, which sits in a corner.

## Input

- **New special commands `:up` and `:down`,** dispatched in `execute_command` next to `:left` and `:right`. They are usable in hotkeys and gamepad mappings.
- **Keyboard.** The Up and Down arrow keys become built-in keys. Most HTPC remotes send arrow keys. **Exception:** if `[Hotkeys]` binds the Up or Down arrow, that hotkey keeps priority for that key, so an existing binding is never taken over.
- **Gamepad:**
  - The sample config maps D-pad Up/Down and left stick Y to `:up`/`:down`.
  - If a config maps nothing to `:up` or `:down`, D-pad Up/Down and left stick Y default to them, unless the config assigns those controls to something else.
  - Hold-to-repeat uses the existing timings (`GAMEPAD_REPEAT_DELAY` 500 ms, then `GAMEPAD_REPEAT_INTERVAL` 25 ms).
- **Unchanged:** `:select`, `:back`, `:home`, the existing hotkeys, and `MouseSelect`.

## Errors and limits

- A `Rows` or `Columns` that is not a positive whole number is ignored and logged. For a per-menu override, the log names the menu. The setting falls back to `[Layout]`, then to the built-in default.
- `IconSize` keeps its existing range, `MIN_ICON_SIZE` (32) to `MAX_ICON_SIZE` (1024).
- A shape too large to fit is reduced on the overflowing axis only, and the reduction is logged (see `layout_compute`).
- A menu with no valid entries is handled as today.
- **Debug output.** `debug_settings()` prints `Rows` and `Columns`. `debug_menu_entries()` prints each menu's overrides, its effective shape and its computed button size.

## Existing configs

- **Same look.** `Rows` defaults to 1, `MaxButtons` is read as `Columns`, and an existing `IconSize` becomes the cap. Menus render at today's size and position.
- **Changed:**
  1. A single row with more buttons than fit now slides **one button** at a time instead of flipping a page.
  2. A config with **no `IconSize` line** now gets buttons that grow to fill the screen, where it used to get the built-in 256.

  Both go in the CHANGELOG under "Changed".

## Testing

- **Unit tests (new).** `tests/test_layout.c` is plain C with no SDL, built as its own executable and run through CTest (`enable_testing()`, `add_test`). CI runs it on at least the Debian and Windows jobs. It covers:
  - **sizing:** the cap, a width-limited fit, a height-limited fit, the clock band, and reduction on the overflowing axis only;
  - **highlight padding:** reduced when the gaps are too small;
  - **placement:** a partial single row centered, and a partial last row aligned left;
  - **strip:** one-button scroll, with wrap in both directions;
  - **grid:** Left/Right stopping at row edges, in-row wrap with `WrapEntries`, Up/Down scrolling at the top and bottom, Down into a shorter last row, and vertical wrap;
  - **placement clamp:** a tall grid with `VCenter` at 25% and at 75% stays inside the usable area;
  - **re-layout:** `layout_clamp` after a shape change;
  - **precedence:** resolving the effective settings is a pure function in `layout.c` (menu override, then `[Layout]`, then the built-in default). The test sets all three levels at once, so it proves which one wins, not merely that each works alone;
  - **indicators:** `layout_slot` visibility and `layout_can_scroll`.
- **Manual checks** on the local Windows build (`build\`, VS 2022):
  - the default one-row config, which should look the same except for the one-button scroll;
  - a `2×4` main menu and a `3×6` submenu override;
  - driving it with the keyboard and with a gamepad;
  - a config that maps only Left/Right, to prove the default Up/Down mappings;
  - `[Layout]` with both `Columns` and `MaxButtons`, in both orders, to prove `Columns` wins regardless of which comes first. inih delivers keys in file order, so the parser must track that `Columns` was set rather than letting the last key win.

## Documentation

- **`docs/configuration.md`:**
  - Layout (`Rows`, `Columns`, `IconSize` as a cap, `MaxButtons` as an alias);
  - per-menu overrides and the reserved-name rule;
  - strip and grid navigation;
  - the `:up` and `:down` special commands;
  - Gamepad: the default Up/Down mappings.
- **`config/config.ini.in`:** `Rows`, `Columns` in place of `MaxButtons`, and the D-pad and left stick Y mappings.
- **`CHANGELOG.md`:** the new features, plus the two changes under **Existing configs**.
- **`CONTRIBUTING.md`:** the project structure gains `tests/`. (`design/` was added along with this spec.)

## Out of scope

- Mixed-size or featured tiles.
- Saving layout changes from inside the app (sub-project 3).
- Better downscaling for raster (PNG) icons (sub-project 2, where icon quality is the focus). This sub-project only rasterizes SVG icons at the button size.
- Per-menu title font sizes. The title size stays global, for readability at 10 feet.
- Mouse-driven navigation.
