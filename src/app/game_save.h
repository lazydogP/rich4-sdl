#ifndef RICH4_APP_GAME_SAVE_H
#define RICH4_APP_GAME_SAVE_H

#include <stddef.h>

#include "app/game_session.h"
#include "data/map_data.h"

enum { RICH4_GAME_SAVE_PATH_CAPACITY = 1024 };

typedef void (*Rich4SaveDebugObserver)(const char *phase, int begin, size_t bytes);
/* Optional temporary profiling observer; never changes save completion. */
void rich4_game_save_set_debug_observer(Rich4SaveDebugObserver observer);

/* Own-format session files use fixed-width little-endian fields.  Slot 0 is
 * the automatic save; slots 1..5 are the original user-visible slots. */
int rich4_game_save_slot_path(char *out, size_t capacity,
                              const char *directory, int slot);
int rich4_game_save_ensure_directory(const char *directory);
/* 0x418EBD reaches 0x402FD1(0) only after 0x41CF67's blocking calendar
 * presentations and 0x41C84F's selected-player status work have returned.
 * Keep the SDL autosave request queued until the equivalent session state is
 * writable. */
int rich4_game_save_auto_ready(const GameSession *session);
int rich4_game_save_write(const char *path, int slot,
                          const GameSession *session);
int rich4_game_save_read(const char *path, int slot,
                         GameSession *session);
/* Accept a decoded save into the running session. 0x402F99/0x402FA1
 * reseeds rand from the live clock, retaining the saved event decks/cursors.
 * The decoder itself preserves serialized RNG for replay and inspection. */
int rich4_game_save_resume(GameSession *session, const GameSession *loaded,
                           uint32_t now);
/* Consume the one-shot platform audio resync request after a resume.  Music
 * markers live in the serialized presentation but describe a process-local
 * backend, so they must be cleared at the app boundary before SDL restarts
 * the current owner. */
int rich4_game_save_take_music_restart(GameSession *session);
/* A decoded save can only be attached to a preloaded map whose typed domain
 * counts match the state that was serialized with it. */
int rich4_game_save_session_matches_map(const GameSession *session,
                                        const MapData *map);

#endif
