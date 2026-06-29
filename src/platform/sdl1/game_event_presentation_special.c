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
#include "data/character_speech.h"
#include "data/special_event_text.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_route_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_stock_panel_names.h"
#include "resource/byte_reader.h"
#include "resource/spr_smp.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"

static const char news_tax_player_format[] =
    "%s\xc3\xba\xa5\xe6%d\xa4\xb8";
static const char news_bank_bonus_player_format[] =
    "%s\xb1\x6f\xa8\xec%d\xa4\xb8";
static const char fortune_card_target_format[] =
    "\xa6\x56%s\xa6\xac\xa4\x40\xb1\x69\xa5\x64\xa4\xf9";

static int news_uses_target_owner(int logical_id)
{
    return logical_id == 5 || logical_id == 6 || logical_id == 14 ||
           logical_id == 15 || logical_id == 18 || logical_id == 19 ||
           logical_id == 20 || logical_id == 21;
}

static int news_uses_property_name(int logical_id)
{
    return logical_id == 5 || logical_id == 6 || logical_id == 7 ||
           logical_id == 14 || logical_id == 15 || logical_id == 18 ||
           logical_id == 19 || logical_id == 20 || logical_id == 21;
}

static const char *news_property_name(
    const GamePreviewAssets *assets, const GameState *state,
    unsigned int target_mask, char out[21])
{
    GameDomainRef target;
    const MapData *map;
    const unsigned char *source = NULL;
    int background;

    if (assets == NULL || state == NULL || out == NULL ||
        !rich4_special_event_decode_property_target(
            state, target_mask, &target))
        return NULL;
    background = state->stage * GAME_MAPS_PER_STAGE + state->map;
    if (background < 0 || background >= GAME_BACKGROUND_COUNT)
        return NULL;
    map = &assets->map.maps[background];
    if (target.kind == GAME_DOMAIN_LAND && target.id >= 0 &&
        map->lands != NULL && (size_t)(target.id + 1) < map->land_count)
        source = map->lands[target.id + 1].raw + 4;
    else if (target.kind == GAME_DOMAIN_FACILITY && target.id >= 0 &&
             map->facilities != NULL &&
             (size_t)(target.id + 1) < map->facility_count)
        source = map->facilities[target.id + 1].raw + 4;
    if (source == NULL) return NULL;
    memcpy(out, source, 20);
    out[20] = '\0';
    return out;
}

static int game_special_event_player_visible(
    const GameState *state, int player)
{
    return state != NULL && player >= 0 && player < state->player_count &&
        (state->players[player].play_state & 0xff) != 0;
}

static void draw_special_event_player_portrait(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameState *state, int player, int chunk, int x, int y)
{
    int character;
    if (!game_special_event_player_visible(state, player)) return;
    character = state->players[player].character;
    if (character < 0 || character >= GAME_JUMP_CHARACTER_COUNT ||
        !assets->character.has_map_character[character])
        return;
    draw_spr_smp_center(
        screen, assets->character.map_character_data[character],
        assets->character.map_character_size[character],
        &assets->character.map_character_spr[character],
        (size_t)chunk, x, y);
}

static void draw_news_event_portraits(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session, int logical_id)
{
    const GameState *state = session->state;
    int player;
    int chunk = -1;
    int y = 328;
    if ((logical_id >= 11 && logical_id <= 13) || logical_id == 23) {
        const char *format = logical_id == 23
            ? news_bank_bonus_player_format : news_tax_player_format;
        y = 346;
        for (player = 0; player < state->player_count; player++) {
            int show = game_special_event_player_visible(state, player);
            int amount = session->presentation->special_event.player_amount[player];
            char text[128];
            if (logical_id == 23)
                show = show && state->players[player].loan == 0;
            else
                show = show && amount != 0;
            if (!show) continue;
            if (rich4_special_event_format_big5(
                    text, sizeof(text), format,
                    new_game_character_name(state->players[player].character),
                    amount))
                draw_big5_text(
                    screen, 24, y, text, 24,
                    rich4_character_rgb565_color(0x00f0f0f0u),
                    rich4_character_rgb565_color(0x00101010u), 3, 0);
            draw_special_event_player_portrait(
                screen, assets, state, player, logical_id == 23 ? 4 : 3,
                390, y + 12);
            y += 32;
        }
        return;
    }
    if (logical_id >= 0 && logical_id <= 3) {
        int prison = logical_id <= 1;
        chunk = (logical_id == 0 || logical_id == 2) ? 4 : 3;
        for (player = 0; player < state->player_count; player++) {
            unsigned int targets = session->presentation->special_event.target_mask;
            if (targets & (1u << (16 + player))) {
                draw_special_event_player_portrait(
                    screen, assets, state, player, chunk, 390, y);
                y += 42;
            }
        }
        (void)prison;
        return;
    }
    if (logical_id == 8 || logical_id == 9 || logical_id == 10) {
        int selected = session->presentation->special_event.reaction_player;
        /* 0x4498B3, 0x449A8A and 0x449B9C all jump through the +0x3c
         * portrait tail for the phase-0 winner. */
        draw_special_event_player_portrait(
            screen, assets, state, selected, 4, 390, y);
        return;
    }
    if (logical_id == 16 || logical_id == 17)
        chunk = 2;
    if (chunk < 0) return;
    for (player = 0; player < state->player_count; player++) {
        int show = game_special_event_player_visible(state, player);
        if (logical_id == 16)
            show = show && state->players[player].traffic_method == 0;
        else if (logical_id == 17)
            show = show && state->players[player].traffic_method != 0;
        if (show) {
            draw_special_event_player_portrait(
                screen, assets, state, player, chunk, 390, y);
            y += 32;
        }
    }
}

static void draw_fortune_card_target_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    int player;
    char text[128];
    const GameState *state;
    if (screen == NULL || session == NULL || session->state == NULL ||
        session->presentation == NULL)
        return;
    player = session->presentation->special_event.fortune_card_target_player;
    state = session->state;
    if (!game_special_event_player_visible(state, player)) return;
    if (!rich4_special_event_format_big5(
            text, sizeof(text), fortune_card_target_format,
            new_game_character_name(state->players[player].character), 0))
        return;
    /* 0x44C3B7 calls 0x44FABC(0, text, 24, 10, 0) immediately before
     * the per-player card-transfer display.  The map is already underneath
     * this overlay in the SDL frame compositor. */
    draw_big5_text(
        screen, 24, 10, text, 24,
        rich4_character_rgb565_color(0x00f0f0f0u),
        rich4_character_rgb565_color(0x00101010u), 3, 0);
    /* 0x44192A(...,0): only Panel[11].chunk[0] at (14,70), no tools. */
    draw_game_panel_chunk(screen,assets,11,0,14,70,0);
    {
        int i;
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; ++i) {
            int card = state->players[player].card_inventory[i];
            if (card > 0 && card <= GAME_CARD_COUNT)
                draw_big5_text(screen,59+(i%5)*80,103+(i/5)*56,
                    game_card_big5_name(card-1),20,
                    rich4_character_rgb565_color(0x00ffffffu),
                    rich4_character_rgb565_color(0x00101010u),3,2);
        }
        i = session->presentation->special_event.fortune_choice_hover;
        if (i >= 0 && i < GAME_PLAYER_CARD_SLOTS) {
            SDL_LockSurface(screen);
            stroke_rect(screen,19+(i%5)*80,75+(i/5)*56,80,56,0xffff);
            SDL_UnlockSurface(screen);
        }
    }
}

void draw_special_event_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    int asset_index;
    int family;
    int actor;
    int character = -1;
    int logical_id;
    int resolved_id;
    int value;
    char body[512];
    char property_name[21];
    const char *subject;
    const char *second_subject = NULL;
    const char *marked_text = "";
    if (screen == NULL || assets == NULL || session == NULL ||
        !session->presentation->special_event.active)
        return;
    if (session->presentation->special_event.phase ==
            RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET) {
        draw_fortune_card_target_presentation(screen, assets, session);
        return;
    }
    if (session->presentation->special_event.phase == RICH4_SPECIAL_PRESENT_PASSIVE_CARD ||
        session->presentation->special_event.phase == RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET ||
        session->presentation->special_event.phase == RICH4_SPECIAL_PRESENT_GOD_NOTICE ||
        session->presentation->special_event.phase == RICH4_SPECIAL_PRESENT_REDIRECT_NOTICE) {
        draw_fortune_passive_choice(screen,assets,session);
        return;
    }
    if (session->presentation->special_event.phase ==
            RICH4_SPECIAL_PRESENT_REACTION ||
        session->presentation->special_event.phase ==
            RICH4_SPECIAL_PRESENT_REACTION_DELAY ||
        session->presentation->special_event.phase ==
            RICH4_SPECIAL_PRESENT_POST_HOLD ||
        session->presentation->special_event.phase == RICH4_SPECIAL_PRESENT_PASSIVE_ACTOR_SPEECH ||
        session->presentation->special_event.phase == RICH4_SPECIAL_PRESENT_PASSIVE_VICTIM_SPEECH)
        return;
    if (session->presentation->special_event.phase ==
            RICH4_SPECIAL_PRESENT_CONFINEMENT_ANIMATION) {
        const unsigned char *data;
        size_t size;
        int prison =
            session->presentation->special_event.confinement ==
                GAME_CONFINEMENT_PRISON;
        int resource = prison ? 538 : 524;
        if (game_tool_effect_data(assets, resource, &data, &size))
            draw_game_graph_frame_data(
                screen, data, size,
                session->presentation->special_event.frame,
                0, prison ? 40 : 210);
        return;
    }
    if (session->presentation->special_event.phase ==
            RICH4_SPECIAL_PRESENT_FORTUNE_ANIMATION) {
        const unsigned char *data;
        size_t size;
        int resource =
            session->presentation->special_event.resolved_id == 7
                ? 533 : 558;
        if (game_tool_effect_data(assets, resource, &data, &size))
            draw_game_graph_frame_data(
                screen, data, size,
                session->presentation->special_event.frame, 0, 40);
        return;
    }
    if (session->presentation->special_event.phase ==
            RICH4_SPECIAL_PRESENT_CONSEQUENCE) {
        int consequence =
            session->presentation->special_event.consequence_index;
        if (consequence >= 0 &&
            consequence < GAME_NEWS_CONSEQUENCE_COUNT &&
            game_asset_ready(assets, &assets->ui.has_news_consequence[consequence]))
            draw_game_graph_frame_data(
                screen, assets->ui.news_consequence_data[consequence],
                assets->ui.news_consequence_size[consequence],
                session->presentation->special_event.frame, 0, 40);
        return;
    }
    family = session->presentation->special_event.family;
    actor = session->presentation->special_event.actor;
    logical_id = session->presentation->special_event.logical_id;
    resolved_id = session->presentation->special_event.resolved_id;
    value = session->presentation->special_event.value;
    if (actor >= 0 && actor < session->state->player_count)
        character = session->state->players[actor].character;
    asset_index = game_special_event_asset_index(
        assets, session->presentation->special_event.resource);
    if (family == GAME_SPECIAL_EVENT_NEWS) {
        if ((news_uses_target_owner(logical_id) ||
             logical_id == 8 || logical_id == 9 || logical_id == 10) &&
            session->presentation->special_event.reaction_player >= 0 &&
            session->presentation->special_event.reaction_player <
                session->state->player_count)
            character = session->state->players[
                session->presentation->special_event.reaction_player]
                .character;
        subject = new_game_character_name(character);
        second_subject = subject;
        if (logical_id >= 29 && logical_id <= 35) {
            unsigned int mask = session->presentation->special_event.target_mask;
            int company = logical_id == 29 ? (int)((mask >> 8) & 0xffffu) - 1 :
                (int)(mask & 0xffffu) - 1;
            if (company >= 0 && company < session->state->company_count)
                subject = session->presentation->map_company_names[company];
            if (logical_id == 29) {
                int owner = (int)((mask >> 24) & 0x0fu) - 1;
                if (owner >= 0 && owner < session->state->player_count)
                    second_subject = new_game_character_name(session->state->players[owner].character);
            }
        } else if (logical_id == 27 || logical_id == 28) {
            int stock = (int)(session->presentation->special_event.target_mask & 0xffffu) - 1;
            if (stock >= 0 && stock < GAME_STOCK_COUNT)
                subject = game_stock_name_for_state(session->state,stock);
        }
        draw_game_panel_chunk(screen, assets, 66, 0, 0, 0, 0);
        if (asset_index >= 0) {
            Rich4SprSmpChunk image;
            const unsigned char *data =
                assets->ui.special_event_data[asset_index];
            size_t size = assets->ui.special_event_size[asset_index];
            uint16_t image_width = 0;
            uint16_t image_height = 0;
            memset(&image, 0, sizeof(image));
            image.width = 388;
            image.height = 251;
            if (size >= 4u &&
                rich4_byte_reader_le16(data, size, 0, &image_width) &&
                rich4_byte_reader_le16(data, size, 2, &image_height) &&
                image_width == 388 && image_height == 251)
                image.data_offset = 4u;
            image.data_size = 388u * 251u * 2u;
            if (size >= image.data_offset + image.data_size)
                draw_chunk_topleft(screen, data, &image, 25, 44, 0);
        }
        draw_big5_text(
            screen, 24, 8, rich4_news_title_big5(logical_id), 28,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 0);
        if (logical_id == 1 || logical_id == 3)
            value = 3;
        else if (logical_id >= 8 && logical_id <= 10) {
            int winner = session->presentation->special_event.reaction_player;
            value = winner >= 0 && winner < session->state->player_count ?
                session->presentation->special_event.player_amount[winner] : 0;
        }
        marked_text = rich4_news_text_big5(logical_id);
        if (news_uses_property_name(logical_id)) {
            const char *selected_name = news_property_name(
                assets, session->state,
                session->presentation->special_event.target_mask,
                property_name);
            if (selected_name != NULL)
                subject = selected_name;
        }
        if (rich4_special_event_format_names_big5(
                body, sizeof(body), marked_text,
                subject, second_subject, value))
            draw_big5_text(
                screen, 24, 310, body, 28,
                rich4_character_rgb565_color(0x00f0f0f0u),
                rich4_character_rgb565_color(0x00101010u), 3, 0);
        draw_news_event_portraits(
            screen, assets, session, logical_id);
    } else if (family == GAME_SPECIAL_EVENT_FORTUNE) {
        int portrait_chunk;
        draw_game_panel_chunk(screen, assets, 66, 1, 0, 0, 0);
        if (asset_index >= 0) {
            Rich4SprSmpChunk image;
            const unsigned char *data =
                assets->ui.special_event_data[asset_index];
            size_t size = assets->ui.special_event_size[asset_index];
            uint16_t width16 = 0;
            uint16_t height16 = 0;
            int width = 0;
            int height = 0;
            if (size >= 4u &&
                rich4_byte_reader_le16(data, size, 0, &width16) &&
                rich4_byte_reader_le16(data, size, 2, &height16)) {
                width = (int)width16;
                height = (int)height16;
            }
            if (width > 0 && height > 0 &&
                size >= 4u + (size_t)width * (size_t)height * 2u) {
                memset(&image, 0, sizeof(image));
                image.width = width;
                image.height = height;
                image.data_offset = 4;
                image.data_size = (size_t)width * (size_t)height * 2u;
                draw_chunk_topleft(screen, data, &image, 25, 44, 0);
            } else if (size == (size_t)(388 * 251 * 2)) {
                memset(&image, 0, sizeof(image));
                image.width = 388;
                image.height = 251;
                image.data_offset = 0;
                image.data_size = size;
                draw_chunk_topleft(screen, data, &image, 25, 44, 0);
            } else {
                draw_game_graph_frame_data(
                    screen, data, size,
                    session->presentation->special_event.frame, 0, 40);
            }
        }
        marked_text = rich4_fortune_text_big5(resolved_id);
        if (rich4_special_event_format_big5(
                body, sizeof(body), marked_text,
                new_game_character_name(character), value))
            draw_big5_text(
                screen, 24, 330, body, 28,
                rich4_character_rgb565_color(0x00f0f0f0u),
                rich4_character_rgb565_color(0x00101010u), 3, 0);
        portrait_chunk = rich4_fortune_portrait_chunk(resolved_id);
        if (portrait_chunk >= 0)
            draw_special_event_player_portrait(
                screen, assets, session->state, actor,
                portrait_chunk, 390, 344);
    }
}
