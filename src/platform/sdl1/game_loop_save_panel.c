#include "platform/sdl1/game_lazy_assets.h"
#include <stdio.h>
#include <stdlib.h>

#include <SDL/SDL.h>

#include "app/game_save.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/stock.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/spr_smp.h"

enum {
    SAVE_LOAD_PANEL_LOAD_BACKGROUND_CHUNK = 0,
    SAVE_LOAD_PANEL_SAVE_BACKGROUND_CHUNK = 1,
    SAVE_LOAD_PANEL_SLOT_CHUNK = 10,
    SAVE_LOAD_PANEL_SLOT_FIRST_X = 129,
    SAVE_LOAD_PANEL_SLOT_LAST_X = 577,
    SAVE_LOAD_PANEL_SAVE_FIRST_Y = 55,
    SAVE_LOAD_PANEL_LOAD_FIRST_Y = 22,
    SAVE_LOAD_PANEL_ROW_HEIGHT = 72,
    SAVE_LOAD_PANEL_SAVE_ROWS = 5,
    SAVE_LOAD_PANEL_LOAD_ROWS = 6
};

static int save_load_panel_row_count(GameSaveLoadPanelMode mode)
{
    if (mode == GAME_SAVE_LOAD_PANEL_SAVE)
        return SAVE_LOAD_PANEL_SAVE_ROWS;
    if (mode == GAME_SAVE_LOAD_PANEL_LOAD)
        return SAVE_LOAD_PANEL_LOAD_ROWS;
    return 0;
}

static int save_load_panel_first_y(GameSaveLoadPanelMode mode)
{
    return mode == GAME_SAVE_LOAD_PANEL_SAVE
        ? SAVE_LOAD_PANEL_SAVE_FIRST_Y : SAVE_LOAD_PANEL_LOAD_FIRST_Y;
}

static int save_load_panel_slot(GameSaveLoadPanelMode mode, int row)
{
    return mode == GAME_SAVE_LOAD_PANEL_SAVE ? row + 1 : row;
}

static int save_load_panel_row_at(GameSaveLoadPanelMode mode, int x, int y)
{
    int row;
    int count = save_load_panel_row_count(mode);
    int first_y = save_load_panel_first_y(mode);
    if (x <= SAVE_LOAD_PANEL_SLOT_FIRST_X ||
        x >= SAVE_LOAD_PANEL_SLOT_LAST_X || y <= first_y ||
        y >= first_y + count * SAVE_LOAD_PANEL_ROW_HEIGHT)
        return -1;
    row = (y - first_y) / SAVE_LOAD_PANEL_ROW_HEIGHT;
    return row >= 0 && row < count ? row : -1;
}

static int save_load_panel_slot_valid(
    const GameSession *current, const GamePreviewAssets *assets, int slot,
    int *out_elapsed_days, int *out_player_count,
    GameSaveLoadSlotMetadata *out_metadata)
{
    char path[RICH4_GAME_SAVE_PATH_CAPACITY];
    GameSession *loaded;
    int map_index;
    int valid = 0;
    if (out_elapsed_days != NULL) *out_elapsed_days = 0;
    if (out_player_count != NULL) *out_player_count = 0;
    if (current == NULL || assets == NULL || !rich4_game_save_slot_path(
            path, sizeof(path), "saves", slot))
        return 0;
    loaded = (GameSession *)malloc(sizeof(*loaded));
    if (loaded == NULL) return 0;
    valid = rich4_game_save_read(path, slot, loaded);
    map_index = valid
        ? loaded->state.stage * GAME_MAPS_PER_STAGE + loaded->state.map : -1;
    valid = valid && map_index >= 0 && map_index < GAME_BACKGROUND_COUNT &&
        assets->map.maps[map_index].routes != NULL &&
        rich4_game_save_session_matches_map(
            loaded, &assets->map.maps[map_index]);
    if (valid) {
        if (out_elapsed_days != NULL)
            *out_elapsed_days = loaded->state.elapsed_days;
        if (out_player_count != NULL)
            *out_player_count = loaded->state.player_count;
        if (out_metadata != NULL) {
            int i;
            (void)rich4_stock_calendar_date(&loaded->state,
                &out_metadata->year, &out_metadata->month,
                &out_metadata->day, NULL);
            out_metadata->stage = loaded->state.stage;
            out_metadata->map = loaded->state.map;
            for (i = 0; i < loaded->state.player_count; i++)
                out_metadata->characters[i] = loaded->state.players[i].character;
        }
    }
    free(loaded);
    return valid;
}

static int game_save_load_panel_open_internal(
    GameSession *session, const GamePreviewAssets *assets,
    GameSaveLoadPanelMode mode, int from_menu)
{
    int slot_elapsed_days[6] = {0};
    int slot_player_counts[6] = {0};
    GameSaveLoadSlotMetadata metadata[6] = {{0}};
    unsigned int valid_slot_mask = 0;
    int slot;
    if (session == NULL || assets == NULL ||
        (!from_menu && session->state.active &&
         (session->state.current_player < 0 ||
          session->state.current_player >= session->state.player_count ||
          session->state.players[session->state.current_player].is_ai)) ||
        (!from_menu && !session->state.active) ||
        (from_menu && mode != GAME_SAVE_LOAD_PANEL_LOAD) ||
        save_load_panel_row_count(mode) == 0 ||
        !game_asset_ready(assets, &assets->ui.has_save_load_panel))
        return 0;
    for (slot = GAME_SAVE_AUTO_SLOT;
         slot < GAME_SAVE_SLOT_COUNT; slot++) {
        if (save_load_panel_slot_valid(
                session, assets, slot, &slot_elapsed_days[slot],
                &slot_player_counts[slot], &metadata[slot]))
            valid_slot_mask |= 1u << slot;
    }
    rich4_presentation_save_load_panel_begin(
        &session->presentation.save_load, mode, valid_slot_mask,
        slot_elapsed_days, slot_player_counts);
    rich4_presentation_save_load_panel_set_metadata(
        &session->presentation.save_load, metadata);
    return 1;
}

int game_save_load_panel_open(GameSession *session,
    const GamePreviewAssets *assets, GameSaveLoadPanelMode mode)
{
    return game_save_load_panel_open_internal(session, assets, mode, 0);
}

int game_save_load_panel_open_menu(GameSession *session,
    const GamePreviewAssets *assets)
{
    return game_save_load_panel_open_internal(
        session, assets, GAME_SAVE_LOAD_PANEL_LOAD, 1);
}

int game_save_load_panel_menu_activate(GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings, int x, int y)
{
    int row;
    if (session == NULL ||
        session->presentation.save_load.mode != GAME_SAVE_LOAD_PANEL_LOAD)
        return 0;
    row = save_load_panel_row_at(GAME_SAVE_LOAD_PANEL_LOAD, x, y);
    if (row < 0 ||
        (session->presentation.save_load.valid_slot_mask & (1u << row)) == 0)
        return 0;
    return game_session_load_menu_slot(session, assets, settings, row);
}

int game_save_load_panel_open_after_ending(
    GameSession *session, const GamePreviewAssets *assets)
{
    GameEndingPresentationState *ending;
    int previous_player;
    if (session == NULL) return 0;
    ending = &session->presentation.ending;
    /* 0x407842(arg == 0) plays Data.MKF[556] before it enters the
     * code-4 load path.  Start that animation once and let the frame loop
     * reopen this function with the ready bit after the last frame. */
    if (ending->continuation_active && ending->winner_player < 0 &&
        !ending->continuation_confirmation_ready &&
        !ending->continuation_confirmation_active && assets != NULL &&
        game_asset_ready(assets, &assets->ui.has_ending_success_animation)) {
        ending->continuation_confirmation_active = 1;
        ending->continuation_confirmation_frame = 0;
        rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_ENDING]);
        ending->continuation_confirmation_started = rich4_sdl1_ticks();
        return 1;
    }
    if (ending->continuation_confirmation_active) return 1;
    ending->continuation_confirmation_ready = 0;
    if (!rich4_app_begin_ending_load(session, &previous_player)) return 0;
    if (!game_save_load_panel_open(
            session, assets, GAME_SAVE_LOAD_PANEL_LOAD)) {
        rich4_app_cancel_ending_load(session, previous_player);
        return 0;
    }
    rich4_app_set_ending_load_previous_player(session, previous_player);
    return 1;
}

void game_save_load_panel_cancel(GameSession *session, Rich4Audio *audio)
{
    int previous_player;
    if (session == NULL || session->presentation.save_load.mode ==
            GAME_SAVE_LOAD_PANEL_CLOSED)
        return;
    previous_player =
        session->presentation.save_load.terminal_previous_player;
    rich4_presentation_save_load_panel_close(
        &session->presentation.save_load);
    rich4_app_cancel_ending_load(session, previous_player);
    if (audio != NULL) rich4_audio_play_cancel(audio);
}

static int save_load_panel_activate_slot(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, Rich4Audio *audio, int slot);

void game_save_load_panel_mouse_down(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, Rich4Audio *audio, int x, int y)
{
    GameSaveLoadPresentationState *panel;
    int row;
    if (session == NULL) return;
    panel = &session->presentation.save_load;
    if (panel->save_confirmation_active) return;
    row = save_load_panel_row_at(panel->mode, x, y);
    rich4_presentation_save_load_panel_press(panel, row);
    if (row >= 0)
        (void)save_load_panel_activate_slot(session, assets, settings,
            audio, save_load_panel_slot(panel->mode, row));
}

void game_save_load_panel_mouse_motion(GameSession *session, int x, int y)
{
    GameSaveLoadPresentationState *panel;
    int row;
    if (session == NULL) return;
    panel = &session->presentation.save_load;
    row = save_load_panel_row_at(panel->mode, x, y);
    if (panel->save_confirmation_active) return;
    if (row < 0) {
        rich4_presentation_save_load_panel_select(panel, -1);
        return;
    }
    rich4_presentation_save_load_panel_select(panel, row);
}

static int save_load_panel_activate_slot(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, Rich4Audio *audio, int slot)
{
    GameSaveLoadPanelMode mode;
    GameCommand command;
    int terminal_previous_player;
    int accepted;
    if (session == NULL || assets == NULL || settings == NULL ||
        session->presentation.save_load.mode ==
            GAME_SAVE_LOAD_PANEL_CLOSED)
        return 0;
    mode = session->presentation.save_load.mode;
    terminal_previous_player =
        session->presentation.save_load.terminal_previous_player;
    if (mode == GAME_SAVE_LOAD_PANEL_SAVE) {
        if (slot < 1 || slot > 5) return 0;
        command = rich4_game_command_save_game(
            GAME_COMMAND_SOURCE_HUMAN, session->state.current_player, slot);
    } else {
        if (slot < GAME_SAVE_AUTO_SLOT ||
            slot >= GAME_SAVE_SLOT_COUNT ||
            (session->presentation.save_load.valid_slot_mask &
             (1u << slot)) == 0)
            return 0;
        command = rich4_game_command_load_game(
            GAME_COMMAND_SOURCE_HUMAN, session->state.current_player, slot);
    }
    if (mode == GAME_SAVE_LOAD_PANEL_LOAD)
        rich4_presentation_save_load_panel_close(
            &session->presentation.save_load);
    accepted = game_session_apply_command_with_map_switch(
        session, assets, settings, &command, rich4_sdl1_ticks());
    if (!accepted) {
        fprintf(stderr, "save/load failed for slot %d\n", slot);
        if (!game_save_load_panel_open(session, assets, mode))
            rich4_presentation_save_load_panel_set_mode(
                &session->presentation.save_load, mode);
        rich4_app_set_ending_load_previous_player(
            session, terminal_previous_player);
        return 0;
    }
    if (mode == GAME_SAVE_LOAD_PANEL_SAVE) {
        int row = slot - 1;
        if (game_save_load_panel_open(session, assets, mode))
            rich4_presentation_save_load_panel_confirm_saved(
                &session->presentation.save_load, row,
                rich4_sdl1_ticks());
    }
    if (audio != NULL) rich4_audio_play_ok(audio);
    return 1;
}

int game_save_load_panel_mouse_release(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, Rich4Audio *audio,
    int button, int x, int y)
{
    if (session == NULL || session->presentation.save_load.mode ==
            GAME_SAVE_LOAD_PANEL_CLOSED)
        return 0;
    if (session->presentation.save_load.save_confirmation_active)
        return 1;
    if (button == SDL_BUTTON_RIGHT) {
        game_save_load_panel_cancel(session, audio);
        return 1;
    }
    if (button != SDL_BUTTON_LEFT) return 1;
    (void)assets;
    (void)settings;
    (void)x;
    (void)y;
    /* The original slot callback commits on WM_LBUTTONDOWN. */
    return 1;
}

int game_save_load_panel_key_down(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, Rich4Audio *audio,
    Rich4AppKey key)
{
    GameSaveLoadPresentationState *panel;
    int count;
    int row;
    if (session == NULL || session->presentation.save_load.mode ==
            GAME_SAVE_LOAD_PANEL_CLOSED)
        return 0;
    panel = &session->presentation.save_load;
    if (panel->save_confirmation_active) return 1;
    count = save_load_panel_row_count(panel->mode);
    if (key == RICH4_APP_KEY_ESCAPE) {
        game_save_load_panel_cancel(session, audio);
    } else if (key == RICH4_APP_KEY_UP || key == RICH4_APP_KEY_DOWN) {
        int direction = key == RICH4_APP_KEY_UP ? -1 : 1;
        row = panel->selected_row;
        if (row < 0) row = direction > 0 ? -1 : count;
        for (row += direction; row >= 0 && row < count; row += direction) {
            int slot = save_load_panel_slot(panel->mode, row);
            if (panel->mode == GAME_SAVE_LOAD_PANEL_SAVE ||
                (panel->valid_slot_mask & (1u << slot)) != 0) {
                panel->selected_row = row;
                break;
            }
        }
    } else if (key == RICH4_APP_KEY_RETURN ||
               key == RICH4_APP_KEY_KP_ENTER) {
        if (panel->selected_row >= 0 && panel->selected_row < count) {
            int slot = save_load_panel_slot(panel->mode,
                                            panel->selected_row);
            (void)save_load_panel_activate_slot(
                session, assets, settings, audio, slot);
        }
    }
    return 1;
}

int game_save_load_panel_confirm(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, Rich4Audio *audio)
{
    GameSaveLoadPresentationState *panel;
    if (session == NULL || session->presentation.save_load.mode ==
            GAME_SAVE_LOAD_PANEL_CLOSED)
        return 0;
    panel = &session->presentation.save_load;
    if (panel->save_confirmation_active) return 1;
    if (panel->selected_row >= 0 &&
        panel->selected_row < save_load_panel_row_count(panel->mode)) {
        int slot = save_load_panel_slot(panel->mode, panel->selected_row);
        (void)save_load_panel_activate_slot(
            session, assets, settings, audio, slot);
    }
    return 1;
}

static int save_load_panel_draw_chunk(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    size_t index, int x, int y, int transparent_zero)
{
    Rich4SprSmpChunk chunk;
    if (screen == NULL || assets == NULL ||
        !game_asset_ready(assets, &assets->ui.has_save_load_panel) ||
        rich4_spr_smp_get_chunk(
            assets->ui.save_load_panel_data,
            assets->ui.save_load_panel_size,
            &assets->ui.save_load_panel_spr, index, &chunk) !=
                RICH4_SPR_SMP_OK)
        return 0;
    return draw_chunk_at(screen, assets->ui.save_load_panel_data,
                         &chunk, x, y, transparent_zero);
}

static int save_load_panel_draw_character(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    int character, int x, int y)
{
    Rich4SprSmpChunk chunk;
    if (character < 0 || !game_asset_ready(assets, &assets->ui.has_save_load_icons) ||
        (size_t)character >= assets->ui.save_load_icons_spr.num_chunks ||
        rich4_spr_smp_get_chunk(assets->ui.save_load_icons_data,
            assets->ui.save_load_icons_size,
            &assets->ui.save_load_icons_spr, (size_t)character,
            &chunk) != RICH4_SPR_SMP_OK)
        return 0;
    return draw_chunk_at(screen, assets->ui.save_load_icons_data,
                         &chunk, x, y, 0);
}

void game_draw_save_load_panel(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session)
{
    const GameSaveLoadPresentationState *panel;
    int row;
    int count;
    int first_y;
    if (screen == NULL || assets == NULL || session == NULL) return;
    panel = &session->presentation.save_load;
    count = save_load_panel_row_count(panel->mode);
    if (count == 0 || !game_asset_ready(assets, &assets->ui.has_save_load_panel)) return;
    (void)save_load_panel_draw_chunk(screen, assets,
        panel->mode == GAME_SAVE_LOAD_PANEL_SAVE
            ? SAVE_LOAD_PANEL_SAVE_BACKGROUND_CHUNK
            : SAVE_LOAD_PANEL_LOAD_BACKGROUND_CHUNK,
        40, panel->mode == GAME_SAVE_LOAD_PANEL_SAVE ? 48 : 15, 1);
    first_y = save_load_panel_first_y(panel->mode);
    for (row = 0; row < count; row++) {
        int slot = save_load_panel_slot(panel->mode, row);
        int saved = (panel->valid_slot_mask & (1u << slot)) != 0;
        /* Original blits begin two pixels below the hit-test row bounds. */
        int top = first_y + row * SAVE_LOAD_PANEL_ROW_HEIGHT + 2;
        char year[32];
        char date[32];
        const uint16_t color = 0xf79e; /* Original 0xF0F0F0 text color. */
        if (!saved)
            continue;
        (void)save_load_panel_draw_chunk(
            screen, assets, SAVE_LOAD_PANEL_SLOT_CHUNK, 129, top, 0);
        if (slot == GAME_SAVE_AUTO_SLOT)
            draw_big5_text(screen, 165, top + 15, "AUTO", 16,
                           color, 0x1082, 3, 2);
        snprintf(year, sizeof(year), "%d", panel->slot_year[slot]);
        snprintf(date, sizeof(date), "%d/%d",
                 panel->slot_month[slot], panel->slot_day[slot]);
        draw_big5_text(screen, 165, top + 36, year, 16,
                       color, 0x1082, 3, 2);
        draw_big5_text(screen, 165, top + 57, date, 16,
                       color, 0x1082, 3, 2);
        (void)save_load_panel_draw_chunk(screen, assets,
            (size_t)(4 * panel->slot_stage[slot] +
                     panel->slot_map[slot] + 2), 209, top, 0);
        {
            int i;
            for (i = 0; i < panel->slot_player_counts[slot]; i++)
                (void)save_load_panel_draw_character(screen, assets,
                    panel->slot_characters[slot][i], 289 + 72 * i, top);
        }
    }
    if (panel->selected_row >= 0 && panel->selected_row < count) {
        int y = first_y + panel->selected_row * SAVE_LOAD_PANEL_ROW_HEIGHT;
        stroke_rect_thin(screen, 127, y, 452, 76, 0xffe0);
        stroke_rect_thin(screen, 128, y + 1, 450, 74, 0xffe0);
    }
}
