#ifndef RICH4_PLATFORM_SDL1_RUNTIME_ASSET_BOOTSTRAP_H
#define RICH4_PLATFORM_SDL1_RUNTIME_ASSET_BOOTSTRAP_H

#include "resource/mkf.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/runtime_options_types.h"
#include "resource/resource_store.h"

/* R10.1d: runtime asset/MKF acquisition moved out of runtime_main.c. The
 * process-level store and all asset objects remain owned by the caller. */
typedef struct RuntimeAssetBootstrapContext {
    ResourceStore *resource_store;
    const SdlOptions *options;
    Rich4Mkf *mkf;
    Rich4Mkf **effect_mkf;
    Rich4Mkf **speaking_mkf;
    MenuAssets *menu_assets;
    CursorAssets *cursor_assets;
    GamePreviewAssets *game_preview_assets;
    HelpAssets *help_assets;
} RuntimeAssetBootstrapContext;

void game_load_runtime_asset_bundle(
    RuntimeAssetBootstrapContext *context);

#endif
