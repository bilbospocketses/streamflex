// Saving the settings screen's changes into config.ini: only the changed keys, and the changed lines
// of the list sections, applied to the file as it is on disk at that moment, with the old file kept
// as .bak and the new one swapped in whole. Pure: no SDL; memory comes from alloc.h.
#ifndef CONFIG_SAVE_H
#define CONFIG_SAVE_H

#include <stdbool.h>
#include "inidoc.h"

#define CONFIG_SAVE_PATH_MAX 1024

typedef struct {
    const char *section;
    const char *key;
    const char *alias;          // An older name the parser reads as `key` (MaxButtons for Columns),
                                // edited in its place when only it is present; NULL if none
    const char *value;          // NULL removes the key
    IniDocPlacement placement;  // Where a new key goes
} ConfigEdit;

typedef enum {
    CONFIG_LIST_SET,       // Change the line that read `original` when settings opened
    CONFIG_LIST_ADD,       // Add a line after the section's last
    CONFIG_LIST_REMOVE     // Remove the line that read `original`
} ConfigListOp;

// One edit to a list section ([Hotkeys], [Gamepad]), whose lines are found by their text, not their key
typedef struct {
    ConfigListOp op;
    const char *section;
    const char *original;   // SET, REMOVE: the line as settings read it
    const char *key;        // SET, ADD: the key, or with `numbered` its stem ("Hotkey")
    bool numbered;          // Number the key one above the highest <key>N in the section
    const char *value;      // SET, ADD
} ConfigListEdit;

typedef struct {
    char path[CONFIG_SAVE_PATH_MAX];    // The file written, or that could not be
    char backup[CONFIG_SAVE_PATH_MAX];  // Its backup; "" when there was no file to back up
    char why[512];                      // Why the save failed, in a few words
    char warning[160];                  // What a save that succeeded could not keep, such as the file's
                                        // permissions; "" when nothing
    char notes[1024];                   // One line per list edit that could not be made as asked; "" when
                                        // none, and always "" after a save that failed (nothing was made)
} ConfigSaveResult;

bool config_save(const char *loaded, const char *system_prefix, const char *user_config,
                 const ConfigEdit *edits, int count, ConfigSaveResult *result);
bool config_save_all(const char *loaded, const char *system_prefix, const char *user_config,
                     const ConfigEdit *edits, int count, const ConfigListEdit *lists, int list_count,
                     ConfigSaveResult *result);

#endif
