#ifndef RICH4_APP_CARD_PANEL_FLOW_H
#define RICH4_APP_CARD_PANEL_FLOW_H

#include <stdint.h>

#include "app/card_panel_state.h"
#include "app/game_session.h"
#include "core/game_command.h"

typedef enum Rich4CardPanelSelectionKind {
    RICH4_CARD_PANEL_SELECTION_INVALID = 0,
    RICH4_CARD_PANEL_SELECTION_UNAVAILABLE,
    RICH4_CARD_PANEL_SELECTION_PRE_EFFECT,
    RICH4_CARD_PANEL_SELECTION_SUBMIT,
    RICH4_CARD_PANEL_SELECTION_STOCK_TARGET,
    RICH4_CARD_PANEL_SELECTION_MAP_TARGET
} Rich4CardPanelSelectionKind;

typedef struct Rich4CardPanelSelection {
    Rich4CardPanelSelectionKind kind;
    GameDomainRef target;
    int value;
} Rich4CardPanelSelection;

typedef enum Rich4CardPanelActionKind {
    RICH4_CARD_PANEL_ACTION_NONE = 0,
    RICH4_CARD_PANEL_ACTION_INVALID_SELECTION,
    RICH4_CARD_PANEL_ACTION_INVALID_TARGET,
    RICH4_CARD_PANEL_ACTION_PRE_EFFECT,
    RICH4_CARD_PANEL_ACTION_SUBMIT,
    RICH4_CARD_PANEL_ACTION_STOCK_TARGET,
    RICH4_CARD_PANEL_ACTION_MAP_TARGET,
    RICH4_CARD_PANEL_ACTION_ROBBERY_TARGET
} Rich4CardPanelActionKind;

typedef enum Rich4CardPanelSound {
    RICH4_CARD_PANEL_SOUND_NONE = 0,
    RICH4_CARD_PANEL_SOUND_OK,
    RICH4_CARD_PANEL_SOUND_INVALID,
    RICH4_CARD_PANEL_SOUND_OK_INVALID,
    RICH4_CARD_PANEL_SOUND_OK_CARD,
    RICH4_CARD_PANEL_SOUND_TARGET
} Rich4CardPanelSound;

typedef struct Rich4CardPanelAction {
    Rich4CardPanelActionKind kind;
    Rich4CardPanelSound sound;
    GameCommand command;
    GameDomainRef target;
} Rich4CardPanelAction;

typedef enum Rich4CardPanelCommandResult {
    RICH4_CARD_PANEL_COMMAND_CLOSE = 0,
    RICH4_CARD_PANEL_COMMAND_EFFECT,
    RICH4_CARD_PANEL_COMMAND_MONSTER_ANIMATION,
    RICH4_CARD_PANEL_COMMAND_DEMOLISH_ANIMATION
} Rich4CardPanelCommandResult;

typedef enum Rich4CardPanelTimedResult {
    RICH4_CARD_PANEL_TIMED_CLOSED = 0,
    RICH4_CARD_PANEL_TIMED_RESUME_PRE_EFFECT,
    RICH4_CARD_PANEL_TIMED_MONSTER_SPEECH
} Rich4CardPanelTimedResult;

/* App-owned card inventory/robbery hit semantics. SDL supplies only the
 * pointer coordinates; the inventory relationship is read from GameSession. */
void rich4_card_panel_reset(CardPanelState *panel);
void rich4_card_panel_open(CardPanelState *panel, GameSession *session);
int rich4_card_panel_cancel(CardPanelState *panel);
GameDomainRef rich4_card_panel_current_property_ref(
    const GameSession *session);
int rich4_card_panel_slot_at(int x, int y);
int rich4_card_panel_facility_choice_at(int x, int y);
int rich4_card_panel_robbery_card_slot_at(int x, int y);
int rich4_card_panel_robbery_tool_at(
    const GameSession *session, int primary_target, int x, int y);
int rich4_card_panel_map_target_kind(const CardPanelState *panel);
void rich4_card_panel_update_edge_scroll(
    GameSession *session, CardPanelState *panel,
    int cursor_x, int cursor_y, int inset, uint32_t now);
void rich4_card_panel_set_hover(CardPanelState *panel, int hover);
Rich4CardPanelSelection rich4_card_panel_select_inventory(
    CardPanelState *panel, const GameSession *session, int slot);
void rich4_card_panel_begin_effect(
    CardPanelState *panel, int card, uint32_t now, int precedes_handler);
Rich4CardPanelAction rich4_card_panel_pointer_action(
    CardPanelState *panel, const GameSession *session, int x, int y,
    GameDomainRef map_target, int map_value, uint32_t now);
/* SDL supplies the original global-table god target when the card is selected
 * from an active map view; an empty target means no visible eligible god. */
Rich4CardPanelAction rich4_card_panel_pointer_action_with_god_target(
    CardPanelState *panel, const GameSession *session, int x, int y,
    GameDomainRef map_target, int map_value, uint32_t now,
    GameDomainRef projected_god, int projected_god_type);
Rich4CardPanelAction rich4_card_panel_resume_pre_effect(
    CardPanelState *panel, const GameSession *session);
void rich4_card_panel_command_rejected(CardPanelState *panel);
Rich4CardPanelCommandResult rich4_card_panel_finish_command(
    CardPanelState *panel, const GameSession *session, GameDomainRef target,
    uint32_t now, uint32_t animation_duration, int *out_speech_player);
void rich4_card_panel_update_animation(CardPanelState *panel, uint32_t now);
int rich4_card_panel_effect_due(const CardPanelState *panel, uint32_t now);
Rich4CardPanelTimedResult rich4_card_panel_finish_timed_effect(
    CardPanelState *panel, int *out_speech_player);
GameCommand rich4_card_panel_build_command(
    const CardPanelState *panel, GameDomainRef target,
    int value, int value2, unsigned int flags);

#endif
