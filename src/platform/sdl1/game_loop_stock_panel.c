#include "platform/sdl1/game_lazy_assets.h"
#include <SDL/SDL.h>

#include "app/app_command.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "app/stock_interaction_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_render_api.h"

/* SDL adapter for the app-owned stock interaction. It owns only the
 * render-cache lifetime and translates app result cues to the audio backend. */

static void play_stock_audio(Rich4Audio *audio,
                             Rich4AppStockAudioCue cue)
{
    if (audio == NULL) return;
    switch (cue) {
    case RICH4_APP_STOCK_AUDIO_SELECT:
        rich4_audio_play_select(audio);
        break;
    case RICH4_APP_STOCK_AUDIO_CANCEL:
        rich4_audio_play_cancel(audio);
        break;
    case RICH4_APP_STOCK_AUDIO_SLIDER:
        rich4_audio_play_bank_slider(audio);
        break;
    case RICH4_APP_STOCK_AUDIO_KEY:
        rich4_audio_play_bank_key(audio);
        break;
    case RICH4_APP_STOCK_AUDIO_OK:
        rich4_audio_play_ok(audio);
        break;
    default:
        break;
    }
}

static void apply_stock_result(
    StockPanelState *panel,
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio,
    Rich4AppStockInteractionResult *result)
{
    if (panel != NULL) panel->render_dirty = 1;
    if (session == NULL || result == NULL) return;
    if (result->command_ready) {
        int succeeded = game_session_apply_command(
            session, assets, settings, &result->command,
            rich4_sdl1_ticks());
        if (succeeded)
            result->audio_cue = (Rich4AppStockAudioCue)
                rich4_app_stock_interaction_command_applied(session, 1);
    }
    play_stock_audio(audio, result->audio_cue);
}

static Rich4StockQuantityMask stock_quantity_mask(
    const GamePreviewAssets *assets)
{
    Rich4StockQuantityMask mask;
    mask.data = NULL;
    mask.size = 0;
    mask.available = 0;
    if (assets != NULL && game_asset_ready(assets, &assets->ui.has_panel_raw[22]) &&
        assets->ui.panel_data[22] != NULL) {
        mask.data = assets->ui.panel_data[22];
        mask.size = assets->ui.panel_size[22];
        mask.available = 1;
    }
    return mask;
}

void game_open_stock_panel(StockPanelState *panel, GameSession *session)
{
    if (panel != NULL) panel->render_dirty = 1;
    rich4_app_stock_interaction_open(session);
}

void game_close_stock_panel(StockPanelState *panel, GameSession *session)
{
    if (panel != NULL) panel->render_dirty = 1;
    rich4_app_stock_interaction_close(session);
}

void game_destroy_stock_panel(StockPanelState *panel)
{
    if (panel == NULL) return;
    if (panel->render_cache != NULL)
        SDL_FreeSurface(panel->render_cache);
    panel->render_cache = NULL;
}

void game_stock_panel_cancel(StockPanelState *panel, GameSession *session)
{
    if (panel != NULL) panel->render_dirty = 1;
    rich4_app_stock_interaction_cancel(session);
}

void game_stock_panel_mouse_down(
    StockPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, int x, int y)
{
    Rich4StockQuantityMask mask = stock_quantity_mask(assets);
    if (panel != NULL) panel->render_dirty = 1;
    rich4_app_stock_interaction_mouse_down_with_mask(
        session, &mask, x, y);
}

void game_stock_panel_mouse_motion(
    StockPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, Rich4Audio *audio, int x, int y)
{
    Rich4AppStockInteractionResult result;
    Rich4StockQuantityMask mask = stock_quantity_mask(assets);
    if (panel != NULL) panel->render_dirty = 1;
    rich4_app_stock_interaction_mouse_motion(
        session, &mask, x, y, &result);
    play_stock_audio(audio, result.audio_cue);
}

void game_stock_panel_mouse_release(
    StockPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    Rich4Audio *audio, int button, int x, int y)
{
    Rich4AppStockInteractionResult result;
    Rich4StockQuantityMask mask = stock_quantity_mask(assets);
    if (panel != NULL) panel->render_dirty = 1;
    rich4_app_stock_interaction_mouse_release_with_mask(
        session, &mask, button, x, y, &result);
    apply_stock_result(panel, session, assets, settings, audio, &result);
}

void game_stock_panel_key_down(
    StockPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    Rich4Audio *audio, Rich4AppKey key)
{
    Rich4AppStockInteractionResult result;
    if (panel != NULL) panel->render_dirty = 1;
    rich4_app_stock_interaction_key_down(session, key, &result);
    apply_stock_result(panel, session, assets, settings, audio, &result);
}
