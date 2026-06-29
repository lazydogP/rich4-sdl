#ifndef RICH4_SDL_AUDIO_MIXER_INTERNAL_H
#define RICH4_SDL_AUDIO_MIXER_INTERNAL_H

#include <SDL/SDL_mixer.h>

#include "app/audio_event.h"
#include "core/game_constants.h"
#include "resource/mkf.h"

struct Rich4Audio {
    int ready;
    int effects_enabled;
    int visual_frame_open;
    int visual_sound_count;
    int flc_effect_channel;
    Mix_Chunk *flc_effect_chunk;
    Mix_Chunk *prepared_flc_effect;
    Mix_Chunk *visual_sounds[32];
    Mix_Music *music;
    Mix_Chunk *ok;
    Mix_Chunk *select;
    Mix_Chunk *cancel;
    Mix_Chunk *invalid;
    Mix_Chunk *target_confirm;
    Mix_Chunk *new_game_transition;
    Mix_Chunk *bank_key;
    Mix_Chunk *bank_slider;
    Mix_Chunk *dice;
    Mix_Chunk *jump;
    Mix_Chunk *door;
    Mix_Chunk *property_purchase;
    Mix_Chunk *property_upgrade;
    Mix_Chunk *property_toll;
    Mix_Chunk *property_max_level;
    Mix_Chunk *card;
    Mix_Chunk *card_monster_animation;
    Mix_Chunk *card_demolish_animation;
    Mix_Chunk *god_effect[GAME_GOD_TYPE_COUNT + 1];
    Mix_Chunk *god_departure;
    Mix_Chunk *special_place;
    Mix_Chunk *special_reward_animation[2];
    Mix_Chunk *road_chest;
    Mix_Chunk *road_gift;
    Mix_Chunk *auction_action;
    Mix_Chunk *auction_gavel;
    Mix_Chunk *finance_effect[6];
    Mix_Chunk *confinement_visit;
    Mix_Chunk *lottery_purchase;
    Mix_Chunk *roulette;
    int roulette_channel;
    Mix_Chunk *magic_hover;
    Mix_Chunk *magic_turn;
    Mix_Chunk *magic_finale;
    Mix_Chunk *news_consequence[5];
    Mix_Chunk *point_minigame_effect[9];
    int point_minigame_loop_channel;
    Mix_Chunk *tool[13];
    Mix_Chunk *tool_effect[12];
    int tool_robot_channel;
    Mix_Chunk *attached_bomb;
    int attached_bomb_channel;
    Rich4Mkf *speaking_mkf;
    Mix_Chunk *speech;
    int speech_channel;
    Mix_Chunk *movement[RICH4_MOVEMENT_SOUND_COUNT];
    int movement_channel;
    int movement_method;
};

#endif
