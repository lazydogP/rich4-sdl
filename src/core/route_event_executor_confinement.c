#include "core/route_event_executor_internal.h"
#include "core/game_event_payload.h"

int confinement_has_candidate(const GameState *state, int kind)
{
    int i;
    for (i = 0; i < GAME_MAX_PLAYERS; i++)
        if (rich4_confinement_visit_candidate(
                state, rich4_game_domain_ref(GAME_DOMAIN_PLAYER, i), kind))
            return 1;
    for (i = 0; i < GAME_SPECIAL_PLAYER_COUNT; i++)
        if (rich4_confinement_visit_candidate(
                state,
                rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER, i), kind))
            return 1;
    return 0;
}

RouteEventExecutionResult run_ai_confinement_visit(
    RouteEventExecutor *executor, GameState *state, int kind, GameEventQueue *events)
{
    GameDomainRef candidates[8];
    GameState next_state = *state;
    RouteEventExecutor next_queue = *executor;
    GameEventQueue staged;
    Rich4Rng rng;
    GameDomainRef target;
    int count = 0;
    int personality;
    int i;
    int cost;

    rng.state = next_state.ai_rng_state;
    /* 0x43D304/0x43E9A4: AI only considers a visit when rand()&1 != 0. */
    if ((rich4_rng_original_state_next(&rng) & 1) == 0) {
        next_state.ai_rng_state = rng.state;
        goto complete_without_release;
    }
    /* 0x43D3EE/0x43EA9A reads player +23, not traffic at +17. */
    personality = next_state.players[executor->state.execution_actor].ai_personality;
    if (personality == 0 || personality == 1) {
        for (i = 0; i < GAME_MAX_PLAYERS; i++) {
            GameDomainRef ref =
                rich4_game_domain_ref(GAME_DOMAIN_PLAYER, i);
            if (rich4_confinement_visit_candidate(&next_state, ref, kind))
                candidates[count++] = ref;
        }
    }
    if (personality == 2 ||
        (personality == 1 && rich4_rng_original_state_range(&rng, 3) == 0)) {
        for (i = 0; i < GAME_SPECIAL_PLAYER_COUNT && i < 4; i++) {
            GameDomainRef ref =
                rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER, i);
            if (rich4_confinement_visit_candidate(&next_state, ref, kind))
                candidates[count++] = ref;
        }
    }
    next_state.ai_rng_state = rng.state;
    if (count == 0) {
        goto complete_without_release;
    }
    target = candidates[rich4_rng_original_state_range(&rng, count)];
    next_state.ai_rng_state = rng.state;
    cost = rich4_confinement_visit_cost(target);
    /*
     * The original normal-player branch uses points > 30, while its
     * special-player branch requires points >= 700 even though the table
     * charge committed below is only 300.
     */
    if ((target.kind == GAME_DOMAIN_PLAYER &&
         next_state.players[executor->state.execution_actor].points <= cost) ||
        (target.kind == GAME_DOMAIN_SPECIAL_PLAYER &&
         next_state.players[executor->state.execution_actor].points < 700)) {
        goto complete_without_release;
    }
    rich4_game_event_queue_reset(&staged);
    /* 0x43D550/0x43EBFC: successful AI visits show 保釋%s for 1500 ms. */
    {
        GameEvent notice = rich4_game_event_special_place_notice(
            executor->state.execution_actor,
            kind == GAME_CONFINEMENT_PRISON
                ? GAME_SPECIAL_NOTICE_PRISON_RELEASE
                : GAME_SPECIAL_NOTICE_HOSPITAL_RELEASE,
            target, cost, 1500);
        if (!rich4_game_event_queue_push(&staged, &notice))
            return ROUTE_EVENT_EXECUTION_INVALID;
    }
    next_queue.state.waiting_for_presentation = 1;
    next_queue.state.presentation_action = kind == GAME_CONFINEMENT_PRISON ? 4 : 5;
    next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
    /* The original message precedes both the point debit and release. */
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = next_state;
    *executor = next_queue;
    return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;

complete_without_release:
    rich4_game_event_queue_reset(&staged);
    if (!rich4_route_event_executor_complete_events(
            &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = next_state;
    *executor = next_queue;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}



