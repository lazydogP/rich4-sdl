#include "app/game_update_flow.h"

#include <string.h>

#include "app/dice_flow.h"
#include "core/game_constants.h"

static int has_post_terminal_presentation_work(const GameSession *session)
{
    return session != NULL &&
        (session->presentation.bankruptcy.active ||
         session->presentation.auction.active ||
         session->deferred_events.count > 0 ||
         session->bankruptcy_auction_events.count > 0 ||
         session->bankruptcy_terminal_events.count > 0);
}

Rich4AppGameUpdateStage rich4_app_game_update_stage(
    const GameSession *session)
{
    if (session == NULL || !session->state.active ||
        session->state.player_count <= 0 ||
        (!session->flow.turn_flow_active &&
         !has_post_terminal_presentation_work(session)) ||
        (session->state.game_over &&
         !has_post_terminal_presentation_work(session))) {
        return RICH4_APP_GAME_UPDATE_IDLE;
    }
    if (has_post_terminal_presentation_work(session) &&
        session->deferred_events.count > 0)
        return RICH4_APP_GAME_UPDATE_ROUTE_EVENTS;
    switch (session->flow.turn_state) {
    case GAME_TURN_STATE_ROUTE_EVENTS:
        return RICH4_APP_GAME_UPDATE_ROUTE_EVENTS;
    case GAME_TURN_STATE_LANDING:
        return RICH4_APP_GAME_UPDATE_FIRST_LANDING;
    case GAME_TURN_STATE_PREROLL:
        return RICH4_APP_GAME_UPDATE_DICE;
    case GAME_TURN_STATE_MOVE:
        return RICH4_APP_GAME_UPDATE_MOVEMENT;
    case GAME_TURN_STATE_IDLE:
    default:
        return RICH4_APP_GAME_UPDATE_IDLE;
    }
}

int rich4_app_game_update_prepare(
    GameSession *session, uint32_t now, Rich4AppGameUpdatePlan *out_plan)
{
    Rich4AppGameUpdatePlan plan;

    memset(&plan, 0, sizeof(plan));
    plan.stage = RICH4_APP_GAME_UPDATE_IDLE;
    plan.turn_start.action = RICH4_TURN_START_NONE;
    plan.turn_start.player_index = -1;
    plan.turn_start.status_slot = -1;
    if (session == NULL || out_plan == NULL || !session->state.active ||
        session->state.player_count <= 0 ||
        (session->state.game_over &&
         !has_post_terminal_presentation_work(session)))
        return 0;
    /* Other rotation callers (e.g. a forced tool stop) also enter 0x418EBD.
     * Drain its deferred god/engineering return before preparing input. */
    if (session->presentation.engineering_attack.pending_return &&
        (!session->flow.turn_flow_active ||
         session->flow.turn_state != GAME_TURN_STATE_ROUTE_EVENTS)) {
        session->flow.turn_flow_active = 1;
        session->flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
        session->flow.turn_wait = 0;
        session->flow.route_events.state.execution_started = 0;
    }
    (void)rich4_turn_start_prepare_request(session, now, &plan.turn_start);
    plan.stage = rich4_app_game_update_stage(session);
    *out_plan = plan;
    return 1;
}

int rich4_app_game_update(
    GameSession *session, uint32_t now,
    const Rich4AppGameUpdateServices *services)
{
    Rich4AppGameUpdatePlan plan;

    if (session == NULL || services == NULL || !session->state.active ||
        session->state.player_count <= 0)
        return 0;
    if (services->update_presentation != NULL &&
        services->update_presentation(services->presentation_context))
        return 1;
    /* A god's transfer loop can be suspended between bankruptcy and its
     * queued auction/speech. Do not resume the outer turn in that gap. */
    if (session->presentation.god_acquisition.active) return 1;
    if (session->state.game_over &&
        !has_post_terminal_presentation_work(session))
        return 0;
    /* The original 0x4018E7 nested wait freezes ordinary turn dispatch while
     * either the winner speech/ending prompt or its post-confirmation FLC is
     * on screen.  Keep that gate in app flow rather than writing GameState
     * from the SDL save-panel adapter. */
    if (session->presentation.ending.active)
        return 0;
    rich4_dice_update_status_blink(session, now);
    if (!rich4_app_game_update_prepare(session, now, &plan)) return 0;
    if (services->apply_turn_start != NULL)
        services->apply_turn_start(
            services->turn_start_context, &plan.turn_start);
    switch (plan.stage) {
    case RICH4_APP_GAME_UPDATE_ROUTE_EVENTS:
        return services->update_route_events != NULL
            ? services->update_route_events(services->route_context) : 0;
    case RICH4_APP_GAME_UPDATE_FIRST_LANDING:
        return services->update_first_landing != NULL
            ? services->update_first_landing(
                  services->first_landing_context) : 0;
    case RICH4_APP_GAME_UPDATE_DICE:
        if (services->update_dice != NULL &&
            services->update_dice(services->dice_context))
            return 1;
        /* Dice animation completion falls through to movement in the same
         * way as the former SDL entrypoint. */
        /* fall through */
    case RICH4_APP_GAME_UPDATE_MOVEMENT:
        return services->update_movement != NULL
            ? services->update_movement(services->movement_context) : 0;
    case RICH4_APP_GAME_UPDATE_IDLE:
    default:
        return 0;
    }
}
