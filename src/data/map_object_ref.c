#include "data/map_object_ref.h"

static int least_set_bit(uint16_t value, int first_bit, int bit_count)
{
    int i;
    for (i = 0; i < bit_count; i++) {
        if ((value & (uint16_t)(1u << (first_bit + i))) != 0) {
            return i;
        }
    }
    return -1;
}

MapObjectRef rich4_map_object_ref(uint16_t object_id)
{
    MapObjectRef ref;
    ref.kind = MAP_OBJECT_NONE;
    ref.index = -1;
    ref.raw_object_id = object_id;

    if (object_id < 2000u) {
        ref.kind = MAP_OBJECT_ROAD;
        ref.index = object_id;
    } else if (object_id > 2000u && object_id < 4000u) {
        ref.kind = MAP_OBJECT_LAND;
        ref.index = (int)object_id - 2001;
    } else if (object_id >= 4000u && object_id < 6000u) {
        ref.kind = MAP_OBJECT_FACILITY;
        ref.index = (int)object_id - 4000;
    } else if (object_id >= 6000u && object_id < 8000u) {
        ref.kind = MAP_OBJECT_STOCK;
        ref.index = (int)object_id - 6000;
    } else if (object_id >= 8000u && object_id < 10000u) {
        ref.kind = MAP_OBJECT_SPECIAL;
        ref.index = (int)object_id - 8000;
    } else if ((object_id & 0x8000u) != 0) {
        int index = least_set_bit(object_id, 0, 4);
        if (index >= 0) {
            ref.kind = MAP_OBJECT_PLAYER;
            ref.index = index;
        } else {
            index = least_set_bit(object_id, 4, 4);
            if (index >= 0) {
                ref.kind = MAP_OBJECT_SPECIAL_PLAYER;
                ref.index = index;
            } else if ((object_id & 0x7f00u) != 0) {
                ref.kind = MAP_OBJECT_ROUTE;
                ref.index = (int)((object_id >> 8) & 0x7fu) - 1;
            }
        }
    }
    return ref;
}
