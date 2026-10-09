// The one place a setting's value moves between the settings table's form (SettingValue) and the
// launcher's Config, or a menu's grid: the parser stores every key through it, and the settings
// screen reads and stores through it. SDL-side, since Config holds SDL types.
#ifndef CONFIG_FIELDS_H
#define CONFIG_FIELDS_H

#include "settings.h"
#include "derive.h"

void config_apply_defaults(void);
void config_store(SettingId id, Menu *menu, const SettingValue *value);
SettingValue config_read(SettingId id, const Menu *menu);
DeriveInput derive_input(void);

#endif
