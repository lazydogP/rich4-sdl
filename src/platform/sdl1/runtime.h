#ifndef RICH4_PLATFORM_SDL1_RUNTIME_H
#define RICH4_PLATFORM_SDL1_RUNTIME_H

#include <stdint.h>

typedef struct SDL_Surface SDL_Surface;

SDL_Surface *rich4_sdl1_create_window(int width,
                                      int height,
                                      int bits_per_pixel,
                                      const char *title);
uint32_t rich4_sdl1_ticks(void);
/* Smoke fixtures use a frame-advanced logical clock so timer-driven
 * presentations do not depend on host scheduling. Interactive runs retain
 * SDL_GetTicks(). */
void rich4_sdl1_set_deterministic_ticks(int enabled);
int rich4_sdl1_present(SDL_Surface *screen);
/* QA-only framebuffer export, enabled by RICH4_UI_QA_BMP. */
int rich4_sdl1_request_debug_capture(void);
void rich4_sdl1_destroy_window(SDL_Surface *screen);
void rich4_sdl1_delay(uint32_t delay_ms);
/* Input/cursor polling uses the original 20 ms timer quantum. Animation
 * deadlines may wake it sooner; rendering/presentation count towards it. */
void rich4_sdl1_begin_frame(void);
uint32_t rich4_sdl1_frame_start_ticks(void);
void rich4_sdl1_wake_at(uint32_t deadline);
void rich4_sdl1_finish_frame(void);
int rich4_sdl1_main(int argc, char **argv);

#endif
