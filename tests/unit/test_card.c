#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/card.h"
#include "core/game_event_payload.h"

static GameDomainRef none(void)
{
    return rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
}

static void setup(GameState *state)
{
    int i;
    memset(state, 0, sizeof(*state));
    state->active = 1;
    state->player_count = 3;
    state->players[0].play_state = 1;
    state->players[1].play_state = 1;
    state->players[2].play_state = 0;
    for (i = 0; i < GAME_CARD_COUNT; i++)
        state->card_amount[i] = rich4_card_catalog[i].initial_pool;
}

static GameCommand use_card(int player, int card, GameDomainRef target,
                            GameCommandSource source)
{
    return rich4_game_domain_command(GAME_COMMAND_USE_CARD, source, player,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, card), target, 0);
}

int main(void)
{
    GameState state, before;
    GameEventQueue events;
    GameCommand command;
    int i;
    for (i = 0; i < GAME_CARD_COUNT; i++) {
        const GameCardInfo *info = rich4_card_info(i);
        assert(info != NULL && info->original_id == i + 1 &&
               info->name != NULL && info->handler_symbol != NULL &&
               info->precondition != NULL && info->side_effect != NULL &&
               info->handler_slot_address ==
                   GAME_CARD_ORIGINAL_HANDLER_TABLE_ADDRESS + (uintptr_t)i * 4U);
    }
    assert(rich4_card_info(-1) == NULL && rich4_card_info(30) == NULL);

    setup(&state);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_inventory_add_from_pool(&state, 0, 0, &events));
    assert(state.players[0].card_inventory[0] == 1 &&
           state.card_amount[0] == 0 &&
           rich4_card_inventory_count(&state, 0) == 1 &&
           rich4_card_inventory_has(&state, 0, 0));
    for (i = 1; i < GAME_PLAYER_CARD_SLOTS; i++) {
        state.players[0].card_inventory[i] = i + 1;
    }
    before = state;
    rich4_game_event_queue_reset(&events);
    assert(!rich4_card_inventory_add_from_pool(&state, 0, 29, &events));
    assert(memcmp(&state, &before, sizeof(state)) == 0 && events.count == 0);

    setup(&state);
    state.players[0].cash = 10;
    state.players[1].cash = 21;
    state.players[2].cash = 1000;
    state.players[0].card_inventory[0] = 1;
    state.players[0].card_inventory[1] = 30;
    command = use_card(0, 0, none(), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[0].cash == 15 && state.players[1].cash == 15 &&
           state.players[2].cash == 1000);
    assert(state.players[1].relation_balance[0] == 0);
    assert(state.players[0].card_inventory[0] == 30 &&
           state.players[0].card_inventory[1] == 0 &&
           state.card_amount[0] == 2 && events.count == 4);

    setup(&state);
    state.players[0].cash = 100;
    state.players[1].cash = 500;
    state.players[0].card_inventory[0] = 1;
    command = use_card(0, 0, none(), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[0].cash == 300 && state.players[1].cash == 300 &&
           state.players[1].relation_balance[0] == 2);

    setup(&state);
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_level[0] = 4;
    state.land_type[0] = 0;
    state.players[0].card_inventory[0] = 7;
    command = use_card(0, 6,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.land_owner[0] == 1 && state.land_type[0] == 1 &&
           state.land_level[0] == 1);

    setup(&state);
    state.facility_count = 1;
    state.facility_owner[0] = 1;
    state.facility_level[0] = 4;
    state.facility_type[0] = 2;
    state.players[0].card_inventory[0] = 7;
    command = use_card(0, 6,
        rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0),
        GAME_COMMAND_SOURCE_HUMAN);
    command.value2 = 3;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.facility_owner[0] == 1 && state.facility_type[0] == 3 &&
           state.facility_level[0] == 1);

    setup(&state);
    state.price_index = 3;
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_level[0] = 4;
    state.land_type[0] = 1;
    state.players[0].card_inventory[0] = 11;
    command = use_card(0, 10,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.land_owner[0] == 1 && state.land_type[0] == 0 &&
           state.land_level[0] == 0 &&
           state.players[1].relation_balance[0] == 62 * 4 * 3);

    /* Original card ID 12 also targets the three placed dynamic tool
     * ranges.  Clearing one returns the public supply and emits a typed map
     * object change in addition to the normal card effect events. */
    setup(&state);
    state.players[0].card_inventory[0] = 12;
    state.tool_amount[1] = 3;
    state.tool_placed_objects[0].kind = 0x10;
    state.tool_placed_objects[0].node = 7;
    state.tool_placed_objects[0].owner = 1;
    command = use_card(0, 11,
        rich4_game_domain_ref(GAME_DOMAIN_MAP_OBJECT, 0),
        GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_inspect(&state, &command) == GAME_CARD_USE_APPLIED);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.tool_placed_objects[0].kind == 0 &&
           state.tool_amount[1] == 4 &&
           !rich4_card_inventory_has(&state, 0, 11));
    {
        int saw_map_object = 0;
        for (i = 0; i < events.count; i++) {
            if (events.events[i].type == GAME_EVENT_MAP_OBJECT_CHANGED &&
                rich4_game_event_map_object_subject(&events.events[i]).kind ==
                    GAME_DOMAIN_CARD &&
                rich4_game_event_map_object_subject(&events.events[i]).id == 11 &&
                rich4_game_event_tool_followup_slot(&events.events[i]) == 0)
                saw_map_object = 1;
        }
        assert(saw_map_object);
    }

    setup(&state);
    state.players[0].card_inventory[0] = 12;
    state.players[0].attached_timed_bomb_slot = 20;
    state.tool_amount[3] = 2;
    state.tool_placed_objects[20].kind = 0x12;
    state.tool_placed_objects[20].attached_player = 1;
    state.tool_placed_objects[20].node = 0;
    command = use_card(0, 11,
        rich4_game_domain_ref(GAME_DOMAIN_MAP_OBJECT, 20),
        GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.tool_placed_objects[20].kind == 0 &&
           state.players[0].attached_timed_bomb_slot == -1 &&
           state.tool_amount[3] == 3);

    setup(&state);
    state.players[0].card_inventory[0] = 13;
    state.players[1].tool_inventory[5] = 2;
    command = use_card(0, 12,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1),
        GAME_COMMAND_SOURCE_HUMAN);
    command.value = 5;
    command.flags = GAME_CARD_FLAG_ROB_TOOL;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[1].tool_inventory[5] == 1 &&
           state.players[0].tool_inventory[5] == 1 &&
           state.players[1].relation_balance[0] ==
               rich4_card_catalog[5].shop_price);

    setup(&state);
    state.players[0].card_inventory[0] = 14;
    state.players[0].card_inventory[1] = 14;
    command = use_card(0, 13,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[1].days_stopping == 1 &&
           state.players[0].card_inventory[0] == 14 &&
           state.players[0].card_inventory[1] == 0);
    command.target.id = 0;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[0].days_stopping == GAME_STATUS_PENDING_BIT);

    setup(&state);
    state.players[0].card_inventory[0] = 30;
    command = use_card(0, 29,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[1].days_tortoise_walking == 3);
    setup(&state);
    state.special_player_f10[2] = 0;
    state.players[0].card_inventory[0] = 30;
    command = use_card(0, 29,
        rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER, 2),
        GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.special_player_status[2][3] == 3);

    setup(&state);
    state.players[0].card_inventory[0] = 22;
    state.players[0].god_info = 9;
    state.players[0].god_type = 10;
    state.players[0].attached_timed_bomb_slot = 20;
    state.tool_placed_objects[20].kind = 0x12;
    state.tool_placed_objects[20].attached_player = 1;
    state.tool_placed_objects[20].node = 7;
    command = use_card(0, 21, none(), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[0].god_info == 0 && state.players[0].god_type == 0 &&
           state.players[0].attached_timed_bomb_slot == -1 &&
           state.tool_placed_objects[20].kind == 0 &&
           state.players[0].card_inventory[0] == 0);
    setup(&state);
    state.players[0].card_inventory[0] = 22;
    state.players[0].god_info = 4;
    state.players[0].god_type = 1;
    command = use_card(0, 21, none(), GAME_COMMAND_SOURCE_HUMAN);
    before = state;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_UNAVAILABLE);
    assert(memcmp(&state, &before, sizeof(state)) == 0);

    setup(&state);
    state.players[0].card_inventory[0] = 23; /* 请神符 */
    state.players[0].god_info = 4;
    state.players[0].god_type = 1;
    command = use_card(0, 22,
        rich4_game_domain_ref(GAME_DOMAIN_GOD, 14),
        GAME_COMMAND_SOURCE_HUMAN);
    command.value = 15; /* Death is a legal road god. */
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_inspect(&state, &command) == GAME_CARD_USE_APPLIED);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[0].god_info == 15 &&
           state.players[0].god_type == 15 &&
           !rich4_card_inventory_has(&state, 0, 22));

    setup(&state);
    state.players[0].card_inventory[0] = 23;
    command = use_card(0, 22,
        rich4_game_domain_ref(GAME_DOMAIN_GOD, 14),
        GAME_COMMAND_SOURCE_HUMAN);
    command.value = 11; /* Dog/object type, not a road god target. */
    before = state;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_inspect(&state, &command) == GAME_CARD_USE_INVALID);
    assert(memcmp(&state, &before, sizeof(state)) == 0 && events.count == 0);

    setup(&state);
    state.players[0].cash = 11;
    state.players[1].cash = 20;
    state.players[0].card_inventory[0] = 2;
    command = use_card(0, 1,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[0].cash == 15 && state.players[1].cash == 15 &&
           !rich4_card_inventory_has(&state, 0, 1));

    setup(&state);
    state.players[0].card_inventory[0] = 6;
    command = use_card(0, 5,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[1].route_reversed == 1);

    setup(&state);
    state.players[0].card_inventory[0] = 17; /* 陷害 */
    state.players[1].card_inventory[0] = 21; /* 免罪 */
    command = use_card(0, 16,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[1].days_in_prison == 0 &&
           !rich4_card_inventory_has(&state, 1, 20));

    setup(&state);
    state.players[0].card_inventory[0] = 16; /* 梦游 */
    state.players[1].card_inventory[0] = 18; /* 复仇 */
    command = use_card(0, 15,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(state.players[0].days_sleep_walking == 5 &&
           state.players[1].days_sleep_walking == 5);

    setup(&state);
    state.players[0].card_inventory[0] = 20; /* 免费 */
    rich4_game_event_queue_reset(&events);
    {
        int payer = -1, amount = -1;
        assert(rich4_card_resolve_charge(&state, 0, 1, 500, 1, -1,
                                         &payer, &amount, &events));
        assert(payer == 0 && amount == 0 &&
               !rich4_card_inventory_has(&state, 0, 19));
    }

    setup(&state);
    state.players[2].play_state = 1;
    state.players[0].card_inventory[0] = 19; /* 嫁祸 */
    rich4_game_event_queue_reset(&events);
    {
        int payer = -1, amount = -1;
        assert(rich4_card_resolve_charge(&state, 0, 1, 500, 0, 2,
                                         &payer, &amount, &events));
        assert(payer == 2 && amount == 500 &&
               !rich4_card_inventory_has(&state, 0, 18));
    }

    state.players[0].card_inventory[0] = 18;
    command = use_card(0, 17, none(), GAME_COMMAND_SOURCE_HUMAN);
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_UNAVAILABLE);
    assert(state.players[0].card_inventory[0] == 18);

    setup(&state);
    state.players[0].card_inventory[0] = 14;
    command = use_card(0, 13,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 2), GAME_COMMAND_SOURCE_HUMAN);
    before = state;
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_INVALID);
    assert(memcmp(&state, &before, sizeof(state)) == 0);

    setup(&state);
    state.players[0].cash = 10;
    state.players[1].cash = 20;
    state.players[0].card_inventory[0] = 1;
    command = use_card(0, 0, none(), GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    while (events.count < GAME_EVENT_QUEUE_CAPACITY)
        assert(rich4_game_event_text_requested_emit(
            &events, 0, 0, 0, 0, 0));
    before = state;
    assert(rich4_card_apply_command(&state, &command, &events) ==
           GAME_CARD_USE_INVALID);
    assert(memcmp(&state, &before, sizeof(state)) == 0);

    setup(&state);
    state.players[0].is_ai = 1;
    state.players[0].card_inventory[0] = 1;
    command = use_card(0, 0, none(), GAME_COMMAND_SOURCE_AI);
    assert(rich4_card_inspect(&state, &command) == GAME_CARD_USE_APPLIED);
    state.players[0].ai_personality = 1;
    assert(rich4_card_inspect(&state, &command) == GAME_CARD_USE_APPLIED);
    state.players[0].ai_personality = 2;
    assert(rich4_card_inspect(&state, &command) == GAME_CARD_USE_APPLIED);

    /* 0x44542D/0x445593 overwrite a shared five-day state for the whole
     * name group, including another owner and a chain-store subtype. */
    setup(&state);
    state.land_count = 4;
    for (i = 0; i < 4; i++) {
        state.land_group[i] = i < 3 ? 7 : 8;
        state.land_owner[i] = i & 1;
    }
    state.land_type[2] = 1;
    state.players[0].card_inventory[0] = 28;
    command = use_card(0, 27, rich4_game_domain_ref(GAME_DOMAIN_LAND, 0),
                        GAME_COMMAND_SOURCE_HUMAN);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) == GAME_CARD_USE_APPLIED);
    for (i = 0; i < 3; i++)
        assert(state.land_price_status[i] == 0x51 && state.land_closed_status[i] == 5);
    assert(state.land_price_status[3] == 0 && state.land_closed_status[3] == 0);
    state.players[0].card_inventory[0] = 27;
    command.subject.id = 26;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) == GAME_CARD_USE_APPLIED);
    for (i = 0; i < 3; i++)
        assert(state.land_price_status[i] == 0x50 && state.land_closed_status[i] == 0);
    state.facility_count = 2;
    state.facility_type[0] = 4;
    state.facility_research_turns[0] = 3;
    state.players[0].card_inventory[0] = 28;
    command.subject.id = 27;
    command.target = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) == GAME_CARD_USE_APPLIED);
    assert(state.facility_price_status[0] == 0x51 &&
           state.facility_closed_status[0] == 5 &&
           state.facility_research_turns[0] == 0 && state.facility_price_status[1] == 0);
    state.players[0].card_inventory[0] = 27;
    command.subject.id = 26;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&state, &command, &events) == GAME_CARD_USE_APPLIED);
    assert(state.facility_price_status[0] == 0x50 && state.facility_closed_status[0] == 0);
    puts("card catalog/inventory/use transaction: ok");
    return 0;
}
