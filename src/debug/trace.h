#ifndef RICH4_DEBUG_TRACE_H
#define RICH4_DEBUG_TRACE_H

typedef enum Rich4TraceEventKind {
    RICH4_TRACE_MENU_NEW_GAME = 1,
    RICH4_TRACE_NEW_GAME_SUBMIT = 2,
    RICH4_TRACE_INTRO_BEGIN = 3,
    RICH4_TRACE_FIRST_LANDING = 4,
    RICH4_TRACE_FIRST_ACTION_DISPATCH = 5,
    RICH4_TRACE_DICE_ROLL = 6,
    RICH4_TRACE_MOVE_BEGIN = 7,
    RICH4_TRACE_MOVE_END = 8,
    RICH4_TRACE_AI_AUTO_MOVE = 9
} Rich4TraceEventKind;

typedef struct Rich4TraceEvent {
    Rich4TraceEventKind kind;
    int actor;
    int value;
    int detail;
} Rich4TraceEvent;

typedef void (*Rich4TraceSink)(void *context, const Rich4TraceEvent *event);

void rich4_trace_set_sink(Rich4TraceSink sink, void *context);
void rich4_trace_emit(Rich4TraceEventKind kind, int actor, int value, int detail);
int rich4_trace_format(char *buffer, unsigned int size, const Rich4TraceEvent *event);

#endif
