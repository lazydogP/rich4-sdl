#ifndef RICH4_CORE_GOD_H
#define RICH4_CORE_GOD_H

#include <stddef.h>

#include "core/game_event.h"
#include "core/game_state.h"

typedef enum GameGodResult {
    GAME_GOD_APPLIED = 1,
    GAME_GOD_INVALID,
    GAME_GOD_OCCUPIED,
    GAME_GOD_NOT_SENDABLE,
    GAME_GOD_EVENT_CAPACITY
} GameGodResult;

#define RICH4_GOD_ACQUIRED_FIRST 0x01u
/* Result metadata is carried in the two domain-reference fields of the
 * scalar acquisition event.  This keeps one event for landing and 请神符,
 * while allowing the original card-result sentence to name the actual card. */
#define RICH4_GOD_ACQUIRED_RESULT_CARD1 0x02u
#define RICH4_GOD_ACQUIRED_RESULT_CARD2 0x04u
/* 0x40EAD7 waits for 0x440706 before committing any money transfer. */
#define RICH4_GOD_ACQUIRED_MONEY_PENDING 0x08u
#define RICH4_GOD_ACQUIRED_DEATH_PENDING 0x10u
#define RICH4_GOD_ACQUIRED_LOST_COUNT_SHIFT 8
#define RICH4_GOD_ACQUIRED_LOST_COUNT_MASK (0xffu << \
                                             RICH4_GOD_ACQUIRED_LOST_COUNT_SHIFT)

static inline int rich4_god_player_type(const GamePlayerState *player)
{
    return player != NULL && player->god_info != 0 ? player->god_type : 0;
}
static inline int rich4_god_blocks_trade(const GamePlayerState *player)
{
    /* 0x40FA61 compares player +63 directly, before resolving the type. */
    int slot = player != NULL ? player->god_info : 0;
    return slot == 7 || slot == 8 || slot == 15;
}
/* 0x41982D: slot 12 skips the ordinary unowned-property purchase branch at
 * landing.  It is not a blanket ban on owning property; 0x40F381 separately
 * transfers built land/facilities to the player. */
static inline int rich4_god_blocks_purchase(const GamePlayerState *player)
{
    return player != NULL && player->god_info == 12;
}
static inline int rich4_god_suppresses_collection(const GamePlayerState *player)
{
    return rich4_god_player_type(player) == 15;
}
static inline int rich4_god_is_sendable_type(int type)
{
    return type == 5 || type == 6 || type == 7 || type == 8 ||
           type == 10 || type == 15;
}
/* 0x40EA62: only these dynamic-object types are road gods.  Type 15 is the
 * Death god; the other dynamic types are the dog, gift/chest and tools. */
static inline int rich4_god_is_road_type(int type)
{
    return (type >= 1 && type <= 10) || type == 12 || type == 15;
}

/* Exact signed tables from Rich4.exe player-record offsets +68/+70/+72.
 * Type 11/13/14 are dynamic objects but have no god attachment effect. */
int rich4_god_stat_deltas(int type, int *out_income,
                          int *out_investment, int *out_land);
/* 0x440706: small gods use three digits, big gods four. No price scaling. */
static inline int rich4_god_money_digits(int type)
{
    return type == 1 || type == 5 ? 3 :
           type == 2 || type == 6 ? 4 : 0;
}
/* 0x40F36E/0x40F377, called after death animation and main explanation. */
GameGodResult rich4_god_resolve_death(GameState *state, int player,
                                     int object_slot, GameEventQueue *events);
GameGodResult rich4_god_resolve_money(GameState *state, int player,
                                      int object_slot, int god_type,
                                      int amount, GameEventQueue *events);
/* Resume one opponent in the type-1/type-5 loop after a blocking bankruptcy. */
GameGodResult rich4_god_resolve_money_player(GameState *state, int player,
    int object_slot, int god_type, int amount, int other, GameEventQueue *events);
GameGodResult rich4_god_attach(GameState *state, int player, int object_slot,
                               int god_type, GameEventQueue *events);
/* 0x40EAD7's replacement form: landing/invitation first detaches the
 * current player's old god, then attaches the selected road god. */
GameGodResult rich4_god_attach_replace(GameState *state, int player,
                                        int object_slot, int god_type,
                                        GameEventQueue *events);
GameGodResult rich4_god_expire(GameState *state, int player, GameEventQueue *events);
GameGodResult rich4_god_send_away(GameState *state, int player,
                                  GameEventQueue *events);

/* 0x40F381: apply the attached god's landing-time route effect exactly once
 * after a movement stop.  The raw route object is resolved from
 * GameState.route_object_raw[node], so pass-through nodes never call this. */
int rich4_god_apply_landing_effect_with_choice(GameState *state, int player,
    int node, int facility_choice, GameEventQueue *events);
int rich4_god_apply_landing_effect(GameState *state, int player, int node,
                                   GameEventQueue *events);

#endif
