#ifndef RICH4_APP_CARD_PANEL_STATE_H
#define RICH4_APP_CARD_PANEL_STATE_H

#include <stdint.h>
#include "app/animation_clock.h"

#include "core/game_domain.h"
#include "app/map_edge_scroll_state.h"

/* App-owned card panel phase and transient selection state. SDL only draws
 * the panel and supplies platform coordinates/resources. */
typedef enum CardPanelPhase {
    CARD_PANEL_CLOSED = 0,
    CARD_PANEL_INVENTORY,
    CARD_PANEL_MAP_TARGET,
    CARD_PANEL_STOCK_TARGET,
    CARD_PANEL_FACILITY_CHOICE,
    CARD_PANEL_ROBBERY_TARGET,
    CARD_PANEL_EFFECT,
    CARD_PANEL_MONSTER_ANIMATION,
    CARD_PANEL_DEMOLISH_ANIMATION
} CardPanelPhase;

typedef struct CardPanelState {
    CardPanelPhase phase;
    int player;
    int selected_slot;
    int selected_card;
    int hover;
    int primary_target;
    GameDomainKind primary_kind;
    uint32_t effect_started;
    uint32_t effect_until;
    int effect_frame;
    int effect_frame_count;
    Rich4AnimationClock effect_clock;
    int effect_precedes_handler;
    int has_projected_god;
    GameDomainRef projected_god;
    int projected_god_type;
    Rich4MapEdgeScrollState edge_scroll;
} CardPanelState;

#endif
