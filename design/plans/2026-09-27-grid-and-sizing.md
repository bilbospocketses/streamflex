# Grid layout and button sizing — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Menus can show several rows of buttons, sized to fill a per-menu grid, and navigable with a remote or gamepad alone.

**Architecture:**
- Layout and navigation move out of `launcher.c` into a new pure module, `src/layout.c`. It holds plain integers only: no SDL and no globals.
- The module gets the repo's first unit tests, run by CTest.
- `launcher.c` keeps the drawing. It asks the layout module for a menu's button size and positions, and it asks it how each key press moves the highlight.
- Per-menu grid settings are parsed into each `Menu`. The icons, titles and highlight are re-rendered whenever a menu's button size changes.

**Tech Stack:** C (C99, as the codebase is), SDL2 (floor 2.0.14), SDL2_image (2.0.5), SDL2_ttf (2.0.15), inih, CMake 3.18+ and CTest, GitHub Actions.

**Spec:** `design/specs/2026-09-27-grid-and-sizing-design.md` (read it first; this plan implements it).

## Global Constraints

- Every repo command uses an absolute path: `git -C C:/Users/jscha/source/repos/streamflex ...`, and files under `C:/Users/jscha/source/repos/streamflex/`. Never `cd` into the repo.
- **Branch:** work on `feat/grid-layout`, cut only after PR #23 (this spec and plan) has merged:

  ```
  pwsh C:/Users/jscha/.claude/scripts/git-new-branch.ps1 -Repo C:/Users/jscha/source/repos/streamflex -Branch feat/grid-layout
  ```

  Stage named files only. Never `git add -A`. Commit messages are conventional (`feat:`, `test:`, `docs:`) with no AI attribution. The PR is squash-merged.
- **`src/layout.c` and `src/layout.h` must not include SDL, `launcher.h` or any SDL-using header.** The test executable links `layout.c` alone.
- **Use no SDL API newer than 2.0.14,** no SDL_image API newer than 2.0.5, and no SDL_ttf API newer than 2.0.15. (`SDL_LoadFile` is 2.0.10, and `SDL_RenderCopyEx` and `SDL_strcasecmp` are 2.0.0.)
- **Keep the codebase's style.** Four-space indents, braces on the same line, and one comment line above each function in the existing voice ("// A function to ..."). Explicit casts where `int` and `unsigned int` mix, so that `-DEXTRA_WARNINGS=ON` gains no new warnings.
- **Existing configs must render at today's size and position.** The only exceptions are the two changes the spec lists under "Existing configs":
  1. a single row slides one button at a time;
  2. no `IconSize` line means the buttons fill the grid.
- **Every local build command on Windows starts with the CMake PATH line.** Shell state does not persist between tool calls:

  ```powershell
  $env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
  ```

  The build directory `C:/Users/jscha/source/repos/streamflex/build` is already configured (VS 2022 generator, vcpkg at CI's pin), so `cmake --build` re-runs the configure step by itself when a `CMakeLists.txt` changes.
- **Exact values from the spec:**
  - `MIN_ICON_SIZE` 32, `MAX_ICON_SIZE` 1024;
  - `SCREEN_MARGIN` 5% of the screen height;
  - `VCenter` limits 25% to 75%;
  - `DEFAULT_MAX_BUTTONS` (the columns default) 4, `DEFAULT_ROWS` 1;
  - `IconSpacing` as a percentage is of the screen **width**;
  - highlight padding is capped at half the gap: horizontally always, vertically only when a menu has 2 or more rows.

## Review Focus

These are the input classes that the unit tests do not reach, because they live in SDL-linked code. Each one is pinned by a fixture-config smoke check in the task that owns the code:

1. **A menu entry whose key happens to be `Rows`, `Columns` or `IconSize`** (for example `Columns=Plex;plex.png;cmd`) must still appear as a button, with a warning in the log. It must not be swallowed as a setting. Pinned in Task 4 (fixture A).
2. **`[Layout]` with both `Columns` and `MaxButtons`, in either order.** `Columns` must win whichever comes first, because inih delivers keys in file order. Pinned in Task 4 (fixtures A and B).
3. **A config written before grids existed** (gamepad enabled, with only Left/Right mapped) must get D-pad and left-stick Up/Down. A control the config already uses for something else must keep its own command. Pinned in Task 6 (fixtures C, D and E).
4. **Moving between menus with different button sizes** (a 1-row main menu and a 3×6 submenu, then `:back`). Each menu must be re-rendered at its own size, with the highlight matching. The parent keeps its selection. Pinned in Task 5 (fixture G), and by hand in Task 8.
5. **SVG icons, and a missing icon file.**
   - An `.svg` icon (and its `_selected` override) must be rasterized at the button size.
   - A missing icon file must log an error and draw nothing, never crash.

   Pinned in Task 5 (fixture F).

## File structure

| File | Status | Responsibility |
|---|---|---|
| `src/layout.h` | new | Layout types and the pure API: resolve settings, parse counts, compute geometry, move, clamp, slot, can-scroll |
| `src/layout.c` | new | The implementation. No SDL |
| `tests/check.h` | new | Minimal `CHECK` / `CHECK_INT` helpers and a report function |
| `tests/test_layout.c` | new | Unit tests for `layout.c` |
| `tests/CMakeLists.txt` | new | Builds `test_layout` from `layout.c` alone, and registers it with CTest |
| `CMakeLists.txt` | modify | `enable_testing()` plus `add_subdirectory("tests")` |
| `src/CMakeLists.txt` | modify | Adds `layout.c` and `layout.h` to the launcher sources |
| `.github/workflows/build.yml` | modify | Runs `ctest` on Windows, Debian and Raspberry Pi |
| `config/config_settings.cmake` | modify | `SETTING_ROWS`, `SETTING_COLUMNS`, `DEFAULT_ROWS` |
| `config/launcher_config.h.in` | modify | The C defines for the three new settings |
| `config/config.ini.in` | modify | Sample config: `Rows`, `Columns`, and the D-pad/stick Up/Down mappings |
| `src/launcher.h` | modify | Includes `layout.h`. `Menu` gets items/overrides/position, `Geometry.vcenter`, highlight and scroll fields, `:up`/`:down` |
| `src/launcher.c` | modify | Uses the layout module for sizing, drawing and moving. Up/Down input |
| `src/util.c`, `src/util.h` | modify | Parsing of `Rows`/`Columns`/per-menu keys, `build_menu_items`, default gamepad Up/Down, `validate_settings` clean-up |
| `src/image.c`, `src/image.h` | modify | `rasterize_svg_from_file` (declared, never defined), `load_icon`, grid scroll-indicator rects |
| `src/debug.c` | modify | Prints `Rows`, `Columns`, `IconSize` as a cap, and per-menu overrides |
| `docs/configuration.md` | modify | Layout, Menu Layouts, Moving Around, `:up`/`:down`, Hotkeys, Gamepad |
| `CHANGELOG.md`, `CONTRIBUTING.md` | modify | Release notes, the `tests/` folder, and how to run the tests |

---

### Task 1: Test harness, layout types, and settings resolution

**Files:**
- Create: `src/layout.h`, `src/layout.c`, `tests/check.h`, `tests/test_layout.c`, `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt` (after `add_subdirectory("src")`), `src/CMakeLists.txt` (the `SOURCES` list), `.github/workflows/build.yml` (three build jobs)

**Interfaces:**
- Produces: every type in `layout.h` (`LayoutDirection`, `LayoutOverrides`, `LayoutParams`, `LayoutArea`, `LayoutGeometry`, `LayoutPosition`) and all seven function declarations. It implements two of them:
  - `LayoutOverrides layout_resolve(LayoutOverrides menu, LayoutOverrides global, LayoutOverrides builtin)`
  - `bool layout_parse_count(const char *value, int *count)`
- Also produces the test harness macros `CHECK(cond)` and `CHECK_INT(actual, expected)`, and `int check_report(void)`.

- [ ] **Step 1: Create `src/layout.h`**

```c
// Grid geometry and navigation for menus. Pure: plain integers, no SDL, no globals, so
// tests/test_layout.c can build it on its own. launcher.c does the drawing.
#ifndef LAYOUT_H
#define LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#define LAYOUT_MIN_BUTTON 32   // Same as MIN_ICON_SIZE in launcher.h
#define LAYOUT_MAX_BUTTON 1024 // Same as MAX_ICON_SIZE in launcher.h

typedef enum {
    LAYOUT_UP,
    LAYOUT_DOWN,
    LAYOUT_LEFT,
    LAYOUT_RIGHT
} LayoutDirection;

// Rows, Columns and IconSize from a menu section or from [Layout]; 0 means "not set"
typedef struct {
    int rows;
    int columns;
    int icon_cap;
} LayoutOverrides;

// Everything layout_compute needs to size a menu's buttons
typedef struct {
    int rows;
    int columns;
    int icon_cap;    // Largest allowed button size; 0 = no cap
    int spacing;     // Gap between buttons in px, across and down
    int title_block; // Title padding + font height below each icon; 0 without titles
    int hpad;        // Requested highlight padding; negative counts as 0
    int vpad;
} LayoutParams;

// The part of the screen the buttons may use
typedef struct {
    int x;
    int y;
    int w;
    int h;
    int vcenter;     // Vertical center of the button block, in px from the top of the screen
} LayoutArea;

// The computed layout of one menu
typedef struct {
    int rows;        // After any reduction to fit the screen
    int columns;
    int button;      // Square button size in px
    int x_advance;   // Distance between neighboring buttons' x
    int y_advance;   // Distance between neighboring rows' y
    int x_origin;    // Top-left of the first visible slot
    int y_origin;
    int hpad;        // Highlight padding after capping
    int vpad;
} LayoutGeometry;

// Where the highlight is, and what is scrolled into view
typedef struct {
    int selected;    // Entry index
    int first;       // Strip: first visible entry. Grid: first visible row
} LayoutPosition;

LayoutOverrides layout_resolve(LayoutOverrides menu, LayoutOverrides global, LayoutOverrides builtin);
bool layout_parse_count(const char *value, int *count);
int layout_compute(const LayoutParams *params, const LayoutArea *area, int entry_count,
                   LayoutGeometry *geometry, char *why, size_t why_size);
LayoutPosition layout_move(const LayoutGeometry *geometry, int entry_count, LayoutPosition position,
                           LayoutDirection direction, bool wrap);
LayoutPosition layout_clamp(const LayoutGeometry *geometry, int entry_count, LayoutPosition position);
bool layout_slot(const LayoutGeometry *geometry, LayoutPosition position, int index, int *x, int *y);
bool layout_can_scroll(const LayoutGeometry *geometry, int entry_count, LayoutPosition position,
                       LayoutDirection direction);

#endif
```

- [ ] **Step 2: Create `tests/check.h`**

```c
// Minimal test helpers for the C unit tests: no framework, one executable per suite.
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int check_count = 0;
static int check_failures = 0;

#define CHECK(condition) do { \
    check_count++; \
    if (!(condition)) { \
        check_failures++; \
        fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

#define CHECK_INT(actual, expected) do { \
    int actual_ = (actual); \
    int expected_ = (expected); \
    check_count++; \
    if (actual_ != expected_) { \
        check_failures++; \
        fprintf(stderr, "%s:%d: %s is %d, expected %d\n", __FILE__, __LINE__, #actual, actual_, expected_); \
    } \
} while (0)

// A function to print the totals and give main() its exit code
static int check_report(void)
{
    printf("%d checks, %d failed\n", check_count, check_failures);
    return check_failures ? 1 : 0;
}

#endif
```

- [ ] **Step 3: Write the failing tests in `tests/test_layout.c`**

```c
#include <stdbool.h>
#include "check.h"
#include "layout.h"

// A function to test that settings come from the menu, then [Layout], then the default,
// with all three levels set at once so the test proves which one wins
static void test_resolve(void)
{
    LayoutOverrides menu = { 3, 6, 200 };
    LayoutOverrides global = { 2, 5, 256 };
    LayoutOverrides builtin = { 1, 4, 0 };
    LayoutOverrides none = { 0, 0, 0 };
    LayoutOverrides r;

    r = layout_resolve(menu, global, builtin);
    CHECK_INT(r.rows, 3);
    CHECK_INT(r.columns, 6);
    CHECK_INT(r.icon_cap, 200);

    r = layout_resolve(none, global, builtin);
    CHECK_INT(r.rows, 2);
    CHECK_INT(r.columns, 5);
    CHECK_INT(r.icon_cap, 256);

    r = layout_resolve(none, none, builtin);
    CHECK_INT(r.rows, 1);
    CHECK_INT(r.columns, 4);
    CHECK_INT(r.icon_cap, 0);

    // Each field resolves on its own
    LayoutOverrides menu_rows = { 3, 0, 0 };
    LayoutOverrides global_columns = { 0, 5, 0 };
    r = layout_resolve(menu_rows, global_columns, builtin);
    CHECK_INT(r.rows, 3);
    CHECK_INT(r.columns, 5);
    CHECK_INT(r.icon_cap, 0);
}

// A function to test that only a positive whole number counts as a Rows/Columns/IconSize value
static void test_parse_count(void)
{
    int n = -7;
    CHECK(layout_parse_count("3", &n));
    CHECK_INT(n, 3);
    CHECK(layout_parse_count("12", &n));
    CHECK_INT(n, 12);
    CHECK(!layout_parse_count("0", &n));
    CHECK(!layout_parse_count("-1", &n));
    CHECK(!layout_parse_count("3x", &n));
    CHECK(!layout_parse_count("", &n));
    CHECK(!layout_parse_count(" 3", &n));
    CHECK(!layout_parse_count("1234567", &n));
    CHECK(!layout_parse_count("Plex;plex.png;cmd", &n));
    CHECK(!layout_parse_count(NULL, &n));
    CHECK_INT(n, 12); // A rejected value leaves the output alone
}

int main(void)
{
    test_resolve();
    test_parse_count();
    return check_report();
}
```

- [ ] **Step 4: Create `tests/CMakeLists.txt`**

```cmake
# Unit tests for the pure layout module. layout.c must stay free of SDL, so this links it alone.
add_executable(test_layout test_layout.c "${PROJECT_SOURCE_DIR}/src/layout.c")
target_include_directories(test_layout PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME layout COMMAND test_layout)
```

- [ ] **Step 5: Register the tests in the top-level `CMakeLists.txt`**

Replace:

```cmake
#Build source files
add_subdirectory("src")
```

with:

```cmake
#Build source files
add_subdirectory("src")

# Unit tests, run with ctest
enable_testing()
add_subdirectory("tests")
```

- [ ] **Step 6: Build it and watch it fail to link**

`layout.c` does not exist yet.

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_layout
```

Expected: FAIL. The configure step or the build reports that `src/layout.c` cannot be found.

- [ ] **Step 7: Create `src/layout.c` with the two functions**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "layout.h"

// A function to pick each setting from the menu, else from [Layout], else the built-in default
LayoutOverrides layout_resolve(LayoutOverrides menu, LayoutOverrides global, LayoutOverrides builtin)
{
    LayoutOverrides result;
    result.rows = menu.rows ? menu.rows : (global.rows ? global.rows : builtin.rows);
    result.columns = menu.columns ? menu.columns : (global.columns ? global.columns : builtin.columns);
    result.icon_cap = menu.icon_cap ? menu.icon_cap : (global.icon_cap ? global.icon_cap : builtin.icon_cap);
    return result;
}

// A function to read a positive whole number, rejecting anything else ("3x", "-1", "0", "")
bool layout_parse_count(const char *value, int *count)
{
    if (value == NULL || value[0] == '\0' || strlen(value) > 6)
        return false;
    for (const char *p = value; *p != '\0'; p++) {
        if (!isdigit((unsigned char) *p))
            return false;
    }
    int number = atoi(value);
    if (number <= 0)
        return false;
    *count = number;
    return true;
}
```

- [ ] **Step 8: Add the layout module to the launcher's sources**

In `src/CMakeLists.txt`, replace:

```cmake
  clock.c
  clock.h
)
```

with:

```cmake
  clock.c
  clock.h
  layout.c
  layout.h
)
```

- [ ] **Step 9: Build and run the tests**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_layout
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`. The test's own line reads `25 checks, 0 failed`.

- [ ] **Step 10: Prove the tests can fail**

Temporarily change `layout_resolve`'s rows line to prefer `global.rows` over `menu.rows`. Rebuild and run `ctest`. Expected: FAIL, with `r.rows is 2, expected 3`. Then restore the line, rebuild and run again. Expected: PASS.

- [ ] **Step 11: Run the tests in CI**

Edit `.github/workflows/build.yml`. In the **Windows** job, after the `Build` step, add:

```yaml
      - name: Unit tests
        run: ctest --test-dir build -C ${{ env.CMAKE_BUILD_TYPE }} --output-on-failure
```

In the **Linux (Debian)** job's `Build` step, add a line after `cmake --build build --target package ...`, inside the `if`:

```yaml
            ctest --test-dir build --output-on-failure
```

In the **Raspberry Pi** job's `Build` step, add a last line:

```yaml
          ctest --test-dir build --output-on-failure
```

(The Arch job builds through `makepkg` from the generated PKGBUILD and runs no tests. The other three jobs cover the same code.)

- [ ] **Step 12: Build the whole launcher, to prove the new sources compile inside it**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
```

Expected: the build succeeds.

- [ ] **Step 13: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add -- src/layout.h src/layout.c tests/check.h tests/test_layout.c tests/CMakeLists.txt CMakeLists.txt src/CMakeLists.txt .github/workflows/build.yml
git -C C:/Users/jscha/source/repos/streamflex commit -m "test: add the layout module's types and its first unit tests (CTest, run in CI)"
```

---

### Task 2: Sizing and placing a menu's buttons (`layout_compute`)

**Files:**
- Modify: `src/layout.c` (append), `tests/test_layout.c` (add tests and the `main` calls)

**Interfaces:**
- Consumes: `LayoutParams`, `LayoutArea`, `LayoutGeometry` and `LAYOUT_MIN_BUTTON`/`LAYOUT_MAX_BUTTON` from Task 1.
- Produces: `int layout_compute(const LayoutParams *params, const LayoutArea *area, int entry_count, LayoutGeometry *geometry, char *why, size_t why_size)`.
  - It returns 0 on success. On success `why` is either empty or describes a reduction.
  - It returns -1 when not even one 32 px button fits, and then leaves `*geometry` untouched.
  - It also produces the static helpers `min_int` and `max_int`, which Task 3 uses.

The reference numbers below are for a 1920×1080 screen. The margin is 54 px (5% of 1080), `IconSpacing` 5% is 96 px, the title block is 60 px (padding plus font), and the highlight padding is 30. The usable area is `{x 0, y 54, w 1920, h 972, vcenter 540}`.

- [ ] **Step 1: Write the failing tests**

Add to `tests/test_layout.c`, above `main`:

```c
#include <string.h>

static const LayoutArea SCREEN_1080 = { 0, 54, 1920, 972, 540 };

static LayoutParams params(int rows, int columns, int icon_cap)
{
    LayoutParams p = { rows, columns, icon_cap, 96, 60, 30, 30 };
    return p;
}

// A function to test that a one-row strip with IconSize set lands exactly where today's layout puts it
static void test_compute_strip_matches_today(void)
{
    LayoutParams p = params(1, 4, 256);
    LayoutGeometry g;
    char why[128];
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, why, sizeof(why)), 0);
    CHECK_INT(g.button, 256);
    CHECK_INT(g.x_origin, 304);  // today: (1920 - 4*256 - 4*96 + 96) / 2
    CHECK_INT(g.y_origin, 382);  // today: 540 - (256 + 60) / 2
    CHECK_INT(g.x_advance, 352);
    CHECK_INT(g.y_advance, 412);
    CHECK_INT(g.rows, 1);
    CHECK_INT(g.columns, 4);
    CHECK(why[0] == '\0');
}

// A function to test that with no IconSize, buttons grow until the width runs out
static void test_compute_no_cap_fills(void)
{
    LayoutParams p = params(1, 4, 0);
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, NULL, 0), 0);
    CHECK_INT(g.button, 393);    // (1920 - 3*96 - 2*30) / 4
}

// A function to test that a tall grid is limited by the height, titles included
static void test_compute_grid_height_limited(void)
{
    LayoutParams p = params(3, 6, 0);
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 18, &g, NULL, 0), 0);
    CHECK_INT(g.button, 180);    // (972 - 2*96 - 2*30) / 3 - 60
    CHECK_INT(g.x_origin, 180);  // (1920 - (6*180 + 5*96)) / 2
    CHECK_INT(g.y_origin, 84);   // centered at 540, then kept inside: 54 + 30
    CHECK_INT(g.y_advance, 336); // 180 + 60 + 96
}

// A function to test that highlight padding is capped at half the gap, vertically only in grids
static void test_compute_padding_caps(void)
{
    LayoutParams p = params(1, 4, 256);
    LayoutGeometry g;
    p.spacing = 40;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, NULL, 0), 0);
    CHECK_INT(g.hpad, 20);
    CHECK_INT(g.vpad, 30);       // A strip has no row gap to protect
    p.rows = 2;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, NULL, 0), 0);
    CHECK_INT(g.hpad, 20);
    CHECK_INT(g.vpad, 20);
    p.hpad = -1;
    p.vpad = -1;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, NULL, 0), 0);
    CHECK_INT(g.hpad, 0);        // An unset (negative) padding counts as 0
    CHECK_INT(g.vpad, 0);
}

// A function to test that only the overflowing axis is reduced, and that the log gets a reason
static void test_compute_reduces_overflowing_axis(void)
{
    LayoutArea narrow = { 0, 0, 300, 972, 486 };
    LayoutParams p = params(1, 6, 0);
    LayoutGeometry g;
    char why[128];
    CHECK_INT(layout_compute(&p, &narrow, 6, &g, why, sizeof(why)), 0);
    CHECK_INT(g.columns, 2);     // 3 columns give 16 px, 2 give 72 px
    CHECK_INT(g.rows, 1);
    CHECK_INT(g.button, 72);
    CHECK(strstr(why, "reducing to 2 x 1") != NULL);

    LayoutArea short_area = { 0, 54, 1920, 300, 200 };
    p = params(3, 4, 0);
    CHECK_INT(layout_compute(&p, &short_area, 12, &g, why, sizeof(why)), 0);
    CHECK_INT(g.rows, 1);
    CHECK_INT(g.columns, 4);     // The width was never the problem
}

// A function to test that the call fails, and says why, when not even one button fits
static void test_compute_fails_when_nothing_fits(void)
{
    LayoutArea tiny = { 0, 0, 40, 972, 486 };
    LayoutParams p = params(1, 1, 0);
    LayoutGeometry g = { 0 };
    char why[128];
    CHECK(layout_compute(&p, &tiny, 1, &g, why, sizeof(why)) != 0);
    CHECK(why[0] != '\0');
    CHECK_INT(g.button, 0);      // Untouched on failure
}

// A function to test centering: a partial single row on its own buttons, a partial last row on the columns
static void test_compute_centering(void)
{
    LayoutParams p = params(3, 6, 0);
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 4, &g, NULL, 0), 0);
    CHECK_INT(g.x_origin, 456);  // (1920 - (4*180 + 3*96)) / 2
    CHECK_INT(g.y_origin, 420);  // one occupied row: 540 - 240 / 2
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 8, &g, NULL, 0), 0);
    CHECK_INT(g.x_origin, 180);  // columns stay put
    CHECK_INT(g.y_origin, 252);  // two occupied rows: 540 - 576 / 2
}

// A function to test that a grid stays inside the area whatever VCenter says,
// and that a block which fits is not moved
static void test_compute_vcenter_clamp(void)
{
    LayoutParams p = params(2, 4, 0);
    LayoutGeometry g;
    LayoutArea high = SCREEN_1080;
    LayoutArea low = SCREEN_1080;
    high.vcenter = 270;
    low.vcenter = 810;
    CHECK_INT(layout_compute(&p, &high, 8, &g, NULL, 0), 0);
    CHECK_INT(g.y_origin, 84);
    CHECK_INT(layout_compute(&p, &low, 8, &g, NULL, 0), 0);
    CHECK_INT(g.y_origin, 84);   // 84 + 912 + 30 is exactly the area's bottom

    p = params(1, 4, 256);
    CHECK_INT(layout_compute(&p, &high, 6, &g, NULL, 0), 0);
    CHECK_INT(g.y_origin, 112);  // 270 - 316 / 2, unchanged
}

// A function to test that the clock band shrinks a tall grid and keeps it below the clock
static void test_compute_clock_band(void)
{
    LayoutArea below_clock = { 0, 150, 1920, 876, 540 };
    LayoutParams p = params(3, 6, 0);
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &below_clock, 18, &g, NULL, 0), 0);
    CHECK_INT(g.button, 148);    // (876 - 192 - 60) / 3 - 60
    CHECK_INT(g.y_origin, 180);  // 150 + 30
}
```

Replace `main` with:

```c
int main(void)
{
    test_resolve();
    test_parse_count();
    test_compute_strip_matches_today();
    test_compute_no_cap_fills();
    test_compute_grid_height_limited();
    test_compute_padding_caps();
    test_compute_reduces_overflowing_axis();
    test_compute_fails_when_nothing_fits();
    test_compute_centering();
    test_compute_vcenter_clamp();
    test_compute_clock_band();
    return check_report();
}
```

- [ ] **Step 2: Build it and watch it fail**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_layout
```

Expected: FAIL at link time, with an unresolved external symbol `layout_compute`.

- [ ] **Step 3: Implement `layout_compute`**

Append to `src/layout.c`:

```c
// A function to return the smaller of two ints
static int min_int(int a, int b)
{
    return a < b ? a : b;
}

// A function to return the larger of two ints
static int max_int(int a, int b)
{
    return a > b ? a : b;
}

// A function to find the largest button for which `count` slots fit in `length` px, with
// `spacing` between slots, `pad` px of highlight at each end and `extra` px under each slot
static int fit(int length, int count, int spacing, int pad, int extra)
{
    return (length - (count - 1) * spacing - 2 * pad) / count - extra;
}

// A function to size and place a menu's buttons for its grid shape
int layout_compute(const LayoutParams *params, const LayoutArea *area, int entry_count,
                   LayoutGeometry *geometry, char *why, size_t why_size)
{
    LayoutGeometry g;
    int spacing = max_int(params->spacing, 0);
    int cap = params->icon_cap > 0 ? min_int(params->icon_cap, LAYOUT_MAX_BUTTON) : LAYOUT_MAX_BUTTON;
    int width_fit, height_fit;
    if (why_size > 0)
        why[0] = '\0';

    // Shrink only the axis that overflows until the smallest button fits. Highlight padding
    // is capped at half the gap between buttons; rows only have a gap between them in a grid.
    g.rows = max_int(params->rows, 1);
    g.columns = max_int(params->columns, 1);
    g.hpad = min_int(max_int(params->hpad, 0), spacing / 2);
    for (;;) {
        g.vpad = max_int(params->vpad, 0);
        if (g.rows > 1)
            g.vpad = min_int(g.vpad, spacing / 2);
        width_fit = fit(area->w, g.columns, spacing, g.hpad, 0);
        height_fit = fit(area->h, g.rows, spacing, g.vpad, params->title_block);
        if (width_fit < LAYOUT_MIN_BUTTON && g.columns > 1)
            g.columns--;
        else if (height_fit < LAYOUT_MIN_BUTTON && g.rows > 1)
            g.rows--;
        else
            break;
    }
    g.button = min_int(cap, min_int(width_fit, height_fit));
    if (g.button < LAYOUT_MIN_BUTTON) {
        if (why_size > 0)
            snprintf(why, why_size, "not even one %i px button fits in %i x %i px",
                LAYOUT_MIN_BUTTON, area->w, area->h);
        return -1;
    }
    if ((g.columns != params->columns || g.rows != params->rows) && why_size > 0)
        snprintf(why, why_size, "not enough screen space for %i x %i buttons, reducing to %i x %i",
            params->columns, params->rows, g.columns, g.rows);

    // Center the block horizontally on the configured columns, so columns stay put as it
    // scrolls. A menu that fills less than one row is centered on its own buttons instead.
    int count = max_int(entry_count, 1);
    int used_columns = min_int(count, g.columns);
    int used_rows = min_int(g.rows, (count + g.columns - 1) / g.columns);
    int block_w = used_columns * g.button + (used_columns - 1) * spacing;
    int block_h = used_rows * (g.button + params->title_block) + (used_rows - 1) * spacing;
    g.x_advance = g.button + spacing;
    g.y_advance = g.button + params->title_block + spacing;
    g.x_origin = area->x + (area->w - block_w) / 2;

    // Center the occupied rows on VCenter, then keep the block and its highlight inside the area
    g.y_origin = area->vcenter - block_h / 2;
    g.y_origin = min_int(g.y_origin, area->y + area->h - g.vpad - block_h);
    g.y_origin = max_int(g.y_origin, area->y + g.vpad);

    *geometry = g;
    return 0;
}
```

- [ ] **Step 4: Build and run the tests**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_layout
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: PASS, with `0 failed`.

- [ ] **Step 5: Prove the tests can fail**

Temporarily delete the line `g.vpad = min_int(g.vpad, spacing / 2);`. Expected: FAIL in `test_compute_padding_caps` (`g.vpad is 30, expected 20`). Restore it. Expected: PASS.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add -- src/layout.c tests/test_layout.c
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat(layout): size and place a menu's buttons for its grid shape"
```

---

### Task 3: Moving the highlight (strip and grid)

**Files:**
- Modify: `src/layout.c` (append), `tests/test_layout.c` (add tests and the `main` calls)

**Interfaces:**
- Consumes: `LayoutGeometry` and `LayoutPosition` from Task 1, and `min_int`/`max_int` from Task 2.
- Produces:
  - `LayoutPosition layout_move(const LayoutGeometry *geometry, int entry_count, LayoutPosition position, LayoutDirection direction, bool wrap)`
  - `LayoutPosition layout_clamp(const LayoutGeometry *geometry, int entry_count, LayoutPosition position)`
  - `bool layout_slot(const LayoutGeometry *geometry, LayoutPosition position, int index, int *x, int *y)`
  - `bool layout_can_scroll(const LayoutGeometry *geometry, int entry_count, LayoutPosition position, LayoutDirection direction)`

  `layout_move` and `layout_slot` read only `rows`, `columns`, `x_origin`, `y_origin`, `x_advance` and `y_advance` from the geometry.

- [ ] **Step 1: Write the failing tests**

Add to `tests/test_layout.c`, above `main`:

```c
static LayoutGeometry shape(int rows, int columns)
{
    LayoutGeometry g = { 0 };
    g.rows = rows;
    g.columns = columns;
    g.button = 100;
    g.x_advance = 120;
    g.y_advance = 150;
    g.x_origin = 10;
    g.y_origin = 20;
    return g;
}

static LayoutPosition at(int selected, int first)
{
    LayoutPosition p = { selected, first };
    return p;
}

#define CHECK_POS(position, want_selected, want_first) do { \
    LayoutPosition p_ = (position); \
    CHECK_INT(p_.selected, (want_selected)); \
    CHECK_INT(p_.first, (want_first)); \
} while (0)

// A function to test a one-row strip of 4 visible buttons holding 6 entries
static void test_strip_moves(void)
{
    LayoutGeometry g = shape(1, 4);
    CHECK_POS(layout_move(&g, 6, at(0, 0), LAYOUT_RIGHT, false), 1, 0);
    CHECK_POS(layout_move(&g, 6, at(3, 0), LAYOUT_RIGHT, false), 4, 1);  // slides one button
    CHECK_POS(layout_move(&g, 6, at(4, 1), LAYOUT_RIGHT, false), 5, 2);
    CHECK_POS(layout_move(&g, 6, at(5, 2), LAYOUT_RIGHT, false), 5, 2);  // end, no wrap
    CHECK_POS(layout_move(&g, 6, at(5, 2), LAYOUT_RIGHT, true), 0, 0);   // wraps to the first
    CHECK_POS(layout_move(&g, 6, at(0, 0), LAYOUT_LEFT, false), 0, 0);
    CHECK_POS(layout_move(&g, 6, at(0, 0), LAYOUT_LEFT, true), 5, 2);    // wraps to the last
    CHECK_POS(layout_move(&g, 6, at(2, 2), LAYOUT_LEFT, false), 1, 1);
    CHECK_POS(layout_move(&g, 6, at(2, 0), LAYOUT_UP, true), 2, 0);      // no rows to move between
    CHECK_POS(layout_move(&g, 6, at(2, 0), LAYOUT_DOWN, true), 2, 0);
    CHECK_POS(layout_move(&g, 3, at(2, 0), LAYOUT_RIGHT, false), 2, 0);  // fewer entries than columns
}

// A function to test where strip buttons are drawn and when the strip can scroll
static void test_strip_slots_and_scroll(void)
{
    LayoutGeometry g = shape(1, 4);
    int x = -1, y = -1;
    CHECK(layout_slot(&g, at(4, 1), 1, &x, &y));
    CHECK_INT(x, 10);
    CHECK_INT(y, 20);
    CHECK(layout_slot(&g, at(4, 1), 4, &x, &y));
    CHECK_INT(x, 370);
    CHECK(!layout_slot(&g, at(4, 1), 0, &x, &y));
    CHECK(!layout_slot(&g, at(4, 1), 5, &x, &y));
    CHECK(!layout_can_scroll(&g, 6, at(0, 0), LAYOUT_LEFT));
    CHECK(layout_can_scroll(&g, 6, at(0, 0), LAYOUT_RIGHT));
    CHECK(layout_can_scroll(&g, 6, at(5, 2), LAYOUT_LEFT));
    CHECK(!layout_can_scroll(&g, 6, at(5, 2), LAYOUT_RIGHT));
    CHECK(!layout_can_scroll(&g, 6, at(0, 0), LAYOUT_UP));
    CHECK(!layout_can_scroll(&g, 3, at(0, 0), LAYOUT_RIGHT));
}

// A function to test a 2-row, 4-column grid holding 10 entries: rows [0-3], [4-7], [8,9]
static void test_grid_moves(void)
{
    LayoutGeometry g = shape(2, 4);
    CHECK_POS(layout_move(&g, 10, at(3, 0), LAYOUT_RIGHT, false), 3, 0);  // stops at the row edge
    CHECK_POS(layout_move(&g, 10, at(3, 0), LAYOUT_RIGHT, true), 0, 0);   // wraps within the row
    CHECK_POS(layout_move(&g, 10, at(9, 1), LAYOUT_RIGHT, true), 8, 1);   // within the short last row
    CHECK_POS(layout_move(&g, 10, at(4, 0), LAYOUT_LEFT, false), 4, 0);
    CHECK_POS(layout_move(&g, 10, at(4, 0), LAYOUT_LEFT, true), 7, 0);
    CHECK_POS(layout_move(&g, 10, at(1, 0), LAYOUT_DOWN, false), 5, 0);
    CHECK_POS(layout_move(&g, 10, at(5, 0), LAYOUT_DOWN, false), 9, 1);   // scrolls one row
    CHECK_POS(layout_move(&g, 10, at(7, 0), LAYOUT_DOWN, false), 9, 1);   // lands on the short row's last
    CHECK_POS(layout_move(&g, 10, at(9, 1), LAYOUT_DOWN, false), 9, 1);
    CHECK_POS(layout_move(&g, 10, at(9, 1), LAYOUT_DOWN, true), 1, 0);    // wraps to the first row
    CHECK_POS(layout_move(&g, 10, at(1, 0), LAYOUT_UP, false), 1, 0);
    CHECK_POS(layout_move(&g, 10, at(3, 0), LAYOUT_UP, true), 9, 1);      // wraps to the last row
    CHECK_POS(layout_move(&g, 10, at(9, 1), LAYOUT_UP, false), 5, 1);     // row 1 is still in view
    CHECK_POS(layout_move(&g, 10, at(5, 1), LAYOUT_UP, false), 1, 0);     // scrolls back
    CHECK_POS(layout_move(&g, 3, at(1, 0), LAYOUT_DOWN, false), 1, 0);    // one partial row only
}

// A function to test where grid buttons are drawn and when the grid can scroll
static void test_grid_slots_and_scroll(void)
{
    LayoutGeometry g = shape(2, 4);
    int x = -1, y = -1;
    CHECK(layout_slot(&g, at(9, 1), 4, &x, &y));
    CHECK_INT(x, 10);
    CHECK_INT(y, 20);
    CHECK(layout_slot(&g, at(9, 1), 9, &x, &y));
    CHECK_INT(x, 130);
    CHECK_INT(y, 170);
    CHECK(!layout_slot(&g, at(9, 1), 3, &x, &y));
    CHECK(!layout_slot(&g, at(9, 1), -1, &x, &y));
    CHECK(layout_can_scroll(&g, 10, at(0, 0), LAYOUT_DOWN));
    CHECK(!layout_can_scroll(&g, 10, at(0, 0), LAYOUT_UP));
    CHECK(!layout_can_scroll(&g, 10, at(0, 0), LAYOUT_LEFT));
    CHECK(!layout_can_scroll(&g, 10, at(9, 1), LAYOUT_DOWN));
    CHECK(layout_can_scroll(&g, 10, at(9, 1), LAYOUT_UP));
    CHECK(!layout_can_scroll(&g, 3, at(0, 0), LAYOUT_DOWN));
}

// A function to test that clamping keeps the selection real and visible after a re-layout
static void test_clamp(void)
{
    LayoutGeometry strip = shape(1, 4);
    LayoutGeometry taller = shape(3, 4);
    CHECK_POS(layout_clamp(&strip, 6, at(9, 0)), 5, 2);
    CHECK_POS(layout_clamp(&strip, 6, at(-3, 5)), 0, 0);
    CHECK_POS(layout_clamp(&taller, 10, at(9, 1)), 9, 0);  // 3 rows show everything now
    CHECK_POS(layout_clamp(&strip, 0, at(4, 2)), 0, 0);
}
```

Replace `main` with:

```c
int main(void)
{
    test_resolve();
    test_parse_count();
    test_compute_strip_matches_today();
    test_compute_no_cap_fills();
    test_compute_grid_height_limited();
    test_compute_padding_caps();
    test_compute_reduces_overflowing_axis();
    test_compute_fails_when_nothing_fits();
    test_compute_centering();
    test_compute_vcenter_clamp();
    test_compute_clock_band();
    test_strip_moves();
    test_strip_slots_and_scroll();
    test_grid_moves();
    test_grid_slots_and_scroll();
    test_clamp();
    return check_report();
}
```

- [ ] **Step 2: Build it and watch it fail**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_layout
```

Expected: FAIL at link time, with unresolved `layout_move`, `layout_clamp`, `layout_slot` and `layout_can_scroll`.

- [ ] **Step 3: Implement the four functions**

Append to `src/layout.c`:

```c
// A function to tell a one-row strip from a grid
static bool is_strip(const LayoutGeometry *g)
{
    return g->rows == 1;
}

// A function to count the buttons in a grid row; only the last row can be short
static int row_length(const LayoutGeometry *g, int entry_count, int row)
{
    return min_int(entry_count - row * g->columns, g->columns);
}

// A function to keep the selection on a real entry and scrolled into view, without
// scrolling past the last strip button or the last grid row
LayoutPosition layout_clamp(const LayoutGeometry *geometry, int entry_count, LayoutPosition position)
{
    LayoutPosition p = position;
    int window, unit, last_first;
    if (entry_count <= 0) {
        p.selected = 0;
        p.first = 0;
        return p;
    }
    p.selected = max_int(0, min_int(p.selected, entry_count - 1));
    if (is_strip(geometry)) {
        window = geometry->columns;
        unit = p.selected;
        last_first = max_int(0, entry_count - geometry->columns);
    }
    else {
        window = geometry->rows;
        unit = p.selected / geometry->columns;
        last_first = max_int(0, (entry_count - 1) / geometry->columns - geometry->rows + 1);
    }
    if (unit < p.first)
        p.first = unit;
    if (unit >= p.first + window)
        p.first = unit - window + 1;
    p.first = max_int(0, min_int(p.first, last_first));
    return p;
}

// A function to move the selection one step, following the strip or grid rules
LayoutPosition layout_move(const LayoutGeometry *geometry, int entry_count, LayoutPosition position,
                           LayoutDirection direction, bool wrap)
{
    if (entry_count <= 0)
        return position;
    LayoutPosition p = layout_clamp(geometry, entry_count, position);
    int s = p.selected;

    // A strip moves along its one row and ignores Up and Down
    if (is_strip(geometry)) {
        if (direction == LAYOUT_RIGHT)
            s = s < entry_count - 1 ? s + 1 : (wrap ? 0 : s);
        else if (direction == LAYOUT_LEFT)
            s = s > 0 ? s - 1 : (wrap ? entry_count - 1 : s);
    }

    // A grid stops at row edges (or wraps within the row), and moves between rows keeping
    // the column, landing on the last button of a shorter last row
    else {
        int columns = geometry->columns;
        int row = s / columns;
        int column = s % columns;
        int last_row = (entry_count - 1) / columns;
        int length = row_length(geometry, entry_count, row);
        int target = -1;
        if (direction == LAYOUT_RIGHT) {
            if (column < length - 1)
                s++;
            else if (wrap)
                s = row * columns;
        }
        else if (direction == LAYOUT_LEFT) {
            if (column > 0)
                s--;
            else if (wrap)
                s = row * columns + length - 1;
        }
        else if (direction == LAYOUT_DOWN)
            target = row < last_row ? row + 1 : (wrap ? 0 : -1);
        else if (direction == LAYOUT_UP)
            target = row > 0 ? row - 1 : (wrap ? last_row : -1);
        if (target >= 0)
            s = target * columns + min_int(column, row_length(geometry, entry_count, target) - 1);
    }
    p.selected = s;
    return layout_clamp(geometry, entry_count, p);
}

// A function to find where an entry is drawn; false when it is scrolled out of view
bool layout_slot(const LayoutGeometry *geometry, LayoutPosition position, int index, int *x, int *y)
{
    int column, row;
    if (index < 0)
        return false;
    if (is_strip(geometry)) {
        column = index - position.first;
        row = 0;
        if (column < 0 || column >= geometry->columns)
            return false;
    }
    else {
        column = index % geometry->columns;
        row = index / geometry->columns - position.first;
        if (row < 0 || row >= geometry->rows)
            return false;
    }
    *x = geometry->x_origin + column * geometry->x_advance;
    *y = geometry->y_origin + row * geometry->y_advance;
    return true;
}

// A function to tell whether more buttons lie off screen in a direction, for the scroll indicators
bool layout_can_scroll(const LayoutGeometry *geometry, int entry_count, LayoutPosition position,
                       LayoutDirection direction)
{
    if (is_strip(geometry)) {
        if (direction == LAYOUT_LEFT)
            return position.first > 0;
        if (direction == LAYOUT_RIGHT)
            return position.first + geometry->columns < entry_count;
        return false;
    }
    if (direction == LAYOUT_UP)
        return position.first > 0;
    if (direction == LAYOUT_DOWN)
        return position.first + geometry->rows <= (entry_count - 1) / geometry->columns;
    return false;
}
```

- [ ] **Step 4: Build and run the tests**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_layout
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: PASS, with `0 failed`.

- [ ] **Step 5: Prove the tests can fail**

1. Temporarily change the in-row wrap `s = row * columns;` to `s++;`, which crosses rows. Expected: FAIL in `test_grid_moves` (`p_.selected is 4, expected 0`).
2. Then, in `layout_clamp`, change `if (unit >= p.first + window)` to `if (unit > p.first + window)`. Expected: FAIL in `test_strip_moves` (`(3,0) RIGHT` stays at first 0).
3. Restore both. Expected: PASS.

Do not use "drop the `min_int(column, ...)` short-row rule" as the mutation. It is **equivalent**: the last row's last button is always entry `entry_count - 1`, which is exactly where `layout_clamp` puts an overshoot. The tests cannot and should not fail on it.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add -- src/layout.c tests/test_layout.c
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat(layout): strip and grid navigation, slots and scroll checks"
```

---

### Task 4: Settings and the menu model (`Rows`, `Columns`, per-menu overrides)

The launcher still lays out with the old code after this task. The task changes what is parsed and stored, and verifies it through the debug log.

**Files:**
- Modify: `config/config_settings.cmake`, `config/launcher_config.h.in`, `src/launcher.h`, `src/launcher.c`, `src/util.c`, `src/util.h`, `src/debug.c`

**Interfaces:**
- Consumes: `LayoutOverrides`, `LayoutPosition` and `layout_parse_count` from Task 1.
- Produces:
  - `config.rows` (`unsigned int`);
  - `Menu.items` (`Entry **`), `Menu.overrides` (`LayoutOverrides`), `Menu.position` (`LayoutPosition`) and `Menu.rendered_size` (`int`);
  - `void build_menu_items(void)`;
  - the defines `SETTING_ROWS`, `SETTING_COLUMNS` and `DEFAULT_ROWS`.

- [ ] **Step 1: Add the setting names and default**

In `config/config_settings.cmake`, replace `set(SETTING_MAX_BUTTONS "MaxButtons")` with:

```cmake
set(SETTING_MAX_BUTTONS "MaxButtons")
set(SETTING_ROWS "Rows")
set(SETTING_COLUMNS "Columns")
```

Then replace `set(DEFAULT_MAX_BUTTONS 4)` with:

```cmake
set(DEFAULT_MAX_BUTTONS 4)
set(DEFAULT_ROWS 1)
```

In `config/launcher_config.h.in`, replace `#define SETTING_MAX_BUTTONS "@SETTING_MAX_BUTTONS@"` with:

```c
#define SETTING_MAX_BUTTONS "@SETTING_MAX_BUTTONS@"
#define SETTING_ROWS "@SETTING_ROWS@"
#define SETTING_COLUMNS "@SETTING_COLUMNS@"
```

Then replace `#define DEFAULT_MAX_BUTTONS @DEFAULT_MAX_BUTTONS@` with:

```c
#define DEFAULT_MAX_BUTTONS @DEFAULT_MAX_BUTTONS@
#define DEFAULT_ROWS @DEFAULT_ROWS@
```

- [ ] **Step 2: Extend `launcher.h`**

Add as the first line of `src/launcher.h`:

```c
#include "layout.h"
```

Replace the `Menu` struct with this version. It keeps the old fields until Task 5 removes them:

```c
// Linked list for menus
typedef struct menu {
    char            *name;
    unsigned int    num_entries;
    bool            rendered;
    unsigned int    page;
    unsigned int    highlight_position;
    Entry           *first_entry;
    Entry           *root_entry;
    Entry           *last_selected_entry;
    Entry           **items;          // Entries by index, for the layout math
    LayoutOverrides overrides;        // Per-menu Rows/Columns/IconSize; 0 = from [Layout]
    LayoutPosition  position;         // Selected entry and scroll position
    int             rendered_size;    // Button size the textures were rendered at; 0 = not yet
    struct menu     *next;
    struct menu     *back;
} Menu;
```

In the `Config` struct, replace `unsigned int max_buttons;` with:

```c
    unsigned int max_buttons; // The Columns setting (MaxButtons is its older name)
    unsigned int rows;
```

- [ ] **Step 3: Default `rows`, build the item arrays, and free them**

In `src/launcher.c`'s `Config config = { ... }`, replace `.max_buttons = DEFAULT_MAX_BUTTONS,` with:

```c
    .max_buttons                      = DEFAULT_MAX_BUTTONS,
    .rows                             = DEFAULT_ROWS,
```

In `main`, replace:

```c
    parse_config_file(config_file_path);
    free(config_file_path);
```

with:

```c
    parse_config_file(config_file_path);
    free(config_file_path);
    build_menu_items();
```

In `cleanup`, replace `free(menu->name);` (inside the menu loop) with:

```c
        free(menu->name);
        free(menu->items);
```

- [ ] **Step 4: Parse `Rows` and `Columns` in `[Layout]`**

In `src/util.c`, after the line `Entry                  *entry = NULL;`, add:

```c
static bool            columns_set = false; // Columns wins over its older name, MaxButtons
```

Replace the `MaxButtons` branch at the start of the `Layout` section:

```c
        if (MATCH(name, SETTING_MAX_BUTTONS)) {
            int max_buttons = atoi(value);
            if (max_buttons > 0)
                config.max_buttons = (unsigned int) max_buttons;
        }
```

with:

```c
        int count;
        if (MATCH(name, SETTING_MAX_BUTTONS)) {
            int max_buttons = atoi(value);
            if (max_buttons > 0 && !columns_set)
                config.max_buttons = (unsigned int) max_buttons;
        }
        else if (MATCH(name, SETTING_COLUMNS)) {
            if (layout_parse_count(value, &count)) {
                config.max_buttons = (unsigned int) count;
                columns_set = true;
            }
            else
                log_error("Invalid %s value '%s' in [Layout], ignoring it", SETTING_COLUMNS, value);
        }
        else if (MATCH(name, SETTING_ROWS)) {
            if (layout_parse_count(value, &count))
                config.rows = (unsigned int) count;
            else
                log_error("Invalid %s value '%s' in [Layout], ignoring it", SETTING_ROWS, value);
        }
```

- [ ] **Step 5: Parse per-menu layout keys, reading entry-shaped values as entries**

In the menu branch of `config_handler`, find the line `// Parse entry line for title, icon path, command`. Directly above it, after the `if (menu_exists == false) { ... }` block closes, insert:

```c
        // Per-menu layout settings. They count only when the value is a number, so an
        // existing entry that happens to be keyed Rows, Columns or IconSize still parses.
        bool layout_key = MATCH(name, SETTING_ROWS) || MATCH(name, SETTING_COLUMNS) ||
                          MATCH(name, SETTING_ICON_SIZE);
        if (layout_key && strchr(value, ';') == NULL) {
            int count = 0;
            bool valid = layout_parse_count(value, &count);
            if (valid && MATCH(name, SETTING_ICON_SIZE))
                valid = count >= MIN_ICON_SIZE && count <= MAX_ICON_SIZE;
            if (!valid)
                log_error("Invalid %s value '%s' in menu '%s', ignoring it", name, value, section);
            else if (MATCH(name, SETTING_ROWS))
                menu->overrides.rows = count;
            else if (MATCH(name, SETTING_COLUMNS))
                menu->overrides.columns = count;
            else
                menu->overrides.icon_cap = count;
            return 0;
        }
        if (layout_key)
            log_error("Menu '%s': '%s' holds an entry, so it is read as an entry", section, name);
```

- [ ] **Step 6: Zero new entries and menus, and build the item arrays**

In the same branch, replace both `malloc(sizeof(Entry))` calls with `calloc(1, sizeof(Entry))`:
- `menu->first_entry = malloc(sizeof(Entry));` becomes `menu->first_entry = calloc(1, sizeof(Entry));`
- `entry = malloc(sizeof(Entry));` becomes `entry = calloc(1, sizeof(Entry));`

The texture pointers must start as NULL, because Task 5 destroys old textures before re-rendering.

In `create_menu`, replace the compound literal with:

```c
    *menu = (Menu) {
        .first_entry = NULL,
        .next = NULL,
        .back = NULL,
        .root_entry = NULL,
        .num_entries = 0,
        .page = 0,
        .highlight_position = 0,
        .rendered = false,
        .items = NULL,
        .overrides = { 0, 0, 0 },
        .position = { 0, 0 },
        .rendered_size = 0
    };
```

After `create_menu` in `src/util.c`, add:

```c
// A function to give every menu an array of its entries by index, for the layout math
void build_menu_items()
{
    for (Menu *m = config.first_menu; m != NULL; m = m->next) {
        if (m->num_entries == 0)
            continue;
        m->items = malloc(m->num_entries * sizeof(Entry*));
        Entry *e = m->first_entry;
        for (unsigned int i = 0; i < m->num_entries; i++) {
            m->items[i] = e;
            e = e->next;
        }
    }
}
```

In `src/util.h`, after `void parse_config_file(const char *config_file_path);`, add:

```c
void build_menu_items(void);
```

- [ ] **Step 7: Print the new settings in the debug log**

In `src/debug.c` `debug_settings`, replace `DEBUG_INT(SETTING_MAX_BUTTONS, config.max_buttons);` with:

```c
    DEBUG_INT(SETTING_ROWS, config.rows);
    DEBUG_INT(SETTING_COLUMNS, config.max_buttons);
```

In `debug_menu_entries`, after `log_debug("Number of Entries: %i",menu->num_entries);`, add:

```c
        log_debug("Layout overrides (0 = from [Layout]): %s %i, %s %i, %s %i",
            SETTING_ROWS, menu->overrides.rows,
            SETTING_COLUMNS, menu->overrides.columns,
            SETTING_ICON_SIZE, menu->overrides.icon_cap);
```

- [ ] **Step 8: Build everything and run the unit tests**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: the build succeeds and the tests pass.

- [ ] **Step 9: Write the parser fixtures**

These are not committed; they hold machine paths. Create the folder `C:/Users/jscha/ClaudeScratch/streamflex-fixtures/`.

`C:/Users/jscha/ClaudeScratch/streamflex-fixtures/a-parse.ini`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Layout]
MaxButtons=7
Columns=5
Rows=2
IconSize=200

[Titles]
Font=C:/Users/jscha/source/repos/streamflex/assets/fonts/OpenSans-Regular.ttf

[Clock]
Enabled=false

[Main]
Rows=3
Columns=6
IconSize=abc
Entry1=Kodi;C:/Users/jscha/source/repos/streamflex/assets/icons/kodi.png;:quit
Columns=Plex;C:/Users/jscha/source/repos/streamflex/assets/icons/plex.png;:quit
Entry3=Steam;C:/Users/jscha/source/repos/streamflex/assets/icons/steam.png;:quit
```

`C:/Users/jscha/ClaudeScratch/streamflex-fixtures/b-columns-first.ini` is identical to A, except its `[Layout]` section reads:

```ini
[Layout]
Columns=5
MaxButtons=7
Rows=2
IconSize=200
```

- [ ] **Step 10: Run the fixtures and check the log**

`StartupCmd=:quit` makes the launcher start, write its debug log, lay out the default menu and exit. **Tell the user before running this: a full-screen window flashes for about a second each run.** The executable is a Windows GUI program, so it must be run with `Start-Process -Wait`; PowerShell's `&` would not wait for it.

```powershell
$exe = "C:/Users/jscha/source/repos/streamflex/build/Release/streamflex.exe"
$log = "C:/Users/jscha/source/repos/streamflex/build/Release/streamflex.log"
foreach ($f in "a-parse", "b-columns-first") {
    $p = Start-Process -FilePath $exe -ArgumentList "-c", "C:/Users/jscha/ClaudeScratch/streamflex-fixtures/$f.ini", "-d" -Wait -PassThru
    "== $f (exit $($p.ExitCode))"
    Select-String -Path $log -Pattern 'Rows:\s+\d+|Columns:\s+\d+|Invalid|holds an entry|Layout overrides|Number of Entries|Entry \d Title' | ForEach-Object { "  $($_.Line)" }
}
```

Expected, for both fixtures:
- `Columns:` reads `5` (not `7`) whichever key comes first;
- `Rows:` reads `2`;
- an `Invalid IconSize value 'abc' in menu 'Main'` line;
- a `Menu 'Main': 'Columns' holds an entry` line;
- `Layout overrides ... Rows 3, Columns 6, IconSize 0`;
- `Number of Entries: 3`, with `Plex` among the titles.

The exit code is 0.

- [ ] **Step 11: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add -- config/config_settings.cmake config/launcher_config.h.in src/launcher.h src/launcher.c src/util.c src/util.h src/debug.c
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat(config): Rows and Columns, per-menu layout overrides, entries by index"
```

---

### Task 5: The launcher draws and moves through the layout module

**Files:**
- Modify: `src/launcher.h`, `src/launcher.c`, `src/util.c`, `src/util.h`, `src/image.c`, `src/image.h`, `src/debug.c`

**Interfaces:**
- Consumes: everything from Tasks 1 to 4.
- Produces:
  - `static void move_selection(LayoutDirection direction)` in `launcher.c` (Task 6 calls it for Up/Down);
  - `SDL_Texture *load_icon(const char *path, int size)` and `SDL_Texture *rasterize_svg_from_file(const char *path, int w, int h, SDL_Rect *rect)` in `image.c`;
  - the `Scroll.rect_up` / `Scroll.rect_down` fields, `Geometry.vcenter`, and the `Highlight` fields `button`, `hpad` and `vpad`;
  - the globals `LayoutGeometry layout` and `LayoutArea layout_area` in `launcher.c`.

- [ ] **Step 1: Replace the old layout fields in `launcher.h`**

Replace the `Menu` struct with the final version:

```c
// Linked list for menus
typedef struct menu {
    char            *name;
    unsigned int    num_entries;
    Entry           *first_entry;
    Entry           **items;          // Entries by index, for the layout math
    LayoutOverrides overrides;        // Per-menu Rows/Columns/IconSize; 0 = from [Layout]
    LayoutPosition  position;         // Selected entry and scroll position
    int             rendered_size;    // Button size the textures were rendered at; 0 = not yet
    struct menu     *next;
    struct menu     *back;
} Menu;
```

Replace the `Geometry` struct:

```c
// Struct for the screen geometry every menu shares
typedef struct {
    int screen_width;
    int screen_height;
    int screen_margin;
    int font_height;
    int vcenter; // The VCenter setting in px from the top of the screen
} Geometry;
```

Replace the `Highlight` struct:

```c
// Struct for highlight, with the button size and padding its texture was rendered for
typedef struct {
    SDL_Texture *texture;
    SDL_Rect rect;
    int button;
    int hpad;
    int vpad;
} Highlight;
```

Replace the `Scroll` struct:

```c
// Struct for scroll indicators: left and right for a strip, up and down for a grid
typedef struct {
    SDL_Texture *texture;
    SDL_Rect rect_right;
    SDL_Rect rect_left;
    SDL_Rect rect_up;
    SDL_Rect rect_down;
} Scroll;
```

Delete the `Direction` enum (`DIRECTION_LEFT`, `DIRECTION_RIGHT`). Only `advance_entries` used it, and that is removed in Step 2.

- [ ] **Step 2: Remove the single-row helpers from `util.c`/`util.h`, and update `create_menu`**

- Delete the function `advance_entries` from `src/util.c`, and the line `Entry *advance_entries(Entry *entry, int spaces, Direction direction);` from `src/util.h`.
- Delete the function `calculate_width` from `src/util.c`, and its prototype line from `src/util.h`.
- In `create_menu`, replace the compound literal with:

```c
    *menu = (Menu) {
        .first_entry = NULL,
        .items = NULL,
        .next = NULL,
        .back = NULL,
        .num_entries = 0,
        .overrides = { 0, 0, 0 },
        .position = { 0, 0 },
        .rendered_size = 0
    };
```

- [ ] **Step 3: Move width-fitting out of `validate_settings`**

In `src/util.c` `validate_settings`:

1. Delete the first block, which starts `// Reduce number of buttons if they can't all fit on screen` and ends with `config.max_buttons = i;` and its closing brace.
2. **Keep** the `// Reduce highlight hpadding to prevent overlaps` block (the `hpadding ≤ icon_spacing / 2` cap). The highlight-outline limit further down is computed from it, and `layout_compute` applies the same cap again, harmlessly. Delete everything from `// Reduce icon spacing and highlight padding if too large to fit onscreen` down to the closing brace of the `if (config.icon_spacing != icon_spacing) { ... }` block: the `required_length` loop and its two `log_error` blocks. `layout_compute` now fits each menu by sizing its buttons.
3. Replace the title padding block:

```c
    // Make sure title padding is in valid range
    if (config.title_padding < 0 || config.title_padding > config.icon_size / 2) {
        int title_padding = config.icon_size / 10;
```

with:

```c
    // Make sure title padding is in valid range. IconSize is an optional cap now, so the range
    // is measured against it when set, and against the old fixed default when not.
    int reference_size = config.icon_size ? (int) config.icon_size : DEFAULT_ICON_SIZE;
    if (config.title_padding < 0 || config.title_padding > reference_size / 2) {
        int title_padding = reference_size / 10;
```

4. Replace the VCenter block, from `// Calculate y margin for buttons from centerline setting string, check limits` down to `geo->y_margin = vcenter - button_height / 2;`, with:

```c
    // Convert the vertical center setting to px and check its limits
    int vcenter = INVALID_PERCENT_VALUE;
    float f_screen_height = (float) geo->screen_height;
    int lower_limit = (int) (MIN_VCENTER*f_screen_height);
    int upper_limit = (int) (MAX_VCENTER*f_screen_height);
    if (config.vcenter[0] != '\0')
        convert_percent_to_int(config.vcenter, &vcenter, geo->screen_height);
    if (vcenter == INVALID_PERCENT_VALUE)
        convert_percent_to_int(DEFAULT_VCENTER, &vcenter, geo->screen_height);
    if (vcenter < lower_limit)
        vcenter = lower_limit;
    else if (vcenter > upper_limit)
        vcenter = upper_limit;
    geo->vcenter = vcenter;
```

- [ ] **Step 4: Load icons at button size, and place the grid's scroll indicators, in `image.c`**

Add `#include <string.h>` after `#include <math.h>` in `src/image.c`. Then, after the `rasterize_svg` function, add:

```c
// A function to rasterize an SVG file; pass -1 for w or h to keep the aspect ratio
SDL_Texture *rasterize_svg_from_file(const char *path, int w, int h, SDL_Rect *rect)
{
    char *buffer = SDL_LoadFile(path, NULL);
    if (buffer == NULL) {
        log_error("Could not load image %s\n%s", path, SDL_GetError());
        return NULL;
    }
    SDL_Texture *texture = rasterize_svg(buffer, w, h, rect);
    SDL_free(buffer);
    return texture;
}

// A function to load a menu icon. SVGs are rasterized at the button size so they stay sharp
// at any size; other formats load at their own size and the renderer scales them.
SDL_Texture *load_icon(const char *path, int size)
{
    if (path == NULL)
        return NULL;
    size_t length = strlen(path);
    if (length > 4 && SDL_strcasecmp(path + length - 4, ".svg") == 0)
        return rasterize_svg_from_file(path, size, -1, NULL);
    return load_texture_from_file(path);
}
```

In `render_scroll_indicators`, after the last line (`scroll->rect_left.x = geo->screen_margin;`), add:

```c

    // Grid indicators: the same arrow drawn a quarter turn round (see draw_screen), sized so
    // its on-screen height is the screen margin and centered in the top and bottom margins.
    // SDL rotates about the rect's center, so the rect keeps the unrotated arrow's proportions,
    // and its width becomes the on-screen height.
    int grid_w = geo->screen_margin;
    int grid_h = grid_w * scroll->rect_right.h / scroll->rect_right.w;
    scroll->rect_up = (SDL_Rect) {
        .x = geo->screen_width / 2 - grid_w / 2,
        .y = geo->screen_margin / 2 - grid_h / 2,
        .w = grid_w,
        .h = grid_h
    };
    scroll->rect_down = scroll->rect_up;
    scroll->rect_down.y = geo->screen_height - geo->screen_margin / 2 - grid_h / 2;
```

In `src/image.h`, after the `rasterize_svg_from_file` prototype, add:

```c
SDL_Texture *load_icon(const char *path, int size);
```

- [ ] **Step 5: Make `IconSize` an optional cap, and add the layout globals and prototypes, in `launcher.c`**

In `Config config = { ... }`, replace `.icon_size = DEFAULT_ICON_SIZE,` with:

```c
    .icon_size                        = 0, // IconSize is an optional cap on button size; 0 = none
```

After `Geometry geo;` in the globals, add:

```c
LayoutGeometry layout;                     // The current menu's layout
LayoutArea layout_area;                    // The part of the screen the buttons may use
```

In the prototypes at the top, replace:

```c
static void calculate_button_geometry(Entry *entry, int buttons);
static void render_buttons(Menu *menu);
static void move_left(void);
static void move_right(void);
```

with:

```c
static void calculate_layout_area(void);
static int apply_layout(Menu *menu);
static void render_buttons(Menu *menu, int size);
static void place_entries(void);
static void move_selection(LayoutDirection direction);
```

In `init_sdl_ttf`, replace `.max_width = config.icon_size,` with:

```c
        .max_width = 0, // Set per menu to its button size, in render_buttons
```

- [ ] **Step 6: Replace the menu loading, geometry and movement functions**

In `src/launcher.c`, delete the functions `load_menu`, `calculate_button_geometry`, `render_buttons`, `move_left` and `move_right`. Keep `load_menu_by_name` between them. Put these in their place:

```c
// A function to load a menu
static int load_menu(Menu *menu, bool set_back_menu, bool reset_position)
{
    if (menu == NULL)
        return 1;
    log_debug("Loading menu '%s'", menu->name);

    // Return error if the menu doesn't contain entries
    if (menu->num_entries == 0) {
        log_error("No valid entries found for Menu '%s'", menu->name);
        return 1;
    }

    Menu *previous_menu = current_menu;
    current_menu = menu;
    if (reset_position)
        current_menu->position = (LayoutPosition) { 0, 0 };
    if (apply_layout(current_menu)) {
        current_menu = previous_menu;
        return 1;
    }
    if (set_back_menu)
        current_menu->back = previous_menu;
    return 0;
}
```

(`load_menu_by_name` stays as it is.)

```c
// A function to work out the screen area the buttons may use: the full width, and the height
// between the top and bottom margins, starting below the clock when it is shown
static void calculate_layout_area()
{
    int top = geo.screen_margin;
    if (config.clock_enabled && clk != NULL) {
        SDL_Rect *lowest = config.clock_show_date ? &clk->date_rect : &clk->time_rect;
        if (lowest->y + lowest->h > top)
            top = lowest->y + lowest->h;
    }
    layout_area = (LayoutArea) {
        .x = 0,
        .y = top,
        .w = geo.screen_width,
        .h = geo.screen_height - geo.screen_margin - top,
        .vcenter = geo.vcenter
    };
}

// A function to lay out the current menu: size its buttons for its grid, re-render its
// textures if that size changed, and place the visible entries
static int apply_layout(Menu *menu)
{
    LayoutOverrides global = { (int) config.rows, (int) config.max_buttons, (int) config.icon_size };
    LayoutOverrides builtin = { DEFAULT_ROWS, DEFAULT_MAX_BUTTONS, 0 };
    LayoutOverrides effective = layout_resolve(menu->overrides, global, builtin);
    LayoutParams params = {
        .rows        = effective.rows,
        .columns     = effective.columns,
        .icon_cap    = effective.icon_cap,
        .spacing     = config.icon_spacing,
        .title_block = config.title_padding + geo.font_height,
        .hpad        = config.highlight_hpadding,
        .vpad        = config.highlight_vpadding
    };
    char why[256];
    if (layout_compute(&params, &layout_area, (int) menu->num_entries, &layout, why, sizeof(why))) {
        log_error("Menu '%s' cannot be shown: %s", menu->name, why);
        return 1;
    }
    if (why[0] != '\0')
        log_error("Menu '%s': %s", menu->name, why);
    log_debug("Menu '%s': %i x %i grid, %i px buttons", menu->name, layout.columns, layout.rows, layout.button);

    if (menu->rendered_size != layout.button) {
        render_buttons(menu, layout.button);
        menu->rendered_size = layout.button;
    }
    if (config.highlight && (highlight->button != layout.button ||
    highlight->hpad != layout.hpad || highlight->vpad != layout.vpad)) {
        if (highlight->texture != NULL)
            SDL_DestroyTexture(highlight->texture);
        int button_height = layout.button + config.title_padding + geo.font_height;
        highlight->texture = render_highlight(layout.button + 2*layout.hpad,
                                 button_height + 2*layout.vpad,
                                 &highlight->rect
                             );
        highlight->button = layout.button;
        highlight->hpad = layout.hpad;
        highlight->vpad = layout.vpad;
    }
    menu->position = layout_clamp(&layout, (int) menu->num_entries, menu->position);
    place_entries();
    return 0;
}

// A function to render all buttons (icon and title) of a menu at a button size
static void render_buttons(Menu *menu, int size)
{
    title_info.max_width = size;
    for (unsigned int i = 0; i < menu->num_entries; i++) {
        Entry *entry = menu->items[i];
        if (entry->icon != NULL)
            SDL_DestroyTexture(entry->icon);
        if (entry->icon_selected != NULL)
            SDL_DestroyTexture(entry->icon_selected);
        entry->icon = load_icon(entry->icon_path, size);
        entry->icon_selected = entry->icon_selected_path != NULL ? load_icon(entry->icon_selected_path, size) : NULL;
        if (config.titles_enabled) {
            int h;
            if (entry->title_texture != NULL)
                SDL_DestroyTexture(entry->title_texture);
            entry->title_texture = render_text_texture(entry->title, &title_info, &entry->text_rect, &h);
            entry->title_offset = (config.title_oversize_mode == OVERSIZE_SHRINK && h != geo.font_height)
                                  ? (geo.font_height - h) / 2 : 0;
        }
    }
}

// A function to position the visible buttons and the highlight for the current menu
static void place_entries()
{
    for (unsigned int i = 0; i < current_menu->num_entries; i++) {
        Entry *entry = current_menu->items[i];
        int x, y;
        if (!layout_slot(&layout, current_menu->position, (int) i, &x, &y))
            continue;
        entry->icon_rect = (SDL_Rect) { x, y, layout.button, layout.button };
        entry->text_rect.x = x + (layout.button - entry->text_rect.w) / 2;
        entry->text_rect.y = y + layout.button + entry->title_offset + config.title_padding;
    }
    current_entry = current_menu->items[current_menu->position.selected];
    if (config.highlight) {
        highlight->rect.x = current_entry->icon_rect.x - layout.hpad;
        highlight->rect.y = current_entry->icon_rect.y - layout.vpad;
    }
}

// A function to move the highlight, scrolling the strip or grid when needed
static void move_selection(LayoutDirection direction)
{
    LayoutPosition position = layout_move(&layout, (int) current_menu->num_entries,
                                  current_menu->position, direction, config.wrap_entries);
    if (position.selected == current_menu->position.selected && position.first == current_menu->position.first)
        return;
    current_menu->position = position;
    place_entries();
}
```

Replace `load_submenu` with the following. The parent menu keeps its own position now:

```c
// A function to load a submenu
static void load_submenu(const char *submenu)
{
    load_menu_by_name(submenu, true, true);
}
```

- [ ] **Step 7: Draw through the layout**

In `draw_screen`, replace the two scroll indicator `if` statements (from `// Draw scroll indicators` through the `SDL_RenderCopyEx(... SDL_FLIP_HORIZONTAL);` line) with:

```c
        // Draw scroll indicators: a strip's point left and right from the bottom corners,
        // a grid's are the same arrow turned to point up and down from the top and bottom margins
        if (config.scroll_indicators) {
            int count = (int) current_menu->num_entries;
            LayoutPosition position = current_menu->position;
            if (layout.rows == 1) {
                if (layout_can_scroll(&layout, count, position, LAYOUT_RIGHT))
                    SDL_RenderCopy(renderer, scroll->texture, NULL, &scroll->rect_right);
                if (layout_can_scroll(&layout, count, position, LAYOUT_LEFT))
                    SDL_RenderCopyEx(renderer, scroll->texture, NULL, &scroll->rect_left, 0, NULL, SDL_FLIP_HORIZONTAL);
            }
            else {
                if (layout_can_scroll(&layout, count, position, LAYOUT_UP))
                    SDL_RenderCopyEx(renderer, scroll->texture, NULL, &scroll->rect_up, 270.0, NULL, SDL_FLIP_NONE);
                if (layout_can_scroll(&layout, count, position, LAYOUT_DOWN))
                    SDL_RenderCopyEx(renderer, scroll->texture, NULL, &scroll->rect_down, 90.0, NULL, SDL_FLIP_NONE);
            }
        }
```

Replace the buttons block (from `// Draw buttons` through the closing brace of its `for` loop) with:

```c
        // Draw the visible buttons
        for (unsigned int i = 0; i < current_menu->num_entries; i++) {
            int x, y;
            if (!layout_slot(&layout, current_menu->position, (int) i, &x, &y))
                continue;
            Entry *entry = current_menu->items[i];
            SDL_Texture *icon = (entry->icon_selected != NULL && (int) i == current_menu->position.selected)
                                ? entry->icon_selected : entry->icon;
            SDL_RenderCopy(renderer, icon, NULL, &entry->icon_rect);
            if (config.titles_enabled)
                SDL_RenderCopy(renderer, entry->title_texture, NULL, &entry->text_rect);
        }
```

- [ ] **Step 8: Route Left/Right through `move_selection`, allocate the highlight, and compute the area**

- In `execute_command`, replace `move_left();` with `move_selection(LAYOUT_LEFT);` and `move_right();` with `move_selection(LAYOUT_RIGHT);`.
- In `handle_keypress`, make the same two replacements.
- In `main`, replace the whole `// Render highlight` block with:

```c
    // Allocate the highlight; its texture is rendered for each button size as menus load
    if (config.highlight) {
        highlight = malloc(sizeof(Highlight));
        *highlight = (Highlight) { .texture = NULL, .button = 0, .hpad = 0, .vpad = 0 };
    }
```

- In `main`, after the `// Render scroll indicators` block, add:

```c

    // Work out where the buttons may go, now that the clock's size is known
    calculate_layout_area();
```

- [ ] **Step 9: Print `IconSize` as a cap**

In `src/debug.c`, replace `DEBUG_INT(SETTING_ICON_SIZE, config.icon_size);` with:

```c
    if (config.icon_size)
        DEBUG_INT(SETTING_ICON_SIZE, config.icon_size);
    else
        DEBUG_STR(SETTING_ICON_SIZE, "none (buttons fill the grid)");
```

- [ ] **Step 10: Build, test, and prove nothing of the old layout is left**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: the build succeeds and the tests pass. Then run a Grep over `C:/Users/jscha/source/repos/streamflex/src` (point `path` at that folder and use no glob):

```
geo\.(x_margin|y_margin|x_advance|num_buttons)|root_entry|highlight_position|->page\b|last_selected_entry|->rendered\b|advance_entries|calculate_width|calculate_button_geometry|move_left|move_right|DIRECTION_
```

Expected: no matches.

- [ ] **Step 11: Write the layout fixtures**

Create these in `C:/Users/jscha/ClaudeScratch/streamflex-fixtures/`. Every fixture shares this header, which the fixtures below call `<common>`:

```ini
[General]
DefaultMenu=Main
StartupCmd=:quit

[Titles]
Font=C:/Users/jscha/source/repos/streamflex/assets/fonts/OpenSans-Regular.ttf

[Clock]
Enabled=false
```

`f-today.ini` is `<common>` plus:

```ini
[Layout]
Columns=4
IconSize=256

[Main]
Entry1=Kodi;C:/Users/jscha/source/repos/streamflex/assets/icons/kodi.png;:quit
Entry2=Plex;C:/Users/jscha/source/repos/streamflex/assets/icons/plex.png;:quit
Entry3=Logo;C:/Users/jscha/source/repos/streamflex/docs/streamflex.svg;:quit
Entry4=Missing;C:/Users/jscha/ClaudeScratch/streamflex-fixtures/no-such-icon.png;:quit
Entry5=Steam;C:/Users/jscha/source/repos/streamflex/assets/icons/steam.png;:quit
```

`g-grid.ini` is `<common>` plus:

```ini
[Layout]
Columns=4
IconSize=256

[Main]
Entry1=Kodi;C:/Users/jscha/source/repos/streamflex/assets/icons/kodi.png;:quit
Entry2=Games;C:/Users/jscha/source/repos/streamflex/assets/icons/steam.png;:submenu Games

[Games]
Rows=3
Columns=6
Entry1=A;C:/Users/jscha/source/repos/streamflex/assets/icons/kodi.png;:quit
Entry2=B;C:/Users/jscha/source/repos/streamflex/assets/icons/plex.png;:quit
Entry3=C;C:/Users/jscha/source/repos/streamflex/assets/icons/steam.png;:quit
Entry4=D;C:/Users/jscha/source/repos/streamflex/assets/icons/retroarch.png;:quit
Entry5=E;C:/Users/jscha/source/repos/streamflex/assets/icons/system.png;:quit
Entry6=F;C:/Users/jscha/source/repos/streamflex/assets/icons/sleep.png;:quit
Entry7=G;C:/Users/jscha/source/repos/streamflex/assets/icons/restart.png;:quit
```

`h-no-cap.ini` is `<common>` plus:

```ini
[Layout]
Columns=4

[Main]
Entry1=Kodi;C:/Users/jscha/source/repos/streamflex/assets/icons/kodi.png;:quit
Entry2=Plex;C:/Users/jscha/source/repos/streamflex/assets/icons/plex.png;:quit
```

`g-grid-games.ini` is `g-grid.ini` with `DefaultMenu=Games`. The launcher lays out only the default menu before `StartupCmd` runs, so this is how the Games grid gets computed.

- [ ] **Step 12: Run the layout fixtures and check the log**

Warn the user about the flashing window first.

```powershell
$exe = "C:/Users/jscha/source/repos/streamflex/build/Release/streamflex.exe"
$log = "C:/Users/jscha/source/repos/streamflex/build/Release/streamflex.log"
foreach ($f in "f-today", "g-grid", "g-grid-games", "h-no-cap") {
    $p = Start-Process -FilePath $exe -ArgumentList "-c", "C:/Users/jscha/ClaudeScratch/streamflex-fixtures/$f.ini", "-d" -Wait -PassThru
    "== $f (exit $($p.ExitCode))"
    Select-String -Path $log -Pattern "grid, \d+ px buttons|IconSize:|Could not load image|cannot be shown|reducing" | ForEach-Object { "  $($_.Line)" }
}
```

Expected:
- **`f-today`:** `Menu 'Main': 4 x 1 grid, 256 px buttons`. One `Could not load image ...no-such-icon.png` error. No error for `streamflex.svg`. Exit code 0 (no crash on the missing icon).
- **`g-grid`:** `Menu 'Main': 4 x 1 grid, 256 px buttons`.
- **`g-grid-games`:** `Menu 'Games': 6 x 3 grid, N px buttons`, where N is at most 256 and depends on this monitor's resolution.
- **`h-no-cap`:** `IconSize: none (buttons fill the grid)`, and a `Main` button size larger than 256 on any screen at least 1280 px wide.

- [ ] **Step 13: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add -- src/launcher.h src/launcher.c src/util.c src/util.h src/image.c src/image.h src/debug.c
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: menus lay out and scroll through the layout module; IconSize becomes a cap"
```

---

### Task 6: Up and Down (keys, special commands, gamepad defaults)

**Files:**
- Modify: `src/launcher.h`, `src/launcher.c`, `src/util.c`, `src/util.h`

**Interfaces:**
- Consumes: `move_selection(LayoutDirection)` from Task 5, plus the existing `add_gamepad_control`, `gamepad_controls` and `hotkeys`.
- Produces: `SCMD_UP` (`":up"`), `SCMD_DOWN` (`":down"`), and `void add_default_gamepad_controls(void)`.

- [ ] **Step 1: Add the special commands**

In `src/launcher.h`, replace `#define SCMD_RIGHT ":right"` with:

```c
#define SCMD_RIGHT ":right"
#define SCMD_UP ":up"
#define SCMD_DOWN ":down"
```

In `src/launcher.c` `execute_command`, after the `SCMD_RIGHT` branch, add:

```c
        else if (!strcmp(special_command, SCMD_UP))
            move_selection(LAYOUT_UP);
        else if (!strcmp(special_command, SCMD_DOWN))
            move_selection(LAYOUT_DOWN);
```

- [ ] **Step 2: Add the Up and Down keys, giving way to an existing hotkey**

In `src/launcher.c`, add the prototype `static bool hotkey_bound(SDL_Keycode keycode);` with the others. Add this function above `handle_keypress`:

```c
// A function to check whether the config binds a hotkey to a key
static bool hotkey_bound(SDL_Keycode keycode)
{
    for (Hotkey *i = hotkeys; i != NULL; i = i->next) {
        if (i->keycode == keycode)
            return true;
    }
    return false;
}
```

In `handle_keypress`, replace:

```c
    // Check default keys
    if (key->sym == SDLK_LEFT)
        move_selection(LAYOUT_LEFT);
    else if (key->sym == SDLK_RIGHT)
        move_selection(LAYOUT_RIGHT);
```

with:

```c
    // Check default keys. Up and Down give way to a hotkey bound to the same key, so an
    // existing config's binding keeps working after the upgrade.
    if (key->sym == SDLK_LEFT)
        move_selection(LAYOUT_LEFT);
    else if (key->sym == SDLK_RIGHT)
        move_selection(LAYOUT_RIGHT);
    else if ((key->sym == SDLK_UP || key->sym == SDLK_DOWN) && !hotkey_bound(key->sym))
        move_selection(key->sym == SDLK_UP ? LAYOUT_UP : LAYOUT_DOWN);
```

- [ ] **Step 3: Default gamepad Up/Down**

In `src/util.c`, add these static prototypes after `static Menu *create_menu(...);`:

```c
static bool gamepad_command_mapped(const char *cmd);
static bool gamepad_control_mapped(const char *label);
static void add_default_controls(const char *cmd, const char *const *labels, size_t count);
```

After the `add_gamepad_control` function, add:

```c
// A function to check whether any gamepad control runs a command
static bool gamepad_command_mapped(const char *cmd)
{
    for (GamepadControl *i = gamepad_controls; i != NULL; i = i->next) {
        if (MATCH(i->cmd, cmd))
            return true;
    }
    return false;
}

// A function to check whether a gamepad control is mapped to anything
static bool gamepad_control_mapped(const char *label)
{
    for (GamepadControl *i = gamepad_controls; i != NULL; i = i->next) {
        if (MATCH(i->label, label))
            return true;
    }
    return false;
}

// A function to map a command to each listed control the config leaves free, unless the
// config already maps the command somewhere itself
static void add_default_controls(const char *cmd, const char *const *labels, size_t count)
{
    if (gamepad_command_mapped(cmd))
        return;
    for (size_t i = 0; i < count; i++) {
        if (!gamepad_control_mapped(labels[i]))
            add_gamepad_control(labels[i], cmd);
    }
}

// A function to give Up and Down default gamepad controls. Configs written before grids
// existed map nothing to :up or :down, and a grid is unusable without them.
void add_default_gamepad_controls()
{
    static const char *const up[] = { SETTING_GAMEPAD_BUTTON_DPAD_UP, SETTING_GAMEPAD_LSTICK_YM };
    static const char *const down[] = { SETTING_GAMEPAD_BUTTON_DPAD_DOWN, SETTING_GAMEPAD_LSTICK_YP };
    add_default_controls(SCMD_UP, up, sizeof(up) / sizeof(up[0]));
    add_default_controls(SCMD_DOWN, down, sizeof(down) / sizeof(down[0]));
}
```

In `src/util.h`, after `void build_menu_items(void);`, add:

```c
void add_default_gamepad_controls(void);
```

In `src/launcher.c` `main`, after `build_menu_items();`, add:

```c
    if (config.gamepad_enabled)
        add_default_gamepad_controls();
```

- [ ] **Step 4: Build and test**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: the build succeeds and the tests pass.

- [ ] **Step 5: Write the gamepad fixtures**

Create these in `C:/Users/jscha/ClaudeScratch/streamflex-fixtures/`, each being `<common>` from Task 5 plus the lines shown.

`c-pad-old.ini`:

```ini
[Gamepad]
Enabled=true
ButtonDPadLeft=:left
ButtonDPadRight=:right
ButtonA=:select

[Main]
Entry1=Kodi;C:/Users/jscha/source/repos/streamflex/assets/icons/kodi.png;:quit
```

`d-pad-reused.ini`:

```ini
[Gamepad]
Enabled=true
ButtonDPadUp=:home
ButtonDPadLeft=:left
ButtonDPadRight=:right

[Main]
Entry1=Kodi;C:/Users/jscha/source/repos/streamflex/assets/icons/kodi.png;:quit
```

`e-pad-own-up.ini`:

```ini
[Gamepad]
Enabled=true
ButtonY=:up
ButtonDPadLeft=:left
ButtonDPadRight=:right

[Main]
Entry1=Kodi;C:/Users/jscha/source/repos/streamflex/assets/icons/kodi.png;:quit
```

- [ ] **Step 6: Run the gamepad fixtures and check the mappings**

Warn the user about the flashing window first.

```powershell
$exe = "C:/Users/jscha/source/repos/streamflex/build/Release/streamflex.exe"
$log = "C:/Users/jscha/source/repos/streamflex/build/Release/streamflex.log"
foreach ($f in "c-pad-old", "d-pad-reused", "e-pad-own-up") {
    $p = Start-Process -FilePath $exe -ArgumentList "-c", "C:/Users/jscha/ClaudeScratch/streamflex-fixtures/$f.ini", "-d" -Wait -PassThru
    "== $f (exit $($p.ExitCode))"
    Select-String -Path $log -Pattern '^.*(ButtonDPad(Up|Down)|LStickY[-+]|ButtonY)\s+:' | ForEach-Object { "  $($_.Line)" }
}
```

Expected:
- **`c-pad-old`:** `ButtonDPadUp :up`, `LStickY- :up`, `ButtonDPadDown :down` and `LStickY+ :down`.
- **`d-pad-reused`:** `ButtonDPadUp :home` (kept), `LStickY- :up`, `ButtonDPadDown :down` and `LStickY+ :down`. There is no second `ButtonDPadUp` line.
- **`e-pad-own-up`:** `ButtonY :up`, with **no** default `:up` lines, plus `ButtonDPadDown :down` and `LStickY+ :down`.

- [ ] **Step 7: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add -- src/launcher.h src/launcher.c src/util.c src/util.h
git -C C:/Users/jscha/source/repos/streamflex commit -m "feat: Up and Down move between grid rows (keys, :up/:down, gamepad defaults)"
```

---

### Task 7: Documentation, sample config, changelog

**Files:**
- Modify: `docs/configuration.md`, `config/config.ini.in`, `CHANGELOG.md`, `CONTRIBUTING.md`

- [ ] **Step 1: Sample config**

In `config/config.ini.in`, replace `@SETTING_MAX_BUTTONS@=@DEFAULT_MAX_BUTTONS@` with:

```ini
@SETTING_ROWS@=@DEFAULT_ROWS@
@SETTING_COLUMNS@=@DEFAULT_MAX_BUTTONS@
```

Then replace these four commented lines:

```ini
#@SETTING_GAMEPAD_LSTICK_YM@=
#@SETTING_GAMEPAD_LSTICK_YP@=
```

```ini
#@SETTING_GAMEPAD_BUTTON_DPAD_UP@=
#@SETTING_GAMEPAD_BUTTON_DPAD_DOWN@=
```

with:

```ini
@SETTING_GAMEPAD_LSTICK_YM@=:up
@SETTING_GAMEPAD_LSTICK_YP@=:down
```

```ini
@SETTING_GAMEPAD_BUTTON_DPAD_UP@=:up
@SETTING_GAMEPAD_BUTTON_DPAD_DOWN@=:down
```

- [ ] **Step 2: The Layout section of `docs/configuration.md`**

Replace everything from `#### Layout` up to (not including) `#### Titles` with:

````markdown
#### Layout
The settings in this section define the geometric layout of the launcher. A menu shows its buttons in a grid of `Rows` × `Columns`, and the buttons are sized to fill it. Any menu can override these settings; see [Menu Layouts](#menu-layouts).

- [Rows](#rows)
- [Columns](#columns)
- [IconSize](#iconsize)
- [IconSpacing](#iconspacing)
- [VCenter](#vcenter)

##### Rows
The number of rows of buttons shown at once. With 1 row the menu is a strip, which slides one button at a time when you move past its edge. With 2 or more the menu is a grid: Left and Right stop at the end of a row, Up and Down move between rows, and the grid scrolls one row at a time. See [Moving Around](#moving-around).

Default: 1

##### Columns
The number of buttons in each row. The buttons are sized so that `Columns` of them fit across the screen and `Rows` of them fit down it, titles included. For a single row, 3-5 is sensible for a typical TV at a typical viewing distance.

`MaxButtons`, the older name for this setting, is still accepted. If both are set, `Columns` is used.

Default: 4

##### IconSize
The largest size of a button, in pixels. Buttons are sized to fill the grid but never grow past this. Leave it out to let them grow as large as the grid allows. An icon image that is not square is stretched to fit. SVG icons are drawn at the button's size, so they stay sharp at any size.

Default: none (the sample config sets 256)

##### IconSpacing
The gap between buttons, across and down, in pixels or percent of the screen width.

Default: 5%

##### VCenter
The vertical center of the buttons, in percent of the screen height. A value of 50% centers them halfway down the screen; a higher value lowers them and a lower value raises them. The rows that have buttons are centered on this line, but a tall grid is kept on the screen and below the clock.

Default: 50%
````

- [ ] **Step 3: Menu Layouts and Moving Around**

In `docs/configuration.md`, directly before `### Selected Icon Overrides`, add:

````markdown
### Menu Layouts
A menu can set its own `Rows`, `Columns` and `IconSize`, which override the [Layout](#layout) settings for that menu only. For example, big buttons on the main menu and a denser grid for games:
```ini
[Main]
Entry1=Games;C:\Pictures\Icons\games.png;:submenu Games

[Games]
Rows=3
Columns=6
Entry1=...
```
These three names count as layout settings only when their value is a number. A key with one of these names whose value is an entry (`title;icon_path;command`) is still read as an entry, and the log notes it.

### Moving Around
- **A one-row menu** is a strip. Left and Right move along it, and it slides one button at a time at its edges. With `WrapEntries`, moving past the last button selects the first, and the other way round.
- **A menu with two or more rows** is a grid.
  - Left and Right move within a row and stop at its ends. With `WrapEntries`, they wrap within the row.
  - Up and Down move between rows, keeping the column. Moving down into a shorter last row lands on its last button.
  - The grid scrolls one row at a time. With `WrapEntries`, moving down from the last row goes to the first, and up from the first goes to the last.
- Each menu remembers its selected button and scroll position when you come back to it, unless `ResetOnBack` is set.
````

- [ ] **Step 4: Special commands, hotkeys, gamepad**

In `docs/configuration.md`, after the `#### :right` entry (`Move the highlight cursor right.`), add:

```markdown

#### :up
Move the highlight cursor up one row. Only a menu with two or more [Rows](#rows) has rows to move between.

#### :down
Move the highlight cursor down one row.
```

In the Hotkeys section, replace the sentence:

```markdown
Any key can be set as a hotkey, except keys that are reserved for the default controls: the left and right arrow keys, enter/return, and backspace.
```

with:

```markdown
Any key can be set as a hotkey, except keys that are reserved for the default controls: the left and right arrow keys, enter/return, and backspace. The up and down arrow keys move between rows of a grid, unless a hotkey is bound to them, in which case the hotkey is used.
```

In the Gamepad Controls section, replace the paragraph that starts `The default controls in StreamFlex allow the user to move the highlight cursor left and right` with:

```markdown
The default controls in StreamFlex allow the user to move the highlight cursor with the left stick or the DPad, select an entry by pressing A, and go back to the previous menu by pressing B. These controls are simple and will suffice for the vast majority of use cases.

Up and down have defaults of their own. If your config maps nothing to `:up` or `:down`, the DPad's up and down buttons and the left stick's vertical axis run them, unless your config already uses those controls for something else. A config written before grids existed can still move between rows.
```

- [ ] **Step 5: CHANGELOG**

In `CHANGELOG.md`, under `## [Unreleased]`, add:

```markdown
### Added
- **Menus can show several rows of buttons.** `Rows` sets how many rows are visible at once. With two or more, a menu is a grid: Left and Right stop at the end of a row, Up and Down move between rows, and the grid scrolls one row at a time. A single row still scrolls sideways.
- **Buttons are sized to fill the grid.** Choose its shape with `Rows` and `Columns`, and the buttons scale to fit the screen, titles included.
- **Each menu can have its own layout.** `Rows`, `Columns` and `IconSize` in a menu's section override the `[Layout]` settings for that menu.
- The `:up` and `:down` special commands. The Up and Down arrow keys also move between rows; a hotkey already bound to Up or Down keeps working.
- Gamepads get Up and Down by default. When the config maps nothing to `:up` or `:down`, the D-pad and the left stick's vertical axis run them, unless the config uses those controls for something else.
- Unit tests for the layout logic, run by CTest in CI.

### Changed
- **`MaxButtons` is now `Columns`.** The old name still works; if both are set, `Columns` wins.
- **`IconSize` is now the largest a button may grow**, not a fixed size. Menus that set it look the same as before. A config with no `IconSize` line now gets buttons that grow to fill the screen, where it used to get 256 px.
- **A single row with more buttons than fit now slides one button at a time**, instead of flipping to the next page.
- SVG icons are drawn at the button's size, so they stay sharp at any size.
```

- [ ] **Step 6: CONTRIBUTING**

In `CONTRIBUTING.md`, after the `design/` line of the structure block, add:

```
tests/               Unit tests (CTest); run them with ctest after building
```

At the end of the Building section's paragraph, add this sentence:

```markdown
After building, run the unit tests with `ctest --test-dir build -C Release --output-on-failure`.
```

- [ ] **Step 7: Check the generated sample config**

Build (`cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release`, with the PATH line first). Then open `C:/Users/jscha/source/repos/streamflex/build/config.ini` and confirm it contains `Rows=1`, `Columns=4`, `LStickY-=:up`, `LStickY+=:down`, `ButtonDPadUp=:up` and `ButtonDPadDown=:down`.

- [ ] **Step 8: Commit**

```powershell
git -C C:/Users/jscha/source/repos/streamflex add -- docs/configuration.md config/config.ini.in CHANGELOG.md CONTRIBUTING.md
git -C C:/Users/jscha/source/repos/streamflex commit -m "docs: grids, per-menu layouts, Up/Down and the new sample config"
```

---

### Task 8: Verification and the pull request

- [ ] **Step 1: Clean Debug and Release builds, and the tests**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Debug
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target package
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: both builds succeed, `streamflex-0.1.3-win64.zip` is regenerated, and the tests pass.

- [ ] **Step 2: Re-run every fixture**

Re-run the fixture loops from Task 4 Step 10, Task 5 Step 12 and Task 6 Step 6, and confirm the same expected lines.

- [ ] **Step 3: Hands-on check with the user**

Only the user can do this: it needs the TV, remote and gamepad. Give the user the build path (`build/Release/streamflex.exe`) and this list, and wait for their results:
1. The default sample config (`build/config.ini`): it looks the same as 0.1.3, and a menu with more than 4 buttons slides one button at a time.
2. A `2×4` main menu: Left and Right stop at the row edges; Up and Down move rows; the arrows appear top and bottom only when more rows exist.
3. A `3×6` submenu, then Back: each menu is at its own size, the highlight fits, and the parent keeps its selection.
4. A gamepad with a config that maps only Left/Right: Up and Down still work.
5. A hotkey bound to the Up arrow still runs its command.
6. With the clock enabled, a tall grid stays below the clock.

- [ ] **Step 4: Push and open the PR**

```powershell
git -C C:/Users/jscha/source/repos/streamflex push -u origin feat/grid-layout
```

Write the PR body to a scratch file and pass it with `--body-file`. The body covers the Goal, the two behavior changes for existing configs, the test counts from Step 1, the fixture results, and the user's hands-on results. Then:

```powershell
gh pr create -R bilbospocketses/streamflex --base master --head feat/grid-layout --title "feat: multi-row grids and button sizing" --body-file <scratch file>
```

- [ ] **Step 5: CI green, then merge**

Wait for every check to read pass or skipping, including the new `Unit tests` step on Windows and the `ctest` lines in the Debian and Raspberry Pi logs. Then:

```powershell
gh pr merge <N> -R bilbospocketses/streamflex --squash --delete-branch
```

Confirm `state` reads `MERGED`. Then switch the local checkout back to `master` and pull.
