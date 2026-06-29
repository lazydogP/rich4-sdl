#include <assert.h>
#include <stdio.h>
#include <SDL/SDL.h>
#undef main
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/audio/audio_sdl_mixer_internal.h"

int main(void)
{
    static unsigned char samples[4096]; /* About 46 ms at the mixer format. */
    static unsigned char finance_samples[88200];
    Rich4Audio *audio = NULL;
    Rich4AudioEvent event = {RICH4_AUDIO_EVENT_MAGIC_FINALE, 1, 59};
    int method, bomb_channel;
    SDL_putenv("SDL_AUDIODRIVER=dummy");
    assert(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0);
    assert(rich4_audio_init(&audio));
    for (method = 0; method < RICH4_MOVEMENT_SOUND_COUNT; ++method) {
        audio->movement[method] = Mix_QuickLoad_RAW(samples, sizeof(samples));
        assert(audio->movement[method] != NULL);
    }
    audio->attached_bomb = Mix_QuickLoad_RAW(samples, sizeof(samples));
    audio->magic_finale = Mix_QuickLoad_RAW(samples, sizeof(samples));
    audio->confinement_visit = Mix_QuickLoad_RAW(samples, sizeof(samples));
    audio->special_place = Mix_QuickLoad_RAW(samples, sizeof(samples));
    for (method = 0; method < 2; ++method)
        audio->special_reward_animation[method] = Mix_QuickLoad_RAW(samples, sizeof(samples));
    assert(audio->confinement_visit && audio->special_place &&
           audio->special_reward_animation[0] && audio->special_reward_animation[1]);
    assert(audio->attached_bomb && audio->magic_finale);
    audio->finance_effect[0]=Mix_QuickLoad_RAW(finance_samples,sizeof(finance_samples));
    audio->finance_effect[5]=Mix_QuickLoad_RAW(finance_samples,sizeof(finance_samples));
    assert(audio->finance_effect[0] && audio->finance_effect[5]);
    rich4_audio_update_attached_bomb(audio, 1);
    bomb_channel = audio->attached_bomb_channel;
    assert(bomb_channel >= 0);
    for (method = 0; method < RICH4_MOVEMENT_SOUND_COUNT; ++method) {
        int channel;
        rich4_audio_update_movement(audio, 1, method);
        channel = audio->movement_channel;
        assert(channel >= 0 && channel != bomb_channel);
        SDL_Delay(160); /* Several complete samples, without another update. */
        assert(Mix_Playing(channel) && Mix_Playing(bomb_channel));
        assert(Mix_GetChunk(channel) == audio->movement[method]);
        rich4_audio_update_movement(audio, 1, method);
        rich4_audio_update_attached_bomb(audio, 1);
        assert(audio->movement_channel == channel);
        assert(audio->attached_bomb_channel == bomb_channel);
    }
    /* Transfer: only ticking stops. */
    rich4_audio_update_attached_bomb(audio, 0);
    assert(!Mix_Playing(bomb_channel));
    assert(Mix_Playing(audio->movement_channel));
    /* Roadblock: movement stops before the independent bomb stop. */
    rich4_audio_update_attached_bomb(audio, 1);
    rich4_audio_update_movement(audio, 0, 0);
    assert(Mix_Playing(audio->attached_bomb_channel));
    rich4_audio_update_attached_bomb(audio, 0);
    assert(Mix_Playing(-1) == 0);
    rich4_audio_consume_event(audio, &event);
    assert(Mix_Playing(-1) == 1);
    SDL_Delay(160);
    assert(Mix_Playing(-1) == 0); /* Finale is a one-shot. */
    /* 0x475299 is a slot table. Actions 2..9/14..16 select the
     * Effect[43] chunk; 10..13 select Effect[48], once per entry. */
    for (int action = 1; action <= 16; ++action) {
        Rich4AudioEvent entry = {RICH4_AUDIO_EVENT_SPECIAL_PLACE_ENTRY, 1, action};
        Mix_Chunk *expected = action >= 10 && action <= 13
            ? audio->special_place : audio->confinement_visit;
        int playing = 0;
        rich4_audio_consume_event(audio, &entry);
        for (int channel = 0; channel < Mix_AllocateChannels(-1); ++channel)
            if (Mix_Playing(channel)) {
                assert(Mix_GetChunk(channel) == expected);
                playing++;
            }
        assert(playing == (action == 1 ? 0 : 1));
        SDL_Delay(160);
        assert(Mix_Playing(-1) == 0);
    }
    for (int resource = 98; resource <= 99; ++resource) {
        Rich4AudioEvent reward = {RICH4_AUDIO_EVENT_SPECIAL_REWARD_ANIMATION, 1, resource};
        rich4_audio_consume_event(audio, &reward);
        assert(Mix_Playing(-1) == 1);
        SDL_Delay(160);
        assert(Mix_Playing(-1) == 0);
    }
    {
        int report=-1,bank=-1;
        rich4_audio_update_movement(audio,1,0);
        rich4_audio_play_finance_effect(audio,61);
        rich4_audio_play_finance_effect(audio,27);
        for (int c=0;c<Mix_AllocateChannels(-1);++c) {
            if (Mix_GetChunk(c)==audio->finance_effect[5]) report=c;
            if (Mix_GetChunk(c)==audio->finance_effect[0]) bank=c;
        }
        assert(report>=0 && bank>=0 && Mix_Playing(report) && Mix_Playing(bank));
        rich4_audio_stop_finance_effects(audio,61);
        assert(!Mix_Playing(report) && Mix_Playing(bank) && Mix_Playing(audio->movement_channel));
        rich4_audio_stop_finance_effects(audio,0);
        assert(!Mix_Playing(bank) && Mix_Playing(audio->movement_channel));
        rich4_audio_update_movement(audio,0,0);
    }
    rich4_audio_update_movement(audio, 1, 0);
    rich4_audio_update_attached_bomb(audio, 1);
    rich4_audio_set_effects_enabled(audio, 0);
    assert(Mix_Playing(-1) == 0);
    assert(audio->attached_bomb_channel == -1);
    rich4_audio_set_effects_enabled(audio, 1);
    rich4_audio_update_attached_bomb(audio, 1);
    assert(Mix_Playing(audio->attached_bomb_channel));
    rich4_audio_shutdown(audio);
    SDL_Quit();
    puts("SDL audio loops: ok");
    return 0;
}
