#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/god_money_flow.h"
#include "core/card.h"
#include "core/game_event_payload.h"
#include "core/god.h"

static GameSession session;
static GameState before;
static GameEventQueue events;

static void setup(void)
{
    int i;
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 4;
    session.state.price_index = 7;
    session.state.ai_rng_state = 12345;
    for (i = 0; i < 4; i++) {
        session.state.players[i].play_state = 1;
        session.state.players[i].cash = 20000;
        session.state.players[i].bank = 10000;
        session.state.players[i].linked_player = -1;
        session.state.players[i].attached_timed_bomb_slot = -1;
    }
    rich4_game_event_queue_reset(&events);
    rich4_audio_event_queue_init(&session.audio_events);
}

static void attach(int type)
{
    GameEvent event;
    int acquired = 0;
    assert(rich4_god_attach_replace(&session.state, 0, type, type, &events) ==
           GAME_GOD_APPLIED);
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type != GAME_EVENT_GOD_ACQUIRED) continue;
        acquired++;
        if (type == 15) assert(rich4_game_event_god_flags(&event) &
                               RICH4_GOD_ACQUIRED_DEATH_PENDING);
        session.presentation.god_acquisition.active = 1;
        session.presentation.god_acquisition.player = 0;
        session.presentation.god_acquisition.god_type = type;
        session.presentation.god_acquisition.object_slot = type;
        session.presentation.god_acquisition.money_pending =
            (rich4_game_event_god_flags(&event) &
             RICH4_GOD_ACQUIRED_MONEY_PENDING) != 0;
    }
    assert(acquired == 1);
}

static void test_settlement(void)
{
    const int types[] = {1, 2, 5, 6};
    int k;
    int i;
    for (k = 0; k < 4; k++) {
        int type = types[k];
        setup();
        attach(type);
        /* No money changes before 0x440706 returns. Price index is 7. */
        for (i = 0; i < 4; i++)
            assert(session.state.players[i].cash == 20000 &&
                   session.state.players[i].bank == 10000);
        assert(session.presentation.god_acquisition.money_pending);
        assert(rich4_god_resolve_money(&session.state, 0, type, type,
                                       731, &events) == GAME_GOD_APPLIED);
        assert(session.state.players[0].cash ==
               (type == 1 ? 22193 : type == 2 ? 20731 :
                type == 5 ? 17807 : 19269));
        for (i = 1; i < 4; i++) {
            assert(session.state.players[i].cash == (type == 1 ? 19269 : 20000));
            assert(session.state.players[i].bank == (type == 5 ? 10731 : 10000));
        }
    }
    setup(); attach(2); before = session.state;
    events.count = GAME_EVENT_QUEUE_CAPACITY;
    assert(rich4_god_resolve_money(&session.state, 0, 2, 2, 9999, &events) ==
           GAME_GOD_EVENT_CAPACITY);
    assert(memcmp(&session.state, &before, sizeof(before)) == 0);
    rich4_game_event_queue_reset(&events);
    assert(rich4_god_resolve_money(&session.state, 0, 2, 2, 10000, &events) ==
           GAME_GOD_INVALID);
    assert(rich4_god_resolve_money(&session.state, 0, 2, 2, 0, &events) ==
           GAME_GOD_APPLIED);
    assert(memcmp(&session.state, &before, sizeof(before)) == 0);
    setup(); attach(5);
    assert(rich4_god_resolve_money(&session.state, 0, 5, 5, 1000, &events) ==
           GAME_GOD_INVALID);
    session.state.players[0].cash = 100;
    session.state.players[0].bank = 0;
    assert(rich4_god_resolve_money(&session.state, 0, 5, 5, 999, &events) ==
           GAME_GOD_APPLIED);
    assert(session.state.players[0].play_state == 0);
    assert(session.state.players[0].god_info == 0);
    assert(session.state.players[2].bank == 10000);
}

static void test_cards_and_death(void)
{
    int i;
    setup(); session.state.card_amount[2] = 3; attach(3);
    assert(session.state.players[0].card_inventory[0] == 3);
    assert(session.state.card_amount[2] == 2);
    setup(); session.state.card_amount[4] = 3; attach(4);
    assert(rich4_card_inventory_count(&session.state, 0) == 2);
    assert(session.state.card_amount[4] == 1);
    setup(); session.state.players[0].card_inventory[0] = 7; attach(7);
    assert(rich4_card_inventory_count(&session.state, 0) == 0);
    assert(session.state.card_amount[6] == 1);
    setup();
    for (i = 0; i < 6; i++) session.state.players[0].card_inventory[i] = i + 1;
    attach(8);
    assert(rich4_card_inventory_count(&session.state, 0) == 3);
    assert(session.state.players[0].card_inventory[0] == 2 &&
           session.state.players[0].card_inventory[1] == 4 &&
           session.state.players[0].card_inventory[2] == 6);
    setup();
    /* The third selected ID is 2; 0x441343 removes its earlier occurrence. */
    session.state.players[0].card_inventory[0] = 1;
    session.state.players[0].card_inventory[1] = 2;
    session.state.players[0].card_inventory[2] = 3;
    session.state.players[0].card_inventory[3] = 4;
    session.state.players[0].card_inventory[4] = 2;
    session.state.players[0].card_inventory[5] = 5;
    attach(8);
    assert(session.state.players[0].card_inventory[0] == 4 &&
           session.state.players[0].card_inventory[1] == 2 &&
           session.state.players[0].card_inventory[2] == 5);
    setup();
    session.state.players[0].card_inventory[0] = 1;
    session.state.players[0].tool_inventory[0] = 2;
    session.state.players[0].tool_inventory[9] = 1;
    session.state.players[0].traffic_method = 2;
    session.state.players[0].ndices = 3;
    attach(15);
    assert(session.state.players[0].traffic_method == 2);
    assert(session.state.players[0].tool_inventory[0] == 2);
    assert(session.state.players[0].card_inventory[0] == 1);
    {
        GameState before = session.state;
        events.count = GAME_EVENT_QUEUE_CAPACITY;
        assert(rich4_god_resolve_death(&session.state, 0, 15, &events) ==
               GAME_GOD_EVENT_CAPACITY);
        assert(memcmp(&before, &session.state, sizeof(before)) == 0);
        rich4_game_event_queue_reset(&events);
        assert(rich4_god_resolve_death(&session.state, 0, 14, &events) == GAME_GOD_INVALID);
        assert(memcmp(&before, &session.state, sizeof(before)) == 0);
        assert(rich4_god_resolve_death(&session.state, 0, 15, &events) == GAME_GOD_APPLIED);
    }
    assert(session.state.players[0].traffic_method == 0 &&
           session.state.players[0].ndices == 1);
    for (i = 0; i < GAME_TOOL_COUNT; i++)
        assert(session.state.players[0].tool_inventory[i] == 0);
    assert(session.state.tool_amount[0] == 2 && session.state.tool_amount[5] == 1);
    assert(session.state.card_amount[0] == 1);
    assert(rich4_card_inventory_count(&session.state, 0) == 0);
}

static void test_death_vehicle_returns(void)
{
    int traffic;
    for (traffic = 0; traffic < 4; traffic++) {
        setup();
        session.state.players[0].traffic_method = traffic;
        session.state.players[0].ndices = 3;
        attach(15);
        assert(session.state.players[0].traffic_method == traffic);
        assert(rich4_god_resolve_death(&session.state, 0, 15, &events) == GAME_GOD_APPLIED);
        assert(session.state.players[0].traffic_method == 0);
        assert(session.state.players[0].ndices == 1);
        assert(session.state.tool_amount[4] == (traffic == 1));
        assert(session.state.tool_amount[5] == (traffic == 2));
        assert(session.state.tool_amount[11] == 0);
    }
}

static void test_modifiers(void)
{
    const int expected[][4] = {
        {1,-100,100,0}, {2,-200,150,0}, {3,-100,0,100}, {4,-200,0,150},
        {5,100,-60,0}, {6,200,-100,0}, {7,100,0,-60}, {8,200,0,-100},
        {9,-100,60,60}, {10,100,-60,-60}, {12,-500,0,0}, {15,1000,-200,-200}
    };
    size_t i;
    setup();
    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        attach(expected[i][0]);
        assert(session.state.players[0].god_income_modifier == expected[i][1]);
        assert(session.state.players[0].god_investment_modifier == expected[i][2]);
        assert(session.state.players[0].god_land_modifier == expected[i][3]);
    }
}

static void test_wheel(int type, int automatic)
{
    GameGodMoneyPresentationState *wheel;
    Rich4AudioEvent audio;
    uint32_t now = 100;
    int spinning = 0, stop_sound = 0, button_sound = 0;
    int i;
    setup(); attach(type);
    session.state.players[0].is_ai = automatic;
    rich4_god_money_begin(&session, now, &session.audio_events);
    wheel = &session.presentation.god_acquisition.money;
    assert(wheel->mode == (type == 1 || type == 5));
    assert(rich4_god_money_update(&session, now, &session.audio_events, &events));
    assert(wheel->ticks == 1);
    assert(rich4_god_money_update(&session, now + 29, &session.audio_events, &events));
    assert(wheel->ticks == 1);
    if (!automatic) {
        for (i = 0; i < 50; i++) {
            now += 30;
            assert(rich4_god_money_update(&session, now, &session.audio_events, &events));
        }
        assert(wheel->phase == 1);
        rich4_god_money_stop(&session);
    } else {
        rich4_god_money_stop(&session);
        assert(wheel->phase == 1);
    }
    before = session.state;
    for (i = 0; i < 1000 && session.presentation.god_acquisition.active; i++) {
        now += 30;
        rich4_god_money_update(&session, now, &session.audio_events, &events);
        if (wheel->active)
            assert(session.state.players[0].cash == 20000);
    }
    assert(i < 1000 && !wheel->active);
    assert(!session.presentation.god_acquisition.money_pending);
    assert(wheel->amount >= 0 && wheel->amount <= (wheel->mode ? 999 : 9999));
    for (i = wheel->mode; i < 4; i++) assert((wheel->digits[i] & 1) == 0);
    before = session.state;
    rich4_god_money_update(&session, now + 3000, &session.audio_events, &events);
    assert(memcmp(&session.state, &before, sizeof(before)) == 0);
    while (rich4_audio_event_pop(&session.audio_events, &audio)) {
        spinning += audio.type == RICH4_AUDIO_EVENT_ROULETTE;
        stop_sound += audio.type == RICH4_AUDIO_EVENT_ROULETTE_STOP;
        button_sound += audio.type == RICH4_AUDIO_EVENT_OK;
    }
    assert(spinning == 1 && stop_sound == 1 && button_sound == 1);
}

int main(void)
{
    test_settlement();
    test_cards_and_death();
    test_death_vehicle_returns();
    test_modifiers();
    test_wheel(1, 0); test_wheel(2, 0);
    test_wheel(5, 1); test_wheel(6, 1);
    puts("original god acquisition, number wheel and settlement: ok");
    return 0;
}
