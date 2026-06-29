#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_INPUT_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/app_command.h"
#include "app/bank_interaction_flow.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "core/game_command.h"
#include "platform/sdl1/app_asset_types.h"

/* R10.1d: in-game bank panel input sub-cluster (pure input-to-command
 * translation for the withdraw/deposit transfer page and the loan/special-
 * finance page), moved verbatim. All of these are called from
 * rich4_sdl1_main's mouse/keyboard handling (still in runtime_main.c). The
 * loan-page control ids are a shared contract between the hit-test/apply
 * helpers here and rich4_sdl1_main's manager-page confirm-dialog dispatch. */

int game_bank_interaction_hit(const GameSession *session,
                              const GamePreviewAssets *assets,
                              int x, int y);

int game_bank_panel_apply_control(GameSession *session,
                                  int control,
                                  int pointer_x,
                                  GameCommand *out_command);

int game_bank_interaction_release(GameSession *session,
                                  int control,
                                  int pointer_x,
                                  GameCommand *out_command);

void game_bank_interaction_mouse_button_up(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings,
    int control, int pointer_x, VirtualCursor *cursor);

void game_bank_play_press_sound(Rich4Audio *audio, int control);

int game_handle_bank_interaction_key_down(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    Rich4Audio *audio, Rich4AppKey key);

#endif
