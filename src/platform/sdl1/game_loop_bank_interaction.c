#include "platform/sdl1/game_lazy_assets.h"
#include "platform/sdl1/game_loop_input.h"

#include "app/bank_interaction_flow.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/scene.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/runtime_options_types.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/runtime.h"

/* SDL adapter for the app-owned bank interaction flow. It supplies raw panel
 * bytes and the clock, applies returned commands through the existing command
 * presentation bridge, and owns only bank sounds/cursor reset. */

static Rich4BankInteractionMask bank_mask(const GamePreviewAssets *assets)
{
    Rich4BankInteractionMask mask;
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

int game_bank_interaction_hit(const GameSession *session,
                              const GamePreviewAssets *assets,
                              int x, int y)
{
    Rich4BankInteractionMask mask = bank_mask(assets);
    return rich4_app_bank_interaction_hit(session, &mask, x, y);
}

int game_bank_panel_apply_control(GameSession *session, int control,
                                  int pointer_x, GameCommand *out_command)
{
    return rich4_app_bank_interaction_apply_control(
        session, control, pointer_x, rich4_sdl1_ticks(), out_command);
}

int game_bank_interaction_release(GameSession *session, int control,
                                  int pointer_x, GameCommand *out_command)
{
    return rich4_app_bank_interaction_release(
        session, control, pointer_x, rich4_sdl1_ticks(), out_command);
}

void game_bank_interaction_mouse_button_up(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, int control, int pointer_x,
    VirtualCursor *cursor)
{
    GameCommand command;
    if (session == NULL || control < 0) return;
    if (game_bank_interaction_release(session, control, pointer_x, &command))
        game_session_apply_command(
            session, assets, settings, &command, rich4_sdl1_ticks());
    if (cursor != NULL) {
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
    }
}

void game_bank_play_press_sound(Rich4Audio *audio, int control)
{
    if (control == 0 || control == 1)
        rich4_audio_play_ok(audio);
    else if (control == 3)
        rich4_audio_play_bank_slider(audio);
    else
        rich4_audio_play_bank_key(audio);
}

int game_handle_bank_interaction_key_down(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    Rich4Audio *audio, Rich4AppKey key)
{
    int transfer_page;
    int control;
    GameCommand command;
    if (app_state != RICH4_SCENE_GAME || session == NULL ||
        assets == NULL || settings == NULL)
        return 0;
    transfer_page = rich4_app_bank_interaction_is_transfer(session);
    if (!transfer_page &&
        !rich4_app_bank_interaction_is_loan_page(session)) return 0;
    control = transfer_page
        ? rich4_app_bank_transfer_key_control(key)
        : rich4_app_bank_loan_key_control(key);
    if (control < 0 || (!transfer_page &&
                        !rich4_app_bank_interaction_is_amount_entry(session)))
        return 0;
    if (transfer_page)
        game_bank_play_press_sound(audio, control);
    else
        rich4_audio_play_bank_key(audio);
    if (transfer_page && control == 3) {
        rich4_app_bank_interaction_apply_control(
            session, control, 220, rich4_sdl1_ticks(), &command);
    } else if (rich4_app_bank_interaction_release(
                   session, control, 0, rich4_sdl1_ticks(), &command)) {
        game_session_apply_command(
            session, assets, settings, &command, rich4_sdl1_ticks());
    }
    return 1;
}
