/* Purpose: own special-event presentation timeline transitions.
 * Owns: reaction/consequence timing, callback and route-interaction state.
 * Borrows: GameSession domain/presentation state and immutable event facts.
 * Allowed deps: app state, core events and immutable character speech data.
 * Invariant: one timeline transition updates all related deadline fields.
 * Tests: test_special_presentation_flow.
 */
#include "app/special_event_timeline.h"
#include "app/insurance_flow.h"
#include "app/game_speech_flow.h"
#include "data/character_speech.h"

#include "app/game_presentation.h"
#include "core/confinement.h"
#include "core/card.h"
#include "core/game_constants.h"
#include "core/special_event.h"

static int positive_delay(int delay_ms)
{
    return delay_ms > 0 ? delay_ms : 71;
}

static void clear_terminal_special_event_view(GameSession *session)
{
    const GameSpecialEventPresentationState *special;

    if (session == NULL)
        return;
    special = &session->presentation.special_event;

    /* News callback 4 (the multi-victim alien attack) calls the original
     * camera reset after its last victim.  Other callbacks leave their
     * target camera in place until the turn finishes. */
    if (special->family == GAME_SPECIAL_EVENT_NEWS &&
        special->resolved_id == 4)
        session->presentation.has_view_override = 0;
}

static int next_fortune_card_target(const GameSession *session)
{
    const GameSpecialEventPresentationState *special;
    int player;
    if (session == NULL) return -1;
    special = &session->presentation.special_event;
    for (player = 0; player < session->state.player_count; player++)
        if (special->fortune_card_target_mask & (1u << player))
            return player;
    return -1;
}

int rich4_special_event_begin_fortune_card_targets(
    GameSession *session, uint32_t now)
{
    GameSpecialEventPresentationState *special;
    int player;
    if (session == NULL) return 0;
    special = &session->presentation.special_event;
    if (special->family != GAME_SPECIAL_EVENT_FORTUNE ||
        special->resolved_id != 5 || special->fortune_card_target_mask == 0)
        return 0;
    player = next_fortune_card_target(session);
    if (player < 0) return 0;
    special->fortune_card_target_player = player;
    special->phase = RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET;
    special->fortune_choice_hover = -1;
    special->until = now;
    return 1;
}

int rich4_special_event_advance_fortune_card_target(
    GameSession *session, uint32_t now)
{
    GameSpecialEventPresentationState *special;
    int player;
    if (session == NULL) return 0;
    special = &session->presentation.special_event;
    player = special->fortune_card_target_player;
    if (player >= 0 && player < session->state.player_count)
        special->fortune_card_target_mask &= ~(1u << player);
    player = next_fortune_card_target(session);
    if (player < 0) {
        special->fortune_card_target_player = -1;
        return 0;
    }
    special->fortune_card_target_player = player;
    special->fortune_choice_hover = -1;
    special->until = now;
    return 1;
}

static int fortune_passive_actor(const GameSession *session)
{
    const GameSpecialEventPresentationState *s=&session->presentation.special_event;
    int i;
    if (s->family==GAME_SPECIAL_EVENT_NEWS) {
        for (i=0;i<session->state.player_count;i++)
            if (s->target_mask & (1u<<i)) return i;
        return -1;
    }
    return s->actor;
}

static void fortune_choices_finished(GameSpecialEventPresentationState *s, uint32_t now)
{
    s->fortune_choices_done=1;
    s->phase=RICH4_SPECIAL_PRESENT_PAGE;
    s->until=now;
}

static int fortune_passive_speech(GameSession *session, int redirect, uint32_t now)
{
    GameSpecialEventPresentationState *s=&session->presentation.special_event;
    GameEventQueue events;
    int actor=fortune_passive_actor(session), victim;
    rich4_game_event_queue_reset(&events);
    victim=rich4_special_event_resolve_passive_target(&session->state,actor,redirect,&events);
    if (victim==-2) return 0;
    s->fortune_redirect_target=victim;
    s->fortune_passive_resolved=1;
    if (victim>=0) s->reaction_player=victim;
    s->phase=RICH4_SPECIAL_PRESENT_PASSIVE_ACTOR_SPEECH;
    (void)game_start_speech_entry(session,actor,
        s->fortune_passive_card==20 ?
            rich4_character_absolution_speech(session->state.players[actor].character) :
            rich4_character_insurance_card_speech(session->state.players[actor].character,0),
        0,GAME_PROPERTY_PRESENT_NONE,now);
    return 1;
}

/* 0x44B896 -> notice -> 0x441210 -> card notice/choice -> consume ->
 * actor/victim speech -> callback effect. All players share this order. */
int rich4_special_event_prepare_choices(GameSession *session, uint32_t now)
{
    GameSpecialEventPresentationState *s;
    int actor, human;
    if (session==NULL) return 0;
    s=&session->presentation.special_event;
    if ((s->family!=GAME_SPECIAL_EVENT_FORTUNE &&
         !(s->family==GAME_SPECIAL_EVENT_NEWS && s->resolved_id==29)) ||
        s->fortune_choices_done) return 0;
    actor=fortune_passive_actor(session);
    if (actor<0 || actor>=session->state.player_count) return 0;
    human=(session->state.players[actor].play_state & 0xff)==1;
    if (!s->fortune_defense_ready) {
        s->defense=rich4_special_event_default_defense(&session->state);
        s->fortune_defense_ready=1;
    }
    switch (s->phase) {
    case RICH4_SPECIAL_PRESENT_GOD_NOTICE:
        s->phase=RICH4_SPECIAL_PRESENT_PAGE;
        break;
    case RICH4_SPECIAL_PRESENT_PASSIVE_CARD:
        if (s->fortune_passive_card==20)
            return fortune_passive_speech(session,-1,now);
        s->phase=human ? RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET :
            RICH4_SPECIAL_PRESENT_REDIRECT_NOTICE;
        s->until=now+1500;
        return 1;
    case RICH4_SPECIAL_PRESENT_REDIRECT_NOTICE:
        return fortune_passive_speech(session,s->fortune_redirect_target,now);
    case RICH4_SPECIAL_PRESENT_PASSIVE_ACTOR_SPEECH:
        if (s->fortune_passive_card==18 && s->fortune_redirect_target>=0) {
            int victim=s->fortune_redirect_target;
            s->phase=RICH4_SPECIAL_PRESENT_PASSIVE_VICTIM_SPEECH;
            (void)game_start_speech_entry(session,victim,
                rich4_character_insurance_card_speech(session->state.players[victim].character,2),
                2,GAME_PROPERTY_PRESENT_NONE,now);
            return 1;
        }
        fortune_choices_finished(s,now);
        return 0;
    case RICH4_SPECIAL_PRESENT_PASSIVE_VICTIM_SPEECH:
        fortune_choices_finished(s,now);
        return 0;
    default: break;
    }
    if (!s->fortune_god_notice_done) {
        s->fortune_god_notice_done=1;
        if (s->family==GAME_SPECIAL_EVENT_FORTUNE && s->defense!=GAME_SPECIAL_DEFENSE_NONE &&
            (s->defense!=GAME_SPECIAL_DEFENSE_DOUBLED ||
             !(s->resolved_id==3 || s->resolved_id==8 || s->resolved_id==9 ||
               s->resolved_id==10 || s->resolved_id==11 || s->resolved_id==32))) {
            s->phase=RICH4_SPECIAL_PRESENT_GOD_NOTICE;
            s->until=now+1500;
            return 1;
        }
    }
    if (s->family==GAME_SPECIAL_EVENT_FORTUNE && s->resolved_id==5 &&
        rich4_special_event_begin_fortune_card_targets(session,now)) return 1;
    if ((s->family==GAME_SPECIAL_EVENT_NEWS ||
         rich4_special_event_fortune_has_target(s->resolved_id)) &&
        s->defense!=GAME_SPECIAL_DEFENSE_BLOCKED) {
        if (rich4_card_inventory_has(&session->state,actor,20))
            s->fortune_passive_card=20;
        else if (rich4_card_inventory_has(&session->state,actor,18)) {
            s->fortune_passive_card=18;
            if (!human) {
                s->fortune_redirect_target=rich4_special_event_fortune_choose_redirect(&session->state,actor);
                if (s->fortune_redirect_target<0) {
                    /* No notice, no card use, and no second AI target draw. */
                    s->fortune_passive_resolved=1;
                    s->fortune_redirect_target=actor;
                    fortune_choices_finished(s,now);
                    return 0;
                }
            }
        }
        if (s->fortune_passive_card>=0) {
            s->phase=RICH4_SPECIAL_PRESENT_PASSIVE_CARD;
            s->until=now+1500;
            rich4_audio_event_emit(&session->audio_events,RICH4_AUDIO_EVENT_CARD,1,0);
            return 1;
        }
    }
    fortune_choices_finished(s,now);
    return 0;
}

int rich4_special_event_choice_select(GameSession *session, int choice, uint32_t now)
{
    GameSpecialEventPresentationState *s;
    if (session == NULL) return 0;
    s = &session->presentation.special_event;
    if (!s->active) return 0;
    if (s->phase == RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET) {
        if (!rich4_special_event_fortune_take_card(
                &session->state,s->fortune_card_target_player,choice)) return 0;
        s->fortune_card_targets_seen++;
        if (rich4_special_event_advance_fortune_card_target(session,now)) return 1;
    } else if (s->phase == RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET) {
        int actor = s->family == GAME_SPECIAL_EVENT_NEWS ? s->reaction_player : s->actor;
        if (choice != -1 && (choice < 0 || choice >= session->state.player_count ||
            choice == actor || !(session->state.players[choice].play_state & 0xff))) return 0;
        if (choice>=0) return fortune_passive_speech(session,choice,now);
        s->fortune_redirect_target = choice;
    } else return 0;
    s->fortune_choices_done = 1;
    s->phase = RICH4_SPECIAL_PRESENT_PAGE;
    s->until = now;
    return 1;
}

int rich4_special_event_choice_hover(GameSession *session, int choice)
{
    GameSpecialEventPresentationState *s;
    if (session == NULL) return 0;
    s = &session->presentation.special_event;
    if (!s->active || (s->phase != RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET &&
        s->phase != RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET)) return 0;
    /* 0x4413EC has no mouse-move selection; the card changes on left-down. */
    if (s->phase == RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET)
        s->fortune_choice_hover = choice;
    return 1;
}

int rich4_special_event_choice_press(GameSession *session, int choice)
{
    GameSpecialEventPresentationState *s;
    if (session == NULL) return 0;
    s = &session->presentation.special_event;
    if (!s->active || s->phase != RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET) return 0;
    if (choice >= 0 && choice < GAME_PLAYER_CARD_SLOTS &&
        session->state.players[s->fortune_card_target_player].card_inventory[choice])
        s->fortune_choice_hover = choice;
    return 1;
}

const GamePlayerState *rich4_special_event_original_player(
    const GameSession *session, int player)
{
    if (session == NULL || player < 0 ||
        player >= session->state.player_count)
        return NULL;
    if (session->presentation.special_event.original_players_valid)
        return &session->presentation.special_event.original_players[player];
    if (session->presentation.special_event.pre_state_valid)
        return &session->presentation.special_event.pre_state.players[player];
    return &session->state.players[player];
}

int rich4_special_event_prepare_next_news_victim(GameSession *session)
{
    unsigned int staged_mask;
    int staged_player;
    int player;
    GameSpecialEventPresentationState *special;

    if (session == NULL) return 0;
    special = &session->presentation.special_event;
    if (special->family != GAME_SPECIAL_EVENT_NEWS ||
        special->resolved_id != 4)
        return 0;
    for (player = 0; player < session->state.player_count; player++)
        if (special->reaction_mask & (1u << player)) {
            special->reaction_mask &= ~(1u << player);
            special->reaction_player = player;
            special->confinement = GAME_CONFINEMENT_HOSPITAL;
            if (special->original_players_valid) {
                /* The original callback stages one victim at a time while
                 * the core has already committed all pending victims. */
                special->pre_state = session->state;
                staged_mask = special->reaction_mask | (1u << player);
                for (staged_player = 0;
                     staged_player < session->state.player_count;
                     staged_player++)
                    if (staged_mask & (1u << staged_player))
                        special->pre_state.players[staged_player] =
                            special->original_players[staged_player];
                special->pre_state_valid = 1;
            }
            rich4_special_event_set_first_confinement(
                session, rich4_special_event_original_player(session, player));
            special->reaction_stage = 0;
            return 1;
        }
    return 0;
}

void rich4_special_event_set_reaction_stage(
    GameSession *session, int reaction_stage)
{
    if (session == NULL) return;
    session->presentation.special_event.reaction_stage = reaction_stage;
    session->presentation.special_event.phase =
        RICH4_SPECIAL_PRESENT_REACTION;
}

void rich4_special_event_set_first_confinement(
    GameSession *session, const GamePlayerState *before)
{
    GameSpecialEventPresentationState *special;
    if (session == NULL) return;
    special = &session->presentation.special_event;
    if (before == NULL) {
        special->first_confinement = 1;
        return;
    }
    special->first_confinement = special->confinement ==
        GAME_CONFINEMENT_PRISON
        ? (before->days_in_prison & 0x7f) == 0
        : special->confinement == GAME_CONFINEMENT_HOSPITAL
            ? (before->days_in_hospital & 0x7f) == 0
            : 0;
}

int rich4_special_event_begin_timed_phase(
    GameSession *session, int phase, int frame_count, int delay_ms,
    uint32_t now, int scene_refresh_frame)
{
    GameSpecialEventPresentationState *special;
    int delay;
    if (session == NULL || !session->presentation.special_event.active ||
        frame_count <= 0)
        return 0;
    delay = positive_delay(delay_ms);
    special = &session->presentation.special_event;
    special->phase = phase;
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_SPECIAL]);
    special->frame = 0;
    special->tick = now;
    special->until = now + (uint32_t)(frame_count * delay);
    special->scene_refresh_frame = scene_refresh_frame;
    special->scene_refreshed = 0;
    return 1;
}

int rich4_special_event_begin_consequence(
    GameSession *session, int logical_id, int consequence_index,
    int frame_count, int delay_ms, uint32_t now)
{
    int scene_refresh_frame = 0;
    if (logical_id == 4) scene_refresh_frame = 24;
    else if (logical_id == 5) scene_refresh_frame = 32;
    else if (logical_id == 15) scene_refresh_frame = 5;
    else if (logical_id == 20 || logical_id == 21)
        scene_refresh_frame = 8;
    if (!rich4_special_event_begin_timed_phase(
            session, RICH4_SPECIAL_PRESENT_CONSEQUENCE,
            frame_count, delay_ms, now, scene_refresh_frame))
        return 0;
    session->presentation.special_event.consequence_index =
        consequence_index;
    return 1;
}

void rich4_special_event_update_timeline(
    GameSession *session, int frame_count, int delay_ms, uint32_t now)
{
    GameSpecialEventPresentationState *special;
    int frame;
    if (session == NULL || !session->presentation.special_event.active ||
        frame_count <= 0)
        return;
    special = &session->presentation.special_event;
    delay_ms = positive_delay(delay_ms);
    if (rich4_animation_clock_step(
            &session->presentation.animation_clock[RICH4_ANIMATION_SPECIAL],
            now, delay_ms) && special->frame < frame_count)
        special->frame++;
    frame = special->frame;
    special->tick = session->presentation.animation_clock[RICH4_ANIMATION_SPECIAL].tick;
    if (special->phase != RICH4_SPECIAL_PRESENT_PAGE)
        special->until = special->tick + (uint32_t)((frame_count - frame) * delay_ms);
    if (special->scene_refreshed || special->scene_refresh_frame <= 0 ||
        frame + 1 < special->scene_refresh_frame)
        return;
    special->scene_refreshed = 1;
    if (special->phase == RICH4_SPECIAL_PRESENT_CONFINEMENT_ANIMATION &&
        special->family == GAME_SPECIAL_EVENT_NEWS &&
        special->resolved_id == 4 && special->pre_state_valid &&
        special->reaction_player >= 0 &&
        special->reaction_player < session->state.player_count)
        special->pre_state.players[special->reaction_player] =
            session->state.players[special->reaction_player];
}

void rich4_special_event_mark_scene_refreshed(GameSession *session)
{
    if (session != NULL)
        session->presentation.special_event.scene_refreshed = 1;
}

int rich4_special_event_start_reaction_delay(
    GameSession *session, uint32_t now)
{
    int id;
    GameSpecialEventPresentationState *special;
    if (session == NULL) return 0;
    special = &session->presentation.special_event;
    id = special->resolved_id;
    if (special->family == GAME_SPECIAL_EVENT_NEWS) {
        if (id != 15 && id != 19 && id != 21) return 0;
    } else if (special->family == GAME_SPECIAL_EVENT_FORTUNE) {
        if (id != 0 && id != 1) return 0;
    } else {
        return 0;
    }
    special->phase = RICH4_SPECIAL_PRESENT_REACTION_DELAY;
    special->until = now + 300;
    return 1;
}

int rich4_special_event_finish_callback(GameSession *session, uint32_t now)
{
    GameSpecialEventPresentationState *special;
    if (session == NULL) return 0;
    if (rich4_insurance_flow_resume_special_claim(session, now)) return 1;
    special = &session->presentation.special_event;
    if (special->family == GAME_SPECIAL_EVENT_FORTUNE) {
        special->phase = RICH4_SPECIAL_PRESENT_POST_HOLD;
        special->until = now + 800;
        return 1;
    }
    clear_terminal_special_event_view(session);
    special->active = 0;
    return 0;
}

void rich4_special_event_begin_hold(GameSession *session, uint32_t now)
{
    if (session == NULL) return;
    session->presentation.special_event.phase =
        RICH4_SPECIAL_PRESENT_HOLD;
    session->presentation.special_event.until = now + 800;
}

void rich4_special_event_center_callback_view(GameSession *session)
{
    GameDomainRef target;
    GameSpecialEventPresentationState *special;
    int world_x;
    int world_y;
    int id;
    int property_callback = 0;

    if (session == NULL) return;
    special = &session->presentation.special_event;
    id = special->resolved_id;
    if (special->family == GAME_SPECIAL_EVENT_FORTUNE)
        property_callback = id == 0 || id == 1;
    else if (special->family == GAME_SPECIAL_EVENT_NEWS)
        property_callback = id == 4 || id == 5 || id == 6 || id == 14 ||
            id == 15 || id == 18 || id == 19 || id == 20 || id == 21;
    if (!property_callback || !rich4_special_event_decode_property_target(
            &session->state, special->target_mask, &target))
        return;
    if (target.kind == GAME_DOMAIN_FACILITY) {
        world_x = session->state.tool_facility_x[target.id];
        world_y = session->state.tool_facility_y[target.id];
    } else {
        world_x = session->state.tool_land_x[target.id];
        world_y = session->state.tool_land_y[target.id];
    }
    session->presentation.view_world_x = world_x;
    session->presentation.view_world_y = world_y;
    session->presentation.has_view_override = 1;
}

int rich4_special_event_prepare_news_area_view(
    GameSession *session, uint32_t now, int speech_playing)
{
    GameSpecialEventPresentationState *special;
    if (session == NULL) return 0;
    special = &session->presentation.special_event;
    if (!special->active || special->family != GAME_SPECIAL_EVENT_NEWS ||
        (special->resolved_id != 4 && special->resolved_id != 20) ||
        special->phase != RICH4_SPECIAL_PRESENT_PAGE || speech_playing ||
        (int32_t)(now - special->until) < 0) return 0;
    if (!rich4_special_event_prepare_news_phase1(&session->state)) return 0;
    special->target_mask = session->state.special_event_target_mask;
    rich4_special_event_center_callback_view(session);
    return 1;
}

void rich4_special_event_deactivate(GameSession *session)
{
    GameSpecialEventPresentationState *special;

    if (session == NULL)
        return;

    special = &session->presentation.special_event;

    clear_terminal_special_event_view(session);
    special->active = 0;
}

void rich4_special_event_complete_route_interaction(GameSession *session)
{
    if (session != NULL)
        session->presentation.route_interaction.active = 0;
}

void rich4_special_event_restore_route_interaction(
    GameSession *session, const GameCommand *command)
{
    if (session == NULL || command == NULL) return;
    session->presentation.special_event.active = 0;
    session->presentation.route_interaction.accept = *command;
    session->presentation.route_interaction.decline = *command;
    session->presentation.route_interaction.active = 1;
    session->presentation.route_interaction.pressed = -1;
}
