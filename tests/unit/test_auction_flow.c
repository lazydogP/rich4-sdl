#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/auction_flow.h"
#include "core/game_event_payload.h"
#include "app/special_presentation_flow.h"
#include "core/special_event.h"
#include "core/route_event_executor.h"

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
    session->state.tool_land_x[0] = 640;
    session->state.tool_land_y[0] = 960;
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
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_SPEECH_START &&
           sound.value == 132);
    rich4_auction_flow_skip_speech(&session, NULL);
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
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_AUCTION_EFFECT &&
           sound.value == 29);
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_SPEECH_START &&
           sound.value == 135);
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    sound = pop_audio(&audio);
    assert(sound.type == RICH4_AUDIO_EVENT_SPEECH_START &&
           sound.value == 136);
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    assert(session.presentation.auction.stage ==
           RICH4_AUCTION_PRESENT_MAP_BEFORE_TRANSFER);
    assert(session.presentation.has_view_override &&
           session.presentation.view_world_x == 640 &&
           session.presentation.view_world_y == 960);
    assert(session.state.land_owner[0] == -1);
    assert(session.state.players[0].cash == 20000);
    assert(rich4_auction_flow_update(&session, 2999, 0, &audio, &events));
    assert(session.state.land_owner[0] == -1);
    assert(rich4_auction_flow_release_wait(&session));
    assert(rich4_auction_flow_update(&session, 2999, 0, &audio, &events));
    assert(session.presentation.auction.stage ==
           RICH4_AUCTION_PRESENT_MAP_AFTER_TRANSFER);
    assert(session.state.land_owner[0] == 0);
    assert(session.state.players[0].cash == 20000);
    assert(session.state.system_cash == 100);
    assert(rich4_auction_flow_update(&session, 3998, 0, &audio, &events));
    assert(session.state.players[0].cash == 20000);
    assert(rich4_auction_flow_release_wait(&session));
    assert(!rich4_auction_flow_update(&session, 3998, 0, &audio, &events));
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
    session.state.fixed_asset_value[0] = 0;
    session.state.land_lease_date[0] = 0x07d10a0fu;
    auction_request = request(0, GAME_AUCTION_CLEAR_OWNER_IF_UNSOLD);
    assert(rich4_auction_flow_enqueue(&session, &auction_request, NULL));
    assert(session.presentation.auction.speech_pending);
    rich4_audio_event_queue_init(&audio);
    rich4_game_event_queue_reset(&events);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    sound = pop_audio(&audio);
    assert(sound.value == 131);
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    sound = pop_audio(&audio);
    assert(sound.value == 148);
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(!rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    assert(session.state.land_owner[0] == -1);
    assert(session.state.fixed_asset_value[0] == 0);
    assert(session.state.land_lease_date[0] == 0);
}

static void celebration_stops_at_sixty_ticks(void)
{
    GameSession session;
    GameEventQueue events;
    GameAuctionPresentationState *presentation;
    setup(&session, 1);
    rich4_game_event_queue_reset(&events);
    presentation = &session.presentation.auction;
    presentation->active = 1;
    presentation->stage = RICH4_AUCTION_PRESENT_WINNER;
    presentation->bidding.leader = 0;
    presentation->celebration_started = 0;
    presentation->speech_visible = 1;
    presentation->speech_clock_started = 1;
    presentation->speech_until = 10000;
    assert(rich4_auction_flow_update(
        &session, 5999, 1, &session.audio_events, &events));
    assert(presentation->celebration_tick == 59);
    assert(rich4_auction_flow_update(
        &session, 6000, 1, &session.audio_events, &events));
    assert(presentation->celebration_tick == 0);
}

static void owned_sale_repairs_stale_asset_cache(void)
{
    GameSession session;
    GameEventQueue events;
    GameAuctionPresentationState *presentation;
    setup(&session, 2);
    rich4_game_event_queue_reset(&events);
    session.state.land_owner[0] = 1;
    /* Reproduces a card-auction save whose derived aggregate was not
     * populated.  This used to reject the transfer forever. */
    session.state.fixed_asset_value[0] = 0;
    session.state.fixed_asset_value[1] = 0;
    presentation = &session.presentation.auction;
    presentation->active = 1;
    presentation->stage = RICH4_AUCTION_PRESENT_MAP_BEFORE_TRANSFER;
    presentation->action_until = 1000;
    presentation->bidding.active = 1;
    presentation->bidding.phase = GAME_AUCTION_SOLD;
    presentation->bidding.seller = 1;
    presentation->bidding.target =
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
    presentation->bidding.leader = 0;
    presentation->bidding.current_price = 1100;
    presentation->request.seller = 1;

    assert(rich4_auction_flow_update(
        &session, 1000, 0, &session.audio_events, &events));
    assert(presentation->stage == RICH4_AUCTION_PRESENT_MAP_AFTER_TRANSFER);
    assert(session.state.land_owner[0] == 0);
    assert(session.state.fixed_asset_value[0] == 1000);
    assert(session.state.fixed_asset_value[1] == 0);
    assert(session.state.players[0].cash == 20000);
    assert(session.state.players[1].cash == 20000);

    assert(rich4_auction_flow_release_wait(&session));
    assert(!rich4_auction_flow_update(
        &session, 1000, 0, &session.audio_events, &events));
    assert(session.state.players[0].cash == 18900);
    assert(session.state.players[1].cash == 20000);
    assert(session.state.players[1].bank == 1100);
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
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    (void)pop_audio(&audio);
    rich4_auction_flow_skip_speech(&session, NULL);
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
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    assert(session.presentation.auction.bidding.current_player == 0);
    rich4_auction_flow_skip_speech(&session, NULL);
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

static void speech_waits_without_audio_and_can_be_skipped(void)
{
    GameSession session;
    Rich4AudioEventQueue audio;
    GameEventQueue events;
    GameEvent event = request(-1, 0);
    setup(&session, 2);
    rich4_audio_event_queue_init(&audio);
    rich4_game_event_queue_reset(&events);
    assert(rich4_auction_flow_enqueue(&session, &event, &audio));
    assert(pop_audio(&audio).value == 131);
    assert(rich4_auction_flow_update(&session, 100, 0, &audio, &events));
    assert(rich4_auction_flow_update(&session, 2099, 0, &audio, &events));
    assert(session.presentation.auction.stage == RICH4_AUCTION_PRESENT_INTRO);
    assert(session.presentation.auction.speech_visible);
    assert(rich4_auction_flow_update(&session, 2100, 1, &audio, &events));
    assert(session.presentation.auction.stage == RICH4_AUCTION_PRESENT_INTRO);
    assert(rich4_auction_flow_update(&session, 2300, 0, &audio, &events));
    assert(pop_audio(&audio).value == 132);
    assert(rich4_auction_flow_update(&session, 2300, 0, &audio, &events));
    assert(rich4_auction_flow_update(&session, 4299, 0, &audio, &events));
    assert(session.presentation.auction.stage == RICH4_AUCTION_PRESENT_RESERVE);
    assert(rich4_auction_flow_update(&session, 4300, 0, &audio, &events));
    assert(!session.presentation.auction.speech_visible);
    assert(rich4_auction_flow_is_human_turn(&session));

    setup(&session, 2);
    rich4_audio_event_queue_init(&audio);
    assert(rich4_auction_flow_enqueue(&session, &event, NULL));
    rich4_auction_flow_skip_speech(&session, &audio);
    assert(pop_audio(&audio).type == RICH4_AUDIO_EVENT_SPEECH_STOP);
    assert(!session.presentation.auction.speech_pending);
    assert(rich4_auction_flow_update(&session, 50, 0, &audio, &events));
    assert(pop_audio(&audio).value == 132);
}

static void settlement_keeps_the_ending_continuation(void)
{
    GameSession session;
    Rich4AudioEventQueue audio;
    GameEventQueue events;
    GameEvent event = request(-1, 0);
    setup(&session, 2);
    session.state.win_target = 19500;
    rich4_audio_event_queue_init(&audio);
    rich4_game_event_queue_reset(&events);
    assert(rich4_auction_flow_enqueue(&session, &event, &audio));
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(rich4_auction_flow_update(&session, 0, 0, &audio, &events));
    assert(rich4_auction_flow_submit(&session, GAME_AUCTION_BID_100, 0, &audio)
        == GAME_AUCTION_STEP_NEXT);
    assert(rich4_auction_flow_update(&session, 1000, 0, &audio, &events));
    assert(rich4_auction_flow_submit(&session, GAME_AUCTION_PASS, 1000, &audio)
        == GAME_AUCTION_STEP_SOLD);
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    rich4_auction_flow_skip_speech(&session, NULL);
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    assert(!session.presentation.ending.active);
    assert(rich4_auction_flow_release_wait(&session));
    assert(rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    assert(!session.presentation.ending.active);
    assert(rich4_auction_flow_release_wait(&session));
    assert(!rich4_auction_flow_update(&session, 2000, 0, &audio, &events));
    assert(session.state.game_over);
    /* SDL discards the temporary output queue; the continuation must survive. */
    assert(session.presentation.ending.active);
    assert(session.presentation.ending.winner_voice_pending);
    assert(!session.presentation.auction.active);
}

static void news_callback_releases_its_nested_auction(int sold)
{
    static GameSession session;
    Rich4AudioEventQueue audio;
    GameEventQueue events;
    Rich4SpecialEventUpdateInput input = {0};
    Rich4SpecialEventUpdateResult result;
    GameEvent page;
    GameCommand confirm = {0};
    RouteEvent route = {0};
    uint32_t now;
    int bids = 0;
    setup(&session, 2);
    session.state.special_event_family = GAME_SPECIAL_EVENT_NEWS;
    session.state.special_event_actor = 0;
    session.state.special_event_logical_id = 7;
    session.state.special_event_resolved_id = 7;
    session.state.special_event_phase = GAME_SPECIAL_PHASE_PREPARED;
    session.state.special_event_target_mask = 1;
    page = rich4_game_event_special_event_presentation(0, 448, 66, GAME_SPECIAL_EVENT_NEWS);
    assert(rich4_special_event_begin(&session, &page, 0, 0, 0));
    route.phase = ROUTE_EVENT_PHASE_STOP;
    route.node = 2;
    assert(rich4_route_event_queue_push(&session.flow.route_events.queue, &route));
    session.flow.route_events.state.execution_started = 1;
    session.flow.route_events.state.execution_actor = 0;
    session.flow.route_events.state.waiting_for_command = 1;
    confirm.type = GAME_COMMAND_CONFIRM;
    session.flow.route_events.state.pending_accept = confirm;
    rich4_audio_event_queue_init(&audio);
    rich4_game_event_queue_reset(&events);
    (void)rich4_special_event_update(&session, &input, 2400, &result);
    assert(session.presentation.auction.active);
    assert(!session.presentation.special_event.active);
    for (now = 2400; now < 24000 && session.presentation.auction.active; now += 100) {
        if (rich4_auction_flow_is_human_turn(&session)) {
            GameAuctionAction action = sold && bids++ == 0 ?
                GAME_AUCTION_BID_100 : GAME_AUCTION_PASS;
            assert(rich4_auction_flow_submit(&session, action, now, &audio)
                != GAME_AUCTION_STEP_INVALID);
        }
        (void)rich4_auction_flow_update(&session, now, 0, &audio, &events);
        rich4_audio_event_queue_reset(&audio);
    }
    assert(!session.presentation.auction.active);
    assert(!session.presentation.special_event.active);
    assert(!session.presentation.route_interaction.active);
    assert(!session.flow.route_events.state.waiting_for_command);
    assert(!session.flow.route_events.state.waiting_for_presentation);
    assert(!session.state.game_over);
    assert(session.state.land_owner[0] == (sold ? 0 : -1));
    assert(session.state.players[0].cash == (sold ? 18900 : 20000));
}

int main(void)
{
    speech_waits_without_audio_and_can_be_skipped();
    settlement_keeps_the_ending_continuation();
    news_callback_releases_its_nested_auction(0);
    news_callback_releases_its_nested_auction(1);
    pending_landing_players_are_not_bidders();
    hit_boxes_are_original();
    human_sale_timeline_and_settlement();
    deferred_speech_and_unsold_policy();
    owned_sale_repairs_stale_asset_cache();
    celebration_stops_at_sixty_ticks();
    invalid_bid_and_cash_overrun_follow_callback();
    puts("auction flow tests: ok");
    return 0;
}
