#include "app/bank_flow.h"

#include "core/game_command.h"
#include "core/game_event_payload.h"
#include "core/rng.h"
#include "data/character_speech.h"

enum {
    RICH4_BANK_EYE_IDLE = 0,
    RICH4_BANK_EYE_BLINK,
    RICH4_BANK_EYE_GLANCE
};

/* 0x46484C..0x464971, referenced by sub_435062. */
static const Rich4CharacterSpeech bank_teller_speech[11] = {
    {75,"#0075\xc5\x77\xaa\xef\xa5\xfa\xc1\x7b\x0a\xa4\x6a\xb4\x49\xaf\xce\xbb\xc8\xa6\xe6\xa1\x49"},
    {76,"#0076\xbb\xdd\xad\x6e\xa7\xda\xac\xb0\xb1\x7a\x0a\xaa\x41\xb0\xc8\xb6\xdc\xa1\x48"},
    {77,"#0077\xb1\x7a\xaa\xba\xb6\x55\xb4\xda\xc3\x42\xab\xd7\x0a\xac\xb0\xa4\x40\xa6\xca\xb8\x55\xa4\xb8\xa1\x43"},
    {78,"#0078\xbd\xd0\xbf\xe9\xa4\x4a\xb1\x7a\xad\x6e\xb6\x55\xb4\xda\x0a\xaa\xba\xaa\xf7\xc3\x42\xa1\x49"},
    {79,"#0079\xb1\x7a\xaa\xba\xb6\x55\xb4\xda\xa4\xe2\xc4\xf2\x0a\xa4\x77\xb8\x67\xa7\xb9\xa6\xa8\xa1\x43"},
    {80,"#0080\xbd\xd0\xa9\xf3\xa4\x54\xad\xd3\xa4\xeb\xa4\xba\x0a\xc1\xd9\xb2\x4d\xb6\x55\xb4\xda\xa1\x43"},
    {81,"#0081\xab\xdc\xa9\xea\xba\x70\xa1\x49\x0a\xaa\xf7\xc3\x42\xa4\x77\xb6\x57\xb9\x4c\x0a\xb3\x5c\xa5\x69\xc3\x42\xab\xd7\xa1\x43"},
    {82,"#0082\xbd\xd0\xbf\xe9\xa4\x4a\xb1\x7a\xb9\x77\xb3\xc6\x0a\xc1\xd9\xb4\xda\xaa\xba\xaa\xf7\xc3\x42\xa1\x49"},
    {83,"#0083\xab\xdc\xa9\xea\xba\x70\xa1\x49\x0a\xb1\x7a\xaa\xba\xb2\x7b\xaa\xf7\xa4\xa3\xa8\xac\xa1\x43"},
    {84,"#0084\xb1\x7a\xaa\xba\xc1\xd9\xb4\xda\xa4\xe2\xc4\xf2\x0a\xa4\x77\xa7\xb9\xa6\xa8\xa1\x43"},
    {85,"#0085\xc1\xc2\xc1\xc2\xb1\x7a\xaa\xba\xb4\x66\xc5\x55\xa1\x49"}
};

static const Rich4CharacterSpeech *bank_speech(int resource)
{
    return resource >= 75 && resource <= 85
        ? &bank_teller_speech[resource - 75] : NULL;
}

/* 0x464AEE, 0x464A2D, 0x464A4B.  These messages have no voice marker. */
static const Rich4CharacterSpeech loan_reminder_speech[3] = {
    {0, "%s\xb1\x7a\xa6\x6e"},
    {0, "\xb1\x7a\xa6\x56\xbb\xc8\xa6\xe6\xad\xc9\xb6\x55\xaa\xba\n"
        "\xb6\x55\xb4\xda\xa7\x59\xb1\x4e\xa8\xec\xb4\xc1\xa1\x43"},
    {0, "\xbd\xd0\xa4\xa3\xad\x6e\xa7\xd1\xb0\x4f\xb3\xe1\xa1\x49"}
};

int rich4_bank_flow_loan_reminder_active(const GameSession *session)
{
    return session != NULL && session->presentation.bank_loan_due.active &&
           session->presentation.bank_loan_due.days == 3;
}

static void bank_start_reminder_speech(
    GameBankInteractionPresentationState *bank, int index, uint32_t now)
{
    bank->speech = &loan_reminder_speech[index];
    (void)rich4_character_speech_snapshot_capture(
        &bank->speech_snapshot, bank->speech);
    bank->speech_pending = 0;
    bank->speech_tick = now;
}

static int bank_is_loan_interaction(const GameSession *session)
{
    return session != NULL &&
        session->presentation.route_interaction.active &&
        session->presentation.route_interaction.accept.type ==
            GAME_COMMAND_BANK_APPLY_LOAN;
}

static void bank_reset_face_animation(
    GameBankInteractionPresentationState *bank, int special_page,
    uint32_t now)
{
    bank->face_chunk = -1;
    bank->mouth_chunk = -1;
    bank->mouth_ticks = 0;
    bank->eye_mode = RICH4_BANK_EYE_IDLE;
    bank->eye_phase = 0;
    bank->eye_current = 0;
    bank->animation_page = special_page;
    bank->anim_tick = now;
}

static void bank_update_teller_eyes(GameSession *session, GameBankInteractionPresentationState *bank)
{
    static const int blink_faces[4] = { 7, 6, 7, 5 };
    if (bank->eye_mode == RICH4_BANK_EYE_BLINK) {
        bank->face_chunk = blink_faces[bank->eye_phase++];
        if (bank->eye_phase >= 4) {
            bank->eye_mode = RICH4_BANK_EYE_IDLE;
            bank->eye_phase = 0;
            bank->eye_current = 2;
        }
    } else if (bank->eye_mode == RICH4_BANK_EYE_GLANCE) {
        int next = rich4_rng_next_at(&session->state.ai_rng_state) % 3;
        if (next != bank->eye_current) {
            bank->eye_current = next;
            bank->face_chunk = next + 3;
            bank->eye_mode = RICH4_BANK_EYE_IDLE;
        }
    } else {
        int transition = rich4_rng_next_at(&session->state.ai_rng_state) >> 10;
        if (transition == 0) {
            bank->eye_mode = RICH4_BANK_EYE_BLINK;
            bank->eye_phase = 0;
        } else if (transition == 1) {
            bank->eye_mode = RICH4_BANK_EYE_GLANCE;
        }
    }
}

static void bank_update_teller_mouth(
    GameSession *session, GameBankInteractionPresentationState *bank)
{
    if (bank->speech == NULL && bank->mouth_ticks <= 0) return;
    if (bank->mouth_ticks > 0) {
        bank->mouth_ticks--;
        if (bank->mouth_ticks == 0) bank->mouth_chunk = 8;
        return;
    }
    if ((rich4_rng_next_at(&session->state.ai_rng_state) >> 11) < 4) {
        bank->mouth_chunk = 9 + (rich4_rng_next_at(&session->state.ai_rng_state) & 1);
        bank->mouth_ticks = (rich4_rng_next_at(&session->state.ai_rng_state) & 7) + 1;
    }
}

static void bank_update_manager_eyes(GameSession *session, GameBankInteractionPresentationState *bank)
{
    static const int blink_faces[4] = { 12, 11, 12, -1 };
    if (bank->eye_mode == RICH4_BANK_EYE_BLINK) {
        bank->face_chunk = blink_faces[bank->eye_phase++];
        if (bank->eye_phase >= 4) {
            bank->eye_mode = RICH4_BANK_EYE_IDLE;
            bank->eye_phase = 0;
        }
    } else if ((rich4_rng_next_at(&session->state.ai_rng_state) >> 10) == 0) {
        bank->eye_mode = RICH4_BANK_EYE_BLINK;
        bank->eye_phase = 0;
    }
}

static void bank_emit_speech(
    GameBankInteractionPresentationState *bank,
    Rich4AudioEventQueue *audio_events)
{
    if (bank->speech_pending && bank->speech != NULL &&
        audio_events != NULL) {
        rich4_audio_event_emit(
            audio_events, RICH4_AUDIO_EVENT_SPEECH_START, 1,
            bank->speech->speaking_resource);
        bank->speech_pending = 0;
    }
}

static void bank_start_speech(
    GameBankInteractionPresentationState *bank, int resource, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    bank->speech = bank_speech(resource);
    (void)rich4_character_speech_snapshot_capture(
        &bank->speech_snapshot, bank->speech);
    bank->speech_tick = now;
    bank->speech_pending = bank->speech != NULL;
    bank_emit_speech(bank, audio_events);
}

static int bank_finish_speech_if_ready(
    GameBankInteractionPresentationState *bank, uint32_t now,
    int speech_playing)
{
    if (bank->speech == NULL) return 1;
    if (bank->speech_pending || now - bank->speech_tick < 2000u ||
        speech_playing)
        return 0;
    bank->speech = NULL;
    rich4_character_speech_snapshot_clear(&bank->speech_snapshot);
    bank->mouth_chunk = 8;
    bank->mouth_ticks = 0;
    return 1;
}

static int bank_update_card(GameBankInteractionPresentationState *bank)
{
    if (bank->card_velocity == 0) return 0;
    bank->card_x += bank->card_velocity;
    if (bank->card_velocity < 0 && bank->card_x <= 440) {
        bank->card_x = 440;
        bank->card_velocity = 0;
        return 1;
    }
    if (bank->card_velocity > 0 && bank->card_x >= 640) {
        bank->card_x = 640;
        bank->card_velocity = 0;
        return 1;
    }
    return 0;
}

void rich4_bank_flow_begin_interaction(
    GameSession *session, int accept_type, const RouteEvent *route_event,
    uint32_t now, int animation_enabled,
    Rich4AudioEventQueue *audio_events)
{
    GameBankInteractionPresentationState *bank;
    if (session == NULL) return;
    bank = &session->presentation.bank_interaction;
    /* The stop dispatcher owns the shared entry sound, not either bank page. */
    (void)route_event;
    bank->mode = 0;
    bank->amount = 0;
    bank->pressed_control = -1;
    bank->special_page = 0;
    bank->amount_entry = 0;
    bank->card_x = accept_type == GAME_COMMAND_BANK_APPLY_LOAN ? 640 : 0;
    bank->card_velocity = 0;
    bank->success_close = 0;
    bank->speech = NULL;
    rich4_character_speech_snapshot_clear(&bank->speech_snapshot);
    bank->speech_pending = 0;
    bank_reset_face_animation(bank, 0, now);
    bank->started = now;
    if (accept_type == GAME_COMMAND_BANK_APPLY_LOAN) {
        bank->stage = animation_enabled
            ? RICH4_BANK_PRESENT_GREETING_1 : RICH4_BANK_PRESENT_GREETING_2;
        if (animation_enabled)
            bank_start_speech(bank, 75, now, audio_events);
    } else {
        bank->stage = RICH4_BANK_PRESENT_READY;
    }
}

int rich4_bank_flow_begin_amount_prompt(
    GameSession *session, int mode, uint32_t now)
{
    GameBankInteractionPresentationState *bank;
    if (!bank_is_loan_interaction(session) || (mode != 0 && mode != 1))
        return 0;
    bank = &session->presentation.bank_interaction;
    if (bank->stage != RICH4_BANK_PRESENT_READY || bank->special_page)
        return 0;
    bank->mode = mode;
    bank->amount = 0;
    bank->amount_entry = 0;
    bank->pressed_control = -1;
    bank->card_x = 640;
    bank->card_velocity = -40;
    bank->success_close = 0;
    bank->stage = mode == 0
        ? RICH4_BANK_PRESENT_LOAN_PROMPT
        : RICH4_BANK_PRESENT_REPAY_PROMPT;
    bank_start_speech(bank, mode == 0 ? 78 : 82, now, NULL);
    return 1;
}

int rich4_bank_flow_begin_result(
    GameSession *session, GameCommandType command_type, uint32_t now)
{
    GameBankInteractionPresentationState *bank;
    int resource;
    if (!bank_is_loan_interaction(session) ||
        (command_type != GAME_COMMAND_BANK_APPLY_LOAN &&
         command_type != GAME_COMMAND_BANK_REPAY_LOAN))
        return 0;
    bank = &session->presentation.bank_interaction;
    if (!bank->amount_entry ||
        (bank->stage != RICH4_BANK_PRESENT_LOAN_AMOUNT &&
         bank->stage != RICH4_BANK_PRESENT_REPAY_AMOUNT))
        return 0;
    resource = command_type == GAME_COMMAND_BANK_APPLY_LOAN ? 79 : 84;
    bank->amount_entry = 0;
    bank->pressed_control = -1;
    bank->card_x = 440;
    bank->card_velocity = 0;
    bank->success_close = 1;
    bank->stage = RICH4_BANK_PRESENT_LOAN_RESULT_1;
    bank_start_speech(bank, resource, now, NULL);
    return 1;
}

void rich4_bank_flow_cancel_amount_entry(GameSession *session, uint32_t now)
{
    GameBankInteractionPresentationState *bank;
    if (!bank_is_loan_interaction(session)) return;
    bank = &session->presentation.bank_interaction;
    bank->amount_entry = 0;
    bank->amount = 0;
    bank->pressed_control = -1;
    bank->speech = NULL;
    rich4_character_speech_snapshot_clear(&bank->speech_snapshot);
    bank->speech_pending = 0;
    bank->success_close = 0;
    bank->card_velocity = bank->card_x < 640 ? 40 : 0;
    bank->stage = bank->card_velocity != 0
        ? RICH4_BANK_PRESENT_RETRACT : RICH4_BANK_PRESENT_READY;
    bank->anim_tick = now;
}

void rich4_bank_flow_begin_loan_due(
    GameSession *session, const GameEvent *event, uint32_t now)
{
    GameBankLoanDuePresentationState *due;
    if (session == NULL || event == NULL) return;
    due = &session->presentation.bank_loan_due;
    due->active = 1;
    due->player = event->player;
    due->days = rich4_game_event_bank_loan_days(event);
    due->principal = rich4_game_event_bank_loan_principal(event);
    due->forced = rich4_game_event_bank_loan_forced(event) != 0;
    due->payment = rich4_game_event_bank_loan_payment(event);
    due->until = now + 1500;
    if (due->days == 3) {
        rich4_bank_flow_begin_interaction(
            session, GAME_COMMAND_BANK_APPLY_LOAN, NULL, now, 0, NULL);
        session->presentation.bank_interaction.card_x = 440;
        session->presentation.bank_interaction.stage = RICH4_BANK_PRESENT_DUE_GREETING;
        bank_start_reminder_speech(&session->presentation.bank_interaction, 0, now);
    }
}

int rich4_bank_flow_loan_reminder_click(
    GameSession *session, int cancel, uint32_t now)
{
    GameBankInteractionPresentationState *bank;
    if (!rich4_bank_flow_loan_reminder_active(session)) return 0;
    bank = &session->presentation.bank_interaction;
    /* 0x4365B4 ignores a second cancel during the final message. */
    if (cancel && bank->stage >= RICH4_BANK_PRESENT_DUE_GOODBYE) return 1;
    rich4_audio_event_emit(&session->audio_events,
        cancel ? RICH4_AUDIO_EVENT_CANCEL : RICH4_AUDIO_EVENT_OK, 1, 0);
    rich4_audio_event_emit(&session->audio_events, RICH4_AUDIO_EVENT_SPEECH_STOP, 1, 0);
    if (cancel) bank->stage = RICH4_BANK_PRESENT_DUE_EXIT;
    bank->speech_tick = now - 2000u;
    return 1;
}

static int bank_update_loan_reminder(GameSession *session, uint32_t now)
{
    GameBankInteractionPresentationState *bank = &session->presentation.bank_interaction;
    if (now - bank->anim_tick < 50u) return 1;
    bank->anim_tick = now;
    if (bank_finish_speech_if_ready(bank, now, 0)) {
        if (bank->stage == RICH4_BANK_PRESENT_DUE_GREETING) {
            bank->stage = RICH4_BANK_PRESENT_DUE_WARNING;
            bank_start_reminder_speech(bank, 1, now);
        } else if (bank->stage == RICH4_BANK_PRESENT_DUE_WARNING) {
            bank->stage = RICH4_BANK_PRESENT_DUE_GOODBYE;
            bank_start_reminder_speech(bank, 2, now);
        } else {
            session->presentation.bank_loan_due.active = 0;
            bank->stage = RICH4_BANK_PRESENT_IDLE;
            return 0;
        }
    }
    /* 0x43624E..0x43625A skips both face animations on alternate ticks. */
    bank->animation_page ^= 1;
    if (bank->animation_page) return 1;
    bank_update_teller_eyes(session, bank);
    bank_update_teller_mouth(session, bank);
    return 1;
}

int rich4_bank_flow_update(
    GameSession *session, uint32_t now, int speech_playing,
    Rich4AudioEventQueue *audio_events)
{
    if (session == NULL) return 0;
    if (rich4_bank_flow_loan_reminder_active(session))
        return bank_update_loan_reminder(session, now);
    if (bank_is_loan_interaction(session)) {
        GameBankInteractionPresentationState *bank =
            &session->presentation.bank_interaction;
        uint32_t interval = bank->special_page ? 100u : 50u;
        int card_finished;
        bank_emit_speech(bank, audio_events);
        if (bank->animation_page != bank->special_page)
            bank_reset_face_animation(bank, bank->special_page, now);
        if (now - bank->anim_tick < interval) return 1;

        /* Win32 WM_TIMER coalesces overdue messages.  Advance exactly one
         * original timer step instead of replaying a burst of blinks. */
        bank->anim_tick = now;
        card_finished = bank_update_card(bank);
        if (!bank->amount_entry) {
            if (bank->special_page)
                bank_update_manager_eyes(session, bank);
            else {
                bank_update_teller_eyes(session, bank);
                bank_update_teller_mouth(session, bank);
            }
        }

        switch (bank->stage) {
        case RICH4_BANK_PRESENT_GREETING_1:
            if (bank_finish_speech_if_ready(bank, now, speech_playing)) {
                bank->stage = RICH4_BANK_PRESENT_GREETING_2;
                bank_start_speech(bank, 76, now, audio_events);
            }
            break;
        case RICH4_BANK_PRESENT_GREETING_2:
            if (bank_finish_speech_if_ready(bank, now, speech_playing))
                bank->stage = RICH4_BANK_PRESENT_READY;
            break;
        case RICH4_BANK_PRESENT_LOAN_PROMPT:
        case RICH4_BANK_PRESENT_REPAY_PROMPT:
            if (bank->card_velocity == 0 &&
                bank_finish_speech_if_ready(bank, now, speech_playing)) {
                bank->amount_entry = 1;
                bank->stage = bank->stage == RICH4_BANK_PRESENT_LOAN_PROMPT
                    ? RICH4_BANK_PRESENT_LOAN_AMOUNT
                    : RICH4_BANK_PRESENT_REPAY_AMOUNT;
            }
            break;
        case RICH4_BANK_PRESENT_LOAN_RESULT_1:
            if (bank_finish_speech_if_ready(bank, now, speech_playing)) {
                bank->stage = RICH4_BANK_PRESENT_LOAN_RESULT_2;
                bank_start_speech(
                    bank, bank->mode == 0 ? 80 : 85, now, audio_events);
            }
            break;
        case RICH4_BANK_PRESENT_LOAN_RESULT_2:
            if (bank_finish_speech_if_ready(bank, now, speech_playing)) {
                bank->card_velocity = 40;
                bank->stage = RICH4_BANK_PRESENT_RETRACT;
            }
            break;
        case RICH4_BANK_PRESENT_RETRACT:
            if (card_finished ||
                (bank->card_velocity == 0 && bank->card_x >= 640)) {
                bank->stage = bank->success_close
                    ? RICH4_BANK_PRESENT_EXIT : RICH4_BANK_PRESENT_READY;
                /* Match sub_435062: the state-11 close is observed on the
                 * timer message after the final 640px card position. */
                return 1;
            }
            break;
        case RICH4_BANK_PRESENT_EXIT:
            bank->speech = NULL;
            rich4_character_speech_snapshot_clear(&bank->speech_snapshot);
            bank->speech_pending = 0;
            bank->amount_entry = 0;
            bank->pressed_control = -1;
            bank->stage = RICH4_BANK_PRESENT_IDLE;
            session->presentation.route_interaction.active = 0;
            return 0;
        default:
            break;
        }
        return 1;
    }
    if (session->presentation.bank_settlement.active) {
        if ((int32_t)(now - session->presentation.bank_settlement.until) < 0) {
            while (now - session->presentation.bank_settlement.tick >= 100) {
                session->presentation.bank_settlement.frame++;
                session->presentation.bank_settlement.tick += 100;
            }
            return 1;
        }
        session->presentation.bank_settlement.active = 0;
    }
    if (session->presentation.bank_loan_due.active) {
        if ((int32_t)(now - session->presentation.bank_loan_due.until) < 0)
            return 1;
        session->presentation.bank_loan_due.active = 0;
    }
    return 0;
}
