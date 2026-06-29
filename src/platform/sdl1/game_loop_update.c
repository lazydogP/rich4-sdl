#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_update_adapter.h"

/* The app owns frame priority and turn-stage selection.  This SDL entrypoint
 * only forwards platform services and resource-backed inputs to that flow. */
void update_game_session(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now, int cursor_x)
{
    rich4_sdl_update_game_session_adapter(
        session, assets, settings, config, audio, now, cursor_x);
}
