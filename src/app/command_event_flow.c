#include "app/command_event_flow.h"

#include "app/auction_flow.h"
#include "app/audio_event.h"
#include "app/bankruptcy_flow.h"
#include "app/bank_flow.h"
#include "app/card_use_flow.h"
#include "app/dice_flow.h"
#include "app/game_speech_flow.h"
#include "app/insurance_flow.h"
#include "app/property_flow.h"
#include "app/special_presentation_flow.h"
#include "app/special_place_notice_flow.h"
#include "app/tool_animation_flow.h"
#include "app/tool_followup_flow.h"
#include "app/turn_status_flow.h"
#include "app/turn_start_flow.h"
#include "core/character_speech_rules.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/property.h"
#include "core/turn_flow.h"
#include "view/game_view_model.h"

/* 0x4476AF picks the open exit closest to the actor's old heading,
 * breaking ties by route table order. +14 holds an alternate (incoming) node. */
static void sync_transferred_actor(GameSession *session, const GameCommand *command)
{
    GameState *state = &session->state;
    GameDomainRef who = command->target;
    int node = command->target2.id;
    int special = who.kind == GAME_DOMAIN_SPECIAL_PLAYER;
    if ((!special && who.kind != GAME_DOMAIN_PLAYER) || who.id < 0 ||
        who.id >= (special ? GAME_SPECIAL_PLAYER_COUNT : state->player_count) ||
        node <= 0 || node >= state->tool_route_node_count) return;
    int old = special ? state->special_player_direction[who.id] :
                        session->presentation.actors[who.id].direction;
    int heading = old, best = 9, next = 0, first = 0, second = 0;
    for (int i = 0; i < 4; ++i) {
        int other = state->tool_route_next[node][i];
        if (other <= 0 || other >= state->tool_route_node_count ||
            (state->tool_route_flags[node] & (UINT32_C(0x40000000) >> i))) continue;
        if (!first) first = other;
        else if (!second && other != first) second = other;
        int dir = rich4_game_actor_direction_from_delta(
            state->tool_route_x[other] - state->tool_route_x[node],
            state->tool_route_y[other] - state->tool_route_y[node]);
        int delta = (dir - old) & 7;
        if (delta > 4) delta = 8 - delta;
        if (delta < best) {
            best = delta;
            next = other;
            heading = dir;
        }
    }
    int previous = next == first && second ? second : first;
    if (special) {
        state->special_player_previous_node[who.id] = previous;
        state->special_player_direction[who.id] = heading;
    } else {
        GameActorPresentationState *actor = &session->presentation.actors[who.id];
        state->players[who.id].previous_node = previous;
        state->players[who.id].next_node = next;
        actor->direction = heading;
        actor->visual_x = state->tool_route_x[node];
        actor->visual_y = state->tool_route_y[node];
        actor->move_active = actor->walk_frame = 0;
    }
}

static void apply_property_owner_change(
    GameSession *session, const GameCommand *command,
    const GameEvent *event, uint32_t now)
{
    if (rich4_game_event_property_owner(event) < 0 ||
        rich4_game_event_property_value(event) < 0)
        return;
    session->presentation.property.sound_pending = 1;
    if (command != NULL && command->type == GAME_COMMAND_PURCHASE_PROPERTY) {
        int land = rich4_game_event_property_ref(event).id;
        session->presentation.property.purchase_delay_pending = 1;
        if (rich4_game_event_property_ref(event).kind != GAME_DOMAIN_LAND ||
            rich4_property_same_group_owned_count(
                &session->state, event->player, land) < 3 ||
            !game_start_property_speech(
                session, event->player, RICH4_PROPERTY_SPEECH_PURCHASE_THREE,
                0, GAME_PROPERTY_PRESENT_NONE, now)) {
            session->presentation.property.purchase_delay_pending = 0;
            rich4_turn_flow_start_property_end_delay(&session->flow, now);
        }
    } else {
        rich4_turn_flow_start_property_end_delay(&session->flow, now);
    }
}

Rich4CommandEventEffect rich4_app_command_event_effect(
    GameSession *session, const GameCommand *command,
    const GameEvent *event)
{
    Rich4CommandEventEffect effect = {
        RICH4_COMMAND_EVENT_EFFECT_NONE, 0
    };
    if (session == NULL || event == NULL)
        return effect;

    switch (event->type) {
    case GAME_EVENT_STATUS_CHANGED:
        if (rich4_game_event_status_target(event).kind == GAME_DOMAIN_TOOL &&
            rich4_tool_animation_defer_sync(session, event->player))
            return effect;
        effect.kind = RICH4_COMMAND_EVENT_EFFECT_STATUS_SYNC;
        return effect;
    case GAME_EVENT_PROPERTY_LEVEL_CHANGED:
        /* 0x443A81/0x443C10 update the property inside their dedicated
         * animation, without the normal construction sound/end delay. */
        if (command != NULL && command->type == GAME_COMMAND_USE_CARD &&
            (rich4_card_use_animation_resource(command->subject.id, command->target) ||
             command->subject.id == 8 || command->subject.id == 9))
            return effect;
        if (command != NULL && command->type == GAME_COMMAND_USE_TOOL &&
            command->subject.id == 8)
            return effect;
        effect.kind = RICH4_COMMAND_EVENT_EFFECT_PROPERTY_LEVEL;
        return effect;
    case GAME_EVENT_TOOL_PRESENTATION_REQUESTED:
        effect.kind = RICH4_COMMAND_EVENT_EFFECT_TOOL_ANIMATION;
        effect.mark_robot_trigger =
            rich4_game_event_map_object_tool(event) == 0 &&
            rich4_game_event_map_object_flags(event) == 1;
        return effect;
    default:
        return effect;
    }
}

Rich4CommandEventResult rich4_app_apply_command_event(
    GameSession *session, const GameCommand *command,
    const GameEvent *event, uint32_t now)
{
    if (session == NULL || event == NULL)
        return RICH4_COMMAND_EVENT_PLATFORM_FOLLOWUP;

    switch (event->type) {
    case GAME_EVENT_DICE_ROLL_REQUESTED:
        rich4_dice_begin_roll(session, now);
        return RICH4_COMMAND_EVENT_CONSUMED;
    case GAME_EVENT_STATUS_CHANGED:
        if (command != NULL && command->type == GAME_COMMAND_USE_CARD &&
            command->subject.kind == GAME_DOMAIN_CARD &&
            command->subject.id == 5 &&
            rich4_game_event_status_subject(event).kind == GAME_DOMAIN_CARD &&
            rich4_game_event_status_subject(event).id == 5) {
            (void)rich4_app_present_turn_around(
                session, rich4_game_event_status_target(event), NULL);
            return RICH4_COMMAND_EVENT_CONSUMED;
        }
        return RICH4_COMMAND_EVENT_PLATFORM_FOLLOWUP;
    case GAME_EVENT_TURN_FLOW_REQUESTED:
        (void)rich4_turn_start_begin(session, now);
        return RICH4_COMMAND_EVENT_CONSUMED;
    case GAME_EVENT_DICE_COUNT_CHANGED:
        session->presentation.dice.status_visible = 1;
        return RICH4_COMMAND_EVENT_CONSUMED;
    case GAME_EVENT_VOICE_REQUESTED:
        (void)rich4_tool_animation_handle_voice_event(session, event, now);
        return RICH4_COMMAND_EVENT_CONSUMED;
    case GAME_EVENT_TEXT_REQUESTED:
        if (command != NULL &&
            ((command->type == GAME_COMMAND_USE_CARD && command->subject.id == 16) ||
             (command->type == GAME_COMMAND_USE_TOOL &&
              (command->subject.id == 6 || command->subject.id == 12)))) {
            Rich4GameEventFields fields = rich4_game_event_text_requested_fields(event);
            if ((fields.subject == 52 || fields.subject == 53) &&
                rich4_confinement_presentation_enqueue(session, event->player,
                    fields.value, fields.value2, now))
                return RICH4_COMMAND_EVENT_CONSUMED;
        }
        if (rich4_tool_animation_handle_text_event(session, event, now))
            return RICH4_COMMAND_EVENT_CONSUMED;
        return RICH4_COMMAND_EVENT_PLATFORM_FOLLOWUP;
    case GAME_EVENT_ROUTE_EVENT_RESOLVED:
        if (command != NULL && rich4_bank_flow_begin_result(
                session, command->type, now))
            return RICH4_COMMAND_EVENT_CONSUMED;
        session->presentation.route_interaction.active = 0;
        return RICH4_COMMAND_EVENT_CONSUMED;
    case GAME_EVENT_SPECIAL_EVENT_COMMITTED:
        rich4_special_event_capture_commit(session, event);
        return RICH4_COMMAND_EVENT_CONSUMED;
    /* Cash/bank command completion can also trigger 0x436B0A's owner notice. */
    case GAME_EVENT_SPECIAL_PLACE_NOTICE_REQUESTED:
        return rich4_special_place_notice_enqueue(session, event, now)
            ? RICH4_COMMAND_EVENT_CONSUMED : RICH4_COMMAND_EVENT_REJECTED;
    case GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED:
    case GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED:
        rich4_insurance_flow_enqueue_notice(session, event, now);
        return RICH4_COMMAND_EVENT_CONSUMED;
    case GAME_EVENT_GOD_TRADE_REJECTION_PRESENTATION_REQUESTED:
        rich4_property_begin_god_trade_rejection(session, event, now);
        return RICH4_COMMAND_EVENT_CONSUMED;
    case GAME_EVENT_MAP_OBJECT_CHANGED:
        if (command && command->type == GAME_COMMAND_USE_TOOL && command->subject.id == 10)
            sync_transferred_actor(session, command);
        (void)rich4_tool_followup_begin_placement(
            session, command, event, now);
        return RICH4_COMMAND_EVENT_CONSUMED;
    case GAME_EVENT_PROPERTY_OWNER_CHANGED:
        apply_property_owner_change(session, command, event, now);
        return RICH4_COMMAND_EVENT_CONSUMED;
    case GAME_EVENT_AUCTION_REQUESTED:
        if (rich4_game_event_auction_trigger(event) ==
            GAME_AUCTION_TRIGGER_BANKRUPTCY)
            return rich4_bankruptcy_capture_event(
                session, event, now)
                ? RICH4_COMMAND_EVENT_CONSUMED
                : RICH4_COMMAND_EVENT_REJECTED;
        return rich4_auction_flow_enqueue(session, event, NULL) ?
            RICH4_COMMAND_EVENT_CONSUMED : RICH4_COMMAND_EVENT_REJECTED;
    case GAME_EVENT_PLAYER_ELIMINATED:
        if (rich4_bankruptcy_capture_event(session, event, now))
            return RICH4_COMMAND_EVENT_CONSUMED;
        return RICH4_COMMAND_EVENT_PLATFORM_FOLLOWUP;
    case GAME_EVENT_COMMAND_REJECTED:
        return RICH4_COMMAND_EVENT_REJECTED;
    case GAME_EVENT_GAME_ENDED:
        if (rich4_bankruptcy_capture_event(session, event, now))
            return RICH4_COMMAND_EVENT_CONSUMED;
        rich4_app_capture_game_ended(session, event);
        return RICH4_COMMAND_EVENT_CONSUMED;
    default:
        return RICH4_COMMAND_EVENT_PLATFORM_FOLLOWUP;
    }
}
