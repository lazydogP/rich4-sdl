#ifndef RICH4_PLATFORM_SDL1_PIXEL_ROWS_H
#define RICH4_PLATFORM_SDL1_PIXEL_ROWS_H

#include <stdint.h>
#if defined(RICH4_PLATFORM_MIYOO) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif

/* Original sub_451801, n2_6 == 1: direct RGB555 -> RGB565 bit transfer,
 * including bit 15 being ignored for color but NOT for transparent-zero
 * detection. */
static inline void rich4_rgb555_row(uint16_t *dst, const uint16_t *src,
                                   int count, int transparent)
{
    int x = 0;
#if defined(RICH4_PLATFORM_MIYOO) && defined(__ARM_NEON)
    for (; x + 8 <= count; x += 8) {
        uint16x8_t p = vld1q_u16(src + x);
        uint16x8_t g = vandq_u16(vshrq_n_u16(p, 5), vdupq_n_u16(31));
        uint16x8_t color = vorrq_u16(
            vshlq_n_u16(vandq_u16(p, vdupq_n_u16(0x7c00)), 1),
            vorrq_u16(vshlq_n_u16(g, 6), vandq_u16(p, vdupq_n_u16(31))));
        if (transparent)
            color = vbslq_u16(vceqq_u16(p, vdupq_n_u16(0)),
                               vld1q_u16(dst + x), color);
        vst1q_u16(dst + x, color);
    }
#endif
    for (; x < count; x++) {
        unsigned int p = src[x];
        if (!transparent || p != 0)
            dst[x] = (uint16_t)(((p & 0x7c00u) << 1) |
                ((p & 0x03e0u) << 1) | (p & 0x001fu));
    }
}

/* Presentation uses zero-filled low bits, not full-range color expansion.
 * Reverse just this row; the caller selects the vertically reversed row. */
static inline void rich4_present_rgb888_row(uint32_t *dst, const uint16_t *src,
                                           int count, int reverse, uint32_t alpha)
{
    int x = 0;
#if defined(RICH4_PLATFORM_MIYOO) && defined(__ARM_NEON)
    for (; x + 8 <= count; x += 8) {
        uint16x8_t p = vld1q_u16(src + x);
        uint32x4_t lo, hi, a = vdupq_n_u32(alpha);
        uint32_t *out = dst + (reverse ? count - x - 8 : x);
        if (reverse) {
            p = vrev64q_u16(p);
            p = vextq_u16(p, p, 4);
        }
        lo = vmovl_u16(vget_low_u16(p));
        hi = vmovl_u16(vget_high_u16(p));
        vst1q_u32(out, vorrq_u32(a, vorrq_u32(
            vshlq_n_u32(vandq_u32(lo, vdupq_n_u32(0xf800)), 8),
            vorrq_u32(vshlq_n_u32(vandq_u32(lo, vdupq_n_u32(0x07e0)), 5),
                       vshlq_n_u32(vandq_u32(lo, vdupq_n_u32(31)), 3)))));
        vst1q_u32(out + 4, vorrq_u32(a, vorrq_u32(
            vshlq_n_u32(vandq_u32(hi, vdupq_n_u32(0xf800)), 8),
            vorrq_u32(vshlq_n_u32(vandq_u32(hi, vdupq_n_u32(0x07e0)), 5),
                       vshlq_n_u32(vandq_u32(hi, vdupq_n_u32(31)), 3)))));
    }
#endif
    for (; x < count; x++) {
        uint32_t p = src[x];
        dst[reverse ? count - 1 - x : x] = alpha |
            ((p & 0xf800u) << 8) | ((p & 0x07e0u) << 5) | ((p & 31u) << 3);
    }
}

#endif
