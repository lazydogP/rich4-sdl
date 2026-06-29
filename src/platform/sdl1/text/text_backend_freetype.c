#include "platform/sdl1/text/text_backend_freetype.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>
#if defined(HAVE_SDL_TTF)
#include <SDL/SDL_ttf.h>
#endif

#include "platform/sdl1/text/big5_utf8.h"
#include "platform/sdl1/text_rules.h"

/* R10.2b: desktop text backend built on SDL_ttf / FreeType.
 *
 * This is the platform-independent (no Windows GDI) path used on all desktop
 * targets. Glyphs are rendered with the non-antialiased TTF_RenderUTF8_Solid
 * path and composited onto the caller's RGB565 framebuffer, so the backend
 * never touches an OS font API.
 * A per-pixel-height font handle cache bounds memory and re-open cost; the OS
 * text engine is FreeType via SDL_ttf, not GDI.
 *
 * When SDL_ttf is unavailable at build time the backend degrades to a stub
 * whose measure/render return 0 so the caller can pick another backend. */

#if defined(HAVE_SDL_TTF)

#define RICH4_FT_MAX_HEIGHT 128
#define RICH4_FT_TEXT_CAPACITY 8192

typedef struct Rich4FreetypeState {
    TTF_Font *fonts[RICH4_FT_MAX_HEIGHT];
    int initialized;
    int unavailable;
} Rich4FreetypeState;

static Rich4FreetypeState g_freetype_state;
static char g_freetype_font_path[512];

void rich4_text_backend_freetype_set_font_path(const char *font_path)
{
    if (font_path == NULL) {
        g_freetype_font_path[0] = '\0';
        return;
    }
    snprintf(g_freetype_font_path, sizeof(g_freetype_font_path), "%s",
             font_path);
}

static int rich4_ft_file_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp != NULL) {
        fclose(fp);
        return 1;
    }
    return 0;
}

static TTF_Font *rich4_ft_open_font(Rich4FreetypeState *state, int height)
{
    static const char *font_paths[] = {
        "assets/fonts/rich4.ttf",
        "assets/fonts/rich4.ttc",
        "assets/fonts/MingLiU.ttf",
        "assets/fonts/MingLiU.ttc",
        "assets/fonts/PMingLiU.ttf",
        "assets/fonts/PMingLiU.ttc",
        "assets/fonts/SimSun.ttf",
        "assets/fonts/SimSun.ttc",
#ifdef _WIN32
        "C:/Windows/Fonts/mingliu.ttc",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/msjh.ttc",
        "C:/Windows/Fonts/msyh.ttc",
#else
        "/usr/share/fonts/truetype/arphic/uming.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/STHeiti Light.ttc",
#endif
        NULL
    };
    int i;
    TTF_Font *font;

    if (height < 1 || height >= RICH4_FT_MAX_HEIGHT) {
        height = 16;
    }
    if (state->fonts[height] != NULL) {
        return state->fonts[height];
    }
    if (state->unavailable) {
        return NULL;
    }
    if (!state->initialized) {
        if (TTF_Init() != 0) {
            fprintf(stderr, "warning: SDL_ttf init failed: %s\n", TTF_GetError());
            state->unavailable = 1;
            return NULL;
        }
        state->initialized = 1;
    }
    if (g_freetype_font_path[0] != '\0') {
        font = TTF_OpenFont(g_freetype_font_path, height);
        if (font != NULL) {
            state->fonts[height] = font;
            return font;
        }
        fprintf(stderr, "warning: cannot open configured font %s: %s\n",
                g_freetype_font_path, TTF_GetError());
    }
    for (i = 0; font_paths[i] != NULL; i++) {
        if (!rich4_ft_file_exists(font_paths[i])) {
            continue;
        }
        font = TTF_OpenFont(font_paths[i], height);
        if (font != NULL) {
            state->fonts[height] = font;
            return font;
        }
    }
    fprintf(stderr,
            "warning: SDL_ttf font not found; put a CJK font at assets/fonts/rich4.ttf\n");
    state->unavailable = 1;
    return NULL;
}

/* Big5/CP950 -> UTF-8 via the bundled mapping table. SDL_iconv is not used:
 * this MinGW SDL 1.2 is built without HAVE_ICONV and rejects "BIG5". */
static const char *rich4_ft_big5_to_utf8(const char *text)
{
    static char converted_text[RICH4_FT_TEXT_CAPACITY];

    if (text == NULL) {
        return NULL;
    }
    if (!rich4_big5_to_utf8(text, converted_text, sizeof(converted_text))) {
        return NULL;
    }
    return converted_text;
}

static int rich4_ft_utf8_char_size(const unsigned char *text)
{
    if (text[0] < 0x80) return 1;
    if ((text[0] & 0xe0) == 0xc0 && text[1] != 0) return 2;
    if ((text[0] & 0xf0) == 0xe0 && text[1] != 0 && text[2] != 0) return 3;
    if ((text[0] & 0xf8) == 0xf0 &&
        text[1] != 0 && text[2] != 0 && text[3] != 0) return 4;
    return 1;
}

static TTF_Font *rich4_ft_prepare(Rich4TextBackend *self, const Rich4TextStyle *style)
{
    Rich4FreetypeState *state = (Rich4FreetypeState *)self->ctx;
    TTF_Font *font = rich4_ft_open_font(state, style->height);
    if (font == NULL) {
        return NULL;
    }
    /* SDL_ttf's synthetic TTF_STYLE_BOLD expands a glyph by roughly one
     * tenth of the pixel height.  At Rich4's 15--28 px UI sizes that is
     * visibly heavier than the one-pixel weight-700 raster produced by the
     * original 0x44F9D8 path.  Keep FreeType on the requested regular face;
     * rich4_ft_blit_styled_glyph applies the bounded one-pixel bold spread. */
    TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
    return font;
}

static int rich4_ft_bold_spread(const Rich4TextStyle *style)
{
    return style != NULL && rich4_text_is_bold(style->style) ? 1 : 0;
}

static int rich4_ft_copy_line(const char *cursor, char *line,
                              size_t line_capacity,
                              const char **out_next)
{
    const char *newline;
    size_t length;
    if (cursor == NULL || line == NULL || line_capacity == 0 ||
        out_next == NULL) return 0;
    newline = strchr(cursor, '\n');
    length = newline != NULL ? (size_t)(newline - cursor) : strlen(cursor);
    if (length > 0 && cursor[length - 1] == '\r') length--;
    if (length >= line_capacity) return 0;
    memcpy(line, cursor, length);
    line[length] = '\0';
    *out_next = newline != NULL ? newline + 1 : NULL;
    return 1;
}

static int rich4_ft_measure_horizontal(TTF_Font *font, const char *utf8,
                                       const Rich4TextStyle *style,
                                       int *out_width, int *out_height)
{
    const char *cursor = utf8;
    const char *next;
    char line[RICH4_FT_TEXT_CAPACITY];
    int font_height = TTF_FontHeight(font);
    int line_skip = TTF_FontLineSkip(font);
    int line_count = 0;
    int max_width = 0;
    if (font_height <= 0) font_height = style->height;
    if (line_skip <= 0) line_skip = font_height;
    do {
        int line_width = 0;
        int line_height = 0;
        if (!rich4_ft_copy_line(cursor, line, sizeof(line), &next)) return 0;
        if (line[0] != '\0' &&
            TTF_SizeUTF8(font, line, &line_width, &line_height) != 0)
            return 0;
        if (line_width > max_width) max_width = line_width;
        line_count++;
        cursor = next;
    } while (cursor != NULL);
    if (max_width > 0) max_width += rich4_ft_bold_spread(style);
    if (out_width != NULL) *out_width = max_width;
    if (out_height != NULL)
        *out_height = font_height + (line_count - 1) * line_skip;
    return 1;
}

static int rich4_ft_measure(Rich4TextBackend *self,
                            const char *big5_text,
                            const Rich4TextStyle *style,
                            int *out_width,
                            int *out_height)
{
    const char *utf8;
    TTF_Font *font;
    int w = 0;
    int h = 0;

    utf8 = rich4_ft_big5_to_utf8(big5_text);
    if (utf8 == NULL) {
        return 0;
    }
    font = rich4_ft_prepare(self, style);
    if (font == NULL) {
        return 0;
    }
    if (rich4_text_is_vertical(style->mode)) {
        const unsigned char *p = (const unsigned char *)utf8;
        int advance = rich4_text_vertical_advance(style->height, style->style,
                                                  style->spacing);
        while (*p != 0) {
            int cw = 0;
            int ch = 0;
            int size = rich4_ft_utf8_char_size(p);
            char tmp[8];
            int n = size < 7 ? size : 7;
            memcpy(tmp, p, (size_t)n);
            tmp[n] = '\0';
            if (TTF_SizeUTF8(font, tmp, &cw, &ch) == 0 && cw > w) {
                w = cw;
            }
            h += advance;
            p += size;
        }
    } else {
        return rich4_ft_measure_horizontal(font, utf8, style,
                                           out_width, out_height);
    }
    if (w > 0) w += rich4_ft_bold_spread(style);
    if (out_width != NULL) *out_width = w;
    if (out_height != NULL) *out_height = h;
    return 1;
}

/* Composite one non-antialiased, 8-bit Solid glyph mask onto RGB565. */
static void rich4_ft_blit_solid_glyph(Rich4TextTarget *target,
                                      SDL_Surface *glyph,
                                      int dst_x,
                                      int dst_y,
                                      uint16_t color)
{
    int gx;
    int gy;
    if (target == NULL || target->pixels == NULL || glyph == NULL ||
        glyph->format == NULL || glyph->format->BytesPerPixel != 1) {
        return;
    }
    SDL_LockSurface(glyph);
    for (gy = 0; gy < glyph->h; gy++) {
        const Uint8 *src = (const Uint8 *)glyph->pixels + gy * glyph->pitch;
        int ty = dst_y + gy;
        uint16_t *row;
        if (ty < 0 || ty >= target->height) continue;
        row = (uint16_t *)((unsigned char *)target->pixels + ty * target->pitch_bytes);
        for (gx = 0; gx < glyph->w; gx++) {
            int tx = dst_x + gx;
            if (tx < 0 || tx >= target->width) continue;
            /* SDL_ttf Solid surfaces use palette index zero as transparent
             * and nonzero indices as glyph ink. No alpha threshold is used. */
            if (src[gx] != 0) row[tx] = color;
        }
    }
    SDL_UnlockSurface(glyph);
}

static void rich4_ft_blit_styled_glyph(Rich4TextTarget *target,
                                       SDL_Surface *glyph,
                                       int dst_x,
                                       int dst_y,
                                       uint16_t color,
                                       const Rich4TextStyle *style)
{
    rich4_ft_blit_solid_glyph(target, glyph, dst_x, dst_y, color);
    if (rich4_ft_bold_spread(style) != 0)
        rich4_ft_blit_solid_glyph(target, glyph, dst_x + 1, dst_y, color);
}

static int rich4_ft_render(Rich4TextBackend *self,
                           Rich4TextTarget *target,
                           int x,
                           int y,
                           const char *big5_text,
                           const Rich4TextStyle *style)
{
    const char *utf8;
    TTF_Font *font;
    Rich4TextEffect effect;
    SDL_Color white;
    int w = 0;
    int h = 0;
    int off;

    utf8 = rich4_ft_big5_to_utf8(big5_text);
    if (utf8 == NULL) {
        return 0;
    }
    font = rich4_ft_prepare(self, style);
    if (font == NULL) {
        return 0;
    }
    if (!rich4_ft_measure(self, big5_text, style, &w, &h)) {
        return 0;
    }
    rich4_text_apply_anchor(style->mode, w, h, &x, &y);
    effect = rich4_text_effect(style->style);
    off = effect == RICH4_TEXT_EFFECT_OUTLINE ? 1
        : effect == RICH4_TEXT_EFFECT_SHADOW ? 1
        : 0;
    white.r = 255;
    white.g = 255;
    white.b = 255;

    if (rich4_text_is_vertical(style->mode)) {
        const unsigned char *p = (const unsigned char *)utf8;
        int advance = rich4_text_vertical_advance(style->height, style->style,
                                                  style->spacing);
        int pen_y = y;
        while (*p != 0) {
            int size = rich4_ft_utf8_char_size(p);
            char tmp[8];
            int n = size < 7 ? size : 7;
            SDL_Surface *glyph;
            memcpy(tmp, p, (size_t)n);
            tmp[n] = '\0';
            glyph = TTF_RenderUTF8_Solid(font, tmp, white);
            if (glyph != NULL) {
                if (off != 0) {
                    rich4_ft_blit_styled_glyph(
                        target, glyph, x + off, pen_y + off, style->shadow,
                        style);
                }
                rich4_ft_blit_styled_glyph(
                    target, glyph, x, pen_y, style->fg, style);
                SDL_FreeSurface(glyph);
            }
            pen_y += advance;
            p += size;
        }
        return 1;
    }

    {
        const char *cursor = utf8;
        const char *next;
        char line[RICH4_FT_TEXT_CAPACITY];
        int line_skip = TTF_FontLineSkip(font);
        int pen_y = y;
        if (line_skip <= 0) line_skip = TTF_FontHeight(font);
        if (line_skip <= 0) line_skip = style->height;
        do {
            int line_width = 0;
            int line_height = 0;
            int line_x = x;
            SDL_Surface *glyph = NULL;
            if (!rich4_ft_copy_line(cursor, line, sizeof(line), &next))
                return 0;
            if (line[0] != '\0') {
                if (TTF_SizeUTF8(font, line, &line_width, &line_height) != 0)
                    return 0;
                line_width += rich4_ft_bold_spread(style);
                if (rich4_text_centers_lines(style->mode))
                    line_x += (w - line_width) / 2;
                glyph = TTF_RenderUTF8_Solid(font, line, white);
                if (glyph == NULL) return 0;
                if (off != 0)
                    rich4_ft_blit_styled_glyph(
                        target, glyph, line_x + off, pen_y + off,
                        style->shadow, style);
                rich4_ft_blit_styled_glyph(
                    target, glyph, line_x, pen_y, style->fg, style);
                SDL_FreeSurface(glyph);
            }
            pen_y += line_skip;
            cursor = next;
        } while (cursor != NULL);
    }
    return 1;
}

static void rich4_ft_shutdown(Rich4TextBackend *self)
{
    Rich4FreetypeState *state = (Rich4FreetypeState *)self->ctx;
    size_t i;
    if (state == NULL) {
        return;
    }
    for (i = 0; i < RICH4_FT_MAX_HEIGHT; i++) {
        if (state->fonts[i] != NULL) {
            TTF_CloseFont(state->fonts[i]);
            state->fonts[i] = NULL;
        }
    }
    if (state->initialized) {
        TTF_Quit();
        state->initialized = 0;
    }
    state->unavailable = 0;
}

int rich4_text_backend_freetype_init(Rich4TextBackend *backend)
{
    if (backend == NULL) {
        return 0;
    }
    memset(&g_freetype_state, 0, sizeof(g_freetype_state));
    backend->measure = rich4_ft_measure;
    backend->render = rich4_ft_render;
    backend->shutdown = rich4_ft_shutdown;
    backend->ctx = &g_freetype_state;
    backend->borrowed_ctx = NULL;
    return 1;
}

#else /* !HAVE_SDL_TTF */

void rich4_text_backend_freetype_set_font_path(const char *font_path)
{
    (void)font_path;
}

static int rich4_ft_measure_stub(Rich4TextBackend *self,
                                 const char *big5_text,
                                 const Rich4TextStyle *style,
                                 int *out_width,
                                 int *out_height)
{
    (void)self; (void)big5_text; (void)style;
    if (out_width != NULL) *out_width = 0;
    if (out_height != NULL) *out_height = 0;
    return 0;
}

static int rich4_ft_render_stub(Rich4TextBackend *self,
                                Rich4TextTarget *target,
                                int x, int y,
                                const char *big5_text,
                                const Rich4TextStyle *style)
{
    (void)self; (void)target; (void)x; (void)y; (void)big5_text; (void)style;
    return 0;
}

static void rich4_ft_shutdown_stub(Rich4TextBackend *self)
{
    (void)self;
}

int rich4_text_backend_freetype_init(Rich4TextBackend *backend)
{
    if (backend == NULL) {
        return 0;
    }
    backend->measure = rich4_ft_measure_stub;
    backend->render = rich4_ft_render_stub;
    backend->shutdown = rich4_ft_shutdown_stub;
    backend->ctx = NULL;
    backend->borrowed_ctx = NULL;
    return 0; /* backend present but not functional without SDL_ttf */
}

#endif
