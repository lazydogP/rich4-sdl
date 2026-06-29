#include "core/new_game_init.h"

#include <string.h>

#include "core/card.h"
#include "core/special_event.h"
#include "core/stock.h"

static const uint8_t tool_initial_amount[GAME_TOOL_COUNT] = {
    10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0
};

/* sub_406DE7 calls sub_445A4D with these one-based original tool IDs for
 * every active player.  The port stores the corresponding inventory at
 * original_id - 1. */
static const int default_tool_ids[] = {1, 2, 3, 4, 8, 9};

static const int special_player_defaults[GAME_SPECIAL_PLAYER_COUNT] = {1, 1, 2, 2, 3};

int rich4_new_game_init(GameState *state,
                        const NewGameInitConfig *config,
                        NewGameInitReport *report)
{
    int extra;
    int traffic;
    int i;
    if (state == NULL || config == NULL || config->character_templates == NULL ||
        config->character_template_count == 0 || config->map_stocks == NULL ||
        config->map_stock_count < GAME_STOCK_COUNT) return 0;
    memset(state, 0, sizeof(*state));
    state->ai_rng_state = config->special_event_rng != NULL
        ? config->special_event_rng->state : 1u;
    if (report != NULL) memset(report, 0, sizeof(*report));
    extra = config->player_extra;
    if (extra < 0) extra = 0;
    if (extra > 2) extra = 2;
    traffic = config->traffic_method;
    if (traffic < 0 || traffic > 2) traffic = 0;
    state->active = 1;
    state->player_count = extra + 2;
    state->current_player = 0;
    state->last_special_turn_index = -1;
    state->win_candidate_player = -1;
    state->winner_player = -1;
    state->liquidation_player = -1;
    state->liquidation_payee_id = -1;
    state->prison_node = -1;
    state->hospital_node = -1;
    state->initial_fund = config->initial_fund;
    state->day_limit = config->day_limit;
    state->win_target = config->win_target;
    state->land_period_index = config->land_period_index;
    state->calendar_start_day = config->calendar_start_day;
    state->calendar_start_month = config->calendar_start_month;
    state->calendar_start_year = config->calendar_start_year;
    state->initial_traffic_index = traffic;
    state->stage = config->stage;
    state->map = config->map;
    state->price_index = 1;
    for (i = 0; i < GAME_CARD_COUNT; i++)
        state->card_amount[i] = rich4_card_catalog[i].initial_pool;
    memcpy(state->tool_amount, tool_initial_amount, sizeof(state->tool_amount));
    memcpy(state->special_player_f10, special_player_defaults, sizeof(state->special_player_f10));
    memset(state->land_owner, 0xff, sizeof(state->land_owner));
    memset(state->land_group, 0xff, sizeof(state->land_group));
    memset(state->facility_owner, 0xff, sizeof(state->facility_owner));
    memset(state->stock_company_auxiliary, 0xff,
           sizeof(state->stock_company_auxiliary));
    memset(state->stock_company_owner, 0xff,
           sizeof(state->stock_company_owner));
    memset(state->stock_company_ranking, 0xff,
           sizeof(state->stock_company_ranking));
    memcpy(state->stocks, config->map_stocks, sizeof(state->stocks));
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        state->stock_market_total_x10 += (int)(state->stocks[i].f12 * 10.0f);
    }
    for (i = 0; i < GAME_MAX_PLAYERS; i++)
        state->players[i].attached_timed_bomb_slot = -1;
    for (i = 0; i < GAME_SPECIAL_PLAYER_COUNT; i++) {
        state->special_player_node[i] = -1;
        /* init_new_game clears both eight-slot occupancy tables, then sets
         * prison slots 4/5 and hospital slots 6/7.  Their special-record
         * f10 defaults are respectively 1/1 and 2/2. */
        if (state->special_player_f10[i] == GAME_CONFINEMENT_PRISON)
            state->special_player_confinement[i] =
                GAME_CONFINEMENT_PRISON;
        else if (state->special_player_f10[i] ==
                 GAME_CONFINEMENT_HOSPITAL)
            state->special_player_confinement[i] =
                GAME_CONFINEMENT_HOSPITAL;
    }
    for (i = 0; i < state->player_count; i++) {
        int character = config->selected_characters[i];
        int ai_selection = config->selected_ai[i];
        int is_ai = ai_selection != 0;
        int initial_cash_ratio;
        int default_tool_index;
        const GameCharacterTemplate *template;
        GamePlayerState *player = &state->players[i];
        player->linked_player = -1;
        if (character < 0 || (size_t)character >= config->character_template_count) character = 0;
        template = &config->character_templates[character];
        player->character = character;
        player->sex = template->sex;
        player->is_ai = is_ai;
        initial_cash_ratio = is_ai ? template->init_cash_ratio : 50;
        player->cash = (config->initial_fund * initial_cash_ratio) / 100;
        player->bank = config->initial_fund - player->cash;
        player->traffic_method = traffic;
        player->ndices = traffic + 1;
        player->saved_traffic_method = player->traffic_method;
        player->saved_ndices = player->ndices;
        player->engineering_saved_traffic_method = 0;
        player->engineering_saved_ndices = 1;
        player->attached_timed_bomb_slot = -1;
        player->play_state = is_ai ? 2 : 1;
        player->ai_action_flags = template->f22;
        player->ai_personality = is_ai && ai_selection >= 1 && ai_selection <= 3 ?
            ai_selection - 1 : template->f23;
        player->ai_trait = template->f24;
        player->ai_cash_ratio = template->init_cash_ratio;
        player->ai_stock_ratio = template->f26;
        for (default_tool_index = 0;
             default_tool_index < (int)(sizeof(default_tool_ids) /
                                        sizeof(default_tool_ids[0]));
             default_tool_index++) {
            int original_id = default_tool_ids[default_tool_index];
            int tool = original_id - 1;
            /* Match sub_445A4D's per-player cap and its public-pool check
             * for the eight shop-supplied tools. */
            if (player->tool_inventory[tool] >= 9) continue;
            if (original_id <= 8) {
                if (state->tool_amount[tool] == 0) continue;
                state->tool_amount[tool]--;
            }
            player->tool_inventory[tool]++;
        }
        if (!is_ai) state->human_player_count++;
    }
    if (config->special_event_rng != NULL)
        rich4_special_event_sequences_init(state, config->special_event_rng);
    if (config->defer_stock_initialization)
        state->stock_last_update_day = -2;
    else
        rich4_stock_state_init(state, config->special_event_rng);
    if (config->special_event_rng != NULL)
        state->ai_rng_state = config->special_event_rng->state;
    if (report != NULL) {
        report->selected_count = extra;
        report->num_players = state->player_count;
        memcpy(report->character_commit_before, config->character_commit_before,
               sizeof(report->character_commit_before));
        for (i = 0; i < 12; i++) {
            report->character_commit_after[i] =
                config->character_commit_before[i] == 4 ? 2 : 0;
        }
    }
    return 1;
}
