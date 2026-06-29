#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/route_event_flow.h"
#include "app/route_flow.h"
#include "app/input_flow.h"
#include "app/command_event_flow.h"
#include "app/special_place_notice_flow.h"
#include "core/game_event_payload.h"
#include "core/route_event_executor.h"

static void prepare(GameSession *session, int play_state, int action, int phase)
{
    RouteEvent event;
    memset(session, 0, sizeof(*session));
    session->state.active = 1;
    session->state.player_count = 2;
    session->state.price_index = 1;
    session->state.ai_rng_state = 1;
    session->state.players[0].play_state = play_state;
    session->state.players[0].is_ai = play_state == 2;
    session->state.players[0].cash = 100000;
    session->state.players[0].bank = 100000;
    session->state.players[0].points = 1000;
    session->state.players[0].ai_cash_ratio = 50;
    session->state.players[0].linked_player = -1;
    session->state.players[1].play_state = 2;
    session->state.players[1].character = 1;
    session->state.players[1].linked_player = -1;
    memset(&event, 0, sizeof(event));
    event.phase = phase;
    event.node = 20;
    event.object_ref = rich4_map_object_ref(6001);
    event.snapshot.player = 0;
    event.snapshot.route_action = action;
    event.snapshot.event_kind = GAME_ROUTE_EVENT_SECOND_SPECIAL;
    event.snapshot.property_owner = -1;
    assert(rich4_route_event_collection_push(&session->flow.route_facts, &event));
    rich4_app_route_prepare_dispatch(session, 0);
    session->flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
}

static void step(GameSession *session, Rich4AudioEventQueue *audio)
{
    GameEventQueue events;
    RouteEventExecutionResult result = rich4_app_route_event_step(
        session, 1000, 1, &events, audio);
    assert(result != ROUTE_EVENT_EXECUTION_INVALID);
}

static void entry_once_for_every_actor_and_action(void)
{
    int action, state_index;
    const int play_states[] = {1, 2, 5};
    for (state_index = 0; state_index < 3; state_index++)
        for (action = 1; action <= 16; action++) {
            GameSession session;
            Rich4AudioEventQueue audio;
            Rich4AudioEvent event;
            int entries = 0;
            prepare(&session, play_states[state_index], action, ROUTE_EVENT_PHASE_STOP);
            rich4_audio_event_queue_init(&audio);
            step(&session, &audio);
            while (rich4_audio_event_pop(&audio, &event))
                if (event.type == RICH4_AUDIO_EVENT_SPECIAL_PLACE_ENTRY) {
                    assert(event.value == action);
                    entries++;
                }
            assert(entries == (action == 1 ? 0 : 1));
            /* Waiting presentations, or a completed square, never replay entry. */
            step(&session, &audio);
            while (rich4_audio_event_pop(&audio, &event))
                assert(event.type != RICH4_AUDIO_EVENT_SPECIAL_PLACE_ENTRY);
        }
}

static void silent_pass_sleepwalk_and_inactive(void)
{
    int action, scenario;
    for (action = 1; action <= 16; action++)
        for (scenario = 0; scenario < 3; scenario++) {
            GameSession session;
            Rich4AudioEventQueue audio;
            Rich4AudioEvent event;
            prepare(&session, scenario == 2 ? 0 : 5, action,
                    scenario == 0 ? ROUTE_EVENT_PHASE_PASS : ROUTE_EVENT_PHASE_STOP);
            if (scenario == 1) session.state.players[0].days_sleep_walking = 3;
            rich4_audio_event_queue_init(&audio);
            step(&session, &audio);
            while (rich4_audio_event_pop(&audio, &event))
                assert(event.type != RICH4_AUDIO_EVENT_SPECIAL_PLACE_ENTRY);
        }
}

static void visits_only_notify_success(void)
{
    int action, affordable;
    for (action = 4; action <= 5; action++)
        for (affordable = 0; affordable <= 1; affordable++) {
            GameSession session;
            Rich4AudioEventQueue audio;
            const GameEvent *notice;
            prepare(&session, 5, action, ROUTE_EVENT_PHASE_STOP);
            /* 0x43D304/0x43E9A4: seed 3 opens the odd-rand visit gate. */
            session.state.ai_rng_state = 3;
            session.state.players[0].points = affordable ? 31 : 30;
            session.state.prison_node = 40;
            session.state.hospital_node = 50;
            if (action == 4) session.state.players[1].days_in_prison = 3;
            else session.state.players[1].days_in_hospital = 3;
            rich4_audio_event_queue_init(&audio);
            step(&session, &audio);
            notice = rich4_special_place_notice_current(&session.presentation);
            assert((notice != NULL) == affordable);
            if (notice != NULL) {
                Rich4GameEventFields fields =
                    rich4_game_event_special_place_notice_requested_fields(notice);
                assert(fields.subject == (action == 4
                    ? GAME_SPECIAL_NOTICE_PRISON_RELEASE : GAME_SPECIAL_NOTICE_HOSPITAL_RELEASE));
                assert(fields.target_ref.kind == GAME_DOMAIN_PLAYER && fields.target_ref.id == 1);
                assert(fields.value2 == 1500 && session.state.players[0].points == 31);
                assert(session.presentation.special_place_notice.until == 2500);
                assert(rich4_special_place_notice_update(&session, 2499));
                assert(rich4_special_place_notice_current(&session.presentation) != NULL);
                assert(rich4_special_place_notice_skip(&session, 2000));
                assert(rich4_special_place_notice_update(&session, 2000));
                assert(rich4_special_place_notice_current(&session.presentation) == NULL);
                assert(session.state.players[0].points == 1);
            }
        }
}

static void bank_notices_and_silent_lottery_shop(void)
{
    int scenario;
    for (scenario = 0; scenario < 5; scenario++) {
        GameSession session;
        Rich4AudioEventQueue audio;
        const GameEvent *notice;
        prepare(&session, 5, scenario < 3 ? 14 : scenario == 3 ? 9 : 15,
                ROUTE_EVENT_PHASE_STOP);
        if (scenario == 0) {
            session.state.players[0].cash = 1000;
            session.state.players[0].bank = 2000;
            session.state.players[0].ai_trait = 50;
        } else if (scenario == 1) {
            session.state.players[0].loan = 100;
            session.state.players[0].bank = 10000;
        }
        rich4_audio_event_queue_init(&audio);
        step(&session, &audio);
        notice = rich4_special_place_notice_current(&session.presentation);
        assert((notice != NULL) == (scenario < 2));
        if (notice != NULL) {
            Rich4GameEventFields fields =
                rich4_game_event_special_place_notice_requested_fields(notice);
            assert(fields.subject == (scenario == 0
                ? GAME_SPECIAL_NOTICE_BANK_BORROW : GAME_SPECIAL_NOTICE_BANK_REPAY));
            assert(fields.value == (scenario == 0 ? session.state.players[0].loan : 100));
            assert(fields.value2 == 1500);
        }
    }
}

static void gift_notice_and_speech_own_the_turn(void)
{
    GameSession session;
    Rich4AudioEventQueue audio;
    Rich4AppRouteEventServices services;
    Rich4PresentationActivity activity;
    const GameEvent *notice;
    int i;
    prepare(&session, 5, 15, ROUTE_EVENT_PHASE_STOP);
    session.flow.route_facts.events[0].snapshot.property_owner = 0;
    session.state.company_count = 1;
    session.state.company_owner[0] = 0;
    session.state.company_type[0] = 10;
    for (i = 0; i < GAME_CARD_COUNT; i++) session.state.card_amount[i] = 1;
    for (i = 0; i < GAME_TOOL_COUNT; i++) session.state.tool_amount[i] = 1;
    rich4_app_route_prepare_dispatch(&session, 0);
    rich4_audio_event_queue_init(&audio);
    step(&session, &audio);
    notice = rich4_special_place_notice_current(&session.presentation);
    assert(notice != NULL && rich4_game_event_special_place_notice_requested_fields(notice).subject ==
           GAME_SPECIAL_NOTICE_SHOP_GIFT);
    assert(!session.presentation.shop_visit.active);
    memset(&services, 0, sizeof(services));
    assert(rich4_app_route_event_update(&session, NULL, 2499, 1, &services));
    assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
    activity = rich4_presentation_activity_from_state(&session.presentation, &session.state);
    assert(rich4_presentation_mode_resolve(&activity) == RICH4_PRESENTATION_PROPERTY);
    assert(rich4_presentation_handler_for_mode(&session.presentation, RICH4_PRESENTATION_PROPERTY) ==
           RICH4_PRESENTATION_HANDLER_SPECIAL_PLACE_NOTICE);
    assert(rich4_special_place_notice_update(&session, 2500));
    assert(session.presentation.special_place_notice.speech_pending);
    assert(session.presentation.speech.active);
    assert(!rich4_special_place_notice_skip(&session, 2500));
    assert(!rich4_app_input_ui_active(RICH4_SCENE_GAME, NULL, &session, 0));
    assert(session.state.company_total_business[0] == 0);
    session.presentation.speech.active = 0;
    assert(rich4_special_place_notice_update(&session, 3500));
    assert(session.presentation.special_place_notice.queue.count == 0);
    assert(!session.flow.route_events.state.waiting_for_presentation);
}

static void owner_shortfall_and_serial_notices(void)
{
    GameSession session;
    Rich4AudioEventQueue audio;
    const GameEvent *notice;
    GameEvent later;
    prepare(&session, 5, 14, ROUTE_EVENT_PHASE_PASS);
    session.state.company_count = 1;
    session.state.company_type[0] = 7;
    session.state.company_owner[0] = 1;
    session.state.players[0].cash = 2000;
    session.state.players[0].bank = 0;
    session.state.players[1].cash = 0;
    session.state.players[1].bank = 5000;
    session.state.players[1].special_finance = 2000;
    rich4_audio_event_queue_init(&audio);
    step(&session, &audio);
    assert(audio.count == 0); /* The bank pass never plays stop entry. */
    notice = rich4_special_place_notice_current(&session.presentation);
    assert(notice != NULL);
    assert(rich4_game_event_special_place_notice_requested_fields(notice).subject ==
           GAME_SPECIAL_NOTICE_BANK_OWNER_SHORTFALL);
    assert(session.presentation.special_place_notice.until == 3500);
    later = rich4_game_event_special_place_notice(0, GAME_SPECIAL_NOTICE_BANK_BORROW,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 100, 1500);
    assert(rich4_app_apply_command_event(&session, NULL, &later, 1001) ==
           RICH4_COMMAND_EVENT_CONSUMED);
    assert(session.presentation.special_place_notice.queue.count == 2);
    assert(rich4_special_place_notice_update(&session, 3499));
    assert(session.presentation.special_place_notice.queue.count == 2);
    assert(rich4_special_place_notice_update(&session, 3500));
    assert(session.presentation.special_place_notice.queue.count == 1);
    assert(session.presentation.special_place_notice.until == 5000);
    assert(rich4_special_place_notice_update(&session, 5000));
    assert(session.presentation.special_place_notice.queue.count == 0);
}

static void original_text_waits_accept_release(void)
{
    GameSession session;
    memset(&session, 0, sizeof(session));
    session.presentation.bank_rejection.active = 1;
    session.presentation.bank_rejection.until = 2000;
    assert(rich4_special_place_notice_skip(&session, 1100));
    assert(session.presentation.bank_rejection.until == 1100);
    session.presentation.bank_rejection.active = 0;
    session.presentation.point_minigame.active = 1;
    session.presentation.point_minigame.automatic = 1;
    session.presentation.point_minigame.phase = RICH4_POINT_MINIGAME_RESULT;
    session.presentation.point_minigame.finish_until = 3000;
    assert(rich4_special_place_notice_skip(&session, 1200));
    assert(session.presentation.point_minigame.finish_until == 1200);
    session.presentation.point_minigame.active = 0;
    session.presentation.magic_house.active = 1;
    session.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_AI_NOTICE;
    session.presentation.magic_house.phase_until = 2500;
    assert(rich4_special_place_notice_skip(&session, 1300));
    assert(session.presentation.magic_house.phase_until == 1300);
    session.presentation.magic_house.active = 0;
    session.presentation.special_reward.active = 1;
    session.presentation.special_reward.action = 13;
    session.presentation.special_reward.until = 2000;
    assert(!rich4_special_place_notice_skip(&session, 1400));
    session.presentation.special_reward.phase = 2;
    assert(rich4_special_place_notice_skip(&session, 1400));
    assert(session.presentation.special_reward.until == 1400);
}

static void stopped_bank_waits_before_loan_and_does_not_replay_entry(void)
{
    GameSession session;
    Rich4AudioEventQueue audio;
    prepare(&session, 5, 14, ROUTE_EVENT_PHASE_STOP);
    session.state.company_count = 1;
    session.state.company_type[0] = 7;
    session.state.company_owner[0] = 1;
    session.state.players[0].cash = 2000;
    session.state.players[0].bank = 0;
    session.state.players[0].ai_trait = 50;
    session.state.players[1].bank = 5000;
    session.state.players[1].special_finance = 2000;
    rich4_audio_event_queue_init(&audio);
    step(&session, &audio);
    assert(session.flow.route_events.state.bank_stop_stage == 1);
    assert(session.flow.route_events.queue.count == 1);
    assert(session.state.players[0].loan == 0);
    assert(session.presentation.special_place_notice.queue.count == 1);
    assert(rich4_special_place_notice_update(&session, 3500));
    assert(session.presentation.special_place_notice.queue.count == 0);
    step(&session, &audio);
    assert(audio.count == 0);
    assert(session.state.players[0].loan == 1000);
    assert(session.flow.route_events.queue.count == 0);
    assert(rich4_game_event_special_place_notice_requested_fields(
        rich4_special_place_notice_current(&session.presentation)).subject ==
        GAME_SPECIAL_NOTICE_BANK_BORROW);
}

int main(void)
{
    entry_once_for_every_actor_and_action();
    silent_pass_sleepwalk_and_inactive();
    visits_only_notify_success();
    bank_notices_and_silent_lottery_shop();
    gift_notice_and_speech_own_the_turn();
    owner_shortfall_and_serial_notices();
    original_text_waits_accept_release();
    stopped_bank_waits_before_loan_and_does_not_replay_entry();
    puts("special-place original AI presentation: ok");
    return 0;
}
