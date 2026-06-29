#ifndef RICH4_CORE_CHARACTER_SPEECH_RULES_H
#define RICH4_CORE_CHARACTER_SPEECH_RULES_H

#include "core/game_state.h"
#include "data/character_speech.h"

int rich4_player_speech_allowed(const GamePlayerState *player);
int rich4_player_say_try_begin(const char **last_text,
                               const char *text,
                               const GamePlayerState *player);
void rich4_player_say_decode(const char *text,
                             int default_speaking_resource,
                             Rich4PlayerSayText *out_text);
int rich4_player_say_should_wait(uint32_t elapsed_ms,
                                 int speech_playing,
                                 int skip_requested);
int rich4_property_same_group_owned_count(const GameState *state,
                                          int player, int land);
int rich4_property_toll_flash_count(const GameState *state, int land);
int rich4_property_toll_flash_contains(const GameState *state,
                                       int land, int candidate);
int rich4_property_relation_target(const GameState *state, int player);
Rich4PropertySpeechKind rich4_property_payer_speech(
    const GameState *state, int payer, int owner, int amount,
    int special_random, int normal_random);
Rich4PropertySpeechKind rich4_property_receiver_speech(
    const GameState *state, int amount, int normal_random);

#endif
