#include "view/flc_compositor.h"

void rich4_flc_composite_game(const unsigned char *indices,
                              const uint16_t *flc_pixels,
                              const uint16_t *source_pixels,
                              uint16_t *out_pixels,
                              size_t pixel_count,
                              int index_zero_uses_source)
{
    size_t i;
    if (indices == NULL || flc_pixels == NULL || out_pixels == NULL) return;
    for (i = 0; i < pixel_count; i++) {
        if (index_zero_uses_source && indices[i] == 0 && source_pixels != NULL)
            out_pixels[i] = source_pixels[i];
        else
            out_pixels[i] = flc_pixels[i];
    }
}
