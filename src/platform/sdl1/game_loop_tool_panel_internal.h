#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_TOOL_PANEL_INTERNAL_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_TOOL_PANEL_INTERNAL_H

#include <SDL/SDL.h>

#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "app/tool_panel_state.h"
#include "core/game_domain.h"
#include "platform/sdl1/app_asset_types.h"

int game_tool_panel_route_target(const GamePreviewAssets *assets,
                                 const NewGameSettings *settings,
                                 const GameSession *session, int x, int y,
                                 int require_placeable);
GameDomainRef game_tool_panel_property_target(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, int x, int y);
GameDomainRef game_tool_panel_any_target(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, int x, int y);
int draw_remote_dice_chunk(SDL_Surface *screen,
                           const GamePreviewAssets *assets,
                           size_t chunk_index, int x, int y);
void game_draw_tool_panel(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          const GameSession *session,
                          const ToolPanelState *panel);

#endif
