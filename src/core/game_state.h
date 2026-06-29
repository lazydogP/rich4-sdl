#ifndef RICH4_CORE_GAME_STATE_H
#define RICH4_CORE_GAME_STATE_H

#include "core/game_constants.h"
#include "core/game_types.h"
#include "core/game_domain.h"

typedef enum GameMarketListingType {
    GAME_MARKET_LISTING_EMPTY = 0,
    GAME_MARKET_LISTING_STOCK = 1,
    GAME_MARKET_LISTING_ASSET = 2,
    /* Raw type codes verified in the switch at 0x4255DA. */
    GAME_MARKET_LISTING_TOOL = 3,
    GAME_MARKET_LISTING_CARD = 4
} GameMarketListingType;

/* Original per-player 12-byte record written by 0x4246C5.  item_id keeps
 * the original stock/card/tool index or the raw property id (2000+land,
 * 4000+facility); the enclosing array supplies the seller and slot. */
typedef struct GameMarketListing {
    uint8_t type;
    uint8_t flags;
    uint16_t item_id;
    int32_t ask_price;
    int16_t quantity;
    uint16_t reserved;
} GameMarketListing;

typedef struct GameState {
    int active;
    int player_count;
    int current_player;
    int turn_rotation_index;
    /* Transient 0x46CAFF request; deliberately absent from the save codec. */
    int ai_cancel_pending;
    int last_special_turn_index;
    int full_round_count;
    int elapsed_days;
    /* Original calendar globals used by sub_4523D5 and the monthly company
     * profit divisor.  Zero preserves callers without a configured date. */
    int calendar_start_day;
    int calendar_start_month;
    int calendar_start_year;
    int day_limit_reached;
    int win_candidate_player;
    int win_candidate_reason;
    int win_candidate_assets;
    int game_over;
    int winner_player;
    int winner_reason;
    int system_cash;
    int company_cash[GAME_STOCK_COUNT];
    int liquidation_active;
    int liquidation_player;
    int liquidation_payee_kind;
    int liquidation_payee_id;
    int liquidation_amount;
    unsigned int liquidation_flags;
    int human_player_count;
    int initial_fund;
    int day_limit;
    int win_target;
    int land_period_index;
    int initial_traffic_index;
    int stage;
    int map;
    int price_index;
    uint8_t card_amount[GAME_CARD_COUNT];
    uint8_t tool_amount[GAME_TOOL_COUNT];
    int tool_route_node_count;
    /* Raw object ID at each route node, used by magic-house local actions. */
    uint16_t route_object_raw[GAME_MAX_MAP_ROUTE_NODES];
    /* Immutable map flags needed by AI route-target selection. */
    uint32_t tool_route_flags[GAME_MAX_MAP_ROUTE_NODES];
    uint8_t tool_route_node_placeable[GAME_MAX_MAP_ROUTE_NODES];
    int16_t tool_route_x[GAME_MAX_MAP_ROUTE_NODES];
    int16_t tool_route_y[GAME_MAX_MAP_ROUTE_NODES];
    uint16_t tool_route_next[GAME_MAX_MAP_ROUTE_NODES][4];
    /* Original route bits 30..27: one bit per blocked outgoing edge. */
    uint8_t tool_route_edge_blocked[GAME_MAX_MAP_ROUTE_NODES];
    int16_t tool_land_x[GAME_MAX_MAP_PROPERTIES];
    int16_t tool_land_y[GAME_MAX_MAP_PROPERTIES];
    int16_t tool_facility_x[GAME_MAX_MAP_PROPERTIES];
    int16_t tool_facility_y[GAME_MAX_MAP_PROPERTIES];
    /* Map auxiliary/company records also occupy the original global object
     * table, even when no route node repeats their raw object ID. */
    int tool_company_geometry_available;
    int16_t tool_company_x[GAME_MAX_MAP_PROPERTIES];
    int16_t tool_company_y[GAME_MAX_MAP_PROPERTIES];
    struct GameToolPlacedObjectState {
        uint8_t kind;
        uint8_t attached_player;
        uint8_t countdown;
        uint8_t reserved;
        int node;
        int owner;
    } tool_placed_objects[GAME_TOOL_PLACED_OBJECT_COUNT];
    struct GameToolRobotState {
        int active;
        int owner;
        int node;
        int previous_node;
        int next_node;
        int steps_remaining;
    } tool_robot;
    int prison_node;
    int hospital_node;
    int special_player_node[GAME_SPECIAL_PLAYER_COUNT];
    /* Original special-player record +6: the route node just left. */
    int special_player_previous_node[GAME_SPECIAL_PLAYER_COUNT];
    int special_player_actor_type[GAME_SPECIAL_PLAYER_COUNT];
    int special_player_confinement[GAME_SPECIAL_PLAYER_COUNT];
    /* Original record +10: zero means the special actor is active. */
    int special_player_f10[GAME_SPECIAL_PLAYER_COUNT];
    /* Original record +8: owner/rescuer player index; zero is valid. */
    int special_player_play_state[GAME_SPECIAL_PLAYER_COUNT];
    int special_player_status[GAME_SPECIAL_PLAYER_COUNT][4];
    int special_player_route_reversed[GAME_SPECIAL_PLAYER_COUNT];
    int special_player_status_pending_events;
    /* Original 0x499080 dword pool and 0x4990B8 byte owners (0=empty,
     * player index + 1=owned) used by action 9 and the lottery drawing. */
    int lottery_pool;
    uint8_t lottery_owner[36];
    int news_event_occupant[GAME_NEWS_EVENT_COUNT];
    uint8_t news_event_order[GAME_NEWS_EVENT_COUNT];
    uint8_t fortune_event_order[GAME_FORTUNE_EVENT_COUNT];
    int news_event_cursor;
    int fortune_event_cursor;
    uint32_t special_event_rng_state; /* Reserved legacy save field. */
    int special_event_family;
    int special_event_actor;
    int special_event_logical_id;
    int special_event_resolved_id;
    int special_event_resource_id;
    int special_event_panel_id;
    int special_event_prepared_value;
    unsigned int special_event_target_mask;
    /* Phase-0 amounts retained for the per-player news rows.  News tax
     * effects are prepared before the SDL page is shown, so the renderer
     * cannot reconstruct these values from the post-effect balances. */
    int special_event_player_amount[GAME_MAX_PLAYERS];
    int special_event_cancelable;
    int special_event_phase;
    int player_stock_amount[GAME_MAX_PLAYERS][GAME_STOCK_COUNT];
    float player_stock_average_cost[GAME_MAX_PLAYERS][GAME_STOCK_COUNT];
    int fixed_asset_value[GAME_MAX_PLAYERS];
    int land_count;
    int land_owner[GAME_MAX_MAP_PROPERTIES];
    int land_group[GAME_MAX_MAP_PROPERTIES];
    /* Original +0x17/+0x1C: remaining days in high nibble, seal in low
     * nibble. 0x50 is a red price increase, 0x51 a blue seal. */
    int land_price_status[GAME_MAX_MAP_PROPERTIES];
    int land_closed_status[GAME_MAX_MAP_PROPERTIES];
    int land_type[GAME_MAX_MAP_PROPERTIES];
    int land_level[GAME_MAX_MAP_PROPERTIES];
    int land_price[GAME_MAX_MAP_PROPERTIES];
    int land_price_per_level[GAME_MAX_MAP_PROPERTIES];
    int land_toll_by_level[GAME_MAX_MAP_PROPERTIES][6];
    uint32_t land_runtime_value[GAME_MAX_MAP_PROPERTIES];
    int land_value[GAME_MAX_MAP_PROPERTIES];
    int facility_count;
    int facility_owner[GAME_MAX_MAP_PROPERTIES];
    int facility_type[GAME_MAX_MAP_PROPERTIES];
    int facility_price_status[GAME_MAX_MAP_PROPERTIES];
    int facility_closed_status[GAME_MAX_MAP_PROPERTIES];
    int facility_variant[GAME_MAX_MAP_PROPERTIES];
    int facility_level[GAME_MAX_MAP_PROPERTIES];
    int facility_price[GAME_MAX_MAP_PROPERTIES];
    int facility_price_per_level[GAME_MAX_MAP_PROPERTIES];
    int facility_charge_by_level[GAME_MAX_MAP_PROPERTIES][6];
    unsigned int facility_runtime_value[GAME_MAX_MAP_PROPERTIES];
    unsigned int facility_last_change[GAME_MAX_MAP_PROPERTIES];
    int facility_value[GAME_MAX_MAP_PROPERTIES];
    /* Facility record +1d/+1e: one-based tool selection and remaining turns. */
    int facility_research_selection[GAME_MAX_MAP_PROPERTIES];
    int facility_research_turns[GAME_MAX_MAP_PROPERTIES];
    int company_count;
    int company_owner[GAME_MAX_MAP_PROPERTIES];
    int company_stock_index[GAME_MAX_MAP_PROPERTIES];
    int company_type[GAME_MAX_MAP_PROPERTIES];
    int company_base_fee[GAME_MAX_MAP_PROPERTIES];
    /* Auxiliary company record +0x28/+0x2c: business volume.  Premiums add
     * to both fields and insurance claims subtract from both directly. */
    uint32_t company_period_business[GAME_MAX_MAP_PROPERTIES];
    uint32_t company_total_business[GAME_MAX_MAP_PROPERTIES];
    int stock_company_auxiliary[GAME_STOCK_COUNT];
    unsigned int stock_company_value[GAME_STOCK_COUNT];
    unsigned int stock_company_available[GAME_STOCK_COUNT];
    int stock_company_owner[GAME_STOCK_COUNT];
    int stock_company_ranking[GAME_STOCK_COUNT][GAME_MAX_PLAYERS];
    GameStockInfo stocks[GAME_STOCK_COUNT];
    float stock_history[GAME_STOCK_COUNT][GAME_STOCK_HISTORY_COUNT];
    int stock_history_cursor;
    int stock_market_closed;
    int stock_last_update_day;
    uint32_t stock_rng_state; /* Reserved legacy save field. */
    /* Single gameplay/presentation rand word; name retained for save layout. */
    uint32_t ai_rng_state;
    int stock_market_total_x10;
    GameMarketListing market_listings[GAME_MAX_PLAYERS]
                                       [GAME_MARKET_LISTING_SLOT_COUNT];
    GamePlayerState players[GAME_MAX_PLAYERS];
} GameState;

#endif
