#include "core/player_status.h"

#include <string.h>

#include "core/game_event_payload.h"

static int push_status_event(GameEventQueue *events,
                             Rich4GameEventConstructor constructor,
                             int player, int subject)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = subject;
    fields.value = 0;
    fields.value2 = 0;
    fields.flags = 0u;
    fields.subject_ref.kind = GAME_DOMAIN_NONE;
    fields.subject_ref.id = -1;
    fields.target_ref.kind = GAME_DOMAIN_NONE;
    fields.target_ref.id = -1;
    return rich4_game_event_emit_typed(events, constructor, fields);
}

int rich4_player_status_evaluate(const GameState *state,
                                 int player_index,
                                 int a1,
                                 PlayerStatusDecision *out_decision)
{
    PlayerStatusDecision decision;
    memset(&decision, 0, sizeof(decision));
    decision.status_slot = -1;
    if (state == NULL || out_decision == NULL || player_index < 0 || player_index >= 9) {
        return 0;
    }
    if (player_index >= GAME_MAX_PLAYERS) {
        int slot = player_index - GAME_MAX_PLAYERS;
        if (slot >= GAME_SPECIAL_PLAYER_COUNT) return 0;
        if (!a1 && state->special_player_status[slot][0] == 0 &&
            state->special_player_status[slot][1] == 0 &&
            state->special_player_status[slot][2] == 0) decision.result = 2;
        *out_decision = decision;
        return 1;
    }
    {
        const GamePlayerState *player = &state->players[player_index];
        int play_state = player->play_state & 0xff;
        int block[4];
        int i;
        int first_status = -1;
        block[0] = player->delayed_status_block & 0xff;
        block[1] = player->days_disappearing & 0xff;
        block[2] = player->days_in_prison & 0xff;
        block[3] = player->days_in_hospital & 0xff;
        for (i = 0; i < 4; i++) {
            if (block[i] != 0 && first_status < 0) first_status = i;
        }
        if (play_state == 0) {
            *out_decision = decision;
            return 1;
        }
        if (a1) {
            if ((play_state & 0x30) == 0 && first_status < 0 &&
                (player->days_sleeping & 0xff) == 0) decision.result = play_state;
        } else if (first_status >= 0 || (player->days_sleeping & 0xff) != 0) {
            decision.status_slot = first_status >= 0 ? 50 + first_status : 54;
            if ((play_state & 0x30) != 0) {
                decision.events = PLAYER_STATUS_EVENT_ACTOR_REFRESH;
            } else {
                decision.events = PLAYER_STATUS_EVENT_SCENE_REFRESH |
                                  PLAYER_STATUS_EVENT_TEXT;
                if (block[2] != 0 || block[3] != 0 ||
                    (first_status < 0 && (player->days_sleeping & 0xff) != 0)) {
                    decision.events |= PLAYER_STATUS_EVENT_VOICE;
                }
            }
        } else if ((player->days_sleep_walking & 0xff) != 0) {
            decision.result = -1;
            decision.status_slot = 55;
            decision.events = PLAYER_STATUS_EVENT_ACTOR_REFRESH;
        } else {
            decision.result = play_state;
        }
    }
    *out_decision = decision;
    return 1;
}

int rich4_player_status_evaluate_events(const GameState *state,
                                        int player_index,
                                        int a1,
                                        PlayerStatusDecision *out_decision,
                                        GameEventQueue *events)
{
    PlayerStatusDecision decision;
    unsigned int emitted = 0;
    if (!rich4_player_status_evaluate(state, player_index, a1, &decision)) return 0;
    if ((decision.events & PLAYER_STATUS_EVENT_ACTOR_REFRESH) != 0)
        emitted += push_status_event(
            events, rich4_game_event_actor_refresh_requested_from_fields,
            player_index, decision.status_slot);
    if ((decision.events & PLAYER_STATUS_EVENT_SCENE_REFRESH) != 0)
        emitted += push_status_event(
            events, rich4_game_event_scene_refresh_requested_from_fields,
            player_index, decision.status_slot);
    if ((decision.events & PLAYER_STATUS_EVENT_TEXT) != 0)
        emitted += push_status_event(
            events, rich4_game_event_text_requested_from_fields,
            player_index, decision.status_slot);
    if ((decision.events & PLAYER_STATUS_EVENT_VOICE) != 0)
        emitted += push_status_event(
            events, rich4_game_event_voice_requested_from_fields,
            player_index, decision.status_slot);
    if (out_decision != NULL) *out_decision = decision;
    return events == NULL || emitted > 0 || decision.events == PLAYER_STATUS_EVENT_NONE;
}
