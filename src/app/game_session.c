/* Purpose: own the app session lifecycle reset beside the session aggregate.
 * The operation clears only caller-owned app state; platform resources remain
 * outside this boundary. */

#include "app/game_session.h"

#include <string.h>

void rich4_app_reset_session(GameSession *session)
{
    if (session != NULL) memset(session, 0, sizeof(*session));
}

void rich4_app_sync_rng_clock(GameSession *session,
    uint32_t original_tick, uint32_t frame_tick)
{
    if (session != NULL) session->rng_clock_offset = original_tick - frame_tick;
}

void rich4_app_request_lifecycle(GameSession *session,
                                 Rich4AppLifecycleCode code, int player)
{
    if (session == NULL || code <= RICH4_APP_LIFECYCLE_NONE ||
        code > RICH4_APP_LIFECYCLE_LOAD_GAME ||
        session->lifecycle.code != RICH4_APP_LIFECYCLE_NONE)
        return;
    session->lifecycle.code = code;
    session->lifecycle.player = player;
}

int rich4_app_take_lifecycle(GameSession *session,
                             Rich4AppLifecycleRequest *out_request)
{
    if (session == NULL || out_request == NULL ||
        session->lifecycle.code == RICH4_APP_LIFECYCLE_NONE)
        return 0;
    *out_request = session->lifecycle;
    session->lifecycle.code = RICH4_APP_LIFECYCLE_NONE;
    session->lifecycle.player = -1;
    return 1;
}

int rich4_app_begin_ending_load(GameSession *session,
                                int *out_previous_player)
{
    int player;
    if (session == NULL || out_previous_player == NULL ||
        (!session->state.game_over &&
         !session->presentation.ending.continuation_active) ||
        !session->presentation.ending.active)
        return 0;
    *out_previous_player = session->state.current_player >= 0 &&
        session->state.current_player < session->state.player_count
        ? session->state.current_player : -1;
    for (player = 0; player < session->state.player_count; player++) {
        if (!session->state.players[player].is_ai) {
            session->state.current_player = player;
            return 1;
        }
    }
    return 0;
}

void rich4_app_cancel_ending_load(GameSession *session,
                                  int previous_player)
{
    if (session != NULL && previous_player >= 0 &&
        previous_player < session->state.player_count)
        session->state.current_player = previous_player;
}

void rich4_app_set_ending_load_previous_player(GameSession *session,
                                                int previous_player)
{
    if (session != NULL)
        session->presentation.save_load.terminal_previous_player =
            previous_player;
}
