#ifndef R4DIFF_LOCKSTEP_WIRE_H
#define R4DIFF_LOCKSTEP_WIRE_H
/* Local little-endian protocol v1. Only uint32 words cross x86/x64. */
#include <stdint.h>
#define LS_MAGIC 0x32443452u
#define LS_VERSION 1u
#define LS_TRACE_LIMIT 4096u
#define LS_PIXEL_BYTES (640u*480u*2u)
#define LS_TRACE_OFFSET 256u
#define LS_PIXEL_OFFSET (LS_TRACE_OFFSET+LS_TRACE_LIMIT*32u)
#define LS_FRAME_LIMIT 32u
#define LS_FRAME_OFFSET (LS_PIXEL_OFFSET+LS_PIXEL_BYTES)
#define LS_FRAME_STRIDE (32u+LS_PIXEL_BYTES)
#define LS_BYTES (LS_FRAME_OFFSET+LS_FRAME_LIMIT*LS_FRAME_STRIDE)
enum { C_MAGIC=0,C_VERSION,C_COMMAND,C_EPOCH,C_MS,C_MS_HIGH,C_ORIGIN,
 C_EVENT,C_KIND,C_X,C_Y,C_KEY,C_YEAR,C_MONTH,C_DAY,
 R_EPOCH=32,R_MS,R_SCENE,R_ERROR,R_PIXELS,R_PRESENTS,R_ACK,R_TRACES,
 R_NEXT,R_SITE,R_RMASK,R_GMASK,R_BMASK,R_ROLE,R_SUBSTEPS };
enum { LS_STEP=1,LS_STOP=2 };
enum { LS_MOVE=1,LS_DOWN,LS_UP,LS_KEYDOWN,LS_KEYUP };
enum { LS_SEED=1,LS_RNG,LS_INPUT,LS_TIMER,LS_WAIT,LS_WARP };
extern uint32_t *ls_words;
int ls_open(unsigned role);
int ls_active(void);
uint32_t ls_ticks(void);
uint32_t ls_ms(void);
void ls_trace(unsigned type,unsigned site,unsigned a,unsigned b,unsigned c,unsigned thread);
void ls_pixels(const void *data,unsigned pitch,unsigned w,unsigned h,unsigned bpp,
               unsigned r,unsigned g,unsigned b);
void ls_commit(unsigned source);
int ls_exchange(unsigned scene,unsigned next,unsigned site);
void ls_fail(unsigned code,unsigned site);
#endif
