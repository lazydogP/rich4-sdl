#include "core/special_event.h"
#include "core/special_event_internal.h"

#include <limits.h>
#include <string.h>

#include "core/confinement.h"
#include "core/card.h"
#include "core/insurance.h"
#include "core/ledger.h"
#include "core/stock.h"
#include "core/tool.h"
#include "core/tool_internal.h"
#include "core/victory.h"
#include "core/game_event_payload.h"

#define active_player rich4_special_event_active_player
#define active_unconfined_player rich4_special_event_active_unconfined_player
#define return_active_vehicle rich4_special_event_return_active_vehicle
#define special_random rich4_special_event_special_random
#define choose_property rich4_special_event_choose_property
#define update_fixed_asset rich4_special_event_update_fixed_asset
#define clear_property rich4_special_event_clear_property
#define downgrade_property rich4_special_event_downgrade_property
#define presentation_property_target rich4_special_event_presentation_property_target
#define scale_property_price rich4_special_event_scale_property_price
#define extreme_player rich4_special_event_extreme_player
#define add_money rich4_special_event_add_money
#define news_eligible rich4_special_event_news_eligible
#define fortune_eligible rich4_special_event_fortune_eligible
#define emit_prepared rich4_special_event_emit_prepared
#define set_pending rich4_special_event_set_pending
#define magic_sell_cards rich4_special_event_magic_sell_cards
#define magic_sell_tools rich4_special_event_magic_sell_tools

#define PRESENTATION_TARGET_ID_MASK 0x0000ffffu
#define PRESENTATION_PLAYER_SHIFT 16
#define PRESENTATION_PLAYER_MASK 0x00ff0000u
#define PRESENTATION_FACILITY_BIT 0x80000000u

int active_player(const GameState *state, int player)
{
    return player >= 0 && player < state->player_count &&
           (state->players[player].play_state & 0xff) != 0;
}

/* sub_40D73F accepts a company operator only when player +50..+53 are
 * all clear.  News 29 uses this predicate before selecting its callback. */
int active_unconfined_player(const GameState *state, int player)
{
    const GamePlayerState *p;
    if (!active_player(state, player)) return 0;
    p = &state->players[player];
    return p->delayed_status_block == 0 &&
           p->days_disappearing == 0 &&
           p->days_in_prison == 0 &&
           p->days_in_hospital == 0;
}

/* sub_40CD07 and the vehicle-loss fortune callbacks return an active
 * motorcycle/car to globals 0x497324/0x497325 before forcing walking. */
void return_active_vehicle(GameState *state, int player)
{
    GamePlayerState *p = &state->players[player];
    int method = p->traffic_method & 3;
    if (method == 1)
        state->tool_amount[4]++;
    else if (method == 2)
        state->tool_amount[5]++;
    p->traffic_method = 0;
    p->ndices = 1;
}

static void shuffle(uint8_t *order, int count, Rich4Rng *rng)
{
    uint8_t used[GAME_FORTUNE_EVENT_COUNT] = {0};
    int remaining, i;
    for (remaining = count; remaining > 0; --remaining) {
        int nth = rich4_rng_state_range(rng, remaining);
        int chosen = 0;
        for (i = 0; i < count; ++i) {
            if (used[i]) continue;
            if (nth-- == 0) { chosen = i; break; }
        }
        order[count - remaining] = (uint8_t)chosen;
        used[chosen] = 1;
    }
}

void rich4_special_event_sequences_init(GameState *state, Rich4Rng *rng)
{
    if (state == NULL || rng == NULL) return;
    shuffle(state->news_event_order, GAME_NEWS_EVENT_COUNT, rng);
    shuffle(state->fortune_event_order, GAME_FORTUNE_EVENT_COUNT, rng);
    state->news_event_cursor = 0;
    state->fortune_event_cursor = 0;
    state->ai_rng_state = rng->state;
    state->special_event_family = GAME_SPECIAL_EVENT_NONE;
    state->special_event_actor = -1;
    state->special_event_logical_id = -1;
    state->special_event_resolved_id = -1;
    state->special_event_resource_id = -1;
    state->special_event_panel_id = -1;
    state->special_event_prepared_value = 0;
    state->special_event_target_mask = 0;
    memset(state->special_event_player_amount, 0,
           sizeof(state->special_event_player_amount));
    state->special_event_cancelable = 0;
    state->special_event_phase = GAME_SPECIAL_PHASE_IDLE;
}

int rich4_special_event_select(const uint8_t *order, int count, int *cursor,
                               const uint8_t *eligible, int *out_id)
{
    int scanned;
    if (order == NULL || cursor == NULL || eligible == NULL || out_id == NULL ||
        count <= 0 || *cursor < 0 || *cursor >= count) return 0;
    for (scanned = 0; scanned < count; ++scanned) {
        int id = order[*cursor];
        *cursor = (*cursor + 1) % count;
        if (id >= 0 && id < count && eligible[id]) {
            *out_id = id;
            return 1;
        }
    }
    return 0;
}

static int any_property(const GameState *s, int owner_mode)
{
    int i;
    for (i=0;i<s->land_count;i++)
        if ((owner_mode==0) || (owner_mode==1 && s->land_owner[i]<0) ||
            (owner_mode==2 && s->land_owner[i]>=0)) return 1;
    for (i=0;i<s->facility_count;i++)
        if ((owner_mode==0) || (owner_mode==1 && s->facility_owner[i]<0) ||
            (owner_mode==2 && s->facility_owner[i]>=0)) return 1;
    return 0;
}

static int any_built_land(const GameState *s)
{
    int i;
    for (i = 0; i < s->land_count; i++)
        if (s->land_level[i] != 0) return 1;
    return 0;
}

static int any_built_property(const GameState *s)
{
    int i;
    if (any_built_land(s)) return 1;
    for (i = 0; i < s->facility_count; i++)
        if (s->facility_level[i] != 0) return 1;
    return 0;
}

static int any_status(const GameState *s, int prison)
{
    int i;
    for (i=0;i<s->player_count;i++)
        if (rich4_special_event_news_confinement_target(s, i, prison))
            return 1;
    return 0;
}

int rich4_special_event_news_confinement_target(
    const GameState *s, int player, int prison)
{
    const GamePlayerState *p = &s->players[player];
    int days = prison ? p->days_in_prison : p->days_in_hospital;
    int kind = prison ? GAME_CONFINEMENT_PRISON : GAME_CONFINEMENT_HOSPITAL;
    /* 0x496B30/0x496B60 are occupancy bytes, not remaining-day masks.
     * A natural pending release still occupies the building. News 0/2
     * clear occupancy immediately while retaining the 0x80 day marker. */
    return (days & 0x7f) != 0 ||
        ((days & 0x80) != 0 && p->confinement_actor_type == kind);
}

int rich4_special_event_company_operator(const GameState *s, int company)
{
    int stock = s->company_stock_index[company];
    /* Linked stocks maintain the live chairman; unlinked companies still
     * have their original +0x18 controller byte and remain news candidates. */
    if (stock >= 0 && stock < GAME_STOCK_COUNT &&
        s->stock_company_auxiliary[stock] == company)
        return s->stock_company_owner[stock];
    return s->company_owner[company];
}

int news_eligible(const GameState *s, int actor, int id)
{
    int i, j;
    (void)actor;
    switch (id) {
    case 0: case 1: return any_status(s, 1);
    case 2: case 3: return any_status(s, 0);
    case 4: case 5: return any_built_property(s);
    /* Original 0x448BE2(15) also accepts facilities, but 0x44A453
     * collects only built land and divides by that count. SDL deliberately
     * requires the same land-only pool at eligibility to avoid this bug. */
    case 15: return any_built_land(s);
    case 7: return any_property(s, 1);
    case 8: case 9: case 12: return any_property(s, 2);
    case 10: case 13:
        for (i=0;i<s->player_count;i++) if (active_player(s,i))
            for (j=0;j<GAME_STOCK_COUNT;j++) if (s->player_stock_amount[i][j]) return 1;
        return 0;
    case 16:
        for (i=0;i<s->player_count;i++)
            if (active_player(s,i) && s->players[i].traffic_method == 0)
                return 1;
        return 0;
    case 17:
        for (i=0;i<s->player_count;i++)
            if (active_player(s,i) && s->players[i].traffic_method != 0)
                return 1;
        return 0;
    case 28:
        for (i=0;i<GAME_STOCK_COUNT;i++) if (s->stocks[i].f6) return 1;
        return 0;
    case 29:
        for (i=0;i<s->company_count;i++)
            if (active_unconfined_player(s,
                    rich4_special_event_company_operator(s,i))) return 1;
        return 0;
    case 35:
        for (i=0;i<s->company_count;i++)
            if ((int32_t)s->company_period_business[i] > 10000) return 1;
        return 0;
    default: return 1;
    }
}

int fortune_eligible(const GameState *s, int actor, int logical,
                            int *resolved)
{
    int i;
    *resolved = logical;
    switch (logical) {
    case 0: case 1:
        for (i=0;i<s->land_count;i++) if (s->land_owner[i]==actor &&
            ((logical==0 && s->land_level[i]!=0) ||
             (logical==1 && s->land_level[i]==0))) return 1;
        return 0;
    case 5:
        for (i=0;i<s->player_count;i++) if (i!=actor) {
            int j; for (j=0;j<GAME_PLAYER_CARD_SLOTS;j++)
                if (s->players[i].card_inventory[j]) return 1;
        }
        return 0;
    case 8: case 9:
        for (i=0;i<GAME_STOCK_COUNT;i++) if (s->player_stock_amount[actor][i]) return 1;
        return 0;
    case 10: case 11:
        if (s->players[actor].traffic_method!=1 && s->players[actor].traffic_method!=2) return 0;
        *resolved = s->players[actor].traffic_method==1 ? 10 : 11; return 1;
    case 12: case 13:
        if (s->players[actor].traffic_method!=0 && s->players[actor].traffic_method!=1) return 0;
        *resolved = s->players[actor].traffic_method==0 ? 12 : 13; return 1;
    case 14: case 15: case 16:
        if (s->players[actor].traffic_method<0 ||
            s->players[actor].traffic_method>2) return 0;
        *resolved = 14 + s->players[actor].traffic_method;
        return 1;
    case 33: case 34: case 35: case 36: return s->stage == 0;
    default: return 1;
    }
}

int special_random(GameState *s, int upper)
{
    Rich4Rng rng;
    int value;
    if (upper <= 0) return 0;
    rng.state = s->ai_rng_state;

    value = rich4_rng_original_state_range(&rng, upper);
    s->ai_rng_state = rng.state;
    return value;
}

GameSpecialDefenseOutcome rich4_special_event_default_defense(GameState *state)
{
    int modifier;
    int use_land_modifier, income;
    int id;

    if (state == NULL || state->special_event_phase !=
            GAME_SPECIAL_PHASE_PREPARED ||
        state->special_event_family != GAME_SPECIAL_EVENT_FORTUNE ||
        state->special_event_actor < 0 ||
        state->special_event_actor >= state->player_count)
        return GAME_SPECIAL_DEFENSE_NONE;

    id = state->special_event_resolved_id;
    /* 0x44B896(0,0) reverses the block/double meanings for income.
     * (0,1) uses +70; (1,1) uses +72. IDs 0/1/4/5 do not call it. */
    income = id == 20 || id == 21 || id == 22 || id == 25 ||
             id == 27 || id == 28 || id == 29 || id == 31;
    if (income || id == 2 || id == 3 || id == 8 || id == 9 ||
        (id >= 14 && id <= 19) || id == 23 || id == 24 || id == 26 || id == 30)
        use_land_modifier = 0;
    else if (id == 6 || id == 7 || (id >= 10 && id <= 13) ||
             id == 32 || (id >= 33 && id <= 48))
        use_land_modifier = 1;
    else
        return GAME_SPECIAL_DEFENSE_NONE;

    modifier = use_land_modifier
        ? state->players[state->special_event_actor].god_land_modifier
        : state->players[state->special_event_actor].god_investment_modifier;
    if (modifier > 100 || (modifier > 50 && special_random(state, 2)))
        return income ? GAME_SPECIAL_DEFENSE_DOUBLED : GAME_SPECIAL_DEFENSE_BLOCKED;
    if (modifier < 0)
        return income ? GAME_SPECIAL_DEFENSE_BLOCKED : GAME_SPECIAL_DEFENSE_DOUBLED;
    return GAME_SPECIAL_DEFENSE_NONE;
}

int choose_property(GameState *s, int owner_mode, int built_only,
                           int land_only, GameDomainRef *out)
{
    int count = 0, pick, i;
    for (i = 0; i < s->land_count; i++)
        if ((!built_only || s->land_level[i] != 0) &&
            (owner_mode == 0 ||
             (owner_mode == 1 && s->land_owner[i] < 0) ||
             (owner_mode == 2 && s->land_owner[i] >= 0)))
            count++;
    if (!land_only)
        for (i = 0; i < s->facility_count; i++)
            if ((!built_only || s->facility_level[i] != 0) &&
                (owner_mode == 0 ||
                 (owner_mode == 1 && s->facility_owner[i] < 0) ||
                 (owner_mode == 2 && s->facility_owner[i] >= 0)))
                count++;
    if (count == 0) return 0;
    pick = special_random(s, count);
    for (i = 0; i < s->land_count; i++)
        if ((!built_only || s->land_level[i] != 0) &&
            (owner_mode == 0 ||
             (owner_mode == 1 && s->land_owner[i] < 0) ||
             (owner_mode == 2 && s->land_owner[i] >= 0)) &&
            pick-- == 0) {
            *out = rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
            return 1;
        }
    if (!land_only)
        for (i = 0; i < s->facility_count; i++)
            if ((!built_only || s->facility_level[i] != 0) &&
                (owner_mode == 0 ||
                 (owner_mode == 1 && s->facility_owner[i] < 0) ||
                 (owner_mode == 2 && s->facility_owner[i] >= 0)) &&
                pick-- == 0) {
                *out = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
                return 1;
            }
    return 0;
}

int rich4_special_event_prepare_news_phase1(GameState *s)
{
    GameDomainRef target;
    if (s == NULL || s->special_event_family != GAME_SPECIAL_EVENT_NEWS ||
        s->special_event_phase != GAME_SPECIAL_PHASE_PREPARED) return 0;
    if (s->special_event_resolved_id != 4 || s->special_event_target_mask != 0)
        return 1;
    if (!choose_property(s, 0, 1, 0, &target)) return 0;
    s->special_event_target_mask = presentation_property_target(s, target);
    return 1;
}

int rich4_special_event_news_area_next_raw(const GameNewsAreaAnchor *anchors,
    int count, int *cursor, uint16_t *out_raw)
{
    int i, best = -1, previous;
    if (cursor == NULL || out_raw == NULL || count < 0 ||
        (count > 0 && anchors == NULL)) return 0;
    previous = *cursor;
    if (previous < -1 || previous >= count) return 0;
    for (i = 0; i < count; i++) {
        int x = anchors[i].screen_x, y = anchors[i].screen_y;
        if (x < 120 || x >= 320 || y < 120 || y >= 320) continue;
        if (previous >= 0 && (y < anchors[previous].screen_y ||
            (y == anchors[previous].screen_y && x <= anchors[previous].screen_x))) continue;
        if (best < 0 || y < anchors[best].screen_y ||
            (y == anchors[best].screen_y && x < anchors[best].screen_x)) best = i;
    }
    if (best < 0) return 0;
    *out_raw = 0;
    for (i = 0; i < count; i++)
        if (anchors[i].screen_x == anchors[best].screen_x &&
            anchors[i].screen_y == anchors[best].screen_y) *out_raw |= anchors[i].raw_id;
    *cursor = best;
    return 1;
}

int rich4_special_event_apply_news_area(GameState *s,
    const GameNewsAreaAnchor *anchors, int count)
{
    int i, id, cursor = -1;
    uint16_t combined;
    unsigned int victims = 0;
    if (s == NULL || count < 0 || (count > 0 && anchors == NULL) ||
        s->special_event_family != GAME_SPECIAL_EVENT_NEWS ||
        s->special_event_phase != GAME_SPECIAL_PHASE_PREPARED ||
        (s->special_event_resolved_id != 4 && s->special_event_resolved_id != 20) ||
        s->special_event_target_mask == 0) return 0;
    if (s->special_event_prepared_value == 1) return 1;
    id = s->special_event_resolved_id;
    if (id == 4)
        for (i = 0; i < s->player_count && i < 4; i++)
            if (s->players[i].play_state & 0x40)
                victims |= 1u << (PRESENTATION_PLAYER_SHIFT + i);
    while (rich4_special_event_news_area_next_raw(anchors,count,&cursor,&combined)) {
        unsigned int raw = combined;
        GameDomainRef target;
        int index;
        target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
        if (raw > 2000 && raw < 4000 && raw - 2001 < (unsigned int)s->land_count)
            target = rich4_game_domain_ref(GAME_DOMAIN_LAND, (int)raw - 2001);
        else if (raw > 4000 && raw < 6000 && raw - 4001 < (unsigned int)s->facility_count)
            target = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, (int)raw - 4001);
        if (target.kind != GAME_DOMAIN_NONE) {
            if (id == 4) clear_property(s, target);
            else downgrade_property(s, target);
        }
        if (id != 4 || !(raw & 0x8000u)) continue;
        for (index = 0; index < s->player_count && index < 4; index++)
            if ((raw & (1u << index)) && active_unconfined_player(s, index))
                victims |= 1u << (PRESENTATION_PLAYER_SHIFT + index);
        for (index = 0; index < GAME_SPECIAL_PLAYER_COUNT && index < 4; index++)
            if (raw & (1u << (4 + index)))
                victims |= 1u << (PRESENTATION_PLAYER_SHIFT + 4 + index);
        /* Original slots 17..46 are the three ten-record tool pools. */
        index = (int)((raw >> 8) & 0x7fu) - 17;
        if (index >= 0 && index < GAME_TOOL_PLACED_OBJECT_COUNT)
            rich4_tool_release_area_object(s,index);
    }
    s->special_event_target_mask |= victims;
    s->special_event_prepared_value = 1;
    return 1;
}

void update_fixed_asset(GameState *s, int owner, int delta)
{
    if (owner >= 0 && owner < s->player_count)
        s->fixed_asset_value[owner] += delta;
}

void clear_property(GameState *s, GameDomainRef target)
{
    int owner, old;
    if (target.kind == GAME_DOMAIN_LAND) {
        owner = s->land_owner[target.id];
        old = s->land_value[target.id];
        s->land_owner[target.id] = -1;
        s->land_level[target.id] = 0;
        s->land_type[target.id] = 0;
        s->land_value[target.id] = 0;
        s->land_runtime_value[target.id] = 0; /* 0x40AB4A / 0x40AC7B, H+0x30. */
    } else {
        owner = s->facility_owner[target.id];
        old = s->facility_value[target.id];
        s->facility_owner[target.id] = -1;
        s->facility_level[target.id] = 0;
        s->facility_type[target.id] = 0;
        s->facility_value[target.id] = 0;
        s->facility_last_change[target.id] = 0;
    }
    update_fixed_asset(s, owner, -old);
}

void downgrade_property(GameState *s, GameDomainRef target)
{
    int owner, old, value;
    if (target.kind == GAME_DOMAIN_LAND) {
        owner = s->land_owner[target.id];
        old = s->land_value[target.id];
        if (s->land_level[target.id] == 0) return;
        s->land_level[target.id]--;
        if (s->land_type[target.id] != 0) {
            s->land_level[target.id] = 0;
            s->land_type[target.id] = 0;
        }
        value = s->land_price[target.id] +
                s->land_level[target.id] *
                    s->land_price_per_level[target.id];
        s->land_value[target.id] = value;
    } else {
        owner = s->facility_owner[target.id];
        old = s->facility_value[target.id];
        if (s->facility_level[target.id] == 0) return;
        s->facility_level[target.id]--;
        if (s->facility_level[target.id] == 0)
            s->facility_type[target.id] = 0;
        value = s->facility_price[target.id] +
                s->facility_level[target.id] *
                    s->facility_price_per_level[target.id];
        s->facility_value[target.id] = value;
    }
    update_fixed_asset(s, owner, value - old);
}

unsigned int presentation_property_target(
    const GameState *s, GameDomainRef target)
{
    int owner = -1;
    unsigned int encoded;
    if (target.kind == GAME_DOMAIN_LAND &&
        target.id >= 0 && target.id < s->land_count)
        owner = s->land_owner[target.id];
    else if (target.kind == GAME_DOMAIN_FACILITY &&
             target.id >= 0 && target.id < s->facility_count)
        owner = s->facility_owner[target.id];
    encoded = (unsigned int)(target.id + 1);
    if (target.kind == GAME_DOMAIN_FACILITY)
        encoded |= 0x80000000u;
    if (owner >= 0 && owner < GAME_MAX_PLAYERS)
        encoded |= (unsigned int)(owner + 1) << 24;
    return encoded;
}

int rich4_special_event_decode_property_target(
    const GameState *s, unsigned int encoded, GameDomainRef *out)
{
    GameDomainKind kind;
    int id;
    if (s == NULL || out == NULL ||
        (encoded & PRESENTATION_TARGET_ID_MASK) == 0)
        return 0;
    kind = (encoded & PRESENTATION_FACILITY_BIT) != 0 ?
        GAME_DOMAIN_FACILITY : GAME_DOMAIN_LAND;
    id = (int)(encoded & PRESENTATION_TARGET_ID_MASK) - 1;
    if ((kind == GAME_DOMAIN_LAND && (id < 0 || id >= s->land_count)) ||
        (kind == GAME_DOMAIN_FACILITY &&
         (id < 0 || id >= s->facility_count)))
        return 0;
    *out = rich4_game_domain_ref(kind, id);
    return 1;
}

/* Multiply a 32-bit significand by the original double's significand.
 * Round to the x87's 64 significant bits before CHP truncates to integer.
 * Integer limbs keep the result identical on ARM (long double == double).
 * Evidence: 0x4494E0/0x44A220/0x449DE6/0x44A029 and 0x4654DC..0x46561C. */
static uint32_t original_product(uint32_t value, uint64_t significand, int shift)
{
    uint64_t lower = (uint64_t)value * (uint32_t)significand;
    uint64_t upper = (uint64_t)value * (significand >> 32) + (lower >> 32);
    uint64_t high = upper >> 32;
    uint64_t low = (upper << 32) | (uint32_t)lower;
    uint64_t scan = high;
    int discard = 0;
    while (scan != 0) { discard++; scan >>= 1; }
    if (discard != 0) {
        uint64_t mask = (UINT64_C(1) << discard) - 1;
        uint64_t tail = low & mask;
        uint64_t half = UINT64_C(1) << (discard - 1);
        if (tail > half || (tail == half && ((low >> discard) & 1))) {
            uint64_t before = low;
            low += UINT64_C(1) << discard;
            if (low < before) high++;
        }
        low &= ~mask;
    }
    if (shift >= 128) return 0;
    if (shift >= 64) return (uint32_t)(high >> (shift - 64));
    if (shift <= 0) return shift <= -32 ? 0 : (uint32_t)low << -shift;
    return (uint32_t)((low >> shift) | (high << (64 - shift)));
}

int rich4_special_event_original_scale(int value, int numerator, int denominator)
{
    uint64_t significand;
    uint32_t magnitude = value < 0 ? 0u - (uint32_t)value : (uint32_t)value;
    uint32_t result;
    int shift;
    if (numerator == 13 && denominator == 10) {
        significand = UINT64_C(5854679515581645); shift = 52; /* double 1.3 */
    } else if (numerator == 7 && denominator == 10) {
        significand = UINT64_C(3152519739159347); shift = 52; /* double 0.7 */
    } else {
        significand = UINT64_C(3602879701896397); shift = 56; /* double 0.05 */
    }
    result = original_product(magnitude, significand, shift);
    return (int32_t)(value < 0 ? 0u - result : result);
}

int rich4_special_event_original_stock_tax(float value)
{
    uint32_t bits, result;
    int exponent;
    memcpy(&bits, &value, sizeof(bits));
    exponent = (int)((bits >> 23) & 255);
    if (exponent == 255) return INT_MIN;
    result = original_product((bits & 0x7fffffu) | (exponent ? 0x800000u : 0),
        UINT64_C(3602879701896397), 56 + (exponent ? 150 - exponent : 149));
    return (int32_t)((bits & 0x80000000u) ? 0u - result : result);
}

void scale_property_price(GameState *s, GameDomainRef target,
                                 int numerator, int denominator)
{
    int i;
    if (target.kind == GAME_DOMAIN_LAND) {
        int group = s->land_group[target.id];
        for (i = 0; i < s->land_count; i++)
            if (i == target.id || (group >= 0 && s->land_group[i] == group)) {
                int old = s->land_value[i];
                s->land_price[i] =
                    (uint16_t)rich4_special_event_original_scale(
                        (uint16_t)s->land_price[i], numerator, denominator);
                s->land_value[i] =
                    s->land_price[i] +
                    s->land_level[i] * s->land_price_per_level[i];
                update_fixed_asset(s, s->land_owner[i], s->land_value[i] - old);
            }
    } else {
        int old;
        i = target.id;
        old = s->facility_value[i];
        s->facility_price[i] =
            (uint16_t)rich4_special_event_original_scale(
                (uint16_t)s->facility_price[i], numerator, denominator);
        s->facility_value[i] =
            s->facility_price[i] +
            s->facility_level[i] * s->facility_price_per_level[i];
        update_fixed_asset(s, s->facility_owner[i], s->facility_value[i] - old);
    }
}

static int player_property_total(const GameState *s, int player)
{
    int total = 0, i;
    for (i = 0; i < s->land_count; i++)
        if (s->land_owner[i] == player) total++;
    for (i = 0; i < s->facility_count; i++)
        if (s->facility_owner[i] == player) total++;
    return total;
}

static int player_stock_total(const GameState *s, int player)
{
    int total = 0, i;
    for (i = 0; i < GAME_STOCK_COUNT; i++)
        total += s->player_stock_amount[player][i];
    return total;
}

int extreme_player(const GameState *s, int property, int highest)
{
    int chosen = -1, value = highest ? -1 : INT_MAX, i;
    for (i = 0; i < s->player_count; i++) {
        int candidate;
        if (!active_player(s, i)) continue;
        candidate = property ? player_property_total(s, i) :
                               player_stock_total(s, i);
        if ((highest && candidate > value) ||
            (!highest && candidate < value)) {
            value = candidate;
            chosen = i;
        }
    }
    return chosen;
}

int add_money(GameState *s, int player, int amount, int to_bank)
{
    int *balance;
    if (s == NULL || player < 0 || player >= s->player_count ||
        (!active_player(s, player) &&
         !(s->special_event_family == GAME_SPECIAL_EVENT_FORTUNE &&
           s->special_event_actor == player))) return 0;
    balance = to_bank ? &s->players[player].bank :
                        &s->players[player].cash;
    /* 0x41D407/0x41D40F/0x41D418 use DWORD ADD, including a signed
     * amount produced by a wrapped phase-0 multiplication. */
    *balance = (int32_t)((uint32_t)*balance + (uint32_t)amount);
    s->players[player].monthly_income = (int32_t)(
        (uint32_t)s->players[player].monthly_income + (uint32_t)amount);
    return 1;
}
