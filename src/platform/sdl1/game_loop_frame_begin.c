/* R10.1d: per-frame preparation moved verbatim out of runtime_main.c. */

#include "platform/sdl1/game_loop_frame_begin.h"
#include "platform/sdl1/game_loop_panels.h"

void game_begin_runtime_frame(
    VirtualCursor *cursor, Rich4SceneId app_state,
    const Rich4OptionsPanelState *options_panel,
    const Rich4Config *rich4_config, Rich4Audio *audio,
    CardPanelState *card_panel, GameSession *game_session,
    const GamePreviewAssets *game_preview_assets,
    NewGameSettings *new_game_settings, int mouse_left_down)
{
    if (cursor == NULL || options_panel == NULL || rich4_config == NULL ||
        audio == NULL || card_panel == NULL || game_session == NULL ||
        game_preview_assets == NULL || new_game_settings == NULL)
        return;
    cursor->confirm_pressed = 0;
    cursor->confirm_pointer_pressed = 0;
    cursor->cancel_pressed = 0;
    rich4_audio_set_effects_enabled(
        audio,
        app_state == RICH4_SCENE_OPTIONS
            ? options_panel->draft.sound_effect != 0
            : rich4_config->sound_effect != 0);
    game_update_card_panel_timed_effect(
        card_panel, game_session, game_preview_assets, new_game_settings,
        audio);
    if (!mouse_left_down) new_game_settings->pressed_control = -1;
}
