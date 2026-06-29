#ifndef RICH4_PLATFORM_SDL1_APP_ASSET_PACKS_H
#define RICH4_PLATFORM_SDL1_APP_ASSET_PACKS_H

#include <stddef.h>

#include "core/game_constants.h"
#include "data/map_data.h"
#include "data/resource_constants.h"
#include "resource/resource_store.h"
#include "resource/spr_smp.h"

#ifndef GAME_SPECIAL_EVENT_ASSET_COUNT
#define GAME_SPECIAL_EVENT_ASSET_COUNT 85
#endif
#ifndef GAME_NEWS_CONSEQUENCE_COUNT
#define GAME_NEWS_CONSEQUENCE_COUNT 5
#endif
#ifndef GAME_TOOL_EFFECT_ASSET_COUNT
/* Eight tool FLCs plus the two type-11 dog encounter FLCs. */
#define GAME_TOOL_EFFECT_ASSET_COUNT 10
#endif
#ifndef GAME_PANEL_RESOURCE_COUNT
#define GAME_PANEL_RESOURCE_COUNT 113
#endif
#ifndef GAME_BANK_CALENDAR_EVENT_COUNT
#define GAME_BANK_CALENDAR_EVENT_COUNT 24
#endif

typedef struct GameIntroAssetPack {
    unsigned char *jump_data[GAME_BACKGROUND_COUNT];
    size_t jump_size[GAME_BACKGROUND_COUNT];
    int has_jump_background[GAME_BACKGROUND_COUNT];
    unsigned char *jump_intro_scene_data;
    size_t jump_intro_scene_size;
    Rich4SprSmpInfo jump_intro_scene_spr;
    int has_jump_intro_scene;
    unsigned char *jump_intro_window_data;
    size_t jump_intro_window_size;
    int has_jump_intro_window_graph;
    int jump_intro_resource_base;
} GameIntroAssetPack;

typedef struct GameMapAssetPack {
    MapData maps[GAME_BACKGROUND_COUNT];
    unsigned char *map_overview_data[GAME_BACKGROUND_COUNT];
    size_t map_overview_size[GAME_BACKGROUND_COUNT];
    Rich4ResourceHandle map_overview_resource[GAME_BACKGROUND_COUNT];
    Rich4SprSmpInfo map_overview_spr[GAME_BACKGROUND_COUNT];
    int has_map_overview[GAME_BACKGROUND_COUNT];
    unsigned char *map_overlay_data[GAME_BACKGROUND_COUNT];
    size_t map_overlay_size[GAME_BACKGROUND_COUNT];
    int has_map_overlay[GAME_BACKGROUND_COUNT];
    unsigned char *map_view_data[GAME_BACKGROUND_COUNT][GAME_MAP_VIEW_RESOURCE_COUNT];
    size_t map_view_size[GAME_BACKGROUND_COUNT][GAME_MAP_VIEW_RESOURCE_COUNT];
    int has_map_view[GAME_BACKGROUND_COUNT][GAME_MAP_VIEW_RESOURCE_COUNT];
    unsigned char *map_stage_tile_data[GAME_BACKGROUND_COUNT][GAME_MAP_STAGE_TILE_RESOURCE_COUNT];
    size_t map_stage_tile_size[GAME_BACKGROUND_COUNT][GAME_MAP_STAGE_TILE_RESOURCE_COUNT];
    int has_map_stage_tile[GAME_BACKGROUND_COUNT][GAME_MAP_STAGE_TILE_RESOURCE_COUNT];
    unsigned char *map_common_data[GAME_MAP_COMMON_RESOURCE_COUNT];
    size_t map_common_size[GAME_MAP_COMMON_RESOURCE_COUNT];
    Rich4ResourceHandle map_common_resource[GAME_MAP_COMMON_RESOURCE_COUNT];
    Rich4SprSmpInfo map_common_spr[GAME_MAP_COMMON_RESOURCE_COUNT];
    int has_map_common[GAME_MAP_COMMON_RESOURCE_COUNT];
    int has_map_common_graph[GAME_MAP_COMMON_RESOURCE_COUNT];
    unsigned char *map_ground_data[GAME_BACKGROUND_COUNT];
    size_t map_ground_size[GAME_BACKGROUND_COUNT];
    Rich4ResourceHandle map_ground_resource[GAME_BACKGROUND_COUNT];
    int has_map_ground[GAME_BACKGROUND_COUNT];
    unsigned char *map_control_data;
    size_t map_control_size;
    Rich4SprSmpInfo map_control_spr;
    int has_map_control;
    unsigned char *map_control_extra_data;
    size_t map_control_extra_size;
    int has_map_control_extra;
    unsigned char *data_map_status_data[GAME_DATA_MAP_STATUS_RESOURCE_COUNT];
    size_t data_map_status_size[GAME_DATA_MAP_STATUS_RESOURCE_COUNT];
    Rich4SprSmpInfo data_map_status_spr[GAME_DATA_MAP_STATUS_RESOURCE_COUNT];
    /* Original raw-table eligibility depends on a non-null archive resource
     * pointer, separately from whether SDL can decode its frame format. */
    int data_map_status_resource_present[GAME_DATA_MAP_STATUS_RESOURCE_COUNT];
    int has_data_map_status[GAME_DATA_MAP_STATUS_RESOURCE_COUNT];
    unsigned char *map_object_data[GAME_MAX_MAP_OBJECT_SPRITES];
    size_t map_object_size[GAME_MAX_MAP_OBJECT_SPRITES];
    Rich4ResourceHandle map_object_resource[GAME_MAX_MAP_OBJECT_SPRITES];
    Rich4SprSmpInfo map_object_spr[GAME_MAX_MAP_OBJECT_SPRITES];
    int map_object_resource_present[GAME_MAX_MAP_OBJECT_SPRITES];
    int has_map_object_sprite[GAME_MAX_MAP_OBJECT_SPRITES];
    int map_object_count[GAME_BACKGROUND_COUNT];
    int map_object_x[GAME_BACKGROUND_COUNT][GAME_MAX_MAP_OBJECTS];
    int map_object_y[GAME_BACKGROUND_COUNT][GAME_MAX_MAP_OBJECTS];
    int map_object_sprite[GAME_BACKGROUND_COUNT][GAME_MAX_MAP_OBJECTS];
    int map_object_chunk[GAME_BACKGROUND_COUNT][GAME_MAX_MAP_OBJECTS];
    int map_object_id[GAME_BACKGROUND_COUNT][GAME_MAX_MAP_OBJECTS];
    int map_object_owner[GAME_BACKGROUND_COUNT][GAME_MAX_MAP_OBJECTS];
    int map_object_variant_base[GAME_BACKGROUND_COUNT][GAME_MAX_MAP_OBJECTS];
    int map_object_variant_owner[GAME_BACKGROUND_COUNT][GAME_MAX_MAP_OBJECTS];
} GameMapAssetPack;

typedef struct GameCharacterAssetPack {
    unsigned char *jump_intro_actor_data[2][GAME_JUMP_CHARACTER_COUNT];
    size_t jump_intro_actor_size[2][GAME_JUMP_CHARACTER_COUNT];
    int has_jump_intro_actor_graph[2][GAME_JUMP_CHARACTER_COUNT];
    unsigned char *first_landing_data[GAME_JUMP_CHARACTER_COUNT];
    size_t first_landing_size[GAME_JUMP_CHARACTER_COUNT];
    int has_first_landing_graph[GAME_JUMP_CHARACTER_COUNT];
    unsigned char *map_character_data[12];
    size_t map_character_size[12];
    Rich4ResourceHandle map_character_resource[12];
    Rich4SprSmpInfo map_character_spr[12];
    int has_map_character[12];
    unsigned char *walk_data[12][3];
    size_t walk_size[12][3];
    Rich4SprSmpInfo walk_spr[12][3];
    int has_walk[12][3];
    unsigned char *map_player_data[12][GAME_CHARACTER_MODEL_RESOURCES];
    size_t map_player_size[12][GAME_CHARACTER_MODEL_RESOURCES];
    Rich4SprSmpInfo map_player_spr[12][GAME_CHARACTER_MODEL_RESOURCES];
    /* Resource presence mirrors the original non-null Data.mkf pointer; the
     * separate parsed flag only says that SDL can decode/draw the resource. */
    int map_player_resource_present[12][GAME_CHARACTER_MODEL_RESOURCES];
    int has_map_player[12][GAME_CHARACTER_MODEL_RESOURCES];
    unsigned char *map_special_actor_data[GAME_SPECIAL_ACTOR_MODEL_COUNT]
                                          [GAME_SPECIAL_ACTOR_MODEL_RESOURCES];
    size_t map_special_actor_size[GAME_SPECIAL_ACTOR_MODEL_COUNT]
                                 [GAME_SPECIAL_ACTOR_MODEL_RESOURCES];
    Rich4SprSmpInfo map_special_actor_spr[GAME_SPECIAL_ACTOR_MODEL_COUNT]
                                           [GAME_SPECIAL_ACTOR_MODEL_RESOURCES];
    int map_special_actor_resource_present[GAME_SPECIAL_ACTOR_MODEL_COUNT]
                                          [GAME_SPECIAL_ACTOR_MODEL_RESOURCES];
    int has_map_special_actor[GAME_SPECIAL_ACTOR_MODEL_COUNT]
                             [GAME_SPECIAL_ACTOR_MODEL_RESOURCES];
    Rich4RawAsset tool_robot[2];
    Rich4SprSmpInfo tool_robot_spr[2];
    int has_tool_robot[2];
    unsigned char *character_data;
    size_t character_size;
    Rich4SprSmpInfo character_spr;
    int has_character_ui;
} GameCharacterAssetPack;

typedef struct GameMenuAssetPack {
    unsigned char *setup_data;
    size_t setup_size;
    Rich4SprSmpInfo setup_spr;
    int has_setup_ui;
    unsigned char *options_data;
    size_t options_size;
    Rich4SprSmpInfo options_spr;
    int has_options_ui;
    unsigned char *ai_settings_data;
    size_t ai_settings_size;
    Rich4SprSmpInfo ai_settings_spr;
    int has_ai_settings_ui;
} GameMenuAssetPack;

typedef struct GameUiAssetPack {
    unsigned char *bank_calendar_data[GAME_BACKGROUND_COUNT]
                                      [GAME_BANK_CALENDAR_EVENT_COUNT];
    size_t bank_calendar_size[GAME_BACKGROUND_COUNT]
                             [GAME_BANK_CALENDAR_EVENT_COUNT];
    int has_bank_calendar[GAME_BACKGROUND_COUNT]
                         [GAME_BANK_CALENDAR_EVENT_COUNT];
    unsigned char *worker_choice_data;
    size_t worker_choice_size;
    Rich4SprSmpInfo worker_choice_spr;
    int has_worker_choice_ui;
    /* 0x440E3D: Data[518] player-choice frames, portraits from Data[2]. */
    unsigned char *player_choice_data;
    size_t player_choice_size;
    Rich4SprSmpInfo player_choice_spr;
    int has_player_choice;
    unsigned char *monster_card_data;
    size_t monster_card_size;
    int has_monster_card_animation;
    unsigned char *demolish_card_data;
    size_t demolish_card_size;
    int has_demolish_card_animation;
    /* 0x40CD87: bankruptcy/clearance animation, Data.MKF[555]. */
    unsigned char *bankruptcy_data;
    size_t bankruptcy_size;
    int has_bankruptcy_animation;
    /* 0x4385B9/0x438D82, Data.MKF[416+2*character+kind]. */
    unsigned char *monthly_award_data[12][2];
    size_t monthly_award_size[12][2];
    /* 0x407842's successful continuation branch plays Data.MKF[556]
     * before it returns code 4 to the outer load UI. */
    unsigned char *ending_success_data;
    size_t ending_success_size;
    int has_ending_success_animation;
    unsigned char *special_reward_data[2];
    size_t special_reward_size[2];
    int has_special_reward[2];
    unsigned char *god_effect_data[GAME_GOD_TYPE_COUNT + 1];
    size_t god_effect_size[GAME_GOD_TYPE_COUNT + 1];
    int god_effect_resource[GAME_GOD_TYPE_COUNT + 1];
    int has_god_effect[GAME_GOD_TYPE_COUNT + 1];
    unsigned char *special_event_data[GAME_SPECIAL_EVENT_ASSET_COUNT];
    size_t special_event_size[GAME_SPECIAL_EVENT_ASSET_COUNT];
    int special_event_resource[GAME_SPECIAL_EVENT_ASSET_COUNT];
    int has_special_event[GAME_SPECIAL_EVENT_ASSET_COUNT];
    unsigned char *news_consequence_data[GAME_NEWS_CONSEQUENCE_COUNT];
    size_t news_consequence_size[GAME_NEWS_CONSEQUENCE_COUNT];
    int news_consequence_resource[GAME_NEWS_CONSEQUENCE_COUNT];
    int has_news_consequence[GAME_NEWS_CONSEQUENCE_COUNT];
    unsigned char *point_minigame_effect_data;
    size_t point_minigame_effect_size;
    int has_point_minigame_effect;
    unsigned char *card_effect_data[GAME_CARD_COUNT];
    size_t card_effect_size[GAME_CARD_COUNT];
    int has_card_effect[GAME_CARD_COUNT];
    unsigned char *panel_data[GAME_PANEL_RESOURCE_COUNT];
    size_t panel_size[GAME_PANEL_RESOURCE_COUNT];
    Rich4SprSmpInfo panel_spr[GAME_PANEL_RESOURCE_COUNT];
    int has_panel[GAME_PANEL_RESOURCE_COUNT];
    int has_panel_graph[GAME_PANEL_RESOURCE_COUNT];
    int has_panel_raw[GAME_PANEL_RESOURCE_COUNT];
    int has_panel_mask[9];
    unsigned char *remote_dice_data;
    size_t remote_dice_size;
    Rich4SprSmpInfo remote_dice_spr;
    int has_remote_dice_ui;
    unsigned char *confirm_dialog_data;
    size_t confirm_dialog_size;
    Rich4SprSmpInfo confirm_dialog_spr;
    int has_confirm_dialog;
    unsigned char *common_dialog_data;
    size_t common_dialog_size;
    Rich4SprSmpInfo common_dialog_spr;
    int has_common_dialog;
    /* 0x403D97/0x404184 load Data.mkf[520] for the save/load slot
     * dialog. 0x403DB1/0x40419E load Data.mkf[2] for its small slot icons. */
    unsigned char *save_load_panel_data;
    size_t save_load_panel_size;
    Rich4SprSmpInfo save_load_panel_spr;
    int has_save_load_panel;
    unsigned char *save_load_icons_data;
    size_t save_load_icons_size;
    Rich4SprSmpInfo save_load_icons_spr;
    int has_save_load_icons;
    unsigned char *speech_icon_data;
    size_t speech_icon_size;
    Rich4SprSmpInfo speech_icon_spr;
    int has_speech_icons;
} GameUiAssetPack;

typedef struct GameToolAssetPack {
    unsigned char *tool_effect_data[GAME_TOOL_EFFECT_ASSET_COUNT];
    size_t tool_effect_size[GAME_TOOL_EFFECT_ASSET_COUNT];
    int tool_effect_resource[GAME_TOOL_EFFECT_ASSET_COUNT];
    int has_tool_effect[GAME_TOOL_EFFECT_ASSET_COUNT];
    unsigned char *property_max_level_data;
    size_t property_max_level_size;
    int has_property_max_level_graph;
} GameToolAssetPack;

#endif
