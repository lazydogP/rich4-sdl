#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/card_panel_flow.h"
#include "core/card.h"

int main(void)
{
    GameSession session;
    CardPanelState panel;

    memset(&panel, 0xA5, sizeof(panel));
    rich4_card_panel_reset(&panel);
    assert(panel.phase == CARD_PANEL_CLOSED);
    assert(panel.selected_slot == -1 && panel.selected_card == -1);
    assert(panel.hover == -1 && panel.primary_target == -1);
    assert(panel.primary_kind == GAME_DOMAIN_NONE);
    assert(!rich4_card_panel_cancel(&panel));
    panel.phase = CARD_PANEL_FACILITY_CHOICE;
    panel.hover = 3;
    panel.effect_precedes_handler = 1;
    assert(rich4_card_panel_cancel(&panel));
    assert(panel.phase == CARD_PANEL_INVENTORY && panel.hover == -1);
    assert(panel.effect_precedes_handler == 0);
    panel.phase = CARD_PANEL_EFFECT;
    panel.selected_card = 4;
    assert(rich4_card_panel_map_target_kind(&panel) ==
           GAME_CARD_TARGET_PROPERTY);
    assert(rich4_card_panel_cancel(&panel));
    assert(panel.phase == CARD_PANEL_CLOSED && panel.selected_card == -1);

    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.state.active = 1;
    session.state.current_player = 0;
    session.presentation.has_view_override = 1;
    session.state.players[0].tool_inventory[2] = 1;
    session.state.players[0].tool_inventory[7] = 1;

    rich4_card_panel_open(&panel, &session);
    assert(panel.phase == CARD_PANEL_INVENTORY && panel.player == 0);
    assert(session.presentation.has_view_override == 0);
    panel.selected_card = 4;
    rich4_card_panel_reset(&panel);
    assert(panel.phase == CARD_PANEL_CLOSED && panel.selected_card == -1);

    assert(rich4_game_domain_ref_is_none(
        rich4_card_panel_current_property_ref(&session)));

    assert(rich4_card_panel_slot_at(19, 135) == 0);
    assert(rich4_card_panel_slot_at(99, 191) == 6);
    assert(rich4_card_panel_slot_at(419, 135) == -1);
    assert(rich4_card_panel_facility_choice_at(50, 286) == 0);
    assert(rich4_card_panel_facility_choice_at(390, 286) == -1);
    assert(rich4_card_panel_robbery_card_slot_at(19, 75) == 0);
    assert(rich4_card_panel_robbery_card_slot_at(99, 131) == 6);
    assert(rich4_card_panel_robbery_card_slot_at(19, 243) == -1);

    assert(rich4_card_panel_robbery_tool_at(&session, 0, 19, 275) == 2);
    assert(rich4_card_panel_robbery_tool_at(&session, 0, 99, 275) == 7);
    assert(rich4_card_panel_robbery_tool_at(&session, 0, 179, 275) == -1);
    assert(rich4_card_panel_robbery_tool_at(&session, 1, 19, 275) == -1);

    panel.player = 0;
    panel.selected_card = 6;
    {
        GameCommand command = rich4_card_panel_build_command(
            &panel, rich4_game_domain_ref(GAME_DOMAIN_LAND, 4),
            2, 3, GAME_CARD_FLAG_SECONDARY_FACILITY);
        assert(command.type == GAME_COMMAND_USE_CARD);
        assert(command.source == GAME_COMMAND_SOURCE_HUMAN);
        assert(command.player == 0);
        assert(command.subject.kind == GAME_DOMAIN_CARD &&
               command.subject.id == 6);
        assert(command.target.kind == GAME_DOMAIN_LAND &&
               command.target.id == 4);
        assert(command.value == 2 && command.value2 == 3);
        assert(command.flags == GAME_CARD_FLAG_SECONDARY_FACILITY);
    }
    assert(!rich4_game_command_is_valid(
        &(GameCommand){GAME_COMMAND_NONE, GAME_COMMAND_SOURCE_HUMAN, -1,
                       0, 0, 0, 0,
                       {GAME_DOMAIN_NONE, -1}, {GAME_DOMAIN_NONE, -1},
                       {GAME_DOMAIN_NONE, -1}}));

    panel.phase = CARD_PANEL_INVENTORY;
    session.state.players[0].card_inventory[0] = 1;
    {
        Rich4CardPanelSelection selection =
            rich4_card_panel_select_inventory(&panel, &session, 0);
        assert(selection.kind == RICH4_CARD_PANEL_SELECTION_PRE_EFFECT);
        assert(panel.selected_slot == 0 && panel.selected_card == 0);
    }
    session.state.players[0].card_inventory[1] = 24;
    rich4_card_panel_reset(&panel);
    panel.phase = CARD_PANEL_INVENTORY;
    panel.player = 0;
    {
        Rich4CardPanelSelection selection =
            rich4_card_panel_select_inventory(&panel, &session, 1);
        assert(selection.kind == RICH4_CARD_PANEL_SELECTION_PRE_EFFECT);
        assert(panel.selected_card == 23);
    }

    /* Card 12 uses the original mixed property/dynamic-object target mask.
     * Its effect window must lead to map targeting instead of submitting the
     * current property immediately. */
    session.state.players[0].card_inventory[3] = 12;
    rich4_card_panel_reset(&panel);
    panel.phase = CARD_PANEL_INVENTORY;
    panel.player = 0;
    {
        Rich4CardPanelSelection selection =
            rich4_card_panel_select_inventory(&panel, &session, 3);
        Rich4CardPanelAction action;
        assert(selection.kind == RICH4_CARD_PANEL_SELECTION_PRE_EFFECT);
        assert(rich4_card_panel_map_target_kind(&panel) ==
               GAME_CARD_TARGET_PROPERTY_OR_MAP_OBJECT);
        rich4_card_panel_begin_effect(&panel, 11, 100, 1);
        action = rich4_card_panel_resume_pre_effect(&panel, &session);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_MAP_TARGET);
        assert(panel.phase == CARD_PANEL_MAP_TARGET);
        action = rich4_card_panel_pointer_action(
            &panel, &session, 200, 200,
            rich4_game_domain_ref(GAME_DOMAIN_MAP_OBJECT, 20), 0, 100);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_SUBMIT);
        assert(action.command.target.kind == GAME_DOMAIN_MAP_OBJECT &&
               action.command.target.id == 20);
    }

    /* A successful property use of card 12 enters the original Data[529]
     * destruction phase; a dynamic-object target is handled by the core
     * transaction but does not get this property-only animation duration. */
    rich4_card_panel_reset(&panel);
    panel.player = 0;
    panel.selected_card = 11;
    panel.phase = CARD_PANEL_EFFECT;
    panel.effect_precedes_handler = 1;
    {
        int speech_player = -1;
        Rich4CardPanelCommandResult result =
            rich4_card_panel_finish_command(
                &panel, &session,
                rich4_game_domain_ref(GAME_DOMAIN_LAND, 2),
                200, 1380, &speech_player);
        assert(result == RICH4_CARD_PANEL_COMMAND_DEMOLISH_ANIMATION);
        assert(speech_player == -1);
        assert(panel.phase == CARD_PANEL_DEMOLISH_ANIMATION);
        assert(!rich4_card_panel_effect_due(&panel, 1579));
        assert(rich4_card_panel_effect_due(&panel, 1580));
        assert(rich4_card_panel_finish_timed_effect(
                   &panel, &speech_player) == RICH4_CARD_PANEL_TIMED_CLOSED);
        assert(panel.phase == CARD_PANEL_CLOSED);
    }
    panel.player = 0;
    panel.selected_card = 11;
    panel.phase = CARD_PANEL_EFFECT;
    panel.effect_precedes_handler = 1;
    assert(rich4_card_panel_finish_command(
               &panel, &session,
               rich4_game_domain_ref(GAME_DOMAIN_MAP_OBJECT, 20),
               200, 1380, NULL) == RICH4_CARD_PANEL_COMMAND_CLOSE);
    assert(panel.phase == CARD_PANEL_CLOSED);

    /* 请神符 resolves the nearest god after the effect window, without
     * entering a mouse-selected map phase. */
    session.state.tool_route_node_count = 4;
    session.state.players[0].node = 1;
    session.state.tool_route_x[1] = 0;
    session.state.tool_route_y[1] = 0;
    session.state.tool_route_x[2] = 300;
    session.state.tool_route_y[2] = 0;
    session.state.tool_route_x[3] = 50;
    session.state.tool_route_y[3] = 0;
    session.presentation.dynamic_objects[0].type = 1;
    session.presentation.dynamic_objects[0].node = 2;
    session.presentation.dynamic_objects[1].type = 11;
    session.presentation.dynamic_objects[1].node = 3;
    session.state.players[0].card_inventory[2] = 23;
    rich4_card_panel_reset(&panel);
    panel.phase = CARD_PANEL_INVENTORY;
    panel.player = 0;
    {
        Rich4CardPanelSelection selection =
            rich4_card_panel_select_inventory(&panel, &session, 2);
        Rich4CardPanelAction action;
        assert(selection.kind == RICH4_CARD_PANEL_SELECTION_PRE_EFFECT);
        rich4_card_panel_begin_effect(&panel, 22, 100, 1);
        action = rich4_card_panel_resume_pre_effect(&panel, &session);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_SUBMIT);
        assert(action.command.target.kind == GAME_DOMAIN_GOD &&
               action.command.target.id == 0 && action.command.value == 1);
    }

    /* Original active use of cards 18-21 still enters the human card-panel
     * effect window.  Their shared handler then returns zero, so the panel
     * comes back to inventory without submitting a command or consuming the
     * card. */
    {
        int card;
        for (card = 17; card <= 20; card++) {
            Rich4CardPanelSelection selection;
            Rich4CardPanelAction action;
            rich4_card_panel_reset(&panel);
            panel.phase = CARD_PANEL_INVENTORY;
            panel.player = 0;
            session.state.players[0].card_inventory[0] = card + 1;
            selection = rich4_card_panel_select_inventory(&panel, &session, 0);
            assert(selection.kind == RICH4_CARD_PANEL_SELECTION_PRE_EFFECT);
            assert(panel.selected_card == card);
            rich4_card_panel_begin_effect(&panel, card, 100, 1);
            action = rich4_card_panel_resume_pre_effect(&panel, &session);
            assert(action.kind == RICH4_CARD_PANEL_ACTION_INVALID_SELECTION);
            assert(action.sound == RICH4_CARD_PANEL_SOUND_INVALID);
            assert(panel.phase == CARD_PANEL_INVENTORY &&
                   panel.effect_precedes_handler == 0 &&
                   session.state.players[0].card_inventory[0] == card + 1);
        }
    }

    rich4_card_panel_reset(&panel);
    panel.phase = CARD_PANEL_INVENTORY;
    panel.player = 0;
    session.state.players[0].card_inventory[0] = 1;
    {
        Rich4CardPanelAction action = rich4_card_panel_pointer_action(
            &panel, &session, 19, 135,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, 100);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_PRE_EFFECT);
        assert(action.sound == RICH4_CARD_PANEL_SOUND_OK_CARD);
        assert(panel.phase == CARD_PANEL_EFFECT &&
               panel.effect_precedes_handler == 1);
        action = rich4_card_panel_resume_pre_effect(&panel, &session);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_SUBMIT);
        assert(action.command.subject.kind == GAME_DOMAIN_CARD &&
               action.command.subject.id == 0);
    }

    rich4_card_panel_reset(&panel);
    panel.phase = CARD_PANEL_INVENTORY;
    panel.player = 0;
    session.state.players[0].card_inventory[1] = 24;
    {
        Rich4CardPanelAction action = rich4_card_panel_pointer_action(
            &panel, &session, 99, 135,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, 100);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_PRE_EFFECT);
        assert(action.sound == RICH4_CARD_PANEL_SOUND_OK_CARD);
        assert(!rich4_card_panel_effect_due(&panel, 1599));
        assert(rich4_card_panel_effect_due(&panel, 1600));
        action = rich4_card_panel_resume_pre_effect(&panel, &session);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_STOCK_TARGET);
        assert(action.sound == RICH4_CARD_PANEL_SOUND_NONE);
        assert(panel.phase == CARD_PANEL_STOCK_TARGET);
        action = rich4_card_panel_pointer_action(
            &panel, &session, 112, 124,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, 100);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_SUBMIT);
        assert(action.target.kind == GAME_DOMAIN_STOCK &&
               action.target.id == 0);
    }

    rich4_card_panel_reset(&panel);
    panel.phase = CARD_PANEL_MAP_TARGET;
    panel.player = 0;
    panel.selected_card = 4;
    {
        Rich4CardPanelAction action = rich4_card_panel_pointer_action(
            &panel, &session, 200, 200,
            rich4_game_domain_ref(GAME_DOMAIN_LAND, 2), 0, 100);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_SUBMIT);
        assert(action.sound == RICH4_CARD_PANEL_SOUND_TARGET);
        assert(action.target.kind == GAME_DOMAIN_LAND &&
               action.target.id == 2);
    }
    panel.phase = CARD_PANEL_EFFECT;
    panel.effect_precedes_handler = 1;
    rich4_card_panel_command_rejected(&panel);
    assert(panel.phase == CARD_PANEL_INVENTORY &&
           panel.effect_precedes_handler == 0);

    /* Every populated slot shows its card before target selection or
     * handler failure (0x441CBC precedes 0x441CCD). No early consumption. */
    {
        int id;
        for (id = 1; id <= 30; id++) {
            Rich4CardPanelAction action;
            rich4_card_panel_open(&panel, &session);
            session.state.players[0].card_inventory[0] = id;
            action = rich4_card_panel_pointer_action(
                &panel, &session, 19, 135,
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, 100);
            assert(action.kind == RICH4_CARD_PANEL_ACTION_PRE_EFFECT);
            assert(panel.phase == CARD_PANEL_EFFECT);
            assert(session.state.players[0].card_inventory[0] == id);
            action = rich4_card_panel_resume_pre_effect(&panel, &session);
            assert(action.kind != RICH4_CARD_PANEL_ACTION_PRE_EFFECT);
            if (action.kind == RICH4_CARD_PANEL_ACTION_MAP_TARGET ||
                action.kind == RICH4_CARD_PANEL_ACTION_STOCK_TARGET)
                assert(panel.effect_precedes_handler);
        }
    }
    /* Successful targeted use closes without showing the card a second time. */
    rich4_card_panel_open(&panel, &session);
    session.state.players[0].card_inventory[0] = 2;
    rich4_card_panel_pointer_action(
        &panel, &session, 19, 135,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, 100);
    assert(rich4_card_panel_resume_pre_effect(&panel, &session).kind ==
           RICH4_CARD_PANEL_ACTION_MAP_TARGET);
    assert(rich4_card_panel_finish_command(
        &panel, &session, rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1),
        1600, 0, NULL) == RICH4_CARD_PANEL_COMMAND_CLOSE);
    assert(panel.phase == CARD_PANEL_CLOSED);

    /* Preserve SDL's visible-object projection, including no eligible god. */
    {
        Rich4CardPanelAction action;
        rich4_card_panel_open(&panel, &session);
        session.state.players[0].card_inventory[0] = 23;
        action = rich4_card_panel_pointer_action_with_god_target(
            &panel, &session, 19, 135,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, 100,
            rich4_game_domain_ref(GAME_DOMAIN_GOD, 1), 11);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_PRE_EFFECT);
        action = rich4_card_panel_resume_pre_effect(&panel, &session);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_SUBMIT);
        assert(action.command.target.id == 1 && action.command.value == 11);
        rich4_card_panel_open(&panel, &session);
        action = rich4_card_panel_pointer_action_with_god_target(
            &panel, &session, 19, 135,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, 100,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_PRE_EFFECT);
        action = rich4_card_panel_resume_pre_effect(&panel, &session);
        assert(action.kind == RICH4_CARD_PANEL_ACTION_INVALID_SELECTION);
        assert(panel.phase == CARD_PANEL_INVENTORY);
        assert(session.state.players[0].card_inventory[0] == 23);
    }

    puts("app card-panel flow: ok");
    return 0;
}
