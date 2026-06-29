#ifndef RICH4_CORE_GAME_EVENT_INTERNAL_H
#define RICH4_CORE_GAME_EVENT_INTERNAL_H

#include "core/game_event.h"

/* Generic event construction is an implementation detail of core's typed
 * event boundary. Callers outside core must use named constructors. */
GameEvent rich4_game_event_internal(GameEventType type,
                                    int player,
                                    int subject,
                                    int value,
                                    int value2,
                                    unsigned int flags);
GameEvent rich4_game_event_fields_internal(GameEventType type,
                                           Rich4GameEventFields fields);
GameEvent rich4_game_domain_event_internal(GameEventType type,
                                           int player,
                                           int subject_value,
                                           GameDomainRef subject_ref,
                                           GameDomainRef target,
                                           int value,
                                           int value2,
                                           unsigned int flags);

#endif
