#include "data/music.h"

typedef struct Rich4MusicMap {
    const char *midi_name;
    const char *track_stem;
} Rich4MusicMap;

typedef struct Rich4MusicBankInfo {
    const char *directory;
    const char *config_name;
    const char *track_titles[8];
} Rich4MusicBankInfo;

static const Rich4MusicMap fixed_music[] = {
    { "MIDI01.MID", "track10" },
    { "MIDI02.MID", "track11" },
    { "MIDI03.MID", "track12" },
    { "MIDI04.MID", "track13" },
    { "MIDI05.MID", "track14" },
    { "MIDI06.MID", "track15" },
    { "MIDI07.MID", "track16" },
    { "MIDI08.MID", "track17" },
    { "MIDI09.MID", "track18" },
    { "MIDI10.MID", "track19" },
    { "MIDI11.MID", "track20" },
    { "MIDI12.MID", "track21" },
    { "MIDI13.MID", "track22" },
    { "MIDI14-1.MID", "track23" },
    { "MIDI14-2.MID", "track24" },
    { "MIDI15.MID", "track25" },
    { "MIDI16.MID", "track26" }
};

static const Rich4MusicBankInfo music_banks[RICH4_MUSIC_BANK_COUNT] = {
    {
        "Original",
        "original",
        {
            "\x31\x2e\xc4\xe2\xa4\xe2\xb3\xd0\xa9\x5f\xc2\xdd",
            "\x32\x2e\xc5\x77\xbc\xd6\xa6\x6e\xae\xc9\xa5\xfa",
            "\x33\x2e\xb6\xc0\xaa\xf7\xb9\xc5\xa6\x7e\xb5\xd8",
            "\x34\x2e\xac\xfc\xb9\xda\xb7\x7c\xa6\xa8\xaf\x75",
            "\x35\x2e\xa9\x5f\xa4\xdb\xa7\xae\xab\xb0\xa5\xab",
            "\x36\x2e\xac\xdd\xa7\xda\xaa\xba\xbc\x46\xae\x60",
            "\x37\x2e\xa5\x40\xac\xc9\xb3\x70\xbb\xbb\xb9\x43",
            "\x38\x2e\xa4\x6a\xb4\x49\xaf\xce\xa4\xa7\xa9\x5d"
        }
    },
    {
        "MultiverseJourney",
        "multiverse_journey",
        {
            "\x31\x2e\xac\x50\xbb\xda\xc1\x60\xb0\xca\xad\xfb",
            "\x32\x2e\xad\xab\xa6\x5e\xa8\xdc\xc4\xd8\xac\xf6",
            "\x33\x2e\xb9\xda\xa4\xdb\xa5\xec\xa8\x6c\xb6\xe9",
            "\x34\x2e\xa5\xb4\xab\xf7\xac\xb0\xb1\x4e\xa8\xd3",
            "\x35\x2e\xb7\xa6\xaa\x4c\xad\xb7\xb1\xa1\xb5\x65",
            "\x36\x2e\xae\xf6\xba\xa9\xa4\xeb\xa5\x40\xac\xc9",
            "\x37\x2e\xbc\xf6\xb1\xa1\xaa\xba\xae\x4c\xa9\x5d",
            "\x38\x2e\xba\xa9\xa8\x42\xac\x50\xaa\xc5\xa4\x55"
        }
    }
};

size_t rich4_music_fixed_count(void)
{
    return sizeof(fixed_music) / sizeof(fixed_music[0]);
}

const char *rich4_music_fixed_midi_name(size_t index)
{
    if (index >= rich4_music_fixed_count()) {
        return NULL;
    }
    return fixed_music[index].midi_name;
}

const char *rich4_music_fixed_track_stem(size_t index)
{
    if (index >= rich4_music_fixed_count()) {
        return NULL;
    }
    return fixed_music[index].track_stem;
}

const char *rich4_music_cd_track_stem(size_t cd_track)
{
    static const char *cd_tracks[] = {
        NULL,
        NULL,
        "track02",
        "track03",
        "track04",
        "track05",
        "track06",
        "track07",
        "track08",
        "track09"
    };

    if (cd_track >= sizeof(cd_tracks) / sizeof(cd_tracks[0])) {
        return NULL;
    }
    return cd_tracks[cd_track];
}

const char *rich4_music_bank_directory(Rich4MusicBank bank)
{
    if (bank < 0 || bank >= RICH4_MUSIC_BANK_COUNT) {
        return NULL;
    }
    return music_banks[bank].directory;
}

const char *rich4_music_bank_config_name(Rich4MusicBank bank)
{
    if (bank < 0 || bank >= RICH4_MUSIC_BANK_COUNT) {
        return NULL;
    }
    return music_banks[bank].config_name;
}

const char *rich4_music_bank_track_title_big5(Rich4MusicBank bank, size_t track_index)
{
    if (bank < 0 || bank >= RICH4_MUSIC_BANK_COUNT ||
        track_index >= rich4_music_bank_track_count()) {
        return NULL;
    }
    return music_banks[bank].track_titles[track_index];
}

size_t rich4_music_bank_track_count(void)
{
    return sizeof(music_banks[0].track_titles) /
           sizeof(music_banks[0].track_titles[0]);
}

int rich4_music_point_minigame_index(int action)
{
    /* 0x415344/0x41552D/0x41564D call playmid(12/11/10), which maps to
     * MIDI13/MIDI12/MIDI11 and CD-fallback tracks 22/21/20. */
    switch (action) {
    case 6: return 12;
    case 7: return 11;
    case 8: return 10;
    default: return -1;
    }
}
