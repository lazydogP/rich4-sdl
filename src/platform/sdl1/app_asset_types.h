#ifndef RICH4_PLATFORM_SDL1_APP_ASSET_TYPES_H
#define RICH4_PLATFORM_SDL1_APP_ASSET_TYPES_H

#include <stddef.h>

#include "platform/sdl1/app_asset_packs.h"
#include "resource/mkf.h"

/* SDL-owned resource views used by the menu, cursor and help adapters. The
 * resource store owns the bytes; these records only borrow their handles. */
typedef struct MenuAssets {
    Rich4ResourceHandle resource;
    unsigned char *data;
    size_t size;
    Rich4SprSmpInfo spr;
    int available;
} MenuAssets;

typedef struct CursorAssets {
    Rich4ResourceHandle resource;
    unsigned char *data;
    size_t size;
    Rich4SprSmpInfo spr;
    int available;
    size_t chunk_index;
} CursorAssets;

typedef struct HelpAssets {
    ResourceStore *store;
    Rich4Mkf *mkf;
    Rich4ResourceHandle ui_resource;
    const unsigned char *ui_data;
    size_t ui_size;
    Rich4SprSmpInfo ui_spr;
    int available;
} HelpAssets;

typedef struct GamePreviewAssets {
    struct GameLazyAssets *lazy; /* mutable resource leases behind read-only views */
    ResourceStore *resource_store; /* borrowed; outlives all handles below */
    const char *data_dir; /* borrowed startup path for optional AVI playback */
    Rich4ResourceHandle *borrowed_resources;
    size_t borrowed_resource_count;
    size_t borrowed_resource_capacity;
    /* Scope 0 is the explicit complete inventory used by asset audits.
     * Runtime scopes own leases: menu (1), session (2), setup (3), options (4). */
    int load_scope;
    int active_background;
    unsigned int active_characters;
    Rich4Mkf *data_archive, *jump_archive, *panel_archive, *map_archive;
    GameIntroAssetPack intro;
    GameMapAssetPack map;
    GameCharacterAssetPack character;
    GameMenuAssetPack menu;
    GameUiAssetPack ui;
    GameToolAssetPack tool;
} GamePreviewAssets;

#endif
