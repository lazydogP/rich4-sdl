#include "app/bankruptcy_flow.h"

#include "app/game_speech_flow.h"
#include "core/god.h"
#include "core/game_event_payload.h"
#include "data/character_speech.h"

#include <string.h>

enum {
    RICH4_BANKRUPTCY_DEFAULT_DELAY_MS = 100,
    RICH4_BANKRUPTCY_HOLD_MS = 2000
};

static void mark_eliminated_actor(GameSession *session, int player)
{
    if (session == NULL || player < 0 || player >= GAME_MAX_PLAYERS) return;
    session->presentation.actors[player].move_active = 0;
    session->presentation.actors[player].dirty_flags |=
        GAME_PLAYER_DIRTY_TURN;
}

static void detach_dynamic_god_presentation(GameSession *session, int player)
{
    int object_id;
    int owner;
    if (session == NULL || player < 0 || player >= GAME_MAX_PLAYERS)
        return;
    owner = player + 1;
    for (object_id = 0; object_id < GAME_MAP_DYNAMIC_OBJECT_COUNT;
         object_id++) {
        GameMapDynamicObjectState *object =
            &session->presentation.dynamic_objects[object_id];
        int type = object->type;
        if (!rich4_god_is_road_type(type) ||
            object->attached_player != owner)
            continue;
        memset(object, 0, sizeof(*object));
        object->type = (uint8_t)type;
    }
}

static void restore_current_player(GameSession *session)
{
    GameBankruptcyPresentationState *bankruptcy;
    if (session == NULL) return;
    bankruptcy = &session->presentation.bankruptcy;
    if (bankruptcy->previous_current_player >= 0 &&
        bankruptcy->previous_current_player < session->state.player_count)
        session->state.current_player =
            bankruptcy->previous_current_player;
}

static void begin_presentation(GameSession *session, int player, uint32_t now,
                               int original_play_state)
{
    GameBankruptcyPresentationState *bankruptcy;
    if (session == NULL || player < 0 ||
        player >= session->state.player_count)
        return;
    bankruptcy = &session->presentation.bankruptcy;
    bankruptcy->active = 1;
    bankruptcy->player = player;
    bankruptcy->phase = RICH4_BANKRUPTCY_PHASE_ANIMATION;
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_BANKRUPTCY]);
    bankruptcy->frame = 0;
    bankruptcy->frame_count = 0;
    bankruptcy->frame_delay_ms = RICH4_BANKRUPTCY_DEFAULT_DELAY_MS;
    bankruptcy->started = now;
    bankruptcy->frame_tick = now;
    bankruptcy->animation_until = now;
    bankruptcy->until = now;
    bankruptcy->awaiting_auction = 0;
    bankruptcy->original_play_state = original_play_state;
    bankruptcy->previous_current_player = session->state.current_player;
    bankruptcy->post_speech_lifecycle = RICH4_APP_LIFECYCLE_NONE;
    session->presentation.ending.active = 0;
    session->presentation.ending.continuation_active = 0;
    session->presentation.ending.continuation_started = 0;
    session->presentation.ending.jump_transition_active = 0;
    session->presentation.ending.jump_transition_code =
        RICH4_APP_LIFECYCLE_NONE;
    session->presentation.ending.jump_transition_started = 0;
    session->presentation.ending.continuation_confirmation_active = 0;
    session->presentation.ending.continuation_confirmation_ready = 0;
    session->presentation.ending.continuation_confirmation_started = 0;

    /* 0x40CD87 temporarily makes the eliminated player current while it
     * draws Data.MKF[555].  The saved current player is restored at the
     * common return label after the lifecycle decision. */
    session->state.current_player = player;
    mark_eliminated_actor(session, player);

    /* The core ledger owns the economic mutation.  This is the app-side half
     * of the original 0x40E14D detach for an attached road god; it must happen
     * before the map is redrawn under the bankruptcy animation. */
    detach_dynamic_god_presentation(session, player);
}

static int capture_one(GameSession *session, const GameEvent *event,
                       uint32_t now)
{
    int trigger;
    int original_play_state;
    if (session == NULL || event == NULL) return 0;
    if (event->type == GAME_EVENT_AUCTION_REQUESTED) {
        trigger = rich4_game_event_auction_trigger(event);
        if (trigger != GAME_AUCTION_TRIGGER_BANKRUPTCY) return 0;
        return rich4_game_event_queue_push(
            &session->bankruptcy_auction_events, event);
    }
    if (event->type == GAME_EVENT_PLAYER_ELIMINATED) {
        original_play_state =
            rich4_game_event_player_eliminated_play_state(event);
        begin_presentation(session, event->player, now,
                           original_play_state);
        return 1;
    }
    if (event->type == GAME_EVENT_GAME_ENDED) {
        /* A bankruptcy may be followed by zero or more auctions.  Holding
         * GAME_ENDED separately prevents the ending overlay from appearing
         * over the animation or auction panel. */
        if (session->presentation.bankruptcy.active ||
            session->bankruptcy_auction_events.count > 0)
            return rich4_game_event_queue_push(
                &session->bankruptcy_terminal_events, event);
        return 0;
    }
    return 0;
}

void rich4_bankruptcy_capture_events(GameSession *session,
                                     GameEventQueue *events,
                                     uint32_t now)
{
    GameEvent event;
    if (session == NULL || events == NULL) return;
    while (rich4_game_event_queue_pop(events, &event))
        (void)capture_one(session, &event, now);
}

int rich4_bankruptcy_capture_event(GameSession *session,
                                   const GameEvent *event,
                                   uint32_t now)
{
    if (session == NULL || event == NULL) return 0;
    return capture_one(session, event, now);
}

static void release_queue(GameEventQueue *source, GameEventQueue *target)
{
    GameEvent event;
    if (source == NULL || target == NULL) return;
    while (rich4_game_event_queue_pop(source, &event))
        (void)rich4_game_event_queue_push(target, &event);
}

static void discard_queue(GameEventQueue *queue)
{
    if (queue != NULL) rich4_game_event_queue_reset(queue);
}

void rich4_bankruptcy_release_terminal_events(GameSession *session)
{
    if (session == NULL) return;
    release_queue(&session->bankruptcy_terminal_events,
                  &session->deferred_events);
    session->presentation.bankruptcy.awaiting_auction = 0;
}

static int active_player_count(const GameSession *session)
{
    int count = 0;
    int player;
    if (session == NULL) return 0;
    for (player = 0; player < session->state.player_count; player++)
        if ((session->state.players[player].play_state &
             GAME_PLAYER_PLAY_STATE_MASK) != 0)
            count++;
    return count;
}

static int active_human_count(const GameSession *session)
{
    int count = 0;
    int player;
    if (session == NULL) return 0;
    for (player = 0; player < session->state.player_count; player++) {
        const GamePlayerState *state = &session->state.players[player];
        if ((state->play_state & GAME_PLAYER_PLAY_STATE_MASK) != 0 &&
            !state->is_ai)
            count++;
    }
    return count;
}

static int last_active_player(const GameSession *session)
{
    int player;
    if (session == NULL) return -1;
    for (player = session->state.player_count - 1; player >= 0; player--)
        if ((session->state.players[player].play_state &
             GAME_PLAYER_PLAY_STATE_MASK) != 0)
            return player;
    return -1;
}

static void clear_normal_terminal_state(GameSession *session)
{
    if (session == NULL) return;
    /* maybe_end() has already emitted GAME_ENDED for active==1.  The
     * original branch replaces that event with a 0x46CAF8 outer transition,
     * so do not let the ordinary ending overlay consume the stale result. */
    session->state.game_over = 0;
    session->state.winner_player = -1;
    session->state.winner_reason = 0;
    session->presentation.ending.active = 0;
    session->presentation.ending.continuation_active = 0;
    session->presentation.ending.ending_video_pending = 0;
    session->presentation.ending.winner_voice_pending = 0;
    session->presentation.ending.continuation_started = 0;
    session->presentation.ending.jump_transition_active = 0;
    session->presentation.ending.jump_transition_code =
        RICH4_APP_LIFECYCLE_NONE;
    session->presentation.ending.jump_transition_started = 0;
    session->presentation.ending.continuation_confirmation_active = 0;
    session->presentation.ending.continuation_confirmation_ready = 0;
    session->presentation.ending.continuation_confirmation_started = 0;
}

static void finish_with_lifecycle(GameSession *session,
                                  Rich4AppLifecycleCode code)
{
    if (session == NULL) return;
    rich4_app_request_lifecycle(
        session, code, session->presentation.bankruptcy.player);
    clear_normal_terminal_state(session);
    discard_queue(&session->bankruptcy_terminal_events);
    session->presentation.bankruptcy.active = 0;
    session->presentation.bankruptcy.awaiting_auction = 0;
    session->presentation.bankruptcy.post_speech_lifecycle =
        RICH4_APP_LIFECYCLE_NONE;
    restore_current_player(session);
}

static void begin_last_human_continuation(GameSession *session, uint32_t now)
{
    GameEndingPresentationState *ending;
    if (session == NULL) return;
    ending = &session->presentation.ending;
    ending->active = 1;
    ending->continuation_active = 1;
    ending->winner_player = -1;
    ending->winner_character = -1;
    ending->reason = 0;
    ending->assets = 0;
    ending->ending_video_pending = 0;
    ending->winner_voice_pending = 0;
    ending->continuation_started = now;
    ending->jump_transition_active = 0;
    ending->jump_transition_code = RICH4_APP_LIFECYCLE_NONE;
    ending->jump_transition_started = 0;
    ending->continuation_confirmation_active = 0;
    ending->continuation_confirmation_ready = 0;
    ending->continuation_confirmation_started = 0;
    /* The SDL boundary now owns the 0x407842 modal itself.  Do not turn this
     * into an immediate code-4 request: the original shows Panel[112] and
     * waits for confirmation or its ten-second timeout first. */
    discard_queue(&session->bankruptcy_terminal_events);
    session->state.game_over = 0;
    session->state.winner_player = -1;
    session->state.winner_reason = 0;
    session->presentation.bankruptcy.active = 0;
    session->presentation.bankruptcy.awaiting_auction = 0;
    restore_current_player(session);
}

static void finish_auctions_after_bankruptcy(
    GameSession *session, uint32_t now)
{
    GameBankruptcyPresentationState *bankruptcy;
    const Rich4CharacterSpeech *speech;
    int active;
    int humans;
    int player;
    int lifecycle;

    if (session == NULL) return;
    bankruptcy = &session->presentation.bankruptcy;
    bankruptcy->awaiting_auction = 0;
    active = active_player_count(session);
    humans = active_human_count(session);

    /* 0x499104 is the total human count captured at game setup, not the
     * current active-human count.  This special path is checked first. */
    if (session->state.human_player_count == 1 &&
        (bankruptcy->original_play_state & 1) != 0) {
        begin_last_human_continuation(session, now);
        return;
    }

    if (humans == 0) {
        finish_with_lifecycle(session, RICH4_APP_LIFECYCLE_OUTER_MENU);
        return;
    }

    if (active == 1) {
        player = last_active_player(session);
        lifecycle = session->state.human_player_count == 1
            ? RICH4_APP_LIFECYCLE_RESTART_GAME
            : RICH4_APP_LIFECYCLE_OUTER_MENU_AFTER_JUMP;
        bankruptcy->post_speech_lifecycle = lifecycle;
        speech = player >= 0
            ? rich4_character_victory_speech(
                  session->state.players[player].character) : NULL;
        if (player >= 0 && speech != NULL &&
            game_start_speech_entry(
                session, player, speech, 3,
                GAME_PROPERTY_PRESENT_NONE, now)) {
            bankruptcy->active = 1;
            bankruptcy->phase = RICH4_BANKRUPTCY_PHASE_SPEECH;
            return;
        }
        finish_with_lifecycle(session, (Rich4AppLifecycleCode)lifecycle);
        return;
    }

    /* The contested-game branch keeps the eliminated player's type-2 line
     * after the final auction.  It does not end the game. */
    player = bankruptcy->player;
    if (active > 1 && player >= 0 &&
        player < session->state.player_count) {
        speech = rich4_character_bankruptcy_speech(
            session->state.players[player].character);
        if (game_start_speech_entry(
                session, player, speech, 2,
                GAME_PROPERTY_PRESENT_NONE, now)) {
            bankruptcy->active = 1;
            bankruptcy->phase = RICH4_BANKRUPTCY_PHASE_SPEECH;
            return;
        }
    }

    bankruptcy->active = 0;
    rich4_bankruptcy_release_terminal_events(session);
    restore_current_player(session);
}

void rich4_bankruptcy_finish_auctions(GameSession *session, uint32_t now)
{
    if (session == NULL ||
        !session->presentation.bankruptcy.awaiting_auction)
        return;
    finish_auctions_after_bankruptcy(session, now);
}

void rich4_bankruptcy_finish_speech(GameSession *session)
{
    GameBankruptcyPresentationState *bankruptcy;
    int lifecycle;
    if (session == NULL) return;
    bankruptcy = &session->presentation.bankruptcy;
    if (bankruptcy->phase != RICH4_BANKRUPTCY_PHASE_SPEECH)
        return;
    lifecycle = bankruptcy->post_speech_lifecycle;
    bankruptcy->post_speech_lifecycle = RICH4_APP_LIFECYCLE_NONE;
    if (lifecycle != RICH4_APP_LIFECYCLE_NONE) {
        finish_with_lifecycle(session,
                              (Rich4AppLifecycleCode)lifecycle);
        return;
    }
    bankruptcy->active = 0;
    rich4_bankruptcy_release_terminal_events(session);
    restore_current_player(session);
}

static void release_auction_events(GameSession *session)
{
    int had_auction;
    if (session == NULL) return;
    had_auction = session->bankruptcy_auction_events.count > 0;
    release_queue(&session->bankruptcy_auction_events,
                  &session->deferred_events);
    session->presentation.bankruptcy.awaiting_auction = had_auction;
    if (!had_auction)
        finish_auctions_after_bankruptcy(
            session, session->presentation.bankruptcy.until);
}

int rich4_bankruptcy_update(GameSession *session, int frame_count,
                            int delay_ms, uint32_t now)
{
    GameBankruptcyPresentationState *bankruptcy;

    if (session == NULL) return 0;
    bankruptcy = &session->presentation.bankruptcy;
    if (!bankruptcy->active) return 0;

    if (delay_ms <= 0) delay_ms = RICH4_BANKRUPTCY_DEFAULT_DELAY_MS;
    if (frame_count < 0) frame_count = 0;
    bankruptcy->frame_count = frame_count;
    bankruptcy->frame_delay_ms = delay_ms;

    if (bankruptcy->phase == RICH4_BANKRUPTCY_PHASE_ANIMATION) {
        if (frame_count > 0 && rich4_animation_clock_step(
                &session->presentation.animation_clock[RICH4_ANIMATION_BANKRUPTCY],
                now, delay_ms)) bankruptcy->frame++;
        bankruptcy->frame_tick = session->presentation.animation_clock[RICH4_ANIMATION_BANKRUPTCY].tick;
        bankruptcy->animation_until = bankruptcy->frame_tick +
            (uint32_t)((frame_count - bankruptcy->frame) * delay_ms);
        if (bankruptcy->frame < frame_count) return 1;
        bankruptcy->phase = RICH4_BANKRUPTCY_PHASE_HOLD;
        bankruptcy->until = now + RICH4_BANKRUPTCY_HOLD_MS;
        return 1;
    }

    if ((int32_t)(now - bankruptcy->until) < 0) return 1;
    bankruptcy->active = 0;
    release_auction_events(session);
    /* Keep this frame modal.  The deferred auction/ending events are
     * consumed by route_event_flow on the next frame. */
    return 1;
}
