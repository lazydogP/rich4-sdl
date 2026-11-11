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

const char *new_game_character_name(int character)
{
    assert(character == 9);
    return padded_character_name;
}

const char *game_card_big5_name(int card)
{
    (void)card;
    return "";
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
    (void)session;
    return player == 0;
}

int rich4_app_player_info_selected_player(const GameSession *session,
                                          int preferred_player)
{
    (void)session;
    (void)preferred_player;
    return 0;
}

int rich4_player_total_assets(const GameState *state,
                              int player,
                              PlayerAssetBreakdown *out_breakdown)
{
    (void)state;
    (void)player;
    if (out_breakdown != NULL)
        memset(out_breakdown, 0, sizeof(*out_breakdown));
    return 0;
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
    game_draw_player_info_panel(&screen, &assets, &session);

    assert(header_name == padded_character_name);
    assert(strcmp(header_name, padded_character_name) == 0);
    assert(strchr(header_name, ' ') != NULL);
    puts("player-info name-spacing tests passed");
    return 0;
}
