#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/roulette_flow.h"
#include "core/game_event_payload.h"

int main(void)
{
    static const int expected[4][12] = {
        { 1, -1, 0, -1, 1, -1, 2, -1, 3, -1, 2, -1 },
        { -1, -1, 1, -1, -1, 4, -1, 3, -1, -1, 2, -1 },
        { 1, -1, 6, -1, 5, -1, 4, -1, 3, -1, 2, -1 },
        { 5, -1, 3, -1, 30, -1, 20, -1, 15, -1, 10, -1 }
    };
    GameSession session;
    Rich4AudioEventQueue audio_events;
    Rich4AudioEvent audio_event;
    GameEvent event;

    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.state.players[0].is_ai = 0;
    event = rich4_game_event_facility_roulette_request(
        0, rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 3),
        0, 68, 517, 1);
    rich4_audio_event_queue_init(&audio_events);
    rich4_roulette_begin(&session, &event, 1000, &audio_events);
    assert(session.presentation.facility_roulette.active);
    assert(session.presentation.facility_roulette.phase ==
           RICH4_ROULETTE_SPINNING);
    assert(session.presentation.facility_roulette.initial_frame_pending == 1);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_ROULETTE);

    rich4_roulette_stop(&session, 1100, &audio_events);
    assert(session.presentation.facility_roulette.phase ==
           RICH4_ROULETTE_DECELERATING);
    assert(session.presentation.facility_roulette.initial_frame_pending == 0);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_OK);

    assert(rich4_roulette_update(&session, 1136, &audio_events));
    assert(session.presentation.facility_roulette.steps == 0);
    assert(session.presentation.facility_roulette.deceleration_stage == 1);
    assert(rich4_roulette_update(&session, 1172, &audio_events));
    assert(session.presentation.facility_roulette.steps == 1);

    /* 0x43F7C6 indexes the 4x12 byte table by mode then slot.  Exercise
     * every slot through the public update boundary so an invalid 0xFF
     * entry cannot silently become a charge result. */
    {
        int mode;
        int slot;
        for (mode = 0; mode < 4; mode++) {
            for (slot = 0; slot < 12; slot++) {
                memset(&session, 0, sizeof(session));
                session.presentation.facility_roulette.active = 1;
                session.presentation.facility_roulette.mode = mode;
                session.presentation.facility_roulette.phase =
                    RICH4_ROULETTE_DECELERATING;
                session.presentation.facility_roulette.slot = (slot + 11) % 12;
                session.presentation.facility_roulette.steps = 0;
                session.presentation.facility_roulette.tick_count = 4;
                session.presentation.facility_roulette.deceleration_stage = 5;
                session.presentation.facility_roulette.deceleration_cycles = 2;
                session.presentation.facility_roulette.deceleration_last_tick = 0;
                session.presentation.facility_roulette.delay = 180;
                session.presentation.facility_roulette.outcome = -1;
                session.presentation.facility_roulette.next_tick = 0;
                rich4_audio_event_queue_init(&audio_events);
                assert(rich4_roulette_update(&session, 1, &audio_events));
                if (expected[mode][slot] < 0) {
                    assert(session.presentation.facility_roulette.phase ==
                           RICH4_ROULETTE_DECELERATING);
                    assert(session.presentation.facility_roulette.outcome == -1);
                } else {
                    assert(session.presentation.facility_roulette.phase ==
                           RICH4_ROULETTE_LANDED);
                    assert(session.presentation.facility_roulette.outcome ==
                           expected[mode][slot]);
                }
            }
        }
    }

    /* A normal deceleration follows the original stage schedule: the first
     * tick seeds state 2, then stage 1 advances on three consecutive ticks,
     * and stage 2 advances every other tick. */
    memset(&session, 0, sizeof(session));
    session.presentation.facility_roulette.active = 1;
    session.presentation.facility_roulette.mode = 0;
    session.presentation.facility_roulette.phase =
        RICH4_ROULETTE_DECELERATING;
    session.presentation.facility_roulette.slot = 0;
    session.presentation.facility_roulette.next_tick = 0;
    rich4_audio_event_queue_init(&audio_events);
    assert(rich4_roulette_update(&session, 1, &audio_events));
    assert(session.presentation.facility_roulette.deceleration_stage == 1);
    assert(session.presentation.facility_roulette.steps == 0);
    assert(rich4_roulette_update(&session, 37, &audio_events));
    assert(session.presentation.facility_roulette.steps == 1);
    assert(rich4_roulette_update(&session, 73, &audio_events));
    assert(session.presentation.facility_roulette.steps == 2);
    assert(rich4_roulette_update(&session, 109, &audio_events));
    assert(session.presentation.facility_roulette.steps == 3);
    assert(session.presentation.facility_roulette.deceleration_stage == 2);
    assert(rich4_roulette_update(&session, 145, &audio_events));
    assert(session.presentation.facility_roulette.steps == 3);
    assert(rich4_roulette_update(&session, 181, &audio_events));
    assert(session.presentation.facility_roulette.steps == 4);

    /* A valid landing keeps the presentation open for 40 original timer
     * loops, not the former approximate 800ms hold. */
    assert(session.presentation.facility_roulette.phase ==
           RICH4_ROULETTE_DECELERATING);
    session.presentation.facility_roulette.slot = 3;
    session.presentation.facility_roulette.deceleration_stage = 5;
    session.presentation.facility_roulette.deceleration_cycles = 1;
    session.presentation.facility_roulette.deceleration_last_tick =
        session.presentation.facility_roulette.tick_count - 5 + 1;
    assert(rich4_roulette_update(&session, 217, &audio_events));
    assert(session.presentation.facility_roulette.phase ==
           RICH4_ROULETTE_LANDED);
    assert(session.presentation.facility_roulette.finish_until == 217 + 40 * 36);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_ROULETTE_STOP);
    assert(!rich4_audio_event_pop(&audio_events, &audio_event));

    memset(&session, 0, sizeof(session));
    event = rich4_game_event_facility_roulette_request(
        0, rich4_game_domain_ref(GAME_DOMAIN_COMPANY, 2),
        9, 68, 517, 1);
    rich4_audio_event_queue_init(&audio_events);
    rich4_roulette_begin(&session, &event, 1000, &audio_events);
    assert(!session.presentation.facility_roulette.active);

    /* Raw hosted-human state works even before the cached is_ai is synced. */
    memset(&session, 0, sizeof(session));
    session.state.players[0].play_state = 5;
    event = rich4_game_event_facility_roulette_request(0,
        rich4_game_domain_ref(GAME_DOMAIN_COMPANY, 0), 3, 71, 517, 1);
    rich4_roulette_begin(&session, &event, 1000, &audio_events);
    assert(session.presentation.facility_roulette.automatic);
    rich4_roulette_stop(&session, 1001, &audio_events);
    assert(session.presentation.facility_roulette.phase == RICH4_ROULETTE_SPINNING);

    /* Dreamwalking humans use the same 40-tick automatic stop as AI, and
     * cannot stop or dismiss the modal through player input. */
    memset(&session, 0, sizeof(session));
    session.state.players[0].days_sleep_walking = 2;
    event = rich4_game_event_facility_roulette_request(0,
        rich4_game_domain_ref(GAME_DOMAIN_COMPANY, 0), 3, 71, 517, 1);
    rich4_roulette_begin(&session, &event, 1000, &audio_events);
    assert(session.presentation.facility_roulette.automatic);
    rich4_roulette_stop(&session, 1001, &audio_events);
    assert(session.presentation.facility_roulette.phase == RICH4_ROULETTE_SPINNING);
    {
        int tick;
        for (tick = 1; tick <= 39; tick++)
            assert(rich4_roulette_update(&session, 1000 + tick * 36, &audio_events));
        assert(session.presentation.facility_roulette.steps == 39);
        assert(rich4_roulette_update(&session, 2440, &audio_events));
        assert(session.presentation.facility_roulette.phase == RICH4_ROULETTE_DECELERATING);
        assert(session.presentation.facility_roulette.deceleration_stage == 1);
        assert(session.presentation.facility_roulette.steps == 0);
    }
    puts("app roulette flow: ok");
    return 0;
}
