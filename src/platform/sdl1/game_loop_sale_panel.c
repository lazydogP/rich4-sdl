#include "platform/sdl1/game_lazy_assets.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <SDL/SDL.h>

#include "app/sale_interaction_flow.h"
#include "core/property.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_bank_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/spr_smp.h"
#include "view/view_constants.h"

enum {
    SALE_TEXT_LIGHT = 0xf79e,
    SALE_MARKET_GRID_TOP = 114,
    SALE_MARKET_GRID_STEP = 72,
    SALE_PROPERTY_VISIBLE_ROWS = 11,
    SALE_HIT_CLOSE = 1,
    SALE_HIT_SELL = 2,
    SALE_HIT_ROW_BASE = 100,
    SALE_HIT_PREVIOUS = 200,
    SALE_HIT_NEXT = 201,
    SALE_HIT_CONFIRM_NO = 203,
    SALE_HIT_CONFIRM_YES = 204,
    SALE_HIT_GRID_BASE = 300,
    SALE_HIT_PRICE_KEY_BASE = 400
};

static Uint32 sale_notice_started_at;
static int sale_notice_message;
static int sale_notice_seller;
static GameMarketListing sale_notice_listing;

static int sale_draw_panel_chunk(SDL_Surface *screen,
                                 const GamePreviewAssets *assets,
                                 size_t chunk, int x, int y)
{
    /* 0x4563F5 copies the sale panel rectangles as opaque SMP chunks. */
    return draw_game_panel_chunk_at(screen, assets, 73, chunk, x, y, 0);
}

static int sale_panel_chunk_width(const GamePreviewAssets *assets,
                                 size_t chunk_index)
{
    Rich4SprSmpChunk chunk;
    if (assets == NULL || !game_asset_ready(assets, &assets->ui.has_panel[73]) ||
        rich4_spr_smp_get_chunk(
            assets->ui.panel_data[73], assets->ui.panel_size[73],
            &assets->ui.panel_spr[73], chunk_index,
            &chunk) != RICH4_SPR_SMP_OK)
        return 0;
    return chunk.width;
}

static int sale_stock_panel_width(const GamePreviewAssets *assets)
{
    return sale_panel_chunk_width(assets, 1);
}

static int sale_property_panel_width(const GamePreviewAssets *assets)
{
    return sale_panel_chunk_width(assets, 2);
}

static void sale_confirm_dialog_size(const GamePreviewAssets *assets,
                                     int *out_width, int *out_height)
{
    int left;
    int top;
    if (out_width != NULL) *out_width = 0;
    if (out_height != NULL) *out_height = 0;
    (void)game_confirm_dialog_bounds(
        assets, GAME_COMMAND_CONFIRM, &left, &top,
        out_width, out_height);
}

static Rich4StockQuantityMask sale_quantity_mask(
    const GamePreviewAssets *assets)
{
    Rich4StockQuantityMask mask;
    mask.data = NULL;
    mask.size = 0;
    mask.available = 0;
    if (assets != NULL && game_asset_ready(assets, &assets->ui.has_panel_raw[22]) &&
        assets->ui.panel_data[22] != NULL) {
        mask.data = assets->ui.panel_data[22];
        mask.size = assets->ui.panel_size[22];
        mask.available = 1;
    }
    return mask;
}

static void sale_draw_text(SDL_Surface *screen, int x, int y,
                           const char *text, int height, uint16_t foreground,
                           uint16_t shadow, int style, int anchor)
{
    if (screen == NULL || text == NULL) return;
    draw_big5_text(screen, x, y, text, height, foreground, shadow,
                   style, anchor);
}

static Uint32 sale_read_pixel(const SDL_Surface *screen, int x, int y)
{
    const Uint8 *pixel = (const Uint8 *)screen->pixels +
        y * screen->pitch + x * screen->format->BytesPerPixel;
    Uint32 value = 0;
    switch (screen->format->BytesPerPixel) {
    case 1:
        value = pixel[0];
        break;
    case 2: {
        Uint16 word;
        memcpy(&word, pixel, sizeof(word));
        value = word;
        break;
    }
    case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        value = ((Uint32)pixel[0] << 16) |
                 ((Uint32)pixel[1] << 8) | pixel[2];
#else
        value = pixel[0] | ((Uint32)pixel[1] << 8) |
                ((Uint32)pixel[2] << 16);
#endif
        break;
    default:
        memcpy(&value, pixel, sizeof(value));
        break;
    }
    return value;
}

static void sale_write_pixel(SDL_Surface *screen, int x, int y, Uint32 value)
{
    Uint8 *pixel = (Uint8 *)screen->pixels +
        y * screen->pitch + x * screen->format->BytesPerPixel;
    switch (screen->format->BytesPerPixel) {
    case 1:
        pixel[0] = (Uint8)value;
        break;
    case 2: {
        Uint16 word = (Uint16)value;
        memcpy(pixel, &word, sizeof(word));
        break;
    }
    case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        pixel[0] = (Uint8)(value >> 16);
        pixel[1] = (Uint8)(value >> 8);
        pixel[2] = (Uint8)value;
#else
        pixel[0] = (Uint8)value;
        pixel[1] = (Uint8)(value >> 8);
        pixel[2] = (Uint8)(value >> 16);
#endif
        break;
    default:
        memcpy(pixel, &value, sizeof(value));
        break;
    }
}

static Uint32 sale_remap_pixel(SDL_Surface *screen, Uint32 pixel,
                               const Uint8 remap[32])
{
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    int r5;
    int g5;
    int b5;
    SDL_GetRGB(pixel, screen->format, &red, &green, &blue);
    r5 = (red * 31 + 127) / 255;
    g5 = (green * 31 + 127) / 255;
    b5 = (blue * 31 + 127) / 255;
    red = (Uint8)((remap[r5] * 255 + 15) / 31);
    green = (Uint8)((remap[g5] * 255 + 15) / 31);
    blue = (Uint8)((remap[b5] * 255 + 15) / 31);
    return SDL_MapRGB(screen->format, red, green, blue);
}

static void sale_tint_rect(SDL_Surface *screen, int x, int y,
                           int width, int height)
{
    static const Uint8 pressed_color[32] = {
        0, 1, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7, 7, 8, 9, 9,
        10, 10, 11, 12, 12, 13, 13, 14, 15, 15, 16, 17, 17, 18, 18, 19
    };
    int x0;
    int y0;
    int x1;
    int y1;
    int row;
    if (screen == NULL || width <= 0 || height <= 0 ||
        SDL_LockSurface(screen) != 0)
        return;
    x0 = x < 0 ? 0 : x;
    y0 = y < 0 ? 0 : y;
    x1 = x + width > screen->w ? screen->w : x + width;
    y1 = y + height > screen->h ? screen->h : y + height;
    for (row = y0; row < y1; row++) {
        int column;
        for (column = x0; column < x1; column++) {
            Uint32 pixel = sale_read_pixel(screen, column, row);
            sale_write_pixel(screen, column, row,
                sale_remap_pixel(screen, pixel, pressed_color));
        }
    }
    SDL_UnlockSurface(screen);
}

static void sale_press_rect(SDL_Surface *screen, int x, int y,
                            int width, int height)
{
    static const Uint8 bevel_color[32] = {
        0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,
        8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15
    };
    Uint32 pixels[72 * 72];
    int row;
    int column;
    if (screen == NULL || width <= 0 || height <= 0 ||
        width > 72 || height > 72 || x < 0 || y < 0 ||
        x + width > screen->w || y + height > screen->h ||
        SDL_LockSurface(screen) != 0)
        return;
    for (row = 0; row < height; row++)
        for (column = 0; column < width; column++)
            pixels[row * width + column] =
                sale_read_pixel(screen, x + column, y + row);

    /* This matches sub_451B9E's one-pixel diagonal press shift and the
     * sub_4552E7(-16) darkening on its top and left bevels. */
    for (row = 0; row < height - 1; row++)
        for (column = 0; column < width - 1; column++)
            sale_write_pixel(screen, x + column, y + row,
                             pixels[(row + 1) * width + column + 1]);
    for (row = 0; row < height - 1; row++)
        sale_write_pixel(screen, x + width - 1, y + row,
                         pixels[row * width + width - 1]);
    for (column = 0; column < width; column++)
        sale_write_pixel(screen, x + column, y + height - 1,
                         pixels[(height - 1) * width + column]);
    for (column = 0; column < width; column++)
        sale_write_pixel(screen, x + column, y,
            sale_remap_pixel(screen, sale_read_pixel(
                screen, x + column, y), bevel_color));
    for (row = 1; row < height - 1; row++)
        sale_write_pixel(screen, x, y + row,
            sale_remap_pixel(screen, sale_read_pixel(screen, x, y + row),
                             bevel_color));
    SDL_UnlockSurface(screen);
}

static void sale_draw_number(SDL_Surface *screen, int x, int y, int value,
                             int currency_unit)
{
    char text[32];
    format_hud_number(text, sizeof(text) - 2, value, 0);
    if (currency_unit) strcat(text, "\xa4\xb8");
    sale_draw_text(screen, x, y, text, 16, SALE_TEXT_LIGHT, 0x1082, 3, 6);
}

static int sale_listing_item_ref(const GameMarketListing *listing,
                                 GameDomainRef *out_item)
{
    if (listing == NULL || out_item == NULL) return 0;
    switch (listing->type) {
    case GAME_MARKET_LISTING_STOCK:
        *out_item = rich4_game_domain_ref(
            GAME_DOMAIN_STOCK, (int)listing->item_id);
        break;
    case GAME_MARKET_LISTING_ASSET:
        if (listing->item_id > 0x7d0 && listing->item_id < 0xfa0)
            *out_item = rich4_game_domain_ref(
                GAME_DOMAIN_LAND, (int)listing->item_id - 0x7d1);
        else if (listing->item_id >= 0xfa1 && listing->item_id < 0x1770)
            *out_item = rich4_game_domain_ref(
                GAME_DOMAIN_FACILITY, (int)listing->item_id - 0xfa1);
        else
            return 0;
        break;
    case GAME_MARKET_LISTING_CARD:
        if (listing->item_id == 0) return 0;
        *out_item = rich4_game_domain_ref(
            GAME_DOMAIN_CARD, (int)listing->item_id - 1);
        break;
    case GAME_MARKET_LISTING_TOOL:
        if (listing->item_id == 0) return 0;
        *out_item = rich4_game_domain_ref(
            GAME_DOMAIN_TOOL, (int)listing->item_id - 1);
        break;
    default:
        return 0;
    }
    return rich4_game_domain_ref_is_valid(*out_item);
}

static void sale_item_label(const GamePreviewAssets *assets,
                            const GameSession *session,
                            GameDomainRef item, char *out, size_t out_size)
{
    const char *stock_name;
    if (out == NULL || out_size == 0) return;
    switch (item.kind) {
    case GAME_DOMAIN_STOCK:
        stock_name = session != NULL
            ? game_stock_name_for_state(&session->state, item.id) : NULL;
        snprintf(out, out_size, "%s", stock_name != NULL ? stock_name : "");
        break;
    case GAME_DOMAIN_LAND:
    case GAME_DOMAIN_FACILITY: {
        const unsigned char *source = NULL;
        int background;
        if (assets != NULL && session != NULL) {
            background = session->state.stage * GAME_MAPS_PER_STAGE +
                         session->state.map;
            if (background >= 0 && background < GAME_BACKGROUND_COUNT) {
                const MapData *map = &assets->map.maps[background];
                if (item.kind == GAME_DOMAIN_LAND && map->lands != NULL &&
                    item.id >= 0 &&
                    (size_t)(item.id + 1) < map->land_count) {
                    source = map->lands[item.id + 1].raw + 4;
                } else if (item.kind == GAME_DOMAIN_FACILITY &&
                           map->facilities != NULL &&
                           item.id >= 0 &&
                           (size_t)(item.id + 1) < map->facility_count) {
                    source = map->facilities[item.id + 1].raw + 4;
                }
            }
        }
        if (source != NULL) {
            size_t length = out_size < 21 ? out_size - 1 : 20;
            memcpy(out, source, length);
            out[length] = '\0';
        } else if (item.kind == GAME_DOMAIN_LAND) {
            snprintf(out, out_size, "\xa6\x61\xb2\xa3 %03d", item.id + 1);
        } else {
            snprintf(out, out_size, "\xb3\x5d\xac\x49 %03d", item.id + 1);
        }
        break;
    }
    case GAME_DOMAIN_CARD:
        snprintf(out, out_size, "%s", game_card_big5_name(item.id));
        break;
    case GAME_DOMAIN_TOOL:
        snprintf(out, out_size, "%s", game_tool_big5_name(item.id));
        break;
    default:
        snprintf(out, out_size,
                 "\xa5\xbc\xaa\xbe\xaa\xab\xab\x7e"); /* 未知物品 */
        break;
    }
}

static void sale_draw_portrait(SDL_Surface *screen,
                               const GamePreviewAssets *assets,
                               const GamePlayerState *player, int row)
{
    int character = player->character;
    Rich4SprSmpChunk chunk;
    unsigned char gray[85 * 76 * 2];
    const unsigned char *data;
    int i;
    if (character < 0 || character >= 12 ||
        !assets->character.has_map_character[character]) return;
    data = assets->character.map_character_data[character];
    if (player->play_state || player->engineering_saved_traffic_method) {
        draw_spr_smp_center(screen, data,
            assets->character.map_character_size[character],
            &assets->character.map_character_spr[character],
            0, 66, 150 + row * 72);
        return;
    }
    /* 0x407FD6 / 0x455480: inactive portraits retain transparency and use
     * (R5 + G5 + B5 + 16) / 4 for all three channels. Never mutate shared art. */
    if (memcmp(assets->character.map_character_spr[character].signature,
               "SMP", 4) != 0 ||
        rich4_spr_smp_get_chunk(data,
            assets->character.map_character_size[character],
            &assets->character.map_character_spr[character], 0, &chunk)
            != RICH4_SPR_SMP_OK || chunk.data_size > sizeof(gray)) return;
    for (i = 0; i < chunk.width * chunk.height; ++i) {
        unsigned int pixel = data[chunk.data_offset + 2*i] |
                            data[chunk.data_offset + 2*i + 1] << 8;
        if (pixel) {
            unsigned int value = ((pixel & 31) + ((pixel >> 5) & 31) +
                                  ((pixel >> 10) & 31) + 16) >> 2;
            pixel = value | (value << 5) | (value << 10);
        }
        gray[2*i] = (unsigned char)pixel;
        gray[2*i+1] = (unsigned char)(pixel >> 8);
    }
    chunk.data_offset = 0;
    draw_chunk_at(screen, gray, &chunk, 66, 150 + row * 72, 1);
}

static void sale_draw_browse_rows(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *ui)
{
    int player;
    if (session == NULL) return;
    for (player = 0; player < session->state.player_count &&
                      player < GAME_MAX_PLAYERS; player++) {
        int slot;
        int slot_x = 104;
        int row_y = SALE_MARKET_GRID_TOP + player * SALE_MARKET_GRID_STEP;
        int sex_group = session->state.players[player].sex == 0 ? 0 : 1;
        /* 0x428817: Map.mkf[27+character], chunk 0, anchor (44,84+72*p)
         * inside Panel[73] at (22,66), even when there are no listings. */
        sale_draw_portrait(screen, assets, &session->state.players[player], player);

        for (slot = 0; slot < GAME_MARKET_LISTING_SLOT_COUNT; slot++) {
            const GameMarketListing *listing =
                &session->state.market_listings[player][slot];
            if (listing->type == GAME_MARKET_LISTING_EMPTY) continue;
            sale_draw_panel_chunk(
                screen, assets,
                (size_t)(8 + sex_group * 4 + (int)listing->type),
                slot_x, row_y);
            if (ui != NULL && ui->selected_seller == player &&
                ui->selected_slot == slot &&
                ui->pressed_control == SALE_HIT_GRID_BASE +
                    player * GAME_MARKET_LISTING_SLOT_COUNT + slot)
                sale_tint_rect(screen, slot_x, row_y, 72, 72);
            slot_x += SALE_MARKET_GRID_STEP;
        }
    }
}

static void sale_draw_stock_page(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *ui)
{
    int player;
    int row;
    int count;
    if (session == NULL ||
        !sale_draw_panel_chunk(screen, assets, 1, 152, 32))
        return;
    /* 0x42853D..0x42857B writes these labels onto Panel[73] chunk 1. */
    sale_draw_text(screen, 200, 48, "\xaa\xd1\xb2\xbc\xa6\x57\xba\xd9",
                   16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
    sale_draw_text(screen, 296, 48, "\xab\xf9\xa6\xb3\xb1\x69\xbc\xc6",
                   16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
    sale_draw_text(screen, 402, 48, "\xc1\x60 \xa5\xab \xbb\xf9",
                   16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
    player = session->state.current_player;
    if (player < 0 || player >= session->state.player_count) return;
    count = rich4_app_sale_item_count(
        session, RICH4_SALE_CATEGORY_STOCK);
    for (row = 0; row < count && row < GAME_STOCK_COUNT; row++) {
        GameDomainRef item;
        int maximum;
        int y = 80 + row * 32;
        if (!rich4_app_sale_item_at(session, RICH4_SALE_CATEGORY_STOCK,
                                    row, &item, &maximum))
            continue;
        sale_draw_text(screen, 200, y,
                       game_stock_name_for_state(&session->state, item.id),
                       16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
        sale_draw_number(screen, 332, y, maximum, 0);
        sale_draw_number(screen, 478, y,
            rich4_app_sale_market_value(&session->state, item, maximum), 0);
    }
    if (ui != NULL && ui->selected_row >= 0 && ui->selected_row < 12 &&
        SDL_LockSurface(screen) == 0) {
        /* IDA sub_4258C1 frames all 12 stock rows, even empty rows. */
        stroke_rect_thin(screen, 152, 64 + 32 * ui->selected_row,
                         sale_stock_panel_width(assets), 32, 0xffff);
        SDL_UnlockSurface(screen);
    }
    if (ui != NULL && ui->pressed_control == SALE_HIT_CLOSE)
        sale_press_rect(screen, 463, 38, 21, 21);
}

static void sale_draw_property_page(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *ui)
{
    static const char *const filter_labels[] = {
        "\xa5\xfe\x20\x20\xb3\xa1", /* 全  部 */
        "\xa6\xed\xa6\x76\xb0\xcf", /* 住宅區 */
        "\xb0\xd3\xb7\x7e\xb0\xcf", /* 商業區 */
        "\xa9\xd0\x20\x20\xab\xce", /* 房  屋 */
        "\xb3\x73\xc2\xea\xa9\xb1"  /* 連鎖店 */
    };
    static const char *const headers[] = {
        "\xa6\x61  \xc2\x49", "\xb6\x7d\xb5\x6f\xaa\xac\xaa\x70",
        "\xbb\xf9  \xae\xe6", "\xa6\xac  \xb6\x4f",
        "\xaf\xb2  \xb4\xc1"
    };
    static const int column_x[] = {147, 231, 319, 395, 471};
    const GameState *state = &session->state;
    int filter = ui != NULL && ui->selected_slot >= 0 &&
                 ui->selected_slot < 5 ? ui->selected_slot : 0;
    int count = rich4_app_sale_item_count(
        session, RICH4_SALE_CATEGORY_PROPERTY);
    int row;
    int i;
    if (!sale_draw_panel_chunk(screen, assets, 2, 112, 32))
        return;
    sale_draw_panel_chunk(screen, assets, 19, 112 + filter * 80, 32);
    /* 0x42877F/0x4287BD and 0x424C33. */
    for (i = 0; i < 5; i++) {
        sale_draw_text(screen, 152 + i * 80, 48,
                       filter_labels[i], 20, 0x1082, 0, 2, 2);
        sale_draw_text(screen, column_x[i], 80, headers[i],
                       20, 0xffff, 0, 2, 2);
    }
    for (row = 0; row < SALE_PROPERTY_VISIBLE_ROWS; row++) {
        GameDomainRef item;
        int maximum;
        int ordinal = (ui != NULL ? ui->page_offset : 0) + row;
        int y = 112 + row * 32;
        const char *development;
        const char *lease;
        uint32_t lease_date;
        int charge = 0;
        char text[32];
        char label[96];
        if (ordinal >= count || !rich4_app_sale_item_at(
                session, RICH4_SALE_CATEGORY_PROPERTY, ordinal,
                &item, &maximum))
            break;
        sale_item_label(assets, session, item, label, sizeof(label));
        if (item.kind == GAME_DOMAIN_LAND) {
            development = game_land_development_big5_name(
                state->land_type[item.id], state->land_level[item.id]);
            (void)rich4_land_base_toll(state, item.id, &charge);
            lease_date = state->land_lease_date[item.id];
        } else {
            int level = state->facility_level[item.id];
            int type = state->facility_type[item.id];
            development = game_facility_big5_name(type, level);
            if (type != 0 && level > 0 && level <= 5) {
                long long fee = (long long)state->price_index *
                    state->facility_charge_by_level[item.id][level];
                charge = fee > INT_MAX ? INT_MAX : (int)fee;
            }
            lease_date = state->facility_last_change[item.id];
        }
        if (lease_date != 0) {
            snprintf(text, sizeof(text), "%02u/%u/%u",
                     (lease_date >> 16) % 100u,
                     (lease_date >> 8) & 0x0fu, lease_date & 0xffu);
            lease = text;
        } else {
            lease = "\xb5\x4c\xad\xad\xb4\xc1"; /* 無限期 @ 0x463E42 */
        }
        sale_draw_text(screen, column_x[0], y, label, 16,
                       SALE_TEXT_LIGHT, 0x1082, 3, 2);
        sale_draw_text(screen, column_x[1], y, development, 16,
                       SALE_TEXT_LIGHT, 0x1082, 3, 2);
        sale_draw_number(screen, column_x[2] + 33, y,
                         rich4_app_sale_market_value(state, item, 0), 0);
        sale_draw_number(screen, column_x[3] + 29, y, charge, 0);
        sale_draw_text(screen, column_x[4], y, lease, 16,
                       SALE_TEXT_LIGHT, 0x1082, 3, 2);
    }
    if (ui != NULL && ui->selected_row >= 0 &&
        ui->selected_row < SALE_PROPERTY_VISIBLE_ROWS &&
        SDL_LockSurface(screen) == 0) {
        /* IDA sub_42608F frames the row area independently of row contents. */
        stroke_rect_thin(screen, 112, 96 + 32 * ui->selected_row,
                         sale_property_panel_width(assets) - 17,
                         32, 0xffff);
        SDL_UnlockSurface(screen);
    }
    if (ui != NULL && ui->pressed_control == SALE_HIT_CLOSE)
        sale_press_rect(screen,
                        sale_property_panel_width(assets) + 96,
                        33, 15, 17);
    else if (ui != NULL && ui->pressed_control == SALE_HIT_PREVIOUS)
        sale_press_rect(screen,
                        sale_property_panel_width(assets) + 96,
                        65, 15, 30);
    else if (ui != NULL && ui->pressed_control == SALE_HIT_NEXT)
        sale_press_rect(screen,
                        sale_property_panel_width(assets) + 96,
                        97, 15, 30);
}

static void sale_draw_card_page(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *ui)
{
    int count;
    int slot;
    int ordinal = 0;
    int x = 176;
    int y = 208;
    if (session == NULL ||
        !sale_draw_panel_chunk(screen, assets, 4, 140, 160))
        return;
    count = rich4_app_sale_item_count(session, RICH4_SALE_CATEGORY_CARD);
    for (slot = ui != NULL ? ui->page_offset : 0; slot < count; slot++) {
        GameDomainRef item;
        int maximum;
        if (!rich4_app_sale_item_at(session, RICH4_SALE_CATEGORY_CARD,
                                    slot, &item, &maximum))
            break;
        draw_big5_text_with_spacing(screen, x, y, game_card_big5_name(item.id),
                                    16, 0xffff, 0x1082, 3, 0, 2);
        if (ui != NULL &&
            ui->pressed_control == SALE_HIT_ROW_BASE + ordinal) {
            int column = ordinal % 5;
            int row = ordinal / 5;
            sale_press_rect(screen, 141 + 72 * column,
                            193 + 32 * row, 70, 30);
        }
        ordinal++;
        x += 72;
        if (x > 464) {
            x = 176;
            y += 32;
        }
    }
    if (ui != NULL && ui->pressed_control == SALE_HIT_CLOSE)
        sale_press_rect(screen, 429, 161, 70, 30);
}

static void sale_draw_tool_page(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *ui)
{
    int player;
    int tool;
    int ordinal = 0;
    int x = 176;
    int y = 208;
    if (session == NULL ||
        !sale_draw_panel_chunk(screen, assets, 3, 140, 160))
        return;
    player = session->state.current_player;
    if (player < 0 || player >= session->state.player_count) return;
    for (tool = 0; tool < GAME_TOOL_COUNT; tool++) {
        int amount = session->state.players[player].tool_inventory[tool];
        if (amount == 0) continue;
        draw_confinement_panel_chunk(screen, assets, 74, (size_t)tool,
                               x - 16, y);
        {
            char count_text[16];
            snprintf(count_text, sizeof(count_text), "\xa1\xd1%d", amount);
            draw_big5_text_with_spacing(screen, x + 30, y, count_text,
                                        16, 0xffff, 0x1082, 3, 0, 6);
        }
        if (ui != NULL &&
            ui->pressed_control == SALE_HIT_ROW_BASE + ordinal) {
            int column = ordinal % 5;
            int row = ordinal / 5;
            sale_press_rect(screen, 141 + 72 * column,
                            193 + 32 * row, 70, 30);
        }
        ordinal++;
        x += 72;
        if (x > 464) {
            x = 176;
            y += 32;
        }
    }
    if (ui != NULL && ui->pressed_control == SALE_HIT_CLOSE)
        sale_press_rect(screen, 429, 161, 70, 30);
}

static void sale_draw_sell_picker(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSaleInteractionPresentationState *ui)
{
    int category;
    if (ui == NULL || ui->message != RICH4_SALE_PICKER_OPEN) return;
    if (!sale_draw_panel_chunk(screen, assets, 17, 464, 116))
        return;
    category = ui->selected_row;
    if (category >= 0 && category < RICH4_SALE_CATEGORY_COUNT) {
        int x = 473 + (category / 2) * 63;
        int y = 125 + (category % 2) * 39;
        sale_tint_rect(screen, x, y, 62, 38);
    }
}

static void sale_draw_category_page(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *interaction)
{
    if (interaction->category == RICH4_SALE_CATEGORY_STOCK)
        sale_draw_stock_page(screen, assets, session, interaction);
    else if (interaction->category == RICH4_SALE_CATEGORY_PROPERTY)
        sale_draw_property_page(screen, assets, session, interaction);
    else if (interaction->category == RICH4_SALE_CATEGORY_TOOL)
        sale_draw_tool_page(screen, assets, session, interaction);
    else
        sale_draw_card_page(screen, assets, session, interaction);
}

static void sale_draw_price_page(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *ui)
{
    char prompt[128];
    if (screen == NULL || assets == NULL || session == NULL || ui == NULL)
        return;

    /* IDA sub_424502 draws this reference prompt over the selected sell
     * panel before sub_453544 opens Panel[21]'s numeric keypad. */
    if (!sale_draw_panel_chunk(screen, assets, 5, 227, 42)) return;
    if (rich4_app_sale_interaction_entry_is_quantity(session)) {
        snprintf(prompt, sizeof(prompt),
                 "\xbd\xd0\xbf\xe9\xa4\x4a\xb1\xfd\xbd\xe6\xa5\x58"
                 "\xaa\xba\xb1\x69\xbc\xc6");
    } else {
        snprintf(prompt, sizeof(prompt),
                 "\xbd\xd0\xbf\xe9\xa4\x4a\xb1\xfd\xa9\xe7\xbd\xe6"
                 "\xaa\xba\xbb\xf9\xae\xe6\n\n"
                 "\xa1\x5d\xa5\xab\xbb\xf9\xa1\x47%d\xa4\xb8\xa1\x5e",
                 rich4_app_sale_interaction_entry_reference_price(session));
    }
    sale_draw_text(screen, 320, 86, prompt, 16,
                   0x1082, 0x0000, 2, 4);

    /* 0x453544 opens the keypad centered. 0x452C02 then lets it move. */
    draw_game_quantity_panel(
        screen, assets, rich4_app_sale_interaction_entry_value(session),
        rich4_app_sale_interaction_entry_maximum(session),
        ui->pressed_control - SALE_HIT_PRICE_KEY_BASE,
        RICH4_SALE_KEYPAD_X + ui->keypad_dx,
        RICH4_SALE_KEYPAD_Y + ui->keypad_dy);
}

static void sale_draw_confirm_page(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *ui)
{
    static const char *const labels[] = {
        "\xc3\xfe\xab\xac\xa1\x47", /* 類型： */
        "\xa6\x61\xc2\x49\xa1\x47", /* 地點： */
        "\xb5\xa5\xaf\xc5\xa1\x47", /* 等級： */
        "\xb1\x69\xbc\xc6\xa1\x47", /* 張數： */
        "\xa5\xab\xbb\xf9\xa1\x47", /* 市價： */
        "\xbd\xe6\xbb\xf9\xa1\x47"  /* 賣價： */
    };
    const GameState *state = &session->state;
    char label[128];
    int seller = ui->selected_seller;
    int slot = ui->selected_slot;
    int modal_chunk;
    int modal_height;
    int action_y;
    int modal_left;
    int modal_top;
    const GameMarketListing *listing;
    if (ui->message == RICH4_SALE_NOTICE_COMPANY_CONTROL &&
        sale_notice_message == ui->message) {
        seller = sale_notice_seller;
        listing = &sale_notice_listing;
    } else {
        if (seller < 0 || seller >= state->player_count || slot < 0 ||
            slot >= GAME_MARKET_LISTING_SLOT_COUNT)
            return;
        listing = &state->market_listings[seller][slot];
    }
    {
        GameDomainRef item;
        switch (listing->type) {
        case GAME_MARKET_LISTING_STOCK:
            modal_chunk = 7;
            modal_height = 256;
            action_y = 236;
            break;
        case GAME_MARKET_LISTING_ASSET:
            modal_chunk = 8;
            modal_height = 288;
            action_y = 265;
            break;
        case GAME_MARKET_LISTING_TOOL:
        case GAME_MARKET_LISTING_CARD:
            modal_chunk = 6;
            modal_height = 224;
            action_y = 203;
            break;
        default:
            return;
        }
        if (!sale_listing_item_ref(listing, &item)) return;
        modal_left = (screen->w - 192) / 2;
        modal_top = (screen->h - modal_height) / 2;
        /* IDA 0x42704E selects Panel[73] detail chunks 6/7/8 by raw type. */
        if (!sale_draw_panel_chunk(screen, assets, (size_t)modal_chunk,
                                   modal_left, modal_top))
            return;
        sale_draw_panel_chunk(
            screen, assets,
            (size_t)(8 + (session->state.players[seller].sex == 0 ? 0 : 4) +
                     (int)listing->type),
            modal_left + 112, modal_top + 6);
        sale_draw_text(screen, modal_left + 58, modal_top + 40,
            new_game_character_name(
                session->state.players[seller].character),
            16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
        sale_item_label(assets, session, item, label, sizeof(label));
        /* 0x428581..0x42875B puts these labels on Panel[73] chunks 6..8.
         * 0x42704E writes the values and their units at runtime. */
        sale_draw_text(screen, modal_left + 31, modal_top + 92, labels[0],
                       16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
        if (listing->type == GAME_MARKET_LISTING_ASSET) {
            const char *asset_type;
            const char *development;
            int index = item.id;
            if (item.kind == GAME_DOMAIN_LAND) {
                int type = state->land_type[index];
                asset_type = "\xa6\xed\xa6\x76\xa5\xce\xa6\x61";
                development = game_land_development_big5_name(
                    type, state->land_level[index]);
            } else {
                int level = state->facility_level[index];
                asset_type = level == 0
                    ? "\xb0\xd3\xb7\x7e\xa5\xce\xa6\x61"
                    : game_facility_big5_name(state->facility_type[index], level);
                development = game_facility_level_big5_name(level);
            }
            sale_draw_text(screen, modal_left + 31, modal_top + 124,
                           labels[1], 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
            sale_draw_text(screen, modal_left + 31, modal_top + 156,
                           labels[2], 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
            sale_draw_text(screen, modal_left + 120, modal_top + 92,
                           asset_type, 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
            sale_draw_text(screen, modal_left + 120, modal_top + 124,
                           label, 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
            sale_draw_text(screen, modal_left + 120, modal_top + 156,
                           development, 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
        } else {
            sale_draw_text(screen, modal_left + 120, modal_top + 92,
                           label, 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
        }

        if (listing->type == GAME_MARKET_LISTING_STOCK) {
            char quantity[24];
            snprintf(quantity, sizeof(quantity), "%d\xb1\x69", listing->quantity);
            sale_draw_text(screen, modal_left + 31, modal_top + 124,
                           labels[3], 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
            sale_draw_text(screen, modal_left + 178, modal_top + 124,
                           quantity, 16, SALE_TEXT_LIGHT, 0x1082, 3, 6);
        }
        {
            int market_y = listing->type == GAME_MARKET_LISTING_ASSET ? 188
                         : listing->type == GAME_MARKET_LISTING_STOCK ? 156
                         : 122;
            int ask_y = market_y + (modal_chunk == 6 ? 30 : 32);
            sale_draw_text(screen, modal_left + 31, modal_top + market_y,
                           labels[4], 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
            sale_draw_text(screen, modal_left + 31, modal_top + ask_y,
                           labels[5], 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
            sale_draw_number(screen, modal_left + 178, modal_top + market_y,
                rich4_app_sale_market_value(state, item, listing->quantity), 1);
            sale_draw_number(screen, modal_left + 178, modal_top + ask_y,
                             listing->ask_price, 1);
        }
    }
    sale_draw_text(screen, modal_left + 52, modal_top + action_y,
        seller == session->state.current_player
            ? "\xba\x4d \xa5\xf3" : "\xc1\xca \xb6\x52",
        16, 0xffff, 0x8000, 3, 2);
    sale_draw_text(screen, modal_left + 140, modal_top + action_y,
                   "EXIT", 16, 0xffff, 0x8000, 3, 2);
    if (ui->pressed_control == SALE_HIT_CONFIRM_YES ||
        ui->pressed_control == SALE_HIT_CONFIRM_NO) {
        int button_y = modal_top + action_y;
        int button_x = modal_left +
            (ui->pressed_control == SALE_HIT_CONFIRM_YES ? 16 : 104);
        /* sub_42704E presses the selected 72x24 action rectangle on
         * WM_LBUTTONDOWN and restores it when the detail modal closes. */
        sale_press_rect(screen, button_x, button_y - 12, 72, 24);
    }
}

static int sale_notice_visible(int message)
{
    return message >= RICH4_SALE_NOTICE_CASH &&
           message == sale_notice_message &&
           (Uint32)(rich4_sdl1_ticks() - sale_notice_started_at) < 1500;
}

static void sale_draw_message(SDL_Surface *screen,
                              const GamePreviewAssets *assets, int message)
{
    const char *text;
    if (!sale_notice_visible(message)) return;
    /* 0x4255DA and 0x427EF6 use the centered 0x424502 notice. */
    switch (message) {
    case RICH4_SALE_NOTICE_CASH:
        text = "\xb1\x7a\xaa\xba\xb2\x7b\xaa\xf7\xa4\xa3\xa8\xac\xa1\x49";
        break;
    case RICH4_SALE_NOTICE_BOARD_FULL:
        text = "\xa4\xbd\xa7\x47\xc4\xe6\xa4\x77\xba\xa1\n\n"
               "\xbd\xd0\xa5\xfd\xba\x4d\xa5\xf3\xa1\x49";
        break;
    case RICH4_SALE_NOTICE_TOOLS_FULL:
        text = "\xb9\x44\xa8\xe3\xc4\xe6\xa4\x77\xba\xa1\n\n"
               "\xb5\x4c\xaa\x6b\xc1\xca\xb6\x52\xa1\x49";
        break;
    case RICH4_SALE_NOTICE_CARDS_FULL:
        text = "\xa5\x64\xa4\xf9\xc4\xe6\xa4\x77\xba\xa1\n\n"
               "\xb5\x4c\xaa\x6b\xc1\xca\xb6\x52\xa1\x49";
        break;
    case RICH4_SALE_NOTICE_COMPANY_CONTROL:
        text = "\xae\xa5\xb3\xdf\xb1\x7a\xc0\xf2\xb1\x6f"
               "\xb8\x67\xc0\xe7\xc5\x76\xa1\x49";
        break;
    default:
        return;
    }
    if (sale_draw_panel_chunk(screen, assets, 5, 227, 196))
        sale_draw_text(screen, 320, 240, text, 16, 0x1082, 0, 2, 4);
}

static void sale_draw_purchase_dialog(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSaleInteractionPresentationState *interaction)
{
    Rich4SprSmpChunk chunk;
    int left;
    int top;
    size_t chunk_index;
    if (screen == NULL || assets == NULL || interaction == NULL ||
        !game_asset_ready(assets, &assets->ui.has_confirm_dialog) ||
        !game_confirm_dialog_bounds(assets, GAME_COMMAND_CONFIRM,
                                    &left, &top, NULL, NULL))
        return;
    chunk_index = interaction->selected_row == 1 ? 1u :
                  interaction->selected_row == 2 ? 2u : 0u;
    if (rich4_spr_smp_get_chunk(assets->ui.confirm_dialog_data,
                                assets->ui.confirm_dialog_size,
                                &assets->ui.confirm_dialog_spr,
                                chunk_index, &chunk) != RICH4_SPR_SMP_OK)
        return;
    /* This is Data.mkf[440] through sub_453A32/sub_45367E: base chunk 0
     * and the two half-button hover variants in chunks 1 and 2. */
    draw_chunk_topleft(screen, assets->ui.confirm_dialog_data, &chunk,
                       left - chunk.x, top - chunk.y, 1);
}

static void sale_play_audio(Rich4Audio *audio, Rich4AppSaleAudioCue cue)
{
    if (audio == NULL) return;
    if (cue == RICH4_APP_SALE_AUDIO_SELECT)
        rich4_audio_play_select(audio);
    else if (cue == RICH4_APP_SALE_AUDIO_CANCEL)
        rich4_audio_play_cancel(audio);
    else if (cue == RICH4_APP_SALE_AUDIO_OK)
        rich4_audio_play_ok(audio);
}

static int sale_waiting_for_notice(GameSession *session)
{
    const GameSaleInteractionPresentationState *ui;
    if (session == NULL) return 0;
    ui = &session->presentation.sale_interaction;
    if (sale_notice_visible(ui->message)) return 1;
    rich4_app_sale_interaction_clear_notice(session);
    sale_notice_message = 0;
    return 0;
}

int game_sale_panel_release_notice(GameSession *session)
{
    if (!rich4_app_sale_interaction_is_active(session) ||
        !sale_waiting_for_notice(session)) return 0;
    /* 0x4528B9 consumes the first left/right button-up or key-up. */
    rich4_app_sale_interaction_clear_notice(session);
    sale_notice_message = 0;
    return 1;
}

static void sale_start_notice(int message)
{
    if (message < RICH4_SALE_NOTICE_CASH) return;
    sale_notice_message = message;
    sale_notice_started_at = rich4_sdl1_ticks();
}

static void sale_apply_result(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio,
    Rich4AppSaleInteractionResult *result)
{
    GameSaleInteractionPresentationState *ui;
    if (result == NULL || session == NULL) return;
    ui = &session->presentation.sale_interaction;
    if (result->command_ready) {
        int seller = ui->selected_seller;
        int stock = -1;
        int succeeded;
        if (result->command.type == GAME_COMMAND_BUY_MARKET_LISTING) {
            const GameMarketListing *listing =
                &session->state.market_listings[seller][ui->selected_slot];
            if (listing->type == GAME_MARKET_LISTING_STOCK &&
                listing->item_id < GAME_STOCK_COUNT &&
                session->state.stock_company_owner[listing->item_id] !=
                    result->command.player) {
                stock = listing->item_id;
                sale_notice_listing = *listing;
            }
        }
        succeeded = game_session_apply_command(
            session, assets, settings, &result->command,
            rich4_sdl1_ticks());
        rich4_app_sale_interaction_command_applied(
            session, succeeded, result);
        /* 0x425739 acknowledges a change in the listed company's owner. */
        if (succeeded && stock >= 0 &&
            session->state.stock_company_owner[stock] == result->command.player) {
            ui->message = RICH4_SALE_NOTICE_COMPANY_CONTROL;
            sale_notice_seller = seller;
        }
    }
    sale_start_notice(ui->message);
    sale_play_audio(audio, result->audio_cue);
}

static void sale_panel_release(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio,
    int button, int x, int y, int virtual_press)
{
    Rich4AppSaleInteractionResult result;
    Rich4StockQuantityMask mask;
    int dialog_width;
    int dialog_height;
    if (game_sale_panel_release_notice(session)) return;
    mask = sale_quantity_mask(assets);
    sale_confirm_dialog_size(assets, &dialog_width, &dialog_height);
    rich4_app_sale_interaction_mouse_release(
        session, &mask, button, x, y, sale_stock_panel_width(assets),
        sale_property_panel_width(assets), dialog_width, dialog_height, virtual_press,
        &result);
    sale_apply_result(session, assets, settings, audio, &result);
}

void game_sale_panel_mouse_release(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio,
    int button, int x, int y)
{
    if (button != SDL_BUTTON_LEFT && button != SDL_BUTTON_RIGHT) return;
    sale_panel_release(session, assets, settings, audio, button, x, y, 0);
}

void game_sale_panel_virtual_confirm(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio, int x, int y)
{
    sale_panel_release(session, assets, settings, audio, SDL_BUTTON_LEFT, x, y, 1);
}

void game_sale_panel_mouse_down(
    GameSession *session, const GamePreviewAssets *assets, int x, int y)
{
    Rich4StockQuantityMask mask;
    int dialog_width;
    int dialog_height;
    if (sale_waiting_for_notice(session)) return;
    mask = sale_quantity_mask(assets);
    sale_confirm_dialog_size(assets, &dialog_width, &dialog_height);
    rich4_app_sale_interaction_mouse_down(
        session, &mask, x, y, sale_stock_panel_width(assets),
        sale_property_panel_width(assets), dialog_width, dialog_height);
    if (session != NULL) sale_start_notice(session->presentation.sale_interaction.message);
}

void game_sale_panel_mouse_motion(
    GameSession *session, const GamePreviewAssets *assets,
    int x, int y, int mouse_left_down)
{
    Rich4StockQuantityMask mask;
    int dialog_width;
    int dialog_height;
    if (sale_waiting_for_notice(session)) return;
    mask = sale_quantity_mask(assets);
    sale_confirm_dialog_size(assets, &dialog_width, &dialog_height);
    rich4_app_sale_interaction_mouse_motion(
        session, &mask, x, y, mouse_left_down,
        sale_stock_panel_width(assets), sale_property_panel_width(assets),
        dialog_width, dialog_height);
}

void game_sale_panel_key_down(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio, Rich4AppKey key)
{
    Rich4AppSaleInteractionResult result;
    if (sale_waiting_for_notice(session)) return;
    rich4_app_sale_interaction_key_down(session, key, &result);
    sale_apply_result(session, assets, settings, audio, &result);
}

static int sale_draw_browse_page(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *interaction)
{
    if (!sale_draw_panel_chunk(screen, assets, 0, 22, 66))
        return 0;
    sale_draw_browse_rows(screen, assets, session, interaction);
    if (interaction->page == RICH4_SALE_PAGE_BROWSE) {
        if (interaction->pressed_control == SALE_HIT_SELL)
            sale_tint_rect(screen, 464, 74, 72, 40);
        else if (interaction->pressed_control == SALE_HIT_CLOSE)
            sale_tint_rect(screen, 536, 74, 72, 40);
    }
    return 1;
}

void game_draw_sale_panel(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *interaction)
{
    if (screen == NULL || session == NULL || interaction == NULL ||
        !interaction->active)
        return;
    if (interaction->page == RICH4_SALE_PAGE_ITEMS) {
        if (!sale_draw_browse_page(screen, assets, session, interaction))
            return;
        sale_draw_category_page(screen, assets, session, interaction);
        return;
    }
    if (interaction->page == RICH4_SALE_PAGE_PRICE) {
        if (!sale_draw_browse_page(screen, assets, session, interaction))
            return;
        sale_draw_category_page(screen, assets, session, interaction);
        sale_draw_price_page(screen, assets, session, interaction);
        sale_draw_message(screen, assets, interaction->message);
        return;
    }
    /* Panel[73] is the sale UI; Panel[74] is an icon sheet in the original. */
    if (!sale_draw_browse_page(screen, assets, session, interaction)) return;
    sale_draw_sell_picker(screen, assets, interaction);
    if (interaction->page == RICH4_SALE_PAGE_CONFIRM ||
        (interaction->message == RICH4_SALE_NOTICE_COMPANY_CONTROL &&
         sale_notice_visible(interaction->message))) {
        sale_draw_confirm_page(screen, assets, session, interaction);
    } else if (interaction->page == RICH4_SALE_PAGE_PURCHASE_CONFIRM) {
        sale_draw_confirm_page(screen, assets, session, interaction);
        sale_draw_purchase_dialog(screen, assets, interaction);
    }
    sale_draw_message(screen, assets, interaction->message);
}
