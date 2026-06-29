#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#undef main
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/render/blit.h"
#include "resource/mkf.h"

/* 0x441F73 uses an opaque card blit, unlike the transparent title panel.
 * Check all original card pixels over a nonblack destination. */
int main(void)
{
    static GamePreviewAssets assets;
    Rich4Mkf *mkf = NULL;
    SDL_Surface *surface = SDL_CreateRGBSurface(
        SDL_SWSURFACE, 640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    int card;
    assert(surface != NULL);
    assert(rich4_mkf_open("assets/original/Data.mkf", &mkf) == RICH4_MKF_OK);
    for (card = 0; card < 30; ++card) {
        unsigned char *data = NULL;
        size_t size = 0;
        int x, y;
        assert(rich4_mkf_read_resource(mkf, 571 + card, &data, &size,
                                      NULL) == RICH4_MKF_OK);
        assert(size == 165u * 256u * 2u);
        assets.ui.has_card_effect[card] = 1;
        assets.ui.card_effect_data[card] = data;
        assets.ui.card_effect_size[card] = size;
        SDL_FillRect(surface, NULL, 0xf81f);
        draw_original_card_presentation(surface, &assets, card, NULL);
        for (y = 0; y < 256; ++y) {
            const uint16_t *row = (const uint16_t *)(
                (const unsigned char *)surface->pixels + (200 + y) * surface->pitch);
            for (x = 0; x < 165; ++x) {
                size_t offset = (y * 165 + x) * 2;
                uint16_t pixel = data[offset] | ((uint16_t)data[offset + 1] << 8);
                assert(row[138 + x] == map_resource_pixel(surface, pixel));
            }
            assert(row[137] == 0xf81f && row[303] == 0xf81f);
        }
        free(data);
        assets.ui.card_effect_data[card] = NULL;
    }
    rich4_mkf_close(mkf);
    SDL_FreeSurface(surface);
    puts("original card presentation: 30 cards pixel-exact after color mapping");
    return 0;
}
