#include "core/status_countdown.h"

#include "core/game_event_payload.h"

static int push_status_countdown_event(
    GameEventQueue *events, Rich4GameEventConstructor constructor,
    int player, int subject, int value, int value2)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = subject;
    fields.value = value;
    fields.value2 = value2;
    fields.flags = 0u;
    fields.subject_ref.kind = GAME_DOMAIN_NONE;
    fields.subject_ref.id = -1;
    fields.target_ref.kind = GAME_DOMAIN_NONE;
    fields.target_ref.id = -1;
    return rich4_game_event_emit_typed(events, constructor, fields);
}

#include <string.h>

static int tick_regular(int value)
{
    value &= 0xff;
    if ((value & GAME_STATUS_PENDING_BIT) != 0 || value == 0) return value;
    value--;
    return value == 0 ? GAME_STATUS_PENDING_BIT : value;
}

static int tick_disappearing(int value)
{
    value &= 0xff;
    if ((value & GAME_STATUS_PENDING_BIT) != 0 || value == 0) return value;
    value--;
    if ((value & 0x3f) == 0) value |= GAME_STATUS_PENDING_BIT;
    return value;
}

static void tick_field(int *field,
                       int offset,
                       int disappearing,
                       StatusTickResult *result)
{
    int before = *field & 0xff;
    int after = disappearing ? tick_disappearing(before) : tick_regular(before);
    *field = after;
    if (before != after && result != NULL && result->count < GAME_MAX_STATUS_EVENT_RECORDS) {
        result->offsets[result->count] = offset;
        result->values[result->count++] = after;
    }
}

static int delayed_block(const GamePlayerState *player)
{
    return player->delayed_status_block != 0 || player->days_disappearing != 0 ||
           player->days_in_prison != 0 || player->days_in_hospital != 0;
}

void rich4_status_tick_player(GamePlayerState *player, StatusTickResult *out_result)
{
    StatusTickResult result;
    memset(&result, 0, sizeof(result));
    if (player == NULL) {
        if (out_result != NULL) *out_result = result;
        return;
    }
    tick_field(&player->delayed_status_block, 50, 0, &result);
    tick_field(&player->days_disappearing, 51, 1, &result);
    tick_field(&player->days_in_prison, 52, 0, &result);
    tick_field(&player->days_in_hospital, 53, 0, &result);
    if (!delayed_block(player)) {
        tick_field(&player->days_sleeping, 54, 0, &result);
        tick_field(&player->days_sleep_walking, 55, 0, &result);
        tick_field(&player->days_tortoise_walking, 57, 0, &result);
    }
    tick_field(&player->days_stopping, 56, 0, &result);
    tick_field(&player->days_rejected_by_bank, 59, 0, &result);
    tick_field(&player->days_bank_no_loans, 60, 0, &result);
    result.god_relation_tick_requested =
        (player->god_status_days & 0xff) != 0 &&
        (player->god_status_days & GAME_STATUS_PENDING_BIT) == 0;
    tick_field(&player->god_status_days, 61, 0, &result);
    tick_field(&player->days_assurance, 62, 0, &result);
    if (out_result != NULL) *out_result = result;
}

int rich4_status_player_has_pending(const GamePlayerState *player)
{
    if (player == NULL) return 0;
    return ((player->delayed_status_block | player->days_disappearing |
             player->days_in_prison | player->days_in_hospital |
             player->days_sleeping | player->days_sleep_walking |
             player->days_stopping | player->days_tortoise_walking |
             player->days_rejected_by_bank | player->days_bank_no_loans |
             player->god_status_days | player->days_assurance) &
            GAME_STATUS_PENDING_BIT) != 0;
}

static void add_event(StatusEventBatch *batch,
                      int offset,
                      StatusHandlerFamily handler,
                      StatusEffectResult effect)
{
    if (batch->count >= GAME_MAX_STATUS_EVENT_RECORDS) return;
    batch->events[batch->count].original_offset = offset;
    batch->events[batch->count].handler = handler;
    batch->events[batch->count].effect_result = effect;
    batch->count++;
}

static void consume(int *field,
                    int offset,
                    StatusHandlerFamily handler,
                    StatusEffectResult effect,
                    StatusEventBatch *batch)
{
    if ((*field & GAME_STATUS_PENDING_BIT) == 0) return;
    *field = 0;
    add_event(batch, offset, handler, effect);
}

int rich4_status_consume_pending(GameState *state,
                                 int player_index,
                                 StatusEventBatch *out_batch)
{
    GamePlayerState *player;
    StatusEventBatch batch;
    memset(&batch, 0, sizeof(batch));
    if (state == NULL || out_batch == NULL || player_index < 0 ||
        player_index >= state->player_count) return 0;
    player = &state->players[player_index];
    if ((player->delayed_status_block & GAME_STATUS_PENDING_BIT) != 0) {
        player->delayed_status_block = 0;
        /* 0x40D6BE sets +21 bit 0x10 so the next rotation clears the
         * transient normal-state flags before ordinary control resumes. */
        player->play_state |= GAME_PLAYER_PLAY_STATE_STATUS_RETURN;
        add_event(&batch, 50, STATUS_HANDLER_40D6BE,
                  STATUS_EFFECT_APPLIED);
    }
    consume(&player->days_disappearing, 51, STATUS_HANDLER_40D4E5,
            STATUS_EFFECT_APPLIED, &batch);
    if ((player->days_in_prison & GAME_STATUS_PENDING_BIT) != 0) {
        player->days_in_prison = 0;
        player->confinement_actor_type = GAME_CONFINEMENT_NONE;
        add_event(&batch, 52, STATUS_HANDLER_43D7BF, STATUS_EFFECT_APPLIED);
    }
    if ((player->days_in_hospital & GAME_STATUS_PENDING_BIT) != 0) {
        player->days_in_hospital = 0;
        player->confinement_actor_type = GAME_CONFINEMENT_NONE;
        add_event(&batch, 53, STATUS_HANDLER_43EE6E, STATUS_EFFECT_APPLIED);
    }
    if (!delayed_block(player)) {
        if ((player->days_sleeping & GAME_STATUS_PENDING_BIT) != 0) {
            player->days_sleeping = 0;
            player->play_state &= ~GAME_PLAYER_PLAY_STATE_FROZEN;
            add_event(&batch, 54, STATUS_HANDLER_40B8D8_40B93B,
                      STATUS_EFFECT_APPLIED);
        }
        if ((player->days_sleep_walking & GAME_STATUS_PENDING_BIT) != 0) {
            player->days_sleep_walking = 0;
            if (player->saved_traffic_method >= 0 && player->saved_traffic_method <= 3)
                player->traffic_method = player->saved_traffic_method;
            if (player->saved_ndices > 0 && player->saved_ndices <= 4)
                player->ndices = player->saved_ndices;
            else
                player->ndices = (player->traffic_method & 3) + 1;
            add_event(&batch, 55, STATUS_HANDLER_SLEEPWALK_40B93B,
                      STATUS_EFFECT_APPLIED);
        }
        consume(&player->days_tortoise_walking, 57, STATUS_HANDLER_CLEAR_ONLY,
                STATUS_EFFECT_APPLIED, &batch);
    }
    consume(&player->days_stopping, 56, STATUS_HANDLER_CLEAR_ONLY,
            STATUS_EFFECT_APPLIED, &batch);
    consume(&player->days_rejected_by_bank, 59, STATUS_HANDLER_CLEAR_ONLY,
            STATUS_EFFECT_APPLIED, &batch);
    consume(&player->days_bank_no_loans, 60, STATUS_HANDLER_CLEAR_ONLY,
            STATUS_EFFECT_APPLIED, &batch);
    if ((player->god_status_days & GAME_STATUS_PENDING_BIT) != 0) {
        int linked = player->linked_player;
        player->god_status_days = 0;
        player->linked_player = -1;
        if (linked >= 0 && linked < state->player_count) {
            state->players[linked].god_status_days = 0;
            state->players[linked].linked_player = -1;
        }
        add_event(&batch, 61, STATUS_HANDLER_40CC1A, STATUS_EFFECT_APPLIED);
    }
    consume(&player->days_assurance, 62, STATUS_HANDLER_CLEAR_ONLY,
            STATUS_EFFECT_APPLIED, &batch);
    *out_batch = batch;
    return batch.count;
}

void rich4_status_tick_player_events(GamePlayerState *player,
                                     int player_index,
                                     StatusTickResult *out_result,
                                     GameEventQueue *events)
{
    StatusTickResult result;
    int i;
    rich4_status_tick_player(player, &result);
    for (i = 0; i < result.count; i++) {
        Rich4GameEventConstructor constructor =
            (result.values[i] & GAME_STATUS_PENDING_BIT) != 0
                ? rich4_game_event_status_expired_from_fields
                : rich4_game_event_status_changed_from_fields;
        push_status_countdown_event(
            events, constructor, player_index, result.offsets[i],
            result.values[i], 0);
    }
    if (result.god_relation_tick_requested)
        push_status_countdown_event(
            events, rich4_game_event_status_tick_requested_from_fields,
            player_index, -1, 0, 0);
    if (out_result != NULL) *out_result = result;
}

int rich4_status_consume_pending_events(GameState *state,
                                        int player_index,
                                        StatusEventBatch *out_batch,
                                        GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    StatusEventBatch batch;
    int count;
    int i;
    if (state == NULL || events == NULL) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    count = rich4_status_consume_pending(&next, player_index, &batch);
    for (i = 0; i < batch.count; i++) {
        if (!push_status_countdown_event(
                &staged, rich4_game_event_status_effect_resolved_from_fields,
                player_index, batch.events[i].original_offset,
                batch.events[i].handler, batch.events[i].effect_result)) return 0;
        if (batch.events[i].handler == STATUS_HANDLER_43D7BF ||
            batch.events[i].handler == STATUS_HANDLER_43EE6E) {
            int kind = batch.events[i].handler == STATUS_HANDLER_43D7BF
                           ? GAME_CONFINEMENT_PRISON : GAME_CONFINEMENT_HOSPITAL;
            if (!push_status_countdown_event(
                    &staged,
                    rich4_game_event_actor_dirty_requested_from_fields,
                    player_index, batch.events[i].original_offset, kind, 0) ||
                !push_status_countdown_event(
                    &staged,
                    rich4_game_event_scene_refresh_requested_from_fields,
                    player_index, batch.events[i].original_offset, kind, 0)) return 0;
        } else if (batch.events[i].handler == STATUS_HANDLER_40D6BE ||
                   batch.events[i].handler == STATUS_HANDLER_40D4E5 ||
                   batch.events[i].handler == STATUS_HANDLER_40B8D8_40B93B ||
                   batch.events[i].handler == STATUS_HANDLER_SLEEPWALK_40B93B) {
            if (!push_status_countdown_event(
                    &staged, rich4_game_event_actor_dirty_requested_from_fields,
                    player_index, batch.events[i].original_offset,
                    batch.events[i].handler, 0)) return 0;
        }
    }
    if (!rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    if (out_batch != NULL) *out_batch = batch;
    return count;
}
