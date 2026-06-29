#ifndef RICH4_PLATFORM_SDL1_GAME_SPECIAL_EVENT_INTERNAL_H
#define RICH4_PLATFORM_SDL1_GAME_SPECIAL_EVENT_INTERNAL_H

#include "platform/sdl1/game_flow_api.h"

void rich4_sdl_play_special_event_body_voice(
    int family, int logical_id, int resolved_id, Rich4Audio *audio);
int rich4_sdl_start_special_event_confinement_animation(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now);
int rich4_sdl_start_special_event_fortune_animation(
    GameSession *session, const GamePreviewAssets *assets,
    Rich4Audio *audio, Uint32 now);
int rich4_sdl_apply_special_event_reaction(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now,
    const Rich4SpecialEventReactionResult *result);

#endif
