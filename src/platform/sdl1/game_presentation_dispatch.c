#include "platform/sdl1/game_lazy_assets.h"
#include "app/bank_flow.h"
#include "app/engineering_attack_flow.h"
#include "app/auction_flow.h"
#include "app/config_types.h"
#include "app/game_presentation.h"
#include "app/game_session.h"
#include "app/insurance_flow.h"
#include "app/special_presentation_flow.h"
#include "app/special_place_notice_flow.h"
#include "app/game_speech_flow.h"
#include "app/monster_card_flow.h"
#include "app/new_game_setup_flow.h"
#include "data/character_speech.h"
#include "core/game_event_payload.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/game_monthly_finance.h"

static int game_card_animation_frame_count(
    const GamePreviewAssets *assets, int resource)
{
    if (assets == NULL) return 0;
    if (resource == 529 && game_asset_ready(assets, &assets->ui.has_demolish_card_animation))
        return game_asset_graph_frames(assets, &assets->ui.demolish_card_data, assets->ui.demolish_card_size);
    if (resource == 557 && game_asset_ready(assets, &assets->ui.has_monster_card_animation))
        return game_asset_graph_frames(assets, &assets->ui.monster_card_data, assets->ui.monster_card_size);
    return 0;
}

static int game_active_human_count(const GameSession *session)
{
    int count = 0;
    int i;
    if (session == NULL) return 0;
    for (i = 0; i < session->state.player_count; i++) {
        const GamePlayerState *player = &session->state.players[i];
        if ((player->play_state & GAME_PLAYER_PLAY_STATE_MASK) != 0 &&
            !player->is_ai)
            count++;
    }
    return count;
}

static int game_ending_winner_is_human(const GameSession *session)
{
    int winner;
    if (session == NULL) return 0;
    winner = session->presentation.ending.winner_player;
    if (winner < 0 || winner >= session->state.player_count) return 0;
    /* 0x41D89E tests the winner's raw play-state low bit.  New sessions use
     * 1 for a human and 2 for an AI, so retain that distinction instead of
     * inferring it from the number of active actors. */
    return (session->state.players[winner].play_state & 1) != 0;
}

static void game_update_pending_ending_video(
    GameSession *session, Uint32 now)
{
    GameEndingPresentationState *ending;
    int human_count;
    Rich4AppLifecycleCode code;

    if (session == NULL) return;
    ending = &session->presentation.ending;
    if (!ending->active || !ending->ending_video_pending ||
        ending->continuation_active || ending->winner_voice_pending ||
        session->presentation.speech.active)
        return;

    /* This is the post-0x44EF41 dispatch point.  The old SDL code played the
     * AVI here immediately on GAME_ENDED; the original instead selects the
     * typed outer-loop result first and lets code 2/3 enter 0x4075C1. */
    ending->ending_video_pending = 0;
    human_count = session->state.human_player_count;
    if (human_count <= 0) human_count = game_active_human_count(session);
    if (human_count == 1 && !game_ending_winner_is_human(session)) {
        /* 0x41D89E calls 0x407842(1): Panel[112] countdown, then code 4 on
         * confirmation or code 1 on timeout. */
        ending->continuation_active = 1;
        ending->continuation_started = now;
        return;
    }
    if (human_count == 1 && game_ending_winner_is_human(session))
        code = RICH4_APP_LIFECYCLE_RESTART_GAME;
    else if (game_ending_winner_is_human(session))
        code = RICH4_APP_LIFECYCLE_OUTER_MENU_AFTER_JUMP;
    else
        code = RICH4_APP_LIFECYCLE_OUTER_MENU;
    rich4_app_request_lifecycle(session, code, ending->winner_player);
}

static void game_update_pending_ending_voice(GameSession *session, Uint32 now)
{
    GameEndingPresentationState *ending;
    const Rich4CharacterSpeech *speech;

    if (session == NULL) return;
    ending = &session->presentation.ending;
    if (!ending->active || !ending->winner_voice_pending ||
        session->presentation.speech.active)
        return;
    speech = rich4_character_victory_speech(ending->winner_character);
    ending->winner_voice_pending = 0;
    if (speech != NULL)
        (void)game_start_speech_entry(
            session, ending->winner_player, speech, 3,
            GAME_PROPERTY_PRESENT_NONE, now);
}

static int game_update_presentation_handler(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now, int cursor_x,
    Rich4PresentationHandler handler)
{
    if (handler == RICH4_PRESENTATION_HANDLER_AUCTION) {
        Rich4AudioEventQueue *audio_events =
            rich4_app_session_audio_borrow(session);
        GameEventQueue events;
        int active;
        rich4_game_event_queue_reset(&events);
        active = rich4_auction_flow_update(
            session, now, rich4_audio_speech_playing(audio),
            audio_events, &events);
        game_session_audio_flush(session, audio);
        return active;
    }
    switch (handler) {
    case RICH4_PRESENTATION_HANDLER_SPECIAL_PLACE_NOTICE:
        if (session->presentation.speech.active &&
            game_update_property_speech(session, audio, now)) return 1;
        return rich4_special_place_notice_update(session, now);
    case RICH4_PRESENTATION_HANDLER_BANK_REJECTION:
        return game_update_bank_rejection_presentation(session, audio, now);
    case RICH4_PRESENTATION_HANDLER_SPECIAL_ACTOR:
        return game_update_special_actor_flow(
            session, assets, settings, config, now);
    case RICH4_PRESENTATION_HANDLER_PROPERTY_SPEECH:
        return game_update_property_speech(session, audio, now);
    case RICH4_PRESENTATION_HANDLER_MONSTER_CARD:
        return rich4_monster_card_flow_update(
            session,
            game_card_animation_frame_count(
                assets, session->presentation.monster_card.resource),
            now);
    case RICH4_PRESENTATION_HANDLER_SPECIAL_REWARD:
        return game_update_special_reward_presentation(
            session, assets, audio, now);
    case RICH4_PRESENTATION_HANDLER_SPECIAL_EVENT:
        return game_update_special_event_presentation(
            session, assets, settings, config, audio, now);
    case RICH4_PRESENTATION_HANDLER_INSURANCE:
        return rich4_insurance_flow_update(session, now);
    case RICH4_PRESENTATION_HANDLER_LOTTERY:
        return game_update_lottery_visit(session, assets, audio, now);
    case RICH4_PRESENTATION_HANDLER_CONFINEMENT_EXIT:
        return game_update_confinement_exit(session, config, now);
    case RICH4_PRESENTATION_HANDLER_CONFINEMENT_VISIT:
        return game_update_confinement_visit(session, audio, now);
    case RICH4_PRESENTATION_HANDLER_CONFINEMENT_STATUS:
        return game_update_confinement_status_presentation(
            session, assets, settings, now);
    case RICH4_PRESENTATION_HANDLER_POINT_MINIGAME:
        return game_update_point_minigame(
            session, assets, audio, now, cursor_x);
    case RICH4_PRESENTATION_HANDLER_SHOP:
        return game_update_shop_visit(session, audio, now);
    case RICH4_PRESENTATION_HANDLER_FACILITY_ROULETTE:
        return game_update_facility_roulette(session, audio, now);
    case RICH4_PRESENTATION_HANDLER_COMPANY_ISSUE:
        return rich4_company_issue_update(session, now);
    case RICH4_PRESENTATION_HANDLER_MAGIC_HOUSE:
        return game_update_magic_house(
            session, audio, assets, settings, now);
    case RICH4_PRESENTATION_HANDLER_BANKRUPTCY:
        return game_update_bankruptcy_presentation(session, assets, audio, now);
    case RICH4_PRESENTATION_HANDLER_GOD:
        return game_update_god_presentation(session, assets, audio, now,
                config == NULL || config->animation != 0);
    case RICH4_PRESENTATION_HANDLER_SPEECH:
        return game_update_property_speech(session, audio, now);
    case RICH4_PRESENTATION_HANDLER_NONE:
    default:
        return 0;
    }
}

/* Owns SDL-side presentation dispatch while feature flows migrate to app.
 * Borrowed session/assets/audio are advanced in the legacy observable order.
 * The app mode policy is updated once per frame at this boundary. */
int game_update_presentation_dispatch(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now, int cursor_x)
{
    Rich4PresentationActivity activity;

    game_limit_god_detachment(session, assets, settings);
    if (game_update_god_detachment(session, audio, now)) return 1;
    if (game_update_ai_tool(session, assets, settings, audio, now)) return 1;

    /* 0x40EC99 waits for each payer's bankruptcy/auction before charging
     * the next. These auctions are nested inside acquisition, not a route
     * event: hand off the released requests without advancing that route. */
    if (session->presentation.god_acquisition.money.phase == 10 &&
        session->presentation.bankruptcy.awaiting_auction &&
        !session->presentation.bankruptcy.active &&
        !session->presentation.auction.active) {
        Rich4AudioEventQueue *audio_events =
            rich4_app_session_audio_borrow(session);
        GameEvent event;
        int remaining = session->deferred_events.count;
        while (remaining-- > 0 &&
               rich4_game_event_queue_pop(&session->deferred_events, &event)) {
            if (event.type != GAME_EVENT_AUCTION_REQUESTED ||
                rich4_game_event_auction_trigger(&event) !=
                    GAME_AUCTION_TRIGGER_BANKRUPTCY ||
                !rich4_auction_flow_enqueue(session, &event, audio_events))
                (void)rich4_game_event_queue_push(&session->deferred_events, &event);
        }
        game_session_audio_flush(session, audio);
    }

    if (game_update_monthly_finance(session, assets, config, audio, now)) return 1;
    switch (rich4_presentation_update_step(
                &session->presentation, &session->state)) {
    case RICH4_PRESENTATION_UPDATE_GOD:
        if (game_update_god_presentation(session, assets, audio, now,
                config == NULL || config->animation != 0)) return 1;
        break;
    case RICH4_PRESENTATION_UPDATE_BANK:
        {
            Rich4AudioEventQueue *audio_events =
                rich4_app_session_audio_borrow(session);
            int active = rich4_bank_flow_update(
                session, now, rich4_audio_speech_playing(audio), audio_events);
            game_session_audio_flush(session, audio);
            if (active) return 1;
        }
        break;
    case RICH4_PRESENTATION_UPDATE_TOOL_PLACEMENT:
        if (game_tool_followup_update_placement(
                session, assets, settings, audio, now))
            return 1;
        break;
    case RICH4_PRESENTATION_UPDATE_PROPERTY_SPEECH:
        if (game_update_property_speech(session, audio, now)) return 1;
        break;
    case RICH4_PRESENTATION_UPDATE_TOOL_QUEUE:
        if (game_update_tool_animation_queue(
                session, assets, settings, audio, now,
                config != NULL && config->animation != 0)) return 1;
        break;
    case RICH4_PRESENTATION_UPDATE_TOOL_TRIGGER:
        if (game_tool_followup_finish_trigger_turn(
                session, assets, settings, now)) return 1;
        break;
    case RICH4_PRESENTATION_UPDATE_TOOL_ROBOT:
        if (game_tool_followup_update_robot(
                session, assets, settings, config, audio, now)) return 1;
        break;
    case RICH4_PRESENTATION_UPDATE_ENGINEERING_ATTACK:
        {
            int character = session->state.players[
                session->presentation.engineering_attack.player].character;
            int actor_frames = 0, effect_frames = 0;
            Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(session);
            if (assets != NULL && character >= 0 && character < 12 &&
                game_asset_ready(assets, &assets->character.has_map_player[character][12]))
                actor_frames = (int)assets->character.map_player_spr[
                    character][12].num_chunks / 8;
            /* Data[526] is already borrowed by the point-minigame pack. */
            if (assets != NULL && game_asset_ready(assets, &assets->ui.has_point_minigame_effect))
                effect_frames = game_asset_graph_frames(assets, &assets->ui.point_minigame_effect_data, assets->ui.point_minigame_effect_size);
            (void)rich4_engineering_attack_update(
                session, actor_frames, effect_frames,
                game_move_frame_ms_from_config(config), now, events);
            game_session_audio_flush(session, audio);
            return 1;
        }
    case RICH4_PRESENTATION_UPDATE_FEATURES:
    default:
        break;
    }

    game_update_intro_animation(session, assets, settings, now);
    game_update_dynamic_objects(session, assets, settings, now);
    game_update_pending_ending_video(session, now);
    game_update_pending_ending_voice(session, now);
    activity = rich4_presentation_activity_from_state(
        &session->presentation, &session->state);
    rich4_presentation_transition(
        &session->presentation,
        rich4_presentation_mode_resolve(&activity), NULL);
    return game_update_presentation_handler(
        session, assets, settings, config, audio, now, cursor_x,
        rich4_presentation_handler_for_mode(
            &session->presentation, session->presentation.mode));
}
