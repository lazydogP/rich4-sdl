#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL/SDL.h>
#undef main
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/tool.h"
#include "app/tool_animation_flow.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_overlay_view.h"
#include "platform/sdl1/render/game_overlay_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "resource/mkf.h"

static GameSession session;
static GamePreviewAssets assets;
static GameOverlayRenderView view;
static NewGameSettings settings;

static void check_level(int facility, int level)
{
    int x, y;
    assert(build_game_overlay_render_view(&assets, &settings, &session, &view));
    assert((facility ? view.scene.game.facilities[0].level :
                      view.scene.game.lands[0].level) == level);
    rich4_game_viewport_project(&view.scene.game.viewport, 352, 640, &x, &y);
    assert(x == 220 && y == 260);
}

static void upgrade(int facility, int old_level, int direction)
{
    GameEventQueue events;
    GameCommand command;
    Rich4ToolAnimationCompletion completion;
    int i, delay, frames;
    unsigned char *data = assets.tool.tool_effect_data[0];
    size_t size = assets.tool.tool_effect_size[0];
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].tool_inventory[8] = 1;
    session.state.land_count = session.state.facility_count = 1;
    session.state.land_level[0] = session.state.facility_level[0] = old_level;
    session.state.facility_type[0] = 1;
    session.state.tool_land_x[0] = session.state.tool_facility_x[0] = 352;
    session.state.tool_land_y[0] = session.state.tool_facility_y[0] = 640;
    session.presentation.map_direction = direction;
    command = rich4_game_domain_command(GAME_COMMAND_USE_TOOL,
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, 8),
        rich4_game_domain_ref(facility ? GAME_DOMAIN_FACILITY : GAME_DOMAIN_LAND, 0),
        facility && old_level == 0 ? 1 : 0);
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&session.state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert((facility ? session.state.facility_level[0] :
                       session.state.land_level[0]) == old_level + 1);
    for (i = 0; i < events.count; ++i)
        if (events.events[i].type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED)
            game_tool_animation_enqueue_from_platform(
                &session, &assets, &events.events[i], 1000);
    assert(session.presentation.tool_animation_count == (old_level == 4 ? 2 : 1));
    check_level(facility, old_level);
    frames = game_graph_frame_count_data(data, size);
    delay = game_graph_frame_delay_ms_data(data, size);
    assert(frames > 44 && delay > 0);
    assert(rich4_tool_animation_update(&session, frames, delay,
        1000 + 43 * delay - 1, NULL, &completion));
    check_level(facility, old_level);
    assert(rich4_tool_animation_update(&session, frames, delay,
        1000 + 43 * delay, NULL, &completion));
    check_level(facility, old_level + 1);
    assert((facility ? session.state.facility_level[0] :
                       session.state.land_level[0]) == old_level + 1);
    (void)rich4_tool_animation_update(&session, frames, delay,
        1000 + frames * delay, NULL, &completion);
    if (old_level == 4) {
        uint32_t start = 1001 + frames * delay;
        assert(session.presentation.has_view_override);
        assert(session.presentation.tool_animation[session.presentation.tool_animation_head].resource == 523);
        frames = game_graph_frame_count_data(assets.tool.property_max_level_data,
                                             assets.tool.property_max_level_size);
        delay = game_graph_frame_delay_ms_data(assets.tool.property_max_level_data,
                                                assets.tool.property_max_level_size);
        assert(rich4_tool_animation_update(&session, frames, delay, start, NULL, &completion));
        check_level(facility, 5);
        assert(!rich4_tool_animation_update(&session, frames, delay,
            start + frames * delay, NULL, &completion));
    }
    assert(!session.presentation.tool_animation_count);
    assert(!session.presentation.has_view_override);
}

static void placement(int resource, const unsigned char *data, size_t size)
{
    SDL_Surface *actual = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
                                               0x7c00, 0x3e0, 0x1f, 0);
    SDL_Surface *expected = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
                                                 0x7c00, 0x3e0, 0x1f, 0);
    assert(actual && expected);
    SDL_FillRect(actual, NULL, 0x1234);
    SDL_FillRect(expected, NULL, 0x1234);
    memset(&view, 0, sizeof(view));
    view.tool_animation_count = 1;
    view.tool_animation[0].active = 1;
    view.tool_animation[0].has_world_position = 1;
    view.tool_animation[0].resource = resource;
    view.tool_animation[0].frame = 30;
    draw_game_tool_animation(actual, &assets, &view);
    assert(draw_game_graph_frame_data(expected, data, size, 30, 0, 40));
    assert(actual->pitch == expected->pitch);
    assert(memcmp(actual->pixels, expected->pixels, actual->pitch * 480) == 0);
    SDL_FreeSurface(actual);
    SDL_FreeSurface(expected);
}

int main(void)
{
    Rich4Mkf *mkf = NULL;
    int facility, level, direction;
    assert(rich4_mkf_open("assets/original/Data.mkf", &mkf) == RICH4_MKF_OK);
    assert(rich4_mkf_read_resource(mkf, 553, &assets.tool.tool_effect_data[0],
        &assets.tool.tool_effect_size[0], NULL) == RICH4_MKF_OK);
    assets.tool.tool_effect_resource[0] = 553;
    assets.tool.has_tool_effect[0] = 1;
    assert(rich4_mkf_read_resource(mkf, 523, &assets.tool.property_max_level_data,
        &assets.tool.property_max_level_size, NULL) == RICH4_MKF_OK);
    assets.tool.has_property_max_level_graph = 1;
    for (facility = 0; facility < 2; ++facility)
        for (level = 0; level <= 4; level += 2)
            for (direction = 0; direction < 8; ++direction)
                upgrade(facility, level, direction);
    placement(553, assets.tool.tool_effect_data[0], assets.tool.tool_effect_size[0]);
    placement(523, assets.tool.property_max_level_data, assets.tool.property_max_level_size);
    free(assets.tool.tool_effect_data[0]);
    free(assets.tool.property_max_level_data);
    rich4_mkf_close(mkf);
    puts("worker animation: original resources, alignment, frame 44 and camera restore ok");
    return 0;
}
