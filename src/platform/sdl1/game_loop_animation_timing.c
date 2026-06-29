#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>

#include "app/config_types.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "resource/flc_animation.h"
#include "view/view_constants.h"

/* R10.1d: shared FLC frame-count and timing queries, moved verbatim out of
 * runtime_main.c. Animation state machines remain in their game_loop modules. */

static int game_panel_graph_frame_count(const GamePreviewAssets *assets, int resource_index)
{
    if (assets == NULL || resource_index < 0 || resource_index >= 9) {
        return 0;
    }
    return game_asset_graph_frames(assets, &assets->ui.panel_data[resource_index], assets->ui.panel_size[resource_index]);
}

int game_graph_frame_count_data(const unsigned char *data, size_t size)
{
    Rich4FlcHeader header;
    if (rich4_flc_parse_header(data, size, &header) != RICH4_FLC_OK) {
        return 0;
    }
    return header.frame_count;
}

int game_graph_frame_delay_ms_data(const unsigned char *data, size_t size)
{
    Rich4FlcHeader header;
    int delay;

    if (rich4_flc_parse_header(data, size, &header) != RICH4_FLC_OK) {
        return GAME_INTRO_DEFAULT_FRAME_MS;
    }
    delay = (int)header.delay_ms;
    if (delay <= 0) {
        return GAME_INTRO_DEFAULT_FRAME_MS;
    }
    return delay;
}

int game_dice_graph_frame_count(const GamePreviewAssets *assets, int dice_count)
{
    if (dice_count < 1 || dice_count > 3) {
        return 0;
    }
    return game_panel_graph_frame_count(assets, 3 + dice_count);
}

int game_move_frame_ms_from_config(const Rich4Config *config)
{
    static const int speed_tab[3] = { 6, 4, 2 };
    int speed = config != NULL ? config->game_speed : 1;
    if (speed < 0 || speed > 2) {
        speed = 1;
    }
    return GAME_TIMER_FRAME_MS * speed_tab[speed];
}
