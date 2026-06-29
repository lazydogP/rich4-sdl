#ifndef RICH4_PLATFORM_SDL1_GAME_FLOW_API_H
#define RICH4_PLATFORM_SDL1_GAME_FLOW_API_H

#include <SDL/SDL.h>

#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "app/game_speech_flow.h"
#include "core/game_command.h"
#include "core/game_state.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/runtime_options_types.h"

/* Narrow session-flow seams; feature state remains owned by GameSession. */
int game_update_property_speech(GameSession *session, Rich4Audio *audio,
                                Uint32 now);
int game_sync_player_actor_to_node(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, int player_index, int center_view);
void game_sync_confinement_event(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GameEvent *event);
void game_finish_control_locked_turn(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Uint32 now);
int game_center_view_on_player_status(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GamePlayerState *player);
int game_session_apply_command(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GameCommand *command, Uint32 now);
/* Save/load panel entry point. A successful cross-map load commits the saved
 * map identity together with the decoded session. */
int game_session_apply_command_with_map_switch(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, const GameCommand *command, Uint32 now);
int game_session_load_menu_slot(GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings, int slot);
const char *game_card_big5_name(int card);
const char *game_tool_big5_name(int tool);
const char *game_stock_name_for_state(const GameState *state, int stock);

#endif
