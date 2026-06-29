#ifndef RICH4_PLATFORM_SDL1_GAME_INTRO_RENDER_VIEW_H
#define RICH4_PLATFORM_SDL1_GAME_INTRO_RENDER_VIEW_H

#include "app/game_session.h"
#include "app/intro_flow.h"
#include "platform/sdl1/app_asset_types.h"

typedef struct GameIntroRenderView {
    int active;
    Rich4IntroStage stage;
    int characters[GAME_MAX_PLAYERS];
    int player_count;
} GameIntroRenderView;

void build_game_intro_render_view(const GamePreviewAssets *assets,
                                   const GameSession *session,
                                   GameIntroRenderView *out_view);

#endif
