#include <stdlib.h>

#include <SDL/SDL.h>

#include "data/resource_constants.h"
#include "resource/mkf.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/scene_loading_render.h"
#include "platform/sdl1/runtime_render_api.h"

/* R10.1b: extracted verbatim from src/platform/sdl1/runtime_main.c. */

int draw_loading_scene(SDL_Surface *screen, Rich4Mkf *mkf)
{
    unsigned char *data = NULL;
    size_t size = 0;
    Rich4MkfError err;
    int drawn = 0;

    if (screen == NULL || mkf == NULL) {
        return 0;
    }

    err = rich4_mkf_read_resource(mkf,
                                  LOADING_SCENE_RESOURCE_INDEX,
                                  &data,
                                  &size,
                                  NULL);
    if (err == RICH4_MKF_OK) {
        drawn = draw_raw_rgb565_fullscreen(screen, data, size);
    }
    free(data);

    if (!drawn) {
        fill_background(screen);
        SDL_LockSurface(screen);
        fill_rect(screen, 176, 198, 288, 84, 0x0000);
        stroke_rect(screen, 176, 198, 288, 84, 0xffff);
        draw_text(screen, 252, 232, "LOADING", 3, 0xffff);
        SDL_UnlockSurface(screen);
    }

    return drawn;
}
