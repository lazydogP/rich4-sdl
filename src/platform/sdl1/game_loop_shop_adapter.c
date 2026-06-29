#include <SDL/SDL.h>

#include "app/shop_flow.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/runtime.h"

void game_begin_shop_visit(GameSession *session, const GameEvent *event,
                           Rich4Audio *audio, Uint32 now,
                           int animation_enabled)
{
    Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
    rich4_shop_begin(session, event, now, animation_enabled, events);
    game_session_audio_flush(session, audio);
}

int game_update_shop_visit(GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
    int active = rich4_shop_update(
        session, now, rich4_audio_speech_playing(audio), events);
    game_session_audio_flush(session, audio);
    return active;
}

int game_shop_skip_message(GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
    int handled = rich4_shop_skip_message(session, now, events);
    game_session_audio_flush(session, audio);
    return handled;
}

void game_shop_mouse_button_up(
    GameSession *session, Rich4Audio *audio, int released_control,
    int x, int y, VirtualCursor *cursor)
{
    Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
    rich4_shop_pointer_up(
        session, released_control, x, y, rich4_sdl1_ticks(), events);
    game_session_audio_flush(session, audio);
    if (cursor != NULL) {
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
    }
}

void game_shop_right_button_up(
    GameSession *session, Rich4Audio *audio,
    int *shop_music_restore_pending)
{
    Rich4AudioEventQueue *events;
    if (session == NULL || !session->presentation.shop_visit.active ||
        session->presentation.shop_visit.phase == RICH4_SHOP_OWNER_GIFT ||
        session->presentation.shop_visit.phase == RICH4_SHOP_OWNER_SPEECH)
        return;
    if (shop_music_restore_pending != NULL)
        *shop_music_restore_pending =
            session->presentation.shop_visit.music_started;
    events = rich4_app_session_audio_borrow(session);
    rich4_shop_cancel(session, events);
    game_session_audio_flush(session, audio);
}

int game_shop_apply_transaction(
    GameSession *session, Rich4Audio *audio, int hit, Uint32 now)
{
    Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
    int applied = rich4_shop_apply_transaction(session, hit, now, events);
    game_session_audio_flush(session, audio);
    return applied;
}

void game_shop_switch_page(GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
    rich4_shop_switch_page(session, now, events);
    game_session_audio_flush(session, audio);
}

void game_shop_begin_exit(GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
    rich4_shop_begin_exit(session, now, events);
    game_session_audio_flush(session, audio);
}
