#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/confinement_flow.h"
#include "core/game_event_payload.h"

int main(void)
{
    GameSession session;
    Rich4AudioEventQueue audio_events;
    Rich4AudioEvent audio_event;
    GameEvent event;

    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].days_in_prison = 1;
    event = rich4_game_event_confinement_visit_request(
        0, GAME_CONFINEMENT_PRISON, 63, 64, 1u);
    rich4_audio_event_queue_init(&audio_events);
    rich4_confinement_begin(&session, &event, 1000, &audio_events);
    assert(session.presentation.confinement_visit.active);
    assert(session.presentation.confinement_visit.phase ==
           RICH4_CONFINEMENT_VISIT_SELECT);
    assert(rich4_confinement_visit_slot_at(&session, 40, 30) == 0);
    assert(!rich4_audio_event_pop(&audio_events, &audio_event));

    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].days_in_hospital = 1;
    event = rich4_game_event_confinement_visit_request(
        0, GAME_CONFINEMENT_HOSPITAL, 63, 64, 1u);
    rich4_audio_event_queue_init(&audio_events);
    rich4_confinement_begin(&session, &event, 1000, &audio_events);
    assert(session.presentation.confinement_visit.phase ==
           RICH4_CONFINEMENT_VISIT_HOSPITAL_INTRO);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_SPEECH_START);

    /* The original hospital timer advances the nurse gesture on each
     * 100ms tick: Panel[65] gesture frames 0, 1, 2, then back to idle.
     * Keep this visual clock independent from route/state mutation. */
    memset(&session, 0, sizeof(session));
    session.presentation.confinement_visit.active = 1;
    session.presentation.confinement_visit.kind = GAME_CONFINEMENT_HOSPITAL;
    session.presentation.confinement_visit.effect_tick = 1000;
    session.presentation.confinement_visit.nurse_gesture_frame = 0;
    session.presentation.confinement_visit.nurse_idle_chunk = 12;
    rich4_audio_event_queue_init(&audio_events);
    assert(rich4_confinement_update(&session, 1100, 0, &audio_events));
    assert(session.presentation.confinement_visit.nurse_gesture_frame == 1);
    assert(rich4_confinement_update(&session, 1200, 0, &audio_events));
    assert(session.presentation.confinement_visit.nurse_gesture_frame == 2);
    assert(rich4_confinement_update(&session, 1300, 0, &audio_events));
    assert(session.presentation.confinement_visit.nurse_gesture_frame == -1);
    assert(session.presentation.confinement_visit.nurse_idle_chunk == 12);

    puts("app confinement flow: ok");
    return 0;
}
