#ifndef RICH4_VIEW_MAP_RENDERER_H
#define RICH4_VIEW_MAP_RENDERER_H

#include <stddef.h>
#include <stdint.h>

#include "view/game_view_model.h"
#include "core/game_map_dynamic_object.h"
#include "api_contract.h"
#include "view/view_types.h"

typedef struct MapRenderObjectInput {
    int asset_index;
    int sprite;
    int object_id;
    int owner;
    int variant;
    int world_x;
    int world_y;
    int screen_offset_x;
    int screen_offset_y;
    int sort_bias;
    int use_fixed_sort_low;
    int fixed_sort_low;
    int drawable;
} MapRenderObjectInput;

typedef struct Rgb565Framebuffer {
    RICH4_CALLER_STORAGE uint16_t *pixels;
    int width;
    int height;
    int stride;
} Rgb565Framebuffer;

typedef struct MapPropertyRenderStyle {
    int visible;
    int use_owner_marker;
    int variant;
    int palette_owner;
} MapPropertyRenderStyle;

int rich4_map_visible_sort_key(int list_index, int screen_y);
int rich4_map_visible_sort_key_bias(int list_index, int screen_y, int type_bias);
int rich4_map_compare_sort_keys(int a_key, int b_key);
int rich4_map_rotated_variant(int variant_base, int view_direction);
int rich4_map_property_render_style(int level,
                                    int owner,
                                    int actor_count,
                                    int owner_character,
                                    int variant_base,
                                    int view_direction,
                                    MapPropertyRenderStyle *out_style);
void rich4_map_sort_visible(GameVisibleMapObject *objects, int count);
int rich4_map_collect_visible(const MapRenderObjectInput *inputs,
                              int input_count,
                              const GameViewportModel *viewport,
                              GameVisibleMapObject *out_objects,
                              int capacity);
uint16_t rich4_map_owner_palette_color(int owner);
uint16_t rich4_map_character_palette_color(uint32_t rgb);
uint16_t rich4_character_rgb565_color(uint32_t rgb);
uint32_t rich4_rgb565_to_rgb888_truncated(uint16_t pixel);
void rich4_rgb565_clear(Rgb565Framebuffer *framebuffer, uint16_t color);
int rich4_map_blit_indexed(Rgb565Framebuffer *framebuffer,
                           const uint8_t *indices,
                           int width,
                           int height,
                           int dst_x,
                           int dst_y,
                           const uint16_t palette[256],
                           int transparent_zero,
                           int owner);
int rich4_map_dynamic_resource_chunk(int type);
void rich4_map_god_detachment_anchor(const GameViewportModel *viewport,
    int world_x, int world_y, int direction, int *out_x, int *out_y);
int rich4_map_dynamic_build_input(const GameMapDynamicObjectState *object,
                                  int slot,
                                  const GameViewportModel *viewport,
                                  const GameActorView *actors,
                                  int actor_count,
                                  const MapRouteNode *routes,
                                  size_t route_count,
                                  const unsigned char *actor_type18_anchor,
                                  MapRenderObjectInput *out_input);

#endif
