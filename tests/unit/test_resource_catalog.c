#include <assert.h>
#include <stdio.h>

#include "data/resource_catalog.h"

static void expect_map(int ok, ResourceCatalogEntry entry, size_t index,
                       unsigned long evidence)
{
    assert(ok);
    assert(entry.archive == RESOURCE_ARCHIVE_MAP);
    assert(entry.resource_index == index);
    assert(entry.evidence_address == evidence);
    assert(entry.confidence == RESOURCE_MAPPING_CONFIRMED);
}

int main(void)
{
    int ok;
    int gmap;
    int slot;
    int stage;
    int map;
    ResourceCatalogEntry entry = rich4_resource_catalog_unknown();
    assert(entry.archive == RESOURCE_ARCHIVE_UNKNOWN && entry.confidence == RESOURCE_MAPPING_UNKNOWN);
    ok = rich4_resource_catalog_map_common(0, &entry); expect_map(ok, entry, 24, 0x407BABul);
    ok = rich4_resource_catalog_map_common(2, &entry); expect_map(ok, entry, 26, 0x407BBFul);
    ok = rich4_resource_catalog_map_view(7, 4, &entry); expect_map(ok, entry, 78, 0x407E3Bul);
    ok = rich4_resource_catalog_map_overlay(7, &entry); expect_map(ok, entry, 86, 0x407E6Cul);
    ok = rich4_resource_catalog_map_stage_tile(0, 3, 16, &entry); expect_map(ok, entry, 103, 0x407E8Eul);
    ok = rich4_resource_catalog_map_stage_tile(1, 3, 16, &entry); expect_map(ok, entry, 171, 0x407EC1ul);
    ok = rich4_resource_catalog_map_character(11, &entry); expect_map(ok, entry, 38, 0x407FC7ul);
    for (slot = 0; slot < 3; slot++) {
        static const size_t common[] = {24, 25, 26};
        assert(rich4_resource_catalog_map_common(slot, &entry));
        assert(entry.resource_index == common[slot]);
    }
    for (gmap = 0; gmap < 8; gmap++) {
        for (slot = 0; slot < 5; slot++) {
            assert(rich4_resource_catalog_map_view(gmap, slot, &entry));
            assert(entry.resource_index == (size_t)(gmap * 5 + 39 + slot));
        }
        assert(rich4_resource_catalog_map_overlay(gmap, &entry));
        assert(entry.resource_index == (size_t)(gmap + 79));
    }
    for (stage = 0; stage < 2; stage++) {
        for (map = 0; map < 4; map++) {
            for (slot = 0; slot < 17; slot++) {
                size_t expected = stage == 0 ? (size_t)(87 + slot)
                                             : (size_t)(104 + map * 17 + slot);
                assert(rich4_resource_catalog_map_stage_tile(stage, map, slot, &entry));
                assert(entry.resource_index == expected);
            }
        }
    }
    assert(rich4_resource_catalog_panel_event(12, &entry));
    assert(entry.archive == RESOURCE_ARCHIVE_PANEL && entry.resource_index == 90);
    assert(rich4_resource_catalog_panel_ending_layout(&entry) && entry.resource_index == 93);
    assert(rich4_resource_catalog_panel_ending_character(11, &entry) && entry.resource_index == 111);
    assert(rich4_resource_catalog_panel_dialog(&entry) && entry.resource_index == 112);
    assert(rich4_resource_catalog_panel_ai_settings(&entry) &&
           entry.archive == RESOURCE_ARCHIVE_PANEL &&
           entry.resource_index == 77 &&
           entry.evidence_address == 0x41E345ul);
    assert(rich4_resource_catalog_data_common_dialog(&entry) &&
           entry.archive == RESOURCE_ARCHIVE_DATA &&
           entry.resource_index == 517 &&
           entry.evidence_address == 0x440C14ul);
    assert(rich4_resource_catalog_data_speech_icons(&entry) &&
           entry.archive == RESOURCE_ARCHIVE_DATA &&
           entry.resource_index == 519 &&
           entry.evidence_address == 0x44F24Bul);
    assert(rich4_resource_catalog_data_confirm(&entry) &&
           entry.archive == RESOURCE_ARCHIVE_DATA &&
           entry.resource_index == 440 &&
           entry.evidence_address == 0x453A64ul);
    assert(rich4_resource_catalog_data_property_max_level(&entry) &&
           entry.archive == RESOURCE_ARCHIVE_DATA &&
           entry.resource_index == 523 &&
           entry.evidence_address == 0x40B0D2ul);
    assert(rich4_resource_catalog_data_dynamic_object(1, &entry) &&
           entry.archive == RESOURCE_ARCHIVE_DATA &&
           entry.resource_index == 396 &&
           entry.evidence_address == 0x40829Dul);
    assert(rich4_resource_catalog_data_dynamic_object(20, &entry) &&
           entry.resource_index == 415);
    assert(rich4_resource_catalog_data_player_model(11, 20, &entry) &&
           entry.archive == RESOURCE_ARCHIVE_DATA &&
           entry.resource_index == 379 &&
           entry.evidence_address == 0x40B93Bul);
    assert(rich4_resource_catalog_data_special_actor(3, 3, &entry) &&
           entry.resource_index == 395 &&
           entry.evidence_address == 0x40B93Bul);
    entry.archive = RESOURCE_ARCHIVE_MAP;
    entry.confidence = RESOURCE_MAPPING_CONFIRMED;
    assert(!rich4_resource_catalog_map_view(8, 0, &entry));
    assert(entry.archive == RESOURCE_ARCHIVE_UNKNOWN && entry.confidence == RESOURCE_MAPPING_UNKNOWN);
    assert(!rich4_resource_catalog_map_stage_tile(2, 0, 0, &entry));
    assert(!rich4_resource_catalog_panel_event(13, &entry));
    assert(!rich4_resource_catalog_data_dynamic_object(21, &entry));
    assert(!rich4_resource_catalog_data_player_model(12, 0, &entry));
    assert(!rich4_resource_catalog_data_special_actor(4, 0, &entry));
    assert(entry.confidence == RESOURCE_MAPPING_UNKNOWN);
    puts("resource catalog: ok");
    return 0;
}
