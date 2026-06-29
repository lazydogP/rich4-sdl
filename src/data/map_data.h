#ifndef RICH4_MAP_DATA_H
#define RICH4_MAP_DATA_H

#include <stddef.h>
#include <stdint.h>

#include "data/map_object_ref.h"
#include "api_contract.h"

typedef enum MapDataArchive {
    MAP_DATA_ARCHIVE_MAP = 0,
    MAP_DATA_ARCHIVE_MAPDAT
} MapDataArchive;

typedef struct MapDataSelection {
    MapDataArchive archive;
    size_t resource_index;
    int gmap;
} MapDataSelection;

typedef struct MapRouteNode {
    int16_t x;
    int16_t y;
    uint16_t next[4];
    MapObjectRef object;
    uint16_t display_chunk;
    uint32_t flags;
    uint8_t raw[40];
} MapRouteNode;

typedef struct MapLandRecord {
    int16_t x;
    int16_t y;
    uint8_t parity_overlay;
    uint8_t type;
    uint8_t owner_raw;
    uint8_t level;
    uint8_t variant;
    uint16_t price;
    uint16_t price_per_level;
    uint16_t toll_by_level[6];
    /* Original record +0x2c, consumed by the spy as current toll income. */
    uint32_t runtime_value;
    uint8_t raw[52];
} MapLandRecord;

typedef struct MapFacilityRecord {
    int16_t x;
    int16_t y;
    uint8_t parity_overlay;
    uint8_t type;
    uint8_t owner_raw;
    uint8_t level;
    uint8_t variant;
    uint8_t research_selection;
    uint8_t research_turns;
    uint16_t price;
    uint16_t price_per_level;
    uint16_t charge_by_level[6];
    uint32_t runtime_value;
    uint32_t last_change;
    uint8_t raw[56];
} MapFacilityRecord;

typedef struct MapAuxiliaryRecord {
    int16_t x;
    int16_t y;
    uint8_t owner_raw;
    uint8_t stock_index;
    uint8_t commerce_type;
    uint8_t variant;
    uint8_t ranking_raw[4];
    uint16_t sprite_resource;
    uint16_t base_fee;
    uint32_t company_value;
    uint32_t period_business;
    uint32_t total_business;
    uint32_t available_shares;
    uint8_t raw[52];
} MapAuxiliaryRecord;

typedef struct MapSpecialRecord {
    int16_t x;
    int16_t y;
    uint8_t variant;
    uint16_t sprite_resource;
    uint8_t raw[28];
} MapSpecialRecord;

typedef struct MapData {
    MapDataArchive source_archive;
    size_t source_resource_index;
    MapRouteNode *routes;
    size_t route_count;
    MapLandRecord *lands;
    size_t land_count;
    MapFacilityRecord *facilities;
    size_t facility_count;
    MapAuxiliaryRecord *auxiliary;
    size_t auxiliary_count;
    MapSpecialRecord *specials;
    size_t special_count;
} MapData;

typedef enum MapDataError {
    MAP_DATA_OK = 0,
    MAP_DATA_ERR_ARGUMENT,
    MAP_DATA_ERR_TRUNCATED,
    MAP_DATA_ERR_RANGE,
    MAP_DATA_ERR_MEMORY
} MapDataError;

MapDataSelection rich4_map_data_select(int stage, int map, int mapdat_available);
MapDataError rich4_map_data_parse(const unsigned char *data,
                                  size_t size,
                                  MapDataArchive source_archive,
                                  size_t source_resource_index,
                                  MapData *out_map);
/* On success out_map owns five arrays; failure leaves it empty. */
RICH4_NULL_SAFE void rich4_map_data_free(MapData *map);
const char *rich4_map_data_error_name(MapDataError error);
int rich4_map_data_company_links(const MapData *map, int *out_links,
                                 size_t link_count);
int rich4_map_data_special_place_nodes(const MapData *map, int *out_prison,
                                       int *out_hospital);
int rich4_map_facility_record_write(const MapFacilityRecord *record,
                                    unsigned char out_record[56]);
int rich4_map_auxiliary_record_write(const MapAuxiliaryRecord *record,
                                     unsigned char out_record[52]);

#endif
