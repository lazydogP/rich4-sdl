#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "core/property.h"
#include "core/game_event_payload.h"

static void init_state(GameState *s)
{
    int i;
    memset(s, 0, sizeof(*s));
    s->active = 1; s->player_count = 3; s->price_index = 2;
    s->initial_fund = 100000; s->winner_player = -1;
    for (i = 0; i < 3; i++) {
        s->players[i].play_state = i == 2 ? 2 : 1;
        s->players[i].is_ai = i == 2;
        s->players[i].linked_player = -1;
    }
    for (i = 0; i < GAME_MAX_MAP_PROPERTIES; i++) {
        s->land_owner[i] = -1; s->land_group[i] = -1;
    }
}

static GameCommand land_command(GameCommandType type, int player, int land)
{
    return rich4_game_domain_command(type,
        player == 2 ? GAME_COMMAND_SOURCE_AI : GAME_COMMAND_SOURCE_HUMAN,
        player, rich4_game_domain_ref(GAME_DOMAIN_LAND, land),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
}

static GameEvent pop(GameEventQueue *q)
{
    GameEvent e;
    assert(rich4_game_event_queue_pop(q, &e));
    return e;
}

int main(void)
{
    GameState s, before;
    GameEventQueue q;
    GameLandAction action;
    GameCommand command;
    GameEvent e;
    int value, i;

    init_state(&s); s.land_count = 1; s.land_level[0] = 2;
    s.land_price[0] = 100; s.land_price_per_level[0] = 20;
    s.players[0].cash = 280; s.players[0].bank = 9999;
    assert(rich4_land_purchase_price(&s, 0, &value) && value == 280);
    assert(rich4_land_inspect(&s, 0, 0, &action) &&
           action.kind == GAME_LAND_ACTION_PURCHASE && action.amount == 280);
    rich4_game_event_queue_reset(&q); command = land_command(GAME_COMMAND_PURCHASE_PROPERTY, 0, 0);
    assert(rich4_land_apply_command(&s, &command, &q));
    assert(s.players[0].cash == 0 && s.players[0].bank == 9999 &&
           s.land_owner[0] == 0 && s.land_value[0] == 140);
    e = pop(&q); assert(e.type == GAME_EVENT_MONEY_TRANSFERRED &&
                       rich4_game_event_value(&e) == 280 &&
                       rich4_game_event_flags(&e) == GAME_PAYMENT_PAYER_CASH_ONLY);
    e = pop(&q); assert(e.type == GAME_EVENT_PROPERTY_OWNER_CHANGED &&
                       rich4_game_event_subject_ref(&e).id == 0);
    assert(pop(&q).type == GAME_EVENT_SCENE_REFRESH_REQUESTED && q.count == 0);

    init_state(&s); s.land_count = 1; s.land_price[0] = 100;
    s.players[0].cash = 199; s.players[0].bank = 10000; before = s;
    assert(rich4_land_inspect(&s, 0, 0, &action) && action.kind == GAME_LAND_ACTION_NONE);
    rich4_game_event_queue_reset(&q); command = land_command(GAME_COMMAND_PURCHASE_PROPERTY, 0, 0);
    assert(!rich4_land_apply_command(&s, &command, &q));
    assert(memcmp(&s, &before, sizeof(s)) == 0 && q.count == 0);
    s.players[0].cash = 1000; s.players[0].god_info = 7; s.players[0].god_type = 7; before = s;
    assert(rich4_land_inspect(&s, 0, 0, &action) && action.kind == GAME_LAND_ACTION_PURCHASE);
    assert(!rich4_land_apply_command(&s, &command, &q));
    assert(memcmp(&s, &before, sizeof(s)) == 0);
    s.players[0].god_type = 12;
    s.players[0].god_info = 12;
    assert(rich4_land_inspect(&s, 0, 0, &action) && action.kind == GAME_LAND_ACTION_NONE);
    before = s;
    rich4_game_event_queue_reset(&q);
    assert(!rich4_land_apply_command(&s, &command, &q));
    assert(memcmp(&s, &before, sizeof(s)) == 0 && q.count == 0);

    init_state(&s); s.land_count = 1; s.land_owner[0] = 0;
    s.land_price[0] = 100; s.land_price_per_level[0] = 20; s.players[0].cash = 200;
    command = land_command(GAME_COMMAND_UPGRADE_PROPERTY, 0, 0);
    for (i = 1; i <= 5; i++) {
        rich4_game_event_queue_reset(&q);
        assert(rich4_land_apply_command(&s, &command, &q));
        assert(s.land_level[0] == i && s.land_value[0] == 100 + i * 20);
        assert(pop(&q).type == GAME_EVENT_MONEY_TRANSFERRED);
        e = pop(&q); assert(e.type == GAME_EVENT_PROPERTY_LEVEL_CHANGED &&
                           rich4_game_event_value(&e) == i);
        assert(pop(&q).type == GAME_EVENT_SCENE_REFRESH_REQUESTED);
    }
    before = s; rich4_game_event_queue_reset(&q);
    assert(!rich4_land_apply_command(&s, &command, &q));
    assert(memcmp(&s, &before, sizeof(s)) == 0 && q.count == 0);

    init_state(&s); s.land_count = 3;
    s.land_owner[0] = s.land_owner[1] = 1;
    s.land_group[0] = s.land_group[1] = 4; s.land_group[2] = 9;
    s.land_level[0] = 1; s.land_level[1] = 2;
    s.land_toll_by_level[0][1] = 100; s.land_toll_by_level[1][2] = 250;
    s.land_price_status[0] = 1;
    assert(rich4_land_toll(&s, 0, &value) && value == 1400);
    s.players[0].god_info = 1; s.players[0].god_type = 1;
    assert(rich4_land_inspect(&s, 0, 0, &action) &&
           action.kind == GAME_LAND_ACTION_CHARGE && action.amount == 700);
    s.players[0].god_type = 2;
    assert(rich4_land_inspect(&s, 0, 0, &action) && action.amount == 0);
    s.players[0].god_type = 5;
    assert(rich4_land_inspect(&s, 0, 0, &action) && action.amount == 2100);
    s.players[0].god_info = 0; s.players[0].god_type = 0;
    s.players[1].linked_player = 2;
    s.land_owner[2] = 2; s.land_group[2] = 4;
    s.land_toll_by_level[2][0] = 50;
    assert(rich4_land_inspect(&s, 0, 0, &action) &&
           action.kind == GAME_LAND_ACTION_CHARGE && action.amount == 1500 &&
           action.related_owner == 2 && action.owner_amount == 1400 &&
           action.related_amount == 100);
    s.players[0].cash = 2000;
    rich4_game_event_queue_reset(&q);
    assert(rich4_land_charge(&s, 0, 0, &value, &q) ==
               GAME_PAYMENT_COMMITTED && value == 1500);
    assert(s.players[0].cash == 500 && s.players[1].bank == 1400 &&
           s.players[2].bank == 100);
    e = pop(&q); assert(e.type == GAME_EVENT_MONEY_TRANSFERRED &&
                       rich4_game_event_target_ref(&e).id == 1 &&
                       rich4_game_event_value(&e) == 1400);
    e = pop(&q); assert(e.type == GAME_EVENT_MONEY_TRANSFERRED &&
                       rich4_game_event_target_ref(&e).id == 2 &&
                       rich4_game_event_value(&e) == 100);
    assert(q.count == 0);

    init_state(&s); s.land_count = 3;
    s.land_owner[0] = s.land_owner[1] = 1;
    s.land_group[0] = s.land_group[1] = 4; s.land_group[2] = 9;
    s.land_level[0] = 1; s.land_level[1] = 2;
    s.land_toll_by_level[0][1] = 100; s.land_toll_by_level[1][2] = 250;
    s.land_price_status[0] = 1;
    s.players[1].days_in_prison = 2;
    assert(rich4_land_inspect(&s, 0, 0, &action) &&
           action.kind == GAME_LAND_ACTION_COLLECTION_BLOCKED &&
           action.collection_block_reason ==
               GAME_PROPERTY_COLLECTION_BLOCK_PRISON &&
           action.amount == 1400);
    s.players[1].days_in_prison = 0;
    s.players[1].days_in_hospital = 2;
    assert(rich4_land_inspect(&s, 0, 0, &action) &&
           action.kind == GAME_LAND_ACTION_COLLECTION_BLOCKED &&
           action.collection_block_reason ==
               GAME_PROPERTY_COLLECTION_BLOCK_HOSPITAL);
    s.players[1].days_in_hospital = 0;
    s.players[1].days_sleep_walking = 1;
    assert(rich4_land_inspect(&s, 0, 0, &action) &&
           action.kind == GAME_LAND_ACTION_COLLECTION_BLOCKED &&
           action.collection_block_reason ==
               GAME_PROPERTY_COLLECTION_BLOCK_SLEEPWALKING);
    s.players[1].days_sleep_walking = 0;
    s.players[0].cash = 1000; s.players[0].bank = 1000;
    rich4_game_event_queue_reset(&q);
    assert(rich4_land_charge(&s, 0, 0, &value, &q) == GAME_PAYMENT_COMMITTED && value == 1400);
    assert(s.players[0].cash == 0 && s.players[0].bank == 600 && s.players[1].bank == 1400);
    s.land_owner[0] = 0; s.land_level[0] = 5;
    assert(rich4_land_inspect(&s, 0, 0, &action) && action.kind == GAME_LAND_ACTION_NONE);

    init_state(&s); s.land_count = 1; s.land_owner[0] = 1;
    s.land_group[0] = 0; s.land_toll_by_level[0][0] = 500;
    s.players[0].cash = 10; s.players[0].bank = 5;
    rich4_game_event_queue_reset(&q);
    assert(rich4_land_charge(&s, 0, 0, &value, &q) == GAME_PAYMENT_BANKRUPTCY);
    assert(s.players[0].play_state == 0 && s.players[1].bank == 15);

    init_state(&s); s.land_count = 1; s.land_owner[0] = 1;
    s.land_group[0] = 0; s.land_toll_by_level[0][0] = 1250;
    s.players[0].cash = 3000; s.players[0].card_inventory[0] = 20;
    rich4_game_event_queue_reset(&q);
    assert(rich4_land_charge_with_cards(&s, 0, 0, 1, -1, &value, &q) ==
           GAME_PAYMENT_COMMITTED);
    assert(value == 0 && s.players[0].cash == 3000 &&
           s.players[0].card_inventory[0] == 0);

    init_state(&s); s.land_count = 1; s.land_owner[0] = 1;
    s.land_group[0] = 0; s.land_toll_by_level[0][0] = 1250;
    s.players[0].cash = 100; s.players[2].cash = 3000;
    s.players[0].card_inventory[0] = 19;
    rich4_game_event_queue_reset(&q);
    assert(rich4_land_charge_with_cards(&s, 0, 0, 0, 2, &value, &q) ==
           GAME_PAYMENT_COMMITTED);
    assert(value == 2500 && s.players[2].cash == 500 &&
           s.players[1].bank == 2500);

    init_state(&s); s.land_count = 2; s.land_owner[0] = 1; s.land_owner[1] = 0;
    s.land_group[0] = 0; s.land_group[1] = 1;
    s.land_toll_by_level[0][0] = 500; s.land_price[1] = 1000;
    s.land_value[1] = 1000; s.players[0].cash = 100;
    rich4_game_event_queue_reset(&q);
    assert(rich4_land_charge(&s, 0, 0, &value, &q) == GAME_PAYMENT_BANKRUPTCY);
    assert(!s.liquidation_active && s.players[0].cash == 0);
    assert((s.players[0].play_state & 0xff) == 0);

    init_state(&s); s.land_count = 1; s.land_price[0] = INT_MAX;
    assert(!rich4_land_purchase_price(&s, 0, &value));
    s.land_price[0] = 10; s.land_price_per_level[0] = 1; s.players[2].cash = 1000;
    s.players[2].bank = 4200;
    assert(!rich4_land_inspect(&s, 2, 0, &action) || action.kind == GAME_LAND_ACTION_NONE);
    s.players[2].bank = 10000;
    assert(rich4_land_inspect(&s, 2, 0, &action) && action.kind == GAME_LAND_ACTION_PURCHASE);

    init_state(&s); s.land_count = 1; s.land_group[0] = 0;
    s.land_price[0] = 10; s.players[0].cash = 20;
    command = land_command(GAME_COMMAND_PURCHASE_PROPERTY, 0, 0);
    rich4_game_event_queue_reset(&q);
    while (q.count < GAME_EVENT_QUEUE_CAPACITY)
        assert(rich4_game_event_text_requested_emit(
            &q, 0, 0, 0, 0, 0));
    before = s;
    assert(!rich4_land_apply_command(&s, &command, &q));
    assert(memcmp(&s, &before, sizeof(s)) == 0 &&
           q.count == GAME_EVENT_QUEUE_CAPACITY);

    init_state(&s);
    s.land_count = 1;
    s.land_owner[0] = 1;
    s.land_group[0] = 0;
    s.land_price_status[0] = 0x51;
    assert(rich4_land_inspect(&s, 0, 0, &action) &&
           action.kind == GAME_LAND_ACTION_COLLECTION_BLOCKED &&
           action.collection_block_reason == GAME_PROPERTY_COLLECTION_BLOCK_SEALED);
    s.land_price_status[0] = 0;
    assert(rich4_land_inspect(&s, 0, 0, &action) &&
           action.kind == GAME_LAND_ACTION_CHARGE);
    puts("ordinary land purchase/upgrade/toll: ok");
    return 0;
}
