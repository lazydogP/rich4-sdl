#include "platform/sdl1/game_lazy_assets.h"
#include "platform/sdl1/game_event_presentation.h"

#include <stdio.h>
#include <string.h>

#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "view/view_constants.h"
#include "view/map_renderer.h"

static int auction_property_chunk(const GameState *state, GameDomainRef target)
{
    static const int stage_one_facility[4] = { 134, 167, 151, 51 };
    static const int stage_one_special[4] = { 150, 183, 167, 62 };
    int owner, character, level, type;
    if (state == NULL || state->stage < 0 || state->stage > 1 ||
        state->map < 0 || state->map > 3)
        return -1;
    if (target.kind == GAME_DOMAIN_LAND && target.id >= 0 &&
        target.id < state->land_count) {
        owner = state->land_owner[target.id];
        level = state->land_level[target.id];
        type = state->land_type[target.id];
        if (level == 0) {
            if (owner < 0 || owner >= state->player_count) return 90;
            character = state->players[owner].character;
            return character >= 0 && character < 12 ? 91 + character : 90;
        }
        if (state->stage != 0)
            return type != 0 ? state->map + 131 :
                level + 5 * state->map + 115;
        return type != 0 ? 50 : level + 5 * state->map + 29;
    }
    if (target.kind == GAME_DOMAIN_FACILITY && target.id >= 0 &&
        target.id < state->facility_count) {
        owner = state->facility_owner[target.id];
        level = state->facility_level[target.id];
        type = state->facility_type[target.id];
        if (level == 0) {
            if (owner < 0 || owner >= state->player_count) return 103;
            character = state->players[owner].character;
            return character >= 0 && character < 12 ? 104 + character : 103;
        }
        if (state->stage == 0)
            return type == 0 ? 51 : level + 5 * (type - 1) + 51;
        if (type == 0) return state->map == 0 ? 151 : 51;
        if (type == 3) return stage_one_special[state->map];
        if (type == 1 || type == 2 || type == 4) {
            int slot = type == 4 ? 2 : type - 1;
            return stage_one_facility[state->map] + 5 * slot + level;
        }
    }
    return -1;
}

static int auction_initial_status(const GameState *state,
                                  const GameAuctionState *auction, int player)
{
    const GamePlayerState *p = &state->players[player];
    int status = p->cash <= auction->reserve_price ? 8 : 0;
    if (p->delayed_status_block != 0) status = 1;
    if (p->days_disappearing != 0) status = 2;
    if (p->days_in_prison != 0) status = 3;
    if (p->days_in_hospital != 0) status = 4;
    if (p->days_sleeping != 0) status = 5;
    if (p->days_sleep_walking != 0) status = 6;
    if (player == auction->seller) status = 7;
    return status;
}

static const char *auction_status_text(int status)
{
    static const char *text[] = {
        "", "\xa6\xed\xb1\x4a\xa4\xa4", "\xae\xf8\xa5\xa2\xa4\xa4",
        "\xa7\xa4\xa8\x63\xa4\xa4", "\xa6\xed\xb0\x7c\xa4\xa4",
        "\xa5\x56\xaf\x76\xa4\xa4", "\xb9\xda\xb9\x43\xa4\xa4",
        "\xbd\xe6\xa4\xe8"
    };
    return status >= 1 && status <= 7 ? text[status] : "";
}

/* Raw NUL-terminated Big5 at 0x465038/7D/98/63 and 0x475B54. */
static void draw_auction_speech(SDL_Surface *screen,
    const GamePreviewAssets *assets, const GameAuctionPresentationState *a)
{
    static const char *buyer[12] = {
        "\xae\xa5\xb3\xdf\xac\xf9\xbf\xab\xb3\xec\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49",
        "\xae\xa5\xb3\xdf\xa8\x46\xb6\xa9\xa4\xda\xb4\xb5\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49",
        "\xae\xa5\xb3\xdf\xa7\xd4\xa4\xd3\xad\xa6\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49",
        "\xae\xa5\xb3\xdf\xbf\xfa\xa4\xd2\xa4\x48\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49",
        "\xae\xa5\xb3\xdf\xaa\xfc\xa4\x67\xa7\x42\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49",
        "\xae\xa5\xb3\xdf\xb2\xef\xa9\xd4\xa4\xbd\xa5\x44\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49",
        "\xae\xa5\xb3\xdf\xae\x63\xa5\xbb\xc4\x5f\xc2\xc3\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49",
        "\xae\xa5\xb3\xdf\xbf\x7d\xbf\x7d\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49",
        "\xae\xa5\xb3\xdf\xaf\x51\xab\x7d\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49",
        "\xae\xa5\xb3\xdf\xae\x5d\xa4\x70\xac\xfc\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49",
        "\xae\xa5\xb3\xdf\xa4\x70\xa4\xa6\xa5\xa7\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49",
        "\xae\xa5\xb3\xdf\xaa\xf7\xa8\xa9\xa8\xa9\xc1\xca\xb1\x6f\xa6\xb9\xa6\x61\xa1\x49"
    };
    const char *text = NULL;
    char message[128];
    if (!a->speech_visible) return;
    switch (a->speech_resource) {
    case 131: text = "\xa4\xbd\xb6\x7d\xa9\xe7\xbd\xe6\xa4\x67\xa6\x61\xa4\x40\xb3\x42\xa1\x43"; break;
    case 132:
        snprintf(message, sizeof(message), "\xa9\xb3\xbb\xf9\x25\x64\xa4\xb8\x0a\xbd\xd0\xb7\x4e\xaa\xcc\xa5\x58\xbb\xf9\xa1\x43", a->bidding.reserve_price);
        text = message; break;
    case 135:
        snprintf(message, sizeof(message), "\x25\x64\xa4\xb8\xa6\xa8\xa5\xe6", a->bidding.current_price);
        text = message; break;
    case 148: text = "\xb5\x4c\xa4\x48\xa5\x58\xbb\xf9\xa1\x41\xab\xc5\xa7\x47\xac\x79\xbc\xd0\xa1\x43"; break;
    default:
        if (a->speech_resource >= 136 && a->speech_resource <= 147)
            text = buyer[a->speech_resource - 136];
        break;
    }
    if (text == NULL) return;
    draw_confinement_panel_chunk(screen, assets, 26, 2, 410, 60);
    draw_big5_text(screen, 410, 60, text, 20, RICH4_FONT_DARK, 0, 2, 4);
}

static void draw_auctioneer(SDL_Surface *screen,
    const GamePreviewAssets *assets, const GameAuctionPresentationState *a)
{
    static const int upper[] = {26, 27, 26};
    static const int lower[] = {18, 19, 18};
    int sold = a->stage == RICH4_AUCTION_PRESENT_SOLD_CALL;
    if (a->expression_frame >= 0 && a->expression_frame < 3) {
        if (a->expression_kind == 1 && !sold)
            draw_confinement_panel_chunk(screen, assets, 26,
                upper[a->expression_frame], 163, 45);
        else if (a->expression_kind == 2)
            draw_game_panel_chunk(screen, assets, 26,
                lower[a->expression_frame], 127, 90, 0);
    }
    if (a->mouth_drawn) {
        Rich4SprSmpChunk base;
        int x = sold ? 183 : 163, y = sold ? 65 : 60;
        /* 0x43BA78/0x43BAA2 and 0x43B8FF/0x43B928:
         * actual push/ESP operands are Rect.left/top, not right/bottom.
         * Restore the opaque Panel[26] chunk-0 crop, then transparent mouth. */
        if (rich4_spr_smp_get_chunk(assets->ui.panel_data[26],
                assets->ui.panel_size[26], &assets->ui.panel_spr[26],
                0, &base) == RICH4_SPR_SMP_OK)
            draw_chunk_crop_topleft(screen, assets->ui.panel_data[26], &base,
                x, y, sold ? 40 : 30, sold ? 30 : 25, x, y, 0);
        draw_confinement_panel_chunk(screen, assets, 26,
            (sold ? 22 : 28) + (a->mouth_ticks > 0), x, y);
    }
}

static void draw_auction_participant(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot,
    int player, int row)
{
    const GameAuctionPresentationState *auction =
        &snapshot->presentation->auction;
    int character = snapshot->state->players[player].character;
    int participant = auction->bidding.participant[player];
    int status = auction_initial_status(
        snapshot->state, &auction->bidding, player);
    int resource;
    size_t chunk = 0;
    char cash[32];
    if (character < 0 || character >= 12) character = 0;
    resource = 27 + 3 * character;
    if ((status != 0 && status != 7) ||
        participant == GAME_AUCTION_PARTICIPANT_WITHDRAWN)
        resource++;
    else if ((auction->stage == RICH4_AUCTION_PRESENT_SOLD_CALL ||
              auction->stage == RICH4_AUCTION_PRESENT_WINNER) &&
             auction->bidding.leader == player &&
             auction->celebration_tick > 0) {
        resource += 2;
        if (game_asset_ready(assets, &assets->ui.has_panel[resource]) &&
            assets->ui.panel_spr[resource].num_chunks > 0)
            chunk = (size_t)(auction->celebration_tick - 1) %
                assets->ui.panel_spr[resource].num_chunks;
    } else if (auction->stage == RICH4_AUCTION_PRESENT_ACTION &&
               auction->bidding.last_player == player &&
               auction->bidding.last_bid > 0 &&
               auction->action_tick > 0 &&
               game_asset_ready(assets, &assets->ui.has_panel[resource]) &&
               assets->ui.panel_spr[resource].num_chunks > 1 &&
               auction->action_tick <
                   (int)assets->ui.panel_spr[resource].num_chunks) {
        chunk = (size_t)auction->action_tick;
    }
    /* 0x45663E: Panel[27 + 3*character + pose] is indexed SPR,
     * with a palette and hotspot, unlike the Panel[26] SMP furniture. */
    draw_game_panel_chunk_at(
        screen, assets, resource, chunk, 590, row, 1);
    if (status >= 1 && status <= 7)
        draw_big5_text_with_spacing(screen, 590, row + 14, auction_status_text(status),
                       18, 0xffff, RICH4_FONT_DARK, 3, 0, 2);
    else {
        snprintf(cash, sizeof(cash), "%d\xa4\xb8",
                 snapshot->state->players[player].cash);
        draw_big5_text_with_spacing(screen, 620, row + 14, cash, 18, 0xffff,
                       RICH4_FONT_DARK, 3, 0, 6);
        if ((auction->bidder_mask & (1u << player)) != 0 &&
            auction->bidding.leader == player) {
            snprintf(cash, sizeof(cash), "%d\xa4\xb8", auction->bidding.current_price);
            draw_big5_text_with_spacing(screen, 620, row + 25, cash, 18,
                rich4_character_rgb565_color(0xf0d020u), RICH4_FONT_DARK, 3, 0, 1);
        }
    }
}

static int auction_player_row(
    const Rich4GameEventPresentationSnapshot *snapshot, int wanted)
{
    int player;
    int row = 80;
    const GameState *state = snapshot->state;
    for (player = 0; player < state->player_count; player++) {
        if ((state->players[player].play_state & 0xff) == 0 ||
            snapshot->presentation->actors[player].first_turn_landing_pending)
            continue;
        if (player == wanted) return row;
        row += 120;
    }
    return -1;
}

static void draw_auction_action_feedback(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameAuctionPresentationState *auction,
    const Rich4GameEventPresentationSnapshot *snapshot)
{
    static const char pass_text[] =
        "\xa2\xde\xa2\xcf\xa2\xe1\xa2\xe1";
    static const char give_up_text[] = "\xa9\xf1\xb1\xf3";
    const char *text = NULL;
    char price[32];
    int row;
    if (auction->stage != RICH4_AUCTION_PRESENT_ACTION ||
        auction->action_tick <= 0 ||
        auction->action_tick >= 10)
        return;
    row = auction_player_row(snapshot, auction->bidding.last_player);
    if (row < 0) return;
    if (auction->bidding.last_action == GAME_AUCTION_PASS)
        text = pass_text;
    else if (auction->bidding.last_action == GAME_AUCTION_GIVE_UP)
        text = give_up_text;
    else if (auction->bidding.last_bid > 0) {
        snprintf(price, sizeof(price), "%d\xa4\xb8", auction->bidding.current_price);
        text = price;
    }
    if (text == NULL) return;
    draw_confinement_panel_chunk(screen, assets, 26, 1, 480, row - 15);
    draw_big5_text_with_spacing(screen, 480, row - 15, text, 16, RICH4_FONT_DARK,
                   0, 2, 0, 2);
}

void draw_auction_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot)
{
    const GameAuctionPresentationState *auction;
    char price[32];
    int property_chunk;
    int player;
    int row = 80;
    if (screen == NULL || assets == NULL || snapshot == NULL ||
        snapshot->state == NULL || snapshot->presentation == NULL)
        return;
    auction = &snapshot->presentation->auction;
    if (!auction->active || !game_asset_ready(assets, &assets->ui.has_panel[26])) return;

    /* sub_43BDE5: fullscreen base, heading and property surround. */
    draw_game_panel_chunk(screen, assets, 26, 0, 0, 0, 0);
    if (auction->stage == RICH4_AUCTION_PRESENT_SOLD_CALL)
        draw_confinement_panel_chunk(screen, assets, 26, 21, 124, 27);
    else
        draw_confinement_panel_chunk(screen, assets, 26, 25, 123, 24);
    draw_confinement_panel_chunk(screen, assets, 26, 17, 67, 63);
    property_chunk = auction_property_chunk(
        snapshot->state, auction->bidding.target);
    if (property_chunk >= 0)
        draw_confinement_panel_chunk(
            screen, assets, 26, (size_t)property_chunk, 232, 180);
    draw_auctioneer(screen, assets, auction);
    /* 0x43C3EA: fixed label at 0x4650A6, not the map property's name. */
    draw_big5_text_with_spacing(screen, 182, 242, "\xbc\xd0\xbb\xf9\xa1\x47",
        18, 0xffff, RICH4_FONT_DARK, 3, 0, 0);
    snprintf(price, sizeof(price), "%d\xa4\xb8", auction->bidding.current_price);
    draw_big5_text_with_spacing(screen, 272, 262, price, 18, 0xffff, RICH4_FONT_DARK, 3, 0, 1);

    for (player = 0; player < snapshot->state->player_count; player++) {
        int character;
        if ((snapshot->state->players[player].play_state & 0xff) == 0 ||
            snapshot->presentation->actors[player].first_turn_landing_pending)
            continue;
        character = snapshot->state->players[player].character;
        if ((auction->stage != RICH4_AUCTION_PRESENT_ACTION
                ? player == auction->bidding.current_player
                : player == auction->bidding.last_player &&
                  (auction->bidding.last_action == GAME_AUCTION_PASS ||
                   (auction->bidding.last_bid > 0 && auction->action_tick == 0))) &&
            character >= 0 && character < 12)
            draw_confinement_panel_chunk(
                screen, assets, 26, (size_t)(78 + character), 590, row);
        draw_auction_participant(screen, assets, snapshot, player, row);
        row += 120;
    }

    draw_auction_action_feedback(
        screen, assets, auction, snapshot);
    draw_auction_speech(screen, assets, auction);

    if (auction->stage == RICH4_AUCTION_PRESENT_BIDDING ||
        auction->stage == RICH4_AUCTION_PRESENT_ACTION ||
        auction->stage == RICH4_AUCTION_PRESENT_UNSOLD) {
        int current = auction->stage == RICH4_AUCTION_PRESENT_BIDDING ?
            auction->bidding.current_player : auction->bidding.last_player;
        if (current >= 0 && current < snapshot->state->player_count &&
            !snapshot->state->players[current].is_ai) {
            int action;
            for (action = GAME_AUCTION_PASS;
                 action <= GAME_AUCTION_GIVE_UP; action++) {
                int chunk = 4 + action * 2;
                if (auction->pressed_action == action) chunk--;
                draw_confinement_panel_chunk(
                    screen, assets, 26, (size_t)chunk,
                    406, 133 + action * 48);
            }
        }
    }
}
