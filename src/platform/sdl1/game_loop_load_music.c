#include "platform/sdl1/game_loop_load_music.h"

#include "app/game_save.h"
#include "core/game_constants.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/game_loop_modal_music.h"
#include "platform/sdl1/runtime_bootstrap.h"

int game_restart_music_after_load(
    GameSession *session, Rich4Audio *audio, const char *music_dir,
    Rich4MusicBank music_bank, int music_enabled,
    int *main_map_music_track, int *main_map_music_active,
    int *bankruptcy_music_kind, int *confinement_music_kind,
    int *bank_music_active,
    int *lottery_music_active, int *magic_house_music_active,
    int *auction_music_active, int *holiday_music_active,
    int *shop_music_restore_pending)
{
    size_t track_count;
    int modal_music_active;

    if (!rich4_game_save_take_music_restart(session))
        return 0;
    modal_music_active = game_session_has_modal_music(session);

    /* These latches describe the audio backend, not the decoded session. */
    if (bankruptcy_music_kind != NULL)
        *bankruptcy_music_kind = GAME_BANKRUPTCY_MUSIC_NONE;
    if (confinement_music_kind != NULL)
        *confinement_music_kind = GAME_CONFINEMENT_NONE;
    if (bank_music_active != NULL) *bank_music_active = 0;
    if (lottery_music_active != NULL) *lottery_music_active = 0;
    if (magic_house_music_active != NULL) *magic_house_music_active = 0;
    if (auction_music_active != NULL) *auction_music_active = 0;
    if (holiday_music_active != NULL) *holiday_music_active = 0;
    if (shop_music_restore_pending != NULL)
        *shop_music_restore_pending = 0;
    /* A decoded game enters a new playlist transition at 0x401981; a
     * pre-load temporary-music bookmark cannot cross that boundary. */
    rich4_audio_clear_saved_music_position(audio);

    track_count = rich4_music_bank_track_count();
    if (main_map_music_track != NULL && track_count > 0) {
        /* 0x401981's load branch calls 0x454D91(0), whose zero argument
         * advances the eight-track playlist before starting playback. */
        *main_map_music_track =
            (*main_map_music_track + 1) % (int)track_count;
    }

    if (main_map_music_active == NULL)
        return 1;
    if (music_enabled && main_map_music_track != NULL &&
        !modal_music_active) {
        *main_map_music_active = start_music_bank_track(
            audio, music_dir, music_bank,
            (size_t)*main_map_music_track, "loaded main map");
    } else {
        *main_map_music_active = 0;
        rich4_audio_stop_music(audio);
    }
    return 1;
}
