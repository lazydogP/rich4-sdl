#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_TURNS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_TURNS_H

#include <SDL/SDL.h>

#include "app/config_types.h"
#include "app/game_session.h"
#include "app/turn_start_flow.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/app_command.h"
#include "core/game_command.h"
#include "core/game_event.h"
#include "core/game_state.h"
#include "core/turn_rotation.h"
#include "data/map_data.h"
#include "platform/sdl1/app_asset_types.h"

/* R10.1d: dice status-panel input sub-cluster, moved verbatim. The panel
 * state helpers are called by the session update/route-event flow, while
 * the mouse/drag helpers are called by rich4_sdl1_main's SDL dispatch. */

int handle_game_dice_status_mouse_down(GameSession *session,
                                       const GamePreviewAssets *assets,
                                       int mouse_x, int mouse_y, Uint32 now);

int game_sync_dice_action_cursor(
    GameSession *session, VirtualCursor *cursor);

int game_handle_dice_shortcut_key_down(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    Rich4Audio *audio, Rich4AppKey key);

/* R10.1d: tool-effect request/voice deferral sub-cluster, moved verbatim.
 * These entrypoints are used by route-event dispatch and the remaining
 * session-update flow. */

void game_tool_animation_enqueue_from_platform(
    GameSession *session, const GamePreviewAssets *assets,
    const GameEvent *event, Uint32 now);

/* R10.1d: tool-animation queue-consumption state machine, moved verbatim
 * out of update_game_session. Returns nonzero when the caller must return
 * from the session update for this tick. */

int game_update_tool_animation_queue(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio, Uint32 now, int animation_enabled);

/* R10.1d: tool-placement completion and tool-robot follow-up state machines,
 * moved verbatim out of update_game_session. */

int game_tool_followup_update_placement(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio, Uint32 now);

int game_tool_followup_update_robot(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now);

/* R10.1d: tool-trigger turn finalization, moved verbatim out of
 * update_game_session and kept with the tool follow-up state machines. */

int game_tool_followup_finish_trigger_turn(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Uint32 now);

/* R10.1d: turn movement preparation, interpolation and route-event commit
 * state machine, moved verbatim out of update_game_session. */

void game_update_turn_movement(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now);

/* R10.1d: special-actor app-flow adapter. The path/action state machine lives
 * in app/special_actor_flow.c; this seam supplies SDL-selected map/config
 * inputs and hands completion back to turn rotation. */

int game_update_special_actor_flow(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config, Uint32 now);

/* R10.1d: first-turn landing animation and first-action dispatch state
 * machine, moved verbatim out of update_game_session. */

int game_update_first_landing(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Uint32 now);

/* R10.1d: dynamic map-object ViewModel refresh, moved verbatim out of
 * update_game_session. */

void game_limit_god_detachment(GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings);
int game_update_god_detachment(GameSession *session, Rich4Audio *audio, Uint32 now);
void game_update_dynamic_objects(GameSession *session,
                                 const GamePreviewAssets *assets,
                                 const NewGameSettings *settings,
                                 Uint32 now);

/* R10.1d: route-event executor/update state machine, moved verbatim out of
 * update_game_session. */

int game_update_route_events(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now);

/* R10.1d: turn rotation, status countdown and confinement-release state
 * machines, moved verbatim out of update_game_session. */

int game_update_confinement_exit(
    GameSession *session, const Rich4Config *config, Uint32 now);

int game_update_confinement_status_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Uint32 now);

/* R10.1d: AI preaction command-selection loop, moved verbatim out of
 * update_game_session. Command application remains in game_loop_command.c. */

int game_update_ai_tool(GameSession *session, const GamePreviewAssets *assets,
                         const NewGameSettings *settings, Rich4Audio *audio,
                         Uint32 now);
void game_run_ai_preactions(GameSession *session,
                            const GamePreviewAssets *assets,
                            const NewGameSettings *settings,
                            int player, Uint32 now);

/* R10.1d: session-intro animation advancement and completion state machine,
 * moved verbatim out of update_game_session. */

void game_update_intro_animation(GameSession *session,
                                 const GamePreviewAssets *assets,
                                 const NewGameSettings *settings,
                                 Uint32 now);

/* R10.1d: consume the app-owned turn-start request and apply only SDL
 * presentation/AI/resource side effects. */

void game_apply_turn_start_request(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Uint32 now,
    const Rich4TurnStartRequest *request);

/* R10.1d: magic-house presentation/action sub-cluster of update_game_session,
 * moved verbatim. The begin/update functions are called from the session
 * update/route-event dispatcher; the remaining helpers are called from the
 * SDL magic-house modal input dispatch. */

void game_begin_magic_house(GameSession *session, const GameEvent *event,
                            Rich4Audio *audio, Uint32 now,
                            int animation_enabled);

int game_update_magic_house(GameSession *session, Rich4Audio *audio,
                            const GamePreviewAssets *assets,
                            const NewGameSettings *settings, Uint32 now);

int game_magic_house_mask_at(const GamePreviewAssets *assets, int x, int y);

int game_magic_house_choose_facility(
    GameSession *session, Rich4Audio *audio, int choice, Uint32 now);

void game_magic_house_start_phase(GameSession *session, Rich4Audio *audio,
                                  int phase, Uint32 now);

void game_magic_house_skip_intro(GameSession *session, Rich4Audio *audio,
                                 Uint32 now);

void game_magic_house_select_action(GameSession *session, Rich4Audio *audio,
                                    int action, Uint32 now);

void game_finish_magic_house(GameSession *session, Rich4Audio *audio,
                             int action, Uint32 now);

/* R10.1d: special-reward, bank-rejection and company-issue single-shot
 * presentation sub-clusters of update_game_session, moved verbatim. The
 * begin_ functions are called from game_session_apply_command's route-
 * event-resolved dispatch (in game_loop_command.c); the update_/finish_
 * functions are called from update_game_session (special reward/bank
 * rejection) or rich4_sdl1_main's confirm-dialog dispatch (company issue). */

void game_begin_special_reward_presentation(GameSession *session,
                                            const GamePreviewAssets *assets,
                                            const GameEvent *event,
                                            Rich4Audio *audio, Uint32 now);

int game_update_special_reward_presentation(GameSession *session,
                                            const GamePreviewAssets *assets,
                                            Rich4Audio *audio, Uint32 now);

void game_begin_bank_rejection_presentation(GameSession *session,
                                            const GameEvent *event,
                                            Rich4Audio *audio, Uint32 now);

int game_update_bank_rejection_presentation(
    GameSession *session, Rich4Audio *audio, Uint32 now);

void game_finish_company_issue(GameSession *session, int quantity);

/* R10.1d: road-god acquisition animation/text.  The core emits one typed
 * event for both landing pickup and 请神符, so the adapter has one entrypoint
 * and the same presentation ordering for both paths. */
void game_begin_god_presentation(GameSession *session,
                                 const GamePreviewAssets *assets,
                                 const GameEvent *event, Uint32 now);
int game_update_god_presentation(GameSession *session,
                                 const GamePreviewAssets *assets,
                                 Rich4Audio *audio, Uint32 now, int animation_enabled);

/* R10.1d: news/fortune special-event presentation state machine, moved
 * verbatim out of update_game_session. The map-render snapshot helper is
 * called by draw_game_preview_adapter; the skip helper is called by the SDL
 * event loop. */

void game_begin_special_event_presentation(GameSession *session,
                                           const GamePreviewAssets *assets,
                                           const GameEvent *event,
                                           Rich4Audio *audio, Uint32 now);

int game_update_special_event_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now);

/* R10.1d: bankruptcy cleanup animation.  Ledger state is already committed;
 * this adapter supplies Data.MKF[555] timing and lets the app release the
 * queued auctions only after the original blocking presentation completes. */
int game_update_bankruptcy_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    Rich4Audio *audio, Uint32 now);

void game_skip_special_event_wait(GameSession *session,
                                  Rich4Audio *audio, Uint32 now);


/* R10.1d: property level/max-level/toll-charge presentation sub-cluster of
 * update_game_session, moved verbatim. The begin_ functions are called
 * from game_session_apply_command's route-event-resolved dispatch; the
 * update_ functions are called from update_game_session (in
 * game_loop_update.c). */

int game_property_start_max_level_animation(
    GameSession *session, const GamePreviewAssets *assets, Uint32 now);
int game_property_update_max_level_animation(
    GameSession *session, const GamePreviewAssets *assets, Uint32 now);
void game_property_begin_level_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    const GameEvent *event, Uint32 now);
int game_property_update_charge_presentation(
    GameSession *session, Rich4Audio *audio, Uint32 now);

#endif
