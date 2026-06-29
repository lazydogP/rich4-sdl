#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <SDL/SDL.h>
#include "core/card.h"
#include "core/confinement.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/special_actor_action.h"
#include "core/special_event.h"
#include "core/tool.h"
#include "app/shop_flow.h"
#include "data/character_speech.h"
#include "data/special_event_text.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_route_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "resource/byte_reader.h"
#include "resource/spr_smp.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"
void draw_special_actor_action_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    static const char thief_format[] =
        "\xb0\xbd\xa8\xfa%s\n\n%d\xc2\x49\xc2\x49\xa8\xe9\xa1\x49";
    static const char robber_card_format[] =
        "\xb9\xdc\xa8\xfa%s%s\xa1\x49"; /* 0x463AF7 / 0x41C2CE */
    static const char robber_bank_format[] =
        "\xb1\x6a\xb5\x73\xb7\x6d\xb9\xdc\xbb\xc8\xa6\xe6\n\n"
        "\xb1\x6f\xb4\xda%d\xa4\xb8\n\n\xb5\xb9%s\xa1\x49";
    static const char hooligan_format[] =
        "\xb0\xc7\xaf\xc1%s\n\n%d\xa4\xb8\xab\x4f\xc5\x40\xb6\x4f\xa1\x49";
    static const char spy_toll_format[] =
        "\xa8\xfa\xa8\xab\xb9\x4c\xb8\xf4\xb6\x4f\n\n%d\xa4\xb8\xa1\x49";
    static const char spy_dividend_format[] =
        "\xa8\xfa\xa8\xab\xac\xd5\xbe\x6c\n\n%d\xa4\xb8\xa1\x49";
    const GameSpecialActorAction *action;
    Rich4SprSmpChunk background;
    const char *target_name = "";
    const char *owner_name = "";
    char message[192];
    int index;
    if (screen == NULL || assets == NULL || session == NULL ||
        session->presentation->special_actor.action_batch.count <= 0)
        return;
    index = session->presentation->special_actor.action_index;
    if (index < 0 || index >= session->presentation->special_actor.action_batch.count)
        return;
    action = &session->presentation->special_actor.action_batch.action[index];
    if (action->target_player >= 0 &&
        action->target_player < session->state->player_count)
        target_name = new_game_character_name(
            session->state->players[action->target_player].character);
    if (action->owner_player >= 0 &&
        action->owner_player < session->state->player_count)
        owner_name = new_game_character_name(
            session->state->players[action->owner_player].character);
    switch (action->kind) {
    case GAME_SPECIAL_ACTOR_ACTION_THIEF_POINTS:
        snprintf(message, sizeof(message), thief_format,
                 target_name, action->amount);
        break;
    case GAME_SPECIAL_ACTOR_ACTION_ROBBER_CARD:
        snprintf(message, sizeof(message), robber_card_format,
                 target_name, game_card_big5_name(action->card));
        break;
    case GAME_SPECIAL_ACTOR_ACTION_ROBBER_BANK:
        snprintf(message, sizeof(message), robber_bank_format,
                 action->amount, owner_name);
        break;
    case GAME_SPECIAL_ACTOR_ACTION_HOOLIGAN_FEE:
        snprintf(message, sizeof(message), hooligan_format,
                 target_name, action->amount);
        break;
    case GAME_SPECIAL_ACTOR_ACTION_SPY_TOLL:
        snprintf(message, sizeof(message), spy_toll_format, action->amount);
        break;
    case GAME_SPECIAL_ACTOR_ACTION_SPY_DIVIDEND:
        snprintf(message, sizeof(message), spy_dividend_format,
                 action->amount);
        break;
    default:
        return;
    }
    if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
        rich4_spr_smp_get_chunk(
            assets->ui.common_dialog_data, assets->ui.common_dialog_size,
            &assets->ui.common_dialog_spr, 5,
            &background) == RICH4_SPR_SMP_OK)
        draw_chunk_at(screen, assets->ui.common_dialog_data,
                      &background, 220, 140, 1);
    draw_big5_text(
        screen, 220, 140, message, 16,
        rich4_character_rgb565_color(0x00f0f0f0u),
        rich4_character_rgb565_color(0x00101010u), 3, 4);
}

static void draw_shop_item_line(
    SDL_Surface *screen, int left_x, int name_y, int price_y,
    const char *name, int price, int sold)
{
    char price_text[24];
    uint16_t color = rich4_character_rgb565_color(
        sold ? 0x00a0a0a0u : 0x00ffffffu);
    uint16_t dark = rich4_character_rgb565_color(0x00101010u);
    draw_big5_text_with_spacing(
        screen, left_x + 90, name_y, name, 20,
        color, dark, 3, 0, 2);
    snprintf(price_text, sizeof(price_text), "$%d", price);
    draw_big5_text_with_spacing(
        screen, left_x + 194, price_y, price_text, 20,
        color, dark, 3, 0, 1);
}

static void draw_shop_inventory_card(
    SDL_Surface *screen, int inventory_x, int slot, int card)
{
    int column = slot % 5;
    int row = slot / 5;
    int x = inventory_x + 45 + column * 80;
    int y = 326 + row * 56;
    if (card < 0 || card >= GAME_CARD_COUNT) return;
    draw_big5_text_with_spacing(
        screen, x, y, game_card_big5_name(card), 20,
        rich4_character_rgb565_color(0x00ffffffu),
        rich4_character_rgb565_color(0x00101010u),
        3, 0, 2);
}

void draw_shop_visit(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    const GameShopPresentationState *shop;
    int base;
    int i;
    int player;
    char points[24];
    if (screen == NULL || assets == NULL || session == NULL ||
        !session->presentation->shop_visit.active)
        return;
    shop = &session->presentation->shop_visit;
    player = shop->player;
    if (shop->phase == RICH4_SHOP_OWNER_GIFT) {
        static const char gift_format[] =
            "\xc5\x77\xaa\xef\xb8\xb3\xa8\xc6\xaa\xf8\xa5\xfa\xc1\x7b"
            "\x0a\x0a\xb0\x65\xb1\x7a%s\xa1\x49";
        const char *name = shop->gift_kind == GAME_DOMAIN_CARD
            ? game_card_big5_name(shop->gift_id)
            : game_tool_big5_name(shop->gift_id);
        Rich4SprSmpChunk background;
        char message[96];
        if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
            rich4_spr_smp_get_chunk(
                assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5,
                &background) == RICH4_SPR_SMP_OK)
            draw_chunk_at(screen, assets->ui.common_dialog_data,
                          &background, 220, 140, 1);
        snprintf(message, sizeof(message), gift_format, name);
        draw_big5_text(
            screen, 220, 140, message, 16,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 4);
        return;
    }
    if (shop->phase == RICH4_SHOP_OWNER_SPEECH) return;
    base = shop->mode != 0 ? 16 : 0;
    draw_game_panel_chunk(screen, assets, 10, (size_t)base, 0, 0, 0);
    draw_game_panel_chunk_at(
        screen, assets, 10, (size_t)(base + 2), 320, 240, 1);
    if (shop->phase == RICH4_SHOP_EXIT_MESSAGE &&
        game_asset_ready(assets, &assets->ui.has_panel[10])) {
        Rich4SprSmpChunk background;
        int left = shop->mode == 0 ? 316 : 310;
        int top = shop->mode == 0 ? 94 : 23;
        int right = shop->mode == 0 ? 416 : 410;
        int bottom = shop->mode == 0 ? 227 : 193;

        /* 0x42E686 restores this rectangle from page chunk 0/16 before
         * replacing the raised arm with chunk 3/19 at (320,240).  Since
         * SDL redraws the complete scene each frame, reproduce that
         * persistent back-buffer edit explicitly. */
        if (rich4_spr_smp_get_chunk(
                assets->ui.panel_data[10], assets->ui.panel_size[10],
                &assets->ui.panel_spr[10], (size_t)base,
                &background) == RICH4_SPR_SMP_OK)
            draw_chunk_crop_topleft(
                screen, assets->ui.panel_data[10], &background,
                left, top, right - left, bottom - top,
                left, top, 0);
        draw_game_panel_chunk_at(
            screen, assets, 10, (size_t)(base + 3), 320, 240, 1);
    }
    if (shop->clerk_face_chunk >= 0)
        draw_game_panel_chunk(
            screen, assets, 10, (size_t)shop->clerk_face_chunk,
            shop->mode == 0 ? 405 : 411,
            shop->mode == 0 ? 60 : 50, 1);
    if (shop->clerk_mouth_chunk >= 0)
        draw_game_panel_chunk(
            screen, assets, 10, (size_t)shop->clerk_mouth_chunk,
            shop->mode == 0 ? 405 : 411,
            shop->mode == 0 ? 91 : 89, 1);
    draw_game_panel_chunk(
        screen, assets, 10, (size_t)(base + 1),
        shop->left_x, 10, 1);
    draw_game_panel_chunk(
        screen, assets, 11, (size_t)shop->mode,
        shop->inventory_x, 293, 1);
    /* 0x42D67D draws the tab and EXIT only after both panels reach x=5/227,
     * then posts 0x40E to draw the points box.  They remain visible during
     * the prompt, feedback and farewell phases. */
    if (shop->phase != RICH4_SHOP_GREETING &&
        shop->phase != RICH4_SHOP_SLIDE_IN) {
        draw_game_panel_chunk_at(
            screen, assets, 10,
            (size_t)(base +
                (shop->pressed == RICH4_SHOP_HIT_TAB ? 14 : 13)),
            542, 13, 1);
        draw_game_panel_chunk_at(
            screen, assets, 10,
            (size_t)(shop->pressed == RICH4_SHOP_HIT_EXIT ? 36 : 35),
            556, 246, 1);
        draw_game_panel_chunk_at(screen, assets, 10, 37, 230, 246, 1);
        snprintf(points, sizeof(points), "%u",
                 (unsigned)session->state->players[player].points);
        draw_big5_text_with_spacing(
            screen, 310, 257, points, 20,
            rich4_character_rgb565_color(0x00ffffffu),
            rich4_character_rgb565_color(0x00101010u),
            3, 0, 1);
    }

    if (shop->mode == 0) {
        for (i = 0; i < shop->card_count; i++) {
            int card = shop->card_merchandise[i];
            const GameCardInfo *info = rich4_card_info(card);
            if (info != NULL)
                draw_shop_item_line(
                    screen, shop->left_x,
                    93 + i * 24, 85 + i * 24,
                    game_card_big5_name(card),
                    info->shop_price,
                    shop->card_sold[i]);
        }
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
            int card =
                session->state->players[player].card_inventory[i] - 1;
            draw_shop_inventory_card(
                screen, shop->inventory_x, i, card);
        }
    } else {
        for (i = 0; i < 8; i++) {
            int tool = shop->tool_merchandise[i];
            const GameToolInfo *info = rich4_tool_info(tool);
            if (info != NULL)
                draw_shop_item_line(
                    screen, shop->left_x,
                    102 + i * 48, 94 + i * 48,
                    game_tool_big5_name(tool),
                    info->shop_price,
                    shop->tool_sold[i]);
        }
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
            int tool = shop->inventory_tool_slot[i];
            int column = i % 5;
            int row = i / 5;
            int x = shop->inventory_x + 29 + column * 80;
            int y = 326 + row * 56;
            if (tool < 0) continue;
            draw_game_panel_chunk_at(
                screen, assets, 11, (size_t)(2 + tool), x, y, 1);
            {
                char count[8];
                snprintf(count, sizeof(count), "\xa1\xd1%u",
                    (unsigned)session->state->players[player]
                        .tool_inventory[tool]);
                draw_big5_text_with_spacing(
                    screen,
                    shop->inventory_x + 79 + column * 80,
                    316 + row * 56, count, 20,
                    rich4_character_rgb565_color(0x00ffffffu),
                    rich4_character_rgb565_color(0x00101010u),
                    3, 0, 1);
            }
        }
    }
    if (shop->message_index >= 0) {
        const char *message = rich4_shop_message_big5(
            shop->message_index);
        draw_game_panel_chunk_at(
            screen, assets, 10, 15, 120, 10, 1);
        draw_big5_text_with_spacing(
            screen, 235, 119, message, 20,
            rich4_character_rgb565_color(0x00101010u),
            0, 2, 1, 4);
    }
}
