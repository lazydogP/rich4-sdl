#ifndef RICH4_PLATFORM_SDL1_TEXT_FONT_ATLAS_H
#define RICH4_PLATFORM_SDL1_TEXT_FONT_ATLAS_H

#include <stddef.h>
#include <stdint.h>

/* R10.2c/d: prebaked bitmap glyph atlas format.
 *
 * The atlas is produced at build time by tools/font_bake from a licensed CJK
 * TrueType font and consumed at runtime by text_backend_atlas with zero font
 * engine (no FreeType/GDI). It is a flat, little-endian file so it maps 1:1
 * into memory on the handheld target:
 *
 *   char     magic[4]   = "R4FA"
 *   uint32   version    = 1
 *   uint32   cell_height        (nominal pixel height glyphs were baked at)
 *   uint32   glyph_count
 *   uint32   bitmap_size        (bytes of the A8 coverage blob)
 *   glyph[glyph_count]          (sorted ascending by code, see Rich4FontGlyph)
 *   uint8    bitmap[bitmap_size](A8 coverage, row-major per glyph)
 *
 * `code` is the Big5 key: ASCII bytes (<0x80) use their byte value; DBCS
 * characters use (lead<<8)|trail. Coverage is 1 byte alpha per pixel. */

#define RICH4_FONT_ATLAS_MAGIC0 'R'
#define RICH4_FONT_ATLAS_MAGIC1 '4'
#define RICH4_FONT_ATLAS_MAGIC2 'F'
#define RICH4_FONT_ATLAS_MAGIC3 'A'
#define RICH4_FONT_ATLAS_VERSION 1u

typedef struct Rich4FontGlyph {
    uint16_t code;        /* Big5 key */
    uint16_t width;       /* bitmap width in pixels */
    uint16_t height;      /* bitmap height in pixels */
    uint16_t advance;     /* horizontal pen advance in pixels */
    int16_t bearing_x;    /* left side bearing */
    int16_t bearing_y;    /* top bearing from baseline row */
    uint32_t bitmap_offset; /* byte offset into the A8 blob */
} Rich4FontGlyph;

typedef struct Rich4FontAtlas {
    uint32_t cell_height;
    uint32_t glyph_count;
    const Rich4FontGlyph *glyphs; /* borrowed or into owned buffer */
    const unsigned char *bitmap;  /* A8 coverage blob */
    size_t bitmap_size;
    Rich4FontGlyph *decoded;      /* owned decoded glyph table (or NULL) */
    unsigned char *owned;         /* owned file buffer (or NULL) */
} Rich4FontAtlas;

/* Parse an atlas from an in-memory image (copies into owned storage). */
int rich4_font_atlas_open_memory(Rich4FontAtlas *atlas,
                                 const unsigned char *data,
                                 size_t size);
/* Load an atlas from a file on disk. */
int rich4_font_atlas_load_file(Rich4FontAtlas *atlas, const char *path);
/* Release owned storage. Safe to call on a zeroed struct. */
void rich4_font_atlas_close(Rich4FontAtlas *atlas);
/* Binary search for a glyph by Big5 key; NULL if absent. */
const Rich4FontGlyph *rich4_font_atlas_find(const Rich4FontAtlas *atlas,
                                            uint16_t code);

/* Serialize an atlas image into a newly malloc'd buffer (caller frees).
 * `glyphs` must be sorted ascending by code. Returns byte size, 0 on error. */
size_t rich4_font_atlas_serialize(uint32_t cell_height,
                                  const Rich4FontGlyph *glyphs,
                                  uint32_t glyph_count,
                                  const unsigned char *bitmap,
                                  size_t bitmap_size,
                                  unsigned char **out_data);
/* Serialize and write directly to a file. Returns non-zero on success. */
int rich4_font_atlas_write_file(const char *path,
                                uint32_t cell_height,
                                const Rich4FontGlyph *glyphs,
                                uint32_t glyph_count,
                                const unsigned char *bitmap,
                                size_t bitmap_size);

#endif
