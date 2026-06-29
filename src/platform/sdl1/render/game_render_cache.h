#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_RENDER_CACHE_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_RENDER_CACHE_H

#include <SDL/SDL.h>

/* Platform-owned derived surfaces used by the dice renderer. The session
 * aggregate carries only a borrowed pointer; lifecycle and destruction stay here. */
typedef struct GameRenderCache {
    SDL_Surface *dice_graph_surface, *dice_source_surface;
    int dice_rendered_frame;
    int dice_surface_resource;
    int dice_surface_x, dice_surface_y;
    int dice_result_composited;
    SDL_Surface *property_flash_base;
    unsigned char *property_flash_mask;
} GameRenderCache;

#endif
