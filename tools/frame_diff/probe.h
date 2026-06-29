#ifndef R4DIFF_PROBE_H
#define R4DIFF_PROBE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <ddraw.h>
enum { EV_BOOT=1, EV_CLOCK, EV_TIMER_SET, EV_TIMER_KILL, EV_TIMER,
 EV_DDRAW, EV_SURFACE, EV_BLT, EV_BLTFAST, EV_FLIP, EV_LOCK, EV_UNLOCK,
 EV_GETDC, EV_RELEASEDC, EV_VBLANK, EV_RNG, EV_SEED, EV_WAIT,
 EV_FRAME, EV_ERROR, EV_END, EV_PEEK, EV_INPUT, EV_HOOK };
typedef struct { uint64_t us, duration; uint32_t type, thread, caller, object;
 int32_t result; uint32_t a,b,c,d; } Event;
uint64_t probe_now(void);
void probe_event(unsigned type, uint64_t started, uintptr_t caller,
 uintptr_t object, int32_t result, uint32_t a,uint32_t b,uint32_t c,uint32_t d);
void probe_error(unsigned code, uintptr_t object);
int probe_graphics_install(void **slot);
int probe_capture_enabled(void);
void probe_frame(void *object, const DDSURFACEDESC *desc, const RECT *rect,
 unsigned reason, HRESULT result);
void probe_graphics_audit(void);
#define CALLER ((uintptr_t)__builtin_return_address(0))
#endif
