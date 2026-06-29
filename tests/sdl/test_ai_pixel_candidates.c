#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <SDL/SDL.h>
#undef main
#include "platform/sdl1/render/game_map_objects.h"
#include "core/special_event.h"

/* Coordinates evaluated from IDA's 0x46CCF0 and 0x474910, via 0x407A2C.
 * Present archives with undecoded SPRs must still contribute anchors. */
static void check_complete_sources(void)
{
    static GamePreviewAssets assets;
    static GameViewModel view;
    static MapRouteNode routes[2];
    static MapLandRecord lands[5];
    static MapFacilityRecord facilities[5];
    static GameMapDynamicObjectState dynamic[GAME_MAP_DYNAMIC_OBJECT_COUNT];
    static const int xy[8][2] = {
        {258,270},{236,285},{207,286},{182,271},
        {181,249},{202,233},{233,231},{256,247}
    };
    static const uint16_t ids[] = {
        0x8001,0x8002,0x8010,0x8100,0x8f00,0x9100,0xa500,
        2002,2003,2004,4001,4002,4003,6001,8001
    };
    GameVisibleMapObject objects[GAME_MAP_RENDER_QUEUE_CAPACITY];
    uint16_t actual[440 * 440];
    uint16_t expected[440 * 440];
    int direction, i, count;
    MapData *map = &assets.map.maps[0];
    map->routes = routes; map->route_count = 2;
    map->lands = lands; map->land_count = 5;
    map->facilities = facilities; map->facility_count = 5;
    routes[1].x = 837; routes[1].y = 869;
    view.active = 1; view.actor_count = 2; view.special_actor_count = 2;
    view.land_count = view.facility_count = 4;
    view.viewport.center_world_x = 812; view.viewport.center_world_y = 846;
    for (i = 0; i < 2; i++) {
        view.actors[i].visible = 1; view.actors[i].character = i;
        view.actors[i].world_x = 837; view.actors[i].world_y = 869;
        view.actors[i].overlay_part = 0;
        assets.character.map_player_resource_present[i][0] = 1;
        view.special_actors[i].visible = 1;
        view.special_actors[i].world_x = 837; view.special_actors[i].world_y = 869;
        view.special_actors[i].overlay_chunk = 0;
    }
    assets.character.map_special_actor_resource_present[0][0] = 1;
    assets.map.has_map_common[1] = assets.map.has_map_common_graph[2] = 1;
    assets.map.map_object_resource_present[1] = 1;
    assets.map.map_object_resource_present[41] = 1;
    assets.map.map_object_resource_present[49] = 1;
    assets.map.map_object_resource_present[50] = 1;
    for (i = 0; i < 4; i++) {
        lands[i + 1].x = facilities[i + 1].x = 837;
        lands[i + 1].y = facilities[i + 1].y = 869;
        view.lands[i].owner = view.facilities[i].owner = -1;
    }
    view.lands[1].owner = 0; /* owned level zero: common owner marker */
    view.lands[2].level = view.lands[3].level = 1;
    view.lands[3].type = 1;
    view.facilities[0].level = view.facilities[1].level = 1;
    view.facilities[1].type = 1;
    view.facilities[2].owner = 0;
    for (i = 0; i < 4; i++) {
        dynamic[i].type = 1; dynamic[i].node = 1;
    }
    dynamic[1].attached_player = 1;
    dynamic[2].flight_active = 1;
    dynamic[2].world_x = 837; dynamic[2].world_y = 869;
    dynamic[3].type = 2; /* absent resource */
    assets.map.data_map_status_resource_present[0] = 1;
    dynamic[14].type = 15; dynamic[14].node = 1;
    dynamic[16].type = 16; dynamic[16].node = 1;
    dynamic[36].type = 18; dynamic[36].node = 1;
    dynamic[37].type = 18; dynamic[37].attached_player = 1;
    assets.map.data_map_status_resource_present[14] = 1;
    assets.map.data_map_status_resource_present[15] = 1;
    assets.map.data_map_status_resource_present[17] = 1;
    assets.map.map_object_count[0] = 6;
    for (i = 0; i < 6; i++) {
        assets.map.map_object_x[0][i] = 837;
        assets.map.map_object_y[0][i] = 869;
        assets.map.map_object_sprite[0][i] = 49;
    }
    assets.map.map_object_id[0][0] = 6001;
    assets.map.map_object_id[0][1] = 8001;
    assets.map.map_object_id[0][2] = 0;
    assets.map.map_object_id[0][3] = 6002;
    assets.map.map_object_sprite[0][3] = 51;
    assets.map.map_object_id[0][4] = 2002; /* stale preview is replaced */
    assets.map.map_object_id[0][5] = 8002;
    assets.map.map_object_x[0][5] = 1260;
    assets.map.map_object_y[0][5] = 1294; /* tile 28: anchor outside pixels */
    for (direction = 0; direction < 8; direction++) {
        uint16_t raw = 0;
        view.viewport.direction = direction;
        count = game_map_collect_ai_objects(&assets, 0, &view, dynamic,
            objects, GAME_MAP_RENDER_QUEUE_CAPACITY);
        assert(count == (int)(sizeof(ids) / sizeof(ids[0])));
        memset(actual, 0, sizeof(actual));
        memset(expected, 0, sizeof(expected));
        for (i = 0; i < count; i++) {
            int j, found = 0;
            assert(objects[i].x == xy[direction][0]);
            assert(objects[i].y == xy[direction][1]);
            for (j = 0; j < count; j++)
                if (objects[i].object_id == ids[j]) found++;
            assert(found == 1);
            actual[(objects[i].y - 40) * 440 + objects[i].x] |=
                (uint16_t)objects[i].object_id;
            raw |= ids[i];
        }
        expected[(xy[direction][1] - 40) * 440 + xy[direction][0]] = raw;
        assert(memcmp(actual, expected, sizeof(actual)) == 0);
    }
}

static void check_news_area_uses_projected_resource_anchors(void)
{
    static GamePreviewAssets assets;
    static GameViewModel view;
    static MapLandRecord lands[3];
    static GameState state;
    GameVisibleMapObject objects[GAME_MAP_RENDER_QUEUE_CAPACITY];
    GameNewsAreaAnchor anchors[GAME_MAP_RENDER_QUEUE_CAPACITY];
    int direction, id, count, i;
    assets.map.maps[0].lands = lands;
    assets.map.maps[0].land_count = 3;
    assets.map.map_object_resource_present[1] = 1;
    /* The second property's original resource pointer is absent. */
    lands[1].x = lands[2].x = 837;
    lands[1].y = lands[2].y = 869;
    view.active = 1; view.land_count = 2;
    view.viewport.center_world_x = 812; view.viewport.center_world_y = 846;
    view.lands[0].level = 1; view.lands[0].owner = 0;
    view.lands[1].level = 2; view.lands[1].owner = 1;
    for (direction = 0; direction < 8; direction++) {
        view.viewport.direction = direction;
        count = game_map_collect_ai_objects(&assets,0,&view,NULL,objects,
            GAME_MAP_RENDER_QUEUE_CAPACITY);
        assert(count == 1 && objects[0].object_id == 2001);
        for (i = 0; i < count; i++) {
            anchors[i].raw_id = (uint16_t)objects[i].object_id;
            anchors[i].screen_x = objects[i].x;
            anchors[i].screen_y = objects[i].y - 40;
        }
        for (id = 4; id <= 20; id += 16) {
            memset(&state,0,sizeof(state));
            state.land_count = 2;
            state.land_owner[0] = 0; state.land_owner[1] = 1;
            state.land_level[0] = 1; state.land_level[1] = 2;
            state.special_event_family = GAME_SPECIAL_EVENT_NEWS;
            state.special_event_phase = GAME_SPECIAL_PHASE_PREPARED;
            state.special_event_resolved_id = id;
            state.special_event_target_mask = 1;
            assert(rich4_special_event_apply_news_area(&state,anchors,count));
            assert(state.land_level[0] == 0 && state.land_level[1] == 2);
            assert(state.land_owner[0] == (id == 4 ? -1 : 0));
            assert(state.land_owner[1] == 1);
        }
    }
}

int main(void)
{
    check_complete_sources();
    check_news_area_uses_projected_resource_anchors();
    puts("SDL AI full 440x440 candidate table, eight directions: ok");
    return 0;
}
