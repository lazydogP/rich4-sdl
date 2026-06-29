#include "app/confinement_flow.h"

#include <string.h>

#include "core/character_speech_rules.h"
#include "core/confinement.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/route_event_executor.h"
#include "data/character_speech.h"
#include "app/route_geometry_flow.h"

int rich4_confinement_anchor_xy(const MapData *map, int kind,
                                int *out_x, int *out_y)
{
    int special_index = kind == GAME_CONFINEMENT_HOSPITAL ? 1 :
                        kind == GAME_CONFINEMENT_PRISON ? 2 : -1;
    if (map == NULL || map->specials == NULL || special_index < 0 ||
        (size_t)special_index >= map->special_count)
        return 0;
    if (out_x != NULL) *out_x = map->specials[special_index].x;
    if (out_y != NULL) *out_y = map->specials[special_index].y;
    return 1;
}

int rich4_confinement_sync_player_actor_to_node(
    GameSession *session, const MapData *map, int player_index)
{
    GamePlayerState *player;
    GameActorPresentationState *actor;
    int world_x;
    int world_y;

    if (session == NULL || player_index < 0 ||
        player_index >= session->state.player_count)
        return 0;
    player = &session->state.players[player_index];
    if (map != NULL && (player->node <= 0 ||
                        (size_t)player->node >= map->route_count))
        return 0;
    if (!rich4_confinement_anchor_xy(
            map, player->confinement_actor_type, &world_x, &world_y))
        rich4_route_node_xy_for_map(map, player->node, &world_x, &world_y);
    actor = &session->presentation.actors[player_index];
    actor->visual_x = world_x;
    actor->visual_y = world_y;
    actor->move_active = 0;
    actor->move_from_node = player->node;
    actor->move_to_node = player->next_node;
    actor->move_from_x = world_x;
    actor->move_from_y = world_y;
    actor->move_to_x = world_x;
    actor->move_to_y = world_y;
    actor->move_progress = 0;
    actor->move_distance = 0;
    actor->direction = rich4_route_direction8_for_nodes(
        map, player->node, player->next_node);
    return 1;
}

static void rich4_confinement_emit(
    Rich4AudioEventQueue *events, Rich4AudioEventType type, int value)
{
    if (events != NULL)
        rich4_audio_event_emit(events, type, 1, value);
}

GameDomainRef rich4_confinement_visit_target(int slot)
{
    return slot < GAME_MAX_PLAYERS
        ? rich4_game_domain_ref(GAME_DOMAIN_PLAYER, slot)
        : rich4_game_domain_ref(
              GAME_DOMAIN_SPECIAL_PLAYER, slot - GAME_MAX_PLAYERS);
}

int rich4_confinement_visit_slot_at(
    const GameSession *session, int x, int y)
{
    static const int prison_xy[8][2] = {
        {33,24},{185,24},{336,24},{487,24},
        {33,183},{185,183},{336,183},{487,183}
    };
    static const int hospital_xy[8][2] = {
        {297,1},{297,121},{297,241},{297,361},
        {481,1},{481,121},{481,241},{481,361}
    };
    const int (*xy)[2];
    int width;
    int height;
    int slot;
    if (session == NULL || !session->presentation.confinement_visit.active)
        return -1;
    xy = session->presentation.confinement_visit.kind == GAME_CONFINEMENT_PRISON
        ? prison_xy : hospital_xy;
    width = session->presentation.confinement_visit.kind == GAME_CONFINEMENT_PRISON
        ? 121 : 147;
    height = session->presentation.confinement_visit.kind == GAME_CONFINEMENT_PRISON
        ? 137 : 102;
    for (slot = 0; slot < 8; slot++) {
        if (x >= xy[slot][0] && x <= xy[slot][0] + width &&
            y >= xy[slot][1] && y <= xy[slot][1] + height &&
            rich4_confinement_visit_candidate(
                &session->state, rich4_confinement_visit_target(slot),
                session->presentation.confinement_visit.kind))
            return slot;
    }
    return -1;
}

void rich4_confinement_set_hover(GameSession *session, int hover)
{
    if (session != NULL)
        session->presentation.confinement_visit.hover = hover;
}

void rich4_confinement_set_confirm_hover(GameSession *session, int hover)
{
    if (session != NULL)
        session->presentation.confinement_visit.confirm_hover = hover;
}

void rich4_confinement_visit_start_speech(
    GameSession *session, const Rich4CharacterSpeech *speech,
    int special_actor, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    Rich4PlayerSayText decoded;
    if (session == NULL) return;
    rich4_player_say_decode(
        speech != NULL ? speech->big5 : NULL,
        speech != NULL ? speech->speaking_resource : -1, &decoded);
    session->presentation.confinement_visit.speech = speech;
    (void)rich4_character_speech_snapshot_capture(
        &session->presentation.confinement_visit.speech_snapshot, speech);
    session->presentation.confinement_visit.speech_active = speech != NULL;
    session->presentation.confinement_visit.voice_started = 0;
    session->presentation.confinement_visit.speech_special_actor = special_actor;
    session->presentation.confinement_visit.speech_tick = now;
    if (speech != NULL && decoded.speaking_resource >= 0) {
        rich4_confinement_emit(
            audio_events, RICH4_AUDIO_EVENT_SPEECH_START,
            decoded.speaking_resource);
        session->presentation.confinement_visit.voice_started = 1;
    }
}

void rich4_confinement_begin(
    GameSession *session, const GameEvent *event, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    GameConfinementVisitPresentationState *visit;
    int kind;
    if (session == NULL || event == NULL) return;
    kind = rich4_game_event_confinement_kind(event);
    if (kind != GAME_CONFINEMENT_PRISON && kind != GAME_CONFINEMENT_HOSPITAL)
        return;
    visit = &session->presentation.confinement_visit;
    memset(visit, 0, sizeof(*visit));
    visit->active = 1;
    visit->kind = kind;
    visit->phase = kind == GAME_CONFINEMENT_HOSPITAL
        ? RICH4_CONFINEMENT_VISIT_HOSPITAL_INTRO
        : RICH4_CONFINEMENT_VISIT_SELECT;
    visit->hover = -1;
    visit->selected = -1;
    visit->confirm_hover = 0;
    visit->effect_tick = now;
    visit->effect_rng = (uint32_t)now ^
        (uint32_t)(event->player + 1) * 0x9e3779b9u;
    if (visit->effect_rng == 0) visit->effect_rng = 1;
    visit->nurse_gesture_frame = -1;
    visit->nurse_idle_chunk = -1;
    if (kind == GAME_CONFINEMENT_HOSPITAL)
        rich4_confinement_visit_start_speech(
            session,
            rich4_hospital_nurse_speech(RICH4_HOSPITAL_SPEECH_GREETING),
            0, now, audio_events);
}

int rich4_confinement_visit_finish_route(
    GameSession *session, int cancel)
{
    GameEventQueue events;
    GameEvent event;
    RouteEventExecutionResult result;
    if (session == NULL || !session->presentation.confinement_visit.active)
        return 0;
    rich4_game_event_queue_reset(&events);
    result = cancel
        ? rich4_route_event_executor_cancel_confinement(
              &session->flow.route_events, &session->state, &events)
        : rich4_route_event_executor_resume_confinement(
              &session->flow.route_events, &session->state,
              rich4_confinement_visit_target(
                  session->presentation.confinement_visit.selected), &events);
    if (result == ROUTE_EVENT_EXECUTION_INVALID) return 0;
    while (rich4_game_event_queue_pop(&events, &event))
        if (event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED)
            session->presentation.route_interaction.active = 0;
    if (!cancel) session->presentation.confinement_visit.release_committed = 1;
    return 1;
}

static void rich4_confinement_close(GameSession *session)
{
    GameConfinementVisitPresentationState *visit;
    if (session == NULL) return;
    visit = &session->presentation.confinement_visit;
    visit->active = 0;
    visit->hover = -1;
    visit->selected = -1;
    visit->speech = NULL;
    rich4_character_speech_snapshot_clear(&visit->speech_snapshot);
    visit->speech_active = 0;
    visit->voice_started = 0;
    visit->speech_special_actor = 0;
}

void rich4_confinement_finish(GameSession *session, int cancel)
{
    if (session == NULL || !session->presentation.confinement_visit.active)
        return;
    if (!session->presentation.confinement_visit.release_committed &&
        !rich4_confinement_visit_finish_route(session, cancel)) return;
    rich4_confinement_close(session);
}

int rich4_confinement_handle_select(
    GameSession *session, int slot, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    GameConfinementVisitPresentationState *visit;
    int actor;
    int cost;
    if (session == NULL || !session->presentation.confinement_visit.active ||
        session->presentation.confinement_visit.phase !=
            RICH4_CONFINEMENT_VISIT_SELECT || slot < 0 || slot >= 8)
        return 0;
    visit = &session->presentation.confinement_visit;
    if (!rich4_confinement_visit_candidate(
            &session->state, rich4_confinement_visit_target(slot),
            visit->kind))
        return 0;
    actor = session->flow.route_events.state.execution_actor;
    cost = rich4_confinement_visit_cost(
        rich4_confinement_visit_target(slot));
    visit->hover = -1;
    if (actor < 0 || actor >= session->state.player_count ||
        session->state.players[actor].points < cost) {
        visit->phase = RICH4_CONFINEMENT_VISIT_INSUFFICIENT;
        rich4_confinement_visit_start_speech(
            session,
            visit->kind == GAME_CONFINEMENT_HOSPITAL
                ? rich4_hospital_nurse_speech(RICH4_HOSPITAL_SPEECH_INSUFFICIENT)
                : rich4_prison_insufficient_speech(),
            0, now, audio_events);
        return 1;
    }
    visit->selected = slot;
    visit->phase = RICH4_CONFINEMENT_VISIT_CONFIRM;
    visit->confirm_hover = 1;
    return 1;
}

int rich4_confinement_handle_confirm(
    GameSession *session, int accept, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    GameConfinementVisitPresentationState *visit;
    if (session == NULL || !session->presentation.confinement_visit.active ||
        session->presentation.confinement_visit.phase !=
            RICH4_CONFINEMENT_VISIT_CONFIRM)
        return 0;
    visit = &session->presentation.confinement_visit;
    if (!accept) {
        rich4_confinement_emit(audio_events, RICH4_AUDIO_EVENT_CANCEL, 0);
        visit->phase = RICH4_CONFINEMENT_VISIT_SELECT;
        visit->selected = -1;
        visit->confirm_hover = 0;
        return 1;
    }
    rich4_confinement_emit(audio_events, RICH4_AUDIO_EVENT_OK, 0);
    if (visit->kind == GAME_CONFINEMENT_HOSPITAL) {
        visit->phase = RICH4_CONFINEMENT_VISIT_HOSPITAL_RELEASE;
        rich4_confinement_visit_start_speech(
            session,
            rich4_hospital_nurse_speech(
                RICH4_HOSPITAL_SPEECH_RELEASE_APPROVED),
            0, now, audio_events);
    } else if (visit->selected >= GAME_MAX_PLAYERS) {
        int special;
        if (!rich4_confinement_visit_finish_route(session, 0)) return 1;
        special = visit->selected - GAME_MAX_PLAYERS;
        visit->phase = RICH4_CONFINEMENT_VISIT_PRISON_RESCUED;
        rich4_confinement_visit_start_speech(
            session, rich4_prison_rescued_special_speech(special),
            1, now, audio_events);
    } else {
        rich4_confinement_finish(session, 0);
    }
    return 1;
}

int rich4_confinement_handle_cancel(
    GameSession *session, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    GameConfinementVisitPresentationState *visit;
    if (session == NULL || !session->presentation.confinement_visit.active)
        return 0;
    visit = &session->presentation.confinement_visit;
    if (visit->phase == RICH4_CONFINEMENT_VISIT_CONFIRM)
        return rich4_confinement_handle_confirm(session, 0, now, audio_events);
    if (visit->kind == GAME_CONFINEMENT_HOSPITAL) {
        if (visit->phase == RICH4_CONFINEMENT_VISIT_SELECT)
            rich4_confinement_visit_start_hospital_exit(
                session, now, audio_events);
        else
            rich4_confinement_visit_skip_speech(
                session, now, audio_events);
    } else {
        rich4_confinement_emit(audio_events, RICH4_AUDIO_EVENT_SPEECH_STOP, 0);
        rich4_confinement_finish(session, 1);
    }
    return 1;
}

void rich4_confinement_visit_start_hospital_exit(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    if (session == NULL) return;
    session->presentation.confinement_visit.phase =
        RICH4_CONFINEMENT_VISIT_HOSPITAL_EXIT;
    session->presentation.confinement_visit.hover = -1;
    session->presentation.confinement_visit.nurse_side = 1;
    session->presentation.confinement_visit.nurse_gesture_frame = -1;
    session->presentation.confinement_visit.nurse_idle_chunk = -1;
    session->presentation.confinement_visit.nurse_idle_ticks = 0;
    rich4_confinement_visit_start_speech(
        session,
        rich4_hospital_nurse_speech(RICH4_HOSPITAL_SPEECH_GOODBYE),
        0, now, audio_events);
}

static void rich4_confinement_advance_speech(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    int special;
    GameConfinementVisitPresentationState *visit;
    if (session == NULL || !session->presentation.confinement_visit.active)
        return;
    visit = &session->presentation.confinement_visit;
    visit->speech_active = 0;
    visit->voice_started = 0;
    visit->speech = NULL;
    rich4_character_speech_snapshot_clear(&visit->speech_snapshot);
    visit->speech_special_actor = 0;
    switch (visit->phase) {
    case RICH4_CONFINEMENT_VISIT_PRISON_RESCUED:
        rich4_confinement_close(session);
        break;
    case RICH4_CONFINEMENT_VISIT_HOSPITAL_INTRO:
        visit->phase = RICH4_CONFINEMENT_VISIT_SELECT;
        break;
    case RICH4_CONFINEMENT_VISIT_HOSPITAL_RELEASE:
        if (!rich4_confinement_visit_finish_route(session, 0)) break;
        if (visit->selected >= GAME_MAX_PLAYERS) {
            special = visit->selected - GAME_MAX_PLAYERS;
            visit->phase = RICH4_CONFINEMENT_VISIT_HOSPITAL_RESCUED;
            visit->nurse_idle_ticks = 0;
            rich4_confinement_visit_start_speech(
                session, rich4_hospital_rescued_special_speech(special),
                1, now, audio_events);
        } else {
            visit->phase = RICH4_CONFINEMENT_VISIT_HOSPITAL_RELEASED_WAIT;
            visit->until = now + 100;
        }
        break;
    case RICH4_CONFINEMENT_VISIT_HOSPITAL_RESCUED:
        visit->phase = RICH4_CONFINEMENT_VISIT_HOSPITAL_RELEASED_WAIT;
        visit->until = now + 100;
        break;
    case RICH4_CONFINEMENT_VISIT_HOSPITAL_EXIT:
        if (visit->release_committed) rich4_confinement_close(session);
        else rich4_confinement_finish(session, 1);
        break;
    case RICH4_CONFINEMENT_VISIT_INSUFFICIENT:
        visit->phase = RICH4_CONFINEMENT_VISIT_SELECT;
        break;
    default:
        break;
    }
}

void rich4_confinement_visit_skip_speech(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    if (session == NULL || !session->presentation.confinement_visit.speech_active)
        return;
    rich4_confinement_emit(audio_events, RICH4_AUDIO_EVENT_SPEECH_STOP, 0);
    rich4_confinement_advance_speech(session, now, audio_events);
}

static unsigned int rich4_confinement_effect_random(GameSession *session)
{
    GameConfinementVisitPresentationState *visit =
        &session->presentation.confinement_visit;
    visit->effect_rng = visit->effect_rng * 214013u + 2531011u;
    return (visit->effect_rng >> 16) & 0x7fffu;
}

static void rich4_confinement_advance_effects(GameSession *session)
{
    GameConfinementVisitPresentationState *visit;
    unsigned int random_value;
    int side;
    if (session == NULL ||
        session->presentation.confinement_visit.kind != GAME_CONFINEMENT_HOSPITAL)
        return;
    visit = &session->presentation.confinement_visit;
    side = visit->nurse_side != 0;
    if (visit->nurse_gesture_frame >= 0) {
        visit->nurse_gesture_frame++;
        if (visit->nurse_gesture_frame >= 3) visit->nurse_gesture_frame = -1;
    } else {
        random_value = rich4_confinement_effect_random(session);
        if ((random_value >> 10) == 0) visit->nurse_gesture_frame = 0;
    }
    if (visit->nurse_idle_ticks > 0) {
        visit->nurse_idle_ticks--;
        if (visit->nurse_idle_ticks == 0)
            visit->nurse_idle_chunk = side ? 13 : 8;
    } else if (visit->speech_active && !visit->speech_special_actor) {
        random_value = rich4_confinement_effect_random(session);
        if ((random_value >> 11) < 4) {
            visit->nurse_idle_chunk =
                (rich4_confinement_effect_random(session) & 1u) != 0
                    ? (side ? 12 : 7) : -1;
            visit->nurse_idle_ticks =
                (int)(rich4_confinement_effect_random(session) & 7u);
            if (visit->nurse_idle_ticks == 0) visit->nurse_idle_ticks = 1;
        }
    }
}

int rich4_confinement_update(
    GameSession *session, uint32_t now, int speech_playing,
    Rich4AudioEventQueue *audio_events)
{
    GameConfinementVisitPresentationState *visit;
    int steps = 0;
    if (session == NULL || !session->presentation.confinement_visit.active)
        return 0;
    visit = &session->presentation.confinement_visit;
    while (now - visit->effect_tick >= 100u && steps < 32) {
        visit->effect_tick += 100u;
        rich4_confinement_advance_effects(session);
        steps++;
    }
    if (steps == 32 && now - visit->effect_tick >= 100u)
        visit->effect_tick = now;
    if (visit->kind == GAME_CONFINEMENT_HOSPITAL &&
        visit->phase == RICH4_CONFINEMENT_VISIT_HOSPITAL_RELEASED_WAIT &&
        (int32_t)(now - visit->until) >= 0) {
        rich4_confinement_visit_start_hospital_exit(
            session, now, audio_events);
        return 1;
    }
    if (visit->speech_active &&
        (now - visit->speech_tick < 2000u || speech_playing)) return 1;
    if (visit->speech_active)
        rich4_confinement_advance_speech(session, now, audio_events);
    return visit->active;
}
