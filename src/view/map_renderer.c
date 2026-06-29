#include "view/map_renderer.h"

#include <stdlib.h>
#include <string.h>

static int sort_low_word(int sort_key)
{
    int value = sort_key & 0xffff;
    return (value & 0x8000) != 0 ? value - 0x10000 : value;
}

int rich4_map_compare_sort_keys(int a_key, int b_key)
{
    int a_low = sort_low_word(a_key);
    int b_low = sort_low_word(b_key);
    return a_low < b_low ? -1 : a_low > b_low ? 1 : 0;
}

int rich4_map_rotated_variant(int variant_base, int view_direction)
{
    return (8 - ((variant_base & 7) + (view_direction & 7))) & 7;
}

int rich4_map_property_render_style(int level,
                                    int owner,
                                    int actor_count,
                                    int owner_character,
                                    int variant_base,
                                    int view_direction,
                                    MapPropertyRenderStyle *out_style)
{
    MapPropertyRenderStyle style;
    if (out_style == NULL || level < 0 || actor_count < 0 ||
        actor_count > GAME_MAX_PLAYERS) {
        return 0;
    }
    memset(&style, 0, sizeof(style));
    if (level > 0) {
        style.visible = 1;
        style.variant = rich4_map_rotated_variant(variant_base, view_direction);
        style.palette_owner = owner >= 0 ? owner + 1 : 0;
    } else if (owner >= 0 && owner < actor_count) {
        style.visible = 1;
        style.use_owner_marker = 1;
        style.variant = owner_character;
        style.palette_owner = 0xff;
    }
    *out_style = style;
    return 1;
}

static int compare_visible_qsort(const void *a, const void *b)
{
    const GameVisibleMapObject *left = (const GameVisibleMapObject *)a;
    const GameVisibleMapObject *right = (const GameVisibleMapObject *)b;
    return rich4_map_compare_sort_keys(left->sort_key, right->sort_key);
}

int rich4_map_visible_sort_key(int list_index, int screen_y)
{
    return rich4_map_visible_sort_key_bias(list_index, screen_y, 0);
}

int rich4_map_visible_sort_key_bias(int list_index, int screen_y, int type_bias)
{
    unsigned int low = ((unsigned int)(screen_y * 16 + type_bias)) & 0xffffu;
    return ((list_index & 0xffff) << 16) | (int)low;
}

void rich4_map_sort_visible(GameVisibleMapObject *objects, int count)
{
    if (objects == NULL || count <= 1) return;
    /* 0x457E6C is an unstable qsort-style routine.  Its comparator at
     * 0x4079F9 reads only the signed low word; the high-word insertion slot
     * is payload, not a tie-break. */
    qsort(objects, (size_t)count, sizeof(*objects), compare_visible_qsort);
}

int rich4_map_collect_visible(const MapRenderObjectInput *inputs,
                              int input_count,
                              const GameViewportModel *viewport,
                              GameVisibleMapObject *out_objects,
                              int capacity)
{
    int count = 0;
    int i;
    if (inputs == NULL || viewport == NULL || out_objects == NULL || capacity <= 0)
        return 0;
    for (i = 0; i < input_count && count < capacity; i++) {
        const MapRenderObjectInput *input = &inputs[i];
        int tile_x;
        int tile_y;
        GameVisibleMapObject *output;
        if (!input->drawable) continue;
        tile_x = (input->world_x >> 5) - (viewport->center_world_x >> 5) + 14;
        tile_y = (input->world_y >> 5) - (viewport->center_world_y >> 5) + 14;
        if (tile_x < 0 || tile_x > 28 || tile_y < 0 || tile_y > 28) continue;
        output = &out_objects[count];
        output->asset_index = input->asset_index;
        output->sprite = input->sprite;
        output->object_id = input->object_id;
        output->owner = input->owner;
        output->variant = input->variant;
        rich4_game_viewport_project(viewport, input->world_x, input->world_y,
                                    &output->x, &output->y);
        output->x += input->screen_offset_x;
        output->y += input->screen_offset_y;
        output->sort_key = input->use_fixed_sort_low
            ? ((count & 0xffff) << 16) | (input->fixed_sort_low & 0xffff)
            : rich4_map_visible_sort_key_bias(count, output->y, input->sort_bias);
        count++;
    }
    rich4_map_sort_visible(out_objects, count);
    return count;
}

int rich4_map_dynamic_resource_chunk(int type)
{
    return type >= 1 && type <= GAME_MAP_DYNAMIC_TYPE_COUNT
        ? 396 + type - 1 : -1;
}

static const int attached_x[8] = {-10,-22,-22,-10,10,22,22,10};
static const int attached_y[8] = {-22,-10,10,22,22,10,-10,-22};

/* 0x40B066 reads the attached queue anchor produced by 0x4090A2/0x408C70.
 * direction is the god chunk, already turned four steps from the actor. */
void rich4_map_god_detachment_anchor(const GameViewportModel *viewport,
    int world_x, int world_y, int direction, int *out_x, int *out_y)
{
    int x, y, actor_direction = (direction + 4) & 7;
    rich4_game_viewport_project(viewport, world_x, world_y, &x, &y);
    if (out_x != NULL) *out_x = x + attached_x[actor_direction];
    if (out_y != NULL) *out_y = y + attached_y[actor_direction];
}

int rich4_map_dynamic_build_input(const GameMapDynamicObjectState *object,
                                  int slot,
                                  const GameViewportModel *viewport,
                                  const GameActorView *actors,
                                  int actor_count,
                                  const MapRouteNode *routes,
                                  size_t route_count,
                                  const unsigned char *actor_type18_anchor,
                                  MapRenderObjectInput *out_input)
{
    static const int type18_x[8] = {-18,-44,-44,-18,18,44,44,18};
    static const int type18_y[8] = {-44,-18,18,44,44,18,-18,-44};
    int direction;
    int player;

    if (object == NULL || viewport == NULL || out_input == NULL ||
        slot < 0 || slot >= GAME_MAP_DYNAMIC_OBJECT_COUNT ||
        (object->node == 0 && object->flight_active == 0 &&
         object->attached_player == 0)) return 0;
    /* 0x408F9F..0x4090BA draws attachments at the actor's position.
     * Unlike the original object's retained node, core tool pickup clears
     * node to remove road occupancy; attached_player still keeps it live. */
    memset(out_input, 0, sizeof(*out_input));
    out_input->asset_index = slot;
    out_input->sprite = object->type;
    out_input->owner = -1;
    out_input->drawable = 1;
    if (object->attached_player != 0) {
        player = (int)object->attached_player - 1;
        if (actors == NULL || player < 0 || player >= actor_count) return 0;
        if (actors[player].attachments_hidden) return 0;
        /* Attached gods follow the actor's interpolated visual position.
         * They remain drawable during the walk; hiding them here makes the
         * object blink out for the entire movement segment. */
        out_input->world_x = actors[player].world_x;
        out_input->world_y = actors[player].world_y;
        direction = actors[player].visual_direction & 7;
        out_input->variant = (direction + 4) & 7;
        if (object->type == 18 &&
            (actors[player].has_dynamic_attachment != 0 ||
             (actor_type18_anchor != NULL &&
              actor_type18_anchor[player] != 0))) {
            out_input->screen_offset_x = type18_x[direction];
            out_input->screen_offset_y = type18_y[direction];
        } else {
            out_input->screen_offset_x = attached_x[direction];
            out_input->screen_offset_y = attached_y[direction];
        }
    } else if (object->flight_active != 0) {
        int tile_x = ((int)object->world_x >> 5) -
                     (viewport->center_world_x >> 5) + 14;
        int tile_y = ((int)object->world_y >> 5) -
                     (viewport->center_world_y >> 5) + 14;
        if (tile_x < 0 || tile_x > 28 || tile_y < 0 || tile_y > 28) {
            return 0;
        }
        out_input->world_x = (int)object->world_x;
        out_input->world_y = (int)object->world_y;
        out_input->variant = (object->flight_direction + 8 -
                              viewport->direction) & 7;
        out_input->use_fixed_sort_low = 1;
        out_input->fixed_sort_low = 0x7ff0;
    } else {
        if (routes == NULL || object->node >= route_count) return 0;
        out_input->world_x = routes[object->node].x;
        out_input->world_y = routes[object->node].y;
        out_input->variant = (object->node_direction + 8 -
                              viewport->direction) & 7;
    }
    /* 0x408C99/0x408CB1 mark attached bombs only. The following flight
     * branch uses ID zero; the ground branch at 0x408E0E has no label bit. */
    out_input->object_id = object->flight_active && !object->attached_player
        ? 0 : 0x8000 | ((slot + 1) << 8);
    if (object->type == 18 && object->attached_player != 0)
        out_input->object_id |= 0x4000;
    return 1;
}

uint16_t rich4_map_owner_palette_color(int owner)
{
    static const uint16_t colors[GAME_MAX_PLAYERS] = {0x7c00, 0x03e0, 0x001f, 0x7fe0};
    int index;
    if (owner <= 0) return 0;
    index = (owner - 1) % GAME_MAX_PLAYERS;
    if (index < 0) index += GAME_MAX_PLAYERS;
    return colors[index];
}

uint16_t rich4_map_character_palette_color(uint32_t rgb)
{
    /* players.c stores the logical character color as 0x00RRGGBB.
     * sub_4551F0 then selects a converter for the active display format;
     * this compositor keeps the resource-side replacement in RGB555. */
    unsigned int r = (rgb >> 16) & 0xffu;
    unsigned int g = (rgb >> 8) & 0xffu;
    unsigned int b = rgb & 0xffu;
    return (uint16_t)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
}

uint16_t rich4_character_rgb565_color(uint32_t rgb)
{
    /* Standard-display branch sub_45525C: logical 0x00RRGGBB to RGB565. */
    return (uint16_t)(((rgb >> 8) & 0xf800u) |
                      ((rgb >> 5) & 0x07e0u) |
                      ((rgb >> 3) & 0x001fu));
}

uint32_t rich4_rgb565_to_rgb888_truncated(uint16_t pixel)
{
    /* Original 16-bit capture/readback convention: expand by zero-filling
     * the discarded low bits, rather than replicating them to reach 255. */
    return ((uint32_t)(pixel & 0xf800u) << 8) |
           ((uint32_t)(pixel & 0x07e0u) << 5) |
           ((uint32_t)(pixel & 0x001fu) << 3);
}

static uint16_t rgb555_to_rgb565(uint16_t pixel)
{
    /* Original sub_451801, n2_6 == 1: do not replicate the 5-bit green
     * high bit into the new sixth bit. */
    return (uint16_t)(((pixel & 0x7c00u) << 1) |
                      ((pixel & 0x03e0u) << 1) | (pixel & 0x001fu));
}

void rich4_rgb565_clear(Rgb565Framebuffer *framebuffer, uint16_t color)
{
    int x, y;
    if (framebuffer == NULL || framebuffer->pixels == NULL ||
        framebuffer->width <= 0 || framebuffer->height <= 0 ||
        framebuffer->stride < framebuffer->width) return;
    for (y = 0; y < framebuffer->height; y++)
        for (x = 0; x < framebuffer->width; x++)
            framebuffer->pixels[y * framebuffer->stride + x] = color;
}

int rich4_map_blit_indexed(Rgb565Framebuffer *framebuffer,
                           const uint8_t *indices,
                           int width,
                           int height,
                           int dst_x,
                           int dst_y,
                           const uint16_t palette[256],
                           int transparent_zero,
                           int owner)
{
    int x, y;
    uint16_t owner_color = rich4_map_owner_palette_color(owner);
    if (framebuffer == NULL || framebuffer->pixels == NULL || indices == NULL ||
        palette == NULL || width <= 0 || height <= 0) return 0;
    for (y = 0; y < height; y++) {
        int py = dst_y + y;
        if (py < 0 || py >= framebuffer->height) continue;
        for (x = 0; x < width; x++) {
            int px = dst_x + x;
            uint8_t index = indices[y * width + x];
            if (px < 0 || px >= framebuffer->width || (transparent_zero && index == 0))
                continue;
            framebuffer->pixels[py * framebuffer->stride + px] = rgb555_to_rgb565(
                index == 0xff && owner > 0 ? owner_color : palette[index]);
        }
    }
    return 1;
}
