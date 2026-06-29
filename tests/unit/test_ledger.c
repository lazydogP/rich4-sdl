#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ledger.h"
#include "core/auction.h"
#include "core/game_event_payload.h"
#include "core/victory.h"

static void init_state(GameState *s, int players)
{
    int i;
    memset(s, 0, sizeof(*s));
    s->active = 1; s->player_count = players; s->winner_player = -1;
    for (i = 0; i < players; i++) s->players[i].play_state = 1;
    for (i = 0; i < GAME_MAX_MAP_PROPERTIES; i++) {
        s->land_owner[i] = -1; s->facility_owner[i] = -1;
    }
}

static int count_type(GameEventQueue *q, GameEventType type)
{
    int i, count = 0;
    for (i = 0; i < q->count; i++)
        if (q->events[(q->head + i) % GAME_EVENT_QUEUE_CAPACITY].type == type) count++;
    return count;
}

static void configure_beggar_route(GameState *s)
{
    s->tool_route_node_count = 8;
    s->tool_route_x[1] = 100; s->tool_route_y[1] = 100;
    s->tool_route_x[2] = 500; s->tool_route_y[2] = 500;
    s->tool_route_x[3] = 900; s->tool_route_y[3] = 500;
    s->tool_route_x[7] = 100; s->tool_route_y[7] = 100;
    s->tool_route_next[2][0] = 3;
    s->tool_route_next[3][0] = 2;
    s->tool_route_next[7][0] = 2;
}

int main(void)
{
    GameState s, before;
    GameEventQueue q;
    GameEvent first, second;
    int fee, i;
    init_state(&s, 2); rich4_game_event_queue_reset(&q); s.players[0].cash = 100;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_system(), 100, 0, &q) == GAME_PAYMENT_COMMITTED);
    assert(s.players[0].cash == 0 && s.system_cash == 100 && q.count == 1);
    init_state(&s, 2); rich4_game_event_queue_reset(&q); s.players[0].cash = 20; s.players[0].bank = 100;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_player(1), 70, 0, &q) == GAME_PAYMENT_COMMITTED);
    assert(s.players[0].cash == 0 && s.players[0].bank == 50 && s.players[1].bank == 70);
    init_state(&s, 2); rich4_game_event_queue_reset(&q); s.players[0].cash = 100; s.players[0].bank = 20;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_company(3), 70, GAME_PAYMENT_PAYER_BANK_FIRST, &q) == GAME_PAYMENT_COMMITTED);
    assert(s.players[0].bank == 0 && s.players[0].cash == 50 && s.company_cash[3] == 70);
    init_state(&s, 3); rich4_game_event_queue_reset(&q); s.players[0].cash = 250;
    assert(rich4_ledger_pay_split(
               &s, 0, rich4_ledger_player(1), 100,
               rich4_ledger_player(2), 100, 0, &q) ==
           GAME_PAYMENT_COMMITTED);
    assert(s.players[0].cash == 50 && s.players[1].bank == 100 &&
           s.players[2].bank == 100 && count_type(&q, GAME_EVENT_MONEY_TRANSFERRED) == 2);
    first = q.events[q.head];
    second = q.events[(q.head + 1) % GAME_EVENT_QUEUE_CAPACITY];
    assert(rich4_game_event_target_ref(&first).id == 1 &&
           rich4_game_event_value(&first) == 100 &&
           rich4_game_event_target_ref(&second).id == 2 &&
           rich4_game_event_value(&second) == 100);
    init_state(&s, 3); rich4_game_event_queue_reset(&q); s.players[0].cash = 150;
    assert(rich4_ledger_pay_split(
               &s, 0, rich4_ledger_player(1), 100,
               rich4_ledger_player(2), 100, 0, &q) ==
           GAME_PAYMENT_BANKRUPTCY);
    assert(s.players[0].cash == 0 && s.players[0].play_state == 0 &&
           s.players[1].bank == 100 && s.players[2].bank == 50 &&
           count_type(&q, GAME_EVENT_MONEY_TRANSFERRED) == 2);
    first = q.events[q.head];
    second = q.events[(q.head + 1) % GAME_EVENT_QUEUE_CAPACITY];
    assert(rich4_game_event_target_ref(&first).id == 1 &&
           rich4_game_event_value(&first) == 100 &&
           rich4_game_event_target_ref(&second).id == 2 &&
           rich4_game_event_value(&second) == 50);
    assert(rich4_ledger_adjust_fee(101, 1, &fee) && fee == 50);
    assert(rich4_ledger_adjust_fee(101, 2, &fee) && fee == 0);
    assert(rich4_ledger_adjust_fee(100, 5, &fee) && fee == 150);
    assert(rich4_ledger_adjust_fee(100, 6, &fee) && fee == 200);
    assert(!rich4_ledger_adjust_fee(INT_MAX, 6, &fee));
    init_state(&s, 2); rich4_game_event_queue_reset(&q); s.players[0].cash = 1;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_system(), 0, 0, &q) == GAME_PAYMENT_COMMITTED);
    assert(q.count == 1 && s.players[0].cash == 1);
    init_state(&s, 2); rich4_game_event_queue_reset(&q); s.players[0].cash = 100; s.players[1].play_state = 0; before = s;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_player(1), 20, 0, &q) == GAME_PAYMENT_REJECTED);
    assert(memcmp(&s, &before, sizeof(s)) == 0 && q.count == 0);
    init_state(&s, 2); rich4_game_event_queue_reset(&q);
    s.players[0].cash = 1; s.system_cash = INT_MAX; before = s;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_system(), 1, 0, &q) == GAME_PAYMENT_REJECTED);
    assert(memcmp(&s, &before, sizeof(s)) == 0 && q.count == 0);
    init_state(&s, 2); rich4_game_event_queue_reset(&q);
    s.players[0].cash = 10; s.land_count = 1; s.land_owner[0] = 0; s.land_value[0] = 100;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_system(), 80, 0, &q) == GAME_PAYMENT_BANKRUPTCY);
    assert(!s.liquidation_active && s.land_owner[0] == -1 && s.land_value[0] == 0);
    assert(s.players[0].cash == 0 && s.system_cash == 10);
    assert((s.players[0].play_state & 0xff) == 0);
    assert(count_type(&q, GAME_EVENT_PROPERTY_OWNER_CHANGED) == 1);
    assert(count_type(&q, GAME_EVENT_MONEY_TRANSFERRED) == 1);
    init_state(&s, 2); rich4_game_event_queue_reset(&q);
    s.players[0].cash = 10; s.players[0].bank = 5; s.players[0].loan = 3;
    s.land_count = 1; s.land_owner[0] = 0; s.land_value[0] = 7;
    s.facility_count = 1; s.facility_owner[0] = 0; s.facility_value[0] = 8;
    s.facility_last_change[0] = 1234;
    s.player_stock_amount[0][0] = 2; s.stocks[0].f8 = 30; s.stocks[0].f10 = 20;
    s.stocks[0].f20 = 4.0f;
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++)
        s.players[0].card_inventory[i] = (i % GAME_CARD_COUNT) + 1;
    for (i = 0; i < GAME_PLAYER_TOOL_SLOTS; i++)
        s.players[0].tool_inventory[i] = (unsigned char)(i + 1);
    s.players[0].attached_timed_bomb_slot = 20;
    s.tool_placed_objects[20].kind = 0x12;
    s.tool_placed_objects[20].attached_player = 1;
    s.players[0].days_in_prison = 2; s.news_event_occupant[4] = 1;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_player(1), 100, 0, &q) == GAME_PAYMENT_BANKRUPTCY);
    assert(s.players[0].play_state == 0 && rich4_player_total_assets(&s, 0, NULL) == 0);
    assert(s.land_owner[0] == -1 && s.facility_owner[0] == -1 &&
           s.facility_last_change[0] == 0);
    assert(s.player_stock_amount[0][0] == 0 && s.stocks[0].f8 == 32 &&
           s.stocks[0].f10 == 22 && s.system_cash == 8);
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++)
        assert(s.players[0].card_inventory[i] == 0);
    for (i = 0; i < GAME_PLAYER_TOOL_SLOTS; i++)
        assert(s.players[0].tool_inventory[i] == 0);
    assert(s.players[0].attached_timed_bomb_slot == -1 &&
           s.tool_placed_objects[20].kind == 0 && s.players[0].days_in_prison == 0);
    assert(s.news_event_occupant[4] == 0 && s.players[1].bank == 15);
    assert(s.game_over && s.winner_player == 1 && count_type(&q, GAME_EVENT_GAME_ENDED) == 1);
    init_state(&s, 2); rich4_game_event_queue_reset(&q);
    s.players[0].cash = 1; s.player_stock_amount[0][0] = 1;
    s.stocks[0].f8 = UINT16_MAX; s.stocks[0].f20 = 1.0f;
    before = s;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_system(), 2, 0, &q) ==
           GAME_PAYMENT_REJECTED);
    assert(memcmp(&s, &before, sizeof(s)) == 0 && q.count == 0);
    rich4_game_event_queue_reset(&q); s.players[1].cash = 5;
    assert(rich4_ledger_pay(&s, 1, rich4_ledger_system(), 1, 0, &q) == GAME_PAYMENT_COMMITTED);
    assert(count_type(&q, GAME_EVENT_GAME_ENDED) == 0);
    init_state(&s, 3); rich4_game_event_queue_reset(&q);
    s.players[0].cash = 1;
    s.players[2].is_ai = 1;
    s.land_count = 5;
    for (i = 0; i < 5; i++) {
        s.land_owner[i] = 0;
        s.land_value[i] = 100;
        s.land_price[i] = 100;
    }
    assert(rich4_ledger_pay(
               &s, 0, rich4_ledger_system(), 2, 0, &q) ==
           GAME_PAYMENT_BANKRUPTCY);
    assert(count_type(&q, GAME_EVENT_AUCTION_REQUESTED) == 3);
    {
        unsigned int seen = 0;
        int requests = 0;
        for (i = 0; i < (int)q.count; i++) {
            const GameEvent *event =
                &q.events[(q.head + (size_t)i) % GAME_EVENT_QUEUE_CAPACITY];
            if (event->type == GAME_EVENT_AUCTION_REQUESTED) {
                GameDomainRef target =
                    rich4_game_event_auction_property(event);
                assert(target.kind == GAME_DOMAIN_LAND &&
                       target.id >= 0 && target.id < 5);
                assert((seen & (1u << target.id)) == 0);
                seen |= 1u << target.id;
                assert(rich4_game_event_auction_seller(event) == -1);
                assert(rich4_game_event_auction_trigger(event) ==
                       GAME_AUCTION_TRIGGER_BANKRUPTCY);
                requests++;
            }
        }
        assert(requests == 3);
    }
    init_state(&s, 2); rich4_game_event_queue_reset(&q);
    s.players[0].cash = 20; s.players[1].cash = 90; s.win_target = 100;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_player(1), 10, GAME_PAYMENT_RECEIVER_CASH, &q) == GAME_PAYMENT_COMMITTED);
    assert(s.game_over && s.winner_player == 1 && count_type(&q, GAME_EVENT_GAME_ENDED) == 1);
    init_state(&s, 2); rich4_game_event_queue_reset(&q); s.players[0].cash = 10;
    while (q.count < GAME_EVENT_QUEUE_CAPACITY)
        assert(rich4_game_event_text_requested_emit(
            &q, 0, 0, 0, 0, 0));
    before = s;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_system(), 5, 0, &q) == GAME_PAYMENT_REJECTED);
    assert(memcmp(&s, &before, sizeof(s)) == 0);
    init_state(&s, 3); rich4_game_event_queue_reset(&q);
    s.price_index = 2; s.players[0].node = 1; s.players[0].cash = 2500;
    s.players[1].node = 1; s.players[1].play_state = 0;
    configure_beggar_route(&s);
    assert(rich4_ledger_find_beggar(&s, 0, 1) == 1);
    assert(rich4_ledger_beggar_amount(&s) == 2000);
    assert(rich4_ledger_beggar_donate(&s, 0, 1, 1, 2000, &q) ==
           GAME_PAYMENT_COMMITTED);
    assert(s.players[0].cash == 500 && s.system_cash == 2000);
    assert(s.players[1].play_state == 0 && s.players[1].node != 1 &&
           s.players[1].next_node != 0);
    assert(count_type(&q, GAME_EVENT_MONEY_TRANSFERRED) == 1);
    init_state(&s, 3); rich4_game_event_queue_reset(&q);
    s.price_index = 2; s.players[0].node = 7; s.players[0].cash = 100;
    s.players[1].node = 7; s.players[1].play_state = 0;
    configure_beggar_route(&s);
    assert(rich4_ledger_beggar_donate(&s, 0, 1, 7, 2000, &q) ==
           GAME_PAYMENT_BANKRUPTCY);
    assert(s.players[0].play_state == 0 && s.players[0].node == 7);
    assert(s.players[1].play_state == 0 && s.players[1].node != 7);
    puts("ledger/payment/bankruptcy: ok");
    return 0;
}
