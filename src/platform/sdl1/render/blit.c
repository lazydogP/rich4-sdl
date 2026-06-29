#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/pixel_rows.h"

#include <string.h>
#include "view/view_constants.h"

/* R10.1a: platform surface primitives moved verbatim from
 * platform/sdl1/runtime_main.c to shrink the runtime monolith and keep the
 * per-feature retrieval surface small. Behavior is unchanged. */

void fill_background(SDL_Surface *screen)
{
    int x;
    int y;
    SDL_LockSurface(screen);
    for (y = 0; y < SCREEN_H; y++) {
        uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels + y * screen->pitch);
        for (x = 0; x < SCREEN_W; x++) {
            uint16_t r = (uint16_t)((x * 31) / SCREEN_W);
            uint16_t g = (uint16_t)((y * 63) / SCREEN_H);
            uint16_t b = (uint16_t)(((x + y) * 31) / (SCREEN_W + SCREEN_H));
            row[x] = (uint16_t)((r << 11) | (g << 5) | b);
        }
    }
    SDL_UnlockSurface(screen);
}

void fill_rect(SDL_Surface *screen, int x, int y, int w, int h, uint16_t color)
{
    int px;
    int py;

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > SCREEN_W) {
        w = SCREEN_W - x;
    }
    if (y + h > SCREEN_H) {
        h = SCREEN_H - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    for (py = y; py < y + h; py++) {
        uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels + py * screen->pitch);
        for (px = x; px < x + w; px++) {
            row[px] = color;
        }
    }
}

uint16_t map_resource_pixel(SDL_Surface *screen, uint16_t pixel)
{
    /* Original sub_451801, n2_6 == 1: RGB555 -> RGB565 is a direct bit
     * transfer. The source green low bit is not replicated into bit 5 of the
     * destination; bit 15 is ignored. */
    if (screen->format->Rmask == 0xf800 && screen->format->Gmask == 0x07e0 &&
        screen->format->Bmask == 0x001f && screen->format->Amask == 0) {
        return (uint16_t)(((pixel & 0x7c00u) << 1) |
            ((pixel & 0x03e0u) << 1) | (pixel & 0x001fu));
    }
    Uint8 r = (Uint8)(((pixel >> 10) & 0x1f) * 255 / 31);
    Uint8 g = (Uint8)(((pixel >> 5) & 0x1f) * 255 / 31);
    Uint8 b = (Uint8)((pixel & 0x1f) * 255 / 31);
    return (uint16_t)SDL_MapRGB(screen->format, r, g, b);
}

uint16_t map_resource_pixel_adjusted(SDL_Surface *screen, uint16_t pixel, int adjust)
{
    int r = (int)((pixel >> 10) & 0x1f);
    int g = (int)((pixel >> 5) & 0x1f);
    int b = (int)(pixel & 0x1f);

    if (adjust < -32) adjust = -32;
    if (adjust > 32) adjust = 32;
    if (adjust < 0) {
        int scale = 32 + adjust;
        r = (r * scale) / 32;
        g = (g * scale) / 32;
        b = (b * scale) / 32;
    } else if (adjust > 0) {
        r += ((31 - r) * adjust + 31) / 32;
        g += ((31 - g) * adjust + 31) / 32;
        b += ((31 - b) * adjust + 31) / 32;
    }
    return map_resource_pixel(screen, (uint16_t)((r << 10) | (g << 5) | b));
}

uint16_t map_surface_pixel_property_flash(SDL_Surface *screen, uint16_t pixel, int adjust)
{
    Uint8 r, g, b;
    int channels[3], i;
    SDL_GetRGB(pixel, screen->format, &r, &g, &b);
    channels[0] = r >> 3;
    channels[1] = g >> 3;
    channels[2] = b >> 3;
    /* Original 0x485D68 LUT: round(c*(31+a)/31) for a<0,
     * round(a+c*(31-a)/31) for a>=0. */
    if (adjust < -31) adjust = -31;
    if (adjust > 31) adjust = 31;
    for (i = 0; i < 3; i++)
        channels[i] = adjust < 0 ? (channels[i] * (31 + adjust) + 15) / 31
            : adjust + (channels[i] * (31 - adjust) + 15) / 31;
    return map_resource_pixel(screen, (uint16_t)(
        (channels[0] << 10) | (channels[1] << 5) | channels[2]));
}

uint16_t map_resource_pixel_jump_darken_minus20(SDL_Surface *screen, uint16_t pixel)
{
    static const unsigned char table[32] = {
        0, 0, 1, 1, 1, 2, 2, 2,
        3, 3, 4, 4, 4, 5, 5, 5,
        6, 6, 6, 7, 7, 7, 8, 8,
        9, 9, 9, 10, 10, 10, 11, 11
    };
    int r = table[(pixel >> 10) & 0x1f];
    int g = table[(pixel >> 5) & 0x1f];
    int b = table[pixel & 0x1f];

    return map_resource_pixel(screen, (uint16_t)((r << 10) | (g << 5) | b));
}

/* One bounded RGB565 chunk kernel. mode 0 is the ordinary resource mapping;
 * mode 1 preserves the intro jump darkening transform. */
static int draw_rgb565_chunk(SDL_Surface *screen,
                             const unsigned char *data,
                             const Rich4SprSmpChunk *chunk,
                             int source_x,
                             int source_y,
                             int width,
                             int height,
                             int dst_x,
                             int dst_y,
                             int transparent_zero,
                             int mode)
{
    int x;
    int y;
    const uint16_t *src;
    int left, top, right, bottom, native565;

    if (screen == NULL || data == NULL || chunk == NULL ||
        source_x < 0 || source_y < 0 || width <= 0 || height <= 0 ||
        source_x + width > chunk->width || source_y + height > chunk->height)
        return 0;
    src = (const uint16_t *)(const void *)(data + chunk->data_offset);
    left = dst_x < 0 ? -dst_x : 0;
    top = dst_y < 0 ? -dst_y : 0;
    right = width < SCREEN_W - dst_x ? width : SCREEN_W - dst_x;
    bottom = height < SCREEN_H - dst_y ? height : SCREEN_H - dst_y;
    if (right > screen->w - dst_x) right = screen->w - dst_x;
    if (bottom > screen->h - dst_y) bottom = screen->h - dst_y;
    if (left >= right || top >= bottom) return 1;
    native565 = !mode && screen->format->Rmask == 0xf800 &&
        screen->format->Gmask == 0x07e0 && screen->format->Bmask == 0x001f &&
        screen->format->Amask == 0;
    SDL_LockSurface(screen);
    for (y = top; y < bottom; y++) {
        uint16_t *row;
        const uint16_t *source_row = src + (source_y + y) * chunk->width + source_x;
        row = (uint16_t *)((unsigned char *)screen->pixels +
                           (dst_y + y) * screen->pitch);
        if (native565) {
            rich4_rgb555_row(row + dst_x + left, source_row + left,
                             right - left, transparent_zero);
            continue;
        }
        for (x = left; x < right; x++) {
            uint16_t pixel;
            pixel = source_row[x];
            if (!transparent_zero || pixel != 0) {
                row[dst_x + x] = mode
                    ? map_resource_pixel_jump_darken_minus20(screen, pixel)
                    : map_resource_pixel(screen, pixel);
            }
        }
    }
    SDL_UnlockSurface(screen);
    return 1;
}

int draw_chunk_at(SDL_Surface *screen,
                  const unsigned char *data,
                  const Rich4SprSmpChunk *chunk,
                  int center_x,
                  int center_y,
                  int transparent_zero)
{
    if (chunk == NULL || chunk->width <= 0 || chunk->height <= 0 ||
        chunk->data_size < (uint32_t)(chunk->width * chunk->height * 2)) return 0;
    return draw_rgb565_chunk(screen, data, chunk, 0, 0, chunk->width,
                             chunk->height, center_x - chunk->x,
                             center_y - chunk->y, transparent_zero, 0);
}

int draw_chunk_at_jump_darken_minus20(SDL_Surface *screen,
                                      const unsigned char *data,
                                      const Rich4SprSmpChunk *chunk,
                                      int center_x,
                                      int center_y,
                                      int transparent_zero)
{
    if (chunk == NULL || chunk->width <= 0 || chunk->height <= 0 ||
        chunk->data_size < (uint32_t)(chunk->width * chunk->height * 2)) return 0;
    return draw_rgb565_chunk(screen, data, chunk, 0, 0, chunk->width,
                             chunk->height, center_x - chunk->x,
                             center_y - chunk->y, transparent_zero, 1);
}

int draw_spr_chunk_at(SDL_Surface *screen,
                      const unsigned char *data,
                      size_t size,
                      const Rich4SprSmpInfo *spr,
                      const Rich4SprSmpChunk *chunk,
                      int center_x,
                      int center_y,
                      int transparent_zero)
{
    int x;
    int y;
    int dst_x;
    int dst_y;
    const unsigned char *src;
    const uint16_t *palette;
    uint16_t mapped_palette[256];
    int color;

    if (data == NULL || spr == NULL || chunk == NULL ||
        memcmp(spr->signature, "SPR", 4) != 0 ||
        spr->start_offset > size ||
        size - spr->start_offset < 512 ||
        chunk->width <= 0 || chunk->height <= 0 ||
        chunk->data_size < (uint32_t)(chunk->width * chunk->height)) {
        return 0;
    }

    dst_x = center_x - chunk->x;
    dst_y = center_y - chunk->y;
    src = data + chunk->data_offset;
    palette = (const uint16_t *)(const void *)(data + spr->start_offset);
    for (color = 0; color < 256; color++)
        mapped_palette[color] = map_resource_pixel(screen, palette[color]);
    SDL_LockSurface(screen);
    for (y = 0; y < chunk->height && dst_y + y < SCREEN_H; y++) {
        uint16_t *row;
        if (dst_y + y < 0) {
            continue;
        }
        row = (uint16_t *)((unsigned char *)screen->pixels + (dst_y + y) * screen->pitch);
        for (x = 0; x < chunk->width && dst_x + x < SCREEN_W; x++) {
            if (dst_x + x >= 0) {
                unsigned char index = src[y * chunk->width + x];
                if (!transparent_zero || index != 0) {
                    row[dst_x + x] = mapped_palette[index];
                }
            }
        }
    }
    SDL_UnlockSurface(screen);
    return 1;
}

int draw_spr_chunk_scaled(SDL_Surface *screen, const unsigned char *data,
    size_t size, const Rich4SprSmpInfo *spr, const Rich4SprSmpChunk *chunk,
    int x, int y, unsigned int scale)
{
    uint16_t palette[256];
    unsigned int inverse;
    int w, h, left, top, right, bottom, dx, dy, i;
    if (!screen || !data || !spr || !chunk || !scale ||
        memcmp(spr->signature, "SPR", 4) || chunk->width <= 0 || chunk->height <= 0 ||
        spr->start_offset > size || size - spr->start_offset < 512 ||
        chunk->data_offset > size || chunk->data_size > size - chunk->data_offset ||
        (size_t)chunk->width * chunk->height > chunk->data_size) return 0;
    w = (int)(((uint64_t)scale * chunk->width) >> 16);
    h = (int)(((uint64_t)scale * chunk->height) >> 16);
    x -= chunk->x;
    y -= chunk->y;
    left = x < 0 ? -x : 0;
    top = y < 0 ? -y : 0;
    right = w < screen->w - x ? w : screen->w - x;
    bottom = h < screen->h - y ? h : screen->h - y;
    if (left >= right || top >= bottom) return 0;
    inverse = (unsigned int)(UINT64_C(0x100000000) / scale);
    for (i = 0; i < 256; i++) {
        uint16_t value;
        memcpy(&value, data + spr->start_offset + i * 2, 2);
        palette[i] = map_resource_pixel(screen, value);
    }
    SDL_LockSurface(screen);
    for (dy = top; dy < bottom; dy++) {
        uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels + (y + dy) * screen->pitch);
        unsigned int sy = ((unsigned int)top * 65536u + (dy - top) * inverse) >> 16;
        if (sy >= (unsigned int)chunk->height) continue;
        for (dx = left; dx < right; dx++) {
            unsigned int sx = ((uint64_t)left * inverse >> 16) +
                ((uint64_t)(dx - left) * inverse >> 16);
            unsigned char index;
            if (sx >= (unsigned int)chunk->width) continue;
            index = data[chunk->data_offset + sy * chunk->width + sx];
            if (index) row[x + dx] = palette[index];
        }
    }
    SDL_UnlockSurface(screen);
    return 1;
}

int draw_spr_chunk_crop_topleft(SDL_Surface *screen,
                                const unsigned char *data,
                                size_t size,
                                const Rich4SprSmpInfo *spr,
                                const Rich4SprSmpChunk *chunk,
                                int src_x,
                                int src_y,
                                int width,
                                int height,
                                int dst_x,
                                int dst_y,
                                int transparent_zero)
{
    int x;
    int y;
    const unsigned char *src;
    const uint16_t *palette;
    uint16_t mapped_palette[256];
    int color;

    if (data == NULL || spr == NULL || chunk == NULL ||
        memcmp(spr->signature, "SPR", 4) != 0 ||
        spr->start_offset > size || size - spr->start_offset < 512 ||
        chunk->width <= 0 || chunk->height <= 0 ||
        chunk->data_size < (uint32_t)(chunk->width * chunk->height) ||
        src_x < 0 || src_y < 0 || width <= 0 || height <= 0 ||
        src_x >= chunk->width || src_y >= chunk->height)
        return 0;
    if (src_x + width > chunk->width) width = chunk->width - src_x;
    if (src_y + height > chunk->height) height = chunk->height - src_y;
    if (width <= 0 || height <= 0) return 0;
    src = data + chunk->data_offset;
    palette = (const uint16_t *)(const void *)(data + spr->start_offset);
    for (color = 0; color < 256; color++)
        mapped_palette[color] = map_resource_pixel(screen, palette[color]);
    SDL_LockSurface(screen);
    for (y = 0; y < height && dst_y + y < SCREEN_H; y++) {
        uint16_t *row;
        if (dst_y + y < 0) continue;
        row = (uint16_t *)((unsigned char *)screen->pixels +
                           (dst_y + y) * screen->pitch);
        for (x = 0; x < width && dst_x + x < SCREEN_W; x++) {
            if (dst_x + x >= 0) {
                unsigned char index = src[(src_y + y) * chunk->width +
                                          src_x + x];
                if (!transparent_zero || index != 0)
                    row[dst_x + x] = mapped_palette[index];
            }
        }
    }
    SDL_UnlockSurface(screen);
    return 1;
}

static uint16_t game_map_parity_pixel_mask(const SDL_Surface *screen,
                                           int alternate)
{
    if (screen == NULL || screen->format == NULL) return 0;
    /* word_488EF0[4 * alternate + pixel_format] used by 0x456C33.
     * Rich4's RGB565 entry intentionally uses 0x003f for the alternate
     * mask, rather than a normalized SDL blue value. */
    if (screen->format->Rmask == 0x001fu)
        return alternate ? 0xf800u : 0x003fu;
    if (screen->format->Rmask == 0x0f00u)
        return alternate ? 0x000fu : 0x0f00u;
    if (screen->format->Rmask == 0x7c00u)
        return alternate ? 0x003fu : 0x7c00u;
    return alternate ? 0x003fu : 0xf800u;
}

int draw_game_map_parity_mask(SDL_Surface *screen,
                              const unsigned char *data,
                              const Rich4SprSmpChunk *chunk,
                              int center_x,
                              int center_y,
                              int alternate)
{
    int x;
    int y;
    int dst_x;
    int dst_y;
    int byte_mask;
    uint16_t pixel_mask;

    if (screen == NULL || data == NULL || chunk == NULL ||
        chunk->width <= 0 || chunk->height <= 0) return 0;
    byte_mask = chunk->data_size >= (uint32_t)(chunk->width * chunk->height) &&
                chunk->data_size < (uint32_t)(chunk->width * chunk->height * 2);
    if (!byte_mask &&
        chunk->data_size < (uint32_t)(chunk->width * chunk->height * 2)) return 0;
    dst_x = center_x - chunk->x;
    dst_y = center_y - chunk->y;
    pixel_mask = game_map_parity_pixel_mask(screen, alternate & 1);
    SDL_LockSurface(screen);
    for (y = 0; y < chunk->height && dst_y + y < SCREEN_H; y++) {
        uint16_t *row;
        if (dst_y + y < 0) continue;
        row = (uint16_t *)((unsigned char *)screen->pixels +
                          (dst_y + y) * screen->pitch);
        for (x = 0; x < chunk->width && dst_x + x < SCREEN_W; x++) {
            int set;
            if (dst_x + x < 0) continue;
            if (byte_mask)
                set = data[chunk->data_offset + y * chunk->width + x] != 0;
            else
                set = ((const uint16_t *)(const void *)(data + chunk->data_offset))
                          [y * chunk->width + x] != 0;
            if (set) row[dst_x + x] |= pixel_mask;
        }
    }
    SDL_UnlockSurface(screen);
    return 1;
}

int draw_spr_chunk_at_owner(SDL_Surface *screen,
                            const unsigned char *data,
                            size_t size,
                            const Rich4SprSmpInfo *spr,
                            const Rich4SprSmpChunk *chunk,
                            int center_x,
                            int center_y,
                            int transparent_zero,
                            uint16_t owner_color,
                            int replace_owner_color,
                            int adjust)
{
    int x;
    int y;
    int dst_x;
    int dst_y;
    const unsigned char *src;
    const uint16_t *palette;
    uint16_t mapped_palette[256];
    int color;
    if (data == NULL || spr == NULL || chunk == NULL ||
        memcmp(spr->signature, "SPR", 4) != 0 ||
        spr->start_offset > size ||
        spr->start_offset + 512u > size ||
        chunk->width <= 0 || chunk->height <= 0 ||
        chunk->data_size < (uint32_t)(chunk->width * chunk->height)) {
        return 0;
    }

    dst_x = center_x - chunk->x;
    dst_y = center_y - chunk->y;
    src = data + chunk->data_offset;
    palette = (const uint16_t *)(const void *)(data + spr->start_offset);
    for (color = 0; color < 256; color++)
        mapped_palette[color] = map_resource_pixel_adjusted(screen,
            replace_owner_color && color == 255 ? owner_color : palette[color], adjust);
    SDL_LockSurface(screen);
    for (y = 0; y < chunk->height && dst_y + y < SCREEN_H; y++) {
        uint16_t *row;
        if (dst_y + y < 0) {
            continue;
        }
        row = (uint16_t *)((unsigned char *)screen->pixels + (dst_y + y) * screen->pitch);
        for (x = 0; x < chunk->width && dst_x + x < SCREEN_W; x++) {
            if (dst_x + x >= 0) {
                unsigned char index = src[y * chunk->width + x];
                if (!transparent_zero || index != 0) {
                    row[dst_x + x] = mapped_palette[index];
                }
            }
        }
    }
    SDL_UnlockSurface(screen);
    return 1;
}

int draw_chunk_topleft(SDL_Surface *screen,
                       const unsigned char *data,
                       const Rich4SprSmpChunk *chunk,
                       int dst_x,
                       int dst_y,
                       int transparent_zero)
{
    if (chunk == NULL || chunk->width <= 0 || chunk->height <= 0 ||
        chunk->data_size < (uint32_t)(chunk->width * chunk->height * 2)) return 0;
    return draw_rgb565_chunk(screen, data, chunk, 0, 0, chunk->width,
                             chunk->height, dst_x, dst_y, transparent_zero, 0);
}

int draw_chunk_crop_topleft(SDL_Surface *screen,
                            const unsigned char *data,
                            const Rich4SprSmpChunk *chunk,
                            int src_x,
                            int src_y,
                            int w,
                            int h,
                            int dst_x,
                            int dst_y,
                            int transparent_zero)
{
    if (chunk == NULL || chunk->width <= 0 || chunk->height <= 0 ||
        chunk->data_size < (uint32_t)(chunk->width * chunk->height * 2)) {
        return 0;
    }
    if (src_x < 0) {
        w += src_x;
        dst_x -= src_x;
        src_x = 0;
    }
    if (src_y < 0) {
        h += src_y;
        dst_y -= src_y;
        src_y = 0;
    }
    if (src_x + w > chunk->width) {
        w = chunk->width - src_x;
    }
    if (src_y + h > chunk->height) {
        h = chunk->height - src_y;
    }
    if (w <= 0 || h <= 0) {
        return 0;
    }

    return draw_rgb565_chunk(screen, data, chunk, src_x, src_y, w, h,
                             dst_x, dst_y, transparent_zero, 0);
}

int draw_setup_chunk(SDL_Surface *screen,
                     const GamePreviewAssets *assets,
                     size_t chunk_index,
                     int x,
                     int y,
                     int transparent_zero)
{
    Rich4SprSmpChunk chunk;
    Rich4SprSmpError err;

    if (assets == NULL || !assets->menu.has_setup_ui) {
        return 0;
    }
    err = rich4_spr_smp_get_chunk(assets->menu.setup_data,
                                      assets->menu.setup_size,
                                      &assets->menu.setup_spr,
                                      chunk_index,
                                      &chunk);
    if (err != RICH4_SPR_SMP_OK) {
        return 0;
    }
    return draw_chunk_topleft(screen, assets->menu.setup_data, &chunk, x, y, transparent_zero);
}

int draw_setup_chunk_crop(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          size_t chunk_index,
                          int src_x,
                          int src_y,
                          int w,
                          int h,
                          int x,
                          int y,
                          int transparent_zero)
{
    Rich4SprSmpChunk chunk;
    Rich4SprSmpError err;

    if (assets == NULL || !assets->menu.has_setup_ui) {
        return 0;
    }
    err = rich4_spr_smp_get_chunk(assets->menu.setup_data,
                                      assets->menu.setup_size,
                                      &assets->menu.setup_spr,
                                      chunk_index,
                                      &chunk);
    if (err != RICH4_SPR_SMP_OK) {
        return 0;
    }
    return draw_chunk_crop_topleft(screen,
                                   assets->menu.setup_data,
                                   &chunk,
                                   src_x,
                                   src_y,
                                   w,
                                   h,
                                   x,
                                   y,
                                   transparent_zero);
}

int draw_raw_rgb565_fullscreen(SDL_Surface *screen, const unsigned char *data, size_t size)
{
    int x;
    int y;
    const uint16_t *src;

    if (data == NULL || size < (size_t)(SCREEN_W * SCREEN_H * 2)) {
        return 0;
    }

    src = (const uint16_t *)(const void *)data;
    SDL_LockSurface(screen);
    for (y = 0; y < SCREEN_H; y++) {
        uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels + y * screen->pitch);
        for (x = 0; x < SCREEN_W; x++) {
            row[x] = map_resource_pixel(screen, src[y * SCREEN_W + x]);
        }
    }
    SDL_UnlockSurface(screen);
    return 1;
}

int draw_raw_rgb565_at(SDL_Surface *screen, const unsigned char *data,
                       size_t size, int width, int height, int x, int y)
{
    const uint16_t *src;
    int sx;
    int sy;
    if (screen == NULL || data == NULL || width <= 0 || height <= 0 ||
        size < (size_t)width * (size_t)height * 2u) return 0;
    src = (const uint16_t *)(const void *)data;
    SDL_LockSurface(screen);
    for (sy = 0; sy < height; sy++) {
        int dy = y + sy;
        uint16_t *row;
        if (dy < 0 || dy >= screen->h) continue;
        row = (uint16_t *)((unsigned char *)screen->pixels + dy * screen->pitch);
        for (sx = 0; sx < width; sx++) {
            int dx = x + sx;
            if (dx >= 0 && dx < screen->w)
                row[dx] = map_resource_pixel(screen, src[sy * width + sx]);
        }
    }
    SDL_UnlockSurface(screen);
    return 1;
}

int draw_raw_rgb565_scrolled(SDL_Surface *screen,
                             const unsigned char *data,
                             size_t size,
                             int scroll_offset,
                             int adjust)
{
    int x;
    int y;
    const uint16_t *src;
    int offset;

    if (data == NULL || size < (size_t)(SCREEN_W * SCREEN_H * 2)) {
        return 0;
    }

    offset = scroll_offset % (SCREEN_W * 2);
    offset /= 2;
    if (offset < 0) {
        offset += SCREEN_W;
    }
    src = (const uint16_t *)(const void *)data;
    SDL_LockSurface(screen);
    for (y = 0; y < SCREEN_H; y++) {
        uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels + y * screen->pitch);
        for (x = 0; x < SCREEN_W; x++) {
            int sx = x + offset;
            if (sx >= SCREEN_W) {
                sx -= SCREEN_W;
            }
            row[x] = map_resource_pixel_adjusted(screen, src[y * SCREEN_W + sx], adjust);
        }
    }
    SDL_UnlockSurface(screen);
    return 1;
}

void stroke_rect(SDL_Surface *screen, int x, int y, int w, int h, uint16_t color)
{
    fill_rect(screen, x, y, w, 2, color);
    fill_rect(screen, x, y + h - 2, w, 2, color);
    fill_rect(screen, x, y, 2, h, color);
    fill_rect(screen, x + w - 2, y, 2, h, color);
}

void stroke_rect_thin(SDL_Surface *screen, int x, int y, int w, int h, uint16_t color)
{
    fill_rect(screen, x, y, w, 1, color);
    fill_rect(screen, x, y + h - 1, w, 1, color);
    fill_rect(screen, x, y, 1, h, color);
    fill_rect(screen, x + w - 1, y, 1, h, color);
}

SDL_Surface *create_game_work_surface(SDL_Surface *screen)
{
    if (screen == NULL || screen->format == NULL) {
        return NULL;
    }
    return SDL_CreateRGBSurface(SDL_SWSURFACE,
                                SCREEN_W,
                                SCREEN_H,
                                screen->format->BitsPerPixel,
                                screen->format->Rmask,
                                screen->format->Gmask,
                                screen->format->Bmask,
                                screen->format->Amask);
}

int draw_spr_smp_center(SDL_Surface *screen,
                        const unsigned char *data,
                        size_t size,
                        const Rich4SprSmpInfo *spr,
                        size_t chunk_index,
                        int center_x,
                        int center_y)
{
    Rich4SprSmpChunk chunk;

    if (screen == NULL || data == NULL || spr == NULL ||
        rich4_spr_smp_get_chunk(data, size, spr, chunk_index, &chunk) != RICH4_SPR_SMP_OK) {
        return 0;
    }
    if (memcmp(spr->signature, "SPR", 4) == 0) {
        return draw_spr_chunk_at(screen, data, size, spr, &chunk, center_x, center_y, 1);
    }
    return draw_chunk_at(screen, data, &chunk, center_x, center_y, 1);
}

int draw_spr_smp_center_owner(SDL_Surface *screen,
                              const unsigned char *data,
                              size_t size,
                              const Rich4SprSmpInfo *spr,
                              size_t chunk_index,
                              int center_x,
                              int center_y,
                              int owner,
                              uint16_t owner_color,
                              int adjust)
{
    Rich4SprSmpChunk chunk;

    if (screen == NULL || data == NULL || spr == NULL ||
        rich4_spr_smp_get_chunk(data, size, spr, chunk_index, &chunk) != RICH4_SPR_SMP_OK) {
        return 0;
    }
    if (memcmp(spr->signature, "SPR", 4) == 0) {
        return draw_spr_chunk_at_owner(screen,
                                       data,
                                       size,
                                       spr,
                                       &chunk,
                                       center_x,
                                       center_y,
                                       1,
                                       owner_color,
                                       owner != 0xff,
                                       adjust);
    }
    return draw_chunk_at(screen, data, &chunk, center_x, center_y, 1);
}
