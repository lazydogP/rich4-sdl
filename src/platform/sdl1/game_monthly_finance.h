#ifndef RICH4_SDL1_GAME_MONTHLY_FINANCE_H
#define RICH4_SDL1_GAME_MONTHLY_FINANCE_H

#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/config_types.h"
#include "app/game_session.h"

int game_update_monthly_finance(GameSession *session,
    const GamePreviewAssets *assets, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now);
int draw_finance_panel_chunk_at(SDL_Surface *screen, const GamePreviewAssets *assets,
    int resource, size_t chunk, int x, int y, int transparent_zero);
void draw_periodic_finance(SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session);
int draw_monthly_award(SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session);
void draw_monthly_bank_speech(SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session);

#endif
