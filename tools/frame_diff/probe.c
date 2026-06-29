/* Test aid: observational by default; R4DIFF_LINK enables virtual scheduling. */
#include "probe.h"
#include "original_lockstep.h"
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tlhelp32.h>

#define QUEUE 32768
#define FRAME_SLOTS 8
#define FRAME_BYTES (1024*768*4)
static Event events[QUEUE];
static unsigned read_index, write_index;
static CRITICAL_SECTION queue_lock;
static volatile LONG lost, stopping, active_frames;
static HANDLE writer;
static FILE *trace;
static LARGE_INTEGER frequency, origin;
static uintptr_t base;
static char output[MAX_PATH];
static volatile LONG capture;
static int capture_interval;
static unsigned frame_number;
typedef struct { volatile LONG state; uint32_t header[16];
 unsigned char pixels[FRAME_BYTES]; } Frame;
static Frame frames[FRAME_SLOTS];
static uint64_t last_frame;
static DWORD tls_index;
typedef struct { uint64_t start[16]; unsigned kind[16],caller[16],before[16],arg[16];
 unsigned depth; uint32_t *rng; uint32_t clock_value[2],clock_count[2]; } ThreadState;
static ThreadState *thread_state(void) {
 ThreadState *s=TlsGetValue(tls_index);
 if (!s) { s=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*s));
  if (s) TlsSetValue(tls_index,s); }
 return s;
}
uint64_t probe_now(void) { LARGE_INTEGER n; QueryPerformanceCounter(&n);
 return (uint64_t)(n.QuadPart-origin.QuadPart)*1000000/frequency.QuadPart; }
void probe_event(unsigned type,uint64_t started,uintptr_t caller,uintptr_t object,
 int32_t result,uint32_t a,uint32_t b,uint32_t c,uint32_t d) {
 Event e; uint64_t n=probe_now();
 e.us=started?started:n; e.duration=started?n-started:0; e.type=type;
 e.thread=GetCurrentThreadId(); e.caller=(caller>=base && caller<base+0x100000)?caller-base:(uint32_t)caller;
 e.object=(uint32_t)object;e.result=result;e.a=a;e.b=b;e.c=c;e.d=d;
 EnterCriticalSection(&queue_lock);
 if(stopping){LeaveCriticalSection(&queue_lock);return;}
 if(write_index-read_index==QUEUE) InterlockedIncrement(&lost);
 else {events[write_index%QUEUE]=e;write_index++;}
 LeaveCriticalSection(&queue_lock);
 if(ls_active() && type>=EV_DDRAW && type<=EV_VBLANK && result<0)ls_fail(70+type,(unsigned)caller);
}
void probe_error(unsigned code,uintptr_t object) {
 probe_event(EV_ERROR,0,0,object,0,code,0,0,0);
 if(ls_active())ls_fail(100+code,(unsigned)object);
}
int probe_capture_enabled(void) {return capture;}
void probe_frame(void *obj,const DDSURFACEDESC *s,const RECT *rect,unsigned reason,HRESULT result) {
 unsigned i,y,w,h,bytes; uint64_t now=probe_now(); Frame *f=NULL;
 if (!capture || result!=DD_OK) return;
 if(!s->lpSurface || s->ddpfPixelFormat.dwRGBBitCount!=16 || s->lPitch<=0) {probe_error(20,(uintptr_t)obj);return;}
 w=rect?(unsigned)(rect->right-rect->left):s->dwWidth;
 h=rect?(unsigned)(rect->bottom-rect->top):s->dwHeight;bytes=w*2;
 if(!w || !h || w>1024 || h>768 || s->lPitch<(LONG)bytes) {probe_error(21,(uintptr_t)obj);return;}
 EnterCriticalSection(&queue_lock);
 if(!capture || stopping || (!rect && last_frame && capture_interval && now-last_frame<(uint64_t)capture_interval*1000)){
  LeaveCriticalSection(&queue_lock);return;}
 InterlockedIncrement(&active_frames);
 if(!rect)last_frame=now;
 LeaveCriticalSection(&queue_lock);
 for(i=0;i<FRAME_SLOTS;i++) if(InterlockedCompareExchange(&frames[i].state,1,0)==0){f=&frames[i];break;}
 if(!f){InterlockedIncrement(&lost);InterlockedDecrement(&active_frames);return;}
 memset(f->header,0,sizeof(f->header));
 f->header[0]=0x31463452;f->header[1]=InterlockedIncrement((LONG*)&frame_number);
 f->header[2]=(uint32_t)now;f->header[3]=(uint32_t)(now>>32);f->header[4]=w;f->header[5]=h;
 f->header[6]=16;f->header[7]=bytes;f->header[8]=s->ddpfPixelFormat.dwRBitMask;
 f->header[9]=s->ddpfPixelFormat.dwGBitMask;f->header[10]=s->ddpfPixelFormat.dwBBitMask;
 f->header[11]=(uint32_t)obj;f->header[12]=reason;
 f->header[13]=rect?(uint32_t)rect->left:0;f->header[14]=rect?(uint32_t)rect->top:0;
 f->header[15]=GetCurrentThreadId();
 for(y=0;y<h;y++) memcpy(f->pixels+y*bytes,(char*)s->lpSurface+y*s->lPitch,bytes);
 probe_event(EV_FRAME,now,0,(uintptr_t)obj,result,f->header[1],w,h,reason);
 InterlockedExchange(&f->state,2);
 InterlockedDecrement(&active_frames);
}
static DWORD WINAPI write_thread(void *unused) {
 Event batch[512]; unsigned n,i;char path[MAX_PATH]; (void)unused;
 for (;;) {
  EnterCriticalSection(&queue_lock); n=write_index-read_index;if(n>512)n=512;
  for(i=0;i<n;i++)batch[i]=events[(read_index+i)%QUEUE];read_index+=n;
  LeaveCriticalSection(&queue_lock);
  for(i=0;i<n;i++){Event *e=&batch[i];fprintf(trace,"%llu,%llu,%u,%u,%x,%x,%d,%u,%u,%u,%u\n",
   (unsigned long long)e->us,(unsigned long long)e->duration,e->type,e->thread,e->caller,e->object,e->result,e->a,e->b,e->c,e->d);}
  for(i=0;i<FRAME_SLOTS;i++)if(InterlockedCompareExchange(&frames[i].state,3,2)==2){
   Frame *f=&frames[i]; FILE *fp;snprintf(path,sizeof(path),"%s/frame-%06u.r4f",output,f->header[1]);
   fp=fopen(path,"wb");if(fp){if(fwrite(f->header,1,64,fp)!=64 || fwrite(f->pixels,1,f->header[7]*f->header[5],fp)!=f->header[7]*f->header[5])InterlockedIncrement(&lost);fclose(fp);}else InterlockedIncrement(&lost);
   InterlockedExchange(&f->state,0);
  }
  fflush(trace);
  if(stopping && n==0 && !active_frames)break;
  if(!n)Sleep(10);
 }
 fprintf(trace,"%llu,0,%d,0,0,0,0,%ld,0,0,0\n",(unsigned long long)probe_now(),EV_END,lost);
 fflush(trace);return 0;
}

static DWORD (WINAPI *real_tick)(void), (WINAPI *real_time)(void);
static MMRESULT (WINAPI *real_set)(UINT,UINT,LPTIMECALLBACK,DWORD_PTR,UINT);
static MMRESULT (WINAPI *real_kill)(UINT);
static BOOL (WINAPI *real_peek)(LPMSG,HWND,UINT,UINT,UINT);
static UINT_PTR (WINAPI *real_wset)(HWND,UINT_PTR,UINT,TIMERPROC);
static BOOL (WINAPI *real_wkill)(HWND,UINT_PTR);
static UINT_PTR WINAPI hooked_wset(HWND h,UINT_PTR id,UINT delay,TIMERPROC fn){uint64_t t=probe_now();UINT_PTR r=ls_active()?ols_set_window(h,id,delay,fn):real_wset(h,id,delay,fn);DWORD err=GetLastError();
 probe_event(EV_TIMER_SET,t,CALLER,(uintptr_t)h,r,delay,id,(uint32_t)fn,1);SetLastError(err);return r;}
static BOOL WINAPI hooked_wkill(HWND h,UINT_PTR id){uint64_t t=probe_now();BOOL r=ls_active()?ols_kill_window(h,id):real_wkill(h,id);DWORD err=GetLastError();
 probe_event(EV_TIMER_KILL,t,CALLER,(uintptr_t)h,r,id,0,0,1);SetLastError(err);return r;}
typedef struct { LPTIMECALLBACK callback; DWORD_PTR user; UINT id; } Timer;
static Timer timers[64];static LONG timer_count;
static DWORD clock_call(unsigned which,uintptr_t caller) {
 if(ls_active())return ols_clock(caller);
 DWORD ret=which?real_tick():real_time();DWORD err=GetLastError();ThreadState*s=thread_state();
 if(s){s->clock_count[which]++;if(s->clock_value[which]!=ret){
  probe_event(EV_CLOCK,0,caller,0,ret,which,s->clock_count[which],0,0);
  s->clock_value[which]=ret;s->clock_count[which]=0;}}
 SetLastError(err);return ret;
}
static DWORD WINAPI hooked_tick(void){return clock_call(1,CALLER);}
static DWORD WINAPI hooked_time(void){return clock_call(0,CALLER);}
static void CALLBACK timer_callback(UINT id,UINT msg,DWORD_PTR user,DWORD_PTR a,DWORD_PTR b){
 Timer *t=(Timer*)user;uint64_t start=probe_now();
 t->callback(id,msg,t->user,a,b);probe_event(EV_TIMER,start,(uintptr_t)t->callback,id,0,msg,0,0,0);
}
static MMRESULT WINAPI hooked_set(UINT delay,UINT resolution,LPTIMECALLBACK cb,DWORD_PTR user,UINT flags){
 unsigned index=(unsigned)InterlockedIncrement(&timer_count)-1; MMRESULT id;uint64_t start=probe_now();
 if(ls_active())return ols_set_mm(delay,resolution,cb,user,flags);
 if(index>=64 || (flags&(TIME_CALLBACK_EVENT_SET|TIME_CALLBACK_EVENT_PULSE))){
  probe_error(30,(uintptr_t)cb);return real_set(delay,resolution,cb,user,flags);}
 timers[index].callback=cb;timers[index].user=user;
 id=real_set(delay,resolution,timer_callback,(DWORD_PTR)&timers[index],flags);timers[index].id=id;
 probe_event(EV_TIMER_SET,start,CALLER,(uintptr_t)cb,id,delay,resolution,flags,index);return id;
}
static MMRESULT WINAPI hooked_kill(UINT id){uint64_t start=probe_now();MMRESULT r=ls_active()?ols_kill_mm(id):real_kill(id);
 probe_event(EV_TIMER_KILL,start,CALLER,id,r,0,0,0,0);return r;}
static BOOL WINAPI hooked_peek(LPMSG msg,HWND win,UINT low,UINT high,UINT flags){
 if(ls_active())return ols_peek(msg,win,low,high,flags,CALLER);
 uint64_t start=probe_now();BOOL r=real_peek(msg,win,low,high,flags);DWORD err=GetLastError();
 if(r || probe_now()-start>=1000)probe_event(EV_PEEK,start,CALLER,(uintptr_t)win,r,r?msg->message:0,r?(uint32_t)msg->wParam:0,flags,r?(uint32_t)msg->lParam:0);
 SetLastError(err);return r;
}
/* Assembly stubs preserve the original register/flag results and caller stack. */
void *tramp_rand,*tramp_seed,*tramp_wait,*tramp_wait_input,*tramp_audio;
extern void stub_rand(void),stub_seed(void),stub_wait(void),stub_wait_input(void),stub_audio(void);
void __cdecl internal_enter(unsigned kind,unsigned caller,unsigned arg){
 DWORD err=GetLastError();ThreadState*s=thread_state();
 if(s && s->depth<16){unsigned i=s->depth++;s->kind[i]=kind;s->caller[i]=caller;s->arg[i]=arg;
  s->before[i]=0;
  if(kind<=1){uint32_t*(__cdecl *state_fn)(void)=(void*)(base+0x56f23);s->rng=state_fn();if(s->rng)s->before[i]=*s->rng;}
  s->start[i]=probe_now();
 }else probe_error(40,caller);SetLastError(err);
}
void __cdecl internal_leave(unsigned result){
 DWORD err=GetLastError();ThreadState*s=thread_state();
 if(s && s->depth){unsigned i=--s->depth,k=s->kind[i];
  if(ls_active() && k<=1)ols_rng(k,s->caller[i],s->before[i],s->rng?*s->rng:0,result,s->arg[i]);
  probe_event(k==0?EV_RNG:k==1?EV_SEED:EV_WAIT,s->start[i],s->caller[i],k,result,
   k<=1?s->before[i]:s->arg[i],k<=1&&s->rng?*s->rng:0,s->arg[i],k);}
 SetLastError(err);
}
static int detour(unsigned rva,const unsigned char *expected,unsigned len,void*hook,void**tramp,int relative){
 unsigned char *p=(void*)(base+rva),*t;DWORD old;int32_t rel;
 if(memcmp(p,expected,len)){probe_error(41,(uintptr_t)p);return 0;}
 t=VirtualAlloc(NULL,len+5,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);if(!t)return 0;
 memcpy(t,p,len);if(relative){memcpy(&rel,p+1,4);rel=(int32_t)((p+5+rel)-(t+5));memcpy(t+1,&rel,4);}
 t[len]=0xe9;rel=(int32_t)((p+len)-(t+len+5));memcpy(t+len+1,&rel,4);*tramp=t;
 if(!VirtualProtect(p,len,PAGE_EXECUTE_READWRITE,&old))return 0;
 p[0]=0xe9;rel=(int32_t)((unsigned char*)hook-(p+5));memcpy(p+1,&rel,4);if(len>5)memset(p+5,0x90,len-5);
 VirtualProtect(p,len,old,&old);FlushInstructionCache(GetCurrentProcess(),p,len);return 1;
}
typedef struct { void **slot;void *hook; } Hook;
static Hook installed[256];static int installed_count;
static int patch(void **slot,void *hook,void **real){unsigned rva=(uintptr_t)slot-base;
 static const unsigned peek[]={0x192c,0x1d38,0x3f724,0x3fa3b,0x514cf,0x5288d,0x528f2,0x5452d,0x61084,0};
 static const unsigned tick[]={0x1707,0x2f99,0x1d066,0x61096,0};
 static const unsigned clock[]={0x2b08,0x2fae,0xb5e7,0xb680,0xb7c0,0xb859,0xe813,0xe97e,0x3f2b3,0x3f709,0x3f83e,0x3fa23,0x4ee07,0x4ee3f,0x51490,0x514b7,0x5286c,0x52875,0x528d1,0x528da,0x543c9,0x543ed,0x544fd,0x5458f,0x61204,0};
 static const unsigned kill[]={0x21e5,0x6120a,0},set[]={0x21a5,0x61210,0};
 static const unsigned wset[]={0x4fa3,0x5cec,0x6217,0x6c30,0x11249,0x148e3,0x14c42,0x15034,0x180ec,0x2b4aa,0x2d468,0x2f8b8,0x30186,0x32684,0x33144,0x34565,0x35165,0x36139,0x37efa,0x3a3ae,0x3cb4c,0x3dae2,0x4619d,0x61012,0};
 static const unsigned wkill[]={0x5a36,0x5aee,0x5cda,0x5f72,0x699d,0x6d67,0x11782,0x117c4,0x11942,0x14941,0x14d22,0x150de,0x192db,0x2b52c,0x2dc21,0x2e8a4,0x2fb17,0x30abd,0x32a54,0x33293,0x3378e,0x34921,0x35880,0x36237,0x39185,0x3b5c7,0x3d278,0x3df7a,0x46110,0x6100c,0};
 const unsigned*sites=rva==0x6230c?peek:rva==0x623cc?tick:rva==0x6246c?clock:rva==0x62470?kill:rva==0x62324?wset:rva==0x622fc?wkill:set;
 return probe_patch_sites(rva,sites,hook,real);
}
int probe_patch_sites(unsigned rva,const unsigned*sites,void*hook,void**real){
 DWORD old;unsigned i;void**cell;void**slot=(void**)(base+rva);
 *real=*slot;cell=HeapAlloc(GetProcessHeap(),0,4);if(!cell)return 0;*cell=hook;
 for(i=0;sites[i];i++){unsigned char*p=(void*)(base+sites[i]);void**operand;
  if(p[0]==0x2e)p++;
  if(p[0]!=0xff || (p[1]!=0x15 && p[1]!=0x25) || *(uint32_t*)(p+2)!=(uint32_t)slot){probe_error(43,(uintptr_t)p);return 0;}
  operand=(void**)(p+2);if(!VirtualProtect(operand,4,PAGE_EXECUTE_READWRITE,&old))return 0;
  *operand=cell;VirtualProtect(operand,4,old,&old);FlushInstructionCache(GetCurrentProcess(),operand,4);
  if(installed_count>=256){probe_error(44,(uintptr_t)p);return 0;}
  installed[installed_count].slot=operand;installed[installed_count++].hook=cell;
 }
 probe_event(EV_HOOK,0,0,(uintptr_t)*real,0,(uint32_t)slot,0,0,0);return 1;}
static int imports(void){
 IMAGE_DOS_HEADER*d=(void*)base;IMAGE_NT_HEADERS*n=(void*)(base+d->e_lfanew);
 IMAGE_IMPORT_DESCRIPTOR*i=(void*)(base+n->OptionalHeader.DataDirectory[1].VirtualAddress);int found=0;
 for(;i->Name;i++){IMAGE_THUNK_DATA *o=(void*)(base+i->OriginalFirstThunk),*t=(void*)(base+i->FirstThunk);
  if(!i->OriginalFirstThunk)continue;
  for(;o->u1.AddressOfData;o++,t++){const char*name;if(IMAGE_SNAP_BY_ORDINAL(o->u1.Ordinal))continue;
   name=(char*)((IMAGE_IMPORT_BY_NAME*)(base+o->u1.AddressOfData))->Name;
#define IAT(nm,fn,real) if(!strcmp(name,nm))found+=patch((void**)&t->u1.Function,fn,(void**)&real)
   IAT("GetTickCount",hooked_tick,real_tick);
   else IAT("timeGetTime",hooked_time,real_time);
   else IAT("timeSetEvent",hooked_set,real_set);
   else IAT("timeKillEvent",hooked_kill,real_kill);
   else IAT("PeekMessageA",hooked_peek,real_peek);
   else IAT("SetTimer",hooked_wset,real_wset);
   else IAT("KillTimer",hooked_wkill,real_wkill);
   else if(!strcmp(name,"DirectDrawCreate"))found+=probe_graphics_install((void**)&t->u1.Function);
  }
 }return found==8;
}
__declspec(dllexport) DWORD WINAPI ProbeStop(void *unused){int i;(void)unused;
 for(i=0;i<installed_count;i++)if(*installed[i].slot!=installed[i].hook)probe_error(42,(uintptr_t)installed[i].slot);
 probe_graphics_audit();
 EnterCriticalSection(&queue_lock);capture=0;LeaveCriticalSection(&queue_lock);
 {DWORD started=GetTickCount();while(active_frames){if(GetTickCount()-started>4000)return 0xffffffff;Sleep(1);}}
 EnterCriticalSection(&queue_lock);InterlockedExchange(&stopping,1);LeaveCriticalSection(&queue_lock);
 if(WaitForSingleObject(writer,5000)!=WAIT_OBJECT_0)return 0xffffffff;return (DWORD)lost;}
__declspec(dllexport) DWORD WINAPI ProbeStart(void *unused){char path[MAX_PATH],buf[32];int ok;(void)unused;
 base=(uintptr_t)GetModuleHandle(NULL);QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&origin);
 InitializeCriticalSection(&queue_lock);tls_index=TlsAlloc();
 if(!GetEnvironmentVariableA("R4DIFF_OUT",output,sizeof(output)))return 1;
 snprintf(path,sizeof(path),"%s/events.csv",output);trace=fopen(path,"wb");if(!trace)return 2;
 setvbuf(trace,NULL,_IOFBF,65536);fprintf(trace,"us,duration_us,type,thread,caller,object,result,a,b,c,d\n");
 capture=GetEnvironmentVariableA("R4DIFF_CAPTURE",buf,sizeof(buf))?atoi(buf):0;
 capture_interval=GetEnvironmentVariableA("R4DIFF_INTERVAL",buf,sizeof(buf))?atoi(buf):100;
 writer=CreateThread(NULL,0,write_thread,NULL,0,NULL);if(!writer)return 3;
 probe_event(EV_BOOT,0,0,base,0,capture,capture_interval,GetModuleHandleA("dxwnd.dll")!=NULL,0);
 ok=imports();ok&=ols_init();
 ok&=detour(0x56f2d,(unsigned char*)"\xe8\xf1\xff\xff\xff",5,stub_rand,&tramp_rand,1);
 ok&=detour(0x56f50,(unsigned char*)"\xe8\xce\xff\xff\xff",5,stub_seed,&tramp_seed,1);
 ok&=detour(0x5285e,(unsigned char*)"\x53\x56\x57\x83\xec\x1c",6,stub_wait,&tramp_wait,0);
 ok&=detour(0x528b9,(unsigned char*)"\x53\x56\x57\x55\x83\xec\x1c",7,stub_wait_input,&tramp_wait_input,0);
 ok&=detour(0x543c4,(unsigned char*)"\x53\x56\x83\xec\x04",5,stub_audio,&tramp_audio,0);
 probe_event(EV_BOOT,0,0,base,ok,1,0,0,0);return ok?0:4;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved){(void)reserved;
 if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(instance);return TRUE;}
