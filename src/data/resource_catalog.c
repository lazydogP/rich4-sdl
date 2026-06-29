#include "data/resource_catalog.h"

#include "data/resource_constants.h"

static int confirmed(ResourceCatalogEntry *out, ResourceArchiveId archive,
                     size_t index, unsigned long evidence)
{
    if (out == NULL) return 0;
    out->archive = archive;
    out->resource_index = index;
    out->evidence_address = evidence;
    out->confidence = RESOURCE_MAPPING_CONFIRMED;
    return 1;
}

static int unknown(ResourceCatalogEntry *out)
{
    if (out != NULL) *out = rich4_resource_catalog_unknown();
    return 0;
}

ResourceCatalogEntry rich4_resource_catalog_unknown(void)
{
    ResourceCatalogEntry entry;
    entry.archive = RESOURCE_ARCHIVE_UNKNOWN;
    entry.resource_index = 0;
    entry.evidence_address = 0;
    entry.confidence = RESOURCE_MAPPING_UNKNOWN;
    return entry;
}

int rich4_resource_catalog_map_common(int slot, ResourceCatalogEntry *out)
{
    static const size_t indices[3] = {24, 25, 26};
    static const unsigned long evidence[3] = {0x407BABul, 0x407F77ul, 0x407BBFul};
    if (slot < 0 || slot >= 3) return unknown(out);
    return confirmed(out, RESOURCE_ARCHIVE_MAP, indices[slot], evidence[slot]);
}

int rich4_resource_catalog_map_view(int gmap, int slot, ResourceCatalogEntry *out)
{
    if (gmap < 0 || gmap >= 8 || slot < 0 || slot >= 5) return unknown(out);
    return confirmed(out, RESOURCE_ARCHIVE_MAP, (size_t)(gmap * 5 + 39 + slot), 0x407E3Bul);
}

int rich4_resource_catalog_map_overlay(int gmap, ResourceCatalogEntry *out)
{
    if (gmap < 0 || gmap >= 8) return unknown(out);
    return confirmed(out, RESOURCE_ARCHIVE_MAP, (size_t)(gmap + 79), 0x407E6Cul);
}

int rich4_resource_catalog_map_stage_tile(int stage, int map, int slot,
                                          ResourceCatalogEntry *out)
{
    size_t index;
    unsigned long evidence;
    if (stage < 0 || stage > 1 || map < 0 || map >= 4 || slot < 0 || slot >= 17) return unknown(out);
    if (stage == 0) {
        index = (size_t)(87 + slot);
        evidence = 0x407E8Eul;
    } else {
        index = (size_t)(104 + map * 17 + slot);
        evidence = 0x407EC1ul;
    }
    return confirmed(out, RESOURCE_ARCHIVE_MAP, index, evidence);
}

int rich4_resource_catalog_map_character(int model_resource_id,
                                         ResourceCatalogEntry *out)
{
    if (model_resource_id < 0 || model_resource_id > 255) return unknown(out);
    return confirmed(out, RESOURCE_ARCHIVE_MAP,
                     (size_t)(27 + model_resource_id), 0x407FC7ul);
}

int rich4_resource_catalog_panel_event(int slot, ResourceCatalogEntry *out)
{
    if (slot < 0 || slot >= 13) return unknown(out);
    return confirmed(out, RESOURCE_ARCHIVE_PANEL, (size_t)(78 + slot), 0x415253ul);
}

int rich4_resource_catalog_panel_ending_layout(ResourceCatalogEntry *out)
{
    return confirmed(out, RESOURCE_ARCHIVE_PANEL, 93, 0x40768Cul);
}

int rich4_resource_catalog_panel_ending_character(int character,
                                                  ResourceCatalogEntry *out)
{
    if (character < 0 || character >= 12) return unknown(out);
    return confirmed(out, RESOURCE_ARCHIVE_PANEL, (size_t)(100 + character), 0x407673ul);
}

int rich4_resource_catalog_panel_dialog(ResourceCatalogEntry *out)
{
    return confirmed(out, RESOURCE_ARCHIVE_PANEL, 112, 0x40785Bul);
}

int rich4_resource_catalog_panel_ai_settings(ResourceCatalogEntry *out)
{
    /* sub_41E345 reads resource 77 through 0x48A05C, the Panel.mkf handle. */
    return confirmed(out, RESOURCE_ARCHIVE_PANEL, 77, 0x41E345ul);
}

int rich4_resource_catalog_data_common_dialog(ResourceCatalogEntry *out)
{
    /* data_0205: read_mkf(mkf_data, 0x205, ...).  sub_440BA8 uses
     * chunk_tab[5] as the common question-dialog background. */
    return confirmed(out, RESOURCE_ARCHIVE_DATA, 0x205, 0x440C14ul);
}

int rich4_resource_catalog_data_speech_icons(ResourceCatalogEntry *out)
{
    /* player_say's @NN branch draws data_0207 chunk NN-1 at (240,130). */
    return confirmed(out, RESOURCE_ARCHIVE_DATA, 0x207, 0x44F24Bul);
}

int rich4_resource_catalog_data_confirm(ResourceCatalogEntry *out)
{
    return confirmed(out, RESOURCE_ARCHIVE_DATA, 440, 0x453A64ul);
}

int rich4_resource_catalog_data_property_max_level(ResourceCatalogEntry *out)
{
    /* sub_40B0CD reads Data.mkf[523] immediately after sub_41982D writes
     * an ordinary land or facility level to 5. */
    return confirmed(out, RESOURCE_ARCHIVE_DATA, 523, 0x40B0D2ul);
}

int rich4_resource_catalog_data_dynamic_object(int type,
                                               ResourceCatalogEntry *out)
{
    /* 0x40829D computes Data.mkf[396 + type - 1] while building the map
     * object queue.  Type 18/19 have different chunk layouts, but their
     * archive mapping is still the same contiguous table. */
    if (type < 1 || type > GAME_DATA_MAP_STATUS_RESOURCE_COUNT)
        return unknown(out);
    return confirmed(out, RESOURCE_ARCHIVE_DATA,
                     (size_t)(396 + type - 1), 0x40829Dul);
}

int rich4_resource_catalog_data_player_model(int character, int part,
                                             ResourceCatalogEntry *out)
{
    /* 0x40B93B forms 0x80 + character * 21 and then selects the model
     * subresource within that block. */
    if (character < 0 || character >= 12 ||
        part < 0 || part >= GAME_CHARACTER_MODEL_RESOURCES)
        return unknown(out);
    return confirmed(out, RESOURCE_ARCHIVE_DATA,
                     (size_t)(128 + character * GAME_CHARACTER_MODEL_RESOURCES + part),
                     0x40B93Bul);
}

int rich4_resource_catalog_data_special_actor(int actor, int part,
                                              ResourceCatalogEntry *out)
{
    /* The fixed map actors are slots 4..7 in 0x40B93B, giving Data.mkf
     * resources 380..395 as four four-resource blocks. */
    if (actor < 0 || actor >= GAME_SPECIAL_ACTOR_MODEL_COUNT ||
        part < 0 || part >= GAME_SPECIAL_ACTOR_MODEL_RESOURCES)
        return unknown(out);
    return confirmed(out, RESOURCE_ARCHIVE_DATA,
                     (size_t)(380 + actor * GAME_SPECIAL_ACTOR_MODEL_RESOURCES + part),
                     0x40B93Bul);
}
