#include "platform/sdl1/game_lazy_assets.h"
/* R10.1d: top-level preview composition adapter moved out of the renderer
 * package. The adapter owns ordering only; state transitions remain in the
 * runtime loop and GameSession update modules. */

#include <SDL/SDL.h>

#include "app/new_game_setup_flow.h"
#include "app/dice_flow.h"
#include "app/bank_interaction_flow.h"
#include "app/special_presentation_flow.h"
#include "platform/sdl1/game_confinement_render_view.h"
#include "platform/sdl1/game_overlay_view.h"
#include "platform/sdl1/game_point_minigame_view.h"
#include "platform/sdl1/game_preview_frame.h"
#include "platform/sdl1/game_monthly_finance.h"
#include "platform/sdl1/game_view_model_input.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_confinement_event_render.h"
#include "platform/sdl1/render/game_dice_render.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/render/game_hud_render.h"
#include "platform/sdl1/render/game_bank_render.h"
#include "platform/sdl1/render/game_intro_render.h"
#include "platform/sdl1/render/game_map_render.h"
#include "platform/sdl1/render/game_minimap_render.h"
#include "platform/sdl1/render/game_overlay_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_point_minigame_render.h"
#include "platform/sdl1/render/game_route_render.h"
#include "platform/sdl1/render/game_scene_view.h"
#include "platform/sdl1/render/game_session_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/runtime_trace.h"
#include "view/view_constants.h"

void draw_game_preview_adapter(SDL_Surface *screen,
                               const GamePreviewAssets *assets,
                               const NewGameSettings *settings,
                               GameSession *session,
                               GameRenderCache *render_cache,
                               int view_mode, int calendar_mode)
{
    int background_index =
        rich4_setup_background_resource(settings->stage, settings->map);
    if (session != NULL && session->state.active) {
        unsigned int part_started = runtime_profile_now();
        GameSceneView session_view;
        const GameViewModel *view;
        const GameState *render_state =
            rich4_special_event_render_state(session);
        GameIntroRenderView intro_view;
        GamePointMinigameRenderView point_minigame_view;
        GameConfinementRenderView confinement_view;
        unsigned int detail_started;
        Rich4GameEventPresentationSnapshot event_snapshot =
            rich4_app_build_game_event_presentation_snapshot(
                &session->state, &session->presentation);
        int defer_main_map = session->presentation.intro.active;
        if (!build_game_session_render_view_for_state(
                assets, settings, session, render_state, &session_view)) return;
        session_view.view_mode = clamp_int(view_mode, 0, 2);
        view = &session_view.game;
        build_game_intro_render_view(assets, session, &intro_view);
        build_point_minigame_render_view(session, &point_minigame_view);
        build_confinement_render_view(session, &confinement_view);
        runtime_profile_render_part(0, part_started);
        part_started = runtime_profile_now();
        if (intro_view.active && draw_game_intro_fullscreen(screen, assets, &intro_view)) {
            runtime_profile_render_part(1, part_started);
            return;
        }
        if (session_view.route_interaction_active &&
            session_view.route_interaction_type ==
                GAME_COMMAND_BANK_APPLY_LOAN) {
            /* sub_436668 opens Panel23 in its own 640x480 modal surface. */
            SDL_LockSurface(screen);
            fill_rect(screen, 0, 0, SCREEN_W, SCREEN_H, 0x0000);
            SDL_UnlockSurface(screen);
            draw_game_bank_interaction(screen, assets, &session_view);
            return;
        }
        trace_game_start("draw preview: clear begin");
        SDL_LockSurface(screen);
        fill_rect(screen, 0, 0, SCREEN_W, SCREEN_H, 0x0000);
        fill_rect(screen, 0, 40, 440, 440, 0x0842);
        SDL_UnlockSurface(screen);
        trace_game_start("draw preview: clear end");
        trace_game_start("draw preview: map begin");
        part_started = runtime_profile_now();
        if (defer_main_map || !draw_game_main_map_view(screen, assets, settings, view)) {
            SDL_LockSurface(screen);
            fill_rect(screen, 20, 60, 400, 400, 0x39e7);
            stroke_rect(screen, 20, 60, 400, 400, 0x7bef);
            SDL_UnlockSurface(screen);
        }
        trace_game_start("draw preview: map end");
        runtime_profile_render_part(2, part_started);
        part_started = runtime_profile_now();
        trace_game_start("draw preview: session begin");
        if (rich4_dice_consume_cache_reset(session)) {
            reset_game_dice_cache(render_cache);
        }
        draw_game_session(screen, assets, settings, &session_view, render_cache);
        trace_game_start("draw preview: session end");
        runtime_profile_render_part(3, part_started);
        part_started = runtime_profile_now();
        if (session_view.property_max_level_active &&
            game_asset_ready(assets, &assets->tool.has_property_max_level_graph)) {
            draw_game_graph_frame_data(
                screen, assets->tool.property_max_level_data,
                assets->tool.property_max_level_size,
                session_view.property_max_level_frame, 0, 40);
        }
        trace_game_start("draw preview: layout begin");
        draw_game_layout(screen, assets, view, session_view.view_mode);
        detail_started = runtime_profile_now();
        if (session_view.view_mode != 1)
            draw_game_calendar(screen, assets, view, calendar_mode != 0);
        runtime_profile_detail(PROFILE_UI_CALENDAR, detail_started);
        detail_started = runtime_profile_now();
        draw_game_dice_status_overlay(screen, assets, view);
        runtime_profile_detail(PROFILE_UI_STATUS, detail_started);
        detail_started = runtime_profile_now();
        draw_special_reward_presentation(screen, assets, &event_snapshot);
        draw_bank_rejection_presentation(screen, assets, &event_snapshot);
        draw_bankruptcy_presentation(screen, assets, &event_snapshot);
        draw_bank_settlement_presentation(screen, assets, &event_snapshot);
        draw_periodic_finance(screen, assets, &event_snapshot);
        draw_bank_loan_due_presentation(screen, assets, &event_snapshot);
        draw_confinement_status_presentation(screen, assets, &confinement_view);
        draw_special_actor_action_presentation(screen, assets, &event_snapshot);
        draw_game_property_presentation(screen, assets, &session_view);
        draw_special_event_presentation(screen, assets, &event_snapshot);
        draw_confinement_visit_panel(screen, assets, &confinement_view);
        draw_lottery_visit_panel(screen, assets, &event_snapshot);
        draw_point_minigame(screen, assets, &point_minigame_view);
        draw_shop_visit(screen, assets, &event_snapshot);
        draw_facility_roulette(screen, assets, &event_snapshot);
        draw_company_issue(screen, assets, &event_snapshot);
        draw_insurance_charge(screen, assets, &event_snapshot);
        draw_magic_house(screen, assets, &event_snapshot);
        draw_insurance_notice(screen, assets, &event_snapshot);
        draw_auction_presentation(screen, assets, &event_snapshot);
        draw_god_presentation(screen, assets, &event_snapshot);
        draw_special_place_notice(screen, assets, &event_snapshot);
        if (session_view.ending_confirmation_active)
            draw_game_ending_confirmation_presentation(
                screen, assets, &session_view);
        else if (session_view.ending_continuation_active)
            draw_game_continuation_presentation(
                screen, assets, &session_view);
        else if (session_view.ending_jump_active)
            draw_game_ending_presentation(screen, assets, &session_view);
        if (session_view.ending_active)
            draw_game_speech_overlay(screen, assets, &session_view);
        runtime_profile_detail(PROFILE_UI_EVENTS, detail_started);
        detail_started = runtime_profile_now();
        game_draw_save_load_panel(screen, assets, session);
        runtime_profile_detail(PROFILE_UI_SAVE_PANEL, detail_started);
        trace_game_start("draw preview: layout end");
        runtime_profile_render_part(4, part_started);
        return;
    }

    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        assets == NULL || !assets->intro.has_jump_background[background_index] ||
        !draw_raw_rgb565_fullscreen(screen,
                                    assets->intro.jump_data[background_index],
                                    assets->intro.jump_size[background_index])) {
        fill_background(screen);
        SDL_LockSurface(screen);
        fill_rect(screen, 72, 64, 496, 326, 0x39e7);
        stroke_rect(screen, 72, 64, 496, 326, 0xffff);
        fill_rect(screen, 120, 116, 400, 196, 0x18c3);
        stroke_rect(screen, 120, 116, 400, 196, 0x7bef);
        SDL_UnlockSurface(screen);
    }
}

int game_draw_preview_modal_overlay(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GameSession *session,
    const HelpAssets *help_assets, const HelpPanelState *help_panel,
    const SmallMapPanelState *small_map_panel, VirtualCursor *cursor)
{
    if (help_panel != NULL && help_panel->active) {
        if (cursor != NULL) {
            cursor->confirm_pressed = 0;
            cursor->cancel_pressed = 0;
        }
        game_draw_help_panel(screen, help_assets, help_panel);
        return 1;
    }
    if (small_map_panel != NULL && small_map_panel->active) {
        GameViewModel small_map_view;
        GameViewModelInput view_input;
        if (cursor != NULL) {
            cursor->confirm_pressed = 0;
            cursor->cancel_pressed = 0;
        }
        if (assets != NULL && settings != NULL && session != NULL) {
            rich4_sdl1_game_view_model_input_build(&session->flow,
                                                   &session->presentation,
                                                   &view_input);
        }
        if (assets != NULL && settings != NULL && session != NULL &&
            rich4_game_view_model_build(
                &session->state, &view_input,
                &assets->map.maps[
                    settings->stage * GAME_MAPS_PER_STAGE + settings->map],
                &small_map_view))
            draw_game_small_map_panel(
                screen, assets, settings, &small_map_view,
                small_map_panel);
        return 1;
    }
    return 0;
}

void draw_game_preview_frame(SDL_Surface *screen,
                             const GamePreviewAssets *assets,
                             const NewGameSettings *settings,
                             GameSession *session,
                             GameRenderCache *render_cache,
                             Rich4AiSettingsState *ai_settings_panel,
                             CardPanelState *card_panel,
                             ToolPanelState *tool_panel,
                             StockPanelState *stock_panel,
                             int view_mode,
                             int calendar_mode,
                             int *trace_count)
{
    if (trace_count != NULL && *trace_count < 3) {
        trace_game_start("preview: update end");
        trace_game_start("preview: draw begin");
    }
    draw_game_preview_adapter(screen, assets, settings, session, render_cache,
                              view_mode, calendar_mode);
    if (rich4_app_bank_interaction_is_loan_page(session)) return;
    game_draw_player_info_panel(screen, assets, session);
    game_draw_ai_settings_panel(screen, assets, session, ai_settings_panel);
    game_draw_card_panel(screen, assets, session, card_panel);
    {
        GameOverlayRenderView overlay_view;
        if (build_game_overlay_render_view(assets, settings, session, &overlay_view)) {
            draw_game_monster_card_animation(screen, assets, &overlay_view);
            draw_game_tool_animation(screen, assets, &overlay_view);
        }
    }
    game_draw_tool_panel(screen, assets, session, tool_panel);
    game_draw_stock_panel(
        screen, assets, settings, &session->state,
        &session->presentation.stock_interaction, stock_panel);
    game_draw_sale_panel(
        screen, assets, session, &session->presentation.sale_interaction);
    game_draw_save_load_panel(screen, assets, session);
    if (trace_count != NULL && *trace_count < 3) {
        trace_game_start("preview: draw end");
        trace_game_start("preview: dice overlay begin");
    }
    if (trace_count != NULL && *trace_count < 3) {
        trace_game_start("preview: dice overlay end");
        (*trace_count)++;
    }
}
