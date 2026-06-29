#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#undef main
#include "app/map_input_flow.h"
#include "core/character_template.h"
#include "platform/sdl1/game_loop_preview_toolbar_input.h"
#include "platform/sdl1/game_loop_key_down_input.h"
#include "platform/sdl1/game_loop_mouse_button_up_input.h"
#include "platform/sdl1/render/game_hud_render.h"
#include "platform/sdl1/render/game_minimap_render.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/text/runtime_text.h"
#include "view/map_renderer.h"

static unsigned char *resource(Rich4Mkf *mkf, int index, size_t *size,
                               Rich4SprSmpInfo *info)
{
    unsigned char *data = NULL;
    assert(rich4_mkf_read_resource(mkf, index, &data, size, NULL) == RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(data, *size, info));
    return data;
}

static uint16_t pixel(SDL_Surface *screen, int x, int y)
{
    return ((uint16_t *)((unsigned char *)screen->pixels + y * screen->pitch))[x];
}

static uint16_t source_pixel(SDL_Surface *screen, const unsigned char *data,
                              const Rich4SprSmpChunk *chunk, int x, int y)
{
    size_t at = chunk->data_offset + 2 * (y * chunk->width + x);
    return map_resource_pixel(screen, data[at] | data[at + 1] << 8);
}

static void check_inputs(const GamePreviewAssets *assets)
{
    static GameSession s;
    Rich4Config cfg, restored;
    VirtualCursor cursor = {0};
    HelpPanelState help = {0};
    SmallMapPanelState small = {0};
    NewGameSettings settings = {0};
    GamePreviewToolbarMouseDownContext down = {0};
    GamePreviewToolbarConfirmContext confirm = {0};
    GameKeyDownInputContext key = {0};
    GameMouseButtonUpInputContext up = {0};
    rich4_config_read(&cfg, NULL);
    s.state.active = 1;
    s.state.player_count = 1;
    cfg.view = 2;
    down.config = &cfg; down.session = &s; down.cursor = &cursor;
    cursor.x = 630; cursor.y = 100;
    assert(!game_handle_preview_toolbar_mouse_down(&down));
    assert(s.presentation.active_info_tab == 0); /* does not cover combined minimap */
    cursor.x = 478; cursor.y = 288;
    assert(game_handle_preview_toolbar_mouse_down(&down));
    assert(cfg.calendar_view == 1);
    assert(rich4_app_calendar_mode_at(504,314,2) == 1);
    assert(rich4_app_calendar_mode_at(505,314,2) == -1);
    assert(rich4_app_calendar_mode_at(478,288,1) == -1);
    confirm.config = &cfg; confirm.session = &s; confirm.cursor = &cursor;
    cursor.x = 448; cursor.y = 314;
    assert(game_handle_preview_toolbar_confirm(&confirm));
    assert(cfg.calendar_view == 0);
    key.app_state = RICH4_SCENE_GAME; key.session = &s; key.config = &cfg;
    key.cursor = &cursor; key.help_panel = &help; key.small_map_panel = &small;
    key.settings = &settings;
    key.key = RICH4_APP_KEY_TAB;
    assert(game_handle_key_down_input(&key) && cfg.view == 0);
    assert(game_handle_key_down_input(&key) && cfg.view == 1);
    assert(game_handle_key_down_input(&key) && cfg.view == 2);
    key.key = RICH4_APP_KEY_PAGEDOWN;
    assert(game_handle_key_down_input(&key) && s.presentation.active_info_tab == 0);
    cfg.view = 0;
    assert(game_handle_key_down_input(&key) && s.presentation.active_info_tab == 1);
    s.state.current_player=1;
    assert(rich4_app_current_info_tab(&s)==0);
    rich4_app_switch_info_tab(&s,3);
    s.state.current_player=0;
    assert(rich4_app_current_info_tab(&s)==1);
    cfg.view = 2; cursor.x = 443; cursor.y = 108;
    assert(!rich4_app_begin_minimap_drag(&s,440,180,2));
    assert(!rich4_app_begin_minimap_drag(&s,540,80,2));
    assert(game_handle_preview_minimap_rotate_mouse_down(&down));
    assert(s.presentation.map_direction == 0 && s.presentation.minimap_rotate_pressed == 1);
    up.app_state = RICH4_SCENE_GAME; up.session = &s; up.cursor = &cursor;
    up.button = SDL_BUTTON_LEFT;
    assert(game_handle_mouse_button_up_input(&up));
    assert(s.presentation.map_direction == 7 && s.presentation.minimap_rotate_pressed == 0);
    assert(!rich4_app_minimap_rotate_button_at(443,109,2));
    rich4_app_set_view_from_minimap(&s, 540, 180, 2);
    assert(s.presentation.view_world_x == 1162 && s.presentation.view_world_y == 1162);
    assert(!rich4_app_restore_minimap_follow(&s,540,280,2));
    s.presentation.route_interaction.active=1;
    assert(!rich4_app_restore_minimap_follow(&s,540,180,2));
    s.presentation.route_interaction.active=0;
    assert(rich4_app_restore_minimap_follow(&s,540,180,2));
    assert(!s.presentation.has_view_override);
    cfg.calendar_view = 1;
    rich4_config_write(&cfg, "build/hud-layout.cfg");
    rich4_config_read(&restored, "build/hud-layout.cfg");
    assert(sizeof(cfg) == 72 && restored.calendar_view == 1 && restored.view == 2);
    assert(memcmp(cfg.hotkeys, restored.hotkeys, sizeof(cfg.hotkeys)) == 0);
    (void)assets;
}

int main(void)
{
    static GamePreviewAssets assets;
    static GameState state;
    GameViewModelInput input = {0};
    GameViewModel view;
    NewGameSettings settings = {0};
    Rich4Mkf *panel, *map, *data;
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_SWSURFACE,
        640,480,16,0xf800,0x07e0,0x001f,0);
    Rich4SprSmpChunk chunk;
    int i, mode;
    assert(screen);
    rich4_sdl_text_set_font_path("assets/fonts/rich4.ttc");
    assert(rich4_mkf_open("assets/original/Panel.mkf", &panel) == RICH4_MKF_OK);
    assert(rich4_mkf_open("assets/original/Map.mkf", &map) == RICH4_MKF_OK);
    assert(rich4_mkf_open("assets/original/Data.mkf", &data) == RICH4_MKF_OK);
    for (i = 0; i < 3; i++) {
        assets.ui.panel_data[i] = resource(panel,i,&assets.ui.panel_size[i],&assets.ui.panel_spr[i]);
        assets.ui.has_panel[i] = 1;
    }
    for (i = 0; i < 12; i++) {
        assets.character.map_character_data[i] = resource(map,27+i,
            &assets.character.map_character_size[i],&assets.character.map_character_spr[i]);
        assets.character.map_character_resource[i].data = assets.character.map_character_data[i];
        assets.character.map_character_resource[i].size = assets.character.map_character_size[i];
        assets.character.has_map_character[i] = 1;
    }
    assets.map.map_overview_resource[0].data = resource(map,16,
        &assets.map.map_overview_resource[0].size,&assets.map.map_overview_spr[0]);
    assets.map.has_map_overview[0] = 1;
    assets.ui.common_dialog_data = resource(data,517,
        &assets.ui.common_dialog_size,&assets.ui.common_dialog_spr);
    assets.ui.has_common_dialog = 1;
    assets.map.map_control_data = assets.ui.common_dialog_data;
    assets.map.map_control_size = assets.ui.common_dialog_size;
    assets.map.map_control_spr = assets.ui.common_dialog_spr;
    assets.map.has_map_control = 1;
    state.active = 1; state.player_count = 2; state.current_player = 0;
    state.players[0].character = 8; state.players[1].character = 9;
    state.players[0].linked_player = 1;
    state.players[0].cash = 12345; state.players[0].bank = 67890;
    state.players[0].loan = 100; state.players[0].loan_due_date = 7;
    state.calendar_start_year = 2000; state.calendar_start_month = 2;
    state.calendar_start_day = 29;
    assert(rich4_game_view_model_build(&state,&input,NULL,&view));
    assert(view.hud.ally_character == 9 && view.hud.loan_days_remaining == 7);
    assert(view.calendar.month_days == 29 && view.calendar.first_weekday == 2);
    assert(view.calendar.weekday == 2 && (view.calendar.closed_days & (1u << 5)));
    for (mode = 0; mode < 3; mode++) {
        int x,y;
        SDL_FillRect(screen,NULL,0x1234);
        draw_game_layout(screen,&assets,&view,mode);
        if (mode == 2) {
            assert(pixel(screen,600,32) == rich4_character_rgb565_color(
                rich4_character_template(8)->color));
            assert(pixel(screen,634,36) == 0);
            assert(rich4_spr_smp_get_chunk(assets.ui.panel_data[0],assets.ui.panel_size[0],
                &assets.ui.panel_spr[0],4,&chunk) == RICH4_SPR_SMP_OK);
            assert(pixel(screen,639,78) == source_pixel(screen,assets.ui.panel_data[0],&chunk,199,78));
        } else assert(pixel(screen,600,56) == rich4_character_rgb565_color(
            rich4_character_template(8)->color));
        assert(rich4_spr_smp_get_chunk(assets.character.map_character_data[8],
            assets.character.map_character_size[8],&assets.character.map_character_spr[8],0,&chunk) == RICH4_SPR_SMP_OK);
        /* Original Map[35]/chunk 0 at (443,0), rather than its bounding-box center. */
        for (y=0;y<chunk.height;y++) for (x=0;x<50;x++) {
            uint16_t value=source_pixel(screen,assets.character.map_character_data[8],&chunk,x,y);
            if (value) assert(pixel(screen,443+x,y) == value);
        }
        if (mode != 1) {
            draw_game_calendar(screen,&assets,&view,1);
            /* Feb 29, 2000: Tuesday column, fifth date row, red 20x14 outline. */
            assert(pixel(screen,506,428) == 0xf800);
            assert(pixel(screen,525,441) == 0xf800);
            assert(pixel(screen,439,428) == 0x1234);
        } else assert(pixel(screen,600,450) == 0x1234);
    }
    {
        MapLandRecord lands[2] = {{0}};
        MapFacilityRecord facilities[2] = {{0}};
        MapRouteNode routes[2] = {{0}};
        uint16_t owner_color = rich4_character_rgb565_color(
            rich4_character_template(8)->color);
        int x, y, frame;
        lands[1].x=1000; lands[1].y=1200;
        facilities[1].x=1600; facilities[1].y=1800; facilities[1].variant=1;
        routes[1].x=700; routes[1].y=900;
        assets.map.maps[0].lands=lands; assets.map.maps[0].land_count=2;
        assets.map.maps[0].facilities=facilities; assets.map.maps[0].facility_count=2;
        assets.map.maps[0].routes=routes; assets.map.maps[0].route_count=2;
        view.route_count=2; view.land_count=1; view.facility_count=1;
        view.lands[0].owner=0; view.facilities[0].owner=0;
        view.actors[0].first_turn_landing_pending=1;
        view.actors[1].first_turn_landing_pending=1;
        SDL_FillRect(screen,NULL,0x1234);
        assert(draw_game_minimap_panel(screen,&assets,&settings,&view,2));
        assert(rich4_spr_smp_get_chunk(assets.map.map_overview_resource[0].data,
            assets.map.map_overview_resource[0].size,&assets.map.map_overview_spr[0],0,&chunk)
            == RICH4_SPR_SMP_OK);
        /* Unowned roads are already in Map[16], with no extra 3x3 route dots. */
        assert(pixel(screen,500,158) == source_pixel(screen,
            assets.map.map_overview_resource[0].data,&chunk,60,78));
        for(frame=22;frame<=25;frame+=3) {
            Rich4SprSmpChunk marker;
            int center_x=frame==22 ? 526 : 579;
            int center_y=frame==22 ? 184 : 236;
            assert(rich4_spr_smp_get_chunk(assets.ui.common_dialog_data,
                assets.ui.common_dialog_size,&assets.ui.common_dialog_spr,frame,&marker)
                == RICH4_SPR_SMP_OK);
            for(y=-7;y<=7;y++) for(x=-7;x<=7;x++) {
                int sx=x+marker.x, sy=y+marker.y;
                uint16_t expected=source_pixel(screen,assets.map.map_overview_resource[0].data,
                    &chunk,center_x+x-440,center_y+y-80);
                if(sx>=0 && sx<marker.width && sy>=0 && sy<marker.height &&
                    source_pixel(screen,assets.ui.common_dialog_data,&marker,sx,sy))
                    expected=owner_color;
                assert(pixel(screen,center_x+x,center_y+y) == expected);
            }
        }
        for(mode=0;mode<3;mode++) {
            char path[80];
            SDL_FillRect(screen,NULL,0x1234);
            draw_game_minimap_panel(screen,&assets,&settings,&view,mode);
            draw_game_layout(screen,&assets,&view,mode);
            if(mode!=1) draw_game_calendar(screen,&assets,&view,mode==2);
            snprintf(path,sizeof(path),"build/hud-layout-%d.bmp",mode);
            assert(SDL_SaveBMP(screen,path)==0);
        }
    }
    check_inputs(&assets);
    /* A remains the current actor while a consequence focuses B at the
     * hospital/prison anchor. White marks A; red marks the observation. */
    view.actor_count = 2;
    view.hud.current_player = 0;
    view.current_actor = 0;
    view.actors[0].first_turn_landing_pending = 0;
    view.actors[0].minimap_world_x = 300;
    view.actors[0].minimap_world_y = 400;
    view.actors[1].first_turn_landing_pending = 0;
    view.viewport.has_override = 1;
    for (i = 0; i < 2; i++) {
        int target_x = 1400 + 200 * i;
        int target_y = 1600 + 200 * i;
        view.actors[1].minimap_world_x = target_x;
        view.actors[1].minimap_world_y = target_y;
        view.viewport.center_world_x = target_x;
        view.viewport.center_world_y = target_y;
        SDL_FillRect(screen, NULL, 0x1234);
        assert(draw_game_minimap_panel(screen, &assets, &settings, &view, 2));
        assert(pixel(screen, 440 + ((5696 * 300) >> 16) - 15,
            80 + ((5696 * 400) >> 16) - 15) == 0xffff);
        assert(pixel(screen, 440 + ((5696 * target_x) >> 16) - 15,
            80 + ((5696 * target_y) >> 16) - 15) == 0xf800);
    }
    /* The small thief/robber/hoodlum/spy card shares the baked compact labels. */
    view.hud.special_actor=4; view.hud.special_portrait_character=2;
    SDL_FillRect(screen,NULL,0x1234);
    draw_game_layout(screen,&assets,&view,0);
    assert(rich4_spr_smp_get_chunk(assets.ui.panel_data[0],assets.ui.panel_size[0],
        &assets.ui.panel_spr[0],5,&chunk)==RICH4_SPR_SMP_OK);
    assert(pixel(screen,639,260)==source_pixel(screen,assets.ui.panel_data[0],&chunk,199,260));
    draw_game_layout(screen,&assets,&view,2);
    assert(rich4_spr_smp_get_chunk(assets.ui.panel_data[0],assets.ui.panel_size[0],
        &assets.ui.panel_spr[0],4,&chunk)==RICH4_SPR_SMP_OK);
    assert(pixel(screen,639,78)==source_pixel(screen,assets.ui.panel_data[0],&chunk,199,78));
    rich4_sdl_text_shutdown(); SDL_FreeSurface(screen);
    for(i=0;i<3;i++) free(assets.ui.panel_data[i]);
    for(i=0;i<12;i++) free(assets.character.map_character_data[i]);
    free((void *)assets.map.map_overview_resource[0].data); free(assets.ui.common_dialog_data);
    rich4_mkf_close(panel); rich4_mkf_close(map); rich4_mkf_close(data);
    puts("original HUD layouts, leap-month calendar, config and input: ok");
    return 0;
}
