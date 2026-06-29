#ifndef RICH4_PLATFORM_SDL1_GAME_OVERLAY_VIEW_H
#define RICH4_PLATFORM_SDL1_GAME_OVERLAY_VIEW_H

#include <SDL/SDL.h>

#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/game_scene_view.h"

typedef struct GameOverlayAnimationView {
    int active;
    int resource;
    int frame;
    int has_world_position;
    int world_x;
    int world_y;
} GameOverlayAnimationView;

typedef struct GameOverlayRenderView {
    GameSceneView scene;
    int monster_card_animation_active;
    int monster_card_animation_resource;
    int monster_card_animation_frame;
    int tool_robot_active;
    int tool_robot_node;
    int tool_robot_previous_node;
    int tool_route_node_count;
    int tool_route_x[GAME_MAX_MAP_ROUTE_NODES];
    int tool_route_y[GAME_MAX_MAP_ROUTE_NODES];
    int tool_route_next[GAME_MAX_MAP_ROUTE_NODES][4];
    Uint32 tool_robot_tick;
    int tool_animation_head;
    int tool_animation_count;
    GameOverlayAnimationView engineering_attack;
    GameOverlayAnimationView tool_animation[GAME_EVENT_QUEUE_CAPACITY];
} GameOverlayRenderView;

int build_game_overlay_render_view(const GamePreviewAssets *assets,
                                   const NewGameSettings *settings,
                                   const GameSession *session,
                                   GameOverlayRenderView *out_view);

#endif
