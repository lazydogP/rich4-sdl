/* R10.1d: point-minigame confirm dispatch moved verbatim out of
 * runtime_main.c. */

#include "platform/sdl1/game_loop_confirm_point_minigame.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_loop_panels.h"

int game_handle_confirm_point_minigame(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, VirtualCursor *cursor,
    Rich4Audio *audio)
{
    if (app_state != RICH4_SCENE_GAME ||
        !session->presentation.point_minigame.active)
        return 0;
    game_point_minigame_click(
        session, assets, audio, cursor->x, cursor->y,
        rich4_sdl1_ticks());
    cursor->confirm_pressed = 0;
    return 1;
}
