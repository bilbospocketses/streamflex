#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "browser.h"

// A pretend file system: each folder's names, '|' between them; a trailing '/' marks a folder, and a
// trailing '=' something that is neither a folder nor a regular file (a pipe, a socket, a device)
static const struct {
    const char *folder;
    const char *names;
} FAKE[] = {
    { "/", "home/|media/" },
    { "/home", "me/" },
    { "/home/me", "Pictures/|Music/|.cache/" },
    { "/home/me/Pictures", "Autumn/|beach.JPG|.hidden.png|notes.txt|zebra.png|apple.webp|Birthdays/" },
    { "/home/me/Pictures/Autumn", "a.jpg|b.jpeg" },
    { "/home/me/Pictures/Birthdays", "one.png" },
    { "/media", "usb/" },
    { "/media/usb", "" },
    { "C:\\", "Users/" },
    { "C:\\Users", "me/" },
    { "C:\\Users\\me", "Pictures/" },
    { "C:\\Users\\me\\Pictures", "trip.png|x.jpg" },
    { "C:/Users/me", "Pictures/" },
    { "C:/Users/me/Pictures", "trip.png|x.jpg" },
    { "C:", "Users/" },
    { "/orphan/child", "a.png" },
    { "/nas", "far.png" },
    { "/home/me/pads", "old.txt|feed=|sub/|game.png=|a.txt" },
    { "/home/me/piped", "a.png=|b.png" },
    { "/home/me/pipeonly", "a.png=" }
};

// A function to list a pretend folder, the way fileio_list() lists a real one
static int fake_list(const char *folder, FileioEntry **entries, void *context)
{
    (void) context;
    for (size_t i = 0; i < sizeof(FAKE) / sizeof(FAKE[0]); i++) {
        if (strcmp(FAKE[i].folder, folder) != 0)
            continue;
        int count = 0;
        int names = FAKE[i].names[0] != '\0' ? 1 : 0;   // One more name than there are '|'
        for (const char *p = FAKE[i].names; *p != '\0'; p++)
            names += *p == '|';
        *entries = calloc((size_t) (names > 0 ? names : 1), sizeof(FileioEntry));
        const char *p = FAKE[i].names;
        while (*p != '\0') {
            const char *end = strchr(p, '|');
            size_t length = end != NULL ? (size_t) (end - p) : strlen(p);
            bool is_dir = length > 0 && p[length - 1] == '/';
            bool special = length > 0 && p[length - 1] == '=';
            size_t name_length = is_dir || special ? length - 1 : length;
            char *name = malloc(name_length + 1);
            memcpy(name, p, name_length);
            name[name_length] = '\0';
            if (count < names)
                (*entries)[count++] = (FileioEntry) { .name = name, .is_dir = is_dir, .is_file = !is_dir && !special,
                                                      .hidden = name[0] == '.' };
            else
                free(name);
            p += length;
            if (*p == '|')
                p++;
        }
        return count;
    }
    return -1;
}

// A function to refuse paths with "zebra" or ending in "Autumn", standing in for config.ini's limits
static const char *fake_check(const char *path, void *context)
{
    (void) context;
    size_t length = strlen(path);
    if (strstr(path, "zebra") != NULL || (length >= 6 && strcmp(path + length - 6, "Autumn") == 0))
        return "it is too long for one line of config.ini";
    return NULL;
}

static const BrowserPlace PLACES[] = {
    { "Pictures", "/home/me/Pictures", false },
    { "Home", "/home/me", false },
    { "/", "/", false }
};

// A function to test the rows of a folder: folders first, then images, each sorted without regard
// to case, with hidden files and other files left out, and the cursor on the image it opened at
static void test_rows_and_start(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures/zebra.png", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures");
    CHECK_INT(browser_row_count(browser), 5);
    CHECK_STR(browser_row(browser, 0)->name, "Autumn");
    CHECK_INT(browser_row(browser, 0)->kind, BROWSER_ROW_FOLDER);
    CHECK_STR(browser_row(browser, 1)->name, "Birthdays");
    CHECK_STR(browser_row(browser, 2)->name, "apple.webp");
    CHECK_STR(browser_row(browser, 3)->name, "beach.JPG");
    CHECK_STR(browser_row(browser, 4)->name, "zebra.png");
    CHECK_STR(browser_row(browser, 4)->path, "/home/me/Pictures/zebra.png");
    CHECK_INT(browser_cursor(browser), 4);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_CHOSEN);
    CHECK_STR(browser_chosen(browser), "/home/me/Pictures/zebra.png");
    browser_free(browser);
}

// A function to test going into folders and back up to the places, and out
static void test_into_and_out_of_folders(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures/zebra.png", PLACES, 3, fake_list, NULL, NULL, NULL);
    for (int i = 0; i < 4; i++)
        browser_command(browser, BROWSER_UP, 10);
    CHECK_INT(browser_cursor(browser), 0);
    CHECK_INT(browser_command(browser, BROWSER_UP, 10), BROWSER_NONE);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_MOVED);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures/Autumn");
    CHECK_INT(browser_row_count(browser), 2);

    // Back comes out a folder at a time, with the cursor on the folder it came out of
    CHECK_INT(browser_command(browser, BROWSER_BACK, 10), BROWSER_MOVED);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures");
    CHECK_INT(browser_cursor(browser), 0);
    browser_command(browser, BROWSER_BACK, 10);
    CHECK_STR(browser_folder(browser), "/home/me");
    CHECK_STR(browser_row(browser, browser_cursor(browser))->name, "Pictures");
    CHECK_INT(browser_row_count(browser), 2);                  // .cache is hidden
    browser_command(browser, BROWSER_BACK, 10);
    CHECK_STR(browser_folder(browser), "/home");
    browser_command(browser, BROWSER_BACK, 10);
    CHECK_STR(browser_folder(browser), "/");

    // Above the root: the places, with the cursor on the one it came from; Back again closes
    browser_command(browser, BROWSER_BACK, 10);
    CHECK(browser_folder(browser) == NULL);
    CHECK_INT(browser_row_count(browser), 3);
    CHECK_INT(browser_row(browser, 0)->kind, BROWSER_ROW_PLACE);
    CHECK_INT(browser_cursor(browser), 2);
    CHECK_INT(browser_command(browser, BROWSER_BACK, 10), BROWSER_CLOSED);
    browser_free(browser);
}

// A function to test the folder mode: Use this folder needs two images, and images cannot be chosen
static void test_folder_mode(void)
{
    Browser *browser = browser_open(BROWSER_FOLDER, "/home/me/Pictures/Autumn", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_INT(browser_row(browser, 0)->kind, BROWSER_ROW_USE_FOLDER);
    CHECK_INT(browser_row(browser, 0)->image_count, 2);
    CHECK(browser_row(browser, 0)->enabled);
    CHECK(!browser_row(browser, 1)->enabled);                  // An image, shown but not chosen
    CHECK_INT(browser_cursor(browser), 0);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_CHOSEN);
    CHECK_STR(browser_chosen(browser), "/home/me/Pictures/Autumn");
    browser_free(browser);

    browser = browser_open(BROWSER_FOLDER, "/home/me/Pictures/Birthdays", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK(!browser_row(browser, 0)->enabled);
    CHECK(browser_row(browser, 0)->why != NULL && strstr(browser_row(browser, 0)->why, "2 or more") != NULL);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_NONE);
    browser_command(browser, BROWSER_DOWN, 10);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_NONE);
    browser_free(browser);
}

// A function to test where the browser opens when there is no start, or it is gone
static void test_start_places(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures");
    browser_free(browser);
    browser = browser_open(BROWSER_IMAGE, "/gone/away.png", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures");
    browser_free(browser);
    static const BrowserPlace gone[] = { { "Gone", "/gone", false } };
    browser = browser_open(BROWSER_IMAGE, "", gone, 1, fake_list, NULL, NULL, NULL);
    CHECK(browser_folder(browser) == NULL);
    CHECK_INT(browser_row_count(browser), 1);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_NONE);   // It cannot be listed
    browser_free(browser);
}

// A function to test moving a page at a time
static void test_paging(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_INT(browser_cursor(browser), 0);
    CHECK_INT(browser_command(browser, BROWSER_PAGE_DOWN, 2), BROWSER_MOVED);
    CHECK_INT(browser_cursor(browser), 2);
    browser_command(browser, BROWSER_PAGE_DOWN, 2);
    CHECK_INT(browser_cursor(browser), 4);
    CHECK_INT(browser_command(browser, BROWSER_PAGE_DOWN, 2), BROWSER_NONE);
    browser_command(browser, BROWSER_PAGE_UP, 3);
    CHECK_INT(browser_cursor(browser), 1);
    browser_free(browser);
}

// A function to test that a path config.ini cannot hold is shown, but refused with the reason
static void test_refused_paths(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures/zebra.png", PLACES, 3, fake_list, fake_check, NULL, NULL);
    const BrowserRow *row = browser_row(browser, browser_cursor(browser));
    CHECK_STR(row->name, "zebra.png");
    CHECK(!row->enabled);
    CHECK(row->why != NULL && strstr(row->why, "too long") != NULL);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_NONE);
    browser_free(browser);
    browser = browser_open(BROWSER_FOLDER, "/home/me/Pictures/Autumn", PLACES, 3, fake_list, fake_check, NULL, NULL);
    CHECK(!browser_row(browser, 0)->enabled);
    CHECK(strstr(browser_row(browser, 0)->why, "too long") != NULL);
    browser_free(browser);
}

// A function to test parent folders in both path styles
static void test_parent(void)
{
    char out[BROWSER_PATH_MAX];
    CHECK(!browser_parent("/", out, sizeof(out)));
    CHECK(browser_parent("/home", out, sizeof(out)));
    CHECK_STR(out, "/");
    CHECK(browser_parent("/home/me/", out, sizeof(out)));
    CHECK_STR(out, "/home");
    CHECK(!browser_parent("C:\\", out, sizeof(out)));
    CHECK(!browser_parent("C:", out, sizeof(out)));
    CHECK(browser_parent("C:\\Users", out, sizeof(out)));
    CHECK_STR(out, "C:\\");
    CHECK(browser_parent("C:\\Users\\me\\", out, sizeof(out)));
    CHECK_STR(out, "C:\\Users");
    CHECK(!browser_parent("\\\\server\\share", out, sizeof(out)));
    CHECK(browser_parent("\\\\server\\share\\dir", out, sizeof(out)));
    CHECK_STR(out, "\\\\server\\share");
    CHECK(!browser_parent("relative", out, sizeof(out)));
}

// A function to test finding a folder's first image, for previewing a folder
static void test_first_image(void)
{
    Browser *browser = browser_open(BROWSER_FOLDER, "/home/me/Pictures", PLACES, 3, fake_list, NULL, NULL, NULL);
    char out[BROWSER_PATH_MAX];
    CHECK(browser_first_image(browser, "/home/me/Pictures", out, sizeof(out)));
    CHECK_STR(out, "/home/me/Pictures/apple.webp");
    CHECK(browser_first_image(browser, "/home/me/Pictures/Autumn", out, sizeof(out)));
    CHECK_STR(out, "/home/me/Pictures/Autumn/a.jpg");
    CHECK(!browser_first_image(browser, "/media/usb", out, sizeof(out)));
    browser_free(browser);
}

// A function to test Windows paths: joined with backslashes, and up to the drive's root
static void test_windows_paths(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "C:\\Users\\me\\Pictures\\trip.png", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_row(browser, browser_cursor(browser))->path, "C:\\Users\\me\\Pictures\\trip.png");
    browser_command(browser, BROWSER_BACK, 10);
    CHECK_STR(browser_folder(browser), "C:\\Users\\me");
    browser_command(browser, BROWSER_BACK, 10);
    browser_command(browser, BROWSER_BACK, 10);
    CHECK_STR(browser_folder(browser), "C:\\");
    CHECK_STR(browser_row(browser, 0)->path, "C:\\Users");
    browser_free(browser);
}

// A function to test telling images by their extension, whatever its case
static void test_is_image(void)
{
    CHECK(browser_is_image("a.jpg"));
    CHECK(browser_is_image("a.JPEG"));
    CHECK(browser_is_image("a.Png"));
    CHECK(browser_is_image("a.webp"));
    CHECK(!browser_is_image("a.gif"));
    CHECK(!browser_is_image(".png"));
    CHECK(!browser_is_image("png"));
}

// A function to test the rule every image scan uses (the browser, the Folder row's count and both
// platforms' slideshows): a regular file with an image's extension in any case, never a folder, a
// hidden file, or a pipe, socket or device named like an image
static void test_is_image_file(void)
{
    FileioEntry camera = { .name = "DSC_0001.JPG", .is_dir = false, .is_file = true, .hidden = false };
    FileioEntry mixed = { .name = "Beach.Png", .is_dir = false, .is_file = true, .hidden = false };
    FileioEntry apple_double = { .name = "._DSC_0001.JPG", .is_dir = false, .is_file = true, .hidden = true };
    FileioEntry hidden_attribute = { .name = "thumb.jpg", .is_dir = false, .is_file = true, .hidden = true };
    FileioEntry folder = { .name = "Holiday.jpg", .is_dir = true, .is_file = false, .hidden = false };
    FileioEntry notes = { .name = "notes.txt", .is_dir = false, .is_file = true, .hidden = false };
    FileioEntry pipe = { .name = "feed.png", .is_dir = false, .is_file = false, .hidden = false };
    CHECK(browser_is_image_file(&camera));
    CHECK(browser_is_image_file(&mixed));
    CHECK(!browser_is_image_file(&apple_double));
    CHECK(!browser_is_image_file(&hidden_attribute));
    CHECK(!browser_is_image_file(&folder));
    CHECK(!browser_is_image_file(&notes));
    CHECK(!browser_is_image_file(&pipe));
}

// A function to test a Windows path written with forward slashes ("C:/Users/me/Pictures/trip.png"):
// its rows use forward slashes too, so the image it started at is highlighted, and choosing gives a
// path in one style
static void test_forward_slash_windows_paths(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "C:/Users/me/Pictures/trip.png", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_folder(browser), "C:/Users/me/Pictures");
    CHECK_INT(browser_cursor(browser), 0);
    CHECK_STR(browser_row(browser, 0)->path, "C:/Users/me/Pictures/trip.png");
    CHECK_STR(browser_row(browser, 1)->path, "C:/Users/me/Pictures/x.jpg");
    browser_command(browser, BROWSER_DOWN, 10);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_CHOSEN);
    CHECK_STR(browser_chosen(browser), "C:/Users/me/Pictures/x.jpg");
    CHECK_INT(browser_command(browser, BROWSER_BACK, 10), BROWSER_MOVED);
    CHECK_STR(browser_folder(browser), "C:/Users/me");
    CHECK_STR(browser_row(browser, browser_cursor(browser))->path, "C:/Users/me/Pictures");
    browser_free(browser);

    // A bare drive has no separator of its own, so it gets Windows' own
    static const BrowserPlace drive[] = { { "C:", "C:", false } };
    browser = browser_open(BROWSER_IMAGE, "", drive, 1, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_folder(browser), "C:");
    CHECK_STR(browser_row(browser, 0)->path, "C:\\Users");
    browser_free(browser);
}

// Names too long for a path of BROWSER_PATH_MAX bytes: a folder, and an image beside it
static char long_folder[1100];
static char long_image[1100];
static char deep_path[1200];

// A function to list a pretend file system of paths too long to choose
static int deep_list(const char *folder, FileioEntry **entries, void *context)
{
    (void) context;
    const char *names[3];
    bool dirs[3] = { false, false, false };
    int count;
    if (strcmp(folder, "/deep") == 0) {
        names[0] = long_folder;
        dirs[0] = true;
        names[1] = long_image;
        names[2] = "ok.png";
        count = 3;
    }
    else if (strcmp(folder, deep_path) == 0) {
        names[0] = "a.png";
        names[1] = "b.png";
        count = 2;
    }
    else
        return -1;
    *entries = calloc((size_t) count, sizeof(FileioEntry));
    for (int i = 0; i < count; i++) {
        size_t size = strlen(names[i]) + 1;
        char *name = malloc(size);
        memcpy(name, names[i], size);
        (*entries)[i] = (FileioEntry) { .name = name, .is_dir = dirs[i], .is_file = !dirs[i], .hidden = false };
    }
    return count;
}

// A function to test that a path too long to keep is still shown, and refused with the reason,
// rather than left out; and that a folder that deep still opens, and Back comes out of it
static void test_paths_too_long_to_choose(void)
{
    memset(long_folder, 'y', 1030);
    long_folder[1030] = '\0';
    memset(long_image, 'x', 1030);
    snprintf(long_image + 1030, sizeof(long_image) - 1030, ".png");
    snprintf(deep_path, sizeof(deep_path), "/deep/%s", long_folder);
    static const BrowserPlace deep[] = { { "Deep", "/deep", false } };

    Browser *browser = browser_open(BROWSER_IMAGE, "", deep, 1, deep_list, fake_check, NULL, NULL);
    CHECK_INT(browser_row_count(browser), 3);
    CHECK_STR(browser_row(browser, 0)->path, deep_path);
    CHECK(browser_row(browser, 0)->enabled);
    CHECK_STR(browser_row(browser, 1)->name, "ok.png");
    CHECK(browser_row(browser, 1)->enabled);
    CHECK_STR(browser_row(browser, 2)->name, long_image);
    CHECK(!browser_row(browser, 2)->enabled);
    CHECK(browser_row(browser, 2)->why != NULL && strstr(browser_row(browser, 2)->why, "too long") != NULL);
    browser_command(browser, BROWSER_PAGE_DOWN, 10);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_NONE);

    // The deep folder opens at its whole path, and Back comes out onto it
    browser_command(browser, BROWSER_PAGE_UP, 10);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_MOVED);
    CHECK_STR(browser_folder(browser), deep_path);
    CHECK_INT(browser_row_count(browser), 2);
    CHECK(!browser_row(browser, 0)->enabled);
    CHECK(strstr(browser_row(browser, 0)->why, "too long") != NULL);
    CHECK_INT(browser_command(browser, BROWSER_BACK, 10), BROWSER_MOVED);
    CHECK_STR(browser_folder(browser), "/deep");
    CHECK_INT(browser_cursor(browser), 0);
    browser_free(browser);

    // In folder mode, a folder too long to keep cannot be used
    browser = browser_open(BROWSER_FOLDER, "", deep, 1, deep_list, NULL, NULL, NULL);
    browser_command(browser, BROWSER_DOWN, 10);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_MOVED);
    CHECK_INT(browser_row(browser, 0)->kind, BROWSER_ROW_USE_FOLDER);
    CHECK(!browser_row(browser, 0)->enabled);
    CHECK(strstr(browser_row(browser, 0)->why, "too long") != NULL);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_NONE);
    browser_free(browser);
}

// A function to test an empty folder in image mode: no rows, and every key but Back does nothing
static void test_empty_folder(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/media/usb", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_folder(browser), "/media/usb");
    CHECK_INT(browser_row_count(browser), 0);
    CHECK(browser_row(browser, 0) == NULL);
    CHECK_INT(browser_command(browser, BROWSER_UP, 10), BROWSER_NONE);
    CHECK_INT(browser_command(browser, BROWSER_DOWN, 10), BROWSER_NONE);
    CHECK_INT(browser_command(browser, BROWSER_PAGE_UP, 10), BROWSER_NONE);
    CHECK_INT(browser_command(browser, BROWSER_PAGE_DOWN, 10), BROWSER_NONE);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_NONE);
    CHECK_INT(browser_cursor(browser), 0);
    CHECK_INT(browser_command(browser, BROWSER_BACK, 10), BROWSER_MOVED);
    CHECK_STR(browser_folder(browser), "/media");
    CHECK_STR(browser_row(browser, browser_cursor(browser))->path, "/media/usb");
    browser_free(browser);
}

// A function to test Back from a folder whose parent cannot be listed: out to the places
static void test_back_to_an_unlisted_parent(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/orphan/child/a.png", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_folder(browser), "/orphan/child");
    CHECK_INT(browser_command(browser, BROWSER_BACK, 10), BROWSER_MOVED);
    CHECK(browser_folder(browser) == NULL);
    CHECK_INT(browser_row_count(browser), 3);
    CHECK_INT(browser_cursor(browser), 0);
    browser_free(browser);
}

// A function to test that a page of fewer than one row moves one row
static void test_page_rows_below_one(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_INT(browser_command(browser, BROWSER_PAGE_DOWN, 0), BROWSER_MOVED);
    CHECK_INT(browser_cursor(browser), 1);
    CHECK_INT(browser_command(browser, BROWSER_PAGE_DOWN, -5), BROWSER_MOVED);
    CHECK_INT(browser_cursor(browser), 2);
    CHECK_INT(browser_command(browser, BROWSER_PAGE_UP, 0), BROWSER_MOVED);
    CHECK_INT(browser_cursor(browser), 1);
    browser_free(browser);
}

// A function to test a start folder written with a trailing separator: it opens as the folder
static void test_start_with_a_trailing_separator(void)
{
    Browser *browser = browser_open(BROWSER_FOLDER, "/home/me/Pictures/Autumn/", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures/Autumn");
    CHECK_STR(browser_row(browser, 0)->path, "/home/me/Pictures/Autumn");
    browser_free(browser);
    browser = browser_open(BROWSER_FOLDER, "C:\\Users\\me\\Pictures\\", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_folder(browser), "C:\\Users\\me\\Pictures");
    browser_free(browser);
    browser = browser_open(BROWSER_FOLDER, "/", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_folder(browser), "/");
    browser_free(browser);
}

// A function to test that a place on a network share is never opened unasked: the browser starts
// at the first local place it can list, or at the places
static void test_network_places_are_not_opened_unasked(void)
{
    static const BrowserPlace mixed[] = {
        { "NAS", "/nas", true },
        { "Pictures", "/home/me/Pictures", false }
    };
    Browser *browser = browser_open(BROWSER_IMAGE, "", mixed, 2, fake_list, NULL, NULL, NULL);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures");
    browser_free(browser);
    browser = browser_open(BROWSER_IMAGE, "", mixed, 1, fake_list, NULL, NULL, NULL);
    CHECK(browser_folder(browser) == NULL);
    CHECK_INT(browser_row_count(browser), 1);
    CHECK_INT(browser_command(browser, BROWSER_OK, 10), BROWSER_MOVED);   // Asked for, it opens
    CHECK_STR(browser_folder(browser), "/nas");
    browser_free(browser);
}

// A function to test file mode: every file that is not hidden, after the folders, whatever its kind;
// opened at a file, its folder with it highlighted; OK on a file chooses it
static void test_file_mode(void)
{
    Browser *browser = browser_open(BROWSER_FILE, "/home/me/Pictures/notes.txt", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK(browser != NULL);
    CHECK_STR(browser_folder(browser), "/home/me/Pictures");
    CHECK_INT(browser_row_count(browser), 6);
    CHECK_INT(browser_row(browser, 0)->kind, BROWSER_ROW_FOLDER);
    CHECK_STR(browser_row(browser, 0)->name, "Autumn");
    CHECK_STR(browser_row(browser, 1)->name, "Birthdays");
    CHECK_INT(browser_row(browser, 2)->kind, BROWSER_ROW_FILE);
    CHECK_STR(browser_row(browser, 2)->name, "apple.webp");
    CHECK_STR(browser_row(browser, 4)->name, "notes.txt");        // Not an image, and listed
    CHECK_STR(browser_row(browser, 5)->name, "zebra.png");        // .hidden.png is left out
    CHECK_INT(browser_cursor(browser), 4);
    CHECK_INT(browser_command(browser, BROWSER_DOWN, 5), BROWSER_MOVED);
    CHECK_INT(browser_command(browser, BROWSER_OK, 5), BROWSER_CHOSEN);
    CHECK_STR(browser_chosen(browser), "/home/me/Pictures/zebra.png");
    browser_free(browser);
}

// A function to test what file mode shares with image mode, and what it does not: a file config.ini
// cannot hold is shown, refused with the reason; image rows stay images in image and folder mode,
// chosen only in image mode
static void test_file_mode_refusals_and_row_kinds(void)
{
    Browser *browser = browser_open(BROWSER_FILE, "/home/me/Pictures/notes.txt", PLACES, 3, fake_list, fake_check, NULL, NULL);
    const BrowserRow *row = browser_row(browser, 5);
    CHECK_STR(row->name, "zebra.png");
    CHECK_INT(row->kind, BROWSER_ROW_FILE);
    CHECK(!row->enabled);
    CHECK(row->why != NULL && strstr(row->why, "too long") != NULL);
    CHECK(browser_row(browser, 4)->enabled);
    CHECK(browser_row(browser, 4)->why == NULL);
    browser_free(browser);

    browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures/zebra.png", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_INT(browser_row(browser, 2)->kind, BROWSER_ROW_IMAGE);
    CHECK(browser_row(browser, 2)->enabled);
    browser_free(browser);

    browser = browser_open(BROWSER_FOLDER, "/home/me/Pictures", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_INT(browser_row_count(browser), 6);   // Use this folder, two folders and three images: no notes.txt
    CHECK_INT(browser_row(browser, 3)->kind, BROWSER_ROW_IMAGE);
    CHECK(!browser_row(browser, 3)->enabled);
    browser_free(browser);
}

// A function to test that file mode lists only regular files: a pipe is left out, even one named as
// an image (a read of it at the next start could wait for good); image mode leaves it out too
static void test_file_mode_lists_only_regular_files(void)
{
    Browser *browser = browser_open(BROWSER_FILE, "/home/me/pads/old.txt", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_INT(browser_row_count(browser), 3);
    CHECK_STR(browser_row(browser, 0)->name, "sub");
    CHECK_STR(browser_row(browser, 1)->name, "a.txt");
    CHECK_STR(browser_row(browser, 2)->name, "old.txt");
    CHECK_INT(browser_cursor(browser), 2);
    browser_free(browser);

    browser = browser_open(BROWSER_IMAGE, "/home/me/pads", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_INT(browser_row_count(browser), 1);
    CHECK_STR(browser_row(browser, 0)->name, "sub");
    browser_free(browser);
}

// A function to test that image mode, a folder's preview and folder mode's count take only regular
// files as images: a pipe named a.png (first by name) is left out, and the picture beside it is not.
// Highlighting the pipe would have its decode wait for good, and the screen with it.
static void test_images_are_regular_files(void)
{
    Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/piped/b.png", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_INT(browser_row_count(browser), 1);
    CHECK_STR(browser_row(browser, 0)->name, "b.png");
    CHECK_INT(browser_cursor(browser), 0);
    browser_free(browser);

    browser = browser_open(BROWSER_FOLDER, "/home/me/piped", PLACES, 3, fake_list, NULL, NULL, NULL);
    CHECK_INT(browser_row(browser, 0)->kind, BROWSER_ROW_USE_FOLDER);
    CHECK_INT(browser_row(browser, 0)->image_count, 1);
    char out[BROWSER_PATH_MAX] = "";
    CHECK(browser_first_image(browser, "/home/me/piped", out, sizeof(out)));
    CHECK_STR(out, "/home/me/piped/b.png");
    CHECK(!browser_first_image(browser, "/home/me/pipeonly", out, sizeof(out)));
    browser_free(browser);
}

int main(void)
{
    test_rows_and_start();
    test_into_and_out_of_folders();
    test_folder_mode();
    test_start_places();
    test_paging();
    test_refused_paths();
    test_parent();
    test_first_image();
    test_windows_paths();
    test_is_image();
    test_is_image_file();
    test_forward_slash_windows_paths();
    test_paths_too_long_to_choose();
    test_empty_folder();
    test_back_to_an_unlisted_parent();
    test_page_rows_below_one();
    test_start_with_a_trailing_separator();
    test_network_places_are_not_opened_unasked();
    test_file_mode();
    test_file_mode_refusals_and_row_kinds();
    test_file_mode_lists_only_regular_files();
    test_images_are_regular_files();
    return check_report();
}
