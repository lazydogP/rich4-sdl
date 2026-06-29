#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/sdl1/text/font_atlas.h"
#include "platform/sdl1/text/text_backend_atlas.h"

/* R10.2d: golden test for the engine-free atlas path. Builds a tiny atlas in
 * memory (two ASCII glyphs with known A8 coverage), round-trips it through the
 * serializer/parser, then renders through the atlas backend and asserts exact
 * RGB565 pixels. No font engine involved. */

int main(void)
{
    /* Glyph 'A' (0x41): 3x2 solid; advance 4. Glyph 'B' (0x42): 2x2 solid;
     * advance 3. Sorted by code. cell_height 4. */
    Rich4FontGlyph glyphs[2];
    unsigned char bitmap[3 * 2 + 2 * 2];
    unsigned char *image = NULL;
    size_t image_size;
    Rich4FontAtlas atlas;
    Rich4TextBackend backend;
    Rich4TextStyle style;
    Rich4TextTarget target;
    uint16_t pixels[16 * 8];
    int w = 0;
    int h = 0;
    int gx;
    int gy;

    memset(bitmap, 0, sizeof(bitmap));
    /* 'A' coverage: full (offset 0, 6 bytes) */
    for (gy = 0; gy < 6; gy++) bitmap[gy] = 255;
    /* 'B' coverage: full (offset 6, 4 bytes) */
    for (gy = 6; gy < 10; gy++) bitmap[gy] = 255;

    glyphs[0].code = 0x41;
    glyphs[0].width = 3;
    glyphs[0].height = 2;
    glyphs[0].advance = 4;
    glyphs[0].bearing_x = 0;
    glyphs[0].bearing_y = 0;
    glyphs[0].bitmap_offset = 0;

    glyphs[1].code = 0x42;
    glyphs[1].width = 2;
    glyphs[1].height = 2;
    glyphs[1].advance = 3;
    glyphs[1].bearing_x = 0;
    glyphs[1].bearing_y = 0;
    glyphs[1].bitmap_offset = 6;

    image_size = rich4_font_atlas_serialize(4, glyphs, 2, bitmap, sizeof(bitmap),
                                            &image);
    assert(image_size > 0 && image != NULL);

    assert(rich4_font_atlas_open_memory(&atlas, image, image_size) == 1);
    free(image);
    assert(atlas.cell_height == 4);
    assert(atlas.glyph_count == 2);

    /* lookup + binary search */
    assert(rich4_font_atlas_find(&atlas, 0x41) != NULL);
    assert(rich4_font_atlas_find(&atlas, 0x42) != NULL);
    assert(rich4_font_atlas_find(&atlas, 0x43) == NULL);

    assert(rich4_text_backend_atlas_init(&backend, &atlas) == 1);

    style.height = 4;
    style.style = 0;
    style.spacing = 0;
    style.mode = 0; /* no anchor offset */
    style.fg = 0x1234;
    style.shadow = 0x0000;

    assert(rich4_text_backend_measure(&backend, "AB", &style, &w, &h) == 1);
    assert(w == 4 + 3);
    assert(h == 4);

    memset(pixels, 0, sizeof(pixels));
    target.pixels = pixels;
    target.pitch_bytes = 16 * (int)sizeof(uint16_t);
    target.width = 16;
    target.height = 8;
    assert(rich4_text_backend_render(&backend, &target, 0, 0, "AB", &style) == 1);

    /* 'A' at x 0..2, y 0..1 */
    for (gy = 0; gy < 2; gy++) {
        for (gx = 0; gx < 3; gx++) {
            assert(pixels[gy * 16 + gx] == 0x1234);
        }
    }
    /* gap at x 3 (advance 4, glyph width 3) */
    assert(pixels[0 * 16 + 3] == 0x0000);
    /* 'B' at pen_x 4, x 4..5, y 0..1 */
    for (gy = 0; gy < 2; gy++) {
        for (gx = 4; gx < 6; gx++) {
            assert(pixels[gy * 16 + gx] == 0x1234);
        }
    }
    /* nothing past B */
    assert(pixels[0 * 16 + 6] == 0x0000);

    rich4_text_backend_shutdown(&backend);
    rich4_font_atlas_close(&atlas);

    printf("font atlas + atlas backend golden: ok\n");
    return 0;
}
