#include "app/card_defense_flow.h"
#include <string.h>
#include "app/game_speech_flow.h"
#include "core/card.h"
#include "core/rng.h"
#include "data/character_speech.h"

static void defense_say(GameSession *session, int player, int role, int card,
    int type, uint32_t now)
{
    if (player >= 0 && player < session->state.player_count)
        (void)game_start_speech_entry(session, player,
            rich4_character_card_speech(session->state.players[player].character,
                role, card), type, session->presentation.property.phase, now);
}

static void defense_popup(GameSession *session, uint32_t now)
{
    GameCardUsePresentationState *use = &session->presentation.card_use;
    GameCardDefenseState *d = &use->defense;
    int node = session->state.players[d->actor].node;
    if (node > 0 && node < session->state.tool_route_node_count)
        game_focus_view_at(session, session->state.tool_route_x[node],
            session->state.tool_route_y[node]);
    d->phase = GAME_PROPERTY_PRESENT_INSURANCE_CARD;
    d->started = now;
    d->skip_requested = 0;
    use->sound_pending = 1;
}

static int defense_ai_target(GameSession *session, int actor)
{
    int i, best = -1, balance = 0, candidates[GAME_MAX_PLAYERS], count = 0;
    Rich4Rng rng = {session->state.ai_rng_state};
    for (i = 0; i < session->state.player_count; ++i) {
        const GamePlayerState *p = &session->state.players[i];
        if (i == actor || !(p->play_state & 0xff)) continue;
        if (session->state.players[actor].relation_balance[i] > balance) {
            balance = session->state.players[actor].relation_balance[i];
            best = i;
        }
        if (!p->delayed_status_block && !p->days_disappearing &&
            !p->days_in_prison && !p->days_in_hospital) candidates[count++] = i;
    }
    if (best < 0 && count)
        best = candidates[rich4_rng_original_state_range(&rng, count)];
    session->state.ai_rng_state = rng.state;
    return best;
}

static void defense_redirect(GameSession *session, uint32_t now)
{
    GameCardUsePresentationState *use = &session->presentation.card_use;
    GameCardDefenseState *d = &use->defense;
    d->phase = GAME_PROPERTY_PRESENT_NONE;
    d->card = 18;
    d->accepted = 0;
    d->target = -1;
    if (!rich4_card_inventory_has(&session->state, d->actor, 18) ||
        (use->command.subject.id == 25 && use->amount <= 2000)) return;
    if (session->state.players[d->actor].is_ai) {
        d->target = defense_ai_target(session, d->actor);
        /* 0x4448B9 chooses (and advances rand) before mode 2's cash gate. */
        if (use->command.subject.id == 25 &&
            (int64_t)session->state.players[d->actor].cash <=
                (int64_t)20000 * session->state.price_index) d->target = -1;
        if (d->target < 0) return;
        d->accepted = 1;
    }
    defense_popup(session, now);
}

void rich4_card_defense_begin(GameSession *session, uint32_t now)
{
    GameCardUsePresentationState *use = &session->presentation.card_use;
    GameCardDefenseState *d = &use->defense;
    int card = use->command.subject.id;
    memset(d, 0, sizeof(*d));
    d->actor = use->victim_player;
    d->target = d->hover = -1;
    if (use->command.target.kind != GAME_DOMAIN_PLAYER ||
        (card != 15 && card != 16 && card != 25)) return;
    rich4_card_prepare_defense(&session->state, &use->command);
    if (card != 25) {
        use->command.flags |= GAME_CARD_FLAG_DEFER_REVENGE;
        if (rich4_card_inventory_has(&session->state, d->actor, 20)) {
            d->card = 20;
            d->accepted = 1;
            use->blocked = 1;
            defense_popup(session, now);
            return;
        }
        use->revenge_pending = rich4_card_inventory_has(&session->state, d->actor, 17);
    } else if (rich4_card_inventory_has(&session->state, d->actor, 19)) {
        d->card = 19;
        if (!session->state.players[d->actor].is_ai) {
            int node = session->state.players[d->actor].node;
            if (node > 0 && node < session->state.tool_route_node_count)
                game_focus_view_at(session, session->state.tool_route_x[node],
                    session->state.tool_route_y[node]);
            d->phase = GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM;
            return;
        } else {
            Rich4Rng rng = {session->state.ai_rng_state};
            int64_t limit = (int64_t)(rich4_rng_original_state_range(&rng, 3000) + 3000) *
                session->state.price_index;
            session->state.ai_rng_state = rng.state;
            if (use->amount > session->state.players[d->actor].cash || use->amount > limit) {
                d->accepted = 1;
                defense_popup(session, now);
                return;
            }
        }
    }
    defense_redirect(session, now);
}

void rich4_card_defense_revenge_begin(GameSession *session, uint32_t now)
{
    GameCardDefenseState *d = &session->presentation.card_use.defense;
    memset(d, 0, sizeof(*d));
    d->actor = session->presentation.card_use.command.target.id;
    d->card = 17;
    d->target = -1;
    d->accepted = 1;
    defense_popup(session, now);
}

int rich4_card_defense_choose(GameSession *session, int target, uint32_t now)
{
    GameCardDefenseState *d = &session->presentation.card_use.defense;
    if (d->phase == GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM) {
        if (target >= 0) { d->accepted = 1; defense_popup(session, now); }
        else defense_redirect(session, now);
    } else if (d->phase == GAME_PROPERTY_PRESENT_INSURANCE_TARGET) {
        if (target >= session->state.player_count || target == d->actor ||
            (target >= 0 && !(session->state.players[target].play_state & 0xff))) return 0;
        d->target = target;
        d->accepted = target >= 0;
        d->phase = target >= 0 ? GAME_PROPERTY_PRESENT_INSURANCE_CARD_SPEECH :
            GAME_PROPERTY_PRESENT_NONE;
        if (target >= 0) defense_say(session, d->actor, 0, d->card, 0, now);
    } else return 0;
    return 1;
}

int rich4_card_defense_hover(GameSession *session, int target)
{
    GameCardDefenseState *d = &session->presentation.card_use.defense;
    if (d->phase != GAME_PROPERTY_PRESENT_INSURANCE_TARGET) return 0;
    d->hover = target;
    return 1;
}

int rich4_card_defense_update(GameSession *session, uint32_t now)
{
    GameCardUsePresentationState *use = &session->presentation.card_use;
    GameCardDefenseState *d = &use->defense;
    if (session->presentation.speech.active) return 1;
    switch (d->phase) {
    case GAME_PROPERTY_PRESENT_NONE: return 0;
    case GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM:
    case GAME_PROPERTY_PRESENT_INSURANCE_TARGET: return 1;
    case GAME_PROPERTY_PRESENT_INSURANCE_CARD:
        if (!d->skip_requested && now - d->started < 1500u) return 1;
        d->skip_requested = 0;
        if (d->card == 18) {
            d->phase = session->state.players[d->actor].is_ai ?
                GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT_NOTICE :
                GAME_PROPERTY_PRESENT_INSURANCE_TARGET;
            d->started = now;
            return 1;
        }
        break;
    case GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT_NOTICE:
        if (!d->skip_requested && now - d->started < 1500u) return 1;
        d->skip_requested = 0;
        break;
    case GAME_PROPERTY_PRESENT_INSURANCE_CARD_SPEECH:
        d->phase = GAME_PROPERTY_PRESENT_INSURANCE_VICTIM_SPEECH;
        if (d->card == 18) defense_say(session, d->target, 2, 18, 2, now);
        else if (d->card == 19) defense_say(session, use->command.player, 2, 19, 1, now);
        return 1;
    case GAME_PROPERTY_PRESENT_INSURANCE_VICTIM_SPEECH:
        if (d->card == 19) {
            use->command.flags |= GAME_CARD_FLAG_ACCEPT_FREE;
            use->blocked = 1;
        } else if (d->card == 18 && d->accepted) {
            use->command.flags |= GAME_CARD_FLAG_USE_MISDIRECT;
            use->command.value = d->target;
            use->victim_player = d->target;
            use->revenge_pending = 0;
            if (use->command.subject.id == 25)
                use->amount = session->state.players[d->target].cash / 5;
        }
        d->phase = GAME_PROPERTY_PRESENT_NONE;
        return 0;
    default: return 1;
    }
    d->phase = GAME_PROPERTY_PRESENT_INSURANCE_CARD_SPEECH;
    defense_say(session, d->actor, 0, d->card, 0, now);
    return 1;
}
