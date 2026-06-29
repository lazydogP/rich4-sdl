/* R10.1d: per-session modal music policy moved verbatim out of
 * runtime_main.c. State-machine transitions remain in the caller; this
 * module keeps the original music start/restore ordering and labels. */

#include "data/music.h"
#include "app/point_minigame_flow.h"
#include "app/shop_flow.h"
#include "app/monthly_finance_flow.h"
#include "platform/sdl1/game_loop_modal_music.h"
#include "platform/sdl1/runtime_bootstrap.h"

void game_capture_modal_music_context(
    GameModalMusicContext *context, GameSession *session,
    Rich4Audio *audio, const char *music_dir, Rich4MusicBank music_bank,
    int music_enabled, int *main_map_music_track,
    int *main_map_music_active, int *confinement_music_kind,
    int *bank_music_active, int *lottery_music_active,
    int *magic_house_music_active,
    int *auction_music_active)
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
    context->confinement_music_kind = confinement_music_kind;
    context->bank_music_active = bank_music_active;
    context->lottery_music_active = lottery_music_active;
    context->magic_house_music_active = magic_house_music_active;
    context->auction_music_active = auction_music_active;
    context->shop_was_active = session->presentation.shop_visit.active;
    context->shop_music_was_started =
        session->presentation.shop_visit.music_started;
    context->point_minigame_was_active = session->presentation.point_minigame.active;
    context->point_minigame_music_was_started =
        session->presentation.point_minigame.music_started;
    context->bank_was_active =
        session->presentation.route_interaction.active &&
        session->presentation.route_interaction.accept.type ==
            GAME_COMMAND_BANK_APPLY_LOAN;
    context->lottery_was_active = session->presentation.lottery.active;
    context->magic_house_was_active = session->presentation.magic_house.active;
    context->finance_was_active = session->presentation.periodic_finance.active &&
        session->presentation.periodic_finance.phase > 0;
    context->finance_music_was_started = session->presentation.periodic_finance.music_started;
    context->settlement_was_active = session->presentation.bank_settlement.active;
}

void game_restore_shop_music_if_pending(
    Rich4Audio *audio, const char *music_dir, Rich4MusicBank music_bank,
    int music_enabled, int *main_map_music_track,
    int *main_map_music_active, int *shop_music_restore_pending)
{
    if (shop_music_restore_pending == NULL ||
        !*shop_music_restore_pending)
        return;
    if (music_enabled)
        *main_map_music_active = start_music_bank_track(
            audio, music_dir, music_bank,
            (size_t)*main_map_music_track, "main map");
    else {
        *main_map_music_active = 0;
        rich4_audio_stop_music(audio);
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
    int bank_active = session->presentation.route_interaction.active &&
        session->presentation.route_interaction.accept.type ==
            GAME_COMMAND_BANK_APPLY_LOAN;
    if (context->music_enabled && session->presentation.bank_settlement.active &&
        *context->bank_music_active != 2) {
        if (start_fixed_music(audio, context->music_dir, 9, "monthly settlement")) {
            *context->bank_music_active = 2;
            *context->main_map_music_active = 0;
        }
    }
    if (context->music_enabled && session->presentation.periodic_finance.active &&
        session->presentation.periodic_finance.phase > 0 &&
        !session->presentation.periodic_finance.music_started) {
        if (start_fixed_music(audio, context->music_dir, 8, "lottery drawing")) {
            rich4_periodic_finance_mark_music_started(session);
            *context->main_map_music_active = 0;
        }
    }
    if ((context->settlement_was_active && !session->presentation.bank_settlement.active) ||
        (context->finance_was_active && context->finance_music_was_started &&
         !session->presentation.periodic_finance.active)) {
        if (context->settlement_was_active) *context->bank_music_active = 0;
        if (context->music_enabled)
            *context->main_map_music_active = start_music_bank_track(audio,
                context->music_dir, context->music_bank,
                (size_t)*context->main_map_music_track, "main map");
        else rich4_audio_stop_music(audio);
    }
    if (context->music_enabled &&
        session->presentation.confinement_visit.active &&
        *context->confinement_music_kind !=
            session->presentation.confinement_visit.kind) {
        int kind = session->presentation.confinement_visit.kind;
        size_t music_index = kind == GAME_CONFINEMENT_PRISON ? 15u : 16u;
        /* sub_43D304/sub_43E9A4 call sub_4549CF(15/16):
         * MIDI15.MID for prison and MIDI16.MID for hospital. */
        if (start_fixed_music(
                audio, context->music_dir, music_index,
                kind == GAME_CONFINEMENT_PRISON
                    ? "prison" : "hospital")) {
            *context->confinement_music_kind = kind;
            *context->main_map_music_active = 0;
        }
    }
    if (!session->presentation.confinement_visit.active &&
        *context->confinement_music_kind != GAME_CONFINEMENT_NONE) {
        /* Both modal entry functions restore the map playlist with
         * sub_454BCC after their callbacks return. */
        *context->confinement_music_kind = GAME_CONFINEMENT_NONE;
        if (context->music_enabled)
            *context->main_map_music_active =
                start_music_bank_track(
                    audio,
                    context->music_dir,
                    context->music_bank,
                    (size_t)*context->main_map_music_track,
                    "main map");
        else {
            *context->main_map_music_active = 0;
            rich4_audio_stop_music(audio);
        }
    }
    if (context->music_enabled && bank_active &&
        !*context->bank_music_active) {
        /* sub_436668 calls sub_4549CF(4) immediately before Panel23's
         * modal callback: zero-based MIDI05.MID. */
        if (start_fixed_music(
                audio, context->music_dir, 4, "bank")) {
            *context->bank_music_active = 1;
            *context->main_map_music_active = 0;
        }
    }
    if (context->music_enabled &&
        session->presentation.point_minigame.active &&
        !session->presentation.point_minigame.automatic &&
        !session->presentation.point_minigame.music_started) {
        int music_index = rich4_music_point_minigame_index(
            session->presentation.point_minigame.action);
        if (music_index >= 0 && start_fixed_music(
                audio, context->music_dir, (size_t)music_index,
                session->presentation.point_minigame.action == 6
                    ? "penguin point game"
                    : session->presentation.point_minigame.action == 7
                    ? "balloon point game"
                    : "catcher point game")) {
            rich4_point_minigame_mark_music_started(session);
            *context->main_map_music_active = 0;
        }
    }
    if (context->music_enabled &&
        session->presentation.shop_visit.active &&
        !session->presentation.shop_visit.music_started &&
        session->presentation.shop_visit.phase != RICH4_SHOP_OWNER_GIFT &&
        session->presentation.shop_visit.phase != RICH4_SHOP_OWNER_SPEECH) {
        /* 0x42ECF4 calls sub_4549CF(6): MIDI07.MID. */
        if (start_fixed_music(
                audio, context->music_dir, 6, "department store")) {
            rich4_shop_mark_music_started(session);
            *context->main_map_music_active = 0;
        }
    }
    if (context->music_enabled &&
        session->presentation.lottery.active &&
        !*context->lottery_music_active) {
        /* 0x431644 calls sub_4549CF(6): MIDI07.MID before opening
         * the lottery callback. */
        if (start_fixed_music(
                audio, context->music_dir, 6, "lottery")) {
            *context->lottery_music_active = 1;
            *context->main_map_music_active = 0;
        }
    }
    if (context->music_enabled &&
        session->presentation.magic_house.active &&
        !session->presentation.magic_house.modal_music_finished &&
        !*context->magic_house_music_active) {
        /* 0x4338A0 calls sub_4549CF(7): MIDI08.MID. */
        if (start_fixed_music(
                audio, context->music_dir, 7, "magic house")) {
            *context->magic_house_music_active = 1;
            *context->main_map_music_active = 0;
        }
    }
    if (context->music_enabled &&
        session->presentation.auction.active &&
        session->presentation.auction.request.manage_music &&
        !*context->auction_music_active) {
        /* sub_43BDE5 calls sub_4549CF(5): MIDI06.MID / track15. */
        if (start_fixed_music(
                audio, context->music_dir, 5, "auction")) {
            *context->auction_music_active = 1;
            *context->main_map_music_active = 0;
        }
    }
    if (context->shop_was_active &&
        !session->presentation.shop_visit.active &&
        context->shop_music_was_started) {
        /* 0x42ED0F restores the playlist after the shop callback. */
        if (context->music_enabled)
            *context->main_map_music_active =
                start_music_bank_track(
                    audio,
                    context->music_dir,
                    context->music_bank,
                    (size_t)*context->main_map_music_track,
                    "main map");
        else {
            *context->main_map_music_active = 0;
            rich4_audio_stop_music(audio);
        }
    }
    if (context->point_minigame_was_active &&
        !session->presentation.point_minigame.active &&
        context->point_minigame_music_was_started) {
        /* Each point-game callback restores the normal playlist on
         * return, just like the other modal activities. */
        if (context->music_enabled)
            *context->main_map_music_active =
                start_music_bank_track(
                    audio,
                    context->music_dir,
                    context->music_bank,
                    (size_t)*context->main_map_music_track,
                    "main map");
        else {
            *context->main_map_music_active = 0;
            rich4_audio_stop_music(audio);
        }
    }
    if (context->bank_was_active && !bank_active &&
        *context->bank_music_active) {
        /* sub_436668 restores the saved map playlist after the bank modal
         * callback returns. */
        *context->bank_music_active = 0;
        if (context->music_enabled)
            *context->main_map_music_active =
                start_music_bank_track(
                    audio, context->music_dir, context->music_bank,
                    (size_t)*context->main_map_music_track, "main map");
        else {
            *context->main_map_music_active = 0;
            rich4_audio_stop_music(audio);
        }
    }
    if (context->lottery_was_active &&
        !session->presentation.lottery.active &&
        *context->lottery_music_active) {
        /* 0x43165B calls sub_454BCC immediately after the modal
         * lottery callback returns. */
        *context->lottery_music_active = 0;
        if (context->music_enabled)
            *context->main_map_music_active =
                start_music_bank_track(
                    audio,
                    context->music_dir,
                    context->music_bank,
                    (size_t)*context->main_map_music_track,
                    "main map");
        else {
            *context->main_map_music_active = 0;
            rich4_audio_stop_music(audio);
        }
    }
    if (*context->magic_house_music_active &&
        ((context->magic_house_was_active &&
          !session->presentation.magic_house.active) ||
         (session->presentation.magic_house.active &&
          session->presentation.magic_house.modal_music_finished))) {
        /* 0x4338B9 calls sub_454BCC immediately after Panel[20]'s
         * modal returns, before sub_431CAA visits target players. */
        *context->magic_house_music_active = 0;
        if (context->music_enabled)
            *context->main_map_music_active =
                start_music_bank_track(
                    audio,
                    context->music_dir,
                    context->music_bank,
                    (size_t)*context->main_map_music_track,
                    "main map");
        else {
            *context->main_map_music_active = 0;
            rich4_audio_stop_music(audio);
        }
    }
    if (*context->auction_music_active &&
        (!session->presentation.auction.active ||
         !session->presentation.auction.request.manage_music)) {
        *context->auction_music_active = 0;
        if (context->music_enabled)
            *context->main_map_music_active =
                start_music_bank_track(
                    audio, context->music_dir, context->music_bank,
                    (size_t)*context->main_map_music_track, "main map");
        else {
            *context->main_map_music_active = 0;
            rich4_audio_stop_music(audio);
        }
    }
}
