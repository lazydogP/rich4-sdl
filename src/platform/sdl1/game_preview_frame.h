#ifndef RICH4_PLATFORM_SDL1_GAME_PREVIEW_FRAME_H
#define RICH4_PLATFORM_SDL1_GAME_PREVIEW_FRAME_H

#include <SDL/SDL.h>

#include "app/ai_settings_state.h"
#include "app/card_panel_state.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "platform/sdl1/render/game_render_cache.h"

/* Top-level session/frame composition belongs to the platform boundary. It
 * assembles immutable presentation views, then delegates drawing to renderers. */
void draw_game_preview_adapter(SDL_Surface *screen,
                               const GamePreviewAssets *assets,
                               const NewGameSettings *settings,
                               GameSession *session,
                               GameRenderCache *render_cache,
                               int view_mode, int calendar_mode);

int game_draw_preview_modal_overlay(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GameSession *session,
    const HelpAssets *help_assets, const HelpPanelState *help_panel,
    const SmallMapPanelState *small_map_panel, VirtualCursor *cursor);

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
                             int *trace_count);

#endif
