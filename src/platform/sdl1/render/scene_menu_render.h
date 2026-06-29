#ifndef RICH4_PLATFORM_SDL1_RENDER_SCENE_MENU_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_SCENE_MENU_RENDER_H

#include <SDL/SDL.h>

#include "app/input_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "resource/mkf.h"
#include "resource/resource_store.h"

typedef struct DrawSelection {
    int has_resource_index;
    size_t resource_index;
    size_t chunk_index;
} DrawSelection;

/* R10.1b: main menu (Data.mkf[1] background + start/load/options/exit/
 * new-stage buttons) scene, moved verbatim out of runtime_main.c. Pure
 * presentation + asset lifecycle; menu_button_at is also called from the
 * event loop's click dispatch (not yet extracted), independently of
 * draw_resource_menu's own hover lookup. */

int load_menu_assets(ResourceStore *store, Rich4Mkf *mkf, MenuAssets *assets);

void free_menu_assets(MenuAssets *assets);

int menu_button_at(const MenuAssets *assets, const VirtualCursor *cursor);

int draw_resource_menu(SDL_Surface *screen,
                       const MenuAssets *assets,
                       const VirtualCursor *cursor);

int draw_spr_smp(SDL_Surface *screen,
                 Rich4Mkf *mkf,
                 const DrawSelection *selection);

#endif
