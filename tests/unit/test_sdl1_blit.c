#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>
#undef main

#include "platform/sdl1/render/blit.h"

static uint16_t *pixel_at(SDL_Surface *surface, int x, int y)
{
    return (uint16_t *)((unsigned char *)surface->pixels +
                        y * surface->pitch) + x;
}

static uint16_t original_rgb555_to_rgb565(uint16_t pixel)
{
    return (uint16_t)(((pixel & 0x7c00u) << 1) |
                      ((pixel & 0x03e0u) << 1) | (pixel & 0x001fu));
}

static uint16_t shrd16(uint16_t dst, uint16_t src, int bits)
{
    return (uint16_t)((dst >> bits) | ((unsigned int)src << (16 - bits)));
}

/* Register-level reference for 0x4555EB/628/66E/6AB, including overflow.
 * Inputs are resource RGB555 words, before 0x451801 surface conversion. */
static uint16_t original_sleeping_pixel(uint16_t pixel, int format)
{
    static const unsigned char lut[32] = {
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,7,6,5,4,3,2,1,0
    };
    unsigned int p = pixel, mask = format == 3 ? 15u : 31u;
    unsigned int ax, bx;
    if (format == 1) p = original_rgb555_to_rgb565(pixel);
    else if (format == 2)
        p = ((pixel & 31u) << 11) | ((pixel & 0x03e0u) << 1) |
            ((pixel & 0x7c00u) >> 10);
    else if (format == 3)
        p = ((pixel & 0x7800u) >> 3) | ((pixel & 0x03c0u) >> 2) |
            ((pixel & 0x001eu) >> 1);
    ax = p & mask;
    bx = (p >> (format == 0 ? 5 : format == 3 ? 4 : 6)) & mask;
    ax += bx;
    bx = (p >> (format == 0 ? 10 : format == 3 ? 8 : 11)) & mask;
    ax += bx;
    ax = (ax + (format == 1 ? 31 : format == 3 ? 20 : 40)) >> 2;
    if (format == 1) ax = (ax & 31u) + lut[ax & 31u];
    if (format == 2) {
        bx = shrd16((uint16_t)bx, (uint16_t)ax, 6);
        bx = shrd16((uint16_t)bx, (uint16_t)ax, 5);
        return shrd16((uint16_t)bx, 31, 5);
    }
    bx = shrd16((uint16_t)bx, 31, format == 0 ? 5 : format == 3 ? 4 : 6);
    bx = shrd16((uint16_t)bx, (uint16_t)ax, format == 3 ? 4 : 5);
    return shrd16((uint16_t)bx, (uint16_t)ax,
        format == 0 ? 6 : format == 3 ? 8 : 5);
}

static void check_original_sleeping_palette(void)
{
    static const Uint32 masks[4][3] = {
        {0x7c00,0x03e0,0x001f}, {0xf800,0x07e0,0x001f},
        {0x001f,0x07e0,0xf800}, {0x0f00,0x00f0,0x000f}
    };
    int format;
    unsigned int color;
    for (format = 0; format < 4; format++) {
        SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
            masks[format][0], masks[format][1], masks[format][2], 0);
        assert(s);
        for (color = 0; color <= 65535; color++) {
            uint16_t actual = map_resource_pixel_sleeping(s, (uint16_t)color);
            uint16_t expected = original_sleeping_pixel((uint16_t)color, format);
            if (actual != expected)
                fprintf(stderr, "sleep palette: format=%d color=%04x actual=%04x expected=%04x\n",
                    format, color, actual, expected);
            assert(actual == expected);
        }
        SDL_FreeSurface(s);
    }
}

static void check_clipped_color_blits(void)
{
    static uint16_t source[256 * 256];
    static const int positions[][2] = {{0,0}, {-13,-7}, {631,473}, {700,20}};
    Rich4SprSmpChunk chunk = {0};
    int format, transparent, test, x, y;
    for (x = 0; x < 65536; x++) source[x] = (uint16_t)x;
    chunk.width = chunk.height = 256;
    chunk.data_size = sizeof(source);
    for (format = 0; format < 2; format++) {
        SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
            format ? 0x7c00 : 0xf800, format ? 0x03e0 : 0x07e0, 0x001f, 0);
        assert(s);
        for (transparent = 0; transparent < 2; transparent++) {
            for (test = 0; test < 5; test++) {
                int dx = test < 4 ? positions[test][0] : -3;
                int dy = test < 4 ? positions[test][1] : 469;
                int sx = test < 4 ? 0 : 5, sy = test < 4 ? 0 : 17;
                int w = test < 4 ? 256 : 23, h = test < 4 ? 256 : 19;
                for (y = 0; y < s->h; y++)
                    for (x = 0; x < s->w; x++) *pixel_at(s,x,y) = 0x5aa5;
                assert(draw_chunk_crop_topleft(s, (const unsigned char *)source,
                    &chunk, sx, sy, w, h, dx, dy, transparent));
                for (y = 0; y < s->h; y++) {
                    for (x = 0; x < s->w; x++) {
                        uint16_t expected = 0x5aa5;
                        if (x >= dx && x < dx + w && y >= dy && y < dy + h) {
                            unsigned p = source[(sy + y - dy) * 256 + sx + x - dx];
                            if (!transparent || p != 0)
                                expected = format == 0
                                    ? original_rgb555_to_rgb565((uint16_t)p)
                                    : SDL_MapRGB(s->format,
                                        ((p >> 10) & 31) * 255 / 31,
                                        ((p >> 5) & 31) * 255 / 31,
                                        (p & 31) * 255 / 31);
                        }
                        assert(*pixel_at(s,x,y) == expected);
                    }
                }
            }
        }
        SDL_FreeSurface(s);
    }
}

static void check_original_catcher_scale(void)
{
    unsigned char data[512 + 16] = {0};
    Rich4SprSmpInfo spr = {0};
    Rich4SprSmpChunk c = {0};
    SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE,640,480,16,0xf800,0x07e0,0x001f,0);
    uint16_t red = 0x7c00, green = 0x03e0;
    assert(s);
    memcpy(spr.signature,"SPR",4); spr.start_offset = 0;
    memcpy(data+2,&red,2); memcpy(data+4,&green,2);
    memset(data+512,1,16); data[512] = 0; data[514] = 2;
    c.width = c.height = 4; c.x = 2; c.y = 3;
    c.data_offset = 512; c.data_size = 16;
    SDL_FillRect(s,NULL,0x5aa5);
    assert(draw_spr_chunk_scaled(s,data,sizeof(data),&spr,&c,100,100,32768));
    assert(*pixel_at(s,98,97) == 0x5aa5); /* Transparent source index zero. */
    assert(*pixel_at(s,99,97) == map_resource_pixel(s,green));
    assert(*pixel_at(s,98,98) == map_resource_pixel(s,red));
    assert(*pixel_at(s,100,98) == 0x5aa5); /* Half-sized, unscaled hotspot. */
    SDL_FillRect(s,NULL,0x5aa5);
    assert(draw_spr_chunk_scaled(s,data,sizeof(data),&spr,&c,1,100,32768));
    assert(*pixel_at(s,0,97) == map_resource_pixel(s,green));
    SDL_FreeSurface(s);
}

int main(void)
{
    uint16_t source_pixels[4] = {
        (uint16_t)(31u << 10), 0,
        (uint16_t)(31u << 5), (uint16_t)((31u << 10) | (31u << 5) | 31u)
    };
    Rich4SprSmpChunk chunk;
    SDL_Surface *surface;
    uint16_t red;
    uint16_t green;
    uint16_t white;

    check_clipped_color_blits();
    check_original_catcher_scale();
    check_original_sleeping_palette();

    memset(&chunk, 0, sizeof(chunk));
    chunk.width = 2;
    chunk.height = 2;
    chunk.data_size = sizeof(source_pixels);
    surface = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, 16,
                                   0xf800, 0x07e0, 0x001f, 0);
    assert(surface != NULL);
    {
        unsigned int color;
        SDL_Surface *other = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, 16,
                                                  0x7c00, 0x03e0, 0x001f, 0);
        assert(other != NULL);
        for (color = 0; color < 65536; color++) {
            Uint8 r = (Uint8)(((color >> 10) & 31) * 255 / 31);
            Uint8 g = (Uint8)(((color >> 5) & 31) * 255 / 31);
            Uint8 b = (Uint8)((color & 31) * 255 / 31);
            assert(map_resource_pixel(surface, (uint16_t)color) ==
                   original_rgb555_to_rgb565((uint16_t)color));
            assert(map_resource_pixel(other, (uint16_t)color) == SDL_MapRGB(other->format, r, g, b));
        }
        assert(map_resource_pixel(surface, (uint16_t)(17u << 5)) ==
               original_rgb555_to_rgb565((uint16_t)(17u << 5)));
        assert(map_resource_pixel(surface, (uint16_t)(18u << 5)) ==
               original_rgb555_to_rgb565((uint16_t)(18u << 5)));
        assert(map_resource_pixel(surface, (uint16_t)(31u << 5)) ==
               original_rgb555_to_rgb565((uint16_t)(31u << 5)));
        SDL_FreeSurface(other);
    }
    red = map_resource_pixel(surface, source_pixels[0]);
    green = map_resource_pixel(surface, source_pixels[2]);
    white = map_resource_pixel(surface, source_pixels[3]);
    memset(surface->pixels, 0, (size_t)surface->pitch * surface->h);

    assert(draw_chunk_topleft(surface, (const unsigned char *)source_pixels,
                              &chunk, 1, 1, 1) == 1);
    assert(*pixel_at(surface, 1, 1) == red);
    assert(*pixel_at(surface, 2, 1) == 0); /* transparent source pixel */
    assert(*pixel_at(surface, 1, 2) == green);
    assert(*pixel_at(surface, 2, 2) == white);

    assert(draw_chunk_crop_topleft(surface,
                                   (const unsigned char *)source_pixels,
                                   &chunk, 0, 1, 1, 1, 0, 0, 1) == 1);
    assert(*pixel_at(surface, 0, 0) == green);
    memset(surface->pixels, 0, (size_t)surface->pitch * surface->h);
    assert(draw_chunk_topleft(surface, (const unsigned char *)source_pixels,
                              &chunk, -1, -1, 0) == 1);
    assert(*pixel_at(surface, 0, 0) == white);
    memset(surface->pixels, 0, (size_t)surface->pitch * surface->h);
    assert(draw_chunk_at_jump_darken_minus20(
               surface, (const unsigned char *)source_pixels, &chunk,
               4, 4, 0) == 1);
    assert(*pixel_at(surface, 4, 4) ==
           map_resource_pixel_jump_darken_minus20(surface, source_pixels[0]));

    memset(surface->pixels, 0, (size_t)surface->pitch * surface->h);
    assert(draw_raw_rgb565_at(surface,
                              (const unsigned char *)source_pixels,
                              sizeof(source_pixels), 2, 2, 3, 2) == 1);
    assert(*pixel_at(surface, 3, 2) == red);
    assert(*pixel_at(surface, 4, 2) == 0);
    assert(*pixel_at(surface, 3, 3) == green);
    assert(*pixel_at(surface, 4, 3) == white);
    assert(*pixel_at(surface, 2, 2) == 0);

    {
        /* Raw rows at 0x485D68 +/- 4*32, independently read from IDA. */
        static const unsigned char plus4[32] = {
            4,5,6,7,7,8,9,10,11,12,13,14,14,15,16,17,
            18,19,20,21,21,22,23,24,25,26,27,28,28,29,30,31
        };
        static const unsigned char minus4[32] = {
            0,1,2,3,3,4,5,6,7,8,9,10,10,11,12,13,
            14,15,16,17,17,18,19,20,21,22,23,24,24,25,26,27
        };
        int c;
        for (c = 0; c < 32; c++) {
            uint16_t gray = (uint16_t)((c << 10) | (c << 5) | c);
            uint16_t p = map_resource_pixel(surface, gray);
            int up = plus4[c], down = minus4[c];
            assert(map_surface_pixel_property_flash(surface, p, 4) ==
                map_resource_pixel(surface, (uint16_t)((up << 10) | (up << 5) | up)));
            assert(map_surface_pixel_property_flash(surface, p, -4) ==
                map_resource_pixel(surface, (uint16_t)((down << 10) | (down << 5) | down)));
        }
    }
    SDL_FreeSurface(surface);
    printf("SDL1 blit kernels: ok\n");
    return 0;
}
