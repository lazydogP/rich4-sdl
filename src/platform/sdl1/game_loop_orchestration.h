#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_ORCHESTRATION_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_ORCHESTRATION_H

#include "app/config_types.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/app_command.h"
#include "core/game_command.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "platform/sdl1/render/game_render_cache.h"


/* R10.1d: first slice of the game-session orchestration cluster moved
 * verbatim out of runtime_main.c. init_game_session (and its private helper
 * tree: map property/stock-table setup, intro-AVI staging, initial player
 * placement) is self-contained and has exactly one caller in
 * rich4_sdl1_main's new-game-transition handling, so it moves ahead of the
 * larger update_game_session adapter in game_loop_update.c. */

void init_game_session(GameSession *session,
                       const GamePreviewAssets *assets,
                       const NewGameSettings *settings,
                       const Rich4Config *runtime_config,
                       GameRenderCache *render_cache,
                       int route_count);
/* R10.1d: top-level session-update adapter. App-owned stage selection keeps
 * the original call order; this entrypoint supplies platform resources and
 * consumes the resulting app flow requests. */

void update_game_session(GameSession *session,
                         const GamePreviewAssets *assets,
                         const NewGameSettings *settings,
                         const Rich4Config *config,
                         Rich4Audio *audio,
                         Uint32 now,
                          int cursor_x);
int game_update_presentation_dispatch(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
     Rich4Audio *audio, Uint32 now, int cursor_x);
/* R10.1d: Help.mkf help-panel resource, paging, input and draw cluster,
 * moved verbatim out of runtime_main.c. The event loop retains only the
 * toolbar/release dispatch and calls these lifecycle/interaction entrypoints.
 */

int game_load_help_assets(ResourceStore *store, const char *data_dir,
                           HelpAssets *assets);
int game_open_help_panel(HelpPanelState *panel,
                          const HelpAssets *assets,
                          SDL_Surface *capture_source);
void game_close_help_panel(HelpPanelState *panel);

void game_help_panel_mouse_down(HelpPanelState *panel,
                                const HelpAssets *assets,
                                Rich4Audio *audio,
                                int x, int y);

void game_help_panel_mouse_up(HelpPanelState *panel,
                              const HelpAssets *assets,
                              int x, int y);

void game_help_panel_key_down(HelpPanelState *panel,
                              const HelpAssets *assets,
                              const Rich4Config *config,
                              Rich4Audio *audio,
                              Rich4AppKey key, int key_modifiers);

void game_draw_help_panel(SDL_Surface *screen,
                          const HelpAssets *assets,
                          const HelpPanelState *panel);

/* R10.1d: AI settings panel state/edit/draw cluster, moved verbatim out of
 * runtime_main.c. The event loop retains only modal input dispatch and
 * forwards these panel entrypoints. */

void game_draw_ai_settings_panel(SDL_Surface *screen,
                                 const GamePreviewAssets *assets,
                                 const GameSession *session,
                                 const Rich4AiSettingsState *panel);

void game_draw_player_info_panel(SDL_Surface *screen,
                                 const GamePreviewAssets *assets,
                                 const GameSession *session);

#endif
