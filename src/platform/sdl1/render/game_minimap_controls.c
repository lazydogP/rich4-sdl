#include "platform/sdl1/render/game_minimap_controls.h"
#include "platform/sdl1/render/blit.h"
#include "resource/spr_smp.h"
#include "view/view_constants.h"

void draw_game_minimap_rotate_buttons(SDL_Surface *screen,
                                             const GamePreviewAssets *assets,
                                             const GameViewModel *view,
                                             int minimap_y)
{
    int i;
    int left_x = GAME_MINIMAP_ROTATE_LEFT_X;
    int right_x = GAME_MINIMAP_ROTATE_RIGHT_X;
    int y = minimap_y + 3;
    uint16_t bg = 0x2104;
    uint16_t edge = 0xffff;
    uint16_t arrow = 0xffe0;

    if (screen == NULL || view == NULL || !view->active) {
        return;
    }

    if (assets != NULL && assets->map.has_map_control) {
        Rich4SprSmpChunk left_chunk;
        Rich4SprSmpChunk right_chunk;
        if (rich4_spr_smp_get_chunk(assets->map.map_control_data,
                                        assets->map.map_control_size,
                                        &assets->map.map_control_spr,
                                        view->hud.minimap_rotate_pressed == 1 ? 18 : 20,
                                        &left_chunk) == RICH4_SPR_SMP_OK &&
            rich4_spr_smp_get_chunk(assets->map.map_control_data,
                                        assets->map.map_control_size,
                                        &assets->map.map_control_spr,
                                        view->hud.minimap_rotate_pressed == 2 ? 19 : 21,
                                        &right_chunk) == RICH4_SPR_SMP_OK) {
            if (memcmp(assets->map.map_control_spr.signature, "SPR", 4) == 0) {
                draw_spr_chunk_at(screen,
                                  assets->map.map_control_data,
                                  assets->map.map_control_size,
                                  &assets->map.map_control_spr,
                                  &left_chunk,
                                  left_x,
                                  y,
                                  1);
                draw_spr_chunk_at(screen,
                                  assets->map.map_control_data,
                                  assets->map.map_control_size,
                                  &assets->map.map_control_spr,
                                  &right_chunk,
                                  right_x,
                                  y,
                                  1);
            } else {
                draw_chunk_at(screen,
                              assets->map.map_control_data,
                              &left_chunk,
                              left_x,
                              y,
                              1);
                draw_chunk_at(screen,
                              assets->map.map_control_data,
                              &right_chunk,
                              right_x,
                              y,
                              1);
            }
            return;
        }
    }

    SDL_LockSurface(screen);
    fill_rect(screen, left_x, y, GAME_MINIMAP_ROTATE_W, GAME_MINIMAP_ROTATE_H, bg);
    fill_rect(screen, right_x, y, GAME_MINIMAP_ROTATE_W, GAME_MINIMAP_ROTATE_H, bg);
    stroke_rect(screen, left_x, y, GAME_MINIMAP_ROTATE_W, GAME_MINIMAP_ROTATE_H, edge);
    stroke_rect(screen, right_x, y, GAME_MINIMAP_ROTATE_W, GAME_MINIMAP_ROTATE_H, edge);
    for (i = 0; i < 8; i++) {
        fill_rect(screen, left_x + 16 - i, y + 6 + i, 3, 3, arrow);
        fill_rect(screen, left_x + 16 - i, y + 18 - i, 3, 3, arrow);
        fill_rect(screen, right_x + 7 + i, y + 6 + i, 3, 3, arrow);
        fill_rect(screen, right_x + 7 + i, y + 18 - i, 3, 3, arrow);
    }
    fill_rect(screen, left_x + 8, y + 12, 12, 3, arrow);
    fill_rect(screen, right_x + 7, y + 12, 12, 3, arrow);
    SDL_UnlockSurface(screen);
}
