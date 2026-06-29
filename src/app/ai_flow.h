#ifndef RICH4_APP_AI_FLOW_H
#define RICH4_APP_AI_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/ai.h"

/* App-owned AI preaction orchestration.  The sink is the narrow command
 * application seam; SDL resources stay in the platform adapter. */
typedef struct Rich4AiCommandSink Rich4AiCommandSink;
typedef int (*Rich4AiCommandSubmit)(Rich4AiCommandSink *sink,
                                    const GameCommand *command,
                                    uint32_t now);

/* A deferred sink owns completion and the subsequent roll; stop selecting
 * commands while its modal presentation is pending. */
enum { RICH4_AI_COMMAND_DEFERRED = 2 };
/* Returns a typed count command only when the original preview changes it. */
int rich4_ai_prepare_move(GameSession *session, int player,
                          GameCommand *out_command);

struct Rich4AiCommandSink {
    Rich4AiCommandSubmit submit;
};

void rich4_ai_run_preactions(GameSession *session,
                             GameDomainRef current_property,
                             const uint8_t *player_on_map,
                             const GameAiMapTargetContext *target_context,
                             int player, uint32_t now,
                             Rich4AiCommandSink *sink);
void rich4_ai_run_preactions_with_projection(
    GameSession *session, GameDomainRef current_property,
    const uint8_t *player_on_map,
    const GameAiMapTargetContext *target_context,
    const GameAiGlobalProjection *projection, int player, uint32_t now,
    Rich4AiCommandSink *sink);

#endif
