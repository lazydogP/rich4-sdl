/* Original player-information modal (0x423070 / 0x424492).
 *
 * Panel.mkf[9] owns its three full-screen pages and all selector artwork;
 * Panel.mkf[74] supplies the thirteen tool icons.  This file adds only the
 * live text and numbers that the Win32 original draws over those resources. */

#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>

#include "app/game_session.h"
#include "app/map_input_flow.h"
#include "core/victory.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "view/view_constants.h"

enum {
    PLAYER_INFO_ASSETS_TAB = 0,
    PLAYER_INFO_PROPERTY_TAB = 1,
    PLAYER_INFO_STOCK_TAB = 2,
    PLAYER_INFO_TAB_COUNT = 3,
    PLAYER_INFO_PROPERTY_FILTER_COUNT = 5,
    PLAYER_INFO_PROPERTY_ROWS_PER_PAGE = 10,
    PLAYER_INFO_CHARACTER_NAME_BUFFER_SIZE = 32
};

typedef enum PlayerInfoPropertyKind {
    PLAYER_INFO_PROPERTY_LAND,
    PLAYER_INFO_PROPERTY_FACILITY
} PlayerInfoPropertyKind;

typedef struct PlayerInfoPropertyRow {
    PlayerInfoPropertyKind kind;
    int index;
} PlayerInfoPropertyRow;

static const char *const player_info_tabs[PLAYER_INFO_TAB_COUNT] = {
    "\xb8\xea\xb2\xa3\xb2\x4d\xb3\xe6", /* 資產清單 @ 0x463D98 */
    "\xa6\x61\xb2\xa3\xb2\x4d\xb3\xe6", /* 地產清單 */
    "\xaa\xd1\xb2\xbc\xb2\x4d\xb3\xe6"  /* 股票清單 */
};

static const char *const player_info_asset_labels[8] = {
    "\xb2\x7b\x20\x20\xaa\xf7", /* 現  金 */
    "\xa6\x73\x20\x20\xb4\xda", /* 存  款 */
    "\xb6\x55\x20\x20\xb4\xda", /* 貸  款 */
    "\xc1\x60\xb8\xea\xb2\xa3", /* 總資產 */
    "\xaa\xd1\x20\x20\xb2\xbc", /* 股  票 */
    "\xc2\x49\x20\x20\xa8\xf7", /* 點  卷 */
    "\xab\x4f\xc0\x49\xb4\xc1", /* 保險期 */
    "\xa5\xf8\x20\x20\xb7\x7e"  /* 企  業 */
};

static const char *const player_info_asset_counts[4] = {
    "\xa4\x67\x20\x20\xa6\x61", /* 土  地 */
    "\xb3\x5d\x20\x20\xac\x49", /* 設  施 */
    "\xa9\xd0\x20\x20\xab\xce", /* 房  屋 */
    "\xb3\x73\xc2\xea\xa9\xb1"  /* 連鎖店 */
};

static const char *const player_info_property_filters[
    PLAYER_INFO_PROPERTY_FILTER_COUNT] = {
    "\xa5\xfe\x20\x20\xb3\xa1", /* 全  部 */
    "\xa6\xed\xa6\x76\xb0\xcf", /* 住宅區 */
    "\xb0\xd3\xb7\x7e\xb0\xcf", /* 商業區 */
    "\xa9\xd0\x20\x20\xab\xce", /* 房  屋 */
    "\xb3\x73\xc2\xea\xa9\xb1"  /* 連鎖店 */
};

static const char *const player_info_stock_headers[3] = {
    "\xaa\xd1\xb2\xbc\xa6\x57\xba\xd9", /* 股票名稱 */
    "\xab\xf9\xa6\xb3\xb1\x69\xbc\xc6", /* 持有張數 */
    "\xc1\x60\x20\xa5\xab\x20\xbb\xf9"  /* 總 市 價 */
};

static const char *const player_info_property_headers[5] = {
    "\xa6\x61\x20\x20\xc2\x49",         /* 地  點 */
    "\xb6\x7d\xb5\x6f\xaa\xac\xaa\x70", /* 開發狀況 */
    "\xbb\xf9\x20\x20\xae\xe6",         /* 價  格 */
    "\xa6\xac\x20\x20\xb6\x4f",         /* 收  費 */
    "\xaf\xb2\x20\x20\xb4\xc1"          /* 租  期 */
};

static void player_info_compact_character_name(char *out_name,
                                                size_t capacity,
                                                int character)
{
    const char *source = new_game_character_name(character);
    size_t length = 0;

    if (out_name == NULL || capacity == 0) return;
    while (source != NULL && *source != '\0' && length + 1 < capacity) {
        if (*source != ' ')
            out_name[length++] = *source;
        source++;
    }
    out_name[length] = '\0';
}

/* ref_00475138 / ref_00475150: the status text in 0x4225A3. */
static const char *const player_info_land_development[6] = {
    "\xaa\xc5\x20\x20\xa6\x61", /* 空  地 */
    "\xa5\xad\x20\x20\xa9\xd0", /* 平  房 */
    "\xa9\xb1\x20\x20\xe7\x45", /* 店  舖 */
    "\xb0\xd3\x20\x20\xb3\xf5", /* 商  場 */
    "\xb0\xd3\xb7\x7e\xa4\x6a\xbc\xd3", /* 商業大樓 */
    "\xbc\xaf\xa4\xd1\xa4\x6a\xbc\xd3"  /* 摩天大樓 */
};

static const char *const player_info_facility_development[5] = {
    "\xa4\xbd\x20\x20\xb6\xe9", /* 公  園 */
    "\xae\xc8\x20\x20\xc0\x5d", /* 旅  館 */
    "\xc1\xca\xaa\xab\xa4\xa4\xa4\xdf", /* 購物中心 */
    "\xa5\x5b\xaa\x6f\xaf\xb8", /* 加油站 */
    "\xac\xe3\xa8\x73\xa9\xd2"  /* 研究所 */
};

static int player_info_god_panel_chunk(const GamePlayerState *player)
{
    /* 0x475464 maps the original player +63 god type to Panel.mkf[9].
     * Types without a panel image intentionally map to zero. */
    static const unsigned char chunks[] = {
        0, 13, 14, 15, 16, 17, 18, 19, 20, 21,
        22, 0, 23, 0, 0, 24, 24
    };
    int type;

    if (player == NULL || player->god_info == 0)
        return 0;
    type = player->god_type;
    if (type < 0 || type >= (int)(sizeof(chunks) / sizeof(chunks[0])))
        return 0;
    return chunks[type];
}

static void player_info_draw_number(SDL_Surface *screen,
                                    int x,
                                    int y,
                                    int value,
                                    int height)
{
    char text[40];
    format_hud_number(text, sizeof(text), value, 0);
    draw_big5_text(screen, x, y, text, height, 0x1082, 0, 2, 6);
}

static void player_info_draw_count(SDL_Surface *screen,
                                   int x,
                                   int y,
                                   int value,
                                   int height,
                                   uint16_t foreground,
                                   uint16_t shadow,
                                   int style)
{
    char text[24];
    snprintf(text, sizeof(text), "\xa1\xd1%d", value); /* ×%d */
    draw_big5_text(screen, x, y, text, height, foreground, shadow, style, 6);
}

static void player_info_draw_plain_number(SDL_Surface *screen,
                                          int x,
                                          int y,
                                          int value,
                                          int height)
{
    char text[24];
    snprintf(text, sizeof(text), "%d", value);
    draw_big5_text(screen, x, y, text, height, 0x1082, 0, 2, 6);
}

static void player_info_draw_days(SDL_Surface *screen,
                                  int x,
                                  int y,
                                  int value)
{
    char text[24];
    snprintf(text, sizeof(text), "%d\xa4\xd1", value); /* %d天 */
    draw_big5_text(screen, x, y, text, 28, 0x1082, 0, 2, 6);
}

static int player_info_company_count(const GameState *state, int player)
{
    int count = 0;
    int i;
    if (state == NULL) return 0;
    for (i = 0; i < state->company_count && i < GAME_MAX_MAP_PROPERTIES; i++)
        if (state->company_owner[i] == player) count++;
    return count;
}

static void player_info_draw_header(SDL_Surface *screen,
                                    const GamePreviewAssets *assets,
                                    const GameSession *session,
                                    int selected)
{
    int i;
    int slot = 0;
    const GamePlayerState *player = &session->state.players[selected];
    int character = player->character;
    int god_chunk = player_info_god_panel_chunk(player);

    for (i = 0; i < session->state.player_count && i < GAME_MAX_PLAYERS; i++) {
        char name[PLAYER_INFO_CHARACTER_NAME_BUFFER_SIZE];
        if (!rich4_app_player_info_player_is_visible(session, i)) continue;
        draw_game_panel_chunk(screen, assets, 9,
                              (size_t)(i == selected ? 3 : 4),
                              16 + slot * 88, 14, 1);
        player_info_compact_character_name(
            name, sizeof(name), session->state.players[i].character);
        draw_big5_text(screen, 60 + slot * 88, 30,
                       name, 20, 0x1082, 0, 2, 2);
        slot++;
    }
    if (character >= 0 && character < 12 &&
        assets->character.has_map_character[character]) {
        draw_spr_smp_center(
            screen, assets->character.map_character_resource[character].data,
            assets->character.map_character_resource[character].size,
            &assets->character.map_character_spr[character], 0, 60, 100);
    }
    if (god_chunk != 0)
        draw_game_panel_chunk_at(screen, assets, 9, (size_t)god_chunk,
                                 60, 188, 1);
    if (god_chunk != 0) {
        char text[24];
        /* 0x42315F..0x4231B2: the god panel also carries its remaining
         * duration, centred below the portrait at (60,234). */
        snprintf(text, sizeof(text), "%d\xa4\xd1", player->god_status_days);
        draw_big5_text(screen, 60, 234, text, 16, 0x1082, 0, 2, 2);
    }
    draw_game_panel_chunk_at(
        screen, assets, 9,
        session->presentation.player_info.pressed_control == 1 ? 6 : 5,
        547, 23, 1);
}

static void player_info_draw_tab_selectors(SDL_Surface *screen,
                                            const GamePreviewAssets *assets,
                                            int active_tab,
                                            int pressed_control)
{
    int i;
    for (i = 0; i < PLAYER_INFO_TAB_COUNT; i++) {
        int y = 282 + i * 64;
        if (pressed_control == i + 2)
            draw_game_panel_chunk(screen, assets, 9, 12, 12, y, 1);
        draw_big5_text(screen, 61, y + 21, player_info_tabs[i], 20,
                       i == active_tab ? 0xffff : 0xc618, 0x1082, 3, 2);
    }
}

static void player_info_draw_asset_page(SDL_Surface *screen,
                                        const GamePreviewAssets *assets,
                                        const GameState *state,
                                        int player)
{
    const GamePlayerState *record = &state->players[player];
    PlayerAssetBreakdown asset_breakdown;
    int total_land = 0;
    int facility_count = 0;
    int house_count = 0;
    int chain_count = 0;
    int i;

    for (i = 0; i < 4; i++)
        draw_big5_text(screen, 142, 88 + i * 48, player_info_asset_labels[i],
                       20, 0x1082, 0, 2, 2);
    for (i = 0; i < 4; i++)
        draw_big5_text(screen, 430, 88 + i * 48,
                       player_info_asset_labels[4 + i], 20, 0x1082, 0, 2, 2);
    for (i = 0; i < 4; i++)
        draw_big5_text(screen, 142, 296 + i * 48, player_info_asset_counts[i],
                       20, 0x1082, 0, 2, 2);

    (void)rich4_player_total_assets(state, player, &asset_breakdown);
    player_info_draw_number(screen, 330, 88, record->cash, 28);
    player_info_draw_number(screen, 330, 136, record->bank, 28);
    player_info_draw_number(screen, 330, 184, record->loan, 28);
    player_info_draw_number(screen, 330, 232, asset_breakdown.total, 28);
    player_info_draw_number(screen, 602, 88, asset_breakdown.stock_value, 28);
    player_info_draw_plain_number(screen, 602, 136, record->points, 28);
    player_info_draw_days(screen, 602, 184, record->days_assurance);
    player_info_draw_count(screen, 602, 232,
                           player_info_company_count(state, player), 28,
                           0x1082, 0, 2);

    for (i = 0; i < state->land_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        if (state->land_owner[i] != player) continue;
        total_land++;
        if (state->land_type[i] != 0)
            facility_count++;
        else if (state->land_type[i] == 0 && state->land_level[i] != 0)
            house_count++;
    }
    for (i = 0; i < state->facility_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        if (state->facility_owner[i] != player) continue;
        total_land++;
        if (state->facility_level[i] != 0) chain_count++;
    }
    player_info_draw_count(screen, 250, 296, total_land, 28, 0x1082, 0, 2);
    player_info_draw_count(screen, 250, 344, facility_count, 28,
                           0x1082, 0, 2);
    player_info_draw_count(screen, 250, 392, house_count, 28, 0x1082, 0, 2);
    player_info_draw_count(screen, 250, 440, chain_count, 28, 0x1082, 0, 2);

    /* 0x4236C6 scans the 13 tool slots, lays them out in five columns, and
     * gets the icons from the separately loaded Panel.mkf[74]. */
    {
        int x = 300;
        int y = 281;
        for (i = 0; i < GAME_TOOL_COUNT; i++) {
            int amount = record->tool_inventory[i];
            if (amount == 0) continue;
            draw_game_panel_chunk_at(screen, assets, 74, (size_t)i,
                                     x - 16, y, 1);
            player_info_draw_count(screen, x + 30, y, amount, 16,
                                   0xffff, 0x1082, 3);
            x += 72;
            if (x > 588) {
                x = 300;
                y += 32;
            }
        }
    }
    {
        int x = 300;
        int y = 385;
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
            int card = record->card_inventory[i] - 1;
            if (card < 0 || card >= GAME_CARD_COUNT) continue;
            draw_big5_text(screen, x, y, game_card_big5_name(card), 16,
                           0xffff, 0x1082, 3, 2);
            x += 72;
            if (x > 588) {
                x = 300;
                y += 32;
            }
        }
    }
}

static int player_info_property_matches(const GameState *state,
                                        PlayerInfoPropertyRow row,
                                        int player,
                                        int filter)
{
    if (row.kind == PLAYER_INFO_PROPERTY_LAND) {
        int type = state->land_type[row.index];
        int level = state->land_level[row.index];
        if (state->land_owner[row.index] != player) return 0;
        if (filter == 0 || filter == 1) return 1;
        if (filter == 3) return level != 0 && type == 0;
        if (filter == 4) return level != 0 && type != 0;
        return 0;
    }
    return state->facility_owner[row.index] == player &&
           (filter == 0 || filter == 2);
}

static int player_info_collect_properties(const GameState *state,
                                          int player,
                                          int filter,
                                          PlayerInfoPropertyRow *out_rows,
                                          int capacity)
{
    int count = 0;
    int i;
    if (state == NULL || out_rows == NULL || capacity <= 0) return 0;
    for (i = 0; i < state->land_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        PlayerInfoPropertyRow row = { PLAYER_INFO_PROPERTY_LAND, i };
        if (player_info_property_matches(state, row, player, filter) &&
            count < capacity)
            out_rows[count++] = row;
    }
    for (i = 0; i < state->facility_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        PlayerInfoPropertyRow row = { PLAYER_INFO_PROPERTY_FACILITY, i };
        if (player_info_property_matches(state, row, player, filter) &&
            count < capacity)
            out_rows[count++] = row;
    }
    return count;
}

static void player_info_property_name(char out_name[21],
                                      const MapData *map,
                                      PlayerInfoPropertyRow row)
{
    const unsigned char *source = NULL;
    memset(out_name, 0, 21);
    if (map == NULL) return;
    if (row.kind == PLAYER_INFO_PROPERTY_LAND &&
        (size_t)(row.index + 1) < map->land_count)
        source = map->lands[row.index + 1].raw + 4;
    else if (row.kind == PLAYER_INFO_PROPERTY_FACILITY &&
             (size_t)(row.index + 1) < map->facility_count)
        source = map->facilities[row.index + 1].raw + 4;
    if (source != NULL) memcpy(out_name, source, 20);
}

static const char *player_info_property_kind(const GameState *state,
                                             PlayerInfoPropertyRow row)
{
    if (row.kind == PLAYER_INFO_PROPERTY_FACILITY) {
        int level = state->facility_level[row.index];
        int type = state->facility_type[row.index];
        if (level == 0) return player_info_land_development[0];
        if (type < 0 || type >= 5) type = 0;
        return player_info_facility_development[type];
    }
    if (state->land_type[row.index] != 0)
        return "\xb3\x73\xc2\xea\xa9\xb1"; /* 連鎖店 */
    {
        int level = state->land_level[row.index];
        if (level < 0) level = 0;
        if (level > 5) level = 5;
        return player_info_land_development[level];
    }
}

static void player_info_draw_property_page(SDL_Surface *screen,
                                           const GamePreviewAssets *assets,
                                           const GameSession *session,
                                           int player)
{
    PlayerInfoPropertyRow rows[GAME_MAX_MAP_PROPERTIES * 2];
    const GameState *state = &session->state;
    const MapData *map = NULL;
    int filter = session->presentation.player_info.property_filter;
    int page = session->presentation.player_info.property_page;
    int count;
    int i;
    int background = state->stage * GAME_MAPS_PER_STAGE + state->map;

    if (filter < 0 || filter >= PLAYER_INFO_PROPERTY_FILTER_COUNT) filter = 0;
    if (page < 0) page = 0;
    if (background >= 0 && background < GAME_BACKGROUND_COUNT)
        map = &assets->map.maps[background];
    for (i = 0; i < PLAYER_INFO_PROPERTY_FILTER_COUNT; i++) {
        int x = 120 + i * 75;
        if (i == filter)
            draw_game_panel_chunk(screen, assets, 9, 11, x, 64, 1);
        draw_big5_text(screen, x + 37, 80, player_info_property_filters[i],
                       20, 0x1082, 0, 2, 2);
    }
    if (session->presentation.player_info.pressed_control == 5)
        draw_game_panel_chunk(screen, assets, 9, 7, 593, 369, 1);
    else if (session->presentation.player_info.pressed_control == 6)
        draw_game_panel_chunk(screen, assets, 9, 8, 593, 417, 1);
    for (i = 0; i < 5; i++) {
        int x = i == 0 ? 168 : i == 1 ? 264 : i == 2 ? 356 :
            i == 3 ? 448 : 540;
        draw_big5_text(screen, x, 112, player_info_property_headers[i],
                       20, 0x1082, 0, 2, 2);
    }

    count = player_info_collect_properties(state, player, filter, rows,
                                           (int)(sizeof(rows) / sizeof(rows[0])));
    for (i = page * PLAYER_INFO_PROPERTY_ROWS_PER_PAGE;
         i < count && i < (page + 1) * PLAYER_INFO_PROPERTY_ROWS_PER_PAGE;
         i++) {
        PlayerInfoPropertyRow row = rows[i];
        char name[21];
        int y = 144 + (i - page * PLAYER_INFO_PROPERTY_ROWS_PER_PAGE) * 32;
        int price;
        int fee;
        if (row.kind == PLAYER_INFO_PROPERTY_LAND) {
            int level = state->land_level[row.index];
            if (level < 0) level = 0;
            if (level > 5) level = 5;
            price = state->land_value[row.index];
            fee = state->land_toll_by_level[row.index][level];
        } else {
            int level = state->facility_level[row.index];
            if (level < 0) level = 0;
            if (level > 5) level = 5;
            price = state->facility_value[row.index];
            fee = state->facility_charge_by_level[row.index][level];
        }
        player_info_property_name(name, map, row);
        draw_big5_text(screen, 168, y, name,
                       16, 0x1082, 0, 2, 2);
        draw_big5_text(screen, 264, y, player_info_property_kind(state, row),
                       16, 0x1082, 0, 2, 2);
        player_info_draw_number(screen, 394, y, price, 16);
        player_info_draw_number(screen, 490, y, fee, 16);
        draw_big5_text(screen, 540, y, "\xb5\x4c\xad\xad\xb4\xc1", /* 無限期 */
                       16, 0x1082, 0, 2, 2);
    }
}

static void player_info_draw_stock_page(SDL_Surface *screen,
                                        const GameState *state,
                                        int player)
{
    int i;
    for (i = 0; i < 3; i++)
        draw_big5_text(screen, (i == 0 ? 204 : i == 1 ? 332 : 476), 68,
                       player_info_stock_headers[i], 20, 0x1082, 0, 2, 2);
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        int y = 100 + i * 32;
        int amount = state->player_stock_amount[player][i];
        int market_value = (int)(amount * state->stocks[i].f20);
        draw_big5_text(screen, 204, y, game_stock_name_for_state(state, i),
                       16, 0x1082, 0, 2, 2);
        player_info_draw_number(screen, 384, y, amount, 16);
        player_info_draw_number(screen, 536, y, market_value, 16);
    }
}

void game_draw_player_info_panel(SDL_Surface *screen,
                                 const GamePreviewAssets *assets,
                                 const GameSession *session)
{
    int selected;
    int tab;

    if (screen == NULL || assets == NULL || session == NULL ||
        !session->presentation.player_info.active ||
        session->state.player_count <= 0)
        return;
    selected = rich4_app_player_info_selected_player(
        session, session->presentation.player_info.selected_player);
    if (selected < 0) return;
    tab = session->presentation.player_info.tab;
    if (tab < 0 || tab >= PLAYER_INFO_TAB_COUNT) tab = PLAYER_INFO_ASSETS_TAB;

    if (!draw_game_panel_chunk(screen, assets, 9, (size_t)tab, 0, 0, 0)) {
        SDL_LockSurface(screen);
        fill_rect(screen, 0, 0, SCREEN_W, SCREEN_H, 0x2104);
        SDL_UnlockSurface(screen);
    }
    player_info_draw_header(screen, assets, session, selected);
    player_info_draw_tab_selectors(
        screen, assets, tab,
        session->presentation.player_info.pressed_control);
    if (tab == PLAYER_INFO_ASSETS_TAB)
        player_info_draw_asset_page(screen, assets, &session->state, selected);
    else if (tab == PLAYER_INFO_PROPERTY_TAB)
        player_info_draw_property_page(screen, assets, session, selected);
    else
        player_info_draw_stock_page(screen, &session->state, selected);
}
