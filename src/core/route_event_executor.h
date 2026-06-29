#ifndef RICH4_CORE_ROUTE_EVENT_EXECUTOR_H
#define RICH4_CORE_ROUTE_EVENT_EXECUTOR_H

#include "core/game_event.h"
#include "core/game_state.h"
#include "core/route_event_queue.h"
#include "core/route_event_executor_state.h"

typedef struct RouteEventExecutor {
    RouteEventQueue queue;
    RouteEventExecutorState state;
} RouteEventExecutor;

void rich4_route_event_executor_reset(RouteEventExecutor *executor);
void rich4_route_event_executor_reset_dispatch(RouteEventExecutor *executor);
const RouteEvent *rich4_route_event_executor_current(
    const RouteEventExecutor *executor);
int rich4_route_event_executor_complete_events(
    RouteEventExecutor *executor,
    RouteEventHandlerResult result,
    GameEventQueue *events);
int rich4_route_event_executor_begin(RouteEventExecutor *executor,
                                     int actor,
                                     unsigned int turn_token);
RouteEventExecutionResult rich4_route_event_executor_step(
    RouteEventExecutor *executor,
    GameState *state,
    GameEventQueue *events);
RouteEventCommandResult rich4_route_event_executor_submit(
    RouteEventExecutor *executor,
    GameState *state,
    const GameCommand *command,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_resume_charge(
    RouteEventExecutor *executor,
    GameState *state,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_resume_beggar(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_resume_special_place(
    RouteEventExecutor *executor,
    GameState *state,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_resume_confinement(
    RouteEventExecutor *executor,
    GameState *state,
    GameDomainRef target,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_cancel_confinement(
    RouteEventExecutor *executor,
    GameState *state,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_purchase_lottery(
    RouteEventExecutor *executor,
    GameState *state,
    int number,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_finish_lottery(
    RouteEventExecutor *executor,
    GameState *state,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_finish_point_minigame(
    RouteEventExecutor *executor,
    GameState *state,
    int points,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_shop_buy_card(
    RouteEventExecutor *executor,
    GameState *state,
    int card,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_shop_buy_tool(
    RouteEventExecutor *executor,
    GameState *state,
    int tool,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_shop_sell_card_slot(
    RouteEventExecutor *executor,
    GameState *state,
    int slot,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_shop_sell_tool(
    RouteEventExecutor *executor,
    GameState *state,
    int tool,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_finish_shop(
    RouteEventExecutor *executor,
    GameState *state,
    GameEventQueue *events);
/* 0x42EA23 returns from the owner gift's text/speech before AI trading. */
RouteEventExecutionResult rich4_route_event_executor_resume_ai_shop(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_finish_bank_rejection(
    RouteEventExecutor *executor,
    GameState *state,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_finish_god_trade_rejection(
    RouteEventExecutor *executor,
    GameState *state,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_resume_company_roulette(
    RouteEventExecutor *executor,
    GameState *state,
    int outcome,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_buy_company_issue(
    RouteEventExecutor *executor,
    GameState *state,
    int quantity,
    GameEventQueue *events);

/* 0x41B022, after the app has displayed/resolved the passive charge chain. */
RouteEventExecutionResult rich4_route_event_executor_settle_insurance(
    RouteEventExecutor *executor, GameState *state, int payer, int amount,
    GameEventQueue *events);
RouteEventExecutionResult rich4_route_event_executor_resume_magic_house(
    RouteEventExecutor *executor,
    GameState *state,
    int action,
    GameEventQueue *events);
int rich4_route_event_executor_is_waiting(const RouteEventExecutor *executor);

/* 0x431CAA: continue the next fortune only after the preceding callback. */
RouteEventExecutionResult rich4_route_event_executor_finish_fortune_callback(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events);

RouteEventCommandResult rich4_route_event_executor_submit_deferred_confinement(RouteEventExecutor *executor, GameState *state, const GameCommand *command, GameEventQueue *events);

RouteEventExecutionResult rich4_route_event_executor_resume_magic_house_deferred_confinement(RouteEventExecutor *executor, GameState *state, int action, GameEventQueue *events);

#endif
