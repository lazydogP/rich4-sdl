#include "data/map_data.h"
#include "data/map_data_test.h"

#include <stdlib.h>
#include <string.h>

#ifdef RICH4_MAP_DATA_TESTING
static int test_alloc_failure = -1;

void rich4_map_data_test_fail_alloc_after(int count)
{
    test_alloc_failure = count;
}

void rich4_map_data_test_clear_failures(void)
{
    test_alloc_failure = -1;
}
#endif

static void *map_calloc(size_t count, size_t size)
{
#ifdef RICH4_MAP_DATA_TESTING
    if (test_alloc_failure == 0) {
        test_alloc_failure = -1;
        return NULL;
    }
    if (test_alloc_failure > 0) test_alloc_failure--;
#endif
    return calloc(count, size);
}

static uint16_t read_u16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int table_fits(size_t size, uint32_t offset, uint32_t count, size_t stride)
{
    size_t entries = (size_t)count + 1;
    return offset <= size && entries <= (size - offset) / stride;
}

MapDataSelection rich4_map_data_select(int stage, int map, int mapdat_available)
{
    MapDataSelection selection;
    selection.gmap = stage * 4 + map;
    selection.archive = mapdat_available ? MAP_DATA_ARCHIVE_MAPDAT : MAP_DATA_ARCHIVE_MAP;
    selection.resource_index = mapdat_available
        ? (size_t)selection.gmap
        : (size_t)(selection.gmap * 2 + 1);
    return selection;
}

void rich4_map_data_free(MapData *map)
{
    if (map == NULL) return;
    free(map->routes);
    free(map->lands);
    free(map->facilities);
    free(map->auxiliary);
    free(map->specials);
    memset(map, 0, sizeof(*map));
}

MapDataError rich4_map_data_parse(const unsigned char *data,
                                  size_t size,
                                  MapDataArchive source_archive,
                                  size_t source_resource_index,
                                  MapData *out_map)
{
    uint32_t counts[5];
    uint32_t offsets[5];
    static const size_t strides[5] = {40, 52, 56, 52, 28};
    size_t i;
    MapData parsed;
    if (data == NULL || out_map == NULL) return MAP_DATA_ERR_ARGUMENT;
    memset(out_map, 0, sizeof(*out_map));
    if (size < 40) return MAP_DATA_ERR_TRUNCATED;
    memset(&parsed, 0, sizeof(parsed));
    parsed.source_archive = source_archive;
    parsed.source_resource_index = source_resource_index;
    for (i = 0; i < 5; i++) {
        counts[i] = read_u32(data + i * 8);
        offsets[i] = read_u32(data + i * 8 + 4);
        if (!table_fits(size, offsets[i], counts[i], strides[i])) {
            return MAP_DATA_ERR_RANGE;
        }
    }
    parsed.route_count = (size_t)counts[0] + 1;
    parsed.land_count = (size_t)counts[1] + 1;
    parsed.facility_count = (size_t)counts[2] + 1;
    parsed.auxiliary_count = (size_t)counts[3] + 1;
    parsed.special_count = (size_t)counts[4] + 1;
    parsed.routes = (MapRouteNode *)map_calloc(parsed.route_count, sizeof(*parsed.routes));
    parsed.lands = (MapLandRecord *)map_calloc(parsed.land_count, sizeof(*parsed.lands));
    parsed.facilities = (MapFacilityRecord *)map_calloc(parsed.facility_count, sizeof(*parsed.facilities));
    parsed.auxiliary = (MapAuxiliaryRecord *)map_calloc(parsed.auxiliary_count, sizeof(*parsed.auxiliary));
    parsed.specials = (MapSpecialRecord *)map_calloc(parsed.special_count, sizeof(*parsed.specials));
    if (parsed.routes == NULL || parsed.lands == NULL || parsed.facilities == NULL ||
        parsed.auxiliary == NULL || parsed.specials == NULL) {
        rich4_map_data_free(&parsed);
        return MAP_DATA_ERR_MEMORY;
    }
    for (i = 0; i < parsed.route_count; i++) {
        const unsigned char *p = data + offsets[0] + i * 40;
        MapRouteNode *r = &parsed.routes[i];
        memcpy(r->raw, p, sizeof(r->raw));
        r->x = (int16_t)read_u16(p);
        r->y = (int16_t)read_u16(p + 2);
        r->next[0] = read_u16(p + 24);
        r->next[1] = read_u16(p + 26);
        r->next[2] = read_u16(p + 28);
        r->next[3] = read_u16(p + 30);
        r->object = rich4_map_object_ref(read_u16(p + 32));
        r->display_chunk = read_u16(p + 34);
        r->flags = read_u32(p + 36);
    }
    for (i = 0; i < parsed.land_count; i++) {
        const unsigned char *p = data + offsets[1] + i * 52;
        MapLandRecord *r = &parsed.lands[i];
        memcpy(r->raw, p, sizeof(r->raw));
        r->x = (int16_t)read_u16(p); r->y = (int16_t)read_u16(p + 2);
        r->parity_overlay = p[0x17];
        r->type = p[0x18]; r->owner_raw = p[0x19]; r->level = p[0x1a]; r->variant = p[0x1b];
        r->price = read_u16(p + 0x1c); r->price_per_level = read_u16(p + 0x1e);
        r->toll_by_level[0] = read_u16(p + 0x20);
        r->toll_by_level[1] = read_u16(p + 0x22);
        r->toll_by_level[2] = read_u16(p + 0x24);
        r->toll_by_level[3] = read_u16(p + 0x26);
        r->toll_by_level[4] = read_u16(p + 0x28);
        r->toll_by_level[5] = read_u16(p + 0x2a);
        r->runtime_value = read_u32(p + 0x2c);
    }
    for (i = 0; i < parsed.facility_count; i++) {
        const unsigned char *p = data + offsets[2] + i * 56;
        MapFacilityRecord *r = &parsed.facilities[i];
        memcpy(r->raw, p, sizeof(r->raw));
        r->x = (int16_t)read_u16(p); r->y = (int16_t)read_u16(p + 2);
        r->parity_overlay = p[0x1c];
        r->type = p[0x18]; r->owner_raw = p[0x19]; r->level = p[0x1a]; r->variant = p[0x1b];
        r->research_selection = p[0x1d];
        r->research_turns = p[0x1e];
        r->price = read_u16(p + 0x22); r->price_per_level = read_u16(p + 0x24);
        r->charge_by_level[0] = read_u16(p + 0x24);
        r->charge_by_level[1] = read_u16(p + 0x26);
        r->charge_by_level[2] = read_u16(p + 0x28);
        r->charge_by_level[3] = read_u16(p + 0x2a);
        r->charge_by_level[4] = read_u16(p + 0x2c);
        r->charge_by_level[5] = read_u16(p + 0x2e);
        r->runtime_value = read_u32(p + 0x30);
        r->last_change = read_u32(p + 0x34);
    }
    for (i = 0; i < parsed.auxiliary_count; i++) {
        const unsigned char *p = data + offsets[3] + i * 52;
        MapAuxiliaryRecord *r = &parsed.auxiliary[i];
        memcpy(r->raw, p, sizeof(r->raw));
        r->x = (int16_t)read_u16(p); r->y = (int16_t)read_u16(p + 2);
        r->owner_raw = p[0x18]; r->stock_index = p[0x19];
        r->commerce_type = p[0x1a]; r->variant = p[0x1b];
        memcpy(r->ranking_raw, p + 0x1c, sizeof(r->ranking_raw));
        r->sprite_resource = read_u16(p + 0x20);
        r->base_fee = read_u16(p + 0x22);
        r->company_value = read_u32(p + 0x24);
        r->period_business = read_u32(p + 0x28);
        r->total_business = read_u32(p + 0x2c);
        r->available_shares = read_u32(p + 0x30);
    }
    for (i = 0; i < parsed.special_count; i++) {
        const unsigned char *p = data + offsets[4] + i * 28;
        MapSpecialRecord *r = &parsed.specials[i];
        memcpy(r->raw, p, sizeof(r->raw));
        r->x = (int16_t)read_u16(p); r->y = (int16_t)read_u16(p + 2);
        r->variant = p[0x18]; r->sprite_resource = read_u16(p + 26);
    }
    *out_map = parsed;
    return MAP_DATA_OK;
}

const char *rich4_map_data_error_name(MapDataError error)
{
    switch (error) {
    case MAP_DATA_OK: return "ok";
    case MAP_DATA_ERR_ARGUMENT: return "argument";
    case MAP_DATA_ERR_TRUNCATED: return "truncated";
    case MAP_DATA_ERR_RANGE: return "range";
    case MAP_DATA_ERR_MEMORY: return "memory";
    default: return "unknown";
    }
}

int rich4_map_data_company_links(const MapData *map, int *out_links,
                                 size_t link_count)
{
    size_t i;
    if (map == NULL || out_links == NULL || link_count < 12 ||
        (map->auxiliary_count != 0 && map->auxiliary == NULL)) return 0;
    for (i = 0; i < link_count; i++) out_links[i] = -1;
    for (i = 1; i < map->auxiliary_count; i++) {
        unsigned int stock = map->auxiliary[i].stock_index;
        if (stock < 12) out_links[stock] = (int)i - 1;
    }
    return 1;
}

int rich4_map_data_special_place_nodes(const MapData *map, int *out_prison,
                                       int *out_hospital)
{
    size_t i;
    int prison = -1, hospital = -1;
    if (map == NULL || out_prison == NULL || out_hospital == NULL ||
        (map->route_count != 0 && map->routes == NULL)) return 0;
    for (i = 0; i < map->route_count; i++) {
        if (map->routes[i].object.raw_object_id == 0x1f42) prison = (int)i;
        if (map->routes[i].object.raw_object_id == 0x1f41) hospital = (int)i;
    }
    *out_prison = prison;
    *out_hospital = hospital;
    return prison >= 0 && hospital >= 0;
}

static void write_u16(unsigned char *p, unsigned int value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void write_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

int rich4_map_facility_record_write(const MapFacilityRecord *record,
                                    unsigned char out_record[56])
{
    int i;
    if (record == NULL || out_record == NULL ||
        record->price_per_level != record->charge_by_level[0]) return 0;
    memcpy(out_record, record->raw, 56);
    write_u16(out_record, (uint16_t)record->x);
    write_u16(out_record + 2, (uint16_t)record->y);
    out_record[0x18] = record->type;
    out_record[0x19] = record->owner_raw;
    out_record[0x1a] = record->level;
    out_record[0x1b] = record->variant;
    out_record[0x1c] = record->parity_overlay;
    out_record[0x1d] = record->research_selection;
    out_record[0x1e] = record->research_turns;
    write_u16(out_record + 0x22, record->price);
    for (i = 0; i < 6; i++)
        write_u16(out_record + 0x24 + i * 2, record->charge_by_level[i]);
    write_u32(out_record + 0x30, record->runtime_value);
    write_u32(out_record + 0x34, record->last_change);
    return 1;
}

int rich4_map_auxiliary_record_write(const MapAuxiliaryRecord *record,
                                     unsigned char out_record[52])
{
    if (record == NULL || out_record == NULL) return 0;
    memcpy(out_record, record->raw, 52);
    write_u16(out_record, (uint16_t)record->x);
    write_u16(out_record + 2, (uint16_t)record->y);
    out_record[0x18] = record->owner_raw;
    out_record[0x19] = record->stock_index;
    out_record[0x1a] = record->commerce_type;
    out_record[0x1b] = record->variant;
    memcpy(out_record + 0x1c, record->ranking_raw, sizeof(record->ranking_raw));
    write_u16(out_record + 0x20, record->sprite_resource);
    write_u16(out_record + 0x22, record->base_fee);
    write_u32(out_record + 0x24, record->company_value);
    write_u32(out_record + 0x28, record->period_business);
    write_u32(out_record + 0x2c, record->total_business);
    write_u32(out_record + 0x30, record->available_shares);
    return 1;
}
