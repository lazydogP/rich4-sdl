/* Actual archive/pixel/audio test. Stall injection uses SDL's real clock;
 * it does not use the smoke runner's deterministic clock. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#undef main
#include "app/tool_animation_flow.h"
#include "app/special_presentation_flow.h"
#include "app/special_event_timeline.h"
#include "app/intro_flow.h"
#include "app/property_flow.h"
#include "app/card_panel_flow.h"
#include "app/dice_flow.h"
#include "core/game_event_payload.h"
#include "core/special_event.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_lazy_assets.h"
#include "platform/sdl1/game_special_event_internal.h"
#include "platform/sdl1/render/game_flc_cache.h"
#include "platform/sdl1/audio/audio_sdl_mixer_internal.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"

static unsigned reads;
static size_t last_read;
static Rich4Audio *page_audio;
static int page_preparation_reads;
static void resource_read(void *context, int phase, const char *archive,
    size_t index, size_t bytes, Rich4MkfError error)
{
    (void)context; (void)archive; (void)bytes;
    assert(error == RICH4_MKF_OK);
    if (phase == 1 && page_audio != NULL) {
        assert(!rich4_audio_speech_playing(page_audio));
        page_preparation_reads++;
    }
    if (phase == 2) { reads++; last_read = index; }
}

static void test_clock(void)
{
    Rich4AnimationClock c = {0};
    assert(!rich4_animation_clock_step(&c, 1000, 71));
    assert(rich4_animation_clock_step(&c, 6000, 71));
    assert(!rich4_animation_clock_step(&c, 6001, 71));
    assert(!rich4_animation_clock_step(&c, 6070, 71));
    assert(rich4_animation_clock_step(&c, 6071, 71));
    rich4_animation_clock_reset(&c);
    assert(!rich4_animation_clock_step(&c, 0xfffffff0u, 40));
    assert(!rich4_animation_clock_step(&c, 0x10u, 40));
    assert(rich4_animation_clock_step(&c, 0x18u, 40));
}

static void test_flows(GameSession *s)
{
    Rich4ToolAnimationCompletion completion;
    GameEvent event = rich4_game_event_tool_presentation_requested_from_fields(
        (Rich4GameEventFields){0, 0, 558, 96, 0,
            {GAME_DOMAIN_NONE, -1}, {GAME_DOMAIN_NONE, -1}});
    uint32_t now;
    int i;
    memset(s, 0, sizeof(*s));
    assert(rich4_tool_animation_enqueue(s, &event, 4, 30, 0, 0, 0, 1));
    now = SDL_GetTicks();
    assert(rich4_tool_animation_update(s, 4, 30, now, NULL, &completion));
    assert(s->presentation.tool_animation[0].frame == 0);
    SDL_Delay(180); /* Real loading/render overrun, six nominal intervals. */
    now = SDL_GetTicks();
    assert(rich4_tool_animation_update(s, 4, 30, now, NULL, &completion));
    assert(s->presentation.tool_animation[0].frame == 1);
    assert(rich4_tool_animation_update(s, 4, 30, now + 1, NULL, &completion));
    assert(s->presentation.tool_animation[0].frame == 1);
    for (i = 2; i < 4; ++i) {
        now += 30;
        assert(rich4_tool_animation_update(s, 4, 30, now, NULL, &completion));
        assert(s->presentation.tool_animation[0].frame == i);
    }
    assert(!rich4_tool_animation_update(s, 4, 30, now + 30, NULL, &completion));

    memset(s, 0, sizeof(*s));
    s->presentation.special_event.active = 1;
    assert(rich4_special_event_begin_timed_phase(s,
        RICH4_SPECIAL_PRESENT_FORTUNE_ANIMATION, 4, 71, 10, 2));
    rich4_special_event_update_timeline(s, 4, 71, 2000);
    assert(s->presentation.special_event.frame == 0);
    rich4_special_event_update_timeline(s, 4, 71, 5000);
    assert(s->presentation.special_event.frame == 1);
    assert((int32_t)(s->presentation.special_event.until - 5000) > 0);
    rich4_special_event_update_timeline(s, 4, 71, 5001);
    assert(s->presentation.special_event.frame == 1);
    for (i = 2; i <= 4; i++)
        rich4_special_event_update_timeline(s, 4, 71, 5000 + (i - 1) * 71);
    assert(s->presentation.special_event.frame == 4);

    {
        CardPanelState p;
        memset(&p, 0, sizeof(p));
        p.phase = CARD_PANEL_DEMOLISH_ANIMATION;
        p.effect_frame_count = 3;
        rich4_card_panel_update_animation(&p, 1000);
        rich4_card_panel_update_animation(&p, 9000);
        assert(p.effect_frame == 1 && !rich4_card_panel_effect_due(&p, 9000));
        rich4_card_panel_update_animation(&p, 9001);
        assert(p.effect_frame == 1);
        rich4_card_panel_update_animation(&p, 9080);
        assert(p.effect_frame == 2);
        rich4_card_panel_update_animation(&p, 9160);
        assert(p.effect_frame == 3 && !rich4_card_panel_effect_due(&p, 9659));
        assert(rich4_card_panel_effect_due(&p, 9660));
    }
    puts("QA_FLC_CLOCK_PASS real_stall_ms=180 no_skip=1 no_catchup=1 final_interval=1");
}

static void test_frame_hooks(GameSession *s)
{
    Rich4IntroTiming timing;
    memset(s,0,sizeof(*s)); memset(&timing,0,sizeof(timing));
    s->state.player_count = 1; s->state.players[0].character = 0;
    timing.window_frame_count=1; timing.window_delay_ms=10;
    timing.actor_frame_count[0][0]=25; timing.actor_delay_ms[0][0]=10;
    timing.actor_frame_count[1][0]=1; timing.actor_delay_ms[1][0]=10;
    rich4_intro_begin(s);
    for (int frame=0;frame<20;frame++) rich4_intro_update(s,&timing,NULL,frame*10);
    assert(s->presentation.intro.jump_sound_pending == 0);
    rich4_intro_update(s,&timing,NULL,200);
    assert(rich4_intro_stage_for_frame(&timing,s,s->presentation.intro.frame).graph_frame == 19);
    assert(s->presentation.intro.jump_sound_pending == 1);
    memset(s,0,sizeof(*s));
    s->state.active=1; s->state.player_count=1; s->state.players[0].play_state=1;
    s->state.players[0].ndices=1; s->flow.turn_state=GAME_TURN_STATE_PREROLL;
    rich4_dice_begin_roll(s,1000);
    for (int frame=0;frame<29;frame++)
        assert(rich4_dice_update(s,NULL,0,30,40,10,1000+frame*10));
    assert(s->presentation.dice.sound_pending == 0);
    assert(rich4_dice_update(s,NULL,0,30,40,10,1290));
    assert(s->presentation.dice.animation_frame == 29 && s->presentation.dice.sound_pending == 1);
    assert(rich4_dice_update(s,NULL,0,30,40,10,1300));
    assert(s->presentation.dice.sound_pending == 1);
    memset(s,0,sizeof(*s));
    s->presentation.god_acquisition.active=1;
    s->presentation.god_acquisition.frame_count=4;
    s->presentation.god_acquisition.frame_delay_ms=71;
    s->state.player_count=1; s->state.price_index=1; s->state.players[0].cash=50000;
    s->presentation.god_acquisition.god_type=1;
    s->presentation.god_acquisition.money_pending=1;
    s->presentation.god_acquisition.until=3684;
    assert(game_update_god_presentation(s,NULL,NULL,1000,1));
    assert(game_update_god_presentation(s,NULL,NULL,5000,1));
    assert(s->presentation.god_acquisition.frame == 1);
    assert(!s->presentation.god_acquisition.money.active);
    assert(s->presentation.god_acquisition.message_visible == RICH4_GOD_MESSAGE_HIDDEN);
    assert(game_update_god_presentation(s,NULL,NULL,5001,1));
    assert(s->presentation.god_acquisition.frame == 1);
    assert(!s->presentation.god_acquisition.money.active);
    puts("QA_FLC_HOOK_PASS intro_frame=20 dice_frame=30 one_based=1 god_no_skip=1");
}

static void test_audio(SDL_Surface *screen)
{
    static unsigned char samples[88200];
    Rich4Audio *audio = NULL;
    assert(rich4_audio_init(&audio));
    audio->tool_effect[8] = Mix_QuickLoad_RAW(samples, sizeof(samples));
    audio->tool_effect[4] = Mix_QuickLoad_RAW(samples, sizeof(samples));
    assert(audio->tool_effect[8] != NULL);
    rich4_audio_begin_visual_frame(audio);
    rich4_audio_play_tool_effect(audio, 96);
    SDL_Delay(100);
    assert(Mix_Playing(-1) == 0);
    SDL_FillRect(screen, NULL, 0x1234);
    assert(SDL_Flip(screen) == 0);
    rich4_audio_present_visual_frame(audio);
    assert(Mix_Playing(-1) == 1);
    assert(audio->visual_sound_count == 0);
    rich4_audio_begin_visual_frame(audio);
    rich4_audio_play_tool_effect(audio, 91);
    assert(Mix_Playing(-1) == 0); /* prepare replaces the previous FLC sound */
    assert(SDL_Flip(screen) == 0);
    rich4_audio_present_visual_frame(audio);
    assert(Mix_Playing(-1) == 1);
    assert(Mix_GetChunk(audio->flc_effect_channel) == audio->tool_effect[4]);
    rich4_audio_shutdown(audio);
    puts("QA_FLC_AUDIO_PASS effect_waits_for_first_present=1 replaces_previous_effect=1");
}

static void test_resources(GamePreviewAssets *a, SDL_Surface *screen)
{
    ResourceStore *store = rich4_resource_store_create();
    SdlOptions options;
    unsigned char *same;
    int index, frame, frames, archive_index;
    unsigned total_frames = 0, flc_resources = 0;
    memset(&options, 0, sizeof(options));
    options.data_dir = "assets/original";
    game_load_scoped_preview_assets(store, &options, a, 2, 0, 3u);
    rich4_resource_store_set_debug_observer(store, resource_read, NULL);
    {
        size_t resident = rich4_resource_store_cached_bytes(store), prefix_bytes;
        Uint32 before = SDL_GetTicks(), prefix_ms, target_ms;
        reads = 0;
        /* Test-only reference to the previous prefix-loading lookup. */
        for (index = 0; index < GAME_SPECIAL_EVENT_ASSET_COUNT; index++)
            if (game_asset_ready(a, &a->ui.has_special_event[index]) &&
                a->ui.special_event_resource[index] == 508) break;
        assert(reads == 68);
        prefix_ms = SDL_GetTicks() - before;
        prefix_bytes = rich4_resource_store_cached_bytes(store) - resident;
        game_lazy_assets_begin_frame(a); game_lazy_assets_begin_frame(a);
        assert(rich4_resource_store_cached_bytes(store) <= resident + GAME_LAZY_RECENT_BYTES);
        game_lazy_assets_clear_recent(a);
        assert(rich4_resource_store_cached_bytes(store) == resident);
        reads = 0; before = SDL_GetTicks();
        index = game_special_event_asset_index(a, 508);
        target_ms = SDL_GetTicks() - before;
        printf("QA_FLC_LOOKUP_COMPARE old_reads=68 new_reads=%u old_ms=%u new_ms=%u old_bytes=%lu new_bytes=%lu\n",
            reads,prefix_ms,target_ms,(unsigned long)prefix_bytes,
            (unsigned long)(rich4_resource_store_cached_bytes(store)-resident));
    }
    assert(index >= 0 && reads == 1 && last_read == 508);
    assert(a->ui.special_event_data[0] == NULL);
    assert(game_special_event_asset_index(a, 508) == index && reads == 1);
    frames = game_asset_graph_frames(a, &a->character.first_landing_data[0], 0);
    assert(frames > 0);
    same = a->character.first_landing_data[0];
    assert(same != NULL);
    reads = 0;
    assert(game_asset_ready(a, &a->character.has_first_landing_graph[0]));
    assert(a->character.first_landing_data[0] == same && reads == 0);
    puts("QA_FLC_LOADING_PASS event_reads=1 unrelated_reads=0 timing_draw_same_lease=1");

    /* Every FLC in the shipped Data/Jump/Panel archives is decoded forward,
     * retaining palette/index/delta state, including all monthly/intro films. */
    for (archive_index = 0; archive_index < 3; archive_index++) {
        Rich4Mkf *archive = archive_index == 0 ? a->data_archive :
            archive_index == 1 ? a->jump_archive : a->panel_archive;
        size_t resource;
        assert(archive != NULL);
        for (resource = 0; resource < rich4_mkf_count(archive); resource++) {
            unsigned char *data = NULL;
            size_t size = 0, count;
            Rich4MkfResourceInfo info;
            Rich4FlcPlayer player;
            Rich4FlcHeader h;
            uint16_t *pixels;
            unsigned char *indices, *dirty;
            assert(rich4_mkf_get_info(archive, resource, &info) == RICH4_MKF_OK);
            if (info.uncompressed_size < RICH4_FLC_HEADER_SIZE) continue;
            assert(rich4_mkf_read_resource(archive,resource,&data,&size,NULL) == RICH4_MKF_OK);
            if (rich4_flc_parse_header(data,size,&h) != RICH4_FLC_OK) {
                free(data); continue;
            }
            frames = h.frame_count; count = (size_t)h.width*h.height;
            pixels = calloc(count,sizeof(*pixels)); indices = calloc(count,1); dirty = calloc(count,1);
            assert(pixels && indices && dirty);
            assert(rich4_flc_player_init(&player,data,size,pixels,indices,dirty,count,0) == RICH4_FLC_OK);
            for (frame = 0; frame < frames; frame++) {
                int decoded = 0;
                assert(rich4_flc_player_decode_to(&player,frame,&decoded) == RICH4_FLC_OK);
                assert(decoded == 1 && player.current_frame == frame);
                total_frames++;
            }
            flc_resources++;
            free(pixels); free(indices); free(dirty); free(data);
        }
    }
    assert(flc_resources == 105 && total_frames == 3099);
    {
        static const char *const names[] = {"Speaking.mkf","SPEAKING.MKF","speaking.mkf"};
        char path[512];
        Rich4Mkf *speaking = game_open_named_mkf_from_data_dir(store,
            options.data_dir,names,3,path,sizeof(path));
        Rich4Audio *audio = NULL;
        GameSession *session = calloc(1,sizeof(*session));
        GameEvent event = rich4_game_event_special_event_presentation(0,441,66,GAME_SPECIAL_EVENT_NEWS);
        assert(speaking && session && rich4_audio_init(&audio));
        session->state.active = 1; session->state.player_count = 1;
        session->state.special_event_family = GAME_SPECIAL_EVENT_NEWS;
        session->state.special_event_actor = 0;
        session->state.special_event_logical_id = session->state.special_event_resolved_id = 0;
        rich4_audio_set_speaking_archive(audio,speaking);
        page_audio = audio; page_preparation_reads = 0;
        game_begin_special_event_presentation(session,a,&event,audio,SDL_GetTicks());
        page_audio = NULL;
        assert(page_preparation_reads >= 1);
        assert(a->ui.has_special_event[0] && a->ui.has_panel[66]);
        assert(rich4_audio_speech_playing(audio));
        rich4_audio_stop_speech(audio);
        session->state.players[0].cash = 5000;
        session->flow.route_events.state.execution_actor = 0;
        event = rich4_game_event_lottery_visit_request(0,9,12,13,14u);
        page_audio = audio; page_preparation_reads = 0;
        game_begin_lottery_visit(session,a,&event,audio,SDL_GetTicks(),1);
        page_audio = NULL;
        assert(page_preparation_reads >= 1);
        assert(a->ui.has_panel[13] && a->ui.has_panel_graph[14]);
        assert(session->presentation.lottery.animation_frame == 0);
        assert(game_update_lottery_visit(session,a,audio,SDL_GetTicks()));
        assert(session->presentation.lottery.animation_frame == 0);
        assert(rich4_audio_speech_playing(audio));
        rich4_audio_shutdown(audio); free(session);
        puts("QA_EVENT_VOICE_PASS selected_image_and_panel_ready_before_voice=1 lottery_frame_zero=1");
    }
    (void)screen;
    game_free_preview_assets(a);
    assert(rich4_resource_store_cached_bytes(store) == 0);
    rich4_resource_store_destroy(store);
    printf("QA_FLC_DECODE_PASS resources=%u sequential_frames=%u final_cache_bytes=0\n",
        flc_resources, total_frames);
}

int main(void)
{
    GamePreviewAssets *a = calloc(1,sizeof(*a));
    GameSession *s = calloc(1,sizeof(*s));
    SDL_Surface *screen;
    assert(a && s);
    setvbuf(stdout, NULL, _IOLBF, 0);
    SDL_putenv("SDL_VIDEODRIVER=dummy"); SDL_putenv("SDL_AUDIODRIVER=dummy");
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0);
    screen = SDL_SetVideoMode(640,480,16,SDL_SWSURFACE);
    assert(screen != NULL);
    test_clock(); test_flows(s); test_frame_hooks(s); test_audio(screen); test_resources(a,screen);
    free(a); free(s); SDL_Quit();
    puts("QA_FLC_PLAYBACK_PASS");
    return 0;
}
