#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "data/map_data.h"
#include "data/map_data_test.h"

static void put_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static int map_is_empty(const MapData *map)
{
    return map->routes == NULL && map->route_count == 0 &&
           map->lands == NULL && map->land_count == 0 &&
           map->facilities == NULL && map->facility_count == 0 &&
           map->auxiliary == NULL && map->auxiliary_count == 0 &&
           map->specials == NULL && map->special_count == 0;
}

int main(void)
{
    static const uint32_t offsets[5] = {40, 80, 132, 188, 240};
    unsigned char data[268];
    MapData map;
    int failure;
    int i;
    memset(data, 0, sizeof(data));
    for (i = 0; i < 5; i++) {
        put_u32(data + i * 8, 0);
        put_u32(data + i * 8 + 4, offsets[i]);
    }
    for (failure = 0; failure < 5; failure++) {
        memset(&map, 0xa5, sizeof(map));
        rich4_map_data_test_fail_alloc_after(failure);
        assert(rich4_map_data_parse(data, sizeof(data), MAP_DATA_ARCHIVE_MAP,
                                    7, &map) == MAP_DATA_ERR_MEMORY);
        assert(map_is_empty(&map));
        rich4_map_data_free(&map);
        assert(map_is_empty(&map));
        rich4_map_data_test_clear_failures();
    }
    assert(rich4_map_data_parse(data, sizeof(data), MAP_DATA_ARCHIVE_MAP,
                                7, &map) == MAP_DATA_OK);
    assert(map.route_count == 1 && map.special_count == 1);
    rich4_map_data_free(&map);
    puts("MapData fault injection: ok");
    return 0;
}
