#include "app/special_presentation_flow.h"

#include <string.h>

#include "app/game_presentation.h"
#include "app/game_speech_flow.h"
#include "core/card.h"
#include "core/tool.h"
#include "core/character_speech_rules.h"
#include "core/game_event_payload.h"
#include "core/property.h"
#include "core/rng.h"
#include "core/route_event_executor.h"
#include "data/character_speech.h"

static int positive_delay(int delay_ms)
{
    return delay_ms > 0 ? delay_ms : 71;
}

static void clear_route_interaction(GameSession *session, GameEventQueue *events)
{
    GameEvent event;
    while (rich4_game_event_queue_pop(events, &event)) {
        if (event.type == GAME_EVENT_INVENTORY_CHANGED &&
            rich4_game_event_inventory_subject(&event).kind == GAME_DOMAIN_CARD)
            session->presentation.special_reward.card =
                rich4_game_event_inventory_subject(&event).id;
        else if (event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED)
            session->presentation.route_interaction.active = 0;
    }
}

int rich4_road_reward_skip(GameSession *session, uint32_t now)
{
    if (session == NULL || !session->presentation.special_reward.active ||
        (session->presentation.special_reward.action != 14 &&
         session->presentation.special_reward.action != 15)) return 0;
    /* 0x440CAC -> 0x4528B9 consumes key/left/right release. */
    session->presentation.special_reward.until = now;
    return 1;
}

/* 0x41B8F9: dynamic type 13. Action 15 is a presentation-only tag,
 * distinct from fixed route action 13 (CARD) and road chest tag 14. */
int rich4_road_gift_begin(GameSession *session, int player, uint32_t now)
{
    GameMapDynamicObjectState *gift;
    GameSpecialRewardPresentationState *reward;
    int total = 0;
    int tool;
    int pick;
    if (session == NULL || player < 0 || player >= session->state.player_count ||
        session->presentation.special_reward.active) return 0;
    gift = &session->presentation.dynamic_objects[12];
    if (gift->type != 13 || gift->node == 0 ||
        gift->node != session->state.players[player].node) return 0;
    /* 0x445ADA weights all eight public stocks, including held-at-cap tools.
     * 0x445A4D silently declines the grant at nine, but pickup continues. */
    for (tool = 0; tool < 8; ++tool) total += session->state.tool_amount[tool];
    if (total == 0) return 0;
    pick = rich4_rng_range_at(&session->state.ai_rng_state, total);
    for (tool = 0; tool < 8; ++tool) {
        if (pick < session->state.tool_amount[tool]) break;
        pick -= session->state.tool_amount[tool];
    }
    if (session->state.players[player].tool_inventory[tool] < 9) {
        --session->state.tool_amount[tool];
        ++session->state.players[player].tool_inventory[tool];
    }
    rich4_audio_event_emit(&session->audio_events,
                          RICH4_AUDIO_EVENT_ROAD_GIFT, 1, 0);
    memset(gift, 0, sizeof(*gift));
    gift->type = 13;
    reward = &session->presentation.special_reward;
    memset(reward, 0, sizeof(*reward));
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_REWARD]);
    reward->active = 1;
    reward->action = 15;
    reward->player = player;
    reward->card = tool;
    reward->phase = 1;
    reward->tick = now;
    reward->until = now + 1500;
    return 1;
}

int rich4_road_chest_begin(GameSession *session, const MapData *map,
                          int player, uint32_t now)
{
    GameSpecialRewardPresentationState *reward;
    GameMapDynamicObjectState *chest;
    if (session == NULL || player < 0 || player >= session->state.player_count)
        return 0;
    chest = &session->presentation.dynamic_objects[13];
    if (chest->type != 14 || chest->node == 0 ||
        chest->node != session->state.players[player].node ||
        session->presentation.special_reward.active)
        return 0;
    /* 0x41BB29..0x41BB90: Effect[36], remove, refresh, 1500 ms message,
     * then add 500 to the uint16 point field and speak table entry zero. */
    rich4_audio_event_emit(&session->audio_events,
                          RICH4_AUDIO_EVENT_ROAD_CHEST, 1, 0);
    /* 0x40E14D preserves the fixed type byte and does not respawn slots
     * 13/14 until the month rollover (0x41CF67). */
    (void)map;
    memset(chest, 0, sizeof(*chest));
    chest->type = 14;
    reward = &session->presentation.special_reward;
    memset(reward, 0, sizeof(*reward));
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_REWARD]);
    reward->active = 1;
    reward->action = 14;
    reward->player = player;
    reward->phase = 1;
    reward->tick = now;
    reward->until = now + 1500;
    return 1;
}

void rich4_special_reward_begin(
    GameSession *session, const GameEvent *event, int frame_count,
    int delay_ms, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    GameSpecialRewardPresentationState *reward;
    int action;
    if (session == NULL || event == NULL ||
        event->player < 0 || event->player >= session->state.player_count)
        return;
    action = rich4_game_event_special_place_action(event);
    if (action < 10 || action > 13) return;
    reward = &session->presentation.special_reward;
    memset(reward, 0, sizeof(*reward));
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_REWARD]);
    reward->active = 1;
    reward->action = action;
    reward->player = event->player;
    reward->card = -1;
    reward->sound_played = 1;
    reward->tick = now;
    reward->until = now + (uint32_t)(
        (frame_count > 0 ? frame_count : 1) * positive_delay(delay_ms));
    if (audio_events != NULL && frame_count > 0)
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_SPECIAL_REWARD_ANIMATION,
                              1, action == 13 ? 99 : 98);
}

int rich4_special_reward_update(
    GameSession *session, int frame_count, int delay_ms, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    GameSpecialRewardPresentationState *reward;
    int action;
    int player;
    int delay;
    if (session == NULL || !session->presentation.special_reward.active)
        return 0;
    reward = &session->presentation.special_reward;
    action = reward->action;
    player = reward->player;
    if (action == 15) {
        const GameToolInfo *tool;
        if ((int32_t)(now - reward->until) < 0) return 1;
        reward->active = 0;
        tool = rich4_tool_info(reward->card);
        if (tool != NULL)
            game_start_speech_entry(session, player,
                rich4_character_shop_gift_speech(
                    session->state.players[player].character, tool->shop_price,
                    tool->shop_price > 50 && tool->shop_price <= 100
                        ? (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 1u) : 0),
                0, GAME_PROPERTY_PRESENT_NONE, now);
        return 1;
    }
    if (action == 14) {
        if ((int32_t)(now - reward->until) < 0) return 1;
        reward->active = 0;
        session->state.players[player].points = (uint16_t)(
            session->state.players[player].points + 500u);
        game_start_speech_entry(session, player,
            rich4_character_reward_speech(
                session->state.players[player].character, 1, 0),
            0, GAME_PROPERTY_PRESENT_NONE, now);
        return 1;
    }
    delay = positive_delay(delay_ms);
    if (reward->phase == 0) {
        if (frame_count > 0 && rich4_animation_clock_step(
                &session->presentation.animation_clock[RICH4_ANIMATION_REWARD],
                now, delay)) reward->frame++;
        reward->tick = session->presentation.animation_clock[RICH4_ANIMATION_REWARD].tick;
        reward->until = reward->tick + (uint32_t)((frame_count - reward->frame) * delay);
        if (reward->frame < frame_count) return 1;
        if (action != 13) {
            reward->phase = 1;
            reward->frame = frame_count > 0 ? frame_count - 1 : 0;
            reward->until = now + 1000;
            return 1;
        }
    }
    if (reward->phase == 1 && (int32_t)(now - reward->until) < 0)
        return 1;
    if (reward->phase <= 1) {
        GameEventQueue events;
        RouteEventExecutionResult result;
        rich4_game_event_queue_reset(&events);
        result = rich4_route_event_executor_resume_special_place(
            &session->flow.route_events, &session->state, &events);
        if (result == ROUTE_EVENT_EXECUTION_INVALID) {
            reward->active = 0;
            return 0;
        }
        clear_route_interaction(session, &events);
        if (action == 13 && reward->card >= 0) {
            reward->phase = 2;
            reward->until = now + 1500;
            if (audio_events != NULL)
                rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_CARD,
                                       1, 0);
            return 1;
        }
        reward->active = 0;
        if ((action == 10 || action == 11) && player >= 0 &&
            player < session->state.player_count) {
            const Rich4CharacterSpeech *speech = rich4_character_reward_speech(
                session->state.players[player].character, action == 10,
                (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 1u));
            game_start_speech_entry(session, player, speech, 0,
                                    GAME_PROPERTY_PRESENT_NONE, now);
        }
        return 0;
    }
    if ((int32_t)(now - reward->until) < 0) return 1;
    reward->active = 0;
    if (action == 13 && reward->card >= 0 && player >= 0 &&
        player < session->state.player_count) {
        const GameCardInfo *card = rich4_card_info(reward->card);
        if (card != NULL) {
            const Rich4CharacterSpeech *speech = rich4_character_shop_gift_speech(
                session->state.players[player].character, card->shop_price,
                card->shop_price > 50 && card->shop_price <= 100
                    ? (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 1u) : 0);
            game_start_speech_entry(session, player, speech, 0,
                                    GAME_PROPERTY_PRESENT_NONE, now);
        }
    }
    return 0;
}

void rich4_bank_rejection_begin(
    GameSession *session, const GameEvent *event, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    GameBankRejectionPresentationState *rejection;
    int days;
    int duration;
    if (session == NULL || event == NULL) return;
    days = rich4_game_event_bank_rejection_days(event);
    if (days <= 0) return;
    rejection = &session->presentation.bank_rejection;
    rejection->active = 1;
    rejection->days = days;
    duration = rich4_game_event_bank_rejection_duration(event);
    rejection->until = now + (uint32_t)(duration > 0 ? duration : 1000);
    (void)audio_events;
}

int rich4_bank_rejection_update(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    GameBankRejectionPresentationState *rejection;
    GameEventQueue events;
    GameEvent event;
    RouteEventExecutionResult result;
    (void)audio_events;
    if (session == NULL || !session->presentation.bank_rejection.active)
        return 0;
    rejection = &session->presentation.bank_rejection;
    if ((int32_t)(now - rejection->until) < 0) return 1;
    rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_finish_bank_rejection(
        &session->flow.route_events, &session->state, &events);
    if (result == ROUTE_EVENT_EXECUTION_INVALID) return 1;
    while (rich4_game_event_queue_pop(&events, &event))
        if (event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED)
            session->presentation.route_interaction.active = 0;
    rejection->active = 0;
    rejection->days = 0;
    return 0;
}

int rich4_company_issue_control_notice(GameSession *session, const GameEvent *event)
{
    int company = rich4_game_event_company_control_company(event);
    GameCompanyIssuePresentationState *issue;
    if (session == NULL || company < 0 || company >= session->state.company_count)
        return 0;
    issue = &session->presentation.company_issue;
    memset(issue, 0, sizeof(*issue));
    issue->active = 1;
    issue->phase = 2;
    issue->player = event->player;
    issue->company = company;
    return 1;
}

int rich4_company_issue_update(GameSession *session, uint32_t now)
{
    GameCompanyIssuePresentationState *issue;
    if (session == NULL) return 0;
    issue = &session->presentation.company_issue;
    if (!issue->active) return 0;
    if (issue->phase == 2) {
        if (!issue->notice_until) issue->notice_until = now + 1500u;
        if ((int32_t)(now - issue->notice_until) >= 0)
            memset(issue, 0, sizeof(*issue));
    }
    return issue->active;
}

int rich4_company_issue_skip_notice(GameSession *session, uint32_t now)
{
    if (session == NULL || !session->presentation.company_issue.active ||
        session->presentation.company_issue.phase != 2) return 0;
    /* 0x41D2AA -> 0x440CAC -> 0x4528B9 accepts releases for AI too. */
    session->presentation.company_issue.notice_until = now ? now : 1;
    return 1;
}

void rich4_company_issue_begin(GameSession *session, const GameEvent *event)
{
    GameCompanyIssuePresentationState *issue;
    if (session == NULL || event == NULL ||
        rich4_game_event_company_issue_company(event) < 0 ||
        rich4_game_event_company_issue_price(event) <= 0 ||
        rich4_game_event_company_issue_maximum(event) <= 0)
        return;
    issue = &session->presentation.company_issue;
    memset(issue, 0, sizeof(*issue));
    issue->active = 1;
    issue->player = event->player;
    issue->company = rich4_game_event_company_issue_company(event);
    issue->stock = rich4_game_event_company_issue_stock(event).id;
    issue->price = rich4_game_event_company_issue_price(event);
    issue->maximum = rich4_game_event_company_issue_maximum(event);
    issue->quantity = issue->maximum;
    session->presentation.route_interaction.active = 0;
}

int rich4_company_issue_finish(GameSession *session, int quantity)
{
    GameEventQueue events;
    GameEvent event;
    RouteEventExecutionResult result;
    if (session == NULL || !session->presentation.company_issue.active ||
        session->presentation.company_issue.phase == 2)
        return 0;
    rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_buy_company_issue(
        &session->flow.route_events, &session->state, quantity, &events);
    if (result == ROUTE_EVENT_EXECUTION_INVALID) return 0;
    memset(&session->presentation.company_issue, 0,
           sizeof(session->presentation.company_issue));
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED)
            session->presentation.route_interaction.active = 0;
        else if (!rich4_company_issue_control_notice(session, &event))
            (void)rich4_game_event_queue_push(&session->deferred_events, &event);
    }
    return 1;
}

int rich4_company_issue_input(
    GameSession *session, Rich4CompanyIssueInput input, int digit,
    Rich4AudioEventQueue *audio_events)
{
    GameCompanyIssuePresentationState *issue;
    long next;
    if (session == NULL || !session->presentation.company_issue.active ||
        session->presentation.company_issue.phase != 1)
        return 0;
    issue = &session->presentation.company_issue;
    switch (input) {
    case RICH4_COMPANY_ISSUE_DIGIT:
        if (digit < 0 || digit > 9) return 0;
        next = (long)issue->quantity * 10 + digit;
        issue->quantity = next > issue->maximum ? issue->maximum : (int)next;
        if (audio_events != NULL)
            rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_BANK_KEY,
                                   1, 0);
        return 1;
    case RICH4_COMPANY_ISSUE_BACKSPACE:
        issue->quantity /= 10;
        break;
    case RICH4_COMPANY_ISSUE_CLEAR:
        issue->quantity = 0;
        break;
    case RICH4_COMPANY_ISSUE_MAX:
        issue->quantity = issue->maximum;
        break;
    case RICH4_COMPANY_ISSUE_DECREMENT:
        if (issue->quantity > 0) issue->quantity--;
        break;
    case RICH4_COMPANY_ISSUE_INCREMENT:
        if (issue->quantity < issue->maximum) issue->quantity++;
        break;
    default:
        return 0;
    }
    if (audio_events != NULL)
        rich4_audio_event_emit(audio_events,
                               input == RICH4_COMPANY_ISSUE_DECREMENT ||
                               input == RICH4_COMPANY_ISSUE_INCREMENT
                                   ? RICH4_AUDIO_EVENT_BANK_SLIDER
                                   : RICH4_AUDIO_EVENT_BANK_KEY,
                               1, 0);
    return 1;
}

int rich4_company_issue_confirm(
    GameSession *session, int accept, Rich4AudioEventQueue *audio_events)
{
    GameCompanyIssuePresentationState *issue;
    int quantity;
    if (session == NULL || !session->presentation.company_issue.active)
        return 0;
    issue = &session->presentation.company_issue;
    if (issue->phase == 2) return 0;
    if (issue->phase == 0) {
        if (!accept) {
            if (audio_events != NULL)
                rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_CANCEL,
                                       1, 0);
            return rich4_company_issue_finish(session, 0);
        }
        issue->phase = 1;
        issue->quantity = 0;
        issue->entry_x = 256; issue->entry_y = 144;
        issue->entry_pressed = -1; issue->entry_dragging = 0;
        if (audio_events != NULL)
            rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_OK, 1, 0);
        return 1;
    }
    quantity = issue->quantity;
    if (audio_events != NULL)
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_BANK_KEY, 1, 0);
    return rich4_company_issue_finish(session, quantity);
}

void rich4_special_event_skip_wait(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    int phase;

    if (session == NULL || !session->presentation.special_event.active)
        return;
    phase = session->presentation.special_event.phase;
    if (phase == RICH4_SPECIAL_PRESENT_REACTION ||
        phase == RICH4_SPECIAL_PRESENT_PASSIVE_ACTOR_SPEECH ||
        phase == RICH4_SPECIAL_PRESENT_PASSIVE_VICTIM_SPEECH) {
        if (session->presentation.speech.active)
            session->presentation.speech.skip_requested = 1;
        return;
    }
    if (phase == RICH4_SPECIAL_PRESENT_PAGE && audio_events != NULL)
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_SPEECH_STOP,
                               1, 0);
    /* 0x44925B/0x44947D/0x44A571/0x44AC71/0x44AE2C and
     * confinement/departure calls use flags bit1=0: their FLC is not skippable. */
    if (phase == RICH4_SPECIAL_PRESENT_PAGE ||
        phase == RICH4_SPECIAL_PRESENT_HOLD ||
        phase == RICH4_SPECIAL_PRESENT_REACTION_DELAY ||
        phase == RICH4_SPECIAL_PRESENT_POST_HOLD ||
        phase == RICH4_SPECIAL_PRESENT_PASSIVE_CARD ||
        phase == RICH4_SPECIAL_PRESENT_GOD_NOTICE ||
        phase == RICH4_SPECIAL_PRESENT_REDIRECT_NOTICE)
        session->presentation.special_event.until = now;
}
