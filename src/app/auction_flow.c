#include "app/auction_flow.h"

#include <string.h>

#include "app/bankruptcy_flow.h"
#include "core/game_event_payload.h"

enum {
    RICH4_AUCTION_ACTION_EFFECT = 63,
    RICH4_AUCTION_GAVEL_EFFECT = 29,
    RICH4_AUCTION_INTRO_SPEECH = 131,
    RICH4_AUCTION_RESERVE_SPEECH = 132,
    RICH4_AUCTION_SOLD_SPEECH = 135,
    RICH4_AUCTION_WINNER_SPEECH_BASE = 136,
    RICH4_AUCTION_UNSOLD_SPEECH = 148,
    RICH4_AUCTION_ACTION_DELAY_MS = 1000
};

static int request_from_event(
    const GameEvent *event, GameAuctionPresentationRequest *request)
{
    int seller;
    int manage_music;
    int trigger;
    GameDomainRef target;
    if (event == NULL || request == NULL ||
        event->type != GAME_EVENT_AUCTION_REQUESTED)
        return 0;
    seller = rich4_game_event_auction_seller(event);
    manage_music = rich4_game_event_auction_manage_music(event);
    trigger = rich4_game_event_auction_trigger(event);
    target = rich4_game_event_auction_property(event);
    if (seller < -1 || manage_music < 0 || manage_music > 1 ||
        trigger < GAME_AUCTION_TRIGGER_CARD ||
        trigger > GAME_AUCTION_TRIGGER_BANKRUPTCY ||
        (target.kind != GAME_DOMAIN_LAND &&
         target.kind != GAME_DOMAIN_FACILITY))
        return 0;
    request->initiator = event->player;
    request->seller = seller;
    request->target = target;
    request->manage_music = manage_music;
    request->trigger = trigger;
    request->flags = rich4_game_event_auction_flags(event);
    return 1;
}

static void play_speech(
    GameAuctionPresentationState *presentation, int resource,
    Rich4AudioEventQueue *audio_events)
{
    presentation->speech_resource = resource;
    presentation->speech_pending = !rich4_audio_event_emit(
        audio_events, RICH4_AUDIO_EVENT_SPEECH_START, 1, resource);
}

static int start_request(
    GameSession *session, const GameAuctionPresentationRequest *request,
    Rich4AudioEventQueue *audio_events)
{
    GameAuctionPresentationState *presentation;
    Rich4Rng rng;
    GameState bidding_state;
    int player;
    if (session == NULL || request == NULL ||
        session->state.player_count <= 0 ||
        session->state.player_count > GAME_MAX_PLAYERS) return 0;
    presentation = &session->presentation.auction;
    /* 0x43C11F skips who_plays == 0. SDL reserves play_state early for
     * turn rotation; pending landing corresponds to original who_plays == 0
     * until 0x418D07 activates the player. Do not consume AI RNG for them. */
    bidding_state = session->state;
    for (player = 0; player < bidding_state.player_count; player++)
        if (session->presentation.actors[player].first_turn_landing_pending)
            bidding_state.players[player].play_state = 0;
    rng.state = session->state.ai_rng_state;
    if (!rich4_auction_begin(
            &presentation->bidding, &bidding_state, request->seller,
            request->target, request->manage_music, &rng))
        return 0;
    session->state.ai_rng_state = rng.state;
    presentation->active = 1;
    presentation->stage = RICH4_AUCTION_PRESENT_INTRO;
    presentation->pressed_action = -1;
    presentation->action_tick = 0;
    presentation->action_until = 0;
    presentation->action_started = 0;
    presentation->celebration_tick = 0;
    presentation->celebration_started = 0;
    presentation->bidder_mask = 0;
    presentation->request = *request;
    presentation->last_settlement = GAME_AUCTION_SETTLEMENT_REJECTED;
    play_speech(presentation, RICH4_AUCTION_INTRO_SPEECH, audio_events);
    return 1;
}

int rich4_auction_action_at(int x, int y)
{
    int action;
    if (x < 0x16b || x > 0x1c1) return -1;
    for (action = GAME_AUCTION_PASS;
         action <= GAME_AUCTION_GIVE_UP; action++) {
        int center_y = 0x85 + action * 0x30;
        if (y >= center_y - 0x13 && y <= center_y + 0x13)
            return action;
    }
    return -1;
}

int rich4_auction_flow_is_human_turn(const GameSession *session)
{
    const GameAuctionPresentationState *presentation;
    int player;
    if (session == NULL) return 0;
    presentation = &session->presentation.auction;
    player = presentation->bidding.current_player;
    return presentation->active &&
           presentation->stage == RICH4_AUCTION_PRESENT_BIDDING &&
           player >= 0 && player < session->state.player_count &&
           !session->state.players[player].is_ai;
}

void rich4_auction_flow_set_pressed(GameSession *session, int action)
{
    if (session == NULL) return;
    session->presentation.auction.pressed_action =
        rich4_auction_flow_is_human_turn(session) &&
        action >= GAME_AUCTION_PASS && action <= GAME_AUCTION_GIVE_UP
            ? action : -1;
}

int rich4_auction_flow_enqueue(
    GameSession *session, const GameEvent *event,
    Rich4AudioEventQueue *audio_events)
{
    GameAuctionPresentationState *presentation;
    GameAuctionPresentationRequest request;
    int tail;
    if (session == NULL || !request_from_event(event, &request)) return 0;
    presentation = &session->presentation.auction;
    if (!presentation->active)
        return start_request(session, &request, audio_events);
    if (presentation->pending_count >= GAME_EVENT_QUEUE_CAPACITY) return 0;
    tail = (presentation->pending_head + presentation->pending_count) %
        GAME_EVENT_QUEUE_CAPACITY;
    presentation->pending[tail] = request;
    presentation->pending_count++;
    return 1;
}

static void enter_result_stage(
    GameAuctionPresentationState *presentation,
    uint32_t now, Rich4AudioEventQueue *audio_events)
{
    if (presentation->bidding.phase == GAME_AUCTION_SOLD) {
        presentation->stage = RICH4_AUCTION_PRESENT_SOLD_CALL;
        presentation->celebration_started = now;
        presentation->celebration_tick = 0;
        rich4_audio_event_emit(
            audio_events, RICH4_AUDIO_EVENT_AUCTION_EFFECT, 1,
            RICH4_AUCTION_GAVEL_EFFECT);
        play_speech(presentation, RICH4_AUCTION_SOLD_SPEECH, audio_events);
    } else if (presentation->bidding.phase == GAME_AUCTION_UNSOLD) {
        presentation->stage = RICH4_AUCTION_PRESENT_UNSOLD;
        play_speech(presentation, RICH4_AUCTION_UNSOLD_SPEECH, audio_events);
    }
}

static GameAuctionStepResult apply_action(
    GameSession *session, GameAuctionAction action, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    GameAuctionPresentationState *presentation =
        &session->presentation.auction;
    int player = presentation->bidding.current_player;
    GameAuctionStepResult result = rich4_auction_apply_action(
        &presentation->bidding, &session->state, player, action);
    rich4_audio_event_emit(
        audio_events, RICH4_AUDIO_EVENT_AUCTION_EFFECT, 1,
        RICH4_AUCTION_ACTION_EFFECT);
    if (result == GAME_AUCTION_STEP_INVALID) return result;
    presentation->pressed_action = -1;
    if (presentation->bidding.last_bid > 0)
        presentation->bidder_mask |= 1u << player;
    presentation->stage = RICH4_AUCTION_PRESENT_ACTION;
    presentation->action_started = now;
    presentation->action_tick = 0;
    presentation->action_until = now + RICH4_AUCTION_ACTION_DELAY_MS;
    return result;
}

GameAuctionStepResult rich4_auction_flow_submit(
    GameSession *session, GameAuctionAction action, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    if (!rich4_auction_flow_is_human_turn(session))
        return GAME_AUCTION_STEP_INVALID;
    return apply_action(session, action, now, audio_events);
}

static void finish_current(
    GameSession *session, Rich4AudioEventQueue *audio_events,
    GameEventQueue *out_events, uint32_t now)
{
    GameAuctionPresentationState *presentation =
        &session->presentation.auction;
    GameEventQueue local_events;
    GameAuctionPresentationRequest next_request;
    int have_next = 0;
    if (out_events == NULL) {
        rich4_game_event_queue_reset(&local_events);
        out_events = &local_events;
    }
    presentation->last_settlement = rich4_auction_settle(
        &presentation->bidding, &session->state,
        presentation->request.flags, out_events);
    if (presentation->last_settlement ==
        GAME_AUCTION_SETTLEMENT_REJECTED)
        return;
    if (presentation->pending_count > 0) {
        next_request = presentation->pending[presentation->pending_head];
        presentation->pending_head = (presentation->pending_head + 1) %
            GAME_EVENT_QUEUE_CAPACITY;
        presentation->pending_count--;
        have_next = 1;
    }
    presentation->active = 0;
    presentation->stage = RICH4_AUCTION_PRESENT_IDLE;
    presentation->pressed_action = -1;
    presentation->speech_resource = -1;
    presentation->speech_pending = 0;
    if (have_next)
        (void)start_request(session, &next_request, audio_events);
    else
        rich4_bankruptcy_finish_auctions(session, now);
}

int rich4_auction_flow_update(
    GameSession *session, uint32_t now, int speech_playing,
    Rich4AudioEventQueue *audio_events, GameEventQueue *out_events)
{
    GameAuctionPresentationState *presentation;
    int player;
    if (session == NULL || !session->presentation.auction.active) return 0;
    presentation = &session->presentation.auction;
    if (presentation->speech_pending) {
        if (rich4_audio_event_emit(
                audio_events, RICH4_AUDIO_EVENT_SPEECH_START, 1,
                presentation->speech_resource))
            presentation->speech_pending = 0;
        return 1;
    }
    switch (presentation->stage) {
    case RICH4_AUCTION_PRESENT_INTRO:
        if (!speech_playing) {
            if (presentation->bidding.phase == GAME_AUCTION_UNSOLD)
                enter_result_stage(presentation, now, audio_events);
            else {
                presentation->stage = RICH4_AUCTION_PRESENT_RESERVE;
                play_speech(
                    presentation, RICH4_AUCTION_RESERVE_SPEECH,
                    audio_events);
            }
        }
        break;
    case RICH4_AUCTION_PRESENT_RESERVE:
        if (!speech_playing)
            presentation->stage = RICH4_AUCTION_PRESENT_BIDDING;
        break;
    case RICH4_AUCTION_PRESENT_BIDDING:
        player = presentation->bidding.current_player;
        if (presentation->bidding.phase != GAME_AUCTION_BIDDING)
            enter_result_stage(presentation, now, audio_events);
        else if (player >= 0 && player < session->state.player_count) {
            if (presentation->bidding.current_price >
                session->state.players[player].cash)
                (void)apply_action(
                    session, GAME_AUCTION_GIVE_UP, now, audio_events);
            else if (session->state.players[player].is_ai)
                (void)apply_action(
                    session,
                    rich4_auction_ai_action(
                        &presentation->bidding, &session->state, player),
                    now, audio_events);
        }
        break;
    case RICH4_AUCTION_PRESENT_ACTION:
        presentation->action_tick = (int)(
            (uint32_t)(now - presentation->action_started) / 100u);
        if (presentation->action_tick > 10)
            presentation->action_tick = 10;
        if ((int32_t)(now - presentation->action_until) >= 0) {
            if (presentation->bidding.phase == GAME_AUCTION_BIDDING)
                presentation->stage = RICH4_AUCTION_PRESENT_BIDDING;
            else
                enter_result_stage(presentation, now, audio_events);
        }
        break;
    case RICH4_AUCTION_PRESENT_SOLD_CALL:
        presentation->celebration_tick = (int)(
            (uint32_t)(now - presentation->celebration_started) / 100u);
        if (presentation->celebration_tick > 60)
            presentation->celebration_tick = 60;
        if (!speech_playing) {
            player = presentation->bidding.leader;
            presentation->stage = RICH4_AUCTION_PRESENT_WINNER;
            play_speech(
                presentation,
                RICH4_AUCTION_WINNER_SPEECH_BASE +
                    session->state.players[player].character,
                audio_events);
        }
        break;
    case RICH4_AUCTION_PRESENT_WINNER:
        presentation->celebration_tick = (int)(
            (uint32_t)(now - presentation->celebration_started) / 100u);
        if (presentation->celebration_tick > 60)
            presentation->celebration_tick = 60;
        if (!speech_playing)
            finish_current(session, audio_events, out_events, now);
        break;
    case RICH4_AUCTION_PRESENT_UNSOLD:
        if (!speech_playing)
            finish_current(session, audio_events, out_events, now);
        break;
    default:
        break;
    }
    return session->presentation.auction.active;
}
