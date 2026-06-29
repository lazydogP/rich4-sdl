#include "core/game_event_payload.h"
#include "core/game_event_internal.h"
#include "core/god.h"

#include <stddef.h>

static GameDomainRef none_domain_ref(void)
{
    GameDomainRef value;
    value.kind = GAME_DOMAIN_NONE;
    value.id = -1;
    return value;
}

GameEvent rich4_game_event_special_place_notice(
    int player, GameSpecialPlaceNoticeKind kind, GameDomainRef target,
    int amount, int hold_ms)
{
    Rich4GameEventFields fields = {
        player, kind, amount, hold_ms, 0,
        { GAME_DOMAIN_NONE, -1 }, { GAME_DOMAIN_NONE, -1 }
    };
    fields.target_ref = target;
    return rich4_game_event_special_place_notice_requested_from_fields(fields);
}

int rich4_game_event_emit_typed(
    GameEventQueue *queue, Rich4GameEventConstructor constructor,
    Rich4GameEventFields fields)
{
    GameEvent event;
    if (queue == NULL || constructor == NULL) return 0;
    event = constructor(fields);
    if (!rich4_game_event_is_valid(&event)) return 0;
    return rich4_game_event_queue_push(queue, &event);
}

int rich4_game_event_emit_typed_domain(
    GameEventQueue *queue, Rich4GameEventConstructor constructor, int player,
    GameDomainRef subject_ref, GameDomainRef target_ref, int value, int value2,
    unsigned int flags)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = 0;
    fields.value = value;
    fields.value2 = value2;
    fields.flags = flags;
    fields.subject_ref = subject_ref;
    fields.target_ref = target_ref;
    return rich4_game_event_emit_typed(queue, constructor, fields);
}

int rich4_game_event_insurance_company(const GameEvent *event)
{
    GameDomainRef ref;
    if (event == NULL ||
        (event->type != GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED &&
         event->type != GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED))
        return -1;
    ref = rich4_game_event_subject_ref(event);
    return ref.kind == GAME_DOMAIN_COMPANY ? ref.id : -1;
}

int rich4_game_event_insurance_claimant(const GameEvent *event)
{
    GameDomainRef ref;
    if (event == NULL ||
        (event->type != GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED &&
         event->type != GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED))
        return -1;
    ref = rich4_game_event_target_ref(event);
    return ref.kind == GAME_DOMAIN_PLAYER ? ref.id : -1;
}

int rich4_game_event_insurance_amount(const GameEvent *event)
{
    return event != NULL &&
                   (event->type == GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED ||
                    event->type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED)
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_insurance_duration(const GameEvent *event)
{
    return event != NULL &&
                   (event->type == GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED ||
                    event->type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED)
        ? rich4_game_event_value2(event) : 0;
}

unsigned int rich4_game_event_insurance_cause(const GameEvent *event)
{
    return event != NULL &&
                   (event->type == GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED ||
                    event->type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED)
        ? (event->type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED
            ? rich4_game_event_flags(event) & 0xffu
            : rich4_game_event_flags(event)) : 0u;
}

int rich4_game_event_bank_loan_principal(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED
        ? rich4_game_event_subject(event) : 0;
}

int rich4_game_event_bank_loan_days(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_bank_loan_payment(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED
        ? rich4_game_event_value2(event) : 0;
}

unsigned int rich4_game_event_bank_loan_forced(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED
        ? rich4_game_event_flags(event) : 0u;
}

int rich4_game_event_bank_settlement_interest(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_BANK_SETTLEMENT_APPLIED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_bank_settlement_balance(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_BANK_SETTLEMENT_APPLIED
        ? rich4_game_event_value2(event) : 0;
}

unsigned int rich4_game_event_bank_settlement_loan_outstanding(
    const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_BANK_SETTLEMENT_APPLIED
        ? rich4_game_event_flags(event) : 0u;
}

int rich4_game_event_route_move_result(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_ROUTE_MOVE_RESOLVED
        ? rich4_game_event_subject(event) : 0;
}

int rich4_game_event_player_eliminated_play_state(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_PLAYER_ELIMINATED
        ? rich4_game_event_subject(event) : 0;
}

int rich4_game_event_route_move_origin(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_ROUTE_MOVE_RESOLVED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_route_move_next_node(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_ROUTE_MOVE_RESOLVED
        ? rich4_game_event_value2(event) : 0;
}

int rich4_game_event_route_event_phase(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_ROUTE_EVENT_RESOLVED
        ? rich4_game_event_subject(event) : 0;
}

int rich4_game_event_route_event_result(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_ROUTE_EVENT_RESOLVED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_route_event_node(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_ROUTE_EVENT_RESOLVED
        ? rich4_game_event_value2(event) : 0;
}

unsigned int rich4_game_event_route_event_object_id(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_ROUTE_EVENT_RESOLVED
        ? rich4_game_event_flags(event) : 0u;
}

int rich4_game_event_interaction_accept_type(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_INTERACTION_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_interaction_decline_type(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_INTERACTION_REQUESTED
        ? rich4_game_event_value2(event) : 0;
}

unsigned int rich4_game_event_interaction_turn_token(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_INTERACTION_REQUESTED
        ? rich4_game_event_flags(event) : 0u;
}

GameDomainRef rich4_game_event_auction_property(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_AUCTION_REQUESTED
        ? rich4_game_event_subject_ref(event) : none_domain_ref();
}

int rich4_game_event_auction_seller(const GameEvent *event)
{
    GameDomainRef seller;
    if (event == NULL || event->type != GAME_EVENT_AUCTION_REQUESTED) return -2;
    seller = rich4_game_event_target_ref(event);
    if (seller.kind == GAME_DOMAIN_NONE) return -1;
    return seller.kind == GAME_DOMAIN_PLAYER ? seller.id : -2;
}

int rich4_game_event_auction_manage_music(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_AUCTION_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_auction_trigger(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_AUCTION_REQUESTED
        ? rich4_game_event_value2(event) : 0;
}

unsigned int rich4_game_event_auction_flags(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_AUCTION_REQUESTED
        ? rich4_game_event_flags(event) : 0u;
}

GameDomainRef rich4_game_event_interaction_subject(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_INTERACTION_REQUESTED
        ? rich4_game_event_subject_ref(event) : none_domain_ref();
}

GameDomainRef rich4_game_event_interaction_target(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_INTERACTION_REQUESTED
        ? rich4_game_event_target_ref(event) : none_domain_ref();
}

GameDomainRef rich4_game_event_property_ref(const GameEvent *event)
{
    return event != NULL &&
                   (event->type == GAME_EVENT_PROPERTY_OWNER_CHANGED ||
                    event->type == GAME_EVENT_PROPERTY_LEVEL_CHANGED ||
                    event->type == GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED)
        ? rich4_game_event_subject_ref(event) : none_domain_ref();
}

int rich4_game_event_property_owner(const GameEvent *event)
{
    GameDomainRef target;
    if (event == NULL ||
        (event->type != GAME_EVENT_PROPERTY_OWNER_CHANGED &&
         event->type != GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED))
        return -1;
    target = rich4_game_event_target_ref(event);
    return target.kind == GAME_DOMAIN_PLAYER ? target.id : -1;
}

int rich4_game_event_property_value(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_PROPERTY_OWNER_CHANGED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_property_level(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_PROPERTY_LEVEL_CHANGED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_property_market_value(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_PROPERTY_LEVEL_CHANGED
        ? rich4_game_event_value2(event) : 0;
}

int rich4_game_event_status_offset(const GameEvent *event)
{
    return event != NULL &&
                   (event->type == GAME_EVENT_STATUS_CHANGED ||
                    event->type == GAME_EVENT_STATUS_EXPIRED)
        ? rich4_game_event_subject(event) : 0;
}

int rich4_game_event_status_value(const GameEvent *event)
{
    return event != NULL &&
                   (event->type == GAME_EVENT_STATUS_CHANGED ||
                    event->type == GAME_EVENT_STATUS_EXPIRED)
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_status_effect_handler(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_STATUS_EFFECT_RESOLVED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_status_effect_result(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_STATUS_EFFECT_RESOLVED
        ? rich4_game_event_value2(event) : 0;
}

GameDomainRef rich4_game_event_status_target(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_STATUS_CHANGED
        ? rich4_game_event_target_ref(event) : none_domain_ref();
}

GameDomainRef rich4_game_event_status_subject(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_STATUS_CHANGED
        ? rich4_game_event_subject_ref(event) : none_domain_ref();
}

int rich4_game_event_special_commit_id(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_SPECIAL_EVENT_COMMITTED
        ? rich4_game_event_subject(event) : -1;
}

GameDomainRef rich4_game_event_special_commit_subject(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_SPECIAL_EVENT_COMMITTED
        ? rich4_game_event_subject_ref(event) : none_domain_ref();
}

int rich4_game_event_special_commit_amount(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_SPECIAL_EVENT_COMMITTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_special_commit_defense(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_SPECIAL_EVENT_COMMITTED
        ? rich4_game_event_value2(event) : 0;
}

unsigned int rich4_game_event_special_commit_family(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_SPECIAL_EVENT_COMMITTED
        ? rich4_game_event_flags(event) : 0u;
}

int rich4_game_event_map_object_tool(const GameEvent *event)
{
    GameDomainRef subject;
    if (event == NULL || event->type != GAME_EVENT_MAP_OBJECT_CHANGED)
        return -1;
    subject = rich4_game_event_subject_ref(event);
    return subject.kind == GAME_DOMAIN_TOOL ? subject.id : -1;
}

GameDomainRef rich4_game_event_map_object_subject(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_MAP_OBJECT_CHANGED
        ? rich4_game_event_subject_ref(event) : none_domain_ref();
}

int rich4_game_event_map_object_effect(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_MAP_OBJECT_CHANGED
        ? rich4_game_event_value2(event) : 0;
}

unsigned int rich4_game_event_map_object_flags(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_MAP_OBJECT_CHANGED
        ? rich4_game_event_flags(event) : 0u;
}

int rich4_game_event_victory_reason(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_VICTORY_CANDIDATE
        ? rich4_game_event_subject(event) : 0;
}

int rich4_game_event_victory_assets(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_VICTORY_CANDIDATE
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_game_ended_reason(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_GAME_ENDED
        ? rich4_game_event_subject(event) : 0;
}

int rich4_game_event_game_ended_assets(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_GAME_ENDED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_confinement_kind(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_CONFINEMENT_VISIT_REQUESTED
        ? rich4_game_event_subject(event) : 0;
}

int rich4_game_event_special_place_action(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED
        ? rich4_game_event_subject(event) : -1;
}

int rich4_game_event_special_place_resource(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_special_place_hold_ms(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED
        ? rich4_game_event_value2(event) : 0;
}

int rich4_game_event_bank_rejection_days(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_BANK_REJECTION_PRESENTATION_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_bank_rejection_duration(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_BANK_REJECTION_PRESENTATION_REQUESTED
        ? rich4_game_event_value2(event) : 0;
}

int rich4_game_event_god_trade_type(const GameEvent *event)
{
    return event != NULL && event->type ==
                   GAME_EVENT_GOD_TRADE_REJECTION_PRESENTATION_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_god_trade_duration(const GameEvent *event)
{
    return event != NULL && event->type ==
                   GAME_EVENT_GOD_TRADE_REJECTION_PRESENTATION_REQUESTED
        ? rich4_game_event_value2(event) : 0;
}

unsigned int rich4_game_event_god_trade_turn_token(const GameEvent *event)
{
    return event != NULL && event->type ==
                   GAME_EVENT_GOD_TRADE_REJECTION_PRESENTATION_REQUESTED
        ? rich4_game_event_flags(event) : 0u;
}

int rich4_game_event_tool_resource(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_tool_effect(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED
        ? rich4_game_event_value2(event) : 0;
}

int rich4_game_event_tool_confinement_days(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED &&
        (rich4_game_event_tool_resource(event) == 524 ||
         rich4_game_event_tool_resource(event) == 538)
        ? (int)rich4_game_event_flags(event) : 0;
}

GameDomainRef rich4_game_event_tool_target(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED
        ? rich4_game_event_target_ref(event) : none_domain_ref();
}

int rich4_game_event_tool_followup_slot(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_MAP_OBJECT_CHANGED
        ? rich4_game_event_value(event) : -1;
}

GameDomainRef rich4_game_event_inventory_subject(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_INVENTORY_CHANGED
        ? rich4_game_event_subject_ref(event) : none_domain_ref();
}

GameDomainRef rich4_game_event_voice_subject(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_VOICE_REQUESTED
        ? rich4_game_event_subject_ref(event) : none_domain_ref();
}

GameDomainRef rich4_game_event_voice_target(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_VOICE_REQUESTED
        ? rich4_game_event_target_ref(event) : none_domain_ref();
}

int rich4_game_event_voice_code(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_VOICE_REQUESTED
        ? rich4_game_event_subject(event) : 0;
}

int rich4_game_event_property_charge_amount(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_property_charge_block_reason(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED
        ? rich4_game_event_value2(event) : 0;
}

int rich4_game_event_god_type(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_GOD_ACQUIRED
        ? rich4_game_event_subject(event) : 0;
}

int rich4_game_event_god_object_slot(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_GOD_ACQUIRED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_god_previous_type(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_GOD_ACQUIRED
        ? rich4_game_event_value2(event) : 0;
}

unsigned int rich4_game_event_god_flags(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_GOD_ACQUIRED
        ? rich4_game_event_flags(event) : 0u;
}

int rich4_game_event_god_result_card(const GameEvent *event, int which)
{
    GameDomainRef ref;
    unsigned int flags;
    unsigned int wanted;

    if (event == NULL || event->type != GAME_EVENT_GOD_ACQUIRED ||
        (which != 0 && which != 1))
        return -1;
    flags = rich4_game_event_god_flags(event);
    wanted = which == 0 ? RICH4_GOD_ACQUIRED_RESULT_CARD1 :
                          RICH4_GOD_ACQUIRED_RESULT_CARD2;
    if ((flags & wanted) == 0) return -1;
    ref = which == 0 ? rich4_game_event_subject_ref(event) :
                       rich4_game_event_target_ref(event);
    return ref.kind == GAME_DOMAIN_CARD && ref.id >= 0 &&
           ref.id < GAME_CARD_COUNT ? ref.id : -1;
}

int rich4_game_event_god_result_lost_count(const GameEvent *event)
{
    unsigned int flags;
    if (event == NULL || event->type != GAME_EVENT_GOD_ACQUIRED) return 0;
    flags = rich4_game_event_god_flags(event);
    return (int)((flags & RICH4_GOD_ACQUIRED_LOST_COUNT_MASK) >>
                 RICH4_GOD_ACQUIRED_LOST_COUNT_SHIFT);
}

GameEvent rich4_game_event_special_event_presentation(
    int player, int resource, int panel, unsigned int family)
{
    return rich4_game_event_internal(
        GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED,
        player, resource, panel, 0, family);
}

GameEvent rich4_game_event_facility_roulette_request(
    int player, GameDomainRef subject, int mode, int panel_resource,
    int data_resource, unsigned int turn_token)
{
    GameEvent event = rich4_game_domain_event_internal(
        GAME_EVENT_FACILITY_ROULETTE_REQUESTED, player, mode, subject,
        none_domain_ref(), panel_resource, data_resource, turn_token);
    return event;
}

GameEvent rich4_game_event_company_stock_issue_request(
    int player, int company, GameDomainRef stock, int price, int maximum,
    unsigned int turn_token)
{
    GameEvent event = rich4_game_domain_event_internal(
        GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED, player, company,
        rich4_game_domain_ref(GAME_DOMAIN_COMPANY, company), stock,
        price, maximum, turn_token);
    return event;
}

GameEvent rich4_game_event_shop_visit_request(
    int player, int action, int panel_resource, int panel_aux_resource,
    GameDomainRef gift, unsigned int turn_token)
{
    GameEvent event = rich4_game_domain_event_internal(
        GAME_EVENT_SHOP_VISIT_REQUESTED, player, action, gift, none_domain_ref(),
        panel_resource, panel_aux_resource, turn_token);
    return event;
}

GameEvent rich4_game_event_magic_house_request(
    int player, int condition, unsigned int target_mask)
{
    GameEvent event = rich4_game_domain_event_internal(
        GAME_EVENT_MAGIC_HOUSE_REQUESTED, player, condition, none_domain_ref(),
        none_domain_ref(), 0, 0, target_mask);
    return event;
}

GameEvent rich4_game_event_magic_house_auto_request(
    int player, int condition, unsigned int target_mask, int action)
{
    /* 0x4339BD: preserve the chosen action across the 1500 ms notice.
     * Zero is the existing human request; action+1 marks automatic mode. */
    return rich4_game_domain_event_internal(
        GAME_EVENT_MAGIC_HOUSE_REQUESTED, player, condition, none_domain_ref(),
        none_domain_ref(), action + 1, 0, target_mask);
}

GameEvent rich4_game_event_special_place_request(
    int player, int action, int resource, int hold_ms,
    unsigned int turn_token)
{
    return rich4_game_event_internal(
        GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED,
        player, action, resource, hold_ms, turn_token);
}

GameEvent rich4_game_event_confinement_visit_request(
    int player, int kind, int panel_resource, int panel_aux_resource,
    unsigned int turn_token)
{
    return rich4_game_event_internal(
        GAME_EVENT_CONFINEMENT_VISIT_REQUESTED,
        player, kind, panel_resource, panel_aux_resource, turn_token);
}

GameEvent rich4_game_event_point_minigame_request(
    int player, int action, int panel_resource, int panel_aux_resource,
    unsigned int turn_token)
{
    return rich4_game_event_internal(
        GAME_EVENT_POINT_MINIGAME_REQUESTED,
        player, action, panel_resource, panel_aux_resource, turn_token);
}

GameEvent rich4_game_event_lottery_visit_request(
    int player, int action, int panel_resource, int panel_aux_resource,
    unsigned int flags)
{
    return rich4_game_event_internal(
        GAME_EVENT_LOTTERY_VISIT_REQUESTED,
        player, action, panel_resource, panel_aux_resource, flags);
}

GameEvent rich4_game_event_bank_rejection(
    int player, int days_remaining, int duration_ms, unsigned int turn_token)
{
    return rich4_game_event_internal(
        GAME_EVENT_BANK_REJECTION_PRESENTATION_REQUESTED,
        player, 14, days_remaining, duration_ms, turn_token);
}

GameEvent rich4_game_event_god_trade_rejection(
    int player, int god_type, int duration_ms, unsigned int turn_token)
{
    return rich4_game_domain_event_internal(
        GAME_EVENT_GOD_TRADE_REJECTION_PRESENTATION_REQUESTED,
        player, 0, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), god_type, duration_ms,
        turn_token);
}

GameEvent rich4_game_event_insurance_fee(
    int player, int company, int chairman, int quoted_fee, int duration_ms,
    unsigned int stock)
{
    return rich4_game_domain_event_internal(
        GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED, player, 0,
        rich4_game_domain_ref(GAME_DOMAIN_COMPANY, company),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, chairman), quoted_fee,
        duration_ms, stock);
}

GameEvent rich4_game_event_insurance_claim(
    int player, int company, int claimant, int payout, int duration_ms,
    unsigned int cause)
{
    return rich4_game_domain_event_internal(
        GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED, player, 0,
        rich4_game_domain_ref(GAME_DOMAIN_COMPANY, company),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, claimant), payout,
        duration_ms, cause);
}

GameEvent rich4_game_event_bank_loan_due(
    int player, int principal, int days_remaining, int payment,
    unsigned int forced)
{
    return rich4_game_event_internal(
        GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED, player, principal,
        days_remaining, payment, forced);
}

GameEvent rich4_game_event_bank_settlement(
    int player, int interest, int bank_after, unsigned int loan_outstanding)
{
    return rich4_game_event_internal(
        GAME_EVENT_BANK_SETTLEMENT_APPLIED, player, 25, interest, bank_after,
        loan_outstanding);
}

int rich4_game_event_company_control_company(const GameEvent *event)
{
    GameDomainRef target;
    if (event == NULL || event->type != GAME_EVENT_MARKET_CHANGED ||
        rich4_game_event_value(event) == rich4_game_event_value2(event) ||
        rich4_game_event_value2(event) != event->player) return -1;
    target = rich4_game_event_target_ref(event);
    return target.kind == GAME_DOMAIN_COMPANY ? target.id : -1;
}
