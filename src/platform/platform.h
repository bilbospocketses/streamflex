#ifdef _WIN32
#define FILE_MODE_WRITE "wt"
#else
#define FILE_MODE_WRITE "w"
#endif

// Abstracted platform function prototypes
bool file_exists(const char *path);
bool directory_exists(const char *path);
void get_region(char *buffer);
void scan_slideshow_directory(Slideshow *slideshow, const char *directory);
bool start_process(char *cmd, bool application);
void scmd_shutdown(void);
void scmd_restart(void);
void scmd_sleep(void);
bool find_self(const char *argv0);   // A restart: before anything is torn down; false, logged, when not found
bool start_self(char **argv);        // A restart: after the teardown; false, logged, when it could not start (Windows: lets make_self()'s copy run)

// Linux-specific function prototypes
#ifdef __unix__
#include "../mediakeys.h"   // MediaKeysState
void make_directory(const char *directory);
bool home_directory(char *buffer, size_t size);
#ifdef __linux__
void keep_name(const char *argv0);   // A restart's fresh copy: named after argv[0] again, not /proc/self/exe's "exe"
#endif
void print_usage(void);

// The desktop's media-key shortcuts (mediakeys_unix.c): KDE Plasma's through kglobalaccel, GNOME's through
// gsettings. Each is false, with why, when a call to the desktop failed
bool mediakeys_read(MediaKeysState *out, char *why, size_t whycap);   // The desktop (MEDIAKEYS_NONE, true, for neither) and each key's value; out->desktop is set on failure too
bool mediakeys_clear(MediaKeysDesktop desktop, const MediaKeysState *before, char *why, size_t whycap);   // Every key cleared; a failure puts the keys already cleared back from before first
bool mediakeys_write(const MediaKeysState *saved, char *why, size_t whycap);   // Each key set back to its saved value
bool mediakeys_reset_defaults(MediaKeysDesktop desktop, char *why, size_t whycap);   // Each key set to the desktop's default
#endif

// Windows-specific function prototypes
#ifdef _WIN32
#include "appsfolder.h"   // appsfolder_scan(), appicon_image()
bool has_exit_hotkey(void);
void set_exit_hotkey(SDL_Keycode keycode);
SDL_Keycode exit_hotkey_keycode(void);   // 0 when there is no exit hotkey
int unused_exit_hotkeys(SDL_Keycode *out, int max);   // The later :exit bindings, which nothing runs; how many
SDL_Keycode refused_exit_hotkey(const char **why);    // The exit hotkey Windows would not register, and why; 0 when none
void register_exit_hotkey(void);
void platform_window_changed(SDL_Window *old_window, SDL_Window *new_window);   // UnregisterHotKey on the old HWND, every hotkey on the new one, and the taskbar mark when the new window is Transparent
bool retry_exit_hotkey(bool at_once);   // The exit hotkey Windows refused, tried again when due; true when taken now
void clear_exit_hotkey(void);
void listen_exit_hotkey(bool on);   // While an app runs, the exit hotkey's WM_HOTKEY closes it
void set_foreground_window(void);
bool make_self(void);           // A restart: the fresh copy, made and given the foreground before the teardown
bool take_foreground(void);     // A restart's fresh copy: true when its window came to the front
void hide_cursor(Entry* entry);
void taskbar_mark_fullscreen(SDL_Window *window, bool mark);   // ITaskbarList2::MarkFullscreenWindow on the window's HWND
void taskbar_release(void);                                    // At quit: the taskbar's list, and COM if it was started for it
const char *dpi_awareness(void);   // How Windows scales this process's windows, as the manifest set it
#endif