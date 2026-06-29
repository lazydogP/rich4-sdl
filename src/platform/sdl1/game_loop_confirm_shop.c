/* R10.1d: shop confirmation/click dispatch moved verbatim out of
 * runtime_main.c. */

#include "platform/sdl1/game_loop_confirm_shop.h"
#include "app/shop_flow.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_loop_panels.h"

int game_handle_confirm_shop(
    Rich4SceneId app_state, GameSession *session,
    VirtualCursor *cursor, Rich4Audio *audio)
{
    Uint32 shop_now;
    int hit;
    if (app_state != RICH4_SCENE_GAME ||
        !session->presentation.shop_visit.active)
        return 0;
    shop_now = rich4_sdl1_ticks();
    if (!game_shop_skip_message(session, audio, shop_now)) {
        hit = rich4_shop_hit(session, cursor->x, cursor->y);
        if (hit == RICH4_SHOP_HIT_TAB)
            game_shop_switch_page(session, audio, shop_now);
        else if (hit == RICH4_SHOP_HIT_EXIT)
            game_shop_begin_exit(session, audio, shop_now);
        else
            game_shop_apply_transaction(session, audio, hit, shop_now);
    }
    cursor->confirm_pressed = 0;
    return 1;
}
