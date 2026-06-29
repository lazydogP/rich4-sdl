#include "platform/sdl1/game_lazy_assets.h"
#include "app/special_presentation_flow.h"

#include <string.h>

#include "app/bankruptcy_flow.h"
#include "app/options_game_flow.h"
#include "app/god_money_flow.h"
#include "app/game_speech_flow.h"
#include "app/dynamic_object_flow.h"
#include "core/god.h"
#include "app/game_session.h"
#include "core/game_event_payload.h"

#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_turns.h"

/* SDL adapter for the app-owned single-shot presentation flows.  Resource
 * metadata is reduced to timing scalars here; semantic state transitions and
 * audio requests belong to app/special_presentation_flow.c. */

static void special_reward_timing(
    const GamePreviewAssets *assets, int action,
    int *out_frames, int *out_delay)
{
    int index = action == 13 ? 0 : 1;
    if (out_frames != NULL) *out_frames = 0;
    if (out_delay != NULL) *out_delay = 71;
    if (assets == NULL || action < 10 || action > 13) return;
    if (!game_asset_ready(assets, &assets->ui.has_special_reward[index])) return;
    if (out_frames != NULL)
        *out_frames = game_asset_graph_frames(assets, &assets->ui.special_reward_data[index], assets->ui.special_reward_size[index]);
    if (out_delay != NULL)
        *out_delay = game_asset_graph_delay(assets, &assets->ui.special_reward_data[index], assets->ui.special_reward_size[index]);
}

void game_begin_special_reward_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    const GameEvent *event, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *events;
    int frames;
    int delay;
    special_reward_timing(assets,
                          event == NULL ? -1 :
                              rich4_game_event_special_place_action(event),
                          &frames, &delay);
    events = rich4_app_session_audio_borrow(session);
    now = rich4_sdl1_ticks();
    rich4_special_reward_begin(session, event, frames, delay, now, events);
    game_session_audio_flush(session, audio);
}

int game_update_special_reward_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *events;
    int frames;
    int delay;
    int active;
    special_reward_timing(assets,
                          session == NULL ? -1 :
                              session->presentation.special_reward.action,
                          &frames, &delay);
    events = rich4_app_session_audio_borrow(session);
    now = rich4_sdl1_ticks();
    active = rich4_special_reward_update(
        session, frames, delay, now, events);
    game_session_audio_flush(session, audio);
    return active;
}

void game_begin_bank_rejection_presentation(
    GameSession *session, const GameEvent *event,
    Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
    rich4_bank_rejection_begin(session, event, now, events);
    game_session_audio_flush(session, audio);
}

int game_update_bank_rejection_presentation(
    GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
    int active = rich4_bank_rejection_update(session, now, events);
    /* The current rejection update has no late audio, but draining the
     * queue keeps the adapter contract stable if that flow gains one. */
    game_session_audio_flush(session, audio);
    return active;
}

int game_update_bankruptcy_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    Rich4Audio *audio, Uint32 now)
{
    int frame_count = 0;
    int delay_ms = 100;
    if (assets != NULL && game_asset_ready(assets, &assets->ui.has_bankruptcy_animation)) {
        frame_count = game_asset_graph_frames(assets, &assets->ui.bankruptcy_data, assets->ui.bankruptcy_size);
        delay_ms = game_asset_graph_delay(assets, &assets->ui.bankruptcy_data, assets->ui.bankruptcy_size);
    }
    if (session != NULL && session->presentation.bankruptcy.phase ==
        RICH4_BANKRUPTCY_PHASE_SPEECH) {
        if (game_update_property_speech(session, audio, now)) return 1;
        rich4_bankruptcy_finish_speech(session);
        {
            Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
            rich4_app_surrender_followup(session, now, events);
            game_session_audio_flush(session, audio);
        }
        return 1;
    }
    {
        now = rich4_sdl1_ticks();
        int active = rich4_bankruptcy_update(session, frame_count, delay_ms, now);
        Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
        rich4_app_surrender_followup(session, now, events);
        game_session_audio_flush(session, audio);
        return active;
    }
}

static void god_presentation_timing(
    const GamePreviewAssets *assets, int god_type,
    int *out_frames, int *out_delay)
{
    if (out_frames != NULL) *out_frames = 0;
    if (out_delay != NULL) *out_delay = 71;
    if (assets == NULL || god_type <= 0 ||
        god_type > GAME_GOD_TYPE_COUNT ||
        !game_asset_ready(assets, &assets->ui.has_god_effect[god_type]))
        return;
    if (out_frames != NULL)
        *out_frames = game_asset_graph_frames(assets, &assets->ui.god_effect_data[god_type], assets->ui.god_effect_size[god_type]);
    if (out_delay != NULL)
        *out_delay = game_asset_graph_delay(assets, &assets->ui.god_effect_data[god_type], assets->ui.god_effect_size[god_type]);
    if (out_delay != NULL && *out_delay <= 0) *out_delay = 71;
}

static int god_result_message_available(
    const GameGodAcquisitionPresentationState *god)
{
    if (god == NULL) return 0;
    switch (god->god_type) {
    case 3:
        return god->result_card_1 >= 0;
    case 4:
        return god->result_card_1 >= 0 && god->result_card_2 >= 0;
    case 7:
        return god->result_card_1 >= 0;
    case 8:
        return god->result_lost_count > 0;
    default:
        return 0;
    }
}

void game_begin_god_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    const GameEvent *event, Uint32 now)
{
    GameGodAcquisitionPresentationState *god;
    int type;
    int frames;
    int delay;
    int result_message;
    uint32_t duration = 0u;
    const uint32_t main_message_duration = 2400u;
    const uint32_t result_message_duration = 1500u;

    if (session == NULL) return;
    god = &session->presentation.god_acquisition;
    memset(god, 0, sizeof(*god));
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_GOD]);
    type = event == NULL ? 0 : rich4_game_event_god_type(event);
    if (type <= 0 || type > GAME_GOD_TYPE_COUNT ||
        !rich4_god_is_road_type(type))
        return;

    god_presentation_timing(assets, type, &frames, &delay);
    god->active = 1;
    god->player = event == NULL ? -1 : event->player;
    god->god_type = type;
    god->object_slot = event == NULL
        ? 0 : rich4_game_event_god_object_slot(event);
    god->first_acquisition = event != NULL &&
        (rich4_game_event_god_flags(event) & RICH4_GOD_ACQUIRED_FIRST) != 0;
    god->result_card_1 = event == NULL
        ? -1 : rich4_game_event_god_result_card(event, 0);
    god->result_card_2 = event == NULL
        ? -1 : rich4_game_event_god_result_card(event, 1);
    god->result_lost_count = event == NULL
        ? 0 : rich4_game_event_god_result_lost_count(event);
    god->money_pending = event != NULL &&
        (rich4_game_event_god_flags(event) &
         RICH4_GOD_ACQUIRED_MONEY_PENDING) != 0;
    god->death_pending = event != NULL &&
        (rich4_game_event_god_flags(event) & RICH4_GOD_ACQUIRED_DEATH_PENDING) != 0;
    /* 0x40EF44/0x40F00D/0x40F0AC/0x40F17E/0x40F314. */
    god->arrival_speech_phase =
        type == 5 || type == 6 || type == 7 || type == 8 || type == 15;
    god->waiting_for_detach = session->presentation.god_detachment.active ||
        session->presentation.god_detachment.speech_pending;
    god->sound_pending = !god->waiting_for_detach && !god->arrival_speech_phase;
    god->frame = 0;
    god->frame_count = frames;
    god->frame_delay_ms = delay;
    god->message_visible = frames <= 0 || delay <= 0
        ? RICH4_GOD_MESSAGE_MAIN : RICH4_GOD_MESSAGE_HIDDEN;
    god->started = now;
    god->frame_tick = now;
    /* 0x45144F decodes the first frame immediately, waits the FLC header's
     * +0x10 interval before each subsequent frame, and returns after the
     * final frame has been held for one interval.  The original then calls
     * 0x40E2A2 for the bottom main message and waits 0x960 ms. */
    if (frames > 0 && delay > 0)
        duration = (uint32_t)frames * (uint32_t)delay;
    god->animation_until = now + duration;
    result_message = god_result_message_available(god);
    god->until = god->animation_until + main_message_duration +
        (result_message ? result_message_duration : 0u);
}

int game_update_god_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    Rich4Audio *audio, Uint32 now, int animation_enabled)
{
    GameGodAcquisitionPresentationState *god;
    (void)assets;
    if (session == NULL) return 0;
    god = &session->presentation.god_acquisition;
    if (!god->active) return 0;
    if (!rich4_presentation_god_owns_frame(&session->presentation)) return 0;
    if (!animation_enabled) {
        /* byte_497159 gates each god FLC and 0x40E2A2, but not arrival
         * speech, money/inventory effects, or the card-result popup. */
        god->frame_count = 0;
        god->animation_until = god->started;
        god->until = god->started +
            (god_result_message_available(god) ? 1500u : 0u);
        god->message_visible = god_result_message_available(god)
            ? RICH4_GOD_MESSAGE_RESULT : RICH4_GOD_MESSAGE_HIDDEN;
        god->sound_pending = 0;
    }
    if (god->waiting_for_detach) {
        /* 0x40EB3F completes 0x40E32C before the new god's FLC/sound. */
        uint32_t waited;
        if (game_update_god_detachment(session, audio, now))
            return 1;
        waited = now - god->started;
        god->animation_until += waited;
        god->until += waited;
        god->started = now;
        god->frame_tick = now;
        god->waiting_for_detach = 0;
        god->sound_pending = animation_enabled && !god->arrival_speech_phase;
    }
    if (god->arrival_speech_phase) {
        if (god->arrival_speech_phase == 1) {
            god->arrival_speech_phase = 2;
            /* 0x40EF44/0x40F00D/0x40F0AC/0x40F17E/0x40F314
             * pass player | 0x8000, preserving the god arrival view. */
            if (game_start_speech_entry(session,
                    god->player | GAME_SPEECH_KEEP_VIEW,
                    rich4_character_god_arrival_speech(session->state.players[god->player].character),
                    2, session->presentation.property.phase, now)) return 1;
        }
        if (session->presentation.speech.active) return 1;
        /* The FLC clock starts after the blocking 0x44EF41 call returns. */
        god->animation_until += now - god->started;
        god->until += now - god->started;
        god->started = god->frame_tick = now;
        god->arrival_speech_phase = 0;
        god->sound_pending = animation_enabled != 0;
    }
    if (god->money.active ||
        (god->money_pending && god->message_visible != RICH4_GOD_MESSAGE_HIDDEN &&
         (int32_t)(now - god->until) >= 0)) {
        Rich4AudioEventQueue *audio_events =
            rich4_app_session_audio_borrow(session);
        GameEventQueue events;
        int active;
        rich4_game_event_queue_reset(&events);
        if (!god->money.active)
            rich4_god_money_begin(session, now, audio_events);
        active = rich4_god_money_update(session, now, audio_events, &events);
        rich4_bankruptcy_capture_events(session, &events, now);
        game_session_audio_flush(session, audio);
        return active;
    }
    if (animation_enabled && god->message_visible == RICH4_GOD_MESSAGE_HIDDEN &&
        god->frame_count > 0 && god->frame_delay_ms > 0) {
        if (rich4_animation_clock_step(
                &session->presentation.animation_clock[RICH4_ANIMATION_GOD],
                now, god->frame_delay_ms)) god->frame++;
        god->frame_tick = session->presentation.animation_clock[RICH4_ANIMATION_GOD].tick;
        god->animation_until = god->frame_tick + (uint32_t)(
            (god->frame_count - god->frame) * god->frame_delay_ms);
        god->until = god->animation_until + 2400u +
            (god_result_message_available(god) ? 1500u : 0u);
        if (god->frame >= god->frame_count)
            god->message_visible = RICH4_GOD_MESSAGE_MAIN;
    }
    if (god->message_visible == RICH4_GOD_MESSAGE_MAIN &&
        god_result_message_available(god) &&
        (int32_t)(now - (god->animation_until + 2400u)) >= 0)
        god->message_visible = RICH4_GOD_MESSAGE_RESULT;
    if ((int32_t)(now - god->until) < 0) return 1;
    if (god->death_pending) {
        GameEventQueue events;
        rich4_game_event_queue_reset(&events);
        if (rich4_god_resolve_death(&session->state, god->player,
                god->object_slot, &events) != GAME_GOD_APPLIED) return 1;
        god->death_pending = 0;
    }
    god->active = 0;
    return 0;
}

void game_finish_company_issue(GameSession *session, int quantity)
{
    (void)rich4_company_issue_finish(session, quantity);
}
