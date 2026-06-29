#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CANCEL_PANELS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CANCEL_PANELS_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/ai_settings_state.h"
#include "app/card_panel_state.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/scene.h"
#include "app/tool_panel_state.h"

/* R10.1d: card/tool/AI panel cancel branches moved out of runtime_main.c. */
typedef struct GameCancelPanelsContext {
    Rich4SceneId app_state;
    GameSession *session;
    CardPanelState *card_panel;
    ToolPanelState *tool_panel;
    Rich4AiSettingsState *ai_settings_panel;
    VirtualCursor *cursor;
    Rich4Audio *audio;
} GameCancelPanelsContext;

/* Returns non-zero when a panel consumed cancel. */
int game_handle_cancel_panels(GameCancelPanelsContext *context);

#endif
