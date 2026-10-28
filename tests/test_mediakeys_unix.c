// The Linux media-key backend (src/platform/mediakeys_unix.c), built here with its test seam. It is
// included rather than linked so its helpers can be tested too: run_capture() on real programs (output,
// a non-zero exit, a missing program, output too long, a signal, the kill at the timeout), kglobalaccel's
// a(ai) marshaling without a bus, and no session bus at all. GNOME runs against a fake gsettings the test
// writes and puts first on the PATH: detection by XDG_CURRENT_DESKTOP, read, clear, write, reset, a clear
// that fails partway (the failed key put back too, even when it took effect), and a hung gsettings. The
// session bus is tried for KDE Plasma only when XDG_CURRENT_DESKTOP names KDE (counted through the backend's
// PLASMA_CONNECT_TRIED hook), and with no bus there is no desktop. Then every STREAMFLEX_TEST_MEDIAKEYS mode
// of the seam. Linux only.
static int connects_tried = 0;   // Each plasma_connect(), counted through the backend's hook
#define PLASMA_CONNECT_TRIED() (connects_tried++)
#include "platform/mediakeys_unix.c"
#include <sys/stat.h>
#include "check.h"

#define VALUE_NEXT "['XF86AudioNext']"
#define VALUE_PREVIOUS "['XF86AudioPrev']"
#define VALUE_NEXT_STATIC "['XF86AudioNext', '<Ctrl>XF86AudioNext']"
#define VALUE_PREVIOUS_STATIC "['XF86AudioPrev', '<Ctrl>XF86AudioPrev']"

// A fake gsettings: each key's value in $GSTORE/<key>. A file in $GSTORE asks for a failure: hang (it
// never finishes), fail-<verb>-<key> (exit 1 at once, every time), fail-set-<key>-first (the first set
// exits 1, writing nothing), late-set-<key> (the first set writes, then exits 1, as one killed after it
// wrote) and fail-set-<key>-second (every set after the first exits 1)
static const char FAKE_GSETTINGS[] =
    "#!/bin/sh\n"
    "verb=$1; schema=$2; key=$3; value=$4\n"
    "[ \"$schema\" = org.gnome.settings-daemon.plugins.media-keys ] || { echo 'No such schema' >&2; exit 1; }\n"
    "[ -e \"$GSTORE/hang\" ] && exec sleep 30\n"
    "[ -e \"$GSTORE/fail-$verb-$key\" ] && { echo \"fail $verb $key\" >&2; exit 1; }\n"
    "if [ \"$verb\" = set ]; then\n"
    "    n=$(cat \"$GSTORE/count-$key\" 2>/dev/null || echo 0); n=$((n + 1)); echo $n > \"$GSTORE/count-$key\"\n"
    "    [ -e \"$GSTORE/fail-set-$key-first\" ] && [ $n -eq 1 ] && exit 1\n"
    "    [ -e \"$GSTORE/fail-set-$key-second\" ] && [ $n -ge 2 ] && exit 1\n"
    "    [ -e \"$GSTORE/late-set-$key\" ] && [ $n -eq 1 ] && { printf '%s' \"$value\" > \"$GSTORE/$key\"; exit 1; }\n"
    "fi\n"
    "case $verb in\n"
    "    get) v=$(cat \"$GSTORE/$key\"); [ \"$v\" = '[]' ] && v='@as []'; printf '%s\\n' \"$v\" ;;\n"
    "    set) printf '%s' \"$value\" > \"$GSTORE/$key\" ;;\n"
    "    reset) case $key in\n"
    "        next|previous) printf '%s' \"['']\" > \"$GSTORE/$key\" ;;\n"
    "        next-static) printf '%s' \"" VALUE_NEXT_STATIC "\" > \"$GSTORE/$key\" ;;\n"
    "        previous-static) printf '%s' \"" VALUE_PREVIOUS_STATIC "\" > \"$GSTORE/$key\" ;;\n"
    "    esac ;;\n"
    "    *) exit 1 ;;\n"
    "esac\n";

static char work[256];    // The test's own folder: bin/ holds the fake gsettings, store/ its values
static char store[300];

// util.c's copy_string, which the backend uses: as much as fits, zeros to the end
void copy_string(char *dest, const char *string, size_t size)
{
    size_t length = strnlen(string, size - 1);
    memcpy(dest, string, length);
    memset(dest + length, '\0', size - length);
}

// A function to write a whole file
static void write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    CHECK(file != NULL);
    if (file == NULL)
        return;
    fputs(text, file);
    fclose(file);
}

// A function to read a whole file ("" when it cannot be read)
static void read_text(const char *path, char *out, size_t cap)
{
    FILE *file = fopen(path, "r");
    size_t length = file != NULL ? fread(out, 1, cap - 1, file) : 0;
    out[length] = '\0';
    if (file != NULL)
        fclose(file);
}

// A function to name a file in the fake gsettings' store
static const char *in_store(const char *name)
{
    static char path[400];
    snprintf(path, sizeof(path), "%s/%s", store, name);
    return path;
}

// A function to start every GNOME key at a known value, with no failure asked for
static void reset_store(void)
{
    const char *const names[] = { "hang", "fail-get-next", "fail-get-next-static", "fail-set-previous",
                                  "fail-set-previous-first", "late-set-next", "late-set-previous", "fail-set-next-second", "count-next",
                                  "count-previous", "count-next-static", "count-previous-static" };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        unlink(in_store(names[i]));
    write_text(in_store("next"), VALUE_NEXT);
    write_text(in_store("previous"), VALUE_PREVIOUS);
    write_text(in_store("next-static"), VALUE_NEXT_STATIC);
    write_text(in_store("previous-static"), VALUE_PREVIOUS_STATIC);
}

// A function to make the test's folder, the fake gsettings in it, and put it first on the PATH
static bool make_world(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(work, sizeof(work), "%s/sf-mediakeys-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(work) == NULL)
        return false;
    char path[400];
    snprintf(path, sizeof(path), "%s/bin", work);
    mkdir(path, 0755);
    snprintf(store, sizeof(store), "%s/store", work);
    mkdir(store, 0755);
    snprintf(path, sizeof(path), "%s/bin/gsettings", work);
    write_text(path, FAKE_GSETTINGS);
    chmod(path, 0755);
    char search[4096];
    const char *old = getenv("PATH");
    snprintf(search, sizeof(search), "%s/bin:%s", work, old != NULL ? old : "/usr/bin:/bin");
    setenv("PATH", search, 1);
    setenv("GSTORE", store, 1);
    unsetenv("DBUS_SESSION_BUS_ADDRESS");   // No session bus, so libdbus is never asked to start one
    unsetenv("XDG_RUNTIME_DIR");
    unsetenv("STREAMFLEX_TEST_MEDIAKEYS");
    return true;
}

// A function to test run_capture() on real programs
static void test_run_capture(void)
{
    char out[64];
    const char *printing[] = { "sh", "-c", "echo noise >&2; printf 'abc\\n\\n'", NULL };
    CHECK(run_capture(printing, out, sizeof(out), 2000));
    CHECK_STR(out, "abc");   // stdout only, the trailing newlines dropped

    const char *failing[] = { "sh", "-c", "echo out; exit 3", NULL };
    CHECK(!run_capture(failing, out, sizeof(out), 2000));
    CHECK_STR(out, "sh exited with status 3");

    const char *missing[] = { "streamflex-no-such-program", NULL };
    CHECK(!run_capture(missing, out, sizeof(out), 2000));
    CHECK(strstr(out, "streamflex-no-such-program did not start") == out);

    const char *lots[] = { "seq", "1", "100", NULL };
    CHECK(!run_capture(lots, out, sizeof(out), 2000));
    CHECK_STR(out, "seq printed more than 63 bytes");
    char tiny[8];
    CHECK(!run_capture(lots, tiny, sizeof(tiny), 2000));   // The reason itself cut to fit
    CHECK_INT((int) strlen(tiny), 7);

    const char *signaled[] = { "sh", "-c", "kill -9 $$", NULL };
    CHECK(!run_capture(signaled, out, sizeof(out), 2000));
    CHECK_STR(out, "sh was stopped by signal 9");

    // One that never finishes is killed at the timeout and reaped: its pid is gone when run_capture returns
    char pid_path[400], pid_text[32];
    snprintf(pid_path, sizeof(pid_path), "%s/pid", work);
    char script[512];
    snprintf(script, sizeof(script), "echo $$ > '%s'; exec sleep 30", pid_path);
    const char *hanging[] = { "sh", "-c", script, NULL };
    CHECK(!run_capture(hanging, out, sizeof(out), 300));
    CHECK_STR(out, "sh did not finish within 300 ms");
    read_text(pid_path, pid_text, sizeof(pid_text));
    pid_t pid = (pid_t) atoi(pid_text);
    CHECK(pid > 0 && kill(pid, 0) != 0 && errno == ESRCH);

    // One whose child keeps the pipe open after it exits ends at the timeout too
    const char *lingering[] = { "sh", "-c", "sleep 3 & echo hi", NULL };
    CHECK(!run_capture(lingering, out, sizeof(out), 300));
    CHECK_STR(out, "sh did not finish within 300 ms");

    CHECK(!run_capture(printing, out, 0, 2000));   // No room for anything
}

// A function to test kglobalaccel's messages: the action's id, a(ai) both ways, and replies of other shapes
static void test_marshaling(void)
{
    int seqs[2][4] = { { 16777347, 0, 0, 0 }, { 67108864 + 16777347, 1, 2, -3 } };
    int back[16][4];
    char why[200];

    DBusMessage *call = dbus_message_new_method_call(KGLOBALACCEL_NAME, KGLOBALACCEL_PATH, KGLOBALACCEL_INTERFACE,
                                                     "setForeignShortcutKeys");
    CHECK(append_action(call, "nextmedia"));
    CHECK(append_sequences(call, seqs, 2));
    CHECK_STR(dbus_message_get_signature(call), "asa(ai)");
    DBusMessageIter args, strings;
    dbus_message_iter_init(call, &args);
    dbus_message_iter_recurse(&args, &strings);
    const char *expected[4] = { "mediacontrol", "nextmedia", "mediacontrol", "nextmedia" };
    for (int i = 0; i < 4; i++) {
        const char *id = NULL;
        CHECK_INT(dbus_message_iter_get_arg_type(&strings), DBUS_TYPE_STRING);
        dbus_message_iter_get_basic(&strings, &id);
        CHECK_STR(id, expected[i]);
        dbus_message_iter_next(&strings);
    }
    dbus_message_unref(call);

    DBusMessage *reply = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN);
    CHECK(append_sequences(reply, seqs, 2));
    CHECK_INT(plasma_sequences(reply, "nextmedia", back, 16, why, sizeof(why)), 2);
    CHECK(memcmp(back, seqs, sizeof(seqs)) == 0);
    why[0] = '\0';
    CHECK_INT(plasma_sequences(reply, "nextmedia", back, 1, why, sizeof(why)), -1);
    CHECK_STR(why, "kglobalaccel gave nextmedia more key sequences than StreamFlex keeps");
    dbus_message_unref(reply);

    reply = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN);
    CHECK(append_sequences(reply, seqs, 0));
    CHECK_STR(dbus_message_get_signature(reply), "a(ai)");
    CHECK_INT(plasma_sequences(reply, "nextmedia", back, 16, why, sizeof(why)), 0);
    dbus_message_unref(reply);

    // A sequence of fewer than 4 keys is padded with 0s, and one of more is refused
    reply = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN);
    DBusMessageIter list, sequence, keys;
    dbus_message_iter_init_append(reply, &args);
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "(ai)", &list);
    dbus_message_iter_open_container(&list, DBUS_TYPE_STRUCT, NULL, &sequence);
    dbus_message_iter_open_container(&sequence, DBUS_TYPE_ARRAY, DBUS_TYPE_INT32_AS_STRING, &keys);
    dbus_int32_t key = 16777346;
    dbus_message_iter_append_basic(&keys, DBUS_TYPE_INT32, &key);
    dbus_message_iter_close_container(&sequence, &keys);
    dbus_message_iter_close_container(&list, &sequence);
    dbus_message_iter_close_container(&args, &list);
    memset(back, 0x55, sizeof(back));
    CHECK_INT(plasma_sequences(reply, "previousmedia", back, 16, why, sizeof(why)), 1);
    CHECK(back[0][0] == 16777346 && back[0][1] == 0 && back[0][2] == 0 && back[0][3] == 0);
    dbus_message_unref(reply);

    reply = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN);
    dbus_message_iter_init_append(reply, &args);
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "(ai)", &list);
    dbus_message_iter_open_container(&list, DBUS_TYPE_STRUCT, NULL, &sequence);
    dbus_message_iter_open_container(&sequence, DBUS_TYPE_ARRAY, DBUS_TYPE_INT32_AS_STRING, &keys);
    for (key = 1; key <= 5; key++)
        dbus_message_iter_append_basic(&keys, DBUS_TYPE_INT32, &key);
    dbus_message_iter_close_container(&sequence, &keys);
    dbus_message_iter_close_container(&list, &sequence);
    dbus_message_iter_close_container(&args, &list);
    CHECK_INT(plasma_sequences(reply, "nextmedia", back, 16, why, sizeof(why)), -1);
    CHECK_STR(why, "kglobalaccel gave nextmedia a key sequence of more than 4 keys");
    dbus_message_unref(reply);

    reply = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN);
    CHECK(append_action(reply, "nextmedia"));   // as, not a(ai)
    CHECK_INT(plasma_sequences(reply, "nextmedia", back, 16, why, sizeof(why)), -1);
    CHECK_STR(why, "kglobalaccel's reply for nextmedia is not a list of key sequences");
    dbus_message_unref(reply);
}

// A function to tell whether XDG_CURRENT_DESKTOP, set to value (NULL: unset), names a desktop
static bool names(const char *value, const char *desktop)
{
    if (value == NULL)
        unsetenv("XDG_CURRENT_DESKTOP");
    else
        setenv("XDG_CURRENT_DESKTOP", value, 1);
    return xdg_names(desktop);
}

// A function to test xdg_names(): one ':'-separated name equal to the desktop's, in any case
static void test_xdg_names(void)
{
    CHECK(names("KDE", "KDE"));
    CHECK(names("kde", "KDE"));
    CHECK(names("ubuntu:KDE", "KDE"));
    CHECK(names("KDE:GNOME", "KDE"));
    CHECK(!names("KDEX", "KDE"));
    CHECK(!names("XKDE", "KDE"));
    CHECK(!names("", "KDE"));
    CHECK(!names(NULL, "KDE"));
    CHECK(!names("GNOME", "KDE"));
    CHECK(!names("ubuntu:KDEX:", "KDE"));

    CHECK(names("GNOME", "GNOME"));
    CHECK(names("gnome", "GNOME"));
    CHECK(names("ubuntu:GNOME", "GNOME"));
    CHECK(names("KDE:GNOME", "GNOME"));
    CHECK(!names("GNOMEX", "GNOME"));
    CHECK(!names("GNOME-Classic", "GNOME"));
    CHECK(!names("XGNOME", "GNOME"));
    CHECK(!names("", "GNOME"));
    CHECK(!names(NULL, "GNOME"));
    CHECK(!names("KDE", "GNOME"));
    unsetenv("XDG_CURRENT_DESKTOP");
}

// A function to test that XDG_CURRENT_DESKTOP decides whether the session bus is tried for Plasma at all:
// with a bus address set, under XFCE, GNOME or none it is not, and under KDE it is
static void test_plasma_gate(void)
{
    char why[300];
    MediaKeysState state;
    reset_store();
    setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/streamflex-bus", 1);
    const char *const untried[] = { "XFCE", "GNOME", "KDEX", NULL };
    for (int i = 0; i < 4; i++) {
        names(untried[i], "KDE");
        connects_tried = 0;
        CHECK(mediakeys_read(&state, why, sizeof(why)));
        CHECK_INT(connects_tried, 0);
    }
    const char *const tried[] = { "KDE", "kde", "ubuntu:KDE" };
    for (int i = 0; i < 3; i++) {
        names(tried[i], "KDE");
        connects_tried = 0;
        CHECK(mediakeys_read(&state, why, sizeof(why)));
        CHECK_INT(connects_tried, 1);
        CHECK_INT(state.desktop, MEDIAKEYS_NONE);   // The bus is not there
    }
    unsetenv("DBUS_SESSION_BUS_ADDRESS");
    unsetenv("XDG_CURRENT_DESKTOP");
}

// A function to test which desktop is found: Plasma only under its XDG_CURRENT_DESKTOP name and with a
// bus, GNOME only under its own name and with its schema
static void test_detection(void)
{
    char why[300];
    MediaKeysState state;
    reset_store();

    why[0] = '\0';
    CHECK(plasma_connect(why, sizeof(why)) == NULL);
    CHECK_STR(why, "there is no session bus");

    unsetenv("XDG_CURRENT_DESKTOP");
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_INT(state.desktop, MEDIAKEYS_NONE);

    setenv("XDG_CURRENT_DESKTOP", "KDE", 1);   // With the fake gsettings answering: still not GNOME
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_INT(state.desktop, MEDIAKEYS_NONE);
    setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/streamflex-bus", 1);
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_INT(state.desktop, MEDIAKEYS_NONE);
    why[0] = '\0';
    MediaKeysState plasma = { .desktop = MEDIAKEYS_PLASMA };
    CHECK(!mediakeys_clear(MEDIAKEYS_PLASMA, &plasma, why, sizeof(why)));
    CHECK(strstr(why, "the session bus did not answer: ") == why);
    unsetenv("DBUS_SESSION_BUS_ADDRESS");

    setenv("XDG_CURRENT_DESKTOP", "XFCE", 1);
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_INT(state.desktop, MEDIAKEYS_NONE);
    setenv("XDG_CURRENT_DESKTOP", "GNOME-Classic:GNOMEX", 1);
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_INT(state.desktop, MEDIAKEYS_NONE);

    setenv("XDG_CURRENT_DESKTOP", "GNOME", 1);
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_INT(state.desktop, MEDIAKEYS_GNOME);
    setenv("XDG_CURRENT_DESKTOP", "ubuntu:gnome", 1);
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_INT(state.desktop, MEDIAKEYS_GNOME);
    CHECK_STR(state.values[0], VALUE_NEXT);
    CHECK_STR(state.values[1], VALUE_PREVIOUS);
    CHECK_STR(state.values[2], VALUE_NEXT_STATIC);
    CHECK_STR(state.values[3], VALUE_PREVIOUS_STATIC);

    write_text(in_store("fail-get-next"), "");   // No schema: not GNOME
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_INT(state.desktop, MEDIAKEYS_NONE);
    CHECK_STR(state.values[0], "");
    unsetenv("XDG_CURRENT_DESKTOP");
}

// A function to test GNOME's read, clear, write and reset through the fake gsettings
static void test_gnome(void)
{
    char why[300];
    MediaKeysState before, state;
    setenv("XDG_CURRENT_DESKTOP", "GNOME", 1);
    reset_store();
    CHECK(mediakeys_read(&before, why, sizeof(why)));

    CHECK(mediakeys_clear(MEDIAKEYS_GNOME, &before, why, sizeof(why)));
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    for (int i = 0; i < 4; i++)
        CHECK_STR(state.values[i], "@as []");
    CHECK(!mediakeys_holds_next(&state) && !mediakeys_holds_previous(&state));

    CHECK(mediakeys_write(&before, why, sizeof(why)));
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK(memcmp(state.values, before.values, sizeof(state.values)) == 0);

    CHECK(mediakeys_reset_defaults(MEDIAKEYS_GNOME, why, sizeof(why)));
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_STR(state.values[0], "['']");
    CHECK_STR(state.values[1], "['']");
    CHECK_STR(state.values[2], VALUE_NEXT_STATIC);
    CHECK_STR(state.values[3], VALUE_PREVIOUS_STATIC);

    // A key whose read fails once GNOME is found: false, the desktop still named
    reset_store();
    write_text(in_store("fail-get-next-static"), "");
    why[0] = '\0';
    CHECK(!mediakeys_read(&state, why, sizeof(why)));
    CHECK_INT(state.desktop, MEDIAKEYS_GNOME);
    CHECK_STR(why, "could not get next-static: gsettings exited with status 1");

    // A gsettings that never finishes is killed at 2000 ms
    reset_store();
    write_text(in_store("hang"), "");
    why[0] = '\0';
    CHECK(!mediakeys_write(&before, why, sizeof(why)));
    CHECK_STR(why, "could not set next: gsettings did not finish within 2000 ms");
    unlink(in_store("hang"));

    // A clear refused for another desktop's values
    MediaKeysState plasma = { .desktop = MEDIAKEYS_PLASMA };
    CHECK(!mediakeys_clear(MEDIAKEYS_GNOME, &plasma, why, sizeof(why)));
    CHECK(!mediakeys_clear(MEDIAKEYS_GNOME, NULL, why, sizeof(why)));
    unsetenv("XDG_CURRENT_DESKTOP");
}

// A function to test a clear that fails partway: every key up to the failed one is put back, the failed
// one included when its set took effect before it failed, and a put-back that fails is named
static void test_partial_clear(void)
{
    char why[300];
    MediaKeysState before, state;
    setenv("XDG_CURRENT_DESKTOP", "GNOME", 1);

    // previous fails, writing nothing: next, already cleared, is put back
    reset_store();
    CHECK(mediakeys_read(&before, why, sizeof(why)));
    write_text(in_store("fail-set-previous-first"), "");
    why[0] = '\0';
    CHECK(!mediakeys_clear(MEDIAKEYS_GNOME, &before, why, sizeof(why)));
    CHECK_STR(why, "could not set previous: gsettings exited with status 1");
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK(memcmp(state.values, before.values, sizeof(state.values)) == 0);

    // previous fails after it was written (a gsettings killed late): it is put back too
    reset_store();
    write_text(in_store("late-set-previous"), "");
    why[0] = '\0';
    CHECK(!mediakeys_clear(MEDIAKEYS_GNOME, &before, why, sizeof(why)));
    CHECK_STR(why, "could not set previous: gsettings exited with status 1");
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_STR(state.values[0], VALUE_NEXT);
    CHECK_STR(state.values[1], VALUE_PREVIOUS);
    CHECK_STR(state.values[2], VALUE_NEXT_STATIC);

    // The first key fails after it was written: it is put back
    reset_store();
    write_text(in_store("late-set-next"), "");
    CHECK(!mediakeys_clear(MEDIAKEYS_GNOME, &before, why, sizeof(why)));
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_STR(state.values[0], VALUE_NEXT);
    CHECK_STR(state.values[1], VALUE_PREVIOUS);

    // A put-back that fails too is named after the failure
    reset_store();
    write_text(in_store("fail-set-previous"), "");
    write_text(in_store("fail-set-next-second"), "");
    why[0] = '\0';
    CHECK(!mediakeys_clear(MEDIAKEYS_GNOME, &before, why, sizeof(why)));
    CHECK_STR(why, "could not set previous: gsettings exited with status 1; next could not be put back; "
                   "previous could not be put back");

    // With no room for why, a failed put-back still writes nothing
    reset_store();
    write_text(in_store("fail-set-previous"), "");
    write_text(in_store("fail-set-next-second"), "");
    char untouched[4] = "abc";
    CHECK(!mediakeys_clear(MEDIAKEYS_GNOME, &before, untouched, 0));
    CHECK_STR(untouched, "abc");
    CHECK(!mediakeys_clear(MEDIAKEYS_GNOME, &before, NULL, 0));
    reset_store();
    unsetenv("XDG_CURRENT_DESKTOP");
}

// A function to test the seam: the file in place of the desktop, and each fail= mode
static void test_seam(void)
{
    char file[400], text[2048], why[200];
    MediaKeysState before, state;
    snprintf(file, sizeof(file), "%s/desktop", work);
    setenv("STREAMFLEX_TEST_MEDIAKEYS", file, 1);

    write_text(file, "plasma\n16777347,0,0,0\n16777346,0,0,0|33554432,0,0,0\n");
    CHECK(mediakeys_read(&before, why, sizeof(why)));
    CHECK_INT(before.desktop, MEDIAKEYS_PLASMA);
    CHECK_STR(before.values[1], "16777346,0,0,0|33554432,0,0,0");
    CHECK(mediakeys_clear(MEDIAKEYS_PLASMA, &before, why, sizeof(why)));
    read_text(file, text, sizeof(text));
    CHECK_STR(text, "plasma\n\n\n");
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK(!mediakeys_holds_next(&state) && !mediakeys_holds_previous(&state));
    CHECK(mediakeys_write(&before, why, sizeof(why)));
    read_text(file, text, sizeof(text));
    CHECK_STR(text, "plasma\n16777347,0,0,0\n16777346,0,0,0|33554432,0,0,0\n");
    CHECK(mediakeys_reset_defaults(MEDIAKEYS_PLASMA, why, sizeof(why)));
    read_text(file, text, sizeof(text));
    CHECK_STR(text, "plasma\n16777347,0,0,0\n16777346,0,0,0\n");

    // clear-second: the first key cleared, both put back, the file as it was
    write_text(file, "plasma\n16777347,0,0,0\n16777346,0,0,0\nfail=clear-second\n");
    CHECK(mediakeys_read(&before, why, sizeof(why)));
    CHECK(!mediakeys_clear(MEDIAKEYS_PLASMA, &before, why, sizeof(why)));
    CHECK_STR(why, "the test file fails clear-second");
    read_text(file, text, sizeof(text));
    CHECK_STR(text, "plasma\n16777347,0,0,0\n16777346,0,0,0\nfail=clear-second\n");

    write_text(file, "gnome\n['XF86AudioNext']\n['XF86AudioPrev']\n['']\n@as []\nfail=clear\nfail=write\n");
    CHECK(mediakeys_read(&before, why, sizeof(why)));
    CHECK_INT(before.desktop, MEDIAKEYS_GNOME);
    CHECK_STR(before.values[3], "@as []");
    CHECK(!mediakeys_clear(MEDIAKEYS_GNOME, &before, why, sizeof(why)));
    CHECK_STR(why, "the test file fails clear");
    CHECK(!mediakeys_write(&before, why, sizeof(why)));
    CHECK_STR(why, "the test file fails write");
    CHECK(mediakeys_reset_defaults(MEDIAKEYS_GNOME, why, sizeof(why)));
    read_text(file, text, sizeof(text));
    CHECK_STR(text, "gnome\n['']\n['']\n" VALUE_NEXT_STATIC "\n" VALUE_PREVIOUS_STATIC "\nfail=clear\nfail=write\n");

    write_text(file, "gnome\n['XF86AudioNext']\n['XF86AudioPrev']\n['']\n@as []\n");
    CHECK(mediakeys_read(&before, why, sizeof(why)));
    CHECK(mediakeys_clear(MEDIAKEYS_GNOME, &before, why, sizeof(why)));
    read_text(file, text, sizeof(text));
    CHECK_STR(text, "gnome\n@as []\n@as []\n@as []\n@as []\n");

    write_text(file, "gnome\n['XF86AudioNext']\n['XF86AudioPrev']\n['']\n@as []\nfail=read\nfail=defaults\n");
    CHECK(!mediakeys_read(&state, why, sizeof(why)));
    CHECK_INT(state.desktop, MEDIAKEYS_GNOME);
    CHECK_STR(why, "the test file fails read");
    CHECK(!mediakeys_reset_defaults(MEDIAKEYS_GNOME, why, sizeof(why)));
    CHECK_STR(why, "the test file fails defaults");
    CHECK(!mediakeys_reset_defaults(MEDIAKEYS_PLASMA, why, sizeof(why)));
    CHECK_STR(why, "the test file stands in for another desktop");

    write_text(file, "none\n");
    CHECK(mediakeys_read(&state, why, sizeof(why)));
    CHECK_INT(state.desktop, MEDIAKEYS_NONE);
    write_text(file, "kde\n");
    CHECK(!mediakeys_read(&state, why, sizeof(why)));
    unlink(file);
    CHECK(!mediakeys_read(&state, why, sizeof(why)));
    unsetenv("STREAMFLEX_TEST_MEDIAKEYS");
}

int main(void)
{
    if (!make_world()) {
        printf("could not make the test's folder\n");
        return 1;
    }
    test_run_capture();
    test_marshaling();
    test_xdg_names();
    test_plasma_gate();
    test_detection();
    test_gnome();
    test_partial_clear();
    test_seam();
    char out[64];
    const char *cleanup[] = { "rm", "-rf", work, NULL };
    run_capture(cleanup, out, sizeof(out), 10000);
    return check_report();
}
