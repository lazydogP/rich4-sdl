#ifndef RICH4_RESOURCE_FLC_PLAYER_H
#define RICH4_RESOURCE_FLC_PLAYER_H

#include <stdint.h>

#include "resource/flc_animation.h"
#include "api_contract.h"

typedef struct Rich4FlcPlayer {
    Rich4FlcDecoder decoder;
    RICH4_BORROWED_LIFETIME const unsigned char *data;
    size_t size;
    RICH4_CALLER_STORAGE uint16_t *pixels;
    RICH4_CALLER_STORAGE unsigned char *indices;
    RICH4_CALLER_STORAGE unsigned char *dirty;
    size_t pixel_count;
    int current_frame;
    Rich4FlcFrame frame;
    int has_frame;
    uint32_t delay_ms;
    uint32_t next_frame_time;
    int clock_started;
} Rich4FlcPlayer;

Rich4FlcError rich4_flc_player_init(Rich4FlcPlayer *player,
                                    const unsigned char *data,
                                    size_t size,
                                    uint16_t *pixels,
                                    unsigned char *indices,
                                    unsigned char *dirty,
                                    size_t pixel_capacity,
                                    uint32_t delay_override_ms);
Rich4FlcError rich4_flc_player_seek(Rich4FlcPlayer *player, int frame_index);
/* Decode forward from retained state; repeated frames do no work. A rewind
 * resets pixels, indices and palette before replaying the requested frame. */
Rich4FlcError rich4_flc_player_decode_to(Rich4FlcPlayer *player,
    int frame_index, int *out_decoded);
Rich4FlcError rich4_flc_player_advance(Rich4FlcPlayer *player,
                                       uint32_t now,
                                       int *out_advanced);

#endif
