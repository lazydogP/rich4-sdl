/* App-owned map input semantics.  It mutates only presentation state, borrows
 * the session, depends on view geometry constants, and is covered by the app
 * map-input regression target. */

#include "app/map_input_flow.h"

#include "app/ai_settings_flow.h"
#include "app/card_panel_flow.h"
#include "app/input_flow.h"
#include "app/stock_interaction_flow.h"
#include "app/sale_interaction_flow.h"
#include "app/tool_panel_flow.h"
#include "view/view_constants.h"

static int map_clamp(int value, int low, int high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int map_point_in_rect(int x, int y, int left, int top,
                             int width, int height)
{
    return x >= left && x < left + width &&
           y >= top && y < top + height;
}

int rich4_app_player_info_player_is_visible(const GameSession *session,
                                            int player)
{
    return session != NULL && player >= 0 &&
           player < session->state.player_count && player < GAME_MAX_PLAYERS &&
           !session->presentation.actors[player].first_turn_landing_pending &&
           (session->state.players[player].play_state &
            GAME_PLAYER_PLAY_STATE_MASK) != 0;
}

int rich4_app_player_info_selected_player(const GameSession *session,
                                          int preferred_player)
{
    int player;
    if (rich4_app_player_info_player_is_visible(session, preferred_player))
        return preferred_player;
    if (session == NULL) return -1;
    for (player = 0;
         player < session->state.player_count && player < GAME_MAX_PLAYERS;
         player++) {
        if (rich4_app_player_info_player_is_visible(session, player))
            return player;
    }
    return -1;
}

int rich4_app_player_info_player_at(const GameSession *session, int x, int y)
{
    int player;
    int slot = 0;
    if (session == NULL || y < 14 || y > 47) return -1;
    for (player = 0;
         player < session->state.player_count && player < GAME_MAX_PLAYERS;
         player++) {
        int left;
        if (!rich4_app_player_info_player_is_visible(session, player))
            continue;
        left = 16 + slot * 88;
        if (x >= left && x <= left + 88) return player;
        slot++;
    }
    return -1;
}

int rich4_app_player_info_close_at(int x, int y)
{
    /* 0x423DD1: the Panel.mkf[9] close resource is pressed in this box. */
    return x >= 492 && x <= 602 && y >= 9 && y <= 38;
}

int rich4_app_player_info_tab_at(int x, int y)
{
    int tab;
    if (x < 12 || x > 109) return -1;
    for (tab = 0; tab < 3; tab++) {
        int top = 282 + tab * 64;
        if (y >= top && y <= top + 40) return tab;
    }
    return -1;
}

int rich4_app_player_info_property_filter_at(int x, int y)
{
    int filter;
    if (y < 64 || y > 97) return -1;
    for (filter = 0; filter < 5; filter++) {
        int left = 120 + filter * 75;
        if (x >= left && x <= left + 75) return filter;
    }
    return -1;
}

int rich4_app_player_info_property_page_button_at(int x, int y)
{
    if (x <= 593 || x >= 623) return 0;
    if (y > 369 && y < 399) return -1;
    if (y > 417 && y < 447) return 1;
    return 0;
}

int rich4_app_player_info_property_count(const GameSession *session,
                                         int player, int filter)
{
    const GameState *state;
    int count = 0;
    int i;

    if (session == NULL || player < 0 ||
        player >= session->state.player_count || filter < 0 || filter > 4)
        return 0;
    state = &session->state;
    for (i = 0; i < state->land_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        int type;
        int level;
        if (state->land_owner[i] != player) continue;
        type = state->land_type[i];
        level = state->land_level[i];
        if (filter == 0 || filter == 1 ||
            (filter == 3 && level != 0 && type == 0) ||
            (filter == 4 && level != 0 && type != 0))
            count++;
    }
    if (filter == 0 || filter == 2)
        for (i = 0;
             i < state->facility_count && i < GAME_MAX_MAP_PROPERTIES;
             i++)
            if (state->facility_owner[i] == player) count++;
    return count;
}

void rich4_app_player_info_press(GameSession *session, int x, int y)
{
    int player;
    int tab;

    if (session == NULL || !session->presentation.player_info.active)
        return;
    session->presentation.player_info.pressed_control = 0;
    if (rich4_app_player_info_close_at(x, y)) {
        session->presentation.player_info.pressed_control = 1;
        return;
    }
    player = rich4_app_player_info_player_at(session, x, y);
    if (player >= 0) {
        if (player != session->presentation.player_info.selected_player) {
            session->presentation.player_info.selected_player = player;
            if (session->presentation.player_info.tab == 1)
                session->presentation.player_info.property_page = 0;
            /* 0x4240B1 sets control 7 only after the selected player
             * actually changes; clicking the current player stays idle. */
            session->presentation.player_info.pressed_control = 7;
        }
        return;
    }
    tab = rich4_app_player_info_tab_at(x, y);
    if (tab >= 0) {
        session->presentation.player_info.pressed_control = tab + 2;
        return;
    }
    if (session->presentation.player_info.tab == 1) {
        int filter = rich4_app_player_info_property_filter_at(x, y);
        int page_button = rich4_app_player_info_property_page_button_at(x, y);
        if (filter >= 0 &&
            filter != session->presentation.player_info.property_filter) {
            session->presentation.player_info.property_filter = filter;
            session->presentation.player_info.property_page = 0;
        } else if (page_button < 0) {
            session->presentation.player_info.pressed_control = 5;
        } else if (page_button > 0) {
            session->presentation.player_info.pressed_control = 6;
        }
    }
}

void rich4_app_player_info_release(GameSession *session)
{
    int pressed;
    int selected;

    if (session == NULL || !session->presentation.player_info.active)
        return;
    pressed = session->presentation.player_info.pressed_control;
    session->presentation.player_info.pressed_control = 0;
    if (pressed == 1) {
        session->presentation.player_info.active = 0;
    } else if (pressed >= 2 && pressed <= 4) {
        session->presentation.player_info.tab = pressed - 2;
        if (session->presentation.player_info.tab == 1)
            session->presentation.player_info.property_page = 0;
    } else if (pressed == 5) {
        if (session->presentation.player_info.property_page > 0)
            session->presentation.player_info.property_page--;
    } else if (pressed == 6) {
        int page = session->presentation.player_info.property_page;
        int count;
        selected = rich4_app_player_info_selected_player(
            session, session->presentation.player_info.selected_player);
        count = rich4_app_player_info_property_count(
            session, selected,
            session->presentation.player_info.property_filter);
        /* 0x422618 advances only when an eleventh row remains. */
        if (selected >= 0 && page * 10 + 10 < count)
            session->presentation.player_info.property_page++;
    }
}

void rich4_app_player_info_close(GameSession *session)
{
    if (session == NULL) return;
    session->presentation.player_info.active = 0;
    session->presentation.player_info.pressed_control = 0;
}

int rich4_app_toolbar_action_opens_panel(Rich4AppToolbarActionType type)
{
    switch (type) {
    case RICH4_APP_TOOLBAR_OPEN_HELP:
    case RICH4_APP_TOOLBAR_OPEN_OPTIONS:
    case RICH4_APP_TOOLBAR_OPEN_AI_SETTINGS:
    case RICH4_APP_TOOLBAR_OPEN_LOAD_PANEL:
    case RICH4_APP_TOOLBAR_OPEN_SAVE_PANEL:
    case RICH4_APP_TOOLBAR_OPEN_PLAYER_INFO:
    case RICH4_APP_TOOLBAR_PRESS_SMALL_MAP:
    case RICH4_APP_TOOLBAR_OPEN_CARD_PANEL:
    case RICH4_APP_TOOLBAR_OPEN_TOOL_PANEL:
    case RICH4_APP_TOOLBAR_OPEN_STOCK_PANEL:
    case RICH4_APP_TOOLBAR_OPEN_SALE_PANEL:
        return 1;
    default:
        return 0;
    }
}

int rich4_app_minimap_y_for_view(int view_mode)
{
    return view_mode == 2 ? 80 : 280;
}

int rich4_app_minimap_rotate_button_at(int x, int y, int view_mode)
{
    int top;
    if (view_mode != 1 && view_mode != 2) return 0;
    top = rich4_app_minimap_y_for_view(view_mode) + 3;
    if (map_point_in_rect(x, y, GAME_MINIMAP_ROTATE_LEFT_X, top,
                          GAME_MINIMAP_ROTATE_W, GAME_MINIMAP_ROTATE_H))
        return 1;
    if (map_point_in_rect(x, y, GAME_MINIMAP_ROTATE_RIGHT_X, top,
                          GAME_MINIMAP_ROTATE_W, GAME_MINIMAP_ROTATE_H))
        return 2;
    return 0;
}

int rich4_app_info_tab_at(int x, int y)
{
    if (x < 616 || x >= 640 || y < 0 || y >= 280) return -1;
    return y / 70;
}

/* 0x41835F: inclusive original sun/moon hit rectangles. */
int rich4_app_calendar_mode_at(int x, int y, int view_mode)
{
    if ((view_mode != 0 && view_mode != 2) || y < 288 || y > 314) return -1;
    if (x >= 448 && x <= 474) return 0;
    if (x >= 478 && x <= 504) return 1;
    return -1;
}

int rich4_app_top_panel_button_at(int x, int y)
{
    if (y < 0 || y >= 40 || x < 0 || x >= 440) return -1;
    return x / 40;
}

Rich4AppToolbarAction rich4_app_top_panel_action_at(int x, int y)
{
    Rich4AppToolbarAction action = {
        RICH4_APP_TOOLBAR_NO_ACTION, -1
    };
    int top_button;

    top_button = rich4_app_top_panel_button_at(x, y);
    if (top_button >= 0 && top_button < RICH4_APP_TOP_PANEL_COUNT) {
        action.type = RICH4_APP_TOOLBAR_UNSUPPORTED;
        action.value = top_button;
        switch (top_button) {
        case RICH4_APP_TOP_PANEL_HELP:
            action.type = RICH4_APP_TOOLBAR_OPEN_HELP;
            break;
        case RICH4_APP_TOP_PANEL_OPTIONS:
            action.type = RICH4_APP_TOOLBAR_OPEN_OPTIONS;
            break;
        case RICH4_APP_TOP_PANEL_AI_SETTINGS:
            action.type = RICH4_APP_TOOLBAR_OPEN_AI_SETTINGS;
            break;
        case RICH4_APP_TOP_PANEL_LOAD:
            action.type = RICH4_APP_TOOLBAR_OPEN_LOAD_PANEL;
            break;
        case RICH4_APP_TOP_PANEL_SAVE:
            action.type = RICH4_APP_TOOLBAR_OPEN_SAVE_PANEL;
            break;
        case RICH4_APP_TOP_PANEL_PLAYER_INFO:
            action.type = RICH4_APP_TOOLBAR_OPEN_PLAYER_INFO;
            break;
        case RICH4_APP_TOP_PANEL_SMALL_MAP:
            action.type = RICH4_APP_TOOLBAR_PRESS_SMALL_MAP;
            break;
        case RICH4_APP_TOP_PANEL_TOOL:
            action.type = RICH4_APP_TOOLBAR_OPEN_TOOL_PANEL;
            break;
        case RICH4_APP_TOP_PANEL_CARD:
            action.type = RICH4_APP_TOOLBAR_OPEN_CARD_PANEL;
            break;
        case RICH4_APP_TOP_PANEL_STOCK:
            action.type = RICH4_APP_TOOLBAR_OPEN_STOCK_PANEL;
            break;
        case RICH4_APP_TOP_PANEL_SALE:
            action.type = RICH4_APP_TOOLBAR_OPEN_SALE_PANEL;
            break;
        default:
            break;
        }
        return action;
    }
    return action;
}

int rich4_app_apply_top_panel_action(
    GameSession *session, const Rich4AppToolbarAction *action)
{
    if (session == NULL || action == NULL || !session->state.active)
        return 0;
    /* 0x417D65 clears the top-toolbar hover before entering a new panel.  Do
     * this before the panel state changes so the frame that first presents the
     * panel cannot reuse the clicked button's hover chunk. */
    if (rich4_app_toolbar_action_opens_panel(action->type))
        rich4_app_input_set_top_button_hover(session, -1);
    if (action->type == RICH4_APP_TOOLBAR_INFO_TAB) {
        rich4_app_switch_info_tab(session, action->value);
        return 1;
    }
    if (action->type == RICH4_APP_TOOLBAR_OPEN_AI_SETTINGS) {
        rich4_ai_settings_open(&session->presentation.ai_settings, session);
        return 1;
    }
    if (action->type == RICH4_APP_TOOLBAR_OPEN_LOAD_PANEL ||
        action->type == RICH4_APP_TOOLBAR_OPEN_SAVE_PANEL) {
        session->presentation.save_load.mode =
            action->type == RICH4_APP_TOOLBAR_OPEN_LOAD_PANEL
                ? GAME_SAVE_LOAD_PANEL_LOAD
                : GAME_SAVE_LOAD_PANEL_SAVE;
        session->presentation.save_load.selected_row = 0;
        session->presentation.save_load.pressed_row = -1;
        return 1;
    }
    if (action->type == RICH4_APP_TOOLBAR_OPEN_PLAYER_INFO) {
        int selected = rich4_app_player_info_selected_player(
            session, session->state.current_player);
        if (selected < 0) return 0;
        session->presentation.player_info.active = 1;
        session->presentation.player_info.selected_player = selected;
        session->presentation.player_info.tab = 0;
        session->presentation.player_info.property_filter = 0;
        session->presentation.player_info.property_page = 0;
        session->presentation.player_info.pressed_control = 0;
        return 1;
    }
    if (action->type == RICH4_APP_TOOLBAR_OPEN_CARD_PANEL) {
        rich4_card_panel_open(&session->presentation.card_panel, session);
        return 1;
    }
    if (action->type == RICH4_APP_TOOLBAR_OPEN_TOOL_PANEL) {
        rich4_tool_panel_open(&session->presentation.tool_panel, session);
        return 1;
    }
    if (action->type == RICH4_APP_TOOLBAR_OPEN_STOCK_PANEL) {
        rich4_app_stock_interaction_open(session);
        return 1;
    }
    if (action->type == RICH4_APP_TOOLBAR_OPEN_SALE_PANEL) {
        rich4_app_sale_interaction_open(session);
        return rich4_app_sale_interaction_is_active(session);
    }
    return action->type == RICH4_APP_TOOLBAR_OPEN_HELP ||
           action->type == RICH4_APP_TOOLBAR_OPEN_OPTIONS ||
           action->type == RICH4_APP_TOOLBAR_PRESS_SMALL_MAP ||
           action->type == RICH4_APP_TOOLBAR_UNSUPPORTED;
}

void rich4_app_rotate_map(GameSession *session, int button)
{
    if (session == NULL || !session->state.active) return;
    if (button == 1)
        session->presentation.map_direction =
            (session->presentation.map_direction + 7) & 7;
    else if (button == 2)
        session->presentation.map_direction =
            (session->presentation.map_direction + 1) & 7;
}

void rich4_app_switch_info_tab(GameSession *session, int tab)
{
    if (session == NULL || !session->state.active || tab < 0 || tab >= 4)
        return;
    session->presentation.active_info_tab = tab;
    if (session->state.current_player >= 0 && session->state.current_player < GAME_MAX_PLAYERS)
        session->presentation.player_info_tabs[session->state.current_player] = tab;
}

int rich4_app_current_info_tab(const GameSession *session)
{
    int tab;
    if (session == NULL || session->state.current_player < 0 ||
        session->state.current_player >= GAME_MAX_PLAYERS) return 0;
    tab = session->presentation.player_info_tabs[session->state.current_player];
    return tab >= 0 && tab < 4 ? tab : 0;
}

void rich4_app_press_minimap_rotate(GameSession *session, int button)
{
    if (session == NULL || !session->state.active || button < 1 || button > 2) return;
    session->presentation.dragging_minimap = 0;
    session->presentation.minimap_rotate_pressed = button;
}

int rich4_app_release_minimap_rotate(GameSession *session)
{
    int button;
    if (session == NULL) return 0;
    button = session->presentation.minimap_rotate_pressed;
    session->presentation.minimap_rotate_pressed = 0;
    if (button == 0) return 0;
    rich4_app_rotate_map(session, button);
    return 1;
}

void rich4_app_cancel_minimap_interaction(GameSession *session)
{
    if (session == NULL) return;
    session->presentation.minimap_rotate_pressed = 0;
    session->presentation.dragging_minimap = 0;
}

/* 0x418893: right-button release inside the displayed minimap clears the
 * observation override. Modal callbacks retain ownership of their release. */
int rich4_app_restore_minimap_follow(GameSession *session, int x, int y, int view_mode)
{
    Rich4PresentationActivity activity;
    const GamePresentationState *p;
    if (session == NULL || !session->state.active ||
        !session->presentation.has_view_override || x <= 440 || x >= 640 ||
        (view_mode != 1 && view_mode != 2) ||
        y <= rich4_app_minimap_y_for_view(view_mode) || y >= 480 ||
        (view_mode == 2 && y >= 280)) return 0;
    p = &session->presentation;
    activity = rich4_presentation_activity_from_state(p, &session->state);
    if (!rich4_app_input_ui_active(RICH4_SCENE_GAME, NULL, session, p->ai_settings.active) ||
        rich4_presentation_mode_resolve(&activity) != RICH4_PRESENTATION_NONE ||
        p->route_interaction.active || p->player_info.active ||
        p->card_panel.phase != CARD_PANEL_CLOSED || p->tool_panel.phase != TOOL_PANEL_CLOSED ||
        p->ai_settings.active || p->save_load.mode != GAME_SAVE_LOAD_PANEL_CLOSED ||
        rich4_app_stock_interaction_is_active(session) || rich4_app_sale_interaction_is_active(session))
        return 0;
    rich4_presentation_clear_view_override(&session->presentation);
    return 1;
}

static int minimap_to_world_x(int minimap_x)
{
    int local_x = map_clamp(minimap_x - GAME_MINIMAP_X, 0,
                            GAME_MINIMAP_W - 1);
    return map_clamp((int)(((long)local_x * 761856) >> 16),
                     0, GAME_WORLD_MAX_X);
}

static int minimap_to_world_y(int pointer_y, int minimap_y)
{
    int local_y = map_clamp(pointer_y - minimap_y, 0,
                            GAME_MINIMAP_H - 1);
    return map_clamp((int)(((long)local_y * 761856) >> 16),
                     0, GAME_WORLD_MAX_Y);
}

void rich4_app_set_view_from_minimap(GameSession *session, int x, int y,
                                     int view_mode)
{
    int minimap_y;
    if (session == NULL || !session->state.active) return;
    minimap_y = rich4_app_minimap_y_for_view(view_mode);
    session->presentation.view_world_x = map_clamp(
        minimap_to_world_x(x), 220, 2084);
    session->presentation.view_world_y = map_clamp(
        minimap_to_world_y(y, minimap_y), 220, 2084);
    session->presentation.has_view_override = 1;
}

static int minimap_pointer_inside(int x, int y, int view_mode)
{
    return (view_mode == 1 || view_mode == 2) &&
           x > GAME_MINIMAP_X && y > rich4_app_minimap_y_for_view(view_mode) &&
           map_point_in_rect(
               x, y, GAME_MINIMAP_X, rich4_app_minimap_y_for_view(view_mode),
               GAME_MINIMAP_W, GAME_MINIMAP_H) &&
           !map_point_in_rect(
               x, y, GAME_MINIMAP_X + 3,
               rich4_app_minimap_y_for_view(view_mode) + 3, 50, GAME_MINIMAP_ROTATE_H);
}

int rich4_app_begin_minimap_drag(GameSession *session, int x, int y,
                                 int view_mode)
{
    if (session == NULL || !session->state.active ||
        !minimap_pointer_inside(x, y, view_mode))
        return 0;
    session->presentation.dragging_minimap = 1;
    rich4_app_set_view_from_minimap(session, x, y, view_mode);
    return 1;
}

int rich4_app_update_minimap_drag(GameSession *session, int x, int y,
                                  int view_mode)
{
    if (session == NULL || !session->presentation.dragging_minimap)
        return 0;
    if ((view_mode == 1 || view_mode == 2) && x > GAME_MINIMAP_X &&
        y > rich4_app_minimap_y_for_view(view_mode) && map_point_in_rect(
            x, y, GAME_MINIMAP_X,
            rich4_app_minimap_y_for_view(view_mode),
            GAME_MINIMAP_W, GAME_MINIMAP_H)) {
        rich4_app_set_view_from_minimap(session, x, y, view_mode);
        return 1;
    }
    session->presentation.dragging_minimap = 0;
    return 0;
}

void rich4_app_end_minimap_drag(GameSession *session)
{
    if (session != NULL)
        session->presentation.dragging_minimap = 0;
}
