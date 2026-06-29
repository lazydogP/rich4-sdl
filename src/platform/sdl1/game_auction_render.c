#include "platform/sdl1/game_lazy_assets.h"
#include "platform/sdl1/game_event_presentation.h"

#include <stdio.h>
#include <string.h>

#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "view/view_constants.h"

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

static void draw_auction_property_name(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameState *state, GameDomainRef target)
{
    const unsigned char *source = NULL;
    char name[21];
    int background = state->stage * GAME_MAPS_PER_STAGE + state->map;
    if (background < 0 || background >= GAME_BACKGROUND_COUNT) return;
    if (target.kind == GAME_DOMAIN_LAND && target.id >= 0 &&
        (size_t)(target.id + 1) < assets->map.maps[background].land_count)
        source = assets->map.maps[background].lands[target.id + 1].raw + 4;
    else if (target.kind == GAME_DOMAIN_FACILITY && target.id >= 0 &&
             (size_t)(target.id + 1) <
                 assets->map.maps[background].facility_count)
        source = assets->map.maps[background].facilities[target.id + 1].raw + 4;
    if (source == NULL) return;
    memset(name, 0, sizeof(name));
    memcpy(name, source, 20);
    draw_big5_text(screen, 182, 242, name, 16, 0xffff, 0, 0, 0);
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
        chunk = (size_t)(auction->action_tick - 1);
    }
    /* 0x45663E: Panel[27 + 3*character + pose] is indexed SPR,
     * with a palette and hotspot, unlike the Panel[26] SMP furniture. */
    draw_game_panel_chunk_at(
        screen, assets, resource, chunk, 590, row, 1);
    if (status >= 1 && status <= 7)
        draw_big5_text(screen, 590, row + 14, auction_status_text(status),
                       16, 0xffff, 0, 0, 0);
    else if ((auction->bidder_mask & (1u << player)) == 0 ||
             auction->bidding.leader == player) {
        snprintf(cash, sizeof(cash), "%d",
                 auction->bidding.leader == player &&
                         (auction->bidder_mask & (1u << player)) != 0
                     ? auction->bidding.current_price
                     : snapshot->state->players[player].cash);
        draw_big5_text(screen, 620, row + 14, cash, 18, 0xffff,
                       RICH4_FONT_DARK, 3, 6);
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
        snprintf(price, sizeof(price), "%d", auction->bidding.current_price);
        text = price;
    }
    if (text == NULL) return;
    draw_confinement_panel_chunk(screen, assets, 26, 1, 480, row - 15);
    draw_big5_text(screen, 480, row - 15, text, 16, RICH4_FONT_DARK,
                   0, 2, 2);
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
    draw_auction_property_name(
        screen, assets, snapshot->state, auction->bidding.target);
    snprintf(price, sizeof(price), "%d", auction->bidding.current_price);
    draw_text(screen, 272, 262, price, 1, 0xffff);

    for (player = 0; player < snapshot->state->player_count; player++) {
        int character;
        if ((snapshot->state->players[player].play_state & 0xff) == 0 ||
            snapshot->presentation->actors[player].first_turn_landing_pending)
            continue;
        character = snapshot->state->players[player].character;
        if (player == auction->bidding.current_player &&
            character >= 0 && character < 12)
            draw_confinement_panel_chunk(
                screen, assets, 26, (size_t)(78 + character), 590, row);
        draw_auction_participant(screen, assets, snapshot, player, row);
        row += 120;
    }

    draw_auction_action_feedback(
        screen, assets, auction, snapshot);

    if (auction->stage == RICH4_AUCTION_PRESENT_BIDDING) {
        int current = auction->bidding.current_player;
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
