/* R10.1d: startup/session diagnostic append-log helpers moved out of
 * runtime_main.c. The trace budget remains process-local; callers retain the
 * same exported trace_game_start/reset_game_start_trace seam. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "platform/sdl1/runtime_trace.h"

static int game_start_trace_budget = 256;
static FILE *game_start_trace_file;
static FILE *profile_file;
static int profile_level = -1;
static unsigned int profile_lines, profile_origin, read_started;
static unsigned int reads, hits, read_ms;
static unsigned long read_bytes;
static unsigned int frame_start, phase_start, frame_times[5];
static unsigned int report_start, frame_count, frame_max, slow_count;
static unsigned long frame_totals[5];
static int frame_scene = -1, report_scene = -1;
static unsigned int render_count[5], render_max[5];
static unsigned long render_total[5];
static unsigned int detail_count[PROFILE_DETAIL_COUNT], detail_max[PROFILE_DETAIL_COUNT];
static unsigned long detail_total[PROFILE_DETAIL_COUNT];
static unsigned int detail_report_start;

/* SDL_Init resets SDL's timer origin. Resources are loaded before it, so
 * use a process-independent monotonic clock for startup and frame timing. */
static unsigned int wall_ms(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (unsigned int)((counter.QuadPart / frequency.QuadPart) * 1000 +
        (counter.QuadPart % frequency.QuadPart) * 1000 / frequency.QuadPart);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (unsigned int)(now.tv_sec * 1000u + now.tv_nsec / 1000000u);
#endif
}

static void profile_write(const char *format, ...)
{
    va_list args;
    if (profile_file == NULL || profile_lines >= 20000) return;
    va_start(args, format);
    vfprintf(profile_file, format, args);
    va_end(args);
    profile_lines++;
}

unsigned int runtime_profile_now(void)
{
    if (profile_level < 0) {
        const char *level = getenv("RICH4_PROFILE");
        const char *path = getenv("RICH4_PROFILE_LOG");
        profile_level = level != NULL ? atoi(level) : 0;
        if (profile_level > 0) {
            profile_file = fopen(path != NULL ? path : "rich4_perf.log", "wb");
            if (profile_file != NULL) setvbuf(profile_file, NULL, _IOFBF, 16384);
            profile_origin = wall_ms();
            profile_write("PROFILE level=%d clock=monotonic_wall_ms max_lines=20000\n", profile_level);
        }
    }
    return profile_level > 0 ? wall_ms() : 0;
}

void runtime_profile_mark(const char *name)
{
    unsigned int now = runtime_profile_now();
    if (profile_level <= 0) return;
    profile_write("MARK t_ms=%u name=%s reads=%u hits=%u read_decode_ms=%u bytes=%lu\n",
        now - profile_origin, name, reads, hits, read_ms, read_bytes);
    if (profile_file != NULL) fflush(profile_file);
}

void runtime_profile_stage(const char *name, unsigned int started)
{
    unsigned int now = runtime_profile_now();
    if (profile_level > 0)
        profile_write("STAGE t_ms=%u name=%s elapsed_ms=%u\n",
            now - profile_origin, name, now - started);
}

void runtime_profile_span(const char *group, const char *name,
    unsigned int started, int resource, size_t bytes)
{
    unsigned int now = runtime_profile_now();
    unsigned int elapsed = now - started;
    if (profile_level <= 0) return;
    if (elapsed >= 50 || (profile_level >= 2 &&
            (elapsed >= 2 || strcmp(group, "audio") == 0 || strcmp(group, "save") == 0)))
        profile_write("SPAN t_ms=%u group=%s name=%s elapsed_ms=%u resource=%d bytes=%lu\n",
            now - profile_origin, group, name, elapsed, resource, (unsigned long)bytes);
}

static void profile_resource(void *context, int phase, const char *archive,
    size_t index, size_t bytes, Rich4MkfError error)
{
    unsigned int now = runtime_profile_now();
    (void)context;
    if (phase == 0) { hits++; return; }
    if (phase == 1) { read_started = now; return; }
    reads++;
    read_bytes += (unsigned long)bytes;
    read_ms += now - read_started;
    if (profile_level >= 2 || now - read_started >= 50 || error != RICH4_MKF_OK)
        profile_write("RESOURCE t_ms=%u archive=%s index=%lu bytes=%lu read_decode_ms=%u error=%d\n",
            now - profile_origin, archive, (unsigned long)index,
            (unsigned long)bytes, now - read_started, (int)error);
}

void runtime_profile_flc_frame(size_t archive_id, size_t index, int frame, int decoded)
{
    unsigned int now = runtime_profile_now();
    if (profile_level >= 2)
        profile_write("FLC t_ms=%u archive_id=%lu index=%lu frame=%d decoded=%d\n",
            now-profile_origin,(unsigned long)archive_id,(unsigned long)index,frame,decoded);
}

void runtime_profile_attach_resources(ResourceStore *store)
{
    (void)runtime_profile_now();
    if (profile_level > 0)
        rich4_resource_store_set_debug_observer(store, profile_resource, NULL);
}

static void profile_detail_report(void)
{
    static const char *names[PROFILE_DETAIL_COUNT] = {
        "lock", "convert_rotate", "unlock", "debug_capture", "flip",
        "cursor", "audio_after_flip", "toolbar", "hud", "calendar",
        "status", "events", "save_panel", "render"
    };
    int part;
    for (part = 0; part < PROFILE_DETAIL_COUNT; part++) {
        if (detail_count[part] != 0)
            profile_write("DETAIL scene=%d group=%s name=%s count=%u total_ms=%lu avg_ms=%.3f max_ms=%u\n",
                report_scene, part < PROFILE_UI_TOOLBAR ? "present" : part < PROFILE_TEXT_RENDER ? "ui" : "text",
                names[part], detail_count[part], detail_total[part],
                (double)detail_total[part] / detail_count[part], detail_max[part]);
        detail_count[part] = detail_max[part] = 0;
        detail_total[part] = 0;
    }
    detail_report_start = wall_ms();
}

static void profile_report(void)
{
    static const char *names[5] = { "view_model", "intro", "ground", "actors_objects", "ui_overlays" };
    int part;
    if (frame_count == 0) return;
    profile_write("FRAMES scene=%d count=%u max_ms=%u avg_input_ms=%.2f avg_update_ms=%.2f avg_render_ms=%.2f avg_present_ms=%.2f avg_wait_ms=%.2f\n",
        report_scene, frame_count, frame_max,
        (double)frame_totals[0] / frame_count,
        (double)frame_totals[1] / frame_count,
        (double)frame_totals[2] / frame_count,
        (double)frame_totals[3] / frame_count,
        (double)frame_totals[4] / frame_count);
    for (part = 0; part < 5; part++) {
        if (render_count[part] != 0)
            profile_write("RENDER_PART scene=%d name=%s count=%u avg_ms=%.2f max_ms=%u\n",
                report_scene, names[part], render_count[part],
                (double)render_total[part] / render_count[part], render_max[part]);
        render_count[part] = render_max[part] = 0;
        render_total[part] = 0;
    }
    frame_count = frame_max = slow_count = 0;
    memset(frame_totals, 0, sizeof(frame_totals));
    if (wall_ms() - detail_report_start >= 5000) profile_detail_report();
    if (profile_file != NULL) fflush(profile_file);
}

void runtime_profile_frame_begin(int scene)
{
    unsigned int now = runtime_profile_now();
    if (profile_level <= 0) return;
    if (report_scene != scene) {
        profile_report();
        profile_detail_report();
        report_scene = scene;
        report_start = now;
        runtime_profile_mark("scene.begin");
        profile_write("SCENE t_ms=%u scene=%d\n", now - profile_origin, scene);
    }
    frame_scene = scene;
    frame_start = phase_start = now;
    memset(frame_times, 0, sizeof(frame_times));
}

void runtime_profile_frame_phase(int phase)
{
    unsigned int now;
    if (profile_level <= 0 || frame_scene < 0 || phase < 0 || phase >= 5) return;
    now = wall_ms();
    frame_times[phase] += now - phase_start;
    phase_start = now;
}

void runtime_profile_frame_end(void)
{
    unsigned int now, elapsed, threshold = 50;
    const char *env;
    int phase;
    if (profile_level <= 0 || frame_scene < 0) return;
    now = wall_ms();
    elapsed = now - frame_start;
    frame_count++;
    if (elapsed > frame_max) frame_max = elapsed;
    for (phase = 0; phase < 5; phase++) frame_totals[phase] += frame_times[phase];
    env = getenv("RICH4_PROFILE_SLOW_MS");
    if (env != NULL && atoi(env) > 0) threshold = (unsigned int)atoi(env);
    if (elapsed >= threshold && slow_count++ < 8)
        profile_write("SLOW_FRAME t_ms=%u scene=%d total_ms=%u input_ms=%u update_ms=%u render_ms=%u present_ms=%u wait_ms=%u\n",
            now - profile_origin, frame_scene, elapsed, frame_times[0], frame_times[1],
            frame_times[2], frame_times[3], frame_times[4]);
    if (now - report_start >= 1000) { profile_report(); report_start = now; }
    frame_scene = -1;
}

void runtime_profile_close(void)
{
    profile_report();
    profile_detail_report();
    runtime_profile_mark("runtime.shutdown");
    if (profile_file != NULL) { fclose(profile_file); profile_file = NULL; }
    if (game_start_trace_file != NULL) {
        fclose(game_start_trace_file);
        game_start_trace_file = NULL;
    }
}

void runtime_profile_render_part(int part, unsigned int started)
{
    unsigned int elapsed;
    if (profile_level <= 0 || part < 0 || part >= 5) return;
    elapsed = wall_ms() - started;
    render_count[part]++;
    render_total[part] += elapsed;
    if (elapsed > render_max[part]) render_max[part] = elapsed;
}

void runtime_profile_detail(RuntimeProfileDetail part, unsigned int started)
{
    unsigned int elapsed;
    if (profile_level <= 0 || part < 0 || part >= PROFILE_DETAIL_COUNT) return;
    elapsed = wall_ms() - started;
    detail_count[part]++;
    detail_total[part] += elapsed;
    if (elapsed > detail_max[part]) detail_max[part] = elapsed;
}

void trace_game_start(const char *message)
{
    FILE *fp;
    if (game_start_trace_budget <= 0) {
        return;
    }
    if (game_start_trace_file == NULL)
        game_start_trace_file = fopen("rich4_sdl_trace.log", "ab");
    fp = game_start_trace_file;
    if (fp == NULL) {
        return;
    }
    fprintf(fp, "wall_ms=%u %s\n", wall_ms(), message != NULL ? message : "");
    game_start_trace_budget--;
}

void reset_game_start_trace(void)
{
    if (game_start_trace_file != NULL) fclose(game_start_trace_file);
    game_start_trace_file = fopen("rich4_sdl_trace.log", "wb");
    game_start_trace_budget = 256;
}
