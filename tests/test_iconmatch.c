#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "check.h"
#include "iconmatch.h"
#include "library.h"

static const char *const PRIME[] = { "prime video for windows" };
static const char *const KODI[] = { "xbmc" };
static const char *const MAX[] = { "max" };
static const char *const ESDE[] = { "emulationstation desktop edition", "emulationstation de" };

// A library as the manifest gives one, in its order: generic icons first, then brands
static const LibraryInfo ICONS[] = {
    { "music", "Music", "media", NULL, 0 },
    { "tv-shows", "TV shows", "media", NULL, 0 },
    { "apple-music", "Apple Music", "music", NULL, 0 },
    { "apple-tv", "Apple TV", "video", NULL, 0 },
    { "amazon-music", "Amazon Music", "music", NULL, 0 },
    { "prime-video", "Prime Video", "video", PRIME, 1 },
    { "chrome", "Chrome", "web", NULL, 0 },
    { "kodi", "Kodi", "servers", KODI, 1 },
    { "hbo-max", "HBO Max", "video", MAX, 1 },
    { "es-de", "ES-DE", "games", ESDE, 2 },
    { "disney-plus", "Disney+", "video", NULL, 0 }
};

// A function to normalize a name as an app's is (a static buffer)
static const char *normalized(const char *name, size_t size)
{
    static char out[ICONMATCH_KEY_MAX];
    iconmatch_normalize(name, out, size);
    return out;
}

// A function to find an app's icon in ICONS
static int found(const char *app)
{
    return iconmatch_find(app, ICONS, (int) (sizeof(ICONS) / sizeof(ICONS[0])));
}

// A function to test normalizing an app's name: lower case, punctuation a break between words, an
// apostrophe dropped within its word, vendor words dropped as whole words, other scripts kept
static void test_normalize(void)
{
    CHECK_STR(normalized("Google Chrome", ICONMATCH_KEY_MAX), "chrome");
    CHECK_STR(normalized("GOOGLE   CHROME", ICONMATCH_KEY_MAX), "chrome");
    CHECK_STR(normalized("Mozilla Firefox", ICONMATCH_KEY_MAX), "firefox");
    CHECK_STR(normalized("Apple Music", ICONMATCH_KEY_MAX), "music");
    CHECK_STR(normalized("Amazon Prime Video", ICONMATCH_KEY_MAX), "prime video");
    CHECK_STR(normalized("NVIDIA GeForce NOW", ICONMATCH_KEY_MAX), "geforce now");
    CHECK_STR(normalized("Valve Hammer Editor", ICONMATCH_KEY_MAX), "hammer editor");
    CHECK_STR(normalized("Microsoft Edge\xC2\xAE", ICONMATCH_KEY_MAX), "edge");        // The registered sign
    CHECK_STR(normalized("Halo\xE2\x84\xA2Infinite", ICONMATCH_KEY_MAX), "halo infinite");   // The trade mark sign
    CHECK_STR(normalized("Steam (Runtime)", ICONMATCH_KEY_MAX), "steam runtime");
    CHECK_STR(normalized("Disney+", ICONMATCH_KEY_MAX), "disney");
    CHECK_STR(normalized("ES-DE", ICONMATCH_KEY_MAX), "es de");
    CHECK_STR(normalized("  VLC   media player  ", ICONMATCH_KEY_MAX), "vlc media player");
    CHECK_STR(normalized("Assassin's Creed", ICONMATCH_KEY_MAX), "assassins creed");
    CHECK_STR(normalized("Assassin\xE2\x80\x99s Creed", ICONMATCH_KEY_MAX), "assassins creed");   // U+2019
    CHECK_STR(normalized("Pok\xC3\xA9mon", ICONMATCH_KEY_MAX), "pok\xC3\xA9mon");   // Other letters stay as they are
    CHECK_STR(normalized("Googleplex", ICONMATCH_KEY_MAX), "googleplex");           // A vendor only as a whole word
    CHECK_STR(normalized("Microsoft", ICONMATCH_KEY_MAX), "");
    CHECK_STR(normalized("", ICONMATCH_KEY_MAX), "");
    CHECK_STR(normalized(NULL, ICONMATCH_KEY_MAX), "");
    CHECK_STR(normalized("Google Chrome", 6), "chrom");                             // Cut to the room given
}

// A function to test finding an app's icon: by name, title or alias, the app's name as it is first and
// without its vendor second, the icon's side always as it is; the first in the manifest's order; -1 for
// none, where the caller falls back to the library's apps icon
static void test_find(void)
{
    CHECK_INT(found("Apple Music"), 2);                          // Not Music: as it is, first
    CHECK_INT(found("Music"), 0);
    CHECK_INT(found("Amazon Music"), 4);
    CHECK_INT(found("Amazon Prime Video"), 5);                   // Without its vendor
    CHECK_INT(found("Prime Video for Windows"), 5);              // An alias
    CHECK_INT(found("Google Chrome"), 6);
    CHECK_INT(found("chrome"), 6);                               // The icon's name
    CHECK_INT(found("XBMC"), 7);
    CHECK_INT(found("Max"), 8);
    CHECK_INT(found("EmulationStation Desktop Edition"), 9);
    CHECK_INT(found("ES-DE"), 9);
    CHECK_INT(found("Disney+"), 10);                             // The title, "disney"
    CHECK_INT(found("Disney Plus"), 10);                         // The name, "disney plus"
    CHECK_INT(found("TV Shows"), 1);
    CHECK_INT(found("Google TV"), -1);                           // Apple TV keeps its vendor: never "tv"
    CHECK_INT(found("Mozilla Firefox"), -1);                     // No icon: the apps fallback is the caller's
    CHECK_INT(found("Microsoft"), -1);                           // Nothing left to compare
    CHECK_INT(found(""), -1);
    CHECK_INT(found(NULL), -1);
    CHECK_INT(iconmatch_find("Kodi", ICONS, 0), -1);
    CHECK_INT(iconmatch_find("Kodi", NULL, 0), -1);

    // Two icons the app's name matches alike: the first in the manifest's order
    static const LibraryInfo SAME[] = { { "a", "Same", "", NULL, 0 }, { "b", "Same", "", NULL, 0 } };
    CHECK_INT(iconmatch_find("Same", SAME, 2), 0);
}

// A function to find a loaded icon by its name; -1 when the library has none of that name
static int index_of(const char *name)
{
    for (int i = 0; i < library_count(); i++) {
        if (strcmp(library_info(i)->name, name) == 0)
            return i;
    }
    return -1;
}

// A function to test the shipped manifest, as build-library.py writes it: the names apps install under
// find their icons through its titles and match = aliases, an app the library has no icon for finds
// none, and every icon's title and alias finds that icon, never one before it
static void test_shipped_library(void)
{
    CHECK(library_load(LIBRARY_ROOT) > 0);
    int count = library_count();
    const LibraryInfo *icons = library_info(0);
    static const struct { const char *app; const char *icon; } cases[] = {
        { "Google Chrome", "chrome" }, { "Apple Music", "apple-music" }, { "Apple TV", "apple-tv" },
        { "Amazon Prime Video", "prime-video" }, { "Prime Video for Windows", "prime-video" }, { "Max", "hbo-max" },
        { "Disney+", "disney-plus" }, { "Peacock TV", "peacock" }, { "Tubi TV", "tubi" }, { "Plex HTPC", "plex" },
        { "Plex Media Server", "plex" }, { "Jellyfin Media Player", "jellyfin" }, { "XBMC", "kodi" },
        { "VLC media player", "vlc" }, { "Steam (Runtime)", "steam" }, { "NVIDIA GeForce NOW", "geforce-now" },
        { "Moonlight Game Streaming", "moonlight" }, { "EmulationStation Desktop Edition", "es-de" },
        { "Windows Terminal", "terminal" }, { "Command Prompt", "terminal" }, { "File Explorer", "folder" },
        { "Files", "folder" }, { "Control Panel", "settings" }, { "System Settings", "settings" },
        { "Photos", "photos" }, { "Mozilla Firefox", NULL }, { "Microsoft Edge", NULL }, { "Google TV", NULL }
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        int expected = cases[c].icon != NULL ? index_of(cases[c].icon) : -1;
        int got = iconmatch_find(cases[c].app, icons, count);
        if (got != expected)
            fprintf(stderr, "  '%s' found %s, expected %s\n", cases[c].app, got >= 0 ? icons[got].name : "none",
                    cases[c].icon != NULL ? cases[c].icon : "none");
        CHECK(cases[c].icon == NULL || expected >= 0);
        CHECK_INT(got, expected);
    }
    for (int i = 0; i < count; i++) {
        if (iconmatch_find(icons[i].title, icons, count) != i)
            fprintf(stderr, "  the title '%s' does not find [%s]\n", icons[i].title, icons[i].name);
        CHECK_INT(iconmatch_find(icons[i].title, icons, count), i);
        for (int a = 0; a < icons[i].match_count; a++)
            CHECK_INT(iconmatch_find(icons[i].match[a], icons, count), i);
    }
    library_free();
}

int main(void)
{
    test_normalize();
    test_find();
    test_shipped_library();
    return check_report();
}
