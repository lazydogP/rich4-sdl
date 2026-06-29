#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/auction_flow.h"
#include "core/game_event_payload.h"

static void setup(GameSession *session, int players)
{
    int i;
    memset(session, 0, sizeof(*session));
    rich4_audio_event_queue_init(&session->audio_events);
    session->state.active = 1;
    session->state.player_count = players;
    session->state.price_index = 1;
    session->state.system_cash = 100;
    session->state.ai_rng_state = 1;
    for (i = 0; i < players; i++) {
        session->state.players[i].play_state = 1;
        session->state.players[i].cash = 20000;
        session->state.players[i].character = i;
    }
    session->state.land_count = 1;
    session->state.land_owner[0] = -1;
    session->state.land_price[0] = 1000;
    session->state.land_price_per_level[0] = 500;
    session->state.land_value[0] = 1000;
}

static GameEvent request(int seller, unsigned int flags)
{
    return rich4_game_event_auction_request(
        -1, rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), seller, 1,
        GAME_AUCTION_TRIGGER_NEWS, flags);
}

static Rich4AudioEvent pop_audio(Rich4AudioEventQueue *queue)
{
    Rich4AudioEvent event;
    assert(rich4_audio_event_pop(queue, &event));
    return event;
}

static void hit_boxes_are_original(void)
{
    int action;
    assert(rich4_auction_action_at(362, 133) == -1);
    assert(rich4_auction_action_at(450, 133) == -1);
    for (action = GAME_AUCTION_PASS;
         action <= GAME_AUCTION_GIVE_UP; action++) {
        int y = 133 + action * 48;
        assert(rich4_auction_action_at(363, y - 19) == action);
        assert(rich4_auction_action_at(449, y + 19) == action);
        assert(rich4_auction_action_at(406, y + 20) == -1);
    }
}

static void human_sale_timeline_and_settlement(void)
{
    GameSession session;
    Rich4AudioEventQueue audio;
    Rich4AudioEvent sound;
    GameEventQueue events;
    GameEvent auction_request;
    setup(&session, 2);
    rich4_audio_event_queue_init(&audio);
    rich4_game_event_queue_reset(&events);
    auction_request = request(-1, 0);

    assert(rich4_auction_flow_enqueue(&session, &auction_request, &audio));
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_SPEECH_START &&
           sound.value == 131);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_SPEECH_START &&
           sound.value == 132);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    assert(rich4_auction_flow_is_human_turn(&session));
    rich4_auction_flow_set_pressed(&session, GAME_AUCTION_BID_100);
    assert(session.presentation.auction.pressed_action ==
           GAME_AUCTION_BID_100);

    assert(rich4_auction_flow_submit(
               &session, GAME_AUCTION_BID_100, 0, &audio) ==
           GAME_AUCTION_STEP_NEXT);
    assert(session.presentation.auction.pressed_action == -1);
    assert(session.presentation.auction.bidder_mask == 1u);
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_AUCTION_EFFECT &&
           sound.value == 63);
    assert(rich4_auction_flow_update(&session, 999, 0, &audio, &events));
    assert(session.presentation.auction.stage == RICH4_AUCTION_PRESENT_ACTION);
    assert(rich4_auction_flow_update(&session, 1000, 0, &audio, &events));
    assert(session.presentation.auction.bidding.current_player == 1);

    assert(rich4_auction_flow_submit(
               &session, GAME_AUCTION_PASS, 1000, &audio) ==
           GAME_AUCTION_STEP_SOLD);
    (void)pop_audio(&audio);
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_AUCTION_EFFECT &&
           sound.value == 29);
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_SPEECH_START &&
           sound.value == 135);
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_SPEECH_START &&
           sound.value == 136);
    assert(!rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    assert(session.state.land_owner[0] == 0);
    assert(session.state.players[0].cash == 18900);
    assert(session.state.system_cash == 1200);
    assert(events.count == 2);
}

static void deferred_speech_and_unsold_policy(void)
{
    GameSession session;
    Rich4AudioEventQueue audio;
    GameEventQueue events;
    GameEvent auction_request;
    Rich4AudioEvent sound;
    setup(&session, 1);
    session.state.land_owner[0] = 0;
    session.state.fixed_asset_value[0] = 1000;
    auction_request = request(0, GAME_AUCTION_CLEAR_OWNER_IF_UNSOLD);
    assert(rich4_auction_flow_enqueue(&session, &auction_request, NULL));
    assert(session.presentation.auction.speech_pending);
    rich4_audio_event_queue_init(&audio);
    rich4_game_event_queue_reset(&events);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    sound = pop_audio(&audio);
    assert(sound.value == 131);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    sound = pop_audio(&audio);
    assert(sound.value == 148);
    assert(!rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    assert(session.state.land_owner[0] == -1);
    assert(session.state.fixed_asset_value[0] == 0);
}

static void invalid_bid_and_cash_overrun_follow_callback(void)
{
    GameSession session;
    Rich4AudioEventQueue audio;
    GameEventQueue events;
    GameEvent auction_request;
    Rich4AudioEvent sound;
    setup(&session, 2);
    session.state.players[0].cash = 1500;
    rich4_audio_event_queue_init(&audio);
    rich4_game_event_queue_reset(&events);
    auction_request = request(-1, 0);
    assert(rich4_auction_flow_enqueue(&session, &auction_request, &audio));
    (void)pop_audio(&audio);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    (void)pop_audio(&audio);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));

    assert(rich4_auction_flow_submit(
               &session, GAME_AUCTION_BID_1000, 0, &audio) ==
           GAME_AUCTION_STEP_INVALID);
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_AUCTION_EFFECT &&
           sound.value == 63);
    assert(session.presentation.auction.stage ==
           RICH4_AUCTION_PRESENT_BIDDING);
    assert(rich4_auction_flow_submit(
               &session, GAME_AUCTION_BID_500, 0, &audio) ==
           GAME_AUCTION_STEP_NEXT);
    (void)pop_audio(&audio);
    assert(rich4_auction_flow_update(&session, 1000, 0, &audio, &events));
    assert(session.presentation.auction.bidding.current_player == 1);
    assert(rich4_auction_flow_submit(
               &session, GAME_AUCTION_BID_1000, 1000, &audio) ==
           GAME_AUCTION_STEP_NEXT);
    assert(session.presentation.auction.bidder_mask == 3u);
    (void)pop_audio(&audio);
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    assert(session.presentation.auction.bidding.current_player == 0);
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_AUCTION_EFFECT &&
           sound.value == 63);
    assert(session.presentation.auction.bidding.participant[0] ==
           GAME_AUCTION_PARTICIPANT_WITHDRAWN);
}

static void pending_landing_players_are_not_bidders(void)
{
    GameSession session;
    GameEvent event = request(-1, 0);
    uint32_t rng_before;
    setup(&session, 3);
    session.presentation.actors[1].first_turn_landing_pending = 1;
    session.presentation.actors[2].first_turn_landing_pending = 1;
    session.state.players[2].is_ai = 1;
    rng_before = session.state.ai_rng_state;
    assert(rich4_auction_flow_enqueue(&session, &event, &session.audio_events));
    assert(session.presentation.auction.bidding.eligible_count == 1);
    assert(session.presentation.auction.bidding.current_player == 0);
    assert(session.presentation.auction.bidding.participant[1] ==
           GAME_AUCTION_PARTICIPANT_EXCLUDED);
    assert(session.presentation.auction.bidding.participant[2] ==
           GAME_AUCTION_PARTICIPANT_EXCLUDED);
    assert(session.state.ai_rng_state == rng_before);
    assert(session.state.players[1].play_state == 1);
    session.presentation.auction.active = 0;
    session.presentation.actors[1].first_turn_landing_pending = 0;
    assert(rich4_auction_flow_enqueue(&session, &event, &session.audio_events));
    assert(session.presentation.auction.bidding.eligible_count == 2);
}

int main(void)
{
    pending_landing_players_are_not_bidders();
    hit_boxes_are_original();
    human_sale_timeline_and_settlement();
    deferred_speech_and_unsold_policy();
    invalid_bid_and_cash_overrun_follow_callback();
    puts("auction flow tests: ok");
    return 0;
}
