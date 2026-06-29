#ifndef RICH4_PLATFORM_SDL1_GAME_EVENT_PRESENTATION_H
#define RICH4_PLATFORM_SDL1_GAME_EVENT_PRESENTATION_H

#include <SDL/SDL.h>

#include "app/game_event_presentation_snapshot.h"
#include "platform/sdl1/app_asset_types.h"

/* sub_441F73's 165x256 card-art template, shared by the card-reward overlay
 * below and the not-yet-extracted card-catalog panel's asset size check. */
enum {
    ORIGINAL_CARD_ART_WIDTH = 165,
    ORIGINAL_CARD_ART_HEIGHT = 256
};

/* R10.1c: event/notification presentation cluster, moved verbatim out of
 * runtime_main.c. Unlike the ViewModel-only scenes (game_hud_render,
 * game_map_render, ...), these are one-shot overlays keyed off notification
 * state (bank/confinement/lottery/shop/etc.). The app builds one immutable
 * event snapshot; each independent overlay consumes that snapshot plus its
 * platform assets in the original fixed frame sequence. */

void draw_original_card_presentation(SDL_Surface *screen,
                                     const GamePreviewAssets *assets,
                                     int card,
                                     const char *title_big5);

void draw_special_reward_presentation(SDL_Surface *screen,
                                      const GamePreviewAssets *assets,
                                      const Rich4GameEventPresentationSnapshot *snapshot);

void draw_bank_rejection_presentation(SDL_Surface *screen,
                                      const GamePreviewAssets *assets,
                                      const Rich4GameEventPresentationSnapshot *snapshot);

void draw_bankruptcy_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot);

void draw_bank_settlement_presentation(SDL_Surface *screen,
                                       const GamePreviewAssets *assets,
                                       const Rich4GameEventPresentationSnapshot *snapshot);

void draw_bank_loan_due_presentation(SDL_Surface *screen,
                                     const GamePreviewAssets *assets,
                                     const Rich4GameEventPresentationSnapshot *snapshot);

void draw_insurance_notice(SDL_Surface *screen,
                           const GamePreviewAssets *assets,
                           const Rich4GameEventPresentationSnapshot *snapshot);

void draw_lottery_visit_panel(SDL_Surface *screen,
                              const GamePreviewAssets *assets,
                              const Rich4GameEventPresentationSnapshot *snapshot);

void draw_special_event_presentation(SDL_Surface *screen,
                                     const GamePreviewAssets *assets,
                                     const Rich4GameEventPresentationSnapshot *snapshot);
void draw_fortune_passive_choice(SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot);

void draw_god_money_roulette(SDL_Surface *screen,
    const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot);
void draw_facility_roulette(SDL_Surface *screen,
                            const GamePreviewAssets *assets,
                            const Rich4GameEventPresentationSnapshot *snapshot);

void draw_company_issue(SDL_Surface *screen,
                        const GamePreviewAssets *assets,
                        const Rich4GameEventPresentationSnapshot *snapshot);
void draw_insurance_charge(SDL_Surface *screen, const GamePreviewAssets *assets,
                           const Rich4GameEventPresentationSnapshot *snapshot);
int game_insurance_target_at(const GamePreviewAssets *assets,
                            const GameState *state, int actor, int x, int y);

void draw_magic_house(SDL_Surface *screen,
                      const GamePreviewAssets *assets,
                      const Rich4GameEventPresentationSnapshot *snapshot);

void draw_auction_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot);

void draw_special_actor_action_presentation(SDL_Surface *screen,
                                            const GamePreviewAssets *assets,
                                            const Rich4GameEventPresentationSnapshot *snapshot);

void draw_shop_visit(SDL_Surface *screen,
                     const GamePreviewAssets *assets,
                     const Rich4GameEventPresentationSnapshot *snapshot);

void draw_god_presentation(SDL_Surface *screen,
                           const GamePreviewAssets *assets,
                           const Rich4GameEventPresentationSnapshot *snapshot);

#endif
