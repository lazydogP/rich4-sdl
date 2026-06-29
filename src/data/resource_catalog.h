#ifndef RICH4_RESOURCE_CATALOG_H
#define RICH4_RESOURCE_CATALOG_H

#include <stddef.h>

typedef enum ResourceArchiveId {
    RESOURCE_ARCHIVE_UNKNOWN = 0,
    RESOURCE_ARCHIVE_MAP,
    RESOURCE_ARCHIVE_PANEL,
    RESOURCE_ARCHIVE_DATA
} ResourceArchiveId;

typedef enum ResourceMappingConfidence {
    RESOURCE_MAPPING_UNKNOWN = 0,
    RESOURCE_MAPPING_CONFIRMED
} ResourceMappingConfidence;

typedef struct ResourceCatalogEntry {
    ResourceArchiveId archive;
    size_t resource_index;
    unsigned long evidence_address;
    ResourceMappingConfidence confidence;
} ResourceCatalogEntry;

ResourceCatalogEntry rich4_resource_catalog_unknown(void);
int rich4_resource_catalog_map_common(int slot, ResourceCatalogEntry *out);
int rich4_resource_catalog_map_view(int gmap, int slot, ResourceCatalogEntry *out);
int rich4_resource_catalog_map_overlay(int gmap, ResourceCatalogEntry *out);
int rich4_resource_catalog_map_stage_tile(int stage, int map, int slot,
                                          ResourceCatalogEntry *out);
int rich4_resource_catalog_map_character(int model_resource_id,
                                         ResourceCatalogEntry *out);
int rich4_resource_catalog_panel_event(int slot, ResourceCatalogEntry *out);
int rich4_resource_catalog_panel_ending_layout(ResourceCatalogEntry *out);
int rich4_resource_catalog_panel_ending_character(int character,
                                                  ResourceCatalogEntry *out);
int rich4_resource_catalog_panel_dialog(ResourceCatalogEntry *out);
int rich4_resource_catalog_panel_ai_settings(ResourceCatalogEntry *out);
int rich4_resource_catalog_data_common_dialog(ResourceCatalogEntry *out);
int rich4_resource_catalog_data_speech_icons(ResourceCatalogEntry *out);
int rich4_resource_catalog_data_confirm(ResourceCatalogEntry *out);
int rich4_resource_catalog_data_property_max_level(ResourceCatalogEntry *out);
int rich4_resource_catalog_data_dynamic_object(int type,
                                               ResourceCatalogEntry *out);
int rich4_resource_catalog_data_player_model(int character, int part,
                                             ResourceCatalogEntry *out);
int rich4_resource_catalog_data_special_actor(int actor, int part,
                                              ResourceCatalogEntry *out);

#endif
