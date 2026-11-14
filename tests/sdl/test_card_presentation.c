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
#include "app/card_defense_flow.h"
#include "platform/sdl1/game_loop_confirm_company_issue.h"
#include "app/card_panel_flow.h"
#include "app/game_speech_flow.h"
#include "app/special_place_notice_flow.h"
#include "core/game_event_payload.h"
#include "core/character_speech_rules.h"
#include "view/map_renderer.h"
#include "resource/mkf.h"
#include "resource/flc_animation.h"

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
    ai_waits_before_applying(&assets);
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
