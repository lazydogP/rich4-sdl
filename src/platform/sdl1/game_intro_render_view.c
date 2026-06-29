#include <string.h>

#include "app/game_session.h"
#include "app/intro_flow.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_intro_render_view.h"
#include "platform/sdl1/game_asset_api.h"

void build_game_intro_render_view(const GamePreviewAssets *assets,
                                   const GameSession *session,
                                   GameIntroRenderView *out_view)
{
    int i;

    memset(out_view, 0, sizeof(*out_view));
    if (session == NULL || !session->state.active || !session->presentation.intro.active) return;
    out_view->active = 1;
    out_view->player_count = session->state.player_count;
    if (out_view->player_count > GAME_MAX_PLAYERS) out_view->player_count = GAME_MAX_PLAYERS;
    out_view->stage = game_intro_stage_for_frame(
        assets, session, session->presentation.intro.frame);
    for (i = 0; i < out_view->player_count; i++)
        out_view->characters[i] = rich4_intro_jump_character_index(
            &session->state.players[i], i);
}
