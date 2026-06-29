#include "platform/sdl1/game_lazy_assets.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <SDL/SDL.h>

#include "app/sale_interaction_flow.h"
#include "core/card.h"
#include "core/tool.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/render/blit.h"
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

static Uint32 sale_cash_warning_started_at;
static int sale_cash_warning_active;

static int sale_indexed_price(int base_price, int price_index);

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

enum { SALE_PICKER_OPEN = -1 };

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

static void sale_format_grouped_number(int value, char *out, size_t out_size)
{
    char digits[32];
    size_t length;
    size_t source;
    size_t target = 0;
    size_t digit_start;
    size_t digit_count;
    if (out == NULL || out_size == 0) return;
    snprintf(digits, sizeof(digits), "%d", value);
    length = strlen(digits);
    digit_start = digits[0] == '-' ? 1u : 0u;
    digit_count = length - digit_start;
    for (source = 0; source < length && target + 1 < out_size; source++) {
        if (source > digit_start && digit_count > 0 &&
            digit_count % 3 == 0 && target + 1 < out_size)
            out[target++] = ',';
        out[target++] = digits[source];
        if (source >= digit_start && digit_count > 0) digit_count--;
    }
    out[target] = '\0';
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
            draw_confinement_panel_chunk(
                screen, assets, 73,
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

static int sale_scaled_price(int base_price, int price_index)
{
    long long value;
    if (base_price <= 0 || price_index <= 0) return 0;
    value = (long long)base_price * 100 * price_index;
    return value > INT_MAX ? INT_MAX : (int)value;
}

static int sale_indexed_price(int base_price, int price_index)
{
    long long value;
    if (base_price <= 0 || price_index <= 0) return 0;
    value = (long long)base_price * price_index;
    return value > INT_MAX ? INT_MAX : (int)value;
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
        !draw_confinement_panel_chunk(screen, assets, 73, 1, 152, 32))
        return;
    player = session->state.current_player;
    if (player < 0 || player >= session->state.player_count) return;
    count = rich4_app_sale_item_count(
        session, RICH4_SALE_CATEGORY_STOCK);
    for (row = 0; row < count && row < GAME_STOCK_COUNT; row++) {
        GameDomainRef item;
        int maximum;
        char quantity[32];
        char value[32];
        const char *name;
        int stock;
        int y = 80 + row * 32;
        int amount;
        if (!rich4_app_sale_item_at(session, RICH4_SALE_CATEGORY_STOCK,
                                    row, &item, &maximum))
            continue;
        stock = item.id;
        amount = maximum;
        name = game_stock_name_for_state(&session->state, stock);
        sale_format_grouped_number(amount, quantity, sizeof(quantity));
        sale_format_grouped_number(
            (int)(amount * session->state.stocks[stock].f20),
            value, sizeof(value));
        sale_draw_text(screen, 200, y, name != NULL ? name : "",
                       16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
        sale_draw_text(screen, 332, y, quantity, 16,
                       SALE_TEXT_LIGHT, 0x1082, 3, 6);
        sale_draw_text(screen, 478, y, value, 16,
                       SALE_TEXT_LIGHT, 0x1082, 3, 6);
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
    int filter = ui != NULL && ui->selected_slot >= 0 &&
                 ui->selected_slot < 5 ? ui->selected_slot : 0;
    int count = rich4_app_sale_item_count(
        session, RICH4_SALE_CATEGORY_PROPERTY);
    int row;
    if (!draw_confinement_panel_chunk(screen, assets, 73, 2, 112, 32))
        return;
    draw_confinement_panel_chunk(screen, assets, 73, 19,
                           112 + filter * 80, 32);
    sale_draw_text(screen, 152 + filter * 80, 48,
                   filter_labels[filter], 20, 0x1082, 0, 2, 2);
    for (row = 0; row < SALE_PROPERTY_VISIBLE_ROWS; row++) {
        GameDomainRef item;
        int maximum;
        int ordinal = (ui != NULL ? ui->page_offset : 0) + row;
        int y = 112 + row * 32;
        int level;
        int value;
        int charge;
        char text[32];
        char label[96];
        if (ordinal >= count || !rich4_app_sale_item_at(
                session, RICH4_SALE_CATEGORY_PROPERTY, ordinal,
                &item, &maximum))
            break;
        sale_item_label(assets, session, item, label, sizeof(label));
        if (item.kind == GAME_DOMAIN_LAND) {
            level = session->state.land_level[item.id];
            value = sale_indexed_price(
                session->state.land_price[item.id] +
                    level * session->state.land_price_per_level[item.id],
                session->state.price_index);
            if (level < 0) level = 0;
            if (level > 5) level = 5;
            charge = session->state.land_toll_by_level[item.id][level];
        } else {
            level = session->state.facility_level[item.id];
            value = sale_indexed_price(
                session->state.facility_price[item.id] +
                    level * session->state.facility_price_per_level[item.id],
                session->state.price_index);
            if (level < 0) level = 0;
            if (level > 5) level = 5;
            charge = session->state.facility_charge_by_level[item.id][level];
        }
        sale_draw_text(screen, 147, y, label, 16,
                       SALE_TEXT_LIGHT, 0x1082, 3, 2);
        snprintf(text, sizeof(text), "%d", level);
        sale_draw_text(screen, 231, y, text, 16,
                       SALE_TEXT_LIGHT, 0x1082, 3, 2);
        sale_format_grouped_number(value, text, sizeof(text));
        sale_draw_text(screen, 352, y, text, 16,
                       SALE_TEXT_LIGHT, 0x1082, 3, 6);
        sale_format_grouped_number(charge, text, sizeof(text));
        sale_draw_text(screen, 424, y, text, 16,
                       SALE_TEXT_LIGHT, 0x1082, 3, 6);
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
        !draw_confinement_panel_chunk(screen, assets, 73, 4, 140, 160))
        return;
    count = rich4_app_sale_item_count(session, RICH4_SALE_CATEGORY_CARD);
    for (slot = ui != NULL ? ui->page_offset : 0; slot < count; slot++) {
        GameDomainRef item;
        int maximum;
        if (!rich4_app_sale_item_at(session, RICH4_SALE_CATEGORY_CARD,
                                    slot, &item, &maximum))
            break;
        sale_draw_text(screen, x, y, game_card_big5_name(item.id),
                       16, 0xffff, 0x1082, 3, 2);
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
        !draw_confinement_panel_chunk(screen, assets, 73, 3, 140, 160))
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
            snprintf(count_text, sizeof(count_text), "%d", amount);
            sale_draw_text(screen, x + 30, y, count_text, 16,
                           0xffff, 0x1082, 3, 6);
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
    if (ui == NULL || ui->message != SALE_PICKER_OPEN) return;
    if (!draw_confinement_panel_chunk(screen, assets, 73, 17, 464, 116))
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
    static const int pressed_origins[16][2] = {
        {0,0},{0,0},{264,207},{320,207},
        {264,239},{304,239},{344,239},
        {264,263},{304,263},{344,263},
        {264,287},{304,287},{344,287},
        {264,311},{304,311},{344,311}
    };
    char prompt[128];
    char value_text[32];
    int value;
    int maximum;
    int bar_width;
    int control;
    if (screen == NULL || assets == NULL || session == NULL || ui == NULL)
        return;

    /* IDA sub_424502 draws this reference prompt over the selected sell
     * panel before sub_453544 opens Panel[21]'s numeric keypad. */
    draw_confinement_panel_chunk(screen, assets, 73, 5, 227, 42);
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

    /* sub_453544 centers the 128x192 keypad at (256,144). Keep the source
     * Panel[22] shape map for pointer hit testing in the app flow. */
    if (!game_asset_ready(assets, &assets->ui.has_panel[21])) return;
    draw_confinement_panel_chunk(screen, assets, 21, 0, 256, 144);
    value = rich4_app_sale_interaction_entry_value(session);
    maximum = rich4_app_sale_interaction_entry_maximum(session);
    snprintf(value_text, sizeof(value_text), "%d", value);
    {
        int digit_index = (int)strlen(value_text) - 1;
        int digit_count = 0;
        while (digit_index >= 0 && digit_count < 9) {
            int digit = value_text[digit_index] - '0';
            if (digit >= 0 && digit <= 9)
                draw_confinement_panel_chunk(
                    screen, assets, 21, (size_t)(16 + digit),
                    363 - digit_count * 12, 155);
            digit_index--;
            digit_count++;
        }
    }
    draw_confinement_panel_chunk(screen, assets, 21, 1, 266, 186);
    bar_width = rich4_app_stock_quantity_bar_width(value, maximum);
    if (bar_width > 0) {
        SDL_Rect fill;
        fill.x = 266;
        fill.y = 186;
        fill.w = (Uint16)bar_width;
        fill.h = 12;
        /* sub_45297E crops an auxiliary 128x192 buffer here; it does not
         * crop Panel[21] chunk 0 (the keypad artwork). */
        SDL_FillRect(screen, &fill, SDL_MapRGB(screen->format, 0, 0, 0));
    }
    control = ui->pressed_control - SALE_HIT_PRICE_KEY_BASE;
    if (control >= RICH4_STOCK_QUANTITY_CONTROL_MAX && control <= 15)
        draw_confinement_panel_chunk(
            screen, assets, 21, (size_t)control,
            pressed_origins[control][0], pressed_origins[control][1]);
}

static void sale_draw_confirm_page(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *ui)
{
    char label[128];
    char value[48];
    int seller = ui->selected_seller;
    int slot = ui->selected_slot;
    int modal_chunk;
    int modal_height;
    int action_y;
    int modal_left;
    int modal_top;
    if (seller < 0 || seller >= session->state.player_count || slot < 0 ||
        slot >= GAME_MARKET_LISTING_SLOT_COUNT)
        return;
    {
        const GameMarketListing *listing =
            &session->state.market_listings[seller][slot];
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
        if (!draw_confinement_panel_chunk(screen, assets, 73,
                                    (size_t)modal_chunk,
                                    modal_left, modal_top))
            return;
        draw_confinement_panel_chunk(
            screen, assets, 73,
            (size_t)(8 + (session->state.players[seller].sex == 0 ? 0 : 4) +
                     (int)listing->type),
            modal_left + 112, modal_top + 6);
        sale_draw_text(screen, modal_left + 58, modal_top + 40,
            new_game_character_name(
                session->state.players[seller].character),
            16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
        sale_item_label(assets, session, item, label, sizeof(label));
        if (listing->type == GAME_MARKET_LISTING_ASSET) {
            const char *asset_type = item.kind == GAME_DOMAIN_LAND
                ? "\xa4\x67\xa6\x61" : "\xb3\x5d\xac\x49";
            int index = item.id;
            int level = 0;
            int base_price = 0;
            if (item.kind == GAME_DOMAIN_LAND && index >= 0 &&
                index < session->state.land_count) {
                level = session->state.land_level[index];
                base_price = session->state.land_price[index] +
                    level * session->state.land_price_per_level[index];
            } else if (item.kind == GAME_DOMAIN_FACILITY && index >= 0 &&
                       index < session->state.facility_count) {
                level = session->state.facility_level[index];
                base_price = session->state.facility_price[index] +
                    level * session->state.facility_price_per_level[index];
            }
            sale_draw_text(screen, modal_left + 120, modal_top + 92,
                           asset_type, 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
            sale_draw_text(screen, modal_left + 120, modal_top + 124,
                           label, 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
            snprintf(value, sizeof(value), "%d", level);
            sale_draw_text(screen, modal_left + 120, modal_top + 156,
                           value, 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
            sale_format_grouped_number(
                sale_indexed_price(base_price, session->state.price_index),
                value, sizeof(value));
            sale_draw_text(screen, modal_left + 178, modal_top + 188,
                           value, 16, SALE_TEXT_LIGHT, 0x1082, 3, 6);
        } else {
            sale_draw_text(screen, modal_left + 120, modal_top + 92,
                           label, 16, SALE_TEXT_LIGHT, 0x1082, 3, 2);
        }

        if (listing->type == GAME_MARKET_LISTING_STOCK) {
            snprintf(value, sizeof(value), "%d", listing->quantity);
            sale_draw_text(screen, modal_left + 178, modal_top + 124,
                           value, 16, SALE_TEXT_LIGHT, 0x1082, 3, 6);
            if (item.id >= 0 && item.id < GAME_STOCK_COUNT) {
                sale_format_grouped_number(
                    (int)(listing->quantity *
                          session->state.stocks[item.id].f20),
                    value, sizeof(value));
                sale_draw_text(screen, modal_left + 178, modal_top + 156,
                               value, 16, SALE_TEXT_LIGHT, 0x1082, 3, 6);
            }
            sale_format_grouped_number(listing->ask_price,
                                       value, sizeof(value));
            sale_draw_text(screen, modal_left + 178, modal_top + 188,
                           value, 16, SALE_TEXT_LIGHT, 0x1082, 3, 6);
        } else if (listing->type == GAME_MARKET_LISTING_ASSET) {
            sale_format_grouped_number(listing->ask_price,
                                       value, sizeof(value));
            sale_draw_text(screen, modal_left + 178, modal_top + 220,
                           value, 16, SALE_TEXT_LIGHT, 0x1082, 3, 6);
        } else {
            int reference_price = 0;
            if (item.kind == GAME_DOMAIN_CARD) {
                const GameCardInfo *info = rich4_card_info(item.id);
                if (info != NULL)
                    reference_price = sale_scaled_price(
                        info->shop_price, session->state.price_index);
            } else if (item.kind == GAME_DOMAIN_TOOL) {
                const GameToolInfo *info = rich4_tool_info(item.id);
                if (info != NULL)
                    reference_price = sale_scaled_price(
                        info->shop_price, session->state.price_index);
            }
            sale_format_grouped_number(reference_price,
                                       value, sizeof(value));
            sale_draw_text(screen, modal_left + 178, modal_top + 122,
                           value, 16, SALE_TEXT_LIGHT, 0x1082, 3, 6);
            sale_format_grouped_number(listing->ask_price,
                                       value, sizeof(value));
            sale_draw_text(screen, modal_left + 178, modal_top + 152,
                           value, 16, SALE_TEXT_LIGHT, 0x1082, 3, 6);
        }
    }
    sale_draw_text(screen, modal_left + 52, modal_top + action_y,
        seller == session->state.current_player
            ? "\xba\x4d \xa5\xf3" : "\xc1\xca \xb6\x52",
        16, 0xffff, 0x8000, 3, 2);
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

static void sale_draw_message(SDL_Surface *screen,
                              const GamePreviewAssets *assets, int message)
{
    if (screen == NULL || assets == NULL || message != 4 ||
        !sale_cash_warning_active ||
        (Uint32)(rich4_sdl1_ticks() - sale_cash_warning_started_at) >= 1500)
        return;
    /* sub_4255DA reports insufficient cash through sub_424502(text, 1):
     * Panel.mkf[73] chunk 5 at (227,196), with centered text at (320,240). */
    draw_confinement_panel_chunk(screen, assets, 73, 5, 227, 196);
    sale_draw_text(screen, 320, 240,
                   "\xb1\x7a\xaa\xba\xb2\x7b\xaa\xf7\xa4\xa3\xa8\xac\xa1\x49",
                   16, 0x1082, 0, 2, 4);
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

static void sale_apply_result(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio,
    Rich4AppSaleInteractionResult *result)
{
    int succeeded = 0;
    if (result == NULL || session == NULL) return;
    if (result->command_ready)
        succeeded = game_session_apply_command(
            session, assets, settings, &result->command,
            rich4_sdl1_ticks());
    if (result->command_ready)
        rich4_app_sale_interaction_command_applied(
            session, succeeded, result);
    if (session->presentation.sale_interaction.message == 4) {
        if (!sale_cash_warning_active) {
            sale_cash_warning_started_at = rich4_sdl1_ticks();
            sale_cash_warning_active = 1;
        }
    } else {
        sale_cash_warning_active = 0;
    }
    sale_play_audio(audio, result->audio_cue);
}

void game_sale_panel_mouse_release(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio,
    int button, int x, int y)
{
    Rich4AppSaleInteractionResult result;
    Rich4StockQuantityMask mask = sale_quantity_mask(assets);
    int dialog_width;
    int dialog_height;
    sale_confirm_dialog_size(assets, &dialog_width, &dialog_height);
    rich4_app_sale_interaction_mouse_release(
        session, &mask, button, x, y, sale_stock_panel_width(assets),
        sale_property_panel_width(assets), dialog_width, dialog_height,
        &result);
    sale_apply_result(session, assets, settings, audio, &result);
}

void game_sale_panel_mouse_down(
    GameSession *session, const GamePreviewAssets *assets, int x, int y)
{
    Rich4StockQuantityMask mask = sale_quantity_mask(assets);
    int dialog_width;
    int dialog_height;
    sale_confirm_dialog_size(assets, &dialog_width, &dialog_height);
    rich4_app_sale_interaction_mouse_down(
        session, &mask, x, y, sale_stock_panel_width(assets),
        sale_property_panel_width(assets), dialog_width, dialog_height);
}

void game_sale_panel_mouse_motion(
    GameSession *session, const GamePreviewAssets *assets,
    int x, int y, int mouse_left_down)
{
    Rich4StockQuantityMask mask = sale_quantity_mask(assets);
    int dialog_width;
    int dialog_height;
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
    rich4_app_sale_interaction_key_down(session, key, &result);
    sale_apply_result(session, assets, settings, audio, &result);
}

static int sale_draw_browse_page(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *interaction)
{
    if (!draw_confinement_panel_chunk(screen, assets, 73, 0, 22, 66))
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
    if (interaction->page == RICH4_SALE_PAGE_CONFIRM) {
        sale_draw_confirm_page(screen, assets, session, interaction);
    } else if (interaction->page == RICH4_SALE_PAGE_PURCHASE_CONFIRM) {
        sale_draw_confirm_page(screen, assets, session, interaction);
        sale_draw_purchase_dialog(screen, assets, interaction);
    }
    sale_draw_message(screen, assets, interaction->message);
}
