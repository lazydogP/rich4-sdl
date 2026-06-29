#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/game_speech_flow.h"
#include "core/game_constants.h"
#include "data/character_speech.h"

#ifdef main
#undef main
#endif

static void test_speech_camera(void)
{
    static GameSession s;
    const Rich4CharacterSpeech *speech = rich4_character_property_speech(
        1, RICH4_PROPERTY_SPEECH_RECEIVER_HIGH_1);
    memset(&s, 0, sizeof(s));
    s.state.player_count = 2;
    s.state.players[0].node = 1;
    s.state.players[1].node = 2;
    s.state.tool_route_node_count = 3;
    s.state.tool_route_x[1] = 300; s.state.tool_route_y[1] = 400;
    s.state.tool_route_x[2] = 700; s.state.tool_route_y[2] = 800;
    s.presentation.map_direction = 5;
    /* Stale actor caches must not redirect an ordinary stationary speaker. */
    s.presentation.actors[1].visual_x = 99;
    s.presentation.actors[1].visual_y = 88;
    assert(game_start_speech_entry(&s, 1, speech, 3, 0, 100));
    assert(s.presentation.has_view_override);
    assert(s.presentation.view_world_x == 700 && s.presentation.view_world_y == 800);
    assert(s.state.current_player == 0 && s.presentation.map_direction == 5);

    s.presentation.view_world_x = 900;
    assert(!game_start_speech_entry(&s, 1, speech, 3, 0, 200));
    assert(s.presentation.view_world_x == 900); /* duplicate text */
    s.presentation.speech.last_text = NULL;
    s.state.players[1].days_sleeping = 1;
    assert(!game_start_speech_entry(&s, 1, speech, 3, 0, 200));
    assert(s.presentation.view_world_x == 900); /* suppressed speaker */
    s.state.players[1].days_sleeping = 0;
    s.presentation.speech.last_text = NULL;
    assert(game_start_speech_entry(&s, 1 | GAME_SPEECH_KEEP_VIEW,
        speech, 3, 0, 200));
    assert(s.presentation.speech.player == 1);
    assert(s.presentation.view_world_x == 900);

    s.presentation.speech.last_text = NULL;
    s.state.players[1].node = 1;
    assert(game_start_speech_entry(&s, 1, speech, 3, 0, 200));
    assert(!s.presentation.has_view_override); /* coordinate equality, not identity */
    s.state.players[1].node = 2;
    s.presentation.speech.last_text = NULL;
    s.presentation.actors[1].move_active = 1;
    assert(game_start_speech_entry(&s, 1, speech, 3, 0, 200));
    assert(s.presentation.view_world_x == 99 && s.presentation.view_world_y == 88);
    s.presentation.actors[1].move_active = 0;
    s.state.players[1].confinement_actor_type = 2;
    s.presentation.actors[1].visual_x = 1100;
    s.presentation.actors[1].visual_y = 1200;
    s.presentation.speech.last_text = NULL;
    assert(game_start_speech_entry(&s, 1, speech, 2, 0, 200));
    assert(s.presentation.view_world_x == 1100 && s.presentation.view_world_y == 1200);

    s.state.turn_rotation_index = 4;
    s.presentation.special_actor.active = 1;
    s.presentation.special_actor.slot = 0;
    s.presentation.special_actor.visual_x = 1100;
    s.presentation.special_actor.visual_y = 1200;
    game_focus_view_at(&s, 1100, 1200);
    assert(!s.presentation.has_view_override);
    game_focus_view_at(&s, 700, 800);
    assert(s.presentation.has_view_override);
    assert(s.presentation.map_direction == 5);
}

int main(void)
{
    GameSession session = { 0 };
    const Rich4CharacterSpeech *speech;
    Rich4SpeechAudioRequest request;

    test_speech_camera();

    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].character = 0;
    speech = rich4_character_property_speech(
        0, RICH4_PROPERTY_SPEECH_RECEIVER_HIGH_1);
    assert(speech != NULL);
    assert(game_start_speech_entry(&session, 0, speech, 0, 7, 100));
    assert(session.presentation.speech.active);
    assert(session.presentation.speech.current == speech);
    assert(rich4_speech_prepare_audio(&session, &request));
    assert(request.start_resource == speech->speaking_resource);
    assert(!request.stop);
    assert(session.presentation.speech.voice_started);
    assert(rich4_speech_finish_update(&session, 200, 1));

    rich4_speech_request_skip(&session);
    assert(session.presentation.speech.skip_requested);
    assert(rich4_speech_prepare_audio(&session, &request));
    assert(request.stop);
    assert(rich4_speech_finish_update(&session, 300, 0) == 0);
    assert(!session.presentation.speech.active);
    assert(session.presentation.property.phase == 7);

    speech = rich4_character_victory_speech(0);
    assert(speech != NULL && speech->speaking_resource == 1074);
    assert(game_start_speech_entry(
        &session, 0, speech, 3, GAME_PROPERTY_PRESENT_NONE, 350));
    assert(rich4_speech_prepare_audio(&session, &request));
    assert(request.start_resource == 1074 && !request.stop);
    rich4_speech_request_skip(&session);
    assert(rich4_speech_finish_update(&session, 351, 0) == 0);

    session.presentation.property.purchase_delay_pending = 1;
    assert(game_start_property_speech(
        &session, 0, RICH4_PROPERTY_SPEECH_RECEIVER_LOW, 0,
        GAME_PROPERTY_PRESENT_NONE, 400));
    session.presentation.speech.skip_requested = 1;
    assert(rich4_speech_prepare_audio(&session, &request));
    assert(rich4_speech_finish_update(&session, 500, 0) == 0);
    assert(session.presentation.property.purchase_delay_pending == 0);
    assert(session.flow.property_turn_end_ticks == GAME_PROPERTY_TURN_END_TICKS);

    /* AI must not execute during either notice or speech, and remote dice
     * must not introduce the human-only line (0x4470F8). */
    for (int tool = 0; tool < GAME_TOOL_COUNT; ++tool) {
        GameCommand command = {0};
        int post = tool == 0 || tool == 4 || tool == 5 || tool == 11;
        memset(&session.presentation, 0, sizeof(session.presentation));
        command.type = GAME_COMMAND_USE_TOOL;
        command.source = GAME_COMMAND_SOURCE_AI;
        command.player = 0;
        command.subject.kind = GAME_DOMAIN_TOOL;
        command.subject.id = tool;
        assert(rich4_ai_tool_begin(&session, &command, 100));
        assert(!rich4_ai_tool_begin(&session, &command, 100));
        assert(!rich4_ai_tool_command_ready(&session, 1599));
        assert(!session.presentation.speech.active);
        assert(rich4_ai_tool_command_ready(&session, 1600) == (post || tool == 7));
        if (!post && tool != 7) {
            assert(session.presentation.speech.active);
            assert(rich4_speech_prepare_audio(&session, &request));
            assert(request.start_resource >= 0);
            assert(rich4_speech_finish_update(&session, 2599, 0));
            assert(rich4_speech_finish_update(&session, 3600, 1));
            assert(!rich4_speech_finish_update(&session, 3601, 0));
            assert(rich4_ai_tool_command_ready(&session, 3601));
        }
        rich4_ai_tool_command_completed(&session, 1, 4000);
        assert(session.presentation.ai_tool.phase == GAME_AI_TOOL_EFFECTS);
        assert(session.presentation.speech.active == post);
        assert(!rich4_ai_tool_command_ready(&session, 5000));
    }
    {
        GameCommand command = session.presentation.ai_tool.command;
        memset(&session.presentation, 0, sizeof(session.presentation));
        assert(rich4_ai_tool_begin(&session, &command, 100));
        assert(rich4_ai_tool_skip_notice(&session));
        assert(!rich4_ai_tool_command_ready(&session, 101));
        assert(session.presentation.speech.active);
        assert(!rich4_ai_tool_skip_notice(&session));
        rich4_speech_request_skip(&session);
        assert(rich4_speech_prepare_audio(&session, &request) && request.stop);
        assert(!rich4_speech_finish_update(&session, 102, 0));
        assert(rich4_ai_tool_command_ready(&session, 102));
        rich4_ai_tool_command_completed(&session, 0, 102);
        assert(!session.presentation.speech.active);
    }
    puts("app speech flow: ok");
    return 0;
}
