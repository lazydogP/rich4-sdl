#include "platform/sdl1/game_lazy_assets.h"
#include <string.h>

#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "app/route_flow.h"
#include "app/map_input_flow.h"
#include "core/bank.h"
#include "core/facility.h"
#include "core/game_constants.h"
#include "core/game_state.h"
#include "core/property.h"
#include "core/route_event_queue.h"
#include "core/stock.h"
#include "data/map_data.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/game_view_model_input.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"

/* R10.1d: extracted verbatim from src/platform/sdl1/runtime_main.c. This
 * adapter builds the read-only scene snapshot consumed by game renderers and
 * modal panels; it does not mutate session state or own preview assets. */

int build_game_session_render_view_for_state(
                                           const GamePreviewAssets *assets,
                                           const NewGameSettings *settings,
                                           const GameSession *session,
                                           const GameState *state,
                                           GameSceneView *out_view)
{
    int background_index;
    const MapData *map = NULL;
    GameViewModelInput view_input;
    memset(out_view, 0, sizeof(*out_view));
    out_view->bank_calendar_map_index = -1;
    out_view->bank_calendar_event_index = -1;
    if (session == NULL || state == NULL || !state->active) return 0;
    background_index = settings != NULL
        ? settings->stage * GAME_MAPS_PER_STAGE + settings->map : -1;
    if (assets != NULL && background_index >= 0 &&
        background_index < GAME_BACKGROUND_COUNT) map = &assets->map.maps[background_index];
    rich4_sdl1_game_view_model_input_build(&session->flow,
                                           &session->presentation,
                                           &view_input);
    view_input.active_info_tab = rich4_app_current_info_tab(session);
    if (!rich4_game_view_model_build(state, &view_input,
                                     map, &out_view->game)) return 0;
    if (state->tool_robot.active && map != NULL &&
        !(session->presentation.speech.active &&
          !session->presentation.tool_robot_motion.initialized)) {
        int node = state->tool_robot.node;
        int next_node = state->tool_robot.next_node;
        int stopping = session->presentation.tool_robot_motion.end_wait != 0;
        if (node > 0 && (size_t)node < map->route_count &&
            (stopping || (next_node > 0 && (size_t)next_node < map->route_count))) {
            int direction = session->presentation.tool_robot_motion.direction;
            if (!session->presentation.tool_robot_motion.initialized)
                direction = session->presentation.actors[state->tool_robot.owner].direction;
            out_view->game.tool_robot.visible = 1;
            out_view->game.tool_robot.resource_part = stopping ? 0 : 1;
            out_view->game.tool_robot.world_x =
                session->presentation.tool_robot_motion.initialized
                ? session->presentation.tool_robot_motion.world_x
                : map->routes[node].x;
            out_view->game.tool_robot.world_y =
                session->presentation.tool_robot_motion.initialized
                ? session->presentation.tool_robot_motion.world_y
                : map->routes[node].y;
            out_view->game.tool_robot.visual_direction =
                (8 - out_view->game.viewport.direction + direction) & 7;
            out_view->game.tool_robot.animation_frame =
                session->presentation.tool_robot_motion.animation_frame;
        }
    }
    memcpy(out_view->dynamic_objects, session->presentation.dynamic_objects,
           sizeof(out_view->dynamic_objects));
    /* The on-foot FLC rebuilds the original arrival background on its
     * third frame. Before that refresh the old dog is still in the image. */
    if (view_input.tool_effect_resource == 532 &&
        view_input.tool_effect_frame < 2 && view_input.tool_effect_node > 0) {
        out_view->dynamic_objects[10].node = (uint16_t)view_input.tool_effect_node;
        out_view->dynamic_objects[11].node = 0;
    }
    out_view->god_detachment.active =
        session->presentation.god_detachment.active;
    out_view->god_detachment.god_type =
        session->presentation.god_detachment.god_type;
    out_view->god_detachment.frame =
        session->presentation.god_detachment.frame;
    out_view->god_detachment.direction =
        session->presentation.god_detachment.direction;
    out_view->god_detachment.world_x =
        session->presentation.god_detachment.world_x;
    out_view->god_detachment.world_y =
        session->presentation.god_detachment.world_y;
    out_view->ending_active = session->presentation.ending.active;
    out_view->ending_continuation_active =
        session->presentation.ending.continuation_active;
    out_view->ending_winner_player =
        session->presentation.ending.winner_player;
    out_view->ending_winner_character =
        session->presentation.ending.winner_character;
    out_view->ending_reason = session->presentation.ending.reason;
    out_view->ending_assets = session->presentation.ending.assets;
    out_view->ending_jump_active =
        session->presentation.ending.jump_transition_active;
    out_view->ending_countdown = 10;
    if (session->presentation.ending.continuation_active &&
        session->presentation.ending.continuation_started != 0) {
        uint32_t elapsed = rich4_sdl1_ticks() -
            session->presentation.ending.continuation_started;
        if (elapsed >= 10000u)
            out_view->ending_countdown = 0;
        else
            out_view->ending_countdown = 10 - (int)(elapsed / 1000u);
    }
    out_view->ending_confirmation_active =
        session->presentation.ending.continuation_confirmation_active;
    out_view->ending_confirmation_frame = 0;
    if (out_view->ending_confirmation_active && assets != NULL &&
        game_asset_ready(assets, &assets->ui.has_ending_success_animation) &&
        session->presentation.ending.continuation_confirmation_started != 0) {
        int frame_count = game_asset_graph_frames(assets, &assets->ui.ending_success_data, assets->ui.ending_success_size);
        if (frame_count > 0) {
            out_view->ending_confirmation_frame = session->presentation.ending.continuation_confirmation_frame;
            if (out_view->ending_confirmation_frame >= frame_count)
                out_view->ending_confirmation_frame = frame_count - 1;
        }
    }
    out_view->ending_background_index = background_index;
    out_view->use_main_view = assets != NULL && background_index >= 0 &&
        background_index < GAME_BACKGROUND_COUNT &&
        assets->map.has_map_ground[background_index] && !session->presentation.intro.active;
    out_view->map_info_active = session->presentation.map_info.active;
    out_view->map_info_object_id = session->presentation.map_info.object_id;
    out_view->map_info_cursor_x = session->presentation.map_info.cursor_x;
    out_view->map_info_cursor_y = session->presentation.map_info.cursor_y;
    out_view->intro_player = session->presentation.intro.active
        ? wrap_index(session->presentation.intro.player, state->player_count) : -1;
    out_view->intro_frame = session->presentation.intro.frame;
    out_view->intro_total_frames = session->presentation.intro.active
        ? game_intro_total_frames(assets, session) : 0;
    out_view->landing_player = session->flow.turn_flow_active &&
        session->flow.turn_state == 3 ? session->flow.landing_pending_player - 1 : -1;
    out_view->landing_frame = session->flow.turn_state_frame;
    out_view->show_steps = out_view->game.show_steps;
    if (out_view->landing_player >= 0 &&
        out_view->landing_player < state->player_count)
        out_view->landing_frames = game_first_landing_frame_count(
            assets, &state->players[out_view->landing_player],
            out_view->landing_player);
    if (session->presentation.route_interaction.active) {
        const GameCommand *command = &session->presentation.route_interaction.accept;
        int id = command->subject.id;
        out_view->route_interaction_active = 1;
        out_view->route_interaction_hover = session->presentation.route_interaction.hover;
        out_view->route_interaction_pressed = session->presentation.route_interaction.pressed;
        out_view->route_interaction_type = command->type;
        out_view->route_interaction_facility_build =
            rich4_app_route_interaction_is_mandatory_facility_build(session);
        out_view->route_interaction_facility_research =
            rich4_app_route_interaction_is_facility_research(session);
        out_view->route_interaction_facility_choice_count =
            out_view->route_interaction_facility_build ? 5 :
            out_view->route_interaction_facility_research
                ? state->facility_level[id] : 0;
        out_view->bank_interaction_mode = session->presentation.bank_interaction.mode;
        out_view->bank_interaction_amount = session->presentation.bank_interaction.amount;
        out_view->bank_interaction_pressed_control =
            session->presentation.bank_interaction.pressed_control;
        out_view->bank_interaction_special_page =
            session->presentation.bank_interaction.special_page;
        out_view->bank_interaction_amount_entry =
            session->presentation.bank_interaction.amount_entry;
        out_view->bank_interaction_card_x =
            session->presentation.bank_interaction.card_x;
        out_view->bank_interaction_face_chunk =
            session->presentation.bank_interaction.face_chunk;
        out_view->bank_interaction_mouth_chunk =
            session->presentation.bank_interaction.mouth_chunk;
        out_view->bank_interaction_speech =
            session->presentation.bank_interaction.speech;
        out_view->bank_loan_disabled =
            command->type == GAME_COMMAND_BANK_APPLY_LOAN &&
            state->players[command->player].days_bank_no_loans != 0;
        out_view->bank_character = state->players[command->player].character;
        out_view->bank_cash = state->players[command->player].cash;
        out_view->bank_savings = state->players[command->player].bank;
        out_view->bank_loan = state->players[command->player].loan;
        out_view->bank_loan_available =
            rich4_bank_loan_available(state, command->player);
        out_view->bank_special_finance =
            state->players[command->player].special_finance;
        out_view->bank_special_available =
            rich4_bank_special_available(state, command->player);
        {
            const RouteEvent *event = rich4_route_event_executor_current(
                &session->flow.route_events);
            out_view->bank_special_enabled = event != NULL &&
                event->snapshot.property_owner == command->player;
        }
        out_view->bank_loan_days_remaining =
            rich4_bank_loan_days_remaining(state, command->player);
        out_view->bank_elapsed_days = state->elapsed_days;
        rich4_stock_calendar_date(
            state, &out_view->bank_calendar_year,
            &out_view->bank_calendar_month,
            &out_view->bank_calendar_day,
            &out_view->bank_calendar_weekday);
        out_view->bank_calendar_map_index = state->stage * GAME_MAPS_PER_STAGE +
                                            state->map;
        out_view->bank_calendar_event_index =
            rich4_stock_calendar_event_index(state);
        if ((command->type != GAME_COMMAND_PURCHASE_PROPERTY &&
             command->type != GAME_COMMAND_UPGRADE_PROPERTY) ||
            (command->subject.kind != GAME_DOMAIN_LAND &&
             command->subject.kind != GAME_DOMAIN_FACILITY)) {
            return 1;
        }
        if (command->subject.kind == GAME_DOMAIN_LAND && id >= 0 &&
            id < state->land_count) {
            const MapLandRecord *record = map != NULL &&
                (size_t)(id + 1) < map->land_count ? &map->lands[id + 1] : NULL;
            if (command->type == GAME_COMMAND_PURCHASE_PROPERTY)
                rich4_land_purchase_price(state, id,
                                          &out_view->route_interaction_amount);
            else
                rich4_land_upgrade_price(state, id,
                                         &out_view->route_interaction_amount);
            if (record != NULL)
                memcpy(out_view->route_interaction_name, record->raw + 4, 20);
        } else if (command->subject.kind == GAME_DOMAIN_FACILITY && id >= 0 &&
                   id < state->facility_count) {
            const MapFacilityRecord *record = map != NULL &&
                (size_t)(id + 1) < map->facility_count ?
                &map->facilities[id + 1] : NULL;
            if (command->type == GAME_COMMAND_PURCHASE_PROPERTY)
                rich4_facility_purchase_price(state, id,
                                              &out_view->route_interaction_amount);
            else
                rich4_facility_upgrade_price(state, id,
                                             &out_view->route_interaction_amount);
            if (record != NULL)
                memcpy(out_view->route_interaction_name, record->raw + 4, 20);
        }
        out_view->route_interaction_name[20] = '\0';
    }
    out_view->property_presentation_phase =
        session->presentation.property.phase;
    out_view->property_presentation_amount =
        session->presentation.property.god_type != 0 &&
        session->presentation.property.phase == GAME_PROPERTY_PRESENT_AMOUNT ?
        session->presentation.property.god_original_amount : session->presentation.property.amount;
    out_view->property_god_type = session->presentation.property.god_type;
    out_view->property_fee_label = 0;
    out_view->property_company_type = -1;
    out_view->property_facility_type = -1;
    out_view->property_outcome = 0;
    if (session->presentation.property.property_kind == GAME_DOMAIN_FACILITY &&
        session->presentation.property.land >= 0 && session->presentation.property.land < state->facility_count) {
        int type = state->facility_type[session->presentation.property.land];
        out_view->property_facility_type = type;
        out_view->property_outcome = session->flow.route_events.state.presentation_mode >= 10
            ? session->flow.route_events.state.presentation_mode - 10 : 0;
        out_view->property_fee_label = type == 1 ? 1 : type == 2 ? 12 : 2;
    } else if (session->presentation.property.property_kind == GAME_DOMAIN_COMPANY &&
               session->presentation.property.land >= 0 && session->presentation.property.land < state->company_count) {
        static const int labels[13] = {2,6,0,8,7,3,2,0,0,0,0,9,0};
        int type = state->company_type[session->presentation.property.land];
        out_view->property_company_type = type;
        if (type >= 0 && type < 13) out_view->property_fee_label = labels[type];
    }
    out_view->property_collection_block_reason =
        session->presentation.property.collection_block_reason;
    out_view->property_owner_amount =
        session->presentation.property.owner_amount;
    out_view->property_related_amount =
        session->presentation.property.related_amount;
    if (session->presentation.property.owner >= 0 &&
        session->presentation.property.owner < state->player_count)
        out_view->property_owner_character = state->players[
            session->presentation.property.owner].character;
    else
        out_view->property_owner_character = -1;
    if (session->presentation.property.related_owner >= 0 &&
        session->presentation.property.related_owner < state->player_count)
        out_view->property_related_owner_character = state->players[
            session->presentation.property.related_owner].character;
    else
        out_view->property_related_owner_character = -1;
    if (map != NULL && session->presentation.property.land >= 0) {
        if (session->presentation.property.property_kind == GAME_DOMAIN_LAND &&
            (size_t)(session->presentation.property.land + 1) < map->land_count)
            memcpy(out_view->property_name, map->lands[
                session->presentation.property.land + 1].raw + 4, 20);
        else if (session->presentation.property.property_kind ==
                     GAME_DOMAIN_FACILITY &&
                 (size_t)(session->presentation.property.land + 1) <
                     map->facility_count)
            memcpy(out_view->property_name, map->facilities[
                session->presentation.property.land + 1].raw + 4, 20);
        else if (session->presentation.property.property_kind == GAME_DOMAIN_COMPANY &&
                 (size_t)(session->presentation.property.land + 1) < map->auxiliary_count)
            memcpy(out_view->property_name, map->auxiliary[
                session->presentation.property.land + 1].raw + 4, 20);
    }
    out_view->property_name[20] = '\0';
    out_view->ai_tool_notice =
        session->presentation.ai_tool.phase == GAME_AI_TOOL_NOTICE;
    out_view->ai_tool_id = session->presentation.ai_tool.command.subject.id;
    out_view->speech_active = session->presentation.speech.active;
    out_view->speech_kind = session->presentation.speech.kind;
    out_view->speech_type = session->presentation.speech.type;
    out_view->speech = session->presentation.speech.current;
    if (session->presentation.speech.player >= 0 &&
        session->presentation.speech.player < state->player_count)
        out_view->speech_character = state->players[
            session->presentation.speech.player].character;
    else
        out_view->speech_character = -1;
    out_view->property_max_level_active =
        session->presentation.property.max_level_active;
    out_view->property_max_level_frame =
        session->presentation.property.max_level_frame;
    return 1;
}

int build_game_session_render_view(const GamePreviewAssets *assets,
                                   const NewGameSettings *settings,
                                   const GameSession *session,
                                   GameSceneView *out_view)
{
    return build_game_session_render_view_for_state(
        assets, settings, session,
        session != NULL ? &session->state : NULL, out_view);
}
