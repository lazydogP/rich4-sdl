#ifndef RICH4_APP_AI_SETTINGS_FLOW_H
#define RICH4_APP_AI_SETTINGS_FLOW_H

/* App-owned AI settings state and editing semantics.  SDL supplies only
 * pointer coordinates; drawing remains in the platform adapter. Evidence:
 * A5/A6; tests/unit/test_ai_settings_flow.c. */
#include "app/ai_settings_state.h"
#include "app/game_session.h"

void rich4_ai_settings_reset(Rich4AiSettingsState *panel);
void rich4_ai_settings_open(Rich4AiSettingsState *panel,
                            const GameSession *session);
void rich4_ai_settings_cancel(Rich4AiSettingsState *panel);
void rich4_ai_settings_set_hover(Rich4AiSettingsState *panel, int hover);
int rich4_ai_settings_hit(int x, int y);
int rich4_ai_settings_apply(Rich4AiSettingsState *panel,
                            GameSession *session, int control,
                            int pointer_x);

#endif
