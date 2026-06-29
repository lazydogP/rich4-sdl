#ifndef RICH4_GROUND_RASTERIZER_H
#define RICH4_GROUND_RASTERIZER_H

/*
 * The original 0x4557A1/0x45596A ground rasterizer.  Keep this in a small
 * header because the SDL renderer and the pixel-equivalence reference must
 * execute the same integer algorithm.
 *
 * The original routine keeps four global scanline tables.  These locals are
 * their per-tile equivalent:
 *   x_min/x_max    16.16 screen-space edge positions
 *   uv_left        texture values written by a descending edge
 *   uv_right       texture values written by an ascending edge
 *
 * The signed divisions deliberately follow the assembly's order.  In
 * particular, the U/V quotient is divided by the signed original dy, while
 * the X quotient is divided by abs(dy); the signed division remainder is
 * placed in the initial U/V accumulator.
 */

#include <stdint.h>
#include <string.h>
#include <SDL/SDL.h>

typedef struct Rich4GroundScanline {
    int32_t x_min;
    int32_t x_max;
    int32_t u_left;
    int32_t v_left;
    int32_t u_right;
    int32_t v_right;
} Rich4GroundScanline;

static inline int32_t rich4_ground_idiv(int32_t numerator,
                                         int32_t divisor,
                                         int32_t *remainder)
{
    int32_t quotient = numerator / divisor;
    *remainder = numerator % divisor;
    return quotient;
}

static inline void rich4_ground_add_edge(
    Rich4GroundScanline *scanlines,
    int x0,
    int y0,
    int x1,
    int y1,
    int32_t uv0,
    int32_t uv1,
    int32_t uv_delta,
    int draw_top,
    int draw_bottom)
{
    int signed_dy = y1 - y0;
    int start_x = x0;
    int end_x = x1;
    int start_y = y0;
    int end_y = y1;
    int32_t start_uv = uv0;
    int count;
    int offset;
    int32_t du;
    int32_t dv;
    int32_t u_step;
    int32_t v_step;
    int32_t x_step;
    int32_t u_remainder;
    int32_t v_remainder;
    int32_t x_acc;
    int32_t u_acc;
    int32_t v_acc;
    int y;
    int descending;

    /* fcn_0045596a calculates dy in a signed 16-bit register. */
    signed_dy = (int16_t)signed_dy;
    if (signed_dy == 0)
        return;

    descending = signed_dy < 0;
    if (descending) {
        start_x = x1;
        end_x = x0;
        start_y = y1;
        end_y = y0;
        start_uv = uv1;
    }

    if (end_y <= draw_top || start_y >= draw_bottom)
        return;

    count = descending ? -signed_dy : signed_dy;
    du = (int16_t)((uint32_t)uv_delta >> 16);
    dv = (int16_t)((uint32_t)uv_delta & 0xffffu);

    /* The assembly does these two idiv operations before abs(dy). */
    u_step = rich4_ground_idiv(du * 65536, signed_dy, &u_remainder);
    v_step = rich4_ground_idiv(dv * 65536, signed_dy, &v_remainder);

    x_acc = (int32_t)start_x * 65536;
    x_step = ((int32_t)(end_x - start_x) * 65536) /
        (descending ? -signed_dy : signed_dy);

    u_acc = (int32_t)((uint32_t)start_uv & 0xffff0000u) + u_remainder;
    v_acc = (int32_t)(((uint32_t)start_uv & 0xffffu) << 16) + v_remainder;

    /* Clip the same half-open scanline range as the original helper. */
    if (start_y < draw_top) {
        offset = draw_top - start_y;
        count -= offset;
        x_acc += offset * x_step;
        u_acc += offset * u_step;
        v_acc += offset * v_step;
        start_y = draw_top;
    }
    if (end_y > draw_bottom)
        count -= end_y - draw_bottom;
    if (count <= 0)
        return;

    for (y = start_y; y < start_y + count; y++) {
        Rich4GroundScanline *line = &scanlines[y];

        if (x_acc < line->x_min) {
            line->x_min = x_acc;
            if (descending) {
                line->u_left = u_acc;
                line->v_left = v_acc;
            } else {
                line->u_right = u_acc;
                line->v_right = v_acc;
            }
        }
        if (x_acc > line->x_max) {
            line->x_max = x_acc;
            if (descending) {
                line->u_left = u_acc;
                line->v_left = v_acc;
            } else {
                line->u_right = u_acc;
                line->v_right = v_acc;
            }
        }

        x_acc += x_step;
        u_acc += u_step;
        v_acc += v_step;
    }
}

static inline void rich4_draw_ground_tile_original(
    SDL_Surface *screen,
    int p00_x,
    int p00_y,
    int p10_x,
    int p10_y,
    int p01_x,
    int p01_y,
    int p11_x,
    int p11_y,
    const unsigned char *src,
    const uint16_t *palette,
    unsigned char *coverage)
{
    Rich4GroundScanline scanlines[480];
    const int draw_left = 0;
    const int draw_top = 40;
    const int draw_right = 440;
    const int draw_bottom = 480;
    const int32_t uv00 = 0;
    const int32_t uv10 = 0x1f0000;
    const int32_t uv11 = 0x1f001f;
    const int32_t uv01 = 0x1f;
    const int32_t duv0 = 0x200000;
    const int32_t duv1 = 0x20;
    const int32_t duv2 = (int32_t)0xffe00000u;
    const int32_t duv3 = (int32_t)0xffe0u;
    int min_y;
    int y;

    if (screen == NULL || src == NULL || palette == NULL)
        return;

    for (y = 0; y < 480; y++) {
        scanlines[y].x_min = (int32_t)0x7fff0000u;
        scanlines[y].x_max = (int32_t)0x80000000u;
        scanlines[y].u_left = 0;
        scanlines[y].v_left = 0;
        scanlines[y].u_right = 0;
        scanlines[y].v_right = 0;
    }

    rich4_ground_add_edge(scanlines,
                          p00_x, p00_y, p10_x, p10_y,
                          uv00, uv10, duv0,
                          draw_top, draw_bottom);
    rich4_ground_add_edge(scanlines,
                          p10_x, p10_y, p11_x, p11_y,
                          uv10, uv11, duv1,
                          draw_top, draw_bottom);
    rich4_ground_add_edge(scanlines,
                          p11_x, p11_y, p01_x, p01_y,
                          uv11, uv01, duv2,
                          draw_top, draw_bottom);
    rich4_ground_add_edge(scanlines,
                          p01_x, p01_y, p00_x, p00_y,
                          uv01, uv00, duv3,
                          draw_top, draw_bottom);

    min_y = draw_bottom;
    for (y = draw_top; y < draw_bottom; y++) {
        if (scanlines[y].x_min != (int32_t)0x7fff0000u) {
            min_y = y;
            break;
        }
    }

    for (y = min_y; y < draw_bottom; y++) {
        Rich4GroundScanline *line = &scanlines[y];
        int left;
        int right;
        int width;
        int clip_left;
        int32_t du_dx;
        int32_t dv_dx;
        int32_t u_acc;
        int32_t v_acc;
        uint16_t *row;
        int x;

        if (line->x_min == (int32_t)0x7fff0000u)
            break;

        left = line->x_min >> 16;
        right = line->x_max >> 16;
        width = right - left;
        if (width <= 0 || right <= draw_left || left >= draw_right)
            continue;

        du_dx = (line->u_right - line->u_left) / width;
        dv_dx = (line->v_right - line->v_left) / width;
        u_acc = line->u_left;
        v_acc = line->v_left;

        if (left < draw_left) {
            clip_left = draw_left - left;
            width -= clip_left;
            u_acc += clip_left * du_dx;
            v_acc += clip_left * dv_dx;
            left = draw_left;
        }
        if (right > draw_right)
            width -= right - draw_right;
        if (width <= 0)
            continue;

        row = (uint16_t *)((unsigned char *)screen->pixels +
                           y * screen->pitch);
        if (coverage != NULL)
            memset(coverage + (y - draw_top) * 440 + left, 1, (size_t)width);

        for (x = 0; x < width; x++) {
            int u = ((uint32_t)u_acc >> 16) & 0x1f;
            int v = ((uint32_t)v_acc >> 16) & 0x1f;
            row[left + x] = palette[src[v * 32 + u]];
            u_acc += du_dx;
            v_acc += dv_dx;
        }
    }
}

#endif
