#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <SDL/SDL.h>
#undef main
#include "app/ai_settings_flow.h"
#include "core/turn_rotation.h"
#include "platform/sdl1/game_loop_event_poll.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/app_asset_types.h"
#include "app/game_update_flow.h"

static void check_confirm_starts_ai(void)
{
    static GameSession s;
    static GamePreviewAssets assets;
    NewGameSettings settings = {0};
    GameMouseButtonUpInputContext mouse = {0};
    VirtualCursor cursor = {0};
    Rich4AppGameUpdatePlan plan;
    int left_down = 1;
    s.state.active = 1;
    s.state.player_count = 2;
    s.state.players[0].play_state = 1;
    s.state.players[1].play_state = 1;
    s.state.players[0].ndices = 1;
    s.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
    assert(rich4_turn_start_prepare(&s, 10) == RICH4_TURN_START_HUMAN);
    assert(!(s.presentation.actors[0].dirty_flags & GAME_PLAYER_DIRTY_TURN));
    s.presentation.has_view_override = 1;
    s.presentation.view_world_x = 900;
    s.presentation.view_world_y = 1000;
    s.presentation.map_direction = 5;
    rich4_ai_settings_open(&s.presentation.ai_settings, &s);
    rich4_ai_settings_apply(&s.presentation.ai_settings, &s, 100, 0);
    s.presentation.ai_settings.pressed_control = 9;
    mouse.app_state = RICH4_SCENE_GAME;
    mouse.session = &s;
    mouse.cursor = &cursor;
    mouse.mouse_left_down = &left_down;
    mouse.button = SDL_BUTTON_LEFT;
    assert(game_handle_mouse_button_up_input(&mouse));
    assert(!s.presentation.ai_settings.active && s.state.players[0].is_ai);
    assert(rich4_app_game_update_prepare(&s, 20, &plan));
    assert(plan.turn_start.action == RICH4_TURN_START_AI);
    assert(!s.presentation.has_view_override);
    assert(s.presentation.map_direction == 5);
    game_apply_turn_start_request(&s, &assets, &settings, 20, &plan.turn_start);
    /* Execute the real SDL AI command bridge, not only the hosted flag. */
    assert(s.flow.turn_flow_active);
    assert(s.flow.turn_state == GAME_TURN_STATE_PREROLL);
    assert(!s.presentation.dice.status_visible);
    assert(rich4_app_game_update_prepare(&s, 21, &plan));
    assert(plan.turn_start.action == RICH4_TURN_START_NONE);
}

static void release_key(GameEventPollContext *poll, SDLKey key)
{
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_KEYUP;
    event.key.keysym.sym = key;
    assert(SDL_PushEvent(&event) == 0);
    game_poll_runtime_events(poll);
}

int main(void)
{
    static GameSession session;
    GameKeyDownInputContext down = {0};
    GameKeyUpModalsContext up = {0};
    GameEventPollContext poll = {0};
    GameMouseButtonUpInputContext mouse = {0};
    VirtualCursor cursor = {0};
    TurnAdvanceResult result;
    Rich4SceneId scene = RICH4_SCENE_GAME;
    int running = 1;
    int left_down = 0;
    assert(SDL_Init(0) == 0);
    /* SDL's event queue requires a video subsystem; dummy opens no window. */
    SDL_putenv("SDL_VIDEODRIVER=dummy");
    assert(SDL_InitSubSystem(SDL_INIT_VIDEO) == 0);
    check_confirm_starts_ai();
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.players[0].play_state = 5;
    session.state.players[0].is_ai = 1;
    session.state.players[1].play_state = 5;
    session.state.players[1].is_ai = 1;
    down.session = up.session = &session;
    down.cursor = &cursor;
    down.cancel_key = RICH4_APP_KEY_F2;
    poll.running = &running;
    poll.app_state = &scene;
    poll.key_down = &down;
    poll.key_up = &up;
    release_key(&poll, SDLK_ESCAPE);
    assert(!session.state.ai_cancel_pending);
    session.presentation.ai_settings.active = 1;
    release_key(&poll, SDLK_F2);
    assert(!session.presentation.ai_settings.active);
    assert(!session.state.ai_cancel_pending);
    session.presentation.player_info.active = 1;
    release_key(&poll, SDLK_F2);
    assert(!session.state.ai_cancel_pending);
    session.presentation.player_info.active = 0;
    session.flow.turn_flow_active = 1; /* map motion must not block recovery */
    release_key(&poll, SDLK_F2);
    assert(session.state.ai_cancel_pending);
    assert(session.state.players[0].play_state == 5);
    assert(rich4_turn_advance_once(&session.state, &result));
    assert(session.state.players[0].play_state == 1);
    assert(session.state.players[1].play_state == 5);

    rich4_ai_settings_open(&session.presentation.ai_settings, &session);
    mouse.app_state = scene;
    mouse.session = &session;
    mouse.cursor = &cursor;
    mouse.mouse_left_down = &left_down;
    session.presentation.ai_settings.hosted[0] = 1;
    session.presentation.ai_settings.pressed_control = 9;
    assert(session.state.players[0].play_state == 1);
    mouse.button = SDL_BUTTON_LEFT;
    assert(game_handle_mouse_button_up_input(&mouse));
    assert(session.state.players[0].play_state == 5);
    rich4_ai_settings_open(&session.presentation.ai_settings, &session);
    session.presentation.ai_settings.hosted[0] = 0;
    mouse.button = SDL_BUTTON_RIGHT;
    assert(game_handle_mouse_button_up_input(&mouse));
    assert(!session.presentation.ai_settings.active);
    assert(session.state.players[0].play_state == 5);
    SDL_Quit();
    puts("AI settings SDL release and takeover: ok");
    return 0;
}
