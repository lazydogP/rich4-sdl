#ifndef RICH4_PLATFORM_SDL1_RUNTIME_TRACE_H
#define RICH4_PLATFORM_SDL1_RUNTIME_TRACE_H

#include "resource/resource_store.h"

/* Temporary performance diagnostics; real wall time, independent of the
 * deterministic game clock. RICH4_PROFILE=1 aggregates, =2 lists reads. */
unsigned int runtime_profile_now(void);
void runtime_profile_mark(const char *name);
void runtime_profile_stage(const char *name, unsigned int started);
/* Temporary diagnostic spans; resource=-1 when no archive entry applies. */
void runtime_profile_span(const char *group, const char *name,
    unsigned int started, int resource, size_t bytes);
void runtime_profile_attach_resources(ResourceStore *store);
void runtime_profile_frame_begin(int scene);
void runtime_profile_frame_phase(int phase);
void runtime_profile_frame_end(void);
void runtime_profile_close(void);
void runtime_profile_render_part(int part, unsigned int started);
/* Temporary diagnostics, aggregated every five seconds and at scene changes.
 * UI details nest inside render; present details nest inside present_ms. */
typedef enum RuntimeProfileDetail {
    PROFILE_PRESENT_LOCK, PROFILE_PRESENT_CONVERT, PROFILE_PRESENT_UNLOCK,
    PROFILE_PRESENT_CAPTURE, PROFILE_PRESENT_FLIP, PROFILE_PRESENT_CURSOR,
    PROFILE_PRESENT_AUDIO, PROFILE_UI_TOOLBAR, PROFILE_UI_HUD,
    PROFILE_UI_CALENDAR, PROFILE_UI_STATUS, PROFILE_UI_EVENTS,
    PROFILE_UI_SAVE_PANEL, PROFILE_TEXT_RENDER, PROFILE_DETAIL_COUNT
} RuntimeProfileDetail;
void runtime_profile_detail(RuntimeProfileDetail part, unsigned int started);
void runtime_profile_flc_frame(size_t archive_id, size_t index, int frame, int decoded);

void trace_game_start(const char *message);
void reset_game_start_trace(void);

#endif
