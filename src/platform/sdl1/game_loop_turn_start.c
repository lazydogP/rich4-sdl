#include "app/turn_start_flow.h"
#include "app/dice_flow.h"
#include "app/turn_status_flow.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/runtime_render_api.h"

/* R10.1d: turn-start status gate and turn-flow entry, moved verbatim out of
 * update_game_session. The subsequent route/dice/movement states remain in
 * their dedicated modules. */
void game_apply_turn_start_request(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Uint32 now,
    const Rich4TurnStartRequest *request)
{
    if (session == NULL || request == NULL ||
        request->action == RICH4_TURN_START_NONE ||
        request->action == RICH4_TURN_START_STARTED)
        return;
    if (request->action == RICH4_TURN_START_CONFINEMENT_STATUS)
        rich4_app_begin_confinement_status_presentation(
            session, request->player_index, request->status_slot, now);
    else if (request->action == RICH4_TURN_START_CONTROL_LOCKED)
        game_finish_control_locked_turn(session, assets, settings, now);
    else if (request->action == RICH4_TURN_START_AI)
        game_run_ai_preactions(
            session, assets, settings, request->player_index, now);
    else if (request->action == RICH4_TURN_START_HUMAN)
        rich4_dice_show_action_panel(
            session, now, 0, request->status_result == 1);
}
