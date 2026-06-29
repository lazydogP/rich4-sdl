#include "core/card.h"
#include "core/auction.h"
#include "core/card_internal.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "core/god.h"
#include "core/confinement.h"
#include "core/insurance.h"
#include "core/stock.h"
#include "core/rng.h"
#include "core/game_event_payload.h"

#define SLOT(id) (GAME_CARD_ORIGINAL_HANDLER_TABLE_ADDRESS + ((id) - 1U) * 4U)

const GameCardInfo rich4_card_catalog[GAME_CARD_COUNT] = {
    {1,"均富卡",1,200,2,2,GAME_CARD_TARGET_NONE,GAME_CARD_IMPLEMENTED,SLOT(1),"junfuka","至少一名活动玩家","活动玩家现金改为整数平均值"},
    {2,"均贫卡",2,200,2,2,GAME_CARD_TARGET_PLAYER,GAME_CARD_IMPLEMENTED,SLOT(2),"junpinka","选择其他活动玩家","双方现金改为二者平均值"},
    {3,"购地卡",4,35,0,1,GAME_CARD_TARGET_CURRENT_PROPERTY,GAME_CARD_IMPLEMENTED,SLOT(3),"goudika","停在他人地产且现金足够","付款并转移地产 owner"},
    {4,"换地卡",4,25,0,0,GAME_CARD_TARGET_PROPERTY,GAME_CARD_IMPLEMENTED,SLOT(4),"huandika","当前与目标为可交换地产","交换 owner"},
    {5,"换屋卡",4,20,0,0,GAME_CARD_TARGET_PROPERTY,GAME_CARD_IMPLEMENTED,SLOT(5),"huanwuka","当前与目标为可交换地产","交换 level"},
    {6,"转向卡",3,20,0,0,GAME_CARD_TARGET_PLAYER,GAME_CARD_IMPLEMENTED,SLOT(6),"zhuanxiangka","选择有效角色","反转角色行进方向"},
    {7,"改建卡",8,15,0,0,GAME_CARD_TARGET_CURRENT_PROPERTY,GAME_CARD_IMPLEMENTED,SLOT(7),"gaijianka","当前地产已建设（不要求属于使用者）","切换普通地产类型或选择设施 subtype，并按类型限制等级"},
    {8,"拍卖卡",3,20,0,1,GAME_CARD_TARGET_CURRENT_PROPERTY,GAME_CARD_IMPLEMENTED,SLOT(8),"paimaika","当前对象为地产","进入拍卖并可能释放 owner"},
    {9,"天使卡",2,160,2,0,GAME_CARD_TARGET_PROPERTY,GAME_CARD_IMPLEMENTED,SLOT(9),"tianshika","选择可建设地产","同组地产升级或设施建设"},
    {10,"恶魔卡",1,180,2,2,GAME_CARD_TARGET_PROPERTY,GAME_CARD_IMPLEMENTED,SLOT(10),"emoka","选择已建设地产","同组建筑归零并结算补偿"},
    {11,"怪兽卡",2,60,0,2,GAME_CARD_TARGET_PROPERTY,GAME_CARD_IMPLEMENTED,SLOT(11),"guaishouka","选择合法地产","清除建筑类型与等级，保留 owner 并累计关系补偿"},
    {12,"拆除卡",5,15,0,1,GAME_CARD_TARGET_PROPERTY_OR_MAP_OBJECT,GAME_CARD_IMPLEMENTED,SLOT(12),"chaichuka","他人已建设地产或 16/17/18 型动态道具","地产降一级；动态道具返回公共库存并解除附着"},
    {13,"抢夺卡",4,25,0,2,GAME_CARD_TARGET_PLAYER,GAME_CARD_IMPLEMENTED,SLOT(13),"qiangduoka","目标持有可抢卡片或道具","转移一张卡或一个道具并累计关系值"},
    {14,"停留卡",4,20,0,0,GAME_CARD_TARGET_PLAYER,GAME_CARD_IMPLEMENTED,SLOT(14),"tingliuka","选择活动玩家","目标停留；自己使用写 pending 0x80"},
    {15,"冬眠卡",2,100,2,2,GAME_CARD_TARGET_NONE,GAME_CARD_IMPLEMENTED,SLOT(15),"dongmianka","存在可冬眠角色","其他角色冬眠五天并处理补偿"},
    {16,"梦游卡",4,25,0,1,GAME_CARD_TARGET_PLAYER,GAME_CARD_IMPLEMENTED,SLOT(16),"mengyouka","目标未冬眠且防御链完成","设置梦游、交通恢复值及反制"},
    {17,"陷害卡",4,20,0,2,GAME_CARD_TARGET_PLAYER,GAME_CARD_IMPLEMENTED,SLOT(17),"xianhaika","目标可被陷害","监狱/免罪/嫁祸/复仇链"},
    {18,"复仇卡",4,20,0,0,GAME_CARD_TARGET_NONE,GAME_CARD_UNAVAILABLE,SLOT(18),"fcn_004420d5","原版主动 handler 恒返回 0","仅由反制链被动触发"},
    {19,"嫁祸卡",4,40,0,0,GAME_CARD_TARGET_NONE,GAME_CARD_UNAVAILABLE,SLOT(19),"fcn_004420d5","原版主动 handler 恒返回 0","仅由反制链被动触发"},
    {20,"免费卡",4,25,0,0,GAME_CARD_TARGET_NONE,GAME_CARD_UNAVAILABLE,SLOT(20),"fcn_004420d5","原版主动 handler 恒返回 0","仅由收费链被动触发"},
    {21,"免罪卡",4,25,0,0,GAME_CARD_TARGET_NONE,GAME_CARD_UNAVAILABLE,SLOT(21),"fcn_004420d5","原版主动 handler 恒返回 0","仅由反制链被动触发"},
    {22,"送神符",3,10,0,0,GAME_CARD_TARGET_NONE,GAME_CARD_IMPLEMENTED,SLOT(22),"songshenfu","当前玩家有定时炸弹或类型 5/6/7/8/10/15 神明","解除炸弹和可送神明"},
    {23,"请神符",3,20,0,0,GAME_CARD_TARGET_GOD,GAME_CARD_IMPLEMENTED,SLOT(23),"qingshenfu","选择地图上的神明","移动并附身当前玩家"},
    {24,"红卡",3,50,0,0,GAME_CARD_TARGET_STOCK,GAME_CARD_IMPLEMENTED,SLOT(24),"hongka","选择股票","设置涨停并刷新行情"},
    {25,"黑卡",3,30,0,1,GAME_CARD_TARGET_STOCK,GAME_CARD_IMPLEMENTED,SLOT(25),"heika","选择股票","设置跌停并结算持仓影响"},
    {26,"查税卡",4,35,0,1,GAME_CARD_TARGET_PLAYER,GAME_CARD_IMPLEMENTED,SLOT(26),"chashuika","选择可查税玩家","税款转移与反制链"},
    {27,"涨价卡",3,35,0,0,GAME_CARD_TARGET_PROPERTY,GAME_CARD_IMPLEMENTED,SLOT(27),"zhangjiaka","选择地产","设置同组或设施 price status"},
    {28,"查封卡",3,35,0,1,GAME_CARD_TARGET_PROPERTY,GAME_CARD_IMPLEMENTED,SLOT(28),"chafengka","选择地产","设置查封状态"},
    {29,"同盟卡",2,40,0,0,GAME_CARD_TARGET_PLAYER,GAME_CARD_IMPLEMENTED,SLOT(29),"tongmengka","选择活动玩家","建立玩家关联状态"},
    {30,"乌龟卡",3,70,0,0,GAME_CARD_TARGET_PLAYER,GAME_CARD_IMPLEMENTED,SLOT(30),"wuguika","选择活动玩家","自己两天或他人三天乌龟状态"}
};

const GameCardInfo *rich4_card_info(int card)
{
    return card >= 0 && card < GAME_CARD_COUNT ? &rich4_card_catalog[card] : NULL;
}

static int valid_player(const GameState *state, int player)
{
    return state != NULL && state->player_count > 0 &&
           state->player_count <= GAME_MAX_PLAYERS && player >= 0 &&
           player < state->player_count;
}

static int active_player(const GameState *state, int player)
{
    return valid_player(state, player) &&
           (state->players[player].play_state & 0xff) != 0;
}

int rich4_card_inventory_count(const GameState *state, int player)
{
    int count = 0, i;
    if (!valid_player(state, player)) return -1;
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++)
        if (state->players[player].card_inventory[i] != 0) count++;
    return count;
}

int rich4_card_inventory_has(const GameState *state, int player, int card)
{
    int original_id, i;
    if (!valid_player(state, player) || rich4_card_info(card) == NULL) return 0;
    original_id = card + 1;
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++)
        if (state->players[player].card_inventory[i] == original_id) return 1;
    return 0;
}

int rich4_card_append_events(GameEventQueue *events, const GameEventQueue *staged)
{
    return events->count + staged->count <= GAME_EVENT_QUEUE_CAPACITY &&
           rich4_game_event_queue_append(events, staged);
}

int rich4_card_inventory_add_from_pool(GameState *state, int player, int card,
                                       GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int i;
    if (!valid_player(state, player) || rich4_card_info(card) == NULL ||
        events == NULL || state->card_amount[card] == 0) return 0;
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++)
        if (state->players[player].card_inventory[i] == 0) break;
    if (i == GAME_PLAYER_CARD_SLOTS) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    next.players[player].card_inventory[i] = card + 1;
    next.card_amount[card]--;
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player), 1,
            rich4_card_inventory_count(&next, player), 0) ||
        !rich4_card_append_events(events, &staged)) return 0;
    *state = next;
    return 1;
}

static int target_is_active_character(const GameState *state,
                                      const GameCommand *command)
{
    int target = command->target.id;
    if (command->target.kind == GAME_DOMAIN_PLAYER)
        return target >= 0 && target < state->player_count &&
               (state->players[target].play_state & 0xff) != 0;
    return command->target.kind == GAME_DOMAIN_SPECIAL_PLAYER &&
           target >= 0 && target < GAME_SPECIAL_PLAYER_COUNT &&
           state->special_player_f10[target] == 0;
}

static int valid_land_ref(const GameState *state, GameDomainRef ref)
{
    return ref.kind == GAME_DOMAIN_LAND && ref.id >= 0 &&
           ref.id < state->land_count && ref.id < GAME_MAX_MAP_PROPERTIES;
}

static int valid_facility_ref(const GameState *state, GameDomainRef ref)
{
    return ref.kind == GAME_DOMAIN_FACILITY && ref.id >= 0 &&
           ref.id < state->facility_count && ref.id < GAME_MAX_MAP_PROPERTIES;
}

static int valid_property_ref(const GameState *state, GameDomainRef ref)
{
    return valid_land_ref(state, ref) || valid_facility_ref(state, ref);
}

static int valid_demolish_map_object_ref(const GameState *state,
                                         GameDomainRef ref)
{
    int kind;
    if (state == NULL || ref.kind != GAME_DOMAIN_MAP_OBJECT ||
        ref.id < 0 || ref.id >= GAME_TOOL_PLACED_OBJECT_COUNT)
        return 0;
    kind = state->tool_placed_objects[ref.id].kind;
    return kind >= 0x10 && kind <= 0x12;
}

static int valid_demolish_property_ref(const GameState *state, int actor,
                                       GameDomainRef ref)
{
    int owner;
    int level;
    if (!valid_property_ref(state, ref)) return 0;
    if (ref.kind == GAME_DOMAIN_FACILITY) {
        owner = state->facility_owner[ref.id];
        level = state->facility_level[ref.id];
    } else {
        owner = state->land_owner[ref.id];
        level = state->land_level[ref.id];
    }
    return valid_player(state, owner) && owner != actor && level > 0;
}

GameCardUseResult rich4_card_inspect(const GameState *state,
                                     const GameCommand *command)
{
    const GameCardInfo *info;
    int card;
    if (state == NULL || command == NULL ||
        command->type != GAME_COMMAND_USE_CARD ||
        !rich4_game_command_is_valid(command) ||
        command->subject.kind != GAME_DOMAIN_CARD ||
        !valid_player(state, command->player) ||
        (state->players[command->player].play_state & 0xff) == 0)
        return GAME_CARD_USE_INVALID;
    card = command->subject.id;
    info = rich4_card_info(card);
    if (info == NULL || !rich4_card_inventory_has(state, command->player, card))
        return GAME_CARD_USE_INVALID;
    /* 0x41E69E's personality gate is AI command-selection policy. */
    if (info->implementation == GAME_CARD_UNAVAILABLE)
        return GAME_CARD_USE_UNAVAILABLE;
    if (info->implementation == GAME_CARD_DEFERRED)
        return GAME_CARD_USE_DEFERRED;
    if (info->target_kind == GAME_CARD_TARGET_NONE) {
        if (!rich4_game_domain_ref_is_none(command->target))
            return GAME_CARD_USE_INVALID;
        if (card == 21) {
            int bomb = state->players[command->player].attached_timed_bomb_slot;
            int can_remove_bomb = bomb >= 0 && bomb < GAME_TOOL_PLACED_OBJECT_COUNT &&
                state->tool_placed_objects[bomb].kind == 0x12 &&
                state->tool_placed_objects[bomb].attached_player == command->player + 1;
            if (!can_remove_bomb && !rich4_god_is_sendable_type(
                    rich4_god_player_type(&state->players[command->player])))
                return GAME_CARD_USE_UNAVAILABLE;
        }
    } else if (info->target_kind == GAME_CARD_TARGET_PLAYER) {
        if (!target_is_active_character(state, command)) return GAME_CARD_USE_INVALID;
    } else if (info->target_kind == GAME_CARD_TARGET_PROPERTY_OR_MAP_OBJECT) {
        if (command->target.kind == GAME_DOMAIN_MAP_OBJECT) {
            if (!valid_demolish_map_object_ref(state, command->target))
                return GAME_CARD_USE_INVALID;
        } else if (!valid_demolish_property_ref(
                       state, command->player, command->target)) {
            return GAME_CARD_USE_INVALID;
        }
    } else if (info->target_kind == GAME_CARD_TARGET_PROPERTY ||
               info->target_kind == GAME_CARD_TARGET_CURRENT_PROPERTY) {
        if (!valid_property_ref(state, command->target))
            return GAME_CARD_USE_INVALID;
    } else if (info->target_kind == GAME_CARD_TARGET_STOCK) {
        if (command->target.kind != GAME_DOMAIN_STOCK ||
            command->target.id < 0 || command->target.id >= GAME_STOCK_COUNT)
            return GAME_CARD_USE_INVALID;
    } else if (info->target_kind == GAME_CARD_TARGET_GOD) {
        if (command->target.kind != GAME_DOMAIN_GOD ||
            command->target.id < 0 ||
            command->target.id >= GAME_MAP_DYNAMIC_OBJECT_COUNT ||
            command->value <= 0 || command->value > GAME_GOD_TYPE_COUNT ||
            !rich4_god_is_road_type(command->value))
            return GAME_CARD_USE_INVALID;
    }
    return GAME_CARD_USE_APPLIED;
}

int rich4_card_consume(GameState *state, int player, int card)
{
    int original_id = card + 1, i;
    if (state->card_amount[card] >= 255) return 0;
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++)
        if (state->players[player].card_inventory[i] == original_id) break;
    if (i == GAME_PLAYER_CARD_SLOTS) return 0;
    for (; i + 1 < GAME_PLAYER_CARD_SLOTS; i++)
        state->players[player].card_inventory[i] =
            state->players[player].card_inventory[i + 1];
    state->players[player].card_inventory[GAME_PLAYER_CARD_SLOTS - 1] = 0;
    state->card_amount[card]++;
    return 1;
}

int rich4_card_apply_average_cash(GameState *state, int actor, GameEventQueue *events)
{
    int64_t sum = 0;
    int count = 0, average, i;
    for (i = 0; i < state->player_count; i++) {
        if ((state->players[i].play_state & 0xff) == 0) continue;
        if (state->players[i].cash < 0) return 0;
        sum += state->players[i].cash;
        count++;
    }
    if (count == 0 || sum > INT_MAX * (int64_t)count) return 0;
    average = (int)(sum / count);
    for (i = 0; i < state->player_count; i++) {
        int previous;
        if ((state->players[i].play_state & 0xff) == 0) continue;
        previous = state->players[i].cash;
        if (i != actor && previous > average) {
            int delta = (previous - average) / 100;
            int64_t relation = (int64_t)state->players[i].relation_balance[actor] +
                               delta;
            state->players[i].relation_balance[actor] =
                relation > INT_MAX ? INT_MAX : (int)relation;
        }
        state->players[i].cash = average;
        if (!rich4_game_event_emit_typed_domain(
                events, rich4_game_event_player_cash_changed_from_fields, actor,
                rich4_game_domain_ref(GAME_DOMAIN_PLAYER, i),
                rich4_game_domain_ref(GAME_DOMAIN_CARD, 0), average,
                average - previous, 0)) return 0;
    }
    return 1;
}

int rich4_card_emit_effect(GameEventQueue *events, int actor, GameDomainRef subject,
                           GameDomainRef target, int value, int value2)
{
    return rich4_game_event_emit_typed_domain(
               events, rich4_game_event_status_changed_from_fields, actor,
               target, subject, value, value2, 0) &&
           rich4_game_event_emit_typed_domain(
               events, rich4_game_event_scene_refresh_requested_from_fields,
               actor, target, subject, 0, 0, 0);
}

int rich4_card_inventory_transfer(GameState *state, int from, int to, int card)
{
    int i, empty = -1, found = -1;
    if (!valid_player(state, from) || !valid_player(state, to) ||
        card < 0 || card >= GAME_CARD_COUNT) return 0;
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
        if (state->players[from].card_inventory[i] == card + 1 && found < 0)
            found = i;
        if (state->players[to].card_inventory[i] == 0 && empty < 0) empty = i;
    }
    if (found < 0 || empty < 0) return 0;
    state->players[to].card_inventory[empty] = card + 1;
    for (i = found; i + 1 < GAME_PLAYER_CARD_SLOTS; i++)
        state->players[from].card_inventory[i] =
            state->players[from].card_inventory[i + 1];
    state->players[from].card_inventory[GAME_PLAYER_CARD_SLOTS - 1] = 0;
    return 1;
}

int rich4_card_transfer_tool(GameState *state, int from, int to, int tool)
{
    if (!valid_player(state, from) || !valid_player(state, to) ||
        tool < 0 || tool >= GAME_TOOL_COUNT ||
        state->players[from].tool_inventory[tool] == 0 ||
        state->players[to].tool_inventory[tool] == 255) return 0;
    state->players[from].tool_inventory[tool]--;
    state->players[to].tool_inventory[tool]++;
    return 1;
}

void rich4_card_add_relation_balance(GameState *state, int source, int target,
                                     int64_t delta)
{
    int64_t value;
    if (!valid_player(state, source) || !valid_player(state, target) ||
        source == target || delta <= 0) return;
    value = (int64_t)state->players[source].relation_balance[target] + delta;
    state->players[source].relation_balance[target] =
        value > INT_MAX ? INT_MAX : (int)value;
}

static int apply_attack_status(GameState *state, int victim,
                               GameCardAttackKind kind, int days)
{
    GamePlayerState *player;
    if (!valid_player(state, victim)) return 0;
    player = &state->players[victim];
    if (kind == GAME_CARD_ATTACK_SLEEP_WALK) {
        if (player->days_sleeping != 0) return 0;
        player->days_sleep_walking = 5;
        player->total_confinement_days = (uint8_t)(player->total_confinement_days + 5);
        player->saved_traffic_method = player->traffic_method;
        player->saved_ndices = player->ndices;
        if (player->traffic_method == 2 && player->tool_inventory[4] < 255)
            player->tool_inventory[4]++;
        if (player->traffic_method == 3 && player->tool_inventory[5] < 255)
            player->tool_inventory[5]++;
        player->traffic_method = 0;
        player->ndices = 1;
    } else if (kind == GAME_CARD_ATTACK_FRAME) {
        int current = player->days_in_prison & 0xff;
        if (current == 0) {
            rich4_confinement_clear_status(player);
            if (state->prison_node > 0) player->node = state->prison_node;
            player->previous_node = player->next_node = 0;
            player->confinement_actor_type = GAME_CONFINEMENT_PRISON;
        }
        player->days_in_prison = (current + days) & 0x7f;
        player->total_confinement_days = (player->total_confinement_days + days) & 0xff;
    } else return 0;
    return 1;
}

static int card_resolve_attack(GameState *state, int actor, int target,
                              GameCardAttackKind kind, int redirect_target,
                              GameEventQueue *events, int defer_confinement)
{
    GameState next;
    GameEventQueue staged;
    int victim = target, days;
    int deferred = defer_confinement && kind == GAME_CARD_ATTACK_FRAME;
    if (!valid_player(state, actor) || !valid_player(state, target) ||
        actor == target ||
        events == NULL) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    /* 0x4441DC/0x4444BF: 免罪 precedes 嫁祸; 复仇 is reached only
     * after the original victim actually receives the punishment. */
    if (rich4_card_inventory_has(&next, target, 20)) {
        if (!rich4_card_consume(&next, target, 20) ||
            !rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_card_used_from_fields, target,
                rich4_game_domain_ref(GAME_DOMAIN_CARD, 20),
                rich4_game_domain_ref(GAME_DOMAIN_PLAYER, target), 0,
                next.card_amount[20], 0)) return 0;
    } else {
        if (active_player(&next, redirect_target) && redirect_target != target &&
            rich4_card_inventory_has(&next, target, 18)) {
            if (!rich4_card_consume(&next, target, 18) ||
                !rich4_game_event_emit_typed_domain(
                    &staged, rich4_game_event_card_used_from_fields, target,
                    rich4_game_domain_ref(GAME_DOMAIN_CARD, 18),
                    rich4_game_domain_ref(GAME_DOMAIN_PLAYER, redirect_target),
                    redirect_target, next.card_amount[18], 0)) return 0;
            victim = redirect_target;
        }
        days = kind == GAME_CARD_ATTACK_FRAME && victim == actor ? 4 : 5;
        if ((deferred ? rich4_confinement_request_enter(&next, victim,
                GAME_CONFINEMENT_PRISON, days, &staged) != GAME_CONFINEMENT_APPLIED :
                !apply_attack_status(&next, victim, kind, days)) ||
            (!deferred && !rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_status_changed_from_fields, actor,
                rich4_game_domain_ref(GAME_DOMAIN_PLAYER, victim),
                rich4_game_domain_ref(GAME_DOMAIN_STATUS,
                    kind == GAME_CARD_ATTACK_SLEEP_WALK ?
                    GAME_STATUS_SLEEP_WALKING : GAME_STATUS_PRISON), days, 0, 0)) ||
            (!deferred && kind == GAME_CARD_ATTACK_FRAME &&
             !rich4_insurance_claim_for_days(
                 &next, victim, days, GAME_INSURANCE_CLAIM_PRISON,
                 NULL, &staged)))
            return 0;
        if (victim == target && rich4_card_inventory_has(&next, target, 17)) {
            if (!rich4_card_consume(&next, target, 17) ||
                (deferred ? rich4_confinement_request_enter(&next, actor,
                    GAME_CONFINEMENT_PRISON, 5, &staged) != GAME_CONFINEMENT_APPLIED :
                    !apply_attack_status(&next, actor, kind, 5)) ||
                !rich4_game_event_emit_typed_domain(
                    &staged, rich4_game_event_card_used_from_fields, target,
                    rich4_game_domain_ref(GAME_DOMAIN_CARD, 17),
                    rich4_game_domain_ref(GAME_DOMAIN_PLAYER, actor), actor,
                    next.card_amount[17], 0) ||
                (!deferred && kind == GAME_CARD_ATTACK_FRAME &&
                 !rich4_insurance_claim_for_days(
                     &next, actor, 5, GAME_INSURANCE_CLAIM_PRISON,
                     NULL, &staged))) return 0;
        }
    }
    if (!rich4_card_append_events(events, &staged)) return 0;
    *state = next;
    return 1;
}

int rich4_card_resolve_attack(GameState *state, int actor, int target,
    GameCardAttackKind kind, int redirect_target, GameEventQueue *events)
{
    return card_resolve_attack(state, actor, target, kind, redirect_target, events, 0);
}

int rich4_card_resolve_attack_deferred_confinement(GameState *state, int actor,
    int target, GameCardAttackKind kind, int redirect_target, GameEventQueue *events)
{
    return card_resolve_attack(state, actor, target, kind, redirect_target, events, 1);
}

int rich4_card_resolve_charge(GameState *state, int payer, int payee,
                              int amount, int accept_free,
                              int redirect_target, int *out_payer,
                              int *out_amount, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int resolved = payer;
    if (!valid_player(state, payer) || !valid_player(state, payee) ||
        amount < 0 || events == NULL) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    /* 0x41982D/0x4451F0: 免费 takes the whole charge; otherwise 嫁祸
     * changes the payer before the ledger transaction. */
    if (accept_free && rich4_card_inventory_has(&next, payer, 19)) {
        if (!rich4_card_consume(&next, payer, 19) ||
            !rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_card_used_from_fields, payer,
                rich4_game_domain_ref(GAME_DOMAIN_CARD, 19),
                rich4_game_domain_ref(GAME_DOMAIN_PLAYER, payee), 0,
                next.card_amount[19], 0)) return 0;
        amount = 0;
    } else if (active_player(&next, redirect_target) &&
               redirect_target != payer &&
               rich4_card_inventory_has(&next, payer, 18)) {
        if (!rich4_card_consume(&next, payer, 18) ||
            !rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_card_used_from_fields, payer,
                rich4_game_domain_ref(GAME_DOMAIN_CARD, 18),
                rich4_game_domain_ref(GAME_DOMAIN_PLAYER, redirect_target),
                redirect_target, next.card_amount[18], 0)) return 0;
        resolved = redirect_target;
    }
    if (!rich4_card_append_events(events, &staged)) return 0;
    *state = next;
    if (out_payer != NULL) *out_payer = resolved;
    if (out_amount != NULL) *out_amount = amount;
    return 1;
}

int rich4_card_resolve_landing_charge(GameState *state, int payer, int payee,
    int amount, int allow_free, int accept_free, int redirect_target,
    int *out_payer, int *out_amount, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    Rich4Rng rng;
    int resolved, fee, i;
    if (!valid_player(state, payer) || !valid_player(state, payee) ||
        amount < 0 || events == NULL) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (next.players[payer].is_ai) {
        int threshold = (int64_t)amount >= (int64_t)2000 * next.price_index ||
            (int64_t)amount > (int64_t)next.players[payer].cash +
                              next.players[payer].bank;
        accept_free = 0;
        redirect_target = -1;
        rng.state = next.ai_rng_state;
        /* 0x419E63/0x41AF2B gates calls to 0x444A60 and 0x44476A.
         * A hotel (0x41A639) never calls the free-card handler. */
        if (threshold && allow_free &&
            rich4_card_inventory_has(&next, payer, 19)) {
            int64_t limit = (int64_t)(rich4_rng_original_state_range(&rng, 3000) + 3000) *
                            next.price_index;
            accept_free = amount > next.players[payer].cash || amount > limit;
        }
        if (amount != 0 && !accept_free && threshold &&
            rich4_card_inventory_has(&next, payer, 18)) {
            int best_balance = 0;
            int candidates[GAME_MAX_PLAYERS], count = 0;
            /* 0x40D2D3, then 0x40D31C: choose the relationship maximum
             * before evaluating the 0x444915 random spending threshold. */
            for (i = 0; i < next.player_count; i++) {
                if (i == payer || !active_player(&next, i)) continue;
                if (next.players[payer].relation_balance[i] > best_balance) {
                    best_balance = next.players[payer].relation_balance[i];
                    redirect_target = i;
                }
                if (next.players[i].delayed_status_block == 0 &&
                    next.players[i].days_disappearing == 0 &&
                    next.players[i].days_in_prison == 0 &&
                    next.players[i].days_in_hospital == 0)
                    candidates[count++] = i;
            }
            if (redirect_target < 0 && count > 0)
                redirect_target = candidates[rich4_rng_original_state_range(&rng, count)];
            {
                int64_t limit = (int64_t)(rich4_rng_original_state_range(&rng, 4000) + 4000) *
                                next.price_index;
                if (amount <= next.players[payer].cash && amount <= limit)
                    redirect_target = -1;
            }
        }
        next.ai_rng_state = rng.state;
    }
    if (!rich4_card_resolve_charge(&next, payer, payee, amount,
            allow_free && accept_free, redirect_target, &resolved, &fee, &staged))
        return 0;
    /* 0x40FBB8 runs after the card chain; hotel stays still seek a payer
     * when applicable, even if a preceding card removed the fee. */
    if (fee != 0 || !allow_free) {
        for (i = 0; i < next.player_count; i++) {
            int god = rich4_god_player_type(&next.players[i]);
            if (i != resolved && active_player(&next, i) &&
                (god == 14 || god == 15)) {
                resolved = i;
                break;
            }
        }
    }
    if (!rich4_card_append_events(events, &staged)) return 0;
    *state = next;
    if (out_payer != NULL) *out_payer = resolved;
    if (out_amount != NULL) *out_amount = fee;
    return 1;
}

int rich4_card_apply_property_card(GameState *state, const GameCommand *command,
                                    int card, GameEventQueue *events)
{
    int i, id = command->target.id, actor = command->player;
    int source = command->value;
    int facility = command->target.kind == GAME_DOMAIN_FACILITY;
    int source_facility = (command->flags & GAME_CARD_FLAG_SECONDARY_FACILITY) != 0;
    if (card == 2) {
        int owner, price;
        if (facility) {
            owner = state->facility_owner[id]; price = state->facility_value[id];
            if (price <= 0) price = state->facility_price[id];
        } else {
            owner = state->land_owner[id]; price = state->land_value[id];
            if (price <= 0) price = state->land_price[id] +
                state->land_level[id] * state->land_price_per_level[id];
        }
        if (!valid_player(state, owner) || owner == actor || price < 0 ||
            state->players[actor].cash < price) return 0;
        state->players[actor].cash -= price;
        state->players[owner].cash += price;
        if (facility) state->facility_owner[id] = actor;
        else state->land_owner[id] = actor;
    } else if (card == 3 || card == 4) {
        if (source < 0 || facility != source_facility) return 0;
        if (facility) {
            int t;
            if (source >= state->facility_count) return 0;
            if (card == 3) { t=state->facility_owner[source]; state->facility_owner[source]=state->facility_owner[id]; state->facility_owner[id]=t; }
            else { t=state->facility_level[source]; state->facility_level[source]=state->facility_level[id]; state->facility_level[id]=t; t=state->facility_variant[source]; state->facility_variant[source]=state->facility_variant[id]; state->facility_variant[id]=t; }
        } else {
            int t;
            if (source >= state->land_count || state->land_type[source] != state->land_type[id]) return 0;
            if (card == 3) { t=state->land_owner[source]; state->land_owner[source]=state->land_owner[id]; state->land_owner[id]=t; }
            else { t=state->land_level[source]; state->land_level[source]=state->land_level[id]; state->land_level[id]=t; }
        }
    } else if (card == 6) {
        if (facility) {
            if (state->facility_level[id] <= 0 ||
                command->value2 < 0 || command->value2 > 4) return 0;
            state->facility_type[id] = command->value2;
            if ((command->value2 == 0 || command->value2 == 3) &&
                state->facility_level[id] > 1)
                state->facility_level[id] = 1;
        } else {
            if (state->land_level[id] <= 0) return 0;
            state->land_type[id] = state->land_type[id] == 0 ? 1 : 0;
            if (state->land_type[id] != 0 && state->land_level[id] > 1)
                state->land_level[id] = 1;
        }
    } else if (card == 7) {
        GameEvent request;
        int owner = facility ? state->facility_owner[id] : state->land_owner[id];
        int level = facility ? state->facility_level[id] : state->land_level[id];
        int base_price = facility ? state->facility_price[id] : state->land_price[id];
        /* 0x4432C6/0x4433EF update the directed relation accumulator before
         * entering 0x43BDE5.  This is not a cash payment to the old owner. */
        rich4_card_add_relation_balance(
            state, owner, actor,
            (int64_t)base_price * state->price_index * (level + 2) / 5);
        request = rich4_game_event_auction_request(
            actor, command->target, actor, 1, GAME_AUCTION_TRIGGER_CARD,
            GAME_AUCTION_CLEAR_OWNER_IF_UNSOLD);
        if (!rich4_game_event_queue_push(events, &request)) return 0;
    } else if (card == 8 || card == 9) {
        int group = facility ? -1 : state->land_group[id];
        for (i = 0; i < (facility ? state->facility_count : state->land_count); i++) {
            if (!facility && (state->land_type[i] != 0 || state->land_group[i] != group)) continue;
            if (facility && i != id) continue;
            if (facility) state->facility_level[i] = card == 8 ?
                (state->facility_level[i] < 5 ? state->facility_level[i] + 1 : 5) : 0;
            else state->land_level[i] = card == 8 ?
                (state->land_level[i] < 5 ? state->land_level[i] + 1 : 5) : 0;
        }
    } else if (card == 10) {
        int owner = facility ? state->facility_owner[id] : state->land_owner[id];
        int level = facility ? state->facility_level[id] : state->land_level[id];
        int price_index = state->price_index;
        if (level <= 0) return 0;
        rich4_card_add_relation_balance(state, owner, actor,
                             (int64_t)62 * level * price_index);
        if (facility) {
            state->facility_type[id] = 0;
            state->facility_level[id] = 0;
        } else {
            state->land_type[id] = 0;
            state->land_level[id] = 0;
        }
    } else if (card == 11 && command->target.kind == GAME_DOMAIN_MAP_OBJECT) {
        struct GameToolPlacedObjectState *object =
            &state->tool_placed_objects[id];
        int kind = object->kind;
        int attached = object->attached_player;
        int tool = kind - 0x0f;
        if (!valid_demolish_map_object_ref(state, command->target)) return 0;
        if (kind == 0x12 && attached > 0 &&
            attached <= GAME_MAX_PLAYERS &&
            state->players[attached - 1].attached_timed_bomb_slot == id)
            state->players[attached - 1].attached_timed_bomb_slot = -1;
        /* fcn_0040e14d increments the public supplies for dynamic tool
         * types 0x10/0x11/0x12 before clearing the object record. */
        if (tool >= 1 && tool <= 3 && state->tool_amount[tool] < 255)
            state->tool_amount[tool]++;
        memset(object, 0, sizeof(*object));
        if (!rich4_game_event_emit_typed_domain(
                events, rich4_game_event_map_object_changed_from_fields,
                actor, command->subject, command->target, id, 0, 0))
            return 0;
    } else if (card == 11) {
        if (facility) { if (state->facility_level[id] <= 0) return 0; state->facility_level[id]--; }
        else { if (state->land_level[id] <= 0) return 0; state->land_level[id]--; }
    } else if (card == 26 || card == 27) {
        /* 0x4454EF/0x445659: overwrite the same status byte for every
         * same-name land, regardless of owner or building subtype. */
        int group = facility ? -1 : state->land_group[id];
        int status = card == 26 ? 0x50 : 0x51;
        for (i = 0; i < (facility ? state->facility_count : state->land_count); i++) {
            if (facility ? i != id :
                (i != id && (group < 0 || state->land_group[i] != group))) continue;
            if (facility) {
                state->facility_price_status[i] = status;
                state->facility_closed_status[i] = card == 27 ? 5 : 0;
                /* 0x4456D5: sealing a research institute clears +0x1E. */
                if (card == 27 && state->facility_type[i] == 4)
                    state->facility_research_turns[i] = 0;
            } else {
                state->land_price_status[i] = status;
                state->land_closed_status[i] = card == 27 ? 5 : 0;
            }
        }
    } else return 0;
    return rich4_card_emit_effect(events, actor, command->subject, command->target, card + 1, 0);
}
