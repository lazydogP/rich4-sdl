#ifndef RICH4_SDL1_MIYOO_INPUT_H
#define RICH4_SDL1_MIYOO_INPUT_H

#include <SDL/SDL_events.h>

/* Mini buttons arrive as keyboard events. Keep mouse press/release timing
 * for the original callbacks, including dialogs that ignore keyboard input. */
void rich4_sdl1_miyoo_adapt_event(SDL_Event *event, int cursor_x, int cursor_y);

#endif
