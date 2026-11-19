#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>
#undef main

#include "app/game_session.h"
#include "app/map_input_flow.h"
#include "core/victory.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"

static const char padded_character_name[] =
    "\xae\x5d\x20\xa4\x70\x20\xac\xfc"; /* 孫 小 美 */
static const char *header_name;
static char header_god_days[24];
static size_t header_god_chunk;
static PlayerAssetBreakdown test_assets;
static struct {
    int x, y;
    char text[64];
} drawn_text[256];
static int drawn_text_count;

static void expect_text(int x, int y, const char *expected)
{
    int i;
    for (i = drawn_text_count - 1; i >= 0; i--)
        if (drawn_text[i].x == x && drawn_text[i].y == y) {
            if (strcmp(drawn_text[i].text, expected) != 0)
                fprintf(stderr, "text at (%d,%d): got %s, expected %s\n",
                        x, y, drawn_text[i].text, expected);
            assert(strcmp(drawn_text[i].text, expected) == 0);
            return;
        }
    assert(!"expected player-info text was not drawn");
}

static void draw_panel(SDL_Surface *screen, const GamePreviewAssets *assets,
                        const GameSession *session)
{
    drawn_text_count = 0;
    game_draw_player_info_panel(screen, assets, session);
}

const char *game_stock_name_for_state(const GameState *state, int stock)
{
    (void)state;
    (void)stock;
    return "";
}

int rich4_app_player_info_player_is_visible(const GameSession *session,
                                            int player)
{
    return player >= 0 && player < session->state.player_count;
}

int rich4_app_player_info_selected_player(const GameSession *session,
                                          int preferred_player)
{
    assert(preferred_player >= 0);
    assert(preferred_player < session->state.player_count);
    return preferred_player;
}

int rich4_player_total_assets(const GameState *state,
                              int player,
                              PlayerAssetBreakdown *out_breakdown)
{
    (void)state;
    (void)player;
    if (out_breakdown != NULL) *out_breakdown = test_assets;
    return test_assets.total;
}

int draw_game_panel_chunk(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          int resource_index,
                          size_t chunk_index,
                          int x,
                          int y,
                          int transparent_zero)
{
    (void)screen;
    (void)assets;
    (void)resource_index;
    (void)chunk_index;
    (void)x;
    (void)y;
    (void)transparent_zero;
    return 1;
}

int draw_game_panel_chunk_at(SDL_Surface *screen,
                             const GamePreviewAssets *assets,
                             int resource_index,
                             size_t chunk_index,
                             int center_x,
                             int center_y,
                             int transparent_zero)
{
    (void)screen;
    (void)assets;
    (void)resource_index;
    (void)chunk_index;
    (void)center_x;
    (void)center_y;
    (void)transparent_zero;
    if (resource_index == 9 && center_x == 60 && center_y == 188)
        header_god_chunk = chunk_index;
    return 1;
}

int draw_spr_smp_center(SDL_Surface *screen,
                        const unsigned char *data,
                        size_t size,
                        const Rich4SprSmpInfo *spr,
                        size_t chunk_index,
                        int center_x,
                        int center_y)
{
    (void)screen;
    (void)data;
    (void)size;
    (void)spr;
    (void)chunk_index;
    (void)center_x;
    (void)center_y;
    return 1;
}

void draw_big5_text(SDL_Surface *screen,
                    int x,
                    int y,
                    const char *text,
                    int height,
                    uint16_t foreground,
                    uint16_t shadow,
                    int style,
                    int anchor)
{
    (void)screen;
    (void)height;
    (void)foreground;
    (void)shadow;
    (void)style;
    (void)anchor;
    if (x == 60 && y == 30 && header_name == NULL)
        header_name = text;
    if (x == 60 && y == 234)
        snprintf(header_god_days, sizeof(header_god_days), "%s", text);
    assert(drawn_text_count < 256);
    drawn_text[drawn_text_count].x = x;
    drawn_text[drawn_text_count].y = y;
    snprintf(drawn_text[drawn_text_count].text,
             sizeof(drawn_text[drawn_text_count].text), "%s", text);
    drawn_text_count++;
}

void format_hud_number(char *out,
                       size_t out_size,
                       int value,
                       int money_prefix)
{
    (void)money_prefix;
    snprintf(out, out_size, "%d", value);
}

void fill_rect(SDL_Surface *screen,
               int x,
               int y,
               int width,
               int height,
               uint16_t color)
{
    (void)screen;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
}

int SDLCALL SDL_LockSurface(SDL_Surface *screen)
{
    (void)screen;
    return 0;
}

void SDLCALL SDL_UnlockSurface(SDL_Surface *screen)
{
    (void)screen;
}

static void test_god_remaining_days(SDL_Surface *screen,
                                    const GamePreviewAssets *assets,
                                    GameSession *session)
{
    int tab;
    session->state.player_count = 2;
    session->state.players[0].god_info = 3;
    session->state.players[0].god_type = 5;
    session->state.players[0].god_status_days = 0;
    session->state.players[1].character = 9;
    session->state.players[1].god_info = 16;
    session->state.players[1].god_type = 15;
    session->state.players[1].god_status_days = 5;
    session->presentation.dynamic_objects[2].countdown = 7;
    session->presentation.dynamic_objects[4].countdown = 12;
    session->presentation.dynamic_objects[15].countdown = 13;

    /* 0x42315F resolves player +63 as a one-based dynamic-object slot;
     * the displayed byte is object +4, independent of player +61. */
    for (tab = 0; tab < 3; tab++) {
        session->presentation.player_info.tab = tab;
        session->presentation.player_info.selected_player = 0;
        header_god_days[0] = '\0';
        draw_panel(screen, assets, session);
        assert(strcmp(header_god_days, "7\xa4\xd1") == 0);
        assert(header_god_chunk == 15); /* slot 3, independently of type 5 */

        session->presentation.player_info.selected_player = 1;
        draw_panel(screen, assets, session);
        assert(strcmp(header_god_days, "13\xa4\xd1") == 0);
        assert(header_god_chunk == 24);
    }

    session->presentation.player_info.selected_player = 0;
    session->presentation.dynamic_objects[2].countdown = 6;
    session->state.players[0].god_status_days = 0x80;
    draw_panel(screen, assets, session);
    assert(strcmp(header_god_days, "6\xa4\xd1") == 0);

    session->presentation.dynamic_objects[2].countdown = 1;
    draw_panel(screen, assets, session);
    assert(strcmp(header_god_days, "1\xa4\xd1") == 0);

    session->state.players[0].god_info = 0;
    header_god_days[0] = '\0';
    draw_panel(screen, assets, session);
    assert(header_god_days[0] == '\0');
}

static void test_asset_fields(SDL_Surface *screen,
                               const GamePreviewAssets *assets,
                               GameSession *session)
{
    memset(session, 0, sizeof(*session));
    session->state.player_count = 2;
    session->state.players[0].character = 9;
    session->state.players[1].character = 9;
    session->presentation.player_info.active = 1;
    session->state.players[1].cash = 123;
    session->state.players[1].bank = 456;
    session->state.players[1].loan = 78;
    session->state.players[1].special_finance = 9999;
    session->state.players[1].points = 65535;
    session->state.players[1].days_assurance = 23;
    session->state.players[1].god_status_days = 9;
    session->state.players[1].tool_inventory[2] = 4;
    session->state.players[1].tool_inventory[14] = 99; /* reserved */
    session->state.players[1].card_inventory[0] = 1;
    session->state.players[1].card_inventory[14] = 30;
    session->state.company_count = 4;
    session->state.company_owner[0] = 1;
    session->state.company_owner[1] = 0;
    session->state.company_owner[2] = 1;
    session->state.company_owner[3] = -1;
    session->state.land_count = 4;
    session->state.land_owner[0] = 1; /* empty residential */
    session->state.land_owner[1] = 1;
    session->state.land_level[1] = 3; /* house */
    session->state.land_owner[2] = 1;
    session->state.land_type[2] = 1; /* converted facility, even level 0 */
    session->state.land_owner[3] = 0;
    session->state.facility_count = 3;
    session->state.facility_owner[0] = 1; /* undeveloped commercial */
    session->state.facility_owner[1] = 1;
    session->state.facility_level[1] = 4;
    session->state.facility_owner[2] = 0;
    session->state.facility_level[2] = 5;
    test_assets.total = 876543;
    test_assets.stock_value = 54321;
    session->presentation.player_info.selected_player = 1;
    draw_panel(screen, assets, session);
    expect_text(330, 88, "123");
    expect_text(330, 136, "456");
    expect_text(330, 184, "78");
    expect_text(330, 232, "876543");
    expect_text(602, 88, "54321");
    expect_text(602, 136, "65535");
    expect_text(602, 184, "23\xa4\xd1");
    expect_text(602, 232, "\xa1\xd1" "2");
    expect_text(250, 296, "\xa1\xd1" "5");
    expect_text(250, 344, "\xa1\xd1" "1");
    expect_text(250, 392, "\xa1\xd1" "1");
    expect_text(250, 440, "\xa1\xd1" "1");
    expect_text(330, 281, "\xa1\xd1" "4");
    expect_text(300, 385, "\xa7\xa1\xb4\x49\xa5\x64");
    expect_text(372, 385, "\xaf\x51\xc0\x74\xa5\x64");
}

static void test_property_fields(SDL_Surface *screen,
                                  const GamePreviewAssets *assets,
                                  GameSession *session)
{
    int filter;
    GameState *state = &session->state;
    memset(session, 0, sizeof(*session));
    state->player_count = 1;
    state->players[0].character = 9;
    state->price_index = 3;
    session->presentation.player_info.active = 1;
    session->presentation.player_info.tab = 1;
    state->land_count = 5;
    state->land_level[0] = 1;
    state->land_level[1] = 2;
    state->land_level[2] = 3;
    state->land_owner[2] = -1; /* same group, not owned */
    state->land_group[0] = state->land_group[1] = state->land_group[2] = 1;
    state->land_toll_by_level[0][1] = 100;
    state->land_toll_by_level[1][2] = 220;
    state->land_toll_by_level[2][3] = 999;
    state->land_type[3] = 1;
    state->land_level[3] = 2;
    state->land_type[4] = 2;
    state->land_price[0] = state->land_price[3] = 1000;
    state->land_price_per_level[0] = state->land_price_per_level[3] = 400;
    state->land_value[0] = state->land_value[3] = 777; /* not UI prices */
    state->land_lease_date[0] = (2006u << 16) | (12u << 8) | 31u;
    state->land_price_status[0] = 0x50; /* not part of the listed base fee */
    state->facility_count = 3;
    state->facility_type[0] = 1;
    state->facility_level[0] = 2;
    state->facility_price[0] = 2000;
    state->facility_price_per_level[0] = 600;
    state->facility_value[0] = 888;
    state->facility_charge_by_level[0][2] = 77;
    state->facility_level[1] = 3; /* park: listed fee is zero */
    state->facility_charge_by_level[1][3] = 99;
    state->facility_type[2] = 4; /* level 0: listed fee is zero */
    state->facility_charge_by_level[2][0] = 88;
    state->facility_last_change[0] = (2004u << 16) | (7u << 8) | 9u;
    for (filter = 0; filter < 5; filter++) {
        session->presentation.player_info.property_filter = filter;
        draw_panel(screen, assets, session);
        if (filter == 0 || filter == 1 || filter == 3) {
            expect_text(394, 144, "4200");
            expect_text(490, 144, "960"); /* (100 + 220) * 3 */
            expect_text(540, 144, "06/12/31");
        }
        if (filter == 0 || filter == 1 || filter == 4) {
            int y = filter == 4 ? 144 : 208;
            expect_text(394, y, "5400");
            expect_text(490, y, "12000"); /* two converted facilities */
        }
        if (filter == 0 || filter == 2) {
            int y = filter == 2 ? 144 : 272;
            expect_text(394, y, "9600");
            expect_text(490, y, "231");
            expect_text(540, y, "04/7/9");
            expect_text(490, y + 32, "0");
            expect_text(540, y + 32, "\xb5\x4c\xad\xad\xb4\xc1");
            expect_text(490, y + 64, "0");
        }
    }
}

static void test_stock_fields(SDL_Surface *screen,
                               const GamePreviewAssets *assets,
                               GameSession *session)
{
    memset(session, 0, sizeof(*session));
    session->state.player_count = 2;
    session->state.players[0].character = 9;
    session->state.players[1].character = 9;
    session->presentation.player_info.active = 1;
    session->presentation.player_info.tab = 2;
    session->presentation.player_info.selected_player = 1;
    session->state.player_stock_amount[0][0] = 999;
    session->state.player_stock_amount[1][0] = 10;
    session->state.player_stock_average_cost[1][0] = 99.0f;
    session->state.stocks[0].f20 = 1.3f;
    session->state.stocks[0].f16 = 33.0f;
    draw_panel(screen, assets, session);
    expect_text(384, 100, "10");
    /* x87 multiplies the stored float32 quote at extended precision,
     * then 0x457DBC truncates toward zero: 10 * float32(1.3) < 13. */
    expect_text(536, 100, "12");
}

int main(void)
{
    static SDL_Surface screen;
    static GamePreviewAssets assets;
    static GameSession session;

    memset(&screen, 0, sizeof(screen));
    memset(&assets, 0, sizeof(assets));
    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.state.players[0].character = 9;
    session.presentation.player_info.active = 1;

    /* Original 0x423293 forwards the stored name to 0x44FABC without the
     * 0x452946 space-removal helper. */
    draw_panel(&screen, &assets, &session);

    assert(header_name != NULL);
    assert(strcmp(header_name, padded_character_name) == 0);
    assert(strchr(header_name, ' ') != NULL);
    test_god_remaining_days(&screen, &assets, &session);
    test_asset_fields(&screen, &assets, &session);
    test_property_fields(&screen, &assets, &session);
    test_stock_fields(&screen, &assets, &session);
    puts("player-info panel tests passed");
    return 0;
}
