#include "platform/sdl1/game_lazy_assets.h"
#include <stdio.h>
#include <SDL/SDL.h>

#include "app/card_panel_state.h"
#include "app/game_session.h"
#include "core/game_constants.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_route_render.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/flc_animation.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"

void game_draw_inventory_text(SDL_Surface *screen, int x, int y,
                              const char *big5, int anchor)
{
    draw_big5_text(
        screen, x, y, big5, 20,
        rich4_character_rgb565_color(0x00ffffffu),
        rich4_character_rgb565_color(0x00101010u), 3, anchor);
}

void game_draw_card_panel(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          const GameSession *session,
                          const CardPanelState *panel)
{
    int i;
    if (screen == NULL || assets == NULL || session == NULL || panel == NULL ||
        panel->phase == CARD_PANEL_CLOSED) return;
    if (panel->phase == CARD_PANEL_EFFECT) {
        int card = panel->selected_card;
        if (card >= 0 && card < GAME_CARD_COUNT && game_asset_ready(assets, &assets->ui.has_card_effect[card])) {
            {
                char title[64];
                snprintf(title, sizeof(title), "\xa8\xcf\xa5\xce%s",
                         game_card_big5_name(card));
                draw_original_card_presentation(
                    screen, assets, card, title);
            }
        }
        return;
    }
    if (panel->phase == CARD_PANEL_MONSTER_ANIMATION ||
        panel->phase == CARD_PANEL_DEMOLISH_ANIMATION) {
        const unsigned char *animation_data;
        size_t animation_size;
        int animation_available;
        Rich4FlcHeader header;
        int frame = panel->effect_frame;
        if (panel->phase == CARD_PANEL_MONSTER_ANIMATION) {
            animation_available = game_asset_ready(assets, &assets->ui.has_monster_card_animation);
            animation_data = assets->ui.monster_card_data;
            animation_size = assets->ui.monster_card_size;
        } else {
            animation_available = game_asset_ready(assets, &assets->ui.has_demolish_card_animation);
            animation_data = assets->ui.demolish_card_data;
            animation_size = assets->ui.demolish_card_size;
        }
        if (animation_available &&
            rich4_flc_parse_header(animation_data, animation_size,
                                   &header) == RICH4_FLC_OK) {
            if (header.frame_count <= 0) return;
            if (frame >= header.frame_count) frame = header.frame_count - 1;
            draw_game_graph_frame_data(
                screen, animation_data, animation_size,
                frame, (SCREEN_W - header.width) / 2,
                (SCREEN_H - header.height) / 2);
        }
        return;
    }
    if (panel->phase == CARD_PANEL_INVENTORY) {
        int owner = panel->player;
        if (!draw_game_panel_chunk(screen, assets, 11, 0, 14, 130, 0)) {
            SDL_LockSurface(screen);
            fill_rect(screen, 14, 130, 412, 180, 0x7bef);
            SDL_UnlockSurface(screen);
        }
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
            int id = session->state.players[owner].card_inventory[i];
            int col = i % 5, row = i / 5;
            if (id > 0 && id <= GAME_CARD_COUNT)
                game_draw_inventory_text(screen, 59 + col * 80,
                                    163 + row * 56,
                                    game_card_big5_name(id - 1), 2);
        }
        if (panel->selected_slot >= 0)
            draw_game_panel_chunk(screen, assets, 11, 15,
                19 + (panel->selected_slot % 5) * 80,
                135 + (panel->selected_slot / 5) * 56, 1);
        return;
    }
    if (panel->phase == CARD_PANEL_FACILITY_CHOICE) {
        draw_facility_choice_selector(screen, assets, panel->hover);
        return;
    }
    if (panel->phase == CARD_PANEL_ROBBERY_TARGET) {
        int owner = panel->primary_target;
        if (!draw_game_panel_chunk(screen, assets, 11, 0, 14, 70, 0)) {
            SDL_LockSurface(screen);
            fill_rect(screen, 14, 70, 412, 380, 0x7bef);
            SDL_UnlockSurface(screen);
        }
        draw_game_panel_chunk(screen, assets, 11, 1, 14, 270, 0);
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
            int id = session->state.players[owner].card_inventory[i];
            int col = i % 5, row = i / 5;
            if (id > 0 && id <= GAME_CARD_COUNT)
                game_draw_inventory_text(screen, 59 + col * 80,
                                    103 + row * 56,
                                    game_card_big5_name(id - 1), 2);
        }
        {
            int packed = 0;
        for (i = 0; i < GAME_TOOL_COUNT; i++) {
            if (session->state.players[owner].tool_inventory[i] != 0) {
                int col = packed % 5, row = packed / 5;
                char count[16];
                draw_game_panel_chunk_at(
                    screen, assets, 11, (size_t)(2 + i),
                    43 + col * 80, 303 + row * 56, 1);
                snprintf(count, sizeof(count), "\xa1\xd1%u",
                         (unsigned)session->state.players[owner]
                             .tool_inventory[i]);
                game_draw_inventory_text(screen, 93 + col * 80,
                                    293 + row * 56, count, 1);
                packed++;
            }
        }
        }
        if (panel->selected_slot >= 0) {
            int tool = (panel->selected_slot & 0x8000) != 0;
            int selected = panel->selected_slot & 0x7fff;
            SDL_LockSurface(screen);
            stroke_rect(screen, 19 + (selected % 5) * 80,
                        (tool ? 275 : 75) + (selected / 5) * 56,
                        80, 56, 0xffff);
            SDL_UnlockSurface(screen);
        }
        return;
    }
    if (panel->phase == CARD_PANEL_STOCK_TARGET) {
        SDL_LockSurface(screen);
        fill_rect(screen, 70, 72, 300, 330, 0x7bef);
        stroke_rect(screen, 70, 72, 300, 330, 0x0000);
        SDL_UnlockSurface(screen);
        /* Calls at 0x444FFA / 0x4450BE enter the shared 0x42B58F stock UI.
         * Neither path adds a red/black-card selection title. */
        for (i = 0; i < GAME_STOCK_COUNT; i++) {
            char label[48];
            snprintf(label, sizeof(label), "%02d   %.2f", i + 1,
                     (double)session->state.stocks[i].f12);
            draw_text(screen, 112 + (i / 8) * 130,
                      124 + (i % 8) * 31, label, 1, 0x0000);
        }
        return;
    }
    /* 0x446AE8 -> 0x445E4D selects map targets using cursor/sound
     * feedback; it draws no extra instruction banner. */
}
