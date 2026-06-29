#ifndef RICH4_PLATFORM_SDL1_TEXT_BIG5_UTF8_H
#define RICH4_PLATFORM_SDL1_TEXT_BIG5_UTF8_H

#include <stddef.h>
#include <stdint.h>

/* Convert original-game Big5/CP950 (CreateFontA charset 0x88 at 0x44FA74)
 * to UTF-8. One mapping table, no iconv and no OS code-page API.
 *
 * Returns non-zero on success and writes a NUL-terminated string.
 * Unmapped or truncated DBCS bytes become U+FFFD. */

int rich4_big5_to_utf8(const char *big5, char *out, size_t out_size);

/* Map one Big5/CP950 16-bit key (ASCII in the low byte when < 0x80).
 * Returns 0 when the DBCS pair is unmapped. */
uint16_t rich4_cp950_to_ucs2(uint16_t big5);

#endif
