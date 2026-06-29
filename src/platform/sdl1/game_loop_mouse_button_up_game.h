#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_MOUSE_BUTTON_UP_GAME_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_MOUSE_BUTTON_UP_GAME_H

#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"

/* R10.1d: in-game SDL_MOUSEBUTTONUP cleanup and visit dispatch moved out of
 * runtime_main.c; modal early exits remain at the event-loop boundary. */
typedef struct GameMouseButtonUpGameContext {
    Rich4SceneId app_state;
    GameSession *session;
    const GamePreviewAssets *assets;
    NewGameSettings *settings;
    VirtualCursor *cursor;
    Rich4Audio *audio;
    int button;
    int *mouse_left_down;
    int *shop_music_restore_pending;
} GameMouseButtonUpGameContext;

void game_handle_mouse_button_up_game(
    GameMouseButtonUpGameContext *context);

#endif
