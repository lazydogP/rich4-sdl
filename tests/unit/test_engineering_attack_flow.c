#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/engineering_attack_flow.h"
#include "app/game_update_flow.h"
#include "app/game_speech_flow.h"
#include "app/input_flow.h"
#include "app/property_flow.h"
#include "app/route_event_flow.h"
#include "app/turn_movement_flow.h"
#include "app/turn_rotation_flow.h"
#include "view/game_view_model.h"

static GameSession session;

static void setup(void)
{
    int i;
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 3;
    session.state.price_index = 2;
    session.state.tool_route_node_count = 3;
    session.state.route_object_raw[1] = 2001;
    session.state.tool_route_x[1] = 100;
    session.state.tool_route_y[1] = 100;
    session.state.tool_land_x[0] = 200;
    session.state.tool_land_y[0] = 100;
    session.state.land_count = 1;
    session.state.land_owner[0] = 1;
    session.state.land_level[0] = 3;
    session.state.land_type[0] = 1;
    session.state.land_price[0] = 100;
    session.state.land_value[0] = 400;
    session.state.fixed_asset_value[1] = 400;
    for (i = 0; i < 3; ++i) {
        session.state.players[i].play_state = 1;
        session.state.players[i].node = 1;
        session.state.players[i].cash = 1000;
        session.state.players[i].linked_player = -1;
        session.presentation.actors[i].intro_visible = 1;
    }
    session.state.players[0].traffic_method = 0x1f;
    session.presentation.actors[0].direction = 6;
    session.flow.turn_flow_active = 1;
    session.flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    session.presentation.movement_tick = 1000;
}

static void gates(void)
{
    int i;
    for (i = 0; i < 2; ++i) {
        setup();
        session.state.players[0].is_ai = i;
        session.state.players[0].ai_personality = 0;
        assert(rich4_engineering_attack_begin(&session, 0, 1, 1000));
        assert(!rich4_app_input_ui_active(
            RICH4_SCENE_GAME, NULL, &session, 0));
    }
    setup(); session.state.players[0].traffic_method = 0x9f;
    assert(!rich4_engineering_attack_begin(&session, 0, 1, 1000));
    setup(); session.state.players[0].traffic_method = 2;
    assert(!rich4_engineering_attack_begin(&session, 0, 1, 1000));
    setup(); session.state.players[0].days_sleep_walking = 0x80;
    assert(!rich4_engineering_attack_begin(&session, 0, 1, 1000));
    setup(); session.state.players[0].delayed_status_block = 0x80;
    assert(!rich4_engineering_attack_begin(&session, 0, 1, 1000));
    setup(); session.state.players[0].play_state = 0;
    assert(!rich4_engineering_attack_begin(&session, 0, 1, 1000));
    setup(); session.state.land_owner[0] = 0;
    assert(!rich4_engineering_attack_begin(&session, 0, 1, 1000));
    setup(); session.state.land_level[0] = 0;
    assert(!rich4_engineering_attack_begin(&session, 0, 1, 1000));
    setup(); session.state.route_object_raw[1] = 6001;
    assert(!rich4_engineering_attack_begin(&session, 0, 1, 1000));
    setup(); session.state.land_owner[0] = -1;
    assert(rich4_engineering_attack_begin(&session, 0, 1, 1000));
}

static void timeline(int tick_ms, int actor_frames)
{
    Rich4AudioEventQueue audio;
    Rich4AudioEvent event;
    Rich4AppRouteEventServices services = {0};
    RouteEvent fact = {0};
    int t, sound_count = 0;
    uint32_t now;
    setup();
    services.property_end_delay_ms = (uint32_t)tick_ms;
    fact.phase = ROUTE_EVENT_PHASE_STOP;
    fact.node = 1;
    fact.object_ref = rich4_map_object_ref(2001);
    rich4_route_event_collection_push(&session.flow.route_facts, &fact);
    session.state.players[0].linked_player = 1;
    session.state.players[1].linked_player = 0;
    session.state.players[0].god_status_days = 7;
    session.state.players[1].god_status_days = 7;
    assert(rich4_app_route_event_update(&session, NULL, 1000, 0, &services));
    assert(session.presentation.engineering_attack.active);
    assert(session.flow.property_turn_end_ticks == 8);
    assert(session.state.players[1].relation_balance[0] == 0);
    rich4_audio_event_queue_init(&audio);
    for (t = 0; t <= 15; ++t) {
        now = 1000 + (uint32_t)((t + 1) * tick_ms);
        assert(rich4_engineering_attack_update(
            &session, actor_frames, 8, tick_ms, now, &audio));
        assert(session.state.current_player == 0);
        assert(session.flow.property_turn_end_ticks == 8);
        assert(session.state.land_level[0] == (t < 9 ? 3 : 0));
        assert(session.state.land_type[0] == (t < 9 ? 1 : 0));
        assert(session.state.land_owner[0] == 1);
        assert(session.state.players[0].cash == 1000);
        assert(session.state.players[1].relation_balance[0] == 60);
        assert(session.state.players[0].linked_player == -1);
        assert(session.state.players[1].god_status_days == 0);
        while (rich4_audio_event_pop(&audio, &event)) {
            assert(t == 8 && event.type == RICH4_AUDIO_EVENT_TOOL_EFFECT);
            assert(event.value == 15);
            ++sound_count;
        }
        if (t < 15) {
            assert(session.presentation.engineering_attack.active);
            assert(session.presentation.engineering_attack.effect_frame ==
                   (t < 8 ? -1 : t - 8));
            /* Calling the route runner cannot consume EA5 during attack. */
            assert(rich4_app_route_event_update(&session, NULL, now, 0, &services));
            assert(session.flow.property_turn_end_ticks == 8);
        }
    }
    assert(sound_count == 1);
    assert(!session.presentation.engineering_attack.active);
    assert(session.presentation.actors[0].direction == 6);
    assert(session.state.fixed_asset_value[1] == 100);
    assert(session.state.land_value[0] == 100);
    for (t = 1; t <= 8; ++t) {
        now = 1000 + (uint32_t)((16 + t) * tick_ms);
        assert(rich4_app_route_event_update(&session, NULL, now, 0, &services));
        assert(session.state.current_player == (t < 8 ? 0 : 1));
    }
}

static void facility_and_pulse(void)
{
    int t;
    setup();
    session.state.route_object_raw[1] = 4001;
    session.state.facility_count = 1;
    session.state.facility_owner[0] = -1;
    session.state.facility_type[0] = 4;
    session.state.facility_level[0] = 5;
    session.state.facility_runtime_value[0] = 123;
    session.state.facility_last_change[0] = 456;
    session.state.facility_research_turns[0] = 7;
    session.state.players[1].delayed_status_block = 5;
    session.state.players[2].delayed_status_block = 3;
    session.state.players[2].play_state = 0;
    assert(rich4_engineering_attack_begin(&session, 0, 1, 1000));
    /* No guessed animation when metadata is absent. */
    assert(rich4_engineering_attack_update(&session, 0, 0, 80, 2000, NULL));
    assert(!session.presentation.engineering_attack.initialized);
    assert(rich4_engineering_attack_update(&session, 6, 8, 80, 2000, NULL));
    assert(session.presentation.engineering_attack.hold_ticks == 6);
    assert(rich4_engineering_attack_update(&session, 6, 8, 80, 2000, NULL));
    assert(session.presentation.engineering_attack.hold_ticks == 6);
    for (t = 1; t <= 9; ++t)
        assert(rich4_engineering_attack_update(
            &session, 6, 8, 80, 1960 + t * 80, NULL));
    assert(session.state.facility_level[0] == 0);
    assert(session.state.facility_type[0] == 0);
    assert(session.state.facility_owner[0] == -1);
    assert(session.state.facility_runtime_value[0] == 123);
    assert(session.state.facility_last_change[0] == 456);
    assert(session.state.facility_research_turns[0] == 7);
    assert(session.state.players[1].delayed_status_block == 0x80);
    assert(session.state.players[2].delayed_status_block == 3);
}

static void return_path(void)
{
    Rich4AppRouteEventServices services = {0};
    Rich4AppTurnRotationResult result;
    setup();
    session.state.players[0].play_state = 0x11;
    assert(rich4_app_complete_route_event_turn(&session, NULL, 1000, &result));
    assert(session.state.current_player == 0);
    assert(session.state.players[0].play_state == 1);
    assert(session.presentation.engineering_attack.pending_return);
    assert(rich4_app_route_event_update(&session, NULL, 1000, 0, &services));
    assert(session.presentation.engineering_attack.active);
    assert(!session.presentation.engineering_attack.pending_return);
    assert(session.flow.property_turn_end_ticks == 0);
    assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
    setup();
    session.state.players[0].play_state = 0x11;
    assert(rich4_app_advance_turn_rotation(&session, NULL, 1000, &result));
    session.flow.turn_state = GAME_TURN_STATE_IDLE;
    session.flow.turn_flow_active = 0;
    {
        Rich4AppGameUpdatePlan plan;
        assert(rich4_app_game_update_prepare(&session, 1000, &plan));
        assert(plan.stage == RICH4_APP_GAME_UPDATE_ROUTE_EVENTS);
        assert(plan.turn_start.action == RICH4_TURN_START_NONE);
    }
}

static int update_god_presentation(void *context)
{
    return rich4_property_update_charge_presentation(
        &session, *(uint32_t *)context);
}

static void finish_land_god(Rich4AppRouteEventServices *services,
                            uint32_t *now)
{
    Rich4SpeechAudioRequest audio;
    /* 0x40F82F commits ownership before the 0x40F878 notice and
     * 0x40F8AB speech; neither caller may check demolition yet. */
    assert(session.presentation.property.phase ==
           GAME_PROPERTY_PRESENT_GOD_BUILD_TEXT);
    assert(session.state.land_owner[0] == 0);
    *now = 2499;
    assert(rich4_app_route_event_update(&session, NULL, *now, 0, services));
    assert(session.presentation.property.phase ==
           GAME_PROPERTY_PRESENT_GOD_BUILD_TEXT);
    assert(session.flow.property_turn_end_ticks == 0);
    assert(!session.presentation.engineering_attack.active);
    *now = 2500;
    assert(rich4_app_route_event_update(&session, NULL, *now, 0, services));
    assert(session.presentation.speech.active);
    assert(rich4_speech_prepare_audio(&session, &audio));
    *now = 4500;
    assert(rich4_speech_finish_update(&session, *now, 1));
    assert(rich4_app_route_event_update(&session, NULL, *now, 0, services));
    assert(session.flow.property_turn_end_ticks == 0);
    assert(!session.presentation.engineering_attack.active);
    ++*now;
    assert(!rich4_speech_finish_update(&session, *now, 0));
    assert(rich4_app_route_event_update(&session, NULL, *now, 0, services));
    assert(session.presentation.property.phase == GAME_PROPERTY_PRESENT_NONE);
}

static void god_precedes_attack(void)
{
    Rich4AppRouteEventServices services = {0};
    RouteEvent fact = {0};
    uint32_t now = 1000;
    services.update_property_charge = update_god_presentation;
    services.property_context = &now;
    setup();
    session.state.players[0].god_type = 12;
    session.state.players[0].god_info = 1;
    fact.phase = ROUTE_EVENT_PHASE_STOP;
    fact.node = 1;
    fact.object_ref = rich4_map_object_ref(2001);
    rich4_route_event_collection_push(&session.flow.route_facts, &fact);
    assert(rich4_app_route_event_update(&session, NULL, now, 0, &services));
    finish_land_god(&services, &now);
    assert(session.flow.turn_wait == RICH4_ROUTE_END_COUNTDOWN);
    assert(session.state.land_owner[0] == 0);
    assert(session.state.land_level[0] == 3);
    assert(!session.presentation.engineering_attack.active);
    assert(session.flow.property_turn_end_ticks == 8);

    /* The second caller must also drain god results without running the
     * old route executor or applying a fresh eight-tick delay. */
    setup();
    now = 1000;
    session.state.players[0].god_type = 12;
    session.state.players[0].god_info = 1;
    session.state.players[0].play_state = 0x11;
    assert(rich4_app_complete_route_event_turn(&session, NULL, 1000, NULL));
    assert(session.presentation.engineering_attack.pending_return);
    finish_land_god(&services, &now);
    assert(session.state.land_owner[0] == 0);
    assert(!session.presentation.engineering_attack.active);
    assert(!session.presentation.engineering_attack.pending_return);
    assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
    assert(session.flow.property_turn_end_ticks == 0);
}

static void waits_for_both_animations(void)
{
    int t;
    setup();
    assert(rich4_engineering_attack_begin(&session, 0, 1, 1000));
    for (t = 0; t <= 24; ++t) {
        assert(rich4_engineering_attack_update(
            &session, 20, 8, 80, 1080 + t * 80, NULL));
        assert(session.presentation.engineering_attack.active == (t < 24));
    }
}

int main(void)
{
    gates();
    timeline(120, 5);
    timeline(80, 6);
    timeline(40, 5);
    facility_and_pulse();
    return_path();
    god_precedes_attack();
    waits_for_both_animations();
    puts("engineering attack flow: ok");
    return 0;
}
