#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/dynamic_object_flow.h"
#include "view/map_renderer.h"

int main(void)
{
    GameViewportModel viewport;
    MapRenderObjectInput inputs[3];
    GameVisibleMapObject visible[3];
    uint16_t pixels[12];
    uint16_t expected[12] = {
        0x1111,0x1111,0x1111,0x1111,
        0x1111,0xf800,0xf800,0x1111,
        0x1111,0x07c0,0xf800,0x1111
    };
    uint16_t palette[256];
    uint8_t sprite[4] = {1,255,2,1};
    GameMapDynamicObjectState dynamic[GAME_MAP_DYNAMIC_OBJECT_COUNT];
    MapRenderObjectInput dynamic_input;
    MapRenderObjectInput boundary_inputs[6];
    GameVisibleMapObject boundary_visible[6];
    MapRouteNode routes[2];
    GameActorView actor;
    MapPropertyRenderStyle style;
    Rgb565Framebuffer framebuffer = {pixels, 4, 3, 4};
    int base;
    int direction;
    memset(&viewport, 0, sizeof(viewport));
    memset(inputs, 0, sizeof(inputs));
    inputs[0].asset_index = 10; inputs[0].world_x = 32; inputs[0].world_y = 64; inputs[0].drawable = 1;
    inputs[1].asset_index = 11; inputs[1].world_x = 64; inputs[1].world_y = 64; inputs[1].drawable = 1;
    inputs[2].asset_index = 12; inputs[2].world_x = 4096; inputs[2].world_y = 4096; inputs[2].drawable = 1;
    viewport.center_world_x = 32;
    viewport.center_world_y = 64;
    assert(rich4_map_collect_visible(inputs, 3, &viewport, visible, 3) == 2);
    assert((visible[0].asset_index == 10 || visible[1].asset_index == 10) &&
           (visible[0].asset_index == 11 || visible[1].asset_index == 11));
    assert((rich4_map_visible_sort_key_bias(7, 100, 13) & 0xffff) == 1613);
    for (base = 0; base < 8; base++)
        for (direction = 0; direction < 8; direction++)
            assert(rich4_map_rotated_variant(base, direction) ==
                   ((8 - base - direction) & 7));
    assert(rich4_map_property_render_style(0, 2, 4, 9, 3, 2, &style));
    assert(style.visible && style.use_owner_marker &&
           style.variant == 9 && style.palette_owner == 0xff);
    assert(rich4_map_property_render_style(1, 2, 4, 9, 3, 2, &style));
    assert(style.visible && !style.use_owner_marker &&
           style.variant == 3 && style.palette_owner == 3);
    assert(rich4_map_property_render_style(0, -1, 4, 0, 3, 2, &style));
    assert(!style.visible);
    memset(boundary_inputs, 0, sizeof(boundary_inputs));
    viewport.center_world_x = 100 * 32;
    viewport.center_world_y = 100 * 32;
    boundary_inputs[0].world_x = 86 * 32; boundary_inputs[0].world_y = 100 * 32;
    boundary_inputs[1].world_x = 114 * 32; boundary_inputs[1].world_y = 100 * 32;
    boundary_inputs[2].world_x = 100 * 32; boundary_inputs[2].world_y = 86 * 32;
    boundary_inputs[3].world_x = 100 * 32; boundary_inputs[3].world_y = 114 * 32;
    boundary_inputs[4].world_x = 85 * 32; boundary_inputs[4].world_y = 100 * 32;
    boundary_inputs[5].world_x = 100 * 32; boundary_inputs[5].world_y = 115 * 32;
    for (base = 0; base < 6; base++) boundary_inputs[base].drawable = 1;
    assert(rich4_map_collect_visible(boundary_inputs, 6, &viewport,
                                     boundary_visible, 6) == 4);
    viewport.center_world_x = 32;
    viewport.center_world_y = 64;
    assert(rich4_map_compare_sort_keys(
               rich4_map_visible_sort_key(0, 100),
               rich4_map_visible_sort_key(9, 100)) == 0);
    assert(rich4_map_compare_sort_keys(
               rich4_map_visible_sort_key_bias(0, 100, 12),
               rich4_map_visible_sort_key_bias(1, 100, 13)) < 0);
    memset(palette, 0, sizeof(palette));
    palette[1] = 0x7c00;
    palette[2] = 0x03e0;
    rich4_rgb565_clear(&framebuffer, 0x1111);
    assert(rich4_map_blit_indexed(&framebuffer, sprite, 2, 2, 1, 1,
                                  palette, 1, 1));
    assert(memcmp(pixels, expected, sizeof(expected)) == 0);
    assert(rich4_map_owner_palette_color(1) == 0x7c00);
    assert(rich4_map_owner_palette_color(2) == 0x03e0);
    assert(rich4_map_owner_palette_color(3) == 0x001f);
    assert(rich4_map_owner_palette_color(4) == 0x7fe0);
    assert(rich4_map_character_palette_color(0x00946126u) ==
           (uint16_t)(((0x94u >> 3) << 10) |
                      ((0x61u >> 3) << 5) |
                      (0x26u >> 3)));
    /* Original RGB565 readback values: Danny, Jin Beibei, Sarah and A-Tu. */
    assert(rich4_character_rgb565_color(0x002017feu) == 0x20bf);
    assert(rich4_character_rgb565_color(0x000ebdbdu) == 0x0df7);
    assert(rich4_character_rgb565_color(0x00ed9d9du) == 0xecf3);
    assert(rich4_character_rgb565_color(0x00c5b830u) == 0xc5c6);
    assert(rich4_rgb565_to_rgb888_truncated(0x20bfu) == 0x002014f8u);
    assert(rich4_rgb565_to_rgb888_truncated(0x0df7u) == 0x0008bcb8u);
    assert(rich4_rgb565_to_rgb888_truncated(0xecf3u) == 0x00e89c98u);
    assert(rich4_rgb565_to_rgb888_truncated(0xc5c6u) == 0x00c0b830u);
    assert(sizeof(GameMapDynamicObjectState) == 24);
    {
        GamePresentationState presentation;
        memset(&presentation, 0, sizeof(presentation));
        rich4_app_init_dynamic_object_pool(&presentation);
        memcpy(dynamic, presentation.dynamic_objects, sizeof(dynamic));
    }
    assert(dynamic[0].type == 1 && dynamic[13].type == 14);
    assert(dynamic[14].type == 15 && dynamic[15].type == 15);
    assert(dynamic[16].type == 16 && dynamic[25].type == 16);
    assert(dynamic[26].type == 17 && dynamic[35].type == 17);
    assert(dynamic[36].type == 18 && dynamic[45].type == 18);
    assert(rich4_map_dynamic_resource_chunk(1) == 396);
    assert(rich4_map_dynamic_resource_chunk(20) == 415);
    assert(rich4_map_dynamic_resource_chunk(0) == -1);
    memset(routes, 0, sizeof(routes));
    routes[1].x = 96; routes[1].y = 128;
    dynamic[0].node = 1; dynamic[0].node_direction = 3;
    assert(rich4_map_dynamic_build_input(&dynamic[0], 0, &viewport,
                                         NULL, 0, routes, 2, NULL,
                                         &dynamic_input));
    assert(dynamic_input.world_x == 96 && dynamic_input.world_y == 128);
    assert(dynamic_input.variant == 3 && dynamic_input.object_id == 0x8100);
    memset(&actor, 0, sizeof(actor));
    actor.world_x = 64; actor.world_y = 96; actor.visual_direction = 2;
    actor.moving = 1;
    /* Use the actual core -> presentation state after pickup: the tool
     * no longer occupies a road node, but remains attached to the actor. */
    {
        static GameState state;
        GamePresentationState presentation;
        memset(&state, 0, sizeof(state));
        memset(&presentation, 0, sizeof(presentation));
        rich4_app_init_dynamic_object_pool(&presentation);
        state.tool_placed_objects[20].kind = 18;
        state.tool_placed_objects[20].attached_player = 1;
        state.tool_placed_objects[20].countdown = 38;
        state.players[0].attached_timed_bomb_slot = 20;
        rich4_app_sync_tool_dynamic_objects(&presentation, &state);
        dynamic[36] = presentation.dynamic_objects[36];
    }
    assert(dynamic[36].node == 0 && dynamic[36].flight_active == 0 &&
           dynamic[36].attached_player == 1 && dynamic[36].countdown == 38);
    assert(rich4_map_dynamic_build_input(&dynamic[36], 36, &viewport,
                                         &actor, 1, routes, 2, NULL,
                                         &dynamic_input));
    assert(dynamic_input.variant == 6 && dynamic_input.screen_offset_x == -22 &&
           dynamic_input.screen_offset_y == 10);
    assert(dynamic_input.object_id == 0xe500);
    assert(dynamic_input.world_x == actor.world_x &&
           dynamic_input.world_y == actor.world_y);
    assert(rich4_map_collect_visible(&dynamic_input, 1, &viewport,
                                     visible, 3) == 1);
    assert(visible[0].object_id == 0xe500);
    actor.moving = 0;
    actor.world_x += 32;
    assert(rich4_map_dynamic_build_input(&dynamic[36], 36, &viewport,
                                         &actor, 1, routes, 2, NULL,
                                         &dynamic_input));
    assert(dynamic_input.world_x == actor.world_x);
    actor.attachments_hidden = 1;
    assert(!rich4_map_dynamic_build_input(&dynamic[36], 36, &viewport,
                                          &actor, 1, routes, 2, NULL,
                                          &dynamic_input));
    actor.attachments_hidden = 0;
    actor.has_dynamic_attachment = 1;
    assert(rich4_map_dynamic_build_input(&dynamic[36], 36, &viewport,
                                         &actor, 1, routes, 2, NULL,
                                         &dynamic_input));
    assert(dynamic_input.variant == 6 && dynamic_input.screen_offset_x == -44 &&
           dynamic_input.screen_offset_y == 18);
    dynamic[36].attached_player = 0;
    assert(!rich4_map_dynamic_build_input(&dynamic[36], 36, &viewport,
                                          &actor, 1, routes, 2, NULL,
                                          &dynamic_input));
    dynamic[36].node = 1;
    assert(rich4_map_dynamic_build_input(&dynamic[36], 36, &viewport,
                                         &actor, 1, routes, 2, NULL,
                                         &dynamic_input));
    assert(dynamic_input.object_id == 0xa500); /* no ground countdown */
    dynamic[36].flight_active = 1;
    assert(rich4_map_dynamic_build_input(&dynamic[36], 36, &viewport,
                                         &actor, 1, routes, 2, NULL,
                                         &dynamic_input));
    assert(dynamic_input.object_id == 0); /* no flying countdown */
    dynamic[16].node = 0; dynamic[16].flight_active = 2;
    dynamic[16].world_x = 32.0f; dynamic[16].world_y = 64.0f;
    dynamic[16].velocity_x = 1.5f; dynamic[16].velocity_y = -2.0f;
    assert(rich4_map_dynamic_build_input(&dynamic[16], 16, &viewport,
                                         NULL, 0, routes, 2, NULL,
                                         &dynamic_input));
    assert(dynamic_input.use_fixed_sort_low && dynamic_input.fixed_sort_low == 0x7ff0);
    assert(dynamic_input.object_id == 0);
    assert(dynamic[16].flight_active == 2 && dynamic[16].world_x == 32.0f &&
           dynamic[16].world_y == 64.0f);
    puts("map renderer: ok");
    return 0;
}
