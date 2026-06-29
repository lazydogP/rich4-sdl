#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#undef main
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/render/pixel_rows.h"

static uint32_t original_rgb565_to_rgb888(SDL_PixelFormat *format,
                                           uint16_t pixel)
{
    /* Original presentation expansion: discarded bits are zero-filled. */
    return SDL_MapRGB(format,
        (Uint8)((pixel >> 11) * 8),
        (Uint8)(((pixel >> 5) & 63) * 4),
        (Uint8)((pixel & 31) * 8));
}

static void check_rows(void)
{
    uint16_t src[272], ui[274];
    uint32_t out[274];
    int base, x, reverse, transparent, tail;
    /* All 65536 inputs, vector tails, unaligned-to-vector addresses, guards,
     * transparent black versus nonzero 0x8000, and both row directions. */
    for (base = 0; base < 65536; base += 256) {
        for (x = 0; x < 272; x++) src[x] = (uint16_t)(base + x);
        for (tail = 0; tail < 16; tail++) {
            int count = 256 + tail;
            for (transparent = 0; transparent < 2; transparent++) {
                for (x = 0; x < 274; x++) ui[x] = 0xa55a;
                rich4_rgb555_row(ui + 1, src, count, transparent);
                for (x = 0; x < count; x++) {
                    unsigned p = src[x];
                    unsigned expected = transparent && p == 0 ? 0xa55a :
                        ((p & 0x7c00u) << 1) |
                        ((p & 0x03e0u) << 1) | (p & 0x001fu);
                    assert(ui[x + 1] == expected);
                }
                assert(ui[0] == 0xa55a && ui[count + 1] == 0xa55a);
            }
            for (reverse = 0; reverse < 2; reverse++) {
                uint32_t alpha = tail & 1 ? 0xff000000u : 0;
                for (x = 0; x < 274; x++) out[x] = 0x12345678;
                rich4_present_rgb888_row(out + 1, src, count, reverse, alpha);
                for (x = 0; x < count; x++) {
                    unsigned p = src[reverse ? count - 1 - x : x];
                    unsigned r = (p >> 11) * 8;
                    unsigned g = ((p >> 5) & 63) * 4;
                    unsigned b = (p & 31) * 8;
                    assert(out[x + 1] == (alpha | (r << 16) | (g << 8) | b));
                }
                assert(out[0] == 0x12345678 && out[count + 1] == 0x12345678);
            }
        }
    }
}

static void check_surface(int width, int height)
{
    SDL_Surface *screen = rich4_sdl1_create_window(width, height, 16, "pixel test");
    SDL_Surface *display = SDL_GetVideoSurface();
    int frame, x, y;
    assert(screen && display && display->format->BytesPerPixel == 4);
    for (frame = 0; frame < 4; frame++) {
#ifdef _WIN32
        assert(_putenv(frame & 1 ? "RICH4_MIYOO_ROTATE=1" : "RICH4_MIYOO_ROTATE=0") == 0);
#else
        assert(setenv("RICH4_MIYOO_ROTATE", frame & 1 ? "1" : "0", 1) == 0);
#endif
        for (y = 0; y < height; y++) {
            uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels + y * screen->pitch);
            for (x = 0; x < width; x++) row[x] = (uint16_t)(y * width + x + frame * 7919);
        }
        assert(rich4_sdl1_present(screen));
        assert(SDL_LockSurface(display) == 0);
        for (y = 0; y < height; y++) {
            uint32_t *row = (uint32_t *)((unsigned char *)display->pixels + y * display->pitch);
            for (x = 0; x < width; x++) {
                int sx = frame & 1 ? width - 1 - x : x;
                int sy = frame & 1 ? height - 1 - y : y;
                unsigned p = (uint16_t)(sy * width + sx + frame * 7919);
                uint32_t expected = original_rgb565_to_rgb888(
                    display->format, (uint16_t)p);
                if (row[x] != expected) {
                    fprintf(stderr, "present mismatch size=%dx%d display=%dx%d frame=%d xy=%d,%d actual=%08x expected=%08x masks=%08x,%08x,%08x,%08x\n",
                        width,height,display->w,display->h,frame,x,y,(unsigned)row[x],(unsigned)expected,
                        (unsigned)display->format->Rmask,(unsigned)display->format->Gmask,
                        (unsigned)display->format->Bmask,(unsigned)display->format->Amask);
                    abort();
                }
            }
        }
        SDL_UnlockSurface(display);
    }
    rich4_sdl1_destroy_window(screen);
}

int main(void)
{
    check_rows();
    assert(SDL_putenv("SDL_VIDEODRIVER=dummy") == 0);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    check_surface(640, 480);
    check_surface(19, 17); /* row pitch and non-vector-width tail */
    SDL_Quit();
    puts("QA_PRESENT_PIXELS_PASS all_colors=65536 rotation=0,180 tails=0..15 transparency=exact");
    return 0;
}
