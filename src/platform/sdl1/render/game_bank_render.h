#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_BANK_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_BANK_RENDER_H

#include <SDL/SDL.h>

#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/game_scene_view.h"

/* R10.1c: in-game bank interaction UI (transfer/pass-stop page and the
 * loan/special-finance page), moved verbatim out of runtime_main.c. Reads only
 * the bank_* fields of GameSceneView plus Panel.mkf assets. */

void draw_game_bank_interaction(SDL_Surface *screen,
                                const GamePreviewAssets *assets,
                                const GameSceneView *scene);
/* Shared original 0x453544 number panel; caller supplies its draggable origin. */
void draw_game_quantity_panel(SDL_Surface *screen, const GamePreviewAssets *assets,
    int amount, int maximum, int pressed_control, int x, int y);

#endif
