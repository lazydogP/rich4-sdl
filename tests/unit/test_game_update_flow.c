#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/game_update_flow.h"
#include "core/game_constants.h"

static void setup(GameSession *session)
{
    memset(session, 0, sizeof(*session));
    session->state.active = 1;
    session->state.player_count = 2;
    session->flow.turn_flow_active = 1;
}

typedef struct FakeGameUpdateServices {
    int presentation_calls;
    int turn_start_calls;
    int route_calls;
    int landing_calls;
    int dice_calls;
    int movement_calls;
    int presentation_result;
    int dice_result;
} FakeGameUpdateServices;

static int fake_presentation(void *opaque)
{
    FakeGameUpdateServices *fake = opaque;
    fake->presentation_calls++;
    return fake->presentation_result;
}

static void fake_turn_start(
    void *opaque, const Rich4TurnStartRequest *request)
{
    FakeGameUpdateServices *fake = opaque;
    assert(request != NULL);
    fake->turn_start_calls++;
}

static int fake_route(void *opaque)
{
    FakeGameUpdateServices *fake = opaque;
    fake->route_calls++;
    return 1;
}

static int fake_landing(void *opaque)
{
    FakeGameUpdateServices *fake = opaque;
    fake->landing_calls++;
    return 1;
}

static int fake_dice(void *opaque)
{
    FakeGameUpdateServices *fake = opaque;
    fake->dice_calls++;
    return fake->dice_result;
}

static int fake_movement(void *opaque)
{
    FakeGameUpdateServices *fake = opaque;
    fake->movement_calls++;
    return 1;
}

static Rich4AppGameUpdateServices fake_services(
    FakeGameUpdateServices *fake)
{
    Rich4AppGameUpdateServices services;
    memset(&services, 0, sizeof(services));
    services.presentation_context = fake;
    services.turn_start_context = fake;
    services.route_context = fake;
    services.first_landing_context = fake;
    services.dice_context = fake;
    services.movement_context = fake;
    services.update_presentation = fake_presentation;
    services.apply_turn_start = fake_turn_start;
    services.update_route_events = fake_route;
    services.update_first_landing = fake_landing;
    services.update_dice = fake_dice;
    services.update_movement = fake_movement;
    return services;
}

int main(void)
{
    GameSession session;
    Rich4AppGameUpdatePlan plan;

    setup(&session);
    session.flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    assert(rich4_app_game_update_stage(&session) ==
           RICH4_APP_GAME_UPDATE_ROUTE_EVENTS);
    session.flow.turn_state = GAME_TURN_STATE_LANDING;
    assert(rich4_app_game_update_stage(&session) ==
           RICH4_APP_GAME_UPDATE_FIRST_LANDING);
    session.flow.turn_state = GAME_TURN_STATE_PREROLL;
    assert(rich4_app_game_update_stage(&session) ==
           RICH4_APP_GAME_UPDATE_DICE);
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    assert(rich4_app_game_update_stage(&session) ==
           RICH4_APP_GAME_UPDATE_MOVEMENT);
    session.flow.turn_flow_active = 0;
    assert(rich4_app_game_update_stage(&session) ==
           RICH4_APP_GAME_UPDATE_IDLE);
    session.flow.turn_flow_active = 1;
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    assert(rich4_app_game_update_prepare(&session, 123, &plan));
    assert(plan.stage == RICH4_APP_GAME_UPDATE_MOVEMENT);
    assert(plan.turn_start.action == RICH4_TURN_START_NONE);
    assert(!rich4_app_game_update_prepare(NULL, 123, &plan));

    {
        FakeGameUpdateServices fake = { 0 };
        Rich4AppGameUpdateServices services = fake_services(&fake);
        fake.presentation_result = 1;
        assert(rich4_app_game_update(&session, 123, &services) == 1);
        assert(fake.presentation_calls == 1);
        assert(fake.turn_start_calls == 0);
        assert(fake.movement_calls == 0);
    }

    session.flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    {
        FakeGameUpdateServices fake = { 0 };
        Rich4AppGameUpdateServices services = fake_services(&fake);
        assert(rich4_app_game_update(&session, 123, &services) == 1);
        assert(fake.presentation_calls == 1);
        assert(fake.turn_start_calls == 1);
        assert(fake.route_calls == 1);
        assert(fake.landing_calls == 0);
        assert(fake.dice_calls == 0);
        assert(fake.movement_calls == 0);
    }

    session.flow.turn_state = GAME_TURN_STATE_PREROLL;
    {
        FakeGameUpdateServices fake = { 0 };
        Rich4AppGameUpdateServices services = fake_services(&fake);
        fake.dice_result = 1;
        assert(rich4_app_game_update(&session, 123, &services) == 1);
        assert(fake.turn_start_calls == 1);
        assert(fake.dice_calls == 1);
        assert(fake.movement_calls == 0);
    }

    {
        FakeGameUpdateServices fake = { 0 };
        Rich4AppGameUpdateServices services = fake_services(&fake);
        fake.dice_result = 0;
        assert(rich4_app_game_update(&session, 123, &services) == 1);
        assert(fake.dice_calls == 1);
        assert(fake.movement_calls == 1);
    }

    session.flow.turn_state = GAME_TURN_STATE_LANDING;
    {
        FakeGameUpdateServices fake = { 0 };
        Rich4AppGameUpdateServices services = fake_services(&fake);
        assert(rich4_app_game_update(&session, 123, &services) == 1);
        assert(fake.landing_calls == 1);
        assert(fake.dice_calls == 0);
        assert(fake.movement_calls == 0);
    }
    puts("app game update flow: ok");
    return 0;
}
