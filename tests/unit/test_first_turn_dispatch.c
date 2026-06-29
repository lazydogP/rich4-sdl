#include <assert.h>
#include <stdio.h>

#include "core/first_turn_dispatch.h"

int main(void)
{
    FirstTurnDispatchPlan plan;
    int return_only_results[] = {3, 4, 6, 99};
    unsigned int i;

    plan = rich4_first_turn_dispatch_plan(0, 0);
    assert(plan.branch == GAME_FIRST_DISPATCH_BRANCH_RETURN_ONLY);
    assert(plan.return_only && !plan.show_dice_panel &&
           !plan.action_hint && !plan.input_refresh);

    plan = rich4_first_turn_dispatch_plan(1, 0);
    assert(plan.branch == GAME_FIRST_DISPATCH_BRANCH_INPUT_REFRESH);
    assert(plan.show_dice_panel && !plan.action_hint && plan.input_refresh);

    plan = rich4_first_turn_dispatch_plan(2, 0);
    assert(plan.branch == GAME_FIRST_DISPATCH_BRANCH_AUTOMATIC);
    assert(plan.automatic_preaction && plan.start_preroll && !plan.remark_dirty);
    plan = rich4_first_turn_dispatch_plan(5, 1);
    assert(plan.branch == GAME_FIRST_DISPATCH_BRANCH_AUTOMATIC);
    assert(plan.automatic_preaction && !plan.start_preroll && plan.remark_dirty);

    plan = rich4_first_turn_dispatch_plan(-1, 0);
    assert(plan.branch == GAME_FIRST_DISPATCH_BRANCH_AUTOMATIC);
    assert(plan.start_preroll && !plan.automatic_preaction &&
           !plan.show_dice_panel);

    for (i = 0; i < sizeof(return_only_results) / sizeof(return_only_results[0]); i++) {
        plan = rich4_first_turn_dispatch_plan(return_only_results[i], 0);
        assert(plan.branch == GAME_FIRST_DISPATCH_BRANCH_RETURN_ONLY);
        assert(plan.return_only && !plan.show_dice_panel &&
               !plan.automatic_preaction && !plan.start_preroll);
    }
    puts("first-turn SDL dispatch mapping: ok");
    return 0;
}
