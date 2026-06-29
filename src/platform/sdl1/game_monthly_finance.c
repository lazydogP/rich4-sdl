#include "platform/sdl1/game_lazy_assets.h"
#include "platform/sdl1/game_monthly_finance.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"
#include "app/monthly_finance_flow.h"
#include "app/bankruptcy_flow.h"
#include "core/game_event_payload.h"

static void audio_requests(GameSession *s, Rich4Audio *audio, int enabled)
{
    int voice = rich4_monthly_finance_take_speech(s, enabled);
    int effect = rich4_monthly_finance_take_effect(s);
    if (voice < 0) rich4_audio_stop_speech(audio);
    else if (voice > 0) (void)rich4_audio_play_speech(audio, voice);
    if (effect != 0) rich4_audio_play_finance_effect(audio, effect);
}

int game_update_monthly_finance(GameSession *s,
    const GamePreviewAssets *assets, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now)
{
    int enabled = config == NULL || config->animation != 0;
    if (s == NULL) return 0;
    if (s->presentation.bank_settlement.active &&
        s->presentation.bank_settlement.stage != 0) {
        const GameBankSettlementPresentationState *p = &s->presentation.bank_settlement;
        int frames = 0, delay = 71, player = -1, kind = 0;
        if (enabled && assets != NULL)
            (void)game_asset_ready(assets, &assets->ui.has_panel[25]);
        if (p->stage == 8) { player = p->unlucky_player; kind = 1; }
        if (p->stage == 18) player = p->richest_player;
        if (assets != NULL && player >= 0 && player < s->state.player_count) {
            int c = s->state.players[player].character;
            if (c >= 0 && c < 12) {
                frames = game_asset_graph_frames(assets, &assets->ui.monthly_award_data[c][kind], assets->ui.monthly_award_size[c][kind]);
                delay = game_asset_graph_delay(assets, &assets->ui.monthly_award_data[c][kind], assets->ui.monthly_award_size[c][kind]);
            }
        }
        audio_requests(s, audio, enabled);
        now = rich4_sdl1_ticks();
        int active = rich4_monthly_finance_update(s, now, enabled,
            rich4_audio_speech_playing(audio), frames, delay);
        if (!p->active) rich4_audio_stop_finance_effects(audio,0);
        return active;
    }
    if (s->presentation.periodic_finance.active &&
        !s->presentation.bankruptcy.active && !s->presentation.bankruptcy.awaiting_auction &&
        !s->presentation.auction.active) {
        const GamePeriodicFinancePresentationState *p = &s->presentation.periodic_finance;
        GameEventQueue events;
        int frames = 0, delay = 71, resource = p->phase == 3 ? 16 : 17;
        int active;
        int previous_phase = p->phase;
        if (enabled && assets != NULL)
            (void)game_asset_ready(assets, &assets->ui.has_panel[
                p->phase == RICH4_FINANCE_DIVIDEND ? 76 : 15]);
        if (assets != NULL && (p->phase == 3 || p->phase == 5)) {
            frames = game_asset_graph_frames(assets, &assets->ui.panel_data[resource], assets->ui.panel_size[resource]);
            delay = game_asset_graph_delay(assets, &assets->ui.panel_data[resource], assets->ui.panel_size[resource]);
        }
        rich4_game_event_queue_reset(&events);
        audio_requests(s, audio, enabled);
        now = rich4_sdl1_ticks();
        active = rich4_periodic_finance_update(s, now, enabled,
            rich4_audio_speech_playing(audio), frames, delay, &events);
        if (previous_phase == RICH4_FINANCE_DIVIDEND && p->phase != previous_phase)
            rich4_audio_stop_finance_effects(audio,61);
        if (!p->active) rich4_audio_stop_finance_effects(audio,0);
        rich4_bankruptcy_capture_events(s, &events, now);
        return active;
    }
    return 0;
}
