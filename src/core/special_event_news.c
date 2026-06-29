#include "core/special_event_internal.h"
#include "core/confinement.h"
#include "core/card.h"
#include "core/insurance.h"
#include "core/ledger.h"
#include "core/stock.h"
#include "core/tool.h"
#include "core/victory.h"
#include "core/game_event_payload.h"

#include <limits.h>
#include <string.h>

static const uint16_t fortune_resource[49] = {
    0x1dd,0x1de,0x1df,0x1e0,0x1e1,0x1e2,0x1e3,0x1e4,
    0x1e5,0x1e6,0x1e7,0x1e8,0x1e9,0x1ea,0x1eb,0x1ec,
    0x1ed,0x1ee,0x1ef,0x1f0,0x1f1,0x1f1,0x1f1,0x1f2,
    0x1f2,0x1f3,0x1f4,0x1f5,0x1f5,0x1f5,0x1f6,0x1f7,
    0x1f8,0x1f9,0x1fa,0x1fb,0x1fc,0x1f9,0x1fd,0x1fe,
    0x1ff,0x200,0x1fa,0x1fb,0x201,0x202,0x203,0x1fe,0x204
};

static int news_prepare_effect(GameState *s, int actor, int id)
{
    GameDomainRef target;
    int i, chosen, amount;
    (void)actor;
    memset(s->special_event_player_amount, 0,
           sizeof(s->special_event_player_amount));
    switch (id) {
    case 0:
        for (i = 0; i < s->player_count; i++)
            if (rich4_special_event_news_confinement_target(s, i, 1)) {
                s->players[i].days_in_prison = 0x80;
                s->players[i].confinement_actor_type = GAME_CONFINEMENT_NONE;
                s->special_event_target_mask |= 1u << (RICH4_PRESENTATION_PLAYER_SHIFT+i);
            }
        break;
    case 1:
        for (i = 0; i < s->player_count; i++)
            if (rich4_special_event_news_confinement_target(s, i, 1)) {
                s->players[i].days_in_prison =
                    (s->players[i].days_in_prison + 3) & 0x7f;
                s->special_event_target_mask |= 1u << (RICH4_PRESENTATION_PLAYER_SHIFT+i);
            }
        break;
    case 2:
        for (i = 0; i < s->player_count; i++)
            if (rich4_special_event_news_confinement_target(s, i, 0)) {
                s->players[i].days_in_hospital = 0x80;
                s->players[i].confinement_actor_type = GAME_CONFINEMENT_NONE;
                s->special_event_target_mask |= 1u << (RICH4_PRESENTATION_PLAYER_SHIFT+i);
            }
        break;
    case 3:
        for (i = 0; i < s->player_count; i++)
            if (rich4_special_event_news_confinement_target(s, i, 0)) {
                s->players[i].days_in_hospital =
                    (s->players[i].days_in_hospital + 3) & 0x7f;
                s->special_event_target_mask |= 1u << (RICH4_PRESENTATION_PLAYER_SHIFT+i);
            }
        break;
    case 4:
        /* 0x449175..0x4491DD selects the center in callback phase 1. */
        break;
    case 5:
        if (!rich4_special_event_choose_property(s, 0, 1, 0, &target)) return 0;
        s->special_event_target_mask =
            rich4_special_event_presentation_property_target(s, target);
        break;
    case 6:
        if (!rich4_special_event_choose_property(s, 0, 0, 0, &target)) return 0;
        s->special_event_target_mask =
            rich4_special_event_presentation_property_target(s, target);
        break;
    case 7:
        /*
         * The selected unowned lot is handed to the shared auction flow by
         * the route layer.  Preserve its domain in target_mask.
         */
        if (!rich4_special_event_choose_property(s, 1, 0, 0, &target)) return 0;
        s->special_event_target_mask =
            (unsigned int)(target.id + 1) |
            (target.kind == GAME_DOMAIN_FACILITY ? 0x80000000u : 0u);
        break;
    case 8:
        chosen = rich4_special_event_extreme_player(s, 1, 1);
        if (chosen >= 0) {
            s->special_event_target_mask =
                (unsigned int)(chosen + 1) << 24;
            s->special_event_player_amount[chosen] =
                (int32_t)((uint32_t)s->price_index * 10000u);
        }
        break;
    case 9:
        chosen = rich4_special_event_extreme_player(s, 1, 0);
        if (chosen >= 0) {
            s->special_event_target_mask =
                (unsigned int)(chosen + 1) << 24;
            s->special_event_player_amount[chosen] =
                (int32_t)((uint32_t)s->price_index * 5000u);
        }
        break;
    case 10:
        chosen = rich4_special_event_extreme_player(s, 0, 1);
        if (chosen >= 0) {
            s->special_event_target_mask =
                (unsigned int)(chosen + 1) << 24;
            s->special_event_player_amount[chosen] =
                (int32_t)((uint32_t)s->price_index * 10000u);
        }
        break;
    case 11: case 12: case 13:
        /* 0x449C7C/0x449DE6/0x44A029 save the full assessment in phase 0.
         * 0x41D2C6 settles it only in phase 1, including bankruptcy. */
        for (chosen = 0; chosen < s->player_count; chosen++)
            if (rich4_special_event_active_player(s, chosen)) {
                if (id == 11) {
                    amount = rich4_special_event_original_scale(
                        s->players[chosen].cash, 5, 100);
                } else if (id == 12) {
                    uint32_t base = 0;
                    for (i = 0; i < s->land_count; i++)
                        if (s->land_owner[i] == chosen)
                            base += (uint16_t)s->land_price[i] +
                                (uint32_t)(uint16_t)s->land_price_per_level[i] *
                                (uint8_t)s->land_level[i];
                    for (i = 0; i < s->facility_count; i++)
                        if (s->facility_owner[i] == chosen)
                            base += (uint16_t)s->facility_price[i] +
                                (uint32_t)(uint16_t)s->facility_price_per_level[i] *
                                (uint8_t)s->facility_level[i];
                    amount = (int32_t)((uint32_t)s->price_index *
                        (uint32_t)rich4_special_event_original_scale((int32_t)base, 5, 100));
                } else {
                    float value = 0.0f;
                    for (i = 0; i < GAME_STOCK_COUNT; i++)
                        if (s->player_stock_amount[chosen][i] != 0)
                            value = (float)((double)s->player_stock_amount[chosen][i] *
                                (double)s->stocks[i].f20 + (double)value);
                    amount = (int32_t)((uint32_t)s->price_index *
                        (uint32_t)rich4_special_event_original_stock_tax(value));
                }
                s->special_event_player_amount[chosen] = amount;
                if (amount != 0)
                    s->special_event_target_mask |=
                        1u << (RICH4_PRESENTATION_PLAYER_SHIFT + chosen);
            }
        break;
    case 14:
        if (!rich4_special_event_choose_property(s, 0, 0, 0, &target)) return 0;
        s->special_event_target_mask =
            rich4_special_event_presentation_property_target(s, target);
        break;
    case 15:
        if (!rich4_special_event_choose_property(s, 0, 1, 1, &target)) return 0;
        s->special_event_target_mask =
            rich4_special_event_presentation_property_target(s, target);
        break;
    case 16:
        for (i = 0; i < s->player_count; i++)
            if (rich4_special_event_active_player(s, i) && s->players[i].traffic_method == 0)
                s->players[i].days_stopping = 1;
        break;
    case 17:
        for (i = 0; i < s->player_count; i++)
            if (rich4_special_event_active_player(s, i) && s->players[i].traffic_method != 0)
                s->players[i].days_stopping = 1;
        break;
    case 18:
        if (!rich4_special_event_choose_property(s, 0, 0, 0, &target)) return 0;
        s->special_event_target_mask =
            rich4_special_event_presentation_property_target(s, target);
        break;
    case 19:
        if (!rich4_special_event_choose_property(s, 0, 0, 0, &target)) return 0;
        s->special_event_target_mask =
            rich4_special_event_presentation_property_target(s, target);
        break;
    case 20:
        /* 0x44AB2C saves a random land/facility for Data[534]'s phase 1. */
        if (!rich4_special_event_choose_property(s, 0, 0, 0, &target)) return 0;
        s->special_event_target_mask =
            rich4_special_event_presentation_property_target(s, target);
        break;
    case 21:
        if (!rich4_special_event_choose_property(s, 0, 0, 0, &target)) return 0;
        s->special_event_target_mask =
            rich4_special_event_presentation_property_target(s, target);
        break;
    case 22:
        for (i = 0; i < s->player_count; i++)
            if (rich4_special_event_active_player(s, i)) s->players[i].days_bank_no_loans = 15;
        break;
    case 23:
        for (i = 0; i < s->player_count; i++)
            if (rich4_special_event_active_player(s, i) &&
                s->players[i].loan == 0) {
                amount = s->players[i].bank / 10;
                s->special_event_player_amount[i] = amount;
                if (!rich4_special_event_add_money(s, i, amount, 1))
                    return 0;
            }
        break;
    case 24: case 25:
        for (i = 0; i < GAME_STOCK_COUNT; i++)
        {
            s->stocks[i].f7 = (uint8_t)(id == 24 ? 1 : 16);
            (void)rich4_stock_apply_forced_quote(s, i); /* 0x429040(0). */
        }
        break;
    case 26:
        s->stock_market_closed = 10;
        break;
    case 27:
        i = rich4_special_event_special_random(s, GAME_STOCK_COUNT);
        s->stocks[i].f6 = 15;
        /* 0x44B0D1 restores the reference quote and overwrites history. */
        s->stocks[i].f20 = s->stocks[i].f16;
        s->stock_history[i][s->stock_history_cursor > 0 ?
            s->stock_history_cursor - 1 : GAME_STOCK_HISTORY_COUNT - 1] = s->stocks[i].f20;
        s->special_event_target_mask = (unsigned int)(i + 1);
        break;
    case 28: {
        int candidates[GAME_STOCK_COUNT], count = 0;
        for (i = 0; i < GAME_STOCK_COUNT; i++)
            if (s->stocks[i].f6) candidates[count++] = i;
        if (count == 0) return 0;
        i = candidates[rich4_special_event_special_random(s, count)];
        s->stocks[i].f6 = 0;
        s->special_event_target_mask = (unsigned int)(i + 1);
        break;
    }
    case 29: {
        int candidates[GAME_MAX_MAP_PROPERTIES], count = 0;
        for (i = 0; i < s->company_count; i++) {
            int owner = rich4_special_event_company_operator(s, i);
            /* 0x44B285 only tests company +0x18, unlike 0x448BE2. */
            if (owner >= 0 && owner < s->player_count)
                candidates[count++] = i;
        }
        if (count == 0) return 0;
        i = candidates[rich4_special_event_special_random(s, count)];
        chosen = rich4_special_event_company_operator(s, i);
        /* sub_44B25B saves the selected operator for phase 1; it does not
         * detach or destroy the company record.  Preserve the high-byte
         * owner snapshot used by the presentation layer as well. */
        s->special_event_target_mask =
            (1u << chosen) | ((unsigned int)(i + 1) << 8) |
            ((unsigned int)(chosen + 1) << 24);
        break;
    }
    case 30: case 31: case 32: case 33: case 34: case 35: {
        int candidates[GAME_MAX_MAP_PROPERTIES], count = 0, delta = 0;
        for (i = 0; i < s->company_count; i++) {
            if (id != 35 || (int32_t)s->company_period_business[i] > 10000)
                candidates[count++] = i;
        }
        if (count == 0) return 0;
        chosen = candidates[rich4_special_event_special_random(s, count)];
        if (id == 30) delta = -10000;
        else if (id == 31) delta = 20000;
        else if (id == 32) delta = -20000;
        else if (id == 33) delta = -10000;
        else if (id == 34) delta = -5000;
        if (id == 35) {
            uint32_t old = s->company_period_business[chosen];
            s->company_period_business[chosen] = old * 2u;
            s->company_total_business[chosen] += old * 2u;
            i = s->company_stock_index[chosen];
            if (i >= 0 && i < GAME_STOCK_COUNT)
                s->stocks[i].f7 = (uint8_t)(16u * (old / 10000u));
        } else {
            s->company_period_business[chosen] += (uint32_t)delta;
            s->company_total_business[chosen] += (uint32_t)delta;
            i = s->company_stock_index[chosen];
            if (i >= 0 && i < GAME_STOCK_COUNT)
                s->stocks[i].f7 =
                    (uint8_t)(delta > 0 ?
                        (delta >= 20000 ? 48 : 16) :
                        (delta <= -20000 ? 4 : 3));
        }
        if (i >= 0 && i < GAME_STOCK_COUNT)
            (void)rich4_stock_apply_forced_quote(s, i); /* 0x429040(stock+1). */
        s->special_event_target_mask = (unsigned int)(chosen + 1);
        break;
    }
    default:
        return 0;
    }
    return 1;
}

int rich4_special_event_emit_prepared(GameEventQueue *q, const GameState *s)
{
    return rich4_game_event_special_event_prepared_emit(
               q, s->special_event_actor, s->special_event_logical_id,
               s->special_event_resolved_id, s->special_event_resource_id,
               (unsigned int)s->special_event_family) &&
           rich4_game_event_special_event_presentation_requested_emit(
               q, s->special_event_actor, s->special_event_resource_id,
               s->special_event_panel_id, s->special_event_cancelable,
               (unsigned int)s->special_event_family);
}

void rich4_special_event_set_pending(GameState *s, int family, int actor, int logical,
                        int resolved, int resource, int panel, int value,
                        unsigned int targets, int cancelable, int phase)
{
    s->special_event_family=family; s->special_event_actor=actor;
    s->special_event_logical_id=logical; s->special_event_resolved_id=resolved;
    s->special_event_resource_id=resource; s->special_event_panel_id=panel;
    s->special_event_prepared_value=value; s->special_event_target_mask=targets;
    s->special_event_cancelable=cancelable; s->special_event_phase=phase;
}

GameSpecialEventResult rich4_special_event_begin_news(
    GameState *state, int actor, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    uint8_t eligible[GAME_NEWS_EVENT_COUNT];
    int i, id;
    if (state==NULL || events==NULL || !rich4_special_event_active_player(state,actor) ||
        state->special_event_phase==GAME_SPECIAL_PHASE_PREPARED) return GAME_SPECIAL_EVENT_INVALID;
    next=*state;
    for (i=0;i<GAME_NEWS_EVENT_COUNT;i++) eligible[i]=(uint8_t)rich4_special_event_news_eligible(&next,actor,i);
    if (!rich4_special_event_select(next.news_event_order,GAME_NEWS_EVENT_COUNT,
                                    &next.news_event_cursor,eligible,&id)) {
        *state=next; return GAME_SPECIAL_EVENT_NO_CANDIDATE;
    }
    rich4_game_event_queue_reset(&staged);
    rich4_special_event_set_pending(&next,GAME_SPECIAL_EVENT_NEWS,actor,id,id,441+id,66,
                0,0,0,GAME_SPECIAL_PHASE_PREPARED);
    if (!news_prepare_effect(&next,actor,id))
        return GAME_SPECIAL_EVENT_INVALID;
    rich4_special_event_set_pending(&next,GAME_SPECIAL_EVENT_NEWS,actor,id,id,441+id,66,
                id==24?1:(id==25?16:0),next.special_event_target_mask,0,
                GAME_SPECIAL_PHASE_PREPARED);
    if (((id==24 || id==25 || id>=26) &&
         !rich4_game_event_market_changed_emit(
             &staged, actor, id, next.special_event_prepared_value, 0, 0)) ||
        !rich4_special_event_emit_prepared(&staged,&next) ||
        !rich4_game_event_queue_append(events,&staged)) return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
    *state=next; return GAME_SPECIAL_EVENT_PREPARED;
}

GameSpecialEventResult rich4_special_event_begin_fortune(
    GameState *state, int actor, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    uint8_t eligible[GAME_FORTUNE_EVENT_COUNT];
    int resolved[GAME_FORTUNE_EVENT_COUNT], i, logical, physical, value=0;
    unsigned int target = 0;
    /* 0x431DBC completes all three 0x44DB81 calls even if an earlier
     * fortune eliminated this target. The callback has no play-state gate. */
    if (state==NULL || events==NULL || actor<0 || actor>=state->player_count || state->map<0 || state->map>3 ||
        state->special_event_phase==GAME_SPECIAL_PHASE_PREPARED) return GAME_SPECIAL_EVENT_INVALID;
    next=*state;
    for (i=0;i<GAME_FORTUNE_EVENT_COUNT;i++)
        eligible[i]=(uint8_t)rich4_special_event_fortune_eligible(&next,actor,i,&resolved[i]);
    if (!rich4_special_event_select(next.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,
                                    &next.fortune_event_cursor,eligible,&logical)) {
        *state=next; return GAME_SPECIAL_EVENT_NO_CANDIDATE;
    }
    physical=resolved[logical];
    if (physical>=33) physical += next.map*4;
    if (physical==0 || physical==1) {
        int choices[GAME_MAX_MAP_PROPERTIES], count=0;
        for (i=0;i<next.land_count;i++)
            if (next.land_owner[i]==actor &&
                ((physical==0 && next.land_level[i]!=0) ||
                 (physical==1 && next.land_level[i]==0)))
                choices[count++]=i;
        if (count==0) return GAME_SPECIAL_EVENT_INVALID;
        i=choices[rich4_special_event_special_random(&next,count)];
        target=(unsigned int)(i+1);
        value=physical==0
            ? next.land_level[i]*next.land_price_per_level[i]
            : next.land_price[i];
    } else if (physical==2) value=next.price_index*10000;
    else if (physical==3) value=30;
    else if (physical==4 || physical==8) value=10;
    else if (physical==6 || physical==7 ||
             physical==12 || physical==13) value=3;
    else if (physical>=14 && physical<=16) value=next.price_index*3000;
    else if (physical==17) value=next.price_index*6000;
    else if (physical==18) value=next.price_index*600;
    else if (physical==19) value=next.price_index*1500;
    else if (physical==20) value=next.price_index*1000;
    else if (physical==21) value=next.price_index*2000;
    else if (physical==22) value=next.price_index*3000;
    else if (physical==23) value=next.price_index*1000;
    else if (physical==24) value=next.price_index*2000;
    else if (physical==25) value=next.price_index*10000;
    else if (physical==26) value=next.price_index*8000;
    else if (physical==27) value=next.price_index*4000;
    else if (physical==28) value=next.price_index*6000;
    else if (physical==29) value=next.price_index*8000;
    else if (physical==30 || physical==31)
        value=next.price_index*5000;
    else if (physical>=33) value=3+2*((physical-33)&3);
    rich4_special_event_set_pending(&next,GAME_SPECIAL_EVENT_FORTUNE,actor,logical,physical,
                fortune_resource[physical],66,value,target,0,
                GAME_SPECIAL_PHASE_PREPARED);
    rich4_game_event_queue_reset(&staged);
    if (!rich4_special_event_emit_prepared(&staged,&next) || !rich4_game_event_queue_append(events,&staged))
        return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
    *state=next; return GAME_SPECIAL_EVENT_PREPARED;
}
