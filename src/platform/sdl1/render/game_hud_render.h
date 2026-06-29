#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_HUD_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_HUD_RENDER_H

#include <SDL/SDL.h>

#include "platform/sdl1/app_asset_types.h"
#include "view/game_view_model.h"

/* R10.1c: HUD cluster, moved verbatim out of runtime_main.c. Covers the top
 * panel button row, the resource/property info tabs, and the compact HUD
 * used in combined view; draw_game_layout is the single entry
 * point the event loop calls once per frame. */

void draw_game_layout(SDL_Surface *screen,
                      const GamePreviewAssets *assets,
                      const GameViewModel *view,
                      int view_mode);

/* Restore only the fixed top toolbar on a captured frame.  Modal panels use
 * this before saving their background so a stale hover chunk cannot become
 * part of the modal's restored image. */
void draw_game_top_panel_normal(SDL_Surface *screen,
                                const GamePreviewAssets *assets);

void draw_game_calendar(SDL_Surface *screen, const GamePreviewAssets *assets,
                        const GameViewModel *view, int monthly);

#endif
