#include "platform/sdl1/game_lazy_assets.h"
#include <stdio.h>
#include <SDL/SDL.h>

#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_loop_tool_panel_internal.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_route_render.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "resource/spr_smp.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"

int game_tool_panel_route_target(const GamePreviewAssets *assets,
                                 const NewGameSettings *settings,
                                 const GameSession *session, int x, int y,
                                 int require_placeable)
{
    GameSceneView view;
    const MapData *map;
    int background, best = -1, best_distance = 0x7fffffff, i;
    if (assets == NULL || settings == NULL || session == NULL ||
        !build_game_session_render_view(assets, settings, session, &view))
        return -1;
    background = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background < 0 || background >= GAME_BACKGROUND_COUNT) return -1;
    map = &assets->map.maps[background];
    for (i = 1; i < session->state.tool_route_node_count &&
                    (size_t)i < map->route_count; i++) {
        int sx, sy, dx, dy, distance;
        if (require_placeable &&
            !session->state.tool_route_node_placeable[i]) continue;
        rich4_game_viewport_project(&view.game.viewport, map->routes[i].x,
                                    map->routes[i].y, &sx, &sy);
        dx = sx - x; dy = sy - y; distance = dx * dx + dy * dy;
        if (distance <= 24 * 24 && distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }
    return best;
}

GameDomainRef game_tool_panel_property_target(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, int x, int y)
{
    GameSceneView view;
    const MapData *map;
    int background, best_distance = 0x7fffffff, i;
    GameDomainRef best = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    if (assets == NULL || settings == NULL || session == NULL ||
        !build_game_session_render_view(assets, settings, session, &view))
        return best;
    background = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background < 0 || background >= GAME_BACKGROUND_COUNT) return best;
    map = &assets->map.maps[background];
    for (i = 0; i < session->state.land_count &&
                    (size_t)(i + 1) < map->land_count; i++) {
        int sx, sy, dx, dy, distance;
        rich4_game_viewport_project(&view.game.viewport, map->lands[i + 1].x,
                                    map->lands[i + 1].y, &sx, &sy);
        dx = sx - x; dy = sy - y; distance = dx * dx + dy * dy;
        if (distance <= 24 * 24 && distance < best_distance) {
            best_distance = distance;
            best = rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
        }
    }
    for (i = 0; i < session->state.facility_count &&
                    (size_t)(i + 1) < map->facility_count; i++) {
        int sx, sy, dx, dy, distance;
        rich4_game_viewport_project(&view.game.viewport,
                                    map->facilities[i + 1].x,
                                    map->facilities[i + 1].y, &sx, &sy);
        dx = sx - x; dy = sy - y; distance = dx * dx + dy * dy;
        if (distance <= 24 * 24 && distance < best_distance) {
            best_distance = distance;
            best = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
        }
    }
    return best;
}

GameDomainRef game_tool_panel_any_target(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, int x, int y)
{
    GameSceneView view;
    const MapData *map;
    GameDomainRef best = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    int background, best_distance = 0x7fffffff, i;
    if (assets == NULL || settings == NULL || session == NULL ||
        !build_game_session_render_view(assets, settings, session, &view))
        return best;
    background = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background < 0 || background >= GAME_BACKGROUND_COUNT) return best;
    map = &assets->map.maps[background];
#define CONSIDER_TARGET(kind_, id_, world_x_, world_y_) do { \
        int sx_, sy_, dx_, dy_, distance_; \
        rich4_game_viewport_project(&view.game.viewport, (world_x_), \
                                    (world_y_), &sx_, &sy_); \
        dx_ = sx_ - x; dy_ = sy_ - y; \
        distance_ = dx_ * dx_ + dy_ * dy_; \
        if (distance_ <= 24 * 24 && distance_ <= best_distance) { \
            best_distance = distance_; \
            best = rich4_game_domain_ref((kind_), (id_)); \
        } \
    } while (0)
    for (i = 1; i < session->state.tool_route_node_count &&
                    (size_t)i < map->route_count; i++)
        CONSIDER_TARGET(GAME_DOMAIN_ROUTE_NODE, i,
                        map->routes[i].x, map->routes[i].y);
    for (i = 0; i < session->state.land_count &&
                    (size_t)(i + 1) < map->land_count; i++)
        CONSIDER_TARGET(GAME_DOMAIN_LAND, i,
                        map->lands[i + 1].x, map->lands[i + 1].y);
    for (i = 0; i < session->state.facility_count &&
                    (size_t)(i + 1) < map->facility_count; i++)
        CONSIDER_TARGET(GAME_DOMAIN_FACILITY, i,
                        map->facilities[i + 1].x,
                        map->facilities[i + 1].y);
    for (i = 0; i < session->state.player_count; i++) {
        int node = session->state.players[i].node;
        if ((session->state.players[i].play_state & 0xff) != 0 &&
            node > 0 && (size_t)node < map->route_count)
            CONSIDER_TARGET(GAME_DOMAIN_PLAYER, i,
                            map->routes[node].x, map->routes[node].y);
    }
    for (i = 0; i < GAME_SPECIAL_PLAYER_COUNT; i++) {
        int node = session->state.special_player_node[i];
        if (session->state.special_player_f10[i] == 0 &&
            node > 0 && (size_t)node < map->route_count)
            CONSIDER_TARGET(GAME_DOMAIN_SPECIAL_PLAYER, i,
                            map->routes[node].x, map->routes[node].y);
    }
    for (i = 0; i < GAME_TOOL_PLACED_OBJECT_COUNT; i++) {
        int node = session->state.tool_placed_objects[i].node;
        if (session->state.tool_placed_objects[i].kind != 0 &&
            node > 0 && (size_t)node < map->route_count)
            CONSIDER_TARGET(GAME_DOMAIN_MAP_OBJECT, i,
                            map->routes[node].x, map->routes[node].y);
    }
#undef CONSIDER_TARGET
    return best;
}

int draw_remote_dice_chunk(SDL_Surface *screen,
                                  const GamePreviewAssets *assets,
                                  size_t chunk_index, int x, int y)
{
    Rich4SprSmpChunk chunk;
    if (screen == NULL || assets == NULL || !game_asset_ready(assets, &assets->ui.has_remote_dice_ui) ||
        rich4_spr_smp_get_chunk(assets->ui.remote_dice_data,
                                assets->ui.remote_dice_size,
                                &assets->ui.remote_dice_spr, chunk_index,
                                &chunk) != RICH4_SPR_SMP_OK) return 0;
    /* 0x447183 and 0x44694A both call 0x4563F5.  Its 0x455B3A
     * implementation copies every RGB16 pixel; zero is black, not a
     * transparency key. */
    return draw_chunk_topleft(screen, assets->ui.remote_dice_data, &chunk,
                              x, y, 0);
}

void game_draw_tool_panel(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          const GameSession *session,
                          const ToolPanelState *panel)
{
    int i;
    if (screen == NULL || assets == NULL || session == NULL || panel == NULL ||
        panel->phase == TOOL_PANEL_CLOSED) return;
    if (panel->phase == TOOL_PANEL_INVENTORY) {
        int packed = 0;
        if (!draw_game_panel_chunk(screen, assets, 11, 1, 14, 130, 0)) {
            SDL_LockSurface(screen);
            fill_rect(screen, 14, 130, 412, 180, 0x7bef);
            SDL_UnlockSurface(screen);
        }
        for (i = 0; i < GAME_TOOL_COUNT; i++) {
            int col, row;
            char count[16];
            if (session->state.players[panel->player]
                    .tool_inventory[i] == 0) continue;
            col = packed % 5;
            row = packed / 5;
            /*
             * 0x447C6E passes the packed-cell anchor (45-16,33) to
             * sub_4562A5, which subtracts each chunk's own x/y hotspot.
             * The inventory panel itself is placed at (14,130).
             */
            draw_game_panel_chunk_at(
                screen, assets, 11, (size_t)(2 + i),
                43 + col * 80, 163 + row * 56, 1);
            snprintf(count, sizeof(count), "\xa1\xd1%u",
                     (unsigned)session->state.players[panel->player]
                         .tool_inventory[i]);
            game_draw_inventory_text(screen, 93 + col * 80,
                                153 + row * 56, count, 1);
            packed++;
        }
        if ((session->state.players[panel->player].traffic_method & 3) == 1)
            draw_game_panel_chunk_at(screen, assets, 11, 15, 339, 247, 1);
        else if ((session->state.players[panel->player].traffic_method & 3) == 2)
            draw_game_panel_chunk_at(screen, assets, 11, 16, 339, 247, 1);
        if (panel->hover >= 0 || panel->selected_tool >= 0) {
            int highlighted = panel->hover >= 0
                ? panel->hover : panel->selected_tool;
            int packed_index = 0;
            for (i = 0; i < highlighted; i++)
                if (session->state.players[panel->player]
                        .tool_inventory[i] != 0) packed_index++;
            SDL_LockSurface(screen);
            stroke_rect(screen,
                20 + (packed_index % 5) * 80,
                136 + (packed_index / 5) * 56, 78, 54, 0xffff);
            SDL_UnlockSurface(screen);
        }
        return;
    }
    if (panel->phase == TOOL_PANEL_REMOTE_DICE) {
        if (!draw_remote_dice_chunk(screen, assets, 0, 92, 300)) {
            SDL_LockSurface(screen);
            fill_rect(screen, 92, 300, 256, 55, 0x7bef);
            stroke_rect(screen, 92, 300, 256, 55, 0x0000);
            SDL_UnlockSurface(screen);
        }
        if (panel->hover >= 0 && panel->hover < 6)
            draw_remote_dice_chunk(
                screen, assets, (size_t)(1 + panel->hover),
                104 + panel->hover * 40, 314);
        return;
    }
    if (panel->phase == TOOL_PANEL_SPEECH) return;
    if (panel->phase == TOOL_PANEL_WORKER_CHOICE) {
        draw_facility_choice_selector(screen, assets, panel->hover);
        return;
    }
    /* Tools share 0x446AE8 -> 0x445E4D with cards.  The original map
     * selector draws no extra route-target instruction banner. */
}
