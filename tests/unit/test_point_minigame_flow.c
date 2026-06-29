#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/point_minigame_flow.h"
#include "core/game_event_payload.h"
#include "core/rng.h"
#include "core/route_event_executor.h"

static void drain_audio(Rich4AudioEventQueue *audio_events)
{
    Rich4AudioEvent ignored;
    while (rich4_audio_event_pop(audio_events, &ignored)) {}
}

static void begin_play_minigame(
    GameSession *session, Rich4AudioEventQueue *audio_events,
    int action, uint32_t now)
{
    GameEvent event;
    memset(session, 0, sizeof(*session));
    session->state.player_count = 1;
    session->flow.route_events.state.execution_actor = 0;
    event = rich4_game_event_point_minigame_request(0, action, 78, 81, 1);
    rich4_audio_event_queue_init(audio_events);
    rich4_point_minigame_begin(session, &event, now, 1, audio_events);
    assert(session->presentation.point_minigame.active);
    assert(session->presentation.point_minigame.phase ==
           (action == 8 ? RICH4_POINT_MINIGAME_READY : RICH4_POINT_MINIGAME_INTRO));
    if (action == 8)
        assert(rich4_point_minigame_update(session,20,114,NULL,now,320,audio_events));
    session->presentation.point_minigame.phase = RICH4_POINT_MINIGAME_PLAY;
    session->presentation.point_minigame.next_tick = now;
    drain_audio(audio_events);
}

static void finish_minigame_tick(
    GameSession *session, Rich4AudioEventQueue *audio_events,
    uint32_t now)
{
    session->presentation.point_minigame.ticks_remaining = 1;
    session->presentation.point_minigame.next_tick = now;
    assert(rich4_point_minigame_update(
        session, 0, 0, NULL, now +
        (session->presentation.point_minigame.action == 8 ? 50 : 100),
        320, audio_events));
    assert(session->presentation.point_minigame.phase ==
           RICH4_POINT_MINIGAME_RESULT);
}

static void test_penguin_result_tiers(void)
{
    GameSession session;
    Rich4AudioEventQueue audio_events;
    Rich4AudioEvent audio_event;
    struct PointMinigameState *game;

    begin_play_minigame(&session, &audio_events, 6, 1000);
    game = &session.presentation.point_minigame;
    finish_minigame_tick(&session, &audio_events, 1000);
    assert(game->score == 0 && game->result_tier == 5);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT &&
           audio_event.value == 14);

    begin_play_minigame(&session, &audio_events, 6, 2000);
    game = &session.presentation.point_minigame;
    game->counters[4] = 2;
    finish_minigame_tick(&session, &audio_events, 2000);
    assert(game->score == 40 && game->result_tier == 6);
    assert(!rich4_audio_event_pop(&audio_events, &audio_event));

    begin_play_minigame(&session, &audio_events, 6, 3000);
    game = &session.presentation.point_minigame;
    game->counters[4] = 3;
    finish_minigame_tick(&session, &audio_events, 3000);
    assert(game->score == 60 && game->result_tier == 4);
    assert(rich4_audio_event_pop(&audio_events, &audio_event));
    assert(audio_event.type == RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT &&
           audio_event.value == 13);
}

static void test_catcher_result_tiers(void)
{
    GameSession session;
    Rich4AudioEventQueue audio_events;
    struct PointMinigameState *game;

    begin_play_minigame(&session, &audio_events, 8, 4000);
    game = &session.presentation.point_minigame;
    game->animation_delay = 4;
    finish_minigame_tick(&session, &audio_events, 4000);
    assert(game->score == 0 && game->result_tier == 1);

    begin_play_minigame(&session, &audio_events, 8, 5000);
    game = &session.presentation.point_minigame;
    game->counters[0] = 4;
    game->animation_delay = 4;
    finish_minigame_tick(&session, &audio_events, 5000);
    assert(game->score == 40 && game->result_tier == 2);

    begin_play_minigame(&session, &audio_events, 8, 6000);
    game = &session.presentation.point_minigame;
    game->counters[0] = 5;
    game->animation_delay = 4;
    finish_minigame_tick(&session, &audio_events, 6000);
    assert(game->score == 50 && game->result_tier == 0);

    begin_play_minigame(&session, &audio_events, 8, 7000);
    game = &session.presentation.point_minigame;
    game->counters[0] = 6;
    game->animation_delay = 4;
    finish_minigame_tick(&session, &audio_events, 7000);
    assert(game->score == 60 && game->result_tier == 3);

    begin_play_minigame(&session, &audio_events, 8, 8000);
    game = &session.presentation.point_minigame;
    game->catcher_bomb_active = 1;
    game->catcher.explosion_frame = 7;
    game->catcher.stage = 2;
    game->result_tier = 4;
    game->score = 0;
    finish_minigame_tick(&session, &audio_events, 8000);
    assert(game->score == 0 && game->result_tier == 4);
}

#include "point_minigame_catcher_cases.inc"

int main(void)
{
    GameSession session;
    Rich4AudioEventQueue audio_events;
    Rich4AudioEvent audio_event;
    GameEvent event;

    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.flow.route_events.state.execution_actor = 0;
    event = rich4_game_event_point_minigame_request(0, 6, 78, 81, 1);
    rich4_audio_event_queue_init(&audio_events);
    rich4_point_minigame_begin(&session, &event, 1000, 1, &audio_events);
    assert(session.presentation.point_minigame.active);
    assert(session.presentation.point_minigame.phase ==
           RICH4_POINT_MINIGAME_INTRO);
    assert(!rich4_audio_event_pop(&audio_events, &audio_event));
    assert(rich4_point_minigame_cell_valid(3, 0));
    assert(!rich4_point_minigame_cell_valid(0, 0));
    assert(rich4_point_minigame_penguin_next_cell(56, 3) != 56);

    assert(rich4_point_minigame_update(
        &session, 2, 100, NULL, 1000, 0, &audio_events));
    assert(rich4_point_minigame_update(
        &session, 2, 100, NULL, 1100, 0, &audio_events));
    assert(rich4_point_minigame_update(
        &session, 2, 100, NULL, 1200, 0, &audio_events));
    assert(session.presentation.point_minigame.phase ==
           RICH4_POINT_MINIGAME_PLAY);
    assert(rich4_point_minigame_click(
        &session, 3, 0, 0, 1200, &audio_events));
    assert(session.presentation.point_minigame.penguin_target_cell == 3);

    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.flow.route_events.state.execution_actor = 0;
    event = rich4_game_event_point_minigame_request(0, 7, 78, 81, 1);
    rich4_audio_event_queue_init(&audio_events);
    rich4_point_minigame_begin(&session, &event, 2000, 0, &audio_events);
    assert(session.presentation.point_minigame.automatic);
    assert(session.presentation.point_minigame.phase ==
           RICH4_POINT_MINIGAME_RESULT);

    test_penguin_result_tiers();
    test_catcher_result_tiers();
    test_catcher_original_cases();
    test_catcher_reward_once();

    puts("app point-minigame flow: ok");
    return 0;
}
