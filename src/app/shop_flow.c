/* Purpose: own the general-store visit state machine and its deterministic UI
 * facts. State is stored in GameSession.presentation.shop_visit; assets and
 * audio playback are borrowed through scalar frame/playback inputs and event
 * output. Allowed dependencies are app, core, and data. Invariants: a shop
 * transaction is committed only through the route executor and the visit is
 * closed only after its route continuation succeeds. Evidence: A4.2/A5 and
 * tests/unit/test_shop_flow.c. */
#include "app/shop_flow.h"

#include <stdint.h>
#include <string.h>

#include "app/game_speech_flow.h"
#include "core/card.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/rng.h"
#include "core/route_event_executor.h"
#include "core/tool.h"
#include "data/character_speech.h"

static void rich4_shop_emit(Rich4AudioEventQueue *events,
                            Rich4AudioEventType type, int value)
{
    if (events != NULL)
        rich4_audio_event_emit(events, type, 1, value);
}

const char *rich4_shop_message_big5(int index)
{
    static const char *const messages[11] = {
        "\xa6\xb3\xa4\xb0\xbb\xf2\xa7\xda\xaf\xe0\x0a\xac\xb0\xa7\x41\xaa\x41\xb0\xc8\xaa\xba\xb6\xdc\xa1\x48",
        "\xbd\xd0\xac\x44\xbf\xef\xa7\x41\xb7\x51\xad\x6e\x0a\xa7\x49\xb4\xab\xaa\xba\xa5\x64\xa4\xf9\xa1\x43",
        "\xa9\xea\xba\x70\xa1\x49\x0a\xa7\x41\xaa\xba\xc2\x49\xbc\xc6\xa4\xa3\xa8\xac\xa1\x49",
        "\xb9\xef\xa4\xa3\xb0\x5f\xa1\x49\x0a\xb1\x7a\xaa\xba\xa5\x64\xa4\xf9\xc4\xe6\xa4\x77\xba\xa1\xa1\x49",
        "\xc5\x77\xaa\xef\xa4\x55\xa6\xb8\xa6\x41\xa8\xd3\xa1\x49",
        "\xc5\x77\xaa\xef\xa5\xfa\xc1\x7b\x0a\xb9\x44\xa8\xe3\xa9\xb1\xa1\x49",
        "\xb1\x7a\xad\x6e\xa7\x49\xb4\xab\x0a\xa4\xb0\xbb\xf2\xb9\x44\xa8\xe3\xa1\x48",
        "\xb9\xef\xa4\xa3\xb0\x5f\xa1\x41\x0a\xb1\x7a\xaa\xba\xc2\x49\xa8\xe9\xa4\xa3\xb0\xf7\xa1\x49",
        "\xab\xdc\xa9\xea\xba\x70\xa1\x49\xb1\x7a\xaa\xba\x0a\xb9\x44\xa8\xe3\xc4\xe6\xa4\x77\xba\xa1\xa1\x49",
        "\xb3\x6f\xad\xd3\xb9\x44\xa8\xe3\xb7\x7c\xad\xfb\x0a\xa4\x7e\xaf\xe0\xa7\x49\xb4\xab\xa1\x49",
        "\xc1\xc2\xc1\xc2\xb4\x66\xc5\x55\xa1\x49"
    };
    return index >= 0 && index < 11 ? messages[index] : NULL;
}

static void rich4_shop_rebuild_tool_inventory(GameSession *session)
{
    int player;
    int slot;
    int tool;
    if (session == NULL) return;
    player = session->presentation.shop_visit.player;
    if (player < 0 || player >= session->state.player_count) return;
    for (slot = 0; slot < GAME_PLAYER_CARD_SLOTS; slot++)
        session->presentation.shop_visit.inventory_tool_slot[slot] = -1;
    slot = 0;
    for (tool = 0; tool < GAME_TOOL_COUNT &&
                   slot < GAME_PLAYER_CARD_SLOTS; tool++)
        if (session->state.players[player].tool_inventory[tool] > 0)
            session->presentation.shop_visit.inventory_tool_slot[slot++] = tool;
}

static void rich4_shop_start_message(
    GameSession *session, int index, int phase, int resume_phase,
    uint32_t now, Rich4AudioEventQueue *audio_events)
{
    GameShopPresentationState *shop;
    if (session == NULL || !session->presentation.shop_visit.active ||
        rich4_shop_message_big5(index) == NULL) return;
    shop = &session->presentation.shop_visit;
    shop->message_index = index;
    shop->message_resume_phase = resume_phase;
    shop->message_until = now + 2000;
    shop->phase = phase;
    shop->pressed = RICH4_SHOP_HIT_NONE;
    rich4_shop_emit(audio_events, RICH4_AUDIO_EVENT_SPEECH_START, index);
}

int rich4_shop_skip_message(GameSession *session, uint32_t now,
                            Rich4AudioEventQueue *audio_events)
{
    int phase;
    if (session == NULL || !session->presentation.shop_visit.active) return 0;
    phase = session->presentation.shop_visit.phase;
    if (phase != RICH4_SHOP_GREETING && phase != RICH4_SHOP_PROMPT &&
        phase != RICH4_SHOP_FEEDBACK && phase != RICH4_SHOP_EXIT_MESSAGE)
        return 0;
    rich4_shop_emit(audio_events, RICH4_AUDIO_EVENT_SPEECH_STOP, 0);
    session->presentation.shop_visit.message_until = now;
    return 1;
}

static void rich4_shop_reset_page_animation(
    GameShopPresentationState *shop, uint32_t now)
{
    if (shop == NULL) return;
    shop->clerk_face_chunk = -1;
    shop->clerk_mouth_chunk = -1;
    shop->clerk_idle_state = shop->mode + 3;
    shop->clerk_idle_variant = 0;
    shop->clerk_idle_step = 0;
    if (!shop->ui_started) {
        shop->ui_started = 1;
        shop->clerk_timer_phase = 0;
        shop->clerk_tick = now + 50;
    }
}

static void rich4_shop_enter_page(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    GameShopPresentationState *shop;
    int greeting;
    if (session == NULL || !session->presentation.shop_visit.active) return;
    shop = &session->presentation.shop_visit;
    rich4_shop_reset_page_animation(shop, now);
    shop->left_velocity = 40;
    shop->inventory_velocity = 80;
    if (shop->page_loaded[shop->mode]) {
        shop->message_index = -1;
        shop->left_x = 5;
        shop->inventory_x = 227;
        shop->phase = RICH4_SHOP_SLIDE_IN;
        shop->next_tick = now + 50;
        return;
    }
    shop->left_x = -222;
    shop->inventory_x = 640;
    greeting = shop->mode == 0 ? 0 : 5;
    rich4_shop_start_message(
        session, greeting, RICH4_SHOP_GREETING, RICH4_SHOP_SLIDE_IN,
        now, audio_events);
}

static void rich4_shop_start_prompt(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    int prompt;
    if (session == NULL || !session->presentation.shop_visit.active) return;
    prompt = session->presentation.shop_visit.mode == 0 ? 1 : 6;
    rich4_shop_start_message(
        session, prompt, RICH4_SHOP_PROMPT, RICH4_SHOP_ACTIVE,
        now, audio_events);
}

void rich4_shop_switch_page(GameSession *session, uint32_t now,
                            Rich4AudioEventQueue *audio_events)
{
    if (session == NULL || !session->presentation.shop_visit.active) return;
    session->presentation.shop_visit.mode ^= 1;
    rich4_shop_enter_page(session, now, audio_events);
}

void rich4_shop_begin(GameSession *session, const GameEvent *event,
                      uint32_t now, int animation_enabled,
                      Rich4AudioEventQueue *audio_events)
{
    int remaining[GAME_CARD_COUNT];
    int total = 0;
    int desired;
    int i;
    GameDomainRef gift;
    if (session == NULL || event == NULL) return;
    memset(&session->presentation.shop_visit, 0,
           sizeof(session->presentation.shop_visit));
    gift = rich4_game_event_shop_gift(event);
    session->presentation.shop_visit.active = 1;
    session->presentation.shop_visit.player =
        session->flow.route_events.state.execution_actor;
    session->presentation.shop_visit.gift_kind = gift.kind;
    session->presentation.shop_visit.gift_id = gift.id;
    if (gift.kind == GAME_DOMAIN_CARD) {
        const GameCardInfo *info = rich4_card_info(gift.id);
        session->presentation.shop_visit.gift_price =
            info != NULL ? info->shop_price : 0;
    } else if (gift.kind == GAME_DOMAIN_TOOL) {
        const GameToolInfo *info = rich4_tool_info(gift.id);
        session->presentation.shop_visit.gift_price =
            info != NULL ? info->shop_price : 0;
    }
    session->presentation.shop_visit.phase =
        (gift.kind == GAME_DOMAIN_CARD || gift.kind == GAME_DOMAIN_TOOL)
            ? RICH4_SHOP_OWNER_GIFT : RICH4_SHOP_GREETING;
    session->presentation.shop_visit.page_loaded[0] = !animation_enabled;
    session->presentation.shop_visit.page_loaded[1] = !animation_enabled;
    session->presentation.shop_visit.pressed = RICH4_SHOP_HIT_NONE;
    session->presentation.shop_visit.message_index = -1;
    session->presentation.shop_visit.message_resume_phase = RICH4_SHOP_ACTIVE;
    session->presentation.shop_visit.clerk_face_chunk = -1;
    session->presentation.shop_visit.clerk_mouth_chunk = -1;
    session->presentation.shop_visit.left_x = -222;
    session->presentation.shop_visit.inventory_x = 640;
    session->presentation.shop_visit.next_tick = now;
    session->presentation.shop_visit.gift_until = now + 1500;
    for (i = 0; i < GAME_CARD_COUNT; i++) {
        remaining[i] = session->state.card_amount[i];
        total += remaining[i];
    }
    desired = 6 + rich4_rng_range_at(&session->state.ai_rng_state, 10);
    for (i = 0; i < desired && total > 0; i++) {
        int pick = rich4_rng_range_at(&session->state.ai_rng_state, total);
        int card;
        for (card = 0; card < GAME_CARD_COUNT; card++) {
            if (pick < remaining[card]) break;
            pick -= remaining[card];
        }
        if (card >= GAME_CARD_COUNT) break;
        session->presentation.shop_visit.card_merchandise[
            session->presentation.shop_visit.card_count++] = card;
        remaining[card]--;
        total--;
    }
    {
        int merchandise_slot = 0;
        for (i = 0; i < 8; i++)
            session->presentation.shop_visit.tool_merchandise[i] = -1;
        for (i = 0; i < 8; i++)
            if (session->state.tool_amount[i] > 0)
                session->presentation.shop_visit.tool_merchandise[
                    merchandise_slot++] = i;
    }
    rich4_shop_rebuild_tool_inventory(session);
    if (session->presentation.shop_visit.phase == RICH4_SHOP_GREETING)
        rich4_shop_enter_page(session, now, audio_events);
}

int rich4_shop_hit(const GameSession *session, int x, int y)
{
    int column;
    int row;
    const GameShopPresentationState *shop;
    if (session == NULL || !session->presentation.shop_visit.active ||
        session->presentation.shop_visit.phase != RICH4_SHOP_ACTIVE)
        return RICH4_SHOP_HIT_NONE;
    shop = &session->presentation.shop_visit;
    if (x >= 542 && x <= 627 && y >= 13 && y <= 98)
        return RICH4_SHOP_HIT_TAB;
    if (x >= 556 && x <= 636 && y >= 246 && y <= 286)
        return RICH4_SHOP_HIT_EXIT;
    if (shop->mode == 0) {
        if (x >= 14 && x <= 215 && y >= 81 && y <= 441) {
            row = (y - 81) / 24;
            return row < shop->card_count &&
                   shop->card_merchandise[row] >= 0 && !shop->card_sold[row]
                ? row : RICH4_SHOP_HIT_NONE;
        }
    } else if (x >= 12 && x <= 213 && y >= 80 && y <= 464) {
        row = (y - 80) / 48;
        return row < 8 && shop->tool_merchandise[row] >= 0 &&
               !shop->tool_sold[row] ? row : RICH4_SHOP_HIT_NONE;
    }
    if (x >= 232 && x <= 632 && y >= 298 && y <= 466) {
        column = (x - 232) / 80;
        row = (y - 298) / 56;
        if (column < 5 && row < 3)
            return RICH4_SHOP_HIT_INVENTORY_BASE + row * 5 + column;
    }
    return RICH4_SHOP_HIT_NONE;
}

int rich4_shop_pointer_down(GameSession *session, int x, int y, uint32_t now,
                            Rich4AudioEventQueue *audio_events)
{
    int hit;
    if (session == NULL || !session->presentation.shop_visit.active)
        return 0;
    hit = rich4_shop_hit(session, x, y);
    if (hit == RICH4_SHOP_HIT_TAB || hit == RICH4_SHOP_HIT_EXIT) {
        session->presentation.shop_visit.pressed = hit;
        return 1;
    }
    return rich4_shop_apply_transaction(session, hit, now, audio_events);
}

void rich4_shop_pointer_up(GameSession *session, int released_control,
                           int x, int y, uint32_t now,
                           Rich4AudioEventQueue *audio_events)
{
    int released_hit;
    if (session == NULL || !session->presentation.shop_visit.active ||
        released_control == RICH4_SHOP_HIT_NONE) return;
    released_hit = rich4_shop_hit(session, x, y);
    session->presentation.shop_visit.pressed = RICH4_SHOP_HIT_NONE;
    if (released_hit == released_control) {
        if (released_hit == RICH4_SHOP_HIT_TAB)
            rich4_shop_switch_page(session, now, audio_events);
        else if (released_hit == RICH4_SHOP_HIT_EXIT)
            rich4_shop_begin_exit(session, now, audio_events);
    }
}

void rich4_shop_cancel(GameSession *session,
                       Rich4AudioEventQueue *audio_events)
{
    if (session == NULL || !session->presentation.shop_visit.active ||
        session->presentation.shop_visit.phase == RICH4_SHOP_OWNER_GIFT ||
        session->presentation.shop_visit.phase == RICH4_SHOP_OWNER_SPEECH)
        return;
    rich4_shop_emit(audio_events, RICH4_AUDIO_EVENT_SPEECH_STOP, 0);
    rich4_shop_finish(session);
}

int rich4_shop_apply_transaction(GameSession *session, int hit, uint32_t now,
                                 Rich4AudioEventQueue *audio_events)
{
    GameEventQueue events;
    RouteEventExecutionResult result;
    int before_points;
    int player;
    if (session == NULL || !session->presentation.shop_visit.active) return 0;
    player = session->presentation.shop_visit.player;
    if (player < 0 || player >= session->state.player_count) return 0;
    before_points = session->state.players[player].points;
    rich4_game_event_queue_reset(&events);
    if (hit >= RICH4_SHOP_HIT_INVENTORY_BASE) {
        int slot = hit - RICH4_SHOP_HIT_INVENTORY_BASE;
        if (slot < 0 || slot >= GAME_PLAYER_CARD_SLOTS) return 0;
        if (session->presentation.shop_visit.mode == 0) {
            result = rich4_route_event_executor_shop_sell_card_slot(
                &session->flow.route_events, &session->state, slot, &events);
        } else {
            int tool = session->presentation.shop_visit.inventory_tool_slot[slot];
            if (tool < 0) return 0;
            result = rich4_route_event_executor_shop_sell_tool(
                &session->flow.route_events, &session->state, tool, &events);
        }
    } else if (hit >= 0) {
        if (session->presentation.shop_visit.mode == 0) {
            int card;
            const GameCardInfo *info;
            if (hit >= session->presentation.shop_visit.card_count) return 0;
            card = session->presentation.shop_visit.card_merchandise[hit];
            info = rich4_card_info(card);
            if (info == NULL) return 0;
            if (before_points < info->shop_price) {
                rich4_shop_start_message(session, 2, RICH4_SHOP_FEEDBACK,
                                         RICH4_SHOP_ACTIVE, now, audio_events);
                return 0;
            }
            if (rich4_card_inventory_count(&session->state, player) >=
                GAME_PLAYER_CARD_SLOTS) {
                rich4_shop_start_message(session, 3, RICH4_SHOP_FEEDBACK,
                                         RICH4_SHOP_ACTIVE, now, audio_events);
                return 0;
            }
            result = rich4_route_event_executor_shop_buy_card(
                &session->flow.route_events, &session->state, card, &events);
        } else {
            int tool = hit < 8
                ? session->presentation.shop_visit.tool_merchandise[hit] : -1;
            const GameToolInfo *info = rich4_tool_info(tool);
            if (tool < 0 || info == NULL) return 0;
            if (before_points < info->shop_price) {
                rich4_shop_start_message(session, 7, RICH4_SHOP_FEEDBACK,
                                         RICH4_SHOP_ACTIVE, now, audio_events);
                return 0;
            }
            if (session->state.players[player].tool_inventory[tool] >= 9) {
                rich4_shop_start_message(session, 8, RICH4_SHOP_FEEDBACK,
                                         RICH4_SHOP_ACTIVE, now, audio_events);
                return 0;
            }
            result = rich4_route_event_executor_shop_buy_tool(
                &session->flow.route_events, &session->state, tool, &events);
        }
    } else return 0;
    rich4_shop_rebuild_tool_inventory(session);
    if (result == ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND &&
        session->state.players[player].points != before_points) {
        if (hit >= 0 && hit < RICH4_SHOP_HIT_INVENTORY_BASE) {
            if (session->presentation.shop_visit.mode == 0)
                session->presentation.shop_visit.card_sold[hit] = 1;
            else if (hit < 8)
                session->presentation.shop_visit.tool_sold[hit] = 1;
        }
        return 1;
    }
    return 0;
}

void rich4_shop_begin_exit(GameSession *session, uint32_t now,
                           Rich4AudioEventQueue *audio_events)
{
    if (session == NULL || !session->presentation.shop_visit.active) return;
    rich4_shop_start_message(
        session, session->presentation.shop_visit.mode == 0 ? 4 : 10,
        RICH4_SHOP_EXIT_MESSAGE, 0, now, audio_events);
}

void rich4_shop_finish(GameSession *session)
{
    GameEventQueue events;
    GameEvent event;
    RouteEventExecutionResult result;
    if (session == NULL || !session->presentation.shop_visit.active) return;
    rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_finish_shop(
        &session->flow.route_events, &session->state, &events);
    if (result == ROUTE_EVENT_EXECUTION_INVALID) return;
    while (rich4_game_event_queue_pop(&events, &event))
        if (event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED)
            session->presentation.route_interaction.active = 0;
    memset(&session->presentation.shop_visit, 0,
           sizeof(session->presentation.shop_visit));
}

void rich4_shop_mark_music_started(GameSession *session)
{
    if (session != NULL) session->presentation.shop_visit.music_started = 1;
}

static int rich4_shop_clerk_visible_steps(
    GameShopPresentationState *shop, uint32_t now)
{
    int steps = 0;
    int timer_ticks = 0;
    if (shop == NULL || !shop->ui_started) return 0;
    while ((int32_t)(now - shop->clerk_tick) >= 0 && timer_ticks < 32) {
        shop->clerk_tick += 50;
        shop->clerk_timer_phase ^= 1;
        if (shop->clerk_timer_phase) steps++;
        timer_ticks++;
    }
    if (timer_ticks == 32 && (int32_t)(now - shop->clerk_tick) >= 0)
        shop->clerk_tick = now + 50;
    return steps;
}

static void rich4_shop_update_clerk_idle(GameSession *session, GameShopPresentationState *shop)
{
    static const int card_idle[4] = { 7, 8, 7, 5 };
    static const int tool_idle[4] = { 21, 22, 21, 23 };
    if (shop == NULL) return;
    switch (shop->clerk_idle_state) {
    case 0: {
        int transition = rich4_rng_next_at(&session->state.ai_rng_state) >> 10;
        if (transition == 0 && shop->clerk_idle_variant != 0) {
            shop->clerk_idle_state = shop->mode + 1;
            shop->clerk_idle_step = 0;
        } else if (transition == 1) {
            shop->clerk_idle_state = shop->mode + 3;
            shop->clerk_idle_variant = 0;
            shop->clerk_idle_step = 0;
        }
        break;
    }
    case 1:
    case 2:
        if (shop->clerk_idle_step >= 4) {
            shop->clerk_idle_variant = shop->clerk_idle_state == 1 ? 2 : 1;
            shop->clerk_idle_state = 0;
            shop->clerk_idle_step = 0;
        } else {
            const int *sequence = shop->clerk_idle_state == 1
                ? card_idle : tool_idle;
            shop->clerk_face_chunk = sequence[shop->clerk_idle_step++];
        }
        break;
    case 3: {
        int variant = 1 + ((3 * rich4_rng_next_at(&session->state.ai_rng_state)) >> 15);
        if (variant != shop->clerk_idle_variant) {
            shop->clerk_face_chunk = variant + 3;
            shop->clerk_idle_variant = variant;
            shop->clerk_idle_state = 0;
        }
        break;
    }
    case 4: {
        int variant = 1 + (rich4_rng_next_at(&session->state.ai_rng_state) & 1);
        if (variant != shop->clerk_idle_variant) {
            shop->clerk_face_chunk = variant + 22;
            shop->clerk_idle_variant = variant;
            shop->clerk_idle_state = 0;
        }
        break;
    }
    default:
        shop->clerk_idle_state = shop->mode + 3;
        break;
    }
}

static void rich4_shop_update_clerk_mouth(
    GameSession *session, GameShopPresentationState *shop, int dialog_active)
{
    if (shop == NULL || (!dialog_active && shop->clerk_mouth_ticks == 0)) return;
    if (shop->clerk_mouth_ticks > 0) {
        shop->clerk_mouth_ticks--;
        if (shop->clerk_mouth_ticks == 0)
            shop->clerk_mouth_chunk = shop->mode == 0 ? 9 : 25;
    } else if ((rich4_rng_next_at(&session->state.ai_rng_state) >> 11) < 4) {
        shop->clerk_mouth_chunk = (shop->mode == 0 ? 10 : 26) +
                                  (rich4_rng_next_at(&session->state.ai_rng_state) & 1);
        shop->clerk_mouth_ticks = rich4_rng_next_at(&session->state.ai_rng_state) & 7;
        if (shop->clerk_mouth_ticks == 0) shop->clerk_mouth_ticks = 1;
    }
}

static int rich4_shop_update_owner_speech(
    GameSession *session, uint32_t now, int speech_playing,
    Rich4AudioEventQueue *audio_events)
{
    Rich4SpeechAudioRequest request;
    if (!rich4_speech_prepare_audio(session, &request)) return 0;
    if (request.stop)
        rich4_shop_emit(audio_events, RICH4_AUDIO_EVENT_SPEECH_STOP, 0);
    else if (request.start_resource >= 0)
        rich4_shop_emit(audio_events, RICH4_AUDIO_EVENT_SPEECH_START,
                        request.start_resource);
    return rich4_speech_finish_update(session, now, speech_playing);
}

int rich4_shop_update(GameSession *session, uint32_t now, int speech_playing,
                      Rich4AudioEventQueue *audio_events)
{
    GameShopPresentationState *shop;
    int visible_steps;
    int message_started = 0;
    int i;
    if (session == NULL || !session->presentation.shop_visit.active) return 0;
    shop = &session->presentation.shop_visit;
    visible_steps = rich4_shop_clerk_visible_steps(shop, now);
    for (i = 0; i < visible_steps; i++) rich4_shop_update_clerk_idle(session, shop);
    if (shop->phase == RICH4_SHOP_ACTIVE) goto finish;
    if (shop->phase == RICH4_SHOP_OWNER_GIFT) {
        const Rich4CharacterSpeech *speech;
        int variant;
        if ((int32_t)(now - shop->gift_until) < 0) goto finish;
        variant = shop->gift_price > 50 && shop->gift_price <= 100
            ? rich4_rng_range_at(&session->state.ai_rng_state, 2) : 0;
        speech = rich4_character_shop_gift_speech(
            session->state.players[shop->player].character,
            shop->gift_price, variant);
        if (game_start_speech_entry(
                session, shop->player, speech, 0,
                GAME_PROPERTY_PRESENT_NONE, now)) {
            shop->phase = RICH4_SHOP_OWNER_SPEECH;
            goto finish;
        }
        rich4_shop_enter_page(session, now, audio_events);
        message_started = 1;
        goto finish;
    }
    if (shop->phase == RICH4_SHOP_OWNER_SPEECH) {
        if (rich4_shop_update_owner_speech(
                session, now, speech_playing, audio_events)) goto finish;
        rich4_shop_enter_page(session, now, audio_events);
        message_started = 1;
        goto finish;
    }
    if (shop->phase == RICH4_SHOP_GREETING || shop->phase == RICH4_SHOP_PROMPT ||
        shop->phase == RICH4_SHOP_FEEDBACK || shop->phase == RICH4_SHOP_EXIT_MESSAGE) {
        int phase = shop->phase;
        int resume = shop->message_resume_phase;
        if ((int32_t)(now - shop->message_until) < 0 || speech_playing)
            goto finish;
        shop->message_index = -1;
        if (phase == RICH4_SHOP_EXIT_MESSAGE) {
            rich4_shop_finish(session);
            return session->presentation.shop_visit.active;
        }
        if (phase == RICH4_SHOP_GREETING) {
            shop->phase = RICH4_SHOP_SLIDE_IN;
            shop->next_tick = now;
            shop->left_velocity = 40;
            shop->inventory_velocity = 80;
            goto finish;
        }
        if (phase == RICH4_SHOP_PROMPT) shop->page_loaded[shop->mode] = 1;
        shop->phase = resume;
        goto finish;
    }
    if (shop->phase == RICH4_SHOP_SLIDE_IN) {
        if ((int32_t)(now - shop->next_tick) < 0) goto finish;
        shop->next_tick = now + 50;
        if (shop->left_x < 5) shop->left_x += shop->left_velocity;
        if (shop->inventory_x > 227) shop->inventory_x -= shop->inventory_velocity;
        shop->left_velocity -= 3;
        shop->inventory_velocity -= 7;
        if (shop->left_x >= 5) shop->left_x = 5;
        if (shop->inventory_x <= 227) shop->inventory_x = 227;
        if (shop->left_x == 5 && shop->inventory_x == 227) {
            if (shop->page_loaded[shop->mode]) shop->phase = RICH4_SHOP_ACTIVE;
            else {
                rich4_shop_start_prompt(
                    session, now, audio_events);
                message_started = 1;
            }
        }
    }
finish:
    if (!session->presentation.shop_visit.active) return 0;
    shop = &session->presentation.shop_visit;
    for (i = 0; i < visible_steps; i++)
        rich4_shop_update_clerk_mouth(
            session, shop, shop->message_index >= 0 && !message_started);
    return session->presentation.shop_visit.active;
}
