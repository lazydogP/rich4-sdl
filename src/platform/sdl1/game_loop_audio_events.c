/* R10.1d: pending session-sound bridge moved verbatim out of
 * runtime_main.c. The queue remains the shared app/audio_event contract. */

#include "app/audio_event.h"
#include "platform/sdl1/game_loop_audio_events.h"

void game_emit_session_audio_events(
    GameSession *session,
    const GamePreviewAssets *assets,
    const NewGameSettings *settings,
    Rich4Audio *audio)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    int background = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    const MapData *movement_map =
        background >= 0 && background < GAME_BACKGROUND_COUNT
            ? &assets->map.maps[background] : NULL;
    rich4_app_collect_session_audio_events(
        session, movement_map, audio_events);
    game_session_audio_flush(session, audio);
}

void game_session_audio_flush(GameSession *session, Rich4Audio *audio)
{
    Rich4AudioEventQueue *queue = rich4_app_session_audio_release(session);
    Rich4AudioEvent event;
    while (rich4_audio_event_pop(queue, &event))
        rich4_audio_consume_event(audio, &event);
}
