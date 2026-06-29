#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "data/map_data.h"

static void put_u16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
}

static void put_u32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

int main(void)
{
    unsigned char data[496];
    const unsigned int offsets[5] = {40, 120, 224, 336, 440};
    MapData map;
    MapDataSelection selection;
    size_t i;
    int stage;
    int map_index;
    int company_links[12];
    unsigned char facility_round_trip[56];
    unsigned char auxiliary_round_trip[52];
    MapRouteNode place_routes[2];
    MapData place_map;
    int prison_node, hospital_node;
    memset(data, 0, sizeof(data));
    for (i = 0; i < 5; i++) {
        put_u32(data + i * 8, 1);
        put_u32(data + i * 8 + 4, offsets[i]);
    }
    put_u16(data + 40 + 40, 12); put_u16(data + 40 + 42, 34);
    put_u16(data + 40 + 40 + 24, 2); put_u16(data + 40 + 40 + 32, 2001);
    put_u16(data + 40 + 40 + 34, 9); put_u32(data + 40 + 40 + 36, 0x12345678u);
    data[120 + 52 + 0x17] = 1;
    data[120 + 52 + 0x18] = 2; data[120 + 52 + 0x19] = 3;
    data[120 + 52 + 0x1a] = 4; data[120 + 52 + 0x1b] = 5;
    put_u16(data + 120 + 52 + 0x1c, 600); put_u16(data + 120 + 52 + 0x1e, 70);
    put_u16(data + 120 + 52 + 0x20, 80); put_u16(data + 120 + 52 + 0x2a, 130);
    put_u32(data + 120 + 52 + 0x2c, 0x89abcdefu);
    data[224 + 56 + 0x18] = 6; data[224 + 56 + 0x19] = 2;
    data[224 + 56 + 0x1c] = 3;
    data[224 + 56 + 0x1d] = 4; data[224 + 56 + 0x1e] = 5;
    put_u16(data + 224 + 56 + 0x22, 800); put_u16(data + 224 + 56 + 0x24, 90);
    put_u16(data + 224 + 56 + 0x2e, 150);
    put_u32(data + 224 + 56 + 0x30, 0x11223344u);
    put_u32(data + 224 + 56 + 0x34, 0x55667788u);
    data[336 + 52 + 0x18] = 7; data[336 + 52 + 0x19] = 9;
    data[336 + 52 + 0x1a] = 4;
    data[336 + 52 + 0x1b] = 8;
    data[336 + 52 + 0x1c] = 1; data[336 + 52 + 0x1d] = 2;
    data[336 + 52 + 0x1e] = 3; data[336 + 52 + 0x1f] = 4;
    put_u16(data + 336 + 52 + 32, 15);
    put_u16(data + 336 + 52 + 0x22, 250);
    put_u32(data + 336 + 52 + 0x24, 1230000u);
    put_u32(data + 336 + 52 + 0x28, 321u);
    put_u32(data + 336 + 52 + 0x2c, 654u);
    put_u32(data + 336 + 52 + 0x30, 4567u);
    data[440 + 28 + 0x18] = 9; put_u16(data + 440 + 28 + 26, 16);

    assert(rich4_map_data_parse(data, sizeof(data), MAP_DATA_ARCHIVE_MAPDAT, 3, &map) == MAP_DATA_OK);
    assert(map.route_count == 2 && map.routes[1].x == 12 && map.routes[1].y == 34);
    assert(map.routes[1].next[0] == 2 && map.routes[1].object.kind == MAP_OBJECT_LAND);
    assert(map.routes[1].object.index == 0);
    assert(map.routes[1].object.raw_object_id == 2001 && map.routes[1].display_chunk == 9);
    assert(map.routes[1].flags == 0x12345678u && map.routes[1].raw[36] == 0x78);
    assert(map.lands[1].type == 2 && map.lands[1].owner_raw == 3 &&
           map.lands[1].parity_overlay == 1 && map.lands[1].price == 600);
    assert(map.lands[1].toll_by_level[0] == 80 &&
           map.lands[1].toll_by_level[5] == 130 &&
           map.lands[1].runtime_value == 0x89abcdefu);
    assert(map.facilities[1].type == 6 && map.facilities[1].parity_overlay == 3 &&
           map.facilities[1].price_per_level == 90 &&
           map.facilities[1].research_selection == 4 &&
           map.facilities[1].research_turns == 5 &&
           map.facilities[1].charge_by_level[5] == 150 &&
           map.facilities[1].runtime_value == 0x11223344u &&
           map.facilities[1].last_change == 0x55667788u);
    assert(map.auxiliary[1].owner_raw == 7 &&
           map.auxiliary[1].stock_index == 9 &&
           map.auxiliary[1].commerce_type == 4 &&
           map.auxiliary[1].ranking_raw[3] == 4 &&
           map.auxiliary[1].sprite_resource == 15 &&
           map.auxiliary[1].base_fee == 250 &&
           map.auxiliary[1].company_value == 1230000u &&
           map.auxiliary[1].period_business == 321u &&
           map.auxiliary[1].total_business == 654u &&
           map.auxiliary[1].available_shares == 4567u);
    assert(rich4_map_data_company_links(&map, company_links, 12));
    assert(company_links[9] == 0 && company_links[8] == -1);
    assert(rich4_map_facility_record_write(&map.facilities[1],
                                           facility_round_trip));
    assert(memcmp(facility_round_trip, data + 224 + 56, 56) == 0);
    assert(rich4_map_auxiliary_record_write(&map.auxiliary[1],
                                            auxiliary_round_trip));
    assert(memcmp(auxiliary_round_trip, data + 336 + 52, 52) == 0);
    assert(map.specials[1].variant == 9 && map.specials[1].sprite_resource == 16);
    assert(map.source_archive == MAP_DATA_ARCHIVE_MAPDAT && map.source_resource_index == 3);
    rich4_map_data_free(&map);

    memset(&place_map, 0, sizeof(place_map));
    memset(place_routes, 0, sizeof(place_routes));
    place_map.routes = place_routes; place_map.route_count = 2;
    place_routes[0].object.raw_object_id = 0x1f42;
    place_routes[1].object.raw_object_id = 0x1f41;
    assert(rich4_map_data_special_place_nodes(&place_map, &prison_node,
                                               &hospital_node));
    assert(prison_node == 0 && hospital_node == 1);

    selection = rich4_map_data_select(1, 2, 1);
    assert(selection.gmap == 6 && selection.archive == MAP_DATA_ARCHIVE_MAPDAT && selection.resource_index == 6);
    selection = rich4_map_data_select(1, 2, 0);
    assert(selection.archive == MAP_DATA_ARCHIVE_MAP && selection.resource_index == 13);
    for (stage = 0; stage < 2; stage++) {
        for (map_index = 0; map_index < 4; map_index++) {
            int gmap = stage * 4 + map_index;
            selection = rich4_map_data_select(stage, map_index, 1);
            assert(selection.gmap == gmap &&
                   selection.archive == MAP_DATA_ARCHIVE_MAPDAT &&
                   selection.resource_index == (size_t)gmap);
            selection = rich4_map_data_select(stage, map_index, 0);
            assert(selection.gmap == gmap &&
                   selection.archive == MAP_DATA_ARCHIVE_MAP &&
                   selection.resource_index == (size_t)(gmap * 2 + 1));
        }
    }
    put_u32(data + 4, 480);
    assert(rich4_map_data_parse(data, sizeof(data), MAP_DATA_ARCHIVE_MAP, 1, &map) == MAP_DATA_ERR_RANGE);
    assert(rich4_map_data_parse(data, 39, MAP_DATA_ARCHIVE_MAP, 1, &map) == MAP_DATA_ERR_TRUNCATED);
    puts("map data parser: ok");
    return 0;
}
