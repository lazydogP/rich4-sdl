#include "platform/sdl1/game_lazy_assets.h"
#include "app/lottery_flow.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"

static void game_apply_lottery_audio(
    Rich4Audio *audio, const Rich4LotteryAudioRequest *request)
{
    if (audio == NULL || request == NULL) return;
    if (request->stop_speech)
        rich4_audio_stop_speech(audio);
    switch (request->effect) {
    case RICH4_LOTTERY_AUDIO_VISIT:
        rich4_audio_play_confinement_visit(audio);
        break;
    case RICH4_LOTTERY_AUDIO_PURCHASE:
        rich4_audio_play_lottery_purchase(audio);
        break;
    case RICH4_LOTTERY_AUDIO_CANCEL:
        rich4_audio_play_cancel(audio);
        break;
    default:
        break;
    }
    if (request->start_speech_resource >= 0)
        rich4_audio_play_speech(audio, request->start_speech_resource);
}

void game_begin_lottery_visit(
    GameSession *session, const GamePreviewAssets *assets, const GameEvent *event,
    Rich4Audio *audio, Uint32 now, int animation_enabled)
{
    Rich4LotteryAudioRequest request;
    if (animation_enabled && assets != NULL) {
        (void)game_asset_ready(assets, &assets->ui.has_panel[13]);
        (void)game_asset_ready(assets, &assets->ui.has_panel_graph[14]);
    }
    now = rich4_sdl1_ticks();
    rich4_lottery_begin_visit(
        session, event, now, animation_enabled, &request);
    game_apply_lottery_audio(audio, &request);
    rich4_lottery_animation_prepared(session, rich4_sdl1_ticks());
}

int game_update_lottery_visit(
    GameSession *session, const GamePreviewAssets *assets,
    Rich4Audio *audio, Uint32 now)
{
    Rich4LotteryAudioRequest request;
    int frames = assets != NULL && game_asset_ready(assets, &assets->ui.has_panel_graph[14])
        ? game_asset_graph_frames(assets, &assets->ui.panel_data[14], assets->ui.panel_size[14])
        : 0;
    now = rich4_sdl1_ticks();
    int active = rich4_lottery_update(
        session, frames, now, rich4_audio_speech_playing(audio), &request);
    game_apply_lottery_audio(audio, &request);
    return active;
}

void game_cancel_lottery_visit(
    GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4LotteryAudioRequest request;
    rich4_lottery_cancel_visit(session, now, &request);
    game_apply_lottery_audio(audio, &request);
}

void game_lottery_right_button_up(
    GameSession *session, Rich4Audio *audio, Uint32 now,
    VirtualCursor *cursor)
{
    if (session == NULL || !session->presentation.lottery.active)
        return;
    game_cancel_lottery_visit(session, audio, now);
    if (cursor != NULL)
        cursor->cancel_pressed = 0;
}

void game_lottery_left_button_down(
    GameSession *session, Rich4Audio *audio,
    int pointer_x, int pointer_y, Uint32 now)
{
    Rich4LotteryAudioRequest request;
    rich4_lottery_left_button_down(
        session, pointer_x, pointer_y, now, &request);
    game_apply_lottery_audio(audio, &request);
}
