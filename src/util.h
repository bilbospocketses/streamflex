#include "utf8.h"

#define MAX_LOG_LINE_BYTES 501
#define MAX_PATH_CHARS 1001 //250 wide characters

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

// strtok_r is POSIX; MSVC has the same function, with the same arguments, as strtok_s
#ifdef _MSC_VER
#define strtok_r(string, delimiters, context) strtok_s(string, delimiters, context)
#endif

#define UNUSED(x) (void)(x)
#define SELECTED_SUFFIX "_selected"
#define LEN(x) ((sizeof(x)/sizeof(x[0])) - sizeof(x[0]))
#define MATCH(x, y) !strcmp(x, y)

#define DIV_ROUND_UP(a, b) ((a + (b - 1)) / b)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

struct gamepad_info {               // A gamepad control's type and SDL index; its label is bindings_label()'s
    int type;
    int index;
};

int config_handler(void *user, const char *section, const char *name, const char *value);
const char *get_mode_setting(int type, int value);
char *selected_path(const char *path);
char *join_paths(char *buffer, size_t bytes, int num_paths, ...);
char *find_file(const char *file, int num_prefixes, const char **prefixes);
void handle_arguments(int argc, char *argv[], char **config_file_path);
void copy_string(char* dest, const char* string, size_t size);
void add_hotkey(const char *keycode, const char *cmd);
void add_gamepad_control(const char *label, const char *cmd);
void clear_hotkeys(void);
void clear_gamepad_controls(void);
unsigned int gamepad_controls_version(void);   // Counts each clear_gamepad_controls()
int gamepad_label_count(void);
const struct gamepad_info *gamepad_label_info(int index);
void random_array(int *array, int array_size);
void clean_path(char *path);
void parse_config_file(const char *config_file_path);
void build_menu_items(void);
void resolve_library_icons(void);
void add_default_gamepad_controls(void);
void read_file(const char *path, char **buffer);
void sprintf_alloc(char **buffer, const char *format, ...);
Uint16 get_unicode_code_point(const char *p, int *bytes);
Menu *get_menu(const char *menu_name);
