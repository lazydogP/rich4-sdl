#include <SDL/SDL.h>

#include "app/game_speech_flow.h"
#include "app/game_session.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/game_flow_api.h"

int game_update_property_speech(GameSession *session,
                                Rich4Audio *audio, Uint32 now)
{
    Rich4SpeechAudioRequest request;
    if (!rich4_speech_prepare_audio(session, &request)) return 0;
    if (request.stop)
        rich4_audio_stop_speech(audio);
    else if (request.start_resource >= 0)
        rich4_audio_play_speech(audio, request.start_resource);
    return rich4_speech_finish_update(
        session, now, rich4_audio_speech_playing(audio));
}
