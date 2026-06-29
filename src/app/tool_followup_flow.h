#ifndef RICH4_APP_TOOL_FOLLOWUP_FLOW_H
#define RICH4_APP_TOOL_FOLLOWUP_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "app/turn_rotation_flow.h"
#include "core/game_command.h"
#include "core/game_event.h"
#include "data/map_data.h"
#include "view/game_view_model.h"

typedef struct Rich4ToolRobotUpdate {
    int active;
    int stopped;
    int node;
    int pulse;
    int launched_object;
} Rich4ToolRobotUpdate;

/* App-owned placement, robot, and trigger-turn follow-up state machines. */
int rich4_tool_followup_begin_placement(
    GameSession *session, const GameCommand *command,
    const GameEvent *event, uint32_t now);
int rich4_tool_followup_mark_robot_trigger(
    GameSession *session, uint32_t now);
int rich4_tool_followup_mark_turn_pending(GameSession *session);
int rich4_tool_followup_update_placement(
    GameSession *session, const GameViewportModel *viewport, uint32_t now,
    Rich4AudioEventQueue *audio_events);
int rich4_tool_followup_update_robot(
    GameSession *session, const MapData *map,
    int frame_ms, uint32_t now, GameEventQueue *events,
    Rich4AudioEventQueue *audio_events, Rich4ToolRobotUpdate *update);
int rich4_tool_followup_finish_trigger_turn(
    GameSession *session, uint32_t now);
int rich4_tool_followup_finish_trigger_turn_and_advance(
    GameSession *session, const MapData *map, uint32_t now,
    Rich4AppTurnRotationResult *out_result);
int rich4_tool_followup_next_player(const GameSession *session);

#endif
