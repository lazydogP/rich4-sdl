#include <string.h>

#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_overlay_view.h"
#include "platform/sdl1/game_scene_view_builder.h"

int build_game_overlay_render_view(const GamePreviewAssets *assets,
                                   const NewGameSettings *settings,
                                   const GameSession *session,
                                   GameOverlayRenderView *out_view)
{
    int i;
    int edge;

    memset(out_view, 0, sizeof(*out_view));
    if (session == NULL || !build_game_session_render_view(
            assets, settings, session, &out_view->scene)) return 0;
    out_view->monster_card_animation_active = session->presentation.monster_card.active;
    out_view->engineering_attack.active =
        session->presentation.engineering_attack.active;
    out_view->engineering_attack.frame =
        session->presentation.engineering_attack.effect_frame;
    out_view->engineering_attack.world_x =
        session->presentation.engineering_attack.world_x;
    out_view->engineering_attack.world_y =
        session->presentation.engineering_attack.world_y;
    out_view->monster_card_animation_resource =
        session->presentation.monster_card.resource;
    out_view->monster_card_animation_frame = session->presentation.monster_card.frame;
    out_view->tool_robot_active = session->state.tool_robot.active;
    out_view->tool_robot_node = session->state.tool_robot.node;
    out_view->tool_robot_previous_node = session->state.tool_robot.previous_node;
    out_view->tool_route_node_count = session->state.tool_route_node_count;
    if (out_view->tool_route_node_count > GAME_MAX_MAP_ROUTE_NODES)
        out_view->tool_route_node_count = GAME_MAX_MAP_ROUTE_NODES;
    for (i = 0; i < out_view->tool_route_node_count; i++) {
        out_view->tool_route_x[i] = session->state.tool_route_x[i];
        out_view->tool_route_y[i] = session->state.tool_route_y[i];
        for (edge = 0; edge < 4; edge++)
            out_view->tool_route_next[i][edge] =
                session->state.tool_route_next[i][edge];
    }
    out_view->tool_robot_tick = session->presentation.tool_robot_tick;
    out_view->tool_animation_head = session->presentation.tool_animation_head;
    out_view->tool_animation_count = session->presentation.tool_animation_count;
    if (out_view->tool_animation_head < 0 ||
        out_view->tool_animation_head >= GAME_EVENT_QUEUE_CAPACITY)
        out_view->tool_animation_head = 0;
    for (i = 0; i < GAME_EVENT_QUEUE_CAPACITY; i++) {
        out_view->tool_animation[i].active = session->presentation.tool_animation[i].active &&
            !session->presentation.tool_animation[i].animation_done &&
            session->presentation.tool_animation[i].admission_stage == 0;
        out_view->tool_animation[i].resource = session->presentation.tool_animation[i].resource;
        out_view->tool_animation[i].frame = session->presentation.tool_animation[i].frame;
        out_view->tool_animation[i].has_world_position =
            session->presentation.tool_animation[i].has_world_position;
        out_view->tool_animation[i].world_x = session->presentation.tool_animation[i].world_x;
        out_view->tool_animation[i].world_y = session->presentation.tool_animation[i].world_y;
    }
    return 1;
}
