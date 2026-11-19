#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/audio_event.h"
#include "app/card_use_flow.h"
#include "app/card_defense_flow.h"
#include "app/card_effect_flow.h"
#include "app/property_flow.h"
#include "app/game_speech_flow.h"
#include "app/special_place_notice_flow.h"
#include "core/card.h"
#include "core/rng.h"
#include "core/game_event_payload.h"
#include "core/character_speech_rules.h"

static GameSession session;
static MapData map;
static MapLandRecord lands[2];
static MapFacilityRecord facilities[2];

static void setup(void)
{
    memset(&session, 0, sizeof(session));
    memset(&map, 0, sizeof(map));
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.price_index = 1;
    session.state.players[0].play_state = session.state.players[1].play_state = 1;
    session.state.players[0].character = 0;
    session.state.players[1].character = 1;
    session.state.players[0].node = 1;
    session.state.tool_route_node_count = 3;
    session.state.tool_route_x[1] = session.presentation.actors[0].visual_x = 1000;
    session.state.tool_route_y[1] = session.presentation.actors[0].visual_y = 1000;
    session.state.players[1].node = 2;
    session.state.tool_route_x[2] = 1600;
    session.state.tool_route_y[2] = 1200;
    session.state.land_count = session.state.facility_count = 1;
    session.state.land_owner[0] = session.state.facility_owner[0] = 1;
    session.state.land_level[0] = session.state.facility_level[0] = 3;
    lands[1].x = facilities[1].x = 1700;
    lands[1].y = facilities[1].y = 1500;
    map.lands = lands; map.land_count = 2;
    map.facilities = facilities; map.facility_count = 2;
}

static void finish_speech(uint32_t now)
{
    rich4_speech_request_skip(&session);
    assert(!rich4_speech_finish_update(&session, now, 0));
}

static void assert_speech(int player, int type, int speaking_resource)
{
    Rich4PlayerSayText decoded;
    assert(session.presentation.speech.active);
    assert(session.presentation.speech.player == player);
    assert(session.presentation.speech.type == type);
    rich4_player_say_decode(session.presentation.speech.current->big5, -1, &decoded);
    assert(decoded.speaking_resource == speaking_resource);
}

static void all_notices(void)
{
    int card;
    for (card = 0; card < GAME_CARD_COUNT; ++card) {
        GameCommand command;
        Rich4AudioEventQueue audio;
        Rich4AudioEvent event;
        int card_sounds = 0;
        uint32_t started = 0xfffffff0u;
        setup();
        session.state.players[0].card_inventory[0] = card + 1;
        command = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
            GAME_COMMAND_SOURCE_AI, 0, rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
        assert(rich4_card_use_begin(&session, &command, &map, 1, started));
        assert(!rich4_card_use_begin(&session, &command, &map, 1, started));
        assert(session.presentation.card_use.phase == GAME_CARD_USE_NOTICE);
        assert(rich4_card_use_update(&session, NULL, started + 1499u) == RICH4_CARD_USE_WAIT);
        assert(session.state.players[0].card_inventory[0] == card + 1);
        assert(!session.presentation.dice.popup_active);
        rich4_audio_event_queue_init(&audio);
        rich4_app_collect_session_audio_events(&session, NULL, &audio);
        while (rich4_audio_event_pop(&audio, &event))
            if (event.type == RICH4_AUDIO_EVENT_CARD) card_sounds++;
        assert(card_sounds == 1);
        rich4_app_collect_session_audio_events(&session, NULL, &audio);
        while (rich4_audio_event_pop(&audio, &event))
            assert(event.type != RICH4_AUDIO_EVENT_CARD);
        assert(rich4_card_use_update(&session, NULL, started + 1500u) == RICH4_CARD_USE_WAIT);
        if (session.presentation.speech.active) finish_speech(started + 1501u);
        assert(rich4_card_use_update(&session, NULL, started + 1501u) == RICH4_CARD_USE_COMMAND);
        rich4_card_use_command_completed(&session, 0, started + 1501u);
        assert(rich4_card_use_update(&session, NULL, started + 1502u) == RICH4_CARD_USE_WAIT);
        assert(rich4_card_use_update(&session, NULL, started + 1503u) == RICH4_CARD_USE_FINISHED);
    }
    setup();
    {
        GameCommand command = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
            GAME_COMMAND_SOURCE_AI, 0, rich4_game_domain_ref(GAME_DOMAIN_CARD, 0),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
        session.state.players[0].card_inventory[0] = 1;
        assert(rich4_card_use_begin(&session, &command, &map, 1, 100));
        assert(rich4_card_use_skip_notice(&session, 101));
        (void)rich4_card_use_update(&session, NULL, 101);
        assert(session.presentation.card_use.phase == GAME_CARD_USE_CASTER_SPEECH);
        assert(session.presentation.speech.active);
    }
}

/* 0x443917/0x443B0F: old camera during speech/flight; property camera
 * during the animation; victim speech; return to actor before the next roll. */
static void destruction_sequence(int card, GameDomainKind kind, int ai, int direction)
{
    GameViewportModel viewport = {0};
    GameCommand command;
    uint32_t now = 1700;
    setup();
    session.state.players[0].card_inventory[0] = card + 1;
    viewport.center_world_x = viewport.center_world_y = 1000;
    viewport.direction = direction;
    command = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
        ai ? GAME_COMMAND_SOURCE_AI : GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, card), rich4_game_domain_ref(kind, 0), 0);
    assert(rich4_card_inspect(&session.state, &command) == GAME_CARD_USE_APPLIED);
    assert(rich4_card_use_begin(&session, &command, &map, ai, 100));
    if (ai) (void)rich4_card_use_update(&session, &viewport, 1600);
    assert(session.presentation.speech.active);
    assert(session.presentation.speech.player == 0);
    finish_speech(now);
    if (ai) {
        assert(rich4_card_use_update(&session, &viewport, now) == RICH4_CARD_USE_WAIT);
        assert(session.presentation.card_use.flight_steps > 0);
        assert(!session.presentation.has_view_override);
        for (int step = 0; step < session.presentation.card_use.flight_steps; step++) {
            now += 24;
            assert(rich4_card_use_update(&session, &viewport, now) == RICH4_CARD_USE_WAIT);
        }
        now += 100;
        assert(rich4_card_use_update(&session, &viewport, now - 1) == RICH4_CARD_USE_WAIT);
    }
    assert(rich4_card_use_update(&session, &viewport, now) == RICH4_CARD_USE_COMMAND);
    assert(session.presentation.has_view_override);
    assert(session.presentation.view_world_x == 1700 && session.presentation.view_world_y == 1500);
    rich4_card_use_command_completed(&session, 1, now);
    session.presentation.monster_card.active = 1;
    assert(rich4_card_use_update(&session, &viewport, now + 1) == RICH4_CARD_USE_DISPATCH_EFFECTS);
    session.presentation.monster_card.active = 0;
    assert(rich4_card_use_update(&session, &viewport, now + 2) == RICH4_CARD_USE_WAIT);
    assert(session.presentation.speech.active && session.presentation.speech.player == 1);
    {
        Rich4PlayerSayText decoded;
        rich4_player_say_decode(session.presentation.speech.current->big5, -1, &decoded);
        assert(decoded.speaking_resource == (card == 10 ? 519 : 520));
    }
    finish_speech(now + 3);
    assert(rich4_card_use_update(&session, &viewport, now + 3) == RICH4_CARD_USE_FINISHED);
    assert(!session.presentation.has_view_override);
}

static void stock_card_sequence(int card, int character, int skip_notice)
{
    GameCommand command;
    const GameEvent *notice;
    Rich4GameEventFields fields;
    Rich4PlayerSayText decoded;
    uint32_t now = 5100;
    setup();
    session.state.players[0].character = character;
    session.state.players[0].is_ai = 1;
    session.state.players[0].card_inventory[0] = card + 1;
    command = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
        GAME_COMMAND_SOURCE_AI, 0, rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, 7), 0);
    assert(rich4_card_use_begin(&session, &command, &map, 1, 100));
    (void)rich4_card_use_update(&session, NULL, 1600);
    assert(session.presentation.speech.active && session.presentation.speech.type == 0);
    rich4_player_say_decode(session.presentation.speech.current->big5, -1, &decoded);
    assert(decoded.speaking_resource == 449 + (card - 23) + character * 52);
    /* The speaker's full wait owns the command, including a long voice. */
    assert(rich4_speech_finish_update(&session, 5000, 1));
    assert(rich4_card_use_update(&session, NULL, 5000) == RICH4_CARD_USE_WAIT);
    assert(session.presentation.card_panel.phase == CARD_PANEL_CLOSED);
    assert(!rich4_special_place_notice_current(&session.presentation));
    finish_speech(now);
    assert(rich4_card_use_update(&session, NULL, now) == RICH4_CARD_USE_COMMAND);
    assert(!session.presentation.card_use.flight_initialized);
    rich4_card_use_command_completed(&session, 1, now);
    notice = rich4_special_place_notice_current(&session.presentation);
    assert(notice != NULL);
    fields = rich4_game_event_special_place_notice_requested_fields(notice);
    assert(fields.subject == GAME_SPECIAL_NOTICE_STOCK_CARD && fields.value == card);
    assert(fields.target_ref.kind == GAME_DOMAIN_STOCK && fields.target_ref.id == 7);
    assert(fields.value2 == 1500);
    assert(rich4_card_use_update(&session, NULL, now + 1499) == RICH4_CARD_USE_DISPATCH_EFFECTS);
    if (skip_notice) assert(rich4_special_place_notice_skip(&session, now + 1));
    else assert(rich4_special_place_notice_update(&session, now + 1499));
    assert(rich4_special_place_notice_update(&session, skip_notice ? now + 1 : now + 1500));
    assert(!rich4_special_place_notice_current(&session.presentation));
    (void)rich4_card_use_update(&session, NULL, now + 1501);
    assert(rich4_card_use_update(&session, NULL, now + 1502) == RICH4_CARD_USE_FINISHED);
    assert(session.presentation.card_panel.phase == CARD_PANEL_CLOSED);

    /* Human mode has the same voice before opening the selector, and does
     * not enqueue the AI-only stock target notice or consume on cancel. */
    setup();
    session.state.players[0].character = character;
    session.state.players[0].card_inventory[0] = card + 1;
    assert(rich4_card_use_begin_stock_selection(&session, 0, card, 100));
    assert(session.presentation.card_panel.phase == CARD_PANEL_WAIT_HANDLER);
    assert(rich4_card_use_update(&session, NULL, 101) == RICH4_CARD_USE_WAIT);
    finish_speech(102);
    assert(rich4_card_use_update(&session, NULL, 102) == RICH4_CARD_USE_FINISHED);
    assert(session.presentation.card_panel.phase == CARD_PANEL_STOCK_TARGET);
    assert(session.state.players[0].card_inventory[0] == card + 1);
    assert(!rich4_special_place_notice_current(&session.presentation));
}

static GameCommand player_card(int card)
{
    session.state.players[0].card_inventory[0] = card + 1;
    return rich4_game_domain_command(GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_HUMAN,
        0, rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), 0);
}

static Rich4CardUseStep advance(uint32_t *now)
{
    if (session.presentation.speech.active) finish_speech(++*now);
    return rich4_card_use_update(&session, NULL, ++*now);
}

static void passive_attack(void)
{
    GameCommand command;
    GameEventQueue events;
    uint32_t now = 100;
    setup(); command = player_card(15);
    session.state.players[1].card_inventory[0] = 21;
    session.state.players[1].card_inventory[1] = 19;
    session.state.players[1].card_inventory[2] = 18;
    assert(rich4_card_use_begin(&session, &command, &map, 0, now));
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(session.presentation.card_use.defense.card == 20);
    assert(session.presentation.card_use.defense.phase == GAME_PROPERTY_PRESENT_INSURANCE_CARD);
    assert(rich4_card_use_skip_notice(&session, now));
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(session.presentation.speech.player == 1);
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(advance(&now) == RICH4_CARD_USE_COMMAND);
    command = session.presentation.card_use.command;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&session.state, &command, &events) == GAME_CARD_USE_APPLIED);
    assert(!rich4_card_inventory_has(&session.state, 1, 20));
    assert(rich4_card_inventory_has(&session.state, 1, 18));
    assert(rich4_card_inventory_has(&session.state, 1, 17));
    assert(!session.state.players[1].days_sleep_walking);
    rich4_card_use_command_completed(&session, 1, now);
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(advance(&now) == RICH4_CARD_USE_FINISHED);
    assert(!session.presentation.has_view_override);

    setup(); now = 100; command = player_card(15);
    session.state.player_count = 3;
    session.state.players[2].play_state = 1;
    session.state.players[2].character = 2;
    session.state.players[1].card_inventory[0] = 19;
    session.state.players[1].card_inventory[1] = 18;
    assert(rich4_card_use_begin(&session, &command, &map, 0, now));
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(session.presentation.card_use.defense.card == 18);
    assert(rich4_card_use_skip_notice(&session, now));
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(session.presentation.card_use.defense.phase == GAME_PROPERTY_PRESENT_INSURANCE_TARGET);
    assert(!rich4_card_defense_choose(&session, 1, now));
    assert(rich4_card_defense_choose(&session, 2, now));
    assert(session.presentation.speech.player == 1 && session.presentation.speech.type == 0);
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(session.presentation.speech.player == 2 && session.presentation.speech.type == 2);
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(session.presentation.speech.player == 2 && session.presentation.speech.type == 1);
    assert(advance(&now) == RICH4_CARD_USE_COMMAND);
    command = session.presentation.card_use.command;
    assert(command.flags & GAME_CARD_FLAG_USE_MISDIRECT);
    assert(command.value == 2);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&session.state, &command, &events) == GAME_CARD_USE_APPLIED);
    assert(session.state.players[2].days_sleep_walking == 5);
    assert(!session.state.players[1].days_sleep_walking && !session.state.players[0].days_sleep_walking);
    assert(!rich4_card_inventory_has(&session.state, 1, 18));
    assert(rich4_card_inventory_has(&session.state, 1, 17));
}

static void tax_free_choice(int accept)
{
    GameCommand command;
    GameEventQueue events;
    uint32_t now = 100;
    setup(); command = player_card(25);
    session.state.players[1].cash = 10000;
    session.state.players[1].card_inventory[0] = 20;
    assert(rich4_card_use_begin(&session, &command, &map, 0, now));
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(session.presentation.card_use.defense.phase == GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM);
    assert(!session.presentation.card_use.sound_pending); /* question before card popup */
    assert(rich4_card_defense_choose(&session, accept ? 1 : -1, now));
    if (accept) {
        assert(session.presentation.card_use.defense.phase == GAME_PROPERTY_PRESENT_INSURANCE_CARD);
        assert(rich4_card_use_skip_notice(&session, now));
        assert(advance(&now) == RICH4_CARD_USE_WAIT);
        assert(session.presentation.speech.player == 1);
        assert(advance(&now) == RICH4_CARD_USE_WAIT);
        assert(session.presentation.speech.player == 0);
    }
    assert(advance(&now) == RICH4_CARD_USE_COMMAND);
    command = session.presentation.card_use.command;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&session.state, &command, &events) == GAME_CARD_USE_APPLIED);
    assert(session.state.players[1].cash == (accept ? 10000 : 8000));
    assert(rich4_card_inventory_has(&session.state, 1, 19) == !accept);
    rich4_card_use_command_completed(&session, 1, now);
    if (!accept) {
        const GameEvent *notice = rich4_special_place_notice_current(&session.presentation);
        Rich4GameEventFields fields;
        assert(notice != NULL);
        fields = rich4_game_event_special_place_notice_requested_fields(notice);
        assert(fields.subject == GAME_SPECIAL_NOTICE_CARD_TAX && fields.value == 2000);
        assert(fields.target_ref.id == 1);
        assert(rich4_special_place_notice_skip(&session, ++now));
        assert(rich4_special_place_notice_update(&session, now));
        assert(advance(&now) == RICH4_CARD_USE_WAIT);
        /* Character 1, victim column, card 26: _card_strings #0527. */
        assert_speech(1, 2, 527);
        assert(advance(&now) == RICH4_CARD_USE_FINISHED);
    } else {
        assert(!rich4_special_place_notice_current(&session.presentation));
        assert(advance(&now) == RICH4_CARD_USE_WAIT);
        assert(!session.presentation.speech.active);
        assert(advance(&now) == RICH4_CARD_USE_FINISHED);
    }
}

static void tax_redirect_victim_speech(int redirect_to_caster)
{
    GameCommand command;
    GameEventQueue events;
    uint32_t now = 100;
    int redirect = redirect_to_caster ? 0 : 2;
    int caster_cash;
    setup();
    session.state.player_count = 3;
    session.state.players[2].play_state = 1;
    session.state.players[2].character = 2;
    session.state.players[0].cash = 7000;
    session.state.players[1].cash = 20000;
    session.state.players[2].cash = 5000;
    session.state.players[1].card_inventory[0] = 19;
    command = player_card(25);
    caster_cash = session.state.players[0].cash;
    assert(rich4_card_use_begin(&session, &command, &map, 0, now));
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(session.presentation.card_use.defense.card == 18);
    assert(rich4_card_use_skip_notice(&session, now));
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(session.presentation.card_use.defense.phase == GAME_PROPERTY_PRESENT_INSURANCE_TARGET);
    assert(rich4_card_defense_choose(&session, redirect, now));
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    assert(advance(&now) == RICH4_CARD_USE_COMMAND);
    command = session.presentation.card_use.command;
    assert(command.flags & GAME_CARD_FLAG_USE_MISDIRECT);
    assert(command.value == redirect);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&session.state, &command, &events) == GAME_CARD_USE_APPLIED);
    rich4_card_use_command_completed(&session, 1, now);
    if (redirect_to_caster) {
        /* 0x44536F jumps over payment, message and victim speech. */
        assert(session.state.players[0].cash == caster_cash);
        assert(!rich4_special_place_notice_current(&session.presentation));
        assert(advance(&now) == RICH4_CARD_USE_WAIT);
        assert(!session.presentation.speech.active);
        assert(advance(&now) == RICH4_CARD_USE_FINISHED);
    } else {
        const GameEvent *notice = rich4_special_place_notice_current(&session.presentation);
        Rich4GameEventFields fields;
        assert(session.state.players[0].cash == caster_cash + 1000);
        assert(session.state.players[2].cash == 4000);
        assert(notice != NULL);
        fields = rich4_game_event_special_place_notice_requested_fields(notice);
        assert(fields.subject == GAME_SPECIAL_NOTICE_CARD_TAX && fields.value == 1000);
        assert(fields.target_ref.id == 2);
        assert(rich4_special_place_notice_skip(&session, ++now));
        assert(rich4_special_place_notice_update(&session, now));
        assert(advance(&now) == RICH4_CARD_USE_WAIT);
        /* Character 2, victim column, card 26: _card_strings #0579. */
        assert_speech(2, 2, 579);
        assert(advance(&now) == RICH4_CARD_USE_FINISHED);
    }
}

static void tortoise_target_speech(void)
{
    GameCommand command;
    uint32_t now = 100;

    setup(); command = player_card(29);
    assert(rich4_card_use_begin(&session, &command, &map, 0, now));
    assert_speech(0, 3, 455);
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    /* Character 1, victim column, card 30: _card_strings #0529. */
    assert_speech(1, 2, 529);
    assert(advance(&now) == RICH4_CARD_USE_COMMAND);

    setup();
    session.state.players[0].card_inventory[0] = 30;
    command = rich4_game_domain_command(GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_HUMAN,
        0, rich4_game_domain_ref(GAME_DOMAIN_CARD, 29),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 0), 0);
    assert(rich4_card_use_begin(&session, &command, &map, 0, now));
    assert(!session.presentation.speech.active);
    assert(rich4_card_use_update(&session, NULL, ++now) == RICH4_CARD_USE_WAIT);
    /* Self target uses the success column, not the victim column: #0459. */
    assert_speech(0, 3, 459);
    assert(advance(&now) == RICH4_CARD_USE_COMMAND);

    setup();
    session.state.players[0].card_inventory[0] = 30;
    command = rich4_game_domain_command(GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_HUMAN,
        0, rich4_game_domain_ref(GAME_DOMAIN_CARD, 29),
        rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER, 0), 0);
    assert(rich4_card_use_begin(&session, &command, &map, 0, now));
    assert_speech(0, 3, 455);
    assert(advance(&now) == RICH4_CARD_USE_COMMAND);
    assert(!session.presentation.speech.active);
}

static void revenge_continuation(int card)
{
    GameCommand command;
    GameEventQueue events;
    Rich4CardUseStep step;
    uint32_t now = 100;
    setup(); command = player_card(card);
    session.state.players[1].card_inventory[0] = 18;
    assert(rich4_card_use_begin(&session, &command, &map, 0, now));
    if (card == 15) {
        assert(advance(&now) == RICH4_CARD_USE_WAIT); /* dreamwalking response precedes punishment */
        assert(session.presentation.speech.player == 1);
    }
    assert(advance(&now) == RICH4_CARD_USE_COMMAND);
    command = session.presentation.card_use.command;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&session.state, &command, &events) == GAME_CARD_USE_APPLIED);
    assert(rich4_card_inventory_has(&session.state, 1, 17));
    assert(!session.state.players[0].days_sleep_walking && !session.state.players[0].days_in_prison);
    rich4_card_use_command_completed(&session, 1, now);
    assert(advance(&now) == RICH4_CARD_USE_WAIT);
    if (card == 16) {
        assert(session.presentation.speech.active && session.presentation.speech.player == 1);
        assert(advance(&now) == RICH4_CARD_USE_WAIT);
    }
    assert(session.presentation.card_use.defense.card == 17);
    assert(rich4_card_use_skip_notice(&session, now));
    do { step = advance(&now); } while (step == RICH4_CARD_USE_WAIT);
    assert(step == RICH4_CARD_USE_REVENGE_COMMAND);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_resolve_revenge(&session.state, 1, 0,
        card == 15 ? GAME_CARD_ATTACK_SLEEP_WALK : GAME_CARD_ATTACK_FRAME, &events, 0));
    assert(!rich4_card_inventory_has(&session.state, 1, 17));
    assert(card == 15 ? session.state.players[0].days_sleep_walking == 5 :
        session.state.players[0].days_in_prison == 5);
}

static void group_effects(void)
{
    GameCommand command;
    GameEventQueue events;
    uint32_t now = 100;
    setup(); session.state.land_count = 3;
    for (int i=0;i<3;i++) {session.state.land_group[i]=4;session.state.land_owner[i]=1;}
    session.state.land_level[0]=4;session.state.land_level[1]=0;session.state.land_level[2]=3;
    session.state.land_type[1]=session.state.land_type[2]=1;
    command=rich4_game_domain_command(GAME_COMMAND_USE_CARD,GAME_COMMAND_SOURCE_HUMAN,
        0,rich4_game_domain_ref(GAME_DOMAIN_CARD,8),rich4_game_domain_ref(GAME_DOMAIN_LAND,0),0);
    session.state.players[0].card_inventory[0]=9;
    /* Use the presentation's snapshot independently of the resource flight. */
    session.presentation.card_use.command=command;
    rich4_card_effect_prepare(&session);
    assert(session.presentation.card_use.highlight_land[1]);
    assert(session.presentation.card_use.max_level_pending);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&session.state,&command,&events)==GAME_CARD_USE_APPLIED);
    assert(session.state.land_level[0]==5 && session.state.land_level[1]==1 && session.state.land_level[2]==3);
    rich4_card_effect_flash(&session,now);
    for (int i=0;i<16;i++) {
        rich4_property_flash_frame_presented(&session,now);
        now+=30;
        assert(rich4_card_effect_flash_update(&session,now));
    }
    now+=400;
    assert(!rich4_card_effect_flash_update(&session,now));
    assert(session.presentation.property.phase==GAME_PROPERTY_PRESENT_NONE);
    command.subject.id=9;session.state.players[0].card_inventory[0]=10;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&session.state,&command,&events)==GAME_CARD_USE_APPLIED);
    for(int i=0;i<3;i++) assert(session.state.land_type[i]==0 && session.state.land_level[i]==0);
}

static void invitation_flight(void)
{
    GameCommand command;
    GameViewportModel viewport = {0};
    Rich4CardUseStep step;
    uint32_t now = 100;
    setup();
    session.state.players[0].card_inventory[0] = 23;
    session.presentation.dynamic_objects[0].type = 1;
    session.presentation.dynamic_objects[0].node = 2;
    session.presentation.dynamic_objects[0].node_direction = 3;
    viewport.center_world_x = viewport.center_world_y = 1000;
    command = rich4_game_domain_command(GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_HUMAN,
        0, rich4_game_domain_ref(GAME_DOMAIN_CARD,22),rich4_game_domain_ref(GAME_DOMAIN_GOD,0),1);
    assert(rich4_card_use_begin(&session,&command,&map,0,now));
    finish_speech(++now);
    assert(rich4_card_use_update(&session,&viewport,++now)==RICH4_CARD_USE_WAIT);
    assert(session.presentation.card_use.flight_sprite_index==0);
    assert(session.presentation.card_use.flight_sprite_chunk==3);
    {
        int x,y;
        rich4_game_viewport_project(&viewport,1600,1200,&x,&y);
        assert(session.presentation.card_use.flight_from_x==x && session.presentation.card_use.flight_from_y==y);
        rich4_game_viewport_project(&viewport,1000,1000,&x,&y);
        assert(session.presentation.card_use.flight_to_x==x && session.presentation.card_use.flight_to_y==y);
    }
    do { now+=24; step=rich4_card_use_update(&session,&viewport,now); }
    while(step==RICH4_CARD_USE_WAIT);
    assert(step==RICH4_CARD_USE_COMMAND);
    assert(now==session.presentation.card_use.flight_tick); /* no 100 ms tail */
}

static void swap_houses(void)
{
    MapLandRecord properties[3] = {{0}};
    MapData local_map = {0};
    GameCommand command;
    GameEventQueue events;
    Rich4CardUseStep step;
    uint32_t now = 100;
    setup(); session.state.land_count=2;
    local_map.lands=properties; local_map.land_count=3;
    properties[1].x=1000;properties[1].y=1000;
    properties[2].x=1700;properties[2].y=1500;
    session.state.land_owner[0]=0;session.state.land_owner[1]=1;
    session.state.land_level[0]=2;session.state.land_level[1]=4;
    session.state.land_type[0]=0;session.state.land_type[1]=1;
    session.state.players[0].card_inventory[0]=5;
    command=rich4_game_domain_command(GAME_COMMAND_USE_CARD,GAME_COMMAND_SOURCE_HUMAN,
        0,rich4_game_domain_ref(GAME_DOMAIN_CARD,4),rich4_game_domain_ref(GAME_DOMAIN_LAND,1),0);
    assert(rich4_card_use_begin(&session,&command,&local_map,0,now));
    assert(session.presentation.speech.player==0);
    assert(advance(&now)==RICH4_CARD_USE_WAIT);
    assert(session.presentation.property.phase==GAME_PROPERTY_PRESENT_CARD_FLASH);
    assert(session.state.land_level[0]==2 && session.state.land_level[1]==4);
    while(session.presentation.card_use.phase==GAME_CARD_USE_BEFORE_COMMAND_FLASH) {
        rich4_property_flash_frame_presented(&session,now);
        now+=30;
        (void)rich4_card_use_update(&session,NULL,now);
    }
    assert(session.presentation.card_use.phase==GAME_CARD_USE_SWAP_HOUSES);
    assert(session.presentation.card_use.swap_steps>1);
    assert(session.presentation.card_use.swap_x>0 && session.presentation.card_use.swap_y>0);
    do { now+=24; step=rich4_card_use_update(&session,NULL,now); }
    while(step==RICH4_CARD_USE_WAIT);
    assert(step==RICH4_CARD_USE_COMMAND);
    assert(session.state.land_level[0]==2 && session.state.land_level[1]==4);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&session.state,&command,&events)==GAME_CARD_USE_APPLIED);
    assert(session.state.land_level[0]==4 && session.state.land_level[1]==2);
    assert(session.state.land_type[0]==1 && session.state.land_type[1]==0);
    rich4_card_use_command_completed(&session,1,now);
    assert(advance(&now)==RICH4_CARD_USE_WAIT);
    assert(session.presentation.card_use.phase==GAME_CARD_USE_FINISH_HOLD);
    assert(rich4_card_use_update(&session,NULL,now+499)==RICH4_CARD_USE_WAIT);
    now+=500;
    assert(rich4_card_use_update(&session,NULL,now)==RICH4_CARD_USE_WAIT);
    assert(advance(&now)==RICH4_CARD_USE_WAIT);
    assert(session.presentation.speech.player==1 && session.presentation.speech.type==2);
    assert(advance(&now)==RICH4_CARD_USE_FINISHED);
}

static void target_effect_order(void)
{
    GameCommand command;
    GameEventQueue events;
    GameViewportModel viewport={0};
    uint32_t now=100;
    setup(); command=player_card(1);
    command.source=GAME_COMMAND_SOURCE_AI;
    session.state.players[0].cash=100;session.state.players[1].cash=900;
    assert(rich4_card_use_begin(&session,&command,&map,0,now));
    assert(advance(&now)==RICH4_CARD_USE_COMMAND);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&session.state,&command,&events)==GAME_CARD_USE_APPLIED);
    rich4_card_use_command_completed(&session,1,now);
    assert(session.state.players[0].cash==500 && session.state.players[1].cash==500);
    assert(session.presentation.card_use.phase==GAME_CARD_USE_FLIGHT);
    assert(rich4_card_use_update(&session,&viewport,++now)==RICH4_CARD_USE_WAIT);
    assert(session.presentation.card_use.flight_steps>0);

    for(int card=2;card<=7;card++) {
        if(card!=2 && card!=6 && card!=7)continue;
        setup();now=100;
        session.state.players[0].card_inventory[0]=card+1;
        session.state.players[0].cash=10000;
        command=rich4_game_domain_command(GAME_COMMAND_USE_CARD,GAME_COMMAND_SOURCE_AI,
            0,rich4_game_domain_ref(GAME_DOMAIN_CARD,card),rich4_game_domain_ref(GAME_DOMAIN_LAND,0),-1);
        assert(rich4_card_use_begin(&session,&command,&map,0,now));
        if(card==7)assert(advance(&now)==RICH4_CARD_USE_WAIT); /* previous owner */
        assert(advance(&now)==RICH4_CARD_USE_COMMAND); /* no viewport/marker wait */
        assert(!session.presentation.card_use.flight_initialized);
    }
}

static void ai_defense_and_failure(void)
{
    GameCommand command;
    uint32_t now=100;
    setup();command=player_card(15);
    session.state.players[1].is_ai=1;
    session.state.players[1].card_inventory[0]=19;
    session.state.ai_rng_state=123;
    assert(rich4_card_use_begin(&session,&command,&map,0,now));
    assert(advance(&now)==RICH4_CARD_USE_WAIT);
    assert(session.state.players[1].relation_balance[0]==150);
    assert(session.presentation.card_use.defense.target==0);
    assert(session.presentation.card_use.defense.accepted);
    assert(session.state.ai_rng_state==123); /* relationship choice precedes random fallback */

    for(int extra=0;extra<2;extra++) {
        setup();now=100;command=player_card(25);
        session.state.players[1].is_ai=1;
        session.state.players[1].cash=20000+extra;
        session.state.players[1].card_inventory[0]=19;
        assert(rich4_card_use_begin(&session,&command,&map,0,now));
        (void)advance(&now);
        assert(session.presentation.card_use.defense.accepted==extra);
        assert(session.presentation.card_use.defense.phase==(extra ?
            GAME_PROPERTY_PRESENT_INSURANCE_CARD : GAME_PROPERTY_PRESENT_NONE));
    }
    setup();now=100;command=player_card(25);
    session.state.players[1].is_ai=1;
    session.state.players[1].cash=30000;
    session.state.players[1].card_inventory[0]=20;
    session.state.ai_rng_state=123;
    {
        Rich4Rng rng={123};
        (void)rich4_rng_original_state_range(&rng,3000);
        assert(rich4_card_use_begin(&session,&command,&map,0,now));
        assert(advance(&now)==RICH4_CARD_USE_WAIT);
        assert(session.state.ai_rng_state==rng.state);
        assert(session.presentation.card_use.defense.card==19 && session.presentation.card_use.defense.accepted);
    }
    setup();now=100;
    session.state.land_value[0]=1000;
    session.state.players[0].cash=999;
    session.state.players[0].card_inventory[0]=3;
    command=rich4_game_domain_command(GAME_COMMAND_USE_CARD,GAME_COMMAND_SOURCE_HUMAN,
        0,rich4_game_domain_ref(GAME_DOMAIN_CARD,2),rich4_game_domain_ref(GAME_DOMAIN_LAND,0),0);
    assert(rich4_card_use_begin(&session,&command,&map,0,now));
    assert(!session.presentation.speech.active);
    assert(rich4_card_use_update(&session,NULL,now)==RICH4_CARD_USE_COMMAND);
    rich4_card_use_command_completed(&session,0,now);
    assert(session.state.players[0].card_inventory[0]==3);
    assert(rich4_special_place_notice_current(&session.presentation));
}

int main(void)
{
    int card, kind, ai, direction;
    all_notices();
    passive_attack();
    tax_free_choice(0); tax_free_choice(1);
    tax_redirect_victim_speech(0); tax_redirect_victim_speech(1);
    tortoise_target_speech();
    revenge_continuation(15); revenge_continuation(16);
    group_effects();
    invitation_flight(); swap_houses(); target_effect_order();
    ai_defense_and_failure();
    for (card = 10; card <= 11; ++card)
        for (kind = 0; kind < 2; ++kind)
            for (ai = 0; ai <= 1; ++ai)
                for (direction = 0; direction < 8; ++direction)
                    destruction_sequence(card, kind ? GAME_DOMAIN_FACILITY : GAME_DOMAIN_LAND,
                        ai, direction);
    for (card = 23; card <= 24; ++card)
        for (int character = 0; character < 12; ++character)
            for (int skip = 0; skip < 2; ++skip)
                stock_card_sequence(card, character, skip);
    puts("card use: shared notices, passive choices, revenge, group effects, flights and finishing: ok");
    return 0;
}
