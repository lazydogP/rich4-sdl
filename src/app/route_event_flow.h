#ifndef RICH4_APP_ROUTE_EVENT_FLOW_H
#define RICH4_APP_ROUTE_EVENT_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "app/turn_rotation_flow.h"
#include "core/game_event.h"
#include "core/route_event.h"

typedef enum Rich4AppRoutePresentationKind {
    RICH4_ROUTE_PRESENTATION_NONE = 0,
    RICH4_ROUTE_PRESENTATION_SPECIAL_EVENT,
    RICH4_ROUTE_PRESENTATION_PROPERTY_LEVEL,
    RICH4_ROUTE_PRESENTATION_GOD_ANIMATION,
    RICH4_ROUTE_PRESENTATION_STATUS,
    RICH4_ROUTE_PRESENTATION_SPECIAL_PLACE,
    RICH4_ROUTE_PRESENTATION_LOTTERY_VISIT
} Rich4AppRoutePresentationKind;

/* The app classifies semantic route results before they cross into SDL.  The
 * event copy is immutable handoff data; SDL may only adapt its resources,
 * audio and geometry for the selected kind. */
typedef struct Rich4AppRoutePresentation {
    Rich4AppRoutePresentationKind kind;
    GameEvent event;
} Rich4AppRoutePresentation;

int rich4_app_route_event_presentation(
    const GameEvent *event, Rich4AppRoutePresentation *out_presentation);

/* Applies route-event results that are entirely app-owned.  A zero return
 * leaves resource-backed legacy presentation events for the SDL adapter. */
int rich4_app_route_event_apply(
    GameSession *session, const GameEvent *event,
    const RouteEvent *current_route_event, uint32_t now,
    int animation_enabled, Rich4AudioEventQueue *audio_events);

/* Runs one deterministic route-event step.  The app owns dispatch lifecycle
 * and semantic event application; presentation_events contains only events
 * that still require a platform/resource adapter. */
RouteEventExecutionResult rich4_app_route_event_step(
    GameSession *session, uint32_t now, int animation_enabled,
    GameEventQueue *presentation_events,
    Rich4AudioEventQueue *audio_events);

/* Completes the app-owned dispatch phase and, when needed, resumes movement
 * after a pass-event modal has finished.  Returns nonzero when movement was
 * resumed and the platform must stop its current frame. */
int rich4_app_route_event_finish(GameSession *session, uint32_t now);

typedef int (*Rich4AppRouteEventCallback)(void *context);
typedef void (*Rich4AppRouteStartCallback)(void *context);
typedef void (*Rich4AppRoutePresentationCallback)(
    void *context, const Rich4AppRoutePresentation *presentation);
typedef void (*Rich4AppRouteTurnRotationCallback)(
    void *context, const Rich4AppTurnRotationResult *result);
typedef void (*Rich4AppRouteAudioEventCallback)(
    void *context, const Rich4AudioEvent *event);

typedef struct Rich4AppRouteEventServices {
    void *property_context;
    void *presentation_context;
    void *actor_context;
    void *audio_context;
    uint32_t property_end_delay_ms;
    Rich4AppRouteEventCallback update_property_charge;
    Rich4AppRouteEventCallback update_property_max_level;
    Rich4AppRouteStartCallback start_property_max_level;
    Rich4AppRoutePresentationCallback present;
    Rich4AppRouteTurnRotationCallback begin_special_actor;
    Rich4AppRouteAudioEventCallback consume_audio_event;
} Rich4AppRouteEventServices;

/* Owns route-event frame priority and lifecycle. Platform supplies only
 * resource-backed presentation callbacks and the borrowed current map. */
int rich4_app_route_event_update(
    GameSession *session, const MapData *map, uint32_t now,
    int animation_enabled, const Rich4AppRouteEventServices *services);

#endif
