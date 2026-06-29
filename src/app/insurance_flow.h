#ifndef RICH4_APP_INSURANCE_FLOW_H
#define RICH4_APP_INSURANCE_FLOW_H

/* Insurance presentation flow owns the bounded FIFO and its time windows.
 * It borrows GameSession and GameEvent; SDL only renders the current entry.
 * Tests: test_insurance_flow. */
#include <stdint.h>

#include "app/game_session.h"
#include "core/game_event.h"

void rich4_insurance_flow_enqueue_notice(
    GameSession *session, const GameEvent *event, uint32_t now);
/* Hospital continuation: show the notice before paying (0x43EDF8). */
void rich4_insurance_flow_enqueue_confinement_claim(
    GameSession *session, const GameEvent *event, uint32_t now);
int rich4_insurance_flow_resume_special_claim(GameSession *session, uint32_t now);
int rich4_insurance_flow_update(GameSession *session, uint32_t now);
int rich4_insurance_flow_skip(GameSession *session, uint32_t now);

#endif
