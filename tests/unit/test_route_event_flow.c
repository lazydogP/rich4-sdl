#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/audio_event.h"
#include "app/command_event_flow.h"
#include "app/game_speech_flow.h"
#include "app/point_minigame_flow.h"
#include "app/route_event_flow.h"
#include "app/route_flow.h"
#include "app/turn_rotation_flow.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/route_event.h"
#include "core/rng.h"

static RouteEvent road_event(void)
{
    RouteEvent event;
    memset(&event, 0, sizeof(event));
    event.phase = ROUTE_EVENT_PHASE_PASS;
    event.node = 3;
    event.object_ref = rich4_map_object_ref(3);
    event.snapshot.player = 0;
    event.snapshot.event_kind = GAME_ROUTE_EVENT_ROAD_NODE;
    return event;
}

static GameEvent make_route_presentation_event(GameEventType type)
{
    Rich4GameEventFields fields = {
        0, 7, 8, 9, 10,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1)
    };
    switch (type) {
    case GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED:
        return rich4_game_event_special_event_presentation(0, 7, 8, 10u);
    case GAME_EVENT_PROPERTY_LEVEL_CHANGED:
        return rich4_game_event_property_level_changed_from_fields(fields);
    case GAME_EVENT_STATUS_CHANGED:
        return rich4_game_event_status_changed_from_fields(fields);
    case GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED:
        return rich4_game_event_special_place_request(0, 7, 8, 9, 10u);
    case GAME_EVENT_LOTTERY_VISIT_REQUESTED:
        return rich4_game_event_lottery_visit_request(0, 7, 8, 9, 10u);
    default:
        return rich4_game_event_route_event_resolved(0, 0, 0, 0, 0);
    }
}

typedef struct RouteUpdateProbe {
    int charge_calls;
    int charge_result;
    int max_start_calls;
    int max_update_calls;
    int present_calls;
    int actor_calls;
    int audio_calls;
} RouteUpdateProbe;

static int probe_property_charge(void *opaque)
{
    RouteUpdateProbe *probe = opaque;
    probe->charge_calls++;
    return probe->charge_result;
}

static int probe_property_max_update(void *opaque)
{
    RouteUpdateProbe *probe = opaque;
    probe->max_update_calls++;
    return 0;
}

static void probe_property_max_start(void *opaque)
{
    RouteUpdateProbe *probe = opaque;
    probe->max_start_calls++;
}

static void probe_presentation(
    void *opaque, const Rich4AppRoutePresentation *presentation)
{
    RouteUpdateProbe *probe = opaque;
    assert(presentation != NULL);
    probe->present_calls++;
}

static void probe_special_actor(
    void *opaque, const Rich4AppTurnRotationResult *result)
{
    RouteUpdateProbe *probe = opaque;
    assert(result != NULL);
    probe->actor_calls++;
}

static void probe_audio(void *opaque, const Rich4AudioEvent *event)
{
    RouteUpdateProbe *probe = opaque;
    assert(event != NULL);
    probe->audio_calls++;
}

static void hosted_minigame_result_then_character_speech(void)
{
    int action, animation, character;
    unsigned int variants_seen = 0;
    for (action = 6; action <= 8; action++)
        for (animation = 0; animation <= 1; animation++)
            for (character = 0; character < 12; character++) {
                GameSession session;
                GameEventQueue events;
                Rich4AudioEventQueue audio;
                Rich4SpeechAudioRequest voice;
                Rich4Rng expected_rng, global_before, global_after;
                const Rich4CharacterSpeech *expected_speech;
                RouteEvent stop = road_event();
                int amount, variant;
                uint16_t awarded_points;
                memset(&session, 0, sizeof(session));
                session.state.active = 1;
                session.state.player_count = 1;
                session.state.players[0].play_state = 5; /* hosted human */
                session.state.players[0].character = character;
                session.state.players[0].points = 65520;
                session.state.ai_rng_state =
                    (unsigned int)(character + 1);
                rich4_rng_state_seed(&expected_rng, character + 1);
                amount = rich4_rng_original_state_range(&expected_rng, 20) + 50;
                variant = rich4_rng_original_state_next(&expected_rng) & 1;
                variants_seen |= 1u << variant;
                expected_speech = rich4_character_reward_speech(
                    character, 1, variant);
                stop.phase = ROUTE_EVENT_PHASE_STOP;
                stop.snapshot.route_action = action;
                assert(rich4_route_event_collection_push(
                    &session.flow.route_facts, &stop));
                rich4_app_route_prepare_dispatch(&session, 0);
                rich4_audio_event_queue_init(&audio);
                rich4_rng_seed(123);
                rich4_rng_global_export(&global_before);
                assert(rich4_app_route_event_step(
                    &session, 1000, animation, &events, &audio) ==
                       ROUTE_EVENT_EXECUTION_PROGRESSED);
                assert(session.presentation.point_minigame.active &&
                       session.presentation.point_minigame.action == action &&
                       session.presentation.point_minigame.player == 0 &&
                       session.presentation.point_minigame.score == amount &&
                       session.presentation.point_minigame.phase ==
                           RICH4_POINT_MINIGAME_RESULT &&
                       session.presentation.point_minigame.finish_until == 3000);
                awarded_points = (uint16_t)(65520 + amount);
                assert(session.state.players[0].points == awarded_points);
                assert(!session.presentation.speech.active);
                assert(rich4_point_minigame_update(
                    &session, 0, 0, NULL, 2999, 320, &audio));
                assert(session.presentation.point_minigame.active &&
                       !session.presentation.speech.active);
                /* The transition frame stays owned by presentation, so
                 * route completion cannot overtake the new speech. */
                assert(rich4_point_minigame_update(
                    &session, 0, 0, NULL, 3000, 320, &audio));
                assert(!session.presentation.point_minigame.active &&
                       session.presentation.speech.active &&
                       session.presentation.speech.current == expected_speech &&
                       session.presentation.speech.player == 0 &&
                       session.presentation.speech.type == 0);
                assert(rich4_speech_prepare_audio(&session, &voice));
                assert(voice.start_resource == expected_speech->speaking_resource);
                assert(rich4_speech_finish_update(&session, 5000, 1));
                assert(!rich4_speech_finish_update(&session, 5000, 0));
                assert(!rich4_point_minigame_update(
                    &session, 0, 0, NULL, 5001, 320, &audio));
                assert(session.state.players[0].points == awarded_points);
                assert(session.state.ai_rng_state == expected_rng.state);
                rich4_rng_global_export(&global_after);
                assert(global_after.state == global_before.state);
            }
    assert(variants_seen == 3u);
}

int main(void)
{
    GameSession session;
    GameEventQueue presentation_events;
    Rich4AudioEventQueue audio_events;
    RouteEvent event = road_event();

    {
        const GameEventType presentation_types[] = {
            GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED,
            GAME_EVENT_PROPERTY_LEVEL_CHANGED,
            GAME_EVENT_STATUS_CHANGED,
            GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED,
            GAME_EVENT_LOTTERY_VISIT_REQUESTED
        };
        const Rich4AppRoutePresentationKind presentation_kinds[] = {
            RICH4_ROUTE_PRESENTATION_SPECIAL_EVENT,
            RICH4_ROUTE_PRESENTATION_PROPERTY_LEVEL,
            RICH4_ROUTE_PRESENTATION_STATUS,
            RICH4_ROUTE_PRESENTATION_SPECIAL_PLACE,
            RICH4_ROUTE_PRESENTATION_LOTTERY_VISIT
        };
        Rich4AppRoutePresentation presentation;
        GameEvent route_presentation_event;
        size_t index;
        for (index = 0; index < sizeof(presentation_types) /
                sizeof(presentation_types[0]); ++index) {
            route_presentation_event = make_route_presentation_event(
                presentation_types[index]);
            assert(rich4_app_route_event_presentation(
                &route_presentation_event, &presentation));
            assert(presentation.kind == presentation_kinds[index]);
            assert(presentation.event.type == presentation_types[index]);
        }
        route_presentation_event = rich4_game_event_route_event_resolved(
            0, 0, 0, 0, 0);
        assert(!rich4_app_route_event_presentation(
            &route_presentation_event, &presentation));
        assert(presentation.kind == RICH4_ROUTE_PRESENTATION_NONE);
    }

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].play_state = 1;
    assert(rich4_route_event_collection_push(
        &session.flow.route_facts, &event));
    rich4_app_route_prepare_dispatch(&session, 0);

    assert(rich4_app_route_event_step(
        &session, 100, 1, &presentation_events, &audio_events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(presentation_events.count == 0);
    assert(audio_events.count == 0);
    assert(session.flow.route_events.queue.count == 0);
    assert(session.flow.route_events.state.dispatch.handled_count == 1);

    assert(rich4_app_route_event_step(
        &session, 120, 1, &presentation_events, &audio_events) ==
           ROUTE_EVENT_EXECUTION_FINISHED);
    assert(!rich4_app_route_event_finish(&session, 120));
    assert(session.flow.route_events.state.dispatch.phase ==
           GAME_ROUTE_EVENT_DISPATCH_DONE);
    session.flow.turn_flow_active = 1;
    session.flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    session.state.current_player = 0;
    {
        Rich4AppTurnRotationResult rotation = { 0 };
        assert(rich4_app_complete_route_event_turn(
            &session, NULL, 120, &rotation));
    }
    assert(session.flow.turn_flow_active == 0);
    assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
    assert((session.presentation.actors[0].dirty_flags &
            GAME_PLAYER_DIRTY_TURN) != 0);

    {
        RouteUpdateProbe probe = { 0 };
        Rich4AppRouteEventServices services;
        session.flow.turn_flow_active = 1;
        session.flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
        rich4_app_route_reset_facts(&session);
        assert(rich4_route_event_collection_push(
            &session.flow.route_facts, &event));
        rich4_app_route_prepare_dispatch(&session, 0);
        memset(&services, 0, sizeof(services));
        services.property_context = &probe;
        services.presentation_context = &probe;
        services.actor_context = &probe;
        services.audio_context = &probe;
        services.property_end_delay_ms = 30;
        services.update_property_charge = probe_property_charge;
        services.update_property_max_level = probe_property_max_update;
        services.start_property_max_level = probe_property_max_start;
        services.present = probe_presentation;
        services.begin_special_actor = probe_special_actor;
        services.consume_audio_event = probe_audio;
        session.presentation.speech.active = 1;
        probe.charge_result = 1;
        assert(rich4_app_route_event_update(
            &session, NULL, 200, 1, &services) == 1);
        assert(probe.charge_calls == 1);
        assert(session.flow.route_events.state.dispatch.handled_count == 0);
        session.presentation.speech.active = 0;
        probe.charge_result = 0;
        assert(rich4_app_route_event_update(
            &session, NULL, 220, 1, &services) == 1);
        assert(probe.charge_calls == 2);
        assert(rich4_app_route_event_update(
            &session, NULL, 240, 1, &services) == 1);
        assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
        assert(session.flow.property_turn_end_ticks == 1);
        assert(rich4_app_route_event_update(
            &session, NULL, 270, 1, &services) == 1);
        assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
    }

    {
        Rich4GameEventFields fields;
        GameCommand command;
        GameEvent command_event;
        GameEvent resolved_event;
        fields.player = 0;
        fields.subject = 0;
        fields.value = 2;
        fields.value2 = 0;
        fields.flags = 0;
        fields.subject_ref = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
        fields.target_ref = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
        command_event = rich4_game_event_dice_count_changed_from_fields(fields);
        assert(rich4_app_apply_command_event(
                   &session, NULL, &command_event,
                   130) == RICH4_COMMAND_EVENT_CONSUMED);
        assert(session.presentation.dice.status_visible == 1);
        session.presentation.route_interaction.active = 1;
        resolved_event = rich4_game_event_route_event_resolved(
            0, 0, 0, 3, 0);
        assert(rich4_app_apply_command_event(
                   &session, NULL, &resolved_event,
                   130) == RICH4_COMMAND_EVENT_CONSUMED);
        assert(session.presentation.route_interaction.active == 0);
        session.presentation.route_interaction.active = 1;
        session.presentation.route_interaction.accept.type =
            GAME_COMMAND_BANK_APPLY_LOAN;
        session.presentation.bank_interaction.amount_entry = 1;
        session.presentation.bank_interaction.stage =
            RICH4_BANK_PRESENT_LOAN_AMOUNT;
        session.presentation.bank_interaction.mode = 0;
        command = rich4_game_command_bank_for_type(
            GAME_COMMAND_BANK_APPLY_LOAN,
            GAME_COMMAND_SOURCE_HUMAN, 0, 50);
        assert(rich4_app_apply_command_event(
                   &session, &command, &resolved_event,
                   140) == RICH4_COMMAND_EVENT_CONSUMED);
        assert(session.presentation.route_interaction.active == 1);
        assert(session.presentation.bank_interaction.stage ==
               RICH4_BANK_PRESENT_LOAN_RESULT_1);
        assert(session.presentation.bank_interaction.amount_entry == 0);
        session.presentation.route_interaction.active = 0;
        command = rich4_game_command_roll_dice(
            GAME_COMMAND_SOURCE_HUMAN, 0);
        command_event = rich4_game_event_command_rejected_from_fields(fields);
        assert(rich4_app_apply_command_event(
                   &session, &command, &command_event,
                   130) == RICH4_COMMAND_EVENT_REJECTED);

        command_event = rich4_game_event_status_changed_from_fields(fields);
        {
            Rich4CommandEventEffect effect =
                rich4_app_command_event_effect(&session, &command,
                                               &command_event);
            assert(effect.kind == RICH4_COMMAND_EVENT_EFFECT_STATUS_SYNC);
            assert(effect.mark_robot_trigger == 0);
        }
        fields.target_ref = rich4_game_domain_ref(GAME_DOMAIN_TOOL, 0);
        command_event = rich4_game_event_status_changed_from_fields(fields);
        session.presentation.tool_animation_count = 1;
        {
            Rich4CommandEventEffect effect =
                rich4_app_command_event_effect(&session, &command,
                                               &command_event);
            assert(effect.kind == RICH4_COMMAND_EVENT_EFFECT_NONE);
        }
        session.presentation.tool_animation_count = 0;
        command = rich4_game_command_use_tool(
            GAME_COMMAND_SOURCE_HUMAN, 0,
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, 8), 0,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1));
        command_event = rich4_game_event_property_level_changed_from_fields(
            fields);
        {
            Rich4CommandEventEffect effect =
                rich4_app_command_event_effect(&session, &command,
                                               &command_event);
            assert(effect.kind == RICH4_COMMAND_EVENT_EFFECT_NONE);
        }

        session.presentation.route_interaction.active = 1;
        command_event = rich4_game_event_god_trade_rejection(
            0, 8, 1500, 130);
        assert(rich4_app_apply_command_event(
                   &session, NULL, &command_event,
                   150) == RICH4_COMMAND_EVENT_CONSUMED);
        assert(session.presentation.property.phase ==
                   GAME_PROPERTY_PRESENT_GOD_TRADE_REJECTION &&
               session.presentation.property.amount == 8 &&
               session.presentation.route_interaction.active == 0);
    }
    {
        GameEvent notice = rich4_game_event_magic_house_auto_request(0, 3, 1u, 6);
        assert(rich4_app_route_event_apply(
            &session, &notice, NULL, 5000, 0, NULL));
        assert(session.presentation.magic_house.active &&
               session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_AI_NOTICE &&
               session.presentation.magic_house.phase_until == 6500 &&
               session.presentation.magic_house.selected_action == 6);
    }
    hosted_minigame_result_then_character_speech();
    puts("app route event flow: ok");
    return 0;
}
