/* Purpose: own special-event phase transitions and render-state selection.
 * Owns: consequence mapping, update actions and app timeline decisions.
 * Borrows: GameSession presentation/domain state and platform timing facts.
 * Allowed deps: app state plus core event and special-event contracts.
 * Invariant: SDL receives typed actions; it never chooses a special phase.
 * Tests: test_special_presentation_flow.
 */
#include "app/special_presentation_flow.h"
#include "app/insurance_flow.h"

#include "app/game_presentation.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/special_event.h"

int rich4_special_event_consequence_index(int logical_id)
{
    static const int news_id[] = {
        4, 5, 15, 20, 21
    };
    int index;
    for (index = 0; index < (int)(sizeof(news_id) / sizeof(news_id[0])); index++)
        if (news_id[index] == logical_id) return index;
    return -1;
}

static int special_event_finish_update(
    GameSession *session, uint32_t now,
    Rich4SpecialEventUpdateResult *result)
{
    if (rich4_special_event_finish_callback(session, now)) {
        result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
        return 1;
    }
    return 0;
}

static int special_event_prepare_update_reaction(
    GameSession *session, uint32_t now,
    Rich4SpecialEventUpdateResult *result)
{
    if (!rich4_special_event_prepare_reaction(
            session, now, &result->reaction))
        return 0;
    result->action = session->presentation.special_event.reaction_stage == 5 ?
        RICH4_SPECIAL_EVENT_UPDATE_WAIT : RICH4_SPECIAL_EVENT_UPDATE_REACTION;
    return 1;
}

int rich4_special_event_update(
    GameSession *session, const Rich4SpecialEventUpdateInput *input,
    uint32_t now, Rich4SpecialEventUpdateResult *result)
{
    GameSpecialEventPresentationState *special;
    GameCommand command;
    RouteEventCommandResult command_result;
    int consequence_index;

    if (result != NULL) {
        result->action = RICH4_SPECIAL_EVENT_UPDATE_NONE;
        result->reaction.action = RICH4_SPECIAL_EVENT_REACTION_NONE;
        result->reaction.player = -1;
        result->reaction.center_original_player = 0;
        result->reaction.sync_player_to_node = 0;
        result->sync_player_to_node = 0;
        result->sync_player = -1;
    }
    if (session == NULL || input == NULL || result == NULL ||
        !session->presentation.special_event.active)
        return 0;
    special = &session->presentation.special_event;

    if (special->phase == RICH4_SPECIAL_PRESENT_REACTION_DELAY) {
        if ((int32_t)(now - special->until) < 0) {
            result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
            return 1;
        }
        if (special_event_prepare_update_reaction(session, now, result))
            return 1;
        return special_event_finish_update(session, now, result);
    }

    if (special->phase == RICH4_SPECIAL_PRESENT_REACTION) {
        if (special->reaction_stage == 5) {
            if (session->presentation.tool_animation_count > 0 ||
                session->presentation.insurance.count > 0 || input->speech_active) {
                result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
                return 1;
            }
            {
                int player;
                for (player = 0; player < session->state.player_count; player++)
                    if (special->pending_confinement_days[player] > 0)
                        return special_event_prepare_update_reaction(session, now, result);
            }
            return special_event_finish_update(session, now, result);
        }
        if (input->speech_active) {
            result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
            return 1;
        }
        if (special->reaction_stage == 4) {
            if (rich4_insurance_flow_resume_special_claim(session, now)) {
                result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
                return 1;
            }
            result->action = RICH4_SPECIAL_EVENT_UPDATE_FORTUNE_ANIMATION;
            return 1;
        }
        if (special->confinement != 0 && special->reaction_stage == 1) {
            result->action =
                RICH4_SPECIAL_EVENT_UPDATE_CONFINEMENT_ANIMATION;
            return 1;
        }
        if (special->family == GAME_SPECIAL_EVENT_NEWS &&
            special->resolved_id == 4 && special->reaction_mask != 0 &&
            special_event_prepare_update_reaction(session, now, result))
            return 1;
        return special_event_finish_update(session, now, result);
    }

    if (special->phase == RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET) {
        result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
        return 1;
    }
    if (special->phase == RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET) {
        result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
        return 1;
    }
    if (special->phase == RICH4_SPECIAL_PRESENT_PASSIVE_CARD ||
        special->phase == RICH4_SPECIAL_PRESENT_GOD_NOTICE ||
        special->phase == RICH4_SPECIAL_PRESENT_REDIRECT_NOTICE ||
        special->phase == RICH4_SPECIAL_PRESENT_PASSIVE_ACTOR_SPEECH ||
        special->phase == RICH4_SPECIAL_PRESENT_PASSIVE_VICTIM_SPEECH) {
        int speech_phase=special->phase==RICH4_SPECIAL_PRESENT_PASSIVE_ACTOR_SPEECH ||
            special->phase==RICH4_SPECIAL_PRESENT_PASSIVE_VICTIM_SPEECH;
        if ((speech_phase ? input->speech_active || input->audio_speech_playing :
                (int32_t)(now-special->until)<0) ||
            rich4_special_event_prepare_choices(session,now)) {
            result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
            return 1;
        }
    }

    if (special->phase == RICH4_SPECIAL_PRESENT_FORTUNE_ANIMATION) {
        if (input->fortune_frame_count > 0)
            rich4_special_event_update_timeline(
                session, input->fortune_frame_count,
                input->fortune_delay_ms, now);
        if ((int32_t)(now - special->until) < 0) {
            result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
            return 1;
        }
        rich4_special_event_mark_scene_refreshed(session);
        return special_event_finish_update(session, now, result);
    }

    if (special->phase == RICH4_SPECIAL_PRESENT_CONFINEMENT_ANIMATION) {
        if (input->confinement_frame_count > 0)
            rich4_special_event_update_timeline(
                session, input->confinement_frame_count,
                input->confinement_delay_ms, now);
        if ((int32_t)(now - special->until) < 0) {
            result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
            return 1;
        }
        rich4_special_event_mark_scene_refreshed(session);
        rich4_special_event_set_reaction_stage(session, 3);
        result->sync_player_to_node = 1;
        result->sync_player = special->reaction_player;
        if (rich4_special_event_start_confinement_final_speech(
                session, special->reaction_player, now)) {
            result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
            return 1;
        }
        if (special->family == GAME_SPECIAL_EVENT_NEWS &&
            special->resolved_id == 4 && special->reaction_mask != 0 &&
            special_event_prepare_update_reaction(session, now, result))
            return 1;
        return special_event_finish_update(session, now, result);
    }

    if (special->phase == RICH4_SPECIAL_PRESENT_PAGE &&
        input->page_frame_count > 0)
        rich4_special_event_update_timeline(
            session, input->page_frame_count, input->page_delay_ms, now);
    else if (special->phase == RICH4_SPECIAL_PRESENT_CONSEQUENCE &&
             input->consequence_frame_count > 0)
        rich4_special_event_update_timeline(
            session, input->consequence_frame_count,
            input->consequence_delay_ms, now);

    if (special->phase != RICH4_SPECIAL_PRESENT_PAGE &&
        special->phase != RICH4_SPECIAL_PRESENT_CONSEQUENCE)
        goto prepared_phase;
    if ((int32_t)(now - special->until) < 0) {
        result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
        return 1;
    }
    if (special->phase == RICH4_SPECIAL_PRESENT_PAGE) {
        if (input->audio_speech_playing) {
            result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
            return 1;
        }
        command = session->flow.route_events.state.pending_accept;
        if (rich4_special_event_prepare_choices(session,now)) {
            result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
            return 1;
        }
        if (special->fortune_defense_ready) {
            command.flags |= RICH4_SPECIAL_COMMAND_DEFENSE_RESOLVED;
            command.value2 = special->defense;
            if (special->fortune_passive_resolved)
                command.flags |= RICH4_SPECIAL_COMMAND_TARGET_RESOLVED;
            if (special->fortune_redirect_target >= 0 || special->fortune_passive_resolved)
                command.target = rich4_game_domain_ref(
                    GAME_DOMAIN_PLAYER,special->fortune_redirect_target);
        }
        command_result = rich4_special_event_submit_page_command_with_area(
            session, &command, now, input->news_area_anchors,
            input->news_area_count, input->news_area_available, input->news_area_map);
        if (command_result != ROUTE_EVENT_COMMAND_APPLIED) {
            rich4_special_event_restore_route_interaction(session, &command);
            return 0;
        }
        rich4_special_event_center_callback_view(session);
        consequence_index = rich4_special_event_consequence_index(
            special->logical_id);
        if (special->family == GAME_SPECIAL_EVENT_NEWS &&
            consequence_index >= 0 && input->consequence_frame_count > 0 &&
            rich4_special_event_begin_consequence(
                session, special->logical_id, consequence_index,
                input->consequence_frame_count,
                input->consequence_delay_ms, now)) {
            result->action = RICH4_SPECIAL_EVENT_UPDATE_CONSEQUENCE;
            return 1;
        }
        if (rich4_special_event_start_reaction_delay(session, now)) {
            result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
            return 1;
        }
        if (special_event_prepare_update_reaction(session, now, result))
            return 1;
        if (special->family == GAME_SPECIAL_EVENT_FORTUNE) {
            if (rich4_insurance_flow_resume_special_claim(session, now)) {
                special->phase = RICH4_SPECIAL_PRESENT_REACTION;
                special->reaction_stage = 0;
                result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
                return 1;
            }
            rich4_special_event_begin_hold(session, now);
            result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
            return 1;
        }
        rich4_special_event_deactivate(session);
        return 0;
    }

    rich4_special_event_mark_scene_refreshed(session);
    if (rich4_special_event_start_reaction_delay(session, now)) {
        result->action = RICH4_SPECIAL_EVENT_UPDATE_WAIT;
        return 1;
    }
    if (special_event_prepare_update_reaction(session, now, result))
        return 1;
    rich4_special_event_deactivate(session);
    return 0;

prepared_phase:
    if (special->phase == RICH4_SPECIAL_PRESENT_HOLD ||
        special->phase == RICH4_SPECIAL_PRESENT_POST_HOLD) {
        if ((int32_t)(now-special->until)<0) {
            result->action=RICH4_SPECIAL_EVENT_UPDATE_WAIT;
            return 1;
        }
        if (session->flow.route_events.state.magic_fortune_sequence &&
            session->flow.route_events.state.waiting_for_presentation &&
            session->state.special_event_phase==GAME_SPECIAL_PHASE_IDLE) {
            GameEventQueue events;
            GameEvent event;
            rich4_game_event_queue_reset(&events);
            if (rich4_route_event_executor_finish_fortune_callback(
                    &session->flow.route_events,&session->state,&events)==
                ROUTE_EVENT_EXECUTION_INVALID) return 0;
            while (rich4_game_event_queue_pop(&events,&event))
                if (event.type==GAME_EVENT_ROUTE_EVENT_RESOLVED)
                    rich4_special_event_complete_route_interaction(session);
        }
    }
    if ((special->phase == RICH4_SPECIAL_PRESENT_HOLD ||
         special->phase == RICH4_SPECIAL_PRESENT_POST_HOLD) &&
        session->state.special_event_phase == GAME_SPECIAL_PHASE_PREPARED) {
        GameEvent event = rich4_game_event_special_event_presentation(
            session->state.special_event_actor,
            session->state.special_event_resource_id,
            session->state.special_event_panel_id,
            (unsigned int)session->state.special_event_family);
        if (rich4_special_event_begin_from_event(session, &event, now)) {
            result->action = RICH4_SPECIAL_EVENT_UPDATE_BEGIN;
            return 1;
        }
    }
    rich4_special_event_deactivate(session);
    return 0;
}

const GameState *rich4_special_event_render_state(
    const GameSession *session)
{
    int phase;
    if (session == NULL) return NULL;
    phase = session->presentation.special_event.phase;
    if (session->presentation.special_event.active &&
        session->presentation.special_event.reaction_stage == 5)
        return &session->state;
    if (session->presentation.special_event.active &&
        session->presentation.special_event.pre_state_valid &&
        session->presentation.special_event.family ==
            GAME_SPECIAL_EVENT_NEWS &&
        session->presentation.special_event.resolved_id == 4 &&
        (phase == RICH4_SPECIAL_PRESENT_REACTION ||
         phase == RICH4_SPECIAL_PRESENT_CONFINEMENT_ANIMATION))
        return &session->presentation.special_event.pre_state;
    if (session->presentation.special_event.active &&
        session->presentation.special_event.pre_state_valid &&
        phase == RICH4_SPECIAL_PRESENT_REACTION &&
        ((session->presentation.special_event.reaction_stage == 4) ||
         (session->presentation.special_event.reaction_stage == 1 &&
          session->presentation.special_event.first_confinement)))
        return &session->presentation.special_event.pre_state;
    if (session->presentation.special_event.active &&
        session->presentation.special_event.pre_state_valid &&
        session->presentation.special_event.scene_refresh_frame > 0 &&
        !session->presentation.special_event.scene_refreshed &&
        (phase == RICH4_SPECIAL_PRESENT_CONSEQUENCE ||
         phase == RICH4_SPECIAL_PRESENT_CONFINEMENT_ANIMATION ||
         phase == RICH4_SPECIAL_PRESENT_FORTUNE_ANIMATION))
        return &session->presentation.special_event.pre_state;
    return &session->state;
}
