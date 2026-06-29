#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_FRAME_PRESENT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_FRAME_PRESENT_H

#include <SDL/SDL.h>

#include "platform/sdl1/runtime_cursor.h"

/* The cursor presentation context already is the ownership boundary. */
void game_present_runtime_frame(VirtualCursorPresentationContext *cursor);

#endif
