#ifndef RICH4_PLATFORM_SDL1_RENDER_BLIT_H
#define RICH4_PLATFORM_SDL1_RENDER_BLIT_H

#include <stddef.h>
#include <stdint.h>
#include <SDL/SDL.h>

#include "platform/sdl1/app_asset_types.h"
#include "resource/spr_smp.h"

/* Platform surface primitives extracted from runtime_main.c (R10.1a).
 * These operate only on an SDL_Surface framebuffer plus decoded SPR/SMP
 * resource bytes; they hold no game state. */

void fill_background(SDL_Surface *screen);
void draw_text(SDL_Surface *screen, int x, int y, const char *text,
               int scale, uint16_t color);
void fill_rect(SDL_Surface *screen, int x, int y, int w, int h, uint16_t color);
void stroke_rect(SDL_Surface *screen, int x, int y, int w, int h, uint16_t color);
void stroke_rect_thin(SDL_Surface *screen, int x, int y, int w, int h, uint16_t color);

uint16_t map_resource_pixel(SDL_Surface *screen, uint16_t pixel);
uint16_t map_resource_pixel_adjusted(SDL_Surface *screen, uint16_t pixel, int adjust);
uint16_t map_surface_pixel_property_flash(SDL_Surface *screen, uint16_t pixel, int adjust);
uint16_t map_resource_pixel_jump_darken_minus20(SDL_Surface *screen, uint16_t pixel);

int draw_chunk_at(SDL_Surface *screen,
                  const unsigned char *data,
                  const Rich4SprSmpChunk *chunk,
                  int center_x,
                  int center_y,
                  int transparent_zero);
int draw_chunk_at_jump_darken_minus20(SDL_Surface *screen,
                                      const unsigned char *data,
                                      const Rich4SprSmpChunk *chunk,
                                      int center_x,
                                      int center_y,
                                      int transparent_zero);
int draw_spr_chunk_at(SDL_Surface *screen,
                      const unsigned char *data,
                      size_t size,
                      const Rich4SprSmpInfo *spr,
                      const Rich4SprSmpChunk *chunk,
                      int center_x,
                      int center_y,
                      int transparent_zero);
/* 0x4568C2: 16.16 nearest-neighbour scale, with unscaled sprite hotspots. */
int draw_spr_chunk_scaled(SDL_Surface *screen, const unsigned char *data,
    size_t size, const Rich4SprSmpInfo *spr, const Rich4SprSmpChunk *chunk,
    int x, int y, unsigned int scale);
int draw_spr_chunk_crop_topleft(SDL_Surface *screen,
                                const unsigned char *data,
                                size_t size,
                                const Rich4SprSmpInfo *spr,
                                const Rich4SprSmpChunk *chunk,
                                int src_x,
                                int src_y,
                                int width,
                                int height,
                                int dst_x,
                                int dst_y,
                                int transparent_zero);
int draw_game_map_parity_mask(SDL_Surface *screen,
                              const unsigned char *data,
                              const Rich4SprSmpChunk *chunk,
                              int center_x,
                              int center_y,
                              int alternate);
int draw_spr_chunk_at_owner(SDL_Surface *screen,
                            const unsigned char *data,
                            size_t size,
                            const Rich4SprSmpInfo *spr,
                            const Rich4SprSmpChunk *chunk,
                            int center_x,
                            int center_y,
                            int transparent_zero,
                            uint16_t owner_color,
                            int replace_owner_color,
                            int adjust);
int draw_chunk_topleft(SDL_Surface *screen,
                       const unsigned char *data,
                       const Rich4SprSmpChunk *chunk,
                       int dst_x,
                       int dst_y,
                       int transparent_zero);
int draw_chunk_crop_topleft(SDL_Surface *screen,
                            const unsigned char *data,
                            const Rich4SprSmpChunk *chunk,
                            int src_x,
                            int src_y,
                            int w,
                            int h,
                            int dst_x,
                            int dst_y,
                            int transparent_zero);
int draw_setup_chunk(SDL_Surface *screen,
                     const GamePreviewAssets *assets,
                     size_t chunk_index,
                     int x,
                     int y,
                     int transparent_zero);
int draw_setup_chunk_crop(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          size_t chunk_index,
                          int src_x,
                          int src_y,
                          int w,
                          int h,
                          int x,
                          int y,
                          int transparent_zero);
int draw_spr_smp_center(SDL_Surface *screen,
                        const unsigned char *data,
                        size_t size,
                        const Rich4SprSmpInfo *spr,
                        size_t chunk_index,
                        int center_x,
                        int center_y);
int draw_spr_smp_center_owner(SDL_Surface *screen,
                              const unsigned char *data,
                              size_t size,
                              const Rich4SprSmpInfo *spr,
                              size_t chunk_index,
                              int center_x,
                              int center_y,
                              int owner,
                              uint16_t owner_color,
                              int adjust);
SDL_Surface *create_game_work_surface(SDL_Surface *screen);
int draw_raw_rgb565_fullscreen(SDL_Surface *screen, const unsigned char *data, size_t size);
int draw_raw_rgb565_at(SDL_Surface *screen, const unsigned char *data,
                       size_t size, int width, int height, int x, int y);
int draw_raw_rgb565_scrolled(SDL_Surface *screen,
                             const unsigned char *data,
                             size_t size,
                             int scroll_offset,
                             int adjust);

#endif
