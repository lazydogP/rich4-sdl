#include "app/special_place_notice_flow.h"

#include "app/game_speech_flow.h"
#include "core/game_event_payload.h"
#include "core/rng.h"
#include "core/route_event_executor.h"
#include "data/character_speech.h"

const GameEvent *rich4_special_place_notice_current(
    const GamePresentationState *presentation)
{
    const GameEventQueue *queue;
    if (presentation == NULL) return NULL;
    queue = &presentation->special_place_notice.queue;
    return queue->count > 0 ? &queue->events[queue->head] : NULL;
}

int rich4_special_place_notice_enqueue(
    GameSession *session, const GameEvent *event, uint32_t now)
{
    GameSpecialPlaceNoticePresentationState *notice;
    Rich4GameEventFields fields;
    int empty;
    if (session == NULL || event == NULL ||
        !rich4_game_event_special_place_notice_requested_is_valid(event)) return 0;
    fields = rich4_game_event_special_place_notice_requested_fields(event);
    if (event->player < 0 || event->player >= session->state.player_count ||
        fields.subject < GAME_SPECIAL_NOTICE_PRISON_RELEASE ||
        fields.subject > GAME_SPECIAL_NOTICE_BANK_OWNER_SHORTFALL ||
        fields.value2 <= 0) return 0;
    notice = &session->presentation.special_place_notice;
    empty = notice->queue.count == 0;
    if (!rich4_game_event_queue_push(&notice->queue, event)) return 0;
    if (empty) {
        notice->until = now + (uint32_t)fields.value2;
        notice->speech_pending = 0;
    }
    return 1;
}

int rich4_special_place_notice_update(GameSession *session, uint32_t now)
{
    GameSpecialPlaceNoticePresentationState *notice;
    const GameEvent *event;
    Rich4GameEventFields fields;
    GameEvent completed;
    if (session == NULL) return 0;
    event = rich4_special_place_notice_current(&session->presentation);
    if (event == NULL) return 0;
    notice = &session->presentation.special_place_notice;
    if ((int32_t)(now - notice->until) < 0) return 1;
    fields = rich4_game_event_special_place_notice_requested_fields(event);
    if (!notice->speech_pending && fields.subject == GAME_SPECIAL_NOTICE_SHOP_GIFT) {
        /* 0x42EA23 -> 0x44F230 runs only after the 1500 ms gift text. */
        int variant = fields.value > 50 && fields.value <= 100
            ? (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 1u) : 0;
        const Rich4CharacterSpeech *speech = rich4_character_shop_gift_speech(
            session->state.players[event->player].character, fields.value, variant);
        notice->speech_pending = 1;
        if (game_start_speech_entry(session, event->player, speech, 0,
                                   GAME_PROPERTY_PRESENT_NONE, now)) return 1;
    }
    if (notice->speech_pending && session->presentation.speech.active) return 1;
    if (fields.subject == GAME_SPECIAL_NOTICE_PRISON_RELEASE ||
        fields.subject == GAME_SPECIAL_NOTICE_HOSPITAL_RELEASE) {
        if (rich4_route_event_executor_resume_confinement(
                &session->flow.route_events, &session->state, fields.target_ref,
                &session->deferred_events) == ROUTE_EVENT_EXECUTION_INVALID) return 1;
    } else if (fields.subject == GAME_SPECIAL_NOTICE_SHOP_GIFT) {
        if (rich4_route_event_executor_resume_ai_shop(
                &session->flow.route_events, &session->state,
                &session->deferred_events) == ROUTE_EVENT_EXECUTION_INVALID) return 1;
    }
    rich4_game_event_queue_pop(&notice->queue, &completed);
    notice->speech_pending = 0;
    event = rich4_special_place_notice_current(&session->presentation);
    if (event != NULL)
        notice->until = now + (uint32_t)
            rich4_game_event_special_place_notice_requested_fields(event).value2;
    /* Own the completion frame, so the next route/turn cannot erase the notice. */
    return 1;
}

int rich4_special_place_notice_skip(GameSession *session, uint32_t now)
{
    GamePresentationState *p;
    if (session == NULL) return 0;
    p = &session->presentation;
    /* 0x440CAC -> 0x4528B9 accepts any key-up and left/right button-up.
     * Reward FLCs use flags=1, so their animation itself is not skippable. */
    if (p->special_place_notice.queue.count > 0) {
        if (p->special_place_notice.speech_pending) return 0;
        p->special_place_notice.until = now;
    } else if (p->bank_rejection.active) {
        p->bank_rejection.until = now;
    } else if (p->point_minigame.active && p->point_minigame.automatic &&
               p->point_minigame.phase == RICH4_POINT_MINIGAME_RESULT) {
        p->point_minigame.finish_until = now;
    } else if (p->magic_house.active && p->magic_house.phase == RICH4_MAGIC_HOUSE_AI_NOTICE) {
        p->magic_house.phase_until = now;
    } else if (p->special_reward.active && p->special_reward.action >= 10 &&
               p->special_reward.action <= 13 && p->special_reward.phase > 0) {
        p->special_reward.until = now;
    } else return 0;
    return 1;
}
