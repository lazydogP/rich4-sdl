/* Original: 0x4123D7, 0x413248, 0x414FCD. */
static GameSession catcher_session;
static Rich4AudioEventQueue catcher_audio;
static struct PointMinigameState *catcher_begin(void)
{
    begin_play_minigame(&catcher_session, &catcher_audio, 8, 1000);
    catcher_session.presentation.point_minigame.catcher.drop_frame = 4;
    return &catcher_session.presentation.point_minigame;
}
static void catcher_tick(const Rich4PointMinigameCatcherBounds *b, int cursor)
{
    assert(rich4_point_minigame_update(&catcher_session, 20, 114, b,
        catcher_session.presentation.point_minigame.next_tick, cursor, &catcher_audio));
}
static PointMinigameObject *catcher_object(int slot, int type, int x, int y)
{
    PointMinigameObject *o = &catcher_session.presentation.point_minigame.object[slot];
    memset(o, 0, sizeof(*o));
    o->active = 1; o->type = type; o->x = x; o->y = y;
    return o;
}
static uint32_t catcher_seed(int modulus, int remainder)
{
    uint32_t seed;
    for (seed = 1; seed < 100000; seed++) {
        uint32_t state = seed;
        if (rich4_rng_range_at(&state, modulus) == remainder) return seed;
    }
    assert(0); return 0;
}
static void test_catcher_original_collision(void)
{
    Rich4PointMinigameCatcherBounds b = {1,300,300,340,380};
    struct PointMinigameState *g = catcher_begin();
    PointMinigameObject *o = catcher_object(0,0,320,320);
    catcher_tick(&b,639);
    assert(g->catcher_x == 330 && g->catcher_direction == 2);
    assert(g->score == 0 && o->y == 344); /* Initial standing cannot catch. */
    catcher_tick(&b,330);
    assert(g->score == 10 && !o->active);
    assert(g->catcher_direction == 2 && g->catcher_frame == 1);
    assert(g->catcher_x == 330); /* Near target retains walking state. */
    g = catcher_begin(); g->catcher_direction = 1;
    o = catcher_object(0,0,320,290);
    catcher_tick(&b,320);
    assert(o->active && o->y == 314 && g->score == 0);
    assert(g->catcher.draw_objects[0].y == 290);
    catcher_tick(&b,320);
    assert(!o->active && g->score == 10); /* Collision BEFORE falling. */
    g = catcher_begin(); g->catcher_direction = 1;
    catcher_object(0,1,300,330); catcher_object(1,2,340,330);
    catcher_object(2,3,320,300); catcher_object(3,3,320,380);
    catcher_tick(&b,320);
    assert(g->score == 0 && !g->object[3].active); /* Strict bounds. */
    g = catcher_begin(); g->catcher_direction = 1; g->catcher_frame = 9;
    catcher_tick(NULL,0);
    assert(g->catcher_frame == 0 && g->catcher_x == 310);
    g->catcher_x = 10; catcher_tick(NULL,0); assert(g->catcher_x == 0);
    g->catcher.mouse_initialized = 1; g->catcher.mouse_anchor = 320;
    catcher_tick(NULL,329);
    assert(rich4_point_minigame_take_cursor_warp(&catcher_session) == 639);
    assert(rich4_point_minigame_take_cursor_warp(&catcher_session) == -1);
    catcher_tick(NULL,630);
    assert(rich4_point_minigame_take_cursor_warp(&catcher_session) == 0);
}
static void test_catcher_original_sources(void)
{
    static const int speed[5] = {24,18,15,12,15}, rem[4] = {18,15,9,0};
    struct PointMinigameState *g;
    PointMinigameObject *o;
    int i;
    for (i = 0; i < 4; i++) {
        g = catcher_begin(); g->catcher.drop_frame = 0;
        catcher_session.state.ai_rng_state = catcher_seed(20,rem[i]);
        catcher_tick(NULL,320); o = &g->object[0];
        assert(o->active && o->type == i && o->x == 110);
        assert(o->y == 100 && o->velocity_y == -16 && o->frame == 0);
        assert(!g->catcher.draw_objects[0].active);
        catcher_tick(NULL,320);
        assert(o->y == 86 && o->frame == 1 && g->catcher.draw_objects[0].y == 100);
    }
    g = catcher_begin();
    for (i = 0; i < 5; i++) catcher_object(i,i,320,132);
    g->catcher.bombs = 1; catcher_tick(NULL,320);
    for (i = 0; i < 5; i++) assert(g->object[i].y == 132 + speed[i]);
    o = &g->object[0]; o->y = 372; o->frame = 7; catcher_tick(NULL,320);
    assert(!o->active && o->frame == 0);
    assert(g->catcher.draw_objects[0].active && g->catcher.draw_objects[0].y == 372);
    g = catcher_begin(); g->catcher.drop_frame = 0;
    for (i = 0; i < 16; i++) catcher_object(i,3,320,200);
    catcher_session.state.ai_rng_state = 123; catcher_tick(NULL,320);
    assert(catcher_session.state.ai_rng_state == 123); /* Full slots: no type RNG. */
    g = catcher_begin(); g->catcher_source_x = 530; g->catcher.source_frame = 5;
    catcher_tick(NULL,320);
    assert(g->catcher_source_direction == 2 && g->catcher.source_frame == 0);
    for (i = 0; i < 5; i++) catcher_tick(NULL,320);
    assert(g->catcher_source_direction == 4 && g->catcher.source_frame == 0);
    g = catcher_begin(); g->catcher_source_x = 326;
    catcher_session.state.ai_rng_state = catcher_seed(10,7); catcher_tick(NULL,320);
    assert(g->catcher_secondary_active && g->catcher_secondary_frame == 0);
    assert(g->catcher.secondary_x >= 160 && g->catcher.secondary_x <= 299);
    g->catcher_secondary_frame = 7; g->ticks_remaining = 1;
    catcher_object(15,3,320,150); drain_audio(&catcher_audio); catcher_tick(NULL,320);
    assert(g->ticks_remaining == 0 && g->catcher.bombs == 1);
    assert(g->object[0].type == 4 && g->object[0].x == g->catcher.secondary_x);
    assert(catcher_audio.count == 2); /* Throw, not intro, starts sound. */
}
static void test_catcher_original_endings(void)
{
    Rich4PointMinigameCatcherBounds b = {1,300,300,340,380};
    struct PointMinigameState *g = catcher_begin();
    int i;
    g->catcher_direction = 1; g->counters[0] = 4;
    catcher_object(0,1,320,330); catcher_object(1,4,320,330);
    catcher_object(2,0,320,330); catcher_object(3,3,100,-1000);
    catcher_tick(&b,0);
    assert(g->score == 45 && g->result_tier == 4);
    assert(g->catcher_direction == 0 && g->catcher_x == 320);
    assert(g->object[2].active && g->counters[0] == 4);
    assert(g->catcher.explosion_frame == 1 && g->catcher_bomb_active);
    for (i = 0; i < 7; i++) catcher_tick(&b,639);
    assert(g->catcher.explosion_frame == 8 && g->phase == RICH4_POINT_MINIGAME_PLAY);
    catcher_tick(&b,639);
    assert(g->phase == RICH4_POINT_MINIGAME_RESULT && g->score == 45);
    assert(g->result_tier == 4 && !g->catcher_bomb_active);
    g = catcher_begin(); g->catcher_direction = 1; g->ticks_remaining = 1;
    catcher_object(0,0,320,290); catcher_tick(&b,320);
    assert(g->ticks_remaining == 0 && g->phase == RICH4_POINT_MINIGAME_PLAY);
    catcher_tick(&b,320);
    assert(g->phase == RICH4_POINT_MINIGAME_RESULT && g->score == 10);
    g = catcher_begin(); g->counters[0] = 100;
    finish_minigame_tick(&catcher_session,&catcher_audio,1000);
    assert(g->score == 1000);
}
static void test_catcher_original_ready(void)
{
    GameEvent event = rich4_game_event_point_minigame_request(0,8,78,79,1);
    struct PointMinigameState *g;
    int i;
    memset(&catcher_session,0,sizeof(catcher_session));
    catcher_session.state.player_count = 1; catcher_session.state.players[0].character = 3;
    rich4_audio_event_queue_init(&catcher_audio);
    rich4_point_minigame_begin(&catcher_session,&event,1000,1,&catcher_audio);
    g = &catcher_session.presentation.point_minigame;
    assert(g->phase == RICH4_POINT_MINIGAME_READY && g->catcher.move_frames == 14);
    assert(catcher_session.state.ai_rng_state == 0);
    for (i = 0; i < 10; i++) catcher_tick(NULL,320);
    assert(g->phase == RICH4_POINT_MINIGAME_INTRO && g->ticks_remaining == 360);
    for (i = 0; i <= 20; i++) assert(rich4_point_minigame_update(
        &catcher_session,20,114,NULL,1500 + 114*i,320,&catcher_audio));
    assert(g->phase == RICH4_POINT_MINIGAME_PLAY && g->ticks_remaining == 360);
    assert(catcher_audio.count == 0);
    catcher_tick(NULL,320); assert(g->ticks_remaining == 359);
}
static void test_catcher_original_cases(void)
{
    test_catcher_original_collision(); test_catcher_original_sources();
    test_catcher_original_endings(); test_catcher_original_ready();
}

static void test_catcher_reward_once(void)
{
    struct PointMinigameState *g = catcher_begin();
    RouteEvent stop;
    GameEventQueue events;
    memset(&stop,0,sizeof(stop));
    stop.phase = ROUTE_EVENT_PHASE_STOP; stop.node = 20;
    stop.snapshot.player = 0; stop.snapshot.route_action = 8;
    stop.snapshot.property_owner = -1;
    stop.snapshot.property_branch = GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED;
    catcher_session.state.active = 1;
    catcher_session.state.players[0].play_state = 1;
    catcher_session.state.players[0].points = 65530;
    rich4_route_event_executor_reset(&catcher_session.flow.route_events);
    assert(rich4_route_event_queue_push(&catcher_session.flow.route_events.queue,&stop));
    assert(rich4_route_event_executor_begin(&catcher_session.flow.route_events,0,122));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&catcher_session.flow.route_events,
        &catcher_session.state,&events) == ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    g->phase = RICH4_POINT_MINIGAME_RESULT; g->score = 45;
    g->result_tier = 4; g->finish_until = 2000; g->catcher.drop_frame = 3;
    assert(rich4_point_minigame_update(&catcher_session,0,0,NULL,1999,320,&catcher_audio));
    assert(catcher_session.state.players[0].points == 65530);
    assert(!rich4_point_minigame_update(&catcher_session,0,0,NULL,2000,320,&catcher_audio));
    assert(catcher_session.state.players[0].points == 39);
    assert(g->catcher.drop_frame == 3);
    assert(!rich4_point_minigame_update(&catcher_session,0,0,NULL,2200,320,&catcher_audio));
    assert(catcher_session.state.players[0].points == 39);
}
