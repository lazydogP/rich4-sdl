#ifndef RICH4_CORE_FIRST_TURN_DISPATCH_H
#define RICH4_CORE_FIRST_TURN_DISPATCH_H

#include "core/game_constants.h"

typedef struct FirstTurnDispatchPlan {
    int result;
    int branch;
    int show_dice_panel;
    int action_hint;
    int input_refresh;
    int automatic_preaction;
    int remark_dirty;
    int start_preroll;
    int return_only;
} FirstTurnDispatchPlan;

/* Maps the confirmed 0x40C912(0) result families onto portable SDL flow.
 * Sale/card/tool decisions remain outside M2 and are represented only by the
 * automatic-preaction branch. */
FirstTurnDispatchPlan rich4_first_turn_dispatch_plan(int result,
                                                     int automatic_status_blocked);

#endif
