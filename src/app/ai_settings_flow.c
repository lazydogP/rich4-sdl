#include "app/ai_settings_flow.h"

#include <string.h>

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

void rich4_ai_settings_reset(Rich4AiSettingsState *panel)
{
    if (panel != NULL) memset(panel, 0, sizeof(*panel));
}

void rich4_ai_settings_open(Rich4AiSettingsState *panel,
                            const GameSession *session)
{
    int i;

    if (panel == NULL || session == NULL || !session->state.active) return;
    rich4_ai_settings_reset(panel);
    panel->active = 1;
    panel->selected_row = 0;
    panel->hover = -1;
    panel->pressed_control = -1;
    /* 0x41E5A6: keep human identity bit 0, including hosted humans. */
    for (i = 0; i < clamp_int(session->state.player_count, 0,
                              GAME_MAX_PLAYERS); i++) {
        const GamePlayerState *player = &session->state.players[i];
        int row;
        if (!(player->play_state & 1)) continue;
        row = panel->player_count++;
        if (i == session->state.current_player) panel->selected_row = row;
        panel->player_index[row] = i;
        panel->hosted[row] = (player->play_state & 4) != 0;
        panel->action_flags[row] = player->ai_action_flags & 3;
        panel->personality[row] = clamp_int(player->ai_personality, 0, 2);
        panel->cash_ratio[row] = clamp_int(player->ai_cash_ratio, 0, 100);
        panel->stock_ratio[row] = clamp_int(player->ai_stock_ratio, 0, 100);
    }
}

void rich4_ai_settings_cancel(Rich4AiSettingsState *panel)
{
    if (panel != NULL) panel->active = 0;
}

void rich4_ai_settings_set_hover(Rich4AiSettingsState *panel, int hover)
{
    if (panel != NULL) panel->hover = hover;
}

int rich4_ai_settings_hit(int x, int y)
{
    static const int rects[13][4] = {
        {287,106,383,124}, {287,137,383,155},
        {287,194,383,212}, {287,226,383,244}, {287,259,383,277},
        {298,328,309,350}, {392,328,403,350},
        {298,361,309,383}, {392,361,403,383},
        {479,159,519,215}, {479,247,519,303},
        {310,327,390,351}, {310,360,390,384}
    };
    int i;

    for (i = 0; i < GAME_MAX_PLAYERS; i++)
        if (x > 110 && x < 226 && y > 70 + 83 * i &&
            y < 153 + 83 * i)
            return 100 + i;
    for (i = 0; i < 13; i++)
        if (x > rects[i][0] && x < rects[i][2] &&
            y > rects[i][1] && y < rects[i][3])
            return i;
    return -1;
}

int rich4_ai_settings_apply(Rich4AiSettingsState *panel,
                            GameSession *session, int control,
                            int pointer_x)
{
    int row;

    if (panel == NULL || session == NULL || !panel->active) return 0;
    row = panel->selected_row;
    if (control >= 100) {
        int selected = control - 100;
        if (selected >= panel->player_count) return 0;
        if (selected == row) panel->hosted[selected] ^= 1;
        panel->selected_row = selected;
        return 0;
    }
    if (control == 10) {
        panel->active = 0;
        return -1;
    }
    if (control != 9 && (row < 0 || row >= panel->player_count)) return 0;
    switch (control) {
    case 0: panel->action_flags[row] ^= 1; break;
    case 1: panel->action_flags[row] ^= 2; break;
    case 2:
    case 3:
    case 4: panel->personality[row] = control - 2; break;
    case 5:
        panel->cash_ratio[row] =
            clamp_int(panel->cash_ratio[row] - 10, 0, 100);
        break;
    case 6:
        panel->cash_ratio[row] =
            clamp_int(panel->cash_ratio[row] + 10, 0, 100);
        break;
    case 7:
        panel->stock_ratio[row] =
            clamp_int(panel->stock_ratio[row] - 10, 0, 100);
        break;
    case 8:
        panel->stock_ratio[row] =
            clamp_int(panel->stock_ratio[row] + 10, 0, 100);
        break;
    case 9:
        for (row = 0; row < panel->player_count; row++) {
            GamePlayerState *player =
                &session->state.players[panel->player_index[row]];
            int was_hosted = (player->play_state & 4) != 0;
            /* 0x41DEED / 0x41E25F: change bit 2, retain human identity. */
            player->play_state = (player->play_state & ~4) |
                                 (panel->hosted[row] ? 4 : 0);
            player->is_ai = (player->play_state & 6) != 0;
            /* 0x417DFF calls 0x40DEFE after the panel returns; 0x40DF30
             * marks the current actor for dispatch again. Human input has
             * already consumed this flag at turn start, so merely setting
             * is_ai would lock the UI without ever scheduling AI actions. */
            if (panel->player_index[row] == session->state.current_player &&
                (player->is_ai || was_hosted != panel->hosted[row]))
                session->presentation.actors[panel->player_index[row]].dirty_flags |=
                    GAME_PLAYER_DIRTY_TURN;
            player->ai_action_flags =
                (player->ai_action_flags & ~3) | panel->action_flags[row];
            player->ai_personality = panel->personality[row];
            player->ai_cash_ratio = panel->cash_ratio[row];
            player->ai_stock_ratio = panel->stock_ratio[row];
        }
        panel->active = 0;
        return 1;
    case 10:
        panel->active = 0;
        return -1;
    case 11:
        panel->cash_ratio[row] = clamp_int(
            ((pointer_x - 310 + 4) / 8) * 10, 0, 100);
        break;
    case 12:
        panel->stock_ratio[row] = clamp_int(
            ((pointer_x - 310 + 4) / 8) * 10, 0, 100);
        break;
    default:
        break;
    }
    return 0;
}
