#ifndef RICH4_PLATFORM_SDL1_RUNTIME_INITIAL_SCENE_H
#define RICH4_PLATFORM_SDL1_RUNTIME_INITIAL_SCENE_H

#include <SDL/SDL.h>

#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/render/scene_menu_render.h"
#include "platform/sdl1/runtime_options_types.h"
#include "resource/mkf.h"

/* R10.1d/A12.2: first-frame setup retains one direct ownership seam. The
 * caller retains ownership of every object and surface; the callee receives
 * the objects it actually touches instead of pointer-only sub-contexts. */
typedef struct RuntimeInitialSceneContext {
    SDL_Surface *screen;
    SDL_Surface **background;
    SDL_Surface **cursor_free_frame;
    DrawSelection *selection;
    VirtualCursor *cursor;
    NewGameSettings *new_game_settings;
    GameSession *game_session;
    const SdlOptions *options;
    const Rich4Config *rich4_config;
    Rich4Audio *audio;
    Rich4Mkf *mkf;
    MenuAssets *menu_assets;
    CursorAssets *cursor_assets;
} RuntimeInitialSceneContext;

int game_prepare_initial_scene(RuntimeInitialSceneContext *context);

#endif
