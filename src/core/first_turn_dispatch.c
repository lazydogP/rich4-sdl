#include "core/first_turn_dispatch.h"

#include <string.h>

FirstTurnDispatchPlan rich4_first_turn_dispatch_plan(int result,
                                                     int automatic_status_blocked)
{
    FirstTurnDispatchPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.result = result;
    plan.branch = GAME_FIRST_DISPATCH_BRANCH_RETURN_ONLY;
    if (result == 0) {
        /*
         * 0x418D88 calls 0x418E7F, whose second 0x40C912(1) check
         * suppresses route/dice dispatch for a blocking status.
         */
        plan.return_only = 1;
    } else if (result == 1) {
        plan.branch = GAME_FIRST_DISPATCH_BRANCH_INPUT_REFRESH;
        plan.show_dice_panel = 1;
        plan.input_refresh = 1;
    } else if (result == -1) {
        /* 0x40C912 has already selected the automatic sleepwalking path. */
        plan.branch = GAME_FIRST_DISPATCH_BRANCH_AUTOMATIC;
        plan.start_preroll = 1;
    } else if (result == 2 || result == 5) {
        plan.branch = GAME_FIRST_DISPATCH_BRANCH_AUTOMATIC;
        plan.automatic_preaction = 1;
        plan.remark_dirty = automatic_status_blocked != 0;
        plan.start_preroll = !plan.remark_dirty;
    } else {
        plan.return_only = 1;
    }
    return plan;
}
