#include <stdlib.h>
#include <string.h>

#include "platform/sdl1/render/game_flc_cache.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime_trace.h"

enum { FLC_CACHE_SLOTS = 8, FLC_CACHE_MAX_BYTES = 8 * 1024 * 1024 };
typedef struct FlcCacheEntry {
    Rich4FlcPlayer player;
    size_t bytes;
    size_t archive_id, resource_index;
    unsigned long used_frame, used_order;
} FlcCacheEntry;

static FlcCacheEntry entries[FLC_CACHE_SLOTS];
static const GamePreviewAssets *cache_owner;
static unsigned long cache_frame, cache_order;
static GameFlcCacheStats stats;

static void free_entry(FlcCacheEntry *entry)
{
    free(entry->player.pixels);
    free(entry->player.indices);
    free(entry->player.dirty);
    stats.live_bytes -= entry->bytes;
    memset(entry, 0, sizeof(*entry));
}

void game_flc_cache_clear(void)
{
    int slot;
    for (slot = 0; slot < FLC_CACHE_SLOTS; slot++) free_entry(&entries[slot]);
    cache_owner = NULL;
    cache_frame = cache_order = 0;
}

void game_flc_cache_release_resource(const unsigned char *data)
{
    int slot;
    for (slot = 0; slot < FLC_CACHE_SLOTS; slot++)
        if (entries[slot].bytes && entries[slot].player.data == data)
            free_entry(&entries[slot]);
}

void game_flc_cache_begin_frame(const GamePreviewAssets *owner)
{
    int slot;
    if (cache_owner != owner) game_flc_cache_clear();
    cache_owner = owner;
    cache_frame++;
    for (slot = 0; slot < FLC_CACHE_SLOTS; slot++) {
        /* Retire animations absent for two complete frames. */
        if (entries[slot].bytes != 0 &&
            cache_frame - entries[slot].used_frame > 2) free_entry(&entries[slot]);
    }
}

Rich4FlcPlayer *game_flc_cache_decode(const unsigned char *data, size_t size,
    int frame_index)
{
    FlcCacheEntry *entry = NULL;
    Rich4FlcHeader header;
    Rich4ResourceHandle handle;
    Rich4FlcError error;
    size_t count, bytes;
    int slot, decoded = 0;
    if (cache_owner == NULL) return NULL;
    for (slot = 0; slot < FLC_CACHE_SLOTS; slot++) {
        if (entries[slot].bytes != 0 && entries[slot].player.data == data &&
            entries[slot].player.size == size) { entry = &entries[slot]; break; }
    }
    if (entry == NULL) {
        handle = game_preview_handle_for_borrowed_view(cache_owner, data);
        if (!rich4_resource_handle_is_valid(&handle) || handle.size != size ||
            rich4_flc_parse_header(data, size, &header) != RICH4_FLC_OK) return NULL;
        count = (size_t)header.width * header.height;
        if (count == 0 || count > FLC_CACHE_MAX_BYTES / 4u) return NULL;
        bytes = count * 4u;
        while (1) {
            FlcCacheEntry *oldest = NULL;
            entry = NULL;
            for (slot = 0; slot < FLC_CACHE_SLOTS; slot++) {
                if (entries[slot].bytes == 0) entry = &entries[slot];
                else if (oldest == NULL || entries[slot].used_order < oldest->used_order)
                    oldest = &entries[slot];
            }
            if (entry != NULL && stats.live_bytes + bytes <= FLC_CACHE_MAX_BYTES) break;
            if (oldest == NULL) return NULL;
            free_entry(oldest);
            stats.evictions++;
        }
        entry->player.pixels = calloc(count, sizeof(uint16_t));
        entry->player.indices = calloc(count, 1);
        entry->player.dirty = calloc(count, 1);
        if (entry->player.pixels == NULL || entry->player.indices == NULL ||
            entry->player.dirty == NULL) { free_entry(entry); return NULL; }
        error = rich4_flc_player_init(&entry->player, data, size,
            entry->player.pixels, entry->player.indices, entry->player.dirty, count, 0);
        if (error != RICH4_FLC_OK) { free_entry(entry); return NULL; }
        entry->bytes = bytes;
        entry->archive_id = handle.archive_id;
        entry->resource_index = handle.resource_index;
        stats.live_bytes += bytes;
        if (stats.live_bytes > stats.peak_bytes) stats.peak_bytes = stats.live_bytes;
        stats.misses++;
    } else stats.hits++;
    if (frame_index < entry->player.current_frame) stats.rewinds++;
    error = rich4_flc_player_decode_to(&entry->player, frame_index, &decoded);
    stats.decoded_frames += (unsigned long)decoded;
    if (decoded > 0) runtime_profile_flc_frame(entry->archive_id,
        entry->resource_index, frame_index, decoded);
    if (error != RICH4_FLC_OK) { free_entry(entry); return NULL; }
    entry->used_frame = cache_frame;
    entry->used_order = ++cache_order;
    return &entry->player;
}

GameFlcCacheStats game_flc_cache_stats(void) { return stats; }
