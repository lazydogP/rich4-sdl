#include <SDL/SDL.h>

#include "app/roulette_flow.h"
#include "app/game_session.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/game_loop_audio_events.h"

void game_begin_facility_roulette(
    GameSession *session, const GameEvent *event,
    Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_roulette_begin(session, event, now, audio_events);
    game_session_audio_flush(session, audio);
}

void game_stop_facility_roulette(
    GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_roulette_stop(session, now, audio_events);
    game_session_audio_flush(session, audio);
}

int game_update_facility_roulette(
    GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    int active = rich4_roulette_update(session, now, audio_events);
    game_session_audio_flush(session, audio);
    return active;
}
