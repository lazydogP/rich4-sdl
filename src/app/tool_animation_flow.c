#include "app/tool_animation_flow.h"

#include "app/game_speech_flow.h"
#include "core/character_speech_rules.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/rng.h"
#include "core/confinement.h"
#include "core/insurance.h"
#include "data/character_speech.h"
#include "data/confinement_presentation.h"

#include <string.h>
#include <limits.h>

static struct ToolAnimationRequest *rich4_tool_animation_last(
    GameSession *session)
{
    int slot;
    if (session == NULL || session->presentation.tool_animation_count <= 0)
        return NULL;
    slot = (session->presentation.tool_animation_head +
            session->presentation.tool_animation_count - 1) %
        GAME_EVENT_QUEUE_CAPACITY;
    return &session->presentation.tool_animation[slot];
}

int rich4_tool_animation_enqueue(
    GameSession *session, const GameEvent *event, int frame_count, int frame_delay_ms,
    int world_x, int world_y, int has_world_position, uint32_t now)
{
    GameToolAnimationRequest *request;
    int slot;
    int was_empty;
    if (session == NULL || event == NULL || frame_count <= 0 || frame_delay_ms <= 0 ||
        rich4_game_event_tool_resource(event) <= 0 ||
        session->presentation.tool_animation_count >= GAME_EVENT_QUEUE_CAPACITY)
        return 0;
    was_empty = session->presentation.tool_animation_count == 0;
    slot = (session->presentation.tool_animation_head +
            session->presentation.tool_animation_count) %
        GAME_EVENT_QUEUE_CAPACITY;
    request = &session->presentation.tool_animation[slot];
    memset(request, 0, sizeof(*request));
    request->resource = rich4_game_event_tool_resource(event);
    request->effect = rich4_game_event_tool_effect(event);
    request->target = rich4_game_event_tool_target(event);
    request->world_x = world_x;
    request->world_y = world_y;
    request->has_world_position = has_world_position;
    request->after_sync_player = -1;
    request->after_speech_player = -1;
    request->after_speech_next_player = -1;
    request->active = 1;
    if (was_empty) {
        rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_TOOL]);
        request->started = now;
        request->until = now + (uint32_t)(frame_count * frame_delay_ms);
        if (has_world_position && request->resource != 525) {
            session->presentation.view_world_x = world_x;
            session->presentation.view_world_y = world_y;
            session->presentation.has_view_override = 1;
        }
    }
    session->presentation.tool_animation_count++;
    if (request->resource == 525 && event->player >= 0 &&
        event->player < session->state.player_count) {
        GamePlayerState *player = &session->state.players[event->player];
        request->explosion_traffic_method = player->traffic_method;
        rich4_confinement_injure_player(&session->state, event->player);
    }
    if (rich4_confinement_presentation_for_resource(request->resource)) {
        request->confinement_days = rich4_game_event_tool_confinement_days(event);
        request->origin_play_state = session->state.players[event->player].play_state |
            GAME_PLAYER_PLAY_STATE_FROZEN;
    }
    /* 0x40F381 commits the little devil's demolition before Data[526]. */
    if (request->resource == 526 && session->presentation.property.god_type == 10)
        session->presentation.property.god_build_animation_started = 1;
    /* 0x41BEA0/0x41B72E/0x41B8A7/0x43ED45: retain actor identity for
     * explosion, dog and ambulance redraws, including special actors 4..7. */
    if (request->resource == 525 || request->resource == 532 ||
        rich4_confinement_presentation_for_resource(request->resource)) {
        request->after_sync_player = event->player;
        if (request->resource == 532 && event->player >= 0 &&
            event->player < session->state.player_count)
            (void)rich4_tool_animation_defer_speech(session, event->player,
                rich4_character_special_event_speech(
                    session->state.players[event->player].character, 1, 0), 2);
    }
    return 1;
}

void rich4_tool_animation_skip_optional_confinement(GameSession *session)
{
    GameToolAnimationRequest *request;
    if (session == NULL || session->presentation.tool_animation_count <= 0) return;
    request = &session->presentation.tool_animation[session->presentation.tool_animation_head];
    /* 0x43D682/0x43ED2E gate the current FLC, retaining its continuation. */
    if (request != NULL &&
        rich4_confinement_presentation_for_resource(request->resource))
        request->animation_done = 1;
}

static void resume_company_amount(GameSession *session, uint32_t now)
{
    GamePropertyPresentationState *property = &session->presentation.property;
    /* Resume 0x41AEB4's amount window after the blocking company effect. */
    if (property->god_type && property->property_kind == GAME_DOMAIN_COMPANY &&
        property->phase == GAME_PROPERTY_PRESENT_AMOUNT)
        property->tick = now;
}

void rich4_tool_animation_actor_synced(GameSession *session, int player)
{
    const GameToolAnimationRequest *request;
    int current;
    if (session == NULL || session->presentation.tool_animation_count <= 0)
        return;
    request = &session->presentation.tool_animation[
        session->presentation.tool_animation_head];
    current = session->state.turn_rotation_index >= 0
        ? session->state.turn_rotation_index : session->state.current_player;
    /* 0x41D4DA: focusing the current actor clears the observation flag. */
    if (rich4_confinement_presentation_for_resource(request->resource) && request->animation_done &&
        request->after_sync_player == -2 - player && current == player)
        session->presentation.has_view_override = 0;
}

int rich4_tool_animation_defer_speech(
    GameSession *session, int player,
    const Rich4CharacterSpeech *speech, int speech_type)
{
    GameToolAnimationRequest *request =
        rich4_tool_animation_last(session);
    if (request == NULL || speech == NULL) return 0;
    if (request->after_speech_player < 0) {
        request->after_speech_player = player;
        request->after_speech = speech;
        (void)rich4_character_speech_snapshot_capture(
            &request->after_speech_snapshot, speech);
        request->after_speech_type = speech_type;
        return 1;
    }
    if (request->after_speech_next_player < 0) {
        request->after_speech_next_player = player;
        request->after_speech_next = speech;
        (void)rich4_character_speech_snapshot_capture(
            &request->after_speech_next_snapshot, speech);
        request->after_speech_next_type = speech_type;
        return 1;
    }
    return 0;
}

int rich4_tool_animation_defer_sync(GameSession *session, int player)
{
    GameToolAnimationRequest *request =
        rich4_tool_animation_last(session);
    if (request == NULL) return 0;
    request->after_sync_player = player;
    return 1;
}

int rich4_confinement_presentation_enqueue(
    GameSession *session, int player, int kind, int days, uint32_t now)
{
    const Rich4ConfinementPresentation *spec = rich4_confinement_presentation_for_kind(kind);
    GameToolAnimationRequest *request;
    GamePlayerState *actor;
    GameEvent event;
    Rich4GameEventFields fields = {0};
    int node, has_xy;
    if (!session || !spec || player < 0 || player >= session->state.player_count ||
        days <= 0 || days >= GAME_STATUS_PENDING_BIT) return 0;
    actor = &session->state.players[player];
    node = actor->node;
    /* Confined actors live at the internal facility anchor, not the exit node. */
    has_xy = node > 0 && node < session->state.tool_route_node_count &&
        actor->confinement_actor_type == GAME_CONFINEMENT_NONE;
    fields.player = player;
    fields.subject_ref = rich4_game_domain_ref(GAME_DOMAIN_STATUS,
        kind == GAME_CONFINEMENT_PRISON ? GAME_STATUS_PRISON : GAME_STATUS_HOSPITAL);
    fields.target_ref = rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, node);
    fields.value = spec->resource;
    fields.value2 = spec->effect;
    fields.flags = (unsigned int)days;
    event = rich4_game_event_tool_presentation_requested_from_fields(fields);
    if (!rich4_tool_animation_enqueue(session, &event, 1, 1,
        has_xy ? session->state.tool_route_x[node] : session->presentation.actors[player].visual_x,
        has_xy ? session->state.tool_route_y[node] : session->presentation.actors[player].visual_y,
        1, now)) return 0;
    request = rich4_tool_animation_last(session);
    request->admission_stage = 1;
    request->origin_play_state = actor->play_state;
    request->origin_traffic_method = actor->traffic_method;
    request->started = 0;
    return 1;
}

static int rich4_tool_animation_start_deferred_speech(
    GameSession *session, GameToolAnimationRequest *request, int next,
    uint32_t now)
{
    const Rich4CharacterSpeech *speech;
    int *player;
    int *speech_type;
    Rich4CharacterSpeechSnapshot *snapshot;
    int started;

    if (session == NULL || request == NULL) return 0;
    if (next) {
        player = &request->after_speech_next_player;
        speech_type = &request->after_speech_next_type;
        snapshot = &request->after_speech_next_snapshot;
        speech = request->after_speech_next;
    } else {
        player = &request->after_speech_player;
        speech_type = &request->after_speech_type;
        snapshot = &request->after_speech_snapshot;
        speech = request->after_speech;
    }
    if (*player < 0 || speech == NULL) {
        *player = -1;
        if (next)
            request->after_speech_next = NULL;
        else
            request->after_speech = NULL;
        rich4_character_speech_snapshot_clear(snapshot);
        return 0;
    }

    /* Keep restored_after_speech alive until the speech finishes.  The
     * presentation dispatcher does not pop this request while speech is
     * active, so a restored pointer remains valid across the audio frames. */
    if (request->confinement_days > 0 && request->resource == 525 &&
        (next || request->after_speech_next_player < 0) &&
        *player >= 0 && *player < session->state.player_count) {
        /* 0x43EC93 clears +50..+53 before the admission reaction. */
        GamePlayerState *actor = &session->state.players[*player];
        rich4_confinement_clear_status(actor);
    }
    if ((request->resource == 525 || request->resource == 532) &&
        request->has_world_position) {
        /* 0x44EFBD focuses the speaker before showing its portrait. */
        session->presentation.view_world_x = request->world_x;
        session->presentation.view_world_y = request->world_y;
        session->presentation.has_view_override = 1;
    }
    started = game_start_speech_entry(
        session, *player, speech, *speech_type,
        GAME_PROPERTY_PRESENT_NONE, now);
    if (started && (speech == &request->restored_after_speech ||
                    speech == &request->restored_after_speech_next)) {
        /* The request snapshot is consumed below. Keep restored text in the
         * speech state's own snapshots through its voice wait and later saves. */
        GameSpeechPresentationState *state = &session->presentation.speech;
        (void)rich4_character_speech_snapshot_restore(
            &state->current_snapshot, &state->restored_current);
        state->current = &state->restored_current;
        state->last_text = state->last_text_snapshot.big5;
        state->last_text_rehydrated = 1;
    }
    *player = -1;
    if (next)
        request->after_speech_next = NULL;
    else
        request->after_speech = NULL;
    rich4_character_speech_snapshot_clear(snapshot);
    return started;
}

int rich4_tool_animation_handle_text_event(
    GameSession *session, const GameEvent *event, uint32_t now)
{
    Rich4GameEventFields fields;
    const Rich4CharacterSpeech *speech;
    int days;
    int character;

    if (session == NULL || event == NULL ||
        event->type != GAME_EVENT_TEXT_REQUESTED ||
        event->player < 0 || event->player >= session->state.player_count)
        return 0;
    fields = rich4_game_event_text_requested_fields(event);
    if (fields.subject != 53 ||
        fields.value != GAME_CONFINEMENT_HOSPITAL || fields.value2 <= 0)
        return 0;
    days = fields.value2;
    {
        GameToolAnimationRequest *request = rich4_tool_animation_last(session);
        if (request != NULL && request->resource == 525 &&
            session->state.players[event->player].days_in_hospital == 0)
            request->confinement_days = days;
    }
    character = session->state.players[event->player].character;
    if (days <= 3)
        speech = rich4_character_special_event_speech(character, 1, 0);
    else if (days <= 6)
        speech = rich4_character_special_event_speech(
            character, 0, (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 1u));
    else
        speech = rich4_character_special_event_speech(character, 0, 0);
    if (speech == NULL) return 0;
    if (rich4_tool_animation_defer_speech(
            session, event->player, speech, 2))
        return 1;
    return game_start_speech_entry(
        session, event->player, speech, 2,
        GAME_PROPERTY_PRESENT_NONE, now);
}

int rich4_tool_animation_handle_voice_event(
    GameSession *session, const GameEvent *event, uint32_t now)
{
    const Rich4CharacterSpeech *speech = NULL;
    GameDomainRef voice_subject;
    GameDomainRef voice_target;
    int speech_type = 0;
    int status_id = -1;
    int player;
    if (session == NULL || event == NULL ||
        event->type != GAME_EVENT_VOICE_REQUESTED) return 0;
    player = event->player;
    if (player < 0 || player >= session->state.player_count) return 0;
    voice_subject = rich4_game_event_voice_subject(event);
    voice_target = rich4_game_event_voice_target(event);
    if (voice_subject.kind == GAME_DOMAIN_TOOL &&
        (voice_subject.id >= 1 && voice_subject.id <= 3)) {
        speech = rich4_character_tool_trigger_speech(
            session->state.players[player].character,
            voice_subject.id);
        speech_type = voice_subject.id == 3 ? 2 : 1;
    } else {
        if (rich4_game_event_voice_subject(event).kind == GAME_DOMAIN_STATUS)
            status_id = rich4_game_event_voice_subject(event).id;
        else if (rich4_game_event_voice_code(event) == 52 ||
                 rich4_game_event_voice_code(event) == 53)
            status_id = rich4_game_event_voice_code(event) - 50;
    }
    if (speech == NULL &&
        (status_id == GAME_STATUS_PRISON ||
         status_id == GAME_STATUS_HOSPITAL)) {
        speech = rich4_character_confinement_speech(
            session->state.players[player].character,
            status_id == GAME_STATUS_HOSPITAL);
        speech_type = 2;
    }
    if (speech == NULL) return 0;
    if ((voice_target.kind == GAME_DOMAIN_TOOL ||
         (voice_subject.kind == GAME_DOMAIN_TOOL && voice_subject.id == 2) ||
         ((status_id == GAME_STATUS_HOSPITAL || status_id == GAME_STATUS_PRISON) &&
          voice_target.kind == GAME_DOMAIN_NONE)) &&
        rich4_tool_animation_defer_speech(
            session, player, speech, speech_type)) return 1;
    return game_start_speech_entry(
        session, player, speech, speech_type,
        GAME_PROPERTY_PRESENT_NONE, now);
}

int rich4_tool_animation_update(
    GameSession *session, int frame_count, int frame_delay_ms, uint32_t now,
    Rich4AudioEventQueue *audio_events,
    Rich4ToolAnimationCompletion *completion)
{
    GameToolAnimationRequest *request;
    const Rich4ConfinementPresentation *confinement;
    if (completion != NULL) {
        completion->sync_player = -1;
        rich4_game_event_queue_reset(&completion->events);
    }
    if (session == NULL || session->presentation.tool_animation_count <= 0)
        return 0;
    request = &session->presentation.tool_animation[
        session->presentation.tool_animation_head];
    confinement = rich4_confinement_presentation_for_resource(request->resource);
    if (confinement && request->admission_stage != 0) {
        int player = request->after_sync_player;
        GamePlayerState *actor = &session->state.players[player];
        if (session->presentation.speech.active) return 1;
        if (request->admission_stage == 1) {
            int current = confinement->kind == GAME_CONFINEMENT_PRISON ?
                actor->days_in_prison : actor->days_in_hospital;
            request->admission_stage = 2;
            session->presentation.view_world_x = request->world_x;
            session->presentation.view_world_y = request->world_y;
            session->presentation.has_view_override = 1;
            if ((current & 0xff) == 0) {
                const Rich4CharacterSpeech *speech;
                int days = request->confinement_days;
                /* 0x40D761 precedes 0x44F2C2 in both entry routines. */
                rich4_confinement_clear_status(actor);
                speech = days <= 3 ? rich4_character_special_event_speech(actor->character, 1, 0) :
                    rich4_character_special_event_speech(actor->character, 0,
                        days <= 6 ? (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 1u) : 0);
                if (game_start_speech_entry(session, player, speech, 2,
                        GAME_PROPERTY_PRESENT_NONE, now)) return 1;
            } else request->animation_done = 1;
        }
        {
            GameEventQueue events;
            rich4_game_event_queue_reset(&events);
            if (rich4_confinement_enter_deferred_claim(&session->state, player,
                    confinement->kind, request->confinement_days, &events) != GAME_CONFINEMENT_APPLIED)
                return 1;
            /* Bind to this head request, even with other admissions queued. */
            request->after_speech_player = player;
            request->after_speech_type = 2;
            request->after_speech = rich4_character_confinement_speech(actor->character,
                confinement->kind == GAME_CONFINEMENT_HOSPITAL);
            (void)rich4_character_speech_snapshot_capture(
                &request->after_speech_snapshot, request->after_speech);
        }
        request->admission_stage = 0;
        /* 0x43D593/0x43EC3F keep the pre-admission camera through the FLC.
         * The reaction speech may have cleared the override while the actor
         * still occupied its old node. Pin it before rendering committed state. */
        session->presentation.view_world_x = request->world_x;
        session->presentation.view_world_y = request->world_y;
        session->presentation.has_view_override = 1;
        request->started = now;
        request->until = now + (uint32_t)(frame_count * frame_delay_ms);
        /* 0x43D6AA/0x43ED59 enter 0x45144F immediately after admission:
         * expose frame zero, start its clock and request its effect together. */
    }
    if (frame_count < 0 || frame_delay_ms <= 0) frame_count = 0;
    if (request->started == 0) {
        request->started = now;
        request->until = now +
            (uint32_t)(frame_count * frame_delay_ms);
        if (request->has_world_position && request->resource != 525) {
            session->presentation.view_world_x = request->world_x;
            session->presentation.view_world_y = request->world_y;
            session->presentation.has_view_override = 1;
        }
    }
    if (!request->animation_done && frame_count > 0) {
        if (rich4_animation_clock_step(
                &session->presentation.animation_clock[RICH4_ANIMATION_TOOL],
                now, frame_delay_ms)) request->frame++;
        request->until = session->presentation.animation_clock[RICH4_ANIMATION_TOOL].tick +
            (uint32_t)((frame_count - request->frame) * frame_delay_ms);
    }
    if (!request->animation_done && request->frame < frame_count) {
        if (!request->sound_played && audio_events != NULL) {
            rich4_audio_event_emit(audio_events,
                                   RICH4_AUDIO_EVENT_TOOL_EFFECT, 1,
                                   request->effect);
            request->sound_played = 1;
        }
        return 1;
    }
    /* 0x43EC93 precedes the reaction at 0x43ECA5. */
    if (!request->animation_done && request->resource == 532 &&
        request->after_sync_player >= 0 && request->after_sync_player < 4 &&
        request->after_sync_player < session->state.player_count &&
        session->state.players[request->after_sync_player].days_in_hospital == 0) {
        GamePlayerState *player = &session->state.players[request->after_sync_player];
        rich4_confinement_clear_status(player);
    }
    request->animation_done = 1;
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_TOOL]);
    /* 0x43ED62 -> 0x43EDA0: ambulance finishes on the old camera;
     * relocate/sync before starting the hospital portrait speech. */
    if (confinement && request->after_sync_player >= 0) {
        if (completion != NULL)
            completion->sync_player = request->after_sync_player;
        request->after_sync_player = -2 - request->after_sync_player;
        session->presentation.has_view_override = 0;
        return 1;
    }
    if (session->presentation.speech.active) return 1;
    if (request->after_speech_player >= 0 ||
        request->after_speech != NULL) {
        if (rich4_tool_animation_start_deferred_speech(
                session, request, 0, now)) return 1;
    }
    if (request->after_speech_next_player >= 0 ||
        request->after_speech_next != NULL) {
        if (rich4_tool_animation_start_deferred_speech(
                session, request, 1, now)) return 1;
    }
    if ((request->resource == 532 ||
         (request->resource == 525 && request->confinement_days > 0)) &&
        request->after_sync_player >= 0 &&
        completion != NULL) {
        GameEventQueue events;
        GameEvent event;
        int player = request->after_sync_player;
        int days = request->resource == 525 ? request->confinement_days : 3;
        rich4_game_event_queue_reset(&events);
        if (rich4_confinement_enter_deferred_claim(
                &session->state, player, GAME_CONFINEMENT_HOSPITAL, days,
                &events) == GAME_CONFINEMENT_APPLIED) {
            if (player < 4)
                (void)rich4_game_event_emit_typed_domain(
                    &completion->events,
                    rich4_game_event_tool_presentation_requested_from_fields,
                    player, rich4_game_domain_ref(GAME_DOMAIN_STATUS,
                                                  GAME_STATUS_HOSPITAL),
                    request->target, 524, 92, (unsigned int)days);
            while (rich4_game_event_queue_pop(&events, &event)) {
                /* The pre-hospital reaction was already played above.
                 * Special actors have no hospital UI/voice (0x43EE12). */
                if (event.type != GAME_EVENT_TEXT_REQUESTED && player < 4)
                    (void)rich4_game_event_queue_push(&completion->events, &event);
            }
        }
    } else if (confinement && completion != NULL &&
               request->target.kind == GAME_DOMAIN_ROUTE_NODE &&
               request->after_sync_player <= -2) {
        /* Negative sync continuation means the hospital actor was already
         * synchronized; retain its identity through the voice wait. */
        int player = -2 - request->after_sync_player;
        int days = request->confinement_days > 0 ? request->confinement_days : 3;
        int64_t amount = (int64_t)session->state.price_index * 2000 * days;
        int company = rich4_insurance_find_company(&session->state);
        if (player >= 0 && player < session->state.player_count &&
            (session->state.players[player].days_assurance & 0xff) != 0 &&
            session->state.price_index > 0 &&
            amount <= INT_MAX && company >= 0) {
            GameEvent claim = rich4_game_event_insurance_claim(
                player, company, player, (int)amount,
                2000, (confinement->kind == GAME_CONFINEMENT_PRISON ?
                    GAME_INSURANCE_CLAIM_PRISON : GAME_INSURANCE_CLAIM_HOSPITAL) |
                    GAME_INSURANCE_CLAIM_DEFERRED |
                    ((unsigned int)days << GAME_INSURANCE_CLAIM_DAYS_SHIFT));
            (void)rich4_game_event_queue_push(&completion->events, &claim);
        } else if (player >= 0 && player < session->state.player_count) {
            /* 0x43EE04 follows the optional claim UI and payout. */
            session->state.players[player].total_confinement_days =
                (session->state.players[player].total_confinement_days + days) & 0xff;
        }
    } else if (completion != NULL && request->resource != 525)
        completion->sync_player = request->after_sync_player;
    /* 0x447378 -> 0x41D546 restores the actor camera only after the
     * optional Data[523] completion animation has finished too. */
    if ((request->resource == 553 || request->resource == 523) &&
        session->presentation.tool_animation_count == 1)
        session->presentation.has_view_override = 0;
    memset(request, 0, sizeof(*request));
    session->presentation.tool_animation_head =
        (session->presentation.tool_animation_head + 1) %
        GAME_EVENT_QUEUE_CAPACITY;
    session->presentation.tool_animation_count--;
    if (session->presentation.tool_animation_count == 0 &&
        !session->presentation.speech.active)
        resume_company_amount(session, now);
    return session->presentation.tool_animation_count > 0 ||
           session->presentation.speech.active;
}
