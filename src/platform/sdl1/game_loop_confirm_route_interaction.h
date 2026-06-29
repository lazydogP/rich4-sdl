#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_ROUTE_INTERACTION_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_ROUTE_INTERACTION_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"

/* R10.1d: route-interaction confirmation activation now owns the initial
 * virtual-cursor placement and its hover seed as well as button dispatch. */
int game_sync_route_interaction_initial_cursor(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, VirtualCursor *cursor,
    int route_interaction_was_active);

/* R10.1d: route-interaction confirmation dispatch moved out of
 * runtime_main.c. */
int game_handle_confirm_route_interaction(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings,
    VirtualCursor *cursor, Rich4Audio *audio, int mouse_left_down);

/* R10.1d: route-interaction virtual-cancel dispatch moved into the same
 * route-interaction seam; all three route cancel modes keep their original
 * command/audio timing. */
int game_handle_cancel_route_interaction(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings,
    VirtualCursor *cursor, Rich4Audio *audio);

/* R10.1d: physical release of the route confirmation dialog shares the
 * existing hit/command seam and keeps pointer-confirm flags local to the
 * release event. */
int game_handle_route_interaction_mouse_button_up(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings,
    VirtualCursor *cursor, Rich4Audio *audio, int button);

#endif
