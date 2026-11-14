#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "app/bank_flow.h"
#include "core/game_event_payload.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/render/game_bank_render.h"
#include "platform/sdl1/text/runtime_text.h"
#include "resource/mkf.h"

int main(void)
{
    static GamePreviewAssets assets;
    static GameSession session;
    GameSceneView scene;
    Rich4GameEventPresentationSnapshot snapshot = {&session.state, &session.presentation};
    GameEvent event = rich4_game_event_bank_loan_due(0, 1000, 3, 0, 0);
    Rich4Mkf *mkf = NULL;
    SDL_Surface *screen = SDL_CreateRGBSurface(
        SDL_SWSURFACE, 640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    unsigned char *greeting;
    const char *screenshot = getenv("RICH4_LOAN_SCREENSHOT");
    const int resources[] = {2, 23};
    int i;
    assert(screen != NULL);
    rich4_sdl_text_set_font_path("assets/fonts/rich4.ttc");
    assert(rich4_mkf_open("assets/original/Panel.mkf", &mkf) == RICH4_MKF_OK);
    for (i = 0; i < 2; i++) {
        int resource = resources[i];
        unsigned char *data = NULL;
        size_t size = 0;
        assert(rich4_mkf_read_resource(mkf, resource, &data, &size, NULL) == RICH4_MKF_OK);
        assets.ui.panel_data[resource] = data;
        assets.ui.panel_size[resource] = size;
        assert(rich4_spr_smp_probe(data, size, &assets.ui.panel_spr[resource]));
        assets.ui.has_panel[resource] = 1;
    }
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].loan = 1000;
    session.state.players[0].loan_due_date = 3;
    session.state.calendar_start_year = 2000;
    session.state.calendar_start_month = session.state.calendar_start_day = 1;
    rich4_bank_flow_begin_loan_due(&session, &event, 1000);
    assert(build_game_session_render_view(&assets, NULL, &session, &scene));
    assert(scene.bank_loan_reminder && !scene.route_interaction_active);
    assert(scene.bank_interaction_card_x == 440 && !scene.bank_interaction_amount_entry);
    SDL_FillRect(screen, NULL, 0x1234);
    draw_bank_loan_due_presentation(screen, &assets, &snapshot);
    for (i = 0; i < screen->pitch * screen->h / 2; i++)
        assert(((uint16_t *)screen->pixels)[i] == 0x1234);
    draw_game_bank_interaction(screen, &assets, &scene);
    if (screenshot != NULL) assert(SDL_SaveBMP(screen, screenshot) == 0);
    greeting = malloc((size_t)screen->pitch * screen->h);
    assert(greeting != NULL);
    memcpy(greeting, screen->pixels, (size_t)screen->pitch * screen->h);
    assert(rich4_bank_flow_update(&session, 3000, 0, NULL));
    assert(build_game_session_render_view(&assets, NULL, &session, &scene));
    draw_game_bank_interaction(screen, &assets, &scene);
    assert(memcmp(greeting, screen->pixels, (size_t)screen->pitch * screen->h) != 0);
    free(greeting);
    for (i = 0; i < 2; i++) free(assets.ui.panel_data[resources[i]]);
    rich4_mkf_close(mkf);
    SDL_FreeSurface(screen);
    puts("bank loan reminder Panel23 presentation: ok");
    return 0;
}
