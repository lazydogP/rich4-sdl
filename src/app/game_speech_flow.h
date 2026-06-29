#ifndef RICH4_APP_GAME_SPEECH_FLOW_H
#define RICH4_APP_GAME_SPEECH_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "data/character_speech.h"

/* App-side speech state machine. The platform supplies only playback state
 * and consumes the narrow audio request below. */
typedef struct Rich4SpeechAudioRequest {
    int start_resource;
    int stop;
} Rich4SpeechAudioRequest;

/* 0x44EF68: this bit on the player argument preserves the current view. */
enum { GAME_SPEECH_KEEP_VIEW = 0x8000 };

/* 0x41D476 compares world coordinates with the current actor, including
 * special actors, before setting or clearing the observation marker. */
void game_focus_view_at(GameSession *session, int world_x, int world_y);

int game_start_speech_entry_with_status(
    GameSession *session, int player,
    const Rich4CharacterSpeech *speech, int speech_type, int after_phase,
    uint32_t now, const GamePlayerState *status_player);
int game_start_speech_entry(GameSession *session, int player,
                            const Rich4CharacterSpeech *speech,
                            int speech_type, int after_phase,
                            uint32_t now);
int game_start_property_speech(GameSession *session, int player,
                               Rich4PropertySpeechKind kind,
                               int speech_type, int after_phase,
                               uint32_t now);
int game_start_tool_speech(GameSession *session, int player, int tool,
                           uint32_t now);
int game_start_monster_victim_speech(GameSession *session, int player,
                                     uint32_t now);

/* Prepares platform audio work without querying or mutating platform state. */
int rich4_speech_prepare_audio(GameSession *session,
                               Rich4SpeechAudioRequest *request);

/* Completes one app-side speech tick using the playback state observed by
 * the platform after applying the prepared request. Returns non-zero while
 * the speech presentation still owns the frame. */
int rich4_speech_finish_update(GameSession *session, uint32_t now,
                               int speech_playing);
void rich4_speech_request_skip(GameSession *session);

int rich4_ai_tool_begin(GameSession *session, const GameCommand *command,
                        uint32_t now);
/* Returns one when the saved tool command may now be applied. */
int rich4_ai_tool_command_ready(GameSession *session, uint32_t now);
void rich4_ai_tool_command_completed(GameSession *session, int accepted,
                                      uint32_t now);
int rich4_ai_tool_skip_notice(GameSession *session);

#endif
