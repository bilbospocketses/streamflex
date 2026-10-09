#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>
#include <SDL.h>
#include <ini.h>
#include "../launcher.h"
#include <launcher_config.h>
#include "unix.h"
#include "../util.h"
#include "../debug.h"
#include "../fileio.h"
#include "../browser.h"
#include "platform.h"

static int desktop_handler(void *user, const char *section, const char *name, const char *value);
static void strip_field_codes(char *cmd);
static bool ends_with(const char *string, const char *phrase);

static char self_path[4096];   // The program a restart starts, found before anything is torn down

// A function to handle .desktop lines
static int desktop_handler(void *user, const char *section, const char *name, const char *value)
{
    Desktop *pdesktop = (Desktop*) user;
    if (!strcmp(pdesktop->section, section) && !strcmp(name, KEY_EXEC))
        pdesktop->exec = strdup(value);
    return 0;
}

// A function to determine if a file exists in the filesystem
bool file_exists(const char *path)
{
    return access(path, R_OK) ? false : true;
}

// A function to determine if a directory exists in the filesystem
bool directory_exists(const char *path)
{
    struct stat directory;
    return stat(path, &directory) == 0 && S_ISDIR(directory.st_mode) ? true : false;
}

// A function to remove field codes from .desktop file Exec line
static void strip_field_codes(char *cmd)
{
    size_t start = 0;
    for (size_t i = 0; i < strlen(cmd); i++) {
        if (cmd[i] == '%' && i > 0 && cmd[i - 1] == ' ')
            start = i;
        else if (start && i > start + 2 && cmd[i] != ' ') {
            strcpy(cmd + start, cmd + i);
            start = 0;
        }
    }
    if (start)
        cmd[start - 1] ='\0'; 
}

// A function to make a directory, including any intermediate
// directories if necessary
void make_directory(const char *directory) 
{
    char buffer[MAX_PATH_CHARS + 1];
    char *i = NULL;
    size_t length;
    snprintf(buffer, sizeof(buffer), "%s", directory);
    length = strlen(buffer);
    if (buffer[length - 1] == '/')
        buffer[length - 1] = '\0';
    for (i = buffer + 1; *i != '\0'; i++) {
        if (*i == '/') {
            *i = '\0';
            mkdir(buffer, S_IRWXU);
            *i = '/';
        }
    }
    mkdir(buffer, S_IRWXU);
}

// A function to find the user's home folder: HOME, else the user database's entry, as a login shell does
bool home_directory(char *buffer, size_t size)
{
    const char *home = getenv("HOME");
    char entry_text[4096];
    struct passwd entry;
    struct passwd *found = NULL;

    // An empty HOME names no folder any more than a missing one does
    if (home == NULL || home[0] == '\0') {
        home = NULL;
        if (getpwuid_r(getuid(), &entry, entry_text, sizeof(entry_text), &found) == 0 && found != NULL)
            home = found->pw_dir;
    }
    if (home == NULL || home[0] == '\0')
        return false;
    int written = snprintf(buffer, size, "%s", home);
    return written > 0 && (size_t) written < size;
}

// A function to determine if a string ends with a phrase
static bool ends_with(const char *string, const char *phrase)
{
    size_t len_string = strlen(string);
    size_t len_phrase = strlen(phrase);
    if (len_phrase > len_string)
        return false;
    char *p = (char*) string + len_string - len_phrase;
    return strcmp(p, phrase) ? false : true;
}

// A function to launch an external application
bool start_process(char *cmd, bool application)
{
    // Check if the command is an XDG .desktop file
    char *exec = NULL;
    char *tmp = strdup(cmd);
    char *file = strtok(tmp, DELIMITER_ACTION);
    if (ends_with(file, EXT_DESKTOP)) {
        Desktop desktop;
        desktop.exec = NULL;

        // Parse the desktop action from the command (if any)
        const char* const action = strtok(NULL, DELIMITER_ACTION);
        if (action == NULL)
            copy_string(desktop.section, DESKTOP_SECTION_HEADER, sizeof(desktop.section));
        else
            snprintf(desktop.section, sizeof(desktop.section), DESKTOP_SECTION_HEADER_ACTION, action);

        // Parse the .desktop file for the Exec line value
        int error = ini_parse(file, desktop_handler, &desktop);
        if (error < 0) {
            log_error("Desktop file '%s' not found", file);
            free(tmp);
            return false;
        }
        if (desktop.exec == NULL) {
            log_debug("No Exec line found in desktop file '%s'", cmd);
            free(tmp);
            return false;
        }
        exec = desktop.exec;
        strip_field_codes(exec);
        cmd = exec;
    }
    free(tmp);

    // Fork new system shell process
    pid_t child_pid = fork();
    switch(child_pid) {
        case -1:
            log_error("Could not fork new process for application");
            free(exec);
            return false;

        // Child process
        case 0:
            setpgid(0, 0);
            const char *file = "/bin/sh";
            const char *args[] = {
                "sh",
                "-c", 
                cmd, 
                NULL
            };
            execvp(file, (char* const*) args);
            break;

        // Parent process
        default:
            if (!application) 
                return true;
            int status;

            // Check to see if the shell successfully launched
            SDL_Delay(10);
            waitpid(child_pid, &status, WNOHANG);
            if (WIFEXITED(status) && WEXITSTATUS(status) > 126) {
                log_error("Application failed to launch");
                return false;
            }
            log_debug("Application launched successfully");
            break;
    }
    free(exec);
    return true;
}

// A function to scan the slideshow directory for image files, by the rule the settings' folder
// browser uses (browser_is_image_file): regular files only, so a pipe named like an image never holds
// the launcher in its read; any case of extension; hidden files left out. It lists the
// folder as the browser does (fileio_list), so a link onto a network mount is never followed, and
// an entry the file system gives no kind for costs at most one lookup of the entry itself.
void scan_slideshow_directory(Slideshow *slideshow, const char *directory)
{
    FileioEntry *entries = NULL;
    int count = fileio_list(directory, &entries);
    char file_path[MAX_PATH_CHARS + 1];
    for (int i = 0; i < count; i++) {
        if (!browser_is_image_file(&entries[i]))
            continue;
        join_paths(file_path, sizeof(file_path), 2, directory, entries[i].name);
        char **grown = realloc(slideshow->images, (size_t) (slideshow->num_images + 1) * sizeof(char*));
        if (grown == NULL)
            break;
        slideshow->images = grown;
        slideshow->images[slideshow->num_images] = strdup(file_path);
        slideshow->num_images++;
    }
    fileio_free_list(entries, count);
}

// A function to get the 2 letter region code from LANG ("en_US.UTF-8" gives "US"). It cuts up a
// copy: strtok() on getenv()'s own string would cut the environment's LANG short for everything
// launched afterwards, and with LANG unset would go on from another caller's string.
void get_region(char *buffer)
{
    const char *lang = getenv("LANG");
    if (lang == NULL)
        return;
    char copy[64];
    snprintf(copy, sizeof(copy), "%s", lang);
    char *rest = NULL;
    char *token = strtok_r(copy, "_", &rest);
    if (token == NULL)
        return;
    token = strtok_r(NULL, ".", &rest);
    if (token != NULL && strlen(token) == 2)
        copy_string(buffer, token, 3);
}

// A function to shutdown the computer
void scmd_shutdown()
{
    start_process(CMD_SHUTDOWN, false);
}

// A function to restart the computer
void scmd_restart()
{
    start_process(CMD_RESTART, false);
}

// A function to put the computer to sleep
void scmd_sleep()
{
    start_process(CMD_SLEEP, false);
}

// A function to tell whether a path is a program this user may run: a regular file it may execute
static bool runnable(const char *path)
{
    struct stat file;
    return stat(path, &file) == 0 && S_ISREG(file.st_mode) && access(path, X_OK) == 0;
}

// A function to find this program for a restart to start again, before anything is torn down:
// /proc/self/exe, the kernel's link to it, else argv[0] as it was started, a path as given or a name
// in a folder of the PATH. False, logged, when none of them is a program that can be run.
bool find_self(const char *argv0)
{
    const char *self = "/proc/self/exe";
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_RESTART_SELF names the file looked for
    // in place of /proc/self/exe, so a check can make it missing, or a file that is not a program
    if (getenv("STREAMFLEX_TEST_RESTART_SELF") != NULL)
        self = getenv("STREAMFLEX_TEST_RESTART_SELF");
#endif
    bool found = runnable(self);
    if (found)
        copy_string(self_path, self, sizeof(self_path));
    else if (strchr(argv0, '/') != NULL) {
        found = runnable(argv0);
        copy_string(self_path, argv0, sizeof(self_path));
    }
    else {
        // Each folder of the PATH in turn, as a shell looks a name up
        const char *folder = getenv("PATH");
        while (folder != NULL && !found) {
            const char *end = strchr(folder, ':');
            int length = end != NULL ? (int) (end - folder) : (int) strlen(folder);
            snprintf(self_path, sizeof(self_path), "%.*s/%s", length, folder, argv0);
            found = runnable(self_path);
            folder = end != NULL ? end + 1 : NULL;
        }
    }
    if (!found) {
        log_error("Cannot restart StreamFlex: the program is neither at %s nor found as %s", self, argv0);
        return false;
    }
    log_debug("Restart: the program is %s", self_path);
    return true;
}

// A function to give a restart's fresh copy its program's name back. find_self() execs through
// /proc/self/exe, which runs this very file even after an upgrade has replaced or removed it, and
// the kernel then names the process after that path's last name, "exe": the name pgrep, pkill and
// ps read (/proc/<pid>/comm). The name becomes argv[0]'s last name again, as the first start had it.
void keep_name(const char *argv0)
{
    const char *name = strrchr(argv0, '/');
    name = name != NULL ? name + 1 : argv0;
    if (prctl(PR_SET_NAME, name, 0, 0, 0) != 0)
        log_error("Restart: the process could not be named %s again: %s", name, strerror(errno));
}

// A function to start the program find_self() found in this process's place (exec), with the
// arguments given; comes back only when it could not, with the reason logged
bool start_self(char **argv)
{
    execv(self_path, argv);
    log_error("Could not restart StreamFlex: %s did not start: %s", self_path, strerror(errno));
    return false;
}

// A function to print usage to the command line
void print_usage()
{
    printf("Usage: " EXECUTABLE_TITLE " [OPTIONS]\n");
    printf("  -c p, --config=p   Load config file from path p.\n");
    printf("  -d,   --debug      Enable debug messages.\n");
    printf("  -h,   --help       Show this help message.\n");
    printf("  -v,   --version    Print version information.\n");
    printf("        --restarted  Internal: StreamFlex restarting itself, which runs no StartupCmd.\n");
}
