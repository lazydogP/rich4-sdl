/* R10.1d: game-preview top-toolbar, information-tab and minimap-rotation
 * input moved verbatim out of runtime_main.c.  Physical mouse activation
 * records the original toolbar command on button-down and dispatches it on
 * button-up; virtual confirmation retains the immediate panel-open semantics. */

#include <stdio.h>

#include "app/app_constants.h"
#include "app/map_input_flow.h"
#include "debug/game_debug_trace.h"
#include "platform/sdl1/game_loop_preview_toolbar_input.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/render/game_hud_render.h"
#include "platform/sdl1/render/scene_options_render.h"
#include "view/view_constants.h"

static void debug_log_unsupported_game_top_panel_button(int button)
{
    /* The original dispatch has no confirmed behavior for these slots. Keep
     * the genuinely unsupported slots as observable no-ops. */
    static const char *const functions[RICH4_APP_TOP_PANEL_COUNT] = {
        "help / sub_44EB39",
        "options / sub_411B53",
        "AI settings / sub_41E345",
        "load game / sub_403D74",
        "save game / sub_404165",
        "small map / sub_40A9BD",
        "player information / sub_424492",
        "use tool / sub_447D97",
        "use card / sub_441BAA",
        "sale / sub_4284BE",
        "stock market / sub_42B58F"
    };

    if (button < 0 || button >= RICH4_APP_TOP_PANEL_COUNT) {
        fprintf(stderr,
                "[unsupported][top-panel] unknown button %d\n",
                button);
        return;
    }
    fprintf(stderr,
            "[unsupported][top-panel] button %d: %s\n",
            button,
            functions[button]);
}

int game_open_small_map_panel(
    SmallMapPanelState *panel, const GamePreviewAssets *assets,
    const NewGameSettings *settings, SDL_Surface *capture_source)
{
    SDL_Surface *saved;
    int background_index;
    if (panel == NULL || assets == NULL || settings == NULL ||
        capture_source == NULL)
        return 0;
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        !assets->map.has_map_overview[background_index] ||
        assets->map.map_overview_spr[background_index].num_chunks <= 1 ||
        !assets->map.has_map_control ||
        assets->map.map_control_spr.num_chunks <=
            GAME_SMALL_MAP_FACILITY_MARKER_FIRST + 1)
        return 0;
    /* The capture source is the previous cursor-free frame.  Its toolbar can
     * still contain the clicked button's hover pixels, so restore the normal
     * toolbar before taking the modal background snapshot. */
    draw_game_top_panel_normal(capture_source, assets);
    saved = SDL_CreateRGBSurface(
        SDL_SWSURFACE, capture_source->w, capture_source->h,
        capture_source->format->BitsPerPixel,
        capture_source->format->Rmask, capture_source->format->Gmask,
        capture_source->format->Bmask, capture_source->format->Amask);
    if (saved == NULL ||
        SDL_BlitSurface(capture_source, NULL, saved, NULL) != 0) {
        if (saved != NULL) SDL_FreeSurface(saved);
        return 0;
    }
    if (panel->saved_background != NULL)
        SDL_FreeSurface(panel->saved_background);
    panel->saved_background = saved;
    panel->button_pressed = 0;
    panel->active = 1;
    return 1;
}

void game_close_small_map_panel(SmallMapPanelState *panel)
{
    if (panel == NULL) return;
    panel->active = 0;
    panel->button_pressed = 0;
    if (panel->saved_background != NULL) {
        SDL_FreeSurface(panel->saved_background);
        panel->saved_background = NULL;
    }
}

typedef struct GamePreviewToolbarDispatchContext {
    const Rich4Config *config;
    const Rich4SdlConfig *sdl_config;
    GameSession *session;
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
    Rich4OptionsPanelState *options_panel;
    Rich4SceneMachine *scene_machine;
    Rich4SceneId *app_state;
    HelpPanelState *help_panel;
    const HelpAssets *help_assets;
    SmallMapPanelState *small_map_panel;
    SDL_Surface *capture_source;
    VirtualCursor *cursor;
} GamePreviewToolbarDispatchContext;

static int game_dispatch_preview_toolbar_action(
    const GamePreviewToolbarDispatchContext *context,
    const Rich4AppToolbarAction *action)
{
    if (context == NULL || context->config == NULL ||
        context->session == NULL || context->cursor == NULL ||
        action == NULL || action->type == RICH4_APP_TOOLBAR_NO_ACTION)
        return 0;
    if (rich4_app_toolbar_action_opens_panel(action->type))
        rich4_app_input_set_top_button_hover(context->session, -1);
    if (action->type == RICH4_APP_TOOLBAR_OPEN_HELP) {
        draw_game_top_panel_normal(
            context->capture_source, context->assets);
        if (!game_open_help_panel(
                context->help_panel, context->help_assets,
                context->capture_source))
            fprintf(stderr,
                    "warning: cannot open original Help.mkf panel\n");
    } else if (action->type == RICH4_APP_TOOLBAR_OPEN_OPTIONS) {
        open_options_panel(
            context->options_panel, context->assets, context->config,
            context->sdl_config, RICH4_SCENE_GAME);
        rich4_scene_machine_request_transition(
            context->scene_machine, context->app_state,
            RICH4_SCENE_OPTIONS);
    } else if (action->type == RICH4_APP_TOOLBAR_OPEN_AI_SETTINGS ||
               action->type == RICH4_APP_TOOLBAR_OPEN_LOAD_PANEL ||
               action->type == RICH4_APP_TOOLBAR_OPEN_SAVE_PANEL ||
               action->type == RICH4_APP_TOOLBAR_OPEN_PLAYER_INFO ||
               action->type == RICH4_APP_TOOLBAR_OPEN_CARD_PANEL ||
               action->type == RICH4_APP_TOOLBAR_OPEN_TOOL_PANEL ||
               action->type == RICH4_APP_TOOLBAR_OPEN_STOCK_PANEL ||
               action->type == RICH4_APP_TOOLBAR_OPEN_SALE_PANEL) {
        if (rich4_app_apply_top_panel_action(context->session, action) &&
            (action->type == RICH4_APP_TOOLBAR_OPEN_LOAD_PANEL ||
             action->type == RICH4_APP_TOOLBAR_OPEN_SAVE_PANEL)) {
            GameSaveLoadPanelMode mode =
                action->type == RICH4_APP_TOOLBAR_OPEN_LOAD_PANEL
                    ? GAME_SAVE_LOAD_PANEL_LOAD
                    : GAME_SAVE_LOAD_PANEL_SAVE;
            if (!game_save_load_panel_open(
                    context->session, context->assets, mode)) {
                game_save_load_panel_cancel(context->session, NULL);
                fprintf(stderr,
                        "warning: original save/load dialog resources unavailable\n");
            }
        }
    } else if (action->type == RICH4_APP_TOOLBAR_PRESS_SMALL_MAP) {
        if (!game_open_small_map_panel(
                context->small_map_panel, context->assets,
                context->settings, context->capture_source))
            fprintf(stderr,
                    "warning: cannot open original 400x400 small-map panel\n");
    } else if (action->type == RICH4_APP_TOOLBAR_UNSUPPORTED) {
        debug_log_unsupported_game_top_panel_button(action->value);
    } else {
        return 0;
    }
    context->cursor->confirm_pressed = 0;
    return 1;
}

int game_handle_preview_toolbar_mouse_down(
    GamePreviewToolbarMouseDownContext *context)
{
    Rich4AppToolbarAction action;
    int info_tab;
    if (context == NULL || context->config == NULL ||
        context->session == NULL || context->cursor == NULL)
        return 0;
    info_tab = context->config->view == 2 ? -1 : rich4_app_info_tab_at(
        context->cursor->x, context->cursor->y);
    if (info_tab >= 0) {
        rich4_app_input_set_top_button_pressed(context->session, -1);
        if (info_tab == rich4_app_current_info_tab(context->session)) {
            context->cursor->confirm_pressed = 0;
            return 1;
        }
        action.type = RICH4_APP_TOOLBAR_INFO_TAB;
        action.value = info_tab;
        rich4_app_apply_top_panel_action(context->session, &action);
        rich4_audio_play_select(context->audio);
        context->cursor->confirm_pressed = 0;
        return 1;
    }
    {
        int mode = rich4_app_calendar_mode_at(context->cursor->x,
            context->cursor->y, context->config->view);
        if (mode >= 0) {
            if (mode != context->config->calendar_view) {
                context->config->calendar_view = (unsigned char)mode;
                rich4_audio_play_select(context->audio);
            }
            context->cursor->confirm_pressed = 0;
            return 1;
        }
    }
    action = rich4_app_top_panel_action_at(
        context->cursor->x, context->cursor->y);
    if (action.type == RICH4_APP_TOOLBAR_NO_ACTION)
        return 0;
    /* 0x417E26 stores x / 40 + 100 on WM_LBUTTONDOWN.  It does not invoke
     * 0x417D65 until WM_LBUTTONUP, so keep the button index in app state. */
    rich4_app_input_set_top_button_pressed(
        context->session, action.value);
    /* sub_417D65 plays Effect.mkf[1] on button-down before dispatching
     * every top-toolbar command, including Options. */
    rich4_audio_play_ok(context->audio);
    context->cursor->confirm_pressed = 0;
    return 1;
}

int game_handle_preview_toolbar_mouse_up(
    GamePreviewToolbarMouseUpContext *context)
{
    GamePreviewToolbarDispatchContext dispatch;
    Rich4AppToolbarAction action;
    int button;
    if (context == NULL || context->config == NULL ||
        context->session == NULL || context->cursor == NULL ||
        context->app_state == NULL || context->button != SDL_BUTTON_LEFT)
        return 0;
    button = rich4_app_input_take_top_button_pressed(context->session);
    if (button < 0) return 0;
    context->cursor->confirm_pressed = 0;
    context->cursor->confirm_pointer_pressed = 0;
    context->cursor->cancel_pressed = 0;
    if (*context->app_state != RICH4_SCENE_GAME ||
        button >= RICH4_APP_TOP_PANEL_COUNT)
        return 1;
    action = rich4_app_top_panel_action_at(button * 40 + 20, 20);
    dispatch.config = context->config;
    dispatch.sdl_config = context->sdl_config;
    dispatch.session = context->session;
    dispatch.assets = context->assets;
    dispatch.settings = context->settings;
    dispatch.options_panel = context->options_panel;
    dispatch.scene_machine = context->scene_machine;
    dispatch.app_state = context->app_state;
    dispatch.help_panel = context->help_panel;
    dispatch.help_assets = context->help_assets;
    dispatch.small_map_panel = context->small_map_panel;
    dispatch.capture_source = context->capture_source;
    dispatch.cursor = context->cursor;
    (void)game_dispatch_preview_toolbar_action(&dispatch, &action);
    return 1;
}

int game_handle_preview_minimap_rotate_mouse_down(
    GamePreviewToolbarMouseDownContext *context)
{
    int button;
    if (context == NULL || context->config == NULL ||
        context->session == NULL || context->cursor == NULL)
        return 0;
    button = rich4_app_minimap_rotate_button_at(
        context->cursor->x, context->cursor->y, context->config->view);
    if (button == 0) return 0;
    /* Minimap controls have their own hit-test and state transition.  They do
     * not enter the top-panel action route, so hovering/clicking the toolbar
     * cannot mutate map_direction. */
    rich4_app_press_minimap_rotate(context->session, button);
    rich4_audio_play_select(context->audio);
    context->cursor->confirm_pressed = 0;
    return 1;
}

int game_handle_preview_toolbar_confirm(
    GamePreviewToolbarConfirmContext *context)
{
    GamePreviewToolbarDispatchContext dispatch;
    Rich4AppToolbarAction action;
    int info_tab;
    if (context == NULL || context->config == NULL ||
        context->session == NULL || context->cursor == NULL)
        return 0;
    info_tab = context->config->view == 2 ? -1 : rich4_app_info_tab_at(
        context->cursor->x, context->cursor->y);
    if (info_tab >= 0) {
        if (info_tab == rich4_app_current_info_tab(context->session)) {
            context->cursor->confirm_pressed = 0;
            return 1;
        }
        action.type = RICH4_APP_TOOLBAR_INFO_TAB;
        action.value = info_tab;
        rich4_app_apply_top_panel_action(context->session, &action);
        rich4_audio_play_select(context->audio);
        context->cursor->confirm_pressed = 0;
        return 1;
    }
    {
        int mode = rich4_app_calendar_mode_at(context->cursor->x,
            context->cursor->y, context->config->view);
        if (mode >= 0) {
            if (mode != context->config->calendar_view) {
                context->config->calendar_view = (unsigned char)mode;
                rich4_audio_play_select(context->audio);
            }
            context->cursor->confirm_pressed = 0;
            return 1;
        }
    }
    action = rich4_app_top_panel_action_at(
        context->cursor->x, context->cursor->y);
    if (action.type == RICH4_APP_TOOLBAR_NO_ACTION)
        return 0;
    dispatch.config = context->config;
    dispatch.sdl_config = context->sdl_config;
    dispatch.session = context->session;
    dispatch.assets = context->assets;
    dispatch.settings = context->settings;
    dispatch.options_panel = context->options_panel;
    dispatch.scene_machine = context->scene_machine;
    dispatch.app_state = context->app_state;
    dispatch.help_panel = context->help_panel;
    dispatch.help_assets = context->help_assets;
    dispatch.small_map_panel = context->small_map_panel;
    dispatch.capture_source = context->capture_source;
    dispatch.cursor = context->cursor;
    return game_dispatch_preview_toolbar_action(&dispatch, &action);
}

int game_handle_preview_minimap_rotate_confirm(
    GamePreviewToolbarConfirmContext *context)
{
    int button;
    if (context == NULL || context->config == NULL ||
        context->session == NULL || context->cursor == NULL)
        return 0;
    button = rich4_app_minimap_rotate_button_at(
        context->cursor->x, context->cursor->y, context->config->view);
    if (button == 0) return 0;
    rich4_app_rotate_map(context->session, button);
    rich4_audio_play_select(context->audio);
    context->cursor->confirm_pressed = 0;
    return 1;
}

int game_handle_preview_small_map_button_up(
    GamePreviewSmallMapButtonUpContext *context)
{
    if (context == NULL || context->panel == NULL ||
        context->session == NULL || context->cursor == NULL ||
        context->mouse_left_down == NULL)
        return 0;
    if (context->button == SDL_BUTTON_LEFT &&
        context->panel->button_pressed && !context->panel->active) {
        /* The saved toolbar command is released even if the pointer moved
         * away from the original 40x40 slot. Effect.mkf[1] already played
         * on button-down. */
        *context->mouse_left_down = 0;
        context->panel->button_pressed = 0;
        rich4_app_input_set_top_button_hover(context->session, -1);
        if (!game_open_small_map_panel(
                context->panel, context->assets, context->settings,
                context->capture_source))
            fprintf(stderr,
                    "warning: cannot open original 400x400 small-map panel\n");
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->panel->active &&
        (context->button == SDL_BUTTON_LEFT ||
         context->button == SDL_BUTTON_RIGHT)) {
        if (context->button == SDL_BUTTON_LEFT) {
            *context->mouse_left_down = 0;
        } else {
            /* 0x40A854 posts the modal close message without calling
             * sub_4542CE: there is no close sound. */
            game_close_small_map_panel(context->panel);
            rich4_presentation_clear_view_override(
                &context->session->presentation);
            rich4_app_end_minimap_drag(context->session);
        }
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    return 0;
}
