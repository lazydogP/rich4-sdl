#include "platform/sdl1/game_view_model_input.h"

#include <string.h>

void rich4_sdl1_game_view_model_input_build(const GameFlowState *flow,
                                            const GamePresentationState *presentation,
                                            GameViewModelInput *out_input)
{
    int i;
    if (out_input == NULL) return;
    memset(out_input, 0, sizeof(*out_input));
    if (flow == NULL) return;
    out_input->route_count = flow->route_count;
    if (presentation != NULL) {
        /* 0x40B166 updates +26 without painting. 0x440CAC overlays the
         * existing map; only 0x40F506 refreshes it after the notice/sound. */
        if (presentation->property.god_type == 9 &&
            presentation->property.god_choice == -2 &&
            presentation->property.phase == GAME_PROPERTY_PRESENT_GOD_BUILD_TEXT)
        {
            out_input->angel_previous_level_target.kind = presentation->property.property_kind;
            out_input->angel_previous_level_target.id = presentation->property.land;
        }
        if (presentation->property.god_type == 10 &&
            presentation->property.phase == GAME_PROPERTY_PRESENT_GOD_BUILD_FINISH) {
            /* Before the queued effect starts, and during its first two
             * frames, 0x30001 still composites over the original building. */
            int hold = !presentation->property.god_build_animation_started;
            int j;
            for (j = 0; j < presentation->tool_animation_count; j++) {
                const GameToolAnimationRequest *request = &presentation->tool_animation[
                    (presentation->tool_animation_head + j) % GAME_EVENT_QUEUE_CAPACITY];
                if (request->resource == 526 &&
                    request->target.kind == presentation->property.property_kind &&
                    request->target.id == presentation->property.land) {
                    hold = !request->animation_done && (j != 0 || request->frame < 2);
                    break;
                }
            }
            if (hold) {
                out_input->demon_previous_building_target.kind = presentation->property.property_kind;
                out_input->demon_previous_building_target.id = presentation->property.land;
                out_input->demon_previous_building_type = presentation->property.god_previous_building_type;
                out_input->demon_previous_building_level = presentation->property.god_previous_building_level;
            }
        }
        out_input->tool_robot_started = presentation->tool_robot_motion.initialized;
        out_input->tool_robot_arrived = presentation->tool_robot_motion.arrived;
        if (presentation->tool_animation_count > 0) {
            const GameToolAnimationRequest *request =
                &presentation->tool_animation[presentation->tool_animation_head];
            if (request->active && request->after_sync_player >= 0 &&
                (request->resource == 525 || request->resource == 532 ||
                   request->resource == 524 || request->resource == 538)) {
                out_input->tool_effect_actor = request->after_sync_player + 1;
                out_input->tool_effect_resource = request->resource;
                out_input->tool_effect_frame = request->admission_stage ? -1 :
                    request->animation_done ? 1000 : request->frame;
                out_input->tool_effect_explosion_traffic_method =
                    request->explosion_traffic_method;
                out_input->tool_effect_origin_play_state = request->origin_play_state;
                out_input->tool_effect_origin_traffic_method = request->origin_traffic_method;
                if (request->target.kind == GAME_DOMAIN_ROUTE_NODE)
                    out_input->tool_effect_node = request->target.id;
                out_input->tool_effect_has_world_position = request->has_world_position;
                out_input->tool_effect_world_x = request->world_x;
                out_input->tool_effect_world_y = request->world_y;
            }
            /* 0x44735C flags 0x2C0001; 0x451183 increments the decoded
             * frame count before the 0x4512BF scene refresh at count 44. */
            if (request->active && request->resource == 553 &&
                !request->animation_done && request->frame < 43)
                out_input->worker_previous_level_target = request->target;
        }
        out_input->engineering_attack_active =
            presentation->engineering_attack.active;
        out_input->engineering_attack_player =
            presentation->engineering_attack.player;
        out_input->engineering_attack_frame =
            presentation->engineering_attack.actor_frame;
        out_input->map_direction = presentation->map_direction;
        out_input->has_view_override = presentation->has_view_override;
        out_input->view_world_x = presentation->view_world_x;
        out_input->view_world_y = presentation->view_world_y;
        /* 0x40E384 centers the camera on the departing god owner. */
        if (presentation->god_detachment.active) {
            out_input->has_view_override = 1;
            out_input->view_world_x = presentation->god_detachment.world_x;
            out_input->view_world_y = presentation->god_detachment.world_y;
        }
        out_input->active_info_tab = presentation->active_info_tab;
        out_input->minimap_rotate_pressed = presentation->minimap_rotate_pressed;
        out_input->top_button_hover = presentation->top_button_hover;
    }
    for (i = 0; i < GAME_MAX_PLAYERS; i++) {
        if (presentation == NULL) continue;
        out_input->actors[i].direction = presentation->actors[i].direction;
        out_input->actors[i].walk_frame = presentation->actors[i].walk_frame;
        out_input->actors[i].visual_x = presentation->actors[i].visual_x;
        out_input->actors[i].visual_y = presentation->actors[i].visual_y;
        out_input->actors[i].move_active = presentation->actors[i].move_active;
        out_input->actors[i].intro_visible = presentation->actors[i].intro_visible;
        out_input->actors[i].first_turn_landing_pending =
            presentation->actors[i].first_turn_landing_pending;
        out_input->actors[i].dirty_flags = presentation->actors[i].dirty_flags;
    }
    if (presentation != NULL) {
        out_input->special_actor.active = presentation->special_actor.active;
        out_input->special_actor.slot = presentation->special_actor.slot;
        out_input->special_actor.visual_x = presentation->special_actor.visual_x;
        out_input->special_actor.visual_y = presentation->special_actor.visual_y;
        out_input->special_actor.direction = presentation->special_actor.direction;
        out_input->special_actor.animation_frame = presentation->special_actor.animation_frame;
        out_input->special_actor.resource_part = presentation->special_actor.resource_part;
        out_input->special_actor.steps_remaining = presentation->special_actor.steps_remaining;
    }
    out_input->turn_flow_active = flow->turn_flow_active;
    out_input->turn_state = flow->turn_state;
    out_input->turn_state_frame = flow->turn_state_frame;
    if (presentation != NULL) {
        out_input->dice_status_visible = presentation->dice.status_visible;
        out_input->dice_status_x = presentation->dice.status_x;
        out_input->dice_status_y = presentation->dice.status_y;
        out_input->dice_status_variant = presentation->dice.status_variant;
        out_input->dice_popup_active = presentation->dice.popup_active;
        out_input->dice_count = presentation->dice.count;
        memcpy(out_input->dice_faces, presentation->dice.faces,
               sizeof(out_input->dice_faces));
        out_input->dice_anim_frame = presentation->dice.animation_frame;
        out_input->dice_result_visible = presentation->dice.result_visible;
    }
    out_input->landing_pending_player = flow->landing_pending_player;
    out_input->steps_remaining = flow->steps_remaining;
    if (presentation != NULL) {
        out_input->property_presentation_group = presentation->property.group;
        out_input->property_presentation_owner = presentation->property.owner;
        out_input->property_presentation_land = presentation->property.land;
        out_input->property_flash_adjust = presentation->property.flash_adjust;
        out_input->property_flash_active =
            presentation->property.phase == GAME_PROPERTY_PRESENT_FLASH;
    }
}
