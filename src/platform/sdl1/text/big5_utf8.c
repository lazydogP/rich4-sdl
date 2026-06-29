#include "platform/sdl1/text/big5_utf8.h"

#define RICH4_CP950_LEAD_MIN 0x81u
#define RICH4_CP950_LEAD_MAX 0xfeu
#define RICH4_CP950_TRAIL_COUNT 157
#define RICH4_CP950_TRAIL_ASCII 63
#define RICH4_U_REPLACEMENT 0xfffdu

#include "cp950_ucs2.inc"

static int trail_index(unsigned char trail)
{
    if (trail >= 0x40 && trail <= 0x7e) {
        return (int)(trail - 0x40);
    }
    if (trail >= 0xa1 && trail <= 0xfe) {
        return RICH4_CP950_TRAIL_ASCII + (int)(trail - 0xa1);
    }
    return -1;
}

static size_t encode_utf8(uint16_t cp, char *out, size_t cap)
{
    if (cp < 0x80) {
        if (cap < 1) {
            return 0;
        }
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        if (cap < 2) {
            return 0;
        }
        out[0] = (char)(0xc0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3f));
        return 2;
    }
    if (cap < 3) {
        return 0;
    }
    out[0] = (char)(0xe0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[2] = (char)(0x80 | (cp & 0x3f));
    return 3;
}

uint16_t rich4_cp950_to_ucs2(uint16_t big5)
{
    unsigned char lead;
    unsigned char trail;
    int tindex;

    if (big5 < 0x80) {
        return big5;
    }
    lead = (unsigned char)(big5 >> 8);
    trail = (unsigned char)(big5 & 0xff);
    if (lead < RICH4_CP950_LEAD_MIN || lead > RICH4_CP950_LEAD_MAX) {
        return 0;
    }
    tindex = trail_index(trail);
    if (tindex < 0) {
        return 0;
    }
    return rich4_cp950_ucs2[(lead - RICH4_CP950_LEAD_MIN) * RICH4_CP950_TRAIL_COUNT
                            + tindex];
}

int rich4_big5_to_utf8(const char *big5, char *out, size_t out_size)
{
    const unsigned char *src;
    size_t used = 0;

    if (big5 == NULL || out == NULL || out_size < 1) {
        return 0;
    }
    src = (const unsigned char *)big5;
    while (*src != 0) {
        uint16_t cp;
        size_t n;

        if (*src < 0x80) {
            cp = *src;
            src++;
        } else {
            unsigned char lead = *src++;
            unsigned char trail = *src;
            uint16_t mapped = 0;
            if (trail != 0) {
                src++;
                mapped = rich4_cp950_to_ucs2((uint16_t)((lead << 8) | trail));
            }
            cp = mapped != 0 ? mapped : RICH4_U_REPLACEMENT;
        }
        n = encode_utf8(cp, out + used, out_size - 1 - used);
        if (n == 0) {
            return 0;
        }
        used += n;
    }
    out[used] = '\0';
    return 1;
}
