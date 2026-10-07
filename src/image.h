// Dynamic SVG generation for highlight
#define HIGHLIGHT_OUTLINE_FORMAT " stroke-width=\"%i\" stroke=\"#%02X%02X%02X\" stroke-opacity=\"%.2f\""
#define HIGHLIGHT_FORMAT "<svg viewBox=\"0 0 %i %i\"><rect x=\"0\" width=\"%i\" height=\"%i\" rx=\"%i\" fill=\"#%02X%02X%02X\" fill-opacity=\"%.2f\"%s/></svg>"
#define SCROLL_INDICATOR_FORMAT "<svg width=\"195\" height=\"300\" viewBox=\"0 0 195 300\" version=\"1.1\" id=\"SVGRoot\" > <defs id=\"defs889\"/> <g id=\"layer1\" transform=\"translate(-105)\"> <path style=\"fill:#%02X%02X%02X;fill-opacity:%.2f;stroke:#%02X%02X%02X;stroke-width:%i;stroke-linecap:butt;stroke-linejoin:miter;stroke-miterlimit:4;stroke-dasharray:none;stroke-opacity:%.2f\" d=\"M 280,150 150,280 125,255 C 170,210 230.69212,149.36112 230,150 L 125,45 150,20 Z\" id=\"path3884\"/> </g></svg>"

// Macro functions
#define format_highlight_outline(buffer, outline_size, outline_color, outline_opacity) sprintf_alloc(buffer, HIGHLIGHT_OUTLINE_FORMAT, outline_size, outline_color.r, outline_color.g, outline_color.b, outline_opacity)
#define format_highlight(buffer, width, height, corner_radius, fill_color, fill_opacity, outline_buffer) sprintf_alloc(buffer, HIGHLIGHT_FORMAT, width, height, width, height, corner_radius, fill_color.r, fill_color.g, fill_color.b, fill_opacity, outline_buffer)
#define format_scroll_indicator(buffer, fill_color, outline_size, outline_color, opacity) sprintf_alloc(buffer, SCROLL_INDICATOR_FORMAT, fill_color.r, fill_color.g, fill_color.b, opacity, outline_color.r, outline_color.g, outline_color.b, outline_size, opacity)

typedef struct {
    TTF_Font *font;
    int font_size;
    char *font_path;       // The file the font was opened from (owned): the configured one, or the bundled fallback
    int font_face;         // The face within it
    SDL_Color *color;
    bool shadow;
    SDL_Color *shadow_color;
    int max_width;
    int min_size; // Shrink mode stops here: the readable minimum
    ModeOversize oversize_mode;
} TextInfo;

int init_svg(void);
char *find_default_font(const char *font);
TTF_Font *open_font_file(const char *path, int size, int face);   // TTF_OpenFontIndex, refusing what is not a regular file
int load_font(TextInfo *info, const char *configured, int face, const char *default_font);
void quit_svg(void);
int render_scroll_indicators(Scroll *scroll, int height, Geometry *geo);
SDL_Surface *load_next_slideshow_background(Slideshow *slideshow, bool transition);
int load_next_slideshow_background_async(void *data);
SDL_Texture *load_texture(SDL_Surface *surface);
SDL_Texture *load_texture_from_file(const char *path);
SDL_Texture *load_texture_measured(const char *path, double *luminance);
double surface_luminance(SDL_Surface *surface);   // -1 when it cannot be read
SDL_Texture *rasterize_svg(char *buffer, int w, int h, SDL_Rect *rect);
SDL_Texture *rasterize_svg_from_file(const char *path, int w, int h, SDL_Rect *rect);
SDL_Texture *load_icon(const char *path, int size);
SDL_Texture *render_highlight(int width, int height, SDL_Rect *rect);
SDL_Surface *render_text(const char *text, TextInfo *info, SDL_Rect *rect, int *text_height);
SDL_Texture *render_text_texture(const char *text, TextInfo *info, SDL_Rect *rect, int *text_height);
TTF_Font *title_font(int size);
void title_fonts_free(void);
void title_fonts_keep(const int *sizes, int count);
