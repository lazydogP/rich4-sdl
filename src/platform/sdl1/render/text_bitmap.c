#include <string.h>
#include <SDL/SDL.h>

#include "platform/sdl1/render/blit.h"

static void glyph_rows(char ch, unsigned char rows[7])
{
    static const unsigned char blank[7] = { 0, 0, 0, 0, 0, 0, 0 };
    static const unsigned char digit0[7] = { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e };
    static const unsigned char digit1[7] = { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e };
    static const unsigned char digit2[7] = { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f };
    static const unsigned char digit3[7] = { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e };
    static const unsigned char digit4[7] = { 0x11, 0x11, 0x11, 0x1f, 0x01, 0x01, 0x01 };
    static const unsigned char digit5[7] = { 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e };
    static const unsigned char digit6[7] = { 0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e };
    static const unsigned char digit7[7] = { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 };
    static const unsigned char digit8[7] = { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e };
    static const unsigned char digit9[7] = { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e };
    static const unsigned char a[7] = { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 };
    static const unsigned char b[7] = { 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e };
    static const unsigned char c[7] = { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e };
    static const unsigned char d[7] = { 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e };
    static const unsigned char e[7] = { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f };
    static const unsigned char f[7] = { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 };
    static const unsigned char g[7] = { 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f };
    static const unsigned char h[7] = { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 };
    static const unsigned char i[7] = { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f };
    static const unsigned char k[7] = { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 };
    static const unsigned char l[7] = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f };
    static const unsigned char m[7] = { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 };
    static const unsigned char n[7] = { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 };
    static const unsigned char o[7] = { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e };
    static const unsigned char p[7] = { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 };
    static const unsigned char q[7] = { 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d };
    static const unsigned char r[7] = { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 };
    static const unsigned char s[7] = { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e };
    static const unsigned char t[7] = { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };
    static const unsigned char u[7] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e };
    static const unsigned char v[7] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 };
    static const unsigned char w[7] = { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a };
    static const unsigned char x[7] = { 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 };
    static const unsigned char y[7] = { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 };
    const unsigned char *src = blank;

    switch (ch) {
    case '0': src = digit0; break;
    case '1': src = digit1; break;
    case '2': src = digit2; break;
    case '3': src = digit3; break;
    case '4': src = digit4; break;
    case '5': src = digit5; break;
    case '6': src = digit6; break;
    case '7': src = digit7; break;
    case '8': src = digit8; break;
    case '9': src = digit9; break;
    case 'A': src = a; break;
    case 'B': src = b; break;
    case 'C': src = c; break;
    case 'D': src = d; break;
    case 'E': src = e; break;
    case 'F': src = f; break;
    case 'G': src = g; break;
    case 'H': src = h; break;
    case 'I': src = i; break;
    case 'K': src = k; break;
    case 'L': src = l; break;
    case 'M': src = m; break;
    case 'N': src = n; break;
    case 'O': src = o; break;
    case 'P': src = p; break;
    case 'Q': src = q; break;
    case 'R': src = r; break;
    case 'S': src = s; break;
    case 'T': src = t; break;
    case 'U': src = u; break;
    case 'V': src = v; break;
    case 'W': src = w; break;
    case 'X': src = x; break;
    case 'Y': src = y; break;
    default: src = blank; break;
    }
    memcpy(rows, src, 7);
}

void draw_text(SDL_Surface *screen, int x, int y, const char *text, int scale, uint16_t color)
{
    int pen_x = x;
    while (*text != '\0') {
        unsigned char rows[7];
        int row;
        int col;
        if (*text == ' ') {
            pen_x += 4 * scale;
            text++;
            continue;
        }
        glyph_rows(*text, rows);
        for (row = 0; row < 7; row++) {
            for (col = 0; col < 5; col++) {
                if ((rows[row] & (1 << (4 - col))) != 0) {
                    fill_rect(screen, pen_x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        pen_x += 6 * scale;
        text++;
    }
}

