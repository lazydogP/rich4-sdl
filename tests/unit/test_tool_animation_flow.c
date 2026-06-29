#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/game_speech_flow.h"
#include "app/tool_animation_flow.h"
#include "core/confinement.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "data/character_speech.h"

static void app_owned_animation_boundaries(void)
{
    GameSession session;
    GameEvent event;
    Rich4ToolAnimationCompletion completion;
    memset(&session, 0, sizeof(session));
    event = rich4_game_event_tool_presentation_requested_from_fields(
        (Rich4GameEventFields){0, 0, 526, 95, 0,
            {GAME_DOMAIN_NONE, -1}, {GAME_DOMAIN_NONE, -1}});
    session.presentation.property.god_type = 10;
    assert(!rich4_tool_animation_enqueue(&session, &event, 0, 30, 0, 0, 0, 1000));
    assert(!session.presentation.property.god_build_animation_started);
    assert(rich4_tool_animation_enqueue(&session, &event, 2, 30, 0, 0, 0, 1000));
    assert(session.presentation.property.god_build_animation_started);
    rich4_tool_animation_skip_optional_confinement(&session);
    assert(!session.presentation.tool_animation[0].animation_done);
    session.presentation.property.property_kind = GAME_DOMAIN_COMPANY;
    session.presentation.property.phase = GAME_PROPERTY_PRESENT_AMOUNT;
    session.presentation.property.tick = 500;
    assert(rich4_tool_animation_update(&session, 2, 30, 1000, NULL, &completion));
    assert(rich4_tool_animation_update(&session, 2, 30, 1059, NULL, &completion));
    assert(session.presentation.property.tick == 500);
    assert(rich4_tool_animation_update(&session, 2, 30, 1060, NULL, &completion));
    assert(session.presentation.tool_animation[0].frame == 1);
    assert(!rich4_tool_animation_update(&session, 2, 30, 1089, NULL, &completion));
    assert(session.presentation.property.tick == 1089);

    memset(&session, 0, sizeof(session));
    event = rich4_game_event_tool_presentation_requested_from_fields(
        (Rich4GameEventFields){0, 0, 524, 92, 0,
            {GAME_DOMAIN_NONE, -1}, {GAME_DOMAIN_NONE, -1}});
    assert(rich4_tool_animation_enqueue(&session, &event, 20, 30, 0, 0, 0, 2000));
    rich4_tool_animation_skip_optional_confinement(&session);
    assert(session.presentation.tool_animation[0].animation_done);
    /* Disabling the FLC still emits actor synchronization, then drains. */
    assert(rich4_tool_animation_update(&session, 20, 30, 2000, NULL, &completion));
    assert(completion.sync_player == 0);
    assert(!rich4_tool_animation_update(&session, 20, 30, 2001, NULL, &completion));
}

static void test_admission_retains_camera(void)
{
    static GameSession s;
    Rich4ToolAnimationCompletion completion;
    Rich4AudioEventQueue audio_events;
    Rich4AudioEvent audio_event;
    int kind;
    for (kind = GAME_CONFINEMENT_PRISON; kind <= GAME_CONFINEMENT_HOSPITAL; ++kind) {
        memset(&s, 0, sizeof(s));
        s.state.active = 1;
        s.state.player_count = 1;
        s.state.players[0].play_state = 1;
        s.state.players[0].node = 1;
        s.state.hospital_node = s.state.prison_node = 2;
        s.state.tool_route_node_count = 3;
        s.state.tool_route_x[1] = 100;
        s.state.tool_route_y[1] = 200;
        s.state.tool_route_x[2] = 900;
        s.state.tool_route_y[2] = 800;
        s.presentation.actors[0].visual_x = 100;
        s.presentation.actors[0].visual_y = 200;
        rich4_audio_event_queue_init(&audio_events);
        assert(rich4_confinement_presentation_enqueue(&s, 0, kind, 3, 1000));
        assert(rich4_tool_animation_update(&s, 40, 50, 1000, &audio_events, &completion));
        assert(s.presentation.speech.active && s.state.players[0].node == 1);
        assert(!rich4_audio_event_pop(&audio_events, &audio_event));
        s.presentation.speech.active = 0;
        s.presentation.has_view_override = 0;
        assert(rich4_tool_animation_update(&s, 40, 50, 2000, &audio_events, &completion));
        assert(s.state.players[0].node == 2);
        assert(s.presentation.has_view_override);
        assert(s.presentation.view_world_x == 100 && s.presentation.view_world_y == 200);
        /* 0x45144F decodes frame zero and starts its effect in the same
         * iteration. The first interval starts when that frame is exposed. */
        assert(s.presentation.tool_animation[0].frame == 0);
        assert(rich4_audio_event_pop(&audio_events, &audio_event));
        assert(audio_event.type == RICH4_AUDIO_EVENT_TOOL_EFFECT);
        assert(audio_event.value == (kind == GAME_CONFINEMENT_PRISON ? 94 : 92));
        assert(rich4_tool_animation_update(&s, 40, 50, 2049, &audio_events, &completion));
        assert(s.presentation.tool_animation[0].frame == 0);
        assert(rich4_tool_animation_update(&s, 40, 50, 2050, &audio_events, &completion));
        assert(s.presentation.tool_animation[0].frame == 1);
        assert(!rich4_audio_event_pop(&audio_events, &audio_event));
        {
            int frame;
            for (frame = 2; frame < 40; ++frame) {
                assert(rich4_tool_animation_update(&s, 40, 50,
                    2000 + (uint32_t)frame * 50, &audio_events, &completion));
                assert(completion.sync_player == -1);
                assert(s.presentation.has_view_override);
            }
        }
        assert(rich4_tool_animation_update(&s, 40, 50, 3999, &audio_events, &completion));
        assert(!s.presentation.tool_animation[0].animation_done);
        assert(rich4_tool_animation_update(&s, 40, 50, 4000, &audio_events, &completion));
        assert(s.presentation.tool_animation[0].animation_done);
        assert(completion.sync_player == 0);
    }
}

static void test_admission_without_flc(void)
{
    static GameSession s;
    Rich4ToolAnimationCompletion completion;
    Rich4AudioEventQueue audio_events;
    Rich4AudioEvent audio_event;
    int kind, repeat;
    for (kind = GAME_CONFINEMENT_PRISON; kind <= GAME_CONFINEMENT_HOSPITAL; ++kind)
    for (repeat = 0; repeat <= 1; ++repeat) {
        memset(&s, 0, sizeof(s));
        s.state.player_count = 1;
        s.state.players[0].play_state = 1;
        s.state.players[0].node = repeat ? 2 : 1;
        s.state.hospital_node = s.state.prison_node = 2;
        if (repeat) {
            s.state.players[0].confinement_actor_type = kind;
            if (kind == GAME_CONFINEMENT_PRISON) s.state.players[0].days_in_prison = 2;
            else s.state.players[0].days_in_hospital = 2;
        }
        rich4_audio_event_queue_init(&audio_events);
        assert(rich4_confinement_presentation_enqueue(&s, 0, kind, 3, 1000));
        if (!repeat) rich4_tool_animation_skip_optional_confinement(&s);
        assert(rich4_tool_animation_update(&s, 40, 50, 1000, &audio_events, &completion));
        if (!repeat) {
            assert(s.presentation.speech.active && s.state.players[0].node == 1);
            s.presentation.speech.active = 0;
            assert(rich4_tool_animation_update(&s, 40, 50, 2000, &audio_events, &completion));
        }
        assert(s.state.players[0].node == 2);
        assert((kind == GAME_CONFINEMENT_PRISON ? s.state.players[0].days_in_prison :
            s.state.players[0].days_in_hospital) == (repeat ? 5 : 3));
        assert(s.presentation.tool_animation[0].animation_done);
        assert(completion.sync_player == 0);
        assert(!rich4_audio_event_pop(&audio_events, &audio_event));
        /* Skipping the FLC still retains the post-sync confinement speech. */
        assert(rich4_tool_animation_update(&s, 40, 50, 2001, &audio_events, &completion));
        assert(s.presentation.speech.active);
    }
}

int main(void)
{
    test_admission_retains_camera();
    test_admission_without_flc();
    GameSession session;
    GameEvent event;
    Rich4AudioEventQueue audio_events;
    Rich4AudioEvent audio_event;
    Rich4ToolAnimationCompletion completion;

    app_owned_animation_boundaries();

    memset(&session, 0, sizeof(session));
    {
        Rich4GameEventFields fields = {
            0, 0, 1, 6, 0,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1)
        };
        event = rich4_game_event_tool_presentation_requested_from_fields(fields);
    }
    assert(rich4_tool_animation_enqueue(
        &session, &event, 2, 114, 100, 200, 1, 1000));
    assert(session.presentation.tool_animation_count == 1);
    assert(session.presentation.view_world_x == 100);
    assert(session.presentation.view_world_y == 200);
    rich4_audio_event_queue_init(&audio_events);
    assert(rich4_tool_animation_update(
        &session, 2, 114, 1000, &audio_events, &completion));
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_TOOL_EFFECT);
    assert(audio_event.value == 6);
    /* 0x45144F waits the final frame, with no synthetic tail. */
    assert(rich4_tool_animation_update(
        &session, 2, 114, 1227, &audio_events, &completion));
    assert(session.presentation.tool_animation[0].frame == 1);
    assert(!rich4_tool_animation_update(
        &session, 2, 114, 1341, &audio_events, &completion));
    assert(session.presentation.tool_animation_count == 0);

    /* A mine chain has two speeches attached to the first animation:
     * the tool reaction, then the pre-hospital reaction.  The request must
     * remain queued between them so the hospital animation cannot overtake
     * either line. */
    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].character = 0;
    assert(rich4_tool_animation_enqueue(
        &session, &event, 1, 100, 0, 0, 0, 2000));
    {
        const Rich4CharacterSpeech *first =
            rich4_character_special_event_speech(0, 1, 0);
        const Rich4CharacterSpeech *second =
            rich4_character_confinement_speech(0, 1);
        assert(first != NULL && second != NULL);
        assert(rich4_tool_animation_defer_speech(&session, 0, first, 1));
        assert(rich4_tool_animation_defer_speech(&session, 0, second, 2));
    }
    rich4_audio_event_queue_init(&audio_events);
    assert(rich4_tool_animation_update(
        &session, 1, 100, 2000, &audio_events, &completion));
    assert(rich4_tool_animation_update(
        &session, 1, 100, 2100, &audio_events, &completion));
    assert(session.presentation.tool_animation_count == 1 &&
           session.presentation.speech.active);
    assert(!rich4_speech_finish_update(&session, 4000, 0));
    assert(rich4_tool_animation_update(
        &session, 1, 100, 4000, &audio_events, &completion));
    assert(session.presentation.tool_animation_count == 1 &&
           session.presentation.speech.active);
    assert(!rich4_speech_finish_update(&session, 6000, 0));
    assert(!rich4_tool_animation_update(
        &session, 1, 100, 6000, &audio_events, &completion));
    assert(session.presentation.tool_animation_count == 0);

    puts("app tool-animation flow: ok");
    return 0;
}
