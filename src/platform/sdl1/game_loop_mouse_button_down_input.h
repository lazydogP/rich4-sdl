#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_MOUSE_BUTTON_DOWN_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_MOUSE_BUTTON_DOWN_INPUT_H

#include "app/ai_settings_state.h"
#include "app/card_panel_state.h"
#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/options_flow.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/scene.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: complete physical mouse-button-down dispatch moved out of
 * runtime_main.c. Pointer normalization remains owned by the caller. */
typedef struct GameMouseButtonDownInputContext {
    Rich4SceneId *app_state;
    Rich4Config *config;
    const Rich4SdlConfig *sdl_config;
    const GamePreviewAssets *assets;
    GameSession *session;
    NewGameSettings *settings;
    Rich4OptionsPanelState *options_panel;
    Rich4SceneMachine *scene_machine;
    HelpPanelState *help_panel;
    const HelpAssets *help_assets;
    SmallMapPanelState *small_map_panel;
    Rich4AiSettingsState *ai_settings_panel;
    CardPanelState *card_panel;
    ToolPanelState *tool_panel;
    StockPanelState *stock_panel;
    VirtualCursor *cursor;
    Rich4Audio *audio;
    int *mouse_left_down;
    int button;
} GameMouseButtonDownInputContext;

/* Returns non-zero when the physical button-down event is consumed. */
int game_handle_mouse_button_down_input(
    GameMouseButtonDownInputContext *context);

#endif
