#include "debug/trace.h"

#include <stdio.h>
#include <string.h>

typedef struct Capture { Rich4TraceEvent events[16]; int count; } Capture;

static void capture(void *context, const Rich4TraceEvent *event)
{
    Capture *out = (Capture *)context;
    if (out->count < 16) out->events[out->count++] = *event;
}

int main(void)
{
    Capture out = {{{0, 0, 0, 0}}, 0};
    char line[64];
    rich4_trace_set_sink(capture, &out);
    rich4_trace_emit(RICH4_TRACE_MENU_NEW_GAME, 0, 0, 0);
    rich4_trace_emit(RICH4_TRACE_NEW_GAME_SUBMIT, 0, 2, 0);
    rich4_trace_emit(RICH4_TRACE_INTRO_BEGIN, 0, 0, 0);
    rich4_trace_emit(RICH4_TRACE_FIRST_LANDING, 0, 12, 0);
    rich4_trace_emit(RICH4_TRACE_FIRST_ACTION_DISPATCH, 0, 1, 0);
    if (out.count != 5 || !rich4_trace_format(line, sizeof(line), &out.events[4]) ||
        strcmp(line, "5,0,1,0") != 0) {
        fprintf(stderr, "structured trace mismatch\n");
        return 1;
    }
    puts("characterization trace: ok");
    return 0;
}
