#include "debug/trace.h"

#include <stdio.h>

static Rich4TraceSink rich4_trace_sink;
static void *rich4_trace_context;

void rich4_trace_set_sink(Rich4TraceSink sink, void *context)
{
    rich4_trace_sink = sink;
    rich4_trace_context = context;
}

void rich4_trace_emit(Rich4TraceEventKind kind, int actor, int value, int detail)
{
    Rich4TraceEvent event;
    if (rich4_trace_sink == NULL) {
        return;
    }
    event.kind = kind;
    event.actor = actor;
    event.value = value;
    event.detail = detail;
    rich4_trace_sink(rich4_trace_context, &event);
}

int rich4_trace_format(char *buffer, unsigned int size, const Rich4TraceEvent *event)
{
    if (buffer == NULL || size == 0 || event == NULL) {
        return 0;
    }
    return snprintf(buffer, size, "%d,%d,%d,%d",
                    (int)event->kind, event->actor, event->value, event->detail) > 0;
}
