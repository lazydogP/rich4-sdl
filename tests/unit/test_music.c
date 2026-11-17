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
static int save_position_count;
static int restore_position_count;
static int clear_position_count;

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

int rich4_audio_save_music_position(Rich4Audio *audio)
{
    (void)audio;
    save_position_count++;
    return 1;
}

int rich4_audio_restore_music_position(Rich4Audio *audio)
{
    (void)audio;
    restore_position_count++;
    return 1;
}

void rich4_audio_clear_saved_music_position(Rich4Audio *audio)
{
    (void)audio;
    clear_position_count++;
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
    int bankruptcy = GAME_BANKRUPTCY_MUSIC_AUCTION;
    int confinement = 2;
    int bank = 2;
    int lottery = 1;
    int magic = 1;
    int auction = 1;
    int holiday = 4;
    int shop_restore = 1;

    assert(!game_restart_music_after_load(
        &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery, &magic,
        &auction, &holiday, &shop_restore));
    session.loaded_music_restart_pending = 1;
    assert(game_restart_music_after_load(
        &session, NULL, "music", RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery, &magic,
        &auction, &holiday, &shop_restore));
    assert(!session.loaded_music_restart_pending);
    assert(track == 0 && main_active == 1);
    assert(started_track == 0 &&
           started_bank == RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY);
    assert(confinement == GAME_CONFINEMENT_NONE);
    assert(bankruptcy == GAME_BANKRUPTCY_MUSIC_NONE && bank == 0 &&
           lottery == 0 && magic == 0 && auction == 0 &&
           holiday == 0 && shop_restore == 0);

    session.loaded_music_restart_pending = 1;
    assert(game_restart_music_after_load(
        &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 0,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery, &magic,
        &auction, &holiday, &shop_restore));
    assert(track == 1 && main_active == 0 && stop_count == 1);

    started_track = -1;
    session.presentation.periodic_finance.active = 1;
    session.presentation.periodic_finance.phase = 4;
    session.presentation.periodic_finance.music_started = 1;
    session.loaded_music_restart_pending = 1;
    assert(game_restart_music_after_load(
        &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery, &magic,
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
    int bankruptcy = 0;
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
    save_position_count = restore_position_count = 0;
    fixed_track = -1;
    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery, &magic,
        &auction, &holiday);
    game_update_modal_music(&context);

    /* A stale closing activity used to restore the map after MIDI09 had
     * already started in the same ordinary (non-load) frame. */
    assert(fixed_start_count == 1 && fixed_track == 8);
    assert(map_start_count == 0 && main_active == 0);
    assert(save_position_count == 1 && restore_position_count == 0);
    assert(session.presentation.periodic_finance.music_started == 1);
    assert(auction == 0);

    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery, &magic,
        &auction, &holiday);
    session.presentation.periodic_finance.active = 0;
    game_update_modal_music(&context);
    assert(map_start_count == 1 && main_active == 1);
    assert(restore_position_count == 1);
}

static void bank_exit_restores_map_music(int reminder)
{
    int close_before_capture, enabled;
    for (close_before_capture = 0; close_before_capture <= 1; close_before_capture++) {
        for (enabled = 0; enabled <= 1; enabled++) {
            GameSession session = {0};
            GameModalMusicContext context;
            int track = 3, main_active = 1;
            int bankruptcy = 0;
            int confinement = GAME_CONFINEMENT_NONE;
            int bank = 0, lottery = 0, magic = 0, auction = 0, holiday = 0;
            session.presentation.route_interaction.active = !reminder;
            session.presentation.route_interaction.accept.type = GAME_COMMAND_BANK_APPLY_LOAN;
            session.presentation.bank_loan_due.active = reminder;
            session.presentation.bank_loan_due.days = 3;
            map_start_count = fixed_start_count = stop_count = 0;
            save_position_count = restore_position_count = 0;
            game_capture_modal_music_context(
                &context, &session, NULL, "music", RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY, 1,
                &track, &main_active, &bankruptcy, &confinement, &bank, &lottery, &magic,
                &auction, &holiday);
            game_update_modal_music(&context);
            assert(bank == 1 && main_active == 0);
            assert(fixed_start_count == 1 && fixed_track == 4);
            assert(save_position_count == 1 && restore_position_count == 0);
            game_update_modal_music(&context);
            assert(fixed_start_count == 1 && map_start_count == 0);

            /* Mouse release closes the panel before the preview update;
             * simulation can also close it after the context is captured. */
            if (close_before_capture) {
                session.presentation.route_interaction.active = 0;
                session.presentation.bank_loan_due.active = 0;
            }
            game_capture_modal_music_context(
                &context, &session, NULL, "music", RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY, enabled,
                &track, &main_active, &bankruptcy, &confinement, &bank, &lottery, &magic,
                &auction, &holiday);
            session.presentation.route_interaction.active = 0;
            session.presentation.bank_loan_due.active = 0;
            game_update_modal_music(&context);
            assert(bank == 0 && track == 3 && main_active == enabled);
            assert(map_start_count == enabled && stop_count == !enabled);
            assert(restore_position_count == enabled);
            if (enabled)
                assert(started_track == 3 && started_bank == RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY);
            game_update_modal_music(&context);
            assert(map_start_count == enabled && stop_count == !enabled);
        }
    }
}

static void bank_exit_preserves_next_modal_music(void)
{
    GameSession session = {0};
    GameModalMusicContext context;
    int track = 3, main_active = 0;
    int bankruptcy = 0;
    int confinement = GAME_CONFINEMENT_NONE;
    int bank = 1, lottery = 0, magic = 0, auction = 0, holiday = 0;
    /* The bank closed during input; a lottery window now owns the music. */
    session.presentation.lottery.active = 1;
    map_start_count = fixed_start_count = 0;
    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery, &magic,
        &auction, &holiday);
    game_update_modal_music(&context);
    assert(bank == 0 && lottery == 1 && main_active == 0);
    assert(map_start_count == 0 && fixed_start_count == 1 && fixed_track == 6);

    session.presentation.lottery.active = 0;
    session.presentation.bank_settlement.active = 1;
    bank = 1;
    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery, &magic,
        &auction, &holiday);
    game_update_modal_music(&context);
    assert(bank == 2 && map_start_count == 0 && main_active == 0);
    assert(fixed_track == 9);
    game_update_modal_music(&context);
    assert(bank == 2 && map_start_count == 0);
}

static void bankruptcy_music_keeps_one_map_bookmark(void)
{
    GameSession session = {0};
    GameModalMusicContext context;
    int track = 4, main_active = 1;
    int bankruptcy = 0;
    int confinement = GAME_CONFINEMENT_NONE;
    int bank = 0, lottery = 0, magic = 0, auction = 0, holiday = 0;

    session.presentation.bankruptcy.active = 1;
    map_start_count = fixed_start_count = 0;
    save_position_count = restore_position_count = clear_position_count = 0;
    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery,
        &magic, &auction, &holiday);
    game_update_modal_music(&context);
    assert(bankruptcy == GAME_BANKRUPTCY_MUSIC_PRESENTATION &&
           fixed_track == 2 && main_active == 0);
    assert(fixed_start_count == 1 && save_position_count == 1);

    session.presentation.bankruptcy.active = 0;
    session.presentation.bankruptcy.awaiting_auction = 1;
    session.presentation.auction.active = 1;
    session.presentation.auction.request.trigger =
        GAME_AUCTION_TRIGGER_BANKRUPTCY;
    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery,
        &magic, &auction, &holiday);
    game_update_modal_music(&context);
    assert(bankruptcy == GAME_BANKRUPTCY_MUSIC_AUCTION && fixed_track == 5);
    assert(fixed_start_count == 2 && save_position_count == 1);

    /* Closing one of the bankruptcy auctions does not pop the map bookmark;
     * the outer 0x40CD87 group still owns it. */
    session.presentation.auction.active = 0;
    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery,
        &magic, &auction, &holiday);
    game_update_modal_music(&context);
    assert(map_start_count == 0 && restore_position_count == 0);

    session.presentation.bankruptcy.awaiting_auction = 0;
    session.presentation.bankruptcy.active = 1;
    session.presentation.bankruptcy.phase = RICH4_BANKRUPTCY_PHASE_SPEECH;
    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery,
        &magic, &auction, &holiday);
    game_update_modal_music(&context);
    assert(fixed_start_count == 2 &&
           bankruptcy == GAME_BANKRUPTCY_MUSIC_AUCTION);

    session.presentation.bankruptcy.active = 0;
    game_update_modal_music(&context);
    assert(bankruptcy == GAME_BANKRUPTCY_MUSIC_NONE &&
           map_start_count == 1 && main_active == 1);
    assert(restore_position_count == 1 && clear_position_count == 0);
}

static void bankruptcy_terminal_does_not_restore_map_music(void)
{
    GameSession session = {0};
    GameModalMusicContext context;
    int track = 1, main_active = 1;
    int bankruptcy = 0;
    int confinement = GAME_CONFINEMENT_NONE;
    int bank = 0, lottery = 0, magic = 0, auction = 0, holiday = 0;

    session.presentation.bankruptcy.active = 1;
    map_start_count = fixed_start_count = 0;
    save_position_count = restore_position_count = clear_position_count = 0;
    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery,
        &magic, &auction, &holiday);
    game_update_modal_music(&context);
    session.presentation.bankruptcy.active = 0;
    session.lifecycle.code = RICH4_APP_LIFECYCLE_OUTER_MENU;
    game_update_modal_music(&context);
    assert(bankruptcy == GAME_BANKRUPTCY_MUSIC_NONE && main_active == 0);
    assert(map_start_count == 0 && restore_position_count == 0);
    assert(clear_position_count == 1);
}

static void surrender_selector_keeps_bankruptcy_bookmark(void)
{
    GameSession session = {0};
    GameModalMusicContext context;
    int track = 6, main_active = 1;
    int bankruptcy = 0;
    int confinement = GAME_CONFINEMENT_NONE;
    int bank = 0, lottery = 0, magic = 0, auction = 0, holiday = 0;

    session.presentation.bankruptcy.active = 1;
    map_start_count = fixed_start_count = 0;
    save_position_count = restore_position_count = 0;
    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery,
        &magic, &auction, &holiday);
    game_update_modal_music(&context);

    session.presentation.bankruptcy.active = 0;
    session.presentation.magic_house.active = 1;
    session.presentation.magic_house.condition = 12;
    game_capture_modal_music_context(
        &context, &session, NULL, "music", RICH4_MUSIC_BANK_ORIGINAL, 1,
        &track, &main_active, &bankruptcy, &confinement, &bank, &lottery,
        &magic, &auction, &holiday);
    game_update_modal_music(&context);
    assert(bankruptcy == GAME_BANKRUPTCY_MUSIC_NONE &&
           magic == 1 && fixed_track == 7);
    assert(save_position_count == 1 && map_start_count == 0);

    session.presentation.magic_house.active = 0;
    game_update_modal_music(&context);
    assert(magic == 0 && map_start_count == 1 && restore_position_count == 1);
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
    bank_exit_restores_map_music(0);
    bank_exit_restores_map_music(1);
    bank_exit_preserves_next_modal_music();
    bankruptcy_music_keeps_one_map_bookmark();
    bankruptcy_terminal_does_not_restore_map_music();
    surrender_selector_keeps_bankruptcy_bookmark();
    puts("point minigame music mapping: ok");
    return 0;
}
