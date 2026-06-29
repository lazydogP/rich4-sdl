#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>

#include "app/tool_animation_flow.h"
#include "app/confinement_flow.h"
#include "core/game_constants.h"
#include "core/game_event.h"
#include "core/game_event_payload.h"
#include "app/game_session.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_loop_turns.h"

/* SDL resource/coordinate adapter for the app-owned tool animation flow. */
int game_tool_effect_data(const GamePreviewAssets *assets,
                          int resource,
                          const unsigned char **out_data,
                          size_t *out_size)
{
    int i;
    if (out_data != NULL) *out_data = NULL;
    if (out_size != NULL) *out_size = 0;
    if (assets == NULL || out_data == NULL || out_size == NULL) return 0;
    if (resource == 526 && game_asset_ready(assets, &assets->ui.has_point_minigame_effect)) {
        *out_data = assets->ui.point_minigame_effect_data;
        *out_size = assets->ui.point_minigame_effect_size;
        return 1;
    }
    if (resource == 523 && game_asset_ready(assets, &assets->tool.has_property_max_level_graph)) {
        *out_data = assets->tool.property_max_level_data;
        *out_size = assets->tool.property_max_level_size;
        return 1;
    }
    for (i = 0; i < GAME_TOOL_EFFECT_ASSET_COUNT; i++)
        if (assets->tool.tool_effect_resource[i] == resource &&
            game_asset_ready(assets, &assets->tool.has_tool_effect[i])) {
            *out_data = assets->tool.tool_effect_data[i];
            *out_size = assets->tool.tool_effect_size[i];
            return 1;
        }
    return 0;
}

static int game_tool_animation_target_xy(
    const GameSession *session, const GamePreviewAssets *assets,
    const GameEvent *event, int *out_x, int *out_y)
{
    GameDomainRef target;
    int node = -1;
    int background_index;
    if (session == NULL || assets == NULL || event == NULL ||
        out_x == NULL || out_y == NULL) return 0;
    target = rich4_game_event_tool_target(event);
    if (target.kind == GAME_DOMAIN_LAND && target.id >= 0 &&
        target.id < session->state.land_count) {
        *out_x = session->state.tool_land_x[target.id];
        *out_y = session->state.tool_land_y[target.id];
        return 1;
    }
    if (target.kind == GAME_DOMAIN_FACILITY && target.id >= 0 &&
        target.id < session->state.facility_count) {
        *out_x = session->state.tool_facility_x[target.id];
        *out_y = session->state.tool_facility_y[target.id];
        return 1;
    }
    if (target.kind == GAME_DOMAIN_STATUS &&
        target.id == GAME_STATUS_HOSPITAL) {
        background_index = session->state.stage * GAME_MAPS_PER_STAGE +
                           session->state.map;
        if (background_index >= 0 &&
            background_index < GAME_BACKGROUND_COUNT)
            return rich4_confinement_anchor_xy(
                &assets->map.maps[background_index], GAME_CONFINEMENT_HOSPITAL,
                out_x, out_y);
    }
    if (target.kind == GAME_DOMAIN_ROUTE_NODE) node = target.id;
    else if (target.kind == GAME_DOMAIN_PLAYER && target.id >= 0 &&
             target.id < session->state.player_count)
        node = session->state.players[target.id].node;
    else if (target.kind == GAME_DOMAIN_SPECIAL_PLAYER && target.id >= 0 &&
             target.id < GAME_SPECIAL_PLAYER_COUNT)
        node = session->state.special_player_node[target.id];
    else if (target.kind == GAME_DOMAIN_MAP_OBJECT && target.id >= 0 &&
             target.id < GAME_TOOL_PLACED_OBJECT_COUNT)
        node = session->state.tool_placed_objects[target.id].node;
    if (node <= 0 || node >= session->state.tool_route_node_count) return 0;
    *out_x = session->state.tool_route_x[node];
    *out_y = session->state.tool_route_y[node];
    return 1;
}

void game_tool_animation_enqueue_from_platform(
    GameSession *session, const GamePreviewAssets *assets,
    const GameEvent *event, Uint32 now)
{
    const unsigned char *data;
    size_t size;
    int frame_count;
    int world_x = 0;
    int world_y = 0;
    int has_world_position;
    if (session == NULL || event == NULL ||
        !game_tool_effect_data(assets, rich4_game_event_tool_resource(event),
                               &data, &size)) return;
    frame_count = game_graph_frame_count_data(data, size);
    if (frame_count <= 0) return;
    has_world_position = game_tool_animation_target_xy(
        session, assets, event, &world_x, &world_y);
    /* 0x43ED59 uses the old camera, not the hospital anchor.  Data[532]
     * and [552] likewise retain the arrival camera. */
    if ((rich4_game_event_tool_resource(event) == 524 ||
         rich4_game_event_tool_resource(event) == 532 ||
         rich4_game_event_tool_resource(event) == 552) &&
        session->presentation.has_view_override) {
        world_x = session->presentation.view_world_x;
        world_y = session->presentation.view_world_y;
        has_world_position = 1;
    }
    now = rich4_sdl1_ticks();
    rich4_tool_animation_enqueue(
        session, event, frame_count, game_graph_frame_delay_ms_data(data, size),
        world_x, world_y,
        rich4_game_event_tool_resource(event) == 526 ? 0 : has_world_position, now);
}
