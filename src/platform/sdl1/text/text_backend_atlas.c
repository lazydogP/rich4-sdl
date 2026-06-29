#include "platform/sdl1/text/text_backend_atlas.h"

#include <stddef.h>

#include "platform/sdl1/text_rules.h"

/* R10.2d: render Big5 text from a prebaked A8 glyph atlas onto an RGB565
 * framebuffer. No font engine at runtime. Layout/anchor decisions reuse
 * text_rules so the atlas backend stays visually consistent with the desktop
 * FreeType backend. */

/* Decode the next Big5 key from `text`; advances `*pos`. Returns 0 at end. */
static uint16_t atlas_next_code(const unsigned char *text, size_t *pos)
{
    unsigned char lead = text[*pos];
    if (lead == 0) {
        return 0;
    }
    if (lead < 0x80 || text[*pos + 1] == 0) {
        (*pos)++;
        return (uint16_t)lead;
    }
    /* Big5 DBCS: (lead<<8)|trail */
    {
        uint16_t code = (uint16_t)((lead << 8) | text[*pos + 1]);
        *pos += 2;
        return code;
    }
}

static int atlas_measure(Rich4TextBackend *self,
                         const char *big5_text,
                         const Rich4TextStyle *style,
                         int *out_width,
                         int *out_height)
{
    const Rich4FontAtlas *atlas =
        (const Rich4FontAtlas *)self->borrowed_ctx;
    const unsigned char *text = (const unsigned char *)big5_text;
    size_t pos = 0;
    int total = 0;
    int glyph_count = 0;
    uint16_t code;

    if (atlas == NULL) {
        return 0;
    }
    while ((code = atlas_next_code(text, &pos)) != 0) {
        const Rich4FontGlyph *g = rich4_font_atlas_find(atlas, code);
        int advance = g != NULL ? g->advance : (int)(atlas->cell_height / 2);
        if (rich4_text_is_vertical(style->mode)) {
            if (advance > total) {
                /* width tracks widest glyph in vertical mode */
            }
        }
        total += advance;
        glyph_count++;
    }
    if (rich4_text_is_vertical(style->mode)) {
        int advance = rich4_text_vertical_advance((int)atlas->cell_height,
                                                  style->style, style->spacing);
        if (out_width != NULL) *out_width = (int)atlas->cell_height;
        if (out_height != NULL) *out_height = advance * glyph_count;
        return 1;
    }
    if (out_width != NULL) *out_width = total;
    if (out_height != NULL) *out_height = (int)atlas->cell_height;
    return 1;
}

static void atlas_blit_glyph(const Rich4FontAtlas *atlas,
                             const Rich4FontGlyph *g,
                             Rich4TextTarget *target,
                             int pen_x,
                             int pen_y,
                             uint16_t color)
{
    int gx;
    int gy;
    const unsigned char *cov;
    if (g == NULL || g->width == 0 || g->height == 0) {
        return;
    }
    if ((size_t)g->bitmap_offset + (size_t)g->width * g->height > atlas->bitmap_size) {
        return;
    }
    cov = atlas->bitmap + g->bitmap_offset;
    for (gy = 0; gy < g->height; gy++) {
        int ty = pen_y + g->bearing_y + gy;
        uint16_t *row;
        if (ty < 0 || ty >= target->height) continue;
        row = (uint16_t *)((unsigned char *)target->pixels + ty * target->pitch_bytes);
        for (gx = 0; gx < g->width; gx++) {
            int tx = pen_x + g->bearing_x + gx;
            if (tx < 0 || tx >= target->width) continue;
            if (cov[gy * g->width + gx] >= 96) {
                row[tx] = color;
            }
        }
    }
}

static int atlas_render(Rich4TextBackend *self,
                        Rich4TextTarget *target,
                        int x,
                        int y,
                        const char *big5_text,
                        const Rich4TextStyle *style)
{
    const Rich4FontAtlas *atlas =
        (const Rich4FontAtlas *)self->borrowed_ctx;
    const unsigned char *text = (const unsigned char *)big5_text;
    size_t pos = 0;
    int w = 0;
    int h = 0;
    int pen_x;
    int pen_y;
    int vertical;
    int vadvance;
    Rich4TextEffect effect;
    int off;
    uint16_t code;

    if (atlas == NULL) {
        return 0;
    }
    if (!atlas_measure(self, big5_text, style, &w, &h)) {
        return 0;
    }
    rich4_text_apply_anchor(style->mode, w, h, &x, &y);
    vertical = rich4_text_is_vertical(style->mode);
    vadvance = rich4_text_vertical_advance((int)atlas->cell_height,
                                           style->style, style->spacing);
    effect = rich4_text_effect(style->style);
    off = effect == RICH4_TEXT_EFFECT_NONE ? 0 : 1;
    pen_x = x;
    pen_y = y;
    while ((code = atlas_next_code(text, &pos)) != 0) {
        const Rich4FontGlyph *g = rich4_font_atlas_find(atlas, code);
        if (off != 0) {
            atlas_blit_glyph(atlas, g, target, pen_x + off, pen_y + off,
                             style->shadow);
        }
        atlas_blit_glyph(atlas, g, target, pen_x, pen_y, style->fg);
        if (vertical) {
            pen_y += vadvance;
        } else {
            pen_x += g != NULL ? g->advance : (int)(atlas->cell_height / 2);
        }
    }
    return 1;
}

static void atlas_shutdown(Rich4TextBackend *self)
{
    if (self != NULL) {
        self->borrowed_ctx = NULL; /* caller frees the atlas */
    }
}

int rich4_text_backend_atlas_init(Rich4TextBackend *backend,
                                  const Rich4FontAtlas *atlas)
{
    if (backend == NULL || atlas == NULL) {
        return 0;
    }
    backend->measure = atlas_measure;
    backend->render = atlas_render;
    backend->shutdown = atlas_shutdown;
    backend->ctx = NULL;
    backend->borrowed_ctx = atlas;
    return 1;
}
