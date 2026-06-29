#ifndef RICH4_CORE_ROUTE_EVENT_EXECUTOR_INTERNAL_H
#define RICH4_CORE_ROUTE_EVENT_EXECUTOR_INTERNAL_H

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "core/route_event_executor.h"
#include "core/ledger.h"
#include "core/bank.h"
#include "core/card.h"
#include "core/confinement.h"
#include "core/facility.h"
#include "core/god.h"
#include "core/property.h"
#include "core/rng.h"
#include "core/special_event.h"
#include "core/tool.h"

void clear_pending(RouteEventExecutor *executor);
int emit_actor_eliminated(RouteEventExecutor *executor,
                          const RouteEvent *event,
                          GameEventQueue *events);
int property_domain_ref(const RouteEvent *event,
                        const GameState *state,
                        GameDomainRef *out_ref);
int request_property_decision(RouteEventExecutor *executor,
                              const RouteEvent *event,
                              const GameState *state,
                              GameCommandType accept_type,
                              GameCommandType decline_type,
                              GameEventQueue *events);
int rich4_route_event_executor_begin_god_trade_rejection(
    RouteEventExecutor *executor, const GameState *state,
    const RouteEvent *event, GameEventQueue *events);
int request_special_confirmation(RouteEventExecutor *executor,
                                 const GameState *state,
                                 GameEventQueue *events);
int first_player_in_mask(const GameState *state, unsigned int mask);
GameDomainRef choose_construction_target(GameState *state,
                                         int actor,
                                         int *facility_type);
int begin_next_magic_fortune(RouteEventExecutor *executor,
                             GameState *state,
                             GameEventQueue *events);
int request_bank_transfer(RouteEventExecutor *executor,
                          const GameState *state,
                          GameEventQueue *events);
int request_bank_loan_page(RouteEventExecutor *executor,
                           const GameState *state,
                           GameEventQueue *events);
RouteEventExecutionResult request_bank_rejection(
    RouteEventExecutor *executor, const GameState *state, GameEventQueue *events);

int apply_points_reward(GameState *state, int player, int amount,
                        GameEventQueue *events);
int apply_automatic_point_minigame(GameState *state, int player, int action,
                                   GameEventQueue *events);
int run_ai_lottery_purchase(GameState *state, int player,
                            GameEventQueue *events);
int gain_weighted_random_card(GameState *state, int player,
                              int *out_card, GameEventQueue *events);
int grant_shop_owner_gift(GameState *state, int player,
                          GameDomainRef *out_gift,
                          GameEventQueue *events);
int run_ai_shop(GameState *state, int player, int company,
                GameEventQueue *events);
void shop_add_business_volume(GameState *state, int company,
                              uint32_t amount);
int shop_company_index(const RouteEventExecutor *executor,
                       const GameState *state);

RouteEventExecutionResult request_facility_roulette(
    RouteEventExecutor *executor, GameDomainRef object_ref, int mode,
    GameEventQueue *events);
int request_god_company_charge(RouteEventExecutor *executor, GameState *state,
    int company, int outcome, int dice_total, GameEventQueue *events);
RouteEventExecutionResult finish_company_landing(
    RouteEventExecutor *executor, GameState *state, int company,
    GamePaymentResult payment, GameEventQueue *staged,
    GameEventQueue *events);
int confinement_has_candidate(const GameState *state, int kind);
RouteEventExecutionResult run_ai_confinement_visit(
    RouteEventExecutor *executor, GameState *state, int kind,
    GameEventQueue *events);

RouteEventExecutionResult rich4_route_event_executor_step_property(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events,
    const RouteEvent *event, RouteEventObjectFamily family);
RouteEventExecutionResult rich4_route_event_executor_step_special(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events,
    const RouteEvent *event, RouteEventObjectFamily family);

int presentation_matches(const RouteEventExecutor *executor, int action);

#endif
