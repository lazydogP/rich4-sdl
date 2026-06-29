#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_INTRO_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_INTRO_RENDER_H

#include <SDL/SDL.h>

#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_intro_render_view.h"
#include "view/game_view_model.h"

/* R10.1c: JUMP.MKF session-intro (airplane takeoff / parachute landing)
 * fullscreen cutscene, moved verbatim out of runtime_main.c. The shared
 * frame/stage math (game_intro_stage_for_frame)
 * now lives beside the session updater in game_loop_intro.c; both updater and
 * renderer call the same game_asset_api.h-exported helpers. */

int draw_game_intro_fullscreen(SDL_Surface *screen,
                               const GamePreviewAssets *assets,
                               const GameIntroRenderView *view);

/* Airborne (in-flight takeoff / parachute-landing) player overlay drawn on
 * top of the main map / minimap once the fullscreen intro has ended but the
 * player's landing FLC is still playing. Shared with draw_game_session in
 * render/game_session_render.c. */
int draw_game_airborne_player(SDL_Surface *screen,
                              const GamePreviewAssets *assets,
                              const GameViewModel *view,
                              int player_index,
                              int target_x,
                              int target_y,
                              int frame,
                              int frames,
                              int show_aircraft,
                              int use_main_view);

#endif
