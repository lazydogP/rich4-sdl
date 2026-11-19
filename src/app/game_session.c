/* Purpose: own the app session lifecycle reset beside the session aggregate.
 * The operation clears only caller-owned app state; platform resources remain
 * outside this boundary. */

#include "app/game_session.h"

#include <string.h>

void rich4_app_reset_session(GameSession *session)
{
    if (session != NULL) memset(session, 0, sizeof(*session));
}

void rich4_app_reset_session_for_next_map(GameSession *session)
{
    uint8_t map_access_flags[GAME_MAPS_PER_STAGE];
    if (session == NULL) return;
    memcpy(map_access_flags, session->map_access_flags,
           sizeof(map_access_flags));
    memset(session, 0, sizeof(*session));
    memcpy(session->map_access_flags, map_access_flags,
           sizeof(map_access_flags));
}

int rich4_app_mark_map_access(GameSession *session, int map)
{
    if (session == NULL || map < 0 || map >= GAME_MAPS_PER_STAGE)
        return 0;
    session->map_access_flags[map] = 1;
    return 1;
}

unsigned int rich4_app_map_access_mask(const GameSession *session)
{
    unsigned int mask = 0;
    int map;
    if (session == NULL) return 0;
    for (map = 0; map < GAME_MAPS_PER_STAGE; map++)
        if (session->map_access_flags[map] != 0)
            mask |= 1u << map;
    return mask;
}

int rich4_app_all_maps_accessed(const GameSession *session)
{
    return rich4_app_map_access_mask(session) ==
        ((1u << GAME_MAPS_PER_STAGE) - 1u);
}

int rich4_app_ending_next_map_at(const GameSession *session, int x, int y)
{
    int map;
    if (session == NULL || x <= 200 || x >= 440 ||
        y <= 280 || y >= 440)
        return -1;
    map = (y - 280) / 40;
    if (map < 0 || map >= GAME_MAPS_PER_STAGE ||
        session->map_access_flags[map] != 0)
        return -1;
    return map;
}

int rich4_app_commit_ending_next_map(GameSession *session, int x, int y)
{
    int map = rich4_app_ending_next_map_at(session, x, y);
    if (map >= 0)
        session->state.map = map;
    return map;
}

void rich4_app_sync_rng_clock(GameSession *session,
    uint32_t original_tick, uint32_t frame_tick)
{
    if (session != NULL) session->rng_clock_offset = original_tick - frame_tick;
}

static int *time_machine_valid_slot(GameSession *session, int player)
{
    if (session == NULL || player < 0 || player >= GAME_MAX_PLAYERS)
        return NULL;
    return player == 0 ? &session->time_machine_valid
        : &session->time_machine_other[player - 1].valid;
}

int rich4_app_time_machine_capture(GameSession *session, int player)
{
    if (session == NULL || player < 0 ||
        player >= session->state.player_count || player >= GAME_MAX_PLAYERS ||
        (session->state.players[player].play_state & 1) == 0)
        return 0;
    if (player == 0) {
        session->time_machine_state = session->state;
        session->time_machine_flow = session->flow;
        session->time_machine_presentation = session->presentation;
        session->time_machine_valid = rich4_presentation_rebind_speech(
            &session->time_machine_presentation);
        return session->time_machine_valid;
    } else {
        GameTimeMachineSnapshot *snapshot =
            &session->time_machine_other[player - 1];
        snapshot->state = session->state;
        snapshot->flow = session->flow;
        memcpy(snapshot->actors, session->presentation.actors,
               sizeof(snapshot->actors));
        memcpy(snapshot->dynamic_objects,
               session->presentation.dynamic_objects,
               sizeof(snapshot->dynamic_objects));
        snapshot->valid = 1;
        return 1;
    }
}

int rich4_app_time_machine_get(const GameSession *session, int player,
                               const GameState **out_state,
                               const GameFlowState **out_flow,
                               const GameActorPresentationState **out_actors,
                               const GameMapDynamicObjectState **out_dynamic_objects)
{
    const GameTimeMachineSnapshot *snapshot;
    int valid;

    if (session == NULL || player < 0 || player >= GAME_MAX_PLAYERS ||
        out_state == NULL || out_flow == NULL || out_actors == NULL ||
        out_dynamic_objects == NULL)
        return 0;
    if (player == 0) {
        valid = session->time_machine_valid;
        *out_state = &session->time_machine_state;
        *out_flow = &session->time_machine_flow;
        *out_actors = session->time_machine_presentation.actors;
        *out_dynamic_objects =
            session->time_machine_presentation.dynamic_objects;
    } else {
        snapshot = &session->time_machine_other[player - 1];
        valid = snapshot->valid;
        *out_state = &snapshot->state;
        *out_flow = &snapshot->flow;
        *out_actors = snapshot->actors;
        *out_dynamic_objects = snapshot->dynamic_objects;
    }
    return valid == 1;
}

int rich4_app_time_machine_restore(GameSession *session, int player)
{
    const GameState *state;
    const GameFlowState *flow;
    const GameActorPresentationState *actors;
    const GameMapDynamicObjectState *dynamic_objects;
    uint32_t live_rng_state;

    if (!rich4_app_time_machine_get(session, player, &state, &flow, &actors,
                                    &dynamic_objects))
        return 0;
    live_rng_state = session->state.ai_rng_state;
    session->missile_scene_active = 0;
    session->state = *state;
    session->state.ai_rng_state = live_rng_state;
    session->flow = *flow;
    {
        int map_direction = session->presentation.map_direction;
        rich4_presentation_initialize(&session->presentation);
        session->presentation.map_direction = map_direction;
        memcpy(session->presentation.actors, actors,
               sizeof(session->presentation.actors));
        memcpy(session->presentation.dynamic_objects, dynamic_objects,
               sizeof(session->presentation.dynamic_objects));
    }
    return rich4_presentation_rebind_speech(&session->presentation);
}

int rich4_app_time_machine_rebind_all(GameSession *session)
{
    int player;
    if (session == NULL) return 0;
    for (player = 0; player < GAME_MAX_PLAYERS; player++) {
        int *valid = time_machine_valid_slot(session, player);
        if (*valid < 0 || *valid > 1) return 0;
        if (player == 0 && *valid &&
            !rich4_presentation_rebind_speech(
                &session->time_machine_presentation))
            return 0;
    }
    return 1;
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
