#ifndef RICH4_APP_TOOL_ANIMATION_FLOW_H
#define RICH4_APP_TOOL_ANIMATION_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "core/game_event.h"

typedef struct Rich4ToolAnimationCompletion {
    int sync_player;
    GameEventQueue events;
} Rich4ToolAnimationCompletion;

/* App owns the animation queue and deferred speech; platform supplies only
 * frame counts/delays, target coordinates, and consumes the emitted audio events. */
int rich4_tool_animation_enqueue(
    GameSession *session, const GameEvent *event, int frame_count, int frame_delay_ms,
    int world_x, int world_y, int has_world_position, uint32_t now);
int rich4_tool_animation_defer_speech(
    GameSession *session, int player,
    const Rich4CharacterSpeech *speech, int speech_type);
int rich4_tool_animation_defer_sync(GameSession *session, int player);
void rich4_tool_animation_skip_optional_confinement(GameSession *session);
void rich4_tool_animation_actor_synced(GameSession *session, int player);
int rich4_confinement_presentation_enqueue(
    GameSession *session, int player, int kind, int days, uint32_t now);
int rich4_tool_animation_handle_voice_event(
    GameSession *session, const GameEvent *event, uint32_t now);
int rich4_tool_animation_handle_text_event(
    GameSession *session, const GameEvent *event, uint32_t now);
int rich4_tool_animation_update(
    GameSession *session, int frame_count, int frame_delay_ms, uint32_t now,
    Rich4AudioEventQueue *audio_events,
    Rich4ToolAnimationCompletion *completion);

#endif
