#ifndef RICH4_APP_SPECIAL_PRESENTATION_FLOW_H
#define RICH4_APP_SPECIAL_PRESENTATION_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "app/special_event_timeline.h"
#include "core/game_command.h"
#include "core/game_event.h"
#include "core/route_event_executor.h"
#include "core/special_event.h"

/* App-owned single-shot presentation flows.  They mutate only the
 * app-owned presentation state and emit semantic audio events; resource
 * timing and SDL playback remain in the platform adapter. */
/* 0x41BB29: road object type 14; distinct from special-place action 13. */
int rich4_road_gift_begin(GameSession *session, int player, uint32_t now);
int rich4_road_reward_skip(GameSession *session, uint32_t now);
int rich4_road_chest_begin(GameSession *session, const MapData *map,
                          int player, uint32_t now);
void rich4_special_reward_begin(
    GameSession *session, const GameEvent *event, int frame_count,
    int delay_ms, uint32_t now, Rich4AudioEventQueue *audio_events);
int rich4_special_reward_update(
    GameSession *session, int frame_count, int delay_ms, uint32_t now,
    Rich4AudioEventQueue *audio_events);
void rich4_bank_rejection_begin(
    GameSession *session, const GameEvent *event, uint32_t now,
    Rich4AudioEventQueue *audio_events);
int rich4_bank_rejection_update(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events);
int rich4_company_issue_control_notice(GameSession *session, const GameEvent *event);
int rich4_company_issue_update(GameSession *session, uint32_t now);
int rich4_company_issue_skip_notice(GameSession *session, uint32_t now);
void rich4_company_issue_begin(GameSession *session, const GameEvent *event);
int rich4_company_issue_finish(GameSession *session, int quantity);

/* Start the stateful special-event page before SDL-specific audio/resource
 * work begins.  Confinement facts and phase semantics stay in the app; the
 * platform only performs resource/audio/actor side effects. */
int rich4_special_event_begin(
    GameSession *session, const GameEvent *event, int confinement,
    int first_confinement, uint32_t now);
void rich4_special_event_capture_commit(
    GameSession *session, const GameEvent *event);
RouteEventCommandResult rich4_special_event_submit_page_command(
    GameSession *session, const GameCommand *command, uint32_t now);
RouteEventCommandResult rich4_special_event_submit_page_command_with_area(
    GameSession *session, const GameCommand *command, uint32_t now,
    const GameNewsAreaAnchor *anchors, int count, int available, const MapData *map);
int rich4_special_event_prepare_news_area_view(
    GameSession *session, uint32_t now, int speech_playing);
int rich4_special_event_prepare_choices(GameSession *session, uint32_t now);
int rich4_special_event_choice_select(GameSession *session, int choice, uint32_t now);
/* Returns whether a choice modal owns input. A negative hover clears it. */
int rich4_special_event_choice_hover(GameSession *session, int choice);
int rich4_special_event_choice_press(GameSession *session, int choice);

typedef enum Rich4SpecialEventReactionAction {
    RICH4_SPECIAL_EVENT_REACTION_NONE = 0,
    RICH4_SPECIAL_EVENT_REACTION_SPEECH,
    RICH4_SPECIAL_EVENT_REACTION_CONFINEMENT_ANIMATION,
    RICH4_SPECIAL_EVENT_REACTION_FORTUNE_ANIMATION
} Rich4SpecialEventReactionAction;

typedef struct Rich4SpecialEventReactionResult {
    Rich4SpecialEventReactionAction action;
    int player;
    int center_original_player;
    int sync_player_to_node;
} Rich4SpecialEventReactionResult;

typedef struct Rich4SpecialEventUpdateInput {
    int page_frame_count;
    int page_delay_ms;
    int consequence_frame_count;
    int consequence_delay_ms;
    int fortune_frame_count;
    int fortune_delay_ms;
    int confinement_frame_count;
    int confinement_delay_ms;
    int speech_active;
    int audio_speech_playing;
    const GameNewsAreaAnchor *news_area_anchors;
    int news_area_count;
    int news_area_available;
    const MapData *news_area_map;
} Rich4SpecialEventUpdateInput;

typedef enum Rich4SpecialEventUpdateAction {
    RICH4_SPECIAL_EVENT_UPDATE_NONE = 0,
    RICH4_SPECIAL_EVENT_UPDATE_WAIT,
    RICH4_SPECIAL_EVENT_UPDATE_BEGIN,
    RICH4_SPECIAL_EVENT_UPDATE_REACTION,
    RICH4_SPECIAL_EVENT_UPDATE_CONSEQUENCE,
    RICH4_SPECIAL_EVENT_UPDATE_FORTUNE_ANIMATION,
    RICH4_SPECIAL_EVENT_UPDATE_CONFINEMENT_ANIMATION
} Rich4SpecialEventUpdateAction;

typedef struct Rich4SpecialEventUpdateResult {
    Rich4SpecialEventUpdateAction action;
    Rich4SpecialEventReactionResult reaction;
    int sync_player_to_node;
    int sync_player;
} Rich4SpecialEventUpdateResult;

int rich4_special_event_confinement_kind(int family, int id);
int rich4_special_event_confinement_days(
    const GameSession *session, int player, int confinement);
int rich4_special_event_consequence_index(int logical_id);
int rich4_special_event_begin_from_event(
    GameSession *session, const GameEvent *event, uint32_t now);
int rich4_special_event_start_confinement_final_speech(
    GameSession *session, int player, uint32_t now);
int rich4_special_event_prepare_reaction(
    GameSession *session, uint32_t now,
    Rich4SpecialEventReactionResult *result);
int rich4_special_event_update(
    GameSession *session, const Rich4SpecialEventUpdateInput *input,
    uint32_t now, Rich4SpecialEventUpdateResult *result);
const GameState *rich4_special_event_render_state(
    const GameSession *session);

typedef enum Rich4CompanyIssueInput {
    RICH4_COMPANY_ISSUE_DIGIT = 1,
    RICH4_COMPANY_ISSUE_BACKSPACE,
    RICH4_COMPANY_ISSUE_CLEAR,
    RICH4_COMPANY_ISSUE_MAX,
    RICH4_COMPANY_ISSUE_DECREMENT,
    RICH4_COMPANY_ISSUE_INCREMENT
} Rich4CompanyIssueInput;

int rich4_company_issue_input(
    GameSession *session, Rich4CompanyIssueInput input, int digit,
    Rich4AudioEventQueue *audio_events);
int rich4_company_issue_confirm(
    GameSession *session, int accept, Rich4AudioEventQueue *audio_events);

void rich4_special_event_skip_wait(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events);

#endif
