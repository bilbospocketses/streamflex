#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_thread.h>
#include "launcher.h"
#include <launcher_config.h>
#include "util.h"
#include "image.h"
#include "clock.h"
#include "debug.h"
#include "platform/platform.h"

static void calculate_text_metrics(TTF_Font *font, Alignment alignment, const char *text, int *h, int *x_offset);
static void calculate_clock_geometry(Clock *clk);
static void format_time(Clock *clk);
static void format_date(Clock *clk);
static void calculate_clock_positioning(Clock *clk, SDL_Rect *time_rect, SDL_Rect *date_rect);

extern Config config;
extern State state;
extern Geometry geo;
extern Effective eff;

// A function to calculate height and x offset of text
static void calculate_text_metrics(TTF_Font *font, Alignment alignment, const char *text, int *h, int *x_offset)
{
    int ymin = 0;
    int ymax = 0; 
    // Zero for an empty text, or a glyph TTF_GlyphMetrics cannot measure (it then sets none of them)
    int xmin = 0, xmax = 0, xadvance = 0;
    int current_ymin = 0, current_ymax = 0;
    char *p = (char*) text;
    Uint16 code_point;
    int bytes;

    // Get text height, x offset
    while (*p != '\0') {
        code_point = get_unicode_code_point(p, &bytes);
        TTF_GlyphMetrics(font, 
            code_point, 
            &xmin, 
            &xmax, 
            &current_ymin, 
            &current_ymax,
            &xadvance
        );
        if (current_ymax > ymax)
            ymax = current_ymax;
        if (current_ymin < ymin)
            ymin = current_ymin;
        if (p == text && alignment == ALIGNMENT_LEFT)
            *x_offset = xmin;

    p += bytes;
    }
    if (alignment == ALIGNMENT_RIGHT)
        *x_offset = xadvance - xmax;
    *h = ymax - ymin;
}

// A function to calculate the spacing and offsets of the clock text
static void calculate_clock_geometry(Clock *clk)
{
    int line_skip = TTF_FontLineSkip(clk->text_info.font);
    int h_time, h_date;

    // Calculate text height and x offset
    calculate_text_metrics(clk->text_info.font,
        clk->alignment,
        clk->time_string,
        &h_time,
        &clk->x_offset_time
    );

    if (clk->show_date) {
        calculate_text_metrics(clk->text_info.font,
            clk->alignment,
            clk->date_string,
            &h_date,
            &clk->x_offset_date
        );

        int spacing = (int) (CLOCK_SPACING_FACTOR * (float) h_time);
        clk->y_advance = spacing + h_time;
    }

    // Calculate y offset from margin
    clk->y_offset = (line_skip - h_time) / 2;
}

// A function to convert a time to local time in the caller's struct, where localtime() uses a shared one
static struct tm *to_local_time(const time_t *seconds, struct tm *result)
{
#ifdef _MSC_VER
    return localtime_s(result, seconds) == 0 ? result : NULL;
#else
    return localtime_r(seconds, result);
#endif
}

// A function to get the current time from the operating system
void get_time(Clock *clk)
{
    int previous_min = 0;
    int previous_day = 0;
    if (clk->time_info != NULL) {
        previous_min = clk->time_info->tm_min;
        previous_day = clk->time_info->tm_mday;
    }

    // Get current time
    time(&clk->current_time);
    clk->time_info = to_local_time(&clk->current_time, &clk->local_time);
    
    // Set render flags if time and/or date changed
    if (clk->time_info == NULL || previous_min != clk->time_info->tm_min) {
        clk->render_time = true;
        if (clk->time_info == NULL || previous_day != clk->time_info->tm_mday)
            clk->render_date = true;
    }
}

// A function to format the current time according to user settings
static void format_time(Clock *clk)
{
    char *format = NULL;
    if (clk->time_format == FORMAT_TIME_24HR)
        format = TIME_STRING_24HR;
    else
        format = TIME_STRING_12HR;
    strftime(clk->time_string, 
        sizeof(clk->time_string), 
        format, 
        clk->time_info
    );
}

// A function to format the current date according to user settings
static void format_date(Clock *clk)
{
    char *format = NULL;
 
    // Get date format
    if (clk->date_format == FORMAT_DATE_LITTLE)
        format = DATE_STRING_LITTLE;
    else
        format = DATE_STRING_BIG;
    char weekday[MAX_CLOCK_CHARS + 1];
    char date[MAX_CLOCK_CHARS + 1];

    // Get weekday name
    if (clk->include_weekday) {
        strftime(weekday, 
            sizeof(weekday), 
            "%a ", 
            clk->time_info
        );
    }
    else
        weekday[0] = '\0';
    copy_string(clk->date_string, 
        weekday, 
        sizeof(clk->date_string)
    );

    // Get date, and add as much of it as fits after the weekday
    strftime(date,
        sizeof(date),
        format,
        clk->time_info
    );
    size_t used = strlen(clk->date_string);
    copy_string(clk->date_string + used,
        date,
        sizeof(clk->date_string) - used
    );
}

// A function to calculate the x and y coordinates of the clock text, into the rects given
static void calculate_clock_positioning(Clock *clk, SDL_Rect *time_rect, SDL_Rect *date_rect)
{
    if (clk->alignment == ALIGNMENT_LEFT) {
        time_rect->x = clk->margin - clk->x_offset_time;
        if (clk->show_date)
            date_rect->x = clk->margin - clk->x_offset_date;
    }
    else {
        time_rect->x = clk->screen_width - clk->margin - time_rect->w + clk->x_offset_time;
        if (clk->show_date)
            date_rect->x = clk->screen_width - clk->margin - date_rect->w + clk->x_offset_date;
    }
    time_rect->y = clk->margin - clk->y_offset;
    if (clk->show_date)
        date_rect->y = time_rect->y + clk->y_advance;
}

// A function to initialize the clock; non-zero when no font opens
int init_clock(Clock *clk)
{
    // Initialize clock structure. Its colors are its own copies of eff's, which the clock thread
    // reads while the main thread may derive eff again
    clk->color = (SDL_Color) { eff.clock_color.r, eff.clock_color.g, eff.clock_color.b, eff.clock_color.a };
    clk->shadow_color = (SDL_Color) { eff.clock_shadow_color.r, eff.clock_shadow_color.g,
                                      eff.clock_shadow_color.b, eff.clock_shadow_color.a };
    clk->text_info = (TextInfo) {
        .font = NULL,
        .font_size = (int) config.clock_font_size,
        .font_path = NULL,
        .color = &clk->color,
        .shadow = config.clock_shadows,
        .shadow_color = config.clock_shadows ? &clk->shadow_color : NULL,
        .oversize_mode = OVERSIZE_NONE
    };
    clk->time_format = config.clock_time_format;
    clk->date_format = config.clock_date_format;
    clk->time_info = NULL;
    clk->show_date = config.clock_show_date;
    clk->alignment = config.clock_alignment;
    clk->include_weekday = config.clock_include_weekday;
    clk->margin = eff.clock_margin;
    clk->screen_width = geo.screen_width;

    // Load the font
    int error = load_font(&clk->text_info, config.clock_font_path, config.clock_font_face, FILENAME_DEFAULT_CLOCK_FONT);
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_RELOAD_FONTS opens the font again, as a
    // reload will, so the leak pass shows whether the one it replaces is closed
    if (!error && getenv("STREAMFLEX_TEST_RELOAD_FONTS") != NULL) {
        log_debug("Test hook: the clock font opens again");
        error = load_font(&clk->text_info, config.clock_font_path, config.clock_font_face, FILENAME_DEFAULT_CLOCK_FONT);
    }
#endif
    if (error)
        return 1;

    // Get time and format it into a string
    get_time(clk);
    if (clk->time_format == FORMAT_TIME_AUTO || clk->date_format == FORMAT_DATE_AUTO) {
        char region[3];
        memset(region, '\0', sizeof(region));
        get_region(region);
        if (clk->time_format == FORMAT_TIME_AUTO)
            clk->time_format = get_time_format(region);
        if (clk->show_date && clk->date_format == FORMAT_DATE_AUTO)
            clk->date_format = get_date_format(region);
    }

    // Render the time and date
    format_time(clk);
    clk->time_texture = render_text_texture(clk->time_string,
                            &clk->text_info,
                            &clk->time_rect,
                            NULL
                        );
    if (clk->show_date) {
        format_date(clk);
        clk->date_texture = render_text_texture(clk->date_string,
                                &clk->text_info,
                                &clk->date_rect,
                                NULL
                            );
    }

    // Calculate geometry
    calculate_clock_geometry(clk);
    calculate_clock_positioning(clk, &clk->time_rect, &clk->date_rect);
    clk->render_time = false;
    clk->render_date = false;
    return 0;
}

// A function to render the time to image, on the clock's thread or the main one. It reads and
// writes only the Clock: the new rects go into next_time_rect and next_date_rect, which the main
// thread takes with the surfaces, since it draws from time_rect and date_rect meanwhile.
void render_clock(Clock *clk)
{
    clk->next_time_rect = clk->time_rect;
    clk->next_date_rect = clk->date_rect;
    format_time(clk);
    clk->time_surface = render_text(clk->time_string,
                            &clk->text_info,
                            &clk->next_time_rect,
                            NULL
                        );
    if (clk->render_date) {
        format_date(clk);
        clk->date_surface = render_text(clk->date_string,
                                &clk->text_info,
                                &clk->next_date_rect,
                                NULL
                            );
        calculate_clock_geometry(clk);
    }
    calculate_clock_positioning(clk, &clk->next_time_rect, &clk->next_date_rect);
    SDL_AtomicSet(&state.clock_ready, 1);
}

// A functio nto render the time to image in a separate thread
int render_clock_async(void *data)
{
    Clock *clk = (Clock*) data;
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: each render waits, so a key can land during one
    const char *delay = getenv("STREAMFLEX_TEST_CLOCK_DELAY_MS");
    if (delay != NULL)
        SDL_Delay((Uint32) atoi(delay));
#endif
    // Safe: this thread never opens or closes a font, and FreeType renders separate faces in parallel
    render_clock(clk);
    return 0;
} 

// A function to get the time format for a region
TimeFormat get_time_format(const char *region)
{
    const char *countries[] = {
        "US",
        "CA",
        "GB",
        "AU",
        "NZ",
        "IN"
    };
    TimeFormat format = FORMAT_TIME_24HR;
    for (size_t i = 0; i < sizeof(countries) / sizeof(countries[0]); i++) {
        if (!strcmp(region, countries[i])) {
            format = FORMAT_TIME_12HR;
            break;
        }
    }
    return format;
}

// A function to get the date format for a region
DateFormat get_date_format(const char *region)
{
    const char *countries[] = {
        "US",
        "JP",
        "CN"
    };
    DateFormat format = FORMAT_DATE_LITTLE;
    for (size_t i = 0; i < sizeof(countries) / sizeof(countries[0]); i++) {
        if (!strcmp(region, countries[i])) {
            format = FORMAT_DATE_BIG;
            break;
        }
    }
    return format;
}
