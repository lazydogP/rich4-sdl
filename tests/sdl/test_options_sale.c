#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include <SDL/SDL.h>
#undef main
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_options_input.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/game_loop_key_down_modals.h"
#include "app/options_game_flow.h"
#include "app/bankruptcy_flow.h"
#include "app/magic_house_flow.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/render/scene_options_render.h"
#include "platform/sdl1/render/blit.h"
#include "resource/mkf.h"

static unsigned char *resource(Rich4Mkf *mkf, int index, size_t *size,
                               Rich4SprSmpInfo *info)
{
    unsigned char *data = NULL;
    assert(rich4_mkf_read_resource(mkf, index, &data, size, NULL) == RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(data, *size, info));
    return data;
}

static void check_game_actions(const GamePreviewAssets *assets)
{
    static GameSession session;
    Rich4Config config = {0}, saved;
    Rich4SdlConfig sdl_config = {0};
    Rich4SceneMachine machine;
    Rich4SceneId scene;
    NewGameSettings settings = {0};
    VirtualCursor cursor = {0};
    Rich4OptionsPanelState *options = &session.presentation.options;
    char cwd[4096], temp[128];
    int action, left, top, width, height;
    time_t now = time(NULL);
    struct tm today = *localtime(&now);
    assert(getcwd(cwd, sizeof(cwd)) != NULL);
    snprintf(temp, sizeof(temp), "build/rich4-options-actions-%ld", (long)getpid());
#ifdef _WIN32
    assert(_mkdir(temp) == 0);
#else
    assert(mkdir(temp, 0700) == 0);
#endif
    assert(chdir(temp) == 0);
    assert(game_options_confirm_dialog_bounds(assets, &left, &top, &width, &height));
    for (action = 3; action <= 5; ++action) {
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 3;
        session.state.human_player_count = 3;
        session.state.players[0].play_state = 1;
        session.state.players[1].play_state = 1;
        session.state.players[2].play_state = 1;
        session.state.players[0].cash = 12345;
        config.year = 2000; config.month = 2; config.day = 29;
        config.sound_effect = 4;
        rich4_scene_machine_init(&machine, RICH4_SCENE_GAME);
        scene = RICH4_SCENE_GAME;
        open_options_panel(options, assets, &config, &sdl_config, RICH4_SCENE_GAME);
        rich4_scene_machine_request_transition(&machine, &scene, RICH4_SCENE_OPTIONS);
        options->draft.sound_effect = 0; /* Action buttons do not commit this. */
        options->pressed_control = action;
        cursor.x = options->left+240;
        cursor.y = options->top+(action==3 ? 20 : action==4 ? 80 : 125);
        assert(game_handle_options_input(options, &config, &sdl_config,
            &machine, &scene, &session, assets, &settings, &cursor, "", NULL, 1));
        assert(options->pending_action == action && !options->child);
        assert(session.state.players[0].cash == 12345 && machine.running);
        cursor.x = left + width * 3 / 4; cursor.y = top + height / 2;
        assert(game_handle_options_input(options, &config, &sdl_config,
            &machine, &scene, &session, assets, &settings, &cursor, "", NULL, 1));
        assert(!options->pending_action && options->active && machine.running);
        assert(scene == RICH4_SCENE_OPTIONS && session.state.players[0].cash == 12345);
        assert(rich4_options_panel_begin_action(options, action));
        cursor.x = left + width / 4;
        assert(game_handle_options_input(options, &config, &sdl_config,
            &machine, &scene, &session, assets, &settings, &cursor, "", NULL, 1));
        assert(config.sound_effect == 4);
        if (action == 3) {
            assert(scene == RICH4_SCENE_MENU && machine.running && !session.state.active);
            assert(config.year == today.tm_year+1900 && config.month == today.tm_mon+1 &&
                   config.day == today.tm_mday);
        } else if (action == 4) {
            assert(scene == RICH4_SCENE_GAME && machine.running);
            assert(!session.state.players[0].play_state && session.presentation.bankruptcy.active);
            assert(session.state.current_player == 1);
        } else {
            assert(!machine.running && !options->active);
            assert(scene == RICH4_SCENE_OPTIONS && session.state.active);
            memset(&saved, 0, sizeof(saved));
            rich4_config_read(&saved, "RICH4.CFG");
            assert(saved.sound_effect == 4);
            /* Startup read deliberately refreshes the date; inspect the
             * serialized date instead of expecting read() to preserve it. */
            {
                unsigned char bytes[12];
                FILE *fp = fopen("RICH4.CFG", "rb");
                assert(fp && fread(bytes, 1, sizeof(bytes), fp) == sizeof(bytes));
                fclose(fp);
                assert(bytes[8] == 29 && bytes[9] == 2 &&
                       (bytes[10] | bytes[11] << 8) == 2000);
            }
        }
    }
    assert(remove("RICH4.CFG") == 0);
    assert(chdir(cwd) == 0);
    assert(rmdir(temp) == 0);
}

static void check_surrender_followup(void)
{
    static GameSession s;
    Rich4AudioEventQueue audio = {0};
    GameEventQueue events = {0};
    int i;
    memset(&s, 0, sizeof(s));
    s.state.active = 1;
    s.state.player_count = 3;
    s.state.human_player_count = 2;
    for (i = 0; i < 3; ++i) {
        s.state.players[i].play_state = i == 2 ? 2 : 1;
        s.state.players[i].is_ai = i == 2;
        s.state.players[i].node = i+1;
    }
    assert(rich4_app_surrender_current_player(&s, &audio, 1000));
    rich4_app_surrender_followup(&s, 1000, &audio);
    assert(!s.presentation.magic_house.active);
    assert(rich4_bankruptcy_update(&s, 0, 100, 1000));
    assert(rich4_bankruptcy_update(&s, 0, 100, 3000));
    assert(s.presentation.speech.active);
    s.presentation.speech.active = 0;
    rich4_bankruptcy_finish_speech(&s);
    rich4_app_surrender_followup(&s, 4000, &audio);
    assert(s.presentation.magic_house.active && s.presentation.magic_house.targets == 6);
    assert(!s.presentation.bankruptcy.surrender_followup);
    assert(rich4_magic_house_update(&s, 2, 100, 5999, 0, &audio, &events));
    assert(s.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_INTRO_1);
    assert(rich4_magic_house_update(&s, 2, 100, 6000, 0, &audio, &events));
    assert(s.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_PROMPT);
    assert(rich4_magic_house_update(&s, 2, 100, 8000, 0, &audio, &events));
    assert(s.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_SELECT);
    assert(rich4_magic_house_surrender_target_at(&s, 238, 348) == 1);
    assert(rich4_magic_house_surrender_target_at(&s, 330, 420) == 2);
    rich4_magic_house_surrender_choose(&s, 0, 8000, &audio);
    assert(s.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_SELECT);
    rich4_magic_house_surrender_choose(&s, 2, 8000, &audio);
    assert(s.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_CONFIRM);
    assert(rich4_magic_house_update(&s, 2, 100, 10000, 0, &audio, &events));
    assert(s.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_FINALE);
    assert(rich4_magic_house_update(&s, 2, 100, 10000, 0, &audio, &events));
    assert(rich4_magic_house_update(&s, 2, 100, 10100, 0, &audio, &events));
    assert(rich4_magic_house_update(&s, 2, 100, 10200, 0, &audio, &events));
    assert(s.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_TARGET);
    assert(rich4_magic_house_update(&s, 2, 100, 10300, 0, &audio, &events));
    assert(!s.presentation.magic_house.active);
    assert(s.state.players[2].god_type == 15 && s.state.players[2].god_info == 15);
    assert(s.presentation.dynamic_objects[14].attached_player == 3);
    assert(s.state.current_player == 1);
    rich4_magic_house_begin_surrender(&s, 0, 6, 12000, &audio);
    rich4_magic_house_surrender_cancel(&s, &audio);
    assert(!s.presentation.magic_house.active && !s.state.players[1].god_info);
    s.state.human_player_count = 1;
    s.presentation.bankruptcy.surrender_followup = 1;
    rich4_app_surrender_followup(&s, 14000, &audio);
    assert(!s.presentation.magic_house.active);
}

static void check_options_keys(void)
{
    static GameSession s;
    Rich4Config config = {0};
    VirtualCursor cursor = {0};
    GameKeyDownModalsContext context = {0};
    Rich4OptionsPanelState *p = &s.presentation.options;
    p->active = 1; p->return_app_state = RICH4_SCENE_GAME;
    rich4_options_panel_begin_action(p, 5);
    config.hotkeys[8].key = 'Y'; config.hotkeys[9].key = 'N';
    context.app_state = RICH4_SCENE_OPTIONS; context.session = &s;
    context.config = &config; context.cursor = &cursor;
    context.key = RICH4_APP_KEY_RETURN;
    cursor.confirm_pressed = 1;
    assert(game_handle_key_down_modals(&context));
    assert(!cursor.confirm_pressed && !p->confirm_key_action);
    context.key = RICH4_APP_KEY_LETTER_Y;
    assert(game_handle_key_down_modals(&context));
    assert(cursor.confirm_pressed && p->confirm_key_action == 1);
    context.key = RICH4_APP_KEY_LETTER_N;
    assert(game_handle_key_down_modals(&context));
    assert(cursor.confirm_pressed && p->confirm_key_action == 2);
    config.hotkeys[8].key = 112; config.hotkeys[8].mod = 17;
    context.key = RICH4_APP_KEY_F1; context.key_modifiers = KMOD_LCTRL;
    assert(game_handle_key_down_modals(&context));
    assert(cursor.confirm_pressed && p->confirm_key_action == 1);
    rich4_options_panel_cancel_action(p);
    assert(!p->confirm_key_action);
}

static void check_surrender_render(SDL_Surface *screen, const GamePreviewAssets *assets)
{
    static GameSession s;
    Rich4GameEventPresentationSnapshot snapshot = {&s.state, &s.presentation};
    int player;
    s.state.player_count = 3;
    s.state.players[1].character = 1;
    s.state.players[2].character = 2;
    rich4_magic_house_begin_surrender(&s, 0, 6, 1000, NULL);
    s.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_SELECT;
    draw_magic_house(screen, assets, &snapshot);
    for (player = 1; player <= 2; ++player) {
        Rich4SprSmpChunk chunk;
        const unsigned char *pixels;
        int x, y, anchor = player == 1 ? 238 : 330;
        assert(rich4_spr_smp_get_chunk(assets->character.character_data,
            assets->character.character_size, &assets->character.character_spr,
            player, &chunk) == RICH4_SPR_SMP_OK);
        assert(chunk.width == 72 && chunk.height == 72);
        pixels = assets->character.character_data + chunk.data_offset;
        for (y = 0; y < chunk.height; ++y) for (x = 0; x < chunk.width; ++x) {
            uint16_t pixel = pixels[2*(y*chunk.width+x)] |
                             pixels[2*(y*chunk.width+x)+1] << 8;
            uint16_t actual = *(uint16_t *)((unsigned char *)screen->pixels +
                (348-chunk.y+y)*screen->pitch + (anchor-chunk.x+x)*2);
            if (pixel) assert(actual == map_resource_pixel(screen, pixel));
        }
    }
}

int main(void)
{
    static GamePreviewAssets assets;
    static GameSession session;
    Rich4Mkf *map, *panel, *data;
    Rich4Config config = {0};
    Rich4SdlConfig sdl_config = {0};
    Rich4SceneMachine machine = {0};
    Rich4SceneId scene = RICH4_SCENE_OPTIONS;
    VirtualCursor cursor = {0};
    NewGameSettings settings = {0};
    Rich4OptionsPanelState options = {0};
    static const int portrait[12][4] = {
        {85,71,42,34},{70,65,35,32},{66,60,33,31},{67,74,34,37},
        {77,65,40,35},{77,71,39,37},{76,70,39,35},{79,67,40,33},
        {78,76,39,40},{73,72,37,35},{74,64,35,32},{58,66,34,33}
    };
    int c, i;
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_SWSURFACE,
        640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    assert(screen);
    assert(rich4_mkf_open("assets/original/Map.mkf", &map) == RICH4_MKF_OK);
    assert(rich4_mkf_open("assets/original/Panel.mkf", &panel) == RICH4_MKF_OK);
    assert(rich4_mkf_open("assets/original/Data.mkf", &data) == RICH4_MKF_OK);
    assets.ui.panel_data[73] = resource(panel, 73, &assets.ui.panel_size[73], &assets.ui.panel_spr[73]);
    assets.ui.has_panel[73] = 1;
    assets.menu.options_data = resource(data, 3, &assets.menu.options_size, &assets.menu.options_spr);
    assets.menu.has_options_ui = 1;
    assets.ui.confirm_dialog_data = resource(data, 440,
        &assets.ui.confirm_dialog_size, &assets.ui.confirm_dialog_spr);
    assets.ui.has_confirm_dialog = 1;
    assets.ui.panel_data[18] = resource(panel, 18,
        &assets.ui.panel_size[18], &assets.ui.panel_spr[18]);
    assets.ui.has_panel[18] = 1;
    assets.character.character_data = resource(data, 2,
        &assets.character.character_size, &assets.character.character_spr);
    assets.character.has_character_ui = 1;
    session.state.player_count = 4;
    session.presentation.sale_interaction.active = 1;
    /* No listings: original still draws every player portrait. */
    for (c = 0; c < 12; ++c) {
        Rich4SprSmpChunk chunk;
        assets.character.map_character_data[c] = resource(map, 27+c,
            &assets.character.map_character_size[c], &assets.character.map_character_spr[c]);
        assets.character.has_map_character[c] = 1;
        assert(rich4_spr_smp_get_chunk(assets.character.map_character_data[c],
            assets.character.map_character_size[c], &assets.character.map_character_spr[c], 0, &chunk) == RICH4_SPR_SMP_OK);
        assert(chunk.width == portrait[c][0] && chunk.height == portrait[c][1] &&
               chunk.x == portrait[c][2] && chunk.y == portrait[c][3]);
        session.state.player_count = c == 0 ? 4 : 1;
        for (i = 0; i < 4; ++i) {
            session.state.players[i].character = c;
            session.state.players[i].play_state = i == 1 || i == 2 ? 0 : 1;
            session.state.players[i].engineering_saved_traffic_method = i == 2;
            /* Row 3 has not landed: no node/world coordinates, still colored. */
            session.state.players[i].node = i == 3 ? 0 : 1;
        }
        game_draw_sale_panel(screen, &assets, &session, &session.presentation.sale_interaction);
        for (i = 0; i < session.state.player_count; ++i) {
            int x, y;
            const unsigned char *pixels = assets.character.map_character_data[c]+chunk.data_offset;
            for (y = 0; y < chunk.height; ++y)
                for (x = 0; x < chunk.width; ++x) {
                    int n = (y*chunk.width+x)*2;
                    uint16_t original = pixels[n] | pixels[n+1]<<8;
                    uint16_t actual = *(uint16_t *)((unsigned char *)screen->pixels+
                        (150+72*i-chunk.y+y)*screen->pitch+(66-chunk.x+x)*2);
                    if (original) {
                        if (i == 1) {
                            unsigned int shade = ((original & 31) +
                                ((original >> 5) & 31) + ((original >> 10) & 31) + 16) / 4;
                            original = shade | (shade << 5) | (shade << 10);
                        }
                        assert(actual == map_resource_pixel(screen, original));
                    }
                }
        }
    }
    config.year = 2000; config.month = 2; config.day = 29;
    open_options_panel(&options, &assets, &config, &sdl_config, RICH4_SCENE_MENU);
    for (i = 3; i <= 5; ++i) {
        cursor.x = options.left+240;
        cursor.y = options.top+(i==3 ? 20 : i==4 ? 80 : 125);
        options.pressed_control = i;
        assert(game_handle_options_input(&options, &config, &sdl_config,
            &machine, &scene, &session, &assets, &settings, &cursor, "", NULL, 1));
        assert(options.child == i && !options.pending_action);
        draw_options_panel(screen, &assets, &options);
        if (getenv("RICH4_TEST_SNAPSHOTS") && i != 5) {
            char path[128];
            snprintf(path, sizeof(path), "/tmp/rich4-options-%d.bmp", i);
            SDL_SaveBMP(screen, path);
        }
        rich4_options_child_cancel(&options);
        assert(options.active && scene == RICH4_SCENE_OPTIONS);
    }
    check_game_actions(&assets);
    check_surrender_followup();
    check_options_keys();
    check_surrender_render(screen, &assets);
    SDL_FreeSurface(screen);
    rich4_mkf_close(map); rich4_mkf_close(panel); rich4_mkf_close(data);
    free(assets.ui.panel_data[73]); free(assets.menu.options_data);
    free(assets.ui.confirm_dialog_data);
    free(assets.ui.panel_data[18]); free(assets.character.character_data);
    for (c = 0; c < 12; ++c) free(assets.character.map_character_data[c]);
    puts("title/game options, surrender follow-up and original portraits: ok");
    return 0;
}
