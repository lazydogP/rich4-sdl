#ifndef RICH4_APP_COMMAND_EVENT_FLOW_H
#define RICH4_APP_COMMAND_EVENT_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/game_command.h"
#include "core/game_event.h"

/* Command-event application is the app-owned half of the command bridge.
 * Resource-backed presentations remain explicit platform follow-ups. */
typedef enum Rich4CommandEventResult {
    RICH4_COMMAND_EVENT_PLATFORM_FOLLOWUP = 0,
    RICH4_COMMAND_EVENT_CONSUMED = 1,
    RICH4_COMMAND_EVENT_REJECTED = -1
} Rich4CommandEventResult;

typedef enum Rich4CommandEventEffectKind {
    RICH4_COMMAND_EVENT_EFFECT_NONE = 0,
    RICH4_COMMAND_EVENT_EFFECT_STATUS_SYNC,
    RICH4_COMMAND_EVENT_EFFECT_PROPERTY_LEVEL,
    RICH4_COMMAND_EVENT_EFFECT_TOOL_ANIMATION
} Rich4CommandEventEffectKind;

typedef struct Rich4CommandEventEffect {
    Rich4CommandEventEffectKind kind;
    int mark_robot_trigger;
} Rich4CommandEventEffect;

/* Classifies only the follow-up that still needs platform resources. */
Rich4CommandEventEffect rich4_app_command_event_effect(
    GameSession *session, const GameCommand *command,
    const GameEvent *event);

Rich4CommandEventResult rich4_app_apply_command_event(
    GameSession *session, const GameCommand *command,
    const GameEvent *event, uint32_t now);

#endif
