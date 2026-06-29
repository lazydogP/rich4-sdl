#ifndef RICH4_PLATFORM_SDL1_RENDER_SCENE_LOADING_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_SCENE_LOADING_RENDER_H

#include <SDL/SDL.h>

#include "resource/mkf.h"

/* R10.1b: the brief "LOADING" transition screen shown between new-game setup
 * and the first game-session frame, moved verbatim out of runtime_main.c.
 * Draws Data.mkf[LOADING_SCENE_RESOURCE_INDEX] full-screen, or a plain
 * fallback panel with the "LOADING" label if that resource is unavailable. */

int draw_loading_scene(SDL_Surface *screen, Rich4Mkf *mkf);

#endif
