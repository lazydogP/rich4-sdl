#ifndef RICH4_CORE_GAME_EVENT_PAYLOAD_H
#define RICH4_CORE_GAME_EVENT_PAYLOAD_H

#include "core/game_event.h"

typedef GameEvent (*Rich4GameEventConstructor)(Rich4GameEventFields fields);

/* Pushes an event only through a named typed constructor. */
int rich4_game_event_emit_typed(
    GameEventQueue *queue, Rich4GameEventConstructor constructor,
    Rich4GameEventFields fields);

/* Domain-aware variant for events whose semantic subject/target are domain
 * references rather than only scalar identifiers. */
int rich4_game_event_emit_typed_domain(
    GameEventQueue *queue, Rich4GameEventConstructor constructor, int player,
    GameDomainRef subject_ref, GameDomainRef target_ref, int value, int value2,
    unsigned int flags);

#define RICH4_GAME_EVENT_TYPED_KINDS(X) \
    X(command_rejected, GAME_EVENT_COMMAND_REJECTED) \
    X(turn_flow_requested, GAME_EVENT_TURN_FLOW_REQUESTED) \
    X(dice_roll_requested, GAME_EVENT_DICE_ROLL_REQUESTED) \
    X(dice_count_changed, GAME_EVENT_DICE_COUNT_CHANGED) \
    X(actor_refresh_requested, GAME_EVENT_ACTOR_REFRESH_REQUESTED) \
    X(scene_refresh_requested, GAME_EVENT_SCENE_REFRESH_REQUESTED) \
    X(text_requested, GAME_EVENT_TEXT_REQUESTED) \
    X(voice_requested, GAME_EVENT_VOICE_REQUESTED) \
    X(status_tick_requested, GAME_EVENT_STATUS_TICK_REQUESTED) \
    X(status_expired, GAME_EVENT_STATUS_EXPIRED) \
    X(status_effect_resolved, GAME_EVENT_STATUS_EFFECT_RESOLVED) \
    X(actor_dirty_requested, GAME_EVENT_ACTOR_DIRTY_REQUESTED) \
    X(full_round_completed, GAME_EVENT_FULL_ROUND_COMPLETED) \
    X(day_advanced, GAME_EVENT_DAY_ADVANCED) \
    X(normal_state_cleared, GAME_EVENT_NORMAL_STATE_CLEARED) \
    X(victory_candidate, GAME_EVENT_VICTORY_CANDIDATE) \
    X(route_move_resolved, GAME_EVENT_ROUTE_MOVE_RESOLVED) \
    X(route_event_resolved, GAME_EVENT_ROUTE_EVENT_RESOLVED) \
    X(money_transferred, GAME_EVENT_MONEY_TRANSFERRED) \
    X(property_owner_changed, GAME_EVENT_PROPERTY_OWNER_CHANGED) \
    X(property_level_changed, GAME_EVENT_PROPERTY_LEVEL_CHANGED) \
    X(inventory_changed, GAME_EVENT_INVENTORY_CHANGED) \
    X(status_changed, GAME_EVENT_STATUS_CHANGED) \
    X(market_changed, GAME_EVENT_MARKET_CHANGED) \
    X(interaction_requested, GAME_EVENT_INTERACTION_REQUESTED) \
    X(player_eliminated, GAME_EVENT_PLAYER_ELIMINATED) \
    X(game_ended, GAME_EVENT_GAME_ENDED) \
    X(save_result, GAME_EVENT_SAVE_RESULT) \
    X(load_result, GAME_EVENT_LOAD_RESULT) \
    X(liquidation_requested, GAME_EVENT_LIQUIDATION_REQUESTED) \
    X(card_used, GAME_EVENT_CARD_USED) \
    X(player_cash_changed, GAME_EVENT_PLAYER_CASH_CHANGED) \
    X(tool_used, GAME_EVENT_TOOL_USED) \
    X(map_object_changed, GAME_EVENT_MAP_OBJECT_CHANGED) \
    X(traffic_changed, GAME_EVENT_TRAFFIC_CHANGED) \
    X(special_event_prepared, GAME_EVENT_SPECIAL_EVENT_PREPARED) \
    X(special_event_presentation_requested, GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED) \
    X(special_event_committed, GAME_EVENT_SPECIAL_EVENT_COMMITTED) \
    X(special_event_canceled, GAME_EVENT_SPECIAL_EVENT_CANCELED) \
    X(property_charge_presentation_requested, GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED) \
    X(tool_presentation_requested, GAME_EVENT_TOOL_PRESENTATION_REQUESTED) \
    X(tool_area_resolved, GAME_EVENT_TOOL_AREA_RESOLVED) \
    X(special_place_presentation_requested, GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED) \
    X(confinement_visit_requested, GAME_EVENT_CONFINEMENT_VISIT_REQUESTED) \
    X(point_minigame_automatic_resolved, GAME_EVENT_POINT_MINIGAME_AUTOMATIC_RESOLVED) \
    X(lottery_visit_requested, GAME_EVENT_LOTTERY_VISIT_REQUESTED) \
    X(point_minigame_requested, GAME_EVENT_POINT_MINIGAME_REQUESTED) \
    X(shop_visit_requested, GAME_EVENT_SHOP_VISIT_REQUESTED) \
    X(bank_rejection_presentation_requested, GAME_EVENT_BANK_REJECTION_PRESENTATION_REQUESTED) \
    X(facility_roulette_requested, GAME_EVENT_FACILITY_ROULETTE_REQUESTED) \
    X(company_stock_issue_requested, GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED) \
    X(magic_house_requested, GAME_EVENT_MAGIC_HOUSE_REQUESTED) \
    X(insurance_fee_presentation_requested, GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED) \
    X(insurance_claim_presentation_requested, GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED) \
    X(bank_settlement_applied, GAME_EVENT_BANK_SETTLEMENT_APPLIED) \
    X(bank_loan_due_presentation_requested, GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED) \
    X(auction_requested, GAME_EVENT_AUCTION_REQUESTED) \
    X(god_acquired, GAME_EVENT_GOD_ACQUIRED) \
    X(listing_changed, GAME_EVENT_LISTING_CHANGED) \
    X(beggar_donation_presentation_requested, GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED) \
    X(god_trade_rejection_presentation_requested, GAME_EVENT_GOD_TRADE_REJECTION_PRESENTATION_REQUESTED) \
    X(periodic_finance_requested, GAME_EVENT_PERIODIC_FINANCE_REQUESTED) \
    X(special_place_notice_requested, GAME_EVENT_SPECIAL_PLACE_NOTICE_REQUESTED)

GameEvent rich4_game_event_special_place_notice(
    int player, GameSpecialPlaceNoticeKind kind, GameDomainRef target,
    int amount, int hold_ms);

/* Company issue only: changed chairman and this actor gained control. */
int rich4_game_event_company_control_company(const GameEvent *event);
Rich4GameEventFields rich4_game_event_fields(const GameEvent *event);

/* Semantic route constructors used by core movement/executor producers. */
GameEvent rich4_game_event_route_move_resolved(
    int player, int result, int origin, int next_node);
int rich4_game_event_emit_route_move_resolved(
    GameEventQueue *queue, int player, int result, int origin, int next_node);
GameEvent rich4_game_event_route_event_resolved(
    int player, int phase, int result, int node, unsigned int object_id);
int rich4_game_event_emit_route_event_resolved(
    GameEventQueue *queue, int player, int phase, int result, int node,
    unsigned int object_id);
GameEvent rich4_game_event_interaction_requested(
    int player, int subject, GameDomainRef subject_ref, GameDomainRef target,
    int accept_type, int decline_type, unsigned int turn_token);
int rich4_game_event_emit_interaction_requested(
    GameEventQueue *queue, int player, int subject, GameDomainRef subject_ref,
    GameDomainRef target, int accept_type, int decline_type,
    unsigned int turn_token);

#define RICH4_DECLARE_TYPED_EVENT(name, type) \
    GameEvent rich4_game_event_##name##_from_fields(Rich4GameEventFields fields); \
    int rich4_game_event_##name##_emit(GameEventQueue *queue, int player, \
                                       int subject, int value, int value2, \
                                       unsigned int flags); \
    int rich4_game_event_##name##_is_valid(const GameEvent *event); \
    Rich4GameEventFields rich4_game_event_##name##_fields(const GameEvent *event);
RICH4_GAME_EVENT_TYPED_KINDS(RICH4_DECLARE_TYPED_EVENT)
#undef RICH4_DECLARE_TYPED_EVENT

GameEvent rich4_game_event_special_event_presentation(
    int player, int resource, int panel, unsigned int family);
GameEvent rich4_game_event_facility_roulette_request(
    int player, GameDomainRef subject, int mode, int panel_resource,
    int data_resource, unsigned int turn_token);
GameEvent rich4_game_event_company_stock_issue_request(
    int player, int company, GameDomainRef stock, int price, int maximum,
    unsigned int turn_token);
GameEvent rich4_game_event_shop_visit_request(
    int player, int action, int panel_resource, int panel_aux_resource,
    GameDomainRef gift, unsigned int turn_token);
GameEvent rich4_game_event_magic_house_request(
    int player, int condition, unsigned int target_mask);
GameEvent rich4_game_event_special_place_request(
    int player, int action, int resource, int hold_ms,
    unsigned int turn_token);
GameEvent rich4_game_event_confinement_visit_request(
    int player, int kind, int panel_resource, int panel_aux_resource,
    unsigned int turn_token);
GameEvent rich4_game_event_point_minigame_request(
    int player, int action, int panel_resource, int panel_aux_resource,
    unsigned int turn_token);
GameEvent rich4_game_event_lottery_visit_request(
    int player, int action, int panel_resource, int panel_aux_resource,
    unsigned int flags);
GameEvent rich4_game_event_bank_rejection(
    int player, int days_remaining, int duration_ms, unsigned int turn_token);
GameEvent rich4_game_event_god_trade_rejection(
    int player, int god_type, int duration_ms, unsigned int turn_token);
GameEvent rich4_game_event_insurance_fee(
    int player, int company, int chairman, int quoted_fee, int duration_ms,
    unsigned int stock);
GameEvent rich4_game_event_insurance_claim(
    int player, int company, int claimant, int payout, int duration_ms,
    unsigned int cause);
GameEvent rich4_game_event_bank_loan_due(
    int player, int principal, int days_remaining, int payment,
    unsigned int forced);
GameEvent rich4_game_event_bank_settlement(
    int player, int interest, int bank_after, unsigned int loan_outstanding);
GameEvent rich4_game_event_auction_request(
    int player, GameDomainRef property, int seller, int manage_music,
    int trigger, unsigned int flags);

int rich4_game_event_special_event_resource(const GameEvent *event);
int rich4_game_event_special_event_panel(const GameEvent *event);
unsigned int rich4_game_event_special_event_family(const GameEvent *event);
int rich4_game_event_facility_mode(const GameEvent *event);
GameDomainRef rich4_game_event_facility_subject(const GameEvent *event);
int rich4_game_event_facility_panel_resource(const GameEvent *event);
int rich4_game_event_facility_data_resource(const GameEvent *event);
int rich4_game_event_company_issue_company(const GameEvent *event);
GameDomainRef rich4_game_event_company_issue_stock(const GameEvent *event);
int rich4_game_event_company_issue_price(const GameEvent *event);
int rich4_game_event_company_issue_maximum(const GameEvent *event);
int rich4_game_event_shop_action(const GameEvent *event);
GameDomainRef rich4_game_event_shop_gift(const GameEvent *event);
GameEvent rich4_game_event_magic_house_auto_request(
    int player, int condition, unsigned int target_mask, int action);
int rich4_game_event_magic_house_auto_action(const GameEvent *event);
int rich4_game_event_magic_house_condition(const GameEvent *event);
unsigned int rich4_game_event_magic_house_target_mask(const GameEvent *event);
int rich4_game_event_point_action(const GameEvent *event);
int rich4_game_event_point_panel_resource(const GameEvent *event);
int rich4_game_event_point_data_resource(const GameEvent *event);
int rich4_game_event_insurance_company(const GameEvent *event);
int rich4_game_event_insurance_claimant(const GameEvent *event);
int rich4_game_event_insurance_amount(const GameEvent *event);
int rich4_game_event_insurance_duration(const GameEvent *event);
unsigned int rich4_game_event_insurance_cause(const GameEvent *event);
int rich4_game_event_bank_loan_principal(const GameEvent *event);
int rich4_game_event_bank_loan_days(const GameEvent *event);
int rich4_game_event_bank_loan_payment(const GameEvent *event);
unsigned int rich4_game_event_bank_loan_forced(const GameEvent *event);
int rich4_game_event_bank_settlement_interest(const GameEvent *event);
int rich4_game_event_bank_settlement_balance(const GameEvent *event);
unsigned int rich4_game_event_bank_settlement_loan_outstanding(
    const GameEvent *event);
GameDomainRef rich4_game_event_auction_property(const GameEvent *event);
int rich4_game_event_auction_seller(const GameEvent *event);
int rich4_game_event_auction_manage_music(const GameEvent *event);
int rich4_game_event_auction_trigger(const GameEvent *event);
unsigned int rich4_game_event_auction_flags(const GameEvent *event);
int rich4_game_event_beggar_player(const GameEvent *event);
int rich4_game_event_beggar_node(const GameEvent *event);
int rich4_game_event_beggar_amount(const GameEvent *event);
unsigned int rich4_game_event_beggar_turn_token(const GameEvent *event);
int rich4_game_event_god_trade_type(const GameEvent *event);
int rich4_game_event_god_trade_duration(const GameEvent *event);
unsigned int rich4_game_event_god_trade_turn_token(const GameEvent *event);

/* Named accessors for the remaining route, interaction, property and status
 * payloads.  Platform code must use these semantic names instead of decoding
 * the legacy scalar slots directly. */
int rich4_game_event_route_move_result(const GameEvent *event);
int rich4_game_event_route_move_origin(const GameEvent *event);
int rich4_game_event_route_move_next_node(const GameEvent *event);
int rich4_game_event_route_event_phase(const GameEvent *event);
int rich4_game_event_route_event_result(const GameEvent *event);
int rich4_game_event_route_event_node(const GameEvent *event);
unsigned int rich4_game_event_route_event_object_id(const GameEvent *event);
int rich4_game_event_interaction_accept_type(const GameEvent *event);
int rich4_game_event_interaction_decline_type(const GameEvent *event);
unsigned int rich4_game_event_interaction_turn_token(const GameEvent *event);
GameDomainRef rich4_game_event_interaction_subject(const GameEvent *event);
GameDomainRef rich4_game_event_interaction_target(const GameEvent *event);
GameDomainRef rich4_game_event_property_ref(const GameEvent *event);
int rich4_game_event_property_owner(const GameEvent *event);
int rich4_game_event_property_value(const GameEvent *event);
int rich4_game_event_property_level(const GameEvent *event);
int rich4_game_event_property_market_value(const GameEvent *event);
int rich4_game_event_status_offset(const GameEvent *event);
int rich4_game_event_status_value(const GameEvent *event);
int rich4_game_event_status_effect_handler(const GameEvent *event);
int rich4_game_event_status_effect_result(const GameEvent *event);
GameDomainRef rich4_game_event_status_target(const GameEvent *event);
GameDomainRef rich4_game_event_status_subject(const GameEvent *event);
int rich4_game_event_special_commit_id(const GameEvent *event);
GameDomainRef rich4_game_event_special_commit_subject(const GameEvent *event);
int rich4_game_event_special_commit_amount(const GameEvent *event);
int rich4_game_event_special_commit_defense(const GameEvent *event);
unsigned int rich4_game_event_special_commit_family(const GameEvent *event);
int rich4_game_event_map_object_tool(const GameEvent *event);
GameDomainRef rich4_game_event_map_object_subject(const GameEvent *event);
int rich4_game_event_map_object_effect(const GameEvent *event);
unsigned int rich4_game_event_map_object_flags(const GameEvent *event);
int rich4_game_event_victory_reason(const GameEvent *event);
int rich4_game_event_victory_assets(const GameEvent *event);
int rich4_game_event_game_ended_reason(const GameEvent *event);
int rich4_game_event_game_ended_assets(const GameEvent *event);
/* PLAYER_ELIMINATED.subject carries the original play-state byte.  Older
 * producers may leave it zero; callers must treat zero as unknown. */
int rich4_game_event_player_eliminated_play_state(const GameEvent *event);
int rich4_game_event_confinement_kind(const GameEvent *event);
int rich4_game_event_special_place_action(const GameEvent *event);
int rich4_game_event_special_place_resource(const GameEvent *event);
int rich4_game_event_special_place_hold_ms(const GameEvent *event);
int rich4_game_event_bank_rejection_days(const GameEvent *event);
int rich4_game_event_bank_rejection_duration(const GameEvent *event);
int rich4_game_event_tool_resource(const GameEvent *event);
int rich4_game_event_tool_effect(const GameEvent *event);
/* Data[524] continuation flags carry the admission days; zero is legacy. */
int rich4_game_event_tool_confinement_days(const GameEvent *event);
GameDomainRef rich4_game_event_tool_target(const GameEvent *event);
int rich4_game_event_tool_followup_slot(const GameEvent *event);
GameDomainRef rich4_game_event_inventory_subject(const GameEvent *event);
GameDomainRef rich4_game_event_voice_subject(const GameEvent *event);
GameDomainRef rich4_game_event_voice_target(const GameEvent *event);
int rich4_game_event_voice_code(const GameEvent *event);
int rich4_game_event_property_charge_amount(const GameEvent *event);
int rich4_game_event_property_charge_block_reason(const GameEvent *event);
int rich4_game_event_god_type(const GameEvent *event);
int rich4_game_event_god_object_slot(const GameEvent *event);
int rich4_game_event_god_previous_type(const GameEvent *event);
unsigned int rich4_game_event_god_flags(const GameEvent *event);
int rich4_game_event_god_result_card(const GameEvent *event, int which);
int rich4_game_event_god_result_lost_count(const GameEvent *event);

#endif
