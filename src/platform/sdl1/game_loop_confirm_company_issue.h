#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_COMPANY_ISSUE_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_COMPANY_ISSUE_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/app_command.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "platform/sdl1/app_asset_types.h"

/* R10.1d: company-issue confirmation/quantity dispatch moved out of
 * runtime_main.c. */
typedef struct GameConfirmCompanyIssueContext {
    Rich4SceneId app_state;
    GameSession *session;
    const GamePreviewAssets *assets;
    VirtualCursor *cursor;
    Rich4Audio *audio;
} GameConfirmCompanyIssueContext;

/* Returns non-zero when company issue owns confirm. */
int game_handle_confirm_company_issue(
    GameConfirmCompanyIssueContext *context);
int game_handle_insurance_charge_input(GameSession *session,
    const GamePreviewAssets *assets, int button, int x, int y, uint32_t now);
int game_handle_fortune_choice_input(GameSession *session,
    const GamePreviewAssets *assets, int button, int x, int y, uint32_t now);
int game_fortune_choice_at(GameSession *session, const GamePreviewAssets *assets,
    int x, int y);
int game_fortune_choice_press(GameSession *session, const GamePreviewAssets *assets,
    int button, int x, int y);
/* action 0 down, 1 motion, 2 up; button 3 cancels on release. */
int game_company_issue_pointer(GameSession *session, const GamePreviewAssets *assets,
    Rich4Audio *audio, int action, int button, int x, int y);

/* Returns non-zero when company-issue quantity entry consumes the keydown. */
int game_handle_company_issue_key_down(
    Rich4SceneId app_state, GameSession *session, VirtualCursor *cursor,
    Rich4Audio *audio, Rich4AppKey key, Rich4AppKey confirm_key,
    Rich4AppKey cancel_key);

#endif
