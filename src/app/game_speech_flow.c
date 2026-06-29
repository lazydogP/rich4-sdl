#include "app/game_speech_flow.h"

#include <string.h>

#include "core/character_speech_rules.h"
#include "core/game_constants.h"
#include "core/tool.h"
#include "core/turn_flow.h"

static void speech_player_position(const GameSession *session, int player,
    const GamePlayerState *status, int *x, int *y)
{
    const GameActorPresentationState *actor =
        &session->presentation.actors[player];
    int node = status->node;
    /* Static ordinary actors use the authoritative route coordinate.
     * Synced confined actors retain the internal facility anchor; moving
     * actors retain the coordinate committed by their movement pulse. */
    *x = actor->visual_x;
    *y = actor->visual_y;
    if (!actor->move_active && status->confinement_actor_type == 0 &&
        node > 0 && node < session->state.tool_route_node_count &&
        node < GAME_MAX_MAP_ROUTE_NODES) {
        *x = session->state.tool_route_x[node];
        *y = session->state.tool_route_y[node];
    }
}

void game_focus_view_at(GameSession *session, int world_x, int world_y)
{
    int current, x, y;
    if (session == NULL) return;
    current = session->state.turn_rotation_index;
    if (current < 0) current = session->state.current_player;
    if (current >= 0 && current < session->state.player_count) {
        speech_player_position(session, current,
            &session->state.players[current], &x, &y);
    } else if (current >= 4 && current < 8) {
        int node = session->state.special_player_node[current - 4];
        if (session->presentation.special_actor.active &&
            session->presentation.special_actor.slot == current - 4) {
            x = session->presentation.special_actor.visual_x;
            y = session->presentation.special_actor.visual_y;
        } else if (node > 0 && node < session->state.tool_route_node_count &&
                   node < GAME_MAX_MAP_ROUTE_NODES) {
            x = session->state.tool_route_x[node];
            y = session->state.tool_route_y[node];
        } else return;
    } else if (current == 8) {
        x = session->presentation.tool_robot_motion.world_x;
        y = session->presentation.tool_robot_motion.world_y;
    } else return;
    if (x == world_x && y == world_y) {
        session->presentation.has_view_override = 0;
    } else {
        session->presentation.has_view_override = 1;
        session->presentation.view_world_x = world_x;
        session->presentation.view_world_y = world_y;
    }
}

int game_start_speech_entry_with_status(
    GameSession *session, int player,
    const Rich4CharacterSpeech *speech, int speech_type, int after_phase,
    uint32_t now, const GamePlayerState *status_player)
{
    GameSpeechPresentationState *speech_state;
    int keep_view = player >= 0 && (player & GAME_SPEECH_KEEP_VIEW) != 0;
    if (keep_view) player &= ~GAME_SPEECH_KEEP_VIEW;
    if (session == NULL || player < 0 ||
        player >= session->state.player_count || speech == NULL ||
        status_player == NULL) return 0;
    speech_state = &session->presentation.speech;
    /* Preserve the original ordering: the table string is remembered before
     * disappearing/sleeping status suppresses the visible speech. */
    if (speech_state->last_text_rehydrated) {
        speech_state->last_text_rehydrated = 0;
        if (speech->big5 != NULL &&
            rich4_character_speech_snapshot_is_valid(
                &speech_state->last_text_snapshot) &&
            strcmp(speech_state->last_text_snapshot.big5,
                   speech->big5) == 0) {
            speech_state->last_text = speech->big5;
            (void)rich4_character_speech_snapshot_capture_text(
                &speech_state->last_text_snapshot, speech->big5);
            return 0;
        }
    }
    if (!rich4_player_say_try_begin(
            &speech_state->last_text, speech->big5, status_player)) {
        speech_state->last_text_rehydrated = 0;
        (void)rich4_character_speech_snapshot_capture_text(
            &speech_state->last_text_snapshot, speech->big5);
        return 0;
    }
    /* 0x44EFBD runs only after duplicate-text and status suppression. */
    if (!keep_view) {
        int x, y;
        speech_player_position(session, player, status_player, &x, &y);
        game_focus_view_at(session, x, y);
    }
    speech_state->last_text_rehydrated = 0;
    (void)rich4_character_speech_snapshot_capture_text(
        &speech_state->last_text_snapshot, speech->big5);
    (void)rich4_character_speech_snapshot_capture(
        &speech_state->current_snapshot, speech);
    speech_state->current = speech;
    speech_state->active = 1;
    speech_state->player = player;
    speech_state->kind = -1;
    speech_state->type = speech_type;
    speech_state->after_phase = after_phase;
    speech_state->tick = now;
    speech_state->voice_started = 0;
    speech_state->skip_requested = 0;
    return 1;
}

int game_start_speech_entry(GameSession *session, int player,
                            const Rich4CharacterSpeech *speech,
                            int speech_type, int after_phase,
                            uint32_t now)
{
    int index = player >= 0 ? player & ~GAME_SPEECH_KEEP_VIEW : player;
    if (session == NULL || index < 0 ||
        index >= session->state.player_count) return 0;
    return game_start_speech_entry_with_status(
        session, player, speech, speech_type, after_phase, now,
        &session->state.players[index]);
}

int game_start_property_speech(GameSession *session, int player,
                               Rich4PropertySpeechKind kind,
                               int speech_type, int after_phase,
                               uint32_t now)
{
    const Rich4CharacterSpeech *speech;
    int character;
    if (session == NULL || player < 0 ||
        player >= session->state.player_count ||
        kind < 0 || kind >= RICH4_PROPERTY_SPEECH_COUNT) return 0;
    character = session->state.players[player].character;
    speech = rich4_character_property_speech(character, kind);
    if (!game_start_speech_entry(session, player, speech, speech_type,
                                 after_phase, now)) return 0;
    session->presentation.speech.kind = (int)kind;
    return 1;
}

int game_start_tool_speech(GameSession *session, int player, int tool,
                           uint32_t now)
{
    const Rich4CharacterSpeech *speech;
    int character;
    if (session == NULL || player < 0 ||
        player >= session->state.player_count || tool < 0 ||
        tool >= GAME_TOOL_COUNT) return 0;
    character = session->state.players[player].character;
    speech = rich4_character_tool_use_speech(character, tool);
    return game_start_speech_entry(
        session, player, speech, 0, session->presentation.property.phase, now);
}

int game_start_monster_victim_speech(GameSession *session, int player,
                                     uint32_t now)
{
    const Rich4CharacterSpeech *speech;
    if (session == NULL || player < 0 ||
        player >= session->state.player_count) return 0;
    speech = rich4_character_monster_victim_speech(
        session->state.players[player].character);
    return game_start_speech_entry(
        session, player, speech, 1, session->presentation.property.phase, now);
}

int rich4_speech_prepare_audio(GameSession *session,
                               Rich4SpeechAudioRequest *request)
{
    GameSpeechPresentationState *speech_state;
    Rich4PlayerSayText decoded;
    const Rich4CharacterSpeech *speech;
    if (request != NULL) {
        request->start_resource = -1;
        request->stop = 0;
    }
    if (session == NULL || !session->presentation.speech.active) return 0;
    speech_state = &session->presentation.speech;
    speech = speech_state->current;
    if (speech_state->skip_requested) {
        if (request != NULL) request->stop = 1;
    } else if (!speech_state->voice_started) {
        rich4_player_say_decode(
            speech != NULL ? speech->big5 : NULL,
            speech != NULL ? speech->speaking_resource : -1, &decoded);
        if (request != NULL && decoded.speaking_resource >= 0)
            request->start_resource = decoded.speaking_resource;
        speech_state->voice_started = 1;
    }
    return 1;
}

int rich4_speech_finish_update(GameSession *session, uint32_t now,
                               int speech_playing)
{
    GameSpeechPresentationState *speech_state;
    if (session == NULL || !session->presentation.speech.active) return 0;
    speech_state = &session->presentation.speech;
    if (rich4_player_say_should_wait(
            now - speech_state->tick, speech_playing,
            speech_state->skip_requested)) return 1;
    speech_state->active = 0;
    speech_state->voice_started = 0;
    speech_state->skip_requested = 0;
    speech_state->current = NULL;
    rich4_character_speech_snapshot_clear(
        &speech_state->current_snapshot);
    session->presentation.property.phase = speech_state->after_phase;
    if (session->presentation.property.purchase_delay_pending &&
        session->presentation.property.phase == GAME_PROPERTY_PRESENT_NONE) {
        session->presentation.property.purchase_delay_pending = 0;
        rich4_turn_flow_start_property_end_delay(&session->flow, now);
    }
    return 0;
}

void rich4_speech_request_skip(GameSession *session)
{
    if (session != NULL && session->presentation.speech.active)
        session->presentation.speech.skip_requested = 1;
}

/* Handler evidence: 0x446AFB consumes before speaking; 0x446E4A,
 * 0x446F05 and 0x4479D2 change traffic before speaking. All other
 * handlers speak before their operation, except AI remote dice (0x44725A). */
static int tool_speaks_after_command(int tool)
{
    return tool == 0 || tool == 4 || tool == 5 || tool == 11;
}

int rich4_ai_tool_begin(GameSession *session, const GameCommand *command,
                        uint32_t now)
{
    GameAiToolPresentationState *tool;
    if (session == NULL || command == NULL ||
        command->source != GAME_COMMAND_SOURCE_AI ||
        command->type != GAME_COMMAND_USE_TOOL ||
        command->subject.kind != GAME_DOMAIN_TOOL ||
        command->subject.id < 0 || command->subject.id >= GAME_TOOL_COUNT ||
        command->player < 0 || command->player >= session->state.player_count ||
        session->presentation.ai_tool.phase != GAME_AI_TOOL_NONE)
        return 0;
    tool = &session->presentation.ai_tool;
    tool->phase = GAME_AI_TOOL_NOTICE;
    tool->command = *command;
    tool->started = now;
    tool->skip_requested = 0;
    return 1;
}

int rich4_ai_tool_command_ready(GameSession *session, uint32_t now)
{
    GameAiToolPresentationState *tool;
    if (session == NULL) return 0;
    tool = &session->presentation.ai_tool;
    if (tool->phase == GAME_AI_TOOL_NOTICE) {
        /* 0x448070 -> 0x440CAC(text,1500) -> 0x4528B9. */
        if (!tool->skip_requested && now - tool->started < 1500u) return 0;
        tool->phase = GAME_AI_TOOL_SPEECH;
        if (!tool_speaks_after_command(tool->command.subject.id) &&
            tool->command.subject.id != 7)
            game_start_tool_speech(session, tool->command.player,
                                   tool->command.subject.id, now);
    }
    if (tool->phase == GAME_AI_TOOL_SPEECH &&
        !session->presentation.speech.active)
        tool->phase = GAME_AI_TOOL_COMMAND;
    return tool->phase == GAME_AI_TOOL_COMMAND;
}

void rich4_ai_tool_command_completed(GameSession *session, int accepted,
                                      uint32_t now)
{
    GameAiToolPresentationState *tool;
    if (session == NULL) return;
    tool = &session->presentation.ai_tool;
    if (tool->phase != GAME_AI_TOOL_COMMAND) return;
    tool->phase = GAME_AI_TOOL_EFFECTS;
    if (accepted && tool_speaks_after_command(tool->command.subject.id))
        game_start_tool_speech(session, tool->command.player,
                               tool->command.subject.id, now);
}

int rich4_ai_tool_skip_notice(GameSession *session)
{
    if (session == NULL ||
        session->presentation.ai_tool.phase != GAME_AI_TOOL_NOTICE) return 0;
    session->presentation.ai_tool.skip_requested = 1;
    return 1;
}
