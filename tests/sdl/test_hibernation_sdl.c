#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "app/turn_start_flow.h"
#include "data/resource_catalog.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_confinement_render_view.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_confinement_event_render.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/text/runtime_text.h"
#include "resource/mkf.h"

static GamePreviewAssets assets;
static GameSession session;
static GameViewModel view;

static void equal_frames(SDL_Surface *a, SDL_Surface *b)
{
    assert(a->pitch == b->pitch);
    assert(memcmp(a->pixels, b->pixels, a->pitch * a->h) == 0);
}

/* Expected RGB555 palette encoding of 0x455628's RGB565 output. */
static void recolor_reference(unsigned char *data, const Rich4SprSmpInfo *spr)
{
    static const unsigned char lut[32] = {
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,7,6,5,4,3,2,1,0
    };
    int i;
    for (i = 0; i < 256; i++) {
        uint16_t p;
        unsigned int shade;
        memcpy(&p, data + spr->start_offset + 2 * i, 2);
        shade = ((p & 31u) + ((p >> 5) & 31u) +
                 ((p >> 10) & 31u) + 31u) >> 2;
        shade += lut[shade];
        p = (uint16_t)((shade << 10) | (shade << 5) | 31u);
        memcpy(data + spr->start_offset + 2 * i, &p, 2);
    }
}

static void check_map_actors(SDL_Surface *actual, SDL_Surface *expected)
{
    Rich4Mkf *mkf = NULL;
    NewGameSettings settings = {0};
    int actor, direction, sleeping;
    assert(rich4_mkf_open("assets/original/Data.mkf", &mkf) == RICH4_MKF_OK);
    for (actor = 0; actor < 16; actor++) {
        unsigned char *data, *original, *blue;
        size_t size;
        Rich4SprSmpInfo spr;
        int resource = actor < 12 ? 128 + actor * 21 : 380 + (actor - 12) * 4;
        assert(rich4_mkf_read_resource(mkf, resource, &data, &size, NULL) == RICH4_MKF_OK);
        assert(rich4_spr_smp_probe(data, size, &spr));
        original = malloc(size);
        blue = malloc(size);
        assert(original && blue);
        memcpy(original, data, size);
        memcpy(blue, data, size);
        recolor_reference(blue, &spr);
        memset(&view, 0, sizeof(view));
        view.active = 1;
        view.viewport.center_world_x = view.viewport.center_world_y = 320;
        if (actor < 12) {
            view.actor_count = 1;
            view.actors[0].visible = 1;
            view.actors[0].character = actor;
            view.actors[0].world_x = view.actors[0].world_y = 320;
            view.actors[0].overlay_part = -1;
            assets.character.map_player_data[actor][0] = data;
            assets.character.map_player_size[actor][0] = size;
            assets.character.map_player_spr[actor][0] = spr;
            assets.character.has_map_player[actor][0] = 1;
        } else {
            int slot = actor - 12;
            view.special_actor_count = 4;
            view.special_actors[slot].visible = 1;
            view.special_actors[slot].world_x = view.special_actors[slot].world_y = 320;
            view.special_actors[slot].overlay_chunk = -1;
            assets.character.map_special_actor_data[slot][0] = data;
            assets.character.map_special_actor_size[slot][0] = size;
            assets.character.map_special_actor_spr[slot][0] = spr;
            assets.character.has_map_special_actor[slot][0] = 1;
        }
        for (direction = 0; direction < 8; direction++) {
            for (sleeping = 1; sleeping >= 0; sleeping--) {
                int x, y;
                if (actor < 12) {
                    view.actors[0].visual_direction = direction;
                    view.actors[0].sleeping = sleeping;
                } else {
                    view.special_actors[actor - 12].visual_direction = direction;
                    view.special_actors[actor - 12].sleeping = sleeping;
                }
                SDL_FillRect(actual, NULL, 0x4210);
                SDL_FillRect(expected, NULL, 0x4210);
                draw_game_map_objects(actual, &assets, &settings, &view, NULL);
                rich4_game_viewport_project(&view.viewport, 320, 320, &x, &y);
                assert(draw_spr_smp_center(expected, sleeping ? blue : original,
                    size, &spr, direction * (spr.num_chunks / 8), x, y));
                equal_frames(actual, expected);
                /* Shared resource bytes survive redraws and waking unchanged. */
                assert(memcmp(data, original, size) == 0);
            }
        }
        if (actor < 12) assets.character.has_map_player[actor][0] = 0;
        else assets.character.has_map_special_actor[actor - 12][0] = 0;
        free(data); free(original); free(blue);
    }
    rich4_mkf_close(mkf);
}

static void check_turn_notice(SDL_Surface *actual, SDL_Surface *expected)
{
    GameConfinementRenderView notice;
    Rich4TurnStartRequest request;
    NewGameSettings settings = {0};
    char name[64], text[128];
    Rich4SprSmpChunk background;
    int ai;
    for (ai = 0; ai < 2; ai++) {
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 2;
        session.state.players[0].character = 9;
        session.state.players[0].play_state = ai ? 2 : 1;
        session.state.players[0].is_ai = ai;
        session.state.players[0].days_sleeping = 4;
        session.state.players[1].play_state = 1;
        session.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
        assert(rich4_turn_start_prepare_request(&session, 1000, &request));
        game_apply_turn_start_request(&session, &assets, &settings, 1000, &request);
        assert(session.presentation.confinement_status.active);
        assert(session.state.current_player == 0);
        assert(game_update_confinement_status_presentation(&session, &assets, &settings, 1000));
        assert(session.presentation.confinement_status.until == 2500);
        assert(build_confinement_render_view(&session, &notice));
        SDL_FillRect(actual, NULL, 0x4210);
        SDL_FillRect(expected, NULL, 0x4210);
        draw_confinement_status_presentation(actual, &assets, &notice);
        assert(rich4_spr_smp_get_chunk(assets.ui.common_dialog_data,
            assets.ui.common_dialog_size, &assets.ui.common_dialog_spr,
            5, &background) == RICH4_SPR_SMP_OK);
        assert(draw_chunk_at(expected, assets.ui.common_dialog_data,
            &background, 220, 140, 1));
        game_compact_display_name(name, sizeof(name), new_game_character_name(9));
        snprintf(text, sizeof(text),
            "%s\xa5\x56\xaf\x76\xa4\xa4\n\n\xc1\xd9\xb3\xd1%d\xa4\xd1\xa1\x49", name, 5);
        draw_big5_text(expected, 220, 140, text, 16,
            0xf79e, 0x1082, 3, 4);
        equal_frames(actual, expected);
        assert(game_update_confinement_status_presentation(&session, &assets, &settings, 2499));
        assert(session.state.current_player == 0);
        assert(game_update_confinement_status_presentation(&session, &assets, &settings, 2500));
        assert(!session.presentation.confinement_status.active);
        assert(session.state.current_player == 1);
        assert(!session.presentation.dice.status_visible);
    }
}

int main(void)
{
    Rich4Mkf *mkf = NULL;
    ResourceCatalogEntry dialog;
    SDL_Surface *actual = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
        0xf800, 0x07e0, 0x001f, 0);
    SDL_Surface *expected = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
        0xf800, 0x07e0, 0x001f, 0);
    assert(actual && expected);
    rich4_sdl_text_set_font_path("assets/fonts/rich4.ttc");
    assert(rich4_mkf_open("assets/original/Data.mkf", &mkf) == RICH4_MKF_OK);
    assert(rich4_resource_catalog_data_common_dialog(&dialog));
    assert(rich4_mkf_read_resource(mkf, dialog.resource_index,
        &assets.ui.common_dialog_data, &assets.ui.common_dialog_size, NULL) == RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(assets.ui.common_dialog_data,
        assets.ui.common_dialog_size, &assets.ui.common_dialog_spr));
    assets.ui.has_common_dialog = 1;
    check_map_actors(actual, expected);
    check_turn_notice(actual, expected);
    free(assets.ui.common_dialog_data);
    rich4_mkf_close(mkf);
    SDL_FreeSurface(actual); SDL_FreeSurface(expected);
    puts("hibernation SDL: 12 players, 4 special actors, 8 directions, wake and turn notice: ok");
    return 0;
}
