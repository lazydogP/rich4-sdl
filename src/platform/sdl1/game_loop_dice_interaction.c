#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>

#include "app/dice_flow.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "view/view_constants.h"

int handle_game_dice_status_mouse_down(GameSession *session,
                                              const GamePreviewAssets *assets,
                                              int mouse_x,
                                              int mouse_y,
                                              Uint32 now)
{
    const unsigned char *mask_data = NULL;
    size_t mask_size = 0;
    if (assets == NULL) return 0;
    if (game_asset_ready(assets, &assets->ui.has_panel_mask[8])) {
        mask_data = assets->ui.panel_data[8];
        mask_size = assets->ui.panel_size[8];
    }
    return rich4_dice_status_mouse_down(
        session, mask_data, mask_size, mouse_x, mouse_y, now);
}

int game_sync_dice_action_cursor(
    GameSession *session, VirtualCursor *cursor)
{
    if (session == NULL || cursor == NULL ||
        !session->presentation.dice.input_refresh_active)
        return 0;
    return rich4_sdl1_actor_panel_cursor(
        session->presentation.dice.status_x, session->presentation.dice.status_y,
        &cursor->x, &cursor->y);
}

int game_handle_dice_shortcut_key_down(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    Rich4Audio *audio, Rich4AppKey key)
{
    int result;
    (void)assets;
    if (app_state != RICH4_SCENE_GAME || session == NULL ||
        settings == NULL || key != RICH4_APP_KEY_D)
        return 0;
    result = rich4_dice_shortcut_key_down(
        session, key, 1, rich4_sdl1_ticks());
    if (result == 2)
        rich4_audio_play_select(audio);
    return result != 0;
}
