// Grid geometry and navigation for menus. Pure: plain integers, no SDL, no globals, so
// tests/test_layout.c can build it on its own. launcher.c does the drawing.
#ifndef LAYOUT_H
#define LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#define LAYOUT_MIN_BUTTON 32          // Smallest button, and smallest IconSize
#define LAYOUT_MAX_BUTTON 1024        // Largest button, and largest IconSize
#define LAYOUT_MAX_TITLE_POINTS 512   // Largest fixed FontSize
#define LAYOUT_MAX_TITLE_PERCENT 100  // Largest FontSize percentage
#define LAYOUT_MAX_PADDING_PERCENT 50 // Largest title Padding percentage
#define LAYOUT_MAX_LINE_PM 10000      // Largest title line height per point, in thousandths

typedef enum {
    LAYOUT_UP,
    LAYOUT_DOWN,
    LAYOUT_LEFT,
    LAYOUT_RIGHT
} LayoutDirection;

// Rows, Columns and IconSize from a menu section or from [Layout]; 0 means "not set"
typedef struct {
    int rows;
    int columns;
    int icon_cap;
} LayoutOverrides;

// Everything layout_compute needs to size a menu's buttons
typedef struct {
    int rows;
    int columns;
    int icon_cap;          // Largest allowed button size; 0 = no cap
    int spacing;           // Gap between buttons in px, across and down
    int title_block;       // A fixed FontSize's line height below each icon; 0 for a percentage or without titles
    int hpad;              // Requested highlight padding; negative counts as 0
    int vpad;
    int title_padding;     // A fixed title Padding in px, capped at half the button; 0 without titles
    int title_padding_pct; // Padding as a percentage of the button; 0 = fixed (title_padding)
    int title_size_pct;    // FontSize as a percentage of the button; 0 = fixed (its line height is title_block)
    int title_min_size;    // The readable minimum point size for a percentage FontSize
    int title_line_pm;     // The title font's line height per point, in thousandths, for a percentage FontSize
    bool title_shadow;     // Titles have a shadow, which reaches layout_shadow_offset() px below and right of them
} LayoutParams;

// The part of the screen the buttons may use
typedef struct {
    int x;
    int y;
    int w;
    int h;
    int vcenter;     // Vertical center of the button block, in px from the top of the screen
} LayoutArea;

// The computed layout of one menu
typedef struct {
    int rows;          // After any reduction to fit the screen
    int columns;
    int button;        // Square button size in px
    int x_advance;     // Distance between neighboring buttons' x
    int y_advance;     // Distance between neighboring rows' y
    int x_origin;      // Top-left of the first visible slot
    int y_origin;
    int hpad;          // Highlight padding after capping
    int vpad;
    int title_size;    // Title point size for a percentage FontSize; 0 = the fixed FontSize
    int title_padding; // Space between the button and its title, in px
    int title_block;   // Padding plus the title's line height: all that sits under the button
} LayoutGeometry;

// Where the highlight is, and what is scrolled into view
typedef struct {
    int selected;    // Entry index
    int first;       // Strip: first visible entry. Grid: first visible row
} LayoutPosition;

LayoutOverrides layout_resolve(LayoutOverrides menu, LayoutOverrides global, LayoutOverrides builtin);
bool layout_parse_count(const char *value, int *count);
bool layout_parse_icon_size(const char *value, int *size);
bool layout_parse_title_size(const char *value, int *size, bool *percent);
bool layout_parse_title_padding(const char *value, int *padding, bool *percent);
int layout_title_size(const LayoutParams *params, int button);
int layout_title_padding(const LayoutParams *params, int button);
int layout_title_block(const LayoutParams *params, int button);
int layout_shadow_offset(int line_height);
int layout_title_width(bool shadow, int button, int line_height);
int layout_compute(const LayoutParams *params, const LayoutArea *area, int entry_count,
                   LayoutGeometry *geometry, char *why, size_t why_size);
LayoutPosition layout_move(const LayoutGeometry *geometry, int entry_count, LayoutPosition position,
                           LayoutDirection direction, bool wrap);
LayoutPosition layout_clamp(const LayoutGeometry *geometry, int entry_count, LayoutPosition position);
bool layout_slot(const LayoutGeometry *geometry, LayoutPosition position, int index, int *x, int *y);
bool layout_can_scroll(const LayoutGeometry *geometry, int entry_count, LayoutPosition position,
                       LayoutDirection direction);

#endif
