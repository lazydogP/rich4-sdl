/* R10.1d: runtime asset/MKF acquisition moved verbatim out of
 * runtime_main.c. Release ownership remains with the caller. */

#include <stdio.h>
#include <string.h>

#include "platform/sdl1/render/scene_menu_render.h"
#include "platform/sdl1/runtime_asset_bootstrap.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_loop_orchestration.h"

void game_load_runtime_asset_bundle(
    RuntimeAssetBootstrapContext *context)
{
    static const char *effect_names[] = {
        "Effect.mkf", "EFFECT.MKF", "effect.mkf"
    };
    static const char *speaking_names[] = {
        "Speaking.mkf", "SPEAKING.MKF", "speaking.mkf"
    };
    char effect_path[512];
    char speaking_path[512];

    *context->effect_mkf = game_open_named_mkf_from_data_dir(
        context->resource_store, context->options->data_dir,
        effect_names, sizeof(effect_names) / sizeof(effect_names[0]),
        effect_path, sizeof(effect_path));
    if (*context->effect_mkf != NULL)
        printf("opened Effect MKF: %s\n", effect_path);

    *context->speaking_mkf = game_open_named_mkf_from_data_dir(
        context->resource_store, context->options->data_dir,
        speaking_names, sizeof(speaking_names) / sizeof(speaking_names[0]),
        speaking_path, sizeof(speaking_path));
    if (*context->speaking_mkf != NULL)
        printf("opened Speaking MKF: %s\n", speaking_path);

    memset(context->menu_assets, 0, sizeof(*context->menu_assets));
    if (!context->options->has_resource_index &&
        load_menu_assets(context->resource_store, context->mkf,
                         context->menu_assets)) {
        printf("loaded original menu assets from resource 1\n");
    } else {
        fprintf(stderr, "original menu assets not available\n");
    }

    memset(context->cursor_assets, 0, sizeof(*context->cursor_assets));
    if (game_load_cursor_assets(context->resource_store, context->mkf,
                                context->cursor_assets)) {
        printf("loaded original cursor asset from resource 0 chunk %lu\n",
               (unsigned long)context->cursor_assets->chunk_index);
    } else {
        fprintf(stderr, "original cursor asset not available\n");
    }

    game_load_scoped_preview_assets(
        context->resource_store, context->options,
        context->game_preview_assets, 1, 0, 0);
    game_load_game_map_routes(
        context->resource_store, context->options,
        context->game_preview_assets);
    game_load_new_game_character_assets(
        context->mkf, context->game_preview_assets);
    if (!game_load_help_assets(
            context->resource_store, context->options->data_dir,
            context->help_assets))
        printf("Help.mkf UI/text resources are not available\n");
}
