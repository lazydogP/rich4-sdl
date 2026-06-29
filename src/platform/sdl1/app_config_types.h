#ifndef RICH4_PLATFORM_SDL1_APP_CONFIG_TYPES_H
#define RICH4_PLATFORM_SDL1_APP_CONFIG_TYPES_H

#include "app/config_types.h"
#include "data/music.h"

/* SDL-only edge scroll tuning.  This is measured after host-window pointer
 * normalization, in the fixed 640x480 logical game coordinate system. */
#define RICH4_SDL_EDGE_SCROLL_INSET_DEFAULT 16
#define RICH4_SDL_EDGE_SCROLL_INSET_MAX 200

typedef struct Rich4SdlConfig {
    Rich4MusicBank music_bank;
    int edge_scroll_inset;
} Rich4SdlConfig;

#endif
