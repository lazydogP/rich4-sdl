#ifndef RICH4_PLATFORM_SDL1_GAME_SCENE_VIEW_BUILDER_H
#define RICH4_PLATFORM_SDL1_GAME_SCENE_VIEW_BUILDER_H

#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "core/game_state.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/game_scene_view.h"

/* Session-to-snapshot assembly belongs to the platform boundary, not to the
 * renderer package. The returned view is consumed read-only by renderers. */
int build_game_session_render_view_for_state(
                                   const GamePreviewAssets *assets,
                                   const NewGameSettings *settings,
                                   const GameSession *session,
                                   const GameState *state,
                                   GameSceneView *out_view);

int build_game_session_render_view(const GamePreviewAssets *assets,
                                   const NewGameSettings *settings,
                                   const GameSession *session,
                                   GameSceneView *out_view);

#endif
