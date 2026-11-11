#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "platform/sdl1/text/text_backend_freetype.h"

int main(void)
{
    Rich4TextBackend backend = { 0 };
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

    rich4_text_backend_freetype_set_font_path("assets/fonts/rich4.ttc");
    assert(rich4_text_backend_freetype_init(&backend));
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
    puts("FreeType text backend: ok");
    return 0;
}
