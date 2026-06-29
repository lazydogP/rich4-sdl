#include "app/roulette_flow.h"

#include "core/game_event_payload.h"
#include "core/rng.h"
#include "core/route_event_executor.h"
#include "app/insurance_flow.h"
#include "app/property_flow.h"

#include <string.h>

enum {
    /* loc_0043faab waits for timeGetTime()-iteration_start >= 0x24. */
    RICH4_ROULETTE_TICK_MS = 0x24,
    /* State 5 counts ebp from zero through 0x28 before returning. */
    RICH4_ROULETTE_LANDED_TICKS = 0x28
};

static int rich4_roulette_outcome(int mode, int slot)
{
    static const int outcome[4][12] = {
        /* 0x475D0C: FF is an invalid landing and makes the original
         * deceleration loop continue to another slot.  The table is
         * byte-packed in the executable, so the valid slots are not a
         * contiguous prefix in modes 0..2. */
        { 1, -1, 0, -1, 1, -1, 2, -1, 3, -1, 2, -1 },
        { -1, -1, 1, -1, -1, 4, -1, 3, -1, -1, 2, -1 },
        { 1, -1, 6, -1, 5, -1, 4, -1, 3, -1, 2, -1 },
        { 5, -1, 3, -1, 30, -1, 20, -1, 15, -1, 10, -1 }
    };
    if (mode < 0 || mode > 3 || slot < 0 || slot >= 12) return -1;
    return outcome[mode][slot];
}

static void rich4_roulette_emit(
    Rich4AudioEventQueue *audio_events, Rich4AudioEventType type, int value)
{
    if (audio_events != NULL)
        rich4_audio_event_emit(audio_events, type, 1, value);
}

void rich4_roulette_begin(
    GameSession *session, const GameEvent *event, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    Rich4Rng rng;
    GameFacilityRoulettePresentationState *roulette;
    GameDomainRef subject;
    int mode;

    if (session == NULL || event == NULL) return;
    mode = rich4_game_event_facility_mode(event);
    subject = rich4_game_event_facility_subject(event);
    if (mode < 0 || mode > 3 ||
        (subject.kind != GAME_DOMAIN_FACILITY &&
         subject.kind != GAME_DOMAIN_COMPANY))
        return;
    roulette = &session->presentation.facility_roulette;
    memset(roulette, 0, sizeof(*roulette));
    roulette->active = 1;
    roulette->player = event->player;
    roulette->mode = mode;
    roulette->subject = subject;
    roulette->automatic = session->state.players[event->player].is_ai != 0 ||
        (session->state.players[event->player].play_state & 4) != 0 ||
        session->state.players[event->player].days_sleep_walking != 0;
    rng.state = session->state.ai_rng_state;

    roulette->slot = rich4_rng_state_range(&rng, 12);
    session->state.ai_rng_state = rng.state;
    roulette->initial_frame_pending = 1;
    roulette->outcome = -1;
    roulette->delay = RICH4_ROULETTE_TICK_MS;
    roulette->next_tick = now + RICH4_ROULETTE_TICK_MS;
    session->presentation.route_interaction.active = 0;
    /* 0x41A39C precedes the hotel/department-store roulette call. */
    if (subject.kind == GAME_DOMAIN_FACILITY && (mode == 1 || mode == 2))
        rich4_roulette_emit(audio_events, RICH4_AUDIO_EVENT_PROPERTY_TOLL, 0);
    rich4_roulette_emit(audio_events, RICH4_AUDIO_EVENT_ROULETTE, 0);
}

static void roulette_stop(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    GameFacilityRoulettePresentationState *roulette;

    if (session == NULL || !session->presentation.facility_roulette.active)
        return;
    roulette = &session->presentation.facility_roulette;
    if (roulette->phase == RICH4_ROULETTE_SPINNING) {
        roulette->phase = RICH4_ROULETTE_DECELERATING;
        /* State 2 redraws the random slot immediately, even when the
         * player stops before the first timed spin tick. */
        roulette->initial_frame_pending = 0;
        roulette->steps = 0;
        /* The input branch changes state 1 -> 2.  State 2 initializes
         * stage=1, cycles=3 and last_tick=ebp on its next 36ms loop. */
        roulette->tick_count = 0;
        roulette->deceleration_stage = 0;
        roulette->deceleration_cycles = 0;
        roulette->deceleration_last_tick = 0;
        roulette->delay = RICH4_ROULETTE_TICK_MS;
        roulette->next_tick = now + RICH4_ROULETTE_TICK_MS;
        rich4_roulette_emit(audio_events, RICH4_AUDIO_EVENT_OK, 0);
    } else if (roulette->phase == RICH4_ROULETTE_LANDED) {
        /* State 6 still passes through the original 36ms timer gate. */
        roulette->finish_until = now + RICH4_ROULETTE_TICK_MS;
    }
}

void rich4_roulette_stop(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    /* 0x43FA76 accepts input only for an awake human player. */
    if (session == NULL || session->presentation.facility_roulette.automatic) return;
    roulette_stop(session, now, audio_events);
}

static void rich4_roulette_finish(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    GameEventQueue events;
    GameEvent event;
    RouteEventExecutionResult result;
    int outcome;

    (void)audio_events;

    if (session == NULL || !session->presentation.facility_roulette.active)
        return;
    outcome = session->presentation.facility_roulette.outcome;
    rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_resume_company_roulette(
        &session->flow.route_events, &session->state, outcome, &events);
    if (result == ROUTE_EVENT_EXECUTION_INVALID) return;
    memset(&session->presentation.facility_roulette, 0,
           sizeof(session->presentation.facility_roulette));
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED) {
            rich4_property_begin_charge_presentation(session, &event, now);
        } else if (event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED) {
            session->presentation.route_interaction.active = 0;
        } else if (event.type == GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED) {
            GameCompanyIssuePresentationState *issue = &session->presentation.company_issue;
            memset(issue, 0, sizeof(*issue));
            issue->active = 1; issue->player = event.player;
            issue->company = rich4_game_event_company_issue_company(&event);
            issue->stock = rich4_game_event_company_issue_stock(&event).id;
            issue->price = rich4_game_event_company_issue_price(&event);
            issue->maximum = rich4_game_event_company_issue_maximum(&event);
            issue->quantity = issue->maximum;
        } else if (rich4_game_event_company_control_company(&event) >= 0) {
            /* 0x41D2AA also blocks automatic purchases. */
            GameCompanyIssuePresentationState *issue = &session->presentation.company_issue;
            memset(issue, 0, sizeof(*issue));
            issue->active = 1; issue->phase = 2; issue->player = event.player;
            issue->company = rich4_game_event_company_control_company(&event);
        } else if (
            event.type == GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED ||
            event.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED) {
            rich4_insurance_flow_enqueue_notice(session, &event, now);
        } else {
            (void)rich4_game_event_queue_push(&session->deferred_events, &event);
        }
    }
}

int rich4_roulette_update(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    GameFacilityRoulettePresentationState *roulette;

    if (session == NULL || !session->presentation.facility_roulette.active)
        return 0;
    roulette = &session->presentation.facility_roulette;
    if (roulette->phase == RICH4_ROULETTE_LANDED) {
        if ((int32_t)(now - roulette->finish_until) >= 0)
            rich4_roulette_finish(session, now, audio_events);
        return session->presentation.facility_roulette.active ||
               session->presentation.insurance.count > 0;
    }
    if ((int32_t)(now - roulette->next_tick) < 0) return 1;
    roulette->tick_count++;
    if (roulette->phase == RICH4_ROULETTE_SPINNING) {
        /* 0x43F869 switches to state 2 before the fortieth spin step. */
        if (roulette->automatic && roulette->tick_count >= 40) {
            roulette_stop(session, now, audio_events);
            roulette->deceleration_stage = 1;
            roulette->deceleration_cycles = 3;
            return 1;
        }
        roulette->slot = (roulette->slot + 1) % 12;
        roulette->initial_frame_pending = 0;
        roulette->steps++;
        roulette->next_tick = now + RICH4_ROULETTE_TICK_MS;
    } else {
        /* The first deceleration tick is original state 2: it only seeds
         * the stage/cycle counters.  Subsequent state-3 ticks advance one
         * slot exactly when ebp-last_tick equals the current stage. */
        if (roulette->deceleration_stage == 0) {
            roulette->deceleration_stage = 1;
            roulette->deceleration_cycles = 3;
            roulette->deceleration_last_tick = roulette->tick_count;
            roulette->delay = RICH4_ROULETTE_TICK_MS;
            roulette->next_tick = now + RICH4_ROULETTE_TICK_MS;
        } else {
            int elapsed = roulette->tick_count -
                roulette->deceleration_last_tick;
            roulette->next_tick = now + RICH4_ROULETTE_TICK_MS;
            if (elapsed == roulette->deceleration_stage) {
                roulette->slot = (roulette->slot + 1) % 12;
                roulette->initial_frame_pending = 0;
                roulette->steps++;
                roulette->deceleration_last_tick = roulette->tick_count;
                roulette->deceleration_cycles--;
                if (roulette->deceleration_cycles == 0) {
                    roulette->deceleration_cycles = 3;
                    roulette->deceleration_stage++;
                    roulette->delay = roulette->deceleration_stage *
                        RICH4_ROULETTE_TICK_MS;
                }
                if (roulette->deceleration_stage >= 5) {
                    roulette->outcome = rich4_roulette_outcome(
                        roulette->mode, roulette->slot);
                    if (roulette->outcome >= 0) {
                        roulette->phase = RICH4_ROULETTE_LANDED;
                        roulette->finish_until = now +
                            RICH4_ROULETTE_LANDED_TICKS *
                            RICH4_ROULETTE_TICK_MS;
                        rich4_roulette_emit(
                            audio_events,
                            RICH4_AUDIO_EVENT_ROULETTE_STOP, 0);
                    }
                }
            }
        }
    }
    return 1;
}
