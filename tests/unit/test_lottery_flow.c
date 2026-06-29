#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/lottery_flow.h"
#include "core/game_event_payload.h"

static GameEvent lottery_event(void)
{
    return rich4_game_event_lottery_visit_request(0, 9, 12, 13, 14u);
}

int main(void)
{
    GameSession session;
    Rich4LotteryAudioRequest request;
    GameEvent event = lottery_event();

    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.state.players[0].cash = 5000;
    session.flow.route_events.state.execution_actor = 0;
    rich4_lottery_begin_visit(&session, &event, 1000, 1, &request);
    assert(session.presentation.lottery.active);
    assert(session.presentation.lottery.phase == RICH4_LOTTERY_VISIT_INTRO_1);
    assert(request.effect == RICH4_LOTTERY_AUDIO_NONE);
    assert(request.start_speech_resource >= 0);

    assert(rich4_lottery_update(&session, 4, 3000, 0, &request));
    assert(session.presentation.lottery.phase == RICH4_LOTTERY_VISIT_INTRO_2);
    assert(request.start_speech_resource >= 0);
    assert(session.presentation.lottery.animation_frame == 1);
    assert(rich4_lottery_update(&session, 4, 3001, 0, &request));
    assert(session.presentation.lottery.animation_frame == 1);

    assert(rich4_lottery_number_at(&session, 30, 271) == 0);
    session.state.lottery_owner[0] = 1;
    assert(rich4_lottery_number_at(&session, 30, 271) == -1);

    rich4_lottery_cancel_visit(&session, 4000, &request);
    assert(session.presentation.lottery.phase == RICH4_LOTTERY_VISIT_EXIT);
    assert(request.effect == RICH4_LOTTERY_AUDIO_CANCEL);
    assert(request.start_speech_resource >= 0);

    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.state.players[0].cash = 999;
    session.flow.route_events.state.execution_actor = 0;
    rich4_lottery_begin_visit(&session, &event, 2000, 1, &request);
    assert(session.presentation.lottery.phase == RICH4_LOTTERY_VISIT_INSUFFICIENT);
    assert(request.effect == RICH4_LOTTERY_AUDIO_NONE);

    puts("app lottery flow: ok");
    return 0;
}
