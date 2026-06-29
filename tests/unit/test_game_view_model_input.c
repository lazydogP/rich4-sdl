#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "platform/sdl1/game_view_model_input.h"

int main(void)
{
    GameFlowState flow;
    GamePresentationState presentation;
    GameViewModelInput input;

    memset(&flow, 0, sizeof(flow));
    memset(&presentation, 0, sizeof(presentation));
    flow.route_count = 77;
    presentation.map_direction = 3;
    presentation.has_view_override = 1;
    presentation.view_world_x = 352;
    presentation.view_world_y = 640;
    presentation.actors[0].visual_x = 123;
    presentation.actors[0].move_active = 1;
    presentation.dice.faces[0] = 4;
    presentation.special_actor.active = 1;
    presentation.special_actor.slot = 2;
    presentation.special_actor.visual_x = 456;
    presentation.special_actor.visual_y = 789;
    presentation.special_actor.direction = 5;
    presentation.special_actor.animation_frame = 11;
    presentation.special_actor.resource_part = 3;
    presentation.special_actor.steps_remaining = 5;
    presentation.property.land = 12;
    presentation.property.flash_adjust = -8;
    rich4_sdl1_game_view_model_input_build(&flow, &presentation, &input);
    assert(input.route_count == 77 && input.map_direction == 3);
    assert(input.has_view_override && input.view_world_x == 352);
    assert(input.actors[0].visual_x == 123 && input.actors[0].move_active);
    assert(input.dice_faces[0] == 4);
    assert(input.property_presentation_land == 12 && input.property_flash_adjust == -8);
    assert(input.special_actor.active && input.special_actor.slot == 2 &&
           input.special_actor.visual_x == 456 &&
           input.special_actor.visual_y == 789 &&
           input.special_actor.direction == 5 &&
           input.special_actor.animation_frame == 11 &&
           input.special_actor.resource_part == 3 &&
           input.special_actor.steps_remaining == 5);

    presentation.view_world_x = 999;
    presentation.actors[0].visual_x = 999;
    assert(input.view_world_x == 352 && input.actors[0].visual_x == 123);
    puts("SDL1 view-model input adapter: ok");
    return 0;
}
