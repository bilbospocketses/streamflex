// Finding the installed apps for settings' Add > App page (spec "Installed apps"), on a thread of its own,
// once per settings visit: on Windows the shell's AppsFolder (Task 8), on Linux the .desktop files of the
// applications folders of the data folders applist_data_dirs() names. The thread fills an AppList, sorts
// it, and logs nothing; the main thread polls it each frame and takes the list once it is done.
#ifndef APPSCAN_H
#define APPSCAN_H

#include <stdbool.h>
#include "applist.h"

typedef struct AppScan AppScan;

AppScan *appscan_start(void);                 // Its own thread; NULL when the thread could not start
bool appscan_poll(AppScan *scan, int *found); // True once done
AppList *appscan_take(AppScan *scan);         // After done; the caller owns the list
const char *appscan_why(const AppScan *scan); // Why it failed or found nothing, or NULL
void appscan_free(AppScan *scan);             // Joins the thread first

#endif
