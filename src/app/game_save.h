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
int rich4_game_save_write(const char *path, int slot,
                          const GameSession *session);
int rich4_game_save_read(const char *path, int slot,
                         GameSession *session);
/* Accept a decoded save into the running session. 0x402F99/0x402FA1
 * reseeds rand from the live clock, retaining the saved event decks/cursors.
 * The decoder itself preserves serialized RNG for replay and inspection. */
int rich4_game_save_resume(GameSession *session, const GameSession *loaded,
                           uint32_t now);
/* A decoded save can only be attached to a preloaded map whose typed domain
 * counts match the state that was serialized with it. */
int rich4_game_save_session_matches_map(const GameSession *session,
                                        const MapData *map);

#endif
