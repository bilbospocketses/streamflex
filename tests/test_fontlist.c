#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "fontlist.h"

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

    // Regular when the family has one (a collection's face 3 here); otherwise its first face
    int noto = family(list, "Noto Sans CJK");
    CHECK_STR(fontlist_path(list, noto), "/f/Noto.ttc");
    CHECK_INT(fontlist_face(list, noto), 3);
    int dejavu = family(list, "DejaVu Sans");
    CHECK_STR(fontlist_path(list, dejavu), "/f/DejaVuSans-Bold.ttf");   // No Regular: the first face
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

int main(void)
{
    test_families();
    test_empty();
    test_rules();
    test_add_after_finish();
    return check_report();
}
