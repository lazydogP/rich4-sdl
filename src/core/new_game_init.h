#ifndef RICH4_CORE_NEW_GAME_INIT_H
#define RICH4_CORE_NEW_GAME_INIT_H

#include <stddef.h>

#include "core/game_state.h"
#include "core/rng.h"

typedef struct NewGameInitConfig {
    int player_extra;
    int initial_fund;
    int traffic_method;
    int stage;
    int map;
    int day_limit;
    int win_target;
    int land_period_index;
    int calendar_start_day;
    int calendar_start_month;
    int calendar_start_year;
    int selected_characters[GAME_MAX_PLAYERS];
    /* 0=human; 1..3 select original +23 personalities 0..2. */
    int selected_ai[GAME_MAX_PLAYERS];
    unsigned char character_commit_before[12];
    const GameCharacterTemplate *character_templates;
    size_t character_template_count;
    const GameStockInfo *map_stocks;
    size_t map_stock_count;
    /* Injected original-compatible stream used by the 36/37 event shuffles. */
    Rich4Rng *special_event_rng;
    /* Real-map bootstrap performs 0x42915A after the eight object placements. */
    int defer_stock_initialization;
} NewGameInitConfig;

typedef struct NewGameInitReport {
    int selected_count;
    int num_players;
    unsigned char character_commit_before[12];
    unsigned char character_commit_after[12];
} NewGameInitReport;

int rich4_new_game_init(GameState *state,
                        const NewGameInitConfig *config,
                        NewGameInitReport *report);

#endif
