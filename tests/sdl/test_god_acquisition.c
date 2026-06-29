#define SDL_MAIN_HANDLED
#include <SDL/SDL.h>
#undef main

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/game_event_payload.h"
#include "core/god.h"
#include "app/game_update_flow.h"
#include "app/auction_flow.h"
#include "app/dynamic_object_flow.h"
#include "app/property_flow.h"
#include "app/game_speech_flow.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/render/game_scene_view.h"
#include "platform/sdl1/game_view_model_input.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_route_render.h"
#include "app/bankruptcy_flow.h"
#include "data/resource_catalog.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/game_loop_key_up_modals.h"
#include "platform/sdl1/game_loop_mouse_button_down_input.h"
#include "platform/sdl1/game_loop_mouse_button_up_input.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/text/runtime_text.h"
#include "resource/mkf.h"
#include "view/map_renderer.h"

static GameSession session;
static GamePreviewAssets assets;

/* This fixture drives the intervening FLC frames before checking a
 * nominal deadline. A separate real-stall test verifies that a single late
 * update cannot skip those frames or bypass the final interval. */
static int drive_god_frames(GameSession *s, const GamePreviewAssets *a,
    Rich4Audio *audio, Uint32 now, int enabled)
{
    GameGodAcquisitionPresentationState *g = &s->presentation.god_acquisition;
    Rich4AnimationClock *c = &s->presentation.animation_clock[RICH4_ANIMATION_GOD];
    if (g->active && enabled && !g->arrival_speech_phase && !g->waiting_for_detach &&
        g->message_visible == RICH4_GOD_MESSAGE_HIDDEN && g->frame_count > 0) {
        Uint32 tick = c->started ? c->tick : g->started;
        if (!c->started) (void)game_update_god_presentation(s,a,audio,tick,enabled);
        while (g->message_visible == RICH4_GOD_MESSAGE_HIDDEN &&
               (int32_t)(now-tick) >= g->frame_delay_ms) {
            tick += (Uint32)g->frame_delay_ms;
            (void)game_update_god_presentation(s,a,audio,tick,enabled);
        }
    }
    return game_update_god_presentation(s,a,audio,now,enabled);
}
#define game_update_god_presentation drive_god_frames

static uint32_t pixels_hash(SDL_Surface *surface)
{
    uint32_t hash = 2166136261u;
    int row, byte;
    assert(SDL_LockSurface(surface) == 0);
    for (row = 0; row < surface->h; row++)
        for (byte = 0; byte < surface->w * 2; byte++) {
            hash ^= ((unsigned char *)surface->pixels)[row * surface->pitch + byte];
            hash *= 16777619u;
        }
    SDL_UnlockSurface(surface);
    return hash;
}

static void run_god(SDL_Surface *screen, int type)
{
    GameEventQueue events;
    GameEvent event;
    Rich4GameEventPresentationSnapshot snapshot;
    GameGodAcquisitionPresentationState *god;
    uint32_t now = 100;
    uint32_t first_hash, spin_hash, result_hash = 0;
    int i, began = 0, result_amount = -1;
    Rich4SceneId scene = RICH4_SCENE_GAME;
    VirtualCursor cursor;
    GameMouseButtonDownInputContext down;
    GameMouseButtonUpInputContext up;
    int mouse_down = 1;
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 4;
    session.state.price_index = 7;
    session.state.ai_rng_state = 12345;
    for (i = 0; i < 4; i++) {
        session.state.players[i].play_state = 1;
        session.state.players[i].cash = 50000;
        session.state.players[i].bank = 10000;
        session.state.players[i].linked_player = -1;
        session.state.players[i].attached_timed_bomb_slot = -1;
    }
    rich4_game_event_queue_reset(&events);
    assert(rich4_god_attach(&session.state, 0, type, type, &events) == GAME_GOD_APPLIED);
    if (type == 5) {
        session.presentation.god_detachment.active = 1;
        session.presentation.god_detachment.started = 1;
        session.presentation.god_detachment.frame_tick = now;
        session.presentation.god_detachment.frame_count = 24;
        session.presentation.god_detachment.frame_delay_ms = 60;
    }
    while (rich4_game_event_queue_pop(&events, &event))
        if (event.type == GAME_EVENT_GOD_ACQUIRED) {
            game_begin_god_presentation(&session, &assets, &event, now);
            began++;
        }
    assert(began == 1);
    god = &session.presentation.god_acquisition;
    assert(god->frame_count > 0 && god->money_pending && !god->money.active);
    if (type == 6) {
        session.presentation.has_view_override = 1;
        session.presentation.view_world_x = 900;
        session.presentation.view_world_y = 1000;
        session.presentation.map_direction = 5;
    }
    if (type == 5) {
        uint32_t duration = god->animation_until - now;
        assert(god->waiting_for_detach && !god->sound_pending);
        assert(game_update_god_presentation(&session, &assets, NULL, now + 1439, 1));
        assert(god->waiting_for_detach && god->frame == 0);
        now += 1440;
        assert(game_update_god_presentation(&session, &assets, NULL, now, 1));
        assert(!god->waiting_for_detach && !god->sound_pending);
        assert(god->animation_until == now + duration);
    }
    if (type == 5 || type == 6) {
        if (!session.presentation.speech.active)
            assert(game_update_god_presentation(&session, &assets, NULL, now, 1));
        assert(session.presentation.speech.active && !god->sound_pending);
        assert(session.presentation.speech.current->speaking_resource == 1072);
        if (type == 6) {
            assert(session.presentation.has_view_override);
            assert(session.presentation.view_world_x == 900 &&
                   session.presentation.view_world_y == 1000);
            assert(session.presentation.map_direction == 5);
        }
        rich4_speech_request_skip(&session);
        (void)game_update_property_speech(&session, NULL, ++now);
        assert(game_update_god_presentation(&session, &assets, NULL, now, 1));
        assert(!god->arrival_speech_phase && god->sound_pending);
    }
    snapshot = rich4_app_build_game_event_presentation_snapshot(
        &session.state, &session.presentation);
    SDL_FillRect(screen, NULL, 0);
    draw_god_presentation(screen, &assets, &snapshot);
    first_hash = pixels_hash(screen);
    now = god->animation_until;
    assert(game_update_god_presentation(&session, &assets, NULL, now, 1));
    assert(god->message_visible == RICH4_GOD_MESSAGE_MAIN && !god->money.active);
    now = god->until - 1;
    assert(game_update_god_presentation(&session, &assets, NULL, now, 1));
    assert(!god->money.active && session.state.players[0].cash == 50000);
    now++;
    assert(game_update_god_presentation(&session, &assets, NULL, now, 1));
    assert(god->money.active);
    SDL_FillRect(screen, NULL, 0);
    draw_god_presentation(screen, &assets, &snapshot);
    spin_hash = pixels_hash(screen);
    assert(spin_hash != first_hash);
    /* Full SDL input gates consume pointer input before reaching panels. */
    memset(&cursor, 0, sizeof(cursor));
    memset(&down, 0, sizeof(down));
    down.app_state = &scene;
    down.session = &session;
    down.cursor = &cursor;
    down.button = SDL_BUTTON_LEFT;
    down.mouse_left_down = &mouse_down;
    cursor.confirm_pressed = cursor.confirm_pointer_pressed = 1;
    assert(game_handle_mouse_button_down_input(&down));
    assert(!cursor.confirm_pressed && god->money.phase == 1);
    if (type == 2) {
        GameKeyUpModalsContext key_up = {RICH4_SCENE_GAME, &session, NULL};
        game_handle_key_up_modals(&key_up);
    } else {
        memset(&up, 0, sizeof(up));
        up.app_state = scene;
        up.session = &session;
        up.cursor = &cursor;
        up.mouse_left_down = &mouse_down;
        up.button = type == 1 ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
        assert(game_handle_mouse_button_up_input(&up));
        assert(!cursor.confirm_pressed && !cursor.cancel_pressed);
        if (up.button == SDL_BUTTON_LEFT) assert(mouse_down == 0);
    }
    assert(god->money.phase == 2);
    for (i = 0; i < 1000 && god->active; i++) {
        now += 30;
        (void)game_update_god_presentation(&session, &assets, NULL, now, 1);
        if (god->active) assert(session.state.players[0].cash == 50000);
        if (god->money.phase == 9 && result_amount < 0) {
            result_amount = god->money.amount;
            SDL_FillRect(screen, NULL, 0);
            draw_god_presentation(screen, &assets, &snapshot);
            result_hash = pixels_hash(screen);
            assert(result_hash != spin_hash);
            if (type == 2) assert(SDL_SaveBMP(screen, "build/god_money_sdl.bmp") == 0);
        }
    }
    assert(i < 1000 && result_amount >= 0 && !god->money_pending);
    assert(session.state.players[0].cash == 50000 +
           (type == 1 ? 3 * result_amount : type == 2 ? result_amount :
            type == 5 ? -3 * result_amount : -result_amount));
    printf("SDL god %d: FLC=%08x spin=%08x result=%08x amount=%d\n",
           type, first_hash, spin_hash, result_hash, result_amount);
}

static void test_nested_bankruptcies(void)
{
    GameGodAcquisitionPresentationState *god;
    Rich4AppGameUpdateServices services;
    GameEvent auction;
    Uint32 now = 1100;
    int i;
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 4;
    session.state.human_player_count = 4;
    for (i = 0; i < 4; i++) {
        session.state.players[i].play_state = 1;
        session.state.players[i].cash = i == 1 || i == 2 ? 0 : 50000;
        session.state.players[i].linked_player = -1;
        session.state.players[i].attached_timed_bomb_slot = -1;
    }
    session.state.players[0].god_info = 1;
    session.state.players[0].god_type = 1;
    god = &session.presentation.god_acquisition;
    god->active = god->money_pending = god->money.active = 1;
    god->god_type = god->object_slot = 1;
    god->money.phase = 10;
    god->money.amount = 600;
    assert(game_update_god_presentation(&session, &assets, NULL, 100, 1));
    assert(session.presentation.bankruptcy.active &&
           session.presentation.bankruptcy.player == 1 &&
           session.presentation.bankruptcy.previous_current_player == 0);
    assert(session.state.players[2].play_state == 1 && god->active);
    assert(!rich4_presentation_god_owns_frame(&session.presentation));
    assert(rich4_presentation_update_step(&session.presentation, &session.state) !=
           RICH4_PRESENTATION_UPDATE_GOD);
    /* Nested bankruptcy owns the frame until its existing flow completes. */
    assert(!game_update_god_presentation(&session, &assets, NULL, 1000, 1));
    assert(session.state.players[2].play_state == 1);
    memset(&services, 0, sizeof(services));
    session.presentation.bankruptcy.active = 0;
    session.presentation.bankruptcy.awaiting_auction = 1;
    assert(rich4_app_game_update(&session, 1001, &services));
    assert(!rich4_presentation_god_owns_frame(&session.presentation));
    /* Released bankruptcy auctions must run without a route executor. */
    session.state.price_index = 1;
    session.state.land_count = 1;
    session.state.land_owner[0] = -1;
    session.state.land_price[0] = session.state.land_value[0] = 1000;
    session.state.land_price_per_level[0] = 500;
    auction = rich4_game_event_auction_request(1,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), -1, 1,
        GAME_AUCTION_TRIGGER_BANKRUPTCY, GAME_AUCTION_CLEAR_OWNER_IF_UNSOLD);
    assert(rich4_game_event_queue_push(&session.deferred_events, &auction));
    assert(game_update_presentation_dispatch(
        &session, &assets, NULL, NULL, NULL, now, 0));
    assert(session.presentation.auction.active && !session.deferred_events.count);
    for (i = 0; i < 40 && session.presentation.auction.active; i++) {
        now += 1000;
        if (rich4_auction_flow_is_human_turn(&session))
            assert(rich4_auction_flow_submit(&session, GAME_AUCTION_GIVE_UP,
                now, &session.audio_events) != GAME_AUCTION_STEP_INVALID);
        (void)game_update_presentation_dispatch(
            &session, &assets, NULL, NULL, NULL, now, 0);
        assert(session.state.players[2].play_state == 1);
    }
    assert(i < 40 && !session.presentation.bankruptcy.awaiting_auction);
    if (session.presentation.bankruptcy.active) {
        assert(session.presentation.bankruptcy.phase == RICH4_BANKRUPTCY_PHASE_SPEECH);
        session.presentation.speech.active = 0;
        rich4_bankruptcy_finish_speech(&session);
    }
    assert(session.state.current_player == 0);
    assert(game_update_god_presentation(&session, &assets, NULL, now + 100, 1));
    assert(session.presentation.bankruptcy.player == 2 &&
           session.presentation.bankruptcy.previous_current_player == 0);
    session.presentation.bankruptcy.active = 0;
    session.state.current_player = session.presentation.bankruptcy.previous_current_player;
    assert(!game_update_god_presentation(&session, &assets, NULL, now + 1100, 1));
    assert(session.state.current_player == 0 && !god->active);
    assert(session.state.players[0].cash == 50600 &&
           session.state.players[3].cash == 49400);
}

/* The original advances the data before its popup, but preserves the old
 * background until 0x40F506. Exercise the real SDL -> view-model adapter. */
static void test_angel_background(void)
{
    GameViewModelInput input;
    static GameViewModel model;
    int ai, facility, old_level, skip;
    for (ai = 0; ai < 2; ai++) for (facility = 0; facility < 2; facility++)
    for (old_level = 0; old_level <= 4; old_level++) for (skip = 0; skip < 2; skip++) {
        int *level;
        uint32_t finish = skip ? 200 : 1600;
        /* Empty facilities choose their type after the notice, separately. */
        if (facility && old_level == 0) continue;
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 1;
        session.state.price_index = 1;
        session.state.players[0].play_state = ai ? 2 : 1;
        session.state.players[0].is_ai = ai;
        session.state.players[0].god_info = session.state.players[0].god_type = 9;
        session.state.players[0].node = 1;
        session.state.route_object_raw[1] = facility ? 4001 : 2001;
        session.state.land_count = session.state.facility_count = 1;
        session.state.land_owner[0] = session.state.facility_owner[0] = -1;
        session.state.land_price[0] = session.state.facility_price[0] = 100;
        session.state.land_price_per_level[0] = session.state.facility_price_per_level[0] = 10;
        session.state.facility_type[0] = 1;
        level = facility ? &session.state.facility_level[0] : &session.state.land_level[0];
        *level = old_level;
        assert(rich4_property_begin_god_landing(&session, 0, 1, 100));
        assert(*level == old_level + 1);
        rich4_sdl1_game_view_model_input_build(&session.flow, &session.presentation, &input);
        assert(rich4_game_view_model_build(&session.state, &input, NULL, &model));
        assert((facility ? model.facilities[0].level : model.lands[0].level) == old_level);
        assert(!session.presentation.property.sound_pending);
        assert(rich4_property_update_charge_presentation(&session, finish - 1));
        if (skip) assert(rich4_property_skip_wait(&session, finish));
        (void)rich4_property_update_charge_presentation(&session, finish);
        rich4_sdl1_game_view_model_input_build(&session.flow, &session.presentation, &input);
        assert(rich4_game_view_model_build(&session.state, &input, NULL, &model));
        assert((facility ? model.facilities[0].level : model.lands[0].level) == old_level + 1);
        assert(*level == old_level + 1 && session.presentation.property.sound_pending == 2);
        assert(!session.presentation.speech.active);
        assert(session.deferred_events.count == (old_level == 4));
    }
}

static void test_demon_background(void)
{
    GameViewModelInput input;
    static GameViewModel model;
    int facility, special, frame;
    for (facility = 0; facility < 2; facility++) for (special = 0; special < 2; special++) {
        GameEvent e;
        memset(&session, 0, sizeof(session));
        session.state.active = 1; session.state.player_count = 1;
        session.state.players[0].play_state = 2;
        session.state.players[0].is_ai = 1;
        session.state.players[0].god_info = session.state.players[0].god_type = 10;
        session.state.price_index = 1;
        session.state.route_object_raw[1] = facility ? 4001 : 2001;
        session.state.land_count = session.state.facility_count = 1;
        session.state.land_owner[0] = session.state.facility_owner[0] = -1;
        session.state.land_level[0] = session.state.facility_level[0] = 1;
        session.state.land_type[0] = session.state.facility_type[0] = special;
        assert(rich4_property_begin_god_landing(&session, 0, 1, 100));
        (void)rich4_property_update_charge_presentation(&session, 1600);
        assert((facility ? session.state.facility_level[0] : session.state.land_level[0]) == 0);
        rich4_sdl1_game_view_model_input_build(&session.flow, &session.presentation, &input);
        assert(rich4_game_view_model_build(&session.state, &input, NULL, &model));
        assert((facility ? model.facilities[0].level : model.lands[0].level) == 1);
        assert(rich4_game_event_queue_pop(&session.deferred_events, &e));
        game_tool_animation_enqueue_from_platform(&session, &assets, &e, 1600);
        assert(session.presentation.tool_animation_count == 1);
        for (frame = 0; frame < 4; frame++) {
            session.presentation.tool_animation[0].frame = frame;
            rich4_sdl1_game_view_model_input_build(&session.flow, &session.presentation, &input);
            assert(rich4_game_view_model_build(&session.state, &input, NULL, &model));
            assert((facility ? model.facilities[0].level : model.lands[0].level) == (frame < 2));
            if (frame < 2) assert((facility ? model.facilities[0].type : model.lands[0].type) == special);
        }
        session.presentation.tool_animation_count = 0;
        rich4_sdl1_game_view_model_input_build(&session.flow, &session.presentation, &input);
        assert(input.demon_previous_building_target.kind == GAME_DOMAIN_NONE);
    }
}

static void test_card_result_background(SDL_Surface *screen, Rich4Mkf *data)
{
    int type;
    for (type = 3; type <= 8; type++) {
        Rich4GameEventPresentationSnapshot snapshot;
        uint32_t with_animation, without_animation;
        if (type == 5 || type == 6) continue;
        memset(&session, 0, sizeof(session));
        session.presentation.god_acquisition.active = 1;
        session.presentation.god_acquisition.god_type = type;
        session.presentation.god_acquisition.result_card_1 = 0;
        session.presentation.god_acquisition.result_card_2 = 1;
        session.presentation.god_acquisition.result_lost_count = 2;
        session.presentation.god_acquisition.message_visible = RICH4_GOD_MESSAGE_RESULT;
        assert(rich4_mkf_read_resource(data, 539 + type, &assets.ui.god_effect_data[type],
            &assets.ui.god_effect_size[type], NULL) == RICH4_MKF_OK);
        session.presentation.god_acquisition.frame = game_graph_frame_count_data(
            assets.ui.god_effect_data[type], assets.ui.god_effect_size[type]) - 1;
        snapshot = rich4_app_build_game_event_presentation_snapshot(&session.state, &session.presentation);
        assets.ui.has_god_effect[type] = 1;
        SDL_FillRect(screen, NULL, 0x1234);
        draw_god_presentation(screen, &assets, &snapshot);
        with_animation = pixels_hash(screen);
        assets.ui.has_god_effect[type] = 0;
        SDL_FillRect(screen, NULL, 0x1234);
        draw_god_presentation(screen, &assets, &snapshot);
        without_animation = pixels_hash(screen);
        assert(with_animation == without_animation);
        free(assets.ui.god_effect_data[type]);
        assets.ui.god_effect_data[type] = NULL;
    }
}

/* Resource probe: determine whether Data[551] hides a prematurely changed
 * actor. Compare real character-0 walking/car sprites beneath every frame. */
static void inspect_death_actor_visibility(SDL_Surface *screen, Rich4Mkf *data)
{
    unsigned char *flc, *sprites[2];
    size_t flc_size, sizes[2];
    Rich4SprSmpInfo spr[2];
    int i, frame, count, different = 0, first = -1;
    assert(rich4_mkf_read_resource(data, 551, &flc, &flc_size, NULL) == RICH4_MKF_OK);
    for (i = 0; i < 2; i++) {
        assert(rich4_mkf_read_resource(data, i ? 134 : 128,
            &sprites[i], &sizes[i], NULL) == RICH4_MKF_OK);
        assert(rich4_spr_smp_probe(sprites[i], sizes[i], &spr[i]));
    }
    count = game_graph_frame_count_data(flc, flc_size);
    for (frame = 0; frame < count; frame++) {
        uint32_t hashes[2];
        for (i = 0; i < 2; i++) {
            SDL_FillRect(screen, NULL, 0x1234);
            draw_spr_smp_center(screen, sprites[i], sizes[i], &spr[i], 0, 220, 260);
            assert(draw_game_graph_frame_data(screen, flc, flc_size, frame, 0, 40));
            hashes[i] = pixels_hash(screen);
        }
        if (hashes[0] != hashes[1]) {
            if (first < 0) first = frame;
            different++;
        }
    }
    printf("Death Data[551] actor background: %d/%d frames distinguish car from walking; first=%d (zero-based)\n",
        different, count, first);
    {
        GameEventQueue events;
        GameEvent e;
        GameViewModelInput input;
        static GameViewModel view;
        int before;
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 1;
        session.state.players[0].play_state = 1;
        session.state.players[0].traffic_method = 2;
        session.state.players[0].ndices = 3;
        session.state.players[0].node = 1;
        rich4_game_event_queue_reset(&events);
        assert(rich4_god_attach(&session.state, 0, 1, 1, &events) == GAME_GOD_APPLIED);
        session.presentation.dynamic_objects[0].type = 1;
        session.presentation.dynamic_objects[0].attached_player = 1;
        session.presentation.dynamic_objects[0].node = 1;
        rich4_sdl1_game_view_model_input_build(&session.flow, &session.presentation, &input);
        assert(rich4_game_view_model_build(&session.state, &input, NULL, &view));
        before = view.actors[0].primary_part;
        rich4_game_event_queue_reset(&events);
        assert(rich4_god_attach_replace(&session.state, 0, 15, 15, &events) == GAME_GOD_APPLIED);
        assert(rich4_app_begin_dynamic_god_detach(&session.presentation, &session.state, 0, 100));
        while (rich4_game_event_queue_pop(&events, &e))
            if (e.type == GAME_EVENT_GOD_ACQUIRED)
                game_begin_god_presentation(&session, &assets, &e, 100);
        rich4_sdl1_game_view_model_input_build(&session.flow, &session.presentation, &input);
        assert(rich4_game_view_model_build(&session.state, &input, NULL, &view));
        assert(before == view.actors[0].primary_part);
        assert(session.state.players[0].traffic_method == 2);
        printf("Death replacement probe: waiting_for_detach=%d actor part %d -> %d traffic=%d\n",
            session.presentation.god_acquisition.waiting_for_detach, before,
            view.actors[0].primary_part, session.state.players[0].traffic_method);
    }
    free(flc);
    for (i = 0; i < 2; i++) free(sprites[i]);
}

static void test_departure_geometry(Rich4Mkf *data)
{
    NewGameSettings settings;
    GameViewModelInput input;
    GameViewportModel viewport;
    GameGodDetachmentPresentationState *d;
    Rich4SprSmpChunk chunk;
    MapData map;
    MapRouteNode routes[3];
    static GameViewModel view;
    int direction, rotation, moving, shortened = 0, x, y;
    memset(&map, 0, sizeof(map));
    memset(routes, 0, sizeof(routes));
    routes[1].x = 1600; routes[1].y = 960;
    routes[2].x = 320; routes[2].y = 160;
    map.routes = routes;
    map.route_count = 3;
    memset(&settings, 0, sizeof(settings));
    assert(rich4_mkf_read_resource(data, 400, &assets.map.data_map_status_data[4],
        &assets.map.data_map_status_size[4], NULL) == RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(assets.map.data_map_status_data[4],
        assets.map.data_map_status_size[4], &assets.map.data_map_status_spr[4]));
    assets.map.has_data_map_status[4] = 1;
    for (rotation = 0; rotation < 8; rotation++)
    for (moving = 0; moving < 2; moving++)
    for (direction = 0; direction < 8; direction++) {
        int count, expected_x = moving ? 336 : 320, expected_y = moving ? 176 : 160;
        GameMapDynamicObjectState attached;
        MapRenderObjectInput attached_input;
        GameVisibleMapObject attached_visible;
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 2;
        session.state.players[0].play_state = session.state.players[1].play_state = 1;
        session.state.players[0].node = 1;
        session.state.players[1].node = 2;
        session.state.tool_route_node_count = 3;
        session.state.tool_route_x[1] = 1600; session.state.tool_route_y[1] = 960;
        session.state.tool_route_x[2] = 320; session.state.tool_route_y[2] = 160;
        session.presentation.actors[1].direction = (direction + rotation + 4) & 7;
        session.presentation.actors[1].move_active = moving;
        session.presentation.actors[1].visual_x = moving ? 336 : 1600;
        session.presentation.actors[1].visual_y = moving ? 176 : 960;
        session.presentation.map_direction = rotation;
        session.presentation.has_view_override = 1;
        session.presentation.view_world_x = 992;
        session.presentation.view_world_y = 512;
        memset(&attached, 0, sizeof(attached));
        attached.type = 5;
        attached.node = 1; /* Acquisition node is intentionally stale. */
        attached.attached_player = 2;
        session.presentation.dynamic_objects[0] = attached;
        assert(rich4_app_begin_dynamic_god_detach(
            &session.presentation, &session.state, 1, 100));
        d = &session.presentation.god_detachment;
        assert(d->world_x == expected_x && d->world_y == expected_y);
        assert(d->direction == direction && session.presentation.map_direction == rotation);
        rich4_sdl1_game_view_model_input_build(&session.flow, &session.presentation, &input);
        assert(input.has_view_override && input.view_world_x == expected_x && input.view_world_y == expected_y);
        assert(rich4_game_view_model_build(&session.state, &input, &map, &view));
        viewport = view.viewport;
        assert(viewport.direction == rotation);
        assert(viewport.center_world_x == expected_x && viewport.center_world_y == expected_y);
        rich4_map_god_detachment_anchor(&viewport, d->world_x, d->world_y, direction, &x, &y);
        /* The ascent must start at the same anchor/chunk as the ordinary
         * attached god behind the current actor, after camera recentering. */
        assert(rich4_map_dynamic_build_input(&attached, 0, &viewport,
            view.actors, 2, routes, 3, NULL, &attached_input));
        assert(rich4_map_collect_visible(&attached_input, 1, &viewport,
            &attached_visible, 1) == 1);
        assert(x == attached_visible.x && y == attached_visible.y);
        assert(attached_input.variant == d->direction);
        if (direction == 4) assert(x == 210 && y == 238);
        game_limit_god_detachment(&session, &assets, &settings);
        count = d->frame_count;
        assert(count > 0 && count <= 24);
        assert(rich4_spr_smp_get_chunk(assets.map.data_map_status_data[4],
            assets.map.data_map_status_size[4], &assets.map.data_map_status_spr[4],
            (direction + count) & 7, &chunk) == RICH4_SPR_SMP_OK);
        assert(y - count * 10 - chunk.y + chunk.height >= 40);
        if (count < 24) {
            shortened++;
            assert(rich4_spr_smp_get_chunk(assets.map.data_map_status_data[4],
                assets.map.data_map_status_size[4], &assets.map.data_map_status_spr[4],
                (direction + count + 1) & 7, &chunk) == RICH4_SPR_SMP_OK);
            assert(y - (count + 1) * 10 - chunk.y + chunk.height < 40);
        }
        assert(game_update_god_detachment(&session, NULL, 100 + count * 60 - 1));
        assert(d->active && !session.presentation.speech.active);
        assert(game_update_god_detachment(&session, NULL, 100 + count * 60));
        assert(!d->active && session.presentation.speech.active);
    }
    assert(shortened > 0);
    free(assets.map.data_map_status_data[4]);
    assets.map.data_map_status_data[4] = NULL;
    assets.map.has_data_map_status[4] = 0;
}

static void test_bad_god_arrival(void)
{
    int type, character, enabled;
    for (type = 1; type <= 15; type++) {
        if (!rich4_god_is_road_type(type)) continue;
        for (character = 0; character < 12; character++)
        for (enabled = 0; enabled <= 1; enabled++) {
            GameEventQueue events;
            GameEvent event;
            GameGodAcquisitionPresentationState *god;
            int negative = type == 5 || type == 6 || type == 7 || type == 8 || type == 15;
            memset(&session, 0, sizeof(session));
            session.state.active = 1;
            session.state.player_count = 1;
            session.state.players[0].play_state = 1;
            session.state.players[0].character = character;
            rich4_game_event_queue_reset(&events);
            assert(rich4_god_attach(&session.state, 0, type, type, &events) == GAME_GOD_APPLIED);
            while (rich4_game_event_queue_pop(&events, &event))
                if (event.type == GAME_EVENT_GOD_ACQUIRED)
                    game_begin_god_presentation(&session, &assets, &event, 100);
            god = &session.presentation.god_acquisition;
            assert(god->arrival_speech_phase == negative);
            (void)game_update_god_presentation(&session, &assets, NULL, 100, enabled);
            assert(session.presentation.speech.active == negative);
            if (negative) {
                assert(session.presentation.speech.current->speaking_resource == 1072 + 27 * character);
                assert(session.presentation.speech.type == 2 && !god->sound_pending);
                rich4_speech_request_skip(&session);
                (void)game_update_property_speech(&session, NULL, 200);
                (void)game_update_god_presentation(&session, &assets, NULL, 200, enabled);
                assert(!god->arrival_speech_phase);
                if (enabled) assert(god->started == 200 && god->sound_pending);
            }
            if (!enabled) assert(!god->sound_pending && god->frame_count == 0);
        }
    }
}

static void test_death_timing(Rich4Mkf *data)
{
    int ai, skip, old;
    assert(rich4_mkf_read_resource(data, 551, &assets.ui.god_effect_data[15],
        &assets.ui.god_effect_size[15], NULL) == RICH4_MKF_OK);
    assets.ui.has_god_effect[15] = 1;
    for (ai = 0; ai < 2; ai++) for (skip = 0; skip < 4; skip++)
    for (old = 0; old < 3; old++) {
        GameEventQueue events;
        GameEvent event;
        GameGodAcquisitionPresentationState *god;
        Uint32 now = 100, end;
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 1;
        session.state.players[0].play_state = ai ? 2 : 1;
        session.state.players[0].is_ai = ai;
        session.state.players[0].traffic_method = 2;
        session.state.players[0].ndices = 3;
        session.state.players[0].tool_inventory[0] = 2;
        session.state.players[0].card_inventory[0] = 1;
        rich4_game_event_queue_reset(&events);
        if (old) {
            int type = old == 1 ? 1 : 5;
            assert(rich4_god_attach(&session.state, 0, type, type, &events) == GAME_GOD_APPLIED);
            rich4_game_event_queue_reset(&events);
            session.presentation.god_detachment.active = 1;
            session.presentation.god_detachment.started = 1;
            session.presentation.god_detachment.frame_tick = now;
            session.presentation.god_detachment.frame_count = 24;
            session.presentation.god_detachment.frame_delay_ms = 60;
            session.presentation.god_detachment.god_type = type;
        }
        assert(rich4_god_attach_replace(&session.state, 0, 15, 15, &events) == GAME_GOD_APPLIED);
        while (rich4_game_event_queue_pop(&events, &event))
            if (event.type == GAME_EVENT_GOD_ACQUIRED)
                game_begin_god_presentation(&session, &assets, &event, now);
        god = &session.presentation.god_acquisition;
        assert(god->death_pending && !god->sound_pending);
        if (old) {
            assert(game_update_god_presentation(&session, &assets, NULL, now + 1439, skip != 3));
            assert(session.state.players[0].traffic_method == 2 && god->arrival_speech_phase == 1);
            now += 1440;
            (void)game_update_god_presentation(&session, &assets, NULL, now, skip != 3);
            if (old == 2) {
                assert(session.presentation.god_detachment.speech_pending);
                assert(session.presentation.speech.current->speaking_resource == 1073);
                assert(session.state.players[0].traffic_method == 2);
                rich4_speech_request_skip(&session);
                (void)game_update_god_detachment(&session, NULL, ++now);
            }
        }
        if (!session.presentation.speech.active)
            assert(game_update_god_presentation(&session, &assets, NULL, now, skip != 3));
        assert(session.presentation.speech.active);
        assert(session.presentation.speech.type == 2);
        assert(session.presentation.speech.current->speaking_resource == 1072);
        assert(!rich4_presentation_god_owns_frame(&session.presentation));
        rich4_speech_request_skip(&session);
        now += 100;
        (void)game_update_property_speech(&session, NULL, now);
        assert(!session.presentation.speech.active);
        if (skip == 3) {
            assert(!game_update_god_presentation(&session, &assets, NULL, now, 0));
            assert(!god->active && !god->death_pending && !god->sound_pending);
            assert(session.state.players[0].traffic_method == 0);
            assert(session.state.players[0].card_inventory[0] == 0);
            continue;
        }
        assert(game_update_god_presentation(&session, &assets, NULL, now, 1));
        assert(!god->arrival_speech_phase && god->sound_pending);
        assert(god->frame_count > 0 && god->started == now);
        assert(session.state.players[0].traffic_method == 2);
        assert(game_update_god_presentation(&session, &assets, NULL, god->animation_until, 1));
        assert(god->message_visible == RICH4_GOD_MESSAGE_MAIN);
        assert(god->until == god->animation_until + 2400);
        assert(game_update_god_presentation(&session, &assets, NULL, god->until - 1, 1));
        assert(session.state.players[0].traffic_method == 2);
        assert(session.state.players[0].card_inventory[0] == 1);
        assert(session.state.players[0].tool_inventory[0] == 2);
        if (skip == 1) {
            GameKeyUpModalsContext context;
            memset(&context, 0, sizeof(context));
            context.app_state = RICH4_SCENE_GAME;
            context.session = &session;
            game_handle_key_up_modals(&context);
        } else if (skip == 2) {
            game_handle_mouse_button_up_modals(RICH4_SCENE_GAME, &session, NULL, SDL_BUTTON_RIGHT);
        }
        end = god->until;
        assert(!game_update_god_presentation(&session, &assets, NULL, end, 1));
        assert(!god->active && !god->death_pending);
        assert(session.state.players[0].traffic_method == 0);
        assert(session.state.players[0].ndices == 1);
        assert(session.state.players[0].card_inventory[0] == 0);
        assert(session.state.players[0].tool_inventory[0] == 0);
        assert(session.state.tool_amount[0] == 2 && session.state.tool_amount[5] == 1);
        assert(session.state.card_amount[0] == 1);
        session.state.players[0].card_inventory[0] = 2;
        assert(!game_update_god_presentation(&session, &assets, NULL, end + 1, 1));
        assert(session.state.players[0].card_inventory[0] == 2);
    }
    free(assets.ui.god_effect_data[15]);
    assets.ui.god_effect_data[15] = NULL;
    assets.ui.god_effect_size[15] = 0;
    assets.ui.has_god_effect[15] = 0;
}

static void test_resident_sdl(SDL_Surface *screen, Rich4Mkf *data)
{
    GameSceneView scene;
    int type;
    uint32_t hashes[16] = {0};
    memset(&scene, 0, sizeof(scene));
    scene.property_company_type = -1;
    for (type = 1; type <= 12; type++) {
        if (type != 1 && type != 2 && type != 5 && type != 6 &&
            type != 9 && type != 10 && type != 12) continue;
        scene.property_god_type = type;
        scene.property_presentation_phase = type < 9 ?
            GAME_PROPERTY_PRESENT_GOD_FEE : GAME_PROPERTY_PRESENT_GOD_BUILD_TEXT;
        SDL_FillRect(screen, NULL, 0);
        draw_game_property_presentation(screen, &assets, &scene);
        hashes[type] = pixels_hash(screen);
    }
    assert(hashes[1] != hashes[2] && hashes[5] != hashes[6]);
    assert(hashes[9] != hashes[10] && hashes[10] != hashes[12]);
    for (type = 1; type <= 15; type++) {
        memset(&session, 0, sizeof(session));
        session.state.player_count = 1;
        session.state.players[0].play_state = 1;
        session.presentation.god_detachment.active = 1;
        session.presentation.god_detachment.started = 1;
        session.presentation.god_detachment.frame_tick = 100;
        session.presentation.god_detachment.frame_count = 24;
        session.presentation.god_detachment.frame_delay_ms = 60;
        session.presentation.god_detachment.god_type = type;
        assert(game_update_god_detachment(&session, NULL, 1539));
        (void)game_update_god_detachment(&session, NULL, 1540);
        assert(!session.presentation.god_detachment.active);
        assert(session.presentation.speech.active ==
            (type == 5 || type == 6 || type == 7 || type == 8 || type == 15));
        if (session.presentation.speech.active) {
            assert(session.presentation.speech.type == 2);
            assert(session.presentation.speech.current->speaking_resource == 1073);
            (void)game_update_god_detachment(&session, NULL, 1541);
            (void)game_update_god_detachment(&session, NULL, 4000);
            assert(!session.presentation.god_detachment.speech_pending);
        }
    }
    assert(rich4_mkf_read_resource(data, 526, &assets.ui.point_minigame_effect_data,
        &assets.ui.point_minigame_effect_size, NULL) == RICH4_MKF_OK);
    assets.ui.has_point_minigame_effect = 1;
    {
        GameEventQueue q;
        GameEvent e;
        memset(&session, 0, sizeof(session));
        session.state.land_count = 1;
        session.state.tool_land_x[0] = 100;
        session.state.tool_land_y[0] = 200;
        rich4_game_event_queue_reset(&q);
        assert(rich4_game_event_emit_typed_domain(&q,
            rich4_game_event_tool_presentation_requested_from_fields, 0,
            rich4_game_domain_ref(GAME_DOMAIN_GOD, 9),
            rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), 526, 95, 0));
        assert(rich4_game_event_queue_pop(&q, &e));
        game_tool_animation_enqueue_from_platform(&session, &assets, &e, 100);
        assert(session.presentation.tool_animation_count == 1);
        assert(session.presentation.tool_animation[0].world_x == 100);
        assert(session.presentation.tool_animation[0].effect == 95);
        assert(!session.presentation.has_view_override);
    }
    test_demon_background();
    free(assets.ui.point_minigame_effect_data);
    assets.ui.point_minigame_effect_data = NULL;
    assets.ui.has_point_minigame_effect = 0;
}

int main(void)
{
    Rich4Mkf *data = NULL, *panel = NULL;
    ResourceStore *store;
    const char *const panel_names[] = {"Panel.mkf"};
    const char *const data_names[] = {"Data.mkf"};
    char path[512];
    ResourceCatalogEntry dialog;
    SDL_Surface *screen;
    const int types[] = {1, 2, 5, 6};
    int i;
    SDL_putenv("SDL_VIDEODRIVER=dummy");
    SDL_putenv("SDL_AUDIODRIVER=dummy");
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    screen = SDL_SetVideoMode(640, 480, 16, SDL_SWSURFACE);
    assert(screen != NULL);
    rich4_sdl_text_set_font_path("assets/fonts/rich4.ttc");
    store = rich4_resource_store_create();
    assert(store != NULL);
    panel = game_open_named_mkf_from_data_dir(store, "assets/original",
        panel_names, 1, path, sizeof(path));
    data = game_open_named_mkf_from_data_dir(store, "assets/original",
        data_names, 1, path, sizeof(path));
    assert(panel != NULL && data != NULL);
    assert(rich4_resource_catalog_data_common_dialog(&dialog));
    assert(rich4_mkf_read_resource(data, dialog.resource_index,
        &assets.ui.common_dialog_data, &assets.ui.common_dialog_size, NULL) == RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(assets.ui.common_dialog_data,
        assets.ui.common_dialog_size, &assets.ui.common_dialog_spr));
    assets.ui.has_common_dialog = 1;
    assert(rich4_mkf_read_resource(panel, 67, &assets.ui.panel_data[67],
                                   &assets.ui.panel_size[67], NULL) == RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(assets.ui.panel_data[67], assets.ui.panel_size[67],
                               &assets.ui.panel_spr[67]));
    assert(assets.ui.panel_spr[67].num_chunks == 24);
    assets.ui.has_panel[67] = 1;
    for (i = 0; i < 4; i++) {
        int type = types[i];
        assert(rich4_mkf_read_resource(data, (size_t)(539 + type),
                                       &assets.ui.god_effect_data[type],
                                       &assets.ui.god_effect_size[type], NULL) == RICH4_MKF_OK);
        assets.ui.has_god_effect[type] = 1;
        run_god(screen, type);
        free(assets.ui.god_effect_data[type]);
        assets.ui.god_effect_data[type] = NULL;
        assets.ui.has_god_effect[type] = 0;
    }
    inspect_death_actor_visibility(screen, data);
    test_departure_geometry(data);
    test_bad_god_arrival();
    test_death_timing(data);
    test_card_result_background(screen, data);
    test_angel_background();
    test_resident_sdl(screen, data);
    test_nested_bankruptcies();
    free(assets.ui.panel_data[67]);
    free(assets.ui.common_dialog_data);
    rich4_resource_store_destroy(store);
    rich4_sdl_text_shutdown();
    SDL_Quit();
    puts("SDL original god FLC -> number wheel -> settlement: ok");
    return 0;
}
