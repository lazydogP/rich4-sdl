#ifndef RICH4_PLATFORM_SDL1_RENDER_SCENE_OPTIONS_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_SCENE_OPTIONS_RENDER_H

#include <SDL/SDL.h>

#include "app/options_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_config_types.h"

/* R10.1b: options panel scene, moved verbatim out of runtime_main.c. Renders
 * and drives the extended in-game/menu options overlay (game speed, music,
 * sound effect, animation, auto-save, view mode, music bank/track). Pure
 * presentation + panel state; no core domain mutation. */

void open_options_panel(Rich4OptionsPanelState *panel,
                        const GamePreviewAssets *assets,
                        const Rich4Config *config,
                        const Rich4SdlConfig *sdl_config,
                        Rich4SceneId return_app_state);

void cancel_options_panel(Rich4OptionsPanelState *panel,
                          Rich4Audio *audio,
                          const char *music_dir);

void draw_options_panel(SDL_Surface *screen,
                        const GamePreviewAssets *assets,
                        const Rich4OptionsPanelState *panel);

#endif
