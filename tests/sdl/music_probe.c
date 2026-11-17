/* Original-resource music integration probe. Not part of the game bundle. */
#include <SDL/SDL.h>
#undef main
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/audio/audio_sdl_mixer_internal.h"

int main(int argc, char **argv)
{
    Rich4Audio *audio = NULL;
    int loaded = 0, result = 1, wait_ms = 120, finish = 0;
    int restore_position = 0;
    double position = 0.0;
    Uint32 started;
    if (argc < 2) return 2;
    SDL_putenv("SDL_AUDIODRIVER=dummy");
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0 || !rich4_audio_init(&audio)) {
        fprintf(stderr, "audio init: %s\n", Mix_GetError()); return 1;
    }
    printf("MIDI available=%d\n", rich4_audio_supports_midi(audio));
    if (!strcmp(argv[1], "capabilities")) { result = 0; goto done; }
    if (argc == 4 && !strcmp(argv[1], "fixed"))
        loaded = start_fixed_music_once(audio, argv[2], (size_t)strtoul(argv[3], NULL, 10), "probe");
    else if (argc == 5 && !strcmp(argv[1], "bank"))
        loaded = start_music_bank_track(audio, argv[2], (Rich4MusicBank)atoi(argv[3]),
                                        (size_t)strtoul(argv[4], NULL, 10), "probe");
    else if (argc == 4 && (!strcmp(argv[1], "play") || !strcmp(argv[1], "finish") || !strcmp(argv[1], "loop"))) {
        wait_ms = atoi(argv[3]); finish = !strcmp(argv[1], "finish");
        loaded = !strcmp(argv[1], "loop") ? rich4_audio_play_music(audio, argv[2])
                                          : rich4_audio_play_music_once(audio, argv[2]);
    } else if (argc == 4 && !strcmp(argv[1], "restore")) {
        position = atof(argv[3]);
        restore_position = 1;
        loaded = rich4_audio_play_music_once(audio, argv[2]);
    } else { result = 2; goto done; }
    if (!loaded) { fprintf(stderr, "music load/play: %s\n", Mix_GetError()); goto done; }
    {
        Mix_MusicType type = Mix_GetMusicType(audio->music);
        printf("MUSIC type=%s\n", type == MUS_MID ? "MID" : type == MUS_OGG ? "OGG" :
                                  type == MUS_WAV ? "WAV" : "OTHER");
    }
    if (restore_position) {
        audio->saved_music_position = position;
        audio->saved_music_position_valid = 1;
        if (!rich4_audio_restore_music_position(audio)) {
            fprintf(stderr, "music restore: %s\n", Mix_GetError());
            goto done;
        }
        printf("MUSIC restored=%.3f\n", position);
    }
    Mix_VolumeMusic(MIX_MAX_VOLUME / 2);
    if (Mix_VolumeMusic(-1) != MIX_MAX_VOLUME / 2) goto done;
    Mix_VolumeMusic(MIX_MAX_VOLUME);
    started = SDL_GetTicks();
    while ((int)(SDL_GetTicks() - started) < wait_ms) {
        int playing = rich4_audio_music_playing(audio);
        if (finish && !playing) { printf("MUSIC finished\n"); result = 0; goto done; }
        if (!playing) { fprintf(stderr, "music ended prematurely\n"); goto done; }
        SDL_Delay(20);
    }
    if (finish) { fprintf(stderr, "music did not finish before deadline\n"); goto done; }
    rich4_audio_stop_music(audio);
    if (rich4_audio_music_playing(audio)) { fprintf(stderr, "music did not stop\n"); goto done; }
    printf("MUSIC stopped\n"); result = 0;
done:
    rich4_audio_shutdown(audio); SDL_Quit(); return result;
}
