#include "platform/sdl1/game_lazy_assets.h"
#include <stdint.h>

#include "app/first_turn_flow.h"
#include "core/game_constants.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_turns.h"

/* R10.1d: first-turn landing animation and first-action dispatch state
 * machine, moved verbatim out of update_game_session. */
static int game_first_landing_character_index(const GamePlayerState *player,
                                              int fallback)
{
    int character = player != NULL ? player->character : fallback;
    if (character < 0 || character >= GAME_JUMP_CHARACTER_COUNT) {
        character = wrap_index(character, GAME_JUMP_CHARACTER_COUNT);
    }
    return character;
}

int game_first_landing_frame_count(const GamePreviewAssets *assets,
                                   const GamePlayerState *player,
                                   int fallback)
{
    int character = game_first_landing_character_index(player, fallback);
    if (assets == NULL || !game_asset_ready(assets, &assets->character.has_first_landing_graph[character])) {
        return GAME_LANDING_FRAMES;
    }
    {
        int frames = game_asset_graph_frames(assets, &assets->character.first_landing_data[character], assets->character.first_landing_size[character]);
        return frames > 0 ? frames : GAME_LANDING_FRAMES;
    }
}

int game_first_landing_delay_ms(const GamePreviewAssets *assets,
                                const GamePlayerState *player,
                                int fallback)
{
    int character = game_first_landing_character_index(player, fallback);
    if (assets == NULL || !game_asset_ready(assets, &assets->character.has_first_landing_graph[character])) {
        return GAME_LANDING_FRAME_MS;
    }
    return game_asset_graph_delay(assets, &assets->character.first_landing_data[character], assets->character.first_landing_size[character]);
}

int game_update_first_landing(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Uint32 now)
{
    GamePlayerState *player;
    Rich4FirstTurnPresentationRequest request;
    int current;
    int pending_player;
    int landed_player;
    int view_x;
    int view_y;
    int landed_x;
    int landed_y;
    int landed_direction;
    int landing_frames;
    int landing_delay_ms;

    if (session->flow.turn_state == GAME_TURN_STATE_LANDING) {
        current = wrap_index(session->state.current_player,
                             session->state.player_count);
        pending_player = session->flow.landing_pending_player - 1;
        landed_player = pending_player >= 0 &&
            pending_player < session->state.player_count ? pending_player : current;
        player = &session->state.players[current];
        game_route_node_xy_for_map(assets,
                                   settings,
                                   player->node,
                                   &view_x, &view_y);
        landing_frames = game_first_landing_frame_count(
            assets, player, current);
        landing_delay_ms = game_first_landing_delay_ms(assets, player, current);
        player = &session->state.players[landed_player];
        game_route_node_xy_for_map(assets, settings, player->node,
                                   &landed_x, &landed_y);
        landed_direction = game_route_direction8_for_nodes(
            assets, settings, player->node, player->next_node);
        now = rich4_sdl1_ticks();
        if (!rich4_app_update_first_landing(
                session, current, landed_player, view_x, view_y,
                landed_x, landed_y, landed_direction, landing_frames,
                landing_delay_ms, now, &request))
            return 0;
        if (request == RICH4_FIRST_TURN_PRESENTATION_CONTROL_LOCKED) {
            game_finish_control_locked_turn(session, assets, settings, now);
        }
        return 1;
    }
    return 0;
}
