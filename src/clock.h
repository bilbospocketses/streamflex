#define MAX_CLOCK_CHARS 20
#define CLOCK_SPACING_FACTOR 0.5F

// Clock
typedef struct {
    SDL_Surface *time_surface;
    SDL_Surface *date_surface;
    SDL_Texture *time_texture;
    SDL_Texture *date_texture;
    SDL_Rect time_rect;
    SDL_Rect date_rect;
    TextInfo text_info;
    SDL_Color color;        // eff's clock colors, copied in init_clock(): text_info points at these,
    SDL_Color shadow_color; // and the clock thread reads them, so nothing else may write them while it runs
    bool show_date;         // The clock's settings, copied in init_clock() for the same reason: the
    Alignment alignment;    // clock thread reads these, never config or eff, which settings change
    bool include_weekday;
    int margin;             // eff's clock margin, in px
    int screen_width;
    SDL_Rect next_time_rect; // Where a render places the time and date: the main thread draws from
    SDL_Rect next_date_rect; // time_rect and date_rect meanwhile, and takes these with the textures
    time_t current_time;
    struct tm *time_info; // Points at local_time, or NULL when the time could not be converted
    struct tm local_time;
    int x_offset_time;
    int x_offset_date;
    int y_offset;
    int y_advance;
    char time_string[MAX_CLOCK_CHARS + 1];
    char date_string[MAX_CLOCK_CHARS + 1];
    TimeFormat time_format;
    DateFormat date_format;
    bool render_time;
    bool render_date;
} Clock;

int init_clock(Clock *clk);
void get_time(Clock *clk);
void render_clock(Clock *clk);
int render_clock_async(void *data);
TimeFormat get_time_format(const char *region);
DateFormat get_date_format(const char *region);
