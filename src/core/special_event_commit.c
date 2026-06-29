#include "core/special_event_internal.h"
#include "core/auction.h"
#include "core/confinement.h"
#include "core/card.h"
#include "core/card_internal.h"
#include "core/insurance.h"
#include "core/ledger.h"
#include "core/stock.h"
#include "core/tool.h"
#include "core/victory.h"
#include "core/game_event_payload.h"

#include <limits.h>
#include <string.h>

static void clear_pending_event(GameState *s)
{
    s->special_event_family=GAME_SPECIAL_EVENT_NONE; s->special_event_actor=-1;
    s->special_event_logical_id=-1; s->special_event_resolved_id=-1;
    s->special_event_resource_id=-1; s->special_event_panel_id=-1;
    s->special_event_prepared_value=0; s->special_event_target_mask=0;
    memset(s->special_event_player_amount, 0,
           sizeof(s->special_event_player_amount));
    s->special_event_cancelable=0; s->special_event_phase=GAME_SPECIAL_PHASE_IDLE;
}

static GameSpecialEventResult special_pay(GameState *s, int player, int amount,
                                          GameEventQueue *events)
{
    GamePaymentResult payment;
    if (amount == 0) return GAME_SPECIAL_EVENT_APPLIED;
    payment=(s->special_event_family == GAME_SPECIAL_EVENT_FORTUNE ?
        rich4_ledger_pay_fortune : rich4_ledger_pay)(
            s,player,rich4_ledger_system(),amount,0,events);
    if (payment==GAME_PAYMENT_REJECTED)
        return GAME_SPECIAL_EVENT_INVALID;
    if (payment==GAME_PAYMENT_LIQUIDATION)
        return GAME_SPECIAL_EVENT_LIQUIDATION;
    if (payment==GAME_PAYMENT_BANKRUPTCY)
        return GAME_SPECIAL_EVENT_BANKRUPTCY;
    return GAME_SPECIAL_EVENT_APPLIED;
}

static int fortune_take_card(GameState *s, int actor, int player, int slot)
{
    int card = s->players[player].card_inventory[slot] - 1;
    if (card < 0 || card >= GAME_CARD_COUNT) return 0;
    if (rich4_card_inventory_count(s,actor) == GAME_PLAYER_CARD_SLOTS) {
        int i, cheapest = -1, price = 10000;
        /* 0x4412E4 -> 0x44128F: first cheapest card, including ties. */
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
            int held = s->players[actor].card_inventory[i] - 1;
            if (held >= 0 && held < GAME_CARD_COUNT &&
                rich4_card_catalog[held].shop_price < price) {
                price = rich4_card_catalog[held].shop_price;
                cheapest = held;
            }
        }
        if (cheapest < 0 || !rich4_card_consume(s,actor,cheapest)) return 0;
    }
    return rich4_card_inventory_transfer(s,player,actor,card);
}

int rich4_special_event_fortune_take_card(GameState *s, int victim, int slot)
{
    GameState next;
    int actor;
    if (s == NULL || s->special_event_family != GAME_SPECIAL_EVENT_FORTUNE ||
        s->special_event_phase != GAME_SPECIAL_PHASE_PREPARED ||
        s->special_event_resolved_id != 5) return 0;
    actor = s->special_event_actor;
    if (!rich4_special_event_active_player(s,actor) ||
        (s->players[actor].play_state & 0xff) != 1 || actor == victim ||
        !rich4_special_event_active_player(s,victim)) return 0;
    if (slot == -1) return 1;
    if (slot < 0 || slot >= GAME_PLAYER_CARD_SLOTS) return 0;
    next = *s;
    if (!fortune_take_card(&next,actor,victim,slot)) return 0;
    *s = next;
    return 1;
}

static void fortune_take_cards(GameState *s, int actor)
{
    int player;
    for (player=0;player<s->player_count;player++) {
        int count, slot;
        if (player==actor || !rich4_special_event_active_player(s,player)) continue;
        count=rich4_card_inventory_count(s,player);
        if (count==0) continue;
        /* 0x44C3B7 -> 0x441E77 samples the packed raw inventory. Removal
         * is by card ID, so duplicate cards remove their first occurrence. */
        slot=rich4_special_event_special_random(s,count);
        (void)fortune_take_card(s,actor,player,slot);
    }
}

int rich4_special_event_fortune_has_target(int id)
{
    return id == 6 || id == 7 || id == 12 || id == 13 ||
           (id >= 33 && id <= 48);
}

/* 0x40D2D3 / 0x40D31C, called once before the AI card notice. */
int rich4_special_event_fortune_choose_redirect(GameState *s, int actor)
{
    int candidates[GAME_MAX_PLAYERS], count=0, best=0, redirect=-1, i;
    if (s==NULL || !rich4_special_event_active_player(s,actor)) return -1;
    for (i=0;i<s->player_count;i++) {
        if (i==actor || !rich4_special_event_active_player(s,i)) continue;
        if (s->players[actor].relation_balance[i]>best) {
            best=s->players[actor].relation_balance[i]; redirect=i;
        }
        if (rich4_special_event_active_unconfined_player(s,i)) candidates[count++]=i;
    }
    if (redirect<0 && count)
        redirect=candidates[rich4_special_event_special_random(s,count)];
    return redirect;
}

static int fortune_target(GameState *s, int actor, int redirect,
                          GameEventQueue *events, int target_chosen)
{
    int card=-1, victim=actor;
    if (rich4_card_inventory_has(s,actor,20)) {
        card=20; victim=-1;
    } else if (rich4_card_inventory_has(s,actor,18)) {
        if (!target_chosen && (s->players[actor].play_state & 0xff)>1)
            redirect=rich4_special_event_fortune_choose_redirect(s,actor);
        if (redirect!=-1) {
            if (redirect==actor || !rich4_special_event_active_player(s,redirect)) return -2;
            card=18; victim=redirect;
        }
    }
    if (card >= 0 &&
        (!rich4_card_consume(s,actor,card) ||
         !rich4_game_event_emit_typed_domain(events,
             rich4_game_event_card_used_from_fields, actor,
             rich4_game_domain_ref(GAME_DOMAIN_CARD,card),
             rich4_game_domain_ref(GAME_DOMAIN_PLAYER,victim < 0 ? actor : victim),
             victim, s->card_amount[card], 0))) return -2;
    return victim;
}

/* The app calls this after the notice/selection and before card speech.
 * A resolved target is then passed to commit without running 0x441210 again. */
int rich4_special_event_resolve_passive_target(GameState *s, int actor,
    int redirect, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int victim;
    if (s==NULL || events==NULL || s->special_event_phase!=GAME_SPECIAL_PHASE_PREPARED ||
        !rich4_special_event_active_player(s,actor)) return -2;
    if (s->special_event_family==GAME_SPECIAL_EVENT_FORTUNE) {
        if (actor!=s->special_event_actor ||
            !rich4_special_event_fortune_has_target(s->special_event_resolved_id)) return -2;
    } else if (s->special_event_family!=GAME_SPECIAL_EVENT_NEWS ||
        s->special_event_resolved_id!=29 || !(s->special_event_target_mask & (1u<<actor)))
        return -2;
    next=*s; rich4_game_event_queue_reset(&staged);
    victim=fortune_target(&next,actor,redirect,&staged,1);
    if (victim==-2 || events->count+staged.count>GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events,&staged)) return -2;
    *s=next;
    return victim;
}

/* 0x428E23: return shares to both volume words and truncate each stock's
 * proceeds separately. Mode 0 credits the lottery pool, mode 1 the bank. */
static int fortune_return_stock(GameState *s, int actor, int stock,
                                int quantity, int to_bank)
{
    double value=(double)quantity*s->stocks[stock].f20;
    int *account=to_bank ? &s->players[actor].bank : &s->lottery_pool;
    int proceeds;
    if (!(value>=0.0 && value<=(double)INT_MAX))
        return 0;
    proceeds=(int)value;
    if (*account>INT_MAX-proceeds) return 0;
    s->player_stock_amount[actor][stock]-=quantity;
    if (s->player_stock_amount[actor][stock]==0)
        s->player_stock_average_cost[actor][stock]=0.0f;
    s->stocks[stock].f8=(uint16_t)((uint32_t)s->stocks[stock].f8+(uint32_t)quantity);
    s->stocks[stock].f10=(uint16_t)((uint32_t)s->stocks[stock].f10+(uint32_t)quantity);
    *account+=proceeds;
    rich4_stock_refresh_company_ownership(s,stock,actor);
    return 1;
}

static int fortune_sell_stocks(GameState *s, int actor)
{
    int i;
    for (i=0;i<GAME_STOCK_COUNT;i++)
        if (s->player_stock_amount[actor][i] &&
            !fortune_return_stock(s,actor,i,s->player_stock_amount[actor][i],1))
            return 0;
    return 1;
}

static GameSpecialEventResult rich4_special_event_commit_impl(
    GameState *state, GameSpecialDefenseOutcome defense, GameEventQueue *events,
    int defer_confinement, int redirect_target, int target_resolved)
{
    GameState next;
    GameEventQueue staged;
    GameSpecialEventResult result=GAME_SPECIAL_EVENT_APPLIED;
    int family, actor, id, amount, i, victim;
    if (state==NULL || events==NULL || state->special_event_phase!=GAME_SPECIAL_PHASE_PREPARED ||
        defense<GAME_SPECIAL_DEFENSE_NONE || defense>GAME_SPECIAL_DEFENSE_DOUBLED)
        return GAME_SPECIAL_EVENT_INVALID;
    next=*state; family=next.special_event_family; actor=next.special_event_actor;
    id=next.special_event_resolved_id; amount=next.special_event_prepared_value;
    rich4_game_event_queue_reset(&staged);
    if (family==GAME_SPECIAL_EVENT_NEWS) {
        GameDomainRef property_target;
        if (defense!=GAME_SPECIAL_DEFENSE_NONE) return GAME_SPECIAL_EVENT_INVALID;
        if (id >= 8 && id <= 10) {
            /* 0x4498B3..0x449C7B: the saved winner is paid after the page. */
            victim = (int)((next.special_event_target_mask >> 24) & 15u) - 1;
            if (victim >= 0 && victim < next.player_count) {
                amount = next.special_event_player_amount[victim];
                if (!rich4_special_event_add_money(&next,victim,amount,0))
                    return GAME_SPECIAL_EVENT_INVALID;
            }
        } else if (id >= 11 && id <= 13) {
            /* 0x449C7C/0x449DE6/0x44A029 use the full saved tax, not a
             * balance cap. Stop the player-order loop on the ending latch. */
            for (i=0;i<next.player_count && !next.game_over;i++) {
                GameSpecialEventResult payment;
                if (!rich4_special_event_active_player(&next,i)) continue;
                payment = special_pay(&next,i,next.special_event_player_amount[i],&staged);
                if (payment == GAME_SPECIAL_EVENT_INVALID)
                    return GAME_SPECIAL_EVENT_INVALID;
                if (payment == GAME_SPECIAL_EVENT_BANKRUPTCY &&
                    !rich4_special_event_active_player(&next,actor))
                    result = payment; /* Route result concerns the trigger actor. */
            }
        }
        if (id==4 || id==20) {
            if (next.special_event_prepared_value != 1)
                return GAME_SPECIAL_EVENT_INVALID;
        }
        if (id==4) {
            unsigned int victims = next.special_event_target_mask &
                RICH4_PRESENTATION_PLAYER_MASK;
            for (i=0;i<next.player_count && i<4;i++)
                if (victims & (1u << (RICH4_PRESENTATION_PLAYER_SHIFT+i))) {
                    if (defer_confinement) rich4_confinement_injure_player(&next, i);
                    else rich4_special_event_return_active_vehicle(&next,i);
                    if ((defer_confinement ? rich4_confinement_request_enter : rich4_confinement_enter)(
                            &next,i,GAME_CONFINEMENT_HOSPITAL,3,
                            &staged)!=GAME_CONFINEMENT_APPLIED)
                        return GAME_SPECIAL_EVENT_INVALID;
                }
            for (i=0;i<GAME_SPECIAL_PLAYER_COUNT;i++)
                if (victims &
                        (1u << (RICH4_PRESENTATION_PLAYER_SHIFT+4+i)) &&
                    (defer_confinement ? rich4_confinement_request_enter : rich4_confinement_enter)(
                        &next,4+i,GAME_CONFINEMENT_HOSPITAL,3,
                        &staged)!=GAME_CONFINEMENT_APPLIED)
                    return GAME_SPECIAL_EVENT_INVALID;
        } else if ((id==6 || id==14 || id==18) &&
                   rich4_special_event_decode_property_target(
                       &next,next.special_event_target_mask,&property_target)) {
            if (id == 6 || id == 14)
                rich4_special_event_scale_property_price(
                    &next,property_target,id == 6 ? 13 : 7,10);
            else {
                int group = property_target.kind == GAME_DOMAIN_LAND ?
                    next.land_group[property_target.id] : -1;
                for (i=0;i<next.land_count;i++)
                    if (property_target.kind == GAME_DOMAIN_LAND &&
                        (i == property_target.id ||
                         (group >= 0 && next.land_group[i] == group)))
                        rich4_special_event_downgrade_property(&next,
                            rich4_game_domain_ref(GAME_DOMAIN_LAND,i));
                if (property_target.kind == GAME_DOMAIN_FACILITY)
                    rich4_special_event_downgrade_property(&next,property_target);
            }
        } else if ((id==5 || id==19) &&
                   rich4_special_event_decode_property_target(
                       &next,next.special_event_target_mask,
                       &property_target)) {
            rich4_special_event_clear_property(&next,property_target);
        } else if ((id==15 || id==21) &&
                   rich4_special_event_decode_property_target(
                       &next,next.special_event_target_mask,
                       &property_target)) {
            rich4_special_event_downgrade_property(&next,property_target);
        } else if (id==7 && next.special_event_target_mask!=0) {
            unsigned int encoded=next.special_event_target_mask;
            GameDomainRef target=rich4_game_domain_ref(
                (encoded&0x80000000u) ?
                    GAME_DOMAIN_FACILITY : GAME_DOMAIN_LAND,
                (int)((encoded&0x7fffffffu)-1u));
            GameEvent request=rich4_game_event_auction_request(
                actor,target,-1,1,GAME_AUCTION_TRIGGER_NEWS,0);
            if (!rich4_game_event_queue_push(&staged,&request))
                return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
        } else if (id==29) {
            unsigned int players = next.special_event_target_mask &
                ((1u << next.player_count) - 1u);
            int target_player = -1;
            for (i=0;i<next.player_count;i++)
                if (players & (1u << i)) {
                    target_player = i;
                    break;
                }
            if (target_player < 0) return GAME_SPECIAL_EVENT_INVALID;
            target_player=target_resolved ? redirect_target :
                fortune_target(&next,target_player,redirect_target,&staged,0);
            if (target_player < -1 || target_player>=next.player_count ||
                (target_player>=0 && !rich4_special_event_active_player(&next,target_player)))
                return GAME_SPECIAL_EVENT_INVALID;
            if (target_player == -1) {
                defense = GAME_SPECIAL_DEFENSE_BLOCKED;
            } else if (
                (defer_confinement ? rich4_confinement_request_enter : rich4_confinement_enter)(
                    &next,target_player,GAME_CONFINEMENT_PRISON,5,
                    &staged)!=GAME_CONFINEMENT_APPLIED)
                return GAME_SPECIAL_EVENT_INVALID;
        }
    } else if (family==GAME_SPECIAL_EVENT_FORTUNE) {
        unsigned int target=next.special_event_target_mask;
        if (defense==GAME_SPECIAL_DEFENSE_BLOCKED) amount=0;
        else if (defense==GAME_SPECIAL_DEFENSE_DOUBLED && id!=3 && id!=8) {
            if (amount>INT_MAX/2) return GAME_SPECIAL_EVENT_INVALID;
            amount*=2;
        }
        victim = actor;
        if (amount && rich4_special_event_fortune_has_target(id)) {
            victim = target_resolved ? redirect_target :
                fortune_target(&next,actor,redirect_target,&staged,0);
            if (victim < -1 || victim>=next.player_count ||
                (victim>=0 && victim!=actor &&
                 !rich4_special_event_active_player(&next,victim)))
                return GAME_SPECIAL_EVENT_INVALID;
            if (victim == -1) { amount = 0; defense = GAME_SPECIAL_DEFENSE_BLOCKED; }
        }
        switch (id) {
        case 0:
            if (target && target-1u<(unsigned int)next.land_count) {
                int land=(int)target-1, old=next.land_value[land];
                next.land_level[land]=0;
                next.land_type[land]=0;
                next.land_value[land]=next.land_price[land];
                rich4_special_event_update_fixed_asset(&next,actor,next.land_value[land]-old);
                if (!rich4_special_event_add_money(&next,actor,amount,0))
                    return GAME_SPECIAL_EVENT_INVALID;
            }
            break;
        case 1:
            if (target && target-1u<(unsigned int)next.land_count) {
                int land=(int)target-1, old=next.land_value[land];
                next.land_owner[land]=-1;
                next.land_value[land]=0;
                next.land_runtime_value[land]=0;
                rich4_special_event_update_fixed_asset(&next,actor,-old);
                if (!rich4_special_event_add_money(&next,actor,amount,0))
                    return GAME_SPECIAL_EVENT_INVALID;
            }
            break;
        case 2:
            if (amount) {
                /* 0x44C1FA adds to +36; 0x433B7E retains an existing due date. */
                next.players[actor].loan = (int)((uint32_t)next.players[actor].loan +
                                               (uint32_t)amount);
                if (next.players[actor].loan_due_date == 0)
                {
                    GameState due_day = next;
                    if (next.elapsed_days > INT_MAX - 90)
                        return GAME_SPECIAL_EVENT_INVALID;
                    due_day.elapsed_days += 90;
                    while (rich4_stock_calendar_is_closed(&due_day)) {
                        if (due_day.elapsed_days == INT_MAX)
                            return GAME_SPECIAL_EVENT_INVALID;
                        due_day.elapsed_days++;
                    }
                    next.players[actor].loan_due_date = due_day.elapsed_days;
                }
                if (!(defer_confinement ? rich4_insurance_request_claim(
                        &next, actor, amount, GAME_INSURANCE_CLAIM_FORTUNE, 0, &staged) :
                      rich4_insurance_claim(&next, actor, amount,
                        GAME_INSURANCE_CLAIM_FORTUNE, NULL, &staged)))
                    return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
            }
            break;
        case 3:
            if (defense!=GAME_SPECIAL_DEFENSE_BLOCKED) {
                amount=30;
                next.players[actor].days_rejected_by_bank=
                    (uint8_t)(next.players[actor].days_rejected_by_bank+30);
            }
            break;
        case 4:
            if (amount) {
                for (i=0;i<next.player_count;i++)
                    if (i!=actor && rich4_special_event_active_player(&next,i) &&
                        next.players[i].bank!=0) {
                        float ratio=(float)amount/100.0f;
                        int stolen=(int)((double)next.players[i].bank*ratio);
                        if (rich4_ledger_pay_fortune(&next,i,
                                rich4_ledger_player(actor),stolen,
                                GAME_PAYMENT_PAYER_BANK_FIRST,&staged)!=GAME_PAYMENT_COMMITTED)
                            return GAME_SPECIAL_EVENT_INVALID;
                    }
            }
            break;
        case 5:
            if ((next.players[actor].play_state & 0xff)>1)
                fortune_take_cards(&next,actor);
            break;
        case 6: case 7:
            if (amount) {
                if (rich4_confinement_disappear(&next, victim, amount, id == 7,
                        defer_confinement, &staged) != GAME_CONFINEMENT_APPLIED)
                    return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
            }
            break;
        case 8:
            if (amount)
                for (i=0;i<GAME_STOCK_COUNT;i++) {
                    float ratio=(float)amount/100.0f;
                    int lost=(int)((double)next.player_stock_amount[actor][i]*ratio);
                    if (!fortune_return_stock(&next,actor,i,lost,0))
                        return GAME_SPECIAL_EVENT_INVALID;
                }
            break;
        case 9:
            if (defense!=GAME_SPECIAL_DEFENSE_BLOCKED &&
                !fortune_sell_stocks(&next,actor))
                return GAME_SPECIAL_EVENT_INVALID;
            break;
        case 10: case 11:
            if (defense!=GAME_SPECIAL_DEFENSE_BLOCKED)
                rich4_special_event_return_active_vehicle(&next,actor);
            break;
        case 12: case 13:
            if (amount) {
                if (defer_confinement) rich4_confinement_injure_player(&next, victim);
                else rich4_special_event_return_active_vehicle(&next,victim);
                if ((defer_confinement ? rich4_confinement_request_enter : rich4_confinement_enter)(&next,victim,
                        GAME_CONFINEMENT_HOSPITAL,amount,&staged)!=
                        GAME_CONFINEMENT_APPLIED)
                    return GAME_SPECIAL_EVENT_INVALID;
            }
            break;
        case 14: case 15: case 16: case 17: case 18: case 19:
        case 23: case 24: case 26: case 30:
            result=special_pay(&next,actor,amount,&staged);
            if (result==GAME_SPECIAL_EVENT_INVALID)
                return result;
            if (amount && result==GAME_SPECIAL_EVENT_APPLIED &&
                !(defer_confinement ? rich4_insurance_request_claim(
                    &next, actor, amount, GAME_INSURANCE_CLAIM_FORTUNE, 0, &staged) :
                  rich4_insurance_claim(&next, actor, amount,
                    GAME_INSURANCE_CLAIM_FORTUNE, NULL, &staged)))
                return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
            break;
        case 20: case 21: case 22: case 25:
        case 27: case 28: case 29: case 31:
            if (amount && !rich4_special_event_add_money(&next,actor,amount,0))
                return GAME_SPECIAL_EVENT_INVALID;
            break;
        case 32:
            if (defense!=GAME_SPECIAL_DEFENSE_BLOCKED) {
                int points=rich4_special_event_magic_sell_tools(&next,actor);
                points+=rich4_special_event_magic_sell_cards(&next,actor);
                amount=points;
            }
            break;
        default:
            if (id>=33 && id<=48 && amount &&
                (defer_confinement ? rich4_confinement_request_enter : rich4_confinement_enter)(&next,victim,
                    GAME_CONFINEMENT_PRISON,amount,&staged)!=
                    GAME_CONFINEMENT_APPLIED)
                return GAME_SPECIAL_EVENT_INVALID;
            break;
        }
    } else if (family==GAME_SPECIAL_EVENT_MAGIC_HOUSE && id==4) {
        if (defense!=GAME_SPECIAL_DEFENSE_NONE) return GAME_SPECIAL_EVENT_INVALID;
        for (i=0;i<next.player_count;i++) if (next.special_event_target_mask & (1u<<i)) {
            if (next.players[i].cash<0 || next.players[i].bank>INT_MAX-next.players[i].cash)
                return GAME_SPECIAL_EVENT_INVALID;
            amount=next.players[i].cash;
            next.players[i].bank+=amount; next.players[i].cash=0;
            if (!rich4_game_event_money_transferred_emit(
                    &staged, i, 4, amount, 0, 0))
                return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
        }
    } else return GAME_SPECIAL_EVENT_INVALID;
    if (!rich4_game_event_special_event_committed_emit(
            &staged, actor, id, amount, defense, (unsigned int)family))
        return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
    clear_pending_event(&next);
    if (!rich4_game_event_queue_append(events,&staged)) return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
    *state=next; return result;
}

GameSpecialEventResult rich4_special_event_cancel(GameState *state,
                                                   GameEventQueue *events)
{
    GameState next;
    if (state==NULL || events==NULL || state->special_event_phase!=GAME_SPECIAL_PHASE_PREPARED)
        return GAME_SPECIAL_EVENT_INVALID;
    if (!state->special_event_cancelable) return GAME_SPECIAL_EVENT_NOT_CANCELABLE;
    next=*state;
    if (!rich4_game_event_special_event_canceled_emit(
            events, next.special_event_actor, next.special_event_resolved_id,
            0, 0, (unsigned int)next.special_event_family))
        return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
    clear_pending_event(&next); *state=next; return GAME_SPECIAL_EVENT_CANCELED;
}

int rich4_special_event_snapshot_export(const GameState *s,
                                        GameSpecialEventSnapshot *out)
{
    if (s==NULL || out==NULL) return 0;
    memset(out,0,sizeof(*out)); out->version=GAME_SPECIAL_EVENT_SNAPSHOT_VERSION;
    memcpy(out->news_order,s->news_event_order,sizeof(out->news_order));
    memcpy(out->fortune_order,s->fortune_event_order,sizeof(out->fortune_order));
    for (int i=0;i<GAME_STOCK_COUNT;i++) out->stock_f7[i]=s->stocks[i].f7;
    out->news_cursor=s->news_event_cursor; out->fortune_cursor=s->fortune_event_cursor;
    out->rng_state=s->ai_rng_state; out->family=s->special_event_family;
    out->actor=s->special_event_actor; out->logical_id=s->special_event_logical_id;
    out->resolved_id=s->special_event_resolved_id; out->resource_id=s->special_event_resource_id;
    out->panel_id=s->special_event_panel_id; out->prepared_value=s->special_event_prepared_value;
    out->target_mask=s->special_event_target_mask; out->cancelable=s->special_event_cancelable;
    memcpy(out->player_amount, s->special_event_player_amount,
           sizeof(out->player_amount));
    out->phase=s->special_event_phase; return 1;
}

static int permutation(const uint8_t *order, int count)
{
    uint8_t seen[GAME_FORTUNE_EVENT_COUNT]={0}; int i;
    for (i=0;i<count;i++) { if (order[i]>=count || seen[order[i]]) return 0; seen[order[i]]=1; }
    return 1;
}

static int valid_target_mask(const GameState *state, int family,
                             unsigned int target_mask)
{
    unsigned int player_mask;

    if (family == GAME_SPECIAL_EVENT_NONE)
        return target_mask == 0;
    if (family == GAME_SPECIAL_EVENT_NEWS ||
        family == GAME_SPECIAL_EVENT_FORTUNE) {
        /* News/fortune presentation targets pack a property id into the
         * low word, player/special-actor rows into bits 16..23, the saved
         * owner into bits 24..27, and the facility tag into bit 31. */
        const unsigned int presentation_mask =
            RICH4_PRESENTATION_TARGET_ID_MASK |
            RICH4_PRESENTATION_PLAYER_MASK |
            0x0f000000u | RICH4_PRESENTATION_FACILITY_BIT;
        return (target_mask & ~presentation_mask) == 0;
    }
    if (family != GAME_SPECIAL_EVENT_MAGIC_HOUSE ||
        state->player_count < 0 || state->player_count >= 32)
        return 0;
    player_mask = (1u << state->player_count) - 1u;
    return (target_mask & ~player_mask) == 0;
}

int rich4_special_event_snapshot_import(GameState *s,
                                        const GameSpecialEventSnapshot *in)
{
    GameState next;
    if (s==NULL || in==NULL || in->version!=GAME_SPECIAL_EVENT_SNAPSHOT_VERSION ||
        !permutation(in->news_order,GAME_NEWS_EVENT_COUNT) ||
        !permutation(in->fortune_order,GAME_FORTUNE_EVENT_COUNT) ||
        in->news_cursor<0 || in->news_cursor>=GAME_NEWS_EVENT_COUNT ||
        in->fortune_cursor<0 || in->fortune_cursor>=GAME_FORTUNE_EVENT_COUNT ||
        in->family<GAME_SPECIAL_EVENT_NONE || in->family>GAME_SPECIAL_EVENT_MAGIC_HOUSE ||
        in->phase<GAME_SPECIAL_PHASE_IDLE || in->phase>GAME_SPECIAL_PHASE_DEFERRED ||
        ((in->phase==GAME_SPECIAL_PHASE_IDLE) != (in->family==GAME_SPECIAL_EVENT_NONE)) ||
        in->actor<-1 || in->actor>=s->player_count || in->cancelable<0 || in->cancelable>1 ||
        !valid_target_mask(s, in->family, in->target_mask)) return 0;
    next=*s; memcpy(next.news_event_order,in->news_order,sizeof(in->news_order));
    memcpy(next.fortune_event_order,in->fortune_order,sizeof(in->fortune_order));
    for (int i=0;i<GAME_STOCK_COUNT;i++) next.stocks[i].f7=in->stock_f7[i];
    next.news_event_cursor=in->news_cursor; next.fortune_event_cursor=in->fortune_cursor;
    next.ai_rng_state=in->rng_state; next.special_event_family=in->family;
    next.special_event_actor=in->actor; next.special_event_logical_id=in->logical_id;
    next.special_event_resolved_id=in->resolved_id; next.special_event_resource_id=in->resource_id;
    next.special_event_panel_id=in->panel_id; next.special_event_prepared_value=in->prepared_value;
    next.special_event_target_mask=in->target_mask; next.special_event_cancelable=in->cancelable;
    memcpy(next.special_event_player_amount, in->player_amount,
           sizeof(next.special_event_player_amount));
    next.special_event_phase=in->phase; *s=next; return 1;
}

GameSpecialEventResult rich4_special_event_commit(GameState *state, GameSpecialDefenseOutcome defense, GameEventQueue *events)
{
    return rich4_special_event_commit_impl(state, defense, events, 0, -1, 0);
}

GameSpecialEventResult rich4_special_event_commit_deferred_confinement(GameState *state, GameSpecialDefenseOutcome defense, GameEventQueue *events)
{
    return rich4_special_event_commit_impl(state, defense, events, 1, -1, 0);
}

GameSpecialEventResult rich4_special_event_commit_with_target(
    GameState *state, GameSpecialDefenseOutcome defense, int redirect_target,
    GameEventQueue *events, int defer_confinement)
{
    return rich4_special_event_commit_impl(
        state,defense,events,defer_confinement,redirect_target,0);
}

GameSpecialEventResult rich4_special_event_commit_resolved_target(
    GameState *state, GameSpecialDefenseOutcome defense, int target,
    GameEventQueue *events, int defer_confinement)
{
    return rich4_special_event_commit_impl(state,defense,events,defer_confinement,target,1);
}
