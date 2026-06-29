/* Test scheduler for the verified Rich4 binary. No wall clock drives gameplay. */
#include "original_lockstep.h"
#include "original_post_sites.h"
#include <string.h>

static uintptr_t image_base;
static DWORD main_tid,worker_tid;
static HANDLE work_event,work_done;
static WNDPROC game_proc;
static HOOKPROC keyboard_proc;
static int authorized_dispatch;
static int issued_virtual;
static int empty_returned;
static POINT pointer={320,240};
static MSG queue[256];static unsigned queue_count;
typedef struct {unsigned active,id,delay,next,flags;HWND win;LPTIMECALLBACK cb;DWORD_PTR user;} VirtualTimer;
static VirtualTimer mm[16],wt[16];
static VirtualTimer *worker_timer;
static unsigned next_mm=1,consumed;
static BOOL (WINAPI *native_peek)(LPMSG,HWND,UINT,UINT,UINT);
static LRESULT (WINAPI *native_dispatch)(const MSG*);
static ATOM (WINAPI *native_register)(const WNDCLASSA*);
static void *unused_next;
extern DWORD WINAPI ProbeStop(void*);

static HWND game_window(void){return *(HWND*)(image_base+0x8a0d4);}
static unsigned site(uintptr_t caller){return (unsigned)(caller-image_base);}
static void enqueue(HWND win,UINT message,WPARAM w,LPARAM l){
 MSG*m;if(queue_count>=256)ls_fail(30,message);
 m=&queue[queue_count++];memset(m,0,sizeof(*m));m->hwnd=win;m->message=message;
 m->wParam=w;m->lParam=l;m->time=ls_ticks();m->pt=pointer;
}
static BOOL WINAPI virtual_post(HWND win,UINT message,WPARAM w,LPARAM l){enqueue(win,message,w,l);return TRUE;}
static BOOL WINAPI virtual_cursor(POINT*p){*p=pointer;return TRUE;}
static BOOL WINAPI virtual_warp(int x,int y){
 pointer.x=x;pointer.y=y;ls_trace(LS_WARP,site(CALLER),x,y,0,0);return TRUE;
}
static void WINAPI virtual_date(SYSTEMTIME*t){
 memset(t,0,sizeof(*t));t->wYear=ls_words[C_YEAR];t->wMonth=ls_words[C_MONTH];t->wDay=ls_words[C_DAY];
 t->wSecond=(ls_ms()/1000)%60;t->wMilliseconds=ls_ms()%1000;
}
static HHOOK WINAPI virtual_keyboard(int kind,HOOKPROC fn,HINSTANCE module,DWORD tid){
 (void)module;(void)tid;if(kind!=WH_KEYBOARD)ls_fail(31,kind);keyboard_proc=fn;return (HHOOK)1;
}
static LRESULT CALLBACK window_proc(HWND w,UINT msg,WPARAM a,LPARAM b){
 if(msg==WM_ACTIVATEAPP)a=TRUE;
 if(!authorized_dispatch && ((msg>=WM_KEYFIRST&&msg<=WM_KEYLAST) || (msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST) || msg==WM_TIMER))return 0;
 return game_proc(w,msg,a,b);
}
static ATOM WINAPI virtual_register(const WNDCLASSA*c){WNDCLASSA copy=*c;game_proc=c->lpfnWndProc;copy.lpfnWndProc=window_proc;return native_register(&copy);}
static LRESULT WINAPI virtual_dispatch(const MSG*m){
 LRESULT r;int controlled=issued_virtual;issued_virtual=0;
 authorized_dispatch++;
 /* Synthetic messages already use game coordinates. The native DxWnd
    DispatchMessage path would apply its client-to-game transform twice. */
 r=controlled?game_proc(m->hwnd,m->message,m->wParam,m->lParam):native_dispatch(m);
 authorized_dispatch--;return r;
}
static DWORD WINAPI worker(void*arg){
 (void)arg;worker_tid=GetCurrentThreadId();
 for(;;){VirtualTimer*t;
  if(WaitForSingleObject(work_event,30000)!=WAIT_OBJECT_0)ls_fail(32,0);
  t=worker_timer;ls_trace(LS_TIMER,(unsigned)((uintptr_t)t->cb-image_base),t->id,t->delay,0,1);
  t->cb(t->id,0,t->user,0,0);SetEvent(work_done);
 }return 0;
}
UINT ols_set_mm(UINT delay,UINT resolution,LPTIMECALLBACK cb,DWORD_PTR user,UINT flags){
 unsigned i;(void)resolution;
 if(!delay || !cb || (flags&~(TIME_PERIODIC|TIME_KILL_SYNCHRONOUS)))ls_fail(33,flags);
 for(i=0;i<16;i++)if(!mm[i].active){VirtualTimer*t=&mm[i];
  t->active=1;t->id=next_mm++;t->delay=delay;t->next=ls_ms()+delay;t->flags=flags;t->cb=cb;t->user=user;return t->id;}
 ls_fail(34,0);return 0;
}
MMRESULT ols_kill_mm(UINT id){unsigned i;for(i=0;i<16;i++)if(mm[i].active&&mm[i].id==id){mm[i].active=0;return TIMERR_NOERROR;}return TIMERR_NOCANDO;}
UINT_PTR ols_set_window(HWND win,UINT_PTR id,UINT delay,TIMERPROC fn){
 unsigned i,free_slot=16;if(fn || !win)ls_fail(35,(unsigned)(uintptr_t)fn);
 if(delay<10)delay=10;if(delay>0x7fffffff)delay=0x7fffffff;
 for(i=0;i<16;i++){if(wt[i].active&&wt[i].win==win&&wt[i].id==id){free_slot=i;break;}if(!wt[i].active&&free_slot==16)free_slot=i;}
 if(free_slot==16)ls_fail(34,1);
 wt[free_slot].active=1;wt[free_slot].win=win;wt[free_slot].id=id?id:free_slot+1;
 wt[free_slot].delay=delay;wt[free_slot].next=ls_ms()+delay;return wt[free_slot].id;
}
BOOL ols_kill_window(HWND win,UINT_PTR id){unsigned i;for(i=0;i<16;i++)if(wt[i].active&&wt[i].win==win&&wt[i].id==id){wt[i].active=0;return TRUE;}return FALSE;}
static unsigned next_deadline(void){unsigned i,next=ls_ms()+20;
 for(i=0;i<16;i++){if(mm[i].active&&mm[i].next<next)next=mm[i].next;if(wt[i].active&&wt[i].next<next)next=wt[i].next;}return next;}
static void timers_due(void){unsigned i;
 for(i=0;i<16;i++)if(mm[i].active&&mm[i].next<=ls_ms()){
  VirtualTimer*t=&mm[i];if(t->next!=ls_ms())ls_fail(36,t->next);
  if(t->flags&TIME_PERIODIC)t->next+=t->delay;else t->active=0;
  worker_timer=t;SetEvent(work_event);if(WaitForSingleObject(work_done,5000)!=WAIT_OBJECT_0)ls_fail(32,t->id);
 }
 for(i=0;i<16;i++)if(wt[i].active&&wt[i].next<=ls_ms()){
  VirtualTimer*t=&wt[i];unsigned j,found=0;if(t->next!=ls_ms())ls_fail(36,t->next);
  for(j=0;j<queue_count;j++)if(queue[j].hwnd==t->win&&queue[j].message==WM_TIMER&&queue[j].wParam==t->id)found=1;
  if(!found)enqueue(t->win,WM_TIMER,t->id,0);t->next+=t->delay;
 }
}
static void input_due(void){unsigned kind,id=ls_words[C_EVENT];UINT message;WPARAM w=0;LPARAM l;
 if(!id || id==consumed)return;consumed=id;kind=ls_words[C_KIND];
 if(kind<=LS_UP){pointer.x=ls_words[C_X];pointer.y=ls_words[C_Y];l=MAKELPARAM(pointer.x,pointer.y);
  message=kind==LS_MOVE?WM_MOUSEMOVE:kind==LS_DOWN?WM_LBUTTONDOWN:WM_LBUTTONUP;if(kind==LS_DOWN)w=MK_LBUTTON;
 }else{message=kind==LS_KEYDOWN?WM_KEYDOWN:WM_KEYUP;w=ls_words[C_KEY];l=kind==LS_KEYDOWN?1:0xc0000001;
  if(kind!=LS_KEYDOWN&&kind!=LS_KEYUP)ls_fail(37,kind);
  if(keyboard_proc && keyboard_proc(HC_ACTION,w,l)){ls_words[R_ACK]=id;ls_trace(LS_INPUT,0,id,kind,1,0);return;}
 }
 enqueue(game_window(),message,w,l);ls_words[R_ACK]=id;ls_trace(LS_INPUT,0,id,kind,0,0);
}
static unsigned scene(void){
 int depth=*(int*)(image_base+0x6cad8);uintptr_t callback;
 if(depth<0||depth>=16)ls_fail(38,depth);
 callback=*(uintptr_t*)(image_base+0x8a010+depth*4)-image_base;
 if(callback==0x257a)return 0;
 if(callback==0x103a3)return 1;
 if(callback==0x4e44)return 2;
 if(callback==0x15872)return 3;
 if(callback==0x17e26)return 4;
 if(callback==0x5367e)return 4;
 /* The V3.11 executable stores the main-loop callback as a signed
    sentinel during the first post-setup handoff before the normal
    0x417E26 registration becomes visible at a safe point. */
 if(callback==0xffc00000u)return 4;
 ls_fail(38,(unsigned)callback);return 0xffffffff;
}
DWORD ols_clock(uintptr_t caller){
 /* A read never advances time. Unsupported busy loops hit the controller watchdog. */
 ls_words[R_SITE]=site(caller);return ls_ticks();
}
void ols_rng(unsigned kind,unsigned caller,unsigned before,unsigned after,unsigned result,unsigned arg){
 ls_trace(kind?LS_SEED:LS_RNG,site(caller),before,after,kind?arg:result,GetCurrentThreadId()==worker_tid?1:0);
}
BOOL ols_peek(LPMSG msg,HWND win,UINT low,UINT high,UINT flags,uintptr_t caller){
 unsigned i;MSG host;DWORD tid=GetCurrentThreadId();
 if(!main_tid)main_tid=tid;if(tid!=main_tid)ls_fail(39,site(caller));
 if(flags!=PM_REMOVE && flags!=PM_NOREMOVE)ls_fail(40,flags);
 for(;;){
  for(i=0;i<queue_count;i++)if((!win||win==queue[i].hwnd)&&((!low&&!high)||(queue[i].message>=low&&queue[i].message<=high))){
   *msg=queue[i];issued_virtual=1;if(flags&PM_REMOVE){memmove(queue+i,queue+i+1,(queue_count-i-1)*sizeof(MSG));queue_count--;}return TRUE;
  }
  while(native_peek(&host,win,low,high,PM_REMOVE)){
   if(host.message==WM_QUIT)ls_fail(41,0);
   if((host.message>=WM_KEYFIRST&&host.message<=WM_KEYLAST)||(host.message>=WM_MOUSEFIRST&&host.message<=WM_MOUSELAST)||host.message==WM_TIMER||host.message==MM_MCINOTIFY)continue;
   /* Native paint/lifecycle messages maintain the real DxWnd window. */
   *msg=host;issued_virtual=0;return TRUE;
  }
  if(low||high||win)ls_fail(42,site(caller));
  /* Let the original caller execute the empty-poll continuation once.
     In particular, waiting functions must recheck elapsed virtual time. */
  if(!empty_returned){empty_returned=1;return FALSE;}
  probe_graphics_audit();probe_graphics_checkpoint();
  if(!ls_exchange(scene(),next_deadline(),site(caller))){ProbeStop(NULL);ExitProcess(0);}
  empty_returned=0;
  input_due();timers_due();
 }
}
int ols_init(void){int ok,r;unsigned const cursor[]={0x102f,0x2068,0x2284,0x2385,0x24cb,0x135ff,0x151b0,0x32a25,0x45f48,0x610ae,0};
 unsigned const warp[]={0x107a,0x1361c,0x1363c,0x18db0,0x3fb54,0x3ffc2,0x40355,0x467de,0x53719,0x610a8,0};
 unsigned const date[]={0x5833c,0x60f7c,0},keyboard[]={0x178b,0x61090,0},reg[]={0x1c05,0},dispatch[]={0x194e,0x1d5f,0x61078,0};
 r=ls_open(1);if(!r)return 1;if(r<0)return 0;
 image_base=(uintptr_t)GetModuleHandle(NULL);native_peek=(void*)*(uintptr_t*)(image_base+0x6230c);
 ok=probe_patch_sites(0x622ec,cursor,virtual_cursor,&unused_next);
 ok&=probe_patch_sites(0x6231c,warp,virtual_warp,&unused_next);
 ok&=probe_patch_sites(0x623b0,date,virtual_date,&unused_next);
 ok&=probe_patch_sites(0x62328,keyboard,virtual_keyboard,&unused_next);
 ok&=probe_patch_sites(0x62310,post_sites,virtual_post,&unused_next);
 ok&=probe_patch_sites(0x62318,reg,virtual_register,(void**)&native_register);
 ok&=probe_patch_sites(0x622e0,dispatch,virtual_dispatch,(void**)&native_dispatch);
 work_event=CreateEvent(NULL,FALSE,FALSE,NULL);work_done=CreateEvent(NULL,FALSE,FALSE,NULL);
 if(!work_event||!work_done||!CreateThread(NULL,0,worker,NULL,0,NULL))return 0;
 return ok;
}
