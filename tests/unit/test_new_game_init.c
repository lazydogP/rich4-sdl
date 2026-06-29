#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/confinement.h"
#include "core/new_game_init.h"

int main(void)
{
    GameCharacterTemplate templates[12];
    GameStockInfo stocks[GAME_STOCK_COUNT];
    NewGameInitConfig config;
    NewGameInitReport report;
    GameState a;
    GameState b;
    Rich4Rng rng;
    int i;
    memset(templates, 0, sizeof(templates));
    memset(stocks, 0, sizeof(stocks));
    memset(&config, 0, sizeof(config));
    templates[0].init_cash_ratio = 30;
    templates[1].init_cash_ratio = 70;
    templates[1].f23 = 2;
    templates[1].color = 0x1234;
    for (i = 0; i < GAME_STOCK_COUNT; i++) stocks[i].f12 = (float)(i + 1);
    config.player_extra = 1;
    config.initial_fund = 100000;
    config.traffic_method = 2;
    config.selected_characters[0] = 0;
    config.selected_characters[1] = 1;
    config.selected_characters[2] = 0;
    config.selected_ai[1] = 3;
    config.character_commit_before[0] = 4;
    config.character_commit_before[1] = 2;
    config.character_templates = templates;
    config.character_template_count = 12;
    config.map_stocks = stocks;
    config.map_stock_count = GAME_STOCK_COUNT;
    rich4_rng_state_seed(&rng, 123);
    config.special_event_rng = &rng;
    assert(rich4_new_game_init(&a, &config, &report));
    assert(a.player_count == 3 && report.selected_count == 1 && report.num_players == 3);
    assert(a.players[0].cash == 50000 && a.players[0].bank == 50000);
    assert(a.players[1].cash == 70000 && a.players[1].bank == 30000);
    assert(a.players[0].traffic_method == 2 && a.players[0].ndices == 3);
    assert(a.players[0].is_ai == 0 && a.players[0].play_state == 1);
    assert(a.players[1].is_ai == 1 && a.players[1].play_state == 2);
    assert(a.players[1].ai_personality == 2);
    assert(a.players[1].ai_cash_ratio == 70);
    assert(report.character_commit_before[0] == 4 && report.character_commit_after[0] == 2);
    assert(report.character_commit_before[1] == 2 && report.character_commit_after[1] == 0);
    assert(a.card_amount[0] == 1 && a.card_amount[6] == 8 && a.card_amount[29] == 3);
    assert(a.tool_amount[0] == 7 && a.tool_amount[1] == 7 &&
           a.tool_amount[2] == 7 && a.tool_amount[3] == 7 &&
           a.tool_amount[7] == 7 && a.tool_amount[8] == 0);
    for (i = 0; i < a.player_count; i++) {
        assert(a.players[i].tool_inventory[0] == 1 &&
               a.players[i].tool_inventory[1] == 1 &&
               a.players[i].tool_inventory[2] == 1 &&
               a.players[i].tool_inventory[3] == 1 &&
               a.players[i].tool_inventory[7] == 1 &&
               a.players[i].tool_inventory[8] == 1);
        assert(a.players[i].tool_inventory[4] == 0 &&
               a.players[i].tool_inventory[5] == 0 &&
               a.players[i].tool_inventory[6] == 0 &&
               a.players[i].tool_inventory[9] == 0);
    }
    assert(a.special_player_f10[0] == 1 && a.special_player_f10[4] == 3);
    assert(a.special_player_confinement[0] == GAME_CONFINEMENT_PRISON &&
           a.special_player_confinement[1] == GAME_CONFINEMENT_PRISON &&
           a.special_player_confinement[2] == GAME_CONFINEMENT_HOSPITAL &&
           a.special_player_confinement[3] == GAME_CONFINEMENT_HOSPITAL &&
           a.special_player_confinement[4] == GAME_CONFINEMENT_NONE);
    assert(rich4_confinement_visit_candidate(
               &a, rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER, 0),
               GAME_CONFINEMENT_PRISON));
    assert(rich4_confinement_visit_candidate(
               &a, rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER, 2),
               GAME_CONFINEMENT_HOSPITAL));
    assert(a.stocks[11].f12 == 12.0f && a.stock_market_total_x10 == 780);
    rich4_rng_state_seed(&rng, 123);
    assert(rich4_new_game_init(&b, &config, NULL));
    assert(memcmp(&a, &b, sizeof(a)) == 0);
    assert(a.news_event_cursor == 0 && a.fortune_event_cursor == 0 &&
           a.news_event_order[0] < GAME_NEWS_EVENT_COUNT &&
           a.fortune_event_order[0] < GAME_FORTUNE_EVENT_COUNT);
    config.player_extra = 0;
    assert(rich4_new_game_init(&b, &config, &report));
    assert(b.player_count == 2 && report.selected_count == 0 && report.num_players == 2);
    config.player_extra = 2;
    config.selected_ai[0] = 1;
    config.selected_ai[1] = 2;
    config.selected_ai[2] = 3;
    assert(rich4_new_game_init(&b, &config, &report));
    assert(b.player_count == 4 && report.selected_count == 2 && report.num_players == 4);
    assert(b.players[0].ai_personality == 0 &&
           b.players[1].ai_personality == 1 &&
           b.players[2].ai_personality == 2);
    assert(b.players[3].traffic_method == 2 && b.players[3].ndices == 3);
    puts("new game initialization: ok");
    return 0;
}
