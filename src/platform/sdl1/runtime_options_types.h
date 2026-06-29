#ifndef RICH4_PLATFORM_SDL1_RUNTIME_OPTIONS_TYPES_H
#define RICH4_PLATFORM_SDL1_RUNTIME_OPTIONS_TYPES_H

#include <stddef.h>

/* Platform-owned startup options. The runtime parses these values and
 * resource/bootstrap seams borrow them; app flows never see this type. */
typedef struct SdlOptions {
    const char *data_dir;
    const char *music_dir;
    const char *input_config;
    const char *font_path;
    const char *mkf_name;
    const char *program_path;
    char resolved_font_path[512];
    int custom_data_dir;
    int custom_music_dir;
    int custom_input_config;
    int custom_font_path;
    int has_resource_index;
    size_t resource_index;
    int has_chunk_index;
    size_t chunk_index;
    size_t smoke_frame_limit;
    int smoke_start_game;
} SdlOptions;

#endif
