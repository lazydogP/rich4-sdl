/* R10.1d: process/bootstrap helpers moved out of runtime_main.c. The
 * module owns only CLI/path/config/archive/audio startup plumbing; it does not
 * own session state or the SDL event loop. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "platform/sdl1/test_adapter/diff_adapter.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#include "app/app_constants.h"
#include "core/game_constants.h"
#include "data/resource_constants.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "resource/mkf.h"
#include "data/music.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "resource/resource_store.h"

static void join_path(char *out, size_t out_size, const char *dir, const char *name)
{
    size_t len = strlen(dir);
    const char sep =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif
    if (len > 0 && (dir[len - 1] == '/' || dir[len - 1] == '\\')) {
        snprintf(out, out_size, "%s%s", dir, name);
    } else {
        snprintf(out, out_size, "%s%c%s", dir, sep, name);
    }
}

static int parse_size_arg(const char *text, size_t *out_value)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL || *text == '\0' || out_value == NULL) {
        return 0;
    }
    value = strtoul(text, &end, 10);
    if (*end != '\0') {
        return 0;
    }
    *out_value = (size_t)value;
    return 1;
}

void rich4_sdl_print_usage(FILE *out)
{
    fprintf(out,
            "usage: rich4_sdl [--data-dir DIR] [--music-dir DIR] [--input-config PATH] [--font PATH] [--mkf NAME] [--resource N] [--chunk N] [--smoke-frames N | --smoke-game-frames N]\n");
}

int rich4_sdl_parse_options(int argc, char **argv, SdlOptions *out_options)
{
    int i;

    memset(out_options, 0, sizeof(*out_options));
    out_options->data_dir = "assets/original";
    out_options->music_dir = "assets/music";
    out_options->input_config = "config/input.toml";
    out_options->font_path = "assets/fonts/rich4.ttc";
    out_options->program_path = argc > 0 ? argv[0] : NULL;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--data-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --data-dir requires a directory\n");
                return 0;
            }
            out_options->data_dir = argv[++i];
            out_options->custom_data_dir = 1;
        } else if (strcmp(argv[i], "--music-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --music-dir requires a directory\n");
                return 0;
            }
            out_options->music_dir = argv[++i];
            out_options->custom_music_dir = 1;
        } else if (strcmp(argv[i], "--input-config") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --input-config requires a path\n");
                return 0;
            }
            out_options->input_config = argv[++i];
            out_options->custom_input_config = 1;
        } else if (strcmp(argv[i], "--font") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --font requires a file path\n");
                return 0;
            }
            out_options->font_path = argv[++i];
            out_options->custom_font_path = 1;
        } else if (strcmp(argv[i], "--mkf") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --mkf requires a file name\n");
                return 0;
            }
            out_options->mkf_name = argv[++i];
        } else if (strcmp(argv[i], "--resource") == 0) {
            if (i + 1 >= argc || !parse_size_arg(argv[i + 1], &out_options->resource_index)) {
                fprintf(stderr, "error: --resource requires a non-negative integer\n");
                return 0;
            }
            out_options->has_resource_index = 1;
            i++;
        } else if (strcmp(argv[i], "--chunk") == 0) {
            if (i + 1 >= argc || !parse_size_arg(argv[i + 1], &out_options->chunk_index)) {
                fprintf(stderr, "error: --chunk requires a non-negative integer\n");
                return 0;
            }
            out_options->has_chunk_index = 1;
            i++;
        } else if (strcmp(argv[i], "--smoke-frames") == 0) {
            if (i + 1 >= argc ||
                !parse_size_arg(argv[i + 1], &out_options->smoke_frame_limit) ||
                out_options->smoke_frame_limit == 0) {
                fprintf(stderr,
                        "error: --smoke-frames requires a positive integer\n");
                return 0;
            }
            out_options->smoke_start_game = 0;
            i++;
        } else if (strcmp(argv[i], "--smoke-game-frames") == 0) {
            if (i + 1 >= argc ||
                !parse_size_arg(argv[i + 1], &out_options->smoke_frame_limit) ||
                out_options->smoke_frame_limit == 0) {
                fprintf(stderr,
                        "error: --smoke-game-frames requires a positive integer\n");
                return 0;
            }
            out_options->smoke_start_game = 1;
            i++;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            rich4_sdl_print_usage(stdout);
            exit(0);
        } else {
            fprintf(stderr, "error: unknown option: %s\n", argv[i]);
            return 0;
        }
    }
    if (out_options->has_chunk_index && !out_options->has_resource_index) {
        fprintf(stderr, "error: --chunk requires --resource\n");
        return 0;
    }
    return 1;
}

Rich4Mkf *rich4_sdl_open_mkf_from_options(ResourceStore *store,
                                       const SdlOptions *options,
                                       char *out_path,
                                       size_t out_path_size)
{
    static const char *names[] = { "data.mkf", "Data.mkf", "DATA.MKF", "panel.mkf", "Panel.mkf" };
    static const char *version_dirs[] = { "", "V3.11", "V2.01" };
    size_t i;
    size_t j;
    if (options->mkf_name != NULL) {
        for (j = 0; j < sizeof(version_dirs) / sizeof(version_dirs[0]); j++) {
            Rich4Mkf *mkf = NULL;
            Rich4MkfError err;
            char dir[512];
            if (version_dirs[j][0] == '\0') {
                snprintf(dir, sizeof(dir), "%s", options->data_dir);
            } else {
                join_path(dir, sizeof(dir), options->data_dir, version_dirs[j]);
            }
            join_path(out_path, out_path_size, dir, options->mkf_name);
            err = rich4_resource_store_open_archive(store, out_path, &mkf);
            if (err == RICH4_MKF_OK) {
                return mkf;
            }
        }
        return NULL;
    }
    for (j = 0; j < sizeof(version_dirs) / sizeof(version_dirs[0]); j++) {
        char dir[512];
        if (version_dirs[j][0] == '\0') {
            snprintf(dir, sizeof(dir), "%s", options->data_dir);
        } else {
            join_path(dir, sizeof(dir), options->data_dir, version_dirs[j]);
        }
        for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
            Rich4Mkf *mkf = NULL;
            Rich4MkfError err;
            join_path(out_path, out_path_size, dir, names[i]);
            err = rich4_resource_store_open_archive(store, out_path, &mkf);
            if (err == RICH4_MKF_OK) {
                return mkf;
            }
        }
    }
    return NULL;
}

Rich4Mkf *game_open_named_mkf_from_data_dir(ResourceStore *store,
                                            const char *data_dir,
                                            const char *const *names,
                                            size_t name_count,
                                            char *out_path,
                                            size_t out_path_size)
{
    static const char *version_dirs[] = { "", "V3.11", "V2.01" };
    size_t i;
    size_t j;

    for (j = 0; j < sizeof(version_dirs) / sizeof(version_dirs[0]); j++) {
        char dir[512];
        if (version_dirs[j][0] == '\0') {
            snprintf(dir, sizeof(dir), "%s", data_dir);
        } else {
            join_path(dir, sizeof(dir), data_dir, version_dirs[j]);
        }
        for (i = 0; i < name_count; i++) {
            Rich4Mkf *mkf = NULL;
            Rich4MkfError err;
            join_path(out_path, out_path_size, dir, names[i]);
            err = rich4_resource_store_open_archive(store, out_path, &mkf);
            if (err == RICH4_MKF_OK) {
                return mkf;
            }
        }
    }
    return NULL;
}

int rich4_sdl_file_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }
    fclose(fp);
    return 1;
}

static void rich4_config_set_defaults(Rich4Config *cfg)
{
    static const Rich4Key default_hotkeys[28] = {
        {0x26, 0}, {0x27, 0}, {0x28, 0}, {0x25, 0},
        {0x0d, 0}, {0x1b, 0}, {0x09, 0}, {0x09, 0},
        {0x59, 0}, {0x4e, 0}, {0x20, 0}, {0x44, 0},
        {0x57, 0}, {0x58, 0}, {0x43, 0}, {0x45, 0},
        {0x46, 0}, {0x4d, 0}, {0xbc, 0}, {0xbe, 0},
        {0x41, 0}, {0x56, 0}, {0x53, 0}, {0x4c, 0},
        {0x48, 0}, {0x21, 0}, {0x22, 0}, {0x51, 0x11}
    };
    if (cfg == NULL) {
        return;
    }
    memset(cfg, 0, sizeof(*cfg));
    cfg->game_speed = 1;
    cfg->animation = 1;
    cfg->music = 4;
    cfg->sound_effect = 4;
    cfg->auto_save = 1;
    cfg->view = 1;
    memcpy(cfg->hotkeys, default_hotkeys, sizeof(default_hotkeys));
}

static void rich4_config_refresh_original_date(Rich4Config *cfg)
{
    time_t now;
    struct tm *local;
    int year;

    if (cfg == NULL) return;
    now = time(NULL);
    local = r4diff_localtime(&now);
    if (local == NULL) return;
    year = local->tm_year + 1900;
    if (year < 1998) {
        year = 1998;
        cfg->month = 1;
        cfg->day = 1;
    } else if (year > 2010) {
        year = 2010;
        cfg->month = 1;
        cfg->day = 1;
    } else {
        cfg->month = (unsigned char)(local->tm_mon + 1);
        cfg->day = (unsigned char)local->tm_mday;
    }
    cfg->year = (unsigned short)year;
}

void rich4_config_clamp(Rich4Config *cfg)
{
    if (cfg == NULL) {
        return;
    }
    if (cfg->game_speed > 2) cfg->game_speed = 1;
    if (cfg->animation > 1) cfg->animation = 1;
    if (cfg->music > 4) cfg->music = 4;
    if (cfg->sound_effect > 4) cfg->sound_effect = 4;
    if (cfg->auto_save > 1) cfg->auto_save = 1;
    if (cfg->view > 2) cfg->view = 1;
}

void rich4_config_read(Rich4Config *cfg, const char *path)
{
    FILE *fp;
    unsigned char bytes[RICH4_CONFIG_SIZE];
    size_t got;

    rich4_config_set_defaults(cfg);
    rich4_config_refresh_original_date(cfg);
    if (cfg == NULL || path == NULL) {
        return;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return;
    }
    got = fread(bytes, 1, sizeof(bytes), fp);
    fclose(fp);
    if (got < sizeof(bytes)) {
        return;
    }
    cfg->game_speed = bytes[0];
    cfg->animation = bytes[1];
    cfg->music = bytes[2];
    cfg->sound_effect = bytes[3];
    cfg->auto_save = bytes[4];
    cfg->view = bytes[5];
    cfg->dummy1[0] = bytes[6];
    cfg->dummy1[1] = bytes[7];
    cfg->day = bytes[8];
    cfg->month = bytes[9];
    cfg->year = (unsigned short)(bytes[10] | (bytes[11] << 8));
    cfg->calendar_view = bytes[12] != 0;
    memcpy(cfg->dummy2, bytes + 13, sizeof(cfg->dummy2));
    memcpy(cfg->hotkeys, bytes + 16, sizeof(cfg->hotkeys));
    rich4_config_clamp(cfg);
    rich4_config_refresh_original_date(cfg);
}

void rich4_config_write(const Rich4Config *cfg, const char *path)
{
    FILE *fp;
    unsigned char bytes[RICH4_CONFIG_SIZE];

    if (cfg == NULL || path == NULL) {
        return;
    }
    memset(bytes, 0, sizeof(bytes));
    bytes[0] = cfg->game_speed;
    bytes[1] = cfg->animation;
    bytes[2] = cfg->music;
    bytes[3] = cfg->sound_effect;
    bytes[4] = cfg->auto_save;
    bytes[5] = cfg->view;
    bytes[6] = cfg->dummy1[0];
    bytes[7] = cfg->dummy1[1];
    bytes[8] = cfg->day;
    bytes[9] = cfg->month;
    bytes[10] = (unsigned char)(cfg->year & 0xff);
    bytes[11] = (unsigned char)((cfg->year >> 8) & 0xff);
    bytes[12] = cfg->calendar_view;
    memcpy(bytes + 13, cfg->dummy2, sizeof(cfg->dummy2));
    memcpy(bytes + 16, cfg->hotkeys, sizeof(cfg->hotkeys));
    fp = fopen(path, "wb");
    if (fp == NULL) {
        return;
    }
    fwrite(bytes, 1, sizeof(bytes), fp);
    fclose(fp);
}

static void rich4_sdl_config_set_defaults(Rich4SdlConfig *cfg)
{
    if (cfg == NULL) {
        return;
    }
    cfg->music_bank = RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY;
    cfg->edge_scroll_inset = RICH4_SDL_EDGE_SCROLL_INSET_DEFAULT;
}

void rich4_sdl_config_clamp(Rich4SdlConfig *cfg)
{
    if (cfg == NULL) {
        return;
    }
    if (cfg->music_bank < 0 || cfg->music_bank >= RICH4_MUSIC_BANK_COUNT) {
        cfg->music_bank = RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY;
    }
    if (cfg->edge_scroll_inset < 0 ||
        cfg->edge_scroll_inset > RICH4_SDL_EDGE_SCROLL_INSET_MAX) {
        cfg->edge_scroll_inset = RICH4_SDL_EDGE_SCROLL_INSET_DEFAULT;
    }
}

void rich4_sdl_config_read(Rich4SdlConfig *cfg, const char *path)
{
    FILE *fp;
    char line[128];

    rich4_sdl_config_set_defaults(cfg);
    if (cfg == NULL || path == NULL) {
        return;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        char value[64];
        int edge_scroll_inset;
        if (sscanf(line, "music_bank=%63s", value) == 1) {
            if (strcmp(value, "original") == 0) {
                cfg->music_bank = RICH4_MUSIC_BANK_ORIGINAL;
            } else if (strcmp(value, "multiverse_journey") == 0) {
                cfg->music_bank = RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY;
            }
        }
        if (sscanf(line, "edge_scroll_inset=%d",
                   &edge_scroll_inset) == 1)
            cfg->edge_scroll_inset = edge_scroll_inset;
    }
    fclose(fp);
    rich4_sdl_config_clamp(cfg);
}

void rich4_sdl_config_write(const Rich4SdlConfig *cfg, const char *path)
{
    FILE *fp;
    const char *bank_name;

    if (cfg == NULL || path == NULL) {
        return;
    }
    bank_name = rich4_music_bank_config_name(cfg->music_bank);
    if (bank_name == NULL) {
        return;
    }
    fp = fopen(path, "w");
    if (fp == NULL) {
        return;
    }
    fprintf(fp, "music_bank=%s\n", bank_name);
    fprintf(fp, "edge_scroll_inset=%d\n", cfg->edge_scroll_inset);
    fclose(fp);
}

static int directory_exists(const char *path)
{
    struct stat st;
    if (path == NULL) {
        return 0;
    }
    return stat(path, &st) == 0 && (st.st_mode & S_IFDIR) != 0;
}

static int resolve_font_next_to_executable(SdlOptions *options)
{
    const char *program_path;
    const char *slash;
    size_t dir_len;

    if (options == NULL || options->program_path == NULL) {
        return 0;
    }
    program_path = options->program_path;
    slash = strrchr(program_path, '/');
#ifdef _WIN32
    {
        const char *backslash = strrchr(program_path, '\\');
        if (backslash != NULL && (slash == NULL || backslash > slash)) {
            slash = backslash;
        }
    }
#endif
    if (slash == NULL) {
        return 0;
    }
    dir_len = (size_t)(slash - program_path);
    if (dir_len + sizeof("/../assets/fonts/rich4.ttc") >
        sizeof(options->resolved_font_path)) {
        return 0;
    }
    memcpy(options->resolved_font_path, program_path, dir_len);
    options->resolved_font_path[dir_len] = '\0';
    snprintf(options->resolved_font_path + dir_len,
             sizeof(options->resolved_font_path) - dir_len,
             "/../assets/fonts/rich4.ttc");
    if (!rich4_sdl_file_exists(options->resolved_font_path)) {
        options->resolved_font_path[0] = '\0';
        return 0;
    }
    options->font_path = options->resolved_font_path;
    return 1;
}

void rich4_sdl_resolve_default_paths(SdlOptions *options)
{
    if (options == NULL) {
        return;
    }
    if (!options->custom_data_dir && !directory_exists(options->data_dir) &&
        directory_exists("../assets/original")) {
        options->data_dir = "../assets/original";
    }
    if (!options->custom_music_dir && !directory_exists(options->music_dir) &&
        directory_exists("../assets/music")) {
        options->music_dir = "../assets/music";
    }
    if (!options->custom_input_config && !rich4_sdl_file_exists(options->input_config) &&
        rich4_sdl_file_exists("../config/input.toml")) {
        options->input_config = "../config/input.toml";
    }
    if (!options->custom_font_path && !rich4_sdl_file_exists(options->font_path) &&
        !resolve_font_next_to_executable(options) &&
        rich4_sdl_file_exists("../assets/fonts/rich4.ttc")) {
        options->font_path = "../assets/fonts/rich4.ttc";
    }
}

static int start_fixed_music_mode(Rich4Audio *audio,
                                  const char *music_dir,
                                  size_t music_index,
                                  const char *label,
                                  int play_once)
{
    char path[512];
    char name[64];
    const char *stem = rich4_music_fixed_track_stem(music_index);
    const char *midi_name = rich4_music_fixed_midi_name(music_index);
    static const char *extensions[] = { "ogg", "OGG", "wav", "WAV" };
    size_t i;

    if (audio == NULL || stem == NULL || midi_name == NULL) {
        return 0;
    }
    for (i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        snprintf(name, sizeof(name), "%s.%s", stem, extensions[i]);
        join_path(path, sizeof(path), music_dir, name);
        if ((play_once ? rich4_audio_play_music_once(audio, path)
                      : rich4_audio_play_music(audio, path))) {
            printf("playing %s music: %s (%s)\n", label, path, midi_name);
            return 1;
        }
    }
    snprintf(name, sizeof(name), "%s.ogg", stem);
    join_path(path, sizeof(path), music_dir, name);
    if (rich4_sdl_file_exists(path)) {
        printf("%s music exists but SDL_mixer could not load it: %s\n", label, path);
        printf("check that SDL_mixer's OGG/Vorbis runtime DLLs are next to rich4_sdl\n");
        return 0;
    }
    printf("%s music not found; %s maps to %s.ogg/%s.wav in %s\n",
           label,
           midi_name,
           stem,
           stem,
           music_dir);
    return 0;
}

int start_fixed_music(Rich4Audio *audio,
                      const char *music_dir,
                      size_t music_index,
                      const char *label)
{
    return start_fixed_music_mode(
        audio, music_dir, music_index, label, 0);
}

int start_fixed_music_once(Rich4Audio *audio,
                           const char *music_dir,
                           size_t music_index,
                           const char *label)
{
    return start_fixed_music_mode(
        audio, music_dir, music_index, label, 1);
}

int start_music_bank_track(Rich4Audio *audio,
                                  const char *music_dir,
                                  Rich4MusicBank bank,
                                  size_t track_index,
                                  const char *label)
{
    char path[512];
    char bank_path[512];
    char name[64];
    size_t cd_track = track_index + 2;
    const char *stem = rich4_music_cd_track_stem(cd_track);
    const char *bank_dir = rich4_music_bank_directory(bank);
    static const char *extensions[] = { "ogg", "OGG", "wav", "WAV" };
    size_t i;

    if (audio == NULL || stem == NULL || bank_dir == NULL ||
        track_index >= rich4_music_bank_track_count()) {
        return 0;
    }
    join_path(bank_path, sizeof(bank_path), music_dir, bank_dir);
    for (i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        snprintf(name, sizeof(name), "%s.%s", stem, extensions[i]);
        join_path(path, sizeof(path), bank_path, name);
        if (rich4_audio_play_music_once(audio, path)) {
            printf("playing %s music: %s (%s, CD track %u)\n",
                   label,
                   path,
                   rich4_music_bank_config_name(bank),
                   (unsigned int)cd_track);
            return 1;
        }
    }
    snprintf(name, sizeof(name), "%s.ogg", stem);
    join_path(path, sizeof(path), bank_path, name);
    if (rich4_sdl_file_exists(path)) {
        printf("%s music exists but SDL_mixer could not load it: %s\n", label, path);
        printf("check that SDL_mixer's OGG/Vorbis runtime DLLs are next to rich4_sdl\n");
        return 0;
    }
    printf("%s music not found; %s CD track %u maps to %s.ogg/%s.wav in %s\n",
           label,
           rich4_music_bank_config_name(bank),
           (unsigned int)cd_track,
           stem,
           stem,
           bank_path);
    return 0;
}

int start_menu_music(Rich4Audio *audio, const char *music_dir)
{
    return start_fixed_music(audio, music_dir, 0, "menu");
}

int start_new_game_music(Rich4Audio *audio, const char *music_dir)
{
    return start_fixed_music(audio, music_dir, 1, "new game setup");
}

int play_start_avi_once(const char *data_dir)
{
    char path[512];
    struct stat st;
    join_path(path, sizeof(path), data_dir, "Start.avi");
    if (stat(path, &st) != 0) {
        join_path(path, sizeof(path), data_dir, "START.AVI");
        if (stat(path, &st) != 0) return 0;
    }
#ifdef _WIN32
    {
        char command[640];
        MCIERROR error;
        snprintf(command, sizeof(command),
                 "open \"%s\" type avivideo alias rich4_start", path);
        error = mciSendStringA(command, NULL, 0, NULL);
        if (error != 0) return 0;
        error = mciSendStringA("play rich4_start wait fullscreen", NULL, 0, NULL);
        mciSendStringA("close rich4_start", NULL, 0, NULL);
        return error == 0;
    }
#else
    (void)path;
    return 0;
#endif
}

static int play_named_avi_once(const char *data_dir, const char *name,
                               const char *alias)
{
    char path[512];
    struct stat st;

    if (data_dir == NULL || name == NULL || alias == NULL) return 0;
    join_path(path, sizeof(path), data_dir, name);
    if (stat(path, &st) != 0) return 0;
#ifdef _WIN32
    {
        char command[640];
        MCIERROR error;
        snprintf(command, sizeof(command),
                 "open \"%s\" type avivideo alias %s", path, alias);
        error = mciSendStringA(command, NULL, 0, NULL);
        if (error != 0) return 0;
        {
            char play_command[128];
            snprintf(play_command, sizeof(play_command),
                     "play %s wait fullscreen", alias);
            error = mciSendStringA(play_command, NULL, 0, NULL);
        }
        {
            char close_command[128];
            snprintf(close_command, sizeof(close_command), "close %s", alias);
            mciSendStringA(close_command, NULL, 0, NULL);
        }
        return error == 0;
    }
#else
    (void)path;
    return 0;
#endif
}

int play_ending_avi_sequence(const char *data_dir, int stage,
                             int winner_character, int completed_map_mask)
{
    char winner_name[32];
    int played = 0;

    /* 0x407726 counts the four nonzero lpBuffer__27 map marks.  It is not a
     * count of active players: one completed map may be revisited by a
     * single-player game without reaching the terminal AVI branch. */
    if ((completed_map_mask & 0x0f) != 0x0f ||
        winner_character < 0 ||
        winner_character >= GAME_JUMP_CHARACTER_COUNT)
        return 0;

    if (stage != 0) {
        snprintf(winner_name, sizeof(winner_name), "END%02d.AVI",
                 winner_character + 1);
        played |= play_named_avi_once(data_dir, winner_name, "rich4_end");
        played |= play_named_avi_once(data_dir, "THANKS.AVI", "rich4_thanks");
    } else {
        played |= play_named_avi_once(data_dir, "END.AVI", "rich4_end");
        played |= play_named_avi_once(data_dir, "OVER.AVI", "rich4_over");
    }
    return played;
}
