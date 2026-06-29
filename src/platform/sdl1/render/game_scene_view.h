#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_SCENE_VIEW_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_SCENE_VIEW_H

#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/game_map_dynamic_object.h"
#include "data/character_speech.h"
#include "view/game_view_model.h"

/* R10.1c seam: the aggregate presentation snapshot the SDL1 game scene renders
 * from. Built once per frame by game_scene_view.c from the pure
 * GameState/GameFlowState + preview assets, then consumed read-only by the
 * game_*_render draw code. */

/* Property presentation phase sequence (0x419D50 flow). Shared by the session
 * update logic and the property presentation renderer. */
typedef struct GameGodDetachmentView {
    int active;
    int god_type;
    int frame;
    int direction;
    int world_x;
    int world_y;
} GameGodDetachmentView;

typedef struct GameSceneView {
    GameViewModel game;
    GameMapDynamicObjectState dynamic_objects[GAME_MAP_DYNAMIC_OBJECT_COUNT];
    GameGodDetachmentView god_detachment;
    int ending_active;
    int ending_continuation_active;
    int ending_winner_player;
    int ending_winner_character;
    int ending_reason;
    int ending_assets;
    int ending_countdown;
    int ending_jump_active;
    int ending_confirmation_active;
    int ending_confirmation_frame;
    int ending_background_index;
    int use_main_view;
    int map_info_active;
    int map_info_object_id;
    int map_info_cursor_x;
    int map_info_cursor_y;
    int view_mode;
    int intro_player;
    int intro_frame;
    int intro_total_frames;
    int landing_player;
    int landing_frame;
    int landing_frames;
    int show_steps;
    int route_interaction_active;
    int route_interaction_hover;
    int route_interaction_pressed;
    int route_interaction_facility_build;
    int route_interaction_facility_research;
    int route_interaction_facility_choice_count;
    GameCommandType route_interaction_type;
    int route_interaction_amount;
    char route_interaction_name[21];
    int bank_interaction_mode;
    int bank_interaction_amount;
    int bank_interaction_pressed_control;
    int bank_interaction_special_page;
    int bank_interaction_amount_entry;
    int bank_interaction_card_x;
    int bank_interaction_face_chunk;
    int bank_interaction_mouth_chunk;
    const Rich4CharacterSpeech *bank_interaction_speech;
    int bank_loan_disabled;
    int bank_cash;
    int bank_savings;
    int bank_loan;
    int bank_loan_available;
    int bank_special_finance;
    int bank_special_available;
    int bank_special_enabled;
    int bank_character;
    int bank_loan_days_remaining;
    int bank_elapsed_days;
    int bank_calendar_year;
    int bank_calendar_month;
    int bank_calendar_day;
    int bank_calendar_weekday;
    int bank_calendar_map_index;
    int bank_calendar_event_index;
    int property_presentation_phase;
    int property_presentation_amount;
    int property_god_type;
    int property_fee_label;
    int property_company_type;
    int property_facility_type;
    int property_outcome;
    int property_collection_block_reason;
    int property_owner_character;
    int property_related_owner_character;
    int property_owner_amount;
    int property_related_amount;
    char property_name[21];
    int ai_tool_notice;
    int ai_tool_id;
    int speech_active;
    int speech_character;
    int speech_kind;
    int speech_type;
    const Rich4CharacterSpeech *speech;
    int property_max_level_active;
    int property_max_level_frame;
} GameSceneView;

#endif
