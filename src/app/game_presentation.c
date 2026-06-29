#include "app/game_presentation.h"

#include <string.h>

#include "view/view_constants.h"

void rich4_presentation_save_load_panel_begin(
    GameSaveLoadPresentationState *state, GameSaveLoadPanelMode mode,
    unsigned int valid_slot_mask, const int slot_elapsed_days[6],
    const int slot_player_counts[6])
{
    int slot;
    if (state == NULL) return;
    state->mode = mode;
    state->selected_row = -1;
    state->pressed_row = -1;
    state->save_confirmation_active = 0;
    state->save_confirmation_started = 0;
    state->terminal_previous_player = -1;
    state->valid_slot_mask = valid_slot_mask;
    for (slot = 0; slot < 6; slot++) {
        state->slot_elapsed_days[slot] = slot_elapsed_days != NULL
            ? slot_elapsed_days[slot] : 0;
        state->slot_player_counts[slot] = slot_player_counts != NULL
            ? slot_player_counts[slot] : 0;
        if (mode == GAME_SAVE_LOAD_PANEL_LOAD &&
            state->selected_row < 0 &&
            (valid_slot_mask & (1u << slot)) != 0)
            state->selected_row = slot;
    }
}

void rich4_presentation_save_load_panel_set_metadata(
    GameSaveLoadPresentationState *state,
    const GameSaveLoadSlotMetadata metadata[6])
{
    int slot;
    if (state == NULL || metadata == NULL) return;
    for (slot = 0; slot < 6; slot++) {
        int i;
        state->slot_year[slot] = metadata[slot].year;
        state->slot_month[slot] = metadata[slot].month;
        state->slot_day[slot] = metadata[slot].day;
        state->slot_stage[slot] = metadata[slot].stage;
        state->slot_map[slot] = metadata[slot].map;
        for (i = 0; i < GAME_MAX_PLAYERS; i++)
            state->slot_characters[slot][i] = metadata[slot].characters[i];
    }
}

void rich4_presentation_save_load_panel_close(
    GameSaveLoadPresentationState *state)
{
    if (state == NULL) return;
    state->mode = GAME_SAVE_LOAD_PANEL_CLOSED;
    state->selected_row = -1;
    state->pressed_row = -1;
    state->save_confirmation_active = 0;
    state->terminal_previous_player = -1;
    state->valid_slot_mask = 0;
}

void rich4_presentation_save_load_panel_set_mode(
    GameSaveLoadPresentationState *state, GameSaveLoadPanelMode mode)
{
    if (state != NULL) state->mode = mode;
}

void rich4_presentation_save_load_panel_press(
    GameSaveLoadPresentationState *state, int row)
{
    if (state == NULL) return;
    state->pressed_row = row;
    if (row >= 0) state->selected_row = row;
}

void rich4_presentation_save_load_panel_select(
    GameSaveLoadPresentationState *state, int row)
{
    if (state != NULL) state->selected_row = row;
}

void rich4_presentation_save_load_panel_confirm_saved(
    GameSaveLoadPresentationState *state, int row, uint32_t now)
{
    if (state == NULL || state->mode != GAME_SAVE_LOAD_PANEL_SAVE) return;
    /* The original redraws the slots after writing without repainting the
     * hover outline during the confirmation hold. */
    state->selected_row = -1;
    (void)row;
    state->pressed_row = -1;
    state->save_confirmation_active = 1;
    state->save_confirmation_started = now;
}

int rich4_presentation_save_load_panel_finish_confirmation(
    GameSaveLoadPresentationState *state, uint32_t now)
{
    if (state == NULL || !state->save_confirmation_active ||
        (uint32_t)(now - state->save_confirmation_started) < 500u)
        return 0;
    rich4_presentation_save_load_panel_close(state);
    return 1;
}

int rich4_presentation_take_auto_save_pending(
    GamePresentationState *state)
{
    int pending;
    if (state == NULL) return 0;
    pending = state->auto_save_pending;
    state->auto_save_pending = 0;
    return pending;
}

static int presentation_speech_matches_snapshot(
    const Rich4CharacterSpeechSnapshot *snapshot,
    const Rich4CharacterSpeech *speech)
{
    if (speech == NULL) return snapshot != NULL && snapshot->valid == 0;
    return rich4_character_speech_snapshot_is_valid(snapshot) &&
        speech->speaking_resource == snapshot->speaking_resource &&
        speech->big5 != NULL && strcmp(speech->big5, snapshot->big5) == 0;
}

int rich4_presentation_speech_saveable(
    const GamePresentationState *state)
{
    int i;
    if (state == NULL) return 0;
    if (!presentation_speech_matches_snapshot(
            &state->speech.current_snapshot, state->speech.current) ||
        (state->speech.active && state->speech.current == NULL))
        return 0;
    if (state->speech.last_text == NULL) {
        if (state->speech.last_text_snapshot.valid != 0) return 0;
    } else if (!rich4_character_speech_snapshot_is_valid(
                   &state->speech.last_text_snapshot) ||
               strcmp(state->speech.last_text,
                      state->speech.last_text_snapshot.big5) != 0) {
        return 0;
    }
    if (!presentation_speech_matches_snapshot(
            &state->confinement_visit.speech_snapshot,
            state->confinement_visit.speech) ||
        (state->confinement_visit.speech_active &&
         state->confinement_visit.speech == NULL))
        return 0;
    if (!presentation_speech_matches_snapshot(
            &state->bank_interaction.speech_snapshot,
            state->bank_interaction.speech))
        return 0;
    if (state->tool_animation_count < 0 ||
        state->tool_animation_count > GAME_EVENT_QUEUE_CAPACITY ||
        state->tool_animation_head < 0 ||
        state->tool_animation_head >= GAME_EVENT_QUEUE_CAPACITY)
        return 0;
    for (i = 0; i < state->tool_animation_count; i++) {
        int slot = (state->tool_animation_head + i) %
            GAME_EVENT_QUEUE_CAPACITY;
        const GameToolAnimationRequest *request =
            &state->tool_animation[slot];
        if (!request->active) return 0;
        if (request->animation_done != 0 && request->animation_done != 1)
            return 0;
        if (request->after_speech_player >= 0) {
            if (!presentation_speech_matches_snapshot(
                    &request->after_speech_snapshot,
                    request->after_speech))
                return 0;
        } else if (request->after_speech != NULL ||
                   request->after_speech_snapshot.valid != 0) {
            return 0;
        }
        if (request->after_speech_next_player >= 0) {
            if (!presentation_speech_matches_snapshot(
                    &request->after_speech_next_snapshot,
                    request->after_speech_next))
                return 0;
        } else if (request->after_speech_next != NULL ||
                   request->after_speech_next_snapshot.valid != 0) {
            return 0;
        }
    }
    return 1;
}

int rich4_presentation_rebind_speech(GamePresentationState *state)
{
    int i;
    GameSpeechPresentationState *speech;
    GameConfinementVisitPresentationState *visit;
    GameBankInteractionPresentationState *bank;
    if (state == NULL) return 0;
    speech = &state->speech;
    speech->current = NULL;
    speech->last_text = NULL;
    speech->last_text_rehydrated = 0;
    if (speech->current_snapshot.valid != 0) {
        if (!rich4_character_speech_snapshot_restore(
                &speech->current_snapshot, &speech->restored_current))
            return 0;
        speech->current = &speech->restored_current;
    }
    if (speech->active && speech->current == NULL) return 0;
    if (speech->last_text_snapshot.valid != 0) {
        if (!rich4_character_speech_snapshot_is_valid(
                &speech->last_text_snapshot)) return 0;
        speech->last_text = speech->last_text_snapshot.big5;
        speech->last_text_rehydrated = 1;
    }
    visit = &state->confinement_visit;
    visit->speech = NULL;
    if (visit->speech_snapshot.valid != 0) {
        if (!rich4_character_speech_snapshot_restore(
                &visit->speech_snapshot, &visit->restored_speech))
            return 0;
        visit->speech = &visit->restored_speech;
    }
    if (visit->speech_active && visit->speech == NULL) return 0;
    bank = &state->bank_interaction;
    bank->speech = NULL;
    if (bank->speech_snapshot.valid != 0) {
        if (!rich4_character_speech_snapshot_restore(
                &bank->speech_snapshot, &bank->restored_speech))
            return 0;
        bank->speech = &bank->restored_speech;
    }
    if (state->tool_animation_count < 0 ||
        state->tool_animation_count > GAME_EVENT_QUEUE_CAPACITY ||
        state->tool_animation_head < 0 ||
        state->tool_animation_head >= GAME_EVENT_QUEUE_CAPACITY)
        return 0;
    for (i = 0; i < GAME_EVENT_QUEUE_CAPACITY; i++) {
        state->tool_animation[i].after_speech = NULL;
        state->tool_animation[i].after_speech_next = NULL;
    }
    for (i = 0; i < state->tool_animation_count; i++) {
        int slot = (state->tool_animation_head + i) %
            GAME_EVENT_QUEUE_CAPACITY;
        GameToolAnimationRequest *request = &state->tool_animation[slot];
        if (!request->active) return 0;
        request->after_speech = NULL;
        if (request->after_speech_player >= 0) {
            if (!rich4_character_speech_snapshot_restore(
                    &request->after_speech_snapshot,
                    &request->restored_after_speech))
                return 0;
            request->after_speech = &request->restored_after_speech;
        }
        if (request->after_speech_next_player >= 0) {
            if (!rich4_character_speech_snapshot_restore(
                    &request->after_speech_next_snapshot,
                    &request->restored_after_speech_next))
                return 0;
            request->after_speech_next =
                &request->restored_after_speech_next;
        }
    }
    return 1;
}

void rich4_presentation_initialize(GamePresentationState *state)
{
    int i;

    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->property.property_kind = GAME_DOMAIN_NONE;
    state->property.land = -1;
    state->property.group = -1;
    state->bankruptcy.player = -1;
    state->bankruptcy.frame_delay_ms = 100;
    state->auction.pressed_action = -1;
    state->dice.status_x = GAME_DICE_STATUS_X;
    state->dice.status_y = GAME_DICE_STATUS_Y;
    state->dice.faces[0] = 1;
    state->dice.faces[1] = 1;
    state->dice.faces[2] = 1;
    state->active_info_tab = 0;
    state->top_button_pressed = -1;
    for (i = 0; i < GAME_MAX_PLAYERS; i++)
        state->actors[i].first_turn_landing_pending = 1;
}

/* The order is the observed modal priority from the legacy frame update.
 * Keep this table-driven policy separate from SDL and feature handlers so
 * simultaneous flags have one testable owner during the migration. */
Rich4PresentationMode rich4_presentation_mode_resolve(
    const Rich4PresentationActivity *activity)
{
    if (activity == NULL) return RICH4_PRESENTATION_NONE;
    if (activity->god_acquisition_active)
        return RICH4_PRESENTATION_GOD;
    if (activity->bank_settlement_active || activity->bank_loan_due_active)
        return RICH4_PRESENTATION_BANK;
    if (activity->special_place_notice_active)
        return RICH4_PRESENTATION_PROPERTY;
    if (activity->bankruptcy_active)
        return RICH4_PRESENTATION_BANKRUPTCY;
    if (activity->auction_active)
        return RICH4_PRESENTATION_AUCTION;
    /* 0x44BA63 is blocking: finish the claim before the next admission. */
    if (activity->insurance_notice_active)
        return RICH4_PRESENTATION_INSURANCE;
    if (activity->tool_placement_active || activity->property_speech_waiting ||
        activity->tool_animation_queue_active ||
        activity->tool_trigger_turn_pending || activity->tool_robot_active)
        return RICH4_PRESENTATION_TOOL_ANIMATION;
    if (activity->special_actor_active)
        return RICH4_PRESENTATION_TOOL_ANIMATION;
    if (activity->confinement_exit_active)
        return RICH4_PRESENTATION_CONFINEMENT;
    if (activity->monster_card_active || activity->special_reward_active)
        return RICH4_PRESENTATION_PROPERTY;
    if (activity->bank_rejection_active)
        return RICH4_PRESENTATION_BANK;
    if (activity->special_event_active)
        return RICH4_PRESENTATION_PROPERTY;
    if (activity->lottery_active)
        return RICH4_PRESENTATION_LOTTERY;
    if (activity->confinement_visit_active)
        return RICH4_PRESENTATION_CONFINEMENT;
    if (activity->point_minigame_active)
        return RICH4_PRESENTATION_POINT_MINIGAME;
    if (activity->shop_active)
        return RICH4_PRESENTATION_SHOP;
    if (activity->facility_roulette_active || activity->company_issue_active)
        return RICH4_PRESENTATION_ROULETTE;
    if (activity->magic_house_active)
        return RICH4_PRESENTATION_MAGIC_HOUSE;
    if (activity->speech_active)
        return RICH4_PRESENTATION_SPEECH;
    if (activity->confinement_status_active)
        return RICH4_PRESENTATION_CONFINEMENT;
    if (activity->dice_popup_active)
        return RICH4_PRESENTATION_DICE;
    return RICH4_PRESENTATION_NONE;
}

Rich4PresentationActivity rich4_presentation_activity_from_state(
    const GamePresentationState *presentation, const GameState *state)
{
    Rich4PresentationActivity activity;
    memset(&activity, 0, sizeof(activity));
    if (presentation == NULL) return activity;
    activity.bank_settlement_active = presentation->bank_settlement.active;
    if (presentation->periodic_finance.active && !presentation->bankruptcy.active &&
        !presentation->bankruptcy.awaiting_auction && !presentation->auction.active)
        activity.bank_settlement_active = 1;
    activity.bank_loan_due_active = presentation->bank_loan_due.active;
    activity.tool_placement_active =
        presentation->tool_placement_animation.active;
    activity.property_speech_waiting = presentation->speech.active &&
        (presentation->tool_animation_count > 0 ||
         presentation->tool_trigger_turn_pending);
    activity.tool_animation_queue_active =
        presentation->tool_animation_count > 0 ||
        presentation->engineering_attack.active ||
        presentation->engineering_attack.pending_return;
    activity.tool_trigger_turn_pending = presentation->tool_trigger_turn_pending;
    activity.tool_robot_active = state != NULL && state->tool_robot.active;
    activity.special_actor_active = presentation->special_actor.active;
    activity.confinement_exit_active = presentation->confinement_exit.active;
    activity.monster_card_active = presentation->monster_card.active;
    activity.special_reward_active = presentation->special_reward.active;
    activity.bank_rejection_active = presentation->bank_rejection.active;
    activity.special_place_notice_active =
        presentation->special_place_notice.queue.count > 0;
    activity.insurance_notice_active = presentation->insurance.count > 0 &&
        presentation->insurance.queue[presentation->insurance.head].kind !=
            GAME_INSURANCE_NOTICE_WAIT_SPECIAL;
    activity.special_event_active = presentation->special_event.active;
    activity.lottery_active = presentation->lottery.active;
    activity.confinement_visit_active = presentation->confinement_visit.active;
    activity.point_minigame_active = presentation->point_minigame.active;
    activity.shop_active = presentation->shop_visit.active;
    activity.facility_roulette_active =
        presentation->facility_roulette.active;
    activity.company_issue_active = presentation->company_issue.active;
    activity.magic_house_active = presentation->magic_house.active;
    activity.bankruptcy_active = presentation->bankruptcy.active;
    activity.auction_active = presentation->auction.active;
    activity.god_acquisition_active = rich4_presentation_god_owns_frame(presentation);
    activity.speech_active = presentation->speech.active;
    activity.confinement_status_active =
        presentation->confinement_status.active;
    activity.dice_popup_active = presentation->dice.popup_active;
    return activity;
}

Rich4PresentationUpdateStep rich4_presentation_update_step(
    const GamePresentationState *presentation, const GameState *state)
{
    if (presentation == NULL) return RICH4_PRESENTATION_UPDATE_FEATURES;
    if (presentation->special_place_notice.queue.count > 0)
        return RICH4_PRESENTATION_UPDATE_FEATURES;
    if (rich4_presentation_god_owns_frame(presentation))
        return RICH4_PRESENTATION_UPDATE_GOD;
    if ((presentation->periodic_finance.active && !presentation->bankruptcy.active &&
         !presentation->bankruptcy.awaiting_auction && !presentation->auction.active) ||
        presentation->bank_settlement.active ||
        presentation->bank_loan_due.active ||
        (presentation->route_interaction.active &&
         presentation->route_interaction.accept.type ==
             GAME_COMMAND_BANK_APPLY_LOAN))
        return RICH4_PRESENTATION_UPDATE_BANK;
    if (presentation->tool_placement_animation.active)
        return RICH4_PRESENTATION_UPDATE_TOOL_PLACEMENT;
    if (presentation->insurance.count > 0 &&
        presentation->insurance.queue[presentation->insurance.head].kind !=
            GAME_INSURANCE_NOTICE_WAIT_SPECIAL)
        return RICH4_PRESENTATION_UPDATE_FEATURES;
    if (presentation->speech.active &&
        (presentation->tool_animation_count > 0 ||
         presentation->tool_trigger_turn_pending ||
         (state != NULL && state->tool_robot.active)))
        return RICH4_PRESENTATION_UPDATE_PROPERTY_SPEECH;
    if (presentation->tool_animation_count > 0)
        return RICH4_PRESENTATION_UPDATE_TOOL_QUEUE;
    if (presentation->tool_trigger_turn_pending)
        return RICH4_PRESENTATION_UPDATE_TOOL_TRIGGER;
    if (state != NULL && state->tool_robot.active)
        return RICH4_PRESENTATION_UPDATE_TOOL_ROBOT;
    if (presentation->engineering_attack.active)
        return RICH4_PRESENTATION_UPDATE_ENGINEERING_ATTACK;
    return RICH4_PRESENTATION_UPDATE_FEATURES;
}

Rich4PresentationHandler rich4_presentation_handler_for_mode(
    const GamePresentationState *presentation, Rich4PresentationMode mode)
{
    if (presentation == NULL) return RICH4_PRESENTATION_HANDLER_NONE;
    switch (mode) {
    case RICH4_PRESENTATION_BANK:
        return presentation->bank_rejection.active
            ? RICH4_PRESENTATION_HANDLER_BANK_REJECTION
            : RICH4_PRESENTATION_HANDLER_NONE;
    case RICH4_PRESENTATION_TOOL_ANIMATION:
        if (presentation->special_actor.active)
            return RICH4_PRESENTATION_HANDLER_SPECIAL_ACTOR;
        if (presentation->speech.active &&
            (presentation->tool_animation_count > 0 ||
             presentation->tool_trigger_turn_pending))
            return RICH4_PRESENTATION_HANDLER_PROPERTY_SPEECH;
        return RICH4_PRESENTATION_HANDLER_NONE;
    case RICH4_PRESENTATION_PROPERTY:
        if (presentation->special_place_notice.queue.count > 0)
            return RICH4_PRESENTATION_HANDLER_SPECIAL_PLACE_NOTICE;
        if (presentation->monster_card.active)
            return RICH4_PRESENTATION_HANDLER_MONSTER_CARD;
        if (presentation->special_reward.active)
            return RICH4_PRESENTATION_HANDLER_SPECIAL_REWARD;
        if (presentation->special_event.active)
            return RICH4_PRESENTATION_HANDLER_SPECIAL_EVENT;
        return RICH4_PRESENTATION_HANDLER_NONE;
    case RICH4_PRESENTATION_INSURANCE:
        return RICH4_PRESENTATION_HANDLER_INSURANCE;
    case RICH4_PRESENTATION_LOTTERY:
        return RICH4_PRESENTATION_HANDLER_LOTTERY;
    case RICH4_PRESENTATION_CONFINEMENT:
        if (presentation->confinement_exit.active)
            return RICH4_PRESENTATION_HANDLER_CONFINEMENT_EXIT;
        if (presentation->confinement_visit.active)
            return RICH4_PRESENTATION_HANDLER_CONFINEMENT_VISIT;
        if (presentation->confinement_status.active)
            return RICH4_PRESENTATION_HANDLER_CONFINEMENT_STATUS;
        return RICH4_PRESENTATION_HANDLER_NONE;
    case RICH4_PRESENTATION_POINT_MINIGAME:
        return RICH4_PRESENTATION_HANDLER_POINT_MINIGAME;
    case RICH4_PRESENTATION_SHOP:
        return RICH4_PRESENTATION_HANDLER_SHOP;
    case RICH4_PRESENTATION_ROULETTE:
        if (presentation->facility_roulette.active)
            return RICH4_PRESENTATION_HANDLER_FACILITY_ROULETTE;
        if (presentation->company_issue.active)
            return RICH4_PRESENTATION_HANDLER_COMPANY_ISSUE;
        return RICH4_PRESENTATION_HANDLER_NONE;
    case RICH4_PRESENTATION_MAGIC_HOUSE:
        return RICH4_PRESENTATION_HANDLER_MAGIC_HOUSE;
    case RICH4_PRESENTATION_BANKRUPTCY:
        return RICH4_PRESENTATION_HANDLER_BANKRUPTCY;
    case RICH4_PRESENTATION_AUCTION:
        return RICH4_PRESENTATION_HANDLER_AUCTION;
    case RICH4_PRESENTATION_GOD:
        return RICH4_PRESENTATION_HANDLER_GOD;
    case RICH4_PRESENTATION_SPEECH:
        return RICH4_PRESENTATION_HANDLER_SPEECH;
    case RICH4_PRESENTATION_NONE:
    case RICH4_PRESENTATION_DICE:
    default:
        return RICH4_PRESENTATION_HANDLER_NONE;
    }
}

void rich4_presentation_transition(GamePresentationState *state,
                                   Rich4PresentationMode mode,
                                   const Rich4PresentationPayload *payload)
{
    if (state == NULL) return;
    if (state->mode != mode) {
        state->previous_mode = state->mode;
        state->mode = mode;
        state->transition_serial++;
    }
    if (payload != NULL) state->payload = *payload;
    else if (mode == RICH4_PRESENTATION_NONE)
        memset(&state->payload, 0, sizeof(state->payload));
}

void rich4_presentation_set_view_override(
    GamePresentationState *state, int world_x, int world_y)
{
    if (state == NULL) return;
    state->view_world_x = world_x;
    state->view_world_y = world_y;
    state->has_view_override = 1;
}

void rich4_presentation_set_view_position(
    GamePresentationState *state, int world_x, int world_y)
{
    if (state == NULL) return;
    state->view_world_x = world_x;
    state->view_world_y = world_y;
}

void rich4_presentation_clear_view_override(GamePresentationState *state)
{
    if (state != NULL) state->has_view_override = 0;
}
