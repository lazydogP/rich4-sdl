#include "app/property_flow.h"

#include "app/bankruptcy_flow.h"
#include "app/game_speech_flow.h"
#include "core/character_speech_rules.h"
#include "core/game_event_payload.h"
#include "core/property.h"
#include "core/god.h"
#include "core/facility.h"
#include "core/ledger.h"
#include "core/card.h"
#include <limits.h>
#include "core/rng.h"
#include "core/route_event_executor.h"
#include "core/turn_flow.h"

enum { RICH4_PROPERTY_DEFAULT_FRAME_MS = 20 };

int rich4_property_start_max_level_animation(
    GameSession *session, int frame_count, uint32_t now)
{
    GamePropertyPresentationState *property;
    if (session == NULL) return 0;
    property = &session->presentation.property;
    property->max_level_pending = 0;
    if (frame_count <= 0) {
        rich4_turn_flow_start_property_end_delay(&session->flow, now);
        return 0;
    }
    property->max_level_active = 1;
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_MAX_LEVEL]);
    property->max_level_frame = 0;
    property->max_level_tick = now;
    property->max_level_sound_pending = 1;
    return 1;
}

int rich4_property_update_max_level_animation(
    GameSession *session, int frame_count, int delay_ms, uint32_t now)
{
    GamePropertyPresentationState *property;
    if (session == NULL || !session->presentation.property.max_level_active)
        return 0;
    property = &session->presentation.property;
    if (delay_ms <= 0) delay_ms = RICH4_PROPERTY_DEFAULT_FRAME_MS;
    if (frame_count <= 0) {
        property->max_level_active = 0;
        rich4_turn_flow_start_property_end_delay(&session->flow, now);
        return 0;
    }
    if (property->max_level_frame < frame_count &&
        rich4_animation_clock_step(
            &session->presentation.animation_clock[RICH4_ANIMATION_MAX_LEVEL],
            now, delay_ms)) {
        property->max_level_frame++;
        property->max_level_tick = now;
    }
    if (property->max_level_frame >= frame_count) {
        property->max_level_active = 0;
        rich4_turn_flow_start_property_end_delay(&session->flow, now);
        return 0;
    }
    return 1;
}

void rich4_property_begin_level_presentation(
    GameSession *session, const GameEvent *event, int max_level_frames,
    uint32_t now)
{
    if (session == NULL || event == NULL) return;
    session->presentation.property.sound_pending = 2;
    if (rich4_game_event_property_level(event) == 5) {
        session->presentation.property.max_level_pending = 1;
        if (!game_start_property_speech(
                session, event->player, RICH4_PROPERTY_SPEECH_MAX_LEVEL, 0,
                GAME_PROPERTY_PRESENT_NONE, now))
            rich4_property_start_max_level_animation(
                session, max_level_frames, now);
        return;
    }
    rich4_turn_flow_start_property_end_delay(&session->flow, now);
}

/* 0x419D5A/0x41AEB4 show the unmodified fee before 0x41D709. */
static int original_charge_amount(GameSession *session, int actor,
                                  GameDomainRef ref, int fallback)
{
    if (ref.kind == GAME_DOMAIN_LAND) {
        GameState copy = session->state;
        GameLandAction action;
        copy.players[actor].god_info = 0;
        copy.players[actor].god_type = 0;
        if (rich4_land_inspect(&copy, actor, ref.id, &action) &&
            action.kind == GAME_LAND_ACTION_CHARGE) return action.amount;
    } else if (ref.kind == GAME_DOMAIN_COMPANY) {
        if (session->flow.route_events.state.presentation_mode >= 20)
            return session->flow.route_events.state.presentation_land;
    } else if (ref.kind == GAME_DOMAIN_FACILITY) {
        const RouteEvent *event = rich4_route_event_executor_current(
            &session->flow.route_events);
        int id = ref.id;
        int type = session->state.facility_type[id];
        int64_t fee = 0;
        if (type == 3 && event != NULL) {
            int traffic = session->state.players[actor].traffic_method & 3;
            if (traffic) fee = (int64_t)500 * session->state.price_index *
                event->snapshot.dice_total * (1 << (traffic - 1));
        } else if (type == 1 || type == 2) {
            int outcome = session->flow.route_events.state.presentation_mode - 10;
            fee = (int64_t)session->state.facility_charge_by_level[id][
                session->state.facility_level[id]] * session->state.price_index * outcome;
            if (session->state.facility_price_status[id]) fee *= 2;
        }
        if (fee >= 0 && fee <= INT_MAX) return (int)fee;
    }
    return fallback;
}

void rich4_property_begin_charge_presentation(
    GameSession *session, const GameEvent *event, uint32_t now)
{
    GamePropertyPresentationState *property;
    GameDomainRef property_ref;
    int land;
    if (session == NULL || event == NULL ||
        (rich4_game_event_property_ref(event).kind != GAME_DOMAIN_LAND &&
         rich4_game_event_property_ref(event).kind != GAME_DOMAIN_FACILITY &&
         rich4_game_event_property_ref(event).kind != GAME_DOMAIN_COMPANY) ||
        rich4_game_event_property_owner(event) < 0) return;
    property = &session->presentation.property;
    property_ref = rich4_game_event_property_ref(event);
    land = property_ref.id;
    property->actor = event->player;
    property->owner = rich4_game_event_property_owner(event);
    property->property_kind = property_ref.kind;
    property->land = land;
    property->group = property_ref.kind == GAME_DOMAIN_LAND &&
        land >= 0 && land < session->state.land_count
        ? session->state.land_group[land] : -1;
    property->god_type = 0;
    property->insurance_active = property_ref.kind == GAME_DOMAIN_COMPANY &&
        land >= 0 && land < session->state.company_count &&
        session->state.company_type[land] == 4;
    property->insurance_payer = event->player;
    property->insurance_card = -1;
    property->insurance_target = -1;
    property->insurance_sound_pending = 0;
    property->amount = rich4_game_event_property_charge_amount(event);
    property->god_original_amount = property->amount;
    if (rich4_game_event_property_charge_block_reason(event) == 0) {
        int type = property->insurance_active ?
            (session->state.players[event->player].god_info & 0xff) :
            rich4_god_player_type(&session->state.players[event->player]);
        if (type == 1 || type == 2 || type == 5 || type == 6) {
            property->god_original_amount = original_charge_amount(
                session, event->player, property_ref, property->amount);
            if (property->god_original_amount != property->amount)
                property->god_type = type;
        }
    }
    property->related_owner = -1;
    property->owner_amount = property->amount;
    property->related_amount = 0;
    property->collection_block_reason =
        rich4_game_event_property_charge_block_reason(event);
    if (property_ref.kind == GAME_DOMAIN_FACILITY || property_ref.kind == GAME_DOMAIN_COMPANY) {
        /* Facilities and companies quote without a land-group flash. */
        property->flash_count = 0;
        property->flash_frame = 0;
        property->flash_adjust = 0;
        property->tick = now;
        property->phase = GAME_PROPERTY_PRESENT_AMOUNT;
        property->sound_pending = property_ref.kind == GAME_DOMAIN_FACILITY &&
            session->state.facility_type[land] == 3 ? 3 : 0;
        return;
    }
    if (property->collection_block_reason ==
            GAME_PROPERTY_COLLECTION_BLOCK_NONE) {
        GameLandAction action;
        if (rich4_land_inspect(&session->state, property->actor, land,
                               &action) &&
            action.kind == GAME_LAND_ACTION_CHARGE &&
            action.amount == property->amount && action.related_owner >= 0) {
            property->related_owner = action.related_owner;
            property->owner_amount = action.owner_amount;
            property->related_amount = action.related_amount;
        }
    }
    if (property->collection_block_reason !=
            GAME_PROPERTY_COLLECTION_BLOCK_NONE) {
        property->flash_count = 0;
        property->flash_frame = 0;
        property->flash_adjust = 0;
        property->tick = now;
        property->phase = GAME_PROPERTY_PRESENT_BLOCKED_TEXT;
        property->sound_pending = 3;
        return;
    }
    property->flash_count = rich4_property_toll_flash_count(
        &session->state, land);
    property->flash_frame = 0;
    property->flash_adjust = 0;
    property->tick = now;
    property->phase = property->flash_count > 1
        ? GAME_PROPERTY_PRESENT_FLASH : GAME_PROPERTY_PRESENT_AMOUNT;
    rich4_animation_clock_reset(
        &session->presentation.animation_clock[RICH4_ANIMATION_PROPERTY_FLASH]);
    property->sound_pending = 3;
}

void rich4_property_begin_beggar_presentation(
    GameSession *session, const GameEvent *event, uint32_t now)
{
    GamePropertyPresentationState *property;
    int beggar;
    int amount;
    if (session == NULL || event == NULL ||
        event->type != GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED)
        return;
    beggar = rich4_game_event_beggar_player(event);
    amount = rich4_game_event_beggar_amount(event);
    if (beggar < 0 || beggar >= session->state.player_count || amount <= 0)
        return;
    property = &session->presentation.property;
    property->sound_pending = 3;
    property->phase = GAME_PROPERTY_PRESENT_BEGGAR_AMOUNT;
    property->tick = now;
    property->actor = event->player;
    property->owner = beggar;
    property->property_kind = GAME_DOMAIN_NONE;
    property->land = -1;
    property->group = -1;
    property->amount = amount;
    property->related_owner = -1;
    property->owner_amount = amount;
    property->related_amount = 0;
    property->collection_block_reason = GAME_PROPERTY_COLLECTION_BLOCK_NONE;
    property->flash_count = 0;
    property->flash_frame = 0;
    property->flash_adjust = 0;
}

void rich4_property_begin_god_trade_rejection(
    GameSession *session, const GameEvent *event, uint32_t now)
{
    GamePropertyPresentationState *property;
    int god_type;
    if (session == NULL || event == NULL || event->type !=
            GAME_EVENT_GOD_TRADE_REJECTION_PRESENTATION_REQUESTED)
        return;
    god_type = rich4_game_event_god_trade_type(event);
    if (god_type != 7 && god_type != 8 && god_type != 15) return;
    property = &session->presentation.property;
    /* 0x440CAC is a standalone timed text popup; it does not trigger the
     * ordinary property sound or payer/receiver speech sequence. */
    property->sound_pending = 0;
    property->phase = GAME_PROPERTY_PRESENT_GOD_TRADE_REJECTION;
    property->tick = now;
    property->actor = event->player;
    property->owner = -1;
    property->property_kind = GAME_DOMAIN_NONE;
    property->land = -1;
    property->group = -1;
    property->amount = god_type;
    property->related_owner = -1;
    property->owner_amount = 0;
    property->related_amount = 0;
    property->collection_block_reason = GAME_PROPERTY_COLLECTION_BLOCK_NONE;
    property->flash_count = 0;
    property->flash_frame = 0;
    property->flash_adjust = 0;
    property->purchase_delay_pending = 0;
    /* The confirmation controls disappear as soon as YES is accepted. */
    session->presentation.route_interaction.active = 0;
}

static int commit_god_building(GameSession *session, uint32_t now)
{
    GamePropertyPresentationState *p = &session->presentation.property;
    GameEventQueue events;
    int facility = p->property_kind == GAME_DOMAIN_FACILITY;
    int level;
    rich4_game_event_queue_reset(&events);
    if (p->god_choice != -2 && !rich4_god_apply_landing_effect_with_choice(
            &session->state, p->actor, p->god_node, p->god_choice, &events)) return 0;
    /* 0x40F381 owns these changes; ordinary purchase/level-change handlers
     * would add unrelated sounds and the ordinary level-five speech. */
    level = facility ? session->state.facility_level[p->land] :
                       session->state.land_level[p->land];
    if (p->god_type == 9) p->sound_pending = 2;
    if (p->god_type == 10 || (p->god_type == 9 && level == 5)) {
        (void)rich4_game_event_emit_typed_domain(&session->deferred_events,
            rich4_game_event_tool_presentation_requested_from_fields,
            p->actor, rich4_game_domain_ref(GAME_DOMAIN_GOD,
                session->state.players[p->actor].god_info - 1),
            rich4_game_domain_ref(p->property_kind, p->land),
            p->god_type == 10 ? 526 : 523, p->god_type == 10 ? 95 : 90, 0);
    }
    (void)now;
    return 1;
}

int rich4_property_begin_god_landing(GameSession *session, int player,
                                      int node, uint32_t now)
{
    GamePropertyPresentationState *p;
    int raw, id, facility, type, level, owner, building;
    if (session == NULL || player < 0 || player >= session->state.player_count ||
        node <= 0 || node >= GAME_MAX_MAP_ROUTE_NODES ||
        session->state.players[player].delayed_status_block != 0) return 0;
    type = rich4_god_player_type(&session->state.players[player]);
    if (type != 9 && type != 10 && type != 12) return 0;
    raw = session->state.route_object_raw[node];
    facility = raw > 4000 && raw < 6000;
    if (!facility && !(raw > 2000 && raw < 4000)) return 0;
    id = raw - (facility ? 4001 : 2001);
    if (id < 0 || id >= (facility ? session->state.facility_count : session->state.land_count))
        return 0;
    level = facility ? session->state.facility_level[id] : session->state.land_level[id];
    owner = facility ? session->state.facility_owner[id] : session->state.land_owner[id];
    building = facility ? session->state.facility_type[id] : session->state.land_type[id];
    if (type == 10 && level == 0) return 0;
    if (type == 12 && owner == player) return 0;
    if (type == 9) {
        static const int maximum[5] = {1,5,5,1,5};
        if (facility) {
            if (building < 0 || building > 4 || (level > 0 && level >= maximum[building])) return 0;
        } else if (!((building == 0 && level < 5) || (building == 1 && level == 0))) return 0;
    }
    session->presentation.view_world_x = session->presentation.actors[player].visual_x;
    session->presentation.view_world_y = session->presentation.actors[player].visual_y;
    if (session->state.players[player].node > 0 &&
        session->state.players[player].node < session->state.tool_route_node_count) {
        session->presentation.view_world_x = session->state.tool_route_x[session->state.players[player].node];
        session->presentation.view_world_y = session->state.tool_route_y[session->state.players[player].node];
    }
    session->presentation.has_view_override = 1;
    p = &session->presentation.property;
    p->god_type = type;
    p->god_node = node;
    p->god_choice = -1;
    p->god_build_animation_started = 0;
    p->god_previous_building_type = building;
    p->god_previous_building_level = level;
    p->actor = player;
    p->land = id;
    p->property_kind = facility ? GAME_DOMAIN_FACILITY : GAME_DOMAIN_LAND;
    p->phase = GAME_PROPERTY_PRESENT_GOD_BUILD_TEXT;
    p->tick = now;
    p->sound_pending = 0;
    /* 0x40F493 upgrades before the message, except the empty-facility
     * branch which prompts before 0x440AAC. The SDL view retains the old
     * building during this text, matching 0x440CAC's saved background;
     * 0x40F506 refreshes it only after the notice and build sound. */
    if (type == 9 && !(facility && level == 0)) {
        GameEventQueue ignored;
        rich4_game_event_queue_reset(&ignored);
        if (!rich4_god_apply_landing_effect(&session->state, player, node, &ignored)) {
            p->god_type = 0;
            p->phase = GAME_PROPERTY_PRESENT_NONE;
            return 0;
        }
        p->god_choice = -2;
    }
    /* Land God commits before its message. */
    if (type == 12 && !commit_god_building(session, now)) {
        p->god_type = 0;
        p->phase = GAME_PROPERTY_PRESENT_NONE;
        return 0;
    }
    return 1;
}

int rich4_property_choose_god_building(GameSession *session,
                                       const GameCommand *command)
{
    GamePropertyPresentationState *p;
    if (session == NULL || command == NULL) return 0;
    p = &session->presentation.property;
    if (p->phase != GAME_PROPERTY_PRESENT_GOD_BUILD_CHOICE ||
        command->type != GAME_COMMAND_UPGRADE_PROPERTY ||
        command->player != p->actor || command->subject.kind != GAME_DOMAIN_FACILITY ||
        command->subject.id != p->land || command->value < 0 || command->value > 4) return 0;
    p->god_choice = command->value;
    p->phase = GAME_PROPERTY_PRESENT_GOD_BUILD_COMMIT;
    session->presentation.route_interaction.active = 0;
    return 1;
}

static int insurance_automatic(const GameSession *session)
{
    const GamePlayerState *p = &session->state.players[session->presentation.property.actor];
    return p->is_ai || (p->play_state & 4) || p->days_sleep_walking;
}

static int insurance_card_threshold(const GameSession *session)
{
    const GamePropertyPresentationState *p = &session->presentation.property;
    const GamePlayerState *actor = &session->state.players[p->actor];
    return (int64_t)p->amount >= (int64_t)2000 * session->state.price_index ||
        (int64_t)p->amount > (int64_t)actor->cash + actor->bank;
}

static void insurance_card_notice(GameSession *session, int card, uint32_t now)
{
    GamePropertyPresentationState *p = &session->presentation.property;
    p->insurance_card = card;
    p->insurance_sound_pending = 1; /* 0x441F73: sound 62 */
    p->phase = GAME_PROPERTY_PRESENT_INSURANCE_CARD;
    p->tick = now;
}

static int insurance_consume_card(GameSession *session)
{
    GamePropertyPresentationState *p = &session->presentation.property;
    GameEventQueue events;
    int payer, amount;
    rich4_game_event_queue_reset(&events);
    if (!rich4_card_resolve_charge(&session->state, p->actor, p->owner,
            p->amount, p->insurance_card == 19, p->insurance_target,
            &payer, &amount, &events)) return 0;
    p->insurance_payer = payer;
    p->amount = amount;
    return 1;
}

int rich4_property_insurance_choose(GameSession *session, int target, uint32_t now)
{
    GamePropertyPresentationState *p;
    if (session == NULL) return 0;
    p = &session->presentation.property;
    if (!p->insurance_active) return 0;
    if (p->phase == GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM) {
        if (target < 0) p->phase = GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT;
        else insurance_card_notice(session, 19, now);
        return 1;
    }
    if (p->phase != GAME_PROPERTY_PRESENT_INSURANCE_TARGET) return 0;
    if (target < 0) {
        p->phase = GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE;
    } else if (target < session->state.player_count && target != p->actor &&
               (session->state.players[target].play_state & 0xff)) {
        p->insurance_target = target;
        if (!insurance_consume_card(session)) return 0;
        p->phase = GAME_PROPERTY_PRESENT_INSURANCE_CARD_SPEECH;
    } else return 0;
    return 1;
}

int rich4_property_insurance_hover(GameSession *session, int target)
{
    GamePropertyPresentationState *p;
    if (session == NULL) return 0;
    p = &session->presentation.property;
    if (!p->insurance_active || p->phase != GAME_PROPERTY_PRESENT_INSURANCE_TARGET ||
        p->insurance_target == target) return 0;
    p->insurance_target = target;
    return 1;
}

/* 0x4448B0 chooses the relationship maximum, then a random available
 * opponent; both selections precede the amount-threshold random draw. */
static int insurance_redirect_ai(GameSession *session)
{
    GamePropertyPresentationState *p = &session->presentation.property;
    GameState *s = &session->state;
    int candidates[GAME_MAX_PLAYERS], count = 0, best = 0, target = -1, i;
    Rich4Rng rng;
    int64_t limit;
    rng.state = s->ai_rng_state;
    for (i = 0; i < s->player_count; i++) {
        const GamePlayerState *v = &s->players[i];
        if (i == p->actor || !(v->play_state & 0xff)) continue;
        if (s->players[p->actor].relation_balance[i] > best) {
            best = s->players[p->actor].relation_balance[i]; target = i;
        }
        if (!v->delayed_status_block && !v->days_disappearing &&
            !v->days_in_prison && !v->days_in_hospital) candidates[count++] = i;
    }
    if (target < 0 && count)
        target = candidates[rich4_rng_original_state_range(&rng, count)];
    limit = (int64_t)(rich4_rng_original_state_range(&rng, 4000) + 4000) * s->price_index;
    s->ai_rng_state = rng.state;
    if (p->amount <= s->players[p->actor].cash && p->amount <= limit) return -1;
    return target;
}

static int insurance_update_passive(GameSession *session, uint32_t now)
{
    GamePropertyPresentationState *p = &session->presentation.property;
    int i;
    switch (p->phase) {
    case GAME_PROPERTY_PRESENT_INSURANCE_FREE:
        p->phase = GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT;
        if (insurance_card_threshold(session) &&
            rich4_card_inventory_has(&session->state, p->actor, 19)) {
            if (!insurance_automatic(session)) {
                p->phase = GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM;
            } else {
                Rich4Rng rng;
                int64_t limit;
                rng.state = session->state.ai_rng_state;
                limit = (int64_t)(rich4_rng_original_state_range(&rng, 3000) + 3000) *
                    session->state.price_index;
                session->state.ai_rng_state = rng.state;
                if (p->amount > session->state.players[p->actor].cash || p->amount > limit)
                    insurance_card_notice(session, 19, now);
            }
        }
        break;
    case GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM:
    case GAME_PROPERTY_PRESENT_INSURANCE_TARGET:
        break; /* interactive callbacks own these waits */
    case GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT:
        p->phase = GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE;
        if (p->amount && insurance_card_threshold(session) &&
            rich4_card_inventory_has(&session->state, p->actor, 18)) {
            if (insurance_automatic(session)) {
                p->insurance_target = insurance_redirect_ai(session);
                if (p->insurance_target >= 0) insurance_card_notice(session, 18, now);
            } else {
                int count = 0;
                p->insurance_target = -1;
                for (i = 0; i < session->state.player_count; i++)
                    if (i != p->actor && (session->state.players[i].play_state & 0xff)) {
                        p->insurance_target = i; count++;
                    }
                if (count) insurance_card_notice(session, 18, now);
                if (count > 1) p->insurance_target = -1;
            }
        }
        break;
    case GAME_PROPERTY_PRESENT_INSURANCE_CARD:
        if (now - p->tick < 1500u) break;
        if (p->insurance_card == 19) {
            if (!insurance_consume_card(session)) break;
            p->phase = GAME_PROPERTY_PRESENT_INSURANCE_CARD_SPEECH;
        } else {
            p->phase = insurance_automatic(session) ?
                GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT_NOTICE : GAME_PROPERTY_PRESENT_INSURANCE_TARGET;
            p->tick = now;
        }
        break;
    case GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT_NOTICE:
        if (now - p->tick < 1500u || !insurance_consume_card(session)) break;
        p->phase = GAME_PROPERTY_PRESENT_INSURANCE_CARD_SPEECH;
        break;
    case GAME_PROPERTY_PRESENT_INSURANCE_CARD_SPEECH:
        p->phase = GAME_PROPERTY_PRESENT_INSURANCE_VICTIM_SPEECH;
        (void)game_start_speech_entry(session, p->actor,
            rich4_character_insurance_card_speech(session->state.players[p->actor].character,
                p->insurance_card == 19 ? 1 : 0), 0, p->phase, now);
        break;
    case GAME_PROPERTY_PRESENT_INSURANCE_VICTIM_SPEECH:
        p->phase = GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE;
        if (p->insurance_card == 18 && p->insurance_target >= 0)
            (void)game_start_speech_entry(session, p->insurance_target,
                rich4_character_insurance_card_speech(
                    session->state.players[p->insurance_target].character, 2), 2, p->phase, now);
        break;
    case GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE:
        p->phase = GAME_PROPERTY_PRESENT_PAYER;
        if (p->amount) for (i = 0; i < session->state.player_count; i++) {
            int god = session->state.players[i].god_info & 0xff;
            if (i != p->insurance_payer && (session->state.players[i].play_state & 0xff) &&
                (god == 14 || god == 15)) {
                p->insurance_payer = i;
                p->phase = GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE_NOTICE;
                p->tick = now;
                break;
            }
        }
        break;
    case GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE_NOTICE:
        if (now - p->tick >= 1500u) p->phase = GAME_PROPERTY_PRESENT_PAYER;
        break;
    default: return 0;
    }
    return 1;
}

/* 0x440CAC/0x4528B9 accept key-up or either mouse-button-up. A skip
 * ends only the current wait, not the next wait in the same sequence. */
int rich4_property_skip_wait(GameSession *session, uint32_t now)
{
    GamePropertyPresentationState *property;
    if (session == NULL) return 0;
    property = &session->presentation.property;
    switch (property->phase) {
    case GAME_PROPERTY_PRESENT_FLASH:
        {
            Rich4AnimationClock *clock = &session->presentation.animation_clock[
                RICH4_ANIMATION_PROPERTY_FLASH];
            int finishing_hold = property->flash_frame >= 16;
            property->flash_frame = 16;
            clock->started = 1; clock->delay_ms = 400;
            clock->updated_at = now;
            clock->tick = finishing_hold ? now - 400u : now;
        }
        return 1;
    case GAME_PROPERTY_PRESENT_AMOUNT:
    case GAME_PROPERTY_PRESENT_BLOCKED_TEXT:
    case GAME_PROPERTY_PRESENT_BEGGAR_AMOUNT:
    case GAME_PROPERTY_PRESENT_GOD_TRADE_REJECTION:
    case GAME_PROPERTY_PRESENT_GOD_FEE:
    case GAME_PROPERTY_PRESENT_GOD_BUILD_TEXT:
    case GAME_PROPERTY_PRESENT_INSURANCE_CARD:
    case GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT_NOTICE:
    case GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE_NOTICE:
        property->tick = now - 1500u;
        return 1;
    default:
        return 0;
    }
}

/* sub_451985 starts its 30 ms message-pump wait after the frame is drawn.
 * Repeated presentations of the same frame must not restart that wait. */
void rich4_property_flash_frame_presented(GameSession *session, uint32_t now)
{
    GamePropertyPresentationState *property;
    Rich4AnimationClock *clock;
    if (session == NULL) return;
    property = &session->presentation.property;
    if (property->phase != GAME_PROPERTY_PRESENT_FLASH || property->flash_frame >= 16 ||
        property->flash_presented_frame == property->flash_frame) return;
    property->flash_presented_frame = property->flash_frame;
    clock = &session->presentation.animation_clock[RICH4_ANIMATION_PROPERTY_FLASH];
    clock->tick = clock->updated_at = now;
}

int rich4_property_update_charge_presentation(
    GameSession *session, uint32_t now)
{
    static const int flash_adjust[16] = {
         4, 8,12,16,12, 8, 4, 0,-4,-8,-12,-16,-12,-8,-4,0
    };
    GamePropertyPresentationState *property;
    if (session == NULL) return 0;
    property = &session->presentation.property;
    if (session->presentation.speech.active) return 1;
    if (property->insurance_active && insurance_update_passive(session, now)) return 1;
    switch (property->phase) {
    case GAME_PROPERTY_PRESENT_NONE:
        return 0;
    case GAME_PROPERTY_PRESENT_FLASH:
        {
            Rich4AnimationClock *clock = &session->presentation.animation_clock[
                RICH4_ANIMATION_PROPERTY_FLASH];
            if (property->flash_frame < 16) {
                if (!clock->started) property->flash_presented_frame = -1;
                if (rich4_animation_clock_step(clock, now, 30))
                    property->flash_frame++;
                property->flash_adjust = property->flash_frame < 16
                    ? flash_adjust[property->flash_frame] : 0;
                if (property->flash_frame == 16) clock->delay_ms = 400;
                return 1;
            }
            if (!rich4_animation_clock_step(clock, now, 400)) return 1;
            rich4_animation_clock_reset(clock);
            property->flash_adjust = 0;
            property->phase = GAME_PROPERTY_PRESENT_AMOUNT;
            property->tick = now;
            return 1;
        }
    case GAME_PROPERTY_PRESENT_AMOUNT:
        if (now - property->tick < 1500) return 1;
        if (property->god_type != 0) {
            property->phase = GAME_PROPERTY_PRESENT_GOD_FEE;
            property->tick = now;
            return 1;
        }
        if (property->insurance_active) {
            property->phase = GAME_PROPERTY_PRESENT_INSURANCE_FREE;
            return 1;
        }
        if (property->property_kind == GAME_DOMAIN_FACILITY) {
            property->phase = GAME_PROPERTY_PRESENT_COMMIT;
            return 1;
        }
        property->phase = GAME_PROPERTY_PRESENT_PAYER;
        return 1;
    case GAME_PROPERTY_PRESENT_GOD_FEE:
        if (now - property->tick < 1500u) return 1;
        if (property->amount == 0) {
            int scale = session->state.price_index;
            int variant = property->god_original_amount >= 5000 * scale &&
                property->god_original_amount < 9000 * scale ? rich4_rng_next_at(&session->state.ai_rng_state) & 1 : 0;
            const Rich4CharacterSpeech *speech = rich4_character_payment_blocked_speech(
                session->state.players[property->actor].character,
                property->god_original_amount, scale, variant);
            property->phase = GAME_PROPERTY_PRESENT_COMMIT;
            (void)game_start_speech_entry(session, property->actor, speech, 3,
                                          property->phase, now);
        } else {
            property->phase = property->insurance_active ?
                GAME_PROPERTY_PRESENT_INSURANCE_FREE : GAME_PROPERTY_PRESENT_PAYER;
        }
        return 1;
    case GAME_PROPERTY_PRESENT_PAYER: {
        Rich4PropertySpeechKind kind;
        int special_random = 0;
        int normal_random = 0;
        Rich4PropertyPresentationPhase after_phase =
            property->property_kind == GAME_DOMAIN_COMPANY ?
                GAME_PROPERTY_PRESENT_COMMIT : GAME_PROPERTY_PRESENT_RECEIVER;
        if (property->insurance_active && property->insurance_payer != property->actor) {
            property->phase = GAME_PROPERTY_PRESENT_COMMIT;
            return 1;
        }
        if (property->property_kind != GAME_DOMAIN_COMPANY &&
            rich4_property_relation_target(&session->state,
                                           property->actor) == property->owner &&
            property->amount >= 5000 * session->state.price_index)
            special_random = rich4_rng_next_at(&session->state.ai_rng_state);
        if (!(special_random & 1) && property->amount >=
                5000 * session->state.price_index && property->amount <
                9000 * session->state.price_index)
            normal_random = rich4_rng_next_at(&session->state.ai_rng_state);
        kind = rich4_property_payer_speech(
            &session->state, property->actor, property->owner,
            property->amount, special_random, normal_random);
        if (kind < RICH4_PROPERTY_SPEECH_COUNT &&
            game_start_property_speech(
                session, property->actor, kind,
                kind == RICH4_PROPERTY_SPEECH_PAYER_RELATION ? 1 : 2,
                after_phase, now)) return 1;
        property->phase = after_phase;
        return 1;
    }
    case GAME_PROPERTY_PRESENT_RECEIVER:
    case GAME_PROPERTY_PRESENT_RELATED_RECEIVER: {
        Rich4PropertySpeechKind kind;
        int normal_random = 0;
        int receiver = property->owner;
        int receiver_amount = property->amount;
        Rich4PropertyPresentationPhase after_phase =
            GAME_PROPERTY_PRESENT_COMMIT;
        if (property->phase == GAME_PROPERTY_PRESENT_RECEIVER &&
            property->related_owner >= 0 && property->related_amount > 0)
            after_phase = GAME_PROPERTY_PRESENT_RELATED_RECEIVER;
        else if (property->phase == GAME_PROPERTY_PRESENT_RELATED_RECEIVER) {
            receiver = property->related_owner;
            receiver_amount = property->related_amount;
        }
        if (receiver_amount >= 5000 * session->state.price_index &&
            receiver_amount < 9000 * session->state.price_index)
            normal_random = rich4_rng_next_at(&session->state.ai_rng_state);
        kind = rich4_property_receiver_speech(&session->state,
                                              receiver_amount, normal_random);
        if (kind < RICH4_PROPERTY_SPEECH_COUNT &&
            game_start_property_speech(session, receiver, kind, 3,
                                       after_phase, now)) return 1;
        property->phase = after_phase;
        return 1;
    }
    case GAME_PROPERTY_PRESENT_COMMIT: {
        GameEventQueue ignored;
        RouteEventExecutionResult result;
        rich4_game_event_queue_reset(&ignored);
        result = property->insurance_active ? rich4_route_event_executor_settle_insurance(
            &session->flow.route_events, &session->state, property->insurance_payer,
            property->amount, &ignored) : rich4_route_event_executor_resume_charge(
                &session->flow.route_events, &session->state, &ignored);
        if (result == ROUTE_EVENT_EXECUTION_INVALID) return 1;
        {
            GameEvent event;
            while (rich4_game_event_queue_pop(&ignored, &event))
                if (!rich4_bankruptcy_capture_event(session, &event, now) &&
                    event.type != GAME_EVENT_ROUTE_EVENT_RESOLVED &&
                    event.type != GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED)
                    (void)rich4_game_event_queue_push(&session->deferred_events, &event);
        }
        property->phase = GAME_PROPERTY_PRESENT_NONE;
        property->insurance_active = 0;
        property->god_type = 0;
        property->property_kind = GAME_DOMAIN_NONE;
        property->land = -1;
        property->group = -1;
        property->flash_adjust = 0;
        return result == ROUTE_EVENT_EXECUTION_INVALID ? 1 : 0;
    }
    case GAME_PROPERTY_PRESENT_BLOCKED_TEXT: {
        int player = property->actor;
        const Rich4CharacterSpeech *speech;
        if (now - property->tick < 1500) return 1;
        if (player < 0 || player >= session->state.player_count) {
            property->phase = GAME_PROPERTY_PRESENT_NONE;
            return 0;
        }
        speech = rich4_character_property_collection_blocked_speech(
            session->state.players[player].character);
        if (game_start_speech_entry(
                session, player, speech, 3,
                GAME_PROPERTY_PRESENT_BLOCKED_COMPLETE, now)) {
            property->phase = GAME_PROPERTY_PRESENT_BLOCKED_SPEECH;
            return 1;
        }
        property->phase = GAME_PROPERTY_PRESENT_BLOCKED_COMPLETE;
        return 1;
    }
    case GAME_PROPERTY_PRESENT_BLOCKED_SPEECH:
        return session->presentation.speech.active ? 1 : 0;
    case GAME_PROPERTY_PRESENT_BLOCKED_COMPLETE:
        property->phase = GAME_PROPERTY_PRESENT_NONE;
        property->collection_block_reason = GAME_PROPERTY_COLLECTION_BLOCK_NONE;
        return 0;
    case GAME_PROPERTY_PRESENT_BEGGAR_AMOUNT: {
        GameEventQueue ignored;
        RouteEventExecutionResult result;
        if (now - property->tick < 1500) return 1;
        rich4_game_event_queue_reset(&ignored);
        result = rich4_route_event_executor_resume_beggar(
            &session->flow.route_events, &session->state, &ignored);
        rich4_bankruptcy_capture_events(session, &ignored, now);
        property->phase = GAME_PROPERTY_PRESENT_NONE;
        property->property_kind = GAME_DOMAIN_NONE;
        property->land = -1;
        property->group = -1;
        property->flash_adjust = 0;
        return result == ROUTE_EVENT_EXECUTION_INVALID ? 1 : 0;
    }
    case GAME_PROPERTY_PRESENT_GOD_BUILD_TEXT:
        if (now - property->tick < 1500u) return 1;
        if (property->god_type == 12) {
            property->phase = GAME_PROPERTY_PRESENT_GOD_BUILD_FINISH;
            (void)game_start_speech_entry(session, property->actor,
                rich4_character_reward_speech(session->state.players[property->actor].character, 1, 0),
                0, GAME_PROPERTY_PRESENT_GOD_BUILD_FINISH, now);
            return 1;
        }
        if (property->god_type == 9 && property->property_kind == GAME_DOMAIN_FACILITY &&
            session->state.facility_level[property->land] == 0 &&
            !session->state.players[property->actor].is_ai &&
            !(session->state.players[property->actor].ai_action_flags & 6)) {
            property->phase = GAME_PROPERTY_PRESENT_GOD_BUILD_CHOICE;
            session->presentation.route_interaction.active = 1;
            session->presentation.route_interaction.pressed = -1;
            session->presentation.route_interaction.accept = rich4_game_command_upgrade_property(
                GAME_COMMAND_SOURCE_HUMAN, property->actor,
                rich4_game_domain_ref(GAME_DOMAIN_FACILITY, property->land), 0);
            session->presentation.route_interaction.decline = session->presentation.route_interaction.accept;
            return 1;
        }
        property->phase = GAME_PROPERTY_PRESENT_GOD_BUILD_COMMIT;
        /* fall through */
    case GAME_PROPERTY_PRESENT_GOD_BUILD_COMMIT:
        if (!commit_god_building(session, now)) return 1;
        property->phase = GAME_PROPERTY_PRESENT_GOD_BUILD_FINISH;
        return session->deferred_events.count == 0;
    case GAME_PROPERTY_PRESENT_GOD_BUILD_CHOICE:
        return 1;
    case GAME_PROPERTY_PRESENT_GOD_BUILD_FINISH:
        if (session->deferred_events.count) return 0; /* let the route adapter drain FLC requests */
        if (session->presentation.tool_animation_count) return 1;
        property->god_type = 0;
        property->phase = GAME_PROPERTY_PRESENT_NONE;
        return 0;
    case GAME_PROPERTY_PRESENT_GOD_TRADE_REJECTION: {
        GameEventQueue ignored;
        RouteEventExecutionResult result;
        if (now - property->tick < 1500) return 1;
        rich4_game_event_queue_reset(&ignored);
        result = rich4_route_event_executor_finish_god_trade_rejection(
            &session->flow.route_events, &session->state, &ignored);
        property->phase = GAME_PROPERTY_PRESENT_NONE;
        property->property_kind = GAME_DOMAIN_NONE;
        property->land = -1;
        property->group = -1;
        property->amount = 0;
        property->flash_adjust = 0;
        return result == ROUTE_EVENT_EXECUTION_INVALID ? 1 : 0;
    }
    default:
        property->phase = GAME_PROPERTY_PRESENT_NONE;
        property->flash_adjust = 0;
        return 0;
    }
}
