#include "platform/sdl1/game_lazy_assets.h"
/* Purpose: adapt card-panel app actions to SDL assets, audio and domain apply.
 * Owns: resource-backed map target projection and platform effect timing.
 * Borrows: app card-panel state/session, preview assets and audio service.
 * Tests: test_card_panel_flow.
 */

#include <SDL/SDL.h>

#include "app/card_panel_flow.h"
#include "app/card_panel_state.h"
#include "app/dynamic_object_flow.h"
#include "app/game_session.h"
#include "app/game_speech_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "core/card.h"
#include "core/ai.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/render/game_scene_view.h"
#include "view/map_renderer.h"

static void card_panel_play_sound(Rich4Audio *audio,
                                  Rich4CardPanelSound sound)
{
    if (sound == RICH4_CARD_PANEL_SOUND_OK ||
        sound == RICH4_CARD_PANEL_SOUND_OK_INVALID ||
        sound == RICH4_CARD_PANEL_SOUND_OK_CARD)
        rich4_audio_play_ok(audio);
    if (sound == RICH4_CARD_PANEL_SOUND_INVALID ||
        sound == RICH4_CARD_PANEL_SOUND_OK_INVALID)
        rich4_audio_play_invalid(audio);
    if (sound == RICH4_CARD_PANEL_SOUND_OK_CARD)
        rich4_audio_play_card(audio);
    if (sound == RICH4_CARD_PANEL_SOUND_TARGET)
        rich4_audio_play_target_confirm(audio);
}

static int card_panel_dynamic_slot_from_object_id(int object_id)
{
    int high;

    if ((object_id & 0x8000) == 0 || (object_id & 0xff) != 0)
        return -1;
    high = (object_id >> 8) & 0x7f;
    if ((object_id & 0x4000) != 0) high &= ~0x40;
    /* Slots 0..15 are gods/death.  Card 12's mask only includes the
     * placement ranges 16..45; 0x8100 is the separate tool-robot id. */
    if (high < 17 || high > GAME_MAP_DYNAMIC_OBJECT_COUNT) return -1;
    return high - 1;
}

static int card_panel_demolish_property_valid(
    const GameState *state, int actor, GameDomainRef target)
{
    int owner;
    int level;

    if (state == NULL || actor < 0 || actor >= state->player_count)
        return 0;
    if (target.kind == GAME_DOMAIN_LAND && target.id >= 0 &&
        target.id < state->land_count) {
        owner = state->land_owner[target.id];
        level = state->land_level[target.id];
    } else if (target.kind == GAME_DOMAIN_FACILITY && target.id >= 0 &&
               target.id < state->facility_count) {
        owner = state->facility_owner[target.id];
        level = state->facility_level[target.id];
    } else {
        return 0;
    }
    return owner >= 0 && owner < state->player_count && owner != actor &&
           level > 0;
}

static void card_panel_apply_action(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, CardPanelState *panel,
    const Rich4CardPanelAction *action, Uint32 now, Rich4Audio *audio)
{
    int speech_player = -1;
    uint32_t animation_duration = 0;
    Rich4CardPanelCommandResult result;

    if (action == NULL || action->kind != RICH4_CARD_PANEL_ACTION_SUBMIT)
        return;
    if (!game_session_apply_command(
            session, assets, settings, &action->command, now)) {
        rich4_card_panel_command_rejected(panel);
        rich4_audio_play_invalid(audio);
        return;
    }
    if (panel->selected_card == 10 &&
        game_asset_ready(assets, &assets->ui.has_monster_card_animation)) {
        int frames = game_asset_graph_frames(assets, &assets->ui.monster_card_data, assets->ui.monster_card_size);
        animation_duration = (uint32_t)((frames > 0 ? frames : 1) * 80 + 500);
    } else if (panel->selected_card == 11 &&
               (action->target.kind == GAME_DOMAIN_LAND ||
                action->target.kind == GAME_DOMAIN_FACILITY) &&
               game_asset_ready(assets, &assets->ui.has_demolish_card_animation)) {
        int frames = game_asset_graph_frames(assets, &assets->ui.demolish_card_data, assets->ui.demolish_card_size);
        animation_duration = (uint32_t)((frames > 0 ? frames : 1) * 80 + 500);
    }
    result = rich4_card_panel_finish_command(
        panel, session, action->target, now, animation_duration,
        &speech_player);
    if (result == RICH4_CARD_PANEL_COMMAND_EFFECT)
        rich4_audio_play_card(audio);
    else if (result == RICH4_CARD_PANEL_COMMAND_MONSTER_ANIMATION)
        rich4_audio_play_card_animation(audio, 80);
    else if (result == RICH4_CARD_PANEL_COMMAND_DEMOLISH_ANIMATION)
        rich4_audio_play_card_animation(audio, 97);
    else if (speech_player >= 0)
        game_start_monster_victim_speech(session, speech_player, now);
}

void game_update_card_panel_timed_effect(
    CardPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    Rich4Audio *audio)
{
    Rich4CardPanelTimedResult timed;
    Rich4CardPanelAction action;
    int speech_player = -1;
    Uint32 now;

    rich4_card_panel_update_animation(panel, rich4_sdl1_ticks());
    if (panel == NULL || session == NULL || assets == NULL ||
        settings == NULL || !rich4_card_panel_effect_due(
            panel, rich4_sdl1_ticks()))
        return;
    now = rich4_sdl1_ticks();
    timed = rich4_card_panel_finish_timed_effect(panel, &speech_player);
    if (timed == RICH4_CARD_PANEL_TIMED_RESUME_PRE_EFFECT) {
        action = rich4_card_panel_resume_pre_effect(panel, session);
        if (action.kind != RICH4_CARD_PANEL_ACTION_NONE)
            card_panel_play_sound(audio, action.sound);
        if (action.kind == RICH4_CARD_PANEL_ACTION_SUBMIT) {
            card_panel_apply_action(
                session, assets, settings, panel, &action, now, audio);
        }
    } else if (timed == RICH4_CARD_PANEL_TIMED_MONSTER_SPEECH &&
               speech_player >= 0) {
        game_start_monster_victim_speech(session, speech_player, now);
    }
}

static GameDomainRef card_panel_map_target(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, const CardPanelState *panel,
    int x, int y, int *out_value)
{
    GameSceneView view;
    const MapData *map;
    int target_kind = rich4_card_panel_map_target_kind(panel);
    int card = panel == NULL ? -1 : panel->selected_card;
    int background;
    int best_distance = 0x7fffffff;
    int i;
    GameDomainRef best = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);

    if (out_value != NULL) *out_value = 0;
    if (assets == NULL || settings == NULL || session == NULL ||
        target_kind == GAME_CARD_TARGET_NONE ||
        !build_game_session_render_view(assets, settings, session, &view))
        return best;
    background = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background < 0 || background >= GAME_BACKGROUND_COUNT) return best;
    map = &assets->map.maps[background];
    if (target_kind == GAME_CARD_TARGET_PLAYER) {
        for (i = 0; i < view.game.actor_count; i++) {
            int sx, sy, dx, dy, distance;
            const GameActorView *actor = &view.game.actors[i];
            if (!actor->visible ||
                (session->state.players[i].play_state & 0xff) == 0 ||
                (card == 12 && i == session->state.current_player))
                continue;
            rich4_game_viewport_project(
                &view.game.viewport, actor->world_x, actor->world_y,
                &sx, &sy);
            dx = sx - x;
            dy = sy - y;
            distance = dx * dx + dy * dy;
            if (distance <= 24 * 24 && distance < best_distance) {
                best_distance = distance;
                best = rich4_game_domain_ref(GAME_DOMAIN_PLAYER, i);
            }
        }
        return best;
    }
    /* 请神符 is resolved by the app card flow through the original nearest
     * active-god scan; it never enters a cursor-selected map-target phase. */
    if (target_kind == GAME_CARD_TARGET_GOD) return best;
    if (target_kind == GAME_CARD_TARGET_PROPERTY_OR_MAP_OBJECT) {
        GameVisibleMapObject visible;
        int object_id = game_map_object_at(
            assets, settings, &view.game,
            session->presentation.dynamic_objects, x, y, &visible);
        int dynamic_slot = card_panel_dynamic_slot_from_object_id(object_id);
        if (dynamic_slot >= 0 &&
            session->state.tool_placed_objects[dynamic_slot].kind >= 0x10 &&
            session->state.tool_placed_objects[dynamic_slot].kind <= 0x12)
            return rich4_game_domain_ref(
                GAME_DOMAIN_MAP_OBJECT, dynamic_slot);
    }
    for (i = 0; i < session->state.land_count &&
                (size_t)(i + 1) < map->land_count; i++) {
        int sx, sy, dx, dy, distance;
        if (target_kind == GAME_CARD_TARGET_PROPERTY_OR_MAP_OBJECT &&
            !card_panel_demolish_property_valid(
                &session->state, panel->player,
                rich4_game_domain_ref(GAME_DOMAIN_LAND, i)))
            continue;
        rich4_game_viewport_project(
            &view.game.viewport, map->lands[i + 1].x,
            map->lands[i + 1].y, &sx, &sy);
        dx = sx - x;
        dy = sy - y;
        distance = dx * dx + dy * dy;
        if (distance <= 24 * 24 && distance < best_distance) {
            best_distance = distance;
            best = rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
        }
    }
    for (i = 0; i < session->state.facility_count &&
                (size_t)(i + 1) < map->facility_count; i++) {
        int sx, sy, dx, dy, distance;
        if (target_kind == GAME_CARD_TARGET_PROPERTY_OR_MAP_OBJECT &&
            !card_panel_demolish_property_valid(
                &session->state, panel->player,
                rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i)))
            continue;
        rich4_game_viewport_project(
            &view.game.viewport, map->facilities[i + 1].x,
            map->facilities[i + 1].y, &sx, &sy);
        dx = sx - x;
        dy = sy - y;
        distance = dx * dx + dy * dy;
        if (distance <= 24 * 24 && distance < best_distance) {
            best_distance = distance;
            best = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
        }
    }
    return best;
}

static int card_panel_projected_god_target(
    const GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, int player,
    GameDomainRef *out_target, int *out_type)
{
    GameSceneView view;
    GameVisibleMapObject visible[GAME_MAP_RENDER_QUEUE_CAPACITY];
    GameAiGlobalObjectProjection objects[GAME_MAP_RENDER_QUEUE_CAPACITY];
    GameAiGlobalProjection projection;
    int background_index;
    int object_count;
    int i;
    int object_id;

    if (out_target != NULL)
        *out_target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    if (out_type != NULL) *out_type = 0;
    if (session == NULL || assets == NULL || settings == NULL ||
        out_target == NULL || out_type == NULL ||
        !build_game_session_render_view(assets, settings, session, &view))
        return 0;
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    object_count = game_map_collect_ai_objects(
        assets, background_index, &view.game, view.dynamic_objects,
        visible, GAME_MAP_RENDER_QUEUE_CAPACITY);
    for (i = 0; i < object_count; i++) {
        objects[i].raw_id = (uint16_t)visible[i].object_id;
        objects[i].screen_x = visible[i].x;
        objects[i].screen_y = visible[i].y - 40;
    }
    memset(&projection, 0, sizeof(projection));
    projection.object_table_available = 1;
    projection.objects = objects;
    projection.object_count = object_count;
    projection.center_world_x = view.game.viewport.center_world_x;
    projection.center_world_y = view.game.viewport.center_world_y;
    projection.direction = view.game.viewport.direction;
    object_id = rich4_app_find_nearest_dynamic_god_with_projection(
        &session->presentation, &session->state, player, &projection,
        out_type);
    if (object_id >= 0)
        *out_target = rich4_game_domain_ref(GAME_DOMAIN_GOD, object_id);
    return 1;
}

void game_card_panel_confirm(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, CardPanelState *panel,
    int x, int y, Uint32 now, Rich4Audio *audio)
{
    GameDomainRef target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    GameDomainRef projected_god = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    int value = 0;
    int projected_god_type = 0;
    int god_projection_available = 0;
    Rich4CardPanelAction action;

    if (panel == NULL || session == NULL ||
        panel->phase == CARD_PANEL_CLOSED ||
        panel->phase == CARD_PANEL_EFFECT ||
        panel->phase == CARD_PANEL_MONSTER_ANIMATION ||
        panel->phase == CARD_PANEL_DEMOLISH_ANIMATION)
        return;
    if (panel->phase == CARD_PANEL_MAP_TARGET)
        target = card_panel_map_target(
            assets, settings, session, panel, x, y, &value);
    if (panel->phase == CARD_PANEL_INVENTORY && panel->player >= 0 &&
        panel->player < session->state.player_count) {
        int slot = rich4_card_panel_slot_at(x, y);
        if (slot >= 0 &&
            session->state.players[panel->player].card_inventory[slot] == 23)
            god_projection_available = card_panel_projected_god_target(
                session, assets, settings, panel->player,
                &projected_god, &projected_god_type);
    }
    action = god_projection_available
        ? rich4_card_panel_pointer_action_with_god_target(
              panel, session, x, y, target, value, now,
              projected_god, projected_god_type)
        : rich4_card_panel_pointer_action(
              panel, session, x, y, target, value, now);
    card_panel_play_sound(audio, action.sound);
    card_panel_apply_action(
        session, assets, settings, panel, &action, now, audio);
}

void game_card_panel_resume_pre_effect(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, CardPanelState *panel,
    Uint32 now, Rich4Audio *audio)
{
    Rich4CardPanelAction action;

    if (panel == NULL || session == NULL || assets == NULL ||
        settings == NULL)
        return;
    action = rich4_card_panel_resume_pre_effect(panel, session);
    if (action.kind != RICH4_CARD_PANEL_ACTION_NONE)
        card_panel_play_sound(audio, action.sound);
    if (action.kind == RICH4_CARD_PANEL_ACTION_SUBMIT) {
        card_panel_apply_action(
            session, assets, settings, panel, &action, now, audio);
    }
}
