#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_FLC_CACHE_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_FLC_CACHE_H

#include "platform/sdl1/app_asset_types.h"
#include "resource/flc_player.h"

typedef struct GameFlcCacheStats {
    size_t live_bytes, peak_bytes;
    unsigned long hits, misses, decoded_frames, rewinds, evictions;
} GameFlcCacheStats;

/* The single SDL runtime owns this bounded derived cache. Only bytes leased
 * by owner may be retained. Unowned buffers keep immediate-call lifetime. */
void game_flc_cache_begin_frame(const GamePreviewAssets *owner);
void game_flc_cache_clear(void);
void game_flc_cache_release_resource(const unsigned char *data);
Rich4FlcPlayer *game_flc_cache_decode(const unsigned char *data, size_t size,
    int frame_index);
GameFlcCacheStats game_flc_cache_stats(void);

#endif
