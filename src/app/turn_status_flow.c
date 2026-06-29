#include "app/turn_status_flow.h"

#include <string.h>

#include "app/game_speech_flow.h"
#include "core/confinement.h"
#include "core/facility.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/rng.h"
#include "core/status_countdown.h"
#include "core/victory.h"
#include "data/character_speech.h"
#include "view/game_view_model.h"

static void clear_player_god_status(GameSession *session, int player_index)
{
    GamePlayerState *player;
    int linked_index;

    if (session == NULL || player_index < 0 ||
        player_index >= session->state.player_count)
        return;
    player = &session->state.players[player_index];
    linked_index = player->linked_player;
    player->god_status_days = 0;
    player->linked_player = -1;
    if (linked_index >= 0 && linked_index < session->state.player_count) {
        session->state.players[linked_index].linked_player = -1;
        session->state.players[linked_index].god_status_days = 0;
    }
}

static int apply_relation_balance(GameSession *session, int source_player,
                                  int target_player, int delta)
{
    GamePlayerState *source;
    int value;

    if (session == NULL || source_player < 0 ||
        source_player >= session->state.player_count || target_player < 0 ||
        target_player >= GAME_PLAYER_RELATION_BALANCE_COUNT ||
        source_player == target_player)
        return 0;
    source = &session->state.players[source_player];
    if (delta < 0 && source->relation_balance[target_player] == 0)
        return 0;
    value = source->relation_balance[target_player] + delta;
    if (value < 0) value = 0;
    source->relation_balance[target_player] = value;
    session->debug.god_balance_tick_count++;
    session->debug.last_god_balance_source = source_player;
    session->debug.last_god_balance_target = target_player;
    session->debug.last_god_balance_delta = delta;
    session->debug.last_god_balance_applied = 1;
    if (delta > 0 && source->linked_player == target_player)
        clear_player_god_status(session, source_player);
    return 1;
}

static void tick_god_relation_balance(GameSession *session, int player_index)
{
    GamePlayerState *player;
    int value;
    int linked_index;
    int price_index;
    int delta;

    if (session == NULL || player_index < 0 ||
        player_index >= session->state.player_count)
        return;
    player = &session->state.players[player_index];
    value = player->god_status_days & 0xff;
    if (value == 0 || (value & GAME_STATUS_PENDING_BIT) != 0)
        return;
    linked_index = player->linked_player;
    if (linked_index < 0 || linked_index >= session->state.player_count)
        return;
    price_index = session->state.price_index > 0 ? session->state.price_index : 1;
    delta = -(price_index * 20);
    session->debug.last_god_balance_source = -1;
    session->debug.last_god_balance_target = -1;
    session->debug.last_god_balance_delta = delta;
    session->debug.last_god_balance_applied = 0;
    apply_relation_balance(session, player_index, linked_index, delta);
    apply_relation_balance(session, linked_index, player_index, delta);
}

void rich4_app_update_player_status_countdowns(GameSession *session,
                                               int player_index)
{
    GamePlayerState *player;
    StatusTickResult tick_result;
    GameEventQueue events;
    GameEvent event;

    if (session == NULL || player_index < 0 ||
        player_index >= session->state.player_count)
        return;
    player = &session->state.players[player_index];
    rich4_game_event_queue_reset(&events);
    rich4_status_tick_player_events(player, player_index, &tick_result, &events);
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_STATUS_TICK_REQUESTED)
            tick_god_relation_balance(session, event.player);
    }
    rich4_game_event_queue_reset(&events);
    if (rich4_facility_tick_research(&session->state, player_index, &events)) {
        while (rich4_game_event_queue_pop(&events, &event)) {
            if (event.type == GAME_EVENT_INVENTORY_CHANGED ||
                event.type == GAME_EVENT_STATUS_CHANGED)
                session->presentation.actors[player_index].dirty_flags |=
                    GAME_PLAYER_DIRTY_TURN;
        }
    }
}

static void record_status_event(GameSession *session, int player_index,
                                int original_offset, int handler)
{
    int index;

    if (session == NULL) return;
    index = session->debug.status_event_processed_count;
    if (index < GAME_MAX_STATUS_EVENT_RECORDS) {
        session->debug.status_event_players[index] = player_index;
        session->debug.status_event_offsets[index] = original_offset;
        session->debug.status_event_handlers[index] = handler;
    }
    session->debug.status_event_processed_count++;
    session->debug.last_status_event_player = player_index;
    session->debug.last_status_event_offset = original_offset;
    session->debug.last_status_event_handler = handler;
}

static void refresh_status_actor(GameSession *session, int player_index,
                                 int status_offset)
{
    GamePlayerState *player;
    GameActorPresentationState *actor;
    int node;
    int target;
    int x;
    int y;
    int dx;
    int dy;

    if (session == NULL || player_index < 0 ||
        player_index >= session->state.player_count)
        return;
    player = &session->state.players[player_index];
    actor = &session->presentation.actors[player_index];
    actor->dirty_flags |= GAME_PLAYER_DIRTY_TURN;
    node = player->node;
    if (node <= 0 || node >= session->state.tool_route_node_count)
        return;

    /* The original handlers refresh the actor resource/direction after
     * changing the status.  Route coordinates are the app's copied form of
     * the original route table, so this remains deterministic and does not
     * make the core depend on SDL assets. */
    if (status_offset == 50) {
        x = actor->visual_x;
        y = actor->visual_y;
        dx = session->state.tool_route_x[node] - x;
        dy = session->state.tool_route_y[node] - y;
        if (dx == 0 && dy == 0) {
            target = player->next_node;
            if (target <= 0 || target >= session->state.tool_route_node_count)
                target = 0;
            if (target != 0) {
                dx = session->state.tool_route_x[target] -
                     session->state.tool_route_x[node];
                dy = session->state.tool_route_y[target] -
                     session->state.tool_route_y[node];
            }
        }
    } else {
        target = player->next_node;
        if (target <= 0 || target >= session->state.tool_route_node_count)
            target = 0;
        if (target != 0) {
            dx = session->state.tool_route_x[target] -
                 session->state.tool_route_x[node];
            dy = session->state.tool_route_y[target] -
                 session->state.tool_route_y[node];
        } else {
            dx = 0;
            dy = 0;
        }
    }
    if (dx != 0 || dy != 0)
        actor->direction = rich4_game_actor_direction_from_delta(dx, dy);
}

void rich4_app_capture_game_ended(GameSession *session,
                                  const GameEvent *event)
{
    int winner;

    if (session == NULL || event == NULL ||
        event->type != GAME_EVENT_GAME_ENDED ||
        session->presentation.ending.active)
        return;
    winner = event->player;
    session->state.winner_player = winner;
    session->state.winner_reason =
        rich4_game_event_game_ended_reason(event);
    session->state.game_over = 1;
    session->presentation.ending.active = 1;
    session->presentation.ending.winner_player = winner;
    session->presentation.ending.winner_character =
        winner >= 0 && winner < session->state.player_count
            ? session->state.players[winner].character : -1;
    session->presentation.ending.reason =
        rich4_game_event_game_ended_reason(event);
    session->presentation.ending.assets =
        rich4_game_event_game_ended_assets(event);
    session->presentation.ending.ending_video_pending = 1;
    session->presentation.ending.winner_voice_pending = 1;
    session->presentation.ending.continuation_started = 0;
    session->presentation.ending.jump_transition_active = 0;
    session->presentation.ending.jump_transition_code =
        RICH4_APP_LIFECYCLE_NONE;
    session->presentation.ending.jump_transition_started = 0;
    session->presentation.ending.continuation_confirmation_active = 0;
    session->presentation.ending.continuation_confirmation_ready = 0;
    session->presentation.ending.continuation_confirmation_started = 0;
}

int rich4_app_process_pending_status_events(GameSession *session,
                                            int player_index)
{
    StatusEventBatch batch;
    GameEventQueue events;
    GameEvent event;

    if (session == NULL || player_index < 0 ||
        player_index >= session->state.player_count)
        return 0;
    session->debug.status_event_processed_count = 0;
    session->debug.last_status_event_player = -1;
    session->debug.last_status_event_offset = 0;
    session->debug.last_status_event_handler = GAME_STATUS_EVENT_HANDLER_NONE;
    memset(session->debug.status_event_players, 0xff,
           sizeof(session->debug.status_event_players));
    memset(session->debug.status_event_offsets, 0,
           sizeof(session->debug.status_event_offsets));
    memset(session->debug.status_event_handlers, 0,
           sizeof(session->debug.status_event_handlers));
    rich4_game_event_queue_reset(&events);
    rich4_status_consume_pending_events(&session->state, player_index,
                                        &batch, &events);
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_STATUS_EFFECT_RESOLVED)
            record_status_event(
                session, event.player,
                rich4_game_event_status_effect_handler(&event),
                rich4_game_event_status_effect_result(&event));
        else if (event.type == GAME_EVENT_ACTOR_DIRTY_REQUESTED ||
                 event.type == GAME_EVENT_ACTOR_REFRESH_REQUESTED)
            refresh_status_actor(
                session, event.player,
                event.type == GAME_EVENT_ACTOR_DIRTY_REQUESTED
                    ? rich4_game_event_subject(&event) : -1);
    }
    return batch.count;
}

void rich4_app_update_win_candidate(GameSession *session)
{
    GameEventQueue events;
    GameEvent event;

    if (session == NULL || !session->state.active ||
        session->state.player_count <= 0)
        return;
    rich4_game_event_queue_reset(&events);
    if (!rich4_victory_finalize_events(
            &session->state, &session->flow.victory_result, &events))
        return;
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_VICTORY_CANDIDATE) {
            session->state.win_candidate_player = event.player;
            session->state.win_candidate_reason =
                rich4_game_event_victory_reason(&event);
            session->state.win_candidate_assets =
                rich4_game_event_victory_assets(&event);
        } else if (event.type == GAME_EVENT_GAME_ENDED) {
            rich4_app_capture_game_ended(session, &event);
        }
    }
}

void rich4_app_update_special_player_status_countdowns(GameSession *session,
                                                       int slot)
{
    int i;

    if (session == NULL || slot < 0 || slot >= GAME_SPECIAL_PLAYER_COUNT)
        return;
    session->debug.last_special_status_original_turn_index = -1;
    session->debug.last_special_status_actor_refreshed = 0;
    for (i = 0; i < 4; i++) {
        int value = session->state.special_player_status[slot][i] & 0xff;
        if ((value & GAME_STATUS_PENDING_BIT) != 0) {
            session->state.special_player_status[slot][i] = 0;
            if (i == 0) {
                session->state.special_player_play_state[slot] &=
                    ~GAME_PLAYER_PLAY_STATE_FROZEN;
                session->debug.last_special_status_original_turn_index =
                    slot + GAME_MAX_PLAYERS;
                session->debug.last_special_status_actor_refreshed = 1;
            }
            session->state.special_player_status_pending_events++;
            session->debug.last_special_status_slot = slot;
            session->debug.last_special_status_index = i;
            session->debug.last_special_status_handler =
                i == 0 ? GAME_STATUS_EVENT_HANDLER_40B8D8_40B93B :
                         GAME_STATUS_EVENT_HANDLER_CLEAR_ONLY;
            continue;
        }
        if (value == 0) continue;
        value--;
        if (value == 0) value = GAME_STATUS_PENDING_BIT;
        session->state.special_player_status[slot][i] = value;
    }
}

int rich4_app_begin_confinement_status_presentation(
    GameSession *session, int player_index, int status_slot, uint32_t now)
{
    const GamePlayerState *player;
    const Rich4CharacterSpeech *speech;
    int counter;

    if (session == NULL || player_index < 0 ||
        player_index >= session->state.player_count ||
        (status_slot != 50 && status_slot != 52 && status_slot != 53))
        return 0;
    player = &session->state.players[player_index];
    counter = status_slot == 50 ? player->delayed_status_block : status_slot == 52
        ? player->days_in_prison : player->days_in_hospital;
    session->presentation.confinement_status.active = 1;
    session->presentation.confinement_status.player = player_index;
    session->presentation.confinement_status.slot = status_slot;
    session->presentation.confinement_status.days = (counter & 0x7f) + 1;
    /* The 1500 ms countdown begins after the optional player_say modal. */
    session->presentation.confinement_status.until = 0;
    if (status_slot == 50) return 1; /* 0x40C9D2 has no confinement speech. */
    if ((rich4_rng_next_at(&session->state.ai_rng_state) & 1u) == 0) return 1;
    speech = rich4_character_confinement_speech(
        player->character, status_slot == 53);
    game_start_speech_entry(
        session, player_index, speech, 2,
        GAME_PROPERTY_PRESENT_NONE, now);
    return 1;
}

int rich4_app_update_confinement_status_presentation(
    GameSession *session, uint32_t now, int speech_playing)
{
    GameConfinementStatusPresentationState *status;

    if (session == NULL || !session->presentation.confinement_status.active)
        return 0;
    status = &session->presentation.confinement_status;
    if (speech_playing) return 1;
    if (status->until == 0) {
        status->until = now + 1500;
        return 1;
    }
    if ((int32_t)(now - status->until) < 0) return 1;
    return 2;
}
