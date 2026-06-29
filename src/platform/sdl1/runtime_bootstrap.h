#ifndef RICH4_PLATFORM_SDL1_RUNTIME_BOOTSTRAP_H
#define RICH4_PLATFORM_SDL1_RUNTIME_BOOTSTRAP_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/runtime_options_types.h"
#include "resource/mkf.h"
#include "resource/resource_store.h"
#include <stdio.h>

int rich4_sdl_parse_options(int argc, char **argv, SdlOptions *out_options);
void rich4_sdl_print_usage(FILE *out);
int rich4_sdl_file_exists(const char *path);

Rich4Mkf *rich4_sdl_open_mkf_from_options(
    ResourceStore *store, const SdlOptions *options,
    char *out_path, size_t out_path_size);

Rich4Mkf *game_open_named_mkf_from_data_dir(
    ResourceStore *store, const char *data_dir,
    const char *const *names, size_t name_count,
    char *out_path, size_t out_path_size);

void rich4_config_clamp(Rich4Config *cfg);
void rich4_config_read(Rich4Config *cfg, const char *path);
void rich4_config_write(const Rich4Config *cfg, const char *path);

void rich4_sdl_config_clamp(Rich4SdlConfig *cfg);
void rich4_sdl_config_read(Rich4SdlConfig *cfg, const char *path);
void rich4_sdl_config_write(const Rich4SdlConfig *cfg, const char *path);

void rich4_sdl_resolve_default_paths(SdlOptions *options);

int start_menu_music(Rich4Audio *audio, const char *music_dir);
int start_fixed_music(Rich4Audio *audio, const char *music_dir,
                      size_t music_index, const char *label);
int start_fixed_music_once(Rich4Audio *audio, const char *music_dir,
                           size_t music_index, const char *label);
int start_music_bank_track(Rich4Audio *audio, const char *music_dir,
                           Rich4MusicBank bank, size_t track_index,
                           const char *label);
int start_new_game_music(Rich4Audio *audio, const char *music_dir);
int play_start_avi_once(const char *data_dir);
int play_ending_avi_sequence(const char *data_dir, int stage,
                             int winner_character, int completed_map_mask);

#endif
