#ifndef RICH4_VIEW_GAME_VIEW_MODEL_H
#define RICH4_VIEW_GAME_VIEW_MODEL_H

#include "core/game_state.h"
#include "data/map_data.h"
#include "data/resource_constants.h"
#include "view/view_constants.h"

typedef enum ViewEvidenceConfidence {
    VIEW_EVIDENCE_UNKNOWN = 0,
    VIEW_EVIDENCE_PARTIAL,
    VIEW_EVIDENCE_CONFIRMED
} ViewEvidenceConfidence;

typedef struct GameViewportModel {
    int center_world_x;
    int center_world_y;
    int direction;
    int has_override;
} GameViewportModel;

typedef struct GameActorView {
    int player;
    int character;
    /* Original player +63 (god slot), distinct from bomb slot +64. */
    int has_dynamic_attachment;
    /* 0x408FBD: attached objects require all of player +50..+53 zero. */
    int attachments_hidden;
    int visible;
    /* The original keeps player +8 xpos at zero before the initial landing.
     * SDL assigns a logical route node earlier, so this preserves the
     * original overview-map visibility predicate independently of node. */
    int first_turn_landing_pending;
    int world_x;
    int world_y;
    /* 0x416E6D is not repainted during the blocking ambulance FLC. */
    int minimap_world_x;
    int minimap_world_y;
    int moving;
    int walk_frame;
    int visual_direction;
    int primary_part;
    int fallback_part;
    int overlay_part;
    int use_action_part;
    int action_part;
    int action_frame;
    int action_holds_last_frame;
    ViewEvidenceConfidence part_confidence;
} GameActorView;

typedef struct GameSpecialActorView {
    int slot;
    int visible;
    int world_x;
    int world_y;
    int visual_direction;
    int resource_part;
    int animation_frame;
    /* 0x40829D adds a second Map[25] sprite while the special actor is
     * sleepwalking.  The original render state advances this common-sprite
     * chunk independently of the actor model. */
    int overlay_chunk;
} GameSpecialActorView;

typedef struct GameToolRobotView {
    int visible;
    int resource_part;
    int world_x;
    int world_y;
    int visual_direction;
    int animation_frame;
} GameToolRobotView;

typedef struct GameHudView {
    int visible;
    int active_tab;
    int top_button_hover;
    int minimap_rotate_pressed;
    int current_player;
    int character;
    int cash;
    int bank;
    int total_assets;
    int cash_and_bank;
    int owned_property_count;
    int owned_chain_count;
    int built_facility_count;
    /* 0x41646C: current-market value, acquisition cost, and companies
     * controlled by the current player. */
    int stock_market_value;
    int stock_cost;
    int stock_company_control_count;
    /* 0x4165E1: the final information tab. */
    int points;
    int loan;
    int insurance_days;
    int price_index;
    int ally_character; /* 0x496BA9: linked player, not an attached god. */
    int loan_days_remaining;
    int special_actor; /* Original actor IDs 4..7 use Panel[0]/chunk 5. */
    int special_portrait_character;
} GameHudView;

typedef struct GameCalendarView {
    int year, month, day, weekday;
    int month_days, first_weekday;
    int map_index, event_index;
    uint32_t closed_days; /* sub_4523D5 for each date in the visible month. */
} GameCalendarView;

typedef struct GameDiceView {
    int popup_visible;
    int count;
    int faces[3];
    int animation_frame;
    int result_visible;
    int visual_direction;
    int status_visible;
    int status_x;
    int status_y;
    int status_variant;
    int traffic_method;
    int available_dice;
} GameDiceView;

/* Read-only presentation input assembled at the platform boundary.  The
 * view owns no mutable session state; see the SDL adapter and view tests. */
typedef struct GameViewActorInput {
    int direction;
    int walk_frame;
    int visual_x;
    int visual_y;
    int move_active;
    int intro_visible;
    int first_turn_landing_pending;
    int dirty_flags;
} GameViewActorInput;

typedef struct GameViewSpecialActorInput {
    int active;
    int slot;
    int visual_x;
    int visual_y;
    int direction;
    int animation_frame;
    int resource_part;
    int steps_remaining;
} GameViewSpecialActorInput;

typedef struct GameViewModelInput {
    /* Data[553]: retain the previous building level until frame 44. */
    GameDomainRef worker_previous_level_target;
    /* 0x40F4D9: angel notice retains the pre-upgrade map background. */
    GameDomainRef angel_previous_level_target;
    /* 0x40F675 / 0x4512BF: demolition background changes at frame 3. */
    GameDomainRef demon_previous_building_target;
    int demon_previous_building_type;
    int demon_previous_building_level;
    /* 0x4512BF background snapshots: actor+1 is zero when inactive. */
    int tool_effect_actor;
    int tool_effect_resource;
    int tool_effect_frame;
    int tool_effect_explosion_traffic_method;
    int tool_effect_origin_play_state;
    int tool_effect_origin_traffic_method;
    int tool_effect_node;
    int tool_effect_has_world_position;
    int tool_effect_world_x;
    int tool_effect_world_y;
    int engineering_attack_active;
    int engineering_attack_player;
    int engineering_attack_frame;
    int route_count;
    int map_direction;
    int has_view_override;
    int view_world_x;
    int view_world_y;
    int active_info_tab;
    int minimap_rotate_pressed;
    int top_button_hover;
    GameViewActorInput actors[GAME_MAX_PLAYERS];
    GameViewSpecialActorInput special_actor;
    int tool_robot_started;
    int tool_robot_arrived;
    int turn_flow_active;
    int turn_state;
    int turn_state_frame;
    int dice_status_visible;
    int dice_status_x;
    int dice_status_y;
    int dice_status_variant;
    int dice_popup_active;
    int dice_count;
    int dice_faces[3];
    int dice_anim_frame;
    int dice_result_visible;
    int landing_pending_player;
    int steps_remaining;
    int property_presentation_group;
    int property_presentation_owner;
    int property_presentation_land;
    int property_flash_adjust;
    int property_flash_active;
} GameViewModelInput;

typedef struct GameLandView {
    int owner;
    int type;
    int level;
    int group;
    int price_status;
    int flash_adjust;
} GameLandView;

typedef struct GameFacilityView {
    int owner;
    int type;
    int level;
    int research_selection;
    int research_turns;
    int price_status;
} GameFacilityView;

typedef struct GameCompanyView {
    int owner;
} GameCompanyView;

typedef struct GameViewModel {
    int active;
    int route_count;
    GameViewportModel viewport;
    GameActorView actors[GAME_MAX_PLAYERS];
    int actor_count;
    GameSpecialActorView special_actors[GAME_SPECIAL_ACTOR_MODEL_COUNT];
    int special_actor_count;
    /* Original renderer's current entity index: 0..3 for an ordinary
     * player, GAME_MAX_PLAYERS+slot while a fixed actor owns the turn. */
    int current_actor;
    GameToolRobotView tool_robot;
    GameHudView hud;
    GameCalendarView calendar;
    GameDiceView dice;
    GameLandView lands[GAME_MAX_MAP_PROPERTIES];
    int land_count;
    GameFacilityView facilities[GAME_MAX_MAP_PROPERTIES];
    int facility_count;
    GameCompanyView companies[GAME_MAX_MAP_PROPERTIES];
    int company_count;
    int steps_remaining;
    int show_steps;
    int property_flash_group;
    int property_flash_owner;
    int property_flash_adjust;
    int property_flash_active;
} GameViewModel;

void rich4_game_viewport_build(const GameState *state,
                               const GameViewModelInput *input,
                               const MapData *map,
                               GameViewportModel *out_viewport);
void rich4_game_viewport_project(const GameViewportModel *viewport,
                                 int world_x,
                                 int world_y,
                                 int *out_x,
                                 int *out_y);
int rich4_game_actor_part(const GamePlayerState *player,
                          int special_road_node,
                          int moving,
                          int *out_overlay_part);
int rich4_game_actor_action_part(const GamePlayerState *player,
                                 int special_road_node);
int rich4_game_actor_direction_from_delta(int dx, int dy);
int rich4_game_view_model_build(const GameState *state,
                                const GameViewModelInput *input,
                                const MapData *map,
                                GameViewModel *out_model);

#endif
