#ifndef RICH4_PLATFORM_SDL1_RUNTIME_AUDIO_BOOTSTRAP_H
#define RICH4_PLATFORM_SDL1_RUNTIME_AUDIO_BOOTSTRAP_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "resource/mkf.h"

/* R10.1d: audio device/bootstrap loading moved out of runtime_main.c.
 * The caller retains ownership of the Rich4Audio object and MKF handles. */
typedef struct RuntimeAudioBootstrapContext {
    Rich4Audio **audio;
    Rich4Mkf *effect_mkf;
    Rich4Mkf *speaking_mkf;
} RuntimeAudioBootstrapContext;

void game_init_runtime_audio(RuntimeAudioBootstrapContext *context);

#endif
