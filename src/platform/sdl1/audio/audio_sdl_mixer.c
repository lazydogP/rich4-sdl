#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/audio_event.h"
#include "resource/mkf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL_mixer.h>

#define AUDIO_RATE 22050
#define AUDIO_CHANNELS 2

#include "platform/sdl1/audio/audio_sdl_mixer_internal.h"

static Mix_Chunk *load_effect_resource(Rich4Mkf *effect_mkf,
                                       size_t resource_index);

int rich4_audio_init(Rich4Audio **out_audio)
{
    Rich4Audio *audio;

    if (out_audio == NULL) {
        return 0;
    }
    *out_audio = NULL;
    audio = (Rich4Audio *)calloc(1, sizeof(*audio));
    if (audio == NULL) {
        return 0;
    }

    if (Mix_OpenAudio(AUDIO_RATE, AUDIO_S16SYS, AUDIO_CHANNELS, 1024) != 0) {
        free(audio);
        return 0;
    }
    Mix_AllocateChannels(8);
    audio->ready = 1;
    audio->effects_enabled = 1;
    audio->movement_channel = -1;
    audio->attached_bomb_channel = -1;
    audio->movement_method = -1;
    audio->speech_channel = -1;
    audio->flc_effect_channel = -1;
    audio->tool_robot_channel = -1;
    audio->point_minigame_loop_channel = -1;
    audio->roulette_channel = -1;
    *out_audio = audio;
    return 1;
}

void rich4_audio_shutdown(Rich4Audio *audio)
{
    if (audio == NULL) {
        return;
    }
    if (audio->music != NULL) {
        Mix_HaltMusic();
        Mix_FreeMusic(audio->music);
    }
    if (audio->ok != NULL) {
        Mix_FreeChunk(audio->ok);
    }
    if (audio->select != NULL) {
        Mix_FreeChunk(audio->select);
    }
    if (audio->cancel != NULL) {
        Mix_FreeChunk(audio->cancel);
    }
    if (audio->invalid != NULL) Mix_FreeChunk(audio->invalid);
    if (audio->target_confirm != NULL) Mix_FreeChunk(audio->target_confirm);
    if (audio->new_game_transition != NULL)
        Mix_FreeChunk(audio->new_game_transition);
    if (audio->bank_key != NULL) {
        Mix_FreeChunk(audio->bank_key);
    }
    if (audio->bank_slider != NULL) {
        Mix_FreeChunk(audio->bank_slider);
    }
    if (audio->dice != NULL) {
        Mix_FreeChunk(audio->dice);
    }
    if (audio->jump != NULL) {
        Mix_FreeChunk(audio->jump);
    }
    if (audio->door != NULL) {
        Mix_FreeChunk(audio->door);
    }
    if (audio->property_purchase != NULL) {
        Mix_FreeChunk(audio->property_purchase);
    }
    if (audio->property_upgrade != NULL) {
        Mix_FreeChunk(audio->property_upgrade);
    }
    if (audio->property_toll != NULL) Mix_FreeChunk(audio->property_toll);
    if (audio->property_max_level != NULL)
        Mix_FreeChunk(audio->property_max_level);
    if (audio->card != NULL) Mix_FreeChunk(audio->card);
    if (audio->card_monster_animation != NULL)
        Mix_FreeChunk(audio->card_monster_animation);
    if (audio->card_demolish_animation != NULL)
        Mix_FreeChunk(audio->card_demolish_animation);
    for (int i = 0; i <= GAME_GOD_TYPE_COUNT; i++)
        if (audio->god_effect[i] != NULL)
            Mix_FreeChunk(audio->god_effect[i]);
    if (audio->special_place != NULL) Mix_FreeChunk(audio->special_place);
    for (int i = 0; i < 2; i++)
        if (audio->special_reward_animation[i] != NULL)
            Mix_FreeChunk(audio->special_reward_animation[i]);
    if (audio->god_departure != NULL) Mix_FreeChunk(audio->god_departure);
    if (audio->road_gift != NULL) Mix_FreeChunk(audio->road_gift);
    if (audio->road_chest != NULL) Mix_FreeChunk(audio->road_chest);
    if (audio->auction_action != NULL) Mix_FreeChunk(audio->auction_action);
    if (audio->auction_gavel != NULL) Mix_FreeChunk(audio->auction_gavel);
    for (int i = 0; i < 6; ++i)
        if (audio->finance_effect[i] != NULL) Mix_FreeChunk(audio->finance_effect[i]);
    if (audio->confinement_visit != NULL)
        Mix_FreeChunk(audio->confinement_visit);
    if (audio->lottery_purchase != NULL)
        Mix_FreeChunk(audio->lottery_purchase);
    if (audio->roulette_channel >= 0)
        Mix_HaltChannel(audio->roulette_channel);
    if (audio->roulette != NULL)
        Mix_FreeChunk(audio->roulette);
    if (audio->magic_hover != NULL)
        Mix_FreeChunk(audio->magic_hover);
    if (audio->magic_turn != NULL)
        Mix_FreeChunk(audio->magic_turn);
    if (audio->magic_finale != NULL)
        Mix_FreeChunk(audio->magic_finale);
    {
        int i;
        for (i = 0; i < 5; i++)
            if (audio->news_consequence[i] != NULL)
                Mix_FreeChunk(audio->news_consequence[i]);
        if (audio->point_minigame_loop_channel >= 0)
            Mix_HaltChannel(audio->point_minigame_loop_channel);
        for (i = 0; i < 9; i++)
            if (audio->point_minigame_effect[i] != NULL)
                Mix_FreeChunk(audio->point_minigame_effect[i]);
    }
    if (audio->tool_robot_channel >= 0)
        Mix_HaltChannel(audio->tool_robot_channel);
    for (int i = 0; i < 13; i++)
        if (audio->tool[i] != NULL) Mix_FreeChunk(audio->tool[i]);
    for (int i = 0; i < 12; i++)
        if (audio->tool_effect[i] != NULL)
            Mix_FreeChunk(audio->tool_effect[i]);
    rich4_audio_update_attached_bomb(audio, 0);
    if (audio->attached_bomb != NULL) Mix_FreeChunk(audio->attached_bomb);
    if (audio->speech_channel >= 0) Mix_HaltChannel(audio->speech_channel);
    if (audio->speech != NULL) Mix_FreeChunk(audio->speech);
    if (audio->movement_channel >= 0) {
        Mix_HaltChannel(audio->movement_channel);
    }
    for (int i = 0; i < RICH4_MOVEMENT_SOUND_COUNT; i++) {
        if (audio->movement[i] != NULL) {
            Mix_FreeChunk(audio->movement[i]);
        }
    }
    if (audio->ready) {
        Mix_CloseAudio();
    }
    free(audio);
}

int rich4_audio_load_button_sounds(Rich4Audio *audio, Rich4Mkf *effect_mkf)
{
    if (audio == NULL || !audio->ready || effect_mkf == NULL) {
        return 0;
    }
    if (audio->ok != NULL) {
        Mix_FreeChunk(audio->ok);
        audio->ok = NULL;
    }
    if (audio->select != NULL) {
        Mix_FreeChunk(audio->select);
        audio->select = NULL;
    }
    if (audio->cancel != NULL) {
        Mix_FreeChunk(audio->cancel);
        audio->cancel = NULL;
    }
    if (audio->invalid != NULL) {
        Mix_FreeChunk(audio->invalid);
        audio->invalid = NULL;
    }
    if (audio->target_confirm != NULL) {
        Mix_FreeChunk(audio->target_confirm);
        audio->target_confirm = NULL;
    }
    if (audio->new_game_transition != NULL) {
        Mix_FreeChunk(audio->new_game_transition);
        audio->new_game_transition = NULL;
    }
    if (audio->bank_key != NULL) {
        Mix_FreeChunk(audio->bank_key);
        audio->bank_key = NULL;
    }
    if (audio->bank_slider != NULL) {
        Mix_FreeChunk(audio->bank_slider);
        audio->bank_slider = NULL;
    }
    /* sub_4542CE sound structs 0x48231A/0x482322/0x48232A/0x482332
     * select Effect.mkf resources 0/1/2/4.  Setup timer 0x405D08 starts
     * the outbound character transition with 0x46CCD0, resource 5.
     * The bank callback 0x436EF8 also uses 0x48234A/0x482352,
     * resources 7/9 for keys and slider. */
    audio->select = load_effect_resource(effect_mkf, 0);
    audio->ok = load_effect_resource(effect_mkf, 1);
    audio->cancel = load_effect_resource(effect_mkf, 4);
    audio->invalid = load_effect_resource(effect_mkf, 3);
    audio->target_confirm = load_effect_resource(effect_mkf, 2);
    audio->new_game_transition = load_effect_resource(effect_mkf, 5);
    audio->bank_key = load_effect_resource(effect_mkf, 7);
    audio->bank_slider = load_effect_resource(effect_mkf, 9);
    return audio->ok != NULL || audio->select != NULL || audio->cancel != NULL ||
           audio->invalid != NULL || audio->target_confirm != NULL ||
           audio->new_game_transition != NULL ||
           audio->bank_key != NULL || audio->bank_slider != NULL;
}

int rich4_audio_load_movement_sounds(Rich4Audio *audio, Rich4Mkf *effect_mkf)
{
    /* 0x40D7C4 selects sound-table entries 11..15. Their Effect.mkf
     * resources are 44, 45, 46, 53 (traffic methods 0..3), and 47 for
     * the signed/high-bit water route state selected by sub_40B93B. */
    static const size_t resource_indices[] = { 44, 45, 46, 53, 47 };
    int loaded = 0;
    int i;

    if (audio == NULL || !audio->ready || effect_mkf == NULL) return 0;
    for (i = 0; i < RICH4_MOVEMENT_SOUND_COUNT; i++) {
        unsigned char *data = NULL;
        size_t size = 0;
        Mix_Chunk *chunk = NULL;
        if (audio->movement[i] != NULL) Mix_FreeChunk(audio->movement[i]);
        audio->movement[i] = NULL;
        if (rich4_mkf_read_resource(effect_mkf, resource_indices[i], &data, &size, NULL) == RICH4_MKF_OK &&
            data != NULL && size > 0 && size <= 0x7fffffffU) {
            SDL_RWops *rw = SDL_RWFromConstMem(data, (int)size);
            if (rw != NULL) chunk = Mix_LoadWAV_RW(rw, 1);
        }
        free(data);
        audio->movement[i] = chunk;
        if (chunk != NULL) loaded++;
    }
    return loaded;
}

int rich4_audio_load_dice_sound(Rich4Audio *audio, Rich4Mkf *effect_mkf)
{
    unsigned char *data = NULL;
    size_t size = 0;
    Mix_Chunk *chunk = NULL;

    /* 0x4195F4 selects sound_struct 0x48235A, whose resource id is 10.
     * 0x45138F plays it at Autodesk Animator FLC frame 30 and 0x41962F plays it again
     * after the blocking animation call returns. */
    if (audio == NULL || !audio->ready || effect_mkf == NULL) return 0;
    if (audio->dice != NULL) {
        Mix_FreeChunk(audio->dice);
        audio->dice = NULL;
    }
    if (rich4_mkf_read_resource(effect_mkf, 10, &data, &size, NULL) == RICH4_MKF_OK &&
        data != NULL && size > 0 && size <= 0x7fffffffU) {
        SDL_RWops *rw = SDL_RWFromConstMem(data, (int)size);
        if (rw != NULL) chunk = Mix_LoadWAV_RW(rw, 1);
    }
    free(data);
    audio->dice = chunk;
    return chunk != NULL;
}

int rich4_audio_load_jump_sound(Rich4Audio *audio, Rich4Mkf *effect_mkf)
{
    unsigned char *data = NULL;
    size_t size = 0;
    Mix_Chunk *chunk = NULL;

    /* 0x415B45 binds sound_struct 0x475100 (resource id 26) to the
     * takeoff FLC animation; flags 0x14000003 trigger it once at frame 20. */
    if (audio == NULL || !audio->ready || effect_mkf == NULL) return 0;
    if (audio->jump != NULL) {
        Mix_FreeChunk(audio->jump);
        audio->jump = NULL;
    }
    if (rich4_mkf_read_resource(effect_mkf, 26, &data, &size, NULL) == RICH4_MKF_OK &&
        data != NULL && size > 0 && size <= 0x7fffffffU) {
        SDL_RWops *rw = SDL_RWFromConstMem(data, (int)size);
        if (rw != NULL) chunk = Mix_LoadWAV_RW(rw, 1);
    }
    free(data);
    audio->jump = chunk;
    return chunk != NULL;
}

int rich4_audio_load_door_sound(Rich4Audio *audio, Rich4Mkf *effect_mkf)
{
    unsigned char *data = NULL;
    size_t size = 0;
    Mix_Chunk *chunk = NULL;

    /* 0x4159EA plays sound_struct 0x4750F8 (resource id 25)
     * immediately before the Jump.mkf[46] door FLC animation. */
    if (audio == NULL || !audio->ready || effect_mkf == NULL) return 0;
    if (audio->door != NULL) {
        Mix_FreeChunk(audio->door);
        audio->door = NULL;
    }
    if (rich4_mkf_read_resource(effect_mkf, 25, &data, &size, NULL) == RICH4_MKF_OK &&
        data != NULL && size > 0 && size <= 0x7fffffffU) {
        SDL_RWops *rw = SDL_RWFromConstMem(data, (int)size);
        if (rw != NULL) chunk = Mix_LoadWAV_RW(rw, 1);
    }
    free(data);
    audio->door = chunk;
    return chunk != NULL;
}

static Mix_Chunk *load_effect_resource(Rich4Mkf *effect_mkf,
                                       size_t resource_index)
{
    unsigned char *data = NULL;
    size_t size = 0;
    Mix_Chunk *chunk = NULL;
    if (effect_mkf != NULL &&
        rich4_mkf_read_resource(effect_mkf, resource_index,
                                &data, &size, NULL) == RICH4_MKF_OK &&
        data != NULL && size > 0 && size <= 0x7fffffffU) {
        SDL_RWops *rw = SDL_RWFromConstMem(data, (int)size);
        if (rw != NULL) chunk = Mix_LoadWAV_RW(rw, 1);
    }
    free(data);
    return chunk;
}

int rich4_audio_load_property_sounds(Rich4Audio *audio, Rich4Mkf *effect_mkf)
{
    if (audio == NULL || !audio->ready || effect_mkf == NULL) return 0;
    if (audio->property_purchase != NULL)
        Mix_FreeChunk(audio->property_purchase);
    if (audio->property_upgrade != NULL)
        Mix_FreeChunk(audio->property_upgrade);
    if (audio->property_toll != NULL)
        Mix_FreeChunk(audio->property_toll);
    if (audio->property_max_level != NULL)
        Mix_FreeChunk(audio->property_max_level);
    /* sub_41982D passes sound structs 0x4823D2 and 0x4823DA to
     * 0x4542CE: Effect.mkf resources 49 (purchase) and 50 (upgrade). */
    audio->property_purchase = load_effect_resource(effect_mkf, 49);
    audio->property_upgrade = load_effect_resource(effect_mkf, 50);
    audio->property_toll = load_effect_resource(effect_mkf, 55);
    /* sub_40B0CD passes Effect.mkf[90] as the final sub_45144F argument
     * for the Data.mkf[523] maximum-level FLC. */
    audio->property_max_level = load_effect_resource(effect_mkf, 90);
    return audio->property_purchase != NULL || audio->property_upgrade != NULL ||
           audio->property_toll != NULL || audio->property_max_level != NULL;
}

int rich4_audio_load_card_sound(Rich4Audio *audio, Rich4Mkf *effect_mkf)
{
    int loaded = 0;
    if (audio == NULL || !audio->ready || effect_mkf == NULL) return 0;
    if (audio->card != NULL) Mix_FreeChunk(audio->card);
    if (audio->card_monster_animation != NULL)
        Mix_FreeChunk(audio->card_monster_animation);
    if (audio->card_demolish_animation != NULL)
        Mix_FreeChunk(audio->card_demolish_animation);
    /* 0x441F73 passes sound structure 0x482402; its first byte is 0x3e,
     * therefore the effect archive resource is 62. */
    audio->card = load_effect_resource(effect_mkf, 62);
    /* 0x443917 and 0x443B0F pass these resources to the shared
     * 0x45144F destruction presentation after a successful card action. */
    audio->card_monster_animation = load_effect_resource(effect_mkf, 80);
    audio->card_demolish_animation = load_effect_resource(effect_mkf, 97);
    if (audio->card != NULL) loaded++;
    if (audio->card_monster_animation != NULL) loaded++;
    if (audio->card_demolish_animation != NULL) loaded++;
    return loaded;
}

int rich4_audio_load_god_effect_sounds(Rich4Audio *audio,
                                       Rich4Mkf *effect_mkf)
{
    /* The original god callbacks pass these Effect.mkf resource ids as the
     * fifth argument to 0x45144F. Types 11/13/14 have no callback entry. */
    static const int resource[GAME_GOD_TYPE_COUNT + 1] = {
        0, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
        0x70, 0, 0x6f, 0, 0, 0x71, 0, 0, 0
    };
    int loaded = 0;
    int i;

    if (audio == NULL || !audio->ready || effect_mkf == NULL) return 0;
    for (i = 0; i <= GAME_GOD_TYPE_COUNT; i++) {
        if (audio->god_effect[i] != NULL)
            Mix_FreeChunk(audio->god_effect[i]);
        audio->god_effect[i] = resource[i] != 0
            ? load_effect_resource(effect_mkf, (size_t)resource[i]) : NULL;
        if (audio->god_effect[i] != NULL) loaded++;
    }
    /* 0x4823E2 is {54, buffer}; 0x40E476 plays it once before ascent. */
    if (audio->god_departure != NULL) Mix_FreeChunk(audio->god_departure);
    audio->god_departure = load_effect_resource(effect_mkf, 54);
    if (audio->god_departure != NULL) loaded++;
    return loaded;
}

int rich4_audio_load_special_place_sound(Rich4Audio *audio,
                                         Rich4Mkf *effect_mkf)
{
    static const int minigame_resource[9] = {
        11, 13, 14, 15, 19, 20, 21, 22, 24
    };
    static const int news_consequence_resource[5] = {
        86, 84, 87, 89, 88
    };
    int i;
    if (audio == NULL || !audio->ready || effect_mkf == NULL) return 0;
    {
        static const int resources[6] = {27,28,57,58,60,61};
        for (i = 0; i < 6; ++i) {
            if (audio->finance_effect[i] != NULL) Mix_FreeChunk(audio->finance_effect[i]);
            audio->finance_effect[i] = load_effect_resource(effect_mkf, (size_t)resources[i]);
        }
    }
    if (audio->special_place != NULL)
        Mix_FreeChunk(audio->special_place);
    if (audio->auction_action != NULL)
        Mix_FreeChunk(audio->auction_action);
    if (audio->auction_gavel != NULL)
        Mix_FreeChunk(audio->auction_gavel);
    if (audio->confinement_visit != NULL)
        Mix_FreeChunk(audio->confinement_visit);
    if (audio->lottery_purchase != NULL)
        Mix_FreeChunk(audio->lottery_purchase);
    if (audio->roulette != NULL)
        Mix_FreeChunk(audio->roulette);
    if (audio->magic_hover != NULL)
        Mix_FreeChunk(audio->magic_hover);
    if (audio->magic_turn != NULL)
        Mix_FreeChunk(audio->magic_turn);
    for (i = 0; i < 5; i++) {
        if (audio->news_consequence[i] != NULL)
            Mix_FreeChunk(audio->news_consequence[i]);
        audio->news_consequence[i] =
            load_effect_resource(effect_mkf,
                                 (size_t)news_consequence_resource[i]);
    }
    for (i = 0; i < 9; i++) {
        if (audio->point_minigame_effect[i] != NULL)
            Mix_FreeChunk(audio->point_minigame_effect[i]);
        audio->point_minigame_effect[i] =
            load_effect_resource(effect_mkf,
                                 (size_t)minigame_resource[i]);
    }
    /*
     * byte_475299 stores sound-table slots, not Effect.MKF resource ids.
     * 0x419884 indexes 0x48234A in eight-byte records: slot 16 contains
     * resource 48 for actions 10..13 (points/card rewards), while slot 10
     * contains resource 43 for actions 2..9 and 14..16.
     */
    audio->special_place = load_effect_resource(effect_mkf, 48);
    /* 0x41B1AD/0x41B32B pass these archive indices to 0x45144F,
     * independently of the dispatcher's common Effect[48]. */
    for (i = 0; i < 2; i++) {
        if (audio->special_reward_animation[i] != NULL)
            Mix_FreeChunk(audio->special_reward_animation[i]);
        audio->special_reward_animation[i] = load_effect_resource(effect_mkf, 98 + i);
    }
    if (audio->road_gift != NULL) Mix_FreeChunk(audio->road_gift);
    if (audio->road_chest != NULL) Mix_FreeChunk(audio->road_chest);
    audio->road_chest = load_effect_resource(effect_mkf, 36);
    audio->road_gift = load_effect_resource(effect_mkf, 35);
    /* Auction callback 0x43A2DD plays Effect.MKF[63] after an action and
     * Effect.MKF[29] when the sale is called. */
    audio->auction_action = load_effect_resource(effect_mkf, 63);
    audio->auction_gavel = load_effect_resource(effect_mkf, 29);
    audio->confinement_visit = load_effect_resource(effect_mkf, 43);
    /* 0x4315CC loads structure 0x47566B; its first byte is 31, and
     * 0x430030 plays that structure after a successful number click. */
    audio->lottery_purchase = load_effect_resource(effect_mkf, 31);
    /* 0x44090E uses the looping structure at 0x475D4C: Effect.mkf[52]. */
    audio->roulette = load_effect_resource(effect_mkf, 52);
    /* 0x432A6C uses structure 0x4757E7 while entering a new magic
     * action region; its Effect.MKF resource is 39. */
    audio->magic_hover = load_effect_resource(effect_mkf, 39);
    /* sub_40C78C plays structure 0x4823F2; its first byte is 56. */
    audio->magic_turn = load_effect_resource(effect_mkf, 56);
    if (audio->magic_finale != NULL) Mix_FreeChunk(audio->magic_finale);
    /* 0x432A6A -> 0x45144F: Panel[20] with Effect[59]. */
    audio->magic_finale = load_effect_resource(effect_mkf, 59);
    return audio->special_place != NULL ||
           audio->auction_action != NULL ||
           audio->auction_gavel != NULL ||
           audio->confinement_visit != NULL ||
           audio->lottery_purchase != NULL ||
           audio->roulette != NULL ||
           audio->magic_hover != NULL ||
           audio->magic_turn != NULL ||
           audio->magic_finale != NULL ||
           audio->news_consequence[0] != NULL ||
           audio->news_consequence[1] != NULL ||
           audio->news_consequence[2] != NULL ||
           audio->news_consequence[3] != NULL ||
           audio->news_consequence[4] != NULL;
}

int rich4_audio_load_tool_sounds(Rich4Audio *audio, Rich4Mkf *effect_mkf)
{
    static const int resource[13] = {
        38, 33, 34, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };
    static const int effect_resource[12] = {
        81, 82, 83, 90, 91, 92, 94, 84, 96, 85, 93, 15
    };
    int i, loaded = 0;
    if (audio == NULL || !audio->ready || effect_mkf == NULL) return 0;
    for (i = 0; i < 13; i++) {
        if (audio->tool[i] != NULL) Mix_FreeChunk(audio->tool[i]);
        audio->tool[i] = resource[i] >= 0
            ? load_effect_resource(effect_mkf, (size_t)resource[i]) : NULL;
        if (audio->tool[i] != NULL) loaded++;
    }
    for (i = 0; i < 12; i++) {
        if (audio->tool_effect[i] != NULL)
            Mix_FreeChunk(audio->tool_effect[i]);
        audio->tool_effect[i] =
            load_effect_resource(effect_mkf, (size_t)effect_resource[i]);
        if (audio->tool_effect[i] != NULL) loaded++;
    }
    rich4_audio_update_attached_bomb(audio, 0);
    if (audio->attached_bomb != NULL) Mix_FreeChunk(audio->attached_bomb);
    audio->attached_bomb_channel = -1;
    audio->attached_bomb = load_effect_resource(effect_mkf, 32);
    if (audio->attached_bomb != NULL) loaded++;
    return loaded;
}

void rich4_audio_set_speaking_archive(Rich4Audio *audio, Rich4Mkf *speaking_mkf)
{
    if (audio != NULL) audio->speaking_mkf = speaking_mkf;
}

void rich4_audio_set_effects_enabled(Rich4Audio *audio, int enabled)
{
    if (audio == NULL || !audio->ready) return;
    enabled = enabled != 0;
    if (audio->effects_enabled && !enabled) {
        Mix_HaltChannel(-1);
        audio->attached_bomb_channel = -1;
        audio->tool_robot_channel = -1;
        audio->speech_channel = -1;
        audio->movement_channel = -1;
    }
    audio->effects_enabled = enabled;
}
