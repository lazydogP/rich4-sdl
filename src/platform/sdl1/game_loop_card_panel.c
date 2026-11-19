#include "platform/sdl1/game_lazy_assets.h"
/* Purpose: adapt card-panel app actions to SDL assets, audio and domain apply.
 * Owns: resource-backed map target projection and platform effect timing.
 * Borrows: app card-panel state/session, preview assets and audio service.
 * Tests: test_card_panel_flow.
 */

#include <SDL/SDL.h>

#include "app/card_panel_flow.h"
#include "app/card_use_flow.h"
#include "platform/sdl1/game_loop_turns.h"
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
    if (sound == RICH4_CARD_PANEL_SOUND_OK_CARD || sound == RICH4_CARD_PANEL_SOUND_CARD)
        rich4_audio_play_card(audio);
    if (sound == RICH4_CARD_PANEL_SOUND_TARGET)
        rich4_audio_play_target_confirm(audio);
}

static void card_panel_apply_action(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, CardPanelState *panel,
    const Rich4CardPanelAction *action, Uint32 now, Rich4Audio *audio)
{
    int speech_player = -1;
    Rich4CardPanelCommandResult result;

    if (action != NULL && action->kind == RICH4_CARD_PANEL_ACTION_STOCK_TARGET) {
        (void)rich4_card_use_begin_stock_selection(
            session, panel->player, panel->selected_card, now);
        return;
    }
    if (action != NULL &&
        (action->kind == RICH4_CARD_PANEL_ACTION_ROBBERY_TARGET ||
         (panel->selected_card == 6 && panel->phase == CARD_PANEL_FACILITY_CHOICE && panel->effect_precedes_handler != 2))) {
        int background = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
        const MapData *map = background >= 0 && background < GAME_BACKGROUND_COUNT ?
            &assets->map.maps[background] : NULL;
        GameDomainRef target = rich4_game_domain_ref(panel->primary_kind, panel->primary_target);
        if (panel->selected_card == 12) target = rich4_game_domain_ref(GAME_DOMAIN_PLAYER, panel->primary_target);
        (void)rich4_card_use_begin_selection(session, panel->player, panel->selected_card,
            target, map, panel->phase, now);
        return;
    }
    if (action == NULL || action->kind != RICH4_CARD_PANEL_ACTION_SUBMIT) return;
    if (game_begin_card_use(session, assets, settings, &action->command, 0, now)) {
        panel->phase = CARD_PANEL_WAIT_HANDLER;
        return;
    }
    if (!game_session_apply_command(
            session, assets, settings, &action->command, now)) {
        rich4_card_panel_command_rejected(panel);
        rich4_audio_play_invalid(audio);
        return;
    }
    result = rich4_card_panel_finish_command(
        panel, session, action->target, now, 0,
        &speech_player);
    if (result == RICH4_CARD_PANEL_COMMAND_EFFECT)
        rich4_audio_play_card(audio);
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
        card_panel_apply_action(
            session, assets, settings, panel, &action, now, audio);
    } else if (timed == RICH4_CARD_PANEL_TIMED_MONSTER_SPEECH &&
               speech_player >= 0) {
        game_start_monster_victim_speech(session, speech_player, now);
    }
}

static GameDomainRef card_panel_map_target(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, int player, uint32_t mask, GameDomainRef source,
    int x, int y)
{
    GameSceneView view;
    if (!mask || !build_game_session_render_view(assets, settings, session, &view))
        return rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    int raw = game_map_object_at(assets, settings, &view.game,
        session->presentation.dynamic_objects, x, y, NULL);
    return rich4_card_map_hit(&session->state, player, mask, source, raw);
}

size_t game_card_panel_cursor_chunk_index(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, const CardPanelState *panel,
    int cursor_x, int cursor_y, size_t default_chunk)
{
    static const size_t edge_cursor[5] = {41, 34, 40, 38, 36};
    GameDomainRef source, target;
    uint32_t mask;

    if (assets == NULL || settings == NULL || session == NULL ||
        panel == NULL) return default_chunk;
    if (panel->phase == CARD_PANEL_STOCK_TARGET) return 12;
    if (panel->phase != CARD_PANEL_MAP_TARGET) return default_chunk;
    int direction = rich4_card_panel_edge_scroll_direction(session, panel);
    if (direction) return edge_cursor[direction];
    source = rich4_card_panel_current_property_ref(session);
    mask = rich4_card_map_mask(panel->selected_card, source.kind);
    target = card_panel_map_target(assets, settings, session, panel->player,
        mask, source, cursor_x, cursor_y);
    return target.kind == GAME_DOMAIN_NONE ? 5 : (mask >> 16) & 0xff;
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

static void card_panel_confirm(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, CardPanelState *panel,
    int x, int y, Uint32 now, Rich4Audio *audio, int release)
{
    GameDomainRef target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    GameDomainRef projected_god = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    int projected_god_type = 0;
    int god_projection_available = 0;
    Rich4CardPanelAction action;

    if (panel == NULL || session == NULL ||
        panel->phase == CARD_PANEL_CLOSED ||
        panel->phase == CARD_PANEL_EFFECT ||
        panel->phase == CARD_PANEL_MONSTER_ANIMATION ||
        panel->phase == CARD_PANEL_DEMOLISH_ANIMATION)
        return;
    if (panel->phase == CARD_PANEL_MAP_TARGET) {
        if (rich4_card_panel_edge_scroll_direction(session, panel)) return;
        GameDomainRef source = rich4_card_panel_current_property_ref(session);
        target = card_panel_map_target(assets, settings, session, panel->player,
            rich4_card_map_mask(panel->selected_card, source.kind), source, x, y);
    }
    if (panel->phase == CARD_PANEL_INVENTORY && panel->player >= 0 &&
        panel->player < session->state.player_count) {
        int slot = release ? panel->selected_slot : rich4_card_panel_slot_at(x, y);
        if (slot >= 0 && slot < GAME_PLAYER_CARD_SLOTS &&
            session->state.players[panel->player].card_inventory[slot] == 23)
            god_projection_available = card_panel_projected_god_target(
                session, assets, settings, panel->player,
                &projected_god, &projected_god_type);
    }
    if (release && (panel->phase == CARD_PANEL_INVENTORY || panel->phase == CARD_PANEL_ROBBERY_TARGET))
        action = rich4_card_panel_release_slot(panel, session, god_projection_available,
            projected_god, projected_god_type, now);
    else if (god_projection_available)
        action = rich4_card_panel_pointer_action_with_god_target(
            panel, session, x, y, target, 0, now, projected_god, projected_god_type);
    else
        action = rich4_card_panel_pointer_action(panel, session, x, y, target, 0, now);
    card_panel_play_sound(audio, action.sound);
    card_panel_apply_action(
        session, assets, settings, panel, &action, now, audio);
}

void game_card_panel_confirm(GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, CardPanelState *panel, int x, int y, Uint32 now, Rich4Audio *audio)
{
    card_panel_confirm(session, assets, settings, panel, x, y, now, audio, 0);
}

void game_card_panel_cancel_input(
    const GameSession *session, CardPanelState *panel, Rich4Audio *audio)
{
    CardPanelPhase phase = panel->phase;
    if (rich4_card_panel_edge_scroll_direction(session, panel)) return;
    if (!rich4_card_panel_cancel(panel) || phase == CARD_PANEL_INVENTORY) return;
    if (phase == CARD_PANEL_MAP_TARGET || phase == CARD_PANEL_FACILITY_CHOICE || phase == CARD_PANEL_STOCK_TARGET)
        rich4_audio_play_cancel(audio);
    rich4_audio_play_invalid(audio);
}

int game_card_panel_button(GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, VirtualCursor *cursor, int button, int release, Rich4Audio *audio)
{
    CardPanelState *panel = &session->presentation.card_panel;
    if (panel->phase == CARD_PANEL_CLOSED || panel->phase == CARD_PANEL_WAIT_HANDLER ||
        (button != SDL_BUTTON_LEFT && button != SDL_BUTTON_RIGHT)) return 0;
    cursor->confirm_pressed = cursor->confirm_pointer_pressed = cursor->cancel_pressed = 0;
    if (!release) {
        if (button == SDL_BUTTON_LEFT && rich4_card_panel_press_slot(panel, session, cursor->x, cursor->y) &&
            panel->phase == CARD_PANEL_INVENTORY) rich4_audio_play_ok(audio);
        return 1;
    }
    Uint32 now = rich4_sdl1_ticks();
    if (rich4_card_use_skip_notice(session, now)) return 1;
    if (button == SDL_BUTTON_RIGHT) game_card_panel_cancel_input(session, panel, audio);
    else card_panel_confirm(session, assets, settings, panel, cursor->x, cursor->y, now, audio, 1);
    return 1;
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
    card_panel_apply_action(
        session, assets, settings, panel, &action, now, audio);
}
