#include "platform/sdl1/game_lazy_assets.h"
#include "app/point_minigame_flow.h"
#include "app/game_session.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_audio_events.h"

#include <string.h>

static int game_point_minigame_mask_cell_from_assets(
    const GamePreviewAssets *assets, int x, int y)
{
    int encoded;
    int column;
    int row;
    if (assets == NULL || !game_asset_ready(assets, &assets->ui.has_panel_raw[81]) ||
        x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H)
        return -1;
    encoded = assets->ui.panel_data[81][y * SCREEN_W + x];
    column = encoded % 9;
    row = encoded / 9;
    return rich4_point_minigame_cell_valid(column, row)
        ? row * 9 + column : -1;
}

static void game_point_minigame_catcher_bounds(
    const GameSession *session, const GamePreviewAssets *assets,
    Rich4PointMinigameCatcherBounds *bounds)
{
    Rich4SprSmpChunk chunk;
    int character;
    int resource;
    int chunks;
    int move_frames;
    int chunk_index;
    if (bounds == NULL) return;
    memset(bounds, 0, sizeof(*bounds));
    if (session == NULL || assets == NULL ||
        session->presentation.point_minigame.player < 0 ||
        session->presentation.point_minigame.player >= session->state.player_count ||
        session->presentation.point_minigame.catcher_direction <= 0)
        return;
    character = session->state.players[
        session->presentation.point_minigame.player].character;
    resource = 100 + character;
    if (resource < 100 || resource >= GAME_PANEL_RESOURCE_COUNT ||
        !game_asset_ready(assets, &assets->ui.has_panel[resource]))
        return;
    chunks = (int)assets->ui.panel_spr[resource].num_chunks;
    move_frames = chunks > 5 ? (chunks - 5) / 2 : 0;
    chunk_index = move_frames > 0
        ? 5 + (session->presentation.point_minigame.catcher_direction - 1) *
              move_frames +
              session->presentation.point_minigame.catcher_frame % move_frames
        : 0;
    if (rich4_spr_smp_get_chunk(
            assets->ui.panel_data[resource], assets->ui.panel_size[resource],
            &assets->ui.panel_spr[resource], (size_t)chunk_index,
            &chunk) != RICH4_SPR_SMP_OK)
        return;
    bounds->valid = 1;
    bounds->left = session->presentation.point_minigame.catcher_x - chunk.x;
    bounds->top = 380 - chunk.y;
    bounds->right = bounds->left + chunk.width;
    bounds->bottom = bounds->top + chunk.height;
}

void game_begin_point_minigame(
    GameSession *session, const GameEvent *event,
    Rich4Audio *audio, Uint32 now, int animation_enabled)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_point_minigame_begin(
        session, event, now, animation_enabled, audio_events);
    game_session_audio_flush(session, audio);
}

int game_update_point_minigame(
    GameSession *session, const GamePreviewAssets *assets,
    Rich4Audio *audio, Uint32 now, int cursor_x)
{
    Rich4AudioEventQueue *audio_events;
    Rich4PointMinigameCatcherBounds bounds;
    int frames = 0, delay = 0;
    int active;
    memset(&bounds, 0, sizeof(bounds));
    /* 0x415457 shows the automatic result on the map, without entering
     * the interactive callbacks or loading their Panel[78..111] assets. */
    if (session != NULL && session->presentation.point_minigame.active &&
        !session->presentation.point_minigame.automatic) {
        if (assets != NULL && game_asset_ready(assets, &assets->ui.has_panel_graph[78])) {
            frames = game_asset_graph_frames(assets, &assets->ui.panel_data[78], assets->ui.panel_size[78]);
            delay = game_asset_graph_delay(assets, &assets->ui.panel_data[78], assets->ui.panel_size[78]);
        }
        game_point_minigame_catcher_bounds(session, assets, &bounds);
    }
    audio_events = rich4_app_session_audio_borrow(session);
    now = rich4_sdl1_ticks();
    if (session != NULL && session->presentation.point_minigame.action == 8 &&
        session->presentation.point_minigame.active &&
        !session->presentation.point_minigame.automatic &&
        session->presentation.point_minigame.phase != RICH4_POINT_MINIGAME_RESULT &&
        !(SDL_GetAppState() & SDL_APPINPUTFOCUS)) {
        rich4_point_minigame_pause(session, now);
        return 1;
    }
    active = rich4_point_minigame_update(
        session, frames, delay, &bounds, now, cursor_x, audio_events);
    game_session_audio_flush(session, audio);
    return active;
}

int game_point_minigame_click(
    GameSession *session, const GamePreviewAssets *assets,
    Rich4Audio *audio, int x, int y, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    int cell = session != NULL && session->presentation.point_minigame.active &&
        !session->presentation.point_minigame.automatic &&
        session->presentation.point_minigame.phase == RICH4_POINT_MINIGAME_PLAY &&
        session->presentation.point_minigame.action == 6
        ? game_point_minigame_mask_cell_from_assets(assets, x, y) : -1;
    int active = rich4_point_minigame_click(
        session, cell,
        x, y, now, audio_events);
    game_session_audio_flush(session, audio);
    return active;
}
