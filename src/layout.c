#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "layout.h"

// A function to pick each setting from the menu, else from [Layout], else the built-in default
LayoutOverrides layout_resolve(LayoutOverrides menu, LayoutOverrides global, LayoutOverrides builtin)
{
    LayoutOverrides result;
    result.rows = menu.rows ? menu.rows : (global.rows ? global.rows : builtin.rows);
    result.columns = menu.columns ? menu.columns : (global.columns ? global.columns : builtin.columns);
    result.icon_cap = menu.icon_cap ? menu.icon_cap : (global.icon_cap ? global.icon_cap : builtin.icon_cap);
    return result;
}

// A function to read a positive whole number, rejecting anything else ("3x", "-1", "0", "")
bool layout_parse_count(const char *value, int *count)
{
    if (value == NULL || value[0] == '\0' || strlen(value) > 6)
        return false;
    for (const char *p = value; *p != '\0'; p++) {
        if (!isdigit((unsigned char) *p))
            return false;
    }
    int number = atoi(value);
    if (number <= 0)
        return false;
    *count = number;
    return true;
}

// A function to read an IconSize: a whole number of px from LAYOUT_MIN_BUTTON to LAYOUT_MAX_BUTTON
bool layout_parse_icon_size(const char *value, int *size)
{
    int number;
    if (!layout_parse_count(value, &number) || number < LAYOUT_MIN_BUTTON || number > LAYOUT_MAX_BUTTON)
        return false;
    *size = number;
    return true;
}

// A function to read "N" or "N%": a whole number from min_number to max_number, or a percentage
// from min_percent to max_percent
static bool parse_number_or_percent(const char *value, int min_number, int max_number,
                                    int min_percent, int max_percent, int *number, bool *percent)
{
    if (value == NULL)
        return false;
    size_t length = strlen(value);
    bool is_percent = length > 1 && value[length - 1] == '%';
    size_t digits = is_percent ? length - 1 : length;
    if (digits == 0 || digits > 4)
        return false;
    int n = 0;
    for (size_t i = 0; i < digits; i++) {
        if (!isdigit((unsigned char) value[i]))
            return false;
        n = n * 10 + (value[i] - '0');
    }
    if (is_percent ? (n < min_percent || n > max_percent) : (n < min_number || n > max_number))
        return false;
    *number = n;
    *percent = is_percent;
    return true;
}

// A function to read a FontSize: a percentage of the button ("14%") or a fixed point size ("36")
bool layout_parse_title_size(const char *value, int *size, bool *percent)
{
    return parse_number_or_percent(value, 1, LAYOUT_MAX_TITLE_POINTS, 1, LAYOUT_MAX_TITLE_PERCENT, size, percent);
}

// A function to read a title Padding: a percentage of the button ("8%") or a number of px ("20")
bool layout_parse_title_padding(const char *value, int *padding, bool *percent)
{
    return parse_number_or_percent(value, 0, LAYOUT_MAX_BUTTON, 0, LAYOUT_MAX_PADDING_PERCENT, padding, percent);
}

// A function to return the smaller of two ints
static int min_int(int a, int b)
{
    return a < b ? a : b;
}

// A function to return the larger of two ints
static int max_int(int a, int b)
{
    return a > b ? a : b;
}

// A function to take a whole percentage of a size, rounded to the nearest px
static int percent_of(int size, int percent)
{
    return (size * percent + 50) / 100;
}

// A function to size a menu's titles for its button: a percentage FontSize follows the button but
// never drops below the readable minimum; a fixed one is 0 here (its height is title_block)
int layout_title_size(const LayoutParams *params, int button)
{
    if (params->title_size_pct <= 0)
        return 0;
    return max_int(percent_of(button, params->title_size_pct), params->title_min_size);
}

// A function to find the space between a button and its title: a percentage of the button, or
// a fixed number of px capped at half the button
int layout_title_padding(const LayoutParams *params, int button)
{
    if (params->title_padding_pct > 0)
        return percent_of(button, params->title_padding_pct);
    return min_int(max_int(params->title_padding, 0), max_int(button, 0) / 2);
}

// A function to find how far a title's shadow sits below and right of it: 1/40 of the title's line
// height, at least 2 px. render_text() draws it this far off, so the layout keeps the same room.
int layout_shadow_offset(int line_height)
{
    return max_int(line_height / 40, 2);
}

// A function to find how wide a title may be drawn under its button: the button, less the shadow
// that reaches past the title's right edge
int layout_title_width(bool shadow, int button, int line_height)
{
    return shadow ? max_int(button - layout_shadow_offset(line_height), 0) : button;
}

// A function to find everything under a button: its padding, its title's line height, and its
// shadow's offset below that. A percentage size's line is measured per point at a large size; one
// px more covers rounding. A title drawn in a smaller font (Shrink) has a line and a shadow no taller.
int layout_title_block(const LayoutParams *params, int button)
{
    int size = layout_title_size(params, button);
    int line = size > 0 ? (size * params->title_line_pm + 999) / 1000 + 1 : 0;
    line += max_int(params->title_block, 0);
    if (params->title_shadow && line > 0)
        line += layout_shadow_offset(line);
    return line + layout_title_padding(params, button);
}

// A function to find the largest button for which `rows` rows fit the area's height, each with
// its title block under it. The block grows with the button, so this searches instead of dividing.
// It returns -1 when not even a 0 px button fits.
static int fit_height(const LayoutParams *params, const LayoutArea *area, int rows, int spacing, int vpad)
{
    int room = area->h - (rows - 1) * spacing - 2 * vpad;
    if (room < 0 || rows * layout_title_block(params, 0) > room)
        return -1;
    int low = 0;
    int high = room / rows;
    while (low < high) {
        int middle = low + (high - low + 1) / 2;
        if (rows * (middle + layout_title_block(params, middle)) <= room)
            low = middle;
        else
            high = middle - 1;
    }
    return low;
}

// A function to find the largest button for which `count` slots fit in `length` px, with
// `spacing` between slots, `pad` px of highlight at each end and `extra` px under each slot
static int fit(int length, int count, int spacing, int pad, int extra)
{
    return (length - (count - 1) * spacing - 2 * pad) / count - extra;
}

// A function to size and place a menu's buttons for its grid shape
int layout_compute(const LayoutParams *given, const LayoutArea *area, int entry_count,
                   LayoutGeometry *geometry, char *why, size_t why_size)
{
    LayoutGeometry g;

    // Keep the title arithmetic inside int however large the title settings are, as the gap and
    // paddings are below: a fixed line no taller than the area, a fixed padding no larger than the
    // largest button, and the percentages, sizes and line height within their parsers' limits
    LayoutParams limited = *given;
    const LayoutParams *params = &limited;
    limited.title_block = min_int(max_int(given->title_block, 0), max_int(area->h, 0));
    limited.title_padding = min_int(max_int(given->title_padding, 0), LAYOUT_MAX_BUTTON);
    limited.title_padding_pct = min_int(max_int(given->title_padding_pct, 0), LAYOUT_MAX_PADDING_PERCENT);
    limited.title_size_pct = min_int(max_int(given->title_size_pct, 0), LAYOUT_MAX_TITLE_PERCENT);
    limited.title_min_size = min_int(max_int(given->title_min_size, 0), LAYOUT_MAX_TITLE_POINTS);
    limited.title_line_pm = min_int(max_int(given->title_line_pm, 0), LAYOUT_MAX_LINE_PM);
    int cap = params->icon_cap > 0 ? min_int(params->icon_cap, LAYOUT_MAX_BUTTON) : LAYOUT_MAX_BUTTON;
    int width_fit, height_fit;
    if (why_size > 0)
        why[0] = '\0';

    // Keep the arithmetic inside int however large the settings are: no gap or padding is
    // wider than the area, and no more rows or columns are tried than the smallest button fills
    int spacing = min_int(max_int(params->spacing, 0), max_int(area->w, area->h));
    int max_columns = max_int(area->w / LAYOUT_MIN_BUTTON, 1);
    int max_rows = max_int(area->h / LAYOUT_MIN_BUTTON, 1);

    // Shrink only the axis that overflows until the smallest button fits. Highlight padding
    // is capped at half the gap between buttons; rows only have a gap between them in a grid.
    g.rows = min_int(max_int(params->rows, 1), max_rows);
    g.columns = min_int(max_int(params->columns, 1), max_columns);
    g.hpad = min_int(max_int(params->hpad, 0), spacing / 2);
    for (;;) {
        g.vpad = min_int(max_int(params->vpad, 0), max_int(area->h, 0));
        if (g.rows > 1)
            g.vpad = min_int(g.vpad, spacing / 2);
        width_fit = fit(area->w, g.columns, spacing, g.hpad, 0);
        height_fit = fit_height(params, area, g.rows, spacing, g.vpad);
        if (width_fit < LAYOUT_MIN_BUTTON && g.columns > 1)
            g.columns--;
        else if (height_fit < LAYOUT_MIN_BUTTON && g.rows > 1)
            g.rows--;
        else
            break;
    }
    g.button = min_int(cap, min_int(width_fit, height_fit));
    if (g.button < LAYOUT_MIN_BUTTON) {
        if (why_size > 0)
            snprintf(why, why_size, "not even one %i px button fits in %i x %i px",
                LAYOUT_MIN_BUTTON, area->w, area->h);
        return -1;
    }
    g.title_size = layout_title_size(params, g.button);
    g.title_padding = layout_title_padding(params, g.button);
    g.title_block = layout_title_block(params, g.button);
    if ((g.columns != params->columns || g.rows != params->rows) && why_size > 0)
        snprintf(why, why_size, "not enough screen space for %i x %i buttons, reducing to %i x %i",
            params->columns, params->rows, g.columns, g.rows);

    // Center the block horizontally on the configured columns, so columns stay put as it
    // scrolls. A menu that fills less than one row is centered on its own buttons instead.
    int count = max_int(entry_count, 1);
    int used_columns = min_int(count, g.columns);
    int used_rows = min_int(g.rows, (count + g.columns - 1) / g.columns);
    int block_w = used_columns * g.button + (used_columns - 1) * spacing;
    int block_h = used_rows * (g.button + g.title_block) + (used_rows - 1) * spacing;
    g.x_advance = g.button + spacing;
    g.y_advance = g.button + g.title_block + spacing;
    g.x_origin = area->x + (area->w - block_w) / 2;

    // Center the occupied rows on VCenter, then keep the block and its highlight inside the area
    g.y_origin = area->vcenter - block_h / 2;
    g.y_origin = min_int(g.y_origin, area->y + area->h - g.vpad - block_h);
    g.y_origin = max_int(g.y_origin, area->y + g.vpad);

    *geometry = g;
    return 0;
}

// A function to tell a one-row strip from a grid
static bool is_strip(const LayoutGeometry *g)
{
    return g->rows == 1;
}

// A function to count the buttons in a grid row; only the last row can be short
static int row_length(const LayoutGeometry *g, int entry_count, int row)
{
    return min_int(entry_count - row * g->columns, g->columns);
}

// A function to keep the selection on a real entry and scrolled into view, without
// scrolling past the last strip button or the last grid row
LayoutPosition layout_clamp(const LayoutGeometry *geometry, int entry_count, LayoutPosition position)
{
    LayoutPosition p = position;
    int window, unit, last_first;
    if (entry_count <= 0) {
        p.selected = 0;
        p.first = 0;
        return p;
    }
    p.selected = max_int(0, min_int(p.selected, entry_count - 1));
    if (is_strip(geometry)) {
        window = geometry->columns;
        unit = p.selected;
        last_first = max_int(0, entry_count - geometry->columns);
    }
    else {
        window = geometry->rows;
        unit = p.selected / geometry->columns;
        last_first = max_int(0, (entry_count - 1) / geometry->columns - geometry->rows + 1);
    }
    if (unit < p.first)
        p.first = unit;
    if (unit >= p.first + window)
        p.first = unit - window + 1;
    p.first = max_int(0, min_int(p.first, last_first));
    return p;
}

// A function to move the selection one step, following the strip or grid rules
LayoutPosition layout_move(const LayoutGeometry *geometry, int entry_count, LayoutPosition position,
                           LayoutDirection direction, bool wrap)
{
    if (entry_count <= 0)
        return position;
    LayoutPosition p = layout_clamp(geometry, entry_count, position);
    int s = p.selected;

    // A strip moves along its one row and ignores Up and Down
    if (is_strip(geometry)) {
        if (direction == LAYOUT_RIGHT)
            s = s < entry_count - 1 ? s + 1 : (wrap ? 0 : s);
        else if (direction == LAYOUT_LEFT)
            s = s > 0 ? s - 1 : (wrap ? entry_count - 1 : s);
    }

    // A grid stops at row edges (or wraps within the row), and moves between rows keeping
    // the column, landing on the last button of a shorter last row
    else {
        int columns = geometry->columns;
        int row = s / columns;
        int column = s % columns;
        int last_row = (entry_count - 1) / columns;
        int length = row_length(geometry, entry_count, row);
        int target = -1;
        if (direction == LAYOUT_RIGHT) {
            if (column < length - 1)
                s++;
            else if (wrap)
                s = row * columns;
        }
        else if (direction == LAYOUT_LEFT) {
            if (column > 0)
                s--;
            else if (wrap)
                s = row * columns + length - 1;
        }
        else if (direction == LAYOUT_DOWN)
            target = row < last_row ? row + 1 : (wrap ? 0 : -1);
        else if (direction == LAYOUT_UP)
            target = row > 0 ? row - 1 : (wrap ? last_row : -1);
        if (target >= 0)
            s = target * columns + min_int(column, row_length(geometry, entry_count, target) - 1);
    }
    p.selected = s;
    return layout_clamp(geometry, entry_count, p);
}

// A function to find where an entry is drawn; false when it is scrolled out of view
bool layout_slot(const LayoutGeometry *geometry, LayoutPosition position, int index, int *x, int *y)
{
    int column, row;
    if (index < 0)
        return false;
    if (is_strip(geometry)) {
        column = index - position.first;
        row = 0;
        if (column < 0 || column >= geometry->columns)
            return false;
    }
    else {
        column = index % geometry->columns;
        row = index / geometry->columns - position.first;
        if (row < 0 || row >= geometry->rows)
            return false;
    }
    *x = geometry->x_origin + column * geometry->x_advance;
    *y = geometry->y_origin + row * geometry->y_advance;
    return true;
}

// A function to tell whether more buttons lie off screen in a direction, for the scroll indicators
bool layout_can_scroll(const LayoutGeometry *geometry, int entry_count, LayoutPosition position,
                       LayoutDirection direction)
{
    if (is_strip(geometry)) {
        if (direction == LAYOUT_LEFT)
            return position.first > 0;
        if (direction == LAYOUT_RIGHT)
            return position.first + geometry->columns < entry_count;
        return false;
    }
    if (direction == LAYOUT_UP)
        return position.first > 0;
    if (direction == LAYOUT_DOWN)
        return position.first + geometry->rows <= (entry_count - 1) / geometry->columns;
    return false;
}
