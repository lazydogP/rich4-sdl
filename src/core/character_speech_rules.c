#include "core/character_speech_rules.h"

#include <string.h>

int rich4_player_speech_allowed(const GamePlayerState *player)
{
    return player != 0 && player->days_disappearing == 0 &&
           player->days_sleep_walking == 0 && player->days_sleeping == 0;
}

int rich4_player_say_try_begin(const char **last_text,
                               const char *text,
                               const GamePlayerState *player)
{
    if (last_text == 0 || *last_text == text) return 0;
    *last_text = text;
    return rich4_player_speech_allowed(player);
}

static int decimal_digit(unsigned char value)
{
    return value >= '0' && value <= '9' ? value - '0' : -1;
}

void rich4_player_say_decode(const char *text,
                             int default_speaking_resource,
                             Rich4PlayerSayText *out_text)
{
    Rich4PlayerSayText decoded;
    const char *display = text;
    decoded.display_text = text;
    decoded.speaking_resource = default_speaking_resource;
    decoded.icon_chunk = -1;
    if (display != 0 && display[0] == '#' && strlen(display) >= 5) {
        int d0 = decimal_digit((unsigned char)display[1]);
        int d1 = decimal_digit((unsigned char)display[2]);
        int d2 = decimal_digit((unsigned char)display[3]);
        int d3 = decimal_digit((unsigned char)display[4]);
        if (d0 >= 0 && d1 >= 0 && d2 >= 0 && d3 >= 0) {
            decoded.speaking_resource = d0 * 1000 + d1 * 100 + d2 * 10 + d3;
            display += 5;
            decoded.display_text = display;
        }
    }
    if (display != 0 && display[0] == '@') {
        size_t length = strlen(display);
        int d0 = length >= 2 ? decimal_digit((unsigned char)display[1]) : -1;
        int d1 = length >= 3 ? decimal_digit((unsigned char)display[2]) : -1;
        int icon = d0;
        if (d0 >= 0 && d1 >= 0) icon = d0 * 10 + d1;
        if (icon > 0) decoded.icon_chunk = icon - 1;
    }
    if (out_text != 0) *out_text = decoded;
}

int rich4_player_say_should_wait(uint32_t elapsed_ms,
                                 int speech_playing,
                                 int skip_requested)
{
    return !skip_requested && (elapsed_ms < 1000 || speech_playing);
}

int rich4_property_same_group_owned_count(const GameState *state,
                                          int player, int land)
{
    int i, count = 0;
    if (state == 0 || player < 0 || player >= state->player_count ||
        land < 0 || land >= state->land_count || state->land_type[land] != 0)
        return 0;
    for (i = 0; i < state->land_count; i++)
        if (state->land_type[i] == 0 && state->land_owner[i] == player &&
            state->land_group[i] == state->land_group[land]) count++;
    return count;
}

int rich4_property_toll_flash_contains(const GameState *state,
                                       int land, int candidate)
{
    int owner, related;
    if (state == 0 || land < 0 || land >= state->land_count ||
        candidate < 0 || candidate >= state->land_count) return 0;
    owner = state->land_owner[land];
    if (owner < 0 || owner >= state->player_count) return 0;
    related = state->players[owner].linked_player;
    /* 0x41982D: residential names must match; every nonzero subtype is
     * a chain store and participates regardless of its street name. */
    if (state->land_owner[candidate] != owner &&
        !(related >= 0 && related < state->player_count &&
          state->land_owner[candidate] == related)) return 0;
    if (state->land_type[land] != 0) return state->land_type[candidate] != 0;
    return state->land_type[candidate] == 0 &&
           state->land_group[candidate] == state->land_group[land];
}

int rich4_property_toll_flash_count(const GameState *state, int land)
{
    int i, count = 0;
    if (state == 0) return 0;
    for (i = 0; i < state->land_count; i++)
        count += rich4_property_toll_flash_contains(state, land, i);
    return count;
}

int rich4_property_relation_target(const GameState *state, int player)
{
    int i, best = -1, best_value = -1;
    if (state == 0 || player < 0 || player >= state->player_count) return -1;
    for (i = 0; i < state->player_count; i++) {
        int value;
        if (i == player || (state->players[i].play_state & 0xff) == 0) continue;
        value = state->players[player].relation_balance[i];
        if (best < 0 || value > best_value) { best = i; best_value = value; }
    }
    return best;
}

Rich4PropertySpeechKind rich4_property_payer_speech(
    const GameState *state, int payer, int owner, int amount,
    int special_random, int normal_random)
{
    int scale = state != 0 && state->price_index > 0 ? state->price_index : 1;
    if (rich4_property_relation_target(state, payer) == owner &&
        amount >= 5000 * scale && (special_random & 1))
        return RICH4_PROPERTY_SPEECH_PAYER_RELATION;
    if (amount >= 9000 * scale) return RICH4_PROPERTY_SPEECH_PAYER_HIGH_1;
    if (amount >= 5000 * scale)
        return (normal_random & 1) ? RICH4_PROPERTY_SPEECH_PAYER_HIGH_2 :
                                      RICH4_PROPERTY_SPEECH_PAYER_HIGH_1;
    if (amount > 0) return RICH4_PROPERTY_SPEECH_PAYER_LOW;
    return RICH4_PROPERTY_SPEECH_COUNT;
}

Rich4PropertySpeechKind rich4_property_receiver_speech(
    const GameState *state, int amount, int normal_random)
{
    uint32_t scale = state != 0 && state->price_index > 0 ? (uint32_t)state->price_index : 1u;
    /* 0x44F36F/0x44F3B2/0x44F3FB compare signed DWORD products. */
    if (amount >= (int32_t)(9000u * scale)) return RICH4_PROPERTY_SPEECH_RECEIVER_HIGH_1;
    if (amount >= (int32_t)(5000u * scale))
        return (normal_random & 1) ? RICH4_PROPERTY_SPEECH_RECEIVER_HIGH_2 :
                                      RICH4_PROPERTY_SPEECH_RECEIVER_HIGH_1;
    if (amount >= (int32_t)(2000u * scale)) return RICH4_PROPERTY_SPEECH_RECEIVER_LOW;
    return RICH4_PROPERTY_SPEECH_COUNT;
}
