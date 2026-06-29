#ifndef RICH4_DIFF_ADAPTER_H
#define RICH4_DIFF_ADAPTER_H
#include <stdint.h>
#include <time.h>
#include <SDL/SDL.h>
#ifdef R4DIFF_TEST
int r4diff_init(void);
int r4diff_active(void);
uint32_t r4diff_ticks(void);
int r4diff_poll(SDL_Event *event);
int r4diff_key_pressed(int key);
int r4diff_barrier(SDL_Surface *screen,int scene);
void r4diff_capture(SDL_Surface *screen);
void r4diff_delay(uint32_t ms);
void r4diff_begin(void);
void r4diff_wake_at(uint32_t tick);
struct tm *r4diff_localtime(const time_t *value);
#else
static inline int r4diff_init(void){return 1;}
static inline int r4diff_active(void){return 0;}
static inline uint32_t r4diff_ticks(void){return 0;}
static inline int r4diff_poll(SDL_Event *e){return SDL_PollEvent(e);}
static inline int r4diff_key_pressed(int key){(void)key;return 0;}
static inline int r4diff_barrier(SDL_Surface*s,int scene){(void)s;(void)scene;return 1;}
static inline void r4diff_capture(SDL_Surface*s){(void)s;}
static inline void r4diff_delay(uint32_t ms){(void)ms;}
static inline void r4diff_begin(void){}
static inline void r4diff_wake_at(uint32_t tick){(void)tick;}
static inline struct tm *r4diff_localtime(const time_t*t){return localtime(t);}
#endif
#endif
