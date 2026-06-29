#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/intro_flow.h"

int main(void)
{
    GameSession session;
    MapRouteNode routes[3];
    MapData map;
    Rich4IntroTiming timing;
    Rich4IntroStage stage;
    int total;
    int i;

    memset(&session, 0, sizeof(session));
    memset(routes, 0, sizeof(routes));
    memset(&map, 0, sizeof(map));
    memset(&timing, 0, sizeof(timing));
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.current_player = 1;
    session.state.players[0].character = -1;
    session.state.players[0].node = 1;
    session.state.players[1].node = 2;
    rich4_intro_set_avi_selection(&session, 0, 1, "FLYCHINA.AVI");
    assert(session.presentation.intro.avi_attempted == 1);
    assert(session.presentation.intro.avi_skipped_no_backend == 1);
    assert(strcmp(session.presentation.intro.avi_name, "FLYCHINA.AVI") == 0);
    rich4_intro_begin(&session);
    session.presentation.actors[0].intro_visible = 1;
    session.presentation.actors[1].intro_visible = 1;
    routes[1].x = 100;
    routes[1].y = 200;
    routes[1].next[0] = 2;
    routes[2].x = 140;
    routes[2].y = 240;
    routes[2].next[0] = 1;
    map.routes = routes;
    map.route_count = 3;
    timing.window_frame_count = 1;
    timing.window_delay_ms = 10;
    timing.actor_frame_count[0][0] = 2;
    timing.actor_frame_count[0][1] = 3;
    timing.actor_delay_ms[0][0] = 10;
    timing.actor_delay_ms[0][1] = 10;
    timing.actor_frame_count[1][0] = 1;
    timing.actor_frame_count[1][1] = 2;
    timing.actor_delay_ms[1][0] = 10;
    timing.actor_delay_ms[1][1] = 10;

    assert(rich4_intro_jump_character_index(
               &session.state.players[0], 0) == GAME_JUMP_CHARACTER_COUNT - 1);
    stage = rich4_intro_stage_for_frame(&timing, &session, 0);
    assert(stage.kind == RICH4_INTRO_STAGE_WINDOW);
    stage = rich4_intro_stage_for_frame(&timing, &session, 1);
    assert(stage.kind == RICH4_INTRO_STAGE_TAKEOFF);
    assert(stage.player_index == 0 && stage.graph_frame == 0);
    stage = rich4_intro_stage_for_frame(&timing, &session, 6);
    assert(stage.kind == RICH4_INTRO_STAGE_PARACHUTE);
    assert(stage.player_index == 0);
    total = rich4_intro_total_frames(&timing, &session);
    assert(total == 9);

    rich4_intro_update(&session, &timing, &map, 0);
    assert(session.presentation.intro.frame == 0);
    assert(session.presentation.view_world_x == 100);
    assert(session.presentation.view_world_y == 200);
    for (i = 1; i <= total + 1; i++)
        rich4_intro_update(&session, &timing, &map, (uint32_t)i * 10);
    assert(session.presentation.intro.active == 0);
    assert(session.presentation.has_view_override == 0);
    assert(session.presentation.actors[1].dirty_flags & GAME_PLAYER_DIRTY_TURN);

    rich4_intro_begin(&session);
    rich4_intro_request_skip(&session);
    assert(session.presentation.intro.interrupt_requested == 1);
    session.rng_clock_offset = 4000 - 1000;
    assert(rich4_intro_finish_if_requested_at_tick(&session, 1234));
    assert(session.state.ai_rng_state == 4234);
    assert(session.presentation.intro.active == 0);
    assert(session.presentation.intro.interrupt_requested == 0);

    printf("app intro flow: ok\n");
    return 0;
}
