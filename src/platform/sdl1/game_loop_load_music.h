#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_LOAD_MUSIC_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_LOAD_MUSIC_H

#include "app/game_session.h"
#include "data/music.h"

typedef struct Rich4Audio Rich4Audio;

/* Apply the original post-load playlist transition and clear process-local
 * modal music latches that belonged to the session being replaced. */
int game_restart_music_after_load(
    GameSession *session, Rich4Audio *audio, const char *music_dir,
    Rich4MusicBank music_bank, int music_enabled,
    int *main_map_music_track, int *main_map_music_active,
    int *confinement_music_kind, int *bank_music_active,
    int *lottery_music_active, int *magic_house_music_active,
    int *auction_music_active, int *holiday_music_active,
    int *shop_music_restore_pending);

#endif
