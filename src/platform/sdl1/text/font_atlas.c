#include "platform/sdl1/text/font_atlas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* R10.2c/d: engine-free font atlas (de)serialization and lookup. Platform
 * neutral: no SDL, no FreeType. Explicit little-endian encoding keeps baked
 * atlases portable between the desktop baker and the handheld consumer. */

#define RICH4_FONT_ATLAS_HEADER_SIZE 20u
#define RICH4_FONT_ATLAS_GLYPH_SIZE 16u

static void put_u16(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
}

static void put_u32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
}

static uint16_t get_u16(const unsigned char *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

size_t rich4_font_atlas_serialize(uint32_t cell_height,
                                  const Rich4FontGlyph *glyphs,
                                  uint32_t glyph_count,
                                  const unsigned char *bitmap,
                                  size_t bitmap_size,
                                  unsigned char **out_data)
{
    size_t total;
    unsigned char *buf;
    unsigned char *p;
    uint32_t i;

    if (out_data == NULL || (glyph_count != 0 && glyphs == NULL) ||
        (bitmap_size != 0 && bitmap == NULL)) {
        return 0;
    }
    total = RICH4_FONT_ATLAS_HEADER_SIZE +
            (size_t)glyph_count * RICH4_FONT_ATLAS_GLYPH_SIZE + bitmap_size;
    buf = (unsigned char *)malloc(total);
    if (buf == NULL) {
        return 0;
    }
    p = buf;
    p[0] = RICH4_FONT_ATLAS_MAGIC0;
    p[1] = RICH4_FONT_ATLAS_MAGIC1;
    p[2] = RICH4_FONT_ATLAS_MAGIC2;
    p[3] = RICH4_FONT_ATLAS_MAGIC3;
    put_u32(p + 4, RICH4_FONT_ATLAS_VERSION);
    put_u32(p + 8, cell_height);
    put_u32(p + 12, glyph_count);
    put_u32(p + 16, (uint32_t)bitmap_size);
    p += RICH4_FONT_ATLAS_HEADER_SIZE;
    for (i = 0; i < glyph_count; i++) {
        put_u16(p + 0, glyphs[i].code);
        put_u16(p + 2, glyphs[i].width);
        put_u16(p + 4, glyphs[i].height);
        put_u16(p + 6, glyphs[i].advance);
        put_u16(p + 8, (uint16_t)glyphs[i].bearing_x);
        put_u16(p + 10, (uint16_t)glyphs[i].bearing_y);
        put_u32(p + 12, glyphs[i].bitmap_offset);
        p += RICH4_FONT_ATLAS_GLYPH_SIZE;
    }
    if (bitmap_size != 0) {
        memcpy(p, bitmap, bitmap_size);
    }
    *out_data = buf;
    return total;
}

int rich4_font_atlas_write_file(const char *path,
                                uint32_t cell_height,
                                const Rich4FontGlyph *glyphs,
                                uint32_t glyph_count,
                                const unsigned char *bitmap,
                                size_t bitmap_size)
{
    unsigned char *buf = NULL;
    size_t total;
    FILE *fp;
    size_t written;

    total = rich4_font_atlas_serialize(cell_height, glyphs, glyph_count,
                                       bitmap, bitmap_size, &buf);
    if (total == 0) {
        return 0;
    }
    fp = fopen(path, "wb");
    if (fp == NULL) {
        free(buf);
        return 0;
    }
    written = fwrite(buf, 1, total, fp);
    fclose(fp);
    free(buf);
    return written == total;
}

int rich4_font_atlas_open_memory(Rich4FontAtlas *atlas,
                                 const unsigned char *data,
                                 size_t size)
{
    uint32_t glyph_count;
    uint32_t bitmap_size;
    size_t need;
    uint32_t i;
    Rich4FontGlyph *decoded;
    unsigned char *owned;

    if (atlas == NULL || data == NULL || size < RICH4_FONT_ATLAS_HEADER_SIZE) {
        return 0;
    }
    if (data[0] != RICH4_FONT_ATLAS_MAGIC0 || data[1] != RICH4_FONT_ATLAS_MAGIC1 ||
        data[2] != RICH4_FONT_ATLAS_MAGIC2 || data[3] != RICH4_FONT_ATLAS_MAGIC3) {
        return 0;
    }
    if (get_u32(data + 4) != RICH4_FONT_ATLAS_VERSION) {
        return 0;
    }
    glyph_count = get_u32(data + 12);
    bitmap_size = get_u32(data + 16);
    need = RICH4_FONT_ATLAS_HEADER_SIZE +
           (size_t)glyph_count * RICH4_FONT_ATLAS_GLYPH_SIZE + bitmap_size;
    if (size < need) {
        return 0;
    }

    memset(atlas, 0, sizeof(*atlas));
    atlas->cell_height = get_u32(data + 8);
    atlas->glyph_count = glyph_count;

    owned = NULL;
    decoded = NULL;
    if (bitmap_size != 0) {
        owned = (unsigned char *)malloc(bitmap_size);
        if (owned == NULL) {
            return 0;
        }
        memcpy(owned, data + RICH4_FONT_ATLAS_HEADER_SIZE +
                          (size_t)glyph_count * RICH4_FONT_ATLAS_GLYPH_SIZE,
               bitmap_size);
    }
    if (glyph_count != 0) {
        const unsigned char *gp = data + RICH4_FONT_ATLAS_HEADER_SIZE;
        decoded = (Rich4FontGlyph *)malloc((size_t)glyph_count * sizeof(*decoded));
        if (decoded == NULL) {
            free(owned);
            return 0;
        }
        for (i = 0; i < glyph_count; i++) {
            decoded[i].code = get_u16(gp + 0);
            decoded[i].width = get_u16(gp + 2);
            decoded[i].height = get_u16(gp + 4);
            decoded[i].advance = get_u16(gp + 6);
            decoded[i].bearing_x = (int16_t)get_u16(gp + 8);
            decoded[i].bearing_y = (int16_t)get_u16(gp + 10);
            decoded[i].bitmap_offset = get_u32(gp + 12);
            gp += RICH4_FONT_ATLAS_GLYPH_SIZE;
        }
    }
    atlas->decoded = decoded;
    atlas->glyphs = decoded;
    atlas->owned = owned;
    atlas->bitmap = owned;
    atlas->bitmap_size = bitmap_size;
    return 1;
}

int rich4_font_atlas_load_file(Rich4FontAtlas *atlas, const char *path)
{
    FILE *fp;
    long size;
    unsigned char *data;
    int ok;

    if (atlas == NULL || path == NULL) {
        return 0;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        return 0;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 0;
    }
    data = (unsigned char *)malloc((size_t)size);
    if (data == NULL) {
        fclose(fp);
        return 0;
    }
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    ok = rich4_font_atlas_open_memory(atlas, data, (size_t)size);
    free(data);
    return ok;
}

void rich4_font_atlas_close(Rich4FontAtlas *atlas)
{
    if (atlas == NULL) {
        return;
    }
    free(atlas->decoded);
    free(atlas->owned);
    memset(atlas, 0, sizeof(*atlas));
}

const Rich4FontGlyph *rich4_font_atlas_find(const Rich4FontAtlas *atlas,
                                            uint16_t code)
{
    uint32_t lo;
    uint32_t hi;

    if (atlas == NULL || atlas->glyphs == NULL || atlas->glyph_count == 0) {
        return NULL;
    }
    lo = 0;
    hi = atlas->glyph_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        uint16_t mc = atlas->glyphs[mid].code;
        if (mc == code) {
            return &atlas->glyphs[mid];
        }
        if (mc < code) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return NULL;
}
