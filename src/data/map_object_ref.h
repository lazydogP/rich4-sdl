#ifndef RICH4_MAP_OBJECT_REF_H
#define RICH4_MAP_OBJECT_REF_H

#include <stdint.h>

typedef enum MapObjectKind {
    MAP_OBJECT_NONE = 0,
    MAP_OBJECT_ROAD,
    MAP_OBJECT_LAND,
    MAP_OBJECT_FACILITY,
    MAP_OBJECT_STOCK,
    MAP_OBJECT_SPECIAL,
    MAP_OBJECT_PLAYER,
    MAP_OBJECT_SPECIAL_PLAYER,
    MAP_OBJECT_ROUTE
} MapObjectKind;

typedef struct MapObjectRef {
    MapObjectKind kind;
    int index;
    uint16_t raw_object_id;
} MapObjectRef;

MapObjectRef rich4_map_object_ref(uint16_t object_id);

#endif
