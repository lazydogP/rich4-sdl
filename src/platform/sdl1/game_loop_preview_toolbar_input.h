#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_PREVIEW_TOOLBAR_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_PREVIEW_TOOLBAR_INPUT_H

#include <SDL/SDL.h>

#include "app/ai_settings_state.h"
#include "app/card_panel_state.h"
#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/map_input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/options_flow.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/scene.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/app_panel_types.h"

typedef struct GamePreviewToolbarMouseDownContext {
    Rich4Config *config;
    const Rich4SdlConfig *sdl_config;
    const GamePreviewAssets *assets;
    GameSession *session;
    Rich4OptionsPanelState *options_panel;
    Rich4SceneMachine *scene_machine;
    Rich4SceneId *app_state;
    HelpPanelState *help_panel;
    SmallMapPanelState *small_map_panel;
    Rich4AiSettingsState *ai_settings_panel;
    CardPanelState *card_panel;
    ToolPanelState *tool_panel;
    StockPanelState *stock_panel;
    VirtualCursor *cursor;
    Rich4Audio *audio;
} GamePreviewToolbarMouseDownContext;

typedef struct GamePreviewToolbarMouseUpContext {
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
    int button;
} GamePreviewToolbarMouseUpContext;

typedef struct GamePreviewToolbarConfirmContext {
    Rich4Config *config;
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
    Rich4AiSettingsState *ai_settings_panel;
    CardPanelState *card_panel;
    ToolPanelState *tool_panel;
    StockPanelState *stock_panel;
    SDL_Surface *capture_source;
    VirtualCursor *cursor;
    Rich4Audio *audio;
} GamePreviewToolbarConfirmContext;

typedef struct GamePreviewSmallMapButtonUpContext {
    SmallMapPanelState *panel;
    GameSession *session;
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
    SDL_Surface *capture_source;
    VirtualCursor *cursor;
    int button;
    int *mouse_left_down;
} GamePreviewSmallMapButtonUpContext;

int game_open_small_map_panel(
    SmallMapPanelState *panel, const GamePreviewAssets *assets,
    const NewGameSettings *settings, SDL_Surface *capture_source);

void game_close_small_map_panel(SmallMapPanelState *panel);

/* Returns non-zero when the top toolbar or information tab consumed this
 * physical button-down.  Top-toolbar actions are only recorded here; the
 * original 0x417E26 dispatch occurs from the matching button-up. */
int game_handle_preview_toolbar_mouse_down(
    GamePreviewToolbarMouseDownContext *context);

/* Returns non-zero when the recorded top-toolbar command consumed the
 * physical button-up. */
int game_handle_preview_toolbar_mouse_up(
    GamePreviewToolbarMouseUpContext *context);

/* Returns non-zero when the top toolbar or information tab consumed this
 * virtual confirm.  Minimap controls use the separate route below. */
int game_handle_preview_toolbar_confirm(
    GamePreviewToolbarConfirmContext *context);

int game_handle_preview_minimap_rotate_mouse_down(
    GamePreviewToolbarMouseDownContext *context);
int game_handle_preview_minimap_rotate_confirm(
    GamePreviewToolbarConfirmContext *context);

/* Returns non-zero when an active small-map close consumed this physical
 * button-up. */
int game_handle_preview_small_map_button_up(
    GamePreviewSmallMapButtonUpContext *context);

#endif
