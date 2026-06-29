/* Test-only LD_PRELOAD shim. Never linked into or packaged with the game.
 * Inject real Mini SDL keyboard events, preserve held-key state, capture the
 * actual SDL presentation surface, and report Linux's process RSS high water. */
#include <SDL/SDL.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

typedef struct QaKey { int frame; int key; int down; } QaKey;
static QaKey schedule[256];
static int schedule_count, next_key, frames, initialized;
static Uint8 held[SDLK_LAST], overridden[SDLK_LAST];
static int captures[64], capture_count;
typedef struct QaPointer { int frame, x, y; } QaPointer;
static QaPointer pointers[64];
static int pointer_count, next_pointer;
static int realtime_frames, quit_sent;

static void qa_init(void)
{
    const char *env;
    char *copy, *part;
    if (initialized) return;
    initialized = 1;
    env = getenv("RICH4_QA_REAL_FRAMES");
    realtime_frames = env != NULL ? atoi(env) : 0;
    env = getenv("RICH4_QA_KEYS");
    if (env && (copy = strdup(env)) != NULL) {
        for (part = strtok(copy, ","); part && schedule_count < 256;
             part = strtok(NULL, ",")) {
            QaKey *key = &schedule[schedule_count];
            if (sscanf(part, "%d:%d:%d", &key->frame, &key->key, &key->down) == 3 &&
                key->key >= 0 && key->key < SDLK_LAST) schedule_count++;
        }
        free(copy);
    }
    env = getenv("RICH4_QA_CAPTURE");
    if (env && (copy = strdup(env)) != NULL) {
        for (part = strtok(copy, ","); part && capture_count < 64;
             part = strtok(NULL, ",")) captures[capture_count++] = atoi(part);
        free(copy);
    }
    env = getenv("RICH4_QA_POINTER");
    if (env && (copy = strdup(env)) != NULL) {
        for (part = strtok(copy, ","); part && pointer_count < 64;
             part = strtok(NULL, ",")) {
            QaPointer *p = &pointers[pointer_count];
            if (sscanf(part, "%d:%d:%d", &p->frame, &p->x, &p->y) == 3)
                pointer_count++;
        }
        free(copy);
    }
    printf("QA_BEGIN scheduled_keys=%d capture_count=%d\n", schedule_count, capture_count);
}

int SDL_PollEvent(SDL_Event *event)
{
    static int (*real_poll)(SDL_Event *);
    qa_init();
    if (!real_poll) real_poll = dlsym(RTLD_NEXT, "SDL_PollEvent");
    if (realtime_frames > 0 && frames >= realtime_frames && !quit_sent) {
        memset(event, 0, sizeof(*event));
        event->type = SDL_QUIT;
        quit_sent = 1;
        return 1;
    }
    if (next_pointer < pointer_count && pointers[next_pointer].frame <= frames) {
        QaPointer p = pointers[next_pointer++];
        memset(event, 0, sizeof(*event));
        event->type = SDL_MOUSEMOTION;
        event->motion.x = p.x;
        event->motion.y = p.y;
        printf("QA_POINTER frame=%d x=%d y=%d\n", frames, p.x, p.y);
        return 1;
    }
    if (next_key < schedule_count && schedule[next_key].frame <= frames) {
        QaKey key = schedule[next_key++];
        held[key.key] = key.down != 0;
        overridden[key.key] = 1;
        memset(event, 0, sizeof(*event));
        event->type = key.down ? SDL_KEYDOWN : SDL_KEYUP;
        event->key.state = key.down ? SDL_PRESSED : SDL_RELEASED;
        event->key.keysym.sym = (SDLKey)key.key;
        printf("QA_KEY frame=%d native=%d down=%d\n", frames, key.key, key.down);
        return 1;
    }
    return real_poll(event);
}

Uint8 *SDL_GetKeyState(int *count)
{
    static Uint8 *(*real_keys)(int *);
    static Uint8 keys[SDLK_LAST];
    Uint8 *actual;
    int n = 0, i;
    if (!real_keys) real_keys = dlsym(RTLD_NEXT, "SDL_GetKeyState");
    actual = real_keys(&n);
    for (i = 0; i < SDLK_LAST; i++)
        keys[i] = overridden[i] ? held[i] : (actual && i < n ? actual[i] : 0);
    if (count) *count = SDLK_LAST;
    return keys;
}

int SDL_Flip(SDL_Surface *screen)
{
    static int (*real_flip)(SDL_Surface *);
    const char *directory = getenv("RICH4_QA_OUTPUT");
    int i;
    qa_init();
    if (!real_flip) real_flip = dlsym(RTLD_NEXT, "SDL_Flip");
    ++frames;
    for (i = 0; directory && i < capture_count; i++) {
        if (frames == captures[i]) {
            char path[512];
            snprintf(path, sizeof(path), "%s/frame_%04d.bmp", directory, frames);
            printf("QA_CAPTURE frame=%d result=%d path=%s\n", frames,
                   SDL_SaveBMP(screen, path), path);
        }
    }
    return real_flip(screen);
}

__attribute__((destructor)) static void qa_finish(void)
{
    struct rusage usage;
    if (!initialized) return;
    getrusage(RUSAGE_SELF, &usage);
    printf("QA_END frames=%d injected=%d/%d peak_rss_kb=%ld\n",
           frames, next_key, schedule_count, usage.ru_maxrss);
}
