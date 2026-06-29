#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_PANELS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_PANELS_H

#include "app/card_panel_state.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/app_command.h"
#include "core/game_domain.h"
#include "core/game_command.h"
#include "core/game_event.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: stock-market panel input/render cluster, moved verbatim out of
 * runtime_main.c. The event loop owns only the panel value and forwards
 * toolbar, pointer, keyboard and final-draw calls through these entrypoints.
 */

void game_open_stock_panel(StockPanelState *panel, GameSession *session);

void game_close_stock_panel(StockPanelState *panel, GameSession *session);

void game_destroy_stock_panel(StockPanelState *panel);

void game_stock_panel_cancel(StockPanelState *panel, GameSession *session);

void game_stock_panel_mouse_down(
    StockPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, int x, int y);

void game_stock_panel_mouse_motion(
    StockPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, Rich4Audio *audio, int x, int y);

void game_stock_panel_mouse_release(
    StockPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    Rich4Audio *audio, int button, int x, int y);

void game_stock_panel_key_down(
    StockPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    Rich4Audio *audio, Rich4AppKey key);

void game_draw_stock_panel(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GameState *state,
    const GameStockInteractionPresentationState *interaction,
    StockPanelState *panel);

void game_sale_panel_mouse_release(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio,
    int button, int x, int y);
void game_sale_panel_mouse_down(
    GameSession *session, const GamePreviewAssets *assets, int x, int y);
void game_sale_panel_mouse_motion(
    GameSession *session, const GamePreviewAssets *assets,
    int x, int y, int mouse_left_down);

void game_sale_panel_key_down(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio, Rich4AppKey key);

void game_draw_sale_panel(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session,
    const GameSaleInteractionPresentationState *interaction);

int game_save_load_panel_open(
    GameSession *session, const GamePreviewAssets *assets,
    GameSaveLoadPanelMode mode);
int game_save_load_panel_open_menu(GameSession *session,
    const GamePreviewAssets *assets);
int game_save_load_panel_menu_activate(GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings, int x, int y);
int game_save_load_panel_open_after_ending(
    GameSession *session, const GamePreviewAssets *assets);
void game_save_load_panel_cancel(GameSession *session, Rich4Audio *audio);
void game_save_load_panel_mouse_down(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, Rich4Audio *audio, int x, int y);
void game_save_load_panel_mouse_motion(GameSession *session, int x, int y);
int game_save_load_panel_mouse_release(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, Rich4Audio *audio,
    int button, int x, int y);
int game_save_load_panel_key_down(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, Rich4Audio *audio,
    Rich4AppKey key);
int game_save_load_panel_confirm(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, Rich4Audio *audio);
void game_draw_save_load_panel(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSession *session);

/* R10.1d: card inventory/targeting/effect panel cluster, moved verbatim out
 * of runtime_main.c. The event loop keeps only modal ordering and forwards
 * hit-testing, command confirmation, effect resumption and drawing here. */

void game_update_card_panel_timed_effect(
    CardPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    Rich4Audio *audio);

void game_card_panel_confirm(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, CardPanelState *panel,
    int x, int y, Uint32 now, Rich4Audio *audio);

void game_card_panel_resume_pre_effect(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, CardPanelState *panel,
    Uint32 now, Rich4Audio *audio);

void game_draw_card_panel(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          const GameSession *session,
                          const CardPanelState *panel);

void game_draw_inventory_text(SDL_Surface *screen, int x, int y,
                              const char *big5, int anchor);

/* R10.1d: tool inventory/target-selection panel cluster, moved verbatim out
 * of runtime_main.c. These seams cover the same picker hit tests, edge-scroll
 * update, command confirmation and draw pass used by the original loop. */

void game_tool_panel_finish_speech(
    ToolPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    VirtualCursor *cursor, int *host_pointer_sync_needed,
    Rich4Audio *audio);

int game_tool_panel_finish_inventory_release(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    ToolPanelState *panel, VirtualCursor *cursor, Rich4Audio *audio,
    int button);

void game_tool_panel_dispatch_pending_inventory_release(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, ToolPanelState *panel,
    Rich4Audio *audio);

size_t game_tool_panel_cursor_chunk_index(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, const ToolPanelState *panel,
    int cursor_x, int cursor_y, size_t default_chunk);

int game_tool_panel_route_target(const GamePreviewAssets *assets,
                                 const NewGameSettings *settings,
                                 const GameSession *session,
                                 int x, int y, int require_placeable);

GameDomainRef game_tool_panel_property_target(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, int x, int y);

GameDomainRef game_tool_panel_any_target(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, int x, int y);

int game_tool_panel_press_inventory(GameSession *session,
                                    ToolPanelState *panel,
                                    int x, int y, Rich4Audio *audio);

void game_tool_panel_activate_selected(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, ToolPanelState *panel,
    Uint32 now, Rich4Audio *audio);

void game_tool_panel_confirm(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, ToolPanelState *panel,
    int x, int y, Uint32 now, Rich4Audio *audio);

void game_draw_tool_panel(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          const GameSession *session,
                          const ToolPanelState *panel);

/* R10.1d: confinement (prison/hospital) visit sub-cluster of
 * update_game_session, moved verbatim. game_begin_confinement_visit and
 * game_update_confinement_visit are called from update_game_session (in
 * game_loop_update.c); the rest are called from rich4_sdl1_main's confirm-
 * dialog / confinement click and speech-skip dispatch. */

void game_begin_confinement_visit(GameSession *session, const GameEvent *event,
                                  Rich4Audio *audio, Uint32 now);

int game_update_confinement_visit(GameSession *session, Rich4Audio *audio, Uint32 now);

void game_confinement_visit_start_hospital_exit(GameSession *session,
                                                Rich4Audio *audio, Uint32 now);

void game_confinement_visit_skip_speech(GameSession *session, Rich4Audio *audio, Uint32 now);

void game_confinement_visit_start_speech(GameSession *session, Rich4Audio *audio,
                                         const Rich4CharacterSpeech *speech,
                                         int special_actor, Uint32 now);

/* R10.1d: lottery-counter visit sub-cluster of update_game_session, moved
 * verbatim. game_begin_lottery_visit and game_update_lottery_visit are
 * called from update_game_session (in game_loop_update.c); the rest are
 * called from rich4_sdl1_main's lottery-panel click/cancel/hover dispatch. */

void game_begin_lottery_visit(GameSession *session, const GamePreviewAssets *assets,
                              const GameEvent *event,
                              Rich4Audio *audio, Uint32 now, int animation_enabled);

int game_update_lottery_visit(GameSession *session, const GamePreviewAssets *assets,
                              Rich4Audio *audio, Uint32 now);

void game_cancel_lottery_visit(GameSession *session, Rich4Audio *audio, Uint32 now);

void game_lottery_right_button_up(GameSession *session, Rich4Audio *audio,
                                   Uint32 now, VirtualCursor *cursor);

void game_lottery_left_button_down(GameSession *session, Rich4Audio *audio,
                                   int pointer_x, int pointer_y, Uint32 now);

/* R10.1d: company/facility roulette sub-cluster of update_game_session,
 * moved verbatim. game_begin_facility_roulette and
 * game_update_facility_roulette are called from update_game_session (in
 * game_loop_update.c); game_stop_facility_roulette is also called from
 * rich4_sdl1_main's roulette-stop click dispatch. */

void game_begin_facility_roulette(GameSession *session, const GameEvent *event,
                                  Rich4Audio *audio, Uint32 now);

void game_stop_facility_roulette(GameSession *session, Rich4Audio *audio, Uint32 now);

int game_update_facility_roulette(GameSession *session, Rich4Audio *audio, Uint32 now);

/* R10.1d: point-minigame (penguin/balloon/catcher) sub-cluster of
 * update_game_session, moved verbatim. game_begin_point_minigame and
 * game_update_point_minigame are called from update_game_session (in
 * game_loop_update.c); game_point_minigame_click is also called from
 * rich4_sdl1_main's minigame click dispatch. */

void game_begin_point_minigame(GameSession *session, const GameEvent *event,
                               Rich4Audio *audio, Uint32 now, int animation_enabled);

int game_update_point_minigame(GameSession *session, const GamePreviewAssets *assets,
                               Rich4Audio *audio, Uint32 now, int cursor_x);

int game_point_minigame_click(GameSession *session, const GamePreviewAssets *assets,
                              Rich4Audio *audio, int x, int y, Uint32 now);

/* R10.1d: general-store visit sub-cluster of update_game_session, moved
 * verbatim. game_update_shop_visit and game_begin_shop_visit are called
 * from update_game_session (in game_loop_update.c); the rest are called
 * from rich4_sdl1_main's shop-panel click/skip dispatch. game_shop_message_
 * big5 moved with the cluster too but keeps its shared game_flow_api.h
 * declaration (used by the shop-visit renderer). */

void game_begin_shop_visit(GameSession *session, const GameEvent *event,
                          Rich4Audio *audio, Uint32 now, int animation_enabled);

int game_update_shop_visit(GameSession *session, Rich4Audio *audio, Uint32 now);

int game_shop_skip_message(GameSession *session, Rich4Audio *audio, Uint32 now);

void game_shop_mouse_button_up(
    GameSession *session, Rich4Audio *audio, int released_control,
    int x, int y, VirtualCursor *cursor);

void game_shop_right_button_up(
    GameSession *session, Rich4Audio *audio,
    int *shop_music_restore_pending);

int game_shop_apply_transaction(GameSession *session, Rich4Audio *audio,
                                int hit, Uint32 now);

void game_shop_switch_page(GameSession *session, Rich4Audio *audio, Uint32 now);

void game_shop_begin_exit(GameSession *session, Rich4Audio *audio, Uint32 now);

#endif
