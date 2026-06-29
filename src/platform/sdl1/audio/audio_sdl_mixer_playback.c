#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/audio/audio_sdl_mixer_internal.h"
#include "app/audio_event.h"
#include "platform/sdl1/runtime_trace.h"

#include <stdlib.h>
#include <SDL/SDL_mixer.h>

void rich4_audio_begin_visual_frame(Rich4Audio *audio)
{
    if (audio != NULL) audio->visual_frame_open = 1;
}

void rich4_audio_present_visual_frame(Rich4Audio *audio)
{
    int i;
    if (audio == NULL) return;
    audio->visual_frame_open = 0;
    for (i = 0; i < audio->visual_sound_count; i++)
        if (audio->ready && audio->effects_enabled)
            Mix_PlayChannel(-1, audio->visual_sounds[i], 0);
    audio->visual_sound_count = 0;
    if (audio->prepared_flc_effect != NULL && audio->ready && audio->effects_enabled) {
        audio->flc_effect_chunk = audio->prepared_flc_effect;
        audio->flc_effect_channel = Mix_PlayChannel(-1, audio->flc_effect_chunk, 0);
    }
    audio->prepared_flc_effect = NULL;
}

static void play_visual_effect(Rich4Audio *audio, Mix_Chunk *chunk)
{
    if (audio->visual_frame_open && audio->visual_sound_count < 32)
        audio->visual_sounds[audio->visual_sound_count++] = chunk;
    else
        Mix_PlayChannel(-1, chunk, 0);
}

/* 0x454304 stops the previous shared FLC effect before preparing the
 * next; 0x45434F starts it after 0x450F04. Speech has its own channel. */
static void play_flc_effect(Rich4Audio *audio, Mix_Chunk *chunk)
{
    if (audio->flc_effect_chunk != NULL && audio->flc_effect_channel >= 0 &&
        Mix_GetChunk(audio->flc_effect_channel) == audio->flc_effect_chunk)
        Mix_HaltChannel(audio->flc_effect_channel);
    audio->flc_effect_channel = -1;
    audio->prepared_flc_effect = chunk;
    if (!audio->visual_frame_open) {
        audio->flc_effect_chunk = chunk;
        audio->flc_effect_channel = Mix_PlayChannel(-1, chunk, 0);
        audio->prepared_flc_effect = NULL;
    }
}

static Mix_Chunk *load_effect_resource(Rich4Mkf *archive, size_t resource_index)
{
    unsigned char *data = NULL;
    size_t size = 0;
    Mix_Chunk *chunk = NULL;
    unsigned int started = runtime_profile_now();
    Rich4MkfError error = rich4_mkf_read_resource(archive, resource_index, &data, &size, NULL);
    runtime_profile_span("audio", "speech.read", started, (int)resource_index, size);
    if (error == RICH4_MKF_OK && data && size && size <= 0x7fffffffU) {
        SDL_RWops *rw;
        started = runtime_profile_now();
        rw = SDL_RWFromConstMem(data, (int)size);
        if (rw) chunk = Mix_LoadWAV_RW(rw, 1);
        runtime_profile_span("audio", "speech.convert", started, (int)resource_index, size);
    }
    free(data);
    return chunk;
}

static int rich4_audio_play_music_loops(Rich4Audio *audio,
                                        const char *path,
                                        int loops)
{
    Mix_Music *music;

    if (audio == NULL || !audio->ready || path == NULL) {
        return 0;
    }
    music = Mix_LoadMUS(path);
    if (music == NULL) {
        return 0;
    }
    if (audio->music != NULL) {
        Mix_HaltMusic();
        Mix_FreeMusic(audio->music);
    }
    audio->music = music;
    if (Mix_PlayMusic(audio->music, loops) != 0) {
        Mix_FreeMusic(audio->music);
        audio->music = NULL;
        return 0;
    }
    return 1;
}

int rich4_audio_play_music(Rich4Audio *audio, const char *path)
{
    return rich4_audio_play_music_loops(audio, path, -1);
}

int rich4_audio_play_music_once(Rich4Audio *audio, const char *path)
{
    return rich4_audio_play_music_loops(audio, path, 0);
}

int rich4_audio_music_playing(const Rich4Audio *audio)
{
    return audio != NULL && audio->ready && Mix_PlayingMusic() != 0;
}

void rich4_audio_stop_music(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready) {
        return;
    }
    Mix_HaltMusic();
}

void rich4_audio_play_ok(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->ok == NULL) {
        return;
    }
    Mix_PlayChannel(-1, audio->ok, 0);
}

void rich4_audio_play_select(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->select == NULL) {
        return;
    }
    Mix_PlayChannel(-1, audio->select, 0);
}

void rich4_audio_play_cancel(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->cancel == NULL) {
        return;
    }
    Mix_PlayChannel(-1, audio->cancel, 0);
}

void rich4_audio_play_invalid(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->invalid == NULL) return;
    Mix_PlayChannel(-1, audio->invalid, 0);
}

void rich4_audio_play_target_confirm(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->target_confirm == NULL) return;
    Mix_PlayChannel(-1, audio->target_confirm, 0);
}

void rich4_audio_play_new_game_transition(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->new_game_transition == NULL) return;
    Mix_PlayChannel(-1, audio->new_game_transition, 0);
}

void rich4_audio_play_bank_key(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->bank_key == NULL) return;
    Mix_PlayChannel(-1, audio->bank_key, 0);
}

void rich4_audio_play_bank_slider(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->bank_slider == NULL) return;
    Mix_PlayChannel(-1, audio->bank_slider, 0);
}

void rich4_audio_play_dice(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->dice == NULL) return;
    play_visual_effect(audio, audio->dice);
}

void rich4_audio_play_jump(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->jump == NULL) return;
    play_visual_effect(audio, audio->jump);
}

void rich4_audio_play_door(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->door == NULL) return;
    Mix_PlayChannel(-1, audio->door, 0);
}

void rich4_audio_play_property_purchase(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->property_purchase == NULL) return;
    Mix_PlayChannel(-1, audio->property_purchase, 0);
}

void rich4_audio_play_property_upgrade(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->property_upgrade == NULL) return;
    Mix_PlayChannel(-1, audio->property_upgrade, 0);
}

void rich4_audio_play_property_toll(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->property_toll == NULL) return;
    Mix_PlayChannel(-1, audio->property_toll, 0);
}

void rich4_audio_play_property_max_level(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->property_max_level == NULL)
        return;
    play_flc_effect(audio, audio->property_max_level);
}

void rich4_audio_play_card(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->card == NULL) return;
    Mix_PlayChannel(-1, audio->card, 0);
}

void rich4_audio_play_card_animation(Rich4Audio *audio, int resource)
{
    Mix_Chunk *chunk = NULL;
    if (audio == NULL || !audio->ready || !audio->effects_enabled)
        return;
    if (resource == 80)
        chunk = audio->card_monster_animation;
    else if (resource == 97)
        chunk = audio->card_demolish_animation;
    if (chunk != NULL) play_flc_effect(audio, chunk);
}

void rich4_audio_play_god_effect(Rich4Audio *audio, int god_type)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        god_type <= 0 || god_type > GAME_GOD_TYPE_COUNT ||
        audio->god_effect[god_type] == NULL) return;
    play_flc_effect(audio, audio->god_effect[god_type]);
}

void rich4_audio_play_special_place(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->special_place == NULL) return;
    Mix_PlayChannel(-1, audio->special_place, 0);
}

void rich4_audio_play_finance_effect(Rich4Audio *audio, int resource)
{
    static const int resources[6] = {27,28,57,58,60,61};
    if (audio == NULL || !audio->ready || !audio->effects_enabled) return;
    for (int i = 0; i < 6; ++i)
        if (resources[i] == resource && audio->finance_effect[i] != NULL)
            Mix_PlayChannel(-1, audio->finance_effect[i], 0);
}

void rich4_audio_stop_finance_effects(Rich4Audio *audio, int resource)
{
    static const int resources[6] = {27,28,57,58,60,61};
    int channels;
    if (audio == NULL || !audio->ready) return;
    channels = Mix_AllocateChannels(-1);
    /* 0x454240 stops a still-playing modal sound before releasing it. */
    for (int i=0;i<6;++i) if (resource==0 || resource==resources[i])
        for (int c=0;c<channels;++c)
            if (audio->finance_effect[i] && Mix_Playing(c) &&
                Mix_GetChunk(c)==audio->finance_effect[i]) Mix_HaltChannel(c);
}

void rich4_audio_play_auction_effect(Rich4Audio *audio, int resource)
{
    Mix_Chunk *effect;
    if (audio == NULL || !audio->ready || !audio->effects_enabled) return;
    effect = resource == 63 ? audio->auction_action :
             resource == 29 ? audio->auction_gavel : NULL;
    if (effect != NULL) Mix_PlayChannel(-1, effect, 0);
}

void rich4_audio_play_confinement_visit(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->confinement_visit == NULL) return;
    Mix_PlayChannel(-1, audio->confinement_visit, 0);
}

void rich4_audio_play_lottery_purchase(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->lottery_purchase == NULL) return;
    Mix_PlayChannel(-1, audio->lottery_purchase, 0);
}

void rich4_audio_play_roulette(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->roulette == NULL) return;
    if (audio->roulette_channel >= 0)
        Mix_HaltChannel(audio->roulette_channel);
    audio->roulette_channel = Mix_PlayChannel(-1, audio->roulette, -1);
}

void rich4_audio_stop_roulette(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready) return;
    if (audio->roulette_channel >= 0)
        Mix_HaltChannel(audio->roulette_channel);
    audio->roulette_channel = -1;
}

void rich4_audio_play_magic_hover(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->magic_hover == NULL) return;
    Mix_PlayChannel(-1, audio->magic_hover, 0);
}

void rich4_audio_play_magic_turn(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->magic_turn == NULL) return;
    Mix_PlayChannel(-1, audio->magic_turn, 0);
}

static int point_minigame_effect_index(int resource)
{
    static const int resources[9] = {
        11, 13, 14, 15, 19, 20, 21, 22, 24
    };
    int i;
    for (i = 0; i < 9; i++)
        if (resources[i] == resource) return i;
    return -1;
}

void rich4_audio_play_news_consequence(Rich4Audio *audio, int logical_id)
{
    static const int news_id[5] = {4, 5, 15, 20, 21};
    int index;
    if (audio == NULL || !audio->ready || !audio->effects_enabled) return;
    for (index = 0; index < 5; index++)
        if (news_id[index] == logical_id &&
            audio->news_consequence[index] != NULL) {
            play_flc_effect(audio, audio->news_consequence[index]);
            return;
        }
}

void rich4_audio_play_point_minigame_effect(
    Rich4Audio *audio, int resource)
{
    int index;
    int loops;
    int channel;
    if (audio == NULL || !audio->ready || !audio->effects_enabled)
        return;
    index = point_minigame_effect_index(resource);
    if (index < 0 || audio->point_minigame_effect[index] == NULL)
        return;
    loops = resource == 24 ? -1 : 0;
    if (resource == 24 && audio->point_minigame_loop_channel >= 0)
        Mix_HaltChannel(audio->point_minigame_loop_channel);
    channel = Mix_PlayChannel(
        -1, audio->point_minigame_effect[index], loops);
    if (resource == 24)
        audio->point_minigame_loop_channel = channel;
}

void rich4_audio_stop_point_minigame_effect(
    Rich4Audio *audio, int resource)
{
    if (audio == NULL || !audio->ready || resource != 24)
        return;
    if (audio->point_minigame_loop_channel >= 0)
        Mix_HaltChannel(audio->point_minigame_loop_channel);
    audio->point_minigame_loop_channel = -1;
}

void rich4_audio_play_tool(Rich4Audio *audio, int tool)
{
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        tool < 0 || tool >= 13 ||
        audio->tool[tool] == NULL) return;
    if (tool == 0) {
        if (audio->tool_robot_channel >= 0)
            Mix_HaltChannel(audio->tool_robot_channel);
        audio->tool_robot_channel =
            Mix_PlayChannel(-1, audio->tool[tool], -1);
    } else {
        Mix_PlayChannel(-1, audio->tool[tool], 0);
    }
}

void rich4_audio_stop_tool_robot(Rich4Audio *audio)
{
    if (audio == NULL || !audio->ready) return;
    if (audio->tool_robot_channel >= 0)
        Mix_HaltChannel(audio->tool_robot_channel);
    audio->tool_robot_channel = -1;
}

void rich4_audio_play_tool_effect(Rich4Audio *audio, int resource)
{
    static const int effect_resource[12] = {
        81, 82, 83, 90, 91, 92, 94, 84, 96, 85, 93, 15
    };
    int i;
    if (audio == NULL || !audio->ready || !audio->effects_enabled) return;
    for (i = 0; i < 12; i++) {
        if (effect_resource[i] == resource &&
            audio->tool_effect[i] != NULL) {
            play_flc_effect(audio, audio->tool_effect[i]);
            return;
        }
    }
}

void rich4_audio_play_attached_bomb(Rich4Audio *audio)
{
    rich4_audio_update_attached_bomb(audio, 1);
}

void rich4_audio_update_attached_bomb(Rich4Audio *audio, int active)
{
    int owns_channel;
    if (audio == NULL || !audio->ready) return;
    owns_channel = audio->attached_bomb_channel >= 0 &&
        Mix_GetChunk(audio->attached_bomb_channel) == audio->attached_bomb;
    if (!active || !audio->effects_enabled) {
        if (owns_channel) Mix_HaltChannel(audio->attached_bomb_channel);
        audio->attached_bomb_channel = -1;
        return;
    }
    if (owns_channel && Mix_Playing(audio->attached_bomb_channel)) return;
    audio->attached_bomb_channel = -1;
    /* 0x40DA28 / 0x40DE04: independent Effect[32] buffer, Play flags=1.
     * Repeated state updates must not restart the sample at each node. */
    if (audio->attached_bomb != NULL)
        audio->attached_bomb_channel =
            Mix_PlayChannel(-1, audio->attached_bomb, -1);
}

int rich4_audio_play_speech(Rich4Audio *audio, int resource_index)
{
    unsigned int started, total;
    if (audio == NULL || !audio->ready || !audio->effects_enabled ||
        audio->speaking_mkf == NULL ||
        resource_index < 0) return 0;
    total = started = runtime_profile_now();
    /*
     * SDL_mixer may reuse a completed speech channel for an unrelated
     * auto-selected effect.  Do not halt that new owner merely because the
     * numeric channel id is still cached here.
     */
    if (audio->speech_channel >= 0 &&
        Mix_GetChunk(audio->speech_channel) == audio->speech)
        Mix_HaltChannel(audio->speech_channel);
    runtime_profile_span("audio", "speech.stop", started, resource_index, 0);
    started = runtime_profile_now();
    audio->speech_channel = -1;
    if (audio->speech != NULL) {
        Mix_FreeChunk(audio->speech);
        audio->speech = NULL;
    }
    runtime_profile_span("audio", "speech.free", started, resource_index, 0);
    audio->speech = load_effect_resource(audio->speaking_mkf,
                                         (size_t)resource_index);
    if (audio->speech == NULL) {
        runtime_profile_span("audio", "speech.failed", total, resource_index, 0); return 0;
    }
    started = runtime_profile_now();
    audio->speech_channel = Mix_PlayChannel(-1, audio->speech, 0);
    runtime_profile_span("audio", "speech.play", started, resource_index, audio->speech->alen);
    runtime_profile_span("audio", "speech.total", total, resource_index, audio->speech->alen);
    return audio->speech_channel >= 0;
}

int rich4_audio_speech_playing(const Rich4Audio *audio)
{
    return audio != NULL && audio->ready && audio->speech_channel >= 0 &&
           audio->speech != NULL &&
           Mix_GetChunk(audio->speech_channel) == audio->speech &&
           Mix_Playing(audio->speech_channel) != 0;
}

void rich4_audio_stop_speech(Rich4Audio *audio)
{
    if (audio == NULL) return;
    if (audio->speech_channel >= 0 &&
        Mix_GetChunk(audio->speech_channel) == audio->speech)
        Mix_HaltChannel(audio->speech_channel);
    audio->speech_channel = -1;
    if (audio->speech != NULL) {
        Mix_FreeChunk(audio->speech);
        audio->speech = NULL;
    }
}

void rich4_audio_update_movement(Rich4Audio *audio, int moving, int traffic_method)
{
    int method;
    if (audio == NULL || !audio->ready || !audio->effects_enabled) {
        if (audio != NULL && audio->ready) {
            if (audio->movement_channel >= 0)
                Mix_HaltChannel(audio->movement_channel);
            audio->movement_channel = -1;
            audio->movement_method = -1;
        }
        return;
    }
    if (!moving) {
        if (audio->movement_channel >= 0) Mix_HaltChannel(audio->movement_channel);
        audio->movement_channel = -1;
        audio->movement_method = -1;
        return;
    }
    method = traffic_method;
    if (method < 0 || method >= RICH4_MOVEMENT_SOUND_COUNT)
        method = RICH4_MOVEMENT_SOUND_WALK;
    if (audio->movement_channel >= 0 && audio->movement_method == method &&
        Mix_Playing(audio->movement_channel)) return;
    if (audio->movement_channel >= 0) Mix_HaltChannel(audio->movement_channel);
    audio->movement_channel = -1;
    audio->movement_method = method;
    if (audio->movement[method] != NULL)
        audio->movement_channel = Mix_PlayChannel(-1, audio->movement[method], -1);
}

void rich4_audio_consume_event(Rich4Audio *audio, const Rich4AudioEvent *event)
{
    if (event == NULL) return;
    switch (event->type) {
    case RICH4_AUDIO_EVENT_DICE: rich4_audio_play_dice(audio); break;
    case RICH4_AUDIO_EVENT_JUMP: rich4_audio_play_jump(audio); break;
    case RICH4_AUDIO_EVENT_DOOR: rich4_audio_play_door(audio); break;
    case RICH4_AUDIO_EVENT_PROPERTY_PURCHASE:
        rich4_audio_play_property_purchase(audio);
        break;
    case RICH4_AUDIO_EVENT_PROPERTY_UPGRADE:
        rich4_audio_play_property_upgrade(audio);
        break;
    case RICH4_AUDIO_EVENT_PROPERTY_TOLL:
        rich4_audio_play_property_toll(audio);
        break;
    case RICH4_AUDIO_EVENT_PROPERTY_MAX_LEVEL:
        rich4_audio_play_property_max_level(audio);
        break;
    case RICH4_AUDIO_EVENT_MOVEMENT:
        rich4_audio_update_movement(audio, event->active, event->value);
        break;
    case RICH4_AUDIO_EVENT_OK: rich4_audio_play_ok(audio); break;
    case RICH4_AUDIO_EVENT_INVALID: rich4_audio_play_invalid(audio); break;
    case RICH4_AUDIO_EVENT_TARGET_CONFIRM:
        rich4_audio_play_target_confirm(audio);
        break;
    case RICH4_AUDIO_EVENT_CONFINEMENT_VISIT:
        rich4_audio_play_confinement_visit(audio);
        break;
    case RICH4_AUDIO_EVENT_ROULETTE: rich4_audio_play_roulette(audio); break;
    case RICH4_AUDIO_EVENT_ROULETTE_STOP: rich4_audio_stop_roulette(audio); break;
    case RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT:
        rich4_audio_play_point_minigame_effect(audio, event->value);
        break;
    case RICH4_AUDIO_EVENT_POINT_MINIGAME_STOP_EFFECT:
        rich4_audio_stop_point_minigame_effect(audio, event->value);
        break;
    case RICH4_AUDIO_EVENT_CANCEL: rich4_audio_play_cancel(audio); break;
    case RICH4_AUDIO_EVENT_MAGIC_HOVER: rich4_audio_play_magic_hover(audio); break;
    case RICH4_AUDIO_EVENT_MAGIC_TURN: rich4_audio_play_magic_turn(audio); break;
    case RICH4_AUDIO_EVENT_MAGIC_FINALE:
        if (audio != NULL && audio->ready && audio->effects_enabled &&
            audio->magic_finale != NULL)
            play_flc_effect(audio, audio->magic_finale);
        break;
    case RICH4_AUDIO_EVENT_ATTACHED_BOMB:
        rich4_audio_update_attached_bomb(audio, event->active);
        break;
    case RICH4_AUDIO_EVENT_SPEECH_START:
        rich4_audio_play_speech(audio, event->value);
        break;
    case RICH4_AUDIO_EVENT_SPEECH_STOP: rich4_audio_stop_speech(audio); break;
    case RICH4_AUDIO_EVENT_TOOL_EFFECT:
        rich4_audio_play_tool_effect(audio, event->value);
        break;
    case RICH4_AUDIO_EVENT_TOOL_USE:
        rich4_audio_play_tool(audio, event->value);
        break;
    case RICH4_AUDIO_EVENT_TOOL_PLACEMENT:
        rich4_audio_play_tool(audio, event->value);
        break;
    case RICH4_AUDIO_EVENT_TOOL_ROBOT_STOP:
        rich4_audio_stop_tool_robot(audio);
        break;
    case RICH4_AUDIO_EVENT_CARD:
        rich4_audio_play_card(audio);
        break;
    case RICH4_AUDIO_EVENT_CARD_ANIMATION:
        rich4_audio_play_card_animation(audio, event->value);
        break;
    case RICH4_AUDIO_EVENT_GOD_DEPARTURE:
        if (audio != NULL && audio->ready && audio->effects_enabled &&
            audio->god_departure != NULL)
            Mix_PlayChannel(-1, audio->god_departure, 0);
        break;
    case RICH4_AUDIO_EVENT_GOD_EFFECT:
        rich4_audio_play_god_effect(audio, event->value);
        break;
    case RICH4_AUDIO_EVENT_ROAD_GIFT:
        if (audio != NULL && audio->ready && audio->effects_enabled &&
            audio->road_gift != NULL)
            Mix_PlayChannel(-1, audio->road_gift, 0);
        break;
    case RICH4_AUDIO_EVENT_ROAD_CHEST:
        if (audio != NULL && audio->ready && audio->effects_enabled &&
            audio->road_chest != NULL)
            Mix_PlayChannel(-1, audio->road_chest, 0);
        break;
    case RICH4_AUDIO_EVENT_SPECIAL_PLACE:
        rich4_audio_play_special_place(audio);
        break;
    case RICH4_AUDIO_EVENT_SPECIAL_PLACE_ENTRY:
        if (event->value >= 10 && event->value <= 13)
            rich4_audio_play_special_place(audio);
        else if (event->value >= 2 && event->value <= 16)
            rich4_audio_play_confinement_visit(audio);
        break;
    case RICH4_AUDIO_EVENT_SPECIAL_REWARD_ANIMATION:
        if (audio != NULL && audio->ready && audio->effects_enabled &&
            event->value >= 98 && event->value <= 99 &&
            audio->special_reward_animation[event->value - 98] != NULL)
            play_flc_effect(audio, audio->special_reward_animation[event->value - 98]);
        break;
    case RICH4_AUDIO_EVENT_AUCTION_EFFECT:
        rich4_audio_play_auction_effect(audio, event->value);
        break;
    case RICH4_AUDIO_EVENT_BANK_KEY:
        rich4_audio_play_bank_key(audio);
        break;
    case RICH4_AUDIO_EVENT_BANK_SLIDER:
        rich4_audio_play_bank_slider(audio);
        break;
    default: break;
    }
}
