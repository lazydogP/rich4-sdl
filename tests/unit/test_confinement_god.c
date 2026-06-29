#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/confinement.h"
#include "core/game_event_payload.h"
#include "core/god.h"
#include "core/status_countdown.h"

static void setup(GameState *s)
{
    memset(s, 0, sizeof(*s));
    s->active = 1;
    s->player_count = 2;
    s->prison_node = 10;
    s->hospital_node = 20;
    s->players[0].play_state = 1;
    s->players[1].play_state = 1;
    s->players[0].linked_player = -1;
    s->players[1].linked_player = -1;
    s->players[0].attached_timed_bomb_slot = -1;
    s->players[1].attached_timed_bomb_slot = -1;
}

int main(void)
{
    GameState s, before, restored;
    GameEventQueue q;
    StatusTickResult tick;
    StatusEventBatch batch;
    GameStatusSnapshot snapshot, corrupt;
    int i;

    setup(&s);
    rich4_game_event_queue_reset(&q);
    assert(rich4_confinement_enter(&s, 0, GAME_CONFINEMENT_PRISON, 2, &q) ==
           GAME_CONFINEMENT_APPLIED);
    assert(s.players[0].node == 10 && s.players[0].days_in_prison == 2 &&
           s.players[0].confinement_actor_type == GAME_CONFINEMENT_PRISON &&
           s.players[0].total_confinement_days == 2 && q.count == 5);
    rich4_game_event_queue_reset(&q);
    rich4_status_tick_player_events(&s.players[0], 0, &tick, &q);
    assert(s.players[0].days_in_prison == 1 && q.count == 1 &&
           q.events[q.head].type == GAME_EVENT_STATUS_CHANGED);
    rich4_game_event_queue_reset(&q);
    rich4_status_tick_player_events(&s.players[0], 0, &tick, &q);
    assert(s.players[0].days_in_prison == GAME_STATUS_PENDING_BIT &&
           q.events[q.head].type == GAME_EVENT_STATUS_EXPIRED);
    rich4_game_event_queue_reset(&q);
    assert(rich4_status_consume_pending_events(&s, 0, &batch, &q) == 1);
    assert(s.players[0].days_in_prison == 0 && s.players[0].node == 10 &&
           s.players[0].confinement_actor_type == GAME_CONFINEMENT_NONE &&
           q.count == 3 && batch.events[0].effect_result == STATUS_EFFECT_APPLIED);

    rich4_game_event_queue_reset(&q);
    assert(rich4_confinement_enter(&s, 0, GAME_CONFINEMENT_HOSPITAL, 3, &q) ==
           GAME_CONFINEMENT_APPLIED);
    assert(s.players[0].node == 20 && s.players[0].days_in_hospital == 3 &&
           s.players[0].total_confinement_days == 5);
    rich4_game_event_queue_reset(&q);
    assert(rich4_confinement_enter(&s, 4, GAME_CONFINEMENT_HOSPITAL, 1, &q) ==
           GAME_CONFINEMENT_APPLIED);
    assert(s.special_player_confinement[0] == GAME_CONFINEMENT_HOSPITAL &&
           s.special_player_f10[0] == GAME_CONFINEMENT_HOSPITAL &&
           s.special_player_actor_type[0] == 0);
    s.special_player_f10[0] = 2;
    s.special_player_previous_node[0] = 11;
    rich4_game_event_queue_reset(&q);
    assert(rich4_confinement_release(&s, 4, GAME_CONFINEMENT_HOSPITAL, &q) ==
           GAME_CONFINEMENT_APPLIED);
    assert(s.special_player_node[0] == 20 &&
           s.special_player_previous_node[0] == 0 &&
           s.special_player_f10[0] == 0 &&
           s.special_player_actor_type[0] == GAME_CONFINEMENT_HOSPITAL &&
           s.special_player_confinement[0] == GAME_CONFINEMENT_NONE);

    setup(&s);
    s.players[0].points = 330;
    s.price_index = 2;
    s.players[1].days_in_prison = 2;
    s.players[1].confinement_actor_type = GAME_CONFINEMENT_PRISON;
    s.players[1].relation_balance[0] = 450;
    assert(rich4_confinement_visit_cost(
               (GameDomainRef){GAME_DOMAIN_PLAYER, 1}) == 30);
    assert(rich4_confinement_visit_cost(
               (GameDomainRef){GAME_DOMAIN_SPECIAL_PLAYER, 0}) == 300);
    assert(rich4_confinement_visit_candidate(
               &s, (GameDomainRef){GAME_DOMAIN_PLAYER, 1},
               GAME_CONFINEMENT_PRISON));
    rich4_game_event_queue_reset(&q);
    assert(rich4_confinement_visit_release(
               &s, 0, (GameDomainRef){GAME_DOMAIN_PLAYER, 1},
               GAME_CONFINEMENT_PRISON, &q) == GAME_CONFINEMENT_APPLIED);
    assert(s.players[0].points == 300 &&
           s.players[1].days_in_prison == GAME_STATUS_PENDING_BIT &&
           s.players[1].relation_balance[0] == 50 &&
           q.count == 4);
    s.special_player_confinement[0] = GAME_CONFINEMENT_HOSPITAL;
    s.special_player_f10[0] = 2;
    s.special_player_play_state[0] = 3;
    s.special_player_previous_node[0] = 11;
    rich4_game_event_queue_reset(&q);
    assert(rich4_confinement_visit_release(
               &s, 0, (GameDomainRef){GAME_DOMAIN_SPECIAL_PLAYER, 0},
               GAME_CONFINEMENT_HOSPITAL, &q) == GAME_CONFINEMENT_APPLIED);
    assert(s.players[0].points == 0 &&
           s.special_player_confinement[0] == GAME_CONFINEMENT_NONE &&
           s.special_player_node[0] == s.hospital_node &&
           s.special_player_previous_node[0] == 0 &&
           s.special_player_f10[0] == 0 &&
           s.special_player_play_state[0] == 0 &&
           q.count == 4);

    setup(&s);
    rich4_game_event_queue_reset(&q);
    assert(rich4_god_attach(&s, 0, 9, 7, &q) == GAME_GOD_APPLIED);
    assert(s.players[0].god_info == 9 && s.players[0].god_type == 7 &&
           s.players[0].god_income_modifier == 100 &&
           s.players[0].god_investment_modifier == 0 &&
           s.players[0].god_land_modifier == -60 &&
           !rich4_god_blocks_trade(&s.players[0]) && q.count == 4 &&
           q.events[q.head].type == GAME_EVENT_GOD_ACQUIRED &&
           rich4_game_event_god_type(&q.events[q.head]) == 7 &&
           rich4_game_event_god_object_slot(&q.events[q.head]) == 9 &&
           rich4_game_event_god_previous_type(&q.events[q.head]) == 0 &&
           (rich4_game_event_god_flags(&q.events[q.head]) &
            RICH4_GOD_ACQUIRED_FIRST) != 0);
    assert(rich4_god_attach(&s, 1, 9, 5, &q) == GAME_GOD_OCCUPIED);
    rich4_game_event_queue_reset(&q);
    assert(rich4_god_send_away(&s, 0, &q) == GAME_GOD_APPLIED);
    assert(s.players[0].god_info == 0 && s.players[0].god_type == 0 &&
           s.players[0].god_income_modifier == 0 &&
           s.players[0].god_investment_modifier == 0 &&
           s.players[0].god_land_modifier == 0 && q.count == 3);
    s.players[0].god_info = 4; s.players[0].god_type = 1;
    assert(rich4_god_send_away(&s, 0, &q) == GAME_GOD_NOT_SENDABLE);

    setup(&s);
    s.price_index = 1;
    s.facility_count = 1;
    s.route_object_raw[3] = 0xfa1;
    s.facility_owner[0] = -1;
    s.facility_type[0] = 1;
    s.facility_level[0] = 1;
    s.facility_price[0] = 100;
    s.facility_price_per_level[0] = 10;
    s.facility_value[0] = 110;
    s.players[0].god_info = 9;
    s.players[0].god_type = 9;
    rich4_game_event_queue_reset(&q);
    assert(rich4_god_apply_landing_effect(&s, 0, 3, &q));
    assert(s.facility_level[0] == 2 && s.facility_value[0] == 120 &&
           q.count == 2);

    setup(&s);
    s.price_index = 1;
    s.land_count = 1;
    s.route_object_raw[3] = 0x7d1;
    s.land_owner[0] = 1;
    s.land_type[0] = 0;
    s.land_level[0] = 1;
    s.land_price[0] = 200;
    s.land_price_per_level[0] = 10;
    s.land_value[0] = 210;
    s.fixed_asset_value[1] = 210;
    s.players[0].god_info = 9;
    s.players[0].god_type = 9;
    rich4_game_event_queue_reset(&q);
    assert(rich4_god_apply_landing_effect(&s, 0, 3, &q));
    assert(s.land_level[0] == 2 && s.land_value[0] == 220 &&
           s.fixed_asset_value[1] == 220 && q.count == 2);

    /* 0x40B110 also increments an empty type-1 ordinary-land record once. */
    setup(&s);
    s.price_index = 1;
    s.land_count = 1;
    s.route_object_raw[3] = 0x7d1;
    s.land_owner[0] = -1;
    s.land_type[0] = 1;
    s.land_level[0] = 0;
    s.land_price[0] = 100;
    s.land_price_per_level[0] = 10;
    s.land_value[0] = 100;
    s.players[0].god_info = 9;
    s.players[0].god_type = 9;
    rich4_game_event_queue_reset(&q);
    assert(rich4_god_apply_landing_effect(&s, 0, 3, &q));
    assert(s.land_level[0] == 1 && s.land_value[0] == 110 && q.count == 2);

    setup(&s);
    s.price_index = 2;
    s.land_count = 1;
    s.route_object_raw[3] = 0x7d1;
    s.land_owner[0] = 1;
    s.land_level[0] = 1;
    s.land_price[0] = 200;
    s.land_price_per_level[0] = 10;
    s.land_value[0] = 220;
    s.fixed_asset_value[1] = 220;
    s.players[0].god_info = 10;
    s.players[0].god_type = 10;
    rich4_game_event_queue_reset(&q);
    assert(rich4_god_apply_landing_effect(&s, 0, 3, &q));
    assert(s.players[1].relation_balance[0] == 60 && s.land_level[0] == 0 &&
           s.land_value[0] == 200 && s.fixed_asset_value[1] == 200 && q.count == 2);

    setup(&s);
    s.price_index = 2;
    s.land_count = 1;
    s.route_object_raw[3] = 0x7d1;
    s.land_owner[0] = 1;
    s.land_level[0] = 2;
    s.land_price[0] = 200;
    s.land_price_per_level[0] = 10;
    s.land_value[0] = 220;
    s.fixed_asset_value[1] = 220;
    s.players[0].god_info = 12;
    s.players[0].god_type = 12;
    rich4_game_event_queue_reset(&q);
    assert(rich4_god_apply_landing_effect(&s, 0, 3, &q));
    assert(s.land_owner[0] == 0 && s.fixed_asset_value[1] == 0 &&
           s.fixed_asset_value[0] == 220 &&
           s.players[1].relation_balance[0] == 160 && q.count == 2);

    /* 0x40F68B..0x40F82F takes unowned level-zero properties too. */
    setup(&s);
    s.price_index = 1;
    s.land_count = 1;
    s.route_object_raw[3] = 0x7d1;
    s.land_owner[0] = -1;
    s.land_level[0] = 0;
    s.land_price[0] = 100;
    s.land_price_per_level[0] = 10;
    s.land_value[0] = 100;
    s.players[0].cash = 1000;
    s.players[0].god_info = 12;
    s.players[0].god_type = 12;
    rich4_game_event_queue_reset(&q);
    assert(rich4_god_apply_landing_effect(&s, 0, 3, &q));
    assert(s.land_owner[0] == 0 && s.fixed_asset_value[0] == 100 &&
           s.players[0].cash == 1000 && q.count == 2);

    setup(&s);
    s.price_index = 1;
    s.facility_count = 1;
    s.route_object_raw[3] = 0xfa1;
    s.facility_owner[0] = -1;
    s.facility_type[0] = 1;
    s.facility_level[0] = 0;
    s.facility_price[0] = 100;
    s.facility_price_per_level[0] = 10;
    s.facility_value[0] = 100;
    s.players[0].god_info = 12;
    s.players[0].god_type = 12;
    rich4_game_event_queue_reset(&q);
    assert(rich4_god_apply_landing_effect(&s, 0, 3, &q));
    assert(s.facility_owner[0] == 0 && s.fixed_asset_value[0] == 100 &&
           q.count == 2);

    setup(&s);
    rich4_game_event_queue_reset(&q);
    for (i = 0; i < GAME_EVENT_QUEUE_CAPACITY - 2; i++)
        assert(rich4_game_event_text_requested_emit(
            &q, 0, i, 0, 0, 0));
    before = s;
    assert(rich4_confinement_enter(&s, 0, GAME_CONFINEMENT_PRISON, 1, &q) ==
           GAME_CONFINEMENT_EVENT_CAPACITY);
    assert(memcmp(&s, &before, sizeof(s)) == 0);

    setup(&s);
    s.players[0].node = 10; s.players[0].days_in_prison = 2;
    s.players[0].confinement_actor_type = GAME_CONFINEMENT_PRISON;
    s.players[0].god_info = 9; s.players[0].god_type = 5;
    assert(rich4_status_snapshot_export(&s, &snapshot));
    setup(&restored);
    assert(rich4_status_snapshot_import(&restored, &snapshot));
    assert(restored.players[0].days_in_prison == 2 &&
           restored.players[0].confinement_actor_type == GAME_CONFINEMENT_PRISON &&
           restored.players[0].god_info == 9 && restored.players[0].god_type == 5);
    corrupt = snapshot; corrupt.players[0].god_type = GAME_GOD_TYPE_COUNT + 1;
    before = restored;
    assert(!rich4_status_snapshot_import(&restored, &corrupt));
    assert(memcmp(&restored, &before, sizeof(restored)) == 0);

    puts("confinement, god relation and status snapshot: ok");
    return 0;
}
