#include "app/game_session_map_init.h"

#include <string.h>

/* App-owned map-to-session projection. It owns no resources and borrows one
 * immutable MapData. Deterministic map facts go to GameState; the fixed
 * company display strings remain in transient presentation state so they do
 * not become save/replay fields. Invariant: bounded arrays are initialized
 * before any derived value is computed. Test:
 * tests/unit/test_game_session_map_init.c. */

static int land_value_from_fields(int type, int level, int land_price,
                                  int price_per_level)
{
    int value = land_price;
    if (type != 0) value += price_per_level;
    else if (level != 0) value += level * price_per_level;
    return value;
}

static int facility_value_from_fields(int level, int land_price,
                                      int price_per_level)
{
    return land_price + level * price_per_level;
}

static void copy_map_company_names(GameSession *session, const MapData *map)
{
    int i;

    memset(session->presentation.map_company_names, 0,
           sizeof(session->presentation.map_company_names));
    for (i = 0; i < session->state.company_count; i++) {
        const MapAuxiliaryRecord *record = &map->auxiliary[i + 1];
        memcpy(session->presentation.map_company_names[i], record->raw + 4,
               RICH4_MAP_COMPANY_NAME_BYTES);
        session->presentation.map_company_names[i]
            [RICH4_MAP_COMPANY_NAME_BYTES] = '\0';
    }
}

static int land_names_equal(const MapLandRecord *left,
                            const MapLandRecord *right)
{
    int i;
    for (i = 4; i < 0x18; i++) {
        if (left->raw[i] != right->raw[i]) return 0;
        if (left->raw[i] == 0) return 1;
    }
    return 1;
}

static void recompute_fixed_asset_value(GameSession *session)
{
    int i;
    if (session == NULL) return;
    memset(session->state.fixed_asset_value, 0,
           sizeof(session->state.fixed_asset_value));
    for (i = 0; i < session->state.land_count &&
                i < GAME_MAX_MAP_PROPERTIES; i++) {
        int owner = session->state.land_owner[i];
        session->state.land_value[i] = land_value_from_fields(
            session->state.land_type[i], session->state.land_level[i],
            session->state.land_price[i],
            session->state.land_price_per_level[i]);
        if (owner >= 0 && owner < GAME_MAX_PLAYERS)
            session->state.fixed_asset_value[owner] +=
                session->state.land_value[i];
    }
    for (i = 0; i < session->state.facility_count &&
                i < GAME_MAX_MAP_PROPERTIES; i++) {
        int owner = session->state.facility_owner[i];
        session->state.facility_value[i] = facility_value_from_fields(
            session->state.facility_level[i], session->state.facility_price[i],
            session->state.facility_price_per_level[i]);
        if (owner >= 0 && owner < GAME_MAX_PLAYERS)
            session->state.fixed_asset_value[owner] +=
                session->state.facility_value[i];
    }
}

void rich4_game_session_load_map_state(GameSession *session,
                                       const MapData *map)
{
    int i;
    if (session == NULL || map == NULL) return;

    memset(session->state.route_object_raw, 0,
           sizeof(session->state.route_object_raw));
    memset(session->state.tool_route_flags, 0,
           sizeof(session->state.tool_route_flags));
    session->state.tool_route_node_count = map->route_count <
        GAME_MAX_MAP_ROUTE_NODES ? (int)map->route_count :
        GAME_MAX_MAP_ROUTE_NODES;
    for (i = 0; i < session->state.tool_route_node_count; i++)
        session->state.route_object_raw[i] =
            (uint16_t)map->routes[i].object.raw_object_id;
    for (i = 0; i < session->state.tool_route_node_count; i++)
        session->state.tool_route_flags[i] = map->routes[i].flags;
    memset(session->state.tool_route_edge_blocked, 0,
           sizeof(session->state.tool_route_edge_blocked));
    for (i = 0; i < session->state.tool_route_node_count; i++) {
        int edge;
        for (edge = 0; edge < 4; edge++)
            if ((map->routes[i].flags & (0x40000000u >> edge)) != 0)
                session->state.tool_route_edge_blocked[i] |=
                    (uint8_t)(1u << edge);
    }
    rich4_map_data_special_place_nodes(map, &session->state.prison_node,
                                       &session->state.hospital_node);

    memset(session->state.land_owner, 0xff,
           sizeof(session->state.land_owner));
    memset(session->state.land_group, 0xff,
           sizeof(session->state.land_group));
    memset(session->state.land_price_status, 0,
           sizeof(session->state.land_price_status));
    memset(session->state.land_closed_status, 0,
           sizeof(session->state.land_closed_status));
    memset(session->state.land_type, 0, sizeof(session->state.land_type));
    memset(session->state.land_level, 0, sizeof(session->state.land_level));
    memset(session->state.land_price, 0, sizeof(session->state.land_price));
    memset(session->state.land_price_per_level, 0,
           sizeof(session->state.land_price_per_level));
    memset(session->state.land_runtime_value, 0,
           sizeof(session->state.land_runtime_value));
    memset(session->state.land_value, 0,
           sizeof(session->state.land_value));
    session->state.land_count = map->land_count > 0 ?
        (int)map->land_count - 1 : 0;
    if (session->state.land_count > GAME_MAX_MAP_PROPERTIES)
        session->state.land_count = GAME_MAX_MAP_PROPERTIES;
    for (i = 0; i < session->state.land_count; i++) {
        const MapLandRecord *record = &map->lands[i + 1];
        int owner = (int)record->owner_raw - 1;
        session->state.land_owner[i] = owner >= 0 &&
            owner < GAME_MAX_PLAYERS ? owner : -1;
        session->state.land_price_status[i] = record->parity_overlay;
        session->state.land_closed_status[i] = (record->parity_overlay & 0x0f)
            ? record->parity_overlay >> 4 : 0;
        session->state.land_type[i] = record->type;
        session->state.land_level[i] = record->level;
        session->state.land_price[i] = record->price;
        session->state.land_price_per_level[i] = record->price_per_level;
        {
            int toll_i;
            for (toll_i = 0; toll_i < 6; toll_i++)
                session->state.land_toll_by_level[i][toll_i] =
                    (int)record->toll_by_level[toll_i];
        }
        session->state.land_runtime_value[i] = record->runtime_value;
        {
            int j;
            session->state.land_group[i] = i;
            for (j = 0; j < i; j++) {
                const MapLandRecord *other = &map->lands[j + 1];
                if (land_names_equal(record, other)) {
                    session->state.land_group[i] = session->state.land_group[j];
                    break;
                }
            }
        }
    }

    memset(session->state.facility_owner, 0xff,
           sizeof(session->state.facility_owner));
    memset(session->state.facility_type, 0,
           sizeof(session->state.facility_type));
    memset(session->state.facility_price_status, 0,
           sizeof(session->state.facility_price_status));
    memset(session->state.facility_closed_status, 0,
           sizeof(session->state.facility_closed_status));
    memset(session->state.facility_variant, 0,
           sizeof(session->state.facility_variant));
    memset(session->state.facility_level, 0,
           sizeof(session->state.facility_level));
    memset(session->state.facility_price, 0,
           sizeof(session->state.facility_price));
    memset(session->state.facility_price_per_level, 0,
           sizeof(session->state.facility_price_per_level));
    memset(session->state.facility_charge_by_level, 0,
           sizeof(session->state.facility_charge_by_level));
    memset(session->state.facility_runtime_value, 0,
           sizeof(session->state.facility_runtime_value));
    memset(session->state.facility_last_change, 0,
           sizeof(session->state.facility_last_change));
    memset(session->state.facility_value, 0,
           sizeof(session->state.facility_value));
    memset(session->state.facility_research_selection, 0,
           sizeof(session->state.facility_research_selection));
    memset(session->state.facility_research_turns, 0,
           sizeof(session->state.facility_research_turns));
    session->state.facility_count = map->facility_count > 0 ?
        (int)map->facility_count - 1 : 0;
    if (session->state.facility_count > GAME_MAX_MAP_PROPERTIES)
        session->state.facility_count = GAME_MAX_MAP_PROPERTIES;
    for (i = 0; i < session->state.facility_count; i++) {
        const MapFacilityRecord *record = &map->facilities[i + 1];
        int owner = (int)record->owner_raw - 1;
        session->state.facility_owner[i] = owner >= 0 &&
            owner < GAME_MAX_PLAYERS ? owner : -1;
        session->state.facility_type[i] = record->type;
        session->state.facility_price_status[i] = record->parity_overlay;
        session->state.facility_closed_status[i] = (record->parity_overlay & 0x0f)
            ? record->parity_overlay >> 4 : 0;
        session->state.facility_variant[i] = record->variant;
        session->state.facility_level[i] = record->level;
        session->state.facility_price[i] = record->price;
        session->state.facility_price_per_level[i] =
            record->price_per_level;
        {
            int charge_i;
            for (charge_i = 0; charge_i < 6; charge_i++)
                session->state.facility_charge_by_level[i][charge_i] =
                    (int)record->charge_by_level[charge_i];
        }
        session->state.facility_runtime_value[i] = record->runtime_value;
        session->state.facility_last_change[i] = record->last_change;
        session->state.facility_research_selection[i] =
            record->research_selection;
        session->state.facility_research_turns[i] = record->research_turns;
    }

    session->state.tool_company_geometry_available = 0;
    memset(session->state.tool_company_x, 0,
           sizeof(session->state.tool_company_x));
    memset(session->state.tool_company_y, 0,
           sizeof(session->state.tool_company_y));
    session->state.company_count = map->auxiliary_count > 0 ?
        (int)map->auxiliary_count - 1 : 0;
    if (session->state.company_count > GAME_MAX_MAP_PROPERTIES)
        session->state.company_count = GAME_MAX_MAP_PROPERTIES;
    copy_map_company_names(session, map);
    memset(session->state.company_owner, 0xff,
           sizeof(session->state.company_owner));
    memset(session->state.company_stock_index, 0xff,
           sizeof(session->state.company_stock_index));
    memset(session->state.company_type, 0,
           sizeof(session->state.company_type));
    memset(session->state.company_base_fee, 0,
           sizeof(session->state.company_base_fee));
    memset(session->state.company_period_business, 0,
           sizeof(session->state.company_period_business));
    memset(session->state.company_total_business, 0,
           sizeof(session->state.company_total_business));
    session->state.tool_company_geometry_available =
        session->state.company_count > 0;
    for (i = 0; i < session->state.company_count; i++) {
        const MapAuxiliaryRecord *record = &map->auxiliary[i + 1];
        int owner = (int)record->owner_raw - 1;
        session->state.company_owner[i] = owner >= 0 &&
            owner < GAME_MAX_PLAYERS ? owner : -1;
        session->state.company_stock_index[i] = record->stock_index;
        session->state.company_type[i] = record->commerce_type;
        session->state.company_base_fee[i] = record->base_fee;
        session->state.company_period_business[i] = record->period_business;
        session->state.company_total_business[i] = record->total_business;
        session->state.tool_company_x[i] = record->x;
        session->state.tool_company_y[i] = record->y;
    }
    memset(session->state.stock_company_auxiliary, 0xff,
           sizeof(session->state.stock_company_auxiliary));
    memset(session->state.stock_company_value, 0,
           sizeof(session->state.stock_company_value));
    memset(session->state.stock_company_available, 0,
           sizeof(session->state.stock_company_available));
    rich4_map_data_company_links(map, session->state.stock_company_auxiliary,
                                 GAME_STOCK_COUNT);
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        int link = session->state.stock_company_auxiliary[i];
        if (session->state.stocks[i].f4 == 0) {
            session->state.stock_company_auxiliary[i] = -1;
        } else if (link >= 0) {
            session->state.stocks[i].f4 = (uint16_t)(link + 1);
            session->state.stock_company_value[i] =
                map->auxiliary[link + 1].company_value;
            session->state.stock_company_available[i] =
                map->auxiliary[link + 1].available_shares;
        }
    }
    recompute_fixed_asset_value(session);
}
