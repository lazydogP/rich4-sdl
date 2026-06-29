#ifndef RICH4_APP_AUDIO_EVENT_H
#define RICH4_APP_AUDIO_EVENT_H

#include <stddef.h>

#define RICH4_AUDIO_EVENT_CAPACITY 32

struct GamePlayerState;
struct GameSession;
struct MapData;

typedef enum Rich4MovementSoundMethod {
    RICH4_MOVEMENT_SOUND_WALK = 0,
    RICH4_MOVEMENT_SOUND_LOCOMOTIVE,
    RICH4_MOVEMENT_SOUND_CAR,
    RICH4_MOVEMENT_SOUND_ENGINEERING_VEHICLE,
    RICH4_MOVEMENT_SOUND_WATER,
    RICH4_MOVEMENT_SOUND_COUNT
} Rich4MovementSoundMethod;

typedef enum Rich4AudioEventType {
    RICH4_AUDIO_EVENT_NONE = 0,
    RICH4_AUDIO_EVENT_DICE,
    RICH4_AUDIO_EVENT_JUMP,
    RICH4_AUDIO_EVENT_DOOR,
    RICH4_AUDIO_EVENT_MOVEMENT,
    RICH4_AUDIO_EVENT_PROPERTY_PURCHASE,
    RICH4_AUDIO_EVENT_PROPERTY_UPGRADE,
    RICH4_AUDIO_EVENT_PROPERTY_TOLL,
    RICH4_AUDIO_EVENT_PROPERTY_MAX_LEVEL,
    RICH4_AUDIO_EVENT_OK,
    RICH4_AUDIO_EVENT_INVALID,
    RICH4_AUDIO_EVENT_TARGET_CONFIRM,
    RICH4_AUDIO_EVENT_CONFINEMENT_VISIT,
    RICH4_AUDIO_EVENT_ROULETTE,
    RICH4_AUDIO_EVENT_ROULETTE_STOP,
    RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT,
    RICH4_AUDIO_EVENT_POINT_MINIGAME_STOP_EFFECT,
    RICH4_AUDIO_EVENT_CANCEL,
    RICH4_AUDIO_EVENT_MAGIC_HOVER,
    RICH4_AUDIO_EVENT_MAGIC_TURN,
    RICH4_AUDIO_EVENT_SPEECH_START,
    RICH4_AUDIO_EVENT_SPEECH_STOP,
    RICH4_AUDIO_EVENT_TOOL_EFFECT,
    RICH4_AUDIO_EVENT_TOOL_USE,
    RICH4_AUDIO_EVENT_TOOL_PLACEMENT,
    RICH4_AUDIO_EVENT_TOOL_ROBOT_STOP,
    RICH4_AUDIO_EVENT_CARD,
    RICH4_AUDIO_EVENT_CARD_ANIMATION,
    RICH4_AUDIO_EVENT_SPECIAL_PLACE,
    RICH4_AUDIO_EVENT_AUCTION_EFFECT,
    RICH4_AUDIO_EVENT_BANK_KEY,
    RICH4_AUDIO_EVENT_BANK_SLIDER,
    /* 0x40EC14..0x40F2EB pass Effect.mkf resources 0x66..0x71 while
     * the matching Data.mkf god FLC is being shown. */
    RICH4_AUDIO_EVENT_GOD_EFFECT,
    RICH4_AUDIO_EVENT_ROAD_CHEST,
    RICH4_AUDIO_EVENT_ROAD_GIFT,
    RICH4_AUDIO_EVENT_GOD_DEPARTURE,
    RICH4_AUDIO_EVENT_MAGIC_FINALE,
    RICH4_AUDIO_EVENT_ATTACHED_BOMB,
    /* 0x419884: value is the route action (2..16), not a sound slot. */
    RICH4_AUDIO_EVENT_SPECIAL_PLACE_ENTRY,
    /* 0x41B1AD/0x41B32B: separate reward FLC sound, value=98/99. */
    RICH4_AUDIO_EVENT_SPECIAL_REWARD_ANIMATION,
    RICH4_AUDIO_EVENT_COUNT
} Rich4AudioEventType;

typedef struct Rich4AudioEvent {
    Rich4AudioEventType type;
    int active;
    int value;
} Rich4AudioEvent;

typedef struct Rich4AudioEventQueue {
    Rich4AudioEvent events[RICH4_AUDIO_EVENT_CAPACITY];
    size_t head;
    size_t count;
    size_t dropped;
    int borrow_depth;
} Rich4AudioEventQueue;

void rich4_audio_event_queue_init(Rich4AudioEventQueue *queue);
/* Discard pending events without changing the active borrow scope. */
void rich4_audio_event_queue_reset(Rich4AudioEventQueue *queue);
/* Borrow the session's single audio handoff queue for one interaction.  Only
   the outermost borrow clears it, so a nested interaction cannot discard
   events the enclosing one has not handed to its audio backend yet.  Returns
   NULL without a session; the emit helpers tolerate a NULL queue. */
Rich4AudioEventQueue *rich4_app_session_audio_borrow(struct GameSession *session);
/* Ends the borrow taken above.  Returns the queue when this call closes the
   outermost borrow and the caller must drain it, NULL when an enclosing
   borrow still owns the events. */
Rich4AudioEventQueue *rich4_app_session_audio_release(struct GameSession *session);
int rich4_audio_event_emit(Rich4AudioEventQueue *queue,
                           Rich4AudioEventType type,
                           int active,
                           int value);
int rich4_audio_event_pop(Rich4AudioEventQueue *queue,
                          Rich4AudioEvent *out_event);
int rich4_audio_movement_sound_method(
    const struct GamePlayerState *player,
    const struct MapData *map);
void rich4_app_collect_session_audio_events(
    struct GameSession *session, const struct MapData *movement_map,
    Rich4AudioEventQueue *audio_events);

#endif
