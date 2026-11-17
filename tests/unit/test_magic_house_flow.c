#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/magic_house_flow.h"
#include "app/game_speech_flow.h"
#include "app/insurance_flow.h"
#include "app/special_place_notice_flow.h"
#include "app/tool_animation_flow.h"
#include "app/turn_rotation_flow.h"
#include "core/confinement.h"
#include "core/special_event.h"
#include "core/game_event_payload.h"
#include "view/game_view_model.h"

static void assert_hud_player(const GameSession *session, int player)
{
    GameViewModelInput input;
    GameViewModel view;
    memset(&input, 0, sizeof(input));
    input.presentation_player =
        rich4_magic_house_presentation_player(&session->presentation) + 1;
    assert(rich4_game_view_model_build(&session->state, &input, NULL, &view));
    assert(view.hud.current_player == player);
    assert(view.hud.cash == session->state.players[player].cash);
}

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
        assert(session.presentation.magic_house.active);
        assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_TARGET);
        assert(session.state.players[0].card_inventory[0] != 0);
        assert(session.presentation.magic_house.target_card == 0);
        assert(rich4_magic_house_update(&session, 0, 0, 3999, 0, &audio, &events));
        assert(session.presentation.magic_house.active);
        assert(!rich4_magic_house_update(&session, 0, 0, 4000, 0, &audio, &events));
        assert(!session.presentation.magic_house.active);
        assert(!rich4_route_event_executor_is_waiting(&session.flow.route_events));
        assert(!rich4_magic_house_update(&session, 0, 0, 4001, 0, &audio, &events));
    }
}

static void begin_two_target_magic(
    GameSession *session, int action, uint32_t until)
{
    RouteEvent stop;
    int player;
    memset(session, 0, sizeof(*session));
    memset(&stop, 0, sizeof(stop));
    session->state.active = 1;
    session->state.player_count = 2;
    session->state.price_index = 1;
    for (player = 0; player < 2; player++) {
        session->state.players[player].play_state = 1;
        session->state.players[player].linked_player = -1;
    }
    stop.phase = ROUTE_EVENT_PHASE_STOP;
    stop.snapshot.route_action = 16;
    assert(rich4_route_event_queue_push(
        &session->flow.route_events.queue, &stop));
    session->flow.route_events.state.execution_started = 1;
    session->flow.route_events.state.execution_actor = 0;
    session->flow.route_events.state.waiting_for_presentation = 1;
    session->flow.route_events.state.presentation_action = 16;
    session->flow.route_events.state.presentation_condition = 6;
    session->flow.route_events.state.presentation_targets = 3u;
    session->presentation.magic_house.active = 1;
    session->presentation.magic_house.player = 0;
    session->presentation.magic_house.phase = RICH4_MAGIC_HOUSE_AI_NOTICE;
    session->presentation.magic_house.condition = 6;
    session->presentation.magic_house.targets = 3u;
    session->presentation.magic_house.selected_action = action;
    session->presentation.magic_house.phase_until = until;
    session->presentation.magic_house.target_player = -1;
    session->presentation.magic_house.target_card = -1;
}

static void target_notices_interleave_effects_and_skip(void)
{
    GameSession session;
    GameEventQueue events;
    Rich4AudioEventQueue audio;
    begin_two_target_magic(&session, 3, 1000);
    rich4_game_event_queue_reset(&events);
    rich4_audio_event_queue_init(&audio);
    assert(rich4_magic_house_update(
        &session, 0, 0, 1000, 0, &audio, &events));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_TARGET);
    assert(session.presentation.magic_house.target_player == 0);
    assert(session.state.players[0].days_stopping == 0);
    assert(session.state.players[1].days_stopping == 0);
    assert(rich4_special_place_notice_skip(&session, 1100));
    assert(rich4_magic_house_update(
        &session, 0, 0, 1100, 0, &audio, &events));
    assert(session.state.players[0].days_stopping == 1);
    assert(session.state.players[1].days_stopping == 0);
    assert(session.presentation.magic_house.target_player == 1);
    assert(rich4_special_place_notice_skip(&session, 1200));
    assert(!rich4_magic_house_update(
        &session, 0, 0, 1200, 0, &audio, &events));
    assert(session.state.players[1].days_stopping == 1);
    assert(!rich4_route_event_executor_is_waiting(
        &session.flow.route_events));
}

static void per_target_holds_precede_next_notice(void)
{
    GameSession session;
    GameEventQueue events;
    begin_two_target_magic(&session, 0, 1000);
    session.state.players[0].card_inventory[0] = 1;
    session.state.players[1].card_inventory[0] = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_magic_house_update(
        &session, 0, 0, 1000, 0, NULL, &events));
    assert(rich4_magic_house_update(
        &session, 0, 0, 2500, 0, NULL, &events));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_EFFECTS);
    assert(session.state.players[0].card_inventory[0] == 0);
    assert(session.state.players[1].card_inventory[0] != 0);
    assert(rich4_magic_house_update(
        &session, 0, 0, 2699, 0, NULL, &events));
    assert(session.presentation.magic_house.target_player == 0);
    assert(rich4_magic_house_update(
        &session, 0, 0, 2700, 0, NULL, &events));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_TARGET);
    assert(session.presentation.magic_house.target_player == 1);
}

static void turn_effect_holds_for_500_ms(void)
{
    GameSession session;
    GameEventQueue events;
    begin_two_target_magic(&session, 7, 1000);
    session.state.tool_route_node_count = 5;
    session.state.players[0].node = 1;
    session.state.players[0].next_node = 2;
    session.state.players[1].node = 2;
    session.state.players[1].next_node = 1;
    session.state.route_object_raw[1] = 0x7d1;
    session.state.route_object_raw[2] = 0x7d1;
    session.state.tool_route_next[1][0] = 2;
    session.state.tool_route_next[1][1] = 3;
    session.state.tool_route_next[2][0] = 1;
    session.state.tool_route_next[2][1] = 4;
    rich4_game_event_queue_reset(&events);
    assert(rich4_magic_house_update(
        &session, 0, 0, 1000, 0, NULL, &events));
    assert(rich4_magic_house_update(
        &session, 0, 0, 2500, 0, NULL, &events));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_EFFECTS);
    assert(session.presentation.magic_house.target_player == 0);
    assert(rich4_magic_house_update(
        &session, 0, 0, 2999, 0, NULL, &events));
    assert(session.presentation.magic_house.target_player == 0);
    assert(rich4_magic_house_update(
        &session, 0, 0, 3000, 0, NULL, &events));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_TARGET);
    assert(session.presentation.magic_house.target_player == 1);
}

static void property_actions_emit_original_flcs(void)
{
    int action;
    for (action = 5; action <= 9; action += 4) {
        GameSession session;
        GameEventQueue events;
        GameEvent effect;
        RouteEvent stop = {0};
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 1;
        session.state.players[0].play_state = 1;
        session.state.players[0].node = 1;
        session.state.tool_route_node_count = 2;
        session.state.route_object_raw[1] = 0x7d1;
        session.state.land_count = 1;
        session.state.land_owner[0] = 0;
        session.state.land_level[0] = action == 5 ? 0 : 1;
        session.state.land_price[0] = 100;
        session.state.land_price_per_level[0] = 50;
        stop.phase = ROUTE_EVENT_PHASE_STOP;
        stop.snapshot.route_action = 16;
        assert(rich4_route_event_queue_push(
            &session.flow.route_events.queue, &stop));
        session.flow.route_events.state.execution_started = 1;
        session.flow.route_events.state.waiting_for_presentation = 1;
        session.flow.route_events.state.presentation_action = 16;
        session.flow.route_events.state.presentation_condition = 6;
        session.flow.route_events.state.presentation_targets = 1u;
        session.presentation.magic_house.active = 1;
        session.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_AI_NOTICE;
        session.presentation.magic_house.condition = 6;
        session.presentation.magic_house.targets = 1u;
        session.presentation.magic_house.selected_action = action;
        session.presentation.magic_house.phase_until = 1000;
        session.presentation.magic_house.target_player = -1;
        rich4_game_event_queue_reset(&events);
        assert(rich4_magic_house_update(
            &session, 0, 0, 1000, 0, NULL, &events));
        assert(rich4_magic_house_update(
            &session, 0, 0, 2500, 0, NULL, &events));
        assert(rich4_game_event_queue_pop(&events, &effect));
        assert(effect.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED);
        assert(rich4_game_event_tool_resource(&effect) ==
               (action == 5 ? 553 : 529));
        assert(rich4_game_event_tool_effect(&effect) ==
               (action == 5 ? 91 : 97));
        assert(rich4_game_event_tool_target(&effect).kind == GAME_DOMAIN_LAND);
        assert(rich4_game_event_tool_target(&effect).id == 0);
    }
}

static void skipped_intro_suppresses_turn_prompt(void)
{
    GameSession session;
    GameEvent event;
    Rich4AudioEventQueue audio;
    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    event = rich4_game_event_magic_house_request(0, 3, 1u);
    rich4_audio_event_queue_init(&audio);
    rich4_magic_house_begin(&session, &event, 1000, 1, &audio);
    rich4_magic_house_skip_intro(&session, 1100, &audio);
    assert(rich4_magic_house_update(
        &session, 0, 0, 1199, 0, &audio, NULL));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_REVEAL);
    assert(rich4_magic_house_update(
        &session, 0, 0, 1200, 0, &audio, NULL));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_CONDITION);
    assert(rich4_magic_house_update(
        &session, 0, 0, 3199, 0, &audio, NULL));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_CONDITION);
    assert(rich4_magic_house_update(
        &session, 0, 0, 3200, 0, &audio, NULL));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_SELECT);
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
        assert(s.presentation.magic_house.active);
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

static void walking_admission_keeps_claimant(void)
{
    static GameSession s;
    RouteEvent route = {0};
    Rich4ToolAnimationCompletion completion;
    GameEvent event;
    int i, tick;
    memset(&s, 0, sizeof(s));
    s.state.active = 1;
    s.state.player_count = 3;
    s.state.current_player = s.state.turn_rotation_index = 2; /* Z */
    s.state.price_index = 1;
    s.state.prison_node = 5;
    s.state.tool_route_node_count = 8;
    s.state.company_count = 1;
    s.state.company_type[0] = 4;
    for (i = 0; i < 3; ++i) {
        s.state.players[i].play_state = 1;
        s.state.players[i].character = i;
        s.state.players[i].node = i + 1;
        s.state.players[i].cash = 10000;
        s.state.players[i].days_assurance = 10;
        s.state.players[i].attached_timed_bomb_slot = -1;
    }
    s.state.players[0].traffic_method = s.state.players[2].traffic_method = 2;
    route.phase = ROUTE_EVENT_PHASE_STOP;
    route.snapshot.route_action = 16;
    assert(rich4_route_event_queue_push(&s.flow.route_events.queue, &route));
    s.flow.route_events.state.execution_started = 1;
    s.flow.route_events.state.execution_actor = 2;
    s.flow.route_events.state.waiting_for_presentation = 1;
    s.flow.route_events.state.presentation_action = 16;
    s.flow.route_events.state.presentation_condition = 6;
    s.flow.route_events.state.presentation_targets = rich4_magic_house_target_mask(&s.state, 6);
    assert(s.flow.route_events.state.presentation_targets == 2u);
    s.presentation.magic_house.active = 1;
    s.presentation.magic_house.player = 2;
    rich4_magic_house_finish(&s, 2, 1000, NULL, NULL);
    assert(s.presentation.magic_house.active);
    assert(s.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_EFFECTS);
    assert(s.presentation.tool_animation_count == 1);
    assert(s.presentation.tool_animation[0].after_sync_player == 1);
    assert_hud_player(&s, 1);
    rich4_tool_animation_skip_optional_confinement(&s);
    for (tick = 1000; tick < 20000 && s.presentation.tool_animation_count; tick += 2000) {
        if (s.presentation.speech.active) {
            rich4_speech_request_skip(&s);
            (void)rich4_speech_finish_update(&s, (uint32_t)tick, 0);
        }
        (void)rich4_tool_animation_update(&s, 1, 1, (uint32_t)tick, NULL, &completion);
        while (rich4_game_event_queue_pop(&completion.events, &event)) {
            assert(event.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED);
            assert(event.player == 1);
            assert(rich4_game_event_insurance_claimant(&event) == 1);
            rich4_insurance_flow_enqueue_confinement_claim(&s, &event, (uint32_t)tick);
        }
        assert_hud_player(&s, 1);
    }
    assert(!s.presentation.tool_animation_count);
    assert(s.presentation.insurance.count == 1);
    assert(rich4_insurance_flow_update(&s, 21000));
    assert(rich4_magic_house_update(&s, 0, 0, 21001, 0, NULL, NULL));
    assert(s.presentation.magic_house.active);
    assert_hud_player(&s, 1);
    assert(!rich4_insurance_flow_update(&s, 23000));
    assert(s.state.players[0].cash == 10000);
    assert(s.state.players[1].cash == 16000);
    assert(s.state.players[2].cash == 10000);
    assert(s.state.players[0].days_in_prison == 0);
    assert(s.state.players[1].days_in_prison == 3);
    /* The target camera belongs to the whole blocking magic-house call.
     * Its return restores the caster before the next turn or dice roll. */
    s.presentation.has_view_override = 1;
    assert(!rich4_magic_house_update(&s, 0, 0, 23001, 0, NULL, NULL));
    assert(!s.presentation.magic_house.active);
    assert(!s.presentation.has_view_override);
    assert_hud_player(&s, 2);
    for (i = 0; i < GAME_SPECIAL_PLAYER_COUNT; ++i)
        s.state.special_player_f10[i] = 1;
    assert(rich4_app_complete_route_event_turn(&s, NULL, 24000, NULL));
    assert(s.state.current_player == 0);
    assert(!s.presentation.has_view_override);
    assert_hud_player(&s, 0);
    {
        GameViewModelInput input = {0};
        GameViewportModel viewport;
        /* A's camera follows its movement immediately after the handoff. */
        input.has_view_override = s.presentation.has_view_override;
        input.view_world_x = input.view_world_y = 900;
        input.actors[0].move_active = 1;
        input.actors[0].visual_x = 123;
        input.actors[0].visual_y = 456;
        rich4_game_viewport_build(&s.state, &input, NULL, &viewport);
        assert(viewport.center_world_x == 123 && viewport.center_world_y == 456);
    }
}

static void turn_action_uses_shared_flow(void)
{
    GameSession session;
    RouteEvent route = {0};
    Rich4AudioEventQueue audio;
    Rich4AudioEvent sound;

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].node = 1;
    session.state.players[0].next_node = 2;
    session.state.tool_route_node_count = 4;
    session.state.tool_route_next[1][0] = 2;
    session.state.tool_route_next[1][1] = 3;
    session.presentation.actors[0].direction = 2;
    session.presentation.actors[0].walk_frame = 3;
    route.phase = ROUTE_EVENT_PHASE_STOP;
    route.snapshot.route_action = 16;
    assert(rich4_route_event_queue_push(
        &session.flow.route_events.queue, &route));
    session.flow.route_events.state.execution_started = 1;
    session.flow.route_events.state.waiting_for_presentation = 1;
    session.flow.route_events.state.presentation_action = 16;
    session.flow.route_events.state.presentation_condition = 6;
    session.flow.route_events.state.presentation_targets = 1;
    session.presentation.magic_house.active = 1;
    session.presentation.has_view_override = 1;

    rich4_audio_event_queue_init(&audio);
    rich4_magic_house_finish(&session, 7, 1000, &audio, NULL);
    assert(session.state.players[0].next_node == 3);
    assert(session.presentation.actors[0].direction == 6);
    assert(session.presentation.actors[0].walk_frame == 0);
    assert(!session.presentation.has_view_override);
    assert(rich4_audio_event_pop(&audio, &sound));
    assert(sound.type == RICH4_AUDIO_EVENT_TURN_AROUND &&
           sound.value == 56);
}

int main(void)
{
    walking_admission_keeps_claimant();
    turn_action_uses_shared_flow();
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
    assert(session.presentation.magic_house.phase_until == 1200);
    assert(!session.presentation.magic_house.animation_enabled);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_SPEECH_STOP);
    session.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_INTRO_3;
    rich4_audio_event_queue_init(&audio_events);
    rich4_magic_house_skip_intro(&session, 1150, &audio_events);
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_INTRO_3);
    assert(!rich4_audio_event_pop(&audio_events, &audio_event));
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
    target_notices_interleave_effects_and_skip();
    per_target_holds_precede_next_notice();
    turn_effect_holds_for_500_ms();
    property_actions_emit_original_flcs();
    skipped_intro_suppresses_turn_prompt();
    puts("app magic-house flow: ok");
    return 0;
}
