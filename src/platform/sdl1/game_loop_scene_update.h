#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_SCENE_UPDATE_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_SCENE_UPDATE_H

#include <SDL/SDL.h>

#include "app/audio_event.h"
#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/scene.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/game_loop_scene_presentation.h"
#include "platform/sdl1/game_loop_preview_update.h"
#include "resource/mkf.h"
#include "platform/sdl1/render/game_render_cache.h"

typedef struct GameSceneIntroSurfaceContext {
    SDL_Surface *screen;
    VirtualCursor *cursor;
    GameRenderCache *render_cache;
} GameSceneIntroSurfaceContext;

typedef struct GameSceneIntroContentContext {
    Rich4Mkf *mkf;
    GamePreviewAssets *assets;
    NewGameSettings *settings;
    GameSession *session;
    const Rich4Config *config;
    const Rich4SdlConfig *sdl_config;
} GameSceneIntroContentContext;

typedef struct GameSceneIntroAudioContext {
    const char *music_dir;
    Rich4Audio *audio;
    int *main_map_music_track;
    int *main_map_music_active;
} GameSceneIntroAudioContext;

typedef struct GameSceneIntroTransitionContext {
    Rich4SceneMachine *machine;
    int *trace_count;
    Rich4SetupTransitionInput setup_transition_input;
} GameSceneIntroTransitionContext;

typedef struct GameSceneIntroFrameContext {
    GameSceneIntroSurfaceContext *surface;
    GameSceneIntroContentContext *content;
    GameSceneIntroAudioContext *audio;
    GameSceneIntroTransitionContext *transition;
    Uint32 now;
} GameSceneIntroFrameContext;

typedef struct GameSceneSetupStateContext {
    const GamePreviewAssets *assets;
    NewGameSettings *settings;
    VirtualCursor *cursor;
} GameSceneSetupStateContext;

typedef struct GameSceneSetupFrameContext {
    SDL_Surface *screen;
    GameSceneSetupStateContext *state;
    Rich4SceneMachine *machine;
    Rich4Audio *audio;
    Uint32 now;
} GameSceneSetupFrameContext;

/* R10.1d/A12.2: top-level scene-frame update dispatch moved out of
 * runtime_main.c. Each scene-specific helper keeps its existing ownership;
 * the payload is a tagged-by-scene union so a frame cannot expose all four
 * unrelated scene lifetimes through one aggregate. */
typedef union GameSceneFramePayload {
    GamePreviewFrameContext *game;
    GameSceneIntroFrameContext *intro;
    GameSceneSetupFrameContext *setup;
    GameSceneFallbackFrameContext *fallback;
} GameSceneFramePayload;

void game_update_scene_frame(Rich4SceneMachine *machine,
                             GameSceneFramePayload *payload);

#endif
