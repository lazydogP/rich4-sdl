#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/magic_house_flow.h"
#include "core/game_event_payload.h"

static void ai_notice_precedes_effect(void)
{
    int mode;
    for (mode = 0; mode < 4; mode++) {
        GameSession session;
        GameEventQueue events;
        Rich4AudioEventQueue audio;
        Rich4AudioEvent sound;
        GameEvent request;
        RouteEvent stop;
        uint32_t rng_after_selection;
        memset(&session, 0, sizeof(session));
        memset(&stop, 0, sizeof(stop));
        session.state.active = 1;
        session.state.player_count = 1;
        session.state.players[0].play_state = mode < 2 ? 2 : 5;
        session.state.players[0].is_ai = mode < 2;
        session.state.players[0].cash = 100000;
        session.state.ai_rng_state = 1;
        session.state.card_amount[0] = 1;
        stop.phase = ROUTE_EVENT_PHASE_STOP;
        stop.node = 20;
        stop.snapshot.route_action = 16;
        rich4_route_event_executor_reset(&session.flow.route_events);
        assert(rich4_route_event_queue_push(&session.flow.route_events.queue, &stop));
        assert(rich4_route_event_executor_begin(&session.flow.route_events, 0, 1));
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_step(&session.flow.route_events,
            &session.state, &events) == ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
        assert(rich4_game_event_queue_pop(&events, &request));
        assert(request.type == GAME_EVENT_MAGIC_HOUSE_REQUESTED);
        assert(rich4_game_event_magic_house_auto_action(&request) == 6);
        assert(events.count == 0 && session.state.players[0].card_inventory[0] == 0);
        rng_after_selection = session.state.ai_rng_state;
        rich4_audio_event_queue_init(&audio);
        rich4_magic_house_begin(&session, &request, 1000, mode & 1, &audio);
        assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_AI_NOTICE);
        assert(session.presentation.magic_house.modal_music_finished);
        assert(!rich4_audio_event_pop(&audio, &sound));
        assert(rich4_magic_house_update(&session, 0, 0, 2499, 0, &audio, &events));
        assert(session.presentation.magic_house.active);
        assert(session.state.players[0].card_inventory[0] == 0);
        assert(session.state.ai_rng_state == rng_after_selection);
        assert(rich4_magic_house_update(&session, 0, 0, 2500, 0, &audio, &events));
        assert(!session.presentation.magic_house.active);
        assert(!rich4_route_event_executor_is_waiting(&session.flow.route_events));
        assert(session.state.players[0].card_inventory[0] != 0);
        assert(!rich4_magic_house_update(&session, 0, 0, 2501, 0, &audio, &events));
    }
}

static void confinement_actions_request_shared_presentations(void)
{
    int action;
    for (action = 2; action <= 10; action += 8) {
        GameSession s;
        RouteEvent route = {0};
        int player;
        memset(&s, 0, sizeof(s));
        s.state.active = 1;
        s.state.player_count = 2;
        s.state.price_index = 1;
        s.state.prison_node = 5;
        s.state.hospital_node = 6;
        for (player = 0; player < 2; player++) {
            s.state.players[player].play_state = 1;
            s.state.players[player].node = 2 + player;
        }
        route.phase = ROUTE_EVENT_PHASE_STOP;
        route.snapshot.route_action = 16;
        assert(rich4_route_event_queue_push(&s.flow.route_events.queue, &route));
        s.flow.route_events.state.execution_started = 1;
        s.flow.route_events.state.waiting_for_presentation = 1;
        s.flow.route_events.state.presentation_action = 16;
        s.flow.route_events.state.presentation_condition = 6; /* walking players */
        s.flow.route_events.state.presentation_targets = 3;
        s.presentation.magic_house.active = 1;
        rich4_magic_house_finish(&s, action, 1000, NULL, NULL);
        assert(!s.presentation.magic_house.active);
        assert(s.presentation.tool_animation_count == 2);
        for (player = 0; player < 2; player++) {
            assert(s.state.players[player].node == 2 + player);
            assert(s.state.players[player].days_in_prison == 0);
            assert(s.state.players[player].days_in_hospital == 0);
            assert(s.presentation.tool_animation[player].admission_stage == 1);
            assert(s.presentation.tool_animation[player].after_sync_player == player);
            assert(s.presentation.tool_animation[player].confinement_days == 3);
            assert(s.presentation.tool_animation[player].resource == (action == 2 ? 538 : 524));
        }
    }
}

int main(void)
{
    confinement_actions_request_shared_presentations();
    GameSession session;
    Rich4AudioEventQueue audio_events;
    Rich4AudioEvent audio_event;
    GameEvent event;

    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    event = rich4_game_event_magic_house_request(0, 3, 1u);
    rich4_audio_event_queue_init(&audio_events);
    rich4_magic_house_begin(&session, &event, 1000, 1, &audio_events);
    assert(session.presentation.magic_house.active);
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_INTRO_1);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_SPEECH_START);

    rich4_magic_house_skip_intro(&session, 1100, &audio_events);
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_REVEAL);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_SPEECH_STOP);
    assert(rich4_magic_house_action_big5(0) != NULL);
    assert(rich4_magic_house_phase_big5(
        RICH4_MAGIC_HOUSE_INTRO_1, 0) != NULL);
    assert(rich4_magic_house_facility_at(50, 286) == 0);
    assert(rich4_magic_house_facility_at(389, 353) == 4);
    assert(rich4_magic_house_facility_at(390, 353) == -1);
    rich4_magic_house_request_return_to_selection(&session);
    assert(session.presentation.magic_house.return_to_selection == 1);
    session.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_FACILITY_CHOICE;
    session.presentation.magic_house.facility_choice_player = 0;
    assert(!rich4_magic_house_choose_facility(
        &session, -1, 1200, &audio_events, NULL));
    assert(session.presentation.magic_house.facility_choice[0] == -1);

    session.presentation.magic_house.active = 1;
    session.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_CONFIRM;
    session.presentation.magic_house.selected_action = 7;
    rich4_audio_event_queue_init(&audio_events);
    assert(rich4_magic_house_update(
        &session, 2, 59, 4000, 0, &audio_events, NULL));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_FINALE);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_MAGIC_FINALE);
    assert(audio_event.value == 59);
    assert(!rich4_audio_event_pop(&audio_events, &audio_event));

    /* Every choice shares Panel[20]/Effect[59]; wait for speech first. */
    session.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_CONFIRM;
    session.presentation.magic_house.selected_action = 0;
    assert(rich4_magic_house_update(
        &session, 4, 59, 4100, 1, &audio_events, NULL));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_CONFIRM);
    assert(audio_events.count == 0);
    assert(rich4_magic_house_update(
        &session, 4, 59, 4200, 0, &audio_events, NULL));
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_MAGIC_FINALE);
    assert(rich4_magic_house_update(
        &session, 4, 59, 4201, 0, &audio_events, NULL));
    assert(audio_events.count == 0);

    ai_notice_precedes_effect();
    puts("app magic-house flow: ok");
    return 0;
}
