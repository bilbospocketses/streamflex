#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <getopt.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include "launcher.h"
#include <launcher_config.h>
#include "util.h"
#include "library.h"
#include "fileio.h"
#include "settings.h"
#include "config_fields.h"
#include "debug.h"
#include "platform/platform.h"
#include <ini.h>

static Menu *create_menu(const char *menu_name, size_t *num_menus);
static bool gamepad_command_mapped(const char *cmd);
static bool gamepad_control_mapped(const char *label);
static void add_default_controls(const char *cmd, const char *const *labels, size_t count);

extern Config          config;
extern GamepadControl  *gamepad_controls;
extern Hotkey          *hotkeys;
Menu                   *menu  = NULL;
Entry                  *entry = NULL;
static bool            columns_set = false; // Columns wins over its older name, MaxButtons

static const char *mode_settings[][5] = {
    {"Color", "Image", "Slideshow", "Transparent", NULL}, // Background Mode
    {"Blank", "None", "Quit", NULL, NULL},                // OnLaunch
    {"Truncate", "Shrink", "None", NULL, NULL},           // OversizeMode ("Truncated" is read too)
    {"Left", "Right", NULL, NULL, NULL},                  // Clock Alignment
    {"24hr", "12hr", "Auto", NULL, NULL},                 // Clock Format
    {"Big", "Little", "Auto", NULL, NULL}                 // Date Format
};

// A function to handle the arguments from the command line
void handle_arguments(int argc, char *argv[], char **config_file_path)
{
    // Parse command line arguments
    if (argc > 1) {
        bool version = false;
        bool help = false;
        int rc;
        const char *short_opts = "hvc:d";
        static const struct option long_opts[] = {
            { "help",         no_argument,       NULL, 'h' },
            { "version",      no_argument,       NULL, 'v' },
            { "config",       required_argument, NULL, 'c' },
            { "debug",        no_argument,       NULL, 'd' },
            { 0, 0, 0, 0 }
        };
    
        while ((rc = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
            switch (rc) {
                case 'h':
                    help = true;
                    break;

                case 'v':
                    version = true;
                    break;

                case 'c':
                    if (file_exists(optarg))
                        *config_file_path = strdup(optarg);
                    else
                        log_fatal("Config file '%s' not found", optarg);
                    break;

                case 'd':
                    config.debug = true;
                    break;
            }
        }

        // Check version, help flags
#ifdef __unix__
        if (version) {
            print_version(stdout);
            fputs("\n", stdout);
            print_compiler_info(stdout);
            quit(EXIT_SUCCESS);
        }
        if (help) {
            print_usage();
            quit(EXIT_SUCCESS);
        }
#endif
    }

    // Try to find config file if none is specified on the command line
    if (*config_file_path == NULL) {
#ifdef __unix__
        const char *prefixes[4];
        char home[MAX_PATH_CHARS + 1];
        char home_config_buffer[MAX_PATH_CHARS + 1];
        prefixes[0] = CURRENT_DIRECTORY;
        prefixes[1] = config.exe_path;
        // With no home folder there is no ~/.config to look in; find_file skips a NULL prefix
        prefixes[2] = home_directory(home, sizeof(home))
                      ? join_paths(home_config_buffer, sizeof(home_config_buffer), 3, home, ".config", EXECUTABLE_TITLE)
                      : NULL;
        prefixes[3] = PATH_CONFIG_SYSTEM;
        *config_file_path = find_file(FILENAME_DEFAULT_CONFIG, 4, prefixes);
#else
        char *prefixes[2];
        prefixes[0] = CURRENT_DIRECTORY;
        prefixes[1] = config.exe_path;
        *config_file_path = find_file(FILENAME_DEFAULT_CONFIG, 2, prefixes);
#endif

        if (*config_file_path == NULL)
            log_fatal("No config file found");
    }

    // Keep the file's absolute path, so the log, the settings screen's save and its messages say
    // where it is, not ".\config.ini" from wherever StreamFlex was started. On Linux a link is
    // followed, as the save follows it, so a path that changes is logged as given and as found.
    char full_path[MAX_PATH_CHARS + 1];
    if (!fileio_full_path(*config_file_path, full_path, sizeof(full_path))) {
        log_debug("Config file: its full path could not be found (%s), so it is named as given", fileio_last_error());
        log_debug("Config file found: %s", *config_file_path);
    }
    else if (strcmp(full_path, *config_file_path) == 0)
        log_debug("Config file found: %s", *config_file_path);
    else {
        log_debug("Config file found: %s (%s)", *config_file_path, full_path);
        char *copy = strdup(full_path);
        if (copy != NULL) {
            free(*config_file_path);
            *config_file_path = copy;
        }
    }
}

// A function to parse the config file and store the settings into the config struct
void parse_config_file(const char *config_file_path)
{
    FILE *file = fileio_open(config_file_path, "r");
    if (file == NULL)
        log_fatal("Could not open config file %s: %s", config_file_path, fileio_last_error());
    int error = ini_parse_file(file, config_handler, NULL);
    fclose(file);
    
    if (error < 0)
        log_fatal("Could not parse config file");
}

// A function to tell whether a section holds settings rather than a menu's entries
static bool settings_section(const char *section)
{
    static const char *const sections[] = {
        "General", "Layout", "Background", "Titles", "Highlight", "Scroll Indicators", "Clock",
        "Screensaver", "Gamepad"
    };
    for (size_t i = 0; i < sizeof(sections) / sizeof(sections[0]); i++) {
        if (MATCH(section, sections[i]))
            return true;
    }
    return false;
}

// A function to handle config file parsing: every setting through the settings table, the hotkeys
// and gamepad controls into their lists, and every other section as a menu
int config_handler(void *user, const char *section, const char *name, const char *value)
{
    UNUSED(user);

    if (MATCH(section, "Hotkeys")) {
        char *rest = NULL;
        char *keycode = strtok_r((char*) value, ";", &rest);
        if (keycode != NULL) {
            char *cmd = strtok_r(NULL, "", &rest);
            if (cmd != NULL)
                add_hotkey(keycode, cmd);
        }
        return 0;
    }

    if (settings_section(section)) {
        const SettingDef *def = setting_find(section, name);
        if (def == NULL) {
            // Any other key in [Gamepad] is a control; one in any other settings section is ignored
            if (MATCH(section, "Gamepad"))
                add_gamepad_control(name, value);
            return 0;
        }
        bool alias = def->alias != NULL && MATCH(name, def->alias);
        SettingValue parsed;
        if (!setting_parse(def, value, &parsed))
            log_error("Invalid %s value '%s' in [%s], ignoring it", name, value, section);
        else if (!alias || !columns_set) {
            config_store(def->id, NULL, &parsed);
            // Columns wins over its older name, MaxButtons, whichever comes first
            if (def->id == SET_ID_LAYOUT_COLUMNS && !alias)
                columns_set = true;
        }
        return 0;
    }

    // Parse menus/entries
    {
        Entry *previous_entry = NULL;

        // Point the menu and entry cursors at this section's menu, adding it to the end of the list
        // when it is new. A section can appear twice with another menu between, so the cursors move
        // back to it, and to its last entry, rather than staying on the last menu read.
        Menu *section_menu = NULL;
        Menu *last_menu = NULL;
        for (Menu *tmp = config.first_menu; tmp != NULL; tmp = tmp->next) {
            if (section_menu == NULL && MATCH(tmp->name, section))
                section_menu = tmp;
            last_menu = tmp;
        }
        if (section_menu == NULL) {
            section_menu = create_menu(section, &config.num_menus);
            if (last_menu == NULL)
                config.first_menu = section_menu;
            else
                last_menu->next = section_menu;
        }
        if (section_menu != menu) {
            menu = section_menu;
            entry = menu->first_entry;
            while (entry != NULL && entry->next != NULL)
                entry = entry->next;
        }

        // Per-menu layout settings. They count only when the value is a number, so an
        // existing entry that happens to be keyed Rows, Columns or IconSize still parses.
        bool layout_key = MATCH(name, SETTING_ROWS) || MATCH(name, SETTING_COLUMNS) ||
                          MATCH(name, SETTING_ICON_SIZE);
        if (layout_key && strchr(value, ';') == NULL) {
            SettingId id = MATCH(name, SETTING_ROWS) ? SET_ID_MENU_ROWS
                         : MATCH(name, SETTING_COLUMNS) ? SET_ID_MENU_COLUMNS : SET_ID_MENU_ICON_SIZE;
            SettingValue parsed;
            if (!setting_parse(setting_def(id), value, &parsed))
                log_error("Invalid %s value '%s' in menu '%s', ignoring it", name, value, section);
            else
                config_store(id, menu, &parsed);
            return 0;
        }
        if (layout_key)
            log_error("Menu '%s': '%s' holds an entry, so it is read as an entry", section, name);

        // Parse entry line for title, icon path, command
        char *string = (char*) value;
        char *token;
        char *delimiter = ";";
        char *rest = NULL;
        token = strtok_r(string, delimiter, &rest);
        if (token == NULL) {
            log_error("Menu '%s': '%s' is empty, ignoring it", section, name);
            return 0;
        }

        // Create first entry in the menu if none exists
        if (menu->first_entry == NULL) {
            menu->first_entry = calloc(1, sizeof(Entry));
            entry = menu->first_entry;
            entry->next = NULL;
        }

        // Add entry to the end of the linked list
        else {
            previous_entry = entry;
            entry = entry->next;
            entry = calloc(1, sizeof(Entry));
            previous_entry->next = entry;
            entry->next = NULL;
        }
        entry->title_offset = 0;

        // Store data in entry struct
        int i;
        for (i = 0;i < 3 && token != NULL; i++) {
            if (i == 0)
                entry->title = strdup(token);
            else if (i == 1) {
                entry->icon_path = strdup(token);
                clean_path(entry->icon_path);
                delimiter = "";
            }
            else if (i == 2)
                entry->cmd = strdup(token);

            token = strtok_r(NULL, delimiter, &rest);
        }

        // Delete entry if parse failed to find 3 valid tokens, or its command is :select
        if (i != 3 || MATCH(":select", entry->cmd)) {
            if (i != 3)
                log_error("Menu '%s': '%s' needs a title, an icon and a command, ignoring it", section, name);
            else
                log_error("Menu '%s': '%s' uses :select, which only a hotkey or gamepad button can, ignoring it",
                    section, name);
            free(entry->title);
            free(entry->icon_path);
            free(entry->cmd);
            if (menu->num_entries == 0) {
                free(menu->first_entry);
                menu->first_entry = NULL;
            }
            else {
                free(entry);
                entry = previous_entry;
                entry->next = NULL;
            }
        }
        else {
            if (menu->num_entries == 0)
                entry->previous = NULL;
            else
                entry->previous = previous_entry;
            menu->num_entries++;
            entry->icon_selected_path = selected_path(entry->icon_path);
        }
    }
    return 0;
}

const char *get_mode_setting(int type, int value)
{
    return mode_settings[type][value];
}

// A function to remove quotation marks that enclose a path
// because SDL cannot handle them
void clean_path(char *path)
{
    size_t length = strlen(path);
    if (length >= 3 && path[0] == '"' && path[length - 1] == '"') {
        path[length - 1] = '\0';
        for (size_t i = 1; i <= length; i++)
            *(path + i - 1) = *(path + i);
    }    
}

// A function to get the selected path 
char *selected_path(const char *path)
{
    char buffer[MAX_PATH_CHARS + 1];
    size_t length = strlen(path);
    char *out = NULL;

    // Find file extension; an empty path has none, and the search below would start before it
    if (length == 0 || length + LEN(SELECTED_SUFFIX) + 1 > sizeof(buffer))
        return out;
    char *p = (char*) path + length - 1;
    while (*p != '.' && p > path)
        p--;
    if (p == path)
        return out;

    // Assemble path with suffix; the length test above makes room for all of it
    snprintf(buffer, sizeof(buffer), "%.*s%s%s", (int) (p - path), path, SELECTED_SUFFIX, p);

    if (file_exists(buffer))
        out = strdup(buffer);
    return out;
}

// A function to copy a string into an existing buffer
void copy_string(char *dest, const char *string, size_t size)
{
    // As much as fits, and zeros to the end of the buffer, as strncpy() left it
    size_t length = strnlen(string, size - 1);
    memcpy(dest, string, length);
    memset(dest + length, '\0', size - length);
}

// A function to join paths together
char *join_paths(char *buffer, size_t bytes, int num_paths, ...)
{
    va_list list;
    const char *arg;
    size_t length;
    size_t used = 0;
    va_start(list, num_paths);

    // Add each subdirectory to path, as much of it as fits
    for (int i = 0; i < num_paths && used + 1 < bytes; i++) {
        arg = va_arg(list, char*);

        // Don't copy preceding slash if present
        if (i != 0 && (*arg == '/' || *arg == '\\'))
            arg++;
        length = strnlen(arg, bytes - 1 - used);
        memcpy(buffer + used, arg, length);
        used += length;
        buffer[used] = '\0';

        // Add trailing slash if not present, except last argument
        if (i != num_paths - 1 && used + 1 < bytes &&
        (used == 0 || (buffer[used - 1] != '/' && buffer[used - 1] != '\\'))) {
            buffer[used++] = PATH_SEPARATOR[0];
            buffer[used] = '\0';
        }
    }
    va_end(list);
    return buffer;
}

// A function to find a file from a filename and list of path prefixes
char *find_file(const char *file, int num_prefixes, const char **prefixes)
{
    char buffer[MAX_PATH_CHARS + 1];
    for (int i = 0; i < num_prefixes; i++) {
        if (prefixes[i] != NULL) {
            join_paths(buffer, sizeof(buffer), 2, prefixes[i], file);
            if (file_exists(buffer)) {
                char *output;
                output = strdup(buffer);
                return output;
            }
        }
    }
    return NULL;
}

// A function to extract the Unicode code point from the first character in a UTF-8 string
Uint16 get_unicode_code_point(const char *p, int *bytes)
{
    Uint16 result;

    // 1 byte ASCII char
    if ((*p & 0x80) == 0) {
        result = (Uint16) *p;
        *bytes = 1;
    }

    // If byte is 110xxxxx, then it's a 2 byte char
    else if ((*p & 0xE0) == 0xC0) {
        Uint8 byte1 = *p & 0x1F;
        Uint8 byte2 = *(p + 1) & 0x3F;
        result = (Uint16) ((byte1 << 6) + byte2);
        *bytes = 2;
    }

    // If byte is 1110xxxx, then it's a 3 byte char
    else if ((*p & 0xF0) == 0xE0) {
        Uint8 byte1 = *p & 0x0F;
        Uint8 byte2 = *(p + 1) & 0x3F;
        Uint8 byte3 = *(p + 2) & 0x3F;
        result = (Uint16) ((byte1 << 12) + (byte2 << 6) + byte3);
        *bytes = 3;
    }
    else {
        result = 0;
        *bytes = 1;
    }
    return result;
}

// A function to generate an array of random indices
void random_array(int *array, int array_size)
{
    // Fill array with initial indices
    for (int i = 0; i < array_size; i++)
        array[i] = i;

    // Shuffle array indices randomly, see https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
    srand((unsigned int) time(NULL));
    int tmp;
    for (int i = 0; i < array_size - 1; i++) {
        int j = (rand() % (array_size - i)) + i;
        tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }
}

// A function to add a hotkey to the linked list
void add_hotkey(const char *keycode, const char *cmd)
{
    if (keycode[0] != '#')
        return;
    char *p = (char*) keycode + 1;

    // Convert hex string to binary
    SDL_Keycode code = (SDL_Keycode) strtol(p, NULL, 16);

    // Check if exit hotkey for Windows
#ifdef _WIN32
    if (MATCH(cmd, SCMD_EXIT)) {
        set_exit_hotkey(code);
        return;
    }
#endif

    // Add to the end of the list, found each time: settings rebuild the list, so no tail pointer lives on
    Hotkey *hotkey = malloc(sizeof(Hotkey));
    hotkey->keycode = code;
    hotkey->cmd = strdup(cmd);
    hotkey->next = NULL;
    if (hotkeys == NULL)
        hotkeys = hotkey;
    else {
        Hotkey *last = hotkeys;
        while (last->next != NULL)
            last = last->next;
        last->next = hotkey;
    }
}

// The gamepad's control labels, each with its type and SDL index. bindings.c's LABELS repeats this
// order: change both together.
static const struct gamepad_info GAMEPAD_INFO[] = {
    {SETTING_GAMEPAD_LSTICK_XM,             TYPE_AXIS_NEG, SDL_CONTROLLER_AXIS_LEFTX},
    {SETTING_GAMEPAD_LSTICK_XP,             TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_LEFTX},
    {SETTING_GAMEPAD_LSTICK_YM,             TYPE_AXIS_NEG, SDL_CONTROLLER_AXIS_LEFTY},
    {SETTING_GAMEPAD_LSTICK_YP,             TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_LEFTY},
    {SETTING_GAMEPAD_RSTICK_XM,             TYPE_AXIS_NEG, SDL_CONTROLLER_AXIS_RIGHTX},
    {SETTING_GAMEPAD_RSTICK_XP,             TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_RIGHTX},
    {SETTING_GAMEPAD_RSTICK_YM,             TYPE_AXIS_NEG, SDL_CONTROLLER_AXIS_RIGHTY},
    {SETTING_GAMEPAD_RSTICK_YP,             TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_RIGHTY},
    {SETTING_GAMEPAD_LTRIGGER,              TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_TRIGGERLEFT},
    {SETTING_GAMEPAD_RTRIGGER,              TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_TRIGGERRIGHT},
    {SETTING_GAMEPAD_BUTTON_A,              TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_A},
    {SETTING_GAMEPAD_BUTTON_B,              TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_B},
    {SETTING_GAMEPAD_BUTTON_X,              TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_X},
    {SETTING_GAMEPAD_BUTTON_Y,              TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_Y},
    {SETTING_GAMEPAD_BUTTON_BACK,           TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_BACK},
    {SETTING_GAMEPAD_BUTTON_GUIDE,          TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_GUIDE},
    {SETTING_GAMEPAD_BUTTON_START,          TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_START},
    {SETTING_GAMEPAD_BUTTON_LEFT_STICK,     TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_LEFTSTICK},
    {SETTING_GAMEPAD_BUTTON_RIGHT_STICK,    TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_RIGHTSTICK},
    {SETTING_GAMEPAD_BUTTON_LEFT_SHOULDER,  TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
    {SETTING_GAMEPAD_BUTTON_RIGHT_SHOULDER, TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
    {SETTING_GAMEPAD_BUTTON_DPAD_UP,        TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_DPAD_UP},
    {SETTING_GAMEPAD_BUTTON_DPAD_DOWN,      TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_DPAD_DOWN},
    {SETTING_GAMEPAD_BUTTON_DPAD_LEFT,      TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_DPAD_LEFT},
    {SETTING_GAMEPAD_BUTTON_DPAD_RIGHT,     TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_DPAD_RIGHT}
};

// How many times the gamepad's controls were freed: poll_gamepad() stops reading a list a command
// it ran rebuilt under it
static unsigned int gamepad_controls_freed = 0;

// A function to count the gamepad's control labels
int gamepad_label_count(void)
{
    return (int) (sizeof(GAMEPAD_INFO) / sizeof(GAMEPAD_INFO[0]));
}

// A function to get a gamepad control label's type and SDL index
const struct gamepad_info *gamepad_label_info(int index)
{
    return index >= 0 && index < gamepad_label_count() ? &GAMEPAD_INFO[index] : NULL;
}

// A function to add a gamepad control to the linked list
void add_gamepad_control(const char *label, const char *cmd)
{
    if (cmd[0] == '\0')
        return;

    // Find correct gamepad info for label, return if none found
    size_t i;
    for (i = 0; i < sizeof(GAMEPAD_INFO) / sizeof(GAMEPAD_INFO[0]); i++) {
        if (MATCH(GAMEPAD_INFO[i].label, label))
            break;
    }
    if (i == sizeof(GAMEPAD_INFO) / sizeof(GAMEPAD_INFO[0]))
        return;

    // Add to the end of the list, found each time: settings rebuild the list, so no tail pointer lives on
    GamepadControl *control = malloc(sizeof(GamepadControl));
    *control = (GamepadControl) {
        .type     = GAMEPAD_INFO[i].type,
        .index    = GAMEPAD_INFO[i].index,
        .label    = GAMEPAD_INFO[i].label,
        .repeat   = 0,
        .next     = NULL
    };
    control->cmd = strdup(cmd);
    if (gamepad_controls == NULL)
        gamepad_controls = control;
    else {
        GamepadControl *last = gamepad_controls;
        while (last->next != NULL)
            last = last->next;
        last->next = control;
    }
}

// A function to free every hotkey: at quit, and when settings rebuild the list
void clear_hotkeys(void)
{
    while (hotkeys != NULL) {
        Hotkey *next = hotkeys->next;
        free(hotkeys->cmd);
        free(hotkeys);
        hotkeys = next;
    }
}

// A function to free every gamepad control: at quit, and when settings rebuild the list
void clear_gamepad_controls(void)
{
    while (gamepad_controls != NULL) {
        GamepadControl *next = gamepad_controls->next;
        free(gamepad_controls->cmd);
        free(gamepad_controls);
        gamepad_controls = next;
    }
    gamepad_controls_freed++;
}

// A function to tell how many times the gamepad's controls were freed, so a loop over them can tell
// that a command it ran rebuilt them (the pointer it holds is then gone)
unsigned int gamepad_controls_version(void)
{
    return gamepad_controls_freed;
}

// A function to check whether any gamepad control runs a command
static bool gamepad_command_mapped(const char *cmd)
{
    for (GamepadControl *i = gamepad_controls; i != NULL; i = i->next) {
        if (MATCH(i->cmd, cmd))
            return true;
    }
    return false;
}

// A function to check whether a gamepad control is mapped to anything
static bool gamepad_control_mapped(const char *label)
{
    for (GamepadControl *i = gamepad_controls; i != NULL; i = i->next) {
        if (MATCH(i->label, label))
            return true;
    }
    return false;
}

// A function to map a command to each listed control the config leaves free, unless the
// config already maps the command somewhere itself
static void add_default_controls(const char *cmd, const char *const *labels, size_t count)
{
    if (gamepad_command_mapped(cmd))
        return;
    for (size_t i = 0; i < count; i++) {
        if (!gamepad_control_mapped(labels[i]))
            add_gamepad_control(labels[i], cmd);
    }
}

// A function to give Up, Down and :settings default gamepad controls. Configs written before
// grids existed map nothing to :up or :down, and a grid is unusable without them; and any config
// written before the settings screen needs a way to open it.
void add_default_gamepad_controls()
{
    static const char *const up[] = { SETTING_GAMEPAD_BUTTON_DPAD_UP, SETTING_GAMEPAD_LSTICK_YM };
    static const char *const down[] = { SETTING_GAMEPAD_BUTTON_DPAD_DOWN, SETTING_GAMEPAD_LSTICK_YP };
    static const char *const settings[] = { SETTING_GAMEPAD_BUTTON_START };
    add_default_controls(SCMD_UP, up, sizeof(up) / sizeof(up[0]));
    add_default_controls(SCMD_DOWN, down, sizeof(down) / sizeof(down[0]));
    add_default_controls(SCMD_SETTINGS, settings, sizeof(settings) / sizeof(settings[0]));
}

// A function to retreive menu struct from the linked list via the menu name
Menu *get_menu(const char *menu_name)
{
    for (Menu *m = config.first_menu; m != NULL; m = m->next) {
        if (MATCH(menu_name, m->name))
            return m;
    }
    log_error("Menu '%s' not found in config file", menu_name);
    return NULL;
}

// A function to allocate memory to and initialize a menu struct
Menu *create_menu(const char *menu_name, size_t *num_menus)
{
    Menu *new_menu = malloc(sizeof(Menu));
    *new_menu = (Menu) {
        .first_entry = NULL,
        .items = NULL,
        .next = NULL,
        .back = NULL,
        .num_entries = 0,
        .overrides = { 0, 0, 0 },
        .position = { 0, 0 },
        .rendered_size = 0
    };
    new_menu->name = strdup(menu_name);
    (*num_menus)++;

    return new_menu;
}

// A function to give every menu an array of its entries by index, for the layout maths
void build_menu_items()
{
    for (Menu *m = config.first_menu; m != NULL; m = m->next) {
        if (m->num_entries == 0)
            continue;
        m->items = malloc(m->num_entries * sizeof(Entry*));
        Entry *e = m->first_entry;
        for (unsigned int i = 0; i < m->num_entries; i++) {
            m->items[i] = e;
            e = e->next;
        }
    }
}

// A function to pass the icon library's warnings to the log
static void library_warning(const char *message)
{
    log_error("%s", message);
}

// A function to point every entry that names a library icon at its file. It also rescues a missing
// file that the library can stand in for (see library_rescue): a path to one of the seven icons older
// versions shipped, or a name typed with capitals or stray spaces. Both use the library icon and log a note.
void resolve_library_icons(void)
{
    library_set_warn(library_warning);
    char exe_library[MAX_PATH_CHARS + 1];
    const char *roots[2];
    roots[0] = config.exe_path != NULL
               ? join_paths(exe_library, sizeof(exe_library), 4, config.exe_path, PATH_ASSETS_EXE, PATH_ICONS_EXE, PATH_LIBRARY_EXE)
               : NULL;
#ifdef __unix__
    roots[1] = PATH_LIBRARY_SYSTEM;
#else
    roots[1] = PATH_LIBRARY_RELATIVE;
#endif
    const char *root = NULL;
    char manifest[MAX_PATH_CHARS + 1];
    for (int i = 0; i < 2 && root == NULL; i++) {
        if (roots[i] != NULL && file_exists(join_paths(manifest, sizeof(manifest), 2, roots[i], LIBRARY_MANIFEST)))
            root = roots[i];
    }
    bool loaded = false;
    if (root == NULL)
        log_error("Icon library not found; entries that name a library icon will have no image");
    else {
        int count = library_load(root);
        loaded = count >= 0;
        if (loaded)
            log_debug("Icon library: %s (%i icons)", root, count);
        else
            log_error("Icon library at %s could not be read; entries that name a library icon will have no image", root);
    }

    for (Menu *m = config.first_menu; m != NULL; m = m->next) {
        for (Entry *e = m->first_entry; e != NULL; e = e->next) {
            const char *path = NULL;
            if (e->icon_path == NULL)
                continue;
            if (library_is_name(e->icon_path)) {
                path = library_lookup(e->icon_path);
                // Without a library every name misses; the line above already says why, once
                if (path == NULL && loaded) {
                    log_error("Entry '%s' in menu '%s': no library icon named '%s'", e->title, m->name, e->icon_path);
                    path = library_lookup(LIBRARY_FALLBACK_ICON);
                }
            }
            else if (!file_exists(e->icon_path)) {
                const char *name = NULL;
                path = library_rescue(e->icon_path, &name);
                if (path != NULL)
                    log_error("Entry '%s' in menu '%s': '%s' is not a file; using the library icon '%s' "
                        "(write '%s' in the config to use it directly)", e->title, m->name, e->icon_path, name, name);
            }
            if (path != NULL) {
                free(e->icon_path);
                e->icon_path = strdup(path);
            }
        }
    }
}

// A function to dynamically allocate a buffer for and copy a formatted string
void sprintf_alloc(char **buffer, const char *format, ...)
{
    va_list args1, args2;
    va_start(args1, format);
    va_copy(args2, args1);
    
    size_t length = (size_t) vsnprintf(NULL, 0, format, args1);
    if (length) {
        *buffer = malloc(length + 1);
        vsnprintf(*buffer, length + 1, format, args2);
    }
    va_end(args1);
    va_end(args2);
}