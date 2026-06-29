#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_CONFINEMENT_EVENT_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_CONFINEMENT_EVENT_RENDER_H

#include <SDL/SDL.h>

#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_confinement_render_view.h"
#include "app/game_event_presentation_snapshot.h"

void draw_special_place_notice(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot);

/* R10.1c: confinement status/visit overlays, split out of
 * game_event_render.c to keep it under the file line-budget gate. The
 * renderer consumes a confinement snapshot assembled at the platform edge. */

void draw_confinement_status_presentation(SDL_Surface *screen,
                                          const GamePreviewAssets *assets,
                                          const GameConfinementRenderView *view);

void draw_confinement_visit_panel(SDL_Surface *screen,
                                  const GamePreviewAssets *assets,
                                  const GameConfinementRenderView *view);

#endif
