#include "platform/sdl1/game_lazy_assets.h"
#include <string.h>

#include "app/game_session.h"
#include "app/intro_flow.h"
#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"

/* SDL adapter: convert resource graph metadata and the selected MapData into
 * the app-owned intro timeline contract. Rendering remains in the platform
 * intro ViewModel/renderer modules. */

static int game_intro_actor_frame_count(const GamePreviewAssets *assets,
                                        const GameSession *session,
                                        int phase,
                                        int player_index)
{
    int character;

    if (assets == NULL || session == NULL || phase < 0 || phase >= 2 ||
        player_index < 0 || player_index >= session->state.player_count)
        return 1;
    character = rich4_intro_jump_character_index(
        &session->state.players[player_index], player_index);
    {
        int frames = game_asset_graph_frames(assets, &assets->character.jump_intro_actor_data[phase][character], assets->character.jump_intro_actor_size[phase][character]);
        return frames > 0 ? frames : 1;
    }
}

static int game_intro_actor_delay_ms(const GamePreviewAssets *assets,
                                     const GameSession *session,
                                     int phase,
                                     int player_index)
{
    int character;

    if (assets == NULL || session == NULL || phase < 0 || phase >= 2 ||
        player_index < 0 || player_index >= session->state.player_count)
        return GAME_INTRO_DEFAULT_FRAME_MS;
    character = rich4_intro_jump_character_index(
        &session->state.players[player_index], player_index);
    return game_asset_graph_delay(assets, &assets->character.jump_intro_actor_data[phase][character], assets->character.jump_intro_actor_size[phase][character]);
}

static int game_intro_window_frame_count(const GamePreviewAssets *assets)
{
    if (assets == NULL)
        return 1;
    {
        int frames = game_asset_graph_frames(assets, &assets->intro.jump_intro_window_data, assets->intro.jump_intro_window_size);
        return frames > 0 ? frames : 1;
    }
}

static int game_intro_window_delay_ms(const GamePreviewAssets *assets)
{
    if (assets == NULL)
        return GAME_INTRO_DEFAULT_FRAME_MS;
    return game_asset_graph_delay(assets, &assets->intro.jump_intro_window_data, assets->intro.jump_intro_window_size);
}

static void game_intro_build_timing(const GamePreviewAssets *assets,
                                    const GameSession *session,
                                    Rich4IntroTiming *out_timing)
{
    int phase;
    int player;

    memset(out_timing, 0, sizeof(*out_timing));
    out_timing->window_frame_count = game_intro_window_frame_count(assets);
    out_timing->window_delay_ms = game_intro_window_delay_ms(assets);
    for (phase = 0; phase < 2; phase++) {
        for (player = 0; player < GAME_MAX_PLAYERS; player++) {
            out_timing->actor_frame_count[phase][player] =
                game_intro_actor_frame_count(assets, session, phase, player);
            out_timing->actor_delay_ms[phase][player] =
                game_intro_actor_delay_ms(assets, session, phase, player);
        }
    }
}

Rich4IntroStage game_intro_stage_for_frame(const GamePreviewAssets *assets,
                                           const GameSession *session,
                                           int intro_frame)
{
    Rich4IntroTiming timing;
    game_intro_build_timing(assets, session, &timing);
    return rich4_intro_stage_for_frame(&timing, session, intro_frame);
}

int game_intro_total_frames(const GamePreviewAssets *assets,
                            const GameSession *session)
{
    Rich4IntroTiming timing;
    game_intro_build_timing(assets, session, &timing);
    return rich4_intro_total_frames(&timing, session);
}

void game_update_intro_animation(GameSession *session,
                                 const GamePreviewAssets *assets,
                                 const NewGameSettings *settings,
                                 Uint32 now)
{
    Rich4IntroTiming timing;
    const MapData *map = NULL;
    int background;

    if (session == NULL || !session->presentation.intro.active) return;
    game_intro_build_timing(assets, session, &timing);
    if (assets != NULL && settings != NULL) {
        background = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
        if (background >= 0 && background < GAME_BACKGROUND_COUNT)
            map = &assets->map.maps[background];
    }
    now = rich4_sdl1_ticks();
    rich4_intro_update(session, &timing, map, now);
    if (session->presentation.intro.active) {
        Rich4IntroStage stage = rich4_intro_stage_for_frame(
            &timing, session, session->presentation.intro.frame);
        rich4_sdl1_wake_at(session->presentation.intro.tick +
                          (uint32_t)stage.delay_ms);
    }
}
