#include <string.h>

#include <SDL/SDL.h>

#include "resource/mkf.h"
#include "platform/sdl1/app_asset_types.h"
#include "app/input_flow.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/scene_menu_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/resource_store.h"
#include "resource/spr_smp.h"
#include "view/view_constants.h"

typedef struct MenuButton {
    int x;
    int y;
    size_t normal_chunk;
    size_t highlight_chunk;
} MenuButton;

/* R10.1b: extracted verbatim from src/platform/sdl1/runtime_main.c.
 * menu_buttons is a private table shared only by menu_button_at and
 * draw_resource_menu, which moved with it. */

static const MenuButton *menu_buttons(size_t *out_count)
{
    static const MenuButton buttons[] = {
        { 190, 380, 1, 2 },  /* start */
        { 328, 380, 3, 4 },  /* load */
        { 468, 378, 5, 6 },  /* options */
        { 328, 450, 7, 8 },  /* exit */
        { 62, 380, 9, 10 }   /* new stage, V3.11 */
    };
    if (out_count != NULL) {
        *out_count = sizeof(buttons) / sizeof(buttons[0]);
    }
    return buttons;
}

int load_menu_assets(ResourceStore *store, Rich4Mkf *mkf, MenuAssets *assets)
{
    Rich4MkfError err;

    memset(assets, 0, sizeof(*assets));
    err = rich4_resource_store_borrow(store, mkf, 1, &assets->resource, NULL);
    if (err != RICH4_MKF_OK) {
        memset(assets, 0, sizeof(*assets));
        return 0;
    }
    assets->data = (unsigned char *)assets->resource.data;
    assets->size = assets->resource.size;
    if (!rich4_spr_smp_probe(assets->data, assets->size, &assets->spr) ||
        assets->spr.num_chunks < 9) {
        memset(assets, 0, sizeof(*assets));
        return 0;
    }
    assets->available = 1;
    return 1;
}

void free_menu_assets(MenuAssets *assets)
{
    if (assets == NULL) {
        return;
    }
    memset(assets, 0, sizeof(*assets));
}

int menu_button_at(const MenuAssets *assets, const VirtualCursor *cursor)
{
    size_t button_count;
    const MenuButton *buttons = menu_buttons(&button_count);
    size_t i;

    if (assets == NULL || cursor == NULL) {
        return -1;
    }
    for (i = 0; i < button_count; i++) {
        Rich4SprSmpChunk chunk;
        Rich4SprSmpError err = rich4_spr_smp_get_chunk(assets->data,
                                                        assets->size,
                                                        &assets->spr,
                                                        buttons[i].highlight_chunk,
                                                        &chunk);
        int left;
        int top;
        if (err != RICH4_SPR_SMP_OK) {
            continue;
        }
        left = buttons[i].x - chunk.x;
        top = buttons[i].y - chunk.y;
        if (point_in_rect(cursor->x, cursor->y, left, top, chunk.width, chunk.height)) {
            return (int)i;
        }
    }
    return -1;
}

int draw_resource_menu(SDL_Surface *screen,
                       const MenuAssets *assets,
                       const VirtualCursor *cursor)
{
    size_t button_count;
    const MenuButton *buttons = menu_buttons(&button_count);
    Rich4SprSmpChunk background;
    Rich4SprSmpError err;
    int hover;
    size_t i;

    if (assets == NULL || !assets->available) {
        return 0;
    }
    err = rich4_spr_smp_get_chunk(assets->data, assets->size, &assets->spr, 0, &background);
    if (err != RICH4_SPR_SMP_OK || !draw_chunk_at(screen, assets->data, &background, 0, 0, 0)) {
        return 0;
    }

    hover = menu_button_at(assets, cursor);
    for (i = 0; i < button_count; i++) {
        Rich4SprSmpChunk normal;
        if ((int)i == hover) {
            continue;
        }
        err = rich4_spr_smp_get_chunk(assets->data,
                                          assets->size,
                                          &assets->spr,
                                          buttons[i].normal_chunk,
                                          &normal);
        if (err == RICH4_SPR_SMP_OK) {
            draw_chunk_at(screen, assets->data, &normal, buttons[i].x, buttons[i].y, 1);
        }
    }
    if (hover >= 0) {
        Rich4SprSmpChunk highlight;
        err = rich4_spr_smp_get_chunk(assets->data,
                                          assets->size,
                                          &assets->spr,
                                          buttons[hover].highlight_chunk,
                                          &highlight);
        if (err == RICH4_SPR_SMP_OK) {
            draw_chunk_at(screen, assets->data, &highlight, buttons[hover].x, buttons[hover].y, 1);
        }
    }

    return 1;
}

static int draw_spr_smp_chunk(SDL_Surface *screen,
                              const unsigned char *data,
                              const Rich4SprSmpInfo *spr,
                              const Rich4SprSmpChunk *chunk,
                              size_t resource_index,
                              size_t chunk_index)
{
    if (chunk->width <= 0 || chunk->height <= 0 ||
        chunk->data_size < (uint32_t)(chunk->width * chunk->height * 2)) {
        return 0;
    }
    draw_chunk_topleft(screen, data, chunk,
                       (SCREEN_W - chunk->width) / 2,
                       (SCREEN_H - chunk->height) / 2, 0);
    printf("displayed resource %lu, %.3s chunk %lu (%dx%d)\n",
           (unsigned long)resource_index,
           spr->signature,
           (unsigned long)chunk_index,
           chunk->width,
           chunk->height);
    return 1;
}

static int draw_resource_chunk(SDL_Surface *screen,
                               Rich4Mkf *mkf,
                               size_t resource_index,
                               size_t chunk_index)
{
    unsigned char *data = NULL;
    size_t size = 0;
    Rich4SprSmpInfo spr;
    Rich4SprSmpChunk chunk;
    Rich4MkfError err;
    Rich4SprSmpError spr_err;

    err = rich4_mkf_read_resource(mkf, resource_index, &data, &size, NULL);
    if (err != RICH4_MKF_OK) {
        fprintf(stderr, "warning: cannot read resource %lu [%s]: %s\n",
                (unsigned long)resource_index,
                rich4_mkf_error_name(err),
                rich4_mkf_last_error(mkf));
        free(data);
        return 0;
    }
    if (!rich4_spr_smp_probe(data, size, &spr)) {
        fprintf(stderr, "warning: resource %lu is not SPR/SMP\n",
                (unsigned long)resource_index);
        free(data);
        return 0;
    }
    spr_err = rich4_spr_smp_get_chunk(data, size, &spr, chunk_index, &chunk);
    if (spr_err != RICH4_SPR_SMP_OK) {
        fprintf(stderr, "warning: cannot parse chunk %lu from resource %lu [%s]\n",
                (unsigned long)chunk_index,
                (unsigned long)resource_index,
                rich4_spr_smp_error_name(spr_err));
        free(data);
        return 0;
    }
    if (!draw_spr_smp_chunk(screen, data, &spr, &chunk, resource_index, chunk_index)) {
        fprintf(stderr, "warning: resource %lu chunk %lu is not drawable\n",
                (unsigned long)resource_index,
                (unsigned long)chunk_index);
        free(data);
        return 0;
    }
    free(data);
    return 1;
}

int draw_spr_smp(SDL_Surface *screen, Rich4Mkf *mkf, const DrawSelection *selection)
{
    size_t i;
    size_t count = rich4_mkf_count(mkf);

    if (selection->has_resource_index) {
        return draw_resource_chunk(screen, mkf, selection->resource_index, selection->chunk_index);
    }

    for (i = 0; i < count; i++) {
        unsigned char *data = NULL;
        size_t size = 0;
        Rich4SprSmpInfo spr;
        Rich4SprSmpChunk chunk;
        Rich4MkfError err = rich4_mkf_read_resource(mkf, i, &data, &size, NULL);
        Rich4SprSmpError spr_err;
        if (err != RICH4_MKF_OK) {
            free(data);
            continue;
        }
        if (!rich4_spr_smp_probe(data, size, &spr) || spr.num_chunks == 0) {
            free(data);
            continue;
        }
        spr_err = rich4_spr_smp_get_chunk(data, size, &spr, 0, &chunk);
        if (spr_err == RICH4_SPR_SMP_OK &&
            draw_spr_smp_chunk(screen, data, &spr, &chunk, i, 0)) {
            free(data);
            return 1;
        }
        free(data);
    }
    return 0;
}
