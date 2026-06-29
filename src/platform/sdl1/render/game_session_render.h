#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_SESSION_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_SESSION_RENDER_H

#include <SDL/SDL.h>

#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/game_render_cache.h"
#include "platform/sdl1/render/game_scene_view.h"

/* R10.1c: the per-frame active-game composition pass, moved verbatim out of
 * runtime_main.c. Reads only GameSceneView + assets/settings; dispatches to
 * the already-extracted map/minimap/dice/intro/route renderers and draws the
 * remaining inline pieces (route dots in small-map mode, player markers,
 * steps-remaining digits) that were never split into their own module. */

/* 0x409971: common Data[517] remaining-step digits for all actors. */
void draw_game_steps_remaining(SDL_Surface *screen,
                                const GamePreviewAssets *assets,
                                const GameViewModel *view);

void draw_game_session(SDL_Surface *screen,
                       const GamePreviewAssets *assets,
                       const NewGameSettings *settings,
                       GameSceneView *scene,
                       GameRenderCache *cache);

/* Resource-backed terminal overlay. The core has already committed the
 * winner; this renderer only consumes the immutable scene snapshot. */
void draw_game_ending_presentation(SDL_Surface *screen,
                                   const GamePreviewAssets *assets,
                                   const GameSceneView *scene);

/* Resource-backed 0x407842 continuation/load prompt. */
void draw_game_continuation_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSceneView *scene);

void draw_game_ending_confirmation_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSceneView *scene);

#endif
