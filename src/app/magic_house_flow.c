#include "app/magic_house_flow.h"

#include "app/auction_flow.h"
#include "app/bankruptcy_flow.h"
#include "app/dynamic_object_flow.h"
#include "app/tool_animation_flow.h"
#include "core/god.h"
#include "app/turn_status_flow.h"
#include <stdint.h>
#include <string.h>

#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/game_event_payload.h"
#include "core/character_speech_rules.h"
#include "core/route_event_executor.h"
#include "core/rng.h"
#include "data/character_speech.h"

int rich4_magic_house_facility_at(int x, int y)
{
    if (x < 50 || x >= 390 || y < 286 || y >= 354)
        return -1;
    return (x - 50) / 68;
}

void rich4_magic_house_set_hover(GameSession *session, int hover)
{
    if (session != NULL) session->presentation.magic_house.hover = hover;
}

void rich4_magic_house_set_facility_choice_hover(
    GameSession *session, int hover)
{
    if (session != NULL)
        session->presentation.magic_house.facility_choice_hover = hover;
}

void rich4_magic_house_request_return_to_selection(GameSession *session)
{
    if (session != NULL)
        session->presentation.magic_house.return_to_selection = 1;
}

static void rich4_magic_emit(
    Rich4AudioEventQueue *events, Rich4AudioEventType type, int value)
{
    if (events != NULL)
        rich4_audio_event_emit(events, type, 1, value);
}

const char *rich4_magic_house_condition_big5(int condition)
{
    static const char *const text[12] = {
        "\x23\x30\x30\x34\x36\xb0\x5d\xb2\xa3\xb3\xcc\xa6\x68\xaa\xba\xa4\x48",
        "\x23\x30\x30\x34\x37\xa4\x67\xa6\x61\xb3\xcc\xa6\x68\xaa\xba\xa4\x48",
        "\x23\x30\x30\x34\x38\xa9\xd0\xab\xce\xb3\xcc\xa6\x68\xaa\xba\xa4\x48",
        "\x23\x30\x30\x34\x39\xb2\x7b\xaa\xf7\xb3\xcc\xa6\x68\xaa\xba\xa4\x48",
        "\x23\x30\x30\x35\x30\xa6\x73\xb4\xda\xb3\xcc\xa6\x68\xaa\xba\xa4\x48",
        "\x23\x30\x30\x35\x31\xc2\x49\xa8\xe9\xb3\xcc\xa6\x68\xaa\xba\xa4\x48",
        "\x23\x30\x30\x35\x32\xa8\xab\xb8\xf4\xaa\xba\xa4\x48",
        "\x23\x30\x30\x35\x33\xc3\x4d\xbe\xf7\xa8\xae\xaa\xba\xa4\x48",
        "\x23\x30\x30\x35\x34\xb6\x7d\xa8\x54\xa8\xae\xaa\xba\xa4\x48",
        "\x23\x30\x30\x35\x35\xaf\xab\xa9\xfa\xaa\xfe\xa8\xad\xaa\xba\xa4\x48",
        "\x23\x30\x30\x35\x36\xa9\xd2\xa6\xb3\xa8\x6b\xa5\xcd",
        "\x23\x30\x30\x35\x37\xa9\xd2\xa6\xb3\xa4\x6b\xa5\xcd"
    };
    return condition >= 0 && condition < 12 ? text[condition] : NULL;
}

const char *rich4_magic_house_action_big5(int action)
{
    static const char *const text[12] = {
        "\xc5\xdc\xbd\xe6\xa9\xd2\xa6\xb3\xa5\x64\xa4\xf9",
        "\xa9\xe2\xa8\xfa\xa9\x52\xb9\x42\xa4\x54\xb1\x69",
        "\xa5\xdf\xa8\xe8\xa7\xa4\xa8\x63\xa4\x54\xa4\xd1",
        "\xad\xec\xa6\x61\xb0\xb1\xaf\x64\xa4\x40\xa6\x5e\xa6\x58",
        "\xa6\x73\xa4\x4a\xa9\xd2\xa6\xb3\xb2\x7b\xaa\xf7",
        "\xb4\x4e\xa6\x61\xa5\x5b\xbb\x5c\xa9\xd0\xab\xce",
        "\xb1\x6f\xa4\x40\xb1\x69\xa5\x64\xa4\xf9",
        "\xa6\x56\xab\xe1\xc2\xe0",
        "\xc5\xdc\xbd\xe6\xa9\xd2\xa6\xb3\xb9\x44\xa8\xe3",
        "\xb4\x4e\xa6\x61\xa9\xee\xb0\xa3\xa9\xd0\xab\xce",
        "\xa6\xed\xb0\x7c\xc0\xcb\xac\x64\xa4\x54\xa4\xd1",
        "\xa9\xe7\xbd\xe6\xb7\xed\xae\xe6\xa4\x67\xa6\x61"
    };
    return action >= 0 && action < 12 ? text[action] : NULL;
}

const char *rich4_magic_house_phase_big5(int phase, int condition)
{
    if (condition == RICH4_MAGIC_HOUSE_SURRENDER) {
        /* 0x4756A8 / 0x4756AC: Speaking[42]/[43]. */
        if (phase == RICH4_MAGIC_HOUSE_INTRO_1)
            return "#0042\xa7\x41\xb7\x51\xa5\x6c\xb3\xea\xa6\xba\xaf\xab\xa1\x41\x0a\xac\xb0\xa7\x41\xb4\x5f\xa4\xb3\xb6\xdc\xa1\x48";
        if (phase == RICH4_MAGIC_HOUSE_PROMPT)
            return "#0043\xa7\x41\xad\x6e\xa6\xba\xaf\xab\xaa\xfe\xa8\xad\xa6\x62\xbd\xd6\xa8\xad\xa4\x57\xa1\x48";
    }
    switch (phase) {
    case RICH4_MAGIC_HOUSE_INTRO_1:
        return "\x23\x30\x30\x33\x37\xb6\x69\xa8\xd3\xc5\x5d\xaa\x6b\xab\xce"
               "\xa1\x41\xb4\x4e\xb1\x6f\x0a\xa7\xb9\xa5\xfe\xb7\xd3\xa7\xda"
               "\xaa\xba\xab\xfc\xa5\xdc\xa1\x49";
    case RICH4_MAGIC_HOUSE_INTRO_2:
        return "\x23\x30\x30\x33\x38\xa7\xda\xbf\xef\xa5\x58\xb2\xc5\xa6\x58"
               "\xb1\xf8\xa5\xf3\xaa\xba\xa4\x48\xa1\x43";
    case RICH4_MAGIC_HOUSE_INTRO_3:
        return "\x23\x30\x30\x33\x39\xa7\x41\xa8\xd3\xa8\x4d\xa9\x77\xa5\x4c"
               "\xad\xcc\xaa\xba\xa9\x52\xb9\x42\xa1\xe3";
    case RICH4_MAGIC_HOUSE_CONDITION:
        return rich4_magic_house_condition_big5(condition);
    case RICH4_MAGIC_HOUSE_PROMPT:
        return "\x23\x30\x30\x34\x30\xbc\x4b\xa1\xe3\xbd\xfc\xa8\xec\xa7\x41"
               "\xa4\x46\xa1\x49";
    case RICH4_MAGIC_HOUSE_CONFIRM:
        return "\x23\x30\x30\x34\x31\xa4\xd1\xc6\x46\xc6\x46\xa6\x61\xc6\x46"
               "\xc6\x46\xa1\xe3";
    default:
        return NULL;
    }
}

void rich4_magic_house_start_phase(
    GameSession *session, int phase, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    struct MagicHouseState *magic;
    Rich4PlayerSayText decoded;
    const char *text;
    if (session == NULL || !session->presentation.magic_house.active) return;
    magic = &session->presentation.magic_house;
    magic->phase = phase;
    magic->phase_started = now;
    magic->phase_until = now;
    magic->speech_started = 0;
    text = rich4_magic_house_phase_big5(phase, magic->condition);
    rich4_player_say_decode(text, -1, &decoded);
    magic->speech_started = text != NULL;
    if (decoded.speaking_resource >= 0)
        rich4_magic_emit(audio_events, RICH4_AUDIO_EVENT_SPEECH_START,
                         decoded.speaking_resource);
}

void rich4_magic_house_begin(
    GameSession *session, const GameEvent *event, uint32_t now,
    int animation_enabled, Rich4AudioEventQueue *audio_events)
{
    struct MagicHouseState *magic;
    int player;
    if (session == NULL || event == NULL ||
        rich4_game_event_magic_house_condition(event) < 0 ||
        rich4_game_event_magic_house_condition(event) >= 12 ||
        rich4_game_event_magic_house_target_mask(event) == 0)
        return;
    magic = &session->presentation.magic_house;
    memset(magic, 0, sizeof(*magic));
    magic->active = 1;
    magic->player = event->player;
    magic->condition = rich4_game_event_magic_house_condition(event);
    magic->targets = rich4_game_event_magic_house_target_mask(event);
    magic->hover = -1;
    magic->selected_action = -1;
    magic->animation_enabled = animation_enabled != 0;
    magic->animation_frame = -1;
    magic->animation_tick = now + 100;
    magic->target_player = -1;
    magic->facility_choice_player = -1;
    magic->facility_choice_hover = -1;
    for (player = 0; player < GAME_MAX_PLAYERS; player++)
        magic->facility_choice[player] = -1;
    if (rich4_game_event_magic_house_auto_action(event) >= 0) {
        /* 0x43390B..0x4339CE: timed map notice, then the common effect. */
        magic->selected_action = rich4_game_event_magic_house_auto_action(event);
        magic->phase = RICH4_MAGIC_HOUSE_AI_NOTICE;
        magic->phase_until = now + 1500;
        magic->modal_music_finished = 1;
        session->presentation.route_interaction.active = 0;
        return;
    }
    if (magic->animation_enabled)
        rich4_magic_house_start_phase(
            session, RICH4_MAGIC_HOUSE_INTRO_1, now, audio_events);
    else {
        magic->phase = RICH4_MAGIC_HOUSE_REVEAL;
        magic->phase_started = now;
        magic->phase_until = now + 100;
    }
    session->presentation.route_interaction.active = 0;
}

void rich4_magic_house_begin_surrender(GameSession *session, int player,
    unsigned int targets, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    struct MagicHouseState *m = &session->presentation.magic_house;
    memset(m, 0, sizeof(*m));
    m->active = 1;
    m->player = player;
    m->condition = RICH4_MAGIC_HOUSE_SURRENDER;
    m->targets = targets;
    m->hover = m->target_player = m->selected_action = -1;
    m->animation_frame = -1;
    m->animation_tick = now + 100;
    rich4_magic_house_start_phase(session, RICH4_MAGIC_HOUSE_INTRO_1, now, audio_events);
}

int rich4_magic_house_surrender_target_x(unsigned int targets, int player)
{
    static const int positions[2][3] = {{238,330,0},{211,284,357}}; /* 0x4757D8 */
    int i, count = 0, index = 0;
    if (player < 0 || player >= GAME_MAX_PLAYERS || !(targets & (1u << player))) return -1;
    for (i = 0; i < GAME_MAX_PLAYERS; ++i) if (targets & (1u << i)) {
        ++count;
        if (i < player) ++index;
    }
    return count >= 2 && count <= 3 ? positions[count-2][index] : -1;
}

int rich4_magic_house_surrender_target_at(const GameSession *session, int x, int y)
{
    const struct MagicHouseState *m = &session->presentation.magic_house;
    int i;
    if (!m->active || m->condition != RICH4_MAGIC_HOUSE_SURRENDER ||
        m->phase != RICH4_MAGIC_HOUSE_SELECT || y < 348 || y > 420) return -1;
    for (i = 0; i < session->state.player_count; ++i) {
        int left = rich4_magic_house_surrender_target_x(m->targets, i);
        if (left >= 0 && x >= left && x <= left+72) return i;
    }
    return -1;
}

void rich4_magic_house_surrender_choose(GameSession *session, int player,
    uint32_t now, Rich4AudioEventQueue *audio_events)
{
    struct MagicHouseState *m = &session->presentation.magic_house;
    if (!m->active || m->condition != RICH4_MAGIC_HOUSE_SURRENDER ||
        m->phase != RICH4_MAGIC_HOUSE_SELECT ||
        rich4_magic_house_surrender_target_x(m->targets, player) < 0) return;
    m->target_player = player;
    rich4_magic_emit(audio_events, RICH4_AUDIO_EVENT_OK, 0);
    rich4_magic_house_start_phase(session, RICH4_MAGIC_HOUSE_CONFIRM, now, audio_events);
}

void rich4_magic_house_surrender_cancel(GameSession *session,
    Rich4AudioEventQueue *audio_events)
{
    struct MagicHouseState *m = &session->presentation.magic_house;
    if (!m->active || m->condition != RICH4_MAGIC_HOUSE_SURRENDER ||
        m->phase == RICH4_MAGIC_HOUSE_FINALE) return;
    rich4_magic_emit(audio_events, RICH4_AUDIO_EVENT_CANCEL, 0);
    rich4_magic_emit(audio_events, RICH4_AUDIO_EVENT_SPEECH_STOP, 0);
    m->active = 0;
    m->modal_music_finished = 1;
}

static int rich4_magic_house_empty_facility(
    const GameState *state, int player, int *out_facility)
{
    int node;
    int raw;
    int facility;
    if (state == NULL || out_facility == NULL ||
        player < 0 || player >= state->player_count ||
        state->players[player].delayed_status_block != 0)
        return 0;
    node = state->players[player].node;
    if (node < 0 || node >= state->tool_route_node_count) return 0;
    raw = state->route_object_raw[node];
    if (raw < 0xfa1 || raw >= 0x1770) return 0;
    facility = raw - 0xfa1;
    if (facility < 0 || facility >= state->facility_count ||
        state->facility_level[facility] != 0) return 0;
    *out_facility = facility;
    return 1;
}

static int rich4_magic_house_next_facility_choice(GameSession *session)
{
    struct MagicHouseState *magic;
    int player;
    int facility;
    if (session == NULL || !session->presentation.magic_house.active) return 0;
    magic = &session->presentation.magic_house;
    for (player = 0; player < session->state.player_count; player++) {
        if ((magic->targets & (1u << player)) == 0 ||
            (session->state.players[player].play_state & 0xff) != 1 ||
            magic->facility_choice[player] >= 0 ||
            !rich4_magic_house_empty_facility(
                &session->state, player, &facility)) continue;
        {
            int prior;
            for (prior = 0; prior < session->state.player_count; prior++) {
                int prior_facility;
                if (magic->facility_choice[prior] >= 0 &&
                    rich4_magic_house_empty_facility(
                        &session->state, prior, &prior_facility) &&
                    prior_facility == facility) break;
            }
            if (prior < session->state.player_count) continue;
        }
        magic->facility_choice_player = player;
        magic->facility_choice_hover = -1;
        magic->phase = RICH4_MAGIC_HOUSE_FACILITY_CHOICE;
        return 1;
    }
    magic->facility_choice_player = -1;
    magic->facility_choice_hover = -1;
    return 0;
}

int rich4_magic_house_choose_facility(
    GameSession *session, int choice, uint32_t now,
    Rich4AudioEventQueue *audio_events, GameEventQueue *out_events)
{
    struct MagicHouseState *magic;
    int player;

    if (session == NULL || !session->presentation.magic_house.active ||
        session->presentation.magic_house.phase !=
            RICH4_MAGIC_HOUSE_FACILITY_CHOICE || choice < 0 || choice >= 5)
        return 0;
    magic = &session->presentation.magic_house;
    player = magic->facility_choice_player;
    if (player < 0 || player >= session->state.player_count)
        return 0;
    magic->facility_choice[player] = choice;
    rich4_magic_emit(audio_events, RICH4_AUDIO_EVENT_OK, 0);
    if (!rich4_magic_house_next_facility_choice(session))
        rich4_magic_house_finish(
            session, magic->selected_action, now, audio_events, out_events);
    return 1;
}

static int rich4_magic_house_action_shows_target(
    const GameSession *session, int player, int action)
{
    const GameState *state;
    int node;
    int raw;
    if (session == NULL || player < 0 ||
        player >= session->state.player_count) return 0;
    state = &session->state;
    if ((session->presentation.magic_house.targets & (1u << player)) == 0 ||
        (state->players[player].play_state & 0xff) != 1) return 0;
    if (action != 5 && action != 7 && action != 9 && action != 11) return 1;
    if (state->players[player].delayed_status_block != 0) return 0;
    if (action == 7) return 1;
    node = state->players[player].node;
    if (node < 0 || node >= state->tool_route_node_count) return 0;
    raw = state->route_object_raw[node];
    return raw > 0x7d0 && raw < 0x1770;
}

static int rich4_magic_house_begin_next_target(
    GameSession *session, uint32_t now)
{
    struct MagicHouseState *magic;
    int player;
    if (session == NULL || !session->presentation.magic_house.active) return 0;
    magic = &session->presentation.magic_house;
    for (player = 0; player < session->state.player_count; player++) {
        if ((magic->pending_targets & (1u << player)) == 0) continue;
        magic->pending_targets &= ~(1u << player);
        if (!rich4_magic_house_action_shows_target(
                session, player, magic->selected_action)) continue;
        magic->phase = RICH4_MAGIC_HOUSE_TARGET;
        magic->target_player = player;
        magic->target_until = now + 1500;
        return 1;
    }
    magic->target_player = -1;
    return 0;
}

static void rich4_magic_house_set_selection(GameSession *session, uint32_t now)
{
    struct MagicHouseState *magic;
    if (session == NULL || !session->presentation.magic_house.active) return;
    magic = &session->presentation.magic_house;
    magic->phase = RICH4_MAGIC_HOUSE_SELECT;
    magic->hover = -1;
    magic->phase_until = now;
    magic->phase_started = now;
    magic->speech_started = 0;
    magic->animation_frame = -1;
    magic->animation_countdown = 0;
}

void rich4_magic_house_skip_intro(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    struct MagicHouseState *magic;
    if (session == NULL || !session->presentation.magic_house.active ||
        session->presentation.magic_house.phase > RICH4_MAGIC_HOUSE_INTRO_3)
        return;
    rich4_magic_emit(audio_events, RICH4_AUDIO_EVENT_SPEECH_STOP, 0);
    magic = &session->presentation.magic_house;
    magic->phase = RICH4_MAGIC_HOUSE_REVEAL;
    magic->hover = -1;
    magic->phase_started = now;
    magic->phase_until = now + 1000;
    magic->speech_started = 0;
    magic->animation_frame = -1;
    magic->animation_countdown = 0;
}

void rich4_magic_house_select_action(
    GameSession *session, int action, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    struct MagicHouseState *magic;
    if (session == NULL || !session->presentation.magic_house.active ||
        session->presentation.magic_house.phase != RICH4_MAGIC_HOUSE_SELECT ||
        action < 0 || action >= 12) return;
    magic = &session->presentation.magic_house;
    magic->selected_action = action;
    magic->hover = -1;
    rich4_magic_house_start_phase(
        session, RICH4_MAGIC_HOUSE_CONFIRM, now, audio_events);
}

void rich4_magic_house_finish(
    GameSession *session, int action, uint32_t now,
    Rich4AudioEventQueue *audio_events, GameEventQueue *out_events)
{
    GameEventQueue events;
    GameEvent event;
    RouteEventExecutionResult result;
    int changed_facility[GAME_MAX_PLAYERS];
    int changed_type[GAME_MAX_PLAYERS];
    int changed_count = 0;
    int player;
    (void)audio_events;
    if (out_events != NULL) rich4_game_event_queue_reset(out_events);
    if (session == NULL || !session->presentation.magic_house.active ||
        action < 0 || action >= 12) return;
    if (action == 5) {
        for (player = 0; player < session->state.player_count; player++) {
            int facility;
            if (session->presentation.magic_house.facility_choice[player] < 0 ||
                !rich4_magic_house_empty_facility(
                    &session->state, player, &facility)) continue;
            {
                int prior;
                for (prior = 0; prior < changed_count; prior++)
                    if (changed_facility[prior] == facility) break;
                if (prior < changed_count) continue;
            }
            changed_facility[changed_count] = facility;
            changed_type[changed_count] = session->state.facility_type[facility];
            session->state.facility_type[facility] =
                session->presentation.magic_house.facility_choice[player];
            changed_count++;
        }
    }
    rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_resume_magic_house_deferred_confinement(
        &session->flow.route_events, &session->state, action, &events);
    if (result == ROUTE_EVENT_EXECUTION_INVALID) {
        for (player = 0; player < changed_count; player++)
            session->state.facility_type[changed_facility[player]] =
                changed_type[player];
        return;
    }
    memset(&session->presentation.magic_house, 0,
           sizeof(session->presentation.magic_house));
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_TEXT_REQUESTED) {
            Rich4GameEventFields fields = rich4_game_event_text_requested_fields(&event);
            if (fields.subject == 52 || fields.subject == 53)
                (void)rich4_confinement_presentation_enqueue(session, event.player,
                    fields.value, fields.value2, now);
        }
        if (event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED)
            session->presentation.route_interaction.active = 0;
        else if (event.type == GAME_EVENT_SPECIAL_EVENT_COMMITTED &&
                 rich4_game_event_special_commit_amount(&event) == 7 &&
                 rich4_game_event_special_commit_subject(&event).kind ==
                     GAME_DOMAIN_PLAYER &&
                 rich4_game_event_special_commit_subject(&event).id >= 0 &&
                 rich4_game_event_special_commit_subject(&event).id <
                     session->state.player_count) {
            GameActorPresentationState *actor = &session->presentation.actors[
                rich4_game_event_special_commit_subject(&event).id];
            actor->direction = (actor->direction + 4) & 7;
            actor->walk_frame = 0;
            rich4_magic_emit(audio_events, RICH4_AUDIO_EVENT_MAGIC_TURN, 56);
        } else if (event.type == GAME_EVENT_AUCTION_REQUESTED) {
            if (!rich4_bankruptcy_capture_event(session, &event, now))
                (void)rich4_auction_flow_enqueue(
                    session, &event, audio_events);
        } else if (event.type == GAME_EVENT_PLAYER_ELIMINATED) {
            if (!rich4_bankruptcy_capture_event(session, &event, now) &&
                event.player >= 0 && event.player < GAME_MAX_PLAYERS) {
                session->presentation.actors[event.player].move_active = 0;
                session->presentation.actors[event.player].dirty_flags |=
                    GAME_PLAYER_DIRTY_TURN;
            }
        } else if (event.type == GAME_EVENT_GAME_ENDED) {
            if (!rich4_bankruptcy_capture_event(session, &event, now))
                rich4_app_capture_game_ended(session, &event);
        } else if (event.type == GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED &&
                   out_events != NULL)
            rich4_game_event_queue_push(out_events, &event);
    }
    (void)now;
}

int rich4_magic_house_update(
    GameSession *session, int finale_frames, int finale_delay_ms,
    uint32_t now, int speech_playing,
    Rich4AudioEventQueue *audio_events, GameEventQueue *out_events)
{
    struct MagicHouseState *magic;
    if (session == NULL || !session->presentation.magic_house.active) return 0;
    magic = &session->presentation.magic_house;
    if (magic->phase == RICH4_MAGIC_HOUSE_AI_NOTICE) {
        if ((int32_t)(now - magic->phase_until) >= 0) {
            magic->pending_targets = magic->targets;
            if (!rich4_magic_house_begin_next_target(session, now) &&
                (magic->selected_action != 5 ||
                 !rich4_magic_house_next_facility_choice(session)))
                rich4_magic_house_finish(session, magic->selected_action, now,
                                         audio_events, out_events);
        }
        return 1;
    }
    while (magic->phase != RICH4_MAGIC_HOUSE_FINALE &&
           (int32_t)(now - magic->animation_tick) >= 0) {
        magic->animation_tick += 100;
        if (!magic->speech_started || magic->phase == RICH4_MAGIC_HOUSE_CONFIRM) {
            magic->animation_frame = -1;
            magic->animation_countdown = 0;
        } else if (magic->animation_countdown > 0) {
            magic->animation_countdown--;
            if (magic->animation_countdown == 0) magic->animation_frame = 4;
        } else if ((((int)(rich4_rng_next_at(&session->state.ai_rng_state) & 0x7fffu)) >> 11) < 4) {
            magic->animation_frame = (rich4_rng_next_at(&session->state.ai_rng_state) & 1u) != 0 ? -1 : 5;
            magic->animation_countdown = (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 7u);
            if (magic->animation_countdown == 0) magic->animation_countdown = 1;
        }
    }
    if (magic->phase == RICH4_MAGIC_HOUSE_REVEAL) {
        if ((int32_t)(now - magic->phase_until) >= 0)
            rich4_magic_house_start_phase(
                session, RICH4_MAGIC_HOUSE_CONDITION, now, audio_events);
        return 1;
    }
    if (magic->phase == RICH4_MAGIC_HOUSE_FINALE) {
        int delay = finale_delay_ms > 0 ? finale_delay_ms : 59;
        if (finale_frames > 0 && rich4_animation_clock_step(
                &session->presentation.animation_clock[RICH4_ANIMATION_MAGIC],
                now, delay)) {
            magic->animation_frame++;
            magic->animation_tick = now;
        }
        if (finale_frames <= 0 || magic->animation_frame >= finale_frames) {
            magic->modal_music_finished = 1;
            if (magic->condition == RICH4_MAGIC_HOUSE_SURRENDER) {
                int slot, target = magic->target_player;
                /* 0x411B35 -> 0x40E033(15,0,7,target+1): first vacant
                 * Death slot, then the common replacement acquisition. */
                for (slot = 14; slot < 16; ++slot)
                    if (!session->presentation.dynamic_objects[slot].node) break;
                magic->active = 0;
                if (slot < 16 && target >= 0 && target < session->state.player_count)
                    rich4_app_begin_dynamic_god_detach(&session->presentation,
                        &session->state, target, now);
                if (slot < 16 && target >= 0 && target < session->state.player_count &&
                    rich4_god_attach_replace(&session->state, target, slot+1, 15,
                        out_events) == GAME_GOD_APPLIED) {
                    session->presentation.dynamic_objects[slot].type = 15;
                    rich4_app_sync_dynamic_god_attachment(&session->presentation,
                        &session->state, target, slot);
                    magic->active = 1;
                    magic->phase = RICH4_MAGIC_HOUSE_TARGET;
                }
                return 1;
            }
            magic->pending_targets = magic->targets;
            magic->target_player = -1;
            magic->phase = RICH4_MAGIC_HOUSE_TARGET;
            magic->target_until = now;
        }
        return session->presentation.magic_house.active;
    }
    if (magic->phase == RICH4_MAGIC_HOUSE_TARGET) {
        if (magic->condition == RICH4_MAGIC_HOUSE_SURRENDER) {
            if (session->presentation.god_acquisition.active) return 1;
            magic->active = 0;
            session->presentation.has_view_override = 0;
            return 1;
        }
        if ((int32_t)(now - magic->target_until) < 0) return 1;
        if (!rich4_magic_house_begin_next_target(session, now)) {
            int action = magic->selected_action;
            if (action != 5 || !rich4_magic_house_next_facility_choice(session))
                rich4_magic_house_finish(
                    session, action, now, audio_events, out_events);
        }
        return session->presentation.magic_house.active;
    }
    if (magic->phase == RICH4_MAGIC_HOUSE_FACILITY_CHOICE ||
        magic->phase == RICH4_MAGIC_HOUSE_SELECT) return 1;
    if (now - magic->phase_started < 2000u || speech_playing) return 1;
    magic->speech_started = 0;
    switch (magic->phase) {
    case RICH4_MAGIC_HOUSE_INTRO_1:
        rich4_magic_house_start_phase(
            session, magic->condition == RICH4_MAGIC_HOUSE_SURRENDER
                ? RICH4_MAGIC_HOUSE_PROMPT : RICH4_MAGIC_HOUSE_INTRO_2, now, audio_events);
        break;
    case RICH4_MAGIC_HOUSE_INTRO_2:
        rich4_magic_house_start_phase(
            session, RICH4_MAGIC_HOUSE_INTRO_3, now, audio_events);
        break;
    case RICH4_MAGIC_HOUSE_INTRO_3:
        magic->phase = RICH4_MAGIC_HOUSE_REVEAL;
        magic->phase_started = now;
        magic->phase_until = now + 1000;
        break;
    case RICH4_MAGIC_HOUSE_CONDITION:
        if (magic->return_to_selection) {
            magic->return_to_selection = 0;
            rich4_magic_house_set_selection(session, now);
        } else if (magic->animation_enabled)
            rich4_magic_house_start_phase(
                session, RICH4_MAGIC_HOUSE_PROMPT, now, audio_events);
        else
            rich4_magic_house_set_selection(session, now);
        break;
    case RICH4_MAGIC_HOUSE_PROMPT:
        rich4_magic_house_set_selection(session, now);
        break;
    case RICH4_MAGIC_HOUSE_CONFIRM:
        /* 0x432A6A: Panel[20] starts with Effect[59], once, after #0041.
         * Effect[56] belongs to the later per-target turn operation. */
        if (magic->condition != RICH4_MAGIC_HOUSE_SURRENDER)
            rich4_magic_emit(audio_events, RICH4_AUDIO_EVENT_MAGIC_FINALE, 59);
        rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_MAGIC]);
        magic->phase = RICH4_MAGIC_HOUSE_FINALE;
        magic->animation_frame = 0;
        magic->animation_tick = now;
        break;
    default:
        break;
    }
    return session->presentation.magic_house.active;
}
