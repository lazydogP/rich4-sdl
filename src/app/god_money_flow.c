#include "app/god_money_flow.h"

#include <string.h>

#include "core/god.h"
#include "core/rng.h"

enum { GOD_MONEY_TICK_MS = 30 };

void rich4_god_money_begin(GameSession *session, uint32_t now,
                            Rich4AudioEventQueue *audio_events)
{
    GameGodAcquisitionPresentationState *god;
    GameGodMoneyPresentationState *wheel;
    const GamePlayerState *player;
    if (session == NULL) return;
    god = &session->presentation.god_acquisition;
    if (!god->active || !god->money_pending || god->money.active ||
        god->player < 0 || god->player >= session->state.player_count ||
        rich4_god_money_digits(god->god_type) == 0)
        return;
    player = &session->state.players[god->player];
    wheel = &god->money;
    memset(wheel, 0, sizeof(*wheel));
    wheel->active = 1;
    wheel->phase = 1;
    wheel->mode = rich4_god_money_digits(god->god_type) == 3;
    wheel->automatic = player->is_ai ||
        (player->play_state & 0xff) > 1 || player->days_sleep_walking > 0;
    /* The original runs its first iteration immediately. */
    wheel->next_tick = now;
    rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_ROULETTE, 1, 0);
}

void rich4_god_money_stop(GameSession *session)
{
    GameGodMoneyPresentationState *wheel;
    if (session == NULL || !session->presentation.god_acquisition.active)
        return;
    wheel = &session->presentation.god_acquisition.money;
    if (wheel->active && !wheel->automatic && wheel->phase == 1)
        wheel->phase = 2;
}

/* Literal state/counter order from 0x43EF3E. The current digit's odd/even
 * frame determines when deceleration starts; the higher digits keep rolling. */
static int advance_digits(GameGodMoneyPresentationState *wheel,
                            int mask, int stage)
{
    int digit;
    if (stage != 0) {
        if (wheel->slowdown_digit != stage &&
            (wheel->digits[4 - stage] & 1) != 0) {
            wheel->slowdown_delay = 1;
            wheel->slowdown_ticks = 0;
            wheel->slowdown_digit = stage;
        }
        if (wheel->slowdown_delay != 0) {
            if (wheel->slowdown_delay <= wheel->slowdown_ticks) {
                wheel->slowdown_delay++;
                wheel->slowdown_ticks = 0;
            }
            wheel->slowdown_ticks++;
        }
    }
    for (digit = 3; digit >= wheel->mode; digit--) {
        if ((mask & (1 << (3 - digit))) != 0 &&
            (4 - digit != stage ||
             wheel->slowdown_delay <= wheel->slowdown_ticks))
            wheel->digits[digit] = (wheel->digits[digit] + 1) % 20;
    }
    if (wheel->slowdown_delay != 8) return 0;
    wheel->slowdown_delay = 0;
    wheel->slowdown_ticks = 0;
    return 1;
}

static int settle_money(GameSession *session, GameEventQueue *events)
{
    GameGodAcquisitionPresentationState *god = &session->presentation.god_acquisition;
    GameGodMoneyPresentationState *wheel = &god->money;
    if (session->state.game_over ||
        (session->state.players[god->player].play_state & 0xff) == 0)
        return 1;
    if (god->god_type == 2 || god->god_type == 6)
        return rich4_god_resolve_money(&session->state, god->player,
            god->object_slot, god->god_type, wheel->amount, events) == GAME_GOD_APPLIED;
    while (wheel->settlement_player < session->state.player_count) {
        int other = wheel->settlement_player;
        if (other != god->player &&
            (session->state.players[other].play_state & 0xff) != 0) {
            if (rich4_god_resolve_money_player(&session->state, god->player,
                    god->object_slot, god->god_type, wheel->amount,
                    other, events) != GAME_GOD_APPLIED)
                return 0;
            wheel->settlement_player++;
            if (session->state.game_over ||
                (session->state.players[god->player].play_state & 0xff) == 0)
                return 1;
            /* Yield this event queue before the next charge. Otherwise two
             * eliminated opponents overwrite one bankruptcy presentation. */
            if ((session->state.players[other].play_state & 0xff) == 0)
                return 0;
        } else {
            wheel->settlement_player++;
        }
    }
    return 1;
}

int rich4_god_money_update(GameSession *session, uint32_t now,
                            Rich4AudioEventQueue *audio_events,
                            GameEventQueue *events)
{
    GameGodAcquisitionPresentationState *god;
    GameGodMoneyPresentationState *wheel;
    int digit;
    if (session == NULL || events == NULL) return 0;
    god = &session->presentation.god_acquisition;
    wheel = &god->money;
    if (!god->active || !wheel->active) return 0;
    if (!rich4_presentation_god_owns_frame(&session->presentation)) return 1;
    if ((int32_t)(now - wheel->next_tick) < 0) return 1;
    wheel->next_tick = now + GOD_MONEY_TICK_MS;
    if (wheel->phase < 3 && wheel->ticks % 10 == 0) {
        Rich4Rng rng;
        rng.state = session->state.ai_rng_state;

        for (digit = 0; digit < 4; digit++)
            wheel->digits[digit] = 2 * rich4_rng_state_range(&rng, 10) + 1;
        session->state.ai_rng_state = rng.state;
    }
    wheel->ticks++;
    if (wheel->automatic && wheel->ticks >= 40 && wheel->phase == 1)
        wheel->phase = 2;
    switch (wheel->phase) {
    case 1:
        (void)advance_digits(wheel, 15, 0);
        break;
    case 2:
        (void)advance_digits(wheel, 15, 0);
        wheel->phase = 3;
        wheel->ticks = 0;
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_OK, 1, 0);
        break;
    case 3:
        (void)advance_digits(wheel, 15, 0);
        if (wheel->ticks == 4) wheel->phase = 4;
        break;
    case 4:
        if (advance_digits(wheel, 15, 1)) wheel->phase = 5;
        break;
    case 5:
        if (advance_digits(wheel, 14, 2)) wheel->phase = 6;
        break;
    case 6:
        if (advance_digits(wheel, 12, 3)) wheel->phase = 7 + wheel->mode;
        break;
    case 7:
        if (advance_digits(wheel, 8, 4)) wheel->phase = 8;
        break;
    case 8:
        wheel->amount = 0;
        for (digit = wheel->mode; digit < 4; digit++)
            wheel->amount = wheel->amount * 10 + wheel->digits[digit] / 2;
        wheel->phase = 9;
        wheel->ticks = 0;
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_ROULETTE_STOP,
                               1, 0);
        break;
    case 9:
        if (wheel->ticks >= 40) wheel->phase = 10;
        break;
    case 10:
        /* Settlement happens after the result's 40-tick hold. Retain the
         * pending state on a full output queue so no money is lost. */
        if (!settle_money(session, events)) return 1;
        god->money_pending = 0;
        wheel->active = 0;
        god->active = 0;
        return 0;
    default:
        return 1;
    }
    return 1;
}
