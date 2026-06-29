#ifndef RICH4_PLATFORM_SDL1_GAME_VIEW_MODEL_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_VIEW_MODEL_INPUT_H

/* Assemble a narrow immutable view input from the platform session boundary.
 * The adapter owns no state; callers retain flow and presentation ownership. */

#include "core/game_flow_state.h"
#include "app/game_presentation.h"
#include "view/game_view_model.h"

void rich4_sdl1_game_view_model_input_build(const GameFlowState *flow,
                                            const GamePresentationState *presentation,
                                            GameViewModelInput *out_input);

#endif
