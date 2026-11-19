#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/render/game_overlay_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_confinement_event_render.h"
#include "platform/sdl1/game_stock_panel_names.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "app/card_use_flow.h"
#include "app/game_command_flow.h"
#include "app/monster_card_flow.h"
#include "platform/sdl1/game_loop_event_poll.h"
#include "platform/sdl1/game_loop_confirm_input.h"
#include "platform/sdl1/game_loop_mouse_button_up_input.h"
#include "platform/sdl1/runtime.h"
#include "app/card_defense_flow.h"
#include "app/property_flow.h"
#include "platform/sdl1/game_loop_confirm_company_issue.h"
#include "app/card_panel_flow.h"
#include "app/game_speech_flow.h"
#include "app/map_input_flow.h"
#include "app/special_place_notice_flow.h"
#include "core/game_event_payload.h"
#include "core/character_speech_rules.h"
#include "core/rng.h"
#include "view/map_renderer.h"
#include "resource/mkf.h"
#include "resource/flc_animation.h"

static int dice_panel_visible(const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GameSession *session)
{
    GameSceneView view;
    assert(build_game_session_render_view(assets, settings, session, &view));
    return view.game.dice.status_visible;
}

static void auction_card_yields_pointer_to_auction(GamePreviewAssets *assets)
{
    static GameSession session;
    Rich4SceneId scene = RICH4_SCENE_GAME;
    NewGameSettings settings = {0};
    Rich4Config config = {0};
    Rich4SdlConfig sdl_config = {0};
    Rich4OptionsPanelState options = {0};
    Rich4SceneMachine scene_machine = {0};
    HelpPanelState help = {0};
    HelpAssets help_assets = {0};
    SmallMapPanelState small_map = {0};
    Rich4AiSettingsState ai_settings = {0};
    StockPanelState stock = {0};
    VirtualCursor cursor = {.x = 406, .y = 133};
    int held = 0;
    GameMouseButtonDownInputContext down;
    GameConfirmInputContext confirm;
    GameMouseButtonUpInputContext up;

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].cash = 2000;
    session.presentation.card_use.phase = GAME_CARD_USE_EFFECTS;
    session.presentation.auction.active = 1;
    session.presentation.auction.stage = RICH4_AUCTION_PRESENT_BIDDING;
    session.presentation.auction.pressed_action = -1;
    session.presentation.auction.bidding.active = 1;
    session.presentation.auction.bidding.phase = GAME_AUCTION_BIDDING;
    session.presentation.auction.bidding.seller = -1;
    session.presentation.auction.bidding.current_player = 0;
    session.presentation.auction.bidding.current_price = 1000;
    session.presentation.auction.bidding.eligible_count = 1;
    session.presentation.auction.bidding.participant[0] =
        GAME_AUCTION_PARTICIPANT_READY;

    down = (GameMouseButtonDownInputContext){
        .app_state = &scene, .config = &config, .sdl_config = &sdl_config,
        .assets = assets, .session = &session, .settings = &settings,
        .options_panel = &options, .scene_machine = &scene_machine,
        .help_panel = &help, .help_assets = &help_assets,
        .small_map_panel = &small_map, .ai_settings_panel = &ai_settings,
        .card_panel = &session.presentation.card_panel,
        .tool_panel = &session.presentation.tool_panel, .stock_panel = &stock,
        .cursor = &cursor, .mouse_left_down = &held,
        .button = SDL_BUTTON_LEFT
    };
    confirm = (GameConfirmInputContext){
        .app_state = &scene, .assets = assets, .session = &session,
        .settings = &settings, .config = &config, .sdl_config = &sdl_config,
        .options_panel = &options, .scene_machine = &scene_machine,
        .help_panel = &help, .help_assets = &help_assets,
        .small_map_panel = &small_map, .ai_settings_panel = &ai_settings,
        .card_panel = &session.presentation.card_panel,
        .tool_panel = &session.presentation.tool_panel, .stock_panel = &stock,
        .cursor = &cursor
    };
    up = (GameMouseButtonUpInputContext){
        .app_state = scene, .session = &session, .assets = assets,
        .settings = &settings, .stock_panel = &stock, .help_panel = &help,
        .help_assets = &help_assets, .small_map_panel = &small_map,
        .tool_panel = &session.presentation.tool_panel, .cursor = &cursor,
        .button = SDL_BUTTON_LEFT, .mouse_left_down = &held,
        .config = &config, .sdl_config = &sdl_config,
        .options_panel = &options, .scene_machine = &scene_machine
    };

    assert(rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    assert(!game_handle_mouse_button_down_input(&down));
    assert(held && cursor.confirm_pressed && cursor.confirm_pointer_pressed);
    game_handle_confirm_input(&confirm);
    assert(session.presentation.auction.pressed_action == GAME_AUCTION_PASS);
    assert(!game_handle_mouse_button_up_input(&up));
    assert(!held);
    assert(session.presentation.auction.stage == RICH4_AUCTION_PRESENT_ACTION);
    assert(session.presentation.auction.bidding.last_action == GAME_AUCTION_PASS);
    assert(session.presentation.card_use.phase == GAME_CARD_USE_EFFECTS);
}

static void human_card_clicks(GamePreviewAssets *assets)
{
    static GameSession session;
    Rich4SceneId scene = RICH4_SCENE_GAME;
    NewGameSettings settings = {0};
    CardPanelState *panel = &session.presentation.card_panel;
    VirtualCursor cursor = {.x = 110, .y = 145};
    int held = 0;
    GameMouseButtonDownInputContext down = {
        .app_state = &scene, .session = &session, .settings = &settings, .assets = assets,
        .cursor = &cursor, .mouse_left_down = &held, .button = SDL_BUTTON_LEFT
    };
    GameMouseButtonUpInputContext up = {
        .app_state = scene, .session = &session, .settings = &settings, .assets = assets,
        .cursor = &cursor, .mouse_left_down = &held, .button = SDL_BUTTON_LEFT
    };
    rich4_sdl1_set_deterministic_ticks(1);
    const int cards[] = {1, 24, 25};
    for (size_t i = 0; i < sizeof(cards) / sizeof(cards[0]); ++i) {
        int card = cards[i];
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 2;
        session.state.players[0].play_state = session.state.players[1].play_state = 1;
        session.presentation.dice.status_visible = 1;
        assert(dice_panel_visible(assets, &settings, &session));
        session.state.calendar_start_year = 2000;
        session.state.calendar_start_month = 1;
        session.state.calendar_start_day = 3;
        session.state.stocks[7].f16 = session.state.stocks[7].f20 = 100;
        session.state.players[0].cash = 100;
        session.state.players[1].cash = 900;
        uint8_t *hand = session.state.players[0].card_inventory;
        hand[0] = 2;
        hand[1] = hand[2] = card;
        Rich4AppToolbarAction action = rich4_app_top_panel_action_at(340, 20);
        assert(rich4_app_apply_top_panel_action(&session, &action));
        assert(!dice_panel_visible(assets, &settings, &session));
        cursor.x = 110;
        cursor.y = 145;
        down.button = up.button = SDL_BUTTON_LEFT;
        assert(game_handle_mouse_button_down_input(&down));
        assert(held && panel->phase == CARD_PANEL_INVENTORY && panel->selected_slot == 1);
        cursor.x = 25;
        assert(game_handle_mouse_button_up_input(&up));
        assert(!held && panel->phase == CARD_PANEL_EFFECT && panel->selected_card == card - 1);
        assert(!dice_panel_visible(assets, &settings, &session));
        assert(panel->effect_until - panel->effect_started == 1500);
        assert(!rich4_card_panel_effect_due(panel, panel->effect_started + 1499));
        assert(hand[1] == card && hand[2] == card);

        if (card == 24) {
            rich4_sdl1_delay(1499);
            game_update_card_panel_timed_effect(panel, &session, assets, &settings, NULL);
            assert(panel->phase == CARD_PANEL_EFFECT);
            rich4_sdl1_delay(1);
        } else {
            assert(game_handle_mouse_button_down_input(&down));
            assert(!rich4_card_panel_effect_due(panel, rich4_sdl1_ticks()));
            assert(game_handle_mouse_button_up_input(&up));
        }
        game_update_card_panel_timed_effect(panel, &session, assets, &settings, NULL);
        assert(panel->phase == CARD_PANEL_WAIT_HANDLER);
        assert(session.presentation.speech.active && !session.presentation.speech.skip_requested);
        assert(!dice_panel_visible(assets, &settings, &session));
        assert(hand[0] == 2 && hand[1] == card && hand[2] == (card == 1 ? 0 : card));
        assert(session.state.card_amount[card - 1] == (card == 1));
        uint32_t started = rich4_sdl1_ticks();
        assert(game_update_card_use(&session, assets, &settings, NULL, started + 999));
        assert(session.presentation.speech.active && session.state.players[0].cash == 100);
        assert(game_update_card_use(&session, assets, &settings, NULL, started + 1000));
        if (card == 1) {
            assert(session.state.players[0].cash == 500 && session.state.players[1].cash == 500);
            assert(hand[0] == 2 && hand[1] == 1 && hand[2] == 0);
            assert(session.state.card_amount[0] == 1);
        } else {
            assert(panel->phase == CARD_PANEL_STOCK_TARGET);
            assert(!dice_panel_visible(assets, &settings, &session));
            if (card == 25) {
                cursor.x = 600;
                cursor.y = 80 + 7 * 32;
                assert(game_handle_mouse_button_down_input(&down));
                assert(game_handle_mouse_button_up_input(&up));
                uint32_t selected = rich4_sdl1_ticks();
                assert(game_update_card_use(&session, assets, &settings, NULL, selected));
                assert(session.presentation.card_use.phase == GAME_CARD_USE_STOCK_HOLD);
                assert(!dice_panel_visible(assets, &settings, &session));
                assert(session.state.stocks[7].f20 == 90 && hand[2] == 25);
                assert(game_handle_mouse_button_up_input(&up));
                assert(game_update_card_use(&session, assets, &settings, NULL, selected + 999));
                assert(session.state.card_amount[24] == 0);
                assert(game_update_card_use(&session, assets, &settings, NULL, selected + 1000));
                assert(session.state.stocks[7].f20 == 90 && hand[2] == 0 && session.state.card_amount[24] == 1);
                started = selected + 1000;
            } else {
                down.button = up.button = SDL_BUTTON_RIGHT;
                assert(game_handle_mouse_button_down_input(&down));
                assert(panel->phase == CARD_PANEL_STOCK_TARGET);
                assert(game_handle_mouse_button_up_input(&up));
                assert(panel->phase == CARD_PANEL_INVENTORY && panel->selected_slot == -1);
                assert(!dice_panel_visible(assets, &settings, &session));
                assert(hand[1] == card && hand[2] == card && !session.state.card_amount[card - 1]);
                up.button = SDL_BUTTON_LEFT;
                assert(game_handle_mouse_button_up_input(&up));
                assert(panel->phase == CARD_PANEL_INVENTORY);
                up.button = SDL_BUTTON_RIGHT;
                assert(game_handle_mouse_button_up_input(&up));
            }
        }
        for (int tick = 1; tick <= 8 && panel->phase == CARD_PANEL_WAIT_HANDLER; ++tick) {
            assert(!dice_panel_visible(assets, &settings, &session));
            (void)game_update_card_use(&session, assets, &settings, NULL,
                                       started + 1000 * (uint32_t)tick);
        }
        assert(panel->phase == CARD_PANEL_CLOSED);
        assert(dice_panel_visible(assets, &settings, &session));
    }
    rich4_sdl1_set_deterministic_ticks(0);
}

static void human_card_key_release(GamePreviewAssets *assets)
{
    static GameSession session;
    Rich4SceneId scene = RICH4_SCENE_GAME;
    VirtualCursor cursor = {.x = 25, .y = 145};
    NewGameSettings settings = {0};
    GameKeyDownInputContext down = {.session = &session, .cursor = &cursor, .assets = assets,
        .settings = &settings, .confirm_key = RICH4_APP_KEY_LETTER_A};
    GameKeyUpModalsContext up = {.session = &session};
    GameEventPollContext poll = {.app_state = &scene, .key_down = &down, .key_up = &up};
    CardPanelState *panel = &session.presentation.card_panel;
    SDL_Event key = {0};

    session.state.active = session.state.player_count = session.state.players[0].play_state = 1;
    session.state.players[0].card_inventory[0] = 1;
    rich4_card_panel_open(panel, &session);
    SDL_putenv("SDL_VIDEODRIVER=dummy");
    assert(SDL_InitSubSystem(SDL_INIT_VIDEO) == 0);
    key.type = SDL_KEYDOWN;
    key.key.keysym.sym = SDLK_a;
    assert(SDL_PushEvent(&key) == 0);
    game_poll_runtime_events(&poll);
    assert(panel->phase == CARD_PANEL_INVENTORY);
    key.type = SDL_KEYUP;
    assert(SDL_PushEvent(&key) == 0);
    game_poll_runtime_events(&poll);
    assert(panel->phase == CARD_PANEL_EFFECT && panel->effect_until - panel->effect_started == 1500);
    key.key.keysym.sym = SDLK_LSHIFT;
    assert(SDL_PushEvent(&key) == 0);
    game_poll_runtime_events(&poll);
    assert(rich4_card_panel_effect_due(panel, rich4_sdl1_ticks()));
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void rejected_use_restores_hand(GamePreviewAssets *assets)
{
    static GameSession session;
    NewGameSettings settings = {0};
    GameCommand command = rich4_game_domain_command(GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 0), rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
    const uint8_t hand[GAME_PLAYER_CARD_SLOTS] = {2, 1, 3, 1};
    session.state.active = session.state.player_count = session.state.players[0].play_state = 1;
    session.state.players[0].cash = 100;
    memcpy(session.state.players[0].card_inventory, hand, sizeof(hand));
    assert(game_begin_card_use(&session, assets, &settings, &command, 0, 100));
    assert(session.state.players[0].card_inventory[1] == 3 && session.state.card_amount[0] == 1);
    session.flow.steps_remaining = 1;
    rich4_speech_request_skip(&session);
    assert(game_update_card_use(&session, assets, &settings, NULL, 101));
    assert(!session.presentation.card_use.accepted);
    assert(!memcmp(session.state.players[0].card_inventory, hand, sizeof(hand)));
    assert(!session.state.card_amount[0]);
}

static void angel_building_choice_keeps_debit(GamePreviewAssets *assets)
{
    static GameSession session;
    CardPanelState *panel = &session.presentation.card_panel;
    NewGameSettings settings = {0};
    uint32_t now = 100;
    session.state.active = session.state.player_count = session.state.facility_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].card_inventory[0] = 9;
    panel->phase = CARD_PANEL_WAIT_HANDLER;
    panel->selected_card = 8;
    GameCommand command = rich4_card_panel_build_command(panel,
        rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0), 0, 0, 0);
    assert(game_begin_card_use(&session, assets, &settings, &command, 0, now));
    assert(session.state.card_amount[8] == 1 && !session.state.players[0].card_inventory[0]);
    rich4_speech_request_skip(&session);
    while (panel->phase != CARD_PANEL_FACILITY_CHOICE && now < 2100) {
        now += 30;
        rich4_property_flash_frame_presented(&session, now);
        game_update_card_use(&session, assets, &settings, NULL, now);
    }
    assert(panel->phase == CARD_PANEL_FACILITY_CHOICE && !rich4_card_panel_cancel(panel));
    assert(session.state.card_amount[8] == 1 && !session.state.facility_level[0]);
    game_card_panel_confirm(&session, assets, &settings, panel, 190, 300, now, NULL);
    assert(game_update_card_use(&session, assets, &settings, NULL, now + 1));
    assert(session.state.facility_level[0] == 1 && session.state.facility_type[0] == 2);
    assert(session.state.card_amount[8] == 1 && !session.state.players[0].card_inventory[0]);
}

static void animation_duration(const Rich4FlcHeader *header, int card, int resource)
{
    static GameSession session;
    GameCommand command = rich4_game_domain_command(GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, card), rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), 0);
    uint32_t now = 1900;
    memset(&session, 0, sizeof(session));
    assert(header->frame_count == (resource == 557 ? 22 : 68) && header->delay_ms == 71);
    rich4_app_sync_card_animation(&session, &command, 1, resource, header->frame_count, header->delay_ms, now);
    for (int i = 0; i < header->frame_count; ++i) {
        assert(rich4_monster_card_flow_update(&session, header->frame_count, header->delay_ms, now));
        assert(session.presentation.monster_card.frame == i);
        now += header->delay_ms;
    }
    assert(rich4_monster_card_flow_update(&session, header->frame_count, header->delay_ms, now));
    assert(!rich4_card_use_skip_notice(&session, now));
    assert(rich4_monster_card_flow_update(&session, header->frame_count, header->delay_ms, now + 499));
    assert(!rich4_monster_card_flow_update(&session, header->frame_count, header->delay_ms, now + 500));
}

static void ai_waits_before_applying(GamePreviewAssets *assets)
{
    static GameSession session;
    NewGameSettings settings = {0};
    GameCommand command;
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.players[0].is_ai = 1;
    session.state.players[0].play_state = session.state.players[1].play_state = 1;
    session.state.players[0].cash = 100;
    session.state.players[1].cash = 900;
    session.state.players[0].card_inventory[0] = 1;
    command = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
        GAME_COMMAND_SOURCE_AI, 0, rich4_game_domain_ref(GAME_DOMAIN_CARD, 0),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
    assert(game_begin_card_use(&session, assets, &settings, &command, 1, 100));
    assert(game_update_card_use(&session, assets, &settings, NULL, 1599));
    assert(session.state.players[0].cash == 100);
    assert(session.state.players[0].card_inventory[0] == 1);
    assert(!session.presentation.dice.popup_active);
    assert(!session.flow.turn_flow_active);
    assert(game_update_card_use(&session, assets, &settings, NULL, 1600));
    assert(session.presentation.speech.active && session.presentation.speech.type == 3);
    rich4_speech_request_skip(&session);
    assert(game_update_card_use(&session, assets, &settings, NULL, 1601));
    assert(session.state.players[0].cash == 500);
    assert(session.state.players[0].card_inventory[0] == 0);
    assert(!session.presentation.dice.popup_active);
    assert(game_update_card_use(&session, assets, &settings, NULL, 1602));
    assert(game_update_card_use(&session, assets, &settings, NULL, 1603));
    assert(session.presentation.card_use.phase == GAME_CARD_USE_NONE);
    assert(session.flow.turn_flow_active && session.flow.turn_state == GAME_TURN_STATE_PREROLL);
    assert(session.state.players[0].cash == 500);
}

static void human_frame_card_revenge(GamePreviewAssets *assets)
{
    static GameSession session;
    NewGameSettings settings = {0};
    MapRouteNode routes[4] = {
        [1] = {.x = 1000, .y = 1100}, [2] = {.x = 1600, .y = 1700},
        [3] = {.x = 2100, .y = 2200}
    };
    MapSpecialRecord specials[3] = {[2] = {.x = 2300, .y = 2400}};
    MapData saved_map = assets->map.maps[0];
    GameSceneView view;
    uint32_t rng = 1, last_speech = UINT32_MAX;
    int defender_reaction = rich4_rng_next_at(&rng) & 1;
    int attacker_reaction = rich4_rng_next_at(&rng) & 1;
    int speech_count = 0, saw_notice = 0;
    const struct {
        int player, type, x, y;
        const Rich4CharacterSpeech *line;
    } expected[] = {
        {0, 3, 1000, 1100, rich4_character_card_speech(0, 0, 16)},
        {1, 2, 1600, 1700, rich4_character_special_event_speech(1, 0, defender_reaction)},
        {1, 2, 2300, 2400, rich4_character_confinement_speech(1, 0)},
        {1, 1, 2300, 2400, rich4_character_card_speech(1, 2, 16)},
        {1, 0, 2300, 2400, rich4_character_card_speech(1, 0, 17)},
        {0, 2, 1000, 1100, rich4_character_card_speech(0, 2, 17)},
        {0, 2, 1000, 1100, rich4_character_special_event_speech(0, 0, attacker_reaction)},
        {0, 2, 2300, 2400, rich4_character_confinement_speech(0, 0)}
    };

    assets->map.maps[0] = (MapData){
        .routes = routes, .route_count = 4, .specials = specials, .special_count = 3
    };
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.price_index = 1;
    session.state.ai_rng_state = 1;
    session.state.prison_node = 3;
    session.state.tool_route_node_count = 4;
    for (int node = 1; node < 4; ++node) {
        session.state.tool_route_x[node] = routes[node].x;
        session.state.tool_route_y[node] = routes[node].y;
    }
    for (int player = 0; player < 2; ++player) {
        session.state.players[player].play_state = 1;
        session.state.players[player].character = player;
        session.state.players[player].node = player + 1;
        session.state.players[player].card_inventory[0] = player + 17;
    }
    GameCommand command = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
        GAME_COMMAND_SOURCE_HUMAN, 0, rich4_game_domain_ref(GAME_DOMAIN_CARD, 16),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), 0);
    rich4_sdl1_set_deterministic_ticks(1);
    assert(game_begin_card_use(&session, assets, &settings, &command, 0, rich4_sdl1_ticks()));
    for (int frame = 0; frame < 40 &&
         session.presentation.card_use.phase != GAME_CARD_USE_NONE; ++frame) {
        const GameCardUsePresentationState *use = &session.presentation.card_use;
        if (use->phase == GAME_CARD_USE_REVENGE &&
            use->defense.phase == GAME_PROPERTY_PRESENT_INSURANCE_CARD) {
            assert(build_game_session_render_view(assets, &settings, &session, &view));
            assert(view.game.viewport.center_world_x == specials[2].x);
            assert(view.game.viewport.center_world_y == specials[2].y);
            saw_notice = 1;
        }
        const GameSpeechPresentationState *speech = &session.presentation.speech;
        if (speech->active && speech->tick != last_speech) {
            assert(speech_count < (int)(sizeof(expected) / sizeof(expected[0])));
            assert(speech->player == expected[speech_count].player);
            assert(speech->type == expected[speech_count].type);
            assert(speech->current == expected[speech_count].line);
            assert(build_game_session_render_view(assets, &settings, &session, &view));
            assert(view.game.viewport.center_world_x == expected[speech_count].x);
            assert(view.game.viewport.center_world_y == expected[speech_count].y);
            if (speech_count == 5 || speech_count == 6)
                assert(session.state.players[0].days_in_prison == 0);
            last_speech = speech->tick;
            ++speech_count;
        }
        rich4_sdl1_delay(1000);
        update_game_session(&session, assets, &settings, NULL, NULL, rich4_sdl1_ticks(), 0);
    }
    assert(saw_notice && speech_count == (int)(sizeof(expected) / sizeof(expected[0])));
    assert(session.presentation.card_use.phase == GAME_CARD_USE_NONE);
    assert(!session.presentation.has_view_override);
    assert(session.state.players[0].days_in_prison == 5);
    assert(session.state.players[1].days_in_prison == 5);
    assert(session.state.players[1].card_inventory[0] == 0);
    rich4_sdl1_set_deterministic_ticks(0);
    assets->map.maps[0] = saved_map;
}

static void stock_card_waits(GamePreviewAssets *assets, int card, int skip)
{
    static GameSession session;
    NewGameSettings settings = {0};
    GameCommand command;
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.players[0].is_ai = 1;
    session.state.players[0].play_state = session.state.players[1].play_state = 1;
    session.state.players[0].card_inventory[0] = card + 1;
    session.state.stocks[7].f16 = session.state.stocks[7].f20 = 100.0f;
    command = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
        GAME_COMMAND_SOURCE_AI, 0, rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, 7), 0);
    assert(game_begin_card_use(&session, assets, &settings, &command, 1, 100));
    assert(game_update_card_use(&session, assets, &settings, NULL, 1600));
    assert(session.presentation.speech.active);
    assert(session.state.stocks[7].f20 == 100.0f);
    assert(session.presentation.card_panel.phase == CARD_PANEL_CLOSED);
    rich4_speech_request_skip(&session);
    assert(game_update_card_use(&session, assets, &settings, NULL, 1601));
    assert(session.state.stocks[7].f20 == (card == 23 ? 110.0f : 90.0f));
    assert(session.presentation.special_place_notice.queue.count == 1);
    assert(!session.flow.turn_flow_active);
    /* Use the real shared dispatcher, not the card updater alone. */
    assert(game_update_presentation_dispatch(&session, assets, &settings, NULL, NULL, 1602, 0));
    assert(session.presentation.special_place_notice.queue.count == 1);
    if (skip) assert(rich4_special_place_notice_skip(&session, 1603));
    assert(game_update_presentation_dispatch(&session, assets, &settings, NULL, NULL,
        skip ? 1603 : 3100, 0));
    if (!skip) {
        assert(session.presentation.special_place_notice.queue.count == 1);
        assert(game_update_presentation_dispatch(&session, assets, &settings, NULL, NULL, 3101, 0));
    }
    assert(session.presentation.special_place_notice.queue.count == 0);
    assert(!session.flow.turn_flow_active);
    assert(game_update_card_use(&session, assets, &settings, NULL, 3102));
    assert(game_update_card_use(&session, assets, &settings, NULL, 3103));
    assert(session.flow.turn_flow_active);
    assert(session.presentation.card_panel.phase == CARD_PANEL_CLOSED);
}

static void stock_notice_names(SDL_Surface *surface, SDL_Surface *reference,
    GamePreviewAssets *assets, Rich4Mkf *mkf)
{
    static GameSession session;
    unsigned char *dialog = NULL;
    size_t size = 0;
    assert(rich4_mkf_read_resource(mkf, 517, &dialog, &size, NULL) == RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(dialog, size, &assets->ui.common_dialog_spr));
    assets->ui.common_dialog_data = dialog;
    assets->ui.common_dialog_size = size;
    assets->ui.has_common_dialog = 1;
    for (int map = 0; map < GAME_BACKGROUND_COUNT; map++)
        for (int stock = 0; stock < GAME_STOCK_COUNT; stock++)
            for (int card = 23; card <= 24; card++) {
                char name[64], message[128];
                Rich4GameEventPresentationSnapshot snapshot;
                GameEvent event;
                memset(&session, 0, sizeof(session));
                session.state.player_count = 1;
                session.state.stage = map / GAME_MAPS_PER_STAGE;
                session.state.map = map % GAME_MAPS_PER_STAGE;
                event = rich4_game_event_special_place_notice(0, GAME_SPECIAL_NOTICE_STOCK_CARD,
                    rich4_game_domain_ref(GAME_DOMAIN_STOCK, stock), card, 1500);
                assert(rich4_special_place_notice_enqueue(&session, &event, 0));
                snapshot = rich4_app_build_game_event_presentation_snapshot(
                    &session.state, &session.presentation);
                SDL_FillRect(surface, NULL, 0xf81f);
                SDL_FillRect(reference, NULL, 0xf81f);
                draw_special_place_notice(surface, assets, &snapshot);
                game_compact_display_name(name, sizeof(name),
                    game_stock_name_for_state(&session.state, stock));
                assert(name[0] != '\0');
                snprintf(message, sizeof(message), "\xb9\xef%s\xa8\xcf\xa5\xce%s\xa1\x49",
                    name, game_card_big5_name(card));
                draw_spr_smp_center(reference, dialog, size,
                    &assets->ui.common_dialog_spr, 5, 220, 140);
                assert(memcmp(surface->pixels, reference->pixels, surface->pitch * surface->h) != 0);
                draw_big5_text(reference, 220, 140, message, 16,
                    rich4_character_rgb565_color(0x00f0f0f0u),
                    rich4_character_rgb565_color(0x00101010u), 3, 4);
                assert(memcmp(surface->pixels, reference->pixels, surface->pitch * surface->h) == 0);
            }
    free(dialog);
    assets->ui.common_dialog_data = NULL;
    assets->ui.has_common_dialog = 0;
}

/* 0x441F73 uses an opaque card blit, unlike the transparent title panel.
 * Check all original card pixels over a nonblack destination. */
static void passive_choices(SDL_Surface *surface, SDL_Surface *reference,
    GamePreviewAssets *assets, Rich4Mkf *mkf)
{
    static GameSession session;
    Rich4GameEventPresentationSnapshot snapshot;
    Rich4SprSmpChunk frame;
    unsigned char *portraits=NULL,*choice=NULL,*confirm=NULL,*dialog=NULL;
    int left,top;
    memset(&session,0,sizeof(session));
    session.state.player_count=3;
    for(int i=0;i<3;i++) {session.state.players[i].play_state=1;session.state.players[i].character=i;}
    session.state.players[1].card_inventory[0]=19;
    assert(rich4_mkf_read_resource(mkf,2,&portraits,&assets->character.character_size,NULL)==RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(portraits,assets->character.character_size,&assets->character.character_spr));
    assets->character.character_data=portraits;assets->character.has_character_ui=1;
    assert(rich4_mkf_read_resource(mkf,518,&choice,&assets->ui.player_choice_size,NULL)==RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(choice,assets->ui.player_choice_size,&assets->ui.player_choice_spr));
    assets->ui.player_choice_data=choice;assets->ui.has_player_choice=1;
    assert(rich4_mkf_read_resource(mkf,440,&confirm,&assets->ui.confirm_dialog_size,NULL)==RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(confirm,assets->ui.confirm_dialog_size,&assets->ui.confirm_dialog_spr));
    assets->ui.confirm_dialog_data=confirm;assets->ui.has_confirm_dialog=1;
    assert(rich4_mkf_read_resource(mkf,517,&dialog,&assets->ui.common_dialog_size,NULL)==RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(dialog,assets->ui.common_dialog_size,&assets->ui.common_dialog_spr));
    assets->ui.common_dialog_data=dialog;assets->ui.has_common_dialog=1;
    session.presentation.card_use.phase=GAME_CARD_USE_DEFENSE;
    session.presentation.card_use.command.player=0;
    session.presentation.card_use.defense.actor=1;
    session.presentation.card_use.defense.card=18;
    session.presentation.card_use.defense.hover=2;
    snapshot=rich4_app_build_game_event_presentation_snapshot(&session.state,&session.presentation);
    for(int phase=0;phase<2;phase++) {
        int p=phase ? GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM : GAME_PROPERTY_PRESENT_INSURANCE_TARGET;
        session.presentation.card_use.defense.phase=p;
        SDL_FillRect(surface,NULL,0xf81f);SDL_FillRect(reference,NULL,0xf81f);
        draw_insurance_charge(surface,assets,&snapshot);
        session.presentation.card_use.defense.phase=0;
        session.presentation.property.insurance_active=1;
        session.presentation.property.actor=1;
        session.presentation.property.insurance_target=2;
        session.presentation.property.phase=(Rich4PropertyPresentationPhase)p;
        draw_insurance_charge(reference,assets,&snapshot);
        assert(memcmp(surface->pixels,reference->pixels,surface->pitch*surface->h)==0);
        session.presentation.property.insurance_active=0;
    }
    session.presentation.card_use.defense.phase=GAME_PROPERTY_PRESENT_INSURANCE_TARGET;
    assert(rich4_spr_smp_get_chunk(choice,assets->ui.player_choice_size,
        &assets->ui.player_choice_spr,0,&frame)==RICH4_SPR_SMP_OK);
    left=220-frame.x+12;top=320-frame.y+12;
    assert(game_insurance_target_at(assets,&session.state,1,left+90,top+10)==2);
    assert(game_handle_insurance_charge_input(&session,assets,SDL_BUTTON_MIDDLE,left+90,top+10,100));
    assert(session.presentation.card_use.defense.phase==GAME_PROPERTY_PRESENT_INSURANCE_TARGET);
    assert(game_handle_insurance_charge_input(&session,assets,SDL_BUTTON_LEFT,left+90,top+10,101));
    assert(session.presentation.card_use.defense.target==2);
    assert(session.presentation.speech.active && session.presentation.speech.player==1);
    rich4_speech_request_skip(&session);(void)rich4_speech_finish_update(&session,102,0);
    session.presentation.card_use.defense.phase=GAME_PROPERTY_PRESENT_INSURANCE_TARGET;
    assert(game_handle_insurance_charge_input(&session,assets,SDL_BUTTON_RIGHT,0,0,103));
    assert(session.presentation.card_use.defense.phase==GAME_PROPERTY_PRESENT_NONE);
    assert(session.state.players[1].card_inventory[0]==19);
    free(portraits);free(choice);free(confirm);free(dialog);
}

static void destruction_background(GamePreviewAssets *assets)
{
    static GameSession session;
    GameSceneView view;
    NewGameSettings settings={0};
    for(int facility=0;facility<2;facility++)for(int card=10;card<=11;card++) {
        int refresh=card==10 ? 15 : 37;
        memset(&session,0,sizeof(session));
        session.state.active=1;session.state.player_count=1;
        session.state.players[0].play_state=1;
        session.state.land_count=session.state.facility_count=1;
        session.presentation.card_use.phase=GAME_CARD_USE_EFFECTS;
        session.presentation.card_use.command.target=rich4_game_domain_ref(
            facility ? GAME_DOMAIN_FACILITY : GAME_DOMAIN_LAND,0);
        session.presentation.card_use.previous_building_type=1;
        session.presentation.card_use.previous_building_level=3;
        session.presentation.monster_card.active=1;
        session.presentation.monster_card.resource=card==10 ? 557 : 529;
        session.presentation.monster_card.frame=refresh-1;
        assert(build_game_session_render_view(assets,&settings,&session,&view));
        assert((facility ? view.game.facilities[0].level : view.game.lands[0].level)==3);
        assert((facility ? view.game.facilities[0].type : view.game.lands[0].type)==1);
        session.presentation.monster_card.frame=refresh;
        assert(build_game_session_render_view(assets,&settings,&session,&view));
        assert((facility ? view.game.facilities[0].level : view.game.lands[0].level)==0);
        assert((facility ? view.game.facilities[0].type : view.game.lands[0].type)==0);
    }
}

int main(void)
{
    static GamePreviewAssets assets;
    Rich4Mkf *mkf = NULL;
    SDL_Surface *surface = SDL_CreateRGBSurface(
        SDL_SWSURFACE, 640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    SDL_Surface *reference = SDL_CreateRGBSurface(
        SDL_SWSURFACE, 640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    static GameSession session;
    int card;
    assert(surface != NULL);
    assert(reference != NULL);
    assert(rich4_mkf_open("assets/original/Data.mkf", &mkf) == RICH4_MKF_OK);
    for (card = 0; card < 30; ++card) {
        unsigned char *data = NULL;
        size_t size = 0;
        int x, y;
        assert(rich4_mkf_read_resource(mkf, 571 + card, &data, &size,
                                      NULL) == RICH4_MKF_OK);
        assert(size == 165u * 256u * 2u);
        assets.ui.has_card_effect[card] = 1;
        assets.ui.card_effect_data[card] = data;
        assets.ui.card_effect_size[card] = size;
        SDL_FillRect(surface, NULL, 0xf81f);
        draw_original_card_presentation(surface, &assets, card, NULL);
        for (y = 0; y < 256; ++y) {
            const uint16_t *row = (const uint16_t *)(
                (const unsigned char *)surface->pixels + (200 + y) * surface->pitch);
            for (x = 0; x < 165; ++x) {
                size_t offset = (y * 165 + x) * 2;
                uint16_t pixel = data[offset] | ((uint16_t)data[offset + 1] << 8);
                assert(row[138 + x] == map_resource_pixel(surface, pixel));
            }
            assert(row[137] == 0xf81f && row[303] == 0xf81f);
        }
        /* The original 0x441BAA routes human and AI through 0x441F73. */
        SDL_FillRect(surface, NULL, 0xf81f);
        SDL_FillRect(reference, NULL, 0xf81f);
        session.presentation.card_panel.phase = CARD_PANEL_EFFECT;
        session.presentation.card_panel.selected_card = card;
        game_draw_card_panel(surface, &assets, &session, &session.presentation.card_panel);
        session.presentation.card_use.phase = GAME_CARD_USE_NOTICE;
        session.presentation.card_use.command.subject.id = card;
        draw_card_use_presentation(reference, &assets, &session.presentation.card_use);
        assert(memcmp(surface->pixels, reference->pixels, surface->pitch * surface->h) == 0);
        free(data);
        assets.ui.card_effect_data[card] = NULL;
    }
    /* 0x443AAF/0x443DDC explicitly pass (0,40), independent of FLC size. */
    for (card = 0; card < 2; ++card) {
        unsigned char *data = NULL;
        size_t size = 0;
        Rich4FlcHeader header;
        GameOverlayRenderView view;
        int resource = card ? 529 : 557;
        int frame;
        memset(&view, 0, sizeof(view));
        assert(rich4_mkf_read_resource(mkf, resource, &data, &size, NULL) == RICH4_MKF_OK);
        assert(rich4_flc_parse_header(data, size, &header) == RICH4_FLC_OK);
        printf("card FLC: Data[%d] %ux%u, %u ms/frame\n", resource,
            header.width, header.height, (unsigned)header.delay_ms);
        animation_duration(&header, card + 10, resource);
        frame = header.frame_count / 2;
        if (card) {
            assets.ui.has_demolish_card_animation = 1;
            assets.ui.demolish_card_data = data; assets.ui.demolish_card_size = size;
        } else {
            assets.ui.has_monster_card_animation = 1;
            assets.ui.monster_card_data = data; assets.ui.monster_card_size = size;
        }
        view.monster_card_animation_active = 1;
        view.monster_card_animation_resource = resource;
        view.monster_card_animation_frame = frame;
        SDL_FillRect(surface, NULL, 0xf81f);
        SDL_FillRect(reference, NULL, 0xf81f);
        assert(draw_game_graph_frame_data(reference, data, size, frame, 0, 40));
        draw_game_monster_card_animation(surface, &assets, &view);
        assert(memcmp(surface->pixels, reference->pixels, surface->pitch * surface->h) == 0);
        free(data);
    }
    {
        unsigned char *data = NULL;
        size_t size = 0;
        assert(rich4_mkf_read_resource(mkf, 415, &data, &size, NULL) == RICH4_MKF_OK);
        assert(rich4_spr_smp_probe(data, size, &assets.map.data_map_status_spr[19]));
        assert(assets.map.data_map_status_spr[19].num_chunks == 1);
        assets.map.data_map_status_data[19] = data;
        assets.map.data_map_status_size[19] = size;
        assets.map.has_data_map_status[19] = 1;
        session.presentation.card_use.phase = GAME_CARD_USE_FLIGHT;
        session.presentation.card_use.flight_initialized = 1;
        session.presentation.card_use.flight_sprite_index = 19;
        session.presentation.card_use.flight_steps = 2;
        session.presentation.card_use.flight_x = 220;
        session.presentation.card_use.flight_y = 260;
        SDL_FillRect(surface, NULL, 0xf81f);
        SDL_FillRect(reference, NULL, 0xf81f);
        draw_card_use_presentation(surface, &assets, &session.presentation.card_use);
        assert(memcmp(surface->pixels, reference->pixels, surface->pitch * surface->h) != 0);
        free(data);
        assets.map.data_map_status_data[19] = NULL;
    }
    destruction_background(&assets);
    auction_card_yields_pointer_to_auction(&assets);
    human_card_clicks(&assets);
    angel_building_choice_keeps_debit(&assets);
    rejected_use_restores_hand(&assets);
    human_card_key_release(&assets);
    ai_waits_before_applying(&assets);
    human_frame_card_revenge(&assets);
    for (card = 23; card <= 24; card++)
        for (int skip = 0; skip < 2; skip++) stock_card_waits(&assets, card, skip);
    stock_notice_names(surface, reference, &assets, mkf);
    passive_choices(surface, reference, &assets, mkf);
    rich4_mkf_close(mkf);
    SDL_FreeSurface(reference);
    SDL_FreeSurface(surface);
    puts("card presentation: shared card art, stock target names on all maps, speech and deferred AI roll: ok");
    return 0;
}
