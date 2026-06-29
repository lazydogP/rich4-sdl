#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/game_session_map_init.h"

int main(void)
{
    GameSession session;
    MapData map;
    MapRouteNode routes[3];
    MapLandRecord lands[3];
    MapFacilityRecord facilities[2];
    MapAuxiliaryRecord auxiliary[2];

    memset(&session, 0, sizeof(session));
    memset(&map, 0, sizeof(map));
    memset(routes, 0, sizeof(routes));
    memset(lands, 0, sizeof(lands));
    memset(facilities, 0, sizeof(facilities));
    memset(auxiliary, 0, sizeof(auxiliary));

    routes[1].object.raw_object_id = 0x1f42;
    routes[1].flags = 0x40000000u;
    routes[2].object.raw_object_id = 0x1f41;

    lands[1].owner_raw = 1;
    lands[1].type = 0;
    lands[1].level = 2;
    lands[1].price = 100;
    lands[1].price_per_level = 10;
    lands[1].raw[4] = 'A';
    lands[1].raw[5] = 0;
    lands[2] = lands[1];
    lands[2].owner_raw = 2;

    facilities[1].owner_raw = 2;
    facilities[1].level = 1;
    facilities[1].price = 200;
    facilities[1].price_per_level = 20;

    auxiliary[1].stock_index = 1;
    auxiliary[1].x = 64;
    auxiliary[1].y = -32;
    auxiliary[1].company_value = 77;
    auxiliary[1].available_shares = 88;
    memcpy(auxiliary[1].raw + 4, "RAW-COMPANY", 11);

    map.routes = routes;
    map.route_count = 3;
    map.lands = lands;
    map.land_count = 3;
    map.facilities = facilities;
    map.facility_count = 2;
    map.auxiliary = auxiliary;
    map.auxiliary_count = 2;
    session.state.stocks[1].f4 = 1;

    rich4_game_session_load_map_state(&session, &map);

    assert(session.state.tool_route_node_count == 3);
    assert(session.state.route_object_raw[1] == 0x1f42);
    assert(session.state.tool_route_flags[1] == 0x40000000u);
    assert(session.state.tool_route_edge_blocked[1] == 1);
    assert(session.state.prison_node == 1);
    assert(session.state.hospital_node == 2);
    assert(session.state.land_count == 2);
    assert(session.state.land_owner[0] == 0);
    assert(session.state.land_owner[1] == 1);
    assert(session.state.land_group[0] == session.state.land_group[1]);
    assert(session.state.land_value[0] == 120);
    assert(session.state.land_value[1] == 120);
    assert(session.state.facility_count == 1);
    assert(session.state.facility_owner[0] == 1);
    assert(session.state.facility_value[0] == 220);
    assert(session.state.fixed_asset_value[0] == 120);
    assert(session.state.fixed_asset_value[1] == 340);
    assert(session.state.company_count == 1);
    assert(session.state.tool_company_geometry_available == 1);
    assert(session.state.tool_company_x[0] == 64);
    assert(session.state.tool_company_y[0] == -32);
    assert(session.state.company_owner[0] == -1);
    assert(session.state.company_stock_index[0] == 1);
    assert(strcmp(session.presentation.map_company_names[0],
                  "RAW-COMPANY") == 0);
    assert(session.state.stock_company_auxiliary[1] == 0);
    assert(session.state.stock_company_value[1] == 77);
    assert(session.state.stock_company_available[1] == 88);
    assert(session.state.stocks[1].f4 == 1);

    puts("app game-session map init: ok");
    return 0;
}
