#include "app/lottery_flow.h"

#include "core/character_speech_rules.h"
#include "core/route_event_executor.h"
#include "data/character_speech.h"

#include <string.h>

void rich4_lottery_animation_prepared(GameSession *session, uint32_t now)
{
    if (session != NULL) session->presentation.lottery.effect_tick = now;
}

static void rich4_lottery_request_reset(Rich4LotteryAudioRequest *request)
{
    if (request == NULL) return;
    request->effect = RICH4_LOTTERY_AUDIO_NONE;
    request->start_speech_resource = -1;
    request->stop_speech = 0;
}

static void rich4_lottery_start_speech(
    GameSession *session, int speech_index, uint32_t now,
    Rich4LotteryAudioRequest *request)
{
    const Rich4CharacterSpeech *speech;
    Rich4PlayerSayText decoded;

    if (session == NULL) return;
    speech = rich4_lottery_counter_speech(speech_index);
    rich4_player_say_decode(
        speech != NULL ? speech->big5 : NULL,
        speech != NULL ? speech->speaking_resource : -1, &decoded);
    session->presentation.lottery.speech_index = speech_index;
    session->presentation.lottery.speech_active = speech != NULL;
    session->presentation.lottery.voice_started = 0;
    session->presentation.lottery.speech_tick = now;
    if (speech != NULL && decoded.speaking_resource >= 0) {
        session->presentation.lottery.voice_started = 1;
        if (request != NULL)
            request->start_speech_resource = decoded.speaking_resource;
    }
}

static void rich4_lottery_finish_visit(GameSession *session)
{
    GameEventQueue events;
    GameEvent event;
    RouteEventExecutionResult result;

    if (session == NULL || !session->presentation.lottery.active) return;
    rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_finish_lottery(
        &session->flow.route_events, &session->state, &events);
    if (result == ROUTE_EVENT_EXECUTION_INVALID) return;
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED)
            session->presentation.route_interaction.active = 0;
    }
    session->presentation.lottery.active = 0;
    session->presentation.lottery.hover = -1;
    session->presentation.lottery.selected = -1;
    session->presentation.lottery.speech_active = 0;
}

void rich4_lottery_begin_visit(
    GameSession *session, const GameEvent *event, uint32_t now,
    int animation_enabled, Rich4LotteryAudioRequest *request)
{
    int actor;

    rich4_lottery_request_reset(request);
    if (session == NULL || event == NULL ||
        event->type != GAME_EVENT_LOTTERY_VISIT_REQUESTED)
        return;
    actor = session->flow.route_events.state.execution_actor;
    session->presentation.lottery.active = 1;
    session->presentation.lottery.hover = -1;
    session->presentation.lottery.selected = -1;
    session->presentation.lottery.speech_index = -1;
    session->presentation.lottery.speech_active = 0;
    session->presentation.lottery.voice_started = 0;
    session->presentation.lottery.animation_frame = 0;
    session->presentation.lottery.animation_complete = 0;
    session->presentation.lottery.blink_frame = -1;
    session->presentation.lottery.gesture_chunk = -1;
    session->presentation.lottery.gesture_ticks = 0;
    session->presentation.lottery.effect_tick = now;
    session->presentation.lottery.effect_rng =
        (uint32_t)now ^ (uint32_t)(actor + 1) * 0x9e3779b9u;
    if (session->presentation.lottery.effect_rng == 0)
        session->presentation.lottery.effect_rng = 1;
    if (actor < 0 || actor >= session->state.player_count ||
        session->state.players[actor].cash < 1000) {
        session->presentation.lottery.phase =
            RICH4_LOTTERY_VISIT_INSUFFICIENT;
        rich4_lottery_start_speech(session, 4, now, request);
    } else if (animation_enabled) {
        session->presentation.lottery.phase = RICH4_LOTTERY_VISIT_INTRO_1;
        rich4_lottery_start_speech(session, 0, now, request);
    } else {
        session->presentation.lottery.phase = RICH4_LOTTERY_VISIT_SELECT;
    }
}

static unsigned int rich4_lottery_effect_random(GameSession *session)
{
    session->presentation.lottery.effect_rng =
        session->presentation.lottery.effect_rng * 214013u + 2531011u;
    return (session->presentation.lottery.effect_rng >> 16) & 0x7fffu;
}

static void rich4_lottery_advance_effects(GameSession *session, int frames)
{
    unsigned int random_value;
    if (frames > 0)
        session->presentation.lottery.animation_frame =
            (session->presentation.lottery.animation_frame + 1) % frames;
    session->presentation.lottery.animation_complete = 1;
    if (session->presentation.lottery.blink_frame >= 0) {
        session->presentation.lottery.blink_frame++;
        if (session->presentation.lottery.blink_frame >= 3)
            session->presentation.lottery.blink_frame = -1;
    } else {
        random_value = rich4_lottery_effect_random(session);
        if ((random_value >> 10) == 0)
            session->presentation.lottery.blink_frame = 0;
    }
    if (session->presentation.lottery.gesture_ticks > 0) {
        session->presentation.lottery.gesture_ticks--;
        if (session->presentation.lottery.gesture_ticks == 0)
            session->presentation.lottery.gesture_chunk = -1;
    } else if (session->presentation.lottery.speech_active) {
        random_value = rich4_lottery_effect_random(session);
        if ((random_value >> 11) < 4) {
            session->presentation.lottery.gesture_chunk =
                5 + (int)(rich4_lottery_effect_random(session) & 1u);
            session->presentation.lottery.gesture_ticks =
                (int)(rich4_lottery_effect_random(session) & 7u);
            if (session->presentation.lottery.gesture_ticks == 0)
                session->presentation.lottery.gesture_ticks = 1;
        }
    }
}

int rich4_lottery_update(
    GameSession *session, int panel_frames, uint32_t now,
    int speech_playing, Rich4LotteryAudioRequest *request)
{

    rich4_lottery_request_reset(request);
    if (session == NULL || !session->presentation.lottery.active) return 0;
    if (now - session->presentation.lottery.effect_tick >= 100u) {
        session->presentation.lottery.effect_tick = now;
        rich4_lottery_advance_effects(session, panel_frames);
    }
    if (!session->presentation.lottery.speech_active) return 1;
    if (now - session->presentation.lottery.speech_tick < 2000 ||
        speech_playing)
        return 1;
    session->presentation.lottery.speech_active = 0;
    session->presentation.lottery.voice_started = 0;
    switch (session->presentation.lottery.phase) {
    case RICH4_LOTTERY_VISIT_INTRO_1:
        session->presentation.lottery.phase = RICH4_LOTTERY_VISIT_INTRO_2;
        rich4_lottery_start_speech(session, 1, now, request);
        break;
    case RICH4_LOTTERY_VISIT_INTRO_2:
        session->presentation.lottery.phase = RICH4_LOTTERY_VISIT_SELECT;
        rich4_lottery_start_speech(session, 2, now, request);
        break;
    case RICH4_LOTTERY_VISIT_INSUFFICIENT:
        session->presentation.lottery.phase = RICH4_LOTTERY_VISIT_EXIT;
        rich4_lottery_start_speech(session, 5, now, request);
        break;
    case RICH4_LOTTERY_VISIT_EXIT:
        rich4_lottery_finish_visit(session);
        break;
    default:
        break;
    }
    return session->presentation.lottery.active;
}

int rich4_lottery_number_at(const GameSession *session, int x, int y)
{
    int column;
    int row;
    int number;
    if (session == NULL || !session->presentation.lottery.active ||
        x < 30 || x > 606 || y < 271 || y > 463)
        return -1;
    column = (x - 30) / 64;
    row = (y - 271) / 48;
    number = row * 9 + column;
    return number >= 0 && number < 36 &&
           session->state.lottery_owner[number] == 0 ? number : -1;
}

void rich4_lottery_set_hover(GameSession *session, int hover)
{
    if (session != NULL) session->presentation.lottery.hover = hover;
}

static int rich4_lottery_purchase_number(
    GameSession *session, int number, uint32_t now,
    Rich4LotteryAudioRequest *request)
{
    GameEventQueue events;
    RouteEventExecutionResult result;

    if (session == NULL || !session->presentation.lottery.active ||
        number < 0 || number >= 36)
        return 0;
    rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_purchase_lottery(
        &session->flow.route_events, &session->state, number, &events);
    if (result != ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND ||
        !session->flow.route_events.state.lottery_purchase_committed)
        return 0;
    session->presentation.lottery.hover = -1;
    session->presentation.lottery.selected = number;
    session->presentation.lottery.phase = RICH4_LOTTERY_VISIT_EXIT;
    request->effect = RICH4_LOTTERY_AUDIO_PURCHASE;
    rich4_lottery_start_speech(session, 3, now, request);
    return 1;
}

void rich4_lottery_cancel_visit(
    GameSession *session, uint32_t now, Rich4LotteryAudioRequest *request)
{
    rich4_lottery_request_reset(request);
    if (session == NULL || !session->presentation.lottery.active) return;
    request->effect = RICH4_LOTTERY_AUDIO_CANCEL;
    session->presentation.lottery.hover = -1;
    session->presentation.lottery.phase = RICH4_LOTTERY_VISIT_EXIT;
    rich4_lottery_start_speech(session, 5, now, request);
}

void rich4_lottery_left_button_down(
    GameSession *session, int pointer_x, int pointer_y, uint32_t now,
    Rich4LotteryAudioRequest *request)
{
    int number;

    rich4_lottery_request_reset(request);
    if (session == NULL || !session->presentation.lottery.active) return;
    if (session->presentation.lottery.phase < RICH4_LOTTERY_VISIT_SELECT) {
        request->stop_speech = 1;
        session->presentation.lottery.speech_active = 0;
        session->presentation.lottery.phase = RICH4_LOTTERY_VISIT_SELECT;
    }
    number = rich4_lottery_number_at(session, pointer_x, pointer_y);
    if (session->presentation.lottery.phase == RICH4_LOTTERY_VISIT_SELECT &&
        number >= 0)
        (void)rich4_lottery_purchase_number(session, number, now, request);
}
