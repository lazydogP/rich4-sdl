#include <assert.h>
#include <stdio.h>

#include "data/music.h"
#include "platform/sdl1/game_loop_load_music.h"
#include "platform/sdl1/game_loop_modal_music.h"

static int started_track = -1;
static Rich4MusicBank started_bank = RICH4_MUSIC_BANK_COUNT;
static int stop_count;
static int map_start_count;
static int fixed_start_count;
static int fixed_track = -1;

int rich4_game_save_take_music_restart(GameSession *session)
{
    if (session == NULL || !session->loaded_music_restart_pending) return 0;
    session->loaded_music_restart_pending = 0;
    if (session->presentation.periodic_finance.active &&
        session->presentation.periodic_finance.phase > 0)
        session->presentation.periodic_finance.music_started = 0;
    if (session->presentation.point_minigame.active)
        session->presentation.point_minigame.music_started = 0;
    if (session->presentation.shop_visit.active)
        session->presentation.shop_visit.music_started = 0;
    return 1;
}

int start_music_bank_track(Rich4Audio *audio, const char *music_dir,
                           Rich4MusicBank bank, size_t track_index,
                           const char *label)
{
    (void)audio;
    assert(music_dir != NULL);
    assert(label != NULL);
    started_bank = bank;
    started_track = (int)track_index;
    map_start_count++;
    return 1;
}

int start_fixed_music(Rich4Audio *audio, const char *music_dir,
                      size_t music_index, const char *label)
{
    (void)audio;
    assert(music_dir != NULL);
    assert(label != NULL);
    fixed_start_count++;
    fixed_track = (int)music_index;
    return 1;
}

int rich4_audio_music_playing(const Rich4Audio *audio)
{
    (void)audio;
    return 1;
}

void rich4_periodic_finance_mark_music_started(GameSession *session)
{
    session->presentation.periodic_finance.music_started = 1;
}

void rich4_point_minigame_mark_music_started(GameSession *session)
{
    session->presentation.point_minigame.music_started = 1;
}

void rich4_shop_mark_music_started(GameSession *session)
{
    session->presentation.shop_visit.music_started = 1;
}

int rich4_stock_calendar_music_index(const GameState *state)
{
    (void)state;
    return -1;
}

void rich4_audio_stop_music(Rich4Audio *audio)
{
    (void)audio;
    stop_count++;
}

static void post_load_music_restarts_playlist(void)
{
    GameSession session = {0};
    int track = 7;
    int main_active = 1;
    int confinement = 2;
    int bank = 2;
    int lottery = 1;
    int magic = 1;
    int auction = 1;
    int holiday = 4;
    int shop_restore = 1;

    assert(!game_restart_music_after_load(
        &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &confinement, &bank, &lottery, &magic,
        &auction, &holiday, &shop_restore));
    session.loaded_music_restart_pending = 1;
    assert(game_restart_music_after_load(
        &session, NULL, "music", RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY, 1,
        &track, &main_active, &confinement, &bank, &lottery, &magic,
        &auction, &holiday, &shop_restore));
    assert(!session.loaded_music_restart_pending);
    assert(track == 0 && main_active == 1);
    assert(started_track == 0 &&
           started_bank == RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY);
    assert(confinement == GAME_CONFINEMENT_NONE);
    assert(bank == 0 && lottery == 0 && magic == 0 && auction == 0 &&
           holiday == 0 && shop_restore == 0);

    session.loaded_music_restart_pending = 1;
    assert(game_restart_music_after_load(
        &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 0,
        &track, &main_active, &confinement, &bank, &lottery, &magic,
        &auction, &holiday, &shop_restore));
    assert(track == 1 && main_active == 0 && stop_count == 1);

    started_track = -1;
    session.presentation.periodic_finance.active = 1;
    session.presentation.periodic_finance.phase = 4;
    session.presentation.periodic_finance.music_started = 1;
    session.loaded_music_restart_pending = 1;
    assert(game_restart_music_after_load(
        &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &confinement, &bank, &lottery, &magic,
        &auction, &holiday, &shop_restore));
    assert(track == 2 && main_active == 0 && started_track == -1);
    assert(!session.presentation.periodic_finance.music_started);
    assert(stop_count == 2);
}

static void normal_finance_music_cannot_be_overridden(void)
{
    GameSession session = {0};
    GameModalMusicContext context;
    int track = 3;
    int main_active = 1;
    int confinement = GAME_CONFINEMENT_NONE;
    int bank = 0;
    int lottery = 0;
    int magic = 0;
    int auction = 1;
    int holiday = 0;

    session.presentation.periodic_finance.active = 1;
    session.presentation.periodic_finance.phase = 4;
    session.presentation.periodic_finance.music_started = 0;
    map_start_count = fixed_start_count = 0;
    fixed_track = -1;
    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &confinement, &bank, &lottery, &magic,
        &auction, &holiday);
    game_update_modal_music(&context);

    /* A stale closing activity used to restore the map after MIDI09 had
     * already started in the same ordinary (non-load) frame. */
    assert(fixed_start_count == 1 && fixed_track == 8);
    assert(map_start_count == 0 && main_active == 0);
    assert(session.presentation.periodic_finance.music_started == 1);
    assert(auction == 0);

    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &confinement, &bank, &lottery, &magic,
        &auction, &holiday);
    session.presentation.periodic_finance.active = 0;
    game_update_modal_music(&context);
    assert(map_start_count == 1 && main_active == 1);
}

int main(void)
{
    assert(rich4_music_point_minigame_index(6) == 12);
    assert(rich4_music_point_minigame_index(7) == 11);
    assert(rich4_music_point_minigame_index(8) == 10);
    assert(rich4_music_point_minigame_index(5) == -1);
    assert(rich4_music_point_minigame_index(9) == -1);
    post_load_music_restarts_playlist();
    normal_finance_music_cannot_be_overridden();
    puts("point minigame music mapping: ok");
    return 0;
}
