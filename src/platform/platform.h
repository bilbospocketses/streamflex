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
void make_directory(const char *directory);
bool home_directory(char *buffer, size_t size);
void print_usage(void);
#endif

// Windows-specific function prototypes
#ifdef _WIN32
bool has_exit_hotkey(void);
void set_exit_hotkey(SDL_Keycode keycode);
SDL_Keycode exit_hotkey_keycode(void);   // 0 when there is no exit hotkey
void register_exit_hotkey(void);
void clear_exit_hotkey(void);
void check_exit_hotkey(SDL_SysWMmsg *msg);
void set_foreground_window(void);
bool make_self(void);           // A restart: the fresh copy, made and given the foreground before the teardown
bool take_foreground(void);     // A restart's fresh copy: true when its window came to the front
void make_window_transparent(void);
void make_window_opaque(void);
void hide_cursor(Entry* entry);
#endif