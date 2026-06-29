#include "app/insurance_flow.h"
#include "core/game_event_payload.h"
#include "core/insurance.h"

#include <string.h>

void rich4_insurance_flow_enqueue_notice(
    GameSession *session, const GameEvent *event, uint32_t now)
{
    GameInsuranceNoticeEntry *notice;
    int tail;
    int kind;
    if (session == NULL || event == NULL ||
        (event->type != GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED &&
         event->type != GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED) ||
        rich4_game_event_insurance_amount(event) < 0 ||
        session->presentation.insurance.count >=
            RICH4_INSURANCE_NOTICE_CAPACITY)
        return;
    kind = event->type == GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED ?
        GAME_INSURANCE_NOTICE_FEE : GAME_INSURANCE_NOTICE_CLAIM;
    tail = (session->presentation.insurance.head +
            session->presentation.insurance.count) %
        RICH4_INSURANCE_NOTICE_CAPACITY;
    notice = &session->presentation.insurance.queue[tail];
    memset(notice, 0, sizeof(*notice));
    notice->kind = kind;
    notice->cause = kind == GAME_INSURANCE_NOTICE_CLAIM
        ? (int)rich4_game_event_flags(event) : 0;
    if (kind == GAME_INSURANCE_NOTICE_CLAIM &&
        ((unsigned int)notice->cause & GAME_INSURANCE_CLAIM_DEFERRED))
        notice->kind = session->presentation.special_event.active
            ? GAME_INSURANCE_NOTICE_WAIT_SPECIAL : GAME_INSURANCE_NOTICE_PENDING_CLAIM;
    if (kind == GAME_INSURANCE_NOTICE_CLAIM &&
        rich4_game_event_insurance_cause(event) == GAME_INSURANCE_CLAIM_HOTEL)
        notice->kind = GAME_INSURANCE_NOTICE_PENDING_HOTEL_CLAIM;
    notice->player = event->player;
    notice->company = rich4_game_event_insurance_company(event);
    notice->chairman = kind == GAME_INSURANCE_NOTICE_FEE ?
        rich4_game_event_insurance_claimant(event) : -1;
    notice->amount = rich4_game_event_insurance_amount(event);
    notice->duration = rich4_game_event_insurance_duration(event) > 0 ?
        rich4_game_event_insurance_duration(event) :
        (kind == GAME_INSURANCE_NOTICE_FEE ? 1500 : 2000);
    session->presentation.insurance.count++;
    if (session->presentation.insurance.count == 1)
        session->presentation.insurance.until = notice->kind >= GAME_INSURANCE_NOTICE_PENDING_CONFINEMENT_CLAIM
            ? 0 : now + (uint32_t)notice->duration;
}

int rich4_insurance_flow_resume_special_claim(GameSession *session, uint32_t now)
{
    GameInsuranceNoticeEntry *notice;
    if (session == NULL || session->presentation.insurance.count <= 0) return 0;
    notice = &session->presentation.insurance.queue[session->presentation.insurance.head];
    if (notice->kind != GAME_INSURANCE_NOTICE_WAIT_SPECIAL) return 0;
    notice->kind = GAME_INSURANCE_NOTICE_PENDING_CLAIM;
    session->presentation.insurance.until = now + (uint32_t)notice->duration;
    return 1;
}

void rich4_insurance_flow_enqueue_confinement_claim(
    GameSession *session, const GameEvent *event, uint32_t now)
{
    int count, tail;
    if (session == NULL || event == NULL ||
        event->type != GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED)
        return;
    count = session->presentation.insurance.count;
    rich4_insurance_flow_enqueue_notice(session, event, now);
    if (session->presentation.insurance.count == count) return;
    tail = (session->presentation.insurance.head + count) %
        RICH4_INSURANCE_NOTICE_CAPACITY;
    session->presentation.insurance.queue[tail].kind =
        GAME_INSURANCE_NOTICE_PENDING_CONFINEMENT_CLAIM;
    /* 0x44BA63 waits 2000ms before payment. Start when dispatch reaches
     * this notice; do not reset an older notice's running timer. */
    if (count == 0) session->presentation.insurance.until = 0;
}

int rich4_insurance_flow_update(GameSession *session, uint32_t now)
{
    GameInsuranceNoticeEntry *notice;
    if (session == NULL || session->presentation.insurance.count <= 0)
        return 0;
    notice = &session->presentation.insurance.queue[session->presentation.insurance.head];
    if (notice->kind == GAME_INSURANCE_NOTICE_WAIT_SPECIAL) return 0;
    if (session->presentation.insurance.until == 0) {
        notice = &session->presentation.insurance.queue[
            session->presentation.insurance.head];
        session->presentation.insurance.until = now + (uint32_t)notice->duration;
    }
    if ((int32_t)(now - session->presentation.insurance.until) < 0)
        return 1;
    notice = &session->presentation.insurance.queue[
        session->presentation.insurance.head];
    if (notice->kind == GAME_INSURANCE_NOTICE_PENDING_CONFINEMENT_CLAIM ||
        notice->kind == GAME_INSURANCE_NOTICE_PENDING_HOTEL_CLAIM ||
        notice->kind == GAME_INSURANCE_NOTICE_PENDING_CLAIM) {
        GameEventQueue events;
        int cause = (unsigned int)notice->cause & GAME_INSURANCE_CLAIM_CAUSE_MASK;
        int days = (unsigned int)notice->cause >> GAME_INSURANCE_CLAIM_DAYS_SHIFT & 0xff;
        rich4_game_event_queue_reset(&events);
        if (!rich4_insurance_settle_claim(&session->state, notice->player,
                notice->amount, cause ? (GameInsuranceClaimCause)cause :
                GAME_INSURANCE_CLAIM_HOSPITAL, &events)) return 1;
        /* 0x43EE04 follows the hospital claim for the actual admission days. */
        {
            if (days == 0 && (notice->kind == GAME_INSURANCE_NOTICE_PENDING_CONFINEMENT_CLAIM ||
                             notice->kind == GAME_INSURANCE_NOTICE_PENDING_HOTEL_CLAIM))
                days = session->state.price_index > 0
                    ? (int)((int64_t)notice->amount /
                        ((int64_t)2000 * session->state.price_index)) : 0;
            if (days > 0 && notice->player >= 0 && notice->player < session->state.player_count)
                session->state.players[notice->player].total_confinement_days =
                    (session->state.players[notice->player].total_confinement_days + days) & 0xff;
        }
        notice->kind = GAME_INSURANCE_NOTICE_CLAIM;
    }
    session->presentation.insurance.head =
        (session->presentation.insurance.head + 1) %
        RICH4_INSURANCE_NOTICE_CAPACITY;
    session->presentation.insurance.count--;
    if (session->presentation.insurance.count <= 0) {
        session->presentation.insurance.count = 0;
        session->presentation.insurance.head = 0;
        session->presentation.insurance.until = 0;
        return 0;
    }
    notice = &session->presentation.insurance.queue[
        session->presentation.insurance.head];
    session->presentation.insurance.until = notice->kind == GAME_INSURANCE_NOTICE_WAIT_SPECIAL
        ? 0 : now + (uint32_t)notice->duration;
    return 1;
}

int rich4_insurance_flow_skip(GameSession *session, uint32_t now)
{
    if (session == NULL || session->presentation.insurance.count <= 0)
        return 0;
    if (session->presentation.insurance.queue[session->presentation.insurance.head].kind ==
        GAME_INSURANCE_NOTICE_WAIT_SPECIAL) return 0;
    /* 0x44BAC1 -> 0x440DE8 -> 0x4528B9 accepts a release before payout. */
    session->presentation.insurance.until = now;
    return 1;
}
