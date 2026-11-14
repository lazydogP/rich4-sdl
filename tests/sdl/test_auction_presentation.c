#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/text/runtime_text.h"
#include "resource/mkf.h"

/* Independent raw SMP oracle, including opaque zeroes in restored crops. */
static void copy_smp(SDL_Surface *screen, const GamePreviewAssets *assets,
    int chunk_index, int sx, int sy, int w, int h, int dx, int dy, int transparent)
{
    Rich4SprSmpChunk c;
    int x, y;
    const unsigned char *data = assets->ui.panel_data[26];
    assert(rich4_spr_smp_get_chunk(data, assets->ui.panel_size[26],
        &assets->ui.panel_spr[26], chunk_index, &c) == RICH4_SPR_SMP_OK);
    assert(sx >= 0 && sy >= 0 && sx+w <= c.width && sy+h <= c.height);
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        const unsigned char *p = data + c.data_offset + 2*((sy+y)*c.width+sx+x);
        uint16_t color = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
        uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels+(dy+y)*screen->pitch);
        if (color || !transparent) row[dx+x] = map_resource_pixel(screen, color);
    }
}

static void mouth_pixels(SDL_Surface *actual, SDL_Surface *expected,
    const GamePreviewAssets *assets, GameState *state, GamePresentationState *p)
{
    Rich4GameEventPresentationSnapshot snapshot = {state, p};
    int sold, open;
    state->player_count = 0;
    memset(&p->auction, 0, sizeof(p->auction));
    p->auction.active = 1;
    p->auction.bidding.current_player = p->auction.bidding.leader = -1;
    for (sold = 0; sold <= 1; sold++) {
        int x = sold ? 183 : 163, y = sold ? 65 : 60;
        p->auction.stage = sold ? RICH4_AUCTION_PRESENT_SOLD_CALL : RICH4_AUCTION_PRESENT_INTRO;
        for (open = 1; open >= 0; open--) {
            Rich4SprSmpChunk c;
            int chunk = (sold ? 22 : 28) + open;
            p->auction.mouth_drawn = 0;
            draw_auction_presentation(expected, assets, &snapshot);
            copy_smp(expected, assets, 0, x, y, sold ? 40 : 30, sold ? 30 : 25, x, y, 0);
            assert(rich4_spr_smp_get_chunk(assets->ui.panel_data[26], assets->ui.panel_size[26],
                &assets->ui.panel_spr[26], chunk, &c) == RICH4_SPR_SMP_OK);
            assert(c.x == 0 && c.y == 0);
            copy_smp(expected, assets, chunk, 0, 0, c.width, c.height, x, y, 1);
            p->auction.mouth_drawn = 1;
            p->auction.mouth_ticks = open;
            draw_auction_presentation(actual, assets, &snapshot);
            /* Whole-frame compare also rejects the old right/bottom coordinates. */
            assert(!memcmp(actual->pixels, expected->pixels, actual->pitch * 480));
        }
        p->auction.mouth_drawn = 0;
        draw_auction_presentation(actual, assets, &snapshot);
        draw_auction_presentation(expected, assets, &snapshot);
        assert(!memcmp(actual->pixels, expected->pixels, actual->pitch * 480));
    }
}

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
    rich4_sdl_text_set_font_path("assets/fonts/rich4.ttc");
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
    mouth_pixels(actual, expected, &assets, &state, &presentation);
    /* Frame 1 is the first visible bid frame; N restores frame 0. Compare
     * every character/frame against palette indices from the original SPR. */
    state.player_count = 1;
    presentation.actors[0].first_turn_landing_pending = 0;
    presentation.auction.stage = RICH4_AUCTION_PRESENT_ACTION;
    presentation.auction.bidding.seller = -1;
    presentation.auction.bidding.last_player = 0;
    presentation.auction.bidding.last_bid = 100;
    presentation.auction.bidding.last_action = GAME_AUCTION_BID_100;
    presentation.auction.bidding.participant[0] = GAME_AUCTION_PARTICIPANT_READY;
    for (character = 0; character < 12; character++) {
        int k;
        state.players[0].character = character;
        resource = 27 + 3*character;
        for (k = 1; k <= (int)assets.ui.panel_spr[resource].num_chunks; k++) {
            Rich4SprSmpChunk c;
            int frame = k == (int)assets.ui.panel_spr[resource].num_chunks ? 0 : k;
            int x, y;
            presentation.auction.action_tick = k;
            assets.ui.has_panel[resource] = 0;
            draw_auction_presentation(expected, &assets, &snapshot);
            assets.ui.has_panel[resource] = 1;
            draw_auction_presentation(actual, &assets, &snapshot);
            assert(rich4_spr_smp_get_chunk(assets.ui.panel_data[resource], assets.ui.panel_size[resource],
                &assets.ui.panel_spr[resource], frame, &c) == RICH4_SPR_SMP_OK);
            for (y = 0; y < c.height; y++) for (x = 0; x < c.width; x++) {
                int dx = 590-c.x+x, dy = 80-c.y+y;
                unsigned char index = assets.ui.panel_data[resource][c.data_offset+y*c.width+x];
                const unsigned char *palette;
                uint16_t *row;
                if (!index || dy < 0 || dy >= 84 || dx < 0 || dx >= 640) continue;
                palette = assets.ui.panel_data[resource]+assets.ui.panel_spr[resource].start_offset+2*index;
                row = (uint16_t *)((unsigned char *)expected->pixels+dy*expected->pitch);
                row[dx] = map_resource_pixel(expected, palette[0] | ((uint16_t)palette[1] << 8));
            }
            for (y = 0; y < 84; y++)
                assert(!memcmp((unsigned char *)actual->pixels+y*actual->pitch,
                    (unsigned char *)expected->pixels+y*expected->pitch, 1280));
        }
    }
    if (getenv("RICH4_AUCTION_SNAPSHOT")) {
        state.player_count = 4;
        for (character = 0; character < 4; character++) {
            state.players[character].play_state = 1;
            state.players[character].character = character * 3;
            state.players[character].cash = 20000;
            presentation.auction.bidding.participant[character] = GAME_AUCTION_PARTICIPANT_READY;
        }
        state.land_count = 1;
        state.land_owner[0] = -1;
        state.land_level[0] = 1;
        presentation.auction.bidding.target = rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
        presentation.auction.bidding.current_player = 0;
        presentation.auction.stage = RICH4_AUCTION_PRESENT_RESERVE;
        presentation.auction.speech_resource = 132;
        presentation.auction.speech_visible = 1;
        presentation.auction.bidding.reserve_price = 12345;
        presentation.auction.bidding.current_price = 12345;
        presentation.auction.mouth_drawn = presentation.auction.mouth_ticks = 1;
        draw_auction_presentation(actual, &assets, &snapshot);
        assert(SDL_SaveBMP(actual, getenv("RICH4_AUCTION_SNAPSHOT")) == 0);
    }
    for (resource = 26; resource <= 62; resource++)
        free((void *)assets.ui.panel_data[resource]);
    rich4_mkf_close(mkf);
    SDL_FreeSurface(actual);
    SDL_FreeSurface(expected);
    rich4_sdl_text_shutdown();
    puts("auction presentation: original SPR frames, mouth crops, pending rows OK");
    return 0;
}
