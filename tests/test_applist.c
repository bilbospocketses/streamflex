#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "applist.h"

// A function to read a .desktop file's text, as the scan reads each file: 1 an app, 0 skipped, -1 out of
// memory (which only test_alloc can make)
static int parse(const char *path, const char *text, const char *lang, AppItem *out)
{
    return applist_parse_desktop(path, text, strlen(text), lang, out);
}

// A function to read a .desktop text's name in a locale; "(skipped)" when it is not read as an app
static const char *name_in(const char *text, const char *lang)
{
    static char name[128];
    AppItem item;
    if (parse("/usr/share/applications/x.desktop", text, lang, &item) != 1)
        return "(skipped)";
    snprintf(name, sizeof(name), "%s", item.name);
    applist_item_clear(&item);
    return name;
}

// A function to give an app's name in a list, or "" past its end
static const char *name_at(const AppList *list, int index)
{
    const AppItem *item = applist_item(list, index);
    return item != NULL ? item->name : "";
}

// A function to give an app's command in a list, or "" past its end
static const char *command_at(const AppList *list, int index)
{
    const AppItem *item = applist_item(list, index);
    return item != NULL ? item->command : "";
}

// A function to test reading a .desktop file: its name, its path as the command, its Icon= value; keys
// as the Desktop Entry Specification reads them (white space before a line and around '=', CRLF line
// ends, comments, the first of a key repeated, another group's keys left out, escapes read); booleans as
// GLib reads them; and the item cleared after
static void test_parse(void)
{
    AppItem item;
    CHECK_INT(parse("/usr/share/applications/kodi.desktop",
                    "[Desktop Entry]\nType=Application\nName=Kodi\nIcon=kodi\nExec=kodi\n", "en_US.UTF-8", &item), 1);
    CHECK_STR(item.name, "Kodi");
    CHECK_STR(item.command, "/usr/share/applications/kodi.desktop");
    CHECK_STR(item.icon, "kodi");
    applist_item_clear(&item);
    CHECK(item.name == NULL && item.command == NULL && item.icon == NULL);

    CHECK_INT(parse("/opt/mc.desktop", "# a comment\r\n[Desktop Entry]\r\nType = Application\r\nName = Media\\sCenter\\t2\r\n"
                    "Name=Second\r\nExec = mc %U\r\nIcon = /opt/mc/icon.png\r\n[Desktop Action play]\r\nName=Play\r\nIcon=play\r\n",
                    "C", &item), 1);
    CHECK_STR(item.name, "Media Center 2");
    CHECK_STR(item.icon, "/opt/mc/icon.png");
    applist_item_clear(&item);

    // A backslash written twice is one; spaces at either end go; an empty Icon= is no icon; NoDisplay,
    // Hidden and Terminal false keep the app
    CHECK_INT(parse("/a.desktop", "[Desktop Entry]\nType=Application\nName=  Back\\\\slash  \nIcon=\nExec=a\n"
                    "NoDisplay=false\nHidden=false\nTerminal=false\n", "C", &item), 1);
    CHECK_STR(item.name, "Back\\slash");
    CHECK(item.icon == NULL);
    applist_item_clear(&item);

    // White space before a line is dropped, as GLib drops it (a space, \t, \v, \f): an indented group,
    // key or comment is read as one
    CHECK_INT(parse("/a.desktop", " \t# an indented comment\n \t[Desktop Entry]\n\vType=Application\n \fName=Indented\n"
                    "  Exec=a\n", "C", &item), 1);
    CHECK_STR(item.name, "Indented");
    applist_item_clear(&item);
    CHECK_STR(name_in("[Desktop Entry]\nType=Application\nExec=a\n\tName=Tabbed\nName=Second\n", "C"), "Tabbed");

    // A tab around '=' is dropped as a space is
    CHECK_INT(parse("/a.desktop", "[Desktop Entry]\nType\t=\tApplication\nName \t=\t Tabs\nExec\t=\ta\nIcon\t= \ttabs\n", "C",
                    &item), 1);
    CHECK_STR(item.name, "Tabs");
    CHECK_STR(item.icon, "tabs");
    applist_item_clear(&item);

    // A boolean keeps its case, as GLib reads it: True and TRUE are not true
    CHECK_INT(parse("/a.desktop", "[Desktop Entry]\nType=Application\nName=A\nExec=a\nNoDisplay=True\nHidden=TRUE\n", "C",
                    &item), 1);
    applist_item_clear(&item);

    // An action group's NoDisplay is not the entry's
    CHECK_INT(parse("/a.desktop", "[Desktop Entry]\nType=Application\nName=A\nExec=a\n[Desktop Action hide]\nNoDisplay=true\n"
                    "Exec=a --hide\n", "C", &item), 1);
    applist_item_clear(&item);

    // \n, \r and \t read as a space, as a tile's one line needs; an unknown escape keeps its backslash
    // (GLib rejects the value); an Icon= that is only an escaped space is no icon
    CHECK_INT(parse("/a.desktop", "[Desktop Entry]\nType=Application\nName=A\\nB\\rC\\tD \\q\nIcon=\\s\nExec=a\n", "C", &item), 1);
    CHECK_STR(item.name, "A B C D \\q");
    CHECK(item.icon == NULL);
    applist_item_clear(&item);
}

// A function to test a text with no terminator whose last byte is a lone backslash ending a value: the
// backslash is kept, and nothing past the text is read (ASan reports a read past an exact-size buffer)
static void test_unterminated(void)
{
    static const char desktop[] = "[Desktop Entry]\nType=Application\nExec=a\nName=A\\";
    size_t length = sizeof(desktop) - 1;
    char *text = malloc(length);
    CHECK(text != NULL);
    if (text == NULL)
        return;
    memcpy(text, desktop, length);
    AppItem item;
    CHECK_INT(applist_parse_desktop("/a.desktop", text, length, "C", &item), 1);
    CHECK_STR(item.name, "A\\");
    applist_item_clear(&item);
    free(text);
}

// A function to test the files that are not apps: hidden from menus (NoDisplay, Hidden, either as true
// or as 1, white space after it ignored), run in a terminal (Terminal, which unix.c has none for), not
// an application (Link, Directory, or no Type), with no name or no Exec (unix.c launches nothing without
// one), keys outside [Desktop Entry], an empty file, and a path holding ';', which
// unix.c's start_process() would cut there. Each is 0 (skipped, never -1: nothing ran short) and leaves
// the item empty.
static void test_skipped(void)
{
    static const char *const skipped[] = {
        "[Desktop Entry]\nType=Application\nName=A\nExec=a\nNoDisplay=true\n",
        "[Desktop Entry]\nType=Application\nName=A\nExec=a\nHidden=true\n",
        "[Desktop Entry]\nType=Application\nName=A\nExec=a\nHidden=1\n",
        "[Desktop Entry]\nType=Application\nName=A\nExec=a\nNoDisplay=true \n",    // White space after it, as GLib reads it
        "[Desktop Entry]\nType=Application\nName=A\nExec=a\nNoDisplay=true\t\n",
        "[Desktop Entry]\nType=Application\nName=A\nExec=htop\nTerminal=true\n",
        "[Desktop Entry]\nType=Link\nName=A\nURL=https://example.com\n",
        "[Desktop Entry]\nType=Directory\nName=A\n",
        "[Desktop Entry]\nName=A\nExec=a\n",
        "[Desktop Entry]\nType=Application\nExec=a\n",
        "[Desktop Entry]\nType=Application\nName=A\n",
        "[Desktop Entry]\nType=Application\nName=A\nExec=\n",
        "[Desktop Entry]\nType=Application\nName=   \nExec=a\n",
        "[Desktop Action new]\nType=Application\nName=A\nExec=a\n",
        "Type=Application\nName=A\nExec=a\n",
        "",
        NULL
    };
    AppItem item;
    for (int i = 0; skipped[i] != NULL; i++) {
        CHECK_INT(parse("/a.desktop", skipped[i], "C", &item), 0);
        CHECK(item.name == NULL && item.command == NULL && item.icon == NULL);
    }
    CHECK_INT(parse("/apps/a;b.desktop", "[Desktop Entry]\nType=Application\nName=A\nExec=a\n", "C", &item), 0);
    CHECK(item.name == NULL && item.command == NULL && item.icon == NULL);
    CHECK_INT(applist_parse_desktop(NULL, "", 0, "C", &item), 0);
}

// A function to test the localized name: the best of ll_CC@MOD, ll_CC, ll@MOD and ll, the encoding
// dropped, wherever it stands in the file; Name when none matches, for C and POSIX, and with no locale;
// the first of a key repeated; an empty localized name giving way to Name; and a file with a localized
// name alone, an app only in that locale
static void test_localized(void)
{
    static const char *const gimp = "[Desktop Entry]\nType=Application\nExec=gimp\nName=GIMP\nName[de]=GIMP (de)\n"
        "Name[de_DE]=GIMP Bildbearbeitung\nName[de@euro]=GIMP (de, euro)\nName[de_DE@euro]=GIMP (de_DE, euro)\n"
        "Name[fr]=GIMP (fr)\n";
    CHECK_STR(name_in(gimp, "de_DE.UTF-8@euro"), "GIMP (de_DE, euro)");
    CHECK_STR(name_in(gimp, "de_DE.UTF-8"), "GIMP Bildbearbeitung");
    CHECK_STR(name_in(gimp, "de_DE"), "GIMP Bildbearbeitung");
    CHECK_STR(name_in(gimp, "de_AT.UTF-8@euro"), "GIMP (de, euro)");
    CHECK_STR(name_in(gimp, "de_AT.UTF-8"), "GIMP (de)");
    CHECK_STR(name_in(gimp, "de"), "GIMP (de)");
    CHECK_STR(name_in(gimp, "fr_CA.UTF-8"), "GIMP (fr)");
    CHECK_STR(name_in(gimp, "ja_JP.UTF-8"), "GIMP");
    CHECK_STR(name_in(gimp, "C.UTF-8"), "GIMP");
    CHECK_STR(name_in(gimp, "C"), "GIMP");
    CHECK_STR(name_in(gimp, "POSIX"), "GIMP");
    CHECK_STR(name_in(gimp, ""), "GIMP");
    CHECK_STR(name_in(gimp, NULL), "GIMP");

    CHECK_STR(name_in("[Desktop Entry]\nType=Application\nExec=k\nName[de_DE]=Kodi DE\nName[de]=Kodi de\nName=Kodi\n",
                      "de_DE.UTF-8"), "Kodi DE");
    CHECK_STR(name_in("[Desktop Entry]\nType=Application\nExec=k\nName=Kodi\nName[de]=Erste\nName[de]=Zweite\n", "de_DE"),
              "Erste");
    CHECK_STR(name_in("[Desktop Entry]\nType=Application\nExec=k\nName=Kodi\nName[de]=\n", "de_DE"), "Kodi");
    CHECK_STR(name_in("[Desktop Entry]\nType=Application\nExec=k\nName[de]=Nur deutsch\n", "de_DE"), "Nur deutsch");
    CHECK_STR(name_in("[Desktop Entry]\nType=Application\nExec=k\nName[de]=Nur deutsch\n", "en_US"), "(skipped)");

    // An action group's Name[de] is not the entry's, as Chrome's and Firefox's files carry one
    static const char *const chrome = "[Desktop Entry]\nType=Application\nExec=chrome\nName=Chrome\n"
        "[Desktop Action new-window]\nName=New Window\nName[de]=Neues Fenster\nExec=chrome --new-window\n";
    CHECK_STR(name_in(chrome, "de_DE.UTF-8"), "Chrome");

    // C and POSIX have no localized names, so a Name[C] or Name[POSIX] key is not one
    static const char *const c_named = "[Desktop Entry]\nType=Application\nExec=k\nName=Kodi\nName[C]=Kodi C\n"
        "Name[POSIX]=Kodi POSIX\n";
    CHECK_STR(name_in(c_named, "C"), "Kodi");
    CHECK_STR(name_in(c_named, "C.UTF-8"), "Kodi");
    CHECK_STR(name_in(c_named, "POSIX"), "Kodi");
}

// A function to test the list: what is left out (uninstallers, help and readme entries, documents, web
// links), one app found twice, the order (letters in either case alike, then the exact bytes, then the
// command; other bytes by value), and an index past either end
static void test_list(void)
{
    AppList *list = applist_create();
    CHECK(list != NULL);
    CHECK(applist_add(list, "Steam", "/a/steam.desktop", "steam"));
    CHECK(applist_add(list, "kodi", "/a/kodi.desktop", NULL));
    CHECK(applist_add(list, "\xC3\x89" "cran", "/a/ecran.desktop", "ecran"));
    CHECK(applist_add(list, "Kodi", "/b/kodi.desktop", "kodi"));            // Another command: another install
    CHECK(!applist_add(list, "KODI", "/a/kodi.desktop", "x"));             // The same app, its name in another case
    CHECK(applist_add(list, "Alpha", "/a/alpha.desktop", NULL));
    CHECK(!applist_add(list, "", "/a/x.desktop", NULL));
    CHECK(!applist_add(list, "X", "", NULL));

    static const char *const left_out[] = {
        "Uninstall Foo", "Foo Uninstaller", "Foo uninstallation", "Foo Readme", "Read Me First", "README", "Foo Help",
        "GNOME Help", "Documentation (PDF)", NULL
    };
    for (int i = 0; left_out[i] != NULL; i++)
        CHECK(!applist_add(list, left_out[i], "/a/x.desktop", NULL));
    CHECK(applist_add(list, "Helpdesk", "/a/helpdesk.desktop", NULL));     // Words, not parts of them
    CHECK(applist_add(list, "Read Mentor", "/a/mentor.desktop", NULL));
    CHECK(!applist_add(list, "Foo", "explorer.exe shell:AppsFolder\\{7C5A40EF-A0FB-4BFC-874A-C0F2E0B9FA8E}\\Foo\\unins000.exe", "x"));
    CHECK(!applist_add(list, "Foo notes",
                       "explorer.exe \"shell:AppsFolder\\{6D809377-6AF0-444B-8957-A3773F02200E}\\Foo App\\notes.TXT\"", "x"));
    CHECK(!applist_add(list, "Foo website", "explorer.exe shell:AppsFolder\\https://foo.example/", "x"));
    CHECK(!applist_add(list, "Foo remover", "/usr/share/applications/uninstall-foo.desktop", NULL));
    CHECK(applist_add(list, "Calculator", "explorer.exe shell:AppsFolder\\Microsoft.WindowsCalculator_8wekyb3d8bbwe!App",
                      "Microsoft.WindowsCalculator_8wekyb3d8bbwe!App"));
    CHECK(!applist_failed(list));
    CHECK_INT(applist_count(list), 8);

    applist_sort(list);
    CHECK_STR(name_at(list, 0), "Alpha");
    CHECK_STR(name_at(list, 1), "Calculator");
    CHECK_STR(name_at(list, 2), "Helpdesk");
    CHECK_STR(name_at(list, 3), "Kodi");
    CHECK_STR(command_at(list, 3), "/b/kodi.desktop");
    CHECK_STR(name_at(list, 4), "kodi");
    CHECK_STR(command_at(list, 4), "/a/kodi.desktop");
    CHECK_STR(name_at(list, 5), "Read Mentor");
    CHECK_STR(name_at(list, 6), "Steam");
    CHECK_STR(name_at(list, 7), "\xC3\x89" "cran");
    CHECK_STR(applist_item(list, 6)->icon, "steam");
    CHECK(applist_item(list, 8) == NULL);
    CHECK(applist_item(list, -1) == NULL);

    // The same name and command twice, the second's icon not kept
    CHECK(!applist_add(list, "Steam", "/a/steam.desktop", "other"));
    CHECK_STR(applist_item(list, 6)->icon, "steam");
    applist_free(list);
    applist_free(NULL);

    // Sorting an empty list, or a list of one, is fine
    list = applist_create();
    applist_sort(list);
    CHECK_INT(applist_count(list), 0);
    CHECK(applist_add(list, "One", "/a/one.desktop", NULL));
    applist_sort(list);
    CHECK_STR(name_at(list, 0), "One");
    applist_free(list);

    // Two apps of the same name, the later command added first: the command orders them
    list = applist_create();
    CHECK(applist_add(list, "Steam", "/b/steam.desktop", NULL));
    CHECK(applist_add(list, "Steam", "/a/steam.desktop", NULL));
    applist_sort(list);
    CHECK_STR(command_at(list, 0), "/a/steam.desktop");
    CHECK_STR(command_at(list, 1), "/b/steam.desktop");
    applist_free(list);
}

// A function to test the filter: a substring of the name, ASCII letters in either case alike and other
// bytes as they are, in the list's order; all apps for an empty or no query; the count beyond `max`
static void test_filter(void)
{
    AppList *list = applist_create();
    static const char *const names[] = { "Alpha", "Calculator", "Helpdesk", "Kodi", "kodi", "Read Mentor", "Steam",
                                         "\xC3\x89" "cran", NULL };
    for (int i = 0; names[i] != NULL; i++) {
        char command[32];
        snprintf(command, sizeof(command), "/a/%d.desktop", i);
        CHECK(applist_add(list, names[i], command, NULL));
    }
    applist_sort(list);
    int indexes[8];
    CHECK_INT(applist_filter(list, "KO", indexes, 8), 2);
    CHECK_INT(indexes[0], 3);
    CHECK_INT(indexes[1], 4);
    CHECK_INT(applist_filter(list, "e", indexes, 8), 3);
    CHECK_INT(indexes[0], 2);
    CHECK_INT(indexes[1], 5);
    CHECK_INT(indexes[2], 6);
    CHECK_INT(applist_filter(list, "e", indexes, 1), 3);    // How many, though only one fits
    CHECK_INT(indexes[0], 2);
    CHECK_INT(applist_filter(list, "e", NULL, 0), 3);
    CHECK_INT(applist_filter(list, "\xC3\x89" "cr", indexes, 8), 1);
    CHECK_INT(indexes[0], 7);
    CHECK_INT(applist_filter(list, "\xC3\xA9" "cr", indexes, 8), 0);   // é is not É: only ASCII letters fold
    CHECK_INT(applist_filter(list, "xyz", indexes, 8), 0);
    CHECK_INT(applist_filter(list, "", indexes, 8), 8);
    CHECK_INT(applist_filter(list, NULL, indexes, 8), 8);
    CHECK_INT(indexes[7], 7);
    applist_free(list);
}

// A function to test the data folders the Linux scan reads applications/ in: XDG_DATA_HOME (else
// ~/.local/share), each of XDG_DATA_DIRS in its order (else /usr/local/share and /usr/share), then the
// Flatpak exports and Snap's; each once, a trailing '/' dropped, a relative or empty entry ignored; and
// the folders that do not fit left out
static void test_data_dirs(void)
{
    char text[512];
    const char *dirs[8];
    CHECK_INT(applist_data_dirs(NULL, NULL, "/home/u", text, sizeof(text), dirs, 8), 6);
    CHECK_STR(dirs[0], "/home/u/.local/share");
    CHECK_STR(dirs[1], "/usr/local/share");
    CHECK_STR(dirs[2], "/usr/share");
    CHECK_STR(dirs[3], "/home/u/.local/share/flatpak/exports/share");
    CHECK_STR(dirs[4], "/var/lib/flatpak/exports/share");
    CHECK_STR(dirs[5], "/var/lib/snapd/desktop");

    CHECK_INT(applist_data_dirs("/data/", "/var/lib/flatpak/exports/share:/usr/share/:relative::/usr/share", "/home/u/",
                                text, sizeof(text), dirs, 8), 5);
    CHECK_STR(dirs[0], "/data");
    CHECK_STR(dirs[1], "/var/lib/flatpak/exports/share");
    CHECK_STR(dirs[2], "/usr/share");
    CHECK_STR(dirs[3], "/home/u/.local/share/flatpak/exports/share");
    CHECK_STR(dirs[4], "/var/lib/snapd/desktop");

    CHECK_INT(applist_data_dirs("relative", "", "/home/u", text, sizeof(text), dirs, 8), 6);
    CHECK_STR(dirs[0], "/home/u/.local/share");
    CHECK_INT(applist_data_dirs(NULL, NULL, NULL, text, sizeof(text), dirs, 8), 4);
    CHECK_STR(dirs[0], "/usr/local/share");
    CHECK_INT(applist_data_dirs(NULL, NULL, "", text, sizeof(text), dirs, 8), 4);

    CHECK_INT(applist_data_dirs(NULL, NULL, "/home/u", text, sizeof(text), dirs, 2), 2);
    CHECK_STR(dirs[1], "/usr/local/share");
    CHECK_INT(applist_data_dirs(NULL, NULL, "/home/u", text, 20, dirs, 8), 1);   // "/home/u/.local/share" needs 21
    CHECK_STR(dirs[0], "/usr/local/share");
}

// A function to test the command an AppsFolder app is launched by: explorer.exe opening
// shell:AppsFolder\<app ID>, the ID quoted when it holds a space or a comma; the IDs refused (empty,
// none, a quote, a ';', a control character); a command that does not fit; and a command made so passing
// the list's own rules
static void test_appsfolder_command(void)
{
    static const char *const firefox = "{6D809377-6AF0-444B-8957-A3773F02200E}\\Mozilla Firefox\\firefox.exe";
    char command[256];
    CHECK(applist_appsfolder_command("Microsoft.WindowsCalculator_8wekyb3d8bbwe!App", command, sizeof(command)));
    CHECK_STR(command, "explorer.exe shell:AppsFolder\\Microsoft.WindowsCalculator_8wekyb3d8bbwe!App");
    CHECK(applist_appsfolder_command("MSEdge", command, sizeof(command)));
    CHECK_STR(command, "explorer.exe shell:AppsFolder\\MSEdge");
    CHECK(applist_appsfolder_command("{6D809377-6AF0-444B-8957-A3773F02200E}\\VideoLAN\\VLC\\vlc.exe", command, sizeof(command)));
    CHECK_STR(command, "explorer.exe shell:AppsFolder\\{6D809377-6AF0-444B-8957-A3773F02200E}\\VideoLAN\\VLC\\vlc.exe");
    CHECK(applist_appsfolder_command(firefox, command, sizeof(command)));
    CHECK_STR(command, "explorer.exe \"shell:AppsFolder\\{6D809377-6AF0-444B-8957-A3773F02200E}\\Mozilla Firefox\\firefox.exe\"");
    CHECK(applist_appsfolder_command("Vendor.App,1", command, sizeof(command)));
    CHECK_STR(command, "explorer.exe \"shell:AppsFolder\\Vendor.App,1\"");

    CHECK(!applist_appsfolder_command("", command, sizeof(command)));
    CHECK(!applist_appsfolder_command(NULL, command, sizeof(command)));
    CHECK(!applist_appsfolder_command("A\"B", command, sizeof(command)));
    CHECK(!applist_appsfolder_command("A;B", command, sizeof(command)));
    CHECK(!applist_appsfolder_command("A\tB", command, sizeof(command)));
    CHECK(!applist_appsfolder_command("A\x1F" "B", command, sizeof(command)));   // The last control character
    CHECK(!applist_appsfolder_command("MSEdge", command, 36));   // "explorer.exe shell:AppsFolder\MSEdge" is 36 bytes
    CHECK(applist_appsfolder_command("MSEdge", command, 37));

    AppList *list = applist_create();
    CHECK(applist_appsfolder_command(firefox, command, sizeof(command)));
    CHECK(applist_add(list, "Firefox", command, firefox));
    CHECK_INT(applist_count(list), 1);
    applist_free(list);
}

int main(void)
{
    test_parse();
    test_unterminated();
    test_skipped();
    test_localized();
    test_list();
    test_filter();
    test_data_dirs();
    test_appsfolder_command();
    return check_report();
}
