#include "app/card_panel_flow.h"

#include <string.h>

#include "core/card.h"
#include "core/game_constants.h"
#include "core/route_event.h"
#include "app/dynamic_object_flow.h"
#include "app/map_input_flow.h"

static int rich4_card_panel_wrap_index(int value, int count)
{
    if (count <= 0) return 0;
    value %= count;
    return value < 0 ? value + count : value;
}

/* Cards 18-21 use the original passive-only handler.  The handler returns
 * zero when reached from the active card panel, but the original human UI
 * still shows the card effect first and only then returns to the inventory
 * with the failure sound.  Keep that presentation distinction out of the
 * core catalog: AI and direct commands must continue to reject the no-op
 * handler. */
static int rich4_card_panel_is_passive_only_card(int card)
{
    return card >= 17 && card <= 20;
}

void rich4_card_panel_reset(CardPanelState *panel)
{
    if (panel == NULL) return;
    memset(panel, 0, sizeof(*panel));
    panel->selected_slot = -1;
    panel->selected_card = -1;
    panel->hover = -1;
    panel->primary_target = -1;
    panel->primary_kind = GAME_DOMAIN_NONE;
}

void rich4_card_panel_open(CardPanelState *panel, GameSession *session)
{
    if (panel == NULL || session == NULL || !session->state.active ||
        session->state.player_count <= 0) return;
    session->presentation.has_view_override = 0;
    rich4_card_panel_reset(panel);
    panel->phase = CARD_PANEL_INVENTORY;
    panel->player = rich4_card_panel_wrap_index(
        session->state.current_player, session->state.player_count);
}

int rich4_card_panel_cancel(CardPanelState *panel)
{
    if (panel == NULL || panel->phase == CARD_PANEL_CLOSED) return 0;
    if (panel->phase == CARD_PANEL_FACILITY_CHOICE) {
        panel->phase = CARD_PANEL_INVENTORY;
        rich4_card_panel_set_hover(panel, -1);
        panel->effect_precedes_handler = 0;
    } else {
        rich4_card_panel_reset(panel);
    }
    return 1;
}

GameDomainRef rich4_card_panel_current_property_ref(
    const GameSession *session)
{
    const RouteEvent *stop_event;

    if (session == NULL) return rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    stop_event = rich4_route_event_collection_last_phase(
        &session->flow.route_facts, ROUTE_EVENT_PHASE_STOP);
    if (stop_event != NULL &&
        stop_event->object_ref.kind == MAP_OBJECT_LAND &&
        stop_event->object_ref.index >= 0)
        return rich4_game_domain_ref(
            GAME_DOMAIN_LAND, stop_event->object_ref.index);
    if (stop_event != NULL &&
        stop_event->object_ref.kind == MAP_OBJECT_FACILITY &&
        stop_event->object_ref.index >= 0)
        return rich4_game_domain_ref(
            GAME_DOMAIN_FACILITY, stop_event->object_ref.index);
    return rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
}

int rich4_card_panel_slot_at(int x, int y)
{
    if (x < 19 || x >= 419 || y < 135 || y >= 303) return -1;
    return ((y - 135) / 56) * 5 + (x - 19) / 80;
}

int rich4_card_panel_facility_choice_at(int x, int y)
{
    if (x < 50 || x >= 390 || y < 286 || y >= 354) return -1;
    return (x - 50) / 68;
}

int rich4_card_panel_robbery_card_slot_at(int x, int y)
{
    if (x < 19 || x >= 419 || y < 75 || y >= 243) return -1;
    return ((y - 75) / 56) * 5 + (x - 19) / 80;
}

int rich4_card_panel_robbery_tool_at(
    const GameSession *session, int primary_target, int x, int y)
{
    int packed;
    int tool;

    if (session == NULL || primary_target < 0 ||
        primary_target >= session->state.player_count ||
        x < 19 || x >= 419 || y < 275 || y >= 443)
        return -1;

    packed = ((y - 275) / 56) * 5 + (x - 19) / 80;
    for (tool = 0; tool < GAME_TOOL_COUNT; tool++) {
        if (session->state.players[primary_target]
                .tool_inventory[tool] == 0)
            continue;
        if (packed-- == 0) return tool;
    }
    return -1;
}

int rich4_card_panel_map_target_kind(const CardPanelState *panel)
{
    const GameCardInfo *info;
    if (panel == NULL || panel->selected_card < 0) return GAME_CARD_TARGET_NONE;
    info = rich4_card_info(panel->selected_card);
    return info == NULL ? GAME_CARD_TARGET_NONE : info->target_kind;
}

void rich4_card_panel_update_edge_scroll(
    GameSession *session, CardPanelState *panel,
    int cursor_x, int cursor_y, int inset, uint32_t now)
{
    if (panel == NULL || panel->phase != CARD_PANEL_MAP_TARGET) {
        if (panel != NULL)
            rich4_app_map_edge_scroll_reset(&panel->edge_scroll);
        return;
    }
    rich4_app_map_edge_scroll_update(
        session, &panel->edge_scroll, cursor_x, cursor_y, inset, now);
}

void rich4_card_panel_set_hover(CardPanelState *panel, int hover)
{
    if (panel != NULL) panel->hover = hover;
}

static Rich4CardPanelSelection rich4_card_panel_select_inventory_internal(
    CardPanelState *panel, const GameSession *session, int slot,
    int has_projected_god, GameDomainRef projected_god,
    int projected_god_type, int resolve_handler)
{
    Rich4CardPanelSelection selection;
    const GameCardInfo *info;
    int id;

    selection.kind = RICH4_CARD_PANEL_SELECTION_INVALID;
    selection.target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    selection.value = 0;
    if (panel == NULL || session == NULL ||
        panel->phase != CARD_PANEL_INVENTORY ||
        panel->player < 0 || panel->player >= session->state.player_count ||
        slot < 0 || slot >= GAME_PLAYER_CARD_SLOTS)
        return selection;
    id = session->state.players[panel->player].card_inventory[slot];
    if (id <= 0 || id > GAME_CARD_COUNT) return selection;
    panel->selected_slot = slot;
    panel->selected_card = id - 1;
    /* 0x441BAA calls 0x441F73 before every human card handler, including
     * handlers that fail or ask for a target. Preserve the supplied map
     * projection while the modal effect holds the scene. */
    if (!resolve_handler) {
        panel->has_projected_god = has_projected_god;
        panel->projected_god = projected_god;
        panel->projected_god_type = projected_god_type;
        selection.kind = RICH4_CARD_PANEL_SELECTION_PRE_EFFECT;
        return selection;
    }
    info = rich4_card_info(panel->selected_card);
    if (info == NULL) return selection;
    if (info->implementation == GAME_CARD_UNAVAILABLE &&
        !rich4_card_panel_is_passive_only_card(panel->selected_card)) {
        selection.kind = RICH4_CARD_PANEL_SELECTION_UNAVAILABLE;
        return selection;
    }
    if (panel->selected_card == 0 || panel->selected_card == 6 ||
        panel->selected_card == 10 || panel->selected_card == 11 ||
        panel->selected_card == 12 ||
        rich4_card_panel_is_passive_only_card(panel->selected_card)) {
        selection.kind = RICH4_CARD_PANEL_SELECTION_PRE_EFFECT;
        return selection;
    }
    if (info->target_kind == GAME_CARD_TARGET_NONE) {
        selection.kind = RICH4_CARD_PANEL_SELECTION_SUBMIT;
        return selection;
    }
    if (info->target_kind == GAME_CARD_TARGET_CURRENT_PROPERTY &&
        panel->selected_card != 3 && panel->selected_card != 4) {
        selection.kind = RICH4_CARD_PANEL_SELECTION_SUBMIT;
        selection.target = rich4_card_panel_current_property_ref(session);
        selection.value = panel->selected_card == 7 ? -1 : 0;
        return selection;
    }
    if (info->target_kind == GAME_CARD_TARGET_GOD) {
        int god_type = 0;
        int object_id;
        if (has_projected_god) {
            if (projected_god.kind != GAME_DOMAIN_GOD ||
                projected_god.id < 0 || projected_god_type <= 0) {
                selection.kind = RICH4_CARD_PANEL_SELECTION_UNAVAILABLE;
                return selection;
            }
            object_id = projected_god.id;
            god_type = projected_god_type;
        } else {
            object_id = rich4_app_find_nearest_dynamic_god(
                &session->presentation, &session->state, panel->player,
                &god_type);
            if (object_id < 0) {
                selection.kind = RICH4_CARD_PANEL_SELECTION_UNAVAILABLE;
                return selection;
            }
        }
        selection.kind = RICH4_CARD_PANEL_SELECTION_SUBMIT;
        selection.target = rich4_game_domain_ref(
            GAME_DOMAIN_GOD, object_id);
        selection.value = god_type;
        return selection;
    }
    selection.kind = info->target_kind == GAME_CARD_TARGET_STOCK
        ? RICH4_CARD_PANEL_SELECTION_STOCK_TARGET
        : RICH4_CARD_PANEL_SELECTION_MAP_TARGET;
    return selection;
}

Rich4CardPanelSelection rich4_card_panel_select_inventory(
    CardPanelState *panel, const GameSession *session, int slot)
{
    return rich4_card_panel_select_inventory_internal(
        panel, session, slot, 0,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, 0);
}

void rich4_card_panel_begin_effect(
    CardPanelState *panel, int card, uint32_t now, int precedes_handler)
{
    if (panel == NULL) return;
    panel->phase = CARD_PANEL_EFFECT;
    panel->selected_card = card;
    panel->effect_precedes_handler = precedes_handler != 0;
    panel->effect_started = now;
    panel->effect_until = now + 1500;
}

static Rich4CardPanelAction rich4_card_panel_action(
    Rich4CardPanelActionKind kind, Rich4CardPanelSound sound)
{
    Rich4CardPanelAction action;
    action.kind = kind;
    action.sound = sound;
    action.command = rich4_game_command(
        GAME_COMMAND_NONE, GAME_COMMAND_SOURCE_HUMAN, -1, 0);
    action.target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    return action;
}

static Rich4CardPanelAction rich4_card_panel_submit_action(
    const CardPanelState *panel, GameDomainRef target,
    int value, int value2, unsigned int flags, Rich4CardPanelSound sound)
{
    Rich4CardPanelAction action = rich4_card_panel_action(
        RICH4_CARD_PANEL_ACTION_SUBMIT, sound);
    action.command = rich4_card_panel_build_command(
        panel, target, value, value2, flags);
    action.target = target;
    return action;
}

static int rich4_card_panel_stock_at(int x, int y)
{
    int column = (x - 112) / 130;
    int row = (y - 124) / 31;
    int stock = column * 8 + row;
    if (column < 0 || column > 1 || row < 0 || row >= 8 ||
        stock < 0 || stock >= GAME_STOCK_COUNT)
        return -1;
    return stock;
}

static Rich4CardPanelAction rich4_card_panel_pointer_action_internal(
    CardPanelState *panel, const GameSession *session, int x, int y,
    GameDomainRef map_target, int map_value, uint32_t now,
    int has_projected_god, GameDomainRef projected_god,
    int projected_god_type)
{
    Rich4CardPanelSelection selection;
    int slot;

    if (panel == NULL || session == NULL ||
        panel->phase == CARD_PANEL_CLOSED ||
        panel->phase == CARD_PANEL_EFFECT ||
        panel->phase == CARD_PANEL_MONSTER_ANIMATION ||
        panel->phase == CARD_PANEL_DEMOLISH_ANIMATION)
        return rich4_card_panel_action(
            RICH4_CARD_PANEL_ACTION_NONE, RICH4_CARD_PANEL_SOUND_NONE);

    if (panel->phase == CARD_PANEL_INVENTORY) {
        slot = rich4_card_panel_slot_at(x, y);
        if (slot < 0) return rich4_card_panel_action(
            RICH4_CARD_PANEL_ACTION_NONE, RICH4_CARD_PANEL_SOUND_NONE);
        selection = rich4_card_panel_select_inventory_internal(
            panel, session, slot, has_projected_god, projected_god,
            projected_god_type, 0);
        if (selection.kind == RICH4_CARD_PANEL_SELECTION_INVALID)
            return rich4_card_panel_action(
                RICH4_CARD_PANEL_ACTION_NONE, RICH4_CARD_PANEL_SOUND_NONE);
        if (selection.kind == RICH4_CARD_PANEL_SELECTION_UNAVAILABLE)
            return rich4_card_panel_action(
                RICH4_CARD_PANEL_ACTION_INVALID_SELECTION,
                RICH4_CARD_PANEL_SOUND_OK_INVALID);
        if (selection.kind == RICH4_CARD_PANEL_SELECTION_PRE_EFFECT) {
            rich4_card_panel_begin_effect(
                panel, panel->selected_card, now, 1);
            return rich4_card_panel_action(
                RICH4_CARD_PANEL_ACTION_PRE_EFFECT,
                RICH4_CARD_PANEL_SOUND_OK_CARD);
        }
        if (selection.kind == RICH4_CARD_PANEL_SELECTION_SUBMIT)
            return rich4_card_panel_submit_action(
                panel, selection.target, selection.value, 0, 0,
                RICH4_CARD_PANEL_SOUND_OK);
        panel->phase = selection.kind ==
            RICH4_CARD_PANEL_SELECTION_STOCK_TARGET
            ? CARD_PANEL_STOCK_TARGET : CARD_PANEL_MAP_TARGET;
        return rich4_card_panel_action(
            selection.kind == RICH4_CARD_PANEL_SELECTION_STOCK_TARGET
                ? RICH4_CARD_PANEL_ACTION_STOCK_TARGET
                : RICH4_CARD_PANEL_ACTION_MAP_TARGET,
            RICH4_CARD_PANEL_SOUND_OK);
    }

    if (panel->phase == CARD_PANEL_STOCK_TARGET) {
        int stock = rich4_card_panel_stock_at(x, y);
        if (stock < 0) return rich4_card_panel_action(
            RICH4_CARD_PANEL_ACTION_INVALID_TARGET,
            RICH4_CARD_PANEL_SOUND_INVALID);
        return rich4_card_panel_submit_action(
            panel, rich4_game_domain_ref(GAME_DOMAIN_STOCK, stock),
            0, 0, 0, RICH4_CARD_PANEL_SOUND_NONE);
    }

    if (panel->phase == CARD_PANEL_ROBBERY_TARGET) {
        int tool = rich4_card_panel_robbery_tool_at(
            session, panel->primary_target, x, y);
        slot = rich4_card_panel_robbery_card_slot_at(x, y);
        if (slot >= 0) {
            int id = session->state.players[panel->primary_target]
                .card_inventory[slot];
            if (id <= 0 || id > GAME_CARD_COUNT)
                return rich4_card_panel_action(
                    RICH4_CARD_PANEL_ACTION_NONE,
                    RICH4_CARD_PANEL_SOUND_NONE);
            panel->selected_slot = slot;
            return rich4_card_panel_submit_action(
                panel,
                rich4_game_domain_ref(
                    GAME_DOMAIN_PLAYER, panel->primary_target),
                id - 1, 0, 0, RICH4_CARD_PANEL_SOUND_NONE);
        }
        if (tool >= 0 && session->state.players[panel->primary_target]
                .tool_inventory[tool] != 0) {
            panel->selected_slot = 0x8000 | tool;
            return rich4_card_panel_submit_action(
                panel,
                rich4_game_domain_ref(
                    GAME_DOMAIN_PLAYER, panel->primary_target),
                tool, 0, GAME_CARD_FLAG_ROB_TOOL,
                RICH4_CARD_PANEL_SOUND_NONE);
        }
    return rich4_card_panel_action(
        RICH4_CARD_PANEL_ACTION_NONE, RICH4_CARD_PANEL_SOUND_NONE);
}

    if (panel->phase == CARD_PANEL_FACILITY_CHOICE) {
        int choice = rich4_card_panel_facility_choice_at(x, y);
        if (choice < 0 || choice >= 5)
            return rich4_card_panel_action(
                RICH4_CARD_PANEL_ACTION_NONE,
                RICH4_CARD_PANEL_SOUND_NONE);
        return rich4_card_panel_submit_action(
            panel,
            rich4_game_domain_ref(
                GAME_DOMAIN_FACILITY, panel->primary_target),
            0, choice, 0, RICH4_CARD_PANEL_SOUND_OK);
    }

    if (rich4_game_domain_ref_is_none(map_target))
        return rich4_card_panel_action(
            RICH4_CARD_PANEL_ACTION_INVALID_TARGET,
            RICH4_CARD_PANEL_SOUND_INVALID);
    if (panel->selected_card == 12 &&
        map_target.kind == GAME_DOMAIN_PLAYER) {
        panel->primary_target = map_target.id;
        panel->primary_kind = map_target.kind;
        panel->selected_slot = -1;
        panel->phase = CARD_PANEL_ROBBERY_TARGET;
        return rich4_card_panel_action(
            RICH4_CARD_PANEL_ACTION_ROBBERY_TARGET,
            RICH4_CARD_PANEL_SOUND_TARGET);
    }
    if (panel->selected_card == 3 || panel->selected_card == 4) {
        GameDomainRef source = rich4_card_panel_current_property_ref(session);
        unsigned int flags = source.kind == GAME_DOMAIN_FACILITY
            ? GAME_CARD_FLAG_SECONDARY_FACILITY : 0;
        return rich4_card_panel_submit_action(
            panel, map_target, source.id, 0, flags,
            RICH4_CARD_PANEL_SOUND_TARGET);
    }
    return rich4_card_panel_submit_action(
        panel, map_target, map_value, 0, 0,
        RICH4_CARD_PANEL_SOUND_TARGET);
}

Rich4CardPanelAction rich4_card_panel_pointer_action(
    CardPanelState *panel, const GameSession *session, int x, int y,
    GameDomainRef map_target, int map_value, uint32_t now)
{
    return rich4_card_panel_pointer_action_internal(
        panel, session, x, y, map_target, map_value, now, 0,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
}

Rich4CardPanelAction rich4_card_panel_pointer_action_with_god_target(
    CardPanelState *panel, const GameSession *session, int x, int y,
    GameDomainRef map_target, int map_value, uint32_t now,
    GameDomainRef projected_god, int projected_god_type)
{
    return rich4_card_panel_pointer_action_internal(
        panel, session, x, y, map_target, map_value, now, 1,
        projected_god, projected_god_type);
}

Rich4CardPanelAction rich4_card_panel_resume_pre_effect(
    CardPanelState *panel, const GameSession *session)
{
    GameDomainRef target;
    Rich4CardPanelSelection selection;

    if (panel == NULL || session == NULL ||
        !panel->effect_precedes_handler)
        return rich4_card_panel_action(
            RICH4_CARD_PANEL_ACTION_NONE, RICH4_CARD_PANEL_SOUND_NONE);
    panel->phase = CARD_PANEL_INVENTORY;
    selection = rich4_card_panel_select_inventory_internal(
        panel, session, panel->selected_slot, panel->has_projected_god,
        panel->projected_god, panel->projected_god_type, 1);
    panel->phase = CARD_PANEL_EFFECT;
    if (selection.kind == RICH4_CARD_PANEL_SELECTION_INVALID ||
        selection.kind == RICH4_CARD_PANEL_SELECTION_UNAVAILABLE) {
        rich4_card_panel_command_rejected(panel);
        return rich4_card_panel_action(
            RICH4_CARD_PANEL_ACTION_INVALID_SELECTION,
            RICH4_CARD_PANEL_SOUND_INVALID);
    }
    if (selection.kind == RICH4_CARD_PANEL_SELECTION_SUBMIT)
        return rich4_card_panel_submit_action(
            panel, selection.target, selection.value, 0, 0,
            RICH4_CARD_PANEL_SOUND_NONE);
    if (selection.kind == RICH4_CARD_PANEL_SELECTION_STOCK_TARGET) {
        panel->phase = CARD_PANEL_STOCK_TARGET;
        return rich4_card_panel_action(
            RICH4_CARD_PANEL_ACTION_STOCK_TARGET,
            RICH4_CARD_PANEL_SOUND_NONE);
    }
    if (panel->selected_card == 0)
        return rich4_card_panel_submit_action(
            panel, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            0, 0, 0, RICH4_CARD_PANEL_SOUND_NONE);
    if (rich4_card_panel_is_passive_only_card(panel->selected_card)) {
        /* This is the original fcn_004420D5 path: the effect window is
         * visible, then the active handler returns zero.  No command is
         * submitted and the card remains in the inventory. */
        panel->phase = CARD_PANEL_INVENTORY;
        panel->effect_precedes_handler = 0;
        return rich4_card_panel_action(
            RICH4_CARD_PANEL_ACTION_INVALID_SELECTION,
            RICH4_CARD_PANEL_SOUND_INVALID);
    }
    if (panel->selected_card == 6) {
        target = rich4_card_panel_current_property_ref(session);
        if (target.kind == GAME_DOMAIN_FACILITY) {
            panel->primary_target = target.id;
            panel->primary_kind = target.kind;
            rich4_card_panel_set_hover(panel, -1);
            panel->phase = CARD_PANEL_FACILITY_CHOICE;
            return rich4_card_panel_action(
                RICH4_CARD_PANEL_ACTION_NONE,
                RICH4_CARD_PANEL_SOUND_NONE);
        }
        return rich4_card_panel_submit_action(
            panel, target, 0, 0, 0, RICH4_CARD_PANEL_SOUND_NONE);
    }
    panel->phase = CARD_PANEL_MAP_TARGET;
    rich4_card_panel_set_hover(panel, -1);
    return rich4_card_panel_action(
        RICH4_CARD_PANEL_ACTION_MAP_TARGET,
        RICH4_CARD_PANEL_SOUND_NONE);
}

void rich4_card_panel_command_rejected(CardPanelState *panel)
{
    if (panel == NULL) return;
    panel->phase = CARD_PANEL_INVENTORY;
    panel->effect_precedes_handler = 0;
}

Rich4CardPanelCommandResult rich4_card_panel_finish_command(
    CardPanelState *panel, const GameSession *session, GameDomainRef target,
    uint32_t now, uint32_t animation_duration, int *out_speech_player)
{
    if (out_speech_player != NULL) *out_speech_player = -1;
    if (panel == NULL) return RICH4_CARD_PANEL_COMMAND_CLOSE;
    if (panel->selected_card == 10 && session != NULL) {
        if (target.kind == GAME_DOMAIN_LAND && target.id >= 0 &&
            target.id < session->state.land_count)
            panel->primary_target = session->state.land_owner[target.id];
        else if (target.kind == GAME_DOMAIN_FACILITY && target.id >= 0 &&
                 target.id < session->state.facility_count)
            panel->primary_target =
                session->state.facility_owner[target.id];
        else
            panel->primary_target = -1;
    }
    if (panel->effect_precedes_handler) {
        if (panel->selected_card == 10 && animation_duration > 0) {
            panel->phase = CARD_PANEL_MONSTER_ANIMATION;
            panel->effect_precedes_handler = 0;
            panel->effect_started = now;
            panel->effect_frame = 0;
            panel->effect_frame_count = (int)((animation_duration - 500u) / 80u);
            rich4_animation_clock_reset(&panel->effect_clock);
            panel->effect_until = now + animation_duration;
            return RICH4_CARD_PANEL_COMMAND_MONSTER_ANIMATION;
        }
        if (panel->selected_card == 11 &&
            (target.kind == GAME_DOMAIN_LAND ||
             target.kind == GAME_DOMAIN_FACILITY) &&
            animation_duration > 0) {
            panel->phase = CARD_PANEL_DEMOLISH_ANIMATION;
            panel->effect_precedes_handler = 0;
            panel->effect_started = now;
            panel->effect_frame = 0;
            panel->effect_frame_count = (int)((animation_duration - 500u) / 80u);
            rich4_animation_clock_reset(&panel->effect_clock);
            panel->effect_until = now + animation_duration;
            return RICH4_CARD_PANEL_COMMAND_DEMOLISH_ANIMATION;
        }
        if (out_speech_player != NULL && panel->selected_card == 10)
            *out_speech_player = panel->primary_target;
        rich4_card_panel_reset(panel);
        return RICH4_CARD_PANEL_COMMAND_CLOSE;
    }
    rich4_card_panel_begin_effect(panel, panel->selected_card, now, 0);
    return RICH4_CARD_PANEL_COMMAND_EFFECT;
}

void rich4_card_panel_update_animation(CardPanelState *panel, uint32_t now)
{
    if (panel == NULL || (panel->phase != CARD_PANEL_MONSTER_ANIMATION &&
                         panel->phase != CARD_PANEL_DEMOLISH_ANIMATION)) return;
    if (panel->effect_frame < panel->effect_frame_count) {
        if (rich4_animation_clock_step(&panel->effect_clock, now, 80))
            panel->effect_frame++;
        panel->effect_until = now + 500u;
    }
}

int rich4_card_panel_effect_due(const CardPanelState *panel, uint32_t now)
{
    if (panel == NULL ||
        (panel->phase != CARD_PANEL_EFFECT &&
         panel->phase != CARD_PANEL_MONSTER_ANIMATION &&
         panel->phase != CARD_PANEL_DEMOLISH_ANIMATION))
        return 0;
    return (int32_t)(now - panel->effect_until) >= 0;
}

Rich4CardPanelTimedResult rich4_card_panel_finish_timed_effect(
    CardPanelState *panel, int *out_speech_player)
{
    if (out_speech_player != NULL) *out_speech_player = -1;
    if (panel == NULL) return RICH4_CARD_PANEL_TIMED_CLOSED;
    if (panel->phase == CARD_PANEL_EFFECT &&
        panel->effect_precedes_handler)
        return RICH4_CARD_PANEL_TIMED_RESUME_PRE_EFFECT;
    if (panel->phase == CARD_PANEL_MONSTER_ANIMATION &&
        out_speech_player != NULL)
        *out_speech_player = panel->primary_target;
    if (panel->phase == CARD_PANEL_MONSTER_ANIMATION) {
        rich4_card_panel_reset(panel);
        return RICH4_CARD_PANEL_TIMED_MONSTER_SPEECH;
    }
    rich4_card_panel_reset(panel);
    return RICH4_CARD_PANEL_TIMED_CLOSED;
}

GameCommand rich4_card_panel_build_command(
    const CardPanelState *panel, GameDomainRef target,
    int value, int value2, unsigned int flags)
{
    if (panel == NULL)
        return rich4_game_command(
            GAME_COMMAND_NONE, GAME_COMMAND_SOURCE_HUMAN, -1, 0);
    return rich4_game_command_use_card_on_target(
        GAME_COMMAND_SOURCE_HUMAN, panel->player,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, panel->selected_card),
        target, value, value2, flags);
}
