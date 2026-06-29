#ifndef RICH4_APP_MAP_INPUT_FLOW_H
#define RICH4_APP_MAP_INPUT_FLOW_H

#include "app/app_command.h"
#include "app/game_session.h"
#include "app/map_edge_scroll_state.h"

typedef enum Rich4AppTopPanelButton {
    RICH4_APP_TOP_PANEL_HELP = 0,
    RICH4_APP_TOP_PANEL_OPTIONS,
    RICH4_APP_TOP_PANEL_AI_SETTINGS,
    RICH4_APP_TOP_PANEL_LOAD,
    RICH4_APP_TOP_PANEL_SAVE,
    RICH4_APP_TOP_PANEL_SMALL_MAP,
    RICH4_APP_TOP_PANEL_PLAYER_INFO,
    RICH4_APP_TOP_PANEL_TOOL,
    RICH4_APP_TOP_PANEL_CARD,
    RICH4_APP_TOP_PANEL_SALE,
    RICH4_APP_TOP_PANEL_STOCK,
    RICH4_APP_TOP_PANEL_COUNT
} Rich4AppTopPanelButton;

/* Top UI actions are deliberately independent from minimap controls.  A
 * minimap rotate button is identified by rich4_app_minimap_rotate_button_at()
 * and dispatched directly through rich4_app_rotate_map(). */
typedef enum Rich4AppToolbarActionType {
    RICH4_APP_TOOLBAR_NO_ACTION = 0,
    RICH4_APP_TOOLBAR_INFO_TAB,
    RICH4_APP_TOOLBAR_OPEN_HELP,
    RICH4_APP_TOOLBAR_OPEN_OPTIONS,
    RICH4_APP_TOOLBAR_OPEN_AI_SETTINGS,
    RICH4_APP_TOOLBAR_OPEN_LOAD_PANEL,
    RICH4_APP_TOOLBAR_OPEN_SAVE_PANEL,
    RICH4_APP_TOOLBAR_OPEN_PLAYER_INFO,
    RICH4_APP_TOOLBAR_PRESS_SMALL_MAP,
    RICH4_APP_TOOLBAR_OPEN_CARD_PANEL,
    RICH4_APP_TOOLBAR_OPEN_TOOL_PANEL,
    RICH4_APP_TOOLBAR_OPEN_STOCK_PANEL,
    RICH4_APP_TOOLBAR_OPEN_SALE_PANEL,
    RICH4_APP_TOOLBAR_UNSUPPORTED
} Rich4AppToolbarActionType;

typedef struct Rich4AppToolbarAction {
    Rich4AppToolbarActionType type;
    int value;
} Rich4AppToolbarAction;

int rich4_app_minimap_y_for_view(int view_mode);
int rich4_app_minimap_rotate_button_at(int x, int y, int view_mode);
int rich4_app_info_tab_at(int x, int y);
int rich4_app_current_info_tab(const GameSession *session);
int rich4_app_calendar_mode_at(int x, int y, int view_mode);
void rich4_app_press_minimap_rotate(GameSession *session, int button);
int rich4_app_release_minimap_rotate(GameSession *session);
void rich4_app_cancel_minimap_interaction(GameSession *session);
int rich4_app_restore_minimap_follow(GameSession *session, int x, int y, int view_mode);
int rich4_app_top_panel_button_at(int x, int y);
/* The player-information dialog follows the turn roster: a configured player
 * becomes visible only after its first landing establishes play_state. */
int rich4_app_player_info_player_is_visible(const GameSession *session,
                                            int player);
int rich4_app_player_info_selected_player(const GameSession *session,
                                          int preferred_player);
int rich4_app_player_info_player_at(const GameSession *session, int x, int y);
int rich4_app_player_info_close_at(int x, int y);
int rich4_app_player_info_tab_at(int x, int y);
int rich4_app_player_info_property_filter_at(int x, int y);
/* -1 is previous page, 1 is next page, and 0 is outside both buttons. */
int rich4_app_player_info_property_page_button_at(int x, int y);
int rich4_app_player_info_property_count(const GameSession *session,
                                         int player, int filter);
void rich4_app_player_info_press(GameSession *session, int x, int y);
void rich4_app_player_info_release(GameSession *session);
void rich4_app_player_info_close(GameSession *session);
int rich4_app_toolbar_action_opens_panel(Rich4AppToolbarActionType type);
Rich4AppToolbarAction rich4_app_top_panel_action_at(int x, int y);
int rich4_app_apply_top_panel_action(
    GameSession *session, const Rich4AppToolbarAction *action);
void rich4_app_rotate_map(GameSession *session, int button);
void rich4_app_switch_info_tab(GameSession *session, int tab);
void rich4_app_set_view_from_minimap(GameSession *session, int x, int y,
                                     int view_mode);
int rich4_app_begin_minimap_drag(GameSession *session, int x, int y,
                                 int view_mode);
int rich4_app_update_minimap_drag(GameSession *session, int x, int y,
                                  int view_mode);
void rich4_app_end_minimap_drag(GameSession *session);

/* Shared implementation of the original 50 ms selector timer.  The caller
 * supplies SDL's explicit inset in normalized 640x480 logical pixels; this
 * helper resets the state when the cursor is outside the map edge band or
 * when the requested camera direction is already clamped at 220..2084. */
void rich4_app_map_edge_scroll_reset(Rich4MapEdgeScrollState *state);
int rich4_app_map_edge_scroll_update(
    GameSession *session, Rich4MapEdgeScrollState *state,
    int cursor_x, int cursor_y, int inset, uint32_t now);

#endif
