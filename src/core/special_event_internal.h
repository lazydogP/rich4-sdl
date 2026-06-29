#ifndef RICH4_CORE_SPECIAL_EVENT_INTERNAL_H
#define RICH4_CORE_SPECIAL_EVENT_INTERNAL_H

#include "core/special_event.h"

#define RICH4_PRESENTATION_TARGET_ID_MASK 0x0000ffffu
#define RICH4_PRESENTATION_PLAYER_SHIFT 16
#define RICH4_PRESENTATION_PLAYER_MASK 0x00ff0000u
#define RICH4_PRESENTATION_FACILITY_BIT 0x80000000u

int rich4_special_event_active_player(const GameState *state, int player);
int rich4_special_event_active_unconfined_player(const GameState *state, int player);
int rich4_special_event_news_confinement_target(
    const GameState *state, int player, int prison);
int rich4_special_event_company_operator(const GameState *state, int company);
void rich4_special_event_return_active_vehicle(GameState *state, int player);
int rich4_special_event_special_random(GameState *state, int upper);
int rich4_special_event_choose_property(GameState *state, int owner_mode,
                                        int built_only, int land_only,
                                        GameDomainRef *out);
void rich4_special_event_update_fixed_asset(GameState *state, int owner, int delta);
void rich4_special_event_clear_property(GameState *state, GameDomainRef target);
void rich4_special_event_downgrade_property(GameState *state, GameDomainRef target);
unsigned int rich4_special_event_presentation_property_target(
    const GameState *state, GameDomainRef target);
void rich4_special_event_scale_property_price(GameState *state,
                                              GameDomainRef target,
                                              int numerator, int denominator);
/* Binary-double constants, x87 64-bit significand, then CHP truncation. */
int rich4_special_event_original_scale(int value, int numerator, int denominator);
int rich4_special_event_original_stock_tax(float value);
int rich4_special_event_extreme_player(const GameState *state, int property,
                                       int highest);
int rich4_special_event_add_money(GameState *state, int player, int amount,
                                  int to_bank);
int rich4_special_event_news_eligible(const GameState *state, int actor, int id);
int rich4_special_event_fortune_eligible(const GameState *state, int actor,
                                         int logical, int *out_value);
int rich4_special_event_emit_prepared(GameEventQueue *queue,
                                      const GameState *state);
void rich4_special_event_set_pending(GameState *state, int family, int actor,
                                     int logical, int resolved, int resource,
                                     int panel, int value, unsigned int targets,
                                     int cancelable, int phase);
int rich4_special_event_magic_sell_cards(GameState *state, int player);
int rich4_special_event_magic_sell_tools(GameState *state, int player);

#endif
