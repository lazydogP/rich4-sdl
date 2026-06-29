#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_INPUT_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/ai_settings_state.h"
#include "app/card_panel_state.h"
#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/options_flow.h"
#include "app/scene.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "app/tool_panel_state.h"

/* R10.1d: virtual-confirm priority dispatch moved out of runtime_main.c.
 * The caller owns the event-loop state; this context only exposes the same
 * borrowed references needed by the existing modal and scene handlers. */
typedef struct GameConfirmInputContext {
    Rich4SceneId *app_state;
    MenuAssets *menu_assets;
    GamePreviewAssets *assets;
    GameSession *session;
    NewGameSettings *settings;
    Rich4Config *config;
    Rich4SdlConfig *sdl_config;
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
    const char *music_dir;
    int mouse_left_down;
    int *host_pointer_sync_needed;
    int *running;
} GameConfirmInputContext;

void game_handle_confirm_input(GameConfirmInputContext *context);

#endif
