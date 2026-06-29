#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/tool_followup_flow.h"
#include "core/game_event_payload.h"
#include "core/tool.h"

int main(void)
{
    GameSession session;
    GameCommand command;
    GameEvent event;
    Rich4AudioEventQueue audio_events;
    Rich4AudioEvent audio_event;
    GameEventQueue events;
    Rich4ToolRobotUpdate update;
    GameViewportModel viewport;
    int16_t route_x[3] = {0, 100, 200};
    int16_t route_y[3] = {0, 10, 20};
    uint16_t route_next[3][4] = {{0, 0, 0, 0}, {2, 0, 0, 0},
                                 {1, 0, 0, 0}};

    memset(&session, 0, sizeof(session));
    memset(&viewport, 0, sizeof(viewport));
    session.state.player_count = 1;
    session.state.players[0].node = 1;
    assert(rich4_tool_set_map_geometry(
        &session.state, 3, route_x, route_y, route_next,
        0, NULL, NULL, 0, NULL, NULL));
    command = rich4_game_command_use_tool(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, 1), 0,
        rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 2),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1));
    {
        Rich4GameEventFields fields = {
            0, 0, 0, 1, 0,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1)
        };
        event = rich4_game_event_map_object_changed_from_fields(fields);
    }
    assert(rich4_tool_followup_begin_placement(
        &session, &command, &event, 1000));
    assert(session.presentation.dynamic_objects[16].type == 16);
    assert(session.presentation.dynamic_objects[16].node == 2);
    assert(session.presentation.dynamic_objects[16].flight_active == 1);
    rich4_audio_event_queue_init(&audio_events);
    assert(rich4_tool_followup_update_placement(
        &session, &viewport, 1000, &audio_events));
    assert(session.presentation.tool_placement_animation.until > 1100);
    assert(!rich4_tool_followup_update_placement(
        &session, &viewport,
        session.presentation.tool_placement_animation.until,
        &audio_events));
    assert(session.presentation.dynamic_objects[16].flight_active == 0);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_TOOL_PLACEMENT);
    assert(audio_event.value == 1);

    session.state.tool_robot.active = 1;
    session.state.tool_robot.node = 1;
    session.state.tool_robot.next_node = 2;
    session.state.tool_robot.steps_remaining = 1;
    session.state.tool_robot.owner = 0;
    rich4_game_event_queue_reset(&events);
    rich4_audio_event_queue_init(&audio_events);
    assert(rich4_tool_followup_mark_robot_trigger(&session, 2000));
    assert(rich4_tool_followup_update_robot(
        &session, NULL, 40, 2000, &events, &audio_events, &update));
    assert(update.active && update.node == 1);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_TOOL_USE && audio_event.value == 0);
    assert(!rich4_audio_event_pop(&audio_events, &audio_event));
    assert(rich4_tool_followup_update_robot(
        &session, NULL, 40, 2040, &events, &audio_events, &update));
    assert(session.presentation.tool_robot_motion.world_x == 107);
    assert(session.presentation.tool_robot_motion.animation_frame == 1);
    /* A late host frame consumes one pulse, not several movement steps. */
    assert(rich4_tool_followup_update_robot(
        &session, NULL, 40, 2440, &events, &audio_events, &update));
    assert(session.presentation.tool_robot_motion.progress == 2);
    for (int tick = 3; tick <= 12; tick++)
        assert(rich4_tool_followup_update_robot(
            &session, NULL, 40, 2440 + (uint32_t)(tick - 2) * 40,
            &events, &audio_events, &update));
    assert(session.presentation.tool_robot_motion.world_x == 200);
    assert(session.presentation.tool_robot_motion.world_y == 20);
    assert(!update.stopped && session.state.tool_robot.active);
    assert(rich4_tool_followup_update_robot(
        &session, NULL, 40, 2880, &events, &audio_events, &update));
    assert(!update.stopped && update.node == 2);
    assert(session.presentation.tool_robot_motion.end_wait == 5);
    assert(session.presentation.tool_robot_motion.animation_frame == 0);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_TOOL_ROBOT_STOP);
    session.presentation.has_view_override = 1;
    /* No flight: five stationary pulses plus the three-pulse return. */
    for (int tick = 1; tick <= 8; tick++) {
        assert(rich4_tool_followup_update_robot(
            &session, NULL, 40, 2880 + tick * 40,
            &events, &audio_events, &update));
        assert(update.stopped == (tick == 8));
        assert(session.state.tool_robot.active == (tick < 8));
    }
    assert(!session.presentation.has_view_override);
    assert(session.presentation.actors[0].dirty_flags & GAME_PLAYER_DIRTY_TURN);
    assert(!rich4_audio_event_pop(&audio_events, &audio_event));

    /* Two 16-unit edges at a different game speed: two pulses per edge,
     * arrival handling shares the first pulse of the following edge. */
    session.state.tool_route_x[1] = 0;
    session.state.tool_route_x[2] = 16;
    session.state.tool_route_y[1] = session.state.tool_route_y[2] = 0;
    session.state.tool_robot.active = 1;
    session.state.tool_robot.node = 1;
    session.state.tool_robot.next_node = 2;
    session.state.tool_robot.steps_remaining = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_followup_mark_robot_trigger(&session, 4000));
    assert(rich4_tool_followup_update_robot(
        &session, NULL, 80, 4080, &events, &audio_events, &update));
    assert(session.presentation.tool_robot_motion.world_x == 8);
    assert(rich4_tool_followup_update_robot(
        &session, NULL, 80, 4160, &events, &audio_events, &update));
    assert(session.presentation.tool_robot_motion.world_x == 16);
    assert(rich4_tool_followup_update_robot(
        &session, NULL, 80, 4240, &events, &audio_events, &update));
    assert(session.state.tool_robot.node == 2 &&
           session.state.tool_robot.next_node == 1);
    assert(session.presentation.tool_robot_motion.world_x == 8 &&
           session.presentation.tool_robot_motion.animation_frame == 3);
    assert(rich4_tool_followup_update_robot(
        &session, NULL, 80, 4320, &events, &audio_events, &update));
    assert(session.presentation.tool_robot_motion.world_x == 0);
    assert(rich4_tool_followup_update_robot(
        &session, NULL, 80, 4400, &events, &audio_events, &update));
    assert(!update.stopped && update.node == 1);
    /* Any of the 46 flights holds the return, not only placed tools. */
    session.presentation.dynamic_objects[0].flight_active = 2;
    for (int tick = 1; tick <= 7; tick++) {
        assert(rich4_tool_followup_update_robot(
            &session, NULL, 80, 4400 + tick * 80,
            &events, &audio_events, &update));
        assert(!update.stopped && session.state.tool_robot.active);
    }
    assert(session.presentation.tool_robot_motion.end_wait == 1);
    session.presentation.dynamic_objects[0].flight_active = 0;
    for (int tick = 8; tick <= 11; tick++) {
        assert(rich4_tool_followup_update_robot(
            &session, NULL, 80, 4400 + tick * 80,
            &events, &audio_events, &update));
        assert(update.stopped == (tick == 11));
    }

    session.presentation.tool_trigger_turn_pending = 1;
    session.flow.turn_flow_active = 1;
    session.flow.steps_remaining = 2;
    assert(rich4_tool_followup_finish_trigger_turn(&session, 3000));
    assert(!session.presentation.tool_trigger_turn_pending);
    assert(!session.flow.turn_flow_active && session.flow.steps_remaining == 0);

    session.presentation.tool_trigger_turn_pending = 1;
    session.flow.turn_flow_active = 1;
    session.flow.route_events.state.dispatch.total_count = 1;
    session.flow.route_events.state.dispatch.phase =
        GAME_ROUTE_EVENT_DISPATCH_STOP;
    assert(rich4_tool_followup_finish_trigger_turn_and_advance(
        &session, NULL, 3100, &(Rich4AppTurnRotationResult){0}));
    assert(session.flow.turn_flow_active &&
           session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);

    puts("app tool-followup flow: ok");
    return 0;
}
