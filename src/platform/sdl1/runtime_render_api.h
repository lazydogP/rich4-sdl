#ifndef RICH4_PLATFORM_SDL1_RUNTIME_RENDER_API_H
#define RICH4_PLATFORM_SDL1_RUNTIME_RENDER_API_H

#include <stddef.h>
#include <stdint.h>
#include <SDL/SDL.h>

#include "core/character_template.h"

/* Narrow platform rendering/text services and pure display projections. */
int clamp_int(int value, int min_value, int max_value);
int point_in_rect(int px, int py, int x, int y, int w, int h);
void trace_game_start(const char *message);
void draw_text(SDL_Surface *screen, int x, int y, const char *text,
               int scale, uint16_t color);
void format_hud_number(char *out, size_t out_size, int value, int money_prefix);
void draw_big5_text_with_spacing(SDL_Surface *screen,
                                 int x, int y, const char *text, int height,
                                 uint16_t fg, uint16_t shadow, int style,
                                 int spacing, int anchor);
void draw_big5_text(SDL_Surface *screen,
                    int x, int y, const char *text, int height,
                    uint16_t fg, uint16_t shadow, int style, int anchor);

uint16_t game_player_color(int player);
int game_world_to_minimap_x(int world_x);
int game_world_to_minimap_y_at(int world_y, int minimap_y);
int game_map_view_sprite_for_level(int background_index, int level);
int game_map_overlay_sprite(int background_index);
int game_map_stage_tile_sprite(int background_index, int tile_index);
int game_map_facility_sprite(int background_index, int type, int level);
int wrap_index(int value, int count);

#endif
