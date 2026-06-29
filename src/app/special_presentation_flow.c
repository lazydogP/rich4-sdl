#include "app/special_presentation_flow.h"
#include "app/insurance_flow.h"

#include "app/auction_flow.h"
#include "app/dynamic_object_flow.h"
#include "app/bankruptcy_flow.h"
#include "app/turn_status_flow.h"
#include "app/tool_animation_flow.h"

#include <string.h>

#include "app/game_presentation.h"
#include "app/game_speech_flow.h"
#include "core/card.h"
#include "core/character_speech_rules.h"
#include "core/confinement.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/property.h"
#include "core/rng.h"
#include "core/route_event_executor.h"
#include "core/special_event.h"
#include "data/character_speech.h"

int rich4_special_event_confinement_kind(int family, int id)
{
    if (family == GAME_SPECIAL_EVENT_NEWS)
        return id == 29 ? GAME_CONFINEMENT_PRISON :
               id == 4 ? GAME_CONFINEMENT_HOSPITAL : 0;
    if (family == GAME_SPECIAL_EVENT_FORTUNE) {
        if (id >= 33 && id <= 48) return GAME_CONFINEMENT_PRISON;
        if (id == 12 || id == 13) return GAME_CONFINEMENT_HOSPITAL;
    }
    return 0;
}

int rich4_special_event_confinement_days(
    const GameSession *session, int player, int confinement)
{
    if (session == NULL || player < 0 ||
        player >= session->state.player_count)
        return 0;
    if (confinement == GAME_CONFINEMENT_PRISON)
        return session->state.players[player].days_in_prison & 0x7f;
    if (confinement == GAME_CONFINEMENT_HOSPITAL)
        return session->state.players[player].days_in_hospital & 0x7f;
    return 0;
}

static int fortune_payment_id(int id)
{
    return (id >= 14 && id <= 19) ||
           id == 23 || id == 24 || id == 26 || id == 30;
}

static int fortune_income_id(int id)
{
    return id == 20 || id == 21 || id == 22 || id == 25 ||
           id == 27 || id == 28 || id == 29 || id == 31;
}

/* 0x44F354/0x44F42D/0x44F567 draw only for the middle amount tier. */
static int money_reaction_random(GameSession *session, int amount)
{
    uint32_t scale=(uint32_t)session->state.price_index;
    return amount>=(int32_t)(5000u*scale) && amount<(int32_t)(9000u*scale) ?
        (int)(rich4_rng_next_at(&session->state.ai_rng_state)&1u) : 0;
}

static int start_special_event_speech(
    GameSession *session, int player,
    const Rich4CharacterSpeech *speech, int speech_type,
    int reaction_stage, uint32_t now,
    const GamePlayerState *status_player)
{
    if (!game_start_speech_entry_with_status(
            session, player, speech, speech_type,
            GAME_PROPERTY_PRESENT_NONE, now, status_player))
        return 0;
    rich4_special_event_set_reaction_stage(session, reaction_stage);
    return 1;
}

int rich4_special_event_start_confinement_final_speech(
    GameSession *session, int player, uint32_t now)
{
    const Rich4CharacterSpeech *speech;
    int character;
    int hospital;
    if (session == NULL || player < 0 ||
        player >= session->state.player_count)
        return 0;
    character = session->state.players[player].character;
    hospital = session->presentation.special_event.confinement ==
        GAME_CONFINEMENT_HOSPITAL;
    speech = rich4_character_confinement_speech(character, hospital);
    return start_special_event_speech(
        session, player, speech, 2, 3, now,
        &session->state.players[player]);
}

static const Rich4CharacterSpeech *special_event_days_speech(
    GameSession *session, int character, int days)
{
    if (days <= 0) return NULL;
    if (days <= 3)
        return rich4_character_special_event_speech(character, 1, 0);
    if (days <= 6)
        return rich4_character_special_event_speech(
            character, 0, (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 1u));
    return rich4_character_special_event_speech(character, 0, 0);
}

int rich4_special_event_prepare_reaction(
    GameSession *session, uint32_t now,
    Rich4SpecialEventReactionResult *result)
{
    const Rich4CharacterSpeech *speech = NULL;
    GamePlayerState pre_disappearing_status;
    const GamePlayerState *before_player;
    const GamePlayerState *status_player;
    Rich4PropertySpeechKind money_speech_kind;
    int amount;
    int player;
    int character;
    int speech_type = 2;
    int reaction_stage = 0;
    int id;

    if (result != NULL) {
        result->action = RICH4_SPECIAL_EVENT_REACTION_NONE;
        result->player = -1;
        result->center_original_player = 0;
        result->sync_player_to_node = 0;
    }
    if (session == NULL) return 0;
    {
        GameSpecialEventPresentationState *special = &session->presentation.special_event;
        int target;
        for (target = 0; target < session->state.player_count; target++)
            if (special->pending_confinement_days[target] > 0) {
                if (!rich4_confinement_presentation_enqueue(session, target,
                        special->confinement, special->pending_confinement_days[target], now))
                    return 0;
                special->pending_confinement_days[target] = 0;
                special->reaction_mask = 0;
                rich4_special_event_set_reaction_stage(session, 5);
                return 1;
            }
    }
    id = session->presentation.special_event.resolved_id;
    if (session->presentation.special_event.family == GAME_SPECIAL_EVENT_NEWS &&
        id == 29 && session->presentation.special_event.fortune_passive_card == 20)
        return 0;
    if (session->presentation.special_event.family ==
            GAME_SPECIAL_EVENT_NEWS && id == 4 &&
        (session->presentation.special_event.reaction_player < 0 ||
         session->presentation.special_event.reaction_stage == 3) &&
        !rich4_special_event_prepare_next_news_victim(session))
        return 0;
    player = session->presentation.special_event.reaction_player;
    if (player < 0 || player >= session->state.player_count) return 0;
    character = session->state.players[player].character;
    status_player = &session->state.players[player];
    before_player = rich4_special_event_original_player(session, player);
    if (session->presentation.special_event.family ==
            GAME_SPECIAL_EVENT_FORTUNE &&
        session->presentation.special_event.defense ==
            GAME_SPECIAL_DEFENSE_BLOCKED) {
        if (session->presentation.special_event.fortune_passive_card == 20)
            return 0; /* 0x441210 already ended inside the absolution callback. */
        if (id >= 33 && id <= 48) {
            speech = rich4_character_reward_speech(character, 1, 0);
            if (start_special_event_speech(
                    session, player, speech, 0, 0, now, status_player)) {
        if (result != NULL) {
            result->action = RICH4_SPECIAL_EVENT_REACTION_SPEECH;
            result->player = player;
        }
                return 1;
            }
            return 0;
        }
        if (!fortune_payment_id(id)) return 0;
    }
    if (session->presentation.special_event.confinement != 0) {
        int days = rich4_special_event_confinement_days(
            session, player, session->presentation.special_event.confinement);
        if (before_player != NULL)
            rich4_special_event_set_first_confinement(session, before_player);
        if (days == 0) return 0;
        if (!session->presentation.special_event.first_confinement) {
            if (result != NULL) {
                result->player = player;
                result->sync_player_to_node = 1;
            }
            if (rich4_special_event_start_confinement_final_speech(
                    session, player, now)) {
                if (result != NULL)
                    result->action = RICH4_SPECIAL_EVENT_REACTION_SPEECH;
                return 1;
            }
            return 0;
        }
        if (result != NULL) {
            result->player = player;
            result->center_original_player = 1;
        }
        status_player = before_player;
        speech = special_event_days_speech(session, character, days);
        if (start_special_event_speech(
                session, player, speech, 2, 1, now, status_player)) {
            if (result != NULL)
                result->action = RICH4_SPECIAL_EVENT_REACTION_SPEECH;
            return 1;
        }
        if (result != NULL)
            result->action = RICH4_SPECIAL_EVENT_REACTION_CONFINEMENT_ANIMATION;
        return 1;
    }
    if (session->presentation.special_event.family ==
            GAME_SPECIAL_EVENT_NEWS) {
        if (id == 5 || id == 15 || id == 19 || id == 21)
            speech = rich4_character_special_event_speech(
                character, 0, (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 1u));
        else if (id == 8 || id == 9 || id == 10) {
            amount = session->presentation.special_event.player_amount[player];
            money_speech_kind = rich4_property_receiver_speech(
                &session->state, amount, money_reaction_random(session,amount));
            speech = rich4_character_property_speech(
                character, money_speech_kind);
            speech_type = 3;
            if (result != NULL) result->center_original_player = 1;
        }
    } else if (session->presentation.special_event.family ==
                   GAME_SPECIAL_EVENT_FORTUNE) {
        /* 0x44C2C2 returns after the bank transfers, without a reaction
         * or random draw. 0x44C3B7 alone has the birthday reward tail. */
        if (id == 5) {
            if (session->presentation.special_event.cards_gained <= 0)
                return 0;
            speech = rich4_character_reward_speech(
                character, 1, (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 1u));
            speech_type = 0;
        } else if (id == 6 || id == 7) {
            int days = session->presentation.special_event.committed_amount;
            if (!session->presentation.special_event.pre_state_valid ||
                (session->presentation.special_event.pre_state
                     .players[player].days_disappearing &
                 GAME_DISAPPEARING_DAYS_MASK) != 0 || days <= 0)
                return 0;
            speech = special_event_days_speech(session, character, days);
            pre_disappearing_status = session->presentation.special_event
                .pre_state.players[player];
            status_player = &pre_disappearing_status;
            reaction_stage = 4;
            if (result != NULL) result->center_original_player = 1;
        } else if (id == 9) {
            speech = rich4_character_special_event_speech(
                character, 1, 0);
        } else if (id == 11) {
            speech = rich4_character_special_event_speech(
                character, 0, 0);
        } else if (id == 0 || id == 1 || id == 10) {
            speech = rich4_character_special_event_speech(
                character, 0, (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 1u));
        } else if (id == 32) {
            speech = rich4_character_special_event_speech(
                character, 0, 0);
        } else if (fortune_payment_id(id)) {
            if ((session->state.players[player].play_state & 0xff) == 0)
                return 0;
            if (session->presentation.special_event.defense ==
                    GAME_SPECIAL_DEFENSE_BLOCKED) {
                speech = rich4_character_payment_blocked_speech(
                    character, session->presentation.special_event.value,
                    session->state.price_index,
                    money_reaction_random(session,session->presentation.special_event.value));
                speech_type = 3;
            } else {
                amount = session->presentation.special_event.committed_amount;
                money_speech_kind = rich4_property_payer_speech(
                    &session->state, player, -2, amount, 0,
                    money_reaction_random(session,amount));
                speech = rich4_character_property_speech(
                    character, money_speech_kind);
            }
        } else if (fortune_income_id(id)) {
            amount = session->presentation.special_event.committed_amount;
            money_speech_kind = rich4_property_receiver_speech(
                &session->state, amount,
                money_reaction_random(session,amount));
            speech = rich4_character_property_speech(
                character, money_speech_kind);
            speech_type = 3;
        }
    }
    if (result != NULL) result->player = player;
    if (start_special_event_speech(
            session, player, speech, speech_type,
            reaction_stage, now, status_player)) {
        if (result != NULL) {
            result->action = RICH4_SPECIAL_EVENT_REACTION_SPEECH;
        }
        return 1;
    }
    if (reaction_stage == 4) {
        if (result != NULL)
            result->action = RICH4_SPECIAL_EVENT_REACTION_FORTUNE_ANIMATION;
        return 1;
    }
    return 0;
}

int rich4_special_event_begin(
    GameSession *session, const GameEvent *event, int confinement,
    int first_confinement, uint32_t now)
{
    GameSpecialEventPresentationState *special;
    int family;
    if (session == NULL || event == NULL ||
        rich4_game_event_special_event_panel(event) != 66)
        return 0;
    family = (int)rich4_game_event_special_event_family(event);
    if (family != GAME_SPECIAL_EVENT_NEWS &&
        family != GAME_SPECIAL_EVENT_FORTUNE)
        return 0;

    special = &session->presentation.special_event;
    special->active = 1;
    special->family = family;
    special->resource = rich4_game_event_special_event_resource(event);
    special->actor = session->state.special_event_actor;
    special->logical_id = session->state.special_event_logical_id;
    special->resolved_id = session->state.special_event_resolved_id;
    special->value = session->state.special_event_prepared_value;
    special->target_mask = session->state.special_event_target_mask;
    special->reaction_player = session->state.special_event_actor;
    if (family == GAME_SPECIAL_EVENT_NEWS) {
        int owner = (int)((session->state.special_event_target_mask >> 24) &
                          0x0fu) - 1;
        special->reaction_player = owner;
        if (special->resolved_id == 4)
            special->reaction_player = -1;
    }
    special->consequence_index = -1;
    special->confinement = confinement;
    special->first_confinement = first_confinement != 0;
    special->reaction_stage = 0;
    memset(special->pending_confinement_days, 0, sizeof(special->pending_confinement_days));
    special->pre_state = session->state;
    special->pre_state_valid = 1;
    memcpy(special->original_players, session->state.players,
           sizeof(special->original_players));
    special->original_players_valid = 1;
    special->commit_seen = 0;
    special->committed_amount = 0;
    special->defense = GAME_SPECIAL_DEFENSE_NONE;
    special->cards_gained = 0;
    memcpy(special->player_amount,
           session->state.special_event_player_amount,
           sizeof(special->player_amount));
    special->reaction_mask = family == GAME_SPECIAL_EVENT_NEWS &&
        special->resolved_id == 4
        ? (special->target_mask >> RICH4_SPECIAL_PRESENTATION_PLAYER_SHIFT) &
          0xffu
        : 0;
    special->fortune_card_target_mask = 0;
    special->fortune_card_target_player = -1;
    special->fortune_choices_done = 0;
    special->fortune_defense_ready = 0;
    special->fortune_god_notice_done = 0;
    special->fortune_passive_resolved = 0;
    special->fortune_redirect_target = -1;
    special->fortune_passive_card = -1;
    special->fortune_choice_hover = -1;
    special->fortune_card_targets_seen = 0;
    if (family == GAME_SPECIAL_EVENT_FORTUNE &&
        special->resolved_id == 5 && special->actor >= 0 &&
        special->actor < session->state.player_count &&
        (session->state.players[special->actor].play_state & 0xff) == 1) {
        int player;
        for (player = 0; player < session->state.player_count; player++)
            if (player != special->actor &&
                (special->pre_state.players[player].play_state & 0xff) != 0 &&
                rich4_card_inventory_count(&special->pre_state, player) > 0)
                special->fortune_card_target_mask |= 1u << player;
    }
    special->scene_refresh_frame = 0;
    special->scene_refreshed = 0;
    special->phase = RICH4_SPECIAL_PRESENT_PAGE;
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_SPECIAL]);
    special->frame = 0;
    special->tick = now;
    special->until = now + (family == GAME_SPECIAL_EVENT_NEWS ? 2400u : 1600u);
    session->presentation.route_interaction.active = 0;
    return 1;
}

int rich4_special_event_begin_from_event(
    GameSession *session, const GameEvent *event, uint32_t now)
{
    int family;
    int confinement;
    int confinement_player;
    int first_confinement;

    if (session == NULL || event == NULL) return 0;
    family = (int)rich4_game_event_special_event_family(event);
    confinement = rich4_special_event_confinement_kind(
        family, session->state.special_event_resolved_id);
    confinement_player = session->state.special_event_actor;
    if (family == GAME_SPECIAL_EVENT_NEWS) {
        confinement_player = (int)(
            (session->state.special_event_target_mask >> 24) & 0x0fu) - 1;
        if (session->state.special_event_resolved_id == 4)
            confinement_player = -1;
    }
    first_confinement = confinement != 0 &&
        rich4_special_event_confinement_days(
            session, confinement_player, confinement) == 0;
    return rich4_special_event_begin(
        session, event, confinement, first_confinement, now);
}

static int special_event_card_total(const GamePlayerState *player)
{
    int total = 0;
    int i;
    if (player == NULL) return 0;
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++)
        if (player->card_inventory[i] != 0) total++;
    return total;
}

void rich4_special_event_capture_commit(
    GameSession *session, const GameEvent *event)
{
    int actor;
    if (session == NULL || event == NULL ||
        event->type != GAME_EVENT_SPECIAL_EVENT_COMMITTED)
        return;
    session->presentation.special_event.commit_seen = 1;
    session->presentation.special_event.committed_amount =
        rich4_game_event_special_commit_amount(event);
    session->presentation.special_event.defense =
        rich4_game_event_special_commit_defense(event);
    session->presentation.special_event.cards_gained = 0;
    actor = session->presentation.special_event.actor;
    if (session->presentation.special_event.family ==
            GAME_SPECIAL_EVENT_FORTUNE &&
        session->presentation.special_event.resolved_id == 5 &&
        session->presentation.special_event.pre_state_valid &&
        actor >= 0 && actor < session->state.player_count) {
        int i;
        if ((session->state.players[actor].play_state & 0xff) == 1 &&
            session->presentation.special_event.fortune_choices_done) {
            session->presentation.special_event.cards_gained =
                session->presentation.special_event.fortune_card_targets_seen;
            return;
        }
        /* 0x44C573 counts eligible victims, including a canceled human
         * selection and a transfer into a full hand. */
        for (i = 0; i < session->presentation.special_event.pre_state.player_count; i++)
            if (i != actor &&
                (session->presentation.special_event.pre_state.players[i].play_state & 0xff) &&
                special_event_card_total(&session->presentation.special_event.pre_state.players[i]))
                session->presentation.special_event.cards_gained++;
    }
}

RouteEventCommandResult rich4_special_event_submit_page_command(
    GameSession *session, const GameCommand *command, uint32_t now)
{
    return rich4_special_event_submit_page_command_with_area(
        session, command, now, NULL, 0, 0, NULL);
}

RouteEventCommandResult rich4_special_event_submit_page_command_with_area(
    GameSession *session, const GameCommand *command, uint32_t now,
    const GameNewsAreaAnchor *anchors, int count, int available, const MapData *map)
{
    GameState next;
    GameEventQueue events;
    GameEvent event;
    RouteEventCommandResult result;
    unsigned int area_target_mask = 0;
    GameMapDynamicObjectState saved_objects[GAME_MAP_DYNAMIC_OBJECT_COUNT];
    int changed_objects = 0;
    if (session == NULL || command == NULL)
        return ROUTE_EVENT_COMMAND_INVALID;
    next = session->state;
    if (next.special_event_family == GAME_SPECIAL_EVENT_NEWS &&
        (next.special_event_resolved_id == 4 || next.special_event_resolved_id == 20)) {
        if (!available || !rich4_special_event_apply_news_area(&next,anchors,count))
            return ROUTE_EVENT_COMMAND_INVALID;
        if (next.special_event_resolved_id == 4 &&
            session->state.special_event_prepared_value != 1) {
            int cursor = -1;
            uint16_t raw;
            memcpy(saved_objects,session->presentation.dynamic_objects,sizeof(saved_objects));
            while (rich4_special_event_news_area_next_raw(anchors,count,&cursor,&raw)) {
                int slot = (int)((raw >> 8) & 0x7fu) - 1;
                if (!(raw & 0x8000u) || slot < 0 || slot >= GAME_MAP_DYNAMIC_OBJECT_COUNT) continue;
                if (slot >= 16) {
                    /* Keep occupancy changes in pixel order before the next
                     * god's 0x40AA6C relocation consumes the shared RNG. */
                    rich4_app_clear_dynamic_object(&session->presentation,slot);
                    changed_objects = 1;
                    continue;
                }
                if (slot < 12 && (map == NULL || map->routes == NULL)) {
                    memcpy(session->presentation.dynamic_objects,saved_objects,sizeof(saved_objects));
                    return ROUTE_EVENT_COMMAND_INVALID;
                }
                if (session->presentation.dynamic_objects[slot].node != 0) {
                    (void)rich4_app_remove_dynamic_object_with_rng(
                        &session->presentation,map,slot,&next.ai_rng_state);
                    changed_objects = 1;
                }
            }
        }
        area_target_mask = next.special_event_target_mask;
    }
    rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_submit_deferred_confinement(
        &session->flow.route_events, &next, command, &events);
    if (result == ROUTE_EVENT_COMMAND_INVALID || result == ROUTE_EVENT_COMMAND_UNEXPECTED) {
        if (changed_objects)
            memcpy(session->presentation.dynamic_objects,saved_objects,sizeof(saved_objects));
        return result;
    }
    session->state = next;
    if (area_target_mask)
        rich4_app_sync_tool_dynamic_objects(&session->presentation,&session->state);
    if (session->presentation.special_event.family == GAME_SPECIAL_EVENT_NEWS &&
        session->presentation.special_event.resolved_id == 4) {
        /* The target/victim snapshot was produced after the page, not at begin. */
        session->presentation.special_event.reaction_mask =
            (area_target_mask >> 16) & 0xffu;
        session->presentation.special_event.target_mask = area_target_mask;
    }
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_TEXT_REQUESTED &&
            session->presentation.special_event.confinement != 0 &&
            event.player >= 0 && event.player < session->state.player_count) {
            Rich4GameEventFields fields = rich4_game_event_text_requested_fields(&event);
            if (fields.value == session->presentation.special_event.confinement)
                session->presentation.special_event.pending_confinement_days[event.player] = fields.value2;
        } else if (event.type == GAME_EVENT_CARD_USED) {
            Rich4GameEventFields fields = rich4_game_event_card_used_fields(&event);
            if (fields.subject_ref.kind == GAME_DOMAIN_CARD && fields.subject_ref.id == 18)
                session->presentation.special_event.reaction_player = fields.target_ref.id;
            if (fields.subject_ref.kind == GAME_DOMAIN_CARD && fields.subject_ref.id == 20)
                session->presentation.special_event.fortune_passive_card = 20;
        } else if (event.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED)
            rich4_insurance_flow_enqueue_notice(session, &event, now);
        else if (event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED)
            rich4_special_event_complete_route_interaction(session);
        else if (event.type == GAME_EVENT_SPECIAL_EVENT_COMMITTED)
            rich4_special_event_capture_commit(session, &event);
        else if (event.type == GAME_EVENT_AUCTION_REQUESTED) {
            if (!rich4_bankruptcy_capture_event(session, &event, now))
                (void)rich4_auction_flow_enqueue(session, &event, NULL);
        } else if (event.type == GAME_EVENT_PLAYER_ELIMINATED) {
            if (!rich4_bankruptcy_capture_event(session, &event, now) &&
                event.player >= 0 && event.player < GAME_MAX_PLAYERS) {
                session->presentation.actors[event.player].move_active = 0;
                session->presentation.actors[event.player].dirty_flags |=
                    GAME_PLAYER_DIRTY_TURN;
            }
        } else if (event.type == GAME_EVENT_GAME_ENDED) {
            if (!rich4_bankruptcy_capture_event(session, &event, now))
                rich4_app_capture_game_ended(session, &event);
        }
    }
    return result;
}
