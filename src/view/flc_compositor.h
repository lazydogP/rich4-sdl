#ifndef RICH4_VIEW_FLC_COMPOSITOR_H
#define RICH4_VIEW_FLC_COMPOSITOR_H

#include <stddef.h>
#include <stdint.h>

void rich4_flc_composite_game(const unsigned char *indices,
                              const uint16_t *flc_pixels,
                              const uint16_t *source_pixels,
                              uint16_t *out_pixels,
                              size_t pixel_count,
                              int index_zero_uses_source);

#endif
