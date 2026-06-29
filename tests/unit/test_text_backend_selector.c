#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "platform/platform_profile.h"
#include "platform/sdl1/text/text_backend_selector.h"

int main(void)
{
    Rich4TextBackend backend = { 0 };
    Rich4FontAtlas atlas = { 0 };

#if RICH4_TEXT_BACKEND == RICH4_TEXT_BACKEND_ATLAS
    /* Backend selection needs only a synthetic glyph, not a licensed font. */
    const Rich4FontGlyph glyph = { 'A', 3, 2, 4, 0, 0, 0 };
    const unsigned char bitmap[] = { 255, 255, 255, 255, 255, 255 };
    atlas.cell_height = 16;
    atlas.glyph_count = 1;
    atlas.glyphs = &glyph;
    atlas.bitmap = bitmap;
    atlas.bitmap_size = sizeof(bitmap);
    assert(rich4_text_backend_init_for_profile(&backend, &atlas));
    assert(backend.borrowed_ctx == &atlas);
    {
        uint16_t pixels[32 * 32];
        Rich4TextTarget target = { pixels, 32 * (int)sizeof(uint16_t), 32, 32 };
        Rich4TextStyle style = { 16, 0, 1, 0, 0xffff, 0 };
        int ink = 0;
        int i;
        memset(pixels, 0, sizeof(pixels));
        assert(rich4_text_backend_render(
            &backend, &target, 0, 0, "A", &style));
        for (i = 0; i < (int)(sizeof(pixels) / sizeof(pixels[0])); i++)
            if (pixels[i] != 0) ink = 1;
        assert(ink);
    }
    rich4_text_backend_shutdown(&backend);
    rich4_font_atlas_close(&atlas);
#else
    uint16_t pixels[128 * 128];
    Rich4TextTarget target = {
        pixels, 128 * (int)sizeof(uint16_t), 128, 128
    };
    Rich4TextStyle style = { 20, 2, 1, 0, 0xffff, 0 };
    Rich4TextStyle regular_style = { 20, 0, 1, 0, 0xffff, 0 };
    int regular_width = 0;
    int bold_width = 0;
    int single_height = 0;
    int two_line_height = 0;
    int three_line_height = 0;
    int ink = 0;
    int first_line_ink = 0;
    int third_line_ink = 0;
    int i;

    rich4_text_backend_set_font_path("assets/fonts/rich4.ttc");
    assert(rich4_text_backend_init_for_profile(&backend, &atlas));
    assert(backend.ctx != NULL);
    assert(rich4_text_backend_measure(
        &backend, "\xa8\xfa\xae\xf8", &regular_style,
        &regular_width, &single_height));
    assert(rich4_text_backend_measure(
        &backend, "\xa8\xfa\xae\xf8", &style, &bold_width, NULL));
    assert(bold_width == regular_width + 1);
    assert(rich4_text_backend_measure(
        &backend, "\xa8\xfa\xae\xf8\n\xbd\x54\xa9\x77", &style,
        NULL, &two_line_height));
    assert(rich4_text_backend_measure(
        &backend, "\xa8\xfa\xae\xf8\n\n\xbd\x54\xa9\x77", &style,
        NULL, &three_line_height));
    assert(two_line_height > single_height);
    assert(three_line_height > two_line_height);
    memset(pixels, 0, sizeof(pixels));
    assert(rich4_text_backend_render(
        &backend, &target, 0, 0,
        "\xa8\xfa\xae\xf8\n\n\xbd\x54\xa9\x77", &style));
    for (i = 0; i < (int)(sizeof(pixels) / sizeof(pixels[0])); i++) {
        if (pixels[i] == 0) continue;
        ink = 1;
        if (i / 128 < single_height) first_line_ink = 1;
        if (i / 128 >= two_line_height) third_line_ink = 1;
    }
    assert(ink && first_line_ink && third_line_ink);
    rich4_text_backend_shutdown(&backend);
#endif
    puts("profile text backend selector: ok");
    return 0;
}
