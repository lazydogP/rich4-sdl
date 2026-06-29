/* App-owned event-overlay snapshot builder.  It borrows immutable domain and
 * presentation state and exposes no SDL/resource types. */

#include "app/game_event_presentation_snapshot.h"

Rich4GameEventPresentationSnapshot
rich4_app_build_game_event_presentation_snapshot(
    const GameState *state, const GamePresentationState *presentation)
{
    Rich4GameEventPresentationSnapshot snapshot;
    snapshot.state = state;
    snapshot.presentation = presentation;
    return snapshot;
}
