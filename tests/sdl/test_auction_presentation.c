#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#undef main
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/render/blit.h"
#include "resource/mkf.h"

int main(void)
{
    static GamePreviewAssets assets;
    static GameState state;
    static GamePresentationState presentation;
    Rich4GameEventPresentationSnapshot snapshot = { &state, &presentation };
    Rich4Mkf *mkf = NULL;
    SDL_Surface *actual = SDL_CreateRGBSurface(
        SDL_SWSURFACE, 640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    SDL_Surface *expected = SDL_CreateRGBSurface(
        SDL_SWSURFACE, 640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    int resource, character, pose;
    assert(actual && expected);
    assert(rich4_mkf_open("assets/original/Panel.mkf", &mkf) == RICH4_MKF_OK);
    for (resource = 26; resource <= 62; resource++) {
        unsigned char *data = NULL;
        size_t size = 0;
        assert(rich4_mkf_read_resource(mkf, resource, &data, &size, NULL) == RICH4_MKF_OK);
        assets.ui.panel_data[resource] = data;
        assets.ui.panel_size[resource] = size;
        assert(rich4_spr_smp_probe(data, size, &assets.ui.panel_spr[resource]));
        assets.ui.has_panel[resource] = 1;
    }
    state.player_count = 1;
    state.players[0].play_state = 1;
    state.players[0].cash = 20000;
    presentation.auction.active = 1;
    presentation.auction.bidding.seller = -1;
    presentation.auction.bidding.current_player = -1;
    presentation.auction.bidder_mask = 1;
    for (character = 0; character < 12; character++) {
        state.players[0].character = character;
        for (pose = 0; pose < 3; pose++) {
            Rich4SprSmpChunk chunk;
            int y, changed = 0;
            resource = 27 + 3 * character + pose;
            presentation.auction.stage = pose == 2 ?
                RICH4_AUCTION_PRESENT_WINNER : RICH4_AUCTION_PRESENT_INTRO;
            presentation.auction.celebration_tick = 1;
            presentation.auction.bidding.leader = pose == 2 ? 0 : -1;
            presentation.auction.bidding.participant[0] = pose == 1 ?
                GAME_AUCTION_PARTICIPANT_WITHDRAWN : GAME_AUCTION_PARTICIPANT_READY;
            assets.ui.has_panel[resource] = 0;
            draw_auction_presentation(expected, &assets, &snapshot);
            SDL_BlitSurface(expected, NULL, actual, NULL);
            assets.ui.has_panel[resource] = 1;
            draw_auction_presentation(actual, &assets, &snapshot);
            assert(!memcmp(assets.ui.panel_spr[resource].signature, "SPR", 4));
            assert(rich4_spr_smp_get_chunk(assets.ui.panel_data[resource],
                assets.ui.panel_size[resource], &assets.ui.panel_spr[resource],
                0, &chunk) == RICH4_SPR_SMP_OK);
            /* 0x45663E: palette[index], zero transparent, anchor minus hotspot.
             * Check above the cash label so text does not mask the sprite. */
            for (y = 0; y < chunk.height; y++) {
                int x, dy = 80 - chunk.y + y;
                if (dy < 0 || dy >= 94) continue;
                for (x = 0; x < chunk.width; x++) {
                    int dx = 590 - chunk.x + x;
                    unsigned char index = assets.ui.panel_data[resource][chunk.data_offset + y * chunk.width + x];
                    uint16_t *row;
                    const unsigned char *palette;
                    if (dx < 0 || dx >= 640 || !index) continue;
                    palette = assets.ui.panel_data[resource] + assets.ui.panel_spr[resource].start_offset + 2 * index;
                    row = (uint16_t *)((unsigned char *)expected->pixels + dy * expected->pitch);
                    row[dx] = map_resource_pixel(expected, palette[0] | ((uint16_t)palette[1] << 8));
                    changed++;
                }
            }
            assert(changed > 0);
            for (y = 0; y < 94; y++)
                assert(!memcmp((unsigned char *)actual->pixels + y * actual->pitch,
                               (unsigned char *)expected->pixels + y * expected->pitch, 1280));
        }
    }
    /* A pending player has no row, even though SDL reserves play_state. */
    presentation.actors[0].first_turn_landing_pending = 1;
    draw_auction_presentation(actual, &assets, &snapshot);
    state.player_count = 0;
    draw_auction_presentation(expected, &assets, &snapshot);
    assert(!memcmp(actual->pixels, expected->pixels, actual->pitch * 480));
    for (resource = 26; resource <= 62; resource++)
        free((void *)assets.ui.panel_data[resource]);
    rich4_mkf_close(mkf);
    SDL_FreeSurface(actual);
    SDL_FreeSurface(expected);
    puts("auction presentation: 36 original SPR poses and pending landing rows OK");
    return 0;
}
