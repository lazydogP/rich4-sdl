#ifndef RICH4_DATA_MUSIC_H
#define RICH4_DATA_MUSIC_H

#include <stddef.h>

typedef enum Rich4MusicBank {
    RICH4_MUSIC_BANK_ORIGINAL = 0,
    RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY = 1,
    RICH4_MUSIC_BANK_COUNT = 2
} Rich4MusicBank;

const char *rich4_music_fixed_midi_name(size_t index);
const char *rich4_music_fixed_track_stem(size_t index);
size_t rich4_music_fixed_count(void);
const char *rich4_music_cd_track_stem(size_t cd_track);
const char *rich4_music_bank_directory(Rich4MusicBank bank);
const char *rich4_music_bank_config_name(Rich4MusicBank bank);
const char *rich4_music_bank_track_title_big5(Rich4MusicBank bank, size_t track_index);
size_t rich4_music_bank_track_count(void);
/* Returns the zero-based fixed_music/playmid index for actions 6..8. */
int rich4_music_point_minigame_index(int action);

#endif

