#include <stdio.h>
#include <stdlib.h>

#include "core/route_event_executor_internal.h"

/* S0 gate regression: route executor fields must not rewrite unrelated names
 * through a preprocessor alias.  If execution_started were still a macro,
 * both declarations below would fail to compile. */
static int route_executor_name_binding_regression(void)
{
    int execution_started = 7;
    struct {
        int execution_started;
    } unrelated = {11};
    return execution_started == 7 && unrelated.execution_started == 11;
}

int main(void)
{
    int status;
    if (!route_executor_name_binding_regression()) {
        fprintf(stderr, "route executor name binding regression\n");
        return 1;
    }
    status = system("python tests/architecture/check_context_simplification.py");
    if (status != 0) return 1;
    puts("context simplification gates: ok");
    return 0;
}
