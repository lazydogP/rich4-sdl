#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_AUDIO_EVENTS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_AUDIO_EVENTS_H

#include "app/audio_event.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"

/* R10.1d: bridge pending GameSession presentation sounds into the shared
 * audio-event queue, moved verbatim out of runtime_main.c.  Borrows the
 * session queue for the duration of the bridge like every other interaction. */
void game_emit_session_audio_events(
    GameSession *session,
    const GamePreviewAssets *assets,
    const NewGameSettings *settings,
    Rich4Audio *audio);

/* Ends the borrow taken by rich4_app_session_audio_borrow and hands the queued
   events to the mixer in emit order.  A nested borrow defers the drain to the
   enclosing interaction instead of consuming its events early. */
void game_session_audio_flush(GameSession *session, Rich4Audio *audio);

#endif
