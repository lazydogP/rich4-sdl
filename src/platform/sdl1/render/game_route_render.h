#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_ROUTE_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_ROUTE_RENDER_H

#include <SDL/SDL.h>

#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/game_scene_view.h"

/* R10.1c: route-tile interaction dialogs (purchase/upgrade confirm, facility
 * worker choice, bank dispatch) and property presentation / player speech
 * overlays, moved verbatim out of runtime_main.c. Read-only over GameSceneView
 * + assets; dispatches bank pages to game_bank_render. */

void draw_facility_choice_selector(SDL_Surface *screen,
                                   const GamePreviewAssets *assets,
                                   int hover);

void draw_facility_research_selector(SDL_Surface *screen,
                                     const GamePreviewAssets *assets,
                                     int hover, int choice_count);

void draw_game_route_interaction(SDL_Surface *screen,
                                 const GamePreviewAssets *assets,
                                 const GameSceneView *scene);

void draw_game_property_presentation(SDL_Surface *screen,
                                     const GamePreviewAssets *assets,
                                     const GameSceneView *scene);

/* Speech is normally part of the route/property pass.  Terminal presentation
 * redraws over that pass, so it reuses this focused overlay afterwards. */
void draw_game_speech_overlay(SDL_Surface *screen,
                              const GamePreviewAssets *assets,
                              const GameSceneView *scene);

#endif
