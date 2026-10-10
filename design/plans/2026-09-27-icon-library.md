# Built-in Icon Library Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a built-in icon library — 33 streaming/app services' own icons and 36 generic icons on one shared outline — that a menu entry names with a bare word (`Entry1=Netflix;netflix;...`).

**Architecture:** Icon files live in `assets/icons/library/` with an `icons.ini` manifest. A new pure C module, `src/library.c` (inih + libc, no SDL), reads the manifest. `resolve_library_icons()` in `src/util.c` runs once at startup and rewrites each bare-name entry's `icon_path` to the library file. Everything downstream (sizing, SVG rasterizing, the grid) is unchanged. Python tools in `branding/library/` generate the generic SVGs, the manifest and the brand NOTICE; they import brand art, check the library in CI, and build the docs gallery.

**Tech Stack:** C (SDL2 launcher, inih, nanosvg), CMake/CTest, Python 3 (standard library; Pillow 11.0.0 for the brand-image tools only), GitHub Actions, Jekyll (docs site).

**Spec:** `design/specs/2026-09-27-icon-library-design.md` — the binding authority. Read it before starting; conflicts resolve against it.

## Global Constraints

- **Icon names:** 1-32 characters from `a-z`, `0-9`, `-`; unique across generic and brand icons. Anything else in an entry's icon field is a file path, exactly as today.
- **`src/library.c` stays pure:** inih and the C library only — no SDL, no `util.c`, no `debug.c` — so it is unit-tested without SDL, like `layout.c`.
- **Shared outline:** a rounded square filling the canvas, corner radius **22%** of the side (the app icon's plate, 54 of 244). Generic SVGs use `viewBox 0 0 512 512`, `rx="112.64"`.
- **Plate colors:** every colored plate has CIE L* = 52. Media hue slots `h = 25 + k·360/13`, chroma `floor(0.85 × max in-gamut chroma at L* = 52)` found in 0.5 steps. Pinned results: movies `#E7364B`, news `#C85D21`, kids `#9E7522`, sports `#7A8223`, music `#328E23`, podcasts `#308B69`, audiobooks `#338984`, photos `#358696`, tv-shows `#3784A9`, live-tv `#3A7EC9`, games `#7B68EA`, emulators `#CF37C7`, radio `#DF3784`; system `#5C646D` = LCh(42, 6, 260); general `#07606C` (app icon `TEAL_BG`); devices `#4D7189` = LCh(46, 18, 250). Every plate ≥ 3:1 contrast against white.
- **Glyphs:** Material Symbols Rounded filled, npm `@material-symbols/svg-500` **0.47.5** (Apache-2.0), vendored in `branding/library/glyphs/`; 960-unit em box scaled to **60%** of the plate, centered, times a per-glyph trim (start 1.0).
- **Brand art:** RGBA PNG, square, **512 to 1024 px** (never upscaled; larger art stored at 1024), transparent outside the outline and opaque inside it. Recorded in `branding/library/brands.ini` with `source`, `sha256`, `size`.
- **Library search order** (same as the bundled font): `<exe folder>/assets/icons/library`, then `@CMAKE_INSTALL_PREFIX@/share/streamflex/assets/icons/library` on Linux or `.\assets\icons\library` on Windows.
- **Unknown name → the `apps` icon** plus one log line. **Old Numix path that no longer exists → its library equivalent** (`system.png` → `settings`) plus a note.
- **Specs and plans live in `design/`**, never `docs/` (the published site).
- **Repo rules:** every git command is `git -C C:/Users/jscha/source/repos/streamflex ...`; never `cd`. Implementation happens on a fresh branch (below), squash-merged. No AI attribution in commits or PRs. Stage named files only.
- **Local builds on Windows** need, in every PowerShell session: `$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"`. `ctest` missing from PATH is a CommandNotFound that sets **no** exit code: always confirm the `100% tests passed` line, never only `$LASTEXITCODE`.

## Before Task 1

This plan and its spec are on branch `design/icon-library-spec` (PR #26). Merge PR #26 first (squash), then cut the implementation branch from the updated `master`:

```powershell
pwsh -NoProfile -File C:/Users/jscha/.claude/scripts/git-new-branch.ps1 -Repo C:/Users/jscha/source/repos/streamflex -Branch feat/icon-library
```

## Review Focus

1. **Linux installed layout.** In a `.deb` the executable is `/usr/bin/streamflex` and the library must be at `/usr/share/streamflex/assets/icons/library/icons.ini` (the second search location); nothing in CI runs an installed binary. A person expects library icons on a Debian/Pi install. **Task 6** adds a CI step asserting the path inside the built `.deb`.
2. **Windows zip layout.** `assets/icons/library/icons.ini` must sit in the same folder as `streamflex.exe` inside the zip, or every library icon falls back to nothing. **Task 6** adds a CI step asserting it.
3. **A name typed in the wrong case** (`Entry1=Netflix;Netflix;...`). It is not a valid name, so it is a path, the file is missing, and the button would be blank. A person expects it to work or to be told. **Task 6** rescues it (lowercase copy is a library name → use it, log a note) and pins it with a fixture.
4. **A quoted icon field** (`Entry1=Movies;"movies";...`). `clean_path()` strips the quotes at parse time, so it must still resolve as a name. **Task 6** fixture pins it.
5. **Transparent (chroma-key) background mode.** Library icons have anti-aliased edges, and that mode needs fully opaque or fully transparent pixels, so a faint fringe can show. Not a crash; a person expects to be warned. **Task 7** documents it in the Transparent section.

---

### Task 1: The pure library module

**Files:**
- Create: `src/library.h`, `src/library.c`
- Create: `tests/test_library.c`
- Create fixtures: `tests/fixtures/library/good/icons.ini`, `tests/fixtures/library/good/brands/netflix.png`, `tests/fixtures/library/good/generic/movies.svg`, `tests/fixtures/library/good/generic/apps.svg`, `tests/fixtures/library/bad/icons.ini`, `tests/fixtures/library/bad/generic/ok.svg`, `tests/fixtures/library/bad/generic/other.svg`, `tests/fixtures/library/outside.svg`, `tests/fixtures/library/none/.gitkeep`
- Modify: `tests/CMakeLists.txt`, `src/CMakeLists.txt`

**Interfaces:**
- Consumes: inih (`ini_parse_file`), already a dependency of the launcher.
- Produces (`src/library.h`), used by Tasks 2 and 6:

```c
#define LIBRARY_MANIFEST "icons.ini"
#define LIBRARY_NAME_MAX 32
#define LIBRARY_FALLBACK_ICON "apps"
typedef void (*LibraryWarn)(const char *message);
void        library_set_warn(LibraryWarn function);   // NULL = silent
bool        library_is_name(const char *field);
int         library_load(const char *root);           // icon count, or -1 when icons.ini cannot be opened
const char *library_lookup(const char *name);         // full path, or NULL
const char *library_legacy_name(const char *path);    // "kodi" for .../kodi.png etc., else NULL
void        library_free(void);
```

`library_set_warn()` is an addition to the spec's API. It is how a pure module reports problems without calling the launcher's logger. The spec's "logs" become "calls the warn function", and the launcher's function writes to the log.

- [ ] **Step 1: Write the fixtures**

`tests/fixtures/library/good/icons.ini`:

```ini
; A valid manifest: three icons, with the keys the real one carries
[netflix]
title   = Netflix
group   = video
file    = brands/netflix.png
owner   = Netflix, Inc.
android = com.netflix.ninja

[movies]
title = Movies
group = media
file  = generic/movies.svg

[apps]
title = Apps
group = general
file  = generic/apps.svg
```

`tests/fixtures/library/bad/icons.ini`:

```ini
; Every way a section can be wrong. Only dup, twice, unknown and ok survive loading.
[dup]
file = generic/ok.svg

[nofile]
title = No file

[abs]
file = /etc/hosts

[abs-win]
file = C:\Windows\win.ini

[escape]
file = ../outside.svg

[missing]
file = generic/missing.svg

[Bad_Name]
file = generic/ok.svg

[twice]
file = generic/ok.svg
file = generic/other.svg

[unknown]
file = generic/ok.svg
extra = ignored

[dup]
file = generic/other.svg

[ok]
file = generic/ok.svg
```

Each of these files contains the single line `fixture`: `good/brands/netflix.png`, `good/generic/movies.svg`, `good/generic/apps.svg`, `bad/generic/ok.svg`, `bad/generic/other.svg` and `outside.svg`. The last one sits in `tests/fixtures/library/`, so `bad/../outside.svg` really exists, and `[escape]` must be refused by rule rather than because the file is missing. `none/.gitkeep` is empty: `none/` has no manifest.

- [ ] **Step 2: Write the failing test**

`tests/test_library.c`:

```c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "check.h"
#include "library.h"

static int warnings = 0;

// A function to count the warnings the library reports, printing each so a failure is readable
static void count_warning(const char *message)
{
    warnings++;
    printf("  warning: %s\n", message);
}

static bool ends_with(const char *string, const char *suffix)
{
    if (string == NULL)
        return false;
    size_t a = strlen(string), b = strlen(suffix);
    return a >= b && !strcmp(string + a - b, suffix);
}

// A function to check that library_legacy_name maps a path to the expected name, or to NULL
static bool legacy_is(const char *path, const char *expected)
{
    const char *name = library_legacy_name(path);
    if (expected == NULL)
        return name == NULL;
    return name != NULL && !strcmp(name, expected);
}

static void test_is_name(void)
{
    CHECK(library_is_name("netflix"));
    CHECK(library_is_name("tv-shows"));
    CHECK(library_is_name("a"));
    CHECK(library_is_name("abc123"));
    CHECK(library_is_name("abcdefghijklmnopqrstuvwxyz012345"));   // 32 characters
    CHECK(!library_is_name("abcdefghijklmnopqrstuvwxyz0123456")); // 33
    CHECK(!library_is_name(NULL));
    CHECK(!library_is_name(""));
    CHECK(!library_is_name("Netflix"));
    CHECK(!library_is_name("netflix.png"));
    CHECK(!library_is_name("icons/netflix"));
    CHECK(!library_is_name("icons\\netflix"));
    CHECK(!library_is_name("net flix"));
    CHECK(!library_is_name("net_flix"));
    CHECK(!library_is_name("./kodi"));
}

static void test_load_good(void)
{
    const char *root = LIBRARY_FIXTURES "/good";
    warnings = 0;
    CHECK_INT(library_load(root), 3);
    CHECK_INT(warnings, 0);
    const char *path = library_lookup("netflix");
    CHECK(ends_with(path, "brands/netflix.png"));
    CHECK(path != NULL && !strncmp(path, root, strlen(root)));
    CHECK(ends_with(library_lookup("movies"), "generic/movies.svg"));
    CHECK(ends_with(library_lookup("apps"), "generic/apps.svg"));
    CHECK(library_lookup("nope") == NULL);
    CHECK(library_lookup("Netflix") == NULL);
    CHECK(library_lookup(NULL) == NULL);
}

static void test_load_bad(void)
{
    warnings = 0;
    CHECK_INT(library_load(LIBRARY_FIXTURES "/bad"), 4);
    CHECK_INT(warnings, 8);
    CHECK(ends_with(library_lookup("dup"), "generic/ok.svg"));     // the first [dup] wins
    CHECK(ends_with(library_lookup("twice"), "generic/ok.svg"));   // the first 'file' wins
    CHECK(ends_with(library_lookup("unknown"), "generic/ok.svg")); // unknown keys are ignored
    CHECK(ends_with(library_lookup("ok"), "generic/ok.svg"));
    CHECK(library_lookup("nofile") == NULL);
    CHECK(library_lookup("abs") == NULL);
    CHECK(library_lookup("abs-win") == NULL);
    CHECK(library_lookup("escape") == NULL);   // ../outside.svg exists and is still refused
    CHECK(library_lookup("missing") == NULL);
    CHECK(library_lookup("Bad_Name") == NULL);
}

static void test_load_none(void)
{
    warnings = 0;
    CHECK_INT(library_load(LIBRARY_FIXTURES "/none"), -1);
    CHECK_INT(warnings, 1);
    CHECK(library_lookup("netflix") == NULL);
}

static void test_reload_and_free(void)
{
    library_load(LIBRARY_FIXTURES "/bad");
    CHECK_INT(library_load(LIBRARY_FIXTURES "/good"), 3);
    CHECK(library_lookup("dup") == NULL);   // nothing survives from the previous load
    library_free();
    CHECK(library_lookup("netflix") == NULL);
    library_free();                          // freeing twice is harmless
}

static void test_legacy(void)
{
    CHECK(legacy_is("C:\\StreamFlex\\assets\\icons\\kodi.png", "kodi"));
    CHECK(legacy_is("/usr/share/streamflex/assets/icons/plex.png", "plex"));
    CHECK(legacy_is("./assets/icons/steam.png", "steam"));
    CHECK(legacy_is("retroarch.png", "retroarch"));
    CHECK(legacy_is("/x/system.png", "settings"));
    CHECK(legacy_is("/x/restart.png", "restart"));
    CHECK(legacy_is("/x/SLEEP.PNG", "sleep"));
    CHECK(legacy_is("/x/netflix.png", NULL));
    CHECK(legacy_is("/x/kodi.svg", NULL));
    CHECK(legacy_is("/x/kodi.png.bak", NULL));
    CHECK(legacy_is("", NULL));
    CHECK(legacy_is(NULL, NULL));
}

int main(void)
{
    library_set_warn(count_warning);
    test_is_name();
    test_load_good();
    test_load_bad();
    test_load_none();
    test_reload_and_free();
    test_legacy();
    return check_report();
}
```

In `tests/CMakeLists.txt`, add below the `test_utf8` block:

```cmake
# A function to link inih the way the launcher does
function(link_inih target)
  if (UNIX)
    target_link_libraries(${target} PkgConfig::INIH)
  else ()
    target_link_libraries(${target} ${INIH})
    target_include_directories(${target} PRIVATE ${INIH_INCLUDE_DIR})
  endif ()
endfunction()

# Unit tests for the icon library's manifest reader. library.c must stay free of SDL, so it links with inih alone.
add_executable(test_library test_library.c "${PROJECT_SOURCE_DIR}/src/library.c")
target_include_directories(test_library PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(test_library PRIVATE LIBRARY_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/library")
link_inih(test_library)
add_test(NAME library COMMAND test_library)
```

- [ ] **Step 3: Run it to confirm it fails**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release --target test_library
```

Expected: the build FAILS with `library.h` / `library.c` not found. That is the red state for new code.

- [ ] **Step 4: Write `src/library.h`**

```c
// The built-in icon library: a manifest, icons.ini, whose sections name icons and whose 'file' keys give
// each one's path inside the library folder. Pure: inih and the C library only, so it is unit-tested
// without SDL.
#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdbool.h>

#define LIBRARY_MANIFEST "icons.ini"
#define LIBRARY_NAME_MAX 32
#define LIBRARY_FALLBACK_ICON "apps"

// Receives one message per problem found while loading
typedef void (*LibraryWarn)(const char *message);

void        library_set_warn(LibraryWarn function);
bool        library_is_name(const char *field);
int         library_load(const char *root);
const char *library_lookup(const char *name);
const char *library_legacy_name(const char *path);
void        library_free(void);

#endif
```

- [ ] **Step 5: Write `src/library.c`**

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <ini.h>
#include "library.h"

typedef struct {
    char name[LIBRARY_NAME_MAX + 1];
    char *file;   // as written in the manifest
    char *path;   // the full path once the file is checked; NULL if the icon was rejected
} LibraryIcon;

static LibraryIcon *icons = NULL;
static int icon_count = 0;
static int icon_capacity = 0;
static LibraryWarn warn_function = NULL;
static char *parsing = NULL;   // the manifest section being read
static int target = -1;        // its index in icons, or -1 while its keys are skipped

// A function to format a message and pass it to the warn function, if one is set
static void warn(const char *format, ...)
{
    if (warn_function == NULL)
        return;
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    warn_function(message);
}

void library_set_warn(LibraryWarn function)
{
    warn_function = function;
}

// A function to tell a library icon name (1-32 of a-z, 0-9 and '-') from a file path
bool library_is_name(const char *field)
{
    if (field == NULL)
        return false;
    size_t length = strlen(field);
    if (length == 0 || length > LIBRARY_NAME_MAX)
        return false;
    for (size_t i = 0; i < length; i++) {
        char c = field[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    }
    return true;
}

static int find_icon(const char *name)
{
    for (int i = 0; i < icon_count; i++) {
        if (!strcmp(icons[i].name, name))
            return i;
    }
    return -1;
}

static int add_icon(const char *name)
{
    if (icon_count == icon_capacity) {
        int capacity = icon_capacity ? icon_capacity * 2 : 64;
        LibraryIcon *grown = realloc(icons, (size_t) capacity * sizeof(LibraryIcon));
        if (grown == NULL)
            return -1;
        icons = grown;
        icon_capacity = capacity;
    }
    LibraryIcon *icon = &icons[icon_count];
    snprintf(icon->name, sizeof(icon->name), "%s", name);
    icon->file = NULL;
    icon->path = NULL;
    return icon_count++;
}

// The inih handler. inih reports keys, not section headers, so a change of section name marks a new
// section. A repeated section keeps the first; a repeated 'file' keeps the first.
static int handler(void *user, const char *section, const char *key, const char *value)
{
    (void) user;
    if (parsing == NULL || strcmp(parsing, section)) {
        free(parsing);
        parsing = strdup(section);
        target = -1;
        if (!library_is_name(section))
            warn("Icon library: [%s] is not a valid icon name (a-z, 0-9 and '-', up to %i characters), skipping it",
                section, LIBRARY_NAME_MAX);
        else if (find_icon(section) >= 0)
            warn("Icon library: [%s] appears twice, keeping the first", section);
        else
            target = add_icon(section);
    }
    if (target >= 0 && !strcmp(key, "file")) {
        if (icons[target].file != NULL)
            warn("Icon library: [%s] has 'file' twice, keeping the first", section);
        else
            icons[target].file = strdup(value);
    }
    return 1;
}

// A function to check that a manifest path stays inside the library: relative, with no '..' part
static bool inside_library(const char *file)
{
    if (file[0] == '\0' || file[0] == '/' || file[0] == '\\')
        return false;
    if (isalpha((unsigned char) file[0]) && file[1] == ':')
        return false;
    const char *part = file;
    while (*part != '\0') {
        size_t length = strcspn(part, "/\\");
        if (length == 2 && part[0] == '.' && part[1] == '.')
            return false;
        part += length;
        if (*part != '\0')
            part++;
    }
    return true;
}

// A function to read root/icons.ini, keeping every icon whose file is inside the library and exists
int library_load(const char *root)
{
    library_free();
    size_t root_length = strlen(root);
    const char *separator = (root_length > 0 && (root[root_length - 1] == '/' || root[root_length - 1] == '\\'))
                            ? "" : "/";
    size_t manifest_size = root_length + 1 + strlen(LIBRARY_MANIFEST) + 1;
    char *manifest = malloc(manifest_size);
    if (manifest == NULL)
        return -1;
    snprintf(manifest, manifest_size, "%s%s%s", root, separator, LIBRARY_MANIFEST);
    FILE *file = fopen(manifest, "r");
    if (file == NULL) {
        warn("Icon library: could not open %s", manifest);
        free(manifest);
        return -1;
    }
    int error = ini_parse_file(file, handler, NULL);
    fclose(file);
    if (error > 0)
        warn("Icon library: %s line %i could not be read, skipping it", manifest, error);
    else if (error < 0)
        warn("Icon library: could not read %s", manifest);
    free(manifest);
    free(parsing);
    parsing = NULL;
    target = -1;

    int count = 0;
    for (int i = 0; i < icon_count; i++) {
        LibraryIcon *icon = &icons[i];
        if (icon->file == NULL) {
            warn("Icon library: [%s] has no 'file', skipping it", icon->name);
            continue;
        }
        if (!inside_library(icon->file)) {
            warn("Icon library: [%s] file '%s' must be a path inside the library, skipping it", icon->name, icon->file);
            continue;
        }
        size_t path_size = root_length + 1 + strlen(icon->file) + 1;
        char *path = malloc(path_size);
        if (path == NULL)
            continue;
        snprintf(path, path_size, "%s%s%s", root, separator, icon->file);
        FILE *probe = fopen(path, "rb");
        if (probe == NULL) {
            warn("Icon library: [%s] file %s does not exist, skipping it", icon->name, path);
            free(path);
            continue;
        }
        fclose(probe);
        icon->path = path;
        count++;
    }
    return count;
}

const char *library_lookup(const char *name)
{
    if (name == NULL)
        return NULL;
    for (int i = 0; i < icon_count; i++) {
        if (icons[i].path != NULL && !strcmp(icons[i].name, name))
            return icons[i].path;
    }
    return NULL;
}

// A function to map the file name of one of the seven icons older versions shipped to its library name
const char *library_legacy_name(const char *path)
{
    static const char *const legacy[][2] = {
        { "kodi.png", "kodi" },       { "plex.png", "plex" },       { "steam.png", "steam" },
        { "retroarch.png", "retroarch" }, { "system.png", "settings" },
        { "restart.png", "restart" }, { "sleep.png", "sleep" },
    };
    if (path == NULL)
        return NULL;
    const char *base = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    char lower[16];
    size_t length = strlen(base);
    if (length == 0 || length >= sizeof(lower))
        return NULL;
    for (size_t i = 0; i <= length; i++)
        lower[i] = (char) tolower((unsigned char) base[i]);
    for (size_t i = 0; i < sizeof(legacy) / sizeof(legacy[0]); i++) {
        if (!strcmp(lower, legacy[i][0]))
            return legacy[i][1];
    }
    return NULL;
}

void library_free(void)
{
    for (int i = 0; i < icon_count; i++) {
        free(icons[i].file);
        free(icons[i].path);
    }
    free(icons);
    icons = NULL;
    icon_count = 0;
    icon_capacity = 0;
    free(parsing);
    parsing = NULL;
    target = -1;
}
```

In `src/CMakeLists.txt`, add `library.c` and `library.h` to `SOURCES`, after `utf8.h`. The launcher compiles the module from now on. Task 6 is what first calls it.

- [ ] **Step 6: Run the tests to confirm they pass**

```powershell
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected: `100% tests passed` (layout, utf8, library). The `library` test prints its eight warnings for the bad manifest and one for the missing one, then `N checks, 0 failed`.

- [ ] **Step 7: Commit**

```powershell
$repo = 'C:/Users/jscha/source/repos/streamflex'
git -C $repo add src/library.h src/library.c src/CMakeLists.txt tests/test_library.c tests/CMakeLists.txt tests/fixtures/library
git -C $repo commit -m "feat(library): pure manifest reader for the built-in icon library, with unit tests"
```

---

### Task 2: The generic set, the manifest and the NOTICE generator

**Files:**
- Create: `branding/library/libtools.py`, `branding/library/build-library.py`, `branding/library/fetch-glyphs.py`, `branding/library/test_build_library.py`, `branding/library/brands.ini`
- Create (vendored by `fetch-glyphs.py`): `branding/library/glyphs/LICENSE`, `branding/library/glyphs/<glyph>-fill.svg` × 36
- Create (generated by `build-library.py`, committed): `assets/icons/library/generic/<name>.svg` × 36, `assets/icons/library/generic/LICENSE-material-symbols.txt`, `assets/icons/library/icons.ini`, `assets/icons/library/brands/NOTICE.md`
- Create: `tests/test_library_svg.c`
- Modify: `tests/CMakeLists.txt`, `.gitignore`

**Interfaces:**
- Consumes: the Task 1 fixtures' manifest format.
- Produces, used by Tasks 3-8:
  - `libtools.py`: `ROOT`, `LIBRARY`, `GLYPHS`, `BRANDS_INI`, `NAME_RE`, `GENERIC_GROUPS`, `BRAND_GROUPS`, `OUTLINE_RADIUS`, `BRAND_MIN`, `BRAND_MAX`, `BRAND_KEYS`, `read_sections(path)`, `write_brands(sections, path)`, `sha256_file(path)`.
  - `outline_mask(size)` is added to `libtools.py` in Task 3.
  - `build-library.py`: `GENERIC` table, `plate_color(group, slot)`, `contrast_with_white(hex)`, `generic_svg(color, paths, trim)`, `build_outputs(brands_ini)`.
  - `build-library.py` also has the CLI `--check`.
- **Ruling (plan vs spec):** the spec names the generator `build-generic.py` and describes a hand-kept `icons.ini`. This plan uses one script, `build-library.py`. It generates `icons.ini` and `NOTICE.md` from two sources: its own `GENERIC` table, and `branding/library/brands.ini`, which is kept by the import tool and by hand. Each fact then lives in exactly one place, and the spec's "re-running reproduces the committed files" check covers the manifest and the NOTICE too. Cost if wrong: one renamed script.

- [ ] **Step 1: Write the failing Python test**

`branding/library/test_build_library.py`:

```python
"""Unit tests for build-library.py: the pinned palette, the contrast rule, and the SVG geometry.
Run: python -m unittest discover -s branding/library -p "test_*.py" -v
"""
import importlib.util
import math
import pathlib
import unittest

HERE = pathlib.Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("build_library", HERE / "build-library.py")
bl = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(bl)

MEDIA = {
    "movies": "#E7364B", "news": "#C85D21", "kids": "#9E7522", "sports": "#7A8223", "music": "#328E23",
    "podcasts": "#308B69", "audiobooks": "#338984", "photos": "#358696", "tv-shows": "#3784A9",
    "live-tv": "#3A7EC9", "games": "#7B68EA", "emulators": "#CF37C7", "radio": "#DF3784",
}


def lightness(hex_color):
    """CIE L* of an sRGB hex color."""
    y = sum(w * c for w, c in zip((0.2126, 0.7152, 0.0722), bl.hex_to_linear(hex_color)))
    f = y ** (1 / 3) if y > 216 / 24389 else (24389 / 27 * y + 16) / 116
    return 116 * f - 16


class Palette(unittest.TestCase):
    def test_media_plates_match_the_spec(self):
        media = [row for row in bl.GENERIC if row[2] == "media"]
        self.assertEqual(len(media), 13)
        for name, _title, group, _glyph, slot, _trim in media:
            self.assertEqual(bl.plate_color(group, slot), MEDIA[name], name)

    def test_one_color_groups_match_the_spec(self):
        self.assertEqual(bl.plate_color("system", None), "#5C646D")
        self.assertEqual(bl.plate_color("general", None), "#07606C")
        self.assertEqual(bl.plate_color("devices", None), "#4D7189")

    def test_media_plates_share_one_lightness(self):
        for hex_color in MEDIA.values():
            self.assertAlmostEqual(lightness(hex_color), 52.0, delta=0.6, msg=hex_color)

    def test_every_plate_keeps_white_readable(self):
        for name, _title, group, _glyph, slot, _trim in bl.GENERIC:
            self.assertGreaterEqual(bl.contrast_with_white(bl.plate_color(group, slot)), 3.0, name)


class Geometry(unittest.TestCase):
    def test_trim_one_centers_the_em_box_at_sixty_percent(self):
        svg = bl.generic_svg("#123456", ["M0 0h1"], 1.0)
        self.assertIn('rx="112.64"', svg)
        self.assertIn('transform="translate(102.400 409.600) scale(0.320000)"', svg)

    def test_trim_scales_about_the_center(self):
        svg = bl.generic_svg("#123456", ["M0 0h1"], 0.5)
        self.assertIn('transform="translate(179.200 332.800) scale(0.160000)"', svg)


class Table(unittest.TestCase):
    def test_names_groups_and_slots(self):
        names = [row[0] for row in bl.GENERIC]
        self.assertEqual(len(names), 36)
        self.assertEqual(len(set(names)), 36)
        for name, _title, group, _glyph, slot, trim in bl.GENERIC:
            self.assertRegex(name, r"^[a-z0-9-]{1,32}$")
            self.assertIn(group, bl.libtools.GENERIC_GROUPS)
            self.assertEqual(slot is None, group != "media", name)
            self.assertTrue(0.5 <= trim <= 1.5, name)
        slots = sorted(row[4] for row in bl.GENERIC if row[4] is not None)
        self.assertEqual(slots, list(range(13)))


if __name__ == "__main__":
    unittest.main()
```

Run: `python -m unittest discover -s C:/Users/jscha/source/repos/streamflex/branding/library -p "test_*.py" -v`
Expected: ERROR, `build-library.py` does not exist.

- [ ] **Step 2: Write `branding/library/libtools.py`**

```python
"""Shared paths and file formats for the icon library tools. Standard library only
(outline_mask, added with the brand tools, imports Pillow when it is called)."""
import hashlib
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
LIBRARY = ROOT / "assets" / "icons" / "library"
GLYPHS = HERE / "glyphs"
BRANDS_INI = HERE / "brands.ini"

NAME_RE = re.compile(r"^[a-z0-9-]{1,32}$")
GENERIC_GROUPS = ("system", "media", "general", "devices")
BRAND_GROUPS = ("video", "free-tv", "servers", "music", "games", "web")
OUTLINE_RADIUS = 0.22           # corner radius as a share of the side: the app icon's plate, 54 of 244
BRAND_MIN, BRAND_MAX = 512, 1024
BRAND_KEYS = ("title", "group", "owner", "android", "source", "art", "fill", "size", "sha256")


def read_sections(path):
    """Read an INI file into [(section, {key: value})] in file order. A repeated section or key is an error."""
    sections, current, seen = [], None, set()
    for number, raw in enumerate(pathlib.Path(path).read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            name = line[1:-1].strip()
            if name in seen:
                raise ValueError(f"{path}:{number}: section [{name}] appears twice")
            seen.add(name)
            current = (name, {})
            sections.append(current)
            continue
        if "=" not in line or current is None:
            raise ValueError(f"{path}:{number}: expected 'key = value' inside a section: {raw!r}")
        key, value = (part.strip() for part in line.split("=", 1))
        if key in current[1]:
            raise ValueError(f"{path}:{number}: key '{key}' appears twice in [{current[0]}]")
        current[1][key] = value
    return sections


def write_brands(sections, path=BRANDS_INI):
    """Write brands.ini with a fixed key order, so hand edits and tool edits never fight over layout."""
    lines = [
        "; Brand icons, one section per icon, in the order the manifest lists them.",
        "; import-brand.py writes source, art, fill, size and sha256; title, group, owner and android are edited by hand.",
        "; After any change, run build-library.py to regenerate assets/icons/library/icons.ini and brands/NOTICE.md.",
    ]
    for name, keys in sections:
        lines += ["", f"[{name}]"]
        lines += [f"{key} = {keys[key]}" for key in BRAND_KEYS if keys.get(key)]
    pathlib.Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def sha256_file(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
```

- [ ] **Step 3: Write `branding/library/build-library.py`**

```python
"""Build the icon library's generated files from their sources.

Sources: the GENERIC table below; glyphs/<glyph>-fill.svg (Material Symbols Rounded, filled, vendored by
fetch-glyphs.py); brands.ini (kept by import-brand.py and by hand).
Outputs, committed because they ship: assets/icons/library/generic/<name>.svg,
generic/LICENSE-material-symbols.txt, icons.ini and brands/NOTICE.md.

  python branding/library/build-library.py            write the outputs
  python branding/library/build-library.py --check    write nothing; exit 1 if any output is stale

Standard library only.
"""
import argparse
import math
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libtools  # noqa: E402

CANVAS = 512
RADIUS = round(CANVAS * libtools.OUTLINE_RADIUS, 2)       # 112.64
GLYPH_BOX = 0.60        # the glyph's 960-unit em box, as a share of the plate side
LIGHTNESS = 52.0        # CIE L* of every media plate
VIVID = 0.85            # media chroma: this share of the hue's sRGB maximum at LIGHTNESS
MIN_CONTRAST = 3.0      # white glyph against its plate
FIXED_LCH = {"system": (42.0, 6.0, 260.0), "devices": (46.0, 18.0, 250.0)}
GENERAL_HEX = "#07606C"  # StreamFlex teal: TEAL_BG in branding/icon/build-svg.py

GENERIC = [
    # name, title, group, Material Symbols glyph, media hue slot (media only), optical trim
    ("power", "Power", "system", "power_settings_new", None, 1.0),
    ("restart", "Restart", "system", "restart_alt", None, 1.0),
    ("sleep", "Sleep", "system", "bedtime", None, 1.0),
    ("quit", "Exit", "system", "logout", None, 1.0),
    ("settings", "Settings", "system", "settings", None, 1.0),
    ("back", "Back", "system", "arrow_back", None, 1.0),
    ("lock", "Lock", "system", "lock", None, 1.0),
    ("user", "Switch user", "system", "switch_account", None, 1.0),
    ("movies", "Movies", "media", "movie", 0, 1.0),
    ("news", "News", "media", "newspaper", 1, 1.0),
    ("kids", "Kids", "media", "toys", 2, 1.0),
    ("sports", "Sports", "media", "sports_soccer", 3, 1.0),
    ("music", "Music", "media", "music_note", 4, 1.0),
    ("podcasts", "Podcasts", "media", "podcasts", 5, 1.0),
    ("audiobooks", "Audiobooks", "media", "headphones", 6, 1.0),
    ("photos", "Photos", "media", "photo", 7, 1.0),
    ("tv-shows", "TV shows", "media", "tv", 8, 1.0),
    ("live-tv", "Live TV", "media", "live_tv", 9, 1.0),
    ("games", "Games", "media", "sports_esports", 10, 1.0),
    ("emulators", "Retro games", "media", "joystick", 11, 1.0),
    ("radio", "Radio", "media", "radio", 12, 1.0),
    ("apps", "Apps", "general", "apps", None, 1.0),
    ("web", "Web", "general", "language", None, 1.0),
    ("folder", "Folder", "general", "folder", None, 1.0),
    ("favorites", "Favorites", "general", "favorite", None, 1.0),
    ("home", "Home", "general", "home", None, 1.0),
    ("search", "Search", "general", "search", None, 1.0),
    ("info", "Info", "general", "info", None, 1.0),
    ("download", "Downloads", "general", "download", None, 1.0),
    ("terminal", "Terminal", "general", "terminal", None, 1.0),
    ("desktop", "Desktop", "general", "desktop_windows", None, 1.0),
    ("display", "Display", "devices", "display_settings", None, 1.0),
    ("audio", "Sound", "devices", "volume_up", None, 1.0),
    ("bluetooth", "Bluetooth", "devices", "bluetooth", None, 1.0),
    ("network", "Network", "devices", "wifi", None, 1.0),
    ("gamepad", "Controllers", "devices", "gamepad", None, 1.0),
]

XN, YN, ZN = 0.95047, 1.0, 1.08883   # D65 white


def lch_to_linear(L, C, h):
    """CIE LCh(ab), D65, to linear sRGB (may fall outside 0..1)."""
    a, b = C * math.cos(math.radians(h)), C * math.sin(math.radians(h))
    fy = (L + 16) / 116
    fx, fz = fy + a / 500, fy - b / 200

    def finv(t):
        return t ** 3 if t ** 3 > 216 / 24389 else (116 * t - 16) / (24389 / 27)

    X, Y, Z = XN * finv(fx), YN * finv(fy), ZN * finv(fz)
    return (3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z,
            -0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z,
            0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z)


def in_gamut(rgb):
    return all(-1e-9 <= c <= 1 + 1e-9 for c in rgb)


def max_chroma(L, h):
    c = 0.0
    while in_gamut(lch_to_linear(L, c + 0.5, h)):
        c += 0.5
    return c


def encode(c):
    c = min(max(c, 0.0), 1.0)
    return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def linear_to_hex(rgb):
    return "#" + "".join(f"{round(encode(x) * 255):02X}" for x in rgb)


def hex_to_linear(hex_color):
    values = [int(hex_color[i:i + 2], 16) / 255 for i in (1, 3, 5)]
    return [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in values]


def contrast_with_white(hex_color):
    luminance = sum(w * c for w, c in zip((0.2126, 0.7152, 0.0722), hex_to_linear(hex_color)))
    return 1.05 / (luminance + 0.05)


def plate_color(group, slot):
    if group == "media":
        h = 25 + slot * 360 / 13
        return linear_to_hex(lch_to_linear(LIGHTNESS, math.floor(max_chroma(LIGHTNESS, h) * VIVID), h))
    if group == "general":
        return GENERAL_HEX
    return linear_to_hex(lch_to_linear(*FIXED_LCH[group]))


def glyph_paths(glyph):
    text = (libtools.GLYPHS / f"{glyph}-fill.svg").read_text(encoding="utf-8")
    if 'viewBox="0 -960 960 960"' not in text:
        raise ValueError(f"{glyph}: expected the Material Symbols view box 0 -960 960 960")
    paths = re.findall(r'<path d="([^"]+)"', text)
    if not paths:
        raise ValueError(f"{glyph}: no <path d=...> found")
    return paths


def generic_svg(color, paths, trim):
    scale = CANVAS * GLYPH_BOX / 960 * trim
    x = (CANVAS - 960 * scale) / 2
    y = x + 960 * scale                  # the glyph's y runs from -960 to 0
    body = "".join(f'<path d="{d}" fill="#FFFFFF"/>' for d in paths)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{CANVAS}" height="{CANVAS}" '
            f'viewBox="0 0 {CANVAS} {CANVAS}">\n'
            f'<rect width="{CANVAS}" height="{CANVAS}" rx="{RADIUS:g}" ry="{RADIUS:g}" fill="{color}"/>\n'
            f'<g transform="translate({x:.3f} {y:.3f}) scale({scale:.6f})">{body}</g>\n'
            f'</svg>\n')


NOTICE_HEAD = """# Brand icons

Every icon in this folder is the trademark and artwork of the company named beside it. StreamFlex includes
them only to identify each service on its launcher button. They are not covered by StreamFlex's GPL-3.0
license.

If you own one of these marks and want it removed, open an issue at
https://github.com/bilbospocketses/streamflex/issues and it will be taken out.

| Icon | Service | Owner | Source |
|---|---|---|---|
"""

MANIFEST_HEAD = [
    "; The StreamFlex icon library manifest. GENERATED by branding/library/build-library.py:",
    "; edit its GENERIC table or branding/library/brands.ini, then re-run it. Do not edit this file.",
    "; One section per icon; the section name is the icon's name.",
]


def build_outputs(brands_ini=libtools.BRANDS_INI):
    """Return ({library-relative path: text}, [errors])."""
    outputs, errors, names = {}, [], set()
    manifest = list(MANIFEST_HEAD)
    for name, title, group, glyph, slot, trim in GENERIC:
        if not libtools.NAME_RE.match(name) or name in names:
            errors.append(f"generic '{name}': invalid or repeated name")
        names.add(name)
        color = plate_color(group, slot)
        if contrast_with_white(color) < MIN_CONTRAST:
            errors.append(f"generic '{name}': plate {color} gives white only {contrast_with_white(color):.2f}:1")
        outputs[f"generic/{name}.svg"] = generic_svg(color, glyph_paths(glyph), trim)
        manifest += ["", f"[{name}]", f"title = {title}", f"group = {group}", f"file = generic/{name}.svg"]

    rows = []
    for name, keys in libtools.read_sections(brands_ini):
        if not libtools.NAME_RE.match(name) or name in names:
            errors.append(f"brand '{name}': invalid name, or it repeats another icon's")
        names.add(name)
        missing = [key for key in ("title", "group", "owner", "source", "size", "sha256") if not keys.get(key)]
        if missing:
            errors.append(f"brand '{name}': brands.ini is missing {', '.join(missing)}")
            continue
        if keys["group"] not in libtools.BRAND_GROUPS:
            errors.append(f"brand '{name}': group '{keys['group']}' is not one of {', '.join(libtools.BRAND_GROUPS)}")
        png = libtools.LIBRARY / "brands" / f"{name}.png"
        if not png.is_file():
            errors.append(f"brand '{name}': {png} is missing; run import-brand.py")
        elif libtools.sha256_file(png) != keys["sha256"]:
            errors.append(f"brand '{name}': {png.name} does not match the sha256 in brands.ini; re-import it")
        manifest += ["", f"[{name}]", f"title = {keys['title']}", f"group = {keys['group']}",
                     f"file = brands/{name}.png", f"owner = {keys['owner']}"]
        if keys.get("android"):
            manifest.append(f"android = {keys['android']}")
        manifest += [f"source = {keys['source']}", f"sha256 = {keys['sha256']}"]
        rows.append(f"| `{name}.png` | {keys['title']} | {keys['owner']} | {keys['source']} |")

    outputs["icons.ini"] = "\n".join(manifest) + "\n"
    outputs["brands/NOTICE.md"] = NOTICE_HEAD + ("\n".join(rows) + "\n" if rows else "| (none yet) | | | |\n")
    outputs["generic/LICENSE-material-symbols.txt"] = (libtools.GLYPHS / "LICENSE").read_text(encoding="utf-8")
    return outputs, errors


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="write nothing; exit 1 if any output is stale")
    args = parser.parse_args()

    outputs, errors = build_outputs()
    if errors:
        print("\n".join(f"error: {e}" for e in errors))
        return 1

    changed = []
    for rel, text in outputs.items():
        path = libtools.LIBRARY / rel
        data = text.encode("utf-8")
        if not path.is_file() or path.read_bytes() != data:
            changed.append(rel)
            if not args.check:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
    for path in sorted((libtools.LIBRARY / "generic").glob("*.svg")):
        if f"generic/{path.name}" not in outputs:
            changed.append(f"generic/{path.name} (not in the GENERIC table)")
            if not args.check:
                path.unlink()

    if args.check:
        if changed:
            print("stale library outputs (run build-library.py):\n  " + "\n  ".join(changed))
            return 1
        print(f"library outputs are current ({len(outputs)} files)")
        return 0
    print(f"updated {len(changed)} of {len(outputs)} library files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Write `branding/library/fetch-glyphs.py` and vendor the glyphs**

```python
"""Vendor the Material Symbols glyphs the generic icons use, and their license, into glyphs/.

  python branding/library/fetch-glyphs.py

Downloads rounded/<glyph>-fill.svg for every glyph in build-library.py's GENERIC table from the pinned npm
package @material-symbols/svg-500 on jsDelivr, plus the package's LICENSE (Apache-2.0). Run it when the
table gains a glyph; the repository keeps the vendored copies so building needs no network.
Standard library only.
"""
import importlib.util
import pathlib
import urllib.request

HERE = pathlib.Path(__file__).resolve().parent
PACKAGE = "https://cdn.jsdelivr.net/npm/@material-symbols/svg-500@0.47.5/"

_spec = importlib.util.spec_from_file_location("build_library", HERE / "build-library.py")
bl = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(bl)


def fetch(url):
    with urllib.request.urlopen(url, timeout=30) as response:
        return response.read()


def main():
    out = HERE / "glyphs"
    out.mkdir(exist_ok=True)
    (out / "LICENSE").write_bytes(fetch(PACKAGE + "LICENSE"))
    glyphs = sorted({row[3] for row in bl.GENERIC})
    for glyph in glyphs:
        data = fetch(PACKAGE + f"rounded/{glyph}-fill.svg")
        if b'viewBox="0 -960 960 960"' not in data:
            raise SystemExit(f"{glyph}: unexpected view box")
        (out / f"{glyph}-fill.svg").write_bytes(data)
    print(f"vendored {len(glyphs)} glyphs and LICENSE into {out}")


if __name__ == "__main__":
    main()
```

Write `branding/library/brands.ini`, which has no brands yet:

```powershell
python -c "import sys; sys.path.insert(0, r'C:/Users/jscha/source/repos/streamflex/branding/library'); import libtools; libtools.write_brands([])"
python C:/Users/jscha/source/repos/streamflex/branding/library/fetch-glyphs.py
```

Expected: `vendored 36 glyphs and LICENSE into ...\branding\library\glyphs`.

Add to `.gitignore`:

```
# Python caches and review output from branding/library tools
branding/library/__pycache__/
branding/library/build/
```

- [ ] **Step 5: Run the Python tests**

Run: `python -m unittest discover -s C:/Users/jscha/source/repos/streamflex/branding/library -p "test_*.py" -v`
Expected: 7 tests, OK. The palette test proves the generator reproduces the spec's 13 pinned colors.

- [ ] **Step 6: Write the failing nanosvg test**

`tests/test_library_svg.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#include <ini.h>
#include "check.h"

// Every generic icon listed in the shipped manifest must parse with nanosvg, the launcher's own SVG
// parser, into a 512 px plate plus a glyph that stays on it. Catches SVG features nanosvg cannot draw.

#define MAX_FILES 128
static char files[MAX_FILES][512];
static int file_count = 0;

static int collect(void *user, const char *section, const char *name, const char *value)
{
    (void) user;
    (void) section;
    size_t length = strlen(value);
    if (!strcmp(name, "file") && length > 4 && !strcmp(value + length - 4, ".svg") && file_count < MAX_FILES) {
        snprintf(files[file_count], sizeof(files[0]), "%s/%s", LIBRARY_ROOT, value);
        file_count++;
    }
    return 1;
}

int main(void)
{
    CHECK_INT(ini_parse(LIBRARY_ROOT "/icons.ini", collect, NULL), 0);
    CHECK(file_count >= 36);
    for (int i = 0; i < file_count; i++) {
        NSVGimage *image = nsvgParseFromFile(files[i], "px", 96.0f);
        CHECK(image != NULL);
        if (image == NULL) {
            fprintf(stderr, "  could not parse %s\n", files[i]);
            continue;
        }
        CHECK((int) image->width == 512 && (int) image->height == 512);
        int shapes = 0;
        float glyph_area = 0.0f;
        for (NSVGshape *shape = image->shapes; shape != NULL; shape = shape->next) {
            shapes++;
            bool inside = shape->bounds[0] >= -0.5f && shape->bounds[1] >= -0.5f
                       && shape->bounds[2] <= 512.5f && shape->bounds[3] <= 512.5f;
            if (!inside)
                fprintf(stderr, "  %s: shape %d leaves the plate\n", files[i], shapes);
            CHECK(inside);
            if (shapes > 1)
                glyph_area += (shape->bounds[2] - shape->bounds[0]) * (shape->bounds[3] - shape->bounds[1]);
        }
        if (shapes < 2)
            fprintf(stderr, "  %s: %d shape(s), expected a plate and a glyph\n", files[i], shapes);
        CHECK(shapes >= 2);
        // The glyph's box should cover a real share of the plate: a misparsed path is tiny or huge
        CHECK(glyph_area > 0.05f * 512 * 512 && glyph_area < 0.60f * 512 * 512);
        nsvgDelete(image);
    }
    return check_report();
}
```

In `tests/CMakeLists.txt`, add after the `test_library` block:

```cmake
# Every generic icon in the shipped library must parse with nanosvg, the launcher's own SVG parser
add_executable(test_library_svg test_library_svg.c)
target_include_directories(test_library_svg PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/external")
target_compile_definitions(test_library_svg PRIVATE LIBRARY_ROOT="${PROJECT_SOURCE_DIR}/assets/icons/library")
link_inih(test_library_svg)
if (UNIX)
  target_link_libraries(test_library_svg m)
endif ()
add_test(NAME library_svg COMMAND test_library_svg)
```

Run: `cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release` then `ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release -R library_svg --output-on-failure`
Expected: FAIL. `ini_parse` returns -1 because `assets/icons/library/icons.ini` does not exist yet, and `file_count` is 0.

- [ ] **Step 7: Generate the library and confirm the tests pass**

```powershell
python C:/Users/jscha/source/repos/streamflex/branding/library/build-library.py
python C:/Users/jscha/source/repos/streamflex/branding/library/build-library.py --check
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected:
- the first run prints `updated 39 of 39 library files`;
- `--check` prints `library outputs are current (39 files)`;
- ctest prints `100% tests passed` (layout, utf8, library, library_svg).

If `library_svg` flags a shape leaving the plate, or a glyph area out of range, nanosvg is misreading that glyph's path. Fix the generator, not the test.

- [ ] **Step 8: Commit**

```powershell
$repo = 'C:/Users/jscha/source/repos/streamflex'
git -C $repo add branding/library/libtools.py branding/library/build-library.py branding/library/fetch-glyphs.py branding/library/test_build_library.py branding/library/brands.ini branding/library/glyphs assets/icons/library tests/test_library_svg.c tests/CMakeLists.txt .gitignore
git -C $repo commit -m "feat(library): 36 generic icons generated from Material Symbols, the manifest and its checks"
```

---

### Task 3: The brand import tool

**Files:**
- Create: `branding/library/import-brand.py`, `branding/library/test_import_brand.py`
- Modify: `branding/library/libtools.py` (add `outline_mask`)

**Interfaces:**
- Consumes: `libtools.read_sections`, `write_brands`, `sha256_file`, `NAME_RE`, `BRAND_MIN`, `BRAND_MAX`, `OUTLINE_RADIUS`.
- Produces:
  - `libtools.outline_mask(size) -> PIL.Image` (mode `L`: 255 inside the outline, 0 outside, box-filtered edge). Also used by Task 5's check.
  - `import_brand(name, image_path, source, art=None, fill=None, library=LIBRARY, brands_ini=BRANDS_INI) -> dict`, which raises `ImportRefused`.
  - The CLI `import-brand.py <name> <image> --source URL [--art URL] [--fill #RRGGBB]`.

- [ ] **Step 1: Write the failing test**

`branding/library/test_import_brand.py`:

```python
"""Unit tests for import-brand.py. Needs Pillow. Run with the other library tests:
python -m unittest discover -s branding/library -p "test_*.py" -v
"""
import importlib.util
import pathlib
import tempfile
import unittest

from PIL import Image, ImageDraw

HERE = pathlib.Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("import_brand", HERE / "import-brand.py")
ib = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ib)


class ImportBrand(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = pathlib.Path(self.tmp.name)
        self.library = self.dir / "library"
        (self.library / "brands").mkdir(parents=True)
        self.brands_ini = self.dir / "brands.ini"

    def tearDown(self):
        self.tmp.cleanup()

    def art(self, size, color=(200, 30, 40, 255), mode="RGBA", round_logo=False):
        width, height = size if isinstance(size, tuple) else (size, size)
        if round_logo:
            image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
            ImageDraw.Draw(image).ellipse((50, 50, width - 51, height - 51), fill=color)
        else:
            image = Image.new(mode, (width, height), color if mode == "RGBA" else color[:3])
        path = self.dir / f"src-{width}x{height}-{mode}-{round_logo}.png"
        image.save(path)
        return path

    def run_import(self, name, path, **kwargs):
        return ib.import_brand(name, path, "https://example.com/listing", library=self.library,
                               brands_ini=self.brands_ini, **kwargs)

    def test_full_bleed_art_is_cut_to_the_outline(self):
        info = self.run_import("netflix", self.art(600))
        self.assertEqual(info["size"], 600)
        with Image.open(self.library / "brands" / "netflix.png") as out:
            self.assertEqual(out.mode, "RGBA")
            self.assertEqual(out.size, (600, 600))
            self.assertEqual(out.getpixel((0, 0))[3], 0)        # corner: outside the outline
            self.assertEqual(out.getpixel((300, 300))[3], 255)  # center: inside
            self.assertEqual(out.getpixel((300, 300))[:3], (200, 30, 40))

    def test_brands_ini_records_provenance_and_keeps_hand_edits(self):
        self.brands_ini.write_text("[netflix]\ntitle = Netflix\ngroup = video\nowner = Netflix, Inc.\n", encoding="utf-8")
        info = self.run_import("netflix", self.art(512), art="https://example.com/art.png")
        keys = dict(ib.libtools.read_sections(self.brands_ini))["netflix"]
        self.assertEqual(keys["title"], "Netflix")
        self.assertEqual(keys["owner"], "Netflix, Inc.")
        self.assertEqual(keys["source"], "https://example.com/listing")
        self.assertEqual(keys["art"], "https://example.com/art.png")
        self.assertEqual(keys["size"], "512")
        self.assertEqual(keys["sha256"], info["sha256"])
        self.assertEqual(keys["sha256"], ib.libtools.sha256_file(self.library / "brands" / "netflix.png"))

    def test_big_art_is_reduced_to_1024(self):
        self.assertEqual(self.run_import("big", self.art(2048))["size"], 1024)

    def test_small_art_is_refused_never_enlarged(self):
        with self.assertRaises(ib.ImportRefused):
            self.run_import("small", self.art(300))
        self.assertFalse((self.library / "brands" / "small.png").exists())

    def test_non_square_art_is_refused(self):
        with self.assertRaises(ib.ImportRefused):
            self.run_import("wide", self.art((600, 500)))

    def test_rgb_art_is_accepted(self):
        self.assertEqual(self.run_import("rgb", self.art(512, mode="RGB"))["size"], 512)

    def test_transparent_art_needs_a_fill(self):
        path = self.art(600, round_logo=True)
        with self.assertRaises(ib.ImportRefused):
            self.run_import("round", path)
        self.run_import("round", path, fill="#112233")
        with Image.open(self.library / "brands" / "round.png") as out:
            self.assertEqual(out.getpixel((300, 20)), (17, 34, 51, 255))  # inside the outline, outside the logo
        self.assertEqual(dict(ib.libtools.read_sections(self.brands_ini))["round"]["fill"], "#112233")

    def test_invalid_name_is_refused(self):
        with self.assertRaises(ib.ImportRefused):
            self.run_import("Net_Flix", self.art(512))


if __name__ == "__main__":
    unittest.main()
```

Run: `python -m unittest discover -s C:/Users/jscha/source/repos/streamflex/branding/library -p "test_*.py" -v`
Expected: ERROR in `test_import_brand`, because `import-brand.py` does not exist. Task 2's tests still pass.

- [ ] **Step 2: Add `outline_mask` to `libtools.py`**

```python
def outline_mask(size, supersample=4):
    """The shared outline at size x size: mode L, 255 inside, 0 outside, a box-filtered edge between.
    Box filtering keeps the interior exactly 255 and the exterior exactly 0. Needs Pillow."""
    from PIL import Image, ImageDraw
    big = size * supersample
    mask = Image.new("L", (big, big), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, big - 1, big - 1), radius=OUTLINE_RADIUS * big, fill=255)
    return mask.resize((size, size), Image.Resampling.BOX)
```

- [ ] **Step 3: Write `branding/library/import-brand.py`**

```python
"""Import one brand icon into the library: check the art, cut it to the shared outline, record provenance.

  python branding/library/import-brand.py <name> <image> --source <page URL> [--art <image URL>] [--fill '#RRGGBB']

- The image must be square and at least 512 px. Art above 1024 px is reduced to 1024; art is never enlarged.
- A palette, grayscale or RGB image is converted to RGBA; an embedded color profile is converted to sRGB.
- Transparent pixels inside the outline (an old round or padded logo) are refused unless --fill names a solid
  color to put behind the art. Choose the art's own background color and say so in the task report.
- Writes assets/icons/library/brands/<name>.png, and records source, art, fill, size and sha256 in
  branding/library/brands.ini. Set title, group, owner and android there by hand, then run build-library.py.
Needs Pillow.
"""
import argparse
import io
import pathlib
import sys

from PIL import Image, ImageChops, ImageCms

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libtools  # noqa: E402


class ImportRefused(Exception):
    pass


def load_rgba(image_path):
    with Image.open(image_path) as source:
        source.load()
        profile = source.info.get("icc_profile")
        image = source.convert("RGBA")
    if profile:
        image = ImageCms.profileToProfile(image, ImageCms.ImageCmsProfile(io.BytesIO(profile)),
                                          ImageCms.createProfile("sRGB"), outputMode="RGBA")
    return image


def import_brand(name, image_path, source, art=None, fill=None, library=libtools.LIBRARY,
                 brands_ini=libtools.BRANDS_INI):
    if not libtools.NAME_RE.match(name):
        raise ImportRefused(f"'{name}' is not a valid icon name (a-z, 0-9 and '-', up to 32 characters)")
    image = load_rgba(image_path)
    width, height = image.size
    if width != height:
        raise ImportRefused(f"{image_path} is {width}x{height}; brand art must be square")
    if width < libtools.BRAND_MIN:
        raise ImportRefused(f"{image_path} is {width} px; brand art must be at least {libtools.BRAND_MIN} px "
                            f"(it is never enlarged)")
    size = min(width, libtools.BRAND_MAX)
    if width > size:
        image = image.resize((size, size), Image.Resampling.LANCZOS)

    mask = libtools.outline_mask(size)
    inside = mask.point(lambda v: 255 if v == 255 else 0)
    holes = ImageChops.multiply(ImageChops.invert(image.getchannel("A")), inside).getbbox()
    if holes is not None:
        if fill is None:
            raise ImportRefused(f"{image_path} has transparent pixels inside the outline (around {holes}). "
                                f"Look at it: if it is an old round or padded logo, re-run with --fill "
                                f"'#RRGGBB' set to its own background color")
        rgb = tuple(int(fill[i:i + 2], 16) for i in (1, 3, 5))
        image = Image.alpha_composite(Image.new("RGBA", (size, size), rgb + (255,)), image)

    image.putalpha(ImageChops.darker(image.getchannel("A"), mask))
    destination = pathlib.Path(library) / "brands" / f"{name}.png"
    destination.parent.mkdir(parents=True, exist_ok=True)
    image.save(destination, format="PNG", optimize=True)
    digest = libtools.sha256_file(destination)

    brands_ini = pathlib.Path(brands_ini)
    sections = libtools.read_sections(brands_ini) if brands_ini.is_file() else []
    keys = dict(sections).get(name)
    if keys is None:
        keys = {}
        sections.append((name, keys))
    keys.update({"source": source, "size": str(size), "sha256": digest})
    for key, value in (("art", art), ("fill", fill)):
        if value:
            keys[key] = value
        else:
            keys.pop(key, None)
    libtools.write_brands(sections, brands_ini)
    return {"size": size, "sha256": digest, "reduced": width > size, "filled": fill is not None}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("name")
    parser.add_argument("image")
    parser.add_argument("--source", required=True, help="the listing or press-kit page the art came from")
    parser.add_argument("--art", help="the direct URL of the image file")
    parser.add_argument("--fill", help="#RRGGBB to put behind transparent art")
    args = parser.parse_args()
    try:
        info = import_brand(args.name, args.image, args.source, args.art, args.fill)
    except ImportRefused as refused:
        print(f"refused: {refused}")
        return 1
    note = " (reduced to 1024)" if info["reduced"] else ""
    note += f" (filled with {args.fill})" if info["filled"] else ""
    print(f"imported {args.name}: {info['size']} px{note}, sha256 {info['sha256'][:12]}")
    keys = dict(libtools.read_sections(libtools.BRANDS_INI))[args.name]
    todo = [key for key in ("title", "group", "owner") if not keys.get(key)]
    if todo:
        print(f"now set {', '.join(todo)} (and android, if it has one) for [{args.name}] in brands.ini")
    print("then run build-library.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Run the tests to confirm they pass**

Run: `python -m unittest discover -s C:/Users/jscha/source/repos/streamflex/branding/library -p "test_*.py" -v`
Expected: 15 tests, OK. The `test_full_bleed_art_is_cut_to_the_outline` pixel checks prove the corners are cut and the interior is untouched.

- [ ] **Step 5: Commit**

```powershell
$repo = 'C:/Users/jscha/source/repos/streamflex'
git -C $repo add branding/library/import-brand.py branding/library/test_import_brand.py branding/library/libtools.py
git -C $repo commit -m "feat(library): brand icon importer that cuts owners' art to the shared outline"
```

---

### Task 4: Research and import the 33 brand icons

**Files:**
- Modify: `branding/library/brands.ini`
- Create: `assets/icons/library/brands/<name>.png` × 33
- Regenerate: `assets/icons/library/icons.ini`, `assets/icons/library/brands/NOTICE.md`
- Scratch (not committed): `C:/Users/jscha/ClaudeScratch/streamflex-brand-art/`

**Interfaces:**
- Consumes: `import-brand.py` (Task 3), `build-library.py` (Task 2).
- Produces: a complete `brands.ini`. Every section has `title`, `group`, `owner`, `source` and `size`, and `android` where the service has an Android app.

This task is research, not code. Its test is `build-library.py --check` passing with all 33 sections complete, plus the review report.

| Group | Services (in manifest order) |
|---|---|
| `video` | netflix, prime-video, disney-plus, max, hulu, apple-tv, paramount-plus, peacock, crunchyroll |
| `free-tv` | youtube, youtube-tv, pluto-tv, tubi, twitch |
| `servers` | plex, jellyfin, emby, kodi, vlc, audiobookshelf |
| `music` | spotify, apple-music, amazon-music, pandora, audible |
| `games` | steam, retroarch, es-de, moonlight, geforce-now, xbox, playnite |
| `web` | chrome |

- [ ] **Step 1: For each service, establish its facts before touching any art**

For every service, record the following. **Never commit a package name you have not seen on its live listing.**

1. **`android`:** the Android package, preferring the **TV app** where one exists. Netflix's TV app is `com.netflix.ninja`, not the phone app. Confirm it by opening `https://play.google.com/store/apps/details?id=<package>` and checking that the page is that service.
   - **Steam:** leave `android` out. On the desktop, the Steam tile launches Big Picture.
   - **Playnite:** leave `android` out. It is Windows-only.
   - **Chrome:** record the package that exists (`com.android.chrome`) and note in the report that no TV build exists.
2. **`owner`:** the legal owner of the mark. Take it from the listing's developer name, cross-checked against the company's own legal or brand page. Note in the report when they differ, for example a subsidiary against its parent.
3. **`source` and `art`:**
   - **Services with an Android app:** `source` is the Play listing URL. `art` is the listing's icon image, requested at `=s512` (the `play-lh.googleusercontent.com/...` URL from the page's icon `<img>`, with its size suffix replaced by `=s512`). **Never request a larger size from play-lh.** That server enlarges images, and the importer cannot tell. Use art above 512 px only when the owner's own press or brand kit publishes the **same design** larger; then `source` is that kit page.
   - **Services without an Android app** (Steam, Playnite): the owner's press kit or the project's repository.
4. **`title`:** the service's name as it brands itself, for example `Disney+` and `Prime Video`.

Download each image to `C:/Users/jscha/ClaudeScratch/streamflex-brand-art/<name>.<ext>`.

- [ ] **Step 2: Import each icon**

```powershell
python C:/Users/jscha/source/repos/streamflex/branding/library/import-brand.py netflix C:/Users/jscha/ClaudeScratch/streamflex-brand-art/netflix.png --source "https://play.google.com/store/apps/details?id=com.netflix.ninja" --art "<the =s512 image URL>"
```

After each import, set `title`, `group`, `owner` and `android` for that section in `branding/library/brands.ini`. Keep the sections in the table's order.

When the importer **refuses** an image for transparency:
1. Look at the image.
2. If it is an old round or padded logo, re-run with `--fill` set to the art's own background color.
3. List every `--fill` choice in the report.

Never fill an image that is transparent for another reason; report it instead.

- [ ] **Step 3: Regenerate and verify**

```powershell
python C:/Users/jscha/source/repos/streamflex/branding/library/build-library.py
python C:/Users/jscha/source/repos/streamflex/branding/library/build-library.py --check
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
```

Expected:
- `updated 2 of 39 library files` (icons.ini and NOTICE.md), then `library outputs are current (39 files)`. The brand PNGs are the importer's output, not the generator's.
- `100% tests passed`.
- `icons.ini` has 69 sections: `Select-String -Path C:/Users/jscha/source/repos/streamflex/assets/icons/library/icons.ini -Pattern '^\[' | Measure-Object` gives `Count : 69`.

- [ ] **Step 4: Write the review report into the ledger**

List:
- every service's package (or why it has none), owner and art source;
- which icons are 512-only;
- every `--fill` choice;
- every case where the listing's developer and the legal owner differ.

This report goes to the user with the review sheet in Task 8.

- [ ] **Step 5: Commit**

```powershell
$repo = 'C:/Users/jscha/source/repos/streamflex'
git -C $repo add branding/library/brands.ini assets/icons/library/brands assets/icons/library/icons.ini
git -C $repo commit -m "feat(library): the 33 brand icons, each service's own app icon on the shared outline"
```

---

### Task 5: The library check in CI

**Files:**
- Create: `branding/library/check-library.py`, `branding/library/test_check_library.py`, `branding/library/requirements.txt`
- Modify: `.github/workflows/build.yml`

**Interfaces:**
- Consumes: `libtools` (including `outline_mask`), `build-library.py --check`.
- Produces: `check(library) -> [problem strings]`. The CLI exits 1 on any problem. It runs as a new CI job, `library_check`, which `build-and-test` needs.

- [ ] **Step 1: Write the failing test**

`branding/library/test_check_library.py`:

```python
"""Unit tests for check-library.py's brand and listing checks. Needs Pillow."""
import importlib.util
import pathlib
import shutil
import tempfile
import unittest

from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("check_library", HERE / "check-library.py")
cl = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cl)


class CheckLibrary(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.library = pathlib.Path(self.tmp.name) / "library"
        shutil.copytree(cl.libtools.LIBRARY, self.library)

    def tearDown(self):
        self.tmp.cleanup()

    def test_the_real_library_passes(self):
        self.assertEqual(cl.check_files(self.library), [])

    def test_an_unlisted_file_is_reported(self):
        (self.library / "generic" / "stray.svg").write_text("<svg/>", encoding="utf-8")
        self.assertTrue(any("stray.svg is not listed" in p for p in cl.check_files(self.library)))

    def test_a_non_square_brand_is_reported(self):
        brand = next((self.library / "brands").glob("*.png"))
        Image.new("RGBA", (600, 500), (0, 0, 0, 0)).save(brand)
        problems = cl.check_files(self.library)
        self.assertTrue(any(brand.name in p and "square" in p for p in problems), problems)

    def test_a_brand_opaque_in_its_corner_is_reported(self):
        brand = next((self.library / "brands").glob("*.png"))
        Image.new("RGBA", (512, 512), (10, 20, 30, 255)).save(brand)
        problems = cl.check_files(self.library)
        self.assertTrue(any(brand.name in p and "outside the outline" in p for p in problems), problems)


if __name__ == "__main__":
    unittest.main()
```

Run the library tests. Expected: ERROR, `check-library.py` does not exist.

- [ ] **Step 2: Write `branding/library/check-library.py`**

```python
"""CI check for the icon library. Needs Pillow.

Fails when:
- build-library.py --check finds a stale output (a generated file edited by hand, or a source changed
  without re-running it);
- icons.ini lists a file that does not exist, or a file in generic/ or brands/ is not listed;
- a brand PNG is not square RGBA from 512 to 1024 px, is not transparent outside the shared outline and
  opaque inside it, or does not match its sha256.
"""
import pathlib
import subprocess
import sys

from PIL import Image, ImageChops

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libtools  # noqa: E402


def check_brand(library, name, keys):
    problems = []
    path = library / keys["file"]
    if libtools.sha256_file(path) != keys.get("sha256"):
        problems.append(f"[{name}] {path.name} does not match its sha256")
    with Image.open(path) as image:
        if image.mode != "RGBA":
            problems.append(f"[{name}] {path.name} is {image.mode}, not RGBA")
            return problems
        width, height = image.size
        if width != height or not libtools.BRAND_MIN <= width <= libtools.BRAND_MAX:
            problems.append(f"[{name}] {path.name} is {width}x{height}; brand art must be square, "
                            f"{libtools.BRAND_MIN} to {libtools.BRAND_MAX} px")
            return problems
        alpha = image.getchannel("A")
        mask = libtools.outline_mask(width)
        outside = mask.point(lambda v: 255 if v == 0 else 0)
        inside = mask.point(lambda v: 255 if v == 255 else 0)
        if ImageChops.multiply(alpha, outside).getbbox() is not None:
            problems.append(f"[{name}] {path.name} is not transparent outside the outline")
        if ImageChops.multiply(ImageChops.invert(alpha), inside).getbbox() is not None:
            problems.append(f"[{name}] {path.name} is not opaque inside the outline")
    return problems


def check_files(library):
    problems, listed = [], set()
    for name, keys in libtools.read_sections(library / "icons.ini"):
        rel = keys.get("file", "")
        listed.add(rel)
        if not (library / rel).is_file():
            problems.append(f"[{name}] lists {rel}, which does not exist")
            continue
        group = keys.get("group")
        expected = libtools.BRAND_GROUPS if rel.startswith("brands/") else libtools.GENERIC_GROUPS
        if group not in expected:
            problems.append(f"[{name}] group '{group}' is not one of {', '.join(expected)}")
        if rel.startswith("brands/"):
            problems += check_brand(library, name, keys)
    for folder, pattern in (("generic", "*.svg"), ("brands", "*.png")):
        for path in sorted((library / folder).glob(pattern)):
            if f"{folder}/{path.name}" not in listed:
                problems.append(f"{folder}/{path.name} is not listed in icons.ini")
    return problems


def main():
    problems = []
    build = subprocess.run([sys.executable, str(libtools.HERE / "build-library.py"), "--check"],
                           capture_output=True, text=True)
    print(build.stdout.strip())
    if build.returncode:
        problems.append("build-library.py --check failed:\n" + build.stdout + build.stderr)
    problems += check_files(libtools.LIBRARY)
    if problems:
        print("\n".join(f"problem: {p}" for p in problems))
        return 1
    print("icon library: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

Run the library tests and the check:

```powershell
python -m unittest discover -s C:/Users/jscha/source/repos/streamflex/branding/library -p "test_*.py" -v
python C:/Users/jscha/source/repos/streamflex/branding/library/check-library.py
```

Expected: 19 tests, OK; then `library outputs are current (39 files)` and `icon library: all checks passed`.

- [ ] **Step 3: Pin Pillow with hashes for CI**

`ubuntu-latest` runs Python 3.12 on x86-64. Download the exact wheel and hash it:

```powershell
$dl = 'C:/Users/jscha/ClaudeScratch/pillow-wheel'
python -m pip download pillow==11.0.0 --only-binary=:all: --platform manylinux_2_28_x86_64 --python-version 3.12 --implementation cp -d $dl --no-deps
python -m pip hash (Get-ChildItem $dl -Filter *.whl | Select-Object -First 1).FullName
```

`branding/library/requirements.txt`, using the hash the last command printed:

```
# Pillow for check-library.py in CI (ubuntu-latest, CPython 3.12, x86-64). Installed with --require-hashes.
pillow==11.0.0 --hash=sha256:<the hash printed above>
```

- [ ] **Step 4: Add the CI job**

In `.github/workflows/build.yml`, add this job before `build-and-test`:

```yaml
  # Generated library files are current, every file is listed, and brand art fits the shared outline
  library_check:
    name: Icon library
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Git repository
        uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1
        with:
          persist-credentials: false

      - name: Check the icon library
        run: |
          python3 -m venv .venv
          .venv/bin/pip install --quiet --require-hashes -r branding/library/requirements.txt
          .venv/bin/python -m unittest discover -s branding/library -p "test_*.py" -v
          .venv/bin/python branding/library/check-library.py
```

Change `build-and-test`'s `needs` to `[build_windows, build_linux, build_rpi, build_arch, library_check]`. Its comment says "when every platform build succeeded"; change it to "when every platform build and the icon library check succeeded".

- [ ] **Step 5: Commit**

```powershell
$repo = 'C:/Users/jscha/source/repos/streamflex'
git -C $repo add branding/library/check-library.py branding/library/test_check_library.py branding/library/requirements.txt .github/workflows/build.yml
git -C $repo commit -m "ci: check the icon library on every build"
```

---

### Task 6: Resolve library names in the launcher, and switch the default config

**Files:**
- Modify: `config/launcher_config.h.in` (after line 18, and inside the `_WIN32` block at 19-25)
- Modify: `src/util.c` (includes; new `resolve_library_icons()` after `build_menu_items()` at 1050-1062), `src/util.h`
- Modify: `src/launcher.c` (include; call after `build_menu_items()` at 1234; `library_free()` in cleanup after `free(clk);` at 369)
- Modify: `src/CMakeLists.txt` (post-build copy)
- Modify: `config/config.ini.in` (lines 121-129), `CMakeLists.txt` (`ICON_GAMES` at 82, 86, 96; remove `ICONS_PREFIX` at 238, 244, 252, 255, 260)
- Delete: `assets/icons/kodi.png`, `plex.png`, `restart.png`, `retroarch.png`, `sleep.png`, `steam.png`, `system.png`
- Modify: `.github/workflows/build.yml` (package-content checks)
- Scratch: `C:/Users/jscha/ClaudeScratch/streamflex-fixtures/lib-fixtures.ps1`

**Interfaces:**
- Consumes: `library_set_warn`, `library_load`, `library_is_name`, `library_lookup`, `library_legacy_name`, `library_free`, `LIBRARY_MANIFEST`, `LIBRARY_FALLBACK_ICON`, `LIBRARY_NAME_MAX` (Task 1); the library folder (Tasks 2 and 4).
- Produces: `void resolve_library_icons();` in `util.h`.

- [ ] **Step 1: Write the failing fixture test**

`C:/Users/jscha/ClaudeScratch/streamflex-fixtures/lib-fixtures.ps1`:

```powershell
# Icon library fixtures: each config quits itself (StartupCmd=:quit); the debug log is asserted.
# Tell the user first: a full-screen window flashes for about a second per run.
$exe = "C:/Users/jscha/source/repos/streamflex/build/Release/streamflex.exe"
$log = "C:/Users/jscha/source/repos/streamflex/build/Release/streamflex.log"
$fx  = "C:/Users/jscha/ClaudeScratch/streamflex-fixtures"
$font = "C:/Users/jscha/source/repos/streamflex/assets/fonts/OpenSans-Regular.ttf"

Set-Content -LiteralPath "$fx/j-library.ini" -Encoding utf8 -Value @"
[General]
DefaultMenu=Main
StartupCmd=:quit

[Titles]
Font=$font

[Clock]
Enabled=false

[Main]
Entry1=Netflix;netflix;:quit
Entry2=Typo;netflx;:quit
Entry3=Old;C:/Nowhere/assets/icons/kodi.png;:quit
Entry4=Case;Netflix;:quit
Entry5=Quoted;"movies";:quit
Entry6=Mine;C:/Users/jscha/source/repos/streamflex/docs/streamflex.png;:quit
"@

Set-Content -LiteralPath "$fx/k-cwd.ini" -Encoding utf8 -Value @"
[General]
DefaultMenu=Main
StartupCmd=:quit

[Titles]
Font=$font

[Clock]
Enabled=false

[Main]
Entry1=Movies;movies;:quit
Entry2=Power;power;:quit
"@

$cases = [ordered]@{
    'j-library' = @{ cwd = 'C:/Users/jscha/source/repos/streamflex/build/Release'; expect = @(
        @('Icon library: .*assets[/\\]icons[/\\]library \(69 icons\)', $true),
        @("Entry 'Typo' in menu 'Main': no library icon named 'netflx'", $true),
        @("Entry 'Old' in menu 'Main': C:/Nowhere/assets/icons/kodi\.png no longer exists, using the library icon 'kodi'", $true),
        @("Entry 'Case' in menu 'Main': Netflix is not a file; using the library icon 'netflix'", $true),
        @('Icon Path: .*library[/\\]brands[/\\]netflix\.png', $true),
        @('Icon Path: .*library[/\\]generic[/\\]apps\.svg', $true),
        @('Icon Path: .*library[/\\]brands[/\\]kodi\.png', $true),
        @('Icon Path: .*library[/\\]generic[/\\]movies\.svg', $true),
        @('Icon Path: .*docs/streamflex\.png', $true),
        @('Could not load image', $false)) }
    'k-cwd' = @{ cwd = 'C:/Users/jscha/ClaudeScratch'; expect = @(
        @('Icon library: .*build[/\\]Release[/\\]assets[/\\]icons[/\\]library', $true),
        @('Icon Path: .*library[/\\]generic[/\\]movies\.svg', $true),
        @('Icon library not found', $false),
        @('Could not load image', $false)) }
}

$failed = 0
foreach ($name in $cases.Keys) {
    if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
    $p = Start-Process -FilePath $exe -ArgumentList "-c", "$fx/$name.ini", "-d" -WorkingDirectory $cases[$name].cwd -Wait -PassThru
    $text = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw } else { '' }
    $problems = @()
    if ($p.ExitCode -ne 0) { $problems += "exit $($p.ExitCode)" }
    if ($text.Length -eq 0) { $problems += 'no log written' }
    foreach ($e in $cases[$name].expect) {
        if ([regex]::IsMatch($text, $e[0]) -ne $e[1]) { $problems += $(if ($e[1]) { "missing /$($e[0])/" } else { "unexpected /$($e[0])/" }) }
    }
    if ($problems.Count) { $failed++; '{0,-10} FAIL  {1}' -f $name, ($problems -join '; ') }
    else { '{0,-10} PASS  ({1} assertions)' -f $name, $cases[$name].expect.Count }
}
"$($cases.Count - $failed)/$($cases.Count) library fixtures pass"
exit $(if ($failed) { 1 } else { 0 })
```

Run it (after telling the user about the flashing window): `pwsh -NoProfile -File C:/Users/jscha/ClaudeScratch/streamflex-fixtures/lib-fixtures.ps1`
Expected: FAIL on both. The launcher does not resolve names yet: there is no `Icon library:` line, and `Could not load image` appears for `netflix`, `netflx` and `movies`.

- [ ] **Step 2: Add the path constants**

In `config/launcher_config.h.in`, after `#define PATH_FONTS_SYSTEM ...` (line 18):

```c
#define PATH_ICONS_EXE "icons"
#define PATH_LIBRARY_EXE "library"
#define PATH_LIBRARY_SYSTEM "@CMAKE_INSTALL_PREFIX@/share/@EXECUTABLE_TITLE@/assets/icons/library"
```

And inside the `#ifdef _WIN32` block, after `#define PATH_ASSETS_RELATIVE ".\\assets"`:

```c
#define PATH_LIBRARY_RELATIVE ".\\assets\\icons\\library"
```

- [ ] **Step 3: Write `resolve_library_icons()`**

In `src/util.c`, add `#include <ctype.h>` after `#include <time.h>`, and `#include "library.h"` after `#include "util.h"`. Add after `build_menu_items()`:

```c
// A function to pass the icon library's warnings to the log
static void library_warning(const char *message)
{
    log_error("%s", message);
}

// A function to point every entry that names a library icon at its file. It also rescues two mistakes:
// a path to one of the seven icons older versions shipped (their files are gone), and a name typed in
// the wrong case. Both use the library icon and log a note.
void resolve_library_icons()
{
    library_set_warn(library_warning);
    char exe_library[MAX_PATH_CHARS + 1];
    const char *roots[2];
    roots[0] = config.exe_path != NULL
               ? join_paths(exe_library, sizeof(exe_library), 4, config.exe_path, PATH_ASSETS_EXE, PATH_ICONS_EXE, PATH_LIBRARY_EXE)
               : NULL;
#ifdef __unix__
    roots[1] = PATH_LIBRARY_SYSTEM;
#else
    roots[1] = PATH_LIBRARY_RELATIVE;
#endif
    const char *root = NULL;
    char manifest[MAX_PATH_CHARS + 1];
    for (int i = 0; i < 2 && root == NULL; i++) {
        if (roots[i] != NULL && file_exists(join_paths(manifest, sizeof(manifest), 2, roots[i], LIBRARY_MANIFEST)))
            root = roots[i];
    }
    if (root == NULL)
        log_error("Icon library not found; entries that name a library icon will have no image");
    else {
        int count = library_load(root);
        log_debug("Icon library: %s (%i icons)", root, count);
    }

    for (Menu *m = config.first_menu; m != NULL; m = m->next) {
        for (Entry *e = m->first_entry; e != NULL; e = e->next) {
            const char *path = NULL;
            if (e->icon_path == NULL)
                continue;
            if (library_is_name(e->icon_path)) {
                path = library_lookup(e->icon_path);
                if (path == NULL) {
                    log_error("Entry '%s' in menu '%s': no library icon named '%s'", e->title, m->name, e->icon_path);
                    path = library_lookup(LIBRARY_FALLBACK_ICON);
                }
            }
            else if (!file_exists(e->icon_path)) {
                const char *name = library_legacy_name(e->icon_path);
                if (name != NULL && (path = library_lookup(name)) != NULL) {
                    log_error("Entry '%s' in menu '%s': %s no longer exists, using the library icon '%s' "
                        "(write '%s' in the config to use it directly)", e->title, m->name, e->icon_path, name, name);
                }
                else {
                    char lower[LIBRARY_NAME_MAX + 1];
                    size_t length = strlen(e->icon_path);
                    if (length <= LIBRARY_NAME_MAX) {
                        for (size_t k = 0; k <= length; k++)
                            lower[k] = (char) tolower((unsigned char) e->icon_path[k]);
                        if (library_is_name(lower) && (path = library_lookup(lower)) != NULL)
                            log_error("Entry '%s' in menu '%s': %s is not a file; using the library icon '%s' "
                                "(icon names are lowercase)", e->title, m->name, e->icon_path, lower);
                    }
                }
            }
            if (path != NULL) {
                free(e->icon_path);
                e->icon_path = strdup(path);
            }
        }
    }
}
```

In `src/util.h`, add `void resolve_library_icons();` beside `build_menu_items()`'s prototype.

In `src/launcher.c`:
- add `#include "library.h"` beside the other local includes;
- in `main()`, call `resolve_library_icons();` directly after `build_menu_items();`, before the gamepad defaults;
- in the cleanup function, add `library_free();` after `free(clk);`.

In `src/CMakeLists.txt`, after the `if (WIN32) ... endif()` that creates the executable:

```cmake
# The icon library is found next to the executable, so keep a copy beside it in the build tree as well
add_custom_command(TARGET ${EXECUTABLE_TITLE} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/assets/icons/library"
          "$<TARGET_FILE_DIR:${EXECUTABLE_TITLE}>/assets/icons/library")
```

- [ ] **Step 4: Run the fixtures and the unit tests**

```powershell
cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release
ctest --test-dir C:/Users/jscha/source/repos/streamflex/build -C Release --output-on-failure
pwsh -NoProfile -File C:/Users/jscha/ClaudeScratch/streamflex-fixtures/lib-fixtures.ps1
pwsh -NoProfile -File C:/Users/jscha/ClaudeScratch/streamflex-fixtures/run-all.ps1
```

Expected:
- `100% tests passed`;
- `2/2 library fixtures pass`;
- `10/10 fixtures pass`. The older fixtures name `.../assets/icons/kodi.png`, which still exists until Step 5; after Step 5 they rescue through the legacy mapping and still pass.

- [ ] **Step 5: Switch the default config and remove the Numix icons**

In `CMakeLists.txt`:
- set `ICON_GAMES` to `"retroarch"` (line 82) and `"steam"` (lines 86 and 96), with no `.png`;
- delete the five `ICONS_PREFIX` lines (238, 244, 252, 255, 260).

In `config/config.ini.in`, the menu lines become:

```ini
[@DEFAULT_MENU@]
Entry1=Kodi;kodi;@CMD_KODI@
Entry2=Plex;plex;@CMD_PLEX@
Entry3=@TITLE_GAMES@;@ICON_GAMES@;@CMD_GAMES@
Entry4=System;settings;:submenu System

[System]
Entry1=Shutdown;power;:shutdown
Entry2=Restart;restart;:restart
Entry3=Sleep;sleep;:sleep
```

Delete the seven Numix PNGs:

```powershell
$repo = 'C:/Users/jscha/source/repos/streamflex'
git -C $repo rm assets/icons/kodi.png assets/icons/plex.png assets/icons/restart.png assets/icons/retroarch.png assets/icons/sleep.png assets/icons/steam.png assets/icons/system.png
```

Rebuild and confirm the generated configs hold names:
- `cmake --build C:/Users/jscha/source/repos/streamflex/build --config Release`
- `Select-String -Path C:/Users/jscha/source/repos/streamflex/build/config.ini, C:/Users/jscha/source/repos/streamflex/build/install/config.ini -Pattern 'Entry'`

Expected: every entry's icon field is a bare name, with no `assets\icons` path. Then re-run `lib-fixtures.ps1` and `run-all.ps1`. Expected: both pass.

- [ ] **Step 6: Assert the library's place in the packages (Review Focus 1 and 2)**

In `.github/workflows/build.yml`:

**Debian job.** After the "Show package metadata" step:

```yaml
      - name: Check the icon library is in the package
        run: |
          contents=$(dpkg-deb -c build/*.deb)
          for want in usr/share/streamflex/assets/icons/library/icons.ini usr/share/streamflex/assets/icons/library/brands/NOTICE.md usr/share/streamflex/assets/icons/library/generic/apps.svg; do
            grep -q "\./$want\$" <<< "$contents" || { echo "::error::$want is not in the package"; exit 1; }
          done
```

**Windows job.** After "Unit tests":

```yaml
      - name: Check the icon library is beside the executable in the zip
        shell: pwsh
        run: |
          Add-Type -AssemblyName System.IO.Compression.FileSystem
          $zip = Get-ChildItem build/*.zip | Select-Object -First 1
          $archive = [IO.Compression.ZipFile]::OpenRead($zip.FullName)
          $names = @($archive.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
          $archive.Dispose()
          $exe = $names | Where-Object { $_ -match '(^|/)streamflex\.exe$' } | Select-Object -First 1
          if (-not $exe) { Write-Error "streamflex.exe is not in $($zip.Name)"; exit 1 }
          $prefix = $exe.Substring(0, $exe.Length - 'streamflex.exe'.Length)
          foreach ($want in 'assets/icons/library/icons.ini', 'assets/icons/library/brands/NOTICE.md', 'assets/icons/library/generic/apps.svg') {
            if ($names -notcontains ($prefix + $want)) { Write-Error "$prefix$want is not in $($zip.Name)"; exit 1 }
          }
          "icon library found beside streamflex.exe in $($zip.Name)"
```

These steps can only run in CI. Their proof is the PR's green `Linux` and `Windows` jobs.

- [ ] **Step 7: Commit**

```powershell
$repo = 'C:/Users/jscha/source/repos/streamflex'
git -C $repo add config/launcher_config.h.in src/util.c src/util.h src/launcher.c src/CMakeLists.txt config/config.ini.in CMakeLists.txt .github/workflows/build.yml
git -C $repo commit -m "feat(library): entries name library icons; the default config uses them and the Numix icons are gone"
```

The `git rm` from Step 5 is already staged and goes into this commit.

---

### Task 7: Documentation and the docs-site gallery

**Files:**
- Create: `branding/library/build-gallery.py`, `branding/library/README.md`
- Modify: `.github/workflows/pages.yml`, `docs/_data/menu.yml`, `.gitignore`
- Modify: `docs/configuration.md` (Creating Menus at 367-388, Selected Icon Overrides at 411-414, and the Transparent section's limitations near 659-669), `docs/setup.md` (Selecting Menu Icons at 21-24), `README.md` (Credits at 118-124), `CHANGELOG.md` (`[Unreleased]`), `CONTRIBUTING.md` (repository layout at 14-22)

**Interfaces:**
- Consumes: `libtools.read_sections`, the shipped `icons.ini`.
- Produces: `docs/icons.md` and `docs/assets/library/`, both generated at site build time and gitignored.

- [ ] **Step 1: Write the failing check**

Run: `python C:/Users/jscha/source/repos/streamflex/branding/library/build-gallery.py`
Expected: FAIL, the file does not exist.

- [ ] **Step 2: Write `branding/library/build-gallery.py`**

```python
"""Write the docs site's icon gallery from the shipped manifest. pages.yml runs it before Jekyll.

  python branding/library/build-gallery.py [--page docs/icons.md] [--images docs/assets/library]

Copies every icon into the site and writes one page, grouped by the manifest's groups. Both outputs are
gitignored, so the repository keeps one copy of each icon. Standard library only.
"""
import argparse
import html
import pathlib
import shutil
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libtools  # noqa: E402

GROUPS = [
    ("video", "Subscription video"), ("free-tv", "Free and live TV"), ("servers", "Media servers and players"),
    ("music", "Music and audio"), ("games", "Games"), ("web", "Web browsers"),
    ("system", "System"), ("media", "Kinds of media"), ("general", "General"), ("devices", "Devices"),
]

HEAD = """---
layout: default
title: Icon Library
---
# Icon Library

StreamFlex ships these icons. To use one, write its name where an entry's icon path would go:

~~~ini
[Main]
Entry1=Netflix;netflix;...
Entry2=Movies;movies;:submenu Movies
~~~

A name is lowercase letters, digits and hyphens. Anything else in that field is read as the path to your own
image file, as before. See [Creating Menus](configuration#creating-menus).

<style>
.icon-grid {{ display: flex; flex-wrap: wrap; gap: 18px; margin: 12px 0 28px; }}
.icon-grid figure {{ margin: 0; width: 112px; text-align: center; }}
.icon-grid img {{ width: 96px; height: 96px; }}
.icon-grid figcaption {{ font-size: 13px; line-height: 1.3; }}
</style>

"""

BRAND_NOTE = """The service icons below are the trademarks and artwork of their owners, shown only to identify each
service. They are not covered by StreamFlex's GPL-3.0 license; see the
[brand notice](https://github.com/bilbospocketses/streamflex/blob/master/assets/icons/library/brands/NOTICE.md).

"""


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--page", default=str(libtools.ROOT / "docs" / "icons.md"))
    parser.add_argument("--images", default=str(libtools.ROOT / "docs" / "assets" / "library"))
    args = parser.parse_args()

    sections = libtools.read_sections(libtools.LIBRARY / "icons.ini")
    images = pathlib.Path(args.images)
    if images.exists():
        shutil.rmtree(images)
    parts = [HEAD.format()]
    for group, heading in GROUPS:
        members = [(name, keys) for name, keys in sections if keys.get("group") == group]
        if not members:
            continue
        if group == "video":
            parts.append(BRAND_NOTE)
        parts.append(f"## {heading}\n\n<div class=\"icon-grid\">\n")
        for name, keys in members:
            source = libtools.LIBRARY / keys["file"]
            target = images / keys["file"]
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            url = "{{ '/assets/library/" + keys["file"] + "' | relative_url }}"
            title = html.escape(keys["title"])
            parts.append(f'<figure><img src="{url}" alt="{title}"><figcaption><code>{name}</code><br>{title}'
                         f'</figcaption></figure>\n')
        parts.append("</div>\n\n")
    page = pathlib.Path(args.page)
    page.write_text("".join(parts), encoding="utf-8", newline="\n")
    print(f"wrote {page} with {len(sections)} icons")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

Run it, then build the site locally only if Jekyll is available; otherwise rely on the PR's `Build docs site` job.
Expected: `wrote ...\docs\icons.md with 69 icons`, and `docs/assets/library/` holding `generic/` (36 SVGs) and `brands/` (33 PNGs). Open `docs/icons.md` and confirm ten group headings and 69 figures.

- [ ] **Step 3: Wire the gallery into the site**

In `.gitignore`, add:

```
# Written by branding/library/build-gallery.py when the site is built
docs/icons.md
docs/assets/library/
```

In `docs/_data/menu.yml`, add after `Configuration`:

```yaml
- title: Icon Library
  link: /icons
```

In `.github/workflows/pages.yml`:
- add `"assets/icons/library/**"` and `"branding/library/**"` to both `paths:` lists;
- add this step before "Build with Jekyll":

```yaml
      # Writes docs/icons.md and copies the icons into the site; both are gitignored
      - name: Build the icon gallery
        run: python3 branding/library/build-gallery.py
```

- [ ] **Step 4: Update the prose docs**

`docs/configuration.md`, the second paragraph of "Creating Menus" (line 370) and its example, become:

````markdown
Each entry value contains 3 parts of information in order: the title, the icon, and the command to run when the button is clicked. These are delimited by semicolons:
```ini
Entry=title;icon;command
```
The icon is either the name of an icon from StreamFlex's built-in [Icon Library](icons), such as `netflix` or `movies`, or the path to an image file of your own (PNG, JPEG, WebP or SVG). A name is lowercase letters, digits and hyphens only; anything else is read as a path. To use a file of your own whose name looks like an icon name, write it as a path, for example `./kodi`.
````

Then keep the command list unchanged. Replace the example menu with:

```ini
[Media]
Entry1=Kodi;kodi;"C:\Program Shortcuts\kodi.lnk"
Entry2=Netflix;netflix;"C:\Program Shortcuts\netflix.lnk"
Entry3=Plex;plex;"C:\Program Shortcuts\plex.lnk"
Entry4=Home Videos;C:\Pictures\Icons\camera.png;"C:\Program Shortcuts\videos.lnk"
Entry5=Back;back;:back
```

Also in `docs/configuration.md`:
- In "Selected Icon Overrides", add at the end: "Icons from the library have no selected versions; the highlight shows which one is selected."
- In the Transparent background section's list of limitations, add: "Icons from the [Icon Library](icons) have smooth, partly transparent edges, so a faint outline in the chroma key color can show around them."

In `docs/setup.md`, "Selecting Menu Icons" becomes:

```markdown
## Selecting Menu Icons
StreamFlex ships an [icon library](icons): the app icons of popular streaming and media services, and generic icons for system actions, kinds of media and more. Use one by writing its name in place of an icon path, for example `Entry1=Netflix;netflix;...`.

To use your own icons, transparency is essential, so avoid JPEG, which does not support it; use PNG, WebP or SVG instead.

Icons are scaled to the size of their button, which the menu's grid decides (see [Layout](configuration.md#layout)), so the same icon may be drawn at different sizes in different menus. A PNG or WebP icon scales down best from a large original, 256x256 or more. An SVG icon is drawn at the button's exact size, so it stays sharp at any size; where an SVG version of an icon exists, it is the best choice.
```

In `README.md` Credits, replace `- [Numix icons](https://github.com/numixproject)` with:

```markdown
- [Material Symbols](https://github.com/google/material-design-icons) (Apache-2.0), the glyphs of the generic library icons

The streaming and media service icons in the icon library are the trademarks of their owners; see [the brand notice](assets/icons/library/brands/NOTICE.md).
```

In `CHANGELOG.md` `[Unreleased]`, add the first bullet of each block at the top of the matching subsection:

```markdown
### Added
- **A built-in icon library.** The app icons of 33 streaming and media services, and 36 generic icons for system actions, kinds of media, general use and devices, all on one rounded-square outline. An entry names one instead of a path: `Entry1=Netflix;netflix;...`. The docs site has a gallery of every icon and its name.

### Changed
- **The default config uses library icons.** The seven Numix icons are gone. A config that still points at one of their old files gets the matching library icon, and the log suggests its name.

### Fixed
- On Windows, the default config's icons no longer depend on the folder StreamFlex was started from.
```

In `CONTRIBUTING.md`:
- the `src/` line (14) becomes `src/                 Launcher core (launcher.c, layout.c, library.c, image.c, clock.c, util.c, utf8.c, debug.c)`;
- add after the `branding/logo/` line: `branding/library/    Tools for the icon library: generator, brand importer, checks, gallery (see its README)`.

- [ ] **Step 5: Write `branding/library/README.md`**

```markdown
# Icon library tools

The library itself is `assets/icons/library/`: `icons.ini` (the manifest), `generic/` (our SVGs) and `brands/`
(owners' app icons). Everything there except the brand PNGs is generated; never edit it by hand.

| Tool | Does |
|---|---|
| `build-library.py` | Writes the generic SVGs, `icons.ini` and `brands/NOTICE.md` from the `GENERIC` table and `brands.ini`. `--check` fails if anything is stale. |
| `fetch-glyphs.py` | Vendors the Material Symbols glyphs (pinned 0.47.5) into `glyphs/`. Run it when `GENERIC` gains a glyph. |
| `import-brand.py` | Cuts one owner's app icon to the shared outline and records its provenance in `brands.ini`. |
| `check-library.py` | The CI check: current outputs, every file listed, brand art sized and shaped correctly. |
| `build-gallery.py` | Writes the docs site's Icon Library page; `pages.yml` runs it. |
| `review-sheet.py` | Writes `build/review.html`: every icon at five sizes on four backgrounds, for review by eye. |

**Add a generic icon:** add a row to `GENERIC` in `build-library.py`, run `fetch-glyphs.py` and then `build-library.py`.

**Add a brand icon:**
1. Download its art: the Play listing icon at `=s512`, or the owner's larger press-kit art.
2. Run `import-brand.py`.
3. Fill in `title`, `group`, `owner` and `android` in `brands.ini`.
4. Run `build-library.py`.

**Remove a brand icon on request:** delete its section from `brands.ini` and its PNG, then run `build-library.py`.

The tests: `python -m unittest discover -s branding/library -p "test_*.py" -v` (needs Pillow).
```

- [ ] **Step 6: Commit**

```powershell
$repo = 'C:/Users/jscha/source/repos/streamflex'
git -C $repo add branding/library/build-gallery.py branding/library/README.md .github/workflows/pages.yml docs/_data/menu.yml .gitignore docs/configuration.md docs/setup.md README.md CHANGELOG.md CONTRIBUTING.md
git -C $repo commit -m "docs: the icon library in the configuration and setup guides, and a gallery on the docs site"
```

---

### Task 8: Review by eye, optical trims, and hands-on testing

**Files:**
- Create: `branding/library/review-sheet.py`
- Modify: `branding/library/build-library.py` (trim values only), then regenerate `assets/icons/library/generic/*.svg`

**Interfaces:**
- Consumes: the shipped library.
- Produces: `branding/library/build/review.html` (gitignored), the user's sign-off, and the qa-harness results.

- [ ] **Step 1: Write `branding/library/review-sheet.py`**

```python
"""Write build/review.html: every library icon at 32, 48, 64, 128 and 256 px on light, gray, dark and photo
backgrounds, brand icons first, with 512-only brand art marked. Open it in a browser to judge the set by eye.
Standard library only.
"""
import html
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libtools  # noqa: E402

SIZES = (32, 48, 64, 128, 256)
BACKGROUNDS = {
    "light": "#FFFFFF", "gray": "#808080", "dark": "#101418",
    "photo": "linear-gradient(135deg,#c9d8e6 0%,#e9d9b8 45%,#7a9a6b 100%)",
}


def main():
    sections = libtools.read_sections(libtools.LIBRARY / "icons.ini")
    brands = {name: keys for name, keys in libtools.read_sections(libtools.BRANDS_INI)}
    ordered = [s for s in sections if s[1]["file"].startswith("brands/")] + \
              [s for s in sections if s[1]["file"].startswith("generic/")]
    out = libtools.HERE / "build"
    out.mkdir(exist_ok=True)
    rows = []
    for background, css in BACKGROUNDS.items():
        rows.append(f'<h2>{background}</h2><div class="bg" style="background:{css}">')
        for name, keys in ordered:
            src = (libtools.LIBRARY / keys["file"]).relative_to(libtools.ROOT).as_posix()
            flag = " (512 only)" if brands.get(name, {}).get("size") == "512" else ""
            imgs = "".join(f'<img src="../../../{src}" width="{s}" height="{s}">' for s in SIZES)
            rows.append(f'<div class="icon">{imgs}<span>{html.escape(name)}{flag}</span></div>')
        rows.append("</div>")
    page = ("<!doctype html><meta charset=utf-8><title>Icon library review</title><style>"
            "body{font:13px system-ui,sans-serif;margin:16px}.bg{display:flex;flex-direction:column;gap:6px;"
            "padding:12px;border-radius:8px}.icon{display:flex;align-items:flex-end;gap:10px}"
            ".icon span{color:#fff;text-shadow:0 1px 2px #000;margin-left:8px}</style>" + "".join(rows))
    (out / "review.html").write_text(page, encoding="utf-8")
    print(f"wrote {out / 'review.html'} with {len(ordered)} icons")


if __name__ == "__main__":
    main()
```

Run: `python C:/Users/jscha/source/repos/streamflex/branding/library/review-sheet.py`
Expected: `wrote ...\branding\library\build\review.html with 69 icons`.

- [ ] **Step 2: Tune the optical trims with the user**

1. Open `review.html` with the user; the brainstorm companion can show it.
2. Adjust the trim column of `GENERIC` in `build-library.py` for any glyph that looks too big or too small against its neighbors. A solid mass (`photo`, `folder`) usually wants about 0.9, and a thin one (`bluetooth`) about 1.1.
3. After each change, run `build-library.py`, `review-sheet.py`, the library unit tests and `ctest`.

The Python geometry tests only pin trim 1.0 and 0.5, so they keep passing.

**The user signs off on the set here, together with Task 4's report.** Record the sign-off and the final trims in the ledger.

- [ ] **Step 3: Commit the trims**

```powershell
$repo = 'C:/Users/jscha/source/repos/streamflex'
git -C $repo add branding/library/review-sheet.py branding/library/build-library.py assets/icons/library/generic
git -C $repo commit -m "feat(library): optical trims for the generic icons, tuned on the review sheet"
```

- [ ] **Step 4: Push, open the PR, and hand the CI zip to qa-harness**

1. Push `feat/icon-library` and open a PR. Do not arm auto-merge yet.
2. When CI is green, send the qa-harness session the run's Windows zip and these checks:
   1. The default config at 1080p: every library icon draws sharp, with no blank buttons.
   2. The largest mode the guest offers: the icons stay sharp. 512-only brand art may be slightly soft; it must not be blocky.
   3. A config with `Entry1=Typo;netflx;:quit` shows the Apps icon on that button.
   4. The launcher started through a shortcut whose "Start in" folder is elsewhere still shows every icon.

Wait for its results, and record them in the ledger.

- [ ] **Step 5: Final whole-branch review, then merge**

Run the executing skill's final review over `master..feat/icon-library`. Fix Critical and Important findings with TDD, and ledger the Minor ones. Then report to the user and merge on their word (squash).

---

## Self-Review Notes

- **Spec coverage.** Every spec section maps to a task:

  | Spec section | Task |
  |---|---|
  | The library on disk | 2, 4 |
  | Manifest and licensing | 2, 4 |
  | The generic set | 2, 8 |
  | The brand set | 3, 4 |
  | Runtime | 1, 6 |
  | Errors and limits | 1, 6 |
  | Existing configs | 6 |
  | Packaging | 6 |
  | Documentation | 7 |
  | Testing: unit tests | 1 |
  | Testing: SVG test | 2 |
  | Testing: library check | 5 |
  | Testing: fixture runs | 6 |
  | Testing: review by eye | 8 |
  | Testing: hands-on | 8 |

- **Rulings against the spec.** Both are recorded in their tasks:
  - `library_set_warn()` added to the API (Task 1);
  - one `build-library.py` generating the manifest and NOTICE from `brands.ini`, in place of `build-generic.py` and a hand-kept manifest (Task 2).
- **Additions beyond the spec, from Review Focus:**
  - the wrong-case rescue (Task 6);
  - CI package-layout checks for the `.deb` and the Windows zip (Task 6);
  - the Transparent-mode fringe note (Task 7).
