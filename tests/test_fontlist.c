#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "fontlist.h"
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

// A function to find a family's place by name
static int family(const FontList *list, const char *name)
{
    for (int i = 0; i < fontlist_count(list); i++) {
        if (strcmp(fontlist_family(list, i), name) == 0)
            return i;
    }
    return -1;
}

// A function to test grouping faces into families and choosing each family's face
static void test_families(void)
{
    FontList *list = fontlist_create();
    CHECK(fontlist_add(list, "/f/DejaVuSans-Bold.ttf", 0, "DejaVu Sans", "Bold", false));
    CHECK(fontlist_add(list, "/f/DejaVuSans.ttf", 0, "DejaVu Sans", "Book", false));
    CHECK(fontlist_add(list, "/f/Noto.ttc", 0, "Noto Sans CJK", "Bold", false));
    CHECK(fontlist_add(list, "/f/Noto.ttc", 3, "Noto Sans CJK", "Regular", false));
    CHECK(fontlist_add(list, "/f/roboto.ttf", 0, "roboto", "regular", false));
    CHECK(fontlist_add(list, "/f/Roboto-Italic.ttf", 0, "Roboto", "Italic", false));
    CHECK(fontlist_add(list, "/app/OpenSans-Regular.ttf", 0, "Open Sans", "Regular", true));
    fontlist_finish(list);

    // One family per name, whatever its case; the bundled font first, then by name
    CHECK_INT(fontlist_count(list), 4);
    CHECK_STR(fontlist_family(list, 0), "Open Sans");
    CHECK_STR(fontlist_family(list, 1), "DejaVu Sans");
    CHECK_STR(fontlist_family(list, 2), "Noto Sans CJK");
    CHECK_INT(family(list, "roboto"), 3);                    // The first spelling seen names it

    // Regular when the family has one (a collection's face 3 here); DejaVu's regular style is Book
    int noto = family(list, "Noto Sans CJK");
    CHECK_STR(fontlist_path(list, noto), "/f/Noto.ttc");
    CHECK_INT(fontlist_face(list, noto), 3);
    int dejavu = family(list, "DejaVu Sans");
    CHECK_STR(fontlist_path(list, dejavu), "/f/DejaVuSans.ttf");   // Book, not the Bold seen first
    CHECK_INT(fontlist_face(list, dejavu), 0);
    CHECK_STR(fontlist_path(list, family(list, "roboto")), "/f/roboto.ttf");

    // A face finds its family, whichever face of it is written in the file
    CHECK_INT(fontlist_find(list, "/f/Noto.ttc", 0), noto);
    CHECK_INT(fontlist_find(list, "/f/DejaVuSans.ttf", 0), dejavu);
    CHECK_INT(fontlist_find(list, "/f/Noto.ttc", 7), -1);
    CHECK_INT(fontlist_find(list, "/nowhere.ttf", 0), -1);
    fontlist_free(list);
}

// A function to test an empty list, as a scan that found nothing gives
static void test_empty(void)
{
    FontList *list = fontlist_create();
    fontlist_finish(list);
    CHECK_INT(fontlist_count(list), 0);
    CHECK(fontlist_family(list, 0) == NULL);
    CHECK(fontlist_path(list, 0) == NULL);
    CHECK_INT(fontlist_face(list, 0), 0);
    fontlist_free(list);
    fontlist_free(NULL);
}

// A function to test what the plan's tests leave open: a family's first Regular stays its face,
// whatever the style's case; a bundled face found after others still puts its family first; names
// sort without regard to case; a name that begins another is a family of its own; and a place
// outside the list has nothing
static void test_rules(void)
{
    FontList *list = fontlist_create();
    CHECK(fontlist_add(list, "/f/b-bold.ttf", 0, "Beta", "Bold", false));
    CHECK(fontlist_add(list, "/f/b.ttf", 0, "Beta", "REGULAR", false));
    CHECK(fontlist_add(list, "/f/b2.ttf", 0, "Beta", "Regular", false));
    CHECK(fontlist_add(list, "/f/alpha.ttf", 0, "alpha", "Regular", false));
    CHECK(fontlist_add(list, "/f/z.ttf", 0, "Zeta", "Regular", false));
    CHECK(fontlist_add(list, "/app/z-bold.ttf", 0, "zeta", "Bold", true));
    CHECK(fontlist_add(list, "/f/noto.ttf", 0, "Noto Sans", "Regular", false));
    CHECK(fontlist_add(list, "/f/noto-cjk.ttf", 0, "Noto Sans CJK", "Regular", false));
    fontlist_finish(list);

    CHECK_INT(fontlist_count(list), 5);
    CHECK_STR(fontlist_family(list, 0), "Zeta");          // Bundled by its second face
    CHECK_STR(fontlist_family(list, 1), "alpha");         // Before Beta: case does not count
    CHECK_STR(fontlist_family(list, 2), "Beta");
    CHECK_STR(fontlist_family(list, 3), "Noto Sans");
    CHECK_STR(fontlist_family(list, 4), "Noto Sans CJK");
    CHECK_STR(fontlist_path(list, 2), "/f/b.ttf");        // The first Regular, in capitals, not b2.ttf
    CHECK_STR(fontlist_path(list, 0), "/f/z.ttf");        // A Regular, though not the bundled face
    CHECK_INT(fontlist_find(list, "/app/z-bold.ttf", 0), 0);
    CHECK_INT(fontlist_find(list, "/f/noto-cjk.ttf", 0), 4);
    CHECK(fontlist_family(list, -1) == NULL);
    CHECK(fontlist_family(list, 5) == NULL);
    CHECK(fontlist_path(list, 5) == NULL);
    CHECK_INT(fontlist_face(list, -1), 0);
    fontlist_free(list);
}

// A function to test which face a family writes when none is named Regular (Ubuntu's DejaVu and
// URW fonts): Book, Normal, Roman, Plain and Standard count as regular, though Regular still wins;
// else an upright, normal-weight face (Light, Condensed); else a bold or a slanted one before one
// that is both. Faces as good as each other keep the first seen.
static void test_upright_face(void)
{
    FontList *list = fontlist_create();
    // DejaVu Serif in the order fontconfig lists its files: Book last
    CHECK(fontlist_add(list, "/f/DejaVuSerif-Bold.ttf", 0, "DejaVu Serif", "Bold", false));
    CHECK(fontlist_add(list, "/f/DejaVuSerif-BoldItalic.ttf", 0, "DejaVu Serif", "Bold Italic", false));
    CHECK(fontlist_add(list, "/f/DejaVuSerif-Italic.ttf", 0, "DejaVu Serif", "Italic", false));
    CHECK(fontlist_add(list, "/f/DejaVuSerif.ttf", 0, "DejaVu Serif", "Book", false));
    // DejaVu Sans Mono, which the list drew bold oblique
    CHECK(fontlist_add(list, "/f/Mono-BoldOblique.ttf", 0, "DejaVu Sans Mono", "Bold Oblique", false));
    CHECK(fontlist_add(list, "/f/Mono-Oblique.ttf", 0, "DejaVu Sans Mono", "Oblique", false));
    CHECK(fontlist_add(list, "/f/Mono.ttf", 0, "DejaVu Sans Mono", "book", false));
    CHECK(fontlist_add(list, "/f/Mono-Bold.ttf", 0, "DejaVu Sans Mono", "Bold", false));
    // A Regular after a Book still wins
    CHECK(fontlist_add(list, "/f/g-book.ttf", 0, "Gamma", "Book", false));
    CHECK(fontlist_add(list, "/f/g.ttf", 0, "Gamma", "Regular", false));
    // Roman and Normal
    CHECK(fontlist_add(list, "/f/p-bold.ttf", 0, "P052", "Bold", false));
    CHECK(fontlist_add(list, "/f/p-roman.ttf", 0, "P052", "Roman", false));
    CHECK(fontlist_add(list, "/f/n-italic.ttf", 0, "Nu", "Italic", false));
    CHECK(fontlist_add(list, "/f/n.ttf", 0, "Nu", "Normal", false));
    // No regular-like style: the upright, normal-weight face
    CHECK(fontlist_add(list, "/f/l-bi.ttf", 0, "Lambda", "Bold Italic", false));
    CHECK(fontlist_add(list, "/f/l-li.ttf", 0, "Lambda", "Light Italic", false));
    CHECK(fontlist_add(list, "/f/l.ttf", 0, "Lambda", "Light", false));
    CHECK(fontlist_add(list, "/f/l-b.ttf", 0, "Lambda", "Bold", false));
    // Bold before Bold Italic; SemiBold and Black count as bold; Italic and Bold tie, the first kept
    CHECK(fontlist_add(list, "/f/k-bi.ttf", 0, "Kappa", "Bold Italic", false));
    CHECK(fontlist_add(list, "/f/k-sb.ttf", 0, "Kappa", "SemiBold", false));
    CHECK(fontlist_add(list, "/f/m-black.ttf", 0, "Mu", "Black", false));
    CHECK(fontlist_add(list, "/f/m-ci.ttf", 0, "Mu", "Condensed Italic", false));
    CHECK(fontlist_add(list, "/f/o-i.ttf", 0, "Omega", "Italic", false));
    CHECK(fontlist_add(list, "/f/o-b.ttf", 0, "Omega", "Bold", false));
    // Each other name for regular after an upright Medium, which it beats; each word for heavy or
    // slanted before an upright Light, which beats it; one both heavy and slanted before one that
    // is only slanted
    static const char *const regular[] = { "Book", "Normal", "Roman", "Plain", "Standard" };
    static const char *const beaten[] = { "Black", "Bold", "Heavy", "Demi", "Italic", "Oblique", "Slanted" };
    char name[32];
    for (size_t i = 0; i < sizeof(regular) / sizeof(regular[0]); i++) {
        snprintf(name, sizeof(name), "R %s", regular[i]);
        CHECK(fontlist_add(list, "/f/medium.ttf", 0, name, "Medium", false));
        CHECK(fontlist_add(list, "/f/regular.ttf", 0, name, regular[i], false));
    }
    for (size_t i = 0; i < sizeof(beaten) / sizeof(beaten[0]); i++) {
        snprintf(name, sizeof(name), "B %s", beaten[i]);
        CHECK(fontlist_add(list, "/f/beaten.ttf", 0, name, beaten[i], false));
        CHECK(fontlist_add(list, "/f/light.ttf", 0, name, "Light", false));
    }
    CHECK(fontlist_add(list, "/f/r-bo.ttf", 0, "Rho", "Bold Oblique", false));
    CHECK(fontlist_add(list, "/f/r-o.ttf", 0, "Rho", "Oblique", false));
    fontlist_finish(list);

    CHECK_STR(fontlist_path(list, family(list, "DejaVu Serif")), "/f/DejaVuSerif.ttf");
    CHECK_STR(fontlist_path(list, family(list, "DejaVu Sans Mono")), "/f/Mono.ttf");
    CHECK_STR(fontlist_path(list, family(list, "Gamma")), "/f/g.ttf");
    CHECK_STR(fontlist_path(list, family(list, "P052")), "/f/p-roman.ttf");
    CHECK_STR(fontlist_path(list, family(list, "Nu")), "/f/n.ttf");
    CHECK_STR(fontlist_path(list, family(list, "Lambda")), "/f/l.ttf");
    CHECK_STR(fontlist_path(list, family(list, "Kappa")), "/f/k-sb.ttf");
    CHECK_STR(fontlist_path(list, family(list, "Mu")), "/f/m-black.ttf");
    CHECK_STR(fontlist_path(list, family(list, "Omega")), "/f/o-i.ttf");
    for (size_t i = 0; i < sizeof(regular) / sizeof(regular[0]); i++) {
        snprintf(name, sizeof(name), "R %s", regular[i]);
        CHECK_STR(fontlist_path(list, family(list, name)), "/f/regular.ttf");
    }
    for (size_t i = 0; i < sizeof(beaten) / sizeof(beaten[0]); i++) {
        snprintf(name, sizeof(name), "B %s", beaten[i]);
        CHECK_STR(fontlist_path(list, family(list, name)), "/f/light.ttf");
    }
    CHECK_STR(fontlist_path(list, family(list, "Rho")), "/f/r-o.ttf");
    fontlist_free(list);
}

// A function to test a face added after the families were put in order: the list goes back to the
// order found, every family in it, until it is put in order again
static void test_add_after_finish(void)
{
    FontList *list = fontlist_create();
    CHECK(fontlist_add(list, "/f/b.ttf", 0, "B", "Regular", false));
    CHECK(fontlist_add(list, "/f/a.ttf", 0, "A", "Regular", false));
    fontlist_finish(list);
    CHECK_STR(fontlist_family(list, 0), "A");
    CHECK(fontlist_add(list, "/f/c.ttf", 0, "C", "Regular", true));
    CHECK_INT(fontlist_count(list), 3);
    CHECK_STR(fontlist_family(list, 0), "B");          // The order found
    CHECK_STR(fontlist_family(list, 2), "C");
    CHECK_INT(fontlist_find(list, "/f/c.ttf", 0), 2);
    fontlist_finish(list);
    CHECK_STR(fontlist_family(list, 0), "C");          // In order again: bundled first, then by name
    CHECK_STR(fontlist_family(list, 1), "A");
    CHECK_INT(fontlist_find(list, "/f/b.ttf", 0), 2);
    fontlist_free(list);
}

// A function to test which listed entries the font scan takes: a regular file with a font's
// extension in any case; never a folder, a pipe (whose read would hold the font list for good) or a
// file with another extension
static void test_font_files(void)
{
    FileioEntry font = { .name = "DejaVuSans.TTF", .is_dir = false, .is_file = true, .hidden = false };
    FileioEntry collection = { .name = "Noto.ttc", .is_dir = false, .is_file = true, .hidden = false };
    FileioEntry folder = { .name = "fonts.ttf", .is_dir = true, .is_file = false, .hidden = false };
    FileioEntry pipe = { .name = "feed.ttf", .is_dir = false, .is_file = false, .hidden = false };
    FileioEntry notes = { .name = "notes.txt", .is_dir = false, .is_file = true, .hidden = false };
    CHECK(fontlist_is_font_file(&font));
    CHECK(fontlist_is_font_file(&collection));
    CHECK(!fontlist_is_font_file(&folder));
    CHECK(!fontlist_is_font_file(&pipe));
    CHECK(!fontlist_is_font_file(&notes));
    CHECK(fontlist_is_font_name("a.otf"));
    CHECK(fontlist_is_font_name("a.OTC"));
    CHECK(!fontlist_is_font_name(".ttf"));
    CHECK(!fontlist_is_font_name("a.woff"));
}

#ifndef _WIN32
// The real folder the next test lists, relative to the folder CTest runs the test in
#define PIPES "fontlist-fixture"

// A function to test the font scan's rule on a real folder: a pipe named x.ttf is listed by
// fileio_list() and left out, while the font file beside it is taken
static void test_font_files_leave_out_a_real_pipe(void)
{
    unlink(PIPES "/x.ttf");
    unlink(PIPES "/y.ttf");
    rmdir(PIPES);
    CHECK(mkdir(PIPES, 0755) == 0);
    CHECK(mkfifo(PIPES "/x.ttf", 0644) == 0);
    CHECK(fileio_write_all(PIPES "/y.ttf", "x", 1));
    FileioEntry *entries = NULL;
    int count = fileio_list(PIPES, &entries);
    CHECK_INT(count, 2);
    int taken = 0;
    for (int i = 0; i < count; i++) {
        if (fontlist_is_font_file(&entries[i])) {
            taken++;
            CHECK_STR(entries[i].name, "y.ttf");
        }
    }
    CHECK_INT(taken, 1);
    fileio_free_list(entries, count > 0 ? count : 0);
    unlink(PIPES "/x.ttf");
    unlink(PIPES "/y.ttf");
    rmdir(PIPES);
}
#endif

int main(void)
{
    test_families();
    test_empty();
    test_rules();
    test_upright_face();
    test_add_after_finish();
    test_font_files();
#ifndef _WIN32
    test_font_files_leave_out_a_real_pipe();
#endif
    return check_report();
}
