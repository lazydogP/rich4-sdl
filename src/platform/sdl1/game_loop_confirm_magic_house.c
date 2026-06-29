/* R10.1d: magic-house confirm modal dispatch moved verbatim out of
 * runtime_main.c. */

#include "platform/sdl1/game_loop_confirm_magic_house.h"
#include "app/magic_house_flow.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_turns.h"

int game_handle_confirm_magic_house(
    GameConfirmMagicHouseContext *context)
{
    if (context->app_state != RICH4_SCENE_GAME ||
        !context->session->presentation.magic_house.active)
        return 0;
    if (context->session->presentation.magic_house.condition == RICH4_MAGIC_HOUSE_SURRENDER) {
        /* 0x433088 accepts selection only on physical button-down. */
        context->cursor->confirm_pressed = 0;
        return 1;
    }
    if (context->session->presentation.magic_house.phase ==
            RICH4_MAGIC_HOUSE_FACILITY_CHOICE) {
        int choice = rich4_magic_house_facility_at(
            context->cursor->x, context->cursor->y);
        if (!context->cursor->confirm_pointer_pressed &&
            context->session->presentation.magic_house.facility_choice_hover >= 0)
            choice = context->session->presentation.magic_house.facility_choice_hover;
        (void)game_magic_house_choose_facility(
            context->session, context->audio, choice, rich4_sdl1_ticks());
    } else if (context->session->presentation.magic_house.phase <=
               RICH4_MAGIC_HOUSE_INTRO_3) {
        game_magic_house_skip_intro(
            context->session, context->audio, rich4_sdl1_ticks());
    } else if (context->session->presentation.magic_house.phase ==
               RICH4_MAGIC_HOUSE_SELECT) {
        int mask = game_magic_house_mask_at(
            context->assets, context->cursor->x, context->cursor->y);
        if (mask >= 1 && mask <= 12)
            game_magic_house_select_action(
                context->session, context->audio, mask - 1,
                rich4_sdl1_ticks());
        else if (mask == 13) {
            rich4_magic_house_set_hover(context->session, -1);
            rich4_magic_house_request_return_to_selection(context->session);
            game_magic_house_start_phase(
                context->session, context->audio,
                RICH4_MAGIC_HOUSE_CONDITION, rich4_sdl1_ticks());
        }
    }
    context->cursor->confirm_pressed = 0;
    return 1;
}
