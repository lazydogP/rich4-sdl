#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_MODAL_MUSIC_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_MODAL_MUSIC_H

#include "app/game_session.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "data/music.h"

/* R10.1d: per-session modal music policy moved out of runtime_main.c. The
 * runtime loop supplies the small set of playlist cursors and before/after
 * activity flags; this module owns only the original transition ordering. */
typedef struct GameModalMusicContext {
    GameSession *session;
    Rich4Audio *audio;
    const char *music_dir;
    Rich4MusicBank music_bank;
    int music_enabled;
    int *main_map_music_track;
    int *main_map_music_active;
    int *confinement_music_kind;
    int *bank_music_active;
    int *lottery_music_active;
    int *magic_house_music_active;
    int *auction_music_active;
    int shop_was_active;
    int shop_music_was_started;
    int point_minigame_was_active;
    int point_minigame_music_was_started;
    int bank_was_active;
    int lottery_was_active;
    int magic_house_was_active;
    int finance_was_active;
    int finance_music_was_started;
    int settlement_was_active;
} GameModalMusicContext;

void game_capture_modal_music_context(
    GameModalMusicContext *context, GameSession *session,
    Rich4Audio *audio, const char *music_dir, Rich4MusicBank music_bank,
    int music_enabled, int *main_map_music_track,
    int *main_map_music_active, int *confinement_music_kind,
    int *bank_music_active, int *lottery_music_active,
    int *magic_house_music_active,
    int *auction_music_active);

void game_update_modal_music(GameModalMusicContext *context);

void game_restore_shop_music_if_pending(
    Rich4Audio *audio, const char *music_dir, Rich4MusicBank music_bank,
    int music_enabled, int *main_map_music_track,
    int *main_map_music_active, int *shop_music_restore_pending);

void game_roll_main_map_music_if_finished(
    Rich4Audio *audio, const char *music_dir, Rich4MusicBank music_bank,
    int music_enabled, int *main_map_music_track,
    int *main_map_music_active);

#endif
