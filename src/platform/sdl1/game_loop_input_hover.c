/* R10.1d: post-event virtual-cursor hover/selection synchronization moved
 * verbatim out of runtime_main.c. It updates only transient UI hover state
 * and the same selection sounds the original pass emitted. */

#include <SDL/SDL.h>

#include "platform/sdl1/game_loop_input_hover.h"
#include "app/tool_panel_flow.h"
#include "app/auction_flow.h"
#include "app/card_panel_flow.h"
#include "app/special_presentation_flow.h"
#include "platform/sdl1/game_loop_confirm_company_issue.h"
#include "app/confinement_flow.h"
#include "app/input_flow.h"
#include "app/lottery_flow.h"
#include "app/magic_house_flow.h"
#include "app/menu_flow.h"
#include "app/options_flow.h"
#include "app/route_flow.h"
#include "app/stock_interaction_flow.h"
#include "app/sale_interaction_flow.h"
#include "app/map_input_flow.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/render/scene_menu_render.h"
#include "platform/sdl1/render/scene_new_game_setup_render.h"
#include "platform/sdl1/render/scene_options_render.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_cursor.h"
#include "platform/sdl1/game_event_presentation.h"
#include "app/property_flow.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_options_input.h"

static int game_map_info_input_blocked(const GameInputHoverContext *context)
{
    const GameSession *session;
    if (context == NULL || (session = context->session) == NULL) return 1;
    return (context->small_map_panel != NULL && context->small_map_panel->active) ||
           (context->help_panel != NULL && context->help_panel->active) ||
           rich4_app_stock_interaction_is_active(session) ||
           rich4_app_sale_interaction_is_active(session) ||
           (context->ai_settings_panel != NULL && context->ai_settings_panel->active) ||
           session->presentation.speech.active ||
           session->presentation.player_info.active ||
           session->presentation.card_panel.phase != CARD_PANEL_CLOSED ||
           session->presentation.tool_panel.phase != TOOL_PANEL_CLOSED ||
           session->presentation.confinement_visit.active ||
           session->presentation.lottery.active ||
           session->presentation.point_minigame.active ||
           session->presentation.shop_visit.active ||
           session->presentation.facility_roulette.active ||
           session->presentation.god_acquisition.active ||
           session->presentation.company_issue.active ||
           session->presentation.magic_house.active ||
           session->presentation.auction.active ||
           session->presentation.special_event.active ||
           session->presentation.route_interaction.active ||
           session->presentation.tool_animation_count > 0 ||
           session->presentation.tool_placement_animation.active ||
           session->state.tool_robot.active;
}

static void game_update_map_info_press(GameInputHoverContext *context)
{
    GameSceneView scene;
    GameVisibleMapObject object;
    int object_id;

    if (context == NULL || context->session == NULL || context->cursor == NULL ||
        context->assets == NULL || context->settings == NULL ||
        game_map_info_input_blocked(context)) {
        if (context != NULL) rich4_app_input_clear_map_info(context->session);
        return;
    }
    if (!build_game_session_render_view(
            context->assets, context->settings, context->session, &scene) ||
        !scene.use_main_view) {
        rich4_app_input_clear_map_info(context->session);
        return;
    }
    if (!rich4_app_input_take_map_info_request(context->session)) return;
    object_id = game_map_object_at(
        context->assets, context->settings, &scene.game, scene.dynamic_objects,
        context->cursor->x, context->cursor->y, &object);
    if (object_id == 0) {
        rich4_app_input_clear_map_info(context->session);
        return;
    }
    rich4_app_input_set_map_info(context->session, object.object_id,
                                 context->cursor->x, context->cursor->y);
}

void game_update_input_hovers(GameInputHoverContext *context)
{
    GameSession *session = context->session;
    VirtualCursor *cursor = context->cursor;
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_special_event_choice_hover(session,
            game_fortune_choice_at(session,context->assets,cursor->x,cursor->y)))
        return;
    if (context->app_state == RICH4_SCENE_GAME &&
        session->presentation.property.insurance_active &&
        session->presentation.property.phase == GAME_PROPERTY_PRESENT_INSURANCE_TARGET) {
        int target = game_insurance_target_at(context->assets, &session->state,
            session->presentation.property.actor, cursor->x, cursor->y);
        if (rich4_property_insurance_hover(session, target) && target >= 0)
            rich4_audio_play_select(context->audio);
        return;
    }
    int edge_scroll_inset = context->sdl_config != NULL
        ? context->sdl_config->edge_scroll_inset
        : RICH4_SDL_EDGE_SCROLL_INSET_DEFAULT;
    if (!rich4_app_input_ui_active(
            context->app_state, context->settings, session,
            context->ai_settings_panel != NULL &&
                context->ai_settings_panel->active)) {
        *context->last_menu_hover = -1;
        *context->last_new_game_character_hover = -1;
        *context->last_top_button_hover = -1;
        rich4_app_input_set_top_button_hover(session, -1);
        rich4_app_route_set_hover(session, RICH4_CONFIRM_DIALOG_NONE);
        rich4_options_panel_set_hover(context->options_panel, -1);
        rich4_app_input_clear_map_info(session);
    } else if (context->app_state == RICH4_SCENE_MENU) {
        int hover = context->menu_assets->available
            ? menu_button_at(context->menu_assets, cursor) : -1;
        rich4_app_input_set_top_button_hover(session, -1);
        if (!context->menu_assets->available) {
            hover = rich4_menu_fallback_button_at(cursor->x, cursor->y);
        }
        if (hover != *context->last_menu_hover) {
            if (hover >= 0)
                rich4_audio_play_select(context->audio);
            *context->last_menu_hover = hover;
        }
        *context->last_new_game_character_hover = -1;
        rich4_app_input_clear_map_info(session);
    } else if (context->app_state == RICH4_SCENE_NEW_GAME) {
        int hover = rich4_new_game_character_at(cursor->x, cursor->y);
        if (hover != *context->last_new_game_character_hover) {
            if (hover >= 0 && !context->settings->character_used[hover])
                rich4_audio_play_select(context->audio);
            *context->last_new_game_character_hover = hover;
        }
        *context->last_menu_hover = -1;
        rich4_app_input_set_top_button_hover(session, -1);
        rich4_app_input_clear_map_info(session);
    } else if (context->app_state == RICH4_SCENE_GAME) {
        Rich4ConfirmDialogHit confirm_hover = RICH4_CONFIRM_DIALOG_NONE;
        int confirm_left, confirm_top, confirm_width, confirm_height;
        int top_button_hover;
        int map_input_blocked;
        uint32_t now = rich4_sdl1_ticks();
        rich4_tool_panel_update_edge_scroll(
            session, context->tool_panel, cursor->x, cursor->y,
            edge_scroll_inset, now);
        rich4_card_panel_update_edge_scroll(
            session, context->card_panel, cursor->x, cursor->y,
            edge_scroll_inset, now);
        map_input_blocked = game_map_info_input_blocked(context);
        top_button_hover = map_input_blocked ? -1 :
            rich4_app_top_panel_button_at(cursor->x, cursor->y);
        rich4_app_input_set_top_button_hover(session, top_button_hover);
        if (top_button_hover != *context->last_top_button_hover) {
            /* WM_MOUSEMOVE in 0x417E26 plays Effect[0] only when
             * floor(x/40) changes to another valid toolbar button. */
            if (top_button_hover >= 0)
                rich4_audio_play_select(context->audio);
            *context->last_top_button_hover = top_button_hover;
        }
        if (session->presentation.auction.active) {
            /* 0x43A2DD ignores WM_MOUSEMOVE.  Its odd chunks are the
             * left-button-down state, retained until WM_LBUTTONUP. */
        } else if (session->presentation.magic_house.active) {
            if (session->presentation.magic_house.condition == RICH4_MAGIC_HOUSE_SURRENDER) {
                int previous = session->presentation.magic_house.hover;
                int hover = rich4_magic_house_surrender_target_at(session, cursor->x, cursor->y);
                rich4_magic_house_set_hover(session, hover);
                if (hover >= 0 && hover != previous) rich4_audio_play_select(context->audio);
            } else if (session->presentation.magic_house.phase ==
                    RICH4_MAGIC_HOUSE_FACILITY_CHOICE) {
                int previous =
                    session->presentation.magic_house.facility_choice_hover;
                int hover = rich4_magic_house_facility_at(
                    cursor->x, cursor->y);
                rich4_magic_house_set_hover(session, -1);
                rich4_magic_house_set_facility_choice_hover(session, hover);
                if (hover >= 0 && hover != previous)
                    rich4_audio_play_select(context->audio);
            } else {
                int previous_hover = session->presentation.magic_house.hover;
                int mask = session->presentation.magic_house.phase == RICH4_MAGIC_HOUSE_SELECT
                    ? game_magic_house_mask_at(context->assets, cursor->x, cursor->y) : 0;
                rich4_magic_house_set_hover(session,
                    session->presentation.magic_house.phase == RICH4_MAGIC_HOUSE_SELECT &&
                    mask >= 1 && mask <= 12 ? mask - 1 : -1);
                if (session->presentation.magic_house.hover >= 0 &&
                    session->presentation.magic_house.hover != previous_hover)
                    rich4_audio_play_magic_hover(context->audio);
            }
        } else if (session->presentation.lottery.active) {
            rich4_lottery_set_hover(session,
                session->presentation.lottery.phase <= RICH4_LOTTERY_VISIT_SELECT
                    ? rich4_lottery_number_at(
                          session, cursor->x, cursor->y)
                    : -1);
        } else if (session->presentation.confinement_visit.active) {
            int hover = -1;
            if (session->presentation.confinement_visit.phase ==
                RICH4_CONFINEMENT_VISIT_SELECT) {
                int previous_hover =
                    session->presentation.confinement_visit.hover;
                hover = rich4_confinement_visit_slot_at(
                    session, cursor->x, cursor->y);
                /* 0x43D2D0 uses Effect[0] only when the cursor enters a
                 * different valid visitor slot.  Leaving a slot is silent. */
                if (hover >= 0 && hover != previous_hover)
                    rich4_audio_play_select(context->audio);
                rich4_confinement_set_hover(session, hover);
            } else if (session->presentation.confinement_visit.phase ==
                       RICH4_CONFINEMENT_VISIT_CONFIRM) {
                if (game_confirm_dialog_bounds(
                        context->assets, GAME_COMMAND_CONFIRM,
                        &confirm_left, &confirm_top,
                        &confirm_width, &confirm_height))
                    confirm_hover = rich4_sdl1_confirm_dialog_hit(
                        cursor->x, cursor->y, confirm_left, confirm_top,
                        confirm_width, confirm_height);
                if ((int)confirm_hover !=
                        session->presentation.confinement_visit.confirm_hover &&
                    confirm_hover != RICH4_CONFIRM_DIALOG_NONE)
                    rich4_audio_play_select(context->audio);
                rich4_confinement_set_confirm_hover(session, confirm_hover);
            }
        } else if (context->card_panel->phase == CARD_PANEL_FACILITY_CHOICE) {
            int hover = rich4_card_panel_facility_choice_at(
                cursor->x, cursor->y);
            if (hover != context->card_panel->hover && hover >= 0)
                rich4_audio_play_select(context->audio);
            rich4_card_panel_set_hover(context->card_panel, hover);
        } else if (context->card_panel->phase == CARD_PANEL_INVENTORY) {
            rich4_card_panel_set_hover(context->card_panel,
                rich4_card_panel_slot_at(cursor->x, cursor->y));
        } else if (context->card_panel->phase == CARD_PANEL_ROBBERY_TARGET) {
            int card_slot = rich4_card_panel_robbery_card_slot_at(
                cursor->x, cursor->y);
            int tool = rich4_card_panel_robbery_tool_at(
                session, context->card_panel->primary_target,
                cursor->x, cursor->y);
            rich4_card_panel_set_hover(context->card_panel,
                card_slot >= 0 ? card_slot :
                (tool >= 0 ? (0x8000 | tool) : -1));
        } else if (context->tool_panel->phase == TOOL_PANEL_REMOTE_DICE) {
            int face, hover = -1;
            for (face = 0; face < 6; face++)
                if (cursor->x >= 104 + face * 40 &&
                    cursor->x < 134 + face * 40 &&
                    cursor->y >= 314 && cursor->y < 343) hover = face;
            rich4_tool_panel_set_hover(context->tool_panel, hover);
        } else if (context->tool_panel->phase == TOOL_PANEL_WORKER_CHOICE) {
            int hover = (cursor->x >= 50 && cursor->x < 390 &&
                         cursor->y >= 286 && cursor->y < 353)
                ? (cursor->x - 50) / 68 : -1;
            if (hover != context->tool_panel->hover && hover >= 0)
                rich4_audio_play_select(context->audio);
            rich4_tool_panel_set_hover(context->tool_panel, hover);
        } else if (context->tool_panel->phase == TOOL_PANEL_INVENTORY) {
            rich4_tool_panel_set_hover(context->tool_panel,
                rich4_tool_inventory_slot_at(
                    session, context->tool_panel, cursor->x, cursor->y));
        }
        if (rich4_app_route_interaction_uses_confirm_dialog(session) &&
            game_confirm_dialog_bounds(
                context->assets,
                session->presentation.route_interaction.accept.type,
                &confirm_left, &confirm_top,
                &confirm_width, &confirm_height))
            confirm_hover = rich4_sdl1_confirm_dialog_hit(
                cursor->x, cursor->y, confirm_left, confirm_top,
                confirm_width, confirm_height);
        if (session->presentation.route_interaction.active &&
            rich4_app_route_interaction_is_facility_choice(session)) {
            int choice_count =
                rich4_app_route_interaction_is_mandatory_facility_build(session)
                ? 5
                : session->state.facility_level[
                    session->presentation.route_interaction.accept.subject.id];
            int hover = rich4_app_route_interaction_is_facility_research(
                session)
                ? rich4_app_route_facility_research_choice_at(
                    cursor->x, cursor->y)
                : rich4_app_route_facility_choice_at(
                    cursor->x, cursor->y);
            if (hover >= choice_count) hover = -1;
            if (hover != session->presentation.route_interaction.hover && hover >= 0)
                rich4_audio_play_select(context->audio);
            rich4_app_route_set_hover(session, hover);
        } else {
            if ((int)confirm_hover != session->presentation.route_interaction.hover &&
                confirm_hover != RICH4_CONFIRM_DIALOG_NONE)
                rich4_audio_play_select(context->audio);
            rich4_app_route_set_hover(session, confirm_hover);
        }
        *context->last_menu_hover = -1;
        *context->last_new_game_character_hover = -1;
        game_update_map_info_press(context);
    } else if (context->app_state == RICH4_SCENE_OPTIONS) {
        game_update_options_hover(
            context->options_panel, context->assets,
            cursor->x, cursor->y, context->audio);
        *context->last_menu_hover = -1;
        *context->last_new_game_character_hover = -1;
        rich4_app_input_set_top_button_hover(session, -1);
        rich4_app_input_clear_map_info(session);
    } else {
        *context->last_menu_hover = -1;
        *context->last_new_game_character_hover = -1;
        rich4_app_input_set_top_button_hover(session, -1);
        rich4_app_input_clear_map_info(session);
    }
}
