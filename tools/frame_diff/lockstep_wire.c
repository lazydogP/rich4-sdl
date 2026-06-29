#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "lockstep_wire.h"

uint32_t *ls_words;
static HANDLE mapping,ready,resume;
static uint32_t now,origin,epoch;
static LONG trace_count;
int ls_active(void){return ls_words!=NULL;}
uint32_t ls_ms(void){return now;}
uint32_t ls_ticks(void){return origin+now;}

int ls_open(unsigned role){
 char name[192],suffix[208];
 if(!GetEnvironmentVariableA("R4DIFF_LINK",name,sizeof(name)))return 0;
 mapping=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,name);
 if(!mapping)return -1;
 ls_words=MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,LS_BYTES);
 if(!ls_words || ls_words[C_MAGIC]!=LS_MAGIC || ls_words[C_VERSION]!=LS_VERSION)return -1;
 snprintf(suffix,sizeof(suffix),"%s.ready",name);ready=OpenEventA(EVENT_MODIFY_STATE,FALSE,suffix);
 snprintf(suffix,sizeof(suffix),"%s.resume",name);resume=OpenEventA(SYNCHRONIZE,FALSE,suffix);
 if(!ready || !resume)return -1;
 origin=ls_words[C_ORIGIN];now=0;epoch=0;ls_words[R_ROLE]=role;
 return 1;
}

void ls_fail(unsigned code,unsigned site){
 if(ls_words){ls_words[R_ERROR]=code;ls_words[R_SITE]=site;ls_words[R_EPOCH]=epoch;
  ls_words[R_MS]=now;ls_words[R_TRACES]=(uint32_t)trace_count;__sync_synchronize();SetEvent(ready);}
 ExitProcess(80+code%40);
}

void ls_trace(unsigned type,unsigned site,unsigned a,unsigned b,unsigned c,unsigned thread){
 uint32_t *p;LONG index;
 if(!ls_words)return;
 index=InterlockedIncrement(&trace_count)-1;
 if(index<0 || (unsigned)index>=LS_TRACE_LIMIT)ls_fail(1,site);
 p=(uint32_t*)((unsigned char*)ls_words+LS_TRACE_OFFSET)+index*8;
 p[0]=type;p[1]=epoch;p[2]=now;p[3]=site;p[4]=a;p[5]=b;p[6]=c;p[7]=thread;
}

void ls_pixels(const void*data,unsigned pitch,unsigned w,unsigned h,unsigned bpp,
               unsigned r,unsigned g,unsigned b){
 unsigned y;unsigned char*dst;
 if(!ls_words)return;
 if(!data || w!=640 || h!=480 || bpp!=16 || pitch<1280)ls_fail(2,0);
 dst=(unsigned char*)ls_words+LS_PIXEL_OFFSET;
 for(y=0;y<h;y++)memcpy(dst+y*1280,(const unsigned char*)data+y*pitch,1280);
 ls_words[R_PIXELS]=LS_PIXEL_BYTES;ls_words[R_RMASK]=r;ls_words[R_GMASK]=g;ls_words[R_BMASK]=b;
}
void ls_commit(unsigned source){
 unsigned n;uint32_t*h;unsigned char*slot;
 if(!ls_words)return;
 n=ls_words[R_SUBSTEPS];
 if(n>=LS_FRAME_LIMIT)ls_fail(5,source);
 if(ls_words[R_PIXELS]!=LS_PIXEL_BYTES)ls_fail(2,source);
 slot=(unsigned char*)ls_words+LS_FRAME_OFFSET+n*LS_FRAME_STRIDE;h=(uint32_t*)slot;
 h[0]=epoch;h[1]=now;h[2]=source;h[3]=LS_PIXEL_BYTES;
 h[4]=ls_words[R_RMASK];h[5]=ls_words[R_GMASK];h[6]=ls_words[R_BMASK];h[7]=n;
 memcpy(slot+32,(unsigned char*)ls_words+LS_PIXEL_OFFSET,LS_PIXEL_BYTES);
 ls_words[R_SUBSTEPS]=n+1;ls_words[R_PRESENTS]++;
}

int ls_exchange(unsigned scene,unsigned next,unsigned site){
 if(!ls_words)return 1;
 ls_words[R_EPOCH]=epoch;ls_words[R_MS]=now;ls_words[R_SCENE]=scene;
 ls_words[R_NEXT]=next;ls_words[R_SITE]=site;ls_words[R_TRACES]=(uint32_t)trace_count;
 __sync_synchronize();if(!SetEvent(ready) || WaitForSingleObject(resume,30000)!=WAIT_OBJECT_0)ls_fail(3,site);
 __sync_synchronize();
 if(ls_words[C_COMMAND]==LS_STOP)return 0;
 if(ls_words[C_COMMAND]!=LS_STEP || ls_words[C_EPOCH]!=epoch+1 || ls_words[C_MS]<now || ls_words[C_MS_HIGH])ls_fail(4,site);
 now=ls_words[C_MS];epoch=ls_words[C_EPOCH];trace_count=0;
 ls_words[R_TRACES]=0;ls_words[R_ACK]=0;ls_words[R_SUBSTEPS]=0;
 return 1;
}
