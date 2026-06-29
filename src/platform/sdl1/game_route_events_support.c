#include "platform/sdl1/game_lazy_assets.h"
#include "core/game_command.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/input_adapter.h"

/* SDL-only route interaction geometry. Route facts are app-owned. */
static void game_confirm_dialog_anchor(GameCommandType type,
                                       int *out_x,
                                       int *out_y)
{
    /* sub_440BA8 uses (220,320). The two ported special-event confirm
     * call sites use (320,240). */
    int x = type == GAME_COMMAND_PURCHASE_PROPERTY ||
            type == GAME_COMMAND_UPGRADE_PROPERTY ? 220 : 320;
    int y = type == GAME_COMMAND_PURCHASE_PROPERTY ||
            type == GAME_COMMAND_UPGRADE_PROPERTY ? 320 : 240;
    if (out_x != NULL) *out_x = x;
    if (out_y != NULL) *out_y = y;
}

static int game_confirm_dialog_bounds_at(const GamePreviewAssets *assets,
                                         int anchor_x, int anchor_y,
                                         int *out_left, int *out_top,
                                         int *out_width, int *out_height)
{
    Rich4SprSmpChunk chunk;
    if (assets == NULL || !game_asset_ready(assets, &assets->ui.has_confirm_dialog) ||
        rich4_spr_smp_get_chunk(assets->ui.confirm_dialog_data,
                                assets->ui.confirm_dialog_size,
        &assets->ui.confirm_dialog_spr,
        0, &chunk) != RICH4_SPR_SMP_OK)
        return 0;
    /* The original centers the raw rectangle and does not use hotspot data. */
    if (out_left != NULL) *out_left = anchor_x - chunk.width / 2;
    if (out_top != NULL) *out_top = anchor_y - chunk.height / 2;
    if (out_width != NULL) *out_width = chunk.width;
    if (out_height != NULL) *out_height = chunk.height;
    return 1;
}

int game_confirm_dialog_bounds(const GamePreviewAssets *assets,
                               GameCommandType type,
                               int *out_left,
                               int *out_top,
                               int *out_width,
                               int *out_height)
{
    int anchor_x;
    int anchor_y;
    game_confirm_dialog_anchor(type, &anchor_x, &anchor_y);
    return game_confirm_dialog_bounds_at(
        assets, anchor_x, anchor_y, out_left, out_top,
        out_width, out_height);
}

int game_options_confirm_dialog_bounds(const GamePreviewAssets *assets,
                                       int *out_left, int *out_top,
                                       int *out_width, int *out_height)
{
    return game_confirm_dialog_bounds_at(
        assets, 320, 200, out_left, out_top, out_width, out_height);
}
