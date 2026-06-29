#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "platform/sdl1/text/text_backend.h"

/* R10.2a: verify the platform-neutral text backend vtable dispatch, null
 * safety, and that a concrete backend can measure/render Big5 bytes onto an
 * RGB565 target. Uses a trivial fixed-cell backend so the contract is tested
 * without a real font engine. */

static int fixed_measure(Rich4TextBackend *self,
                         const char *big5_text,
                         const Rich4TextStyle *style,
                         int *out_width,
                         int *out_height)
{
    size_t len = strlen(big5_text);
    (void)self;
    *out_width = (int)len * (style->height / 2);
    *out_height = style->height;
    return 1;
}

static int fixed_render(Rich4TextBackend *self,
                       Rich4TextTarget *target,
                       int x,
                       int y,
                       const char *big5_text,
                       const Rich4TextStyle *style)
{
    int w = 0;
    int h = 0;
    int px;
    int py;
    fixed_measure(self, big5_text, style, &w, &h);
    /* Apply the shared anchor rule from text_rules so backends stay uniform. */
    rich4_text_apply_anchor(style->mode, w, h, &x, &y);
    for (py = y; py < y + h; py++) {
        uint16_t *row;
        if (py < 0 || py >= target->height) continue;
        row = (uint16_t *)((unsigned char *)target->pixels + py * target->pitch_bytes);
        for (px = x; px < x + w; px++) {
            if (px < 0 || px >= target->width) continue;
            row[px] = style->fg;
        }
    }
    return 1;
}

static int shutdown_calls = 0;
static void fixed_shutdown(Rich4TextBackend *self)
{
    (void)self;
    shutdown_calls++;
}

int main(void)
{
    Rich4TextBackend backend;
    Rich4TextStyle style;
    Rich4TextTarget target;
    uint16_t pixels[16 * 16];
    int w = -1;
    int h = -1;

    backend.measure = fixed_measure;
    backend.render = fixed_render;
    backend.shutdown = fixed_shutdown;
    backend.ctx = NULL;

    style.height = 12;
    style.style = 0;
    style.spacing = 0;
    style.mode = 0; /* top-left anchor: no offset */
    style.fg = 0xffff;
    style.shadow = 0x0000;

    /* measure dispatch */
    assert(rich4_text_backend_measure(&backend, "AB", &style, &w, &h) == 1);
    assert(w == 2 * 6);
    assert(h == 12);

    /* null safety: outputs zeroed, returns 0 */
    w = 7; h = 7;
    assert(rich4_text_backend_measure(NULL, "AB", &style, &w, &h) == 0);
    assert(w == 0 && h == 0);

    /* render dispatch onto RGB565 target */
    memset(pixels, 0, sizeof(pixels));
    target.pixels = pixels;
    target.pitch_bytes = 16 * (int)sizeof(uint16_t);
    target.width = 16;
    target.height = 16;
    assert(rich4_text_backend_render(&backend, &target, 0, 0, "A", &style) == 1);
    /* height 12, width 6: pixel (0,0) set to fg */
    assert(pixels[0] == 0xffff);
    assert(pixels[6] == 0x0000); /* just past width */

    /* render null safety */
    assert(rich4_text_backend_render(NULL, &target, 0, 0, "A", &style) == 0);
    target.pixels = NULL;
    assert(rich4_text_backend_render(&backend, &target, 0, 0, "A", &style) == 0);

    /* shutdown dispatch + null safety */
    rich4_text_backend_shutdown(&backend);
    rich4_text_backend_shutdown(NULL);
    assert(shutdown_calls == 1);

    printf("text backend interface: ok\n");
    return 0;
}
