#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <windows.h>
#include <psapi.h>
#include <powrprof.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include "../launcher.h"
#include <launcher_config.h>
#include "platform.h"
#include "../util.h"
#include "../debug.h"
#include "../fileio.h"
#include "../alloc.h"
#include "../browser.h"

static void parse_command(char *cmd, char *file, size_t file_size, char **params);
static char *path_basename(const char *path);
static bool is_browser(const char *exe_basename);
static UINT sdl_to_win32_keycode(SDL_Keycode keycode);
static bool get_shutdown_privilege(void);

extern Config config;
extern SDL_SysWMinfo wm_info;
bool has_shutdown_privilege     = false;
UINT exit_hotkey                = 0;


// A function to determine if a file exists on the filesystem
bool file_exists(const char *path)
{
    return fileio_exists(path);
}

// A function to determine if a directory exists on the filesystem
bool directory_exists(const char *path)
{
    return fileio_is_dir(path);
}

// A function that parses the command string into a file and parameters
static void parse_command(char *cmd, char *file, size_t file_size, char **params)
{
    char *start = NULL;
    char *quote_begin = NULL;
    char *quote_end = NULL;
    char *p = cmd;
    file[0] = '\0';

    // Skip any whitespace at beginning of command
    while (*p == ' ')
        p++;
    start = p;

    // Check for quote, in which case ignore spaces until the end quote is detecetd
    if (*p == '"')
        quote_begin = p;

    while (*p != '\0') {
        // If the quote is complete, copy the file
        if (*p == '"' && p != quote_begin) {
            quote_end = p;
            *p = '\0';
            copy_string(file, quote_begin + 1, file_size);
        }

        else if (*p == ' ') {
            // If a space was detected but there hasn't been a quote detected yet, this is the end of the file
            if (!quote_begin) {
                *p = '\0';
                copy_string(file, start, file_size);
                p++;

                // Skip any preceding white space for parameters
                while (*p == ' ')
                    p++;

                // Copy parameters
                if (*p != '\0')
                    *params = strdup(p);
                break;
            }

            // If a space was detected after the quote
            else if (quote_begin && quote_end) {
                // Skip any preceding white space for parameters
                while (*p == ' ')
                    p++;

                // Copy rest of command as parameters
                if (*p != '\0')
                    *params = strdup(p);
                break;
            }
        }
        p++;
    }

    // If there were no quotes or spaces, copy whole command into file buffer. `start` points into
    // the command, which the loops above have already read, so it is never NULL here.
    if (file[0] == '\0')
        copy_string(file, start, file_size);
}

void set_foreground_window()
{
    SetForegroundWindow(wm_info.info.win.window);
}

void make_window_transparent()
{
    HWND hwnd = wm_info.info.win.window;
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd,
        RGB(config.chroma_key_color.r, config.chroma_key_color.g, config.chroma_key_color.b),
        0,
        LWA_COLORKEY
    );
}

// A function to make the window solid again after a transparent background
void make_window_opaque()
{
    HWND hwnd = wm_info.info.win.window;
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
}

// When the window is transparent, we need to hide the cursor behind the non-transparent icon
void hide_cursor(Entry *entry)
{
    SetCursorPos(entry->icon_rect.x + entry->icon_rect.w / 2,
        entry->icon_rect.y + entry->icon_rect.h / 2
    );
}

// A function to launch an application. The command is UTF-8, as every string from the config is,
// so it goes to Windows as UTF-16: the ANSI call misread any character outside the system code page.
bool start_process(char *cmd, bool application)
{
    bool ret = false;
    char file[MAX_PATH_CHARS + 1];
    char *params = NULL;
    int cmd_show = application ? SW_SHOWMAXIMIZED : SW_HIDE;

    // Parse command into file and parameters strings
    parse_command(cmd, file, sizeof(file), &params);

    wchar_t *wide_file = fileio_wide(file);
    wchar_t *wide_params = params != NULL ? fileio_wide(params) : NULL;
    BOOL successful = FALSE;
    if (wide_file == NULL || (params != NULL && wide_params == NULL))
        log_error("Could not launch '%s': %s", file, fileio_last_error());
    else {
        // Set up info struct
        SHELLEXECUTEINFOW info = {
            .cbSize = sizeof(SHELLEXECUTEINFOW),
            .fMask = 0,
            .hwnd = NULL,
            .lpVerb = L"open",
            .lpFile = wide_file,
            .lpParameters = wide_params,
            .lpDirectory = NULL,
            .nShow = cmd_show,
            .lpIDList = NULL,
            .lpClass = NULL,
        };
        successful = ShellExecuteExW(&info);
    }
    alloc_free(wide_file);
    alloc_free(wide_params);

    if (!application)
        ret = true;
    else {
        // Go down in the window stack so the launched application can take focus
        if (successful) {
            HWND hwnd = wm_info.info.win.window;
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOREDRAW | SWP_NOSIZE | SWP_NOMOVE);
            ret = true;
        }
        else {
            log_debug("Failed to launch command");
            ret = false;
        }
    }
    free(params);
    return ret;
}

// A function to scan the slideshow directory for image files, by the rule the settings' folder
// browser uses (browser_is_image_file): any case of extension, hidden files left out
void scan_slideshow_directory(Slideshow *slideshow, const char *directory)
{
    FileioEntry *entries = NULL;
    int count = fileio_list(directory, &entries);
    char file_output[MAX_PATH_CHARS + 1];
    for (int i = 0; i < count; i++) {
        if (!browser_is_image_file(&entries[i]))
            continue;
        join_paths(file_output, sizeof(file_output), 2, directory, entries[i].name);
        char **grown = realloc(slideshow->images, (size_t) (slideshow->num_images + 1) * sizeof(char*));
        if (grown == NULL)
            break;
        slideshow->images = grown;
        slideshow->images[slideshow->num_images] = strdup(file_output);
        slideshow->num_images++;
    }
    fileio_free_list(entries, count);
}

// A function to get the 2 letter region code
void get_region(char *buffer)
{
    GEOID geo_id = GetUserGeoID(GEOCLASS_NATION);
    GetGeoInfoA(geo_id, GEO_ISO2, buffer, 3, 0);
}

// A function to shutdown the computer
void scmd_shutdown()
{
    if (!has_shutdown_privilege) {
        bool successful = get_shutdown_privilege();
        if (!successful) 
            return;
    }
    InitiateShutdownA(NULL, 
        NULL, 
        0, 
        SHUTDOWN_FORCE_OTHERS | SHUTDOWN_POWEROFF | SHUTDOWN_HYBRID,
        SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER
    );
}

// A function to restart the computer
void scmd_restart()
{
    if (!has_shutdown_privilege) {
        bool successful = get_shutdown_privilege();
        if (!successful) 
            return;
    }
    InitiateShutdownA(NULL,
        NULL,
        0,
        SHUTDOWN_FORCE_OTHERS | SHUTDOWN_RESTART | SHUTDOWN_HYBRID,
        SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER
    );
}

// A function to put the computer to sleep
void scmd_sleep()
{
    if (!has_shutdown_privilege) {
        bool successful = get_shutdown_privilege();
        if (!successful) 
            return;
    }
    SetSuspendState(FALSE, FALSE, FALSE);
}

// A function to get the shutdown privilege from Windows
static bool get_shutdown_privilege()
{
    HANDLE token = NULL;
    BOOL ret = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token);
    if (!ret) {
        log_error("Could not open process token");
        return false;
    }
    LUID luid;
    ret = LookupPrivilegeValueA(NULL, SE_SHUTDOWN_NAME, &luid);
    if (!ret) {
        log_error("Failed to lookup privilege");
        CloseHandle(token);
        return false;
    }
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    ret = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    if (!ret) {
        log_error("Failed to adjust token privileges");
        CloseHandle(token);
        return false;
    }
    has_shutdown_privilege = true;
    CloseHandle(token);
    return true;
}

// A function to check if there is an exit hotkey
bool has_exit_hotkey()
{
    if (exit_hotkey) 
        return true;
    return false;
}

// A function to store an exit hotkey
void set_exit_hotkey(SDL_Keycode keycode)
{
    if (exit_hotkey) 
        return;
    exit_hotkey = sdl_to_win32_keycode(keycode);
    if (!exit_hotkey)
        log_error("Invalid exit hotkey keycode %X", keycode);
}

// A function to register the exit hotkey with Windows
void register_exit_hotkey()
{
    BOOL ret = RegisterHotKey(wm_info.info.win.window, 1, 0, exit_hotkey);
    if (!ret) {
        exit_hotkey = 0;
        log_error("Failed to register exit hotkey with Windows");
    }
}

// A function to let go of the exit hotkey, before settings bind it again
void clear_exit_hotkey()
{
    if (exit_hotkey)
        UnregisterHotKey(wm_info.info.win.window, 1);
    exit_hotkey = 0;
}

// A function to check if the exit hotkey was pressed, and close the active window if so
void check_exit_hotkey(SDL_SysWMmsg *msg)
{
    if (msg->msg.win.msg == WM_HOTKEY) {
        log_debug("Exit hotkey detected");
        HWND hwnd = GetForegroundWindow();
        if (hwnd == NULL) {
            log_error("Could not get top window");
            return;
        }
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
}

// A function to convert an SDL keycode to a WIN32 virtual keycode
static UINT sdl_to_win32_keycode(SDL_Keycode keycode)
{
#include "keycode_convert.h" // Import the conversion table
    for (int i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (table[i].sdl == keycode)
            return table[i].win;
    }
    return 0;
}
