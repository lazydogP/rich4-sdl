#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_PREVIEW_UPDATE_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_PREVIEW_UPDATE_H

#include <SDL/SDL.h>

#include "app/ai_settings_state.h"
#include "app/audio_event.h"
#include "app/card_panel_state.h"
#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/options_flow.h"
#include "app/tool_panel_state.h"
#include "app/scene.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "platform/sdl1/render/game_render_cache.h"

/* R10.1d: one preview-frame update/presentation seam moved out of
 * runtime_main.c. The caller still owns all long-lived state and surfaces;
 * this context makes the original ordering explicit: music rollover, modal
 * overlay gate, toolbar confirm, session update, cursor synchronization,
 * audio-event bridge and completed preview-frame draw. Keep the frame inputs
 * direct here: this is a single callback payload, not a hierarchy of one-shot
 * modal/panel/cursor adapters. */
typedef struct GamePreviewFrameContext {
    SDL_Surface *screen;
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
    GameSession *session;
    GameRenderCache *render_cache;
    Rich4Config *config;
    const Rich4SdlConfig *sdl_config;
    Rich4Audio *audio;
    const char *music_dir;
    int *main_map_music_track;
    int *main_map_music_active;
    int *confinement_music_kind;
    int *bank_music_active;
    int *lottery_music_active;
    int *magic_house_music_active;
    int *auction_music_active;
    int *shop_music_restore_pending;
    HelpPanelState *help_panel;
    const HelpAssets *help_assets;
    SmallMapPanelState *small_map_panel;
    Rich4OptionsPanelState *options_panel;
    Rich4SceneMachine *scene_machine;
    Rich4SceneId *app_state;
    /* Process-lifetime JUMP completion bits mirror 0x4075C1's
     * lpBuffer__27[0..3]; they survive a code-2 session reset. */
    int *ending_map_mask;
    int *running;
    Uint32 *ending_wait_started;
    int *ending_wait_active;
    Rich4AiSettingsState *ai_settings_panel;
    CardPanelState *card_panel;
    ToolPanelState *tool_panel;
    StockPanelState *stock_panel;
    SDL_Surface *cursor_free_frame;
    VirtualCursor *cursor;
    int *host_pointer_sync_needed;
    int *trace_count;
} GamePreviewFrameContext;

void game_update_preview_frame(GamePreviewFrameContext *context);

#endif
