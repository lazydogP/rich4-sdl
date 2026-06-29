#include <assert.h>
#include <stdio.h>

#include "data/map_object_ref.h"

static void expect(uint16_t raw, MapObjectKind kind, int index)
{
    MapObjectRef ref = rich4_map_object_ref(raw);
    assert(ref.raw_object_id == raw);
    assert(ref.kind == kind);
    assert(ref.index == index);
}

int main(void)
{
    expect(1999, MAP_OBJECT_ROAD, 1999);
    expect(2000, MAP_OBJECT_NONE, -1);
    expect(2001, MAP_OBJECT_LAND, 0);
    expect(3999, MAP_OBJECT_LAND, 1998);
    expect(4000, MAP_OBJECT_FACILITY, 0);
    expect(4001, MAP_OBJECT_FACILITY, 1);
    expect(5999, MAP_OBJECT_FACILITY, 1999);
    expect(6000, MAP_OBJECT_STOCK, 0);
    expect(6001, MAP_OBJECT_STOCK, 1);
    expect(7999, MAP_OBJECT_STOCK, 1999);
    expect(8000, MAP_OBJECT_SPECIAL, 0);
    expect(8001, MAP_OBJECT_SPECIAL, 1);
    expect(9999, MAP_OBJECT_SPECIAL, 1999);
    expect(10000, MAP_OBJECT_NONE, -1);

    expect(0x8001u, MAP_OBJECT_PLAYER, 0);
    expect(0x8008u, MAP_OBJECT_PLAYER, 3);
    expect(0x8010u, MAP_OBJECT_SPECIAL_PLAYER, 0);
    expect(0x8080u, MAP_OBJECT_SPECIAL_PLAYER, 3);
    expect(0x8100u, MAP_OBJECT_ROUTE, 0);
    expect(0xff00u, MAP_OBJECT_ROUTE, 126);
    expect(0x8000u, MAP_OBJECT_NONE, -1);

    puts("map object references: ok");
    return 0;
}
