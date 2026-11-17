/* R10.1d: per-session modal music policy moved verbatim out of
 * runtime_main.c. State-machine transitions remain in the caller; this
 * module keeps the original music start/restore ordering and labels. */

#include "data/music.h"
#include "app/point_minigame_flow.h"
#include "app/shop_flow.h"
#include "app/monthly_finance_flow.h"
#include "core/auction.h"
#include "core/stock.h"
#include "platform/sdl1/game_loop_modal_music.h"
#include "platform/sdl1/runtime_bootstrap.h"

static int game_session_has_bankruptcy_music(const GameSession *session)
{
    if (session == NULL) return 0;
    return session->presentation.bankruptcy.active ||
        session->presentation.bankruptcy.awaiting_auction ||
        (session->presentation.auction.active &&
         session->presentation.auction.request.trigger ==
            GAME_AUCTION_TRIGGER_BANKRUPTCY);
}

int game_session_has_modal_music(const GameSession *session)
{
    int bank_active;
    if (session == NULL) return 0;
    bank_active = (session->presentation.bank_loan_due.active &&
                   session->presentation.bank_loan_due.days == 3) ||
        (session->presentation.route_interaction.active &&
        session->presentation.route_interaction.accept.type ==
            GAME_COMMAND_BANK_APPLY_LOAN);
    return game_session_has_bankruptcy_music(session) ||
        session->presentation.bank_settlement.active ||
        (session->presentation.periodic_finance.active &&
         session->presentation.periodic_finance.phase > 0) ||
        session->presentation.confinement_visit.active || bank_active ||
        (session->presentation.point_minigame.active &&
         !session->presentation.point_minigame.automatic) ||
        (session->presentation.shop_visit.active &&
         session->presentation.shop_visit.phase != RICH4_SHOP_OWNER_GIFT &&
         session->presentation.shop_visit.phase != RICH4_SHOP_OWNER_SPEECH) ||
        session->presentation.lottery.active ||
        (session->presentation.magic_house.active &&
         !session->presentation.magic_house.modal_music_finished) ||
        (session->presentation.auction.active &&
         session->presentation.auction.request.manage_music &&
         session->presentation.auction.request.trigger !=
            GAME_AUCTION_TRIGGER_BANKRUPTCY);
}

static int game_start_temporary_music(
    GameModalMusicContext *context, size_t music_index, const char *label)
{
    int interrupted_map;
    if (context == NULL) return 0;
    interrupted_map = *context->main_map_music_active != 0;
    if (interrupted_map)
        (void)rich4_audio_save_music_position(context->audio);
    if (!start_fixed_music(
            context->audio, context->music_dir, music_index, label)) {
        if (interrupted_map)
            rich4_audio_clear_saved_music_position(context->audio);
        return 0;
    }
    *context->main_map_music_active = 0;
    return 1;
}

static void game_restore_main_map_music(GameModalMusicContext *context)
{
    if (context == NULL || game_session_has_modal_music(context->session))
        return;
    if (*context->main_map_music_active)
        return;
    if (context->music_enabled) {
        *context->main_map_music_active = start_music_bank_track(
            context->audio, context->music_dir, context->music_bank,
            (size_t)*context->main_map_music_track, "main map");
        if (*context->main_map_music_active)
            (void)rich4_audio_restore_music_position(context->audio);
        else
            rich4_audio_clear_saved_music_position(context->audio);
    } else {
        *context->main_map_music_active = 0;
        rich4_audio_stop_music(context->audio);
    }
}

void game_capture_modal_music_context(
    GameModalMusicContext *context, GameSession *session,
    Rich4Audio *audio, const char *music_dir, Rich4MusicBank music_bank,
    int music_enabled, int *main_map_music_track,
    int *main_map_music_active, int *bankruptcy_music_kind,
    int *confinement_music_kind,
    int *bank_music_active, int *lottery_music_active,
    int *magic_house_music_active,
    int *auction_music_active, int *holiday_music_active)
{
    if (context == NULL || session == NULL)
        return;
    context->session = session;
    context->audio = audio;
    context->music_dir = music_dir;
    context->music_bank = music_bank;
    context->music_enabled = music_enabled;
    context->main_map_music_track = main_map_music_track;
    context->main_map_music_active = main_map_music_active;
    context->bankruptcy_music_kind = bankruptcy_music_kind;
    context->confinement_music_kind = confinement_music_kind;
    context->bank_music_active = bank_music_active;
    context->lottery_music_active = lottery_music_active;
    context->magic_house_music_active = magic_house_music_active;
    context->auction_music_active = auction_music_active;
    context->holiday_music_active = holiday_music_active;
    context->shop_was_active = session->presentation.shop_visit.active;
    context->shop_music_was_started =
        session->presentation.shop_visit.music_started;
    context->point_minigame_was_active = session->presentation.point_minigame.active;
    context->point_minigame_music_was_started =
        session->presentation.point_minigame.music_started;
    context->lottery_was_active = session->presentation.lottery.active;
    context->magic_house_was_active = session->presentation.magic_house.active;
    context->finance_was_active = session->presentation.periodic_finance.active &&
        session->presentation.periodic_finance.phase > 0;
    context->finance_music_was_started = session->presentation.periodic_finance.music_started;
    context->settlement_was_active = session->presentation.bank_settlement.active;
}

void game_restore_shop_music_if_pending(
    const GameSession *session, Rich4Audio *audio,
    const char *music_dir, Rich4MusicBank music_bank,
    int music_enabled, int *main_map_music_track,
    int *main_map_music_active, int *shop_music_restore_pending)
{
    if (shop_music_restore_pending == NULL ||
        !*shop_music_restore_pending)
        return;
    /* A preceding modal can close in the frame that a settlement starts.
     * Its delayed restore must not replace the new modal's fixed MIDI. */
    if (!game_session_has_modal_music(session)) {
        if (music_enabled && !*main_map_music_active) {
            *main_map_music_active = start_music_bank_track(
                audio, music_dir, music_bank,
                (size_t)*main_map_music_track, "main map");
            if (*main_map_music_active)
                (void)rich4_audio_restore_music_position(audio);
            else
                rich4_audio_clear_saved_music_position(audio);
        } else if (!music_enabled) {
            *main_map_music_active = 0;
            rich4_audio_stop_music(audio);
        }
    }
    *shop_music_restore_pending = 0;
}

void game_roll_main_map_music_if_finished(
    Rich4Audio *audio, const char *music_dir, Rich4MusicBank music_bank,
    int music_enabled, int *main_map_music_track,
    int *main_map_music_active)
{
    if (!music_enabled || main_map_music_track == NULL ||
        main_map_music_active == NULL || !*main_map_music_active ||
        rich4_audio_music_playing(audio))
        return;
    *main_map_music_track =
        (*main_map_music_track + 1) % (int)rich4_music_bank_track_count();
    *main_map_music_active = start_music_bank_track(
        audio, music_dir, music_bank,
        (size_t)*main_map_music_track, "main map");
}

void game_update_modal_music(GameModalMusicContext *context)
{
    GameSession *session = context->session;
    Rich4Audio *audio = context->audio;
    int bankruptcy_active = game_session_has_bankruptcy_music(session);
    int bankruptcy_auction_active =
        session->presentation.auction.active &&
        session->presentation.auction.request.trigger ==
            GAME_AUCTION_TRIGGER_BANKRUPTCY;
    int bank_active = (session->presentation.bank_loan_due.active &&
                       session->presentation.bank_loan_due.days == 3) ||
        (session->presentation.route_interaction.active &&
        session->presentation.route_interaction.accept.type ==
            GAME_COMMAND_BANK_APPLY_LOAN);
    if (context->music_enabled && bankruptcy_active &&
        *context->bankruptcy_music_kind == GAME_BANKRUPTCY_MUSIC_NONE) {
        size_t music_index = bankruptcy_auction_active ? 5u : 2u;
        /* 0x40CF88 starts MIDI03 for 0x40CD87.  If a decoded session is
         * already inside the bankruptcy auction, resume its 0x40D1CD
         * MIDI06 phase instead. */
        if (game_start_temporary_music(
                context, music_index,
                bankruptcy_auction_active ? "bankruptcy auction"
                                           : "bankruptcy"))
            *context->bankruptcy_music_kind =
                bankruptcy_auction_active ? GAME_BANKRUPTCY_MUSIC_AUCTION
                                          : GAME_BANKRUPTCY_MUSIC_PRESENTATION;
    } else if (context->music_enabled && bankruptcy_auction_active &&
               *context->bankruptcy_music_kind ==
                    GAME_BANKRUPTCY_MUSIC_PRESENTATION) {
        /* 0x40D1CD switches once before the bankruptcy auction group; it
         * does not create a second saved map-playlist frame. */
        if (game_start_temporary_music(
                context, 5, "bankruptcy auction"))
            *context->bankruptcy_music_kind = GAME_BANKRUPTCY_MUSIC_AUCTION;
    }
    if (!bankruptcy_active && *context->bankruptcy_music_kind !=
            GAME_BANKRUPTCY_MUSIC_NONE) {
        *context->bankruptcy_music_kind = GAME_BANKRUPTCY_MUSIC_NONE;
        if (session->lifecycle.code != RICH4_APP_LIFECYCLE_NONE ||
            session->presentation.ending.active) {
            /* Terminal 0x40CD87 branches replace this music at the outer
             * lifecycle boundary; only the continuing-game path reaches
             * 0x40D27D/sub_454BCC. */
            *context->main_map_music_active = 0;
            rich4_audio_clear_saved_music_position(audio);
        } else {
            game_restore_main_map_music(context);
        }
    }
    if (context->music_enabled && session->presentation.bank_settlement.active &&
        *context->bank_music_active != 2) {
        if (game_start_temporary_music(
                context, 9, "monthly settlement")) {
            *context->bank_music_active = 2;
        }
    }
    if (context->music_enabled && session->presentation.periodic_finance.active &&
        session->presentation.periodic_finance.phase > 0 &&
        !session->presentation.periodic_finance.music_started) {
        if (game_start_temporary_music(
                context, 8, "lottery drawing")) {
            rich4_periodic_finance_mark_music_started(session);
        }
    }
    if ((context->settlement_was_active && !session->presentation.bank_settlement.active) ||
        (context->finance_was_active && context->finance_music_was_started &&
         !session->presentation.periodic_finance.active)) {
        if (context->settlement_was_active) *context->bank_music_active = 0;
        game_restore_main_map_music(context);
    }
    if (context->music_enabled &&
        session->presentation.confinement_visit.active &&
        *context->confinement_music_kind !=
            session->presentation.confinement_visit.kind) {
        int kind = session->presentation.confinement_visit.kind;
        size_t music_index = kind == GAME_CONFINEMENT_PRISON ? 15u : 16u;
        /* sub_43D304/sub_43E9A4 call sub_4549CF(15/16):
         * MIDI15.MID for prison and MIDI16.MID for hospital. */
        if (game_start_temporary_music(
                context, music_index,
                kind == GAME_CONFINEMENT_PRISON
                    ? "prison" : "hospital")) {
            *context->confinement_music_kind = kind;
        }
    }
    if (!session->presentation.confinement_visit.active &&
        *context->confinement_music_kind != GAME_CONFINEMENT_NONE) {
        /* Both modal entry functions restore the map playlist with
         * sub_454BCC after their callbacks return. */
        *context->confinement_music_kind = GAME_CONFINEMENT_NONE;
        game_restore_main_map_music(context);
    }
    if (context->music_enabled && bank_active &&
        !*context->bank_music_active) {
        /* sub_436668 calls sub_4549CF(4) immediately before Panel23's
         * modal callback: zero-based MIDI05.MID. */
        if (game_start_temporary_music(
                context, 4, "bank")) {
            *context->bank_music_active = 1;
        }
    }
    if (context->music_enabled &&
        session->presentation.point_minigame.active &&
        !session->presentation.point_minigame.automatic &&
        !session->presentation.point_minigame.music_started) {
        int music_index = rich4_music_point_minigame_index(
            session->presentation.point_minigame.action);
        if (music_index >= 0 && game_start_temporary_music(
                context, (size_t)music_index,
                session->presentation.point_minigame.action == 6
                    ? "penguin point game"
                    : session->presentation.point_minigame.action == 7
                    ? "balloon point game"
                    : "catcher point game")) {
            rich4_point_minigame_mark_music_started(session);
        }
    }
    if (context->music_enabled &&
        session->presentation.shop_visit.active &&
        !session->presentation.shop_visit.music_started &&
        session->presentation.shop_visit.phase != RICH4_SHOP_OWNER_GIFT &&
        session->presentation.shop_visit.phase != RICH4_SHOP_OWNER_SPEECH) {
        /* 0x42ECF4 calls sub_4549CF(6): MIDI07.MID. */
        if (game_start_temporary_music(
                context, 6, "department store")) {
            rich4_shop_mark_music_started(session);
        }
    }
    if (context->music_enabled &&
        session->presentation.lottery.active &&
        !*context->lottery_music_active) {
        /* 0x431644 calls sub_4549CF(6): MIDI07.MID before opening
         * the lottery callback. */
        if (game_start_temporary_music(
                context, 6, "lottery")) {
            *context->lottery_music_active = 1;
        }
    }
    if (context->music_enabled &&
        session->presentation.magic_house.active &&
        !session->presentation.magic_house.modal_music_finished &&
        !*context->magic_house_music_active) {
        /* 0x4338A0 calls sub_4549CF(7): MIDI08.MID. */
        if (game_start_temporary_music(
                context, 7, "magic house")) {
            *context->magic_house_music_active = 1;
        }
    }
    if (context->music_enabled &&
        session->presentation.auction.active &&
        session->presentation.auction.request.manage_music &&
        session->presentation.auction.request.trigger !=
            GAME_AUCTION_TRIGGER_BANKRUPTCY &&
        !*context->auction_music_active) {
        /* sub_43BDE5 calls sub_4549CF(5): MIDI06.MID / track15. */
        if (game_start_temporary_music(
                context, 5, "auction")) {
            *context->auction_music_active = 1;
        }
    }
    if (context->shop_was_active &&
        !session->presentation.shop_visit.active &&
        context->shop_music_was_started) {
        /* 0x42ED0F restores the playlist after the shop callback. */
        game_restore_main_map_music(context);
    }
    if (context->point_minigame_was_active &&
        !session->presentation.point_minigame.active &&
        context->point_minigame_music_was_started) {
        /* Each point-game callback restores the normal playlist on
         * return, just like the other modal activities. */
        game_restore_main_map_music(context);
    }
    if (!bank_active && *context->bank_music_active == 1) {
        /* sub_436668 restores the saved map playlist after the bank modal
         * callback returns. Input can close the panel before this frame's
         * context is captured; the playing bank track owns the restore.
         * Value 2 belongs to monthly settlement, which has its own exit. */
        *context->bank_music_active = 0;
        game_restore_main_map_music(context);
    }
    if (context->lottery_was_active &&
        !session->presentation.lottery.active &&
        *context->lottery_music_active) {
        /* 0x43165B calls sub_454BCC immediately after the modal
         * lottery callback returns. */
        *context->lottery_music_active = 0;
        game_restore_main_map_music(context);
    }
    if (*context->magic_house_music_active &&
        ((context->magic_house_was_active &&
          !session->presentation.magic_house.active) ||
         (session->presentation.magic_house.active &&
          session->presentation.magic_house.modal_music_finished))) {
        /* 0x4338B9 calls sub_454BCC immediately after Panel[20]'s
         * modal returns, before sub_431CAA visits target players. */
        *context->magic_house_music_active = 0;
        game_restore_main_map_music(context);
    }
    if (*context->auction_music_active &&
        (!session->presentation.auction.active ||
         !session->presentation.auction.request.manage_music)) {
        *context->auction_music_active = 0;
        game_restore_main_map_music(context);
    }
    {
        int holiday_index = rich4_stock_calendar_music_index(&session->state);
        int modal_music_active = game_session_has_modal_music(session);
        if (modal_music_active) {
            /* A modal replaced the holiday track.  Its close branch restores
             * the map playlist; this tail then restarts a current holiday. */
            *context->holiday_music_active = 0;
        } else if (context->music_enabled && holiday_index >= 0) {
            int desired = holiday_index + 1;
            if (*context->holiday_music_active != desired &&
                start_fixed_music(audio, context->music_dir,
                                  (size_t)holiday_index,
                                  "calendar holiday")) {
                *context->holiday_music_active = desired;
                *context->main_map_music_active = 0;
            }
        } else if (*context->holiday_music_active != 0) {
            *context->holiday_music_active = 0;
            if (context->music_enabled)
                *context->main_map_music_active = start_music_bank_track(
                    audio, context->music_dir, context->music_bank,
                    (size_t)*context->main_map_music_track, "main map");
            else {
                *context->main_map_music_active = 0;
                rich4_audio_stop_music(audio);
            }
        }
    }
}
