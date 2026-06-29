#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_MAP_OBJECTS_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_MAP_OBJECTS_H

#include <SDL/SDL.h>

#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/game_scene_view.h"
#include "view/game_view_model.h"
#include "view/view_types.h"
#include "platform/sdl1/render/game_render_cache.h"

#define GAME_MAP_RENDER_QUEUE_CAPACITY \
    (GAME_MAX_MAP_OBJECTS + 2 * GAME_MAX_MAP_PROPERTIES + \
     2 * GAME_MAX_PLAYERS + 2 * GAME_SPECIAL_ACTOR_MODEL_COUNT + \
     GAME_MAP_DYNAMIC_OBJECT_COUNT + 1)

void draw_game_map_objects(SDL_Surface *screen,
                           const GamePreviewAssets *assets,
                           const NewGameSettings *settings,
                           const GameViewModel *view,
                           const GameMapDynamicObjectState *dynamic_objects);

/* Build the same visible raw-ID queue used by map rendering for the original
 * 0x409DE7/0x40A45C global-table AI scans. */
int game_map_collect_ai_objects(
    const GamePreviewAssets *assets, int background_index,
    const GameViewModel *view,
    const GameMapDynamicObjectState *dynamic_objects,
    GameVisibleMapObject *out_objects, int capacity);

/* Original 0x40A9D7/0x409B18 map-mask query used by 0x417559.  Coordinates
 * are screen coordinates; the main map occupies x=0..439, y=40..479. */
int game_map_object_at(const GamePreviewAssets *assets,
                       const NewGameSettings *settings,
                       const GameViewModel *view,
                       const GameMapDynamicObjectState *dynamic_objects,
                       int cursor_x,
                       int cursor_y,
                       GameVisibleMapObject *out_object);

void draw_game_property_flash(SDL_Surface *screen,
                              const GamePreviewAssets *assets,
                              const NewGameSettings *settings,
                              const GameViewModel *view,
                              const GameMapDynamicObjectState *dynamic_objects);

void reset_game_property_flash_cache(GameRenderCache *cache);
int draw_game_cached_property_flash(SDL_Surface *screen, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GameViewModel *view,
    const GameMapDynamicObjectState *objects, GameRenderCache *cache);

void draw_game_map_info(SDL_Surface *screen,
                        const GamePreviewAssets *assets,
                        const NewGameSettings *settings,
                        const GameSceneView *scene);

#endif
