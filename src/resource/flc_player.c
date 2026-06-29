#include "resource/flc_player.h"

#include <string.h>

static Rich4FlcError player_reset_decoder(Rich4FlcPlayer *player)
{
    Rich4FlcError error = rich4_flc_decoder_init(&player->decoder,
                                                  player->data,
                                                  player->size,
                                                  player->pixels,
                                                  player->pixel_count);
    if (error != RICH4_FLC_OK) return error;
    rich4_flc_decoder_set_indices(&player->decoder, player->indices, player->pixel_count);
    rich4_flc_decoder_set_dirty(&player->decoder, player->dirty, player->pixel_count);
    player->current_frame = -1;
    player->has_frame = 0;
    return RICH4_FLC_OK;
}

Rich4FlcError rich4_flc_player_init(Rich4FlcPlayer *player,
                                    const unsigned char *data,
                                    size_t size,
                                    uint16_t *pixels,
                                    unsigned char *indices,
                                    unsigned char *dirty,
                                    size_t pixel_capacity,
                                    uint32_t delay_override_ms)
{
    Rich4FlcHeader header;
    Rich4FlcError error;
    size_t count;
    if (player == NULL || pixels == NULL || indices == NULL || dirty == NULL)
        return RICH4_FLC_ERR_ARGUMENT;
    error = rich4_flc_parse_header(data, size, &header);
    if (error != RICH4_FLC_OK) return error;
    count = (size_t)header.width * header.height;
    if (pixel_capacity < count) return RICH4_FLC_ERR_RANGE;
    memset(player, 0, sizeof(*player));
    player->data = data;
    player->size = size;
    player->pixels = pixels;
    player->indices = indices;
    player->dirty = dirty;
    player->pixel_count = count;
    player->delay_ms = delay_override_ms != 0 ? delay_override_ms : header.delay_ms;
    if (player->delay_ms == 0) player->delay_ms = 1;
    return player_reset_decoder(player);
}

Rich4FlcError rich4_flc_player_seek(Rich4FlcPlayer *player, int frame_index)
{
    Rich4FlcError error;
    int i;
    if (player == NULL || frame_index < 0 ||
        frame_index >= player->decoder.header.frame_count) return RICH4_FLC_ERR_RANGE;
    error = player_reset_decoder(player);
    if (error != RICH4_FLC_OK) return error;
    for (i = 0; i <= frame_index; i++) {
        error = rich4_flc_decode_next(&player->decoder, &player->frame);
        if (error != RICH4_FLC_OK) return error;
        player->current_frame = i;
        player->has_frame = 1;
    }
    return RICH4_FLC_OK;
}

Rich4FlcError rich4_flc_player_decode_to(Rich4FlcPlayer *player,
    int frame_index, int *out_decoded)
{
    Rich4FlcError error;
    if (out_decoded != NULL) *out_decoded = 0;
    if (player == NULL || frame_index < 0 ||
        frame_index >= player->decoder.header.frame_count) return RICH4_FLC_ERR_RANGE;
    if (frame_index < player->current_frame) {
        error = player_reset_decoder(player);
        if (error != RICH4_FLC_OK) return error;
    }
    /* 0x450F04 advances dword_476378 and retains the delta/palette state. */
    while (player->current_frame < frame_index) {
        error = rich4_flc_decode_next(&player->decoder, &player->frame);
        if (error != RICH4_FLC_OK) return error;
        player->current_frame++;
        player->has_frame = 1;
        if (out_decoded != NULL) (*out_decoded)++;
    }
    return RICH4_FLC_OK;
}

Rich4FlcError rich4_flc_player_advance(Rich4FlcPlayer *player,
                                       uint32_t now,
                                       int *out_advanced)
{
    Rich4FlcError error;
    if (out_advanced != NULL) *out_advanced = 0;
    if (player == NULL) return RICH4_FLC_ERR_ARGUMENT;
    if (player->clock_started && now < player->next_frame_time) return RICH4_FLC_OK;
    error = rich4_flc_decode_next(&player->decoder, &player->frame);
    if (error != RICH4_FLC_OK) return error;
    player->current_frame++;
    player->has_frame = 1;
    player->clock_started = 1;
    player->next_frame_time = now + player->delay_ms;
    if (out_advanced != NULL) *out_advanced = 1;
    return RICH4_FLC_OK;
}
