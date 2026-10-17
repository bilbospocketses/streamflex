#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "check.h"
#include "iconfind.h"
#include "fileio.h"

#define FIXTURE "iconfind-fixture"

// A function to make a one-byte file under the fixture, its folders first
static void make_file(const char *relative)
{
    char path[512];
    char folder[512];
    snprintf(path, sizeof(path), FIXTURE "/%s", relative);
    snprintf(folder, sizeof(folder), "%s", path);
    char *slash = strrchr(folder, '/');
    if (slash != NULL)
        *slash = '\0';
    CHECK(fileio_make_dirs(folder));
    CHECK(fileio_write_all(path, "x", 1));
}

// A function to make a folder under the fixture
static void make_folder(const char *relative)
{
    char path[512];
    snprintf(path, sizeof(path), FIXTURE "/%s", relative);
    CHECK(fileio_make_dirs(path));
}

// A function to look an Icon= value up in the fixture's two data folders; "(none)" when nothing is found
static const char *found(const char *value)
{
    static char out[512];
    static const char *const dirs[] = { FIXTURE "/a", FIXTURE "/b" };
    return iconfind_path(value, dirs, 2, out, sizeof(out)) ? out : "(none)";
}

// The fixture: two data folders, a and b, as /usr/share and a Flatpak export would be
static void make_fixture(void)
{
    make_file("a/icons/hicolor/48x48/apps/kodi.png");
    make_file("a/icons/hicolor/scalable/apps/kodi.svg");
    make_file("b/icons/hicolor/256x256/apps/kodi.png");
    make_folder("b/icons/hicolor/512x512/apps/kodi.png");          // A folder with an icon's name
    make_file("a/icons/hicolor/64x64/apps/inkscape.png");
    make_file("b/icons/hicolor/scalable/apps/inkscape.svg");
    make_file("a/icons/hicolor/32x32/apps/tiny.png");
    make_file("a/icons/hicolor/1024x1024@2/apps/hidpi.png");       // Not a size folder
    make_file("a/icons/hicolor/128x64/apps/hidpi.png");            // Not square: not a size folder
    make_file("b/pixmaps/hidpi.png");
    make_file("a/pixmaps/legacy.xpm");
    make_file("b/pixmaps/legacy.png");
    make_file("a/icons/hicolor/scalable/apps/org.gnome.Calculator.svg");
    make_file("abs.png");
}

// A function to test names in the theme and the pixmaps: the largest PNG across the folders (a folder
// named like one skipped); an SVG over a PNG under 128 px; a small PNG when there is no SVG; only square
// size folders; the pixmaps folder by folder; an extension dropped in any case, other dots kept; and the
// values that find nothing
static void test_names(void)
{
    CHECK_STR(found("kodi"), FIXTURE "/b/icons/hicolor/256x256/apps/kodi.png");
    CHECK_STR(found("kodi.png"), FIXTURE "/b/icons/hicolor/256x256/apps/kodi.png");
    CHECK_STR(found("kodi.PNG"), FIXTURE "/b/icons/hicolor/256x256/apps/kodi.png");
    CHECK_STR(found("inkscape"), FIXTURE "/b/icons/hicolor/scalable/apps/inkscape.svg");
    CHECK_STR(found("tiny"), FIXTURE "/a/icons/hicolor/32x32/apps/tiny.png");
    CHECK_STR(found("hidpi"), FIXTURE "/b/pixmaps/hidpi.png");
    CHECK_STR(found("legacy"), FIXTURE "/a/pixmaps/legacy.xpm");
    CHECK_STR(found("org.gnome.Calculator"), FIXTURE "/a/icons/hicolor/scalable/apps/org.gnome.Calculator.svg");
    CHECK_STR(found("missing"), "(none)");
    CHECK_STR(found(""), "(none)");
    CHECK_STR(found(".png"), "(none)");
    CHECK_STR(found("apps/kodi"), "(none)");
    CHECK_STR(found("..\\kodi"), "(none)");
    char out[512];
    CHECK(!iconfind_path(NULL, NULL, 0, out, sizeof(out)));
    CHECK(!iconfind_path("kodi", NULL, 0, out, sizeof(out)));
    static const char *const dirs[] = { FIXTURE "/a", FIXTURE "/b" };
    CHECK(!iconfind_path("kodi", dirs, 2, out, 10));                 // Found, but it does not fit
}

// A function to test an absolute path: used as it is when it is a file, nothing when it is not there or
// does not fit
static void test_absolute(void)
{
    char absolute[1024];
    char gone[1100];
    char out[1100];
    CHECK(fileio_full_path(FIXTURE "/abs.png", absolute, sizeof(absolute)));
    CHECK(iconfind_path(absolute, NULL, 0, out, sizeof(out)));
    CHECK_STR(out, absolute);
    snprintf(gone, sizeof(gone), "%s.gone", absolute);
    CHECK(!iconfind_path(gone, NULL, 0, out, sizeof(out)));
    CHECK(!iconfind_path(absolute, NULL, 0, out, 4));
}

int main(void)
{
    make_fixture();
    test_names();
    test_absolute();
    return check_report();
}
