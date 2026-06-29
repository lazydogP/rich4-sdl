#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/game_presentation.h"
#include "app/game_event_presentation_snapshot.h"

static Rich4PresentationActivity activity(void)
{
    Rich4PresentationActivity value;
    memset(&value, 0, sizeof(value));
    return value;
}

int main(void)
{
    Rich4PresentationActivity value = activity();
    GamePresentationState state;
    Rich4PresentationPayload payload;
    GameState game_state;
    Rich4PresentationActivity from_state;

    rich4_presentation_save_load_panel_begin(
        &state.save_load, GAME_SAVE_LOAD_PANEL_SAVE, 0, NULL, NULL);
    assert(state.save_load.selected_row == -1);
    rich4_presentation_save_load_panel_confirm_saved(
        &state.save_load, 0, 1000);
    assert(!rich4_presentation_save_load_panel_finish_confirmation(
        &state.save_load, 1499));
    assert(state.save_load.mode == GAME_SAVE_LOAD_PANEL_SAVE);
    assert(rich4_presentation_save_load_panel_finish_confirmation(
        &state.save_load, 1500));
    assert(state.save_load.mode == GAME_SAVE_LOAD_PANEL_CLOSED);

    rich4_presentation_initialize(&state);
    assert(state.property.land == -1 && state.property.group == -1 &&
           state.dice.faces[0] == 1 && state.dice.faces[1] == 1 &&
           state.dice.faces[2] == 1 &&
           state.actors[0].first_turn_landing_pending == 1 &&
           state.top_button_pressed == -1);
    assert(rich4_presentation_mode_resolve(&value) ==
           RICH4_PRESENTATION_NONE);
    state = (GamePresentationState){ 0 };
    game_state = (GameState){ 0 };
    state.god_acquisition.active = 1;
    assert(rich4_presentation_update_step(&state, &game_state) ==
           RICH4_PRESENTATION_UPDATE_GOD);
    state.god_acquisition.active = 0;
    state.bank_settlement.active = 1;
    assert(rich4_presentation_update_step(&state, &game_state) ==
           RICH4_PRESENTATION_UPDATE_BANK);
    state.bank_settlement.active = 0;
    state.route_interaction.active = 1;
    state.route_interaction.accept.type = GAME_COMMAND_BANK_APPLY_LOAN;
    assert(rich4_presentation_update_step(&state, &game_state) ==
           RICH4_PRESENTATION_UPDATE_BANK);
    state.route_interaction.active = 0;
    state.tool_placement_animation.active = 1;
    assert(rich4_presentation_update_step(&state, &game_state) ==
           RICH4_PRESENTATION_UPDATE_TOOL_PLACEMENT);
    state.tool_placement_animation.active = 0;
    state.speech.active = 1;
    state.tool_animation_count = 1;
    assert(rich4_presentation_update_step(&state, &game_state) ==
           RICH4_PRESENTATION_UPDATE_PROPERTY_SPEECH);
    state.speech.active = 0;
    assert(rich4_presentation_update_step(&state, &game_state) ==
           RICH4_PRESENTATION_UPDATE_TOOL_QUEUE);
    state.tool_animation_count = 0;
    state.tool_trigger_turn_pending = 1;
    assert(rich4_presentation_update_step(&state, &game_state) ==
           RICH4_PRESENTATION_UPDATE_TOOL_TRIGGER);
    state.tool_trigger_turn_pending = 0;
    game_state.tool_robot.active = 1;
    assert(rich4_presentation_update_step(&state, &game_state) ==
           RICH4_PRESENTATION_UPDATE_TOOL_ROBOT);
    state.speech.active = 1;
    assert(rich4_presentation_update_step(&state, &game_state) ==
           RICH4_PRESENTATION_UPDATE_PROPERTY_SPEECH);
    state.speech.active = 0;
    game_state.tool_robot.active = 0;
    assert(rich4_presentation_update_step(&state, &game_state) ==
           RICH4_PRESENTATION_UPDATE_FEATURES);
    value.speech_active = 1;
    assert(rich4_presentation_mode_resolve(&value) ==
           RICH4_PRESENTATION_SPEECH);
    value.god_acquisition_active = 1;
    assert(rich4_presentation_mode_resolve(&value) ==
           RICH4_PRESENTATION_GOD);
    value.god_acquisition_active = 0;
    value.lottery_active = 1;
    assert(rich4_presentation_mode_resolve(&value) ==
           RICH4_PRESENTATION_LOTTERY);
    value.tool_animation_queue_active = 1;
    assert(rich4_presentation_mode_resolve(&value) ==
           RICH4_PRESENTATION_TOOL_ANIMATION);
    value.auction_active = 1;
    assert(rich4_presentation_mode_resolve(&value) ==
           RICH4_PRESENTATION_AUCTION);
    value.auction_active = 0;
    value.bank_settlement_active = 1;
    assert(rich4_presentation_mode_resolve(&value) ==
           RICH4_PRESENTATION_BANK);
    value.bank_settlement_active = 0;
    value.special_actor_active = 1;
    assert(rich4_presentation_mode_resolve(&value) ==
           RICH4_PRESENTATION_TOOL_ANIMATION);
    value.special_actor_active = 0;
    value.bank_settlement_active = 0;
    value.tool_animation_queue_active = 0;
    value.lottery_active = 0;
    value.speech_active = 0;
    value.dice_popup_active = 1;
    assert(rich4_presentation_mode_resolve(&value) ==
           RICH4_PRESENTATION_DICE);
    value.dice_popup_active = 0;

    memset(&state, 0, sizeof(state));
    memset(&game_state, 0, sizeof(game_state));
    state.bank_rejection.active = 1;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_BANK) ==
           RICH4_PRESENTATION_HANDLER_BANK_REJECTION);
    state.bank_rejection.active = 0;
    state.special_actor.active = 1;
    state.speech.active = 1;
    state.tool_animation_count = 1;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_TOOL_ANIMATION) ==
           RICH4_PRESENTATION_HANDLER_SPECIAL_ACTOR);
    state.special_actor.active = 0;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_TOOL_ANIMATION) ==
           RICH4_PRESENTATION_HANDLER_PROPERTY_SPEECH);
    state.speech.active = 0;
    state.monster_card.active = 1;
    state.special_reward.active = 1;
    state.special_event.active = 1;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_PROPERTY) ==
           RICH4_PRESENTATION_HANDLER_MONSTER_CARD);
    state.monster_card.active = 0;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_PROPERTY) ==
           RICH4_PRESENTATION_HANDLER_SPECIAL_REWARD);
    state.special_reward.active = 0;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_PROPERTY) ==
           RICH4_PRESENTATION_HANDLER_SPECIAL_EVENT);
    state.special_event.active = 0;
    state.confinement_exit.active = 1;
    state.confinement_visit.active = 1;
    state.confinement_status.active = 1;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_CONFINEMENT) ==
           RICH4_PRESENTATION_HANDLER_CONFINEMENT_EXIT);
    state.confinement_exit.active = 0;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_CONFINEMENT) ==
           RICH4_PRESENTATION_HANDLER_CONFINEMENT_VISIT);
    state.confinement_visit.active = 0;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_CONFINEMENT) ==
           RICH4_PRESENTATION_HANDLER_CONFINEMENT_STATUS);
    state.confinement_status.active = 0;
    state.god_acquisition.active = 1;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_GOD) ==
           RICH4_PRESENTATION_HANDLER_GOD);
    state.god_acquisition.active = 0;
    state.facility_roulette.active = 1;
    state.company_issue.active = 1;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_ROULETTE) ==
           RICH4_PRESENTATION_HANDLER_FACILITY_ROULETTE);
    state.facility_roulette.active = 0;
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_ROULETTE) ==
           RICH4_PRESENTATION_HANDLER_COMPANY_ISSUE);
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_NONE) ==
           RICH4_PRESENTATION_HANDLER_NONE);
    assert(rich4_presentation_handler_for_mode(
               &state, RICH4_PRESENTATION_AUCTION) ==
           RICH4_PRESENTATION_HANDLER_AUCTION);
    state.dice.popup_active = 1;
    state.auction.active = 1;
    state.god_acquisition.active = 1;
    game_state.tool_robot.active = 1;
    from_state = rich4_presentation_activity_from_state(&state, &game_state);
    assert(from_state.dice_popup_active == 1 &&
           from_state.auction_active == 1 &&
           from_state.tool_robot_active == 1 &&
           from_state.god_acquisition_active == 1);
    state.dice.popup_active = 0;
    state.god_acquisition.active = 0;
    game_state.tool_robot.active = 0;
    memset(&payload, 0, sizeof(payload));
    payload.speech.player = 2;
    rich4_presentation_transition(&state, RICH4_PRESENTATION_SPEECH,
                                  &payload);
    assert(state.mode == RICH4_PRESENTATION_SPEECH &&
           state.previous_mode == RICH4_PRESENTATION_NONE &&
           state.transition_serial == 1 && state.payload.speech.player == 2);
    rich4_presentation_transition(&state, RICH4_PRESENTATION_SPEECH, NULL);
    assert(state.transition_serial == 1);
    rich4_presentation_transition(&state, RICH4_PRESENTATION_NONE, NULL);
    assert(state.mode == RICH4_PRESENTATION_NONE &&
           state.previous_mode == RICH4_PRESENTATION_SPEECH &&
           state.transition_serial == 2 && state.payload.speech.player == 0);
    rich4_presentation_set_view_override(&state, 123, 456);
    assert(state.view_world_x == 123);
    assert(state.view_world_y == 456);
    assert(state.has_view_override);
    rich4_presentation_clear_view_override(&state);
    assert(!state.has_view_override);

    game_state.stage = 7;
    state.special_event.active = 1;
    {
        Rich4GameEventPresentationSnapshot snapshot =
            rich4_app_build_game_event_presentation_snapshot(
                &game_state, &state);
        assert(snapshot.state == &game_state);
        assert(snapshot.presentation == &state);
        game_state.stage = 9;
        state.special_event.active = 0;
        assert(snapshot.state->stage == 9);
        assert(snapshot.presentation->special_event.active == 0);
    }
    puts("presentation mode policy: ok");
    return 0;
}
