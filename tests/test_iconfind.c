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

// More of the fixture, for the rules the tests above leave open
static void make_more_fixture(void)
{
    make_file("a/pixmaps/.png");                                    // Only an extension: never a name
    make_file("b/pixmaps/vector.svg");
    make_file("b/pixmaps/both.png");
    make_file("b/pixmaps/both.xpm");
    make_file("a/icons/hicolor/scalable/apps/twin.svg");
    make_file("b/icons/hicolor/scalable/apps/twin.svg");
    make_file("a/icons/hicolor/64x64/apps/tie.png");
    make_file("b/icons/hicolor/64x64/apps/tie.png");
    make_file("a/icons/hicolor/128x128/apps/edge.png");
    make_file("b/icons/hicolor/scalable/apps/edge.svg");
}

// A function to test the rules the tests above leave open: a .svg or .xpm dropped too; an SVG in the
// pixmaps, and a PNG there before an XPM; the first folder's SVG and, on a tie, its PNG; a PNG of exactly
// ICONFIND_GOOD_SIZE over an SVG; a name too long to look for; and an absolute path that fits exactly
static void test_more_names(void)
{
    CHECK_STR(found("inkscape.svg"), FIXTURE "/b/icons/hicolor/scalable/apps/inkscape.svg");
    CHECK_STR(found("legacy.XPM"), FIXTURE "/a/pixmaps/legacy.xpm");
    CHECK_STR(found("vector"), FIXTURE "/b/pixmaps/vector.svg");
    CHECK_STR(found("both"), FIXTURE "/b/pixmaps/both.png");
    CHECK_STR(found("twin"), FIXTURE "/a/icons/hicolor/scalable/apps/twin.svg");
    CHECK_STR(found("tie"), FIXTURE "/a/icons/hicolor/64x64/apps/tie.png");
    CHECK_STR(found("edge"), FIXTURE "/a/icons/hicolor/128x128/apps/edge.png");
    char name[301];
    memset(name, 'k', 300);
    name[300] = '\0';
    CHECK_STR(found(name), "(none)");

    char absolute[1024];
    char out[1100];
    CHECK(fileio_full_path(FIXTURE "/abs.png", absolute, sizeof(absolute)));
    CHECK(!iconfind_path(absolute, NULL, 0, out, strlen(absolute)));
    CHECK(iconfind_path(absolute, NULL, 0, out, strlen(absolute) + 1));
}

#ifndef _WIN32
// A function to give a data folder under the fixture whose path is `length` bytes long, padded with "/."
// and, for an odd length, one more "/"
static void long_dir(char *out, size_t size, const char *name, size_t length)
{
    size_t at = (size_t) snprintf(out, size, FIXTURE "/%s", name);
    while (at + 2 <= length && at + 2 < size) {
        out[at++] = '/';
        out[at++] = '.';
    }
    if (at < length && at + 1 < size)
        out[at++] = '/';
    out[at] = '\0';
}

// A function to make a one-byte file at a path of any length, its folders first
static void make_path(const char *path)
{
    static char folder[8192 + 64];
    snprintf(folder, sizeof(folder), "%s", path);
    char *slash = strrchr(folder, '/');
    if (slash != NULL)
        *slash = '\0';
    CHECK(fileio_make_dirs(folder));
    CHECK(fileio_write_all(path, "x", 1));
}

// A function to test that a path cut short is never taken for the one asked for (Linux, where a path of up
// to 4095 bytes opens): each data folder is padded so that the icon's path is one byte too long for
// iconfind.c's 4096-byte buffer, and a file named as the cut path would be ("cut.pn", "cut.sv") is there
static void test_cut_paths(void)
{
    static char pixmaps[8192];
    static char sizes[8192];
    static char scalable[8192];
    static char file[8192 + 64];   // A folder above and the longest name below it
    static char out[8192];
    long_dir(pixmaps, sizeof(pixmaps), "cut1", 4096 - strlen("/pixmaps/cut.png"));
    long_dir(sizes, sizeof(sizes), "cut2", 4096 - strlen("/icons/hicolor/48x48/apps/cut.png"));
    long_dir(scalable, sizeof(scalable), "cut3", 4096 - strlen("/icons/hicolor/scalable/apps/cut.svg"));
    snprintf(file, sizeof(file), "%s/pixmaps/cut.pn", pixmaps);
    make_path(file);
    snprintf(file, sizeof(file), "%s/icons/hicolor/48x48/apps/cut.pn", sizes);
    make_path(file);
    snprintf(file, sizeof(file), "%s/icons/hicolor/scalable/apps/cut.sv", scalable);
    make_path(file);
    const char *in_pixmaps[] = { pixmaps };
    const char *in_sizes[] = { sizes };
    const char *in_scalable[] = { scalable };
    CHECK(!iconfind_path("cut", in_pixmaps, 1, out, sizeof(out)));
    CHECK(!iconfind_path("cut", in_sizes, 1, out, sizeof(out)));
    CHECK(!iconfind_path("cut", in_scalable, 1, out, sizeof(out)));
}
#endif

int main(void)
{
    make_fixture();
    make_more_fixture();
    test_names();
    test_absolute();
    test_more_names();
#ifndef _WIN32
    test_cut_paths();
#endif
    return check_report();
}
