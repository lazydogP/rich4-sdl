#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>

#include <SDL/SDL.h>

#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_overlay_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_scene_view.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "resource/flc_animation.h"
#include "resource/spr_smp.h"
#include "view/view_constants.h"
#include "data/confinement_presentation.h"

void draw_game_monster_card_animation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameOverlayRenderView *view)
{
    Rich4FlcHeader header;
    const unsigned char *animation_data;
    size_t animation_size;
    int animation_available;
    int frame;
    if (screen == NULL || assets == NULL || view == NULL ||
        !view->monster_card_animation_active) return;
    if (view->monster_card_animation_resource == 529) {
        animation_available = game_asset_ready(assets, &assets->ui.has_demolish_card_animation);
        animation_data = assets->ui.demolish_card_data;
        animation_size = assets->ui.demolish_card_size;
    } else {
        animation_available = game_asset_ready(assets, &assets->ui.has_monster_card_animation);
        animation_data = assets->ui.monster_card_data;
        animation_size = assets->ui.monster_card_size;
    }
    if (!animation_available ||
        rich4_flc_parse_header(animation_data, animation_size,
                               &header) != RICH4_FLC_OK ||
        header.frame_count <= 0) return;
    frame = view->monster_card_animation_frame;
    if (frame < 0) frame = 0;
    if (frame >= header.frame_count) frame = header.frame_count - 1;
    draw_game_graph_frame_data(
        screen, animation_data, animation_size,
        frame, (SCREEN_W - header.width) / 2,
        (SCREEN_H - header.height) / 2);
}

void draw_game_tool_robot(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          const NewGameSettings *settings,
                          const GameOverlayRenderView *view)
{
    const GameToolRobotView *robot;
    int part, chunks, frames, chunk, screen_x, screen_y;
    if (screen == NULL || assets == NULL || settings == NULL || view == NULL)
        return;
    robot = &view->scene.game.tool_robot;
    if (!robot->visible) return;
    part = robot->resource_part;
    if (!game_asset_ready(assets, &assets->character.has_tool_robot[part])) return;
    chunks = (int)assets->character.tool_robot_spr[part].num_chunks;
    frames = chunks / 8;
    if (frames <= 0) return;
    chunk = robot->visual_direction * frames;
    if (part == 1) chunk += robot->animation_frame % frames;
    rich4_game_viewport_project(&view->scene.game.viewport,
        robot->world_x, robot->world_y, &screen_x, &screen_y);
    draw_spr_smp_center(screen, assets->character.tool_robot[part].data,
        assets->character.tool_robot[part].size,
        &assets->character.tool_robot_spr[part],
        (size_t)chunk, screen_x, screen_y);
}

void draw_game_tool_animation(SDL_Surface *screen,
                              const GamePreviewAssets *assets,
                              const GameOverlayRenderView *view)
{
    const GameOverlayAnimationView *request;
    const unsigned char *data;
    size_t size;
    Rich4FlcHeader header;
    int x, y;
    if (screen == NULL || assets == NULL || view == NULL) return;
    if (view->engineering_attack.active &&
        view->engineering_attack.frame >= 0 &&
        game_asset_ready(assets, &assets->ui.has_point_minigame_effect)) {
        /* 0x40DB8F uses the building's rendered anchor; 0x40DBCA subtracts
         * (55,55). The viewport remains centered on the attacking actor. */
        rich4_game_viewport_project(&view->scene.game.viewport,
            view->engineering_attack.world_x, view->engineering_attack.world_y,
            &x, &y);
        draw_game_graph_frame_data(screen,
            assets->ui.point_minigame_effect_data,
            assets->ui.point_minigame_effect_size,
            view->engineering_attack.frame, x - 55, y - 55);
    }
    if (view->tool_animation_count <= 0) return;
    request = &view->tool_animation[view->tool_animation_head];
    if (!request->active ||
        !game_tool_effect_data(assets, request->resource, &data, &size) ||
        rich4_flc_parse_header(data, size, &header) != RICH4_FLC_OK)
        return;
    /* 0x41D476 centers the selected world coordinate in the 440x440
     * gameplay viewport before 0x45144F starts the effect. */
    x = request->has_world_position
        ? 220 - header.width / 2 : (SCREEN_W - header.width) / 2;
    y = request->has_world_position
        ? 220 - header.height / 2 : (SCREEN_H - header.height) / 2;
    /* 0x44735C and 0x40B0FD pass literal (0,40) for Data[553]/[523].
     * Their authored canvas aligns with the building anchor at (220,260). */
    if (request->resource == 553 || request->resource == 523 ||
        request->resource == 532 || request->resource == 552) {
        x = 0;
        y = 40;
    }
    /* 0x43ED59: literal ambulance origin, independent of camera target. */
    {
        const Rich4ConfinementPresentation *confinement =
            rich4_confinement_presentation_for_resource(request->resource);
        if (confinement) {
            x = 0;
            y = confinement->screen_y;
        }
    }
    if (request->resource == 526) {
        /* 0x40F636..0x40F675: property anchor, camera stays on actor. */
        rich4_game_viewport_project(&view->scene.game.viewport,
            request->world_x, request->world_y, &x, &y);
        x -= 55;
        y -= 55;
    }
    draw_game_graph_frame_data(screen, data, size, request->frame, x, y);
}
