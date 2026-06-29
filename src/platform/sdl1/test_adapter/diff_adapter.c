#ifdef R4DIFF_TEST
#include "platform/sdl1/test_adapter/diff_adapter.h"
#include "core/rng.h"
#include "lockstep_wire.h"
#include <string.h>
static unsigned consumed;
static uint32_t next_deadline=20;
static unsigned char keys[512];
int r4diff_key_pressed(int key){return key>=0&&key<512&&keys[key];}
static SDLKey input_key(unsigned key){
 if(key>=65&&key<=90)return (SDLKey)(key+32);
 switch(key){case 37:return SDLK_LEFT;case 38:return SDLK_UP;case 39:return SDLK_RIGHT;case 40:return SDLK_DOWN;}
 if(key==13||key==27||key==32||key==8||key==9||(key>=48&&key<=57))return (SDLKey)key;
 ls_fail(13,key);return SDLK_UNKNOWN;
}
static void observe_rng(unsigned kind,uint32_t before,uint32_t after,unsigned result){
 ls_trace(kind?LS_SEED:LS_RNG,0,before,after,result,0);
}
int r4diff_init(void){int r=ls_open(2);if(r>0)rich4_rng_set_observer(observe_rng);return r>=0;}
int r4diff_active(void){return ls_active();}
uint32_t r4diff_ticks(void){return ls_ticks();}
struct tm *r4diff_localtime(const time_t*value){
 static struct tm today;
 if(!ls_active())return localtime(value);
 memset(&today,0,sizeof(today));today.tm_year=ls_words[C_YEAR]-1900;
 today.tm_mon=ls_words[C_MONTH]-1;today.tm_mday=ls_words[C_DAY];today.tm_isdst=-1;
 today.tm_sec=(ls_ms()/1000)%60;
 return &today;
}
int r4diff_poll(SDL_Event*e){
 SDL_Event host;unsigned kind;
 if(!ls_active())return SDL_PollEvent(e);
 while(SDL_PollEvent(&host)){if(host.type==SDL_QUIT)ls_fail(10,0);}
 if(!ls_words[C_EVENT] || consumed==ls_words[C_EVENT])return 0;
 consumed=ls_words[C_EVENT];kind=ls_words[C_KIND];memset(e,0,sizeof(*e));
 switch(kind){
 case LS_MOVE:e->type=SDL_MOUSEMOTION;e->motion.x=ls_words[C_X];e->motion.y=ls_words[C_Y];break;
 case LS_DOWN:case LS_UP:e->type=kind==LS_DOWN?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;
  e->button.button=SDL_BUTTON_LEFT;e->button.state=kind==LS_DOWN?SDL_PRESSED:SDL_RELEASED;
  e->button.x=ls_words[C_X];e->button.y=ls_words[C_Y];break;
 case LS_KEYDOWN:case LS_KEYUP:e->type=kind==LS_KEYDOWN?SDL_KEYDOWN:SDL_KEYUP;
  e->key.keysym.sym=input_key(ls_words[C_KEY]);e->key.state=kind==LS_KEYDOWN?SDL_PRESSED:SDL_RELEASED;
  keys[e->key.keysym.sym]=kind==LS_KEYDOWN;break;
 default:ls_fail(11,kind);
 }
 ls_words[R_ACK]=consumed;ls_trace(LS_INPUT,0,consumed,kind,0,0);return 1;
}
void r4diff_capture(SDL_Surface*s){
 if(!ls_active())return;
 if(!s || !s->format)ls_fail(2,0);
 if(SDL_MUSTLOCK(s) && SDL_LockSurface(s)!=0)ls_fail(2,1);
 ls_pixels(s->pixels,s->pitch,s->w,s->h,s->format->BitsPerPixel,s->format->Rmask,s->format->Gmask,s->format->Bmask);
 if(SDL_MUSTLOCK(s))SDL_UnlockSurface(s);
 ls_commit(1);
}
int r4diff_barrier(SDL_Surface*s,int scene){
 if(!ls_active())return 1;
 if(!ls_words[R_PIXELS])r4diff_capture(s);
 return ls_exchange((unsigned)scene,next_deadline,0);
}
void r4diff_begin(void){next_deadline=ls_ms()+20;}
void r4diff_wake_at(uint32_t tick){
 int32_t remaining=(int32_t)(tick-ls_ticks());
 if(remaining>0 && ls_ms()+(uint32_t)remaining<next_deadline)next_deadline=ls_ms()+(uint32_t)remaining;
}
void r4diff_delay(uint32_t ms){
 /* Nested synchronous SDL delays need a verified continuation. */
 if(ms)ls_fail(12,ms);
}
#endif
