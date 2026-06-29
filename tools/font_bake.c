/*
 * R10.2c: font_bake -- build a prebaked bitmap glyph atlas for the handheld
 * (text_backend_atlas) target.
 *
 * It scans one or more Big5 text sources for the ASCII and DBCS code points the
 * game actually uses (e.g. src/data/special_event_text.c,
 * src/data/character_speech.c, or dumped MKF text), rasterizes each glyph from
 * a licensed CJK TrueType via SDL_ttf, and writes the flat little-endian atlas
 * defined in font_atlas.h. The runtime consumer needs no font engine.
 *
 * Usage:
 *   font_bake <font.ttf> <pixel_height> <out.r4atlas> <in1> [in2 ...]
 *
 * The chosen font and the exact character set (derived from the inputs) must be
 * recorded as evidence; see docs/evidence for the licensing note.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>
#if defined(HAVE_SDL_TTF)
#include <SDL/SDL_ttf.h>
#endif
#ifdef _WIN32
#undef main
#endif

#include "platform/sdl1/text/big5_utf8.h"
#include "platform/sdl1/text/font_atlas.h"

#if !defined(HAVE_SDL_TTF)
int main(void)
{
    fprintf(stderr, "font_bake requires SDL_ttf at build time (HAVE_SDL_TTF).\n");
    return 2;
}
#else

/* Set of 16-bit Big5 keys, kept sorted-on-demand. */
typedef struct CodeSet {
    uint16_t *codes;
    size_t count;
    size_t capacity;
} CodeSet;

static int code_set_has(const CodeSet *set, uint16_t code)
{
    size_t i;
    for (i = 0; i < set->count; i++) {
        if (set->codes[i] == code) return 1;
    }
    return 0;
}

static void code_set_add(CodeSet *set, uint16_t code)
{
    if (code_set_has(set, code)) {
        return;
    }
    if (set->count == set->capacity) {
        size_t cap = set->capacity ? set->capacity * 2 : 256;
        uint16_t *grown = (uint16_t *)realloc(set->codes, cap * sizeof(uint16_t));
        if (grown == NULL) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        set->codes = grown;
        set->capacity = cap;
    }
    set->codes[set->count++] = code;
}

static int cmp_u16(const void *a, const void *b)
{
    uint16_t x = *(const uint16_t *)a;
    uint16_t y = *(const uint16_t *)b;
    return (x > y) - (x < y);
}

static void scan_file(CodeSet *set, const char *path)
{
    FILE *fp = fopen(path, "rb");
    int c;
    if (fp == NULL) {
        fprintf(stderr, "warning: cannot open %s\n", path);
        return;
    }
    while ((c = fgetc(fp)) != EOF) {
        unsigned char lead = (unsigned char)c;
        if (lead < 0x80) {
            if (lead >= 0x20 && lead < 0x7f) {
                code_set_add(set, (uint16_t)lead);
            }
            continue;
        }
        /* Big5 lead byte range 0x81..0xfe; consume a trail byte. */
        {
            int t = fgetc(fp);
            if (t == EOF) break;
            code_set_add(set, (uint16_t)((lead << 8) | (unsigned char)t));
        }
    }
    fclose(fp);
}

/* Convert a Big5/CP950 key to a UTF-8 NUL-terminated string in `out`. */
static int code_to_utf8(uint16_t code, char out[8])
{
    uint16_t cp = rich4_cp950_to_ucs2(code);
    if (code >= 0x80 && cp == 0) {
        return 0;
    }
    if (cp < 0x80) {
        out[0] = (char)cp;
        out[1] = '\0';
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xc0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3f));
        out[2] = '\0';
        return 1;
    }
    out[0] = (char)(0xe0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[2] = (char)(0x80 | (cp & 0x3f));
    out[3] = '\0';
    return 1;
}

int main(int argc, char **argv)
{
    const char *font_path;
    int height;
    const char *out_path;
    int i;
    CodeSet set;
    TTF_Font *font;
    Rich4FontGlyph *glyphs;
    unsigned char *bitmap = NULL;
    size_t bitmap_size = 0;
    size_t bitmap_cap = 0;
    size_t glyph_out = 0;

    if (argc < 5) {
        fprintf(stderr,
                "usage: %s <font.ttf> <pixel_height> <out.r4atlas> <in1> [in2 ...]\n",
                argv[0]);
        return 2;
    }
    font_path = argv[1];
    height = atoi(argv[2]);
    out_path = argv[3];
    if (height < 1 || height > 128) {
        fprintf(stderr, "pixel_height out of range\n");
        return 2;
    }

    memset(&set, 0, sizeof(set));
    for (i = 4; i < argc; i++) {
        scan_file(&set, argv[i]);
    }
    if (set.count == 0) {
        fprintf(stderr, "no code points found in inputs\n");
        return 1;
    }
    qsort(set.codes, set.count, sizeof(uint16_t), cmp_u16);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return 1;
    }
    font = TTF_OpenFont(font_path, height);
    if (font == NULL) {
        fprintf(stderr, "cannot open font %s: %s\n", font_path, TTF_GetError());
        return 1;
    }

    glyphs = (Rich4FontGlyph *)calloc(set.count, sizeof(*glyphs));
    if (glyphs == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    for (i = 0; i < (int)set.count; i++) {
        uint16_t code = set.codes[i];
        char utf8[8];
        SDL_Color white = { 255, 255, 255, 255 };
        SDL_Surface *glyph;
        int adv = 0;
        int gh = 0;
        int gw = 0;
        int gx;
        int gy;

        if (!code_to_utf8(code, utf8)) {
            continue;
        }
        glyph = TTF_RenderUTF8_Blended(font, utf8, white);
        if (glyph == NULL) {
            continue;
        }
        TTF_SizeUTF8(font, utf8, &adv, &gh);
        gw = glyph->w;
        gh = glyph->h;

        glyphs[glyph_out].code = code;
        glyphs[glyph_out].width = (uint16_t)gw;
        glyphs[glyph_out].height = (uint16_t)gh;
        glyphs[glyph_out].advance = (uint16_t)(adv > 0 ? adv : gw);
        glyphs[glyph_out].bearing_x = 0;
        glyphs[glyph_out].bearing_y = 0;
        glyphs[glyph_out].bitmap_offset = (uint32_t)bitmap_size;

        if (bitmap_size + (size_t)gw * gh > bitmap_cap) {
            size_t cap = bitmap_cap ? bitmap_cap * 2 : 65536;
            while (cap < bitmap_size + (size_t)gw * gh) cap *= 2;
            bitmap = (unsigned char *)realloc(bitmap, cap);
            if (bitmap == NULL) {
                fprintf(stderr, "out of memory\n");
                return 1;
            }
            bitmap_cap = cap;
        }
        SDL_LockSurface(glyph);
        for (gy = 0; gy < gh; gy++) {
            const Uint32 *srow = (const Uint32 *)((const unsigned char *)glyph->pixels +
                                                  gy * glyph->pitch);
            for (gx = 0; gx < gw; gx++) {
                Uint8 r, g, b, a;
                SDL_GetRGBA(srow[gx], glyph->format, &r, &g, &b, &a);
                bitmap[bitmap_size + (size_t)gy * gw + gx] = a;
            }
        }
        SDL_UnlockSurface(glyph);
        bitmap_size += (size_t)gw * gh;
        SDL_FreeSurface(glyph);
        glyph_out++;
    }

    if (!rich4_font_atlas_write_file(out_path, (uint32_t)height, glyphs,
                                     (uint32_t)glyph_out, bitmap, bitmap_size)) {
        fprintf(stderr, "failed to write %s\n", out_path);
        return 1;
    }
    fprintf(stderr, "baked %zu glyphs (%zu bytes coverage) -> %s\n",
            glyph_out, bitmap_size, out_path);

    free(glyphs);
    free(bitmap);
    free(set.codes);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

#endif
