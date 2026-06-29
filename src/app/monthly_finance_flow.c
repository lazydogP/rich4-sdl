#include "app/monthly_finance_flow.h"
#include "core/bank.h"
#include "core/rng.h"

#include <string.h>

static void periodic_stage(GamePeriodicFinancePresentationState *p,
    int phase, int speech, uint32_t now)
{
    p->phase = phase;
    p->ticks = 0;
    p->tick = p->animation_tick = now;
    p->stage_started = now;
    p->animation_frame = 0;
    p->speech_id = speech;
    p->speech_pending = speech != 0;
}

void rich4_periodic_finance_begin(GameSession *s, uint32_t now)
{
    GamePeriodicFinancePresentationState *p;
    if (s == NULL || s->presentation.periodic_finance.active) return;
    p = &s->presentation.periodic_finance;
    memset(p, 0, sizeof(*p));
    p->winner = -1;
    if (!rich4_company_dividend_prepare(&s->state, &p->dividend)) return;
    p->active = 1;
    p->effect_pending = 61;
    periodic_stage(p, RICH4_FINANCE_DIVIDEND, 0, now);
}

/* 0x431110..0x431561: the hosts share the original rand stream with
 * 0x430B52/0x430B66. These calls must run even while speech is playing. */
static void lottery_hosts(GameState *state,
    GamePeriodicFinancePresentationState *p, int speaking)
{
    static const int right_blink[4] = {8,7,8,10};
    static const int left_blink[4] = {16,17,16,15};
    Rich4Rng rng = {state->ai_rng_state};
    unsigned int mode = p->host_state & 15;
    int value;
    if (mode == 0) {
        value = rich4_rng_original_state_next(&rng) >> 9;
        if (value == 0) p->host_state |= 1;
        else if (value == 1) p->host_state |= 2;
        else if (value < 4) p->host_state |= 3;
        else if (value < 6) p->host_state |= 4;
    } else if (mode == 1) {
        p->right_expression = right_blink[(p->host_state >> 4) & 3];
        p->host_state += 16;
        p->host_state &= ~0xc0u;
        if ((p->host_state & 0x30u) == 0) {
            p->host_state |= 0x1000u; p->host_state &= ~15u;
        }
    } else if (mode == 2) {
        p->left_expression = left_blink[(p->host_state >> 8) & 3];
        p->host_state += 256;
        p->host_state &= ~0xc00u;
        if ((p->host_state & 0x300u) == 0) {
            p->host_state |= 0x2000u; p->host_state &= ~15u;
        }
    } else if (mode == 3 || mode == 4) {
        value = rich4_rng_original_state_next(&rng) & 1;
        if (mode == 3 && value != (int)((p->host_state >> 12) & 1))
            p->right_expression = 9 + value;
        if (mode == 4 && value != (int)((p->host_state >> 13) & 1))
            p->left_expression = 14 + value;
        p->host_state &= ~15u;
    }
    if (speaking || p->mouth_ticks > 0) {
        if (p->mouth_ticks > 0) {
            if (--p->mouth_ticks == 0) p->mouth_chunk = 11;
        } else if ((rich4_rng_original_state_next(&rng) >> 11) < 4) {
            p->mouth_chunk = 12 + (rich4_rng_original_state_next(&rng) & 1);
            p->mouth_ticks = rich4_rng_original_state_next(&rng) & 15;
            if (p->mouth_ticks == 0) p->mouth_ticks = 1;
        }
    }
    state->ai_rng_state = rng.state;
}

int rich4_periodic_finance_update(GameSession *s, uint32_t now,
    int animation_enabled, int speech_playing, int frames, int delay,
    GameEventQueue *events)
{
    GamePeriodicFinancePresentationState *p;
    if (s == NULL || !s->presentation.periodic_finance.active) return 0;
    p = &s->presentation.periodic_finance;
    (void)delay; /* 0x430351/0x430F67 advance on WM_TIMER, not FLC speed. */
    if (p->phase == RICH4_FINANCE_DIVIDEND) {
        if (!p->close_requested && (uint32_t)(now - p->tick) < 3000) return 1;
        p->close_requested = 0;
        p->credit_player = 0;
        periodic_stage(p, -2, 0, now);
    }
    if (p->phase == -2) {
        Rich4CompanyDividend one = p->dividend;
        if (s->presentation.bankruptcy.active || s->presentation.bankruptcy.awaiting_auction ||
            s->presentation.auction.active) return 0;
        /* 0x42BE6D credits one index at a time; a bankrupt index blocks on
         * 0x40CD87 before the following player's balance is changed. */
        while (p->credit_player < s->state.player_count) {
            int player = p->credit_player;
            one.player_mask = p->dividend.player_mask & (1u << player);
            if (!rich4_company_dividend_commit(&s->state, &one, events)) return 1;
            s->presentation.actors[player].dirty_flags |= GAME_PLAYER_DIRTY_TURN;
            ++p->credit_player;
            if (s->state.players[player].play_state == 0 && one.player_mask != 0)
                return 1;
        }
        periodic_stage(p, RICH4_FINANCE_AFTER_DIVIDEND, 0, now);
        return 1;
    }
    if (p->phase == RICH4_FINANCE_AFTER_DIVIDEND) {
        if (s->presentation.bankruptcy.active || s->presentation.bankruptcy.awaiting_auction ||
            s->presentation.auction.active)
            return 0;
        if (!rich4_lottery_has_tickets(&s->state)) { p->active = 0; return 0; }
        p->pool = s->state.lottery_pool;
        p->music_started = 0;
        periodic_stage(p, animation_enabled ? 1 : 2,
                       animation_enabled ? 17 : 0, now);
        return 1;
    }
    if (p->phase == RICH4_FINANCE_NO_WINNER_REVEAL) {
        if (now - p->stage_started < 500) return 1;
        periodic_stage(p, 7, 33, now);
        return 1;
    }
    if ((uint32_t)(now - p->tick) < 50) return 1;
    p->tick = now;
    ++p->ticks;
    if (speech_playing || p->speech_pending ||
        (p->speech_id != 0 && now - p->stage_started < 2000)) goto hosts;
    switch (p->phase) {
    case 1: periodic_stage(p, 2, 18, now); break;
    case 2:
        periodic_stage(p, 3, 0, now);
        p->ticks = 1; /* the same original timer also enters state 3 */
        p->effect_pending = 57;
        break;
    case 3:
        if (p->ticks < 20) goto hosts;
        if (p->animation_frame < frames) ++p->animation_frame;
        if (p->animation_frame < frames) goto hosts;
        if (!rich4_lottery_draw(&s->state, &p->number, &p->winner)) return 1;
        periodic_stage(p, p->winner >= 0 ? 4 : RICH4_FINANCE_NO_WINNER_REVEAL,
                       p->winner >= 0 ? 19 : 0, now);
        break;
    case 4:
        periodic_stage(p, 5, 0, now);
        p->ticks = 1;
        p->effect_pending = 58;
        break;
    case 5:
        if (p->ticks <= 30) return 1;
        if (p->animation_frame < frames) ++p->animation_frame;
        if (p->animation_frame < frames) return 1;
        periodic_stage(p, 6, 32, now);
        break;
    case 6: periodic_stage(p, 8, 0, now); break;
    case 7: periodic_stage(p, 8, 34, now); break;
    case 8: periodic_stage(p, 9, 35, now); break;
    case 9: periodic_stage(p, 10, 36, now); break;
    case 10:
        if (!rich4_lottery_claim(&s->state, p->number, events)) return 1;
        if (p->winner >= 0)
            s->presentation.actors[p->winner].dirty_flags |= GAME_PLAYER_DIRTY_TURN;
        p->active = 0;
        break;
    default: break;
    }
hosts:
    if (p->phase <= 3 || p->phase == 9 || p->phase == 10)
        lottery_hosts(&s->state, p, speech_playing);
    return 1;
}

static void monthly_stage(GameBankSettlementPresentationState *p,
    int stage, int speech, uint32_t now)
{
    p->stage = stage;
    if (stage != 5 && stage != 15 && stage != 22) p->display_stage = stage;
    p->display_speech_id = speech;
    p->stage_ticks = 0;
    p->skip_requested = 0;
    p->speech_id = speech;
    p->speech_pending = speech != 0;
    p->animation_frame = 0;
    if (stage == 8 || stage == 18) p->animation_frame = -1;
    p->animation_tick = p->tick = now;
    p->stage_started = now;
}

void rich4_monthly_finance_begin(GameSession *s)
{
    GameBankSettlementPresentationState *p;
    int i;
    if (s == NULL) return;
    p = &s->presentation.bank_settlement;
    p->stage = 1;
    p->display_stage = 1;
    p->display_speech_id = 92;
    p->speech_id = 92;
    p->speech_pending = 1;
    p->stage_started = p->tick;
    p->effect_pending = 0;
    p->face_mode = -1;
    p->eye_state = p->eye_chunk = p->mouth_ticks = p->mouth_chunk = 0;
    p->unlucky_player = rich4_monthly_unlucky_player(&s->state, p->player_mask);
    p->richest_player = rich4_monthly_richest_player(&s->state, p->player_mask);
    for (i = 0; i < s->state.player_count; ++i) {
        p->monthly_loss[i] = s->state.players[i].monthly_loss;
        p->monthly_income[i] = s->state.players[i].monthly_income;
        p->monthly_bad_days[i] = (uint8_t)s->state.players[i].total_confinement_days;
        p->cash[i] = s->state.players[i].cash;
        p->assets[i] = rich4_bank_total_assets(&s->state, i);
    }
}

int rich4_monthly_finance_update(GameSession *s, uint32_t now,
    int animation_enabled, int speech_playing, int frames, int delay)
{
    GameBankSettlementPresentationState *p;
    if (s == NULL || !s->presentation.bank_settlement.active) return 0;
    p = &s->presentation.bank_settlement;
    if (!animation_enabled && p->stage == 1) {
        p->speech_id = 0;
        p->speech_pending = 0;
        p->display_speech_id = 0;
    }
    if (p->stage == 8 || p->stage == 18) {
        int unlucky = p->stage == 8;
        int player = unlucky ? p->unlucky_player : p->richest_player;
        int total = frames * rich4_monthly_animation_repeats(
            s->state.players[player].character, unlucky);
        if (delay <= 0) delay = 71;
        if (p->skip_requested) p->animation_frame = total;
        else if (p->animation_frame < 0) {
            if (now - p->stage_started < 100) return 1;
            p->animation_frame = 0;
            rich4_animation_clock_reset(&s->presentation.animation_clock[RICH4_ANIMATION_MONTHLY]);
            p->animation_tick = now;
        }
        if (p->animation_frame < total && rich4_animation_clock_step(
                &s->presentation.animation_clock[RICH4_ANIMATION_MONTHLY],
                now, delay)) {
            ++p->animation_frame; p->animation_tick = now;
        }
        if (p->animation_frame < total) return 1;
        monthly_stage(p, unlucky ? 9 : 19, unlucky ? 108 : 122, now);
        rich4_monthly_banker_update(s,speech_playing);
        return 1;
    }
    if ((uint32_t)(now - p->tick) < 100) return 1;
    p->tick = now;
    ++p->stage_ticks;
    if (speech_playing || p->speech_pending ||
        (p->speech_id != 0 && !p->skip_requested && now - p->stage_started < 2000)) {
        rich4_monthly_banker_update(s, speech_playing);
        return 1;
    }
    switch (p->stage) {
    case 1: monthly_stage(p, 2, 93, now); p->frame = 1; break;
    case 2:
        p->frame = 2;
        if (!animation_enabled) monthly_stage(p, 22, 0, now);
        else if (p->unlucky_player < 0 || p->unlucky_player == p->richest_player)
            monthly_stage(p, 15, 0, now);
        else monthly_stage(p, 5, 0, now);
        break;
    case 5:
        if (p->stage_ticks == 20) { p->display_stage = 5; p->display_speech_id = 0; }
        if (p->stage_ticks < 30 && !p->skip_requested) break;
        monthly_stage(p, 6, 95, now); p->effect_pending = 27; break;
    case 6:
        monthly_stage(p, 7, 96 + s->state.players[p->unlucky_player].character, now);
        p->effect_pending = 60;
        break;
    case 7:
        monthly_stage(p, 8, 0, now);
        rich4_monthly_banker_update(s,speech_playing);
        break;
    case 9: monthly_stage(p, 15, 0, now); p->stage_ticks = 19; break;
    case 15:
        if (p->stage_ticks == 20) { p->display_stage = 5; p->display_speech_id = 0; }
        if (p->stage_ticks < 30 && !p->skip_requested) break;
        monthly_stage(p, 16, 109, now); break;
    case 16:
        monthly_stage(p, 17, 110 + s->state.players[p->richest_player].character, now);
        p->effect_pending = 28;
        break;
    case 17: monthly_stage(p, 18, 0, now); break;
    case 19: monthly_stage(p, 22, 0, now); p->stage_ticks = 20; break;
    case 22:
        if (p->stage_ticks < 30 && !p->skip_requested) break;
        rich4_monthly_clear_statistics(&s->state, p->player_mask);
        if (s->presentation.bank_loan_due.active)
            s->presentation.bank_loan_due.until = now + 1500;
        p->active = 0;
        break;
    default: break;
    }
    if (p->active && p->stage != 8 && p->stage != 18)
        rich4_monthly_banker_update(s, speech_playing);
    return 1;
}

int rich4_monthly_finance_release(GameSession *s)
{
    if (s == NULL) return 0;
    if (s->presentation.bank_settlement.active) {
        s->presentation.bank_settlement.skip_requested = 1;
        return 1;
    }
    if (s->presentation.periodic_finance.active) {
        if (s->presentation.bankruptcy.active || s->presentation.bankruptcy.awaiting_auction ||
            s->presentation.auction.active) return 0;
        if (s->presentation.periodic_finance.phase == RICH4_FINANCE_DIVIDEND)
            s->presentation.periodic_finance.close_requested = 1;
        return 1;
    }
    return 0;
}

void rich4_periodic_finance_mark_music_started(GameSession *s)
{
    if (s != NULL && s->presentation.periodic_finance.active &&
        s->presentation.periodic_finance.phase > 0)
        s->presentation.periodic_finance.music_started = 1;
}

int rich4_monthly_finance_take_speech(GameSession *s, int enabled)
{
    if (s == NULL) return 0;
    if (s->presentation.bank_settlement.active) {
        GameBankSettlementPresentationState *p = &s->presentation.bank_settlement;
        if (!enabled && p->stage == 1)
            p->speech_id = p->speech_pending = p->display_speech_id = 0;
        if (p->skip_requested) {
            p->speech_pending = p->display_speech_id = 0; return -1;
        }
        if (p->speech_pending) { p->speech_pending = 0; return p->speech_id; }
    } else if (s->presentation.periodic_finance.active &&
               s->presentation.periodic_finance.speech_pending) {
        s->presentation.periodic_finance.speech_pending = 0;
        return s->presentation.periodic_finance.speech_id;
    }
    return 0;
}

int rich4_monthly_finance_take_effect(GameSession *s)
{
    int result = 0;
    if (s == NULL) return 0;
    if (s->presentation.bank_settlement.active) {
        result = s->presentation.bank_settlement.effect_pending;
        s->presentation.bank_settlement.effect_pending = 0;
    } else if (s->presentation.periodic_finance.active) {
        result = s->presentation.periodic_finance.effect_pending;
        s->presentation.periodic_finance.effect_pending = 0;
    }
    return result;
}
