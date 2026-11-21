// For pipe2() and O_CLOEXEC: run_capture()'s pipe is closed in every other program this process starts
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <dbus/dbus.h>
#include <SDL3/SDL.h>
#include "../launcher.h"
#include "../util.h"
#include "../mediakeys.h"
#include "platform.h"

// The desktop's media-key shortcuts: KDE Plasma's (5 and 6) through kglobalaccel on the session bus,
// GNOME's through the gsettings command. Each public function opens what it needs and closes it again.

extern char **environ;

#define KGLOBALACCEL_NAME "org.kde.kglobalaccel"
#define KGLOBALACCEL_PATH "/kglobalaccel"
#define KGLOBALACCEL_INTERFACE "org.kde.KGlobalAccel"
#define KGLOBALACCEL_COMPONENT "mediacontrol"
#define DBUS_TIMEOUT_MS 1000             // Every D-Bus call's reply timeout
#define PLASMA_MEDIA_NEXT 16777347       // Qt::Key_MediaNext, the default when kglobalaccel gives none
#define PLASMA_MEDIA_PREVIOUS 16777346   // Qt::Key_MediaPrevious

#define GNOME_SCHEMA "org.gnome.settings-daemon.plugins.media-keys"
#define GSETTINGS_TIMEOUT_MS 2000        // gsettings is killed when it has not finished by then

// A function to write why something failed, when the caller gave room for it
static void say(char *why, size_t whycap, const char *format, ...) __attribute__((format(printf, 3, 4)));
static void say(char *why, size_t whycap, const char *format, ...)
{
    if (why == NULL || whycap == 0)
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(why, whycap, format, args);
    va_end(args);
}

// A function to give a monotonic clock's time in milliseconds
static long long now_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long long) now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

// A function to run a program with no shell, argv[0] found on the PATH, and capture what it prints: its
// output in out, the trailing newline dropped, its errors thrown away. False when it could not start,
// printed more than out holds, did not finish within timeout_ms (it is then killed), or exited with
// anything but 0; out then says which
static bool run_capture(const char *const argv[], char *out, size_t cap, int timeout_ms)
{
    if (out == NULL || cap == 0)
        return false;
    out[0] = '\0';
    int pipe_ends[2];
    if (pipe2(pipe_ends, O_CLOEXEC) != 0) {
        snprintf(out, cap, "no pipe for %s: %s", argv[0], strerror(errno));
        return false;
    }

    // The child's stdout is the pipe and its stderr /dev/null; dup2() leaves the copy open across exec
    posix_spawn_file_actions_t actions;
    pid_t pid = 0;
    int error = posix_spawn_file_actions_init(&actions);
    if (error == 0) {
        error = posix_spawn_file_actions_adddup2(&actions, pipe_ends[1], STDOUT_FILENO);
        if (error == 0)
            error = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        if (error == 0)
            error = posix_spawnp(&pid, argv[0], &actions, NULL, (char* const*) argv, environ);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(pipe_ends[1]);
    if (error != 0) {
        close(pipe_ends[0]);
        snprintf(out, cap, "%s did not start: %s", argv[0], strerror(error));
        return false;
    }

    // Read until the child closes its end, keeping what fits
    long long deadline = now_ms() + timeout_ms;
    size_t used = 0;
    bool too_long = false;
    bool timed_out = false;
    bool broken = false;
    for (;;) {
        long long left = deadline - now_ms();
        if (left <= 0) {
            timed_out = true;
            break;
        }
        struct pollfd ready = { pipe_ends[0], POLLIN, 0 };
        int polled = poll(&ready, 1, (int) left);
        if (polled < 0 && errno == EINTR)
            continue;
        if (polled <= 0) {
            timed_out = polled == 0;
            broken = polled < 0;
            break;
        }
        char chunk[256];
        ssize_t got = read(pipe_ends[0], chunk, sizeof(chunk));
        if (got < 0 && errno == EINTR)
            continue;
        if (got < 0)
            broken = true;
        if (got <= 0)
            break;
        if (used + (size_t) got >= cap)
            too_long = true;
        else {
            memcpy(out + used, chunk, (size_t) got);
            used += (size_t) got;
        }
    }
    close(pipe_ends[0]);

    // Then wait for it to exit, in what is left of the time; one that has not by then is killed and reaped
    int status = 0;
    bool reaped = false;
    while (!timed_out && !broken) {
        pid_t done = waitpid(pid, &status, WNOHANG);
        if (done == pid) {
            reaped = true;
            break;
        }
        if (done < 0 && errno != EINTR) {
            broken = true;
            reaped = errno == ECHILD;   // Already gone: there is nothing left to kill
            break;
        }
        if (now_ms() >= deadline) {
            timed_out = true;
            break;
        }
        struct timespec nap = { 0, 5 * 1000 * 1000 };
        nanosleep(&nap, NULL);
    }
    if (!reaped) {
        kill(pid, SIGKILL);
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
            ;
    }

    if (timed_out)
        snprintf(out, cap, "%s did not finish within %d ms", argv[0], timeout_ms);
    else if (broken)
        snprintf(out, cap, "%s's output could not be read", argv[0]);
    else if (too_long)
        snprintf(out, cap, "%s printed more than %zu bytes", argv[0], cap - 1);
    else if (!WIFEXITED(status))
        snprintf(out, cap, "%s was stopped by signal %d", argv[0], WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    else if (WEXITSTATUS(status) != 0)
        snprintf(out, cap, "%s exited with status %d", argv[0], WEXITSTATUS(status));
    else {
        while (used > 0 && (out[used - 1] == '\n' || out[used - 1] == '\r'))
            used--;
        out[used] = '\0';
        return true;
    }
    return false;
}

// A function to tell whether there is a session bus to ask: DBUS_SESSION_BUS_ADDRESS names one, or the
// user's bus socket is at $XDG_RUNTIME_DIR/bus. With neither, libdbus would autolaunch a bus of its own
// (dbus-launch), which would be left running after StreamFlex
static bool session_bus_known(void)
{
    const char *address = getenv("DBUS_SESSION_BUS_ADDRESS");
    if (address != NULL && address[0] != '\0')
        return true;
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (runtime == NULL || runtime[0] == '\0')
        return false;
    char socket_path[4096];
    int written = snprintf(socket_path, sizeof(socket_path), "%s/bus", runtime);
    return written > 0 && (size_t) written < sizeof(socket_path) && access(socket_path, F_OK) == 0;
}

// tests/test_mediakeys_unix.c, which includes this file, defines this to count the connections tried
#ifndef PLASMA_CONNECT_TRIED
#define PLASMA_CONNECT_TRIED()
#endif

// A function to open a private connection to the session bus, shared with nothing else in the process, which
// never ends the process when the bus goes away; NULL, with why, when there is none
static DBusConnection *plasma_connect(char *why, size_t whycap)
{
    PLASMA_CONNECT_TRIED();
    if (!session_bus_known()) {
        say(why, whycap, "there is no session bus");
        return NULL;
    }
    DBusError error;
    dbus_error_init(&error);
    DBusConnection *connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (connection == NULL) {
        say(why, whycap, "the session bus did not answer: %s",
            dbus_error_is_set(&error) ? error.message : "no reason given");
        dbus_error_free(&error);
        return NULL;
    }
    dbus_connection_set_exit_on_disconnect(connection, FALSE);
    return connection;
}

// A function to close a private connection and let it go
static void plasma_disconnect(DBusConnection *connection)
{
    dbus_connection_close(connection);
    dbus_connection_unref(connection);
}

// A function to tell whether kglobalaccel is running: whether its bus name has an owner, which holds for
// Plasma 5's daemon and Plasma 6's KWin alike. The bus's own NameHasOwner, called with the 1000 ms timeout
// (dbus_bus_name_has_owner() waits libdbus's default 25 s)
static bool plasma_running(DBusConnection *connection)
{
    DBusMessage *call = dbus_message_new_method_call(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS,
                                                     "NameHasOwner");
    if (call == NULL)
        return false;
    const char *name = KGLOBALACCEL_NAME;
    if (!dbus_message_append_args(call, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID)) {
        dbus_message_unref(call);
        return false;
    }
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection, call, DBUS_TIMEOUT_MS, NULL);
    dbus_message_unref(call);
    dbus_bool_t owned = FALSE;
    if (reply != NULL) {
        if (!dbus_message_get_args(reply, NULL, DBUS_TYPE_BOOLEAN, &owned, DBUS_TYPE_INVALID))
            owned = FALSE;
        dbus_message_unref(reply);
    }
    return owned ? true : false;
}

// A function to append an action's id, kglobalaccel's 4 strings: component and action, each twice (the
// unique names, then the friendly ones)
static bool append_action(DBusMessage *message, const char *action)
{
    const char *id[4] = { KGLOBALACCEL_COMPONENT, action, KGLOBALACCEL_COMPONENT, action };
    DBusMessageIter args;
    DBusMessageIter strings = DBUS_MESSAGE_ITER_INIT_CLOSED;
    dbus_message_iter_init_append(message, &args);
    if (!dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &strings))
        return false;
    for (int i = 0; i < 4; i++) {
        if (!dbus_message_iter_append_basic(&strings, DBUS_TYPE_STRING, &id[i])) {
            dbus_message_iter_abandon_container_if_open(&args, &strings);
            return false;
        }
    }
    return dbus_message_iter_close_container(&args, &strings) ? true : false;
}

// A function to append key sequences as kglobalaccel takes them, a(ai): each sequence a struct of 4 ints
static bool append_sequences(DBusMessage *message, int (*seqs)[4], int nseq)
{
    DBusMessageIter args;
    DBusMessageIter list = DBUS_MESSAGE_ITER_INIT_CLOSED;
    DBusMessageIter sequence = DBUS_MESSAGE_ITER_INIT_CLOSED;
    DBusMessageIter keys = DBUS_MESSAGE_ITER_INIT_CLOSED;
    dbus_message_iter_init_append(message, &args);
    bool appended = dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "(ai)", &list);
    for (int i = 0; appended && i < nseq; i++) {
        appended = dbus_message_iter_open_container(&list, DBUS_TYPE_STRUCT, NULL, &sequence)
                && dbus_message_iter_open_container(&sequence, DBUS_TYPE_ARRAY, DBUS_TYPE_INT32_AS_STRING, &keys);
        for (int k = 0; appended && k < 4; k++) {
            dbus_int32_t key = seqs[i][k];
            appended = dbus_message_iter_append_basic(&keys, DBUS_TYPE_INT32, &key);
        }
        appended = appended && dbus_message_iter_close_container(&sequence, &keys)
                && dbus_message_iter_close_container(&list, &sequence);
    }
    appended = appended && dbus_message_iter_close_container(&args, &list);
    if (!appended) {
        dbus_message_iter_abandon_container_if_open(&sequence, &keys);
        dbus_message_iter_abandon_container_if_open(&list, &sequence);
        dbus_message_iter_abandon_container_if_open(&args, &list);
    }
    return appended;
}

// A function to call one of kglobalaccel's methods on one action, with the key sequences as its second
// argument when nseq is 0 or more; the reply, which the caller frees, or NULL, with why
static DBusMessage *plasma_call(DBusConnection *connection, const char *method, const char *action,
                                int (*seqs)[4], int nseq, char *why, size_t whycap)
{
    DBusMessage *call = dbus_message_new_method_call(KGLOBALACCEL_NAME, KGLOBALACCEL_PATH,
                                                     KGLOBALACCEL_INTERFACE, method);
    if (call == NULL) {
        say(why, whycap, "out of memory");
        return NULL;
    }
    if (!append_action(call, action) || (nseq >= 0 && !append_sequences(call, seqs, nseq))) {
        dbus_message_unref(call);
        say(why, whycap, "out of memory");
        return NULL;
    }
    DBusError error;
    dbus_error_init(&error);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection, call, DBUS_TIMEOUT_MS, &error);
    dbus_message_unref(call);
    if (reply == NULL) {
        say(why, whycap, "kglobalaccel's %s for %s failed: %s", method, action,
            dbus_error_is_set(&error) ? error.message : "no reply");
        dbus_error_free(&error);
    }
    return reply;
}

// A function to read a reply's a(ai) into key sequences, 4 ints each (a shorter one padded with 0s): how
// many, or -1, with why, for a reply of any other shape or with more than max
static int plasma_sequences(DBusMessage *reply, const char *action, int (*seqs)[4], int max, char *why,
                            size_t whycap)
{
    DBusMessageIter args, list, sequence, keys;
    if (!dbus_message_iter_init(reply, &args) || dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY
        || dbus_message_iter_get_element_type(&args) != DBUS_TYPE_STRUCT) {
        say(why, whycap, "kglobalaccel's reply for %s is not a list of key sequences", action);
        return -1;
    }
    int count = 0;
    dbus_message_iter_recurse(&args, &list);
    for (; dbus_message_iter_get_arg_type(&list) == DBUS_TYPE_STRUCT; dbus_message_iter_next(&list)) {
        dbus_message_iter_recurse(&list, &sequence);
        if (dbus_message_iter_get_arg_type(&sequence) != DBUS_TYPE_ARRAY
            || dbus_message_iter_get_element_type(&sequence) != DBUS_TYPE_INT32) {
            say(why, whycap, "kglobalaccel's reply for %s is not a list of key sequences", action);
            return -1;
        }
        if (count >= max) {
            say(why, whycap, "kglobalaccel gave %s more key sequences than StreamFlex keeps", action);
            return -1;
        }
        int k = 0;
        memset(seqs[count], 0, sizeof(seqs[count]));
        dbus_message_iter_recurse(&sequence, &keys);
        for (; dbus_message_iter_get_arg_type(&keys) == DBUS_TYPE_INT32; dbus_message_iter_next(&keys)) {
            if (k >= 4) {
                say(why, whycap, "kglobalaccel gave %s a key sequence of more than 4 keys", action);
                return -1;
            }
            dbus_int32_t key = 0;
            dbus_message_iter_get_basic(&keys, &key);
            seqs[count][k++] = key;
        }
        count++;
    }
    return count;
}

// A function to read one of Plasma's keys as value text
static bool plasma_read_key(DBusConnection *connection, const char *action, char *value, size_t cap, char *why,
                            size_t whycap)
{
    int seqs[MEDIAKEYS_PLASMA_SEQUENCES_MAX][4];
    DBusMessage *reply = plasma_call(connection, "shortcutKeys", action, NULL, -1, why, whycap);
    if (reply == NULL)
        return false;
    int count = plasma_sequences(reply, action, seqs, MEDIAKEYS_PLASMA_SEQUENCES_MAX, why, whycap);
    dbus_message_unref(reply);
    if (count < 0)
        return false;
    if (!mediakeys_plasma_format(seqs, count, value, cap)) {
        say(why, whycap, "%s's key sequences are too long to keep", action);
        return false;
    }
    return true;
}

// A function to set one of Plasma's keys from value text ("" clears it)
static bool plasma_set_key(DBusConnection *connection, const char *action, const char *value, char *why,
                           size_t whycap)
{
    int seqs[MEDIAKEYS_PLASMA_SEQUENCES_MAX][4];
    int count = mediakeys_plasma_parse(value, seqs, MEDIAKEYS_PLASMA_SEQUENCES_MAX);
    if (count < 0) {
        say(why, whycap, "the saved value for %s is not key sequences", action);
        return false;
    }
    DBusMessage *reply = plasma_call(connection, "setForeignShortcutKeys", action, seqs, count, why, whycap);
    if (reply == NULL)
        return false;
    dbus_message_unref(reply);
    return true;
}

// A function to set one of Plasma's keys to its default: kglobalaccel's own, else Media Next or Media Previous
// when it gives none that binds a key or cannot say
static bool plasma_default_key(DBusConnection *connection, int i, char *why, size_t whycap)
{
    const char *action = mediakeys_key_name(MEDIAKEYS_PLASMA, i);
    int seqs[MEDIAKEYS_PLASMA_SEQUENCES_MAX][4];
    int count = -1;
    DBusMessage *reply = plasma_call(connection, "defaultShortcutKeys", action, NULL, -1, NULL, 0);
    if (reply != NULL) {
        count = plasma_sequences(reply, action, seqs, MEDIAKEYS_PLASMA_SEQUENCES_MAX, NULL, 0);
        dbus_message_unref(reply);
    }
    bool binds = false;
    for (int k = 0; k < count; k++)
        binds = binds || seqs[k][0] != 0;
    if (!binds) {
        int fixed[4] = { i == 0 ? PLASMA_MEDIA_NEXT : PLASMA_MEDIA_PREVIOUS, 0, 0, 0 };
        memcpy(seqs[0], fixed, sizeof(fixed));
        count = 1;
    }
    reply = plasma_call(connection, "setForeignShortcutKeys", action, seqs, count, why, whycap);
    if (reply == NULL)
        return false;
    dbus_message_unref(reply);
    return true;
}

// A function to run gsettings on one of GNOME's keys: get (value NULL, the output in out), set (to value)
// or reset (value NULL, out NULL); false, with why, when it fails
static bool gsettings(const char *verb, const char *key, const char *value, char *out, size_t cap, char *why,
                      size_t whycap)
{
    const char *argv[] = { "gsettings", verb, GNOME_SCHEMA, key, value, NULL };
    char output[MEDIAKEYS_VALUE_MAX];
    if (!run_capture(argv, output, sizeof(output), GSETTINGS_TIMEOUT_MS)) {
        say(why, whycap, "could not %s %s: %s", verb, key, output);
        return false;
    }
    if (out != NULL)
        copy_string(out, output, cap);
    return true;
}

// A function to tell whether XDG_CURRENT_DESKTOP names a desktop ("KDE", "GNOME"): one of its ':'-separated
// names, in any case
static bool xdg_names(const char *desktop)
{
    const char *desktops = getenv("XDG_CURRENT_DESKTOP");
    while (desktops != NULL) {
        const char *end = strchr(desktops, ':');
        size_t length = end != NULL ? (size_t) (end - desktops) : strlen(desktops);
        if (length == strlen(desktop) && strncasecmp(desktops, desktop, length) == 0)
            return true;
        desktops = end != NULL ? end + 1 : NULL;
    }
    return false;
}

// A function to set one key of a desktop from value text: Plasma's key sequences, or GNOME's gsettings text
static bool set_key(DBusConnection *connection, MediaKeysDesktop desktop, int i, const char *value, char *why,
                    size_t whycap)
{
    const char *key = mediakeys_key_name(desktop, i);
    if (desktop == MEDIAKEYS_PLASMA)
        return plasma_set_key(connection, key, value, why, whycap);
    return gsettings("set", key, value, NULL, 0, why, whycap);
}

// A function to set one key of a desktop to its default: kglobalaccel's default shortcut, or gsettings reset,
// the schema's own
static bool default_key(DBusConnection *connection, MediaKeysDesktop desktop, int i, char *why, size_t whycap)
{
    if (desktop == MEDIAKEYS_PLASMA)
        return plasma_default_key(connection, i, why, whycap);
    return gsettings("reset", mediakeys_key_name(desktop, i), NULL, NULL, 0, why, whycap);
}

// A function to read each of a desktop's keys as value text, as mediakeys_read() does, through what
// open_desktop() opened
static bool read_keys(DBusConnection *connection, MediaKeysDesktop desktop, MediaKeysState *out, char *why,
                      size_t whycap)
{
    memset(out, 0, sizeof(*out));
    out->desktop = desktop;
    bool read = true;
    for (int i = 0; read && i < mediakeys_key_count(desktop); i++) {
        const char *key = mediakeys_key_name(desktop, i);
        if (desktop == MEDIAKEYS_PLASMA)
            read = plasma_read_key(connection, key, out->values[i], MEDIAKEYS_VALUE_MAX, why, whycap);
        else
            read = gsettings("get", key, NULL, out->values[i], MEDIAKEYS_VALUE_MAX, why, whycap);
    }
    return read;
}

// A function to put a desktop's keys back from before after the change of key `failed` failed: the keys
// before it were changed, and it may have been too (a D-Bus call that timed out was still sent, and gsettings
// killed at its timeout may already have written). So every key up to it is put back, in order; putting back
// a key that never changed is harmless. False when one could not be, which why then names after the failure
static bool put_back(DBusConnection *connection, MediaKeysDesktop desktop, const MediaKeysState *before, int failed,
                     char *why, size_t whycap)
{
    bool all = true;
    for (int k = 0; k <= failed; k++) {
        if (set_key(connection, desktop, k, before->values[k], NULL, 0))
            continue;
        all = false;
        if (why != NULL && whycap > 0) {
            size_t used = strlen(why);
            say(why + used, whycap - used, "; %s could not be put back", mediakeys_key_name(desktop, k));
        }
    }
    return all;
}

// A function to open what a desktop's calls need: Plasma's private bus connection (NULL for GNOME); false,
// with why, when it cannot, or for no desktop
static bool open_desktop(MediaKeysDesktop desktop, DBusConnection **connection, char *why, size_t whycap)
{
    *connection = NULL;
    if (desktop == MEDIAKEYS_GNOME)
        return true;
    if (desktop != MEDIAKEYS_PLASMA) {
        say(why, whycap, "the desktop is neither KDE Plasma nor GNOME");
        return false;
    }
    *connection = plasma_connect(why, whycap);
    return *connection != NULL;
}

#ifdef STREAMFLEX_TEST_HOOKS
// Only the headless harness builds this. STREAMFLEX_TEST_MEDIAKEYS names a file that stands in for the
// desktop: its first line plasma, gnome or none; then each key's value text, a line each, in
// mediakeys_key_name()'s order; then any of fail=read, fail=clear, fail=clear-second (the second key fails
// once the first is cleared), fail=write, fail=write-second (the second key fails once the first is set),
// fail=put-back (with clear-second or write-second, the first key is not put back) and fail=defaults. Every change rewrites
// the whole file, its fail= lines kept
#define FAKE_LINE_MAX 1024
#define FAKE_FAILS_MAX 8
#define FAKE_FAIL_MAX 32

typedef struct {
    MediaKeysState state;
    char fails[FAKE_FAILS_MAX][FAKE_FAIL_MAX];   // Its fail= lines, as written
    int nfails;
} FakeDesktop;

// A function to give the file standing in for the desktop; NULL when there is none
static const char *fake_path(void)
{
    const char *path = getenv("STREAMFLEX_TEST_MEDIAKEYS");
    return path != NULL && path[0] != '\0' ? path : NULL;
}

// A function to tell whether the file asks for a step to fail
static bool fake_fails(const FakeDesktop *fake, const char *step)
{
    for (int i = 0; i < fake->nfails; i++) {
        if (strcmp(fake->fails[i] + 5, step) == 0)   // Past "fail="
            return true;
    }
    return false;
}

// A function to read the file standing in for the desktop
static bool fake_load(FakeDesktop *fake, char *why, size_t whycap)
{
    memset(fake, 0, sizeof(*fake));
    FILE *file = fopen(fake_path(), "r");
    if (file == NULL) {
        say(why, whycap, "the test file %s could not be read", fake_path());
        return false;
    }
    char line[FAKE_LINE_MAX];
    int n = 0;
    bool loaded = true;
    while (loaded && fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (n == 0) {
            fake->state.desktop = mediakeys_desktop_from_config(line);
            loaded = fake->state.desktop != MEDIAKEYS_NONE || strcmp(line, "none") == 0;
        }
        else if (n <= mediakeys_key_count(fake->state.desktop)) {
            loaded = strlen(line) < MEDIAKEYS_VALUE_MAX;
            copy_string(fake->state.values[n - 1], line, MEDIAKEYS_VALUE_MAX);
        }
        else if (strncmp(line, "fail=", 5) == 0 && fake->nfails < FAKE_FAILS_MAX && strlen(line) < FAKE_FAIL_MAX)
            copy_string(fake->fails[fake->nfails++], line, FAKE_FAIL_MAX);
        n++;
    }
    fclose(file);
    if (!loaded || n == 0) {
        say(why, whycap, "the test file %s is not a desktop's shortcuts", fake_path());
        return false;
    }
    return true;
}

// A function to write the file standing in for the desktop back, whole
static bool fake_save(const FakeDesktop *fake, char *why, size_t whycap)
{
    FILE *file = fopen(fake_path(), "w");
    if (file == NULL) {
        say(why, whycap, "the test file %s could not be written", fake_path());
        return false;
    }
    const char *desktop = mediakeys_desktop_config_name(fake->state.desktop);
    fprintf(file, "%s\n", desktop != NULL ? desktop : "none");
    for (int i = 0; i < mediakeys_key_count(fake->state.desktop); i++)
        fprintf(file, "%s\n", fake->state.values[i]);
    for (int i = 0; i < fake->nfails; i++)
        fprintf(file, "%s\n", fake->fails[i]);
    if (fclose(file) != 0) {
        say(why, whycap, "the test file %s could not be written", fake_path());
        return false;
    }
    return true;
}

// A function to read the file for a change to one desktop's keys: false, with why, when it cannot be read,
// stands in for another desktop, or asks for this step to fail
static bool fake_begin(FakeDesktop *fake, MediaKeysDesktop desktop, const char *step, char *why, size_t whycap)
{
    if (!fake_load(fake, why, whycap))
        return false;
    if (fake->state.desktop != desktop || desktop == MEDIAKEYS_NONE) {
        say(why, whycap, "the test file stands in for another desktop");
        return false;
    }
    if (fake_fails(fake, step)) {
        say(why, whycap, "the test file fails %s", step);
        return false;
    }
    return true;
}

// A function to read the desktop from the file
static bool fake_read(MediaKeysState *out, char *why, size_t whycap)
{
    FakeDesktop fake;
    if (!fake_load(&fake, why, whycap))
        return false;
    out->desktop = fake.state.desktop;
    if (fake_fails(&fake, "read")) {
        say(why, whycap, "the test file fails read");
        return false;
    }
    *out = fake.state;
    return true;
}

// A function to clear the keys in the file: empty (Plasma) or @as [] (GNOME), as each desktop reads back. With
// fail=clear-second, the first key is cleared, then it and the second are put back from before, as
// mediakeys_clear() does, and the clear fails; with fail=put-back too, the first key stays cleared and
// *left_changed says so
static bool fake_clear(MediaKeysDesktop desktop, const MediaKeysState *before, bool *left_changed, char *why,
                       size_t whycap)
{
    FakeDesktop fake;
    if (!fake_begin(&fake, desktop, "clear", why, whycap))
        return false;
    const char *cleared = desktop == MEDIAKEYS_PLASMA ? "" : "@as []";
    for (int i = 0; i < mediakeys_key_count(desktop); i++) {
        if (i == 1 && fake_fails(&fake, "clear-second")) {
            bool kept = !fake_fails(&fake, "put-back");
            for (int k = 0; kept && k <= i; k++)
                copy_string(fake.state.values[k], before->values[k], MEDIAKEYS_VALUE_MAX);
            if (!fake_save(&fake, why, whycap))
                return false;
            if (kept)
                say(why, whycap, "the test file fails clear-second");
            else
                say(why, whycap, "the test file fails clear-second; %s could not be put back",
                    mediakeys_key_name(desktop, 0));
            if (left_changed != NULL)
                *left_changed = !kept;
            return false;
        }
        copy_string(fake.state.values[i], cleared, MEDIAKEYS_VALUE_MAX);
    }
    return fake_save(&fake, why, whycap);
}

// A function to write saved values to the file. With fail=write-second, the first key is set, then it and
// the second are put back from before, as mediakeys_write() does, and the write fails; with fail=put-back
// too, the first key stays set and *left_changed says so
static bool fake_write(const MediaKeysState *saved, bool *left_changed, char *why, size_t whycap)
{
    FakeDesktop fake;
    if (!fake_begin(&fake, saved->desktop, "write", why, whycap))
        return false;
    MediaKeysState before = fake.state;
    for (int i = 0; i < mediakeys_key_count(saved->desktop); i++) {
        if (i == 1 && fake_fails(&fake, "write-second")) {
            bool kept = !fake_fails(&fake, "put-back");
            for (int k = 0; kept && k <= i; k++)
                copy_string(fake.state.values[k], before.values[k], MEDIAKEYS_VALUE_MAX);
            if (!fake_save(&fake, why, whycap))
                return false;
            if (kept)
                say(why, whycap, "the test file fails write-second");
            else
                say(why, whycap, "the test file fails write-second; %s could not be put back",
                    mediakeys_key_name(saved->desktop, 0));
            if (left_changed != NULL)
                *left_changed = !kept;
            return false;
        }
        copy_string(fake.state.values[i], saved->values[i], MEDIAKEYS_VALUE_MAX);
    }
    return fake_save(&fake, why, whycap);
}

// A function to write the desktop's defaults to the file, as kglobalaccel's fallback and GNOME's schema
// give them
static bool fake_reset_defaults(MediaKeysDesktop desktop, char *why, size_t whycap)
{
    static const char *const plasma[] = { "16777347,0,0,0", "16777346,0,0,0" };
    static const char *const gnome[] = { "['']", "['']", "['XF86AudioNext', '<Ctrl>XF86AudioNext']",
                                         "['XF86AudioPrev', '<Ctrl>XF86AudioPrev']" };
    FakeDesktop fake;
    if (!fake_begin(&fake, desktop, "defaults", why, whycap))
        return false;
    for (int i = 0; i < mediakeys_key_count(desktop); i++)
        copy_string(fake.state.values[i], desktop == MEDIAKEYS_PLASMA ? plasma[i] : gnome[i], MEDIAKEYS_VALUE_MAX);
    return fake_save(&fake, why, whycap);
}
#endif

// A function to read the desktop's media-key shortcuts: KDE Plasma when XDG_CURRENT_DESKTOP names KDE and
// kglobalaccel is on the session bus (a KDE app can start kglobalaccel under another desktop), else GNOME
// when XDG_CURRENT_DESKTOP names it and gsettings reads its keys, else no desktop (true). False, with why,
// when a call to the desktop found failed; out->desktop then still names it
bool mediakeys_read(MediaKeysState *out, char *why, size_t whycap)
{
    memset(out, 0, sizeof(*out));
    out->desktop = MEDIAKEYS_NONE;
#ifdef STREAMFLEX_TEST_HOOKS
    if (fake_path() != NULL)
        return fake_read(out, why, whycap);
#endif
    DBusConnection *connection = xdg_names("KDE") ? plasma_connect(NULL, 0) : NULL;
    if (connection != NULL) {
        bool found = true;
        if (plasma_running(connection)) {
            out->desktop = MEDIAKEYS_PLASMA;
            for (int i = 0; found && i < mediakeys_key_count(MEDIAKEYS_PLASMA); i++)
                found = plasma_read_key(connection, mediakeys_key_name(MEDIAKEYS_PLASMA, i), out->values[i],
                                       MEDIAKEYS_VALUE_MAX, why, whycap);
        }
        plasma_disconnect(connection);
        if (out->desktop == MEDIAKEYS_PLASMA)
            return found;
    }

    // GNOME: its first key read is also the check that the schema is there
    if (!xdg_names("GNOME") || !gsettings("get", mediakeys_key_name(MEDIAKEYS_GNOME, 0), NULL, out->values[0],
                                         MEDIAKEYS_VALUE_MAX, NULL, 0)) {
        out->values[0][0] = '\0';
        return true;
    }
    out->desktop = MEDIAKEYS_GNOME;
    for (int i = 1; i < mediakeys_key_count(MEDIAKEYS_GNOME); i++) {
        if (!gsettings("get", mediakeys_key_name(MEDIAKEYS_GNOME, i), NULL, out->values[i], MEDIAKEYS_VALUE_MAX,
                       why, whycap))
            return false;
    }
    return true;
}

// A function to clear every one of the desktop's media keys. When one fails, it and every key before it are
// put back from before first; why then also says when one of them could not be, and *left_changed is true
bool mediakeys_clear(MediaKeysDesktop desktop, const MediaKeysState *before, bool *left_changed, char *why,
                     size_t whycap)
{
    if (left_changed != NULL)
        *left_changed = false;
    if (before == NULL || before->desktop != desktop) {
        say(why, whycap, "the shortcuts read before are another desktop's");
        return false;
    }
#ifdef STREAMFLEX_TEST_HOOKS
    if (fake_path() != NULL)
        return fake_clear(desktop, before, left_changed, why, whycap);
#endif
    DBusConnection *connection;
    if (!open_desktop(desktop, &connection, why, whycap))
        return false;
    const char *cleared = desktop == MEDIAKEYS_PLASMA ? "" : "[]";
    int failed = -1;
    for (int i = 0; failed < 0 && i < mediakeys_key_count(desktop); i++) {
        if (!set_key(connection, desktop, i, cleared, why, whycap))
            failed = i;
    }
    if (failed >= 0 && !put_back(connection, desktop, before, failed, why, whycap) && left_changed != NULL)
        *left_changed = true;
    if (connection != NULL)
        plasma_disconnect(connection);
    return failed < 0;
}

// A function to set each of the desktop's media keys, to `saved`'s values when given, else to its defaults.
// They are read first, so that when one fails, it and every key before it are put back as they were: a read
// that fails changes nothing, and *left_changed is true only when a put-back failed
static bool change_keys(MediaKeysDesktop desktop, const MediaKeysState *saved, bool *left_changed, char *why,
                        size_t whycap)
{
    DBusConnection *connection;
    if (!open_desktop(desktop, &connection, why, whycap))
        return false;
    MediaKeysState before;
    bool read = read_keys(connection, desktop, &before, why, whycap);
    int failed = -1;
    for (int i = 0; read && failed < 0 && i < mediakeys_key_count(desktop); i++) {
        bool changed = saved != NULL ? set_key(connection, desktop, i, saved->values[i], why, whycap)
                                     : default_key(connection, desktop, i, why, whycap);
        if (!changed)
            failed = i;
    }
    if (failed >= 0 && !put_back(connection, desktop, &before, failed, why, whycap) && left_changed != NULL)
        *left_changed = true;
    if (connection != NULL)
        plasma_disconnect(connection);
    return read && failed < 0;
}

// A function to set each of the desktop's media keys back to a saved value
bool mediakeys_write(const MediaKeysState *saved, bool *left_changed, char *why, size_t whycap)
{
    if (left_changed != NULL)
        *left_changed = false;
#ifdef STREAMFLEX_TEST_HOOKS
    if (fake_path() != NULL)
        return fake_write(saved, left_changed, why, whycap);
#endif
    return change_keys(saved->desktop, saved, left_changed, why, whycap);
}

// A function to set each of the desktop's media keys to its default: kglobalaccel's default shortcut, or
// gsettings reset, the schema's own
bool mediakeys_reset_defaults(MediaKeysDesktop desktop, bool *left_changed, char *why, size_t whycap)
{
    if (left_changed != NULL)
        *left_changed = false;
#ifdef STREAMFLEX_TEST_HOOKS
    if (fake_path() != NULL)
        return fake_reset_defaults(desktop, why, whycap);
#endif
    return change_keys(desktop, NULL, left_changed, why, whycap);
}
