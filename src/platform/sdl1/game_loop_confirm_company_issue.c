/* SDL input adapter for the app-owned company-issue flow. */

#include "platform/sdl1/game_loop_confirm_company_issue.h"

#include "app/special_presentation_flow.h"
#include "app/property_flow.h"
#include "app/stock_interaction_flow.h"
#include "app/game_speech_flow.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_turns.h"

int game_fortune_choice_at(GameSession *session, const GamePreviewAssets *assets,
    int x, int y)
{
    GameSpecialEventPresentationState *s = &session->presentation.special_event;
    int passive_actor = s->family == GAME_SPECIAL_EVENT_NEWS && s->resolved_id == 29
        ? s->reaction_player : s->actor;
    int slot;
    if (s->phase == RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET)
        return game_insurance_target_at(assets,&session->state,passive_actor,x,y);
    if (s->phase != RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET ||
        x < 19 || x >= 419 || y < 75 || y >= 243) return -1;
    slot = (y-75)/56*5+(x-19)/80;
    return session->state.players[s->fortune_card_target_player].card_inventory[slot]
        ? slot : -1;
}

int game_handle_fortune_choice_input(GameSession *session,
    const GamePreviewAssets *assets, int button, int x, int y, uint32_t now)
{
    GameSpecialEventPresentationState *s;
    int choice, count = 0, i;
    if (session == NULL) return 0;
    s = &session->presentation.special_event;
    if (!s->active || (s->phase != RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET &&
        s->phase != RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET)) return 0;
    if (button != SDL_BUTTON_LEFT && button != SDL_BUTTON_RIGHT) return 1;
    if (button == SDL_BUTTON_RIGHT) {
        (void)rich4_special_event_choice_select(session,-1,now);
        return 1;
    }
    choice = x < 0 ? s->fortune_choice_hover : game_fortune_choice_at(session,assets,x,y);
    if (s->phase == RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET)
        choice = s->fortune_choice_hover;
    if (s->phase == RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET) {
        int only = -1;
        int passive_actor = s->family == GAME_SPECIAL_EVENT_NEWS && s->resolved_id == 29
            ? s->reaction_player : s->actor;
        for (i = 0; i < session->state.player_count; ++i)
            if (i != passive_actor && (session->state.players[i].play_state & 0xff)) {
                count++; only = i;
            }
        if (count == 1) {
            Rich4ConfirmDialogHit hit = RICH4_CONFIRM_DIALOG_ACCEPT;
            if (x >= 0) {
                int left,top,w,h;
                hit = game_confirm_dialog_bounds(assets,GAME_COMMAND_PURCHASE_PROPERTY,
                    &left,&top,&w,&h) ? rich4_sdl1_confirm_dialog_hit(x,y,left,top,w,h)
                    : RICH4_CONFIRM_DIALOG_NONE;
            }
            if (hit == RICH4_CONFIRM_DIALOG_NONE) return 1;
            (void)rich4_special_event_choice_select(session,
                hit == RICH4_CONFIRM_DIALOG_ACCEPT ? only : -1,now);
            return 1;
        }
    }
    if (choice >= 0) (void)rich4_special_event_choice_select(session,choice,now);
    return 1;
}

int game_fortune_choice_press(GameSession *session, const GamePreviewAssets *assets,
    int button, int x, int y)
{
    if (session == NULL || !session->presentation.special_event.active ||
        session->presentation.special_event.phase != RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET)
        return 0;
    if (button == SDL_BUTTON_LEFT)
        (void)rich4_special_event_choice_press(session,
            game_fortune_choice_at(session,assets,x,y));
    return 1;
}

int game_handle_insurance_charge_input(GameSession *session,
    const GamePreviewAssets *assets, int button, int x, int y, uint32_t now)
{
    GamePropertyPresentationState *p;
    int target = -1, count = 0, i;
    Rich4ConfirmDialogHit hit = RICH4_CONFIRM_DIALOG_ACCEPT;
    if (session == NULL || !session->presentation.property.insurance_active) return 0;
    p = &session->presentation.property;
    if (session->presentation.speech.active) { rich4_speech_request_skip(session); return 1; }
    if (p->phase != GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM &&
        p->phase != GAME_PROPERTY_PRESENT_INSURANCE_TARGET) {
        (void)rich4_property_skip_wait(session, now); return 1;
    }
    if (button == SDL_BUTTON_RIGHT) {
        (void)rich4_property_insurance_choose(session, -1, now); return 1;
    }
    for (i = 0; i < session->state.player_count; i++)
        if (i != p->actor && (session->state.players[i].play_state & 0xff)) { count++; target = i; }
    if (p->phase == GAME_PROPERTY_PRESENT_INSURANCE_TARGET && count > 1) {
        target = x < 0 ? p->insurance_target : game_insurance_target_at(assets, &session->state, p->actor, x, y);
        if (target >= 0) (void)rich4_property_insurance_choose(session, target, now);
    } else {
        if (x >= 0) {
            int left, top, width, height;
            hit = game_confirm_dialog_bounds(assets, GAME_COMMAND_PURCHASE_PROPERTY, &left, &top, &width, &height) ?
                rich4_sdl1_confirm_dialog_hit(x, y, left, top, width, height) : RICH4_CONFIRM_DIALOG_NONE;
        }
        if (hit != RICH4_CONFIRM_DIALOG_NONE)
            (void)rich4_property_insurance_choose(session, hit == RICH4_CONFIRM_DIALOG_ACCEPT ?
                (p->phase == GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM ? p->actor : target) : -1, now);
    }
    return 1;
}

int game_company_issue_pointer(GameSession *session, const GamePreviewAssets *assets,
    Rich4Audio *audio, int action, int button, int x, int y)
{
    GameCompanyIssuePresentationState *issue;
    Rich4StockQuantityMask mask;
    Rich4AudioEventQueue *events;
    int control, i;
    static const int digits[9] = {7,8,9,4,5,6,1,2,3};
    if (session == NULL || !session->presentation.company_issue.active ||
        session->presentation.company_issue.phase != 1) return 0;
    issue = &session->presentation.company_issue;
    if (action == 2 && button == SDL_BUTTON_RIGHT) {
        (void)rich4_company_issue_finish(session, 0); rich4_audio_play_cancel(audio); return 1;
    }
    if (button != SDL_BUTTON_LEFT) return 1;
    if (assets == NULL) return 1;
    mask.available = game_asset_ready(assets, &assets->ui.has_panel_raw[22]);
    mask.data = assets->ui.panel_data[22]; mask.size = assets->ui.panel_size[22];
    /* 0x452C02 uses Panel[22]'s byte mask at the movable panel origin. */
    control = mask.available ? rich4_app_stock_quantity_control_at_mask(&mask,
        x - issue->entry_x + 256, y - issue->entry_y + 144) : -1;
    if (action == 0) {
        issue->entry_pressed = control;
        if (control == 1) {
            issue->entry_dragging = 1;
            issue->entry_dx = x - issue->entry_x; issue->entry_dy = y - issue->entry_y;
        }
    }
    if (action == 1 && issue->entry_dragging) {
        issue->entry_x = x - issue->entry_dx; issue->entry_y = y - issue->entry_dy;
        /* 0x45322D..0x453263 clamps the 128x192 window to 640x480. */
        if (issue->entry_x < 0) issue->entry_x = 0;
        if (issue->entry_x > 512) issue->entry_x = 512;
        if (issue->entry_y < 0) issue->entry_y = 0;
        if (issue->entry_y > 288) issue->entry_y = 288;
        return 1;
    }
    events = rich4_app_session_audio_borrow(session);
    if (action == 0 && control >= 2 && control <= 15)
        rich4_audio_event_emit(events, RICH4_AUDIO_EVENT_BANK_KEY, 1, 0);
    if (issue->entry_pressed == 16 && (action == 0 || action == 1)) {
        i = rich4_app_stock_quantity_from_bar(issue->maximum, x - issue->entry_x + 256);
        if (i >= 0 && i != issue->quantity) {
            issue->quantity = i;
            rich4_audio_event_emit(events, RICH4_AUDIO_EVENT_BANK_SLIDER, 1, 0);
        }
    }
    if (action == 2) {
        int pressed = issue->entry_pressed;
        issue->entry_dragging = 0; issue->entry_pressed = -1;
        if (pressed == control && control >= 2 && control <= 15) {
            if (control == 3) (void)rich4_company_issue_confirm(session, 1, NULL);
            else if (control == 2) rich4_company_issue_input(session, RICH4_COMPANY_ISSUE_MAX, 0, NULL);
            else if (control == 4) rich4_company_issue_input(session, RICH4_COMPANY_ISSUE_CLEAR, 0, NULL);
            else if (control == 5) rich4_company_issue_input(session, RICH4_COMPANY_ISSUE_DIGIT, 0, NULL);
            else if (control == 6) rich4_company_issue_input(session, RICH4_COMPANY_ISSUE_BACKSPACE, 0, NULL);
            else rich4_company_issue_input(session, RICH4_COMPANY_ISSUE_DIGIT, digits[control-7], NULL);
        }
    }
    game_session_audio_flush(session, audio);
    return 1;
}

static Rich4CompanyIssueInput company_issue_input_for_key(
    Rich4AppKey key, int *digit)
{
    if (key >= RICH4_APP_KEY_DIGIT_0 && key <= RICH4_APP_KEY_DIGIT_9) {
        if (digit != NULL) *digit = key - RICH4_APP_KEY_DIGIT_0;
        return RICH4_COMPANY_ISSUE_DIGIT;
    }
    if (key == RICH4_APP_KEY_BACKSPACE) return RICH4_COMPANY_ISSUE_BACKSPACE;
    if (key == RICH4_APP_KEY_C) return RICH4_COMPANY_ISSUE_CLEAR;
    if (key == RICH4_APP_KEY_M) return RICH4_COMPANY_ISSUE_MAX;
    if (key == RICH4_APP_KEY_LEFT || key == RICH4_APP_KEY_DOWN)
        return RICH4_COMPANY_ISSUE_DECREMENT;
    if (key == RICH4_APP_KEY_RIGHT || key == RICH4_APP_KEY_UP)
        return RICH4_COMPANY_ISSUE_INCREMENT;
    return (Rich4CompanyIssueInput)0;
}

int game_handle_company_issue_key_down(
    Rich4SceneId app_state, GameSession *session, VirtualCursor *cursor,
    Rich4Audio *audio, Rich4AppKey key, Rich4AppKey confirm_key,
    Rich4AppKey cancel_key)
{
    Rich4AudioEventQueue *events;
    Rich4CompanyIssueInput input;
    int digit = 0;
    if (app_state != RICH4_SCENE_GAME || session == NULL ||
        cursor == NULL || !session->presentation.company_issue.active ||
        session->presentation.company_issue.phase != 1)
        return 0;
    input = company_issue_input_for_key(key, &digit);
    events = rich4_app_session_audio_borrow(session);
    if (input != 0)
        rich4_company_issue_input(session, input, digit, events);
    game_session_audio_flush(session, audio);
    if (key == confirm_key)
        cursor->confirm_pressed = 1;
    else if (key == cancel_key)
        cursor->cancel_pressed = 1;
    return 1;
}

int game_handle_confirm_company_issue(
    GameConfirmCompanyIssueContext *context)
{
    Rich4ConfirmDialogHit hit = RICH4_CONFIRM_DIALOG_ACCEPT;
    if (context->app_state != RICH4_SCENE_GAME ||
        !context->session->presentation.company_issue.active)
        return 0;
    if (context->session->presentation.company_issue.phase == 1 &&
        context->cursor->confirm_pointer_pressed) {
        context->cursor->confirm_pressed = 0;
        return 1;
    }
    if (context->session->presentation.company_issue.phase == 0 &&
        context->cursor->confirm_pointer_pressed) {
        int left;
        int top;
        int width;
        int height;
        hit = game_confirm_dialog_bounds(
                  context->assets, GAME_COMMAND_PURCHASE_PROPERTY,
                  &left, &top, &width, &height)
            ? rich4_sdl1_confirm_dialog_hit(
                  context->cursor->x, context->cursor->y,
                  left, top, width, height)
            : RICH4_CONFIRM_DIALOG_NONE;
    }
    if (hit != RICH4_CONFIRM_DIALOG_NONE) {
        Rich4AudioEventQueue *events =
            rich4_app_session_audio_borrow(context->session);
        rich4_company_issue_confirm(
            context->session, hit == RICH4_CONFIRM_DIALOG_ACCEPT, events);
        game_session_audio_flush(context->session, context->audio);
    }
    context->cursor->confirm_pressed = 0;
    return 1;
}
