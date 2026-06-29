#ifndef RICH4_PLATFORM_SDL1_GAME_ASSET_API_H
#define RICH4_PLATFORM_SDL1_GAME_ASSET_API_H

#include <stddef.h>
#include <SDL/SDL.h>

#include "app/game_session.h"
#include "app/scene.h"
#include "app/intro_flow.h"
#include "app/input_flow.h"
#include "core/game_command.h"
#include "core/game_state.h"
#include "data/map_data.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/runtime_options_types.h"
#include "resource/mkf.h"
#include "resource/resource_store.h"

/* Platform asset lifecycle and read-only asset-derived display queries. */
#include "platform/sdl1/game_lazy_assets.h"
int game_load_cursor_assets(ResourceStore *store, Rich4Mkf *mkf,
                            CursorAssets *assets);
void game_free_cursor_assets(CursorAssets *assets);
int game_draw_resource_cursor(SDL_Surface *screen,
                              const CursorAssets *assets,
                              const VirtualCursor *cursor);

Rich4MkfError game_preview_borrow_resource(
    GamePreviewAssets *assets, Rich4Mkf *mkf, size_t resource_index,
    unsigned char **out_data, size_t *out_size,
    Rich4MkfResourceInfo *out_info);
void game_preview_release_borrowed_view(const void *data);
Rich4ResourceHandle game_preview_handle_for_borrowed_view(
    const GamePreviewAssets *assets, const unsigned char *data);
int game_load_raw_mkf_resource(GamePreviewAssets *assets, Rich4Mkf *mkf,
                               size_t resource_index, unsigned char **out_data,
                               size_t *out_size);
int game_load_new_game_character_assets(Rich4Mkf *data_mkf,
                                        GamePreviewAssets *assets);
int game_load_game_map_routes(ResourceStore *store, const SdlOptions *options,
                              GamePreviewAssets *assets);
int game_load_game_preview_assets(ResourceStore *store,
                                  const SdlOptions *options,
                                  GamePreviewAssets *assets);
int game_load_scoped_preview_assets(ResourceStore *store, const SdlOptions *options,
    GamePreviewAssets *assets, int scope, int background, unsigned int characters);
int game_prepare_asset_scope(GamePreviewAssets *assets, int scope,
    int background, unsigned int characters);
int game_sync_runtime_asset_scope(GamePreviewAssets *assets, Rich4SceneId scene,
    const GameSession *session, const NewGameSettings *settings);
void game_free_preview_assets(GamePreviewAssets *assets);

const char *new_game_character_name(int character);
int game_confirm_dialog_bounds(const GamePreviewAssets *assets,
                              GameCommandType type,
                              int *out_left, int *out_top,
                              int *out_width, int *out_height);
/* The options dialog calls the same Data.mkf[440] button resource as the
 * other confirmations, but the original options procedure anchors it at
 * (320,200), not the ordinary (320,240) confirmation point. */
int game_options_confirm_dialog_bounds(const GamePreviewAssets *assets,
                                       int *out_left, int *out_top,
                                       int *out_width, int *out_height);
int game_dice_graph_frame_count(const GamePreviewAssets *assets, int dice_count);
Rich4IntroStage game_intro_stage_for_frame(const GamePreviewAssets *assets,
                                           const GameSession *session,
                                           int intro_frame);
int game_intro_total_frames(const GamePreviewAssets *assets,
                            const GameSession *session);
int game_first_landing_frame_count(const GamePreviewAssets *assets,
                                   const GamePlayerState *player, int fallback);
int game_first_landing_delay_ms(const GamePreviewAssets *assets,
                                const GamePlayerState *player, int fallback);
int game_tool_effect_data(const GamePreviewAssets *assets, int resource,
                          const unsigned char **out_data, size_t *out_size);
int game_special_event_asset_index(const GamePreviewAssets *assets, int resource);
int game_graph_frame_count_data(const unsigned char *data, size_t size);
int game_graph_frame_delay_ms_data(const unsigned char *data, size_t size);

#endif
