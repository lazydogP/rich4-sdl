#define SDL_MAIN_HANDLED
#include <SDL/SDL.h>
#undef main
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/point_minigame_flow.h"
#include "core/game_event_payload.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_point_minigame_view.h"
#include "platform/sdl1/render/game_point_minigame_render.h"
#include "resource/mkf.h"

static GameSession session;
static GamePreviewAssets assets;

int main(void)
{
    Rich4Mkf *panel = NULL, *data = NULL;
    SDL_Surface *screen;
    GamePointMinigameRenderView view;
    Rich4AudioEventQueue audio;
    int id, character, direction, frame;
    if (!getenv("SDL_VIDEODRIVER")) SDL_putenv("SDL_VIDEODRIVER=dummy");
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    screen = SDL_SetVideoMode(640,480,16,SDL_SWSURFACE);
    assert(screen);
    assert(rich4_mkf_open("assets/original/Panel.mkf",&panel) == RICH4_MKF_OK);
    assert(rich4_mkf_open("assets/original/Data.mkf",&data) == RICH4_MKF_OK);
    for (id = 78; id <= 111; id++) {
        if (id >= 80 && id <= 91) continue;
        assert(rich4_mkf_read_resource(panel,id,&assets.ui.panel_data[id],
            &assets.ui.panel_size[id],NULL) == RICH4_MKF_OK);
        if (id == 78) assets.ui.has_panel_graph[id] = 1;
        else if (id == 92) assets.ui.has_panel_raw[id] = 1;
        else {
            assert(rich4_spr_smp_probe(assets.ui.panel_data[id],assets.ui.panel_size[id],
                &assets.ui.panel_spr[id]));
            assets.ui.has_panel[id] = 1;
        }
    }
    assert(rich4_mkf_read_resource(data,526,&assets.ui.point_minigame_effect_data,
        &assets.ui.point_minigame_effect_size,NULL) == RICH4_MKF_OK);
    assets.ui.has_point_minigame_effect = 1;
    /* All original walking frames, both directions: geometry from actual MKF. */
    for (character = 0; character < 12; character++) {
        int frames = ((int)assets.ui.panel_spr[100+character].num_chunks - 5)/2;
        for (direction = 1; direction <= 2; direction++) for (frame = 0; frame < frames; frame++) {
            Rich4SprSmpChunk chunk;
            struct PointMinigameState *g;
            PointMinigameObject *o;
            GameEvent event = rich4_game_event_point_minigame_request(0,8,78,79,1);
            memset(&session,0,sizeof(session)); session.state.player_count = 1;
            session.state.players[0].character = character;
            rich4_audio_event_queue_init(&audio);
            rich4_point_minigame_begin(&session,&event,0,1,&audio);
            rich4_point_minigame_update(&session,20,114,NULL,0,320,&audio);
            g = &session.presentation.point_minigame;
            g->phase = RICH4_POINT_MINIGAME_PLAY; g->next_tick = 0;
            g->catcher_direction = direction; g->catcher_frame = frame;
            g->catcher.drop_frame = 4;
            assert(g->catcher.move_frames == frames);
            assert(rich4_spr_smp_get_chunk(assets.ui.panel_data[100+character],
                assets.ui.panel_size[100+character],&assets.ui.panel_spr[100+character],
                5+(direction-1)*frames+frame,&chunk) == RICH4_SPR_SMP_OK);
            o = &g->object[0]; o->active = 1; o->type = 0;
            o->x = 320-chunk.x+1; o->y = 380-chunk.y+1;
            assert(game_update_point_minigame(&session,&assets,NULL,0,320));
            assert(g->counters[0] == 1 && !o->active && g->score == 10);
            g->next_tick = 0; o->active = 1; o->x = 320-chunk.x;
            o->y = 380-chunk.y+1;
            assert(game_update_point_minigame(&session,&assets,NULL,0,320));
            assert(g->counters[0] == 1 && o->active); /* Strict frame edge. */
        }
    }
    session.presentation.point_minigame.catcher.draw_objects[0] =
        (PointMinigameObject){1,220,250,0,0,-23,0};
    session.presentation.point_minigame.catcher.draw_objects[1] =
        (PointMinigameObject){1,440,320,4,3,28,0};
    session.presentation.point_minigame.catcher.source_draw_x = 400;
    session.presentation.point_minigame.catcher.source_draw_chunk = 14;
    session.presentation.point_minigame.catcher.secondary_draw_frame = 5;
    session.presentation.point_minigame.catcher.secondary_x = 220;
    assert(build_point_minigame_render_view(&session,&view));
    draw_point_minigame(screen,&assets,&view);
    assert(SDL_SaveBMP(screen,"build/catcher-original-assets.bmp") == 0);
    for (id = 78; id <= 111; id++) free(assets.ui.panel_data[id]);
    free(assets.ui.point_minigame_effect_data);
    rich4_mkf_close(panel); rich4_mkf_close(data); SDL_Quit();
    puts("catcher: all 12 characters, both directions, all 266 walking frames, strict edges and original rendering: ok");
    return 0;
}
