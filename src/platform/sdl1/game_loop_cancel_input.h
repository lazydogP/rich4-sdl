#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CANCEL_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CANCEL_INPUT_H

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
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: virtual-cancel priority dispatch moved out of runtime_main.c.
 * Modal handlers keep their existing contexts; this seam only assembles the
 * original early-return order from caller-owned state. */
typedef struct GameCancelInputContext {
    Rich4SceneId *app_state;
    GameSession *session;
    GamePreviewAssets *assets;
    NewGameSettings *settings;
    Rich4OptionsPanelState *options_panel;
    Rich4SceneMachine *scene_machine;
    Rich4Config *config;
    StockPanelState *stock_panel;
    SmallMapPanelState *small_map_panel;
    CardPanelState *card_panel;
    ToolPanelState *tool_panel;
    Rich4AiSettingsState *ai_settings_panel;
    VirtualCursor *cursor;
    Rich4Audio *audio;
    const char *music_dir;
    int *shop_music_restore_pending;
    int *running;
} GameCancelInputContext;

void game_handle_cancel_input(GameCancelInputContext *context);

#endif
