#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static HANDLE process,job;static DWORD pid;
static HANDLE debug_ready;
static DWORD WINAPI debugger(void*unused){DEBUG_EVENT e;(void)unused;
 if(!DebugActiveProcess(pid)){SetEvent(debug_ready);return 1;}DebugSetProcessKillOnExit(FALSE);SetEvent(debug_ready);
 while(WaitForDebugEvent(&e,30000)) {DWORD status=DBG_CONTINUE;
  if(e.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){DWORD c=e.u.Exception.ExceptionRecord.ExceptionCode;
   printf("exception=%08lx address=%p first=%lu\n",c,e.u.Exception.ExceptionRecord.ExceptionAddress,e.u.Exception.dwFirstChance);fflush(stdout);
   if(c!=EXCEPTION_BREAKPOINT){HANDLE th=OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,e.dwThreadId);CONTEXT ctx;DWORD words[64];SIZE_T got=0;unsigned i;
    memset(&ctx,0,sizeof(ctx));ctx.ContextFlags=CONTEXT_CONTROL;if(th && GetThreadContext(th,&ctx) && ReadProcessMemory(process,(void*)ctx.Esp,words,sizeof(words),&got)){
     printf("stack esp=%08lx:",ctx.Esp);for(i=0;i<got/4;i++)printf(" %08lx",words[i]);printf("\n");fflush(stdout);}
    if(th)CloseHandle(th);
   }
   if(c!=EXCEPTION_BREAKPOINT)status=DBG_EXCEPTION_NOT_HANDLED;}
  if(e.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT && e.u.LoadDll.hFile)CloseHandle(e.u.LoadDll.hFile);
  ContinueDebugEvent(e.dwProcessId,e.dwThreadId,status);
  if(e.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT)break;
 }return 0;}
static uintptr_t remote_module(const char*name){MODULEENTRY32 m;HANDLE h;uintptr_t ret=0;
 h=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid);if(h==INVALID_HANDLE_VALUE)return 0;
 m.dwSize=sizeof(m);if(Module32First(h,&m))do {if(!_stricmp(m.szModule,name)){ret=(uintptr_t)m.modBaseAddr;break;}}while(Module32Next(h,&m));CloseHandle(h);return ret;}
static DWORD remote_call(uintptr_t address,void*arg,DWORD timeout){HANDLE t;DWORD code=0;
 t=CreateRemoteThread(process,NULL,0,(LPTHREAD_START_ROUTINE)address,arg,0,NULL);
 if(!t || WaitForSingleObject(t,timeout)!=WAIT_OBJECT_0){fprintf(stderr,"remote call failed %lu\n",GetLastError());if(t)CloseHandle(t);return 0xffffffff;}
 GetExitCodeThread(t,&code);printf("remote_result=%08lx\n",code);fflush(stdout);CloseHandle(t);return code;}
static uintptr_t load_remote(const char*path){void*mem;DWORD ret;HMODULE owner;char mod[MAX_PATH];uintptr_t addr;
 void*fn=(void*)GetProcAddress(GetModuleHandleA("kernel32.dll"),"LoadLibraryA");
 GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)fn,&owner);
 GetModuleFileNameA(owner,mod,sizeof(mod));addr=remote_module(strrchr(mod,'\\')+1);
 if(!addr){fprintf(stderr,"remote loader module unavailable\n");return 0;}addr+=(uintptr_t)fn-(uintptr_t)owner;
 mem=VirtualAllocEx(process,NULL,strlen(path)+1,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!mem || !WriteProcessMemory(process,mem,path,strlen(path)+1,NULL))return 0;
 ret=remote_call(addr,mem,20000);
 if(ret!=0xffffffff)VirtualFreeEx(process,mem,0,MEM_RELEASE);
 return ret==0xffffffff?0:ret;}
static uintptr_t exported(uintptr_t remote,const char*path,const char*name){HMODULE m=LoadLibraryExA(path,NULL,DONT_RESOLVE_DLL_REFERENCES);uintptr_t r;
 if(!m)return 0;r=(uintptr_t)GetProcAddress(m,name);if(r)r=remote+r-(uintptr_t)m;FreeLibrary(m);return r;}
static BOOL CALLBACK find_window(HWND w,LPARAM p){DWORD id;GetWindowThreadProcessId(w,&id);
 if(id==pid && IsWindowVisible(w) && GetWindow(w,GW_OWNER)==NULL){*(HWND*)p=w;return FALSE;}return TRUE;}
static void click_logical(HWND win,int lx,int ly){RECT r;POINT p;int x,y;GetClientRect(win,&r);x=lx*r.right/640;y=ly*r.bottom/480;p.x=x;p.y=y;ClientToScreen(win,&p);SetCursorPos(p.x,p.y);
 PostMessage(win,WM_MOUSEMOVE,0,MAKELPARAM(x,y));PostMessage(win,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));PostMessage(win,WM_LBUTTONUP,0,MAKELPARAM(x,y));}
static int loader_barrier(HANDLE main_thread){DEBUG_EVENT e;DWORD until=GetTickCount()+15000;
 BYTE saved=0,trap=0xcc;void*entry=NULL;
 DebugSetProcessKillOnExit(FALSE);
 while((LONG)(until-GetTickCount())>0 && WaitForDebugEvent(&e,1000)){
  DWORD status=DBG_CONTINUE;int ready=0;
  if(e.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT){
   entry=e.u.CreateProcessInfo.lpStartAddress;
   if(!ReadProcessMemory(process,entry,&saved,1,NULL) || !WriteProcessMemory(process,entry,&trap,1,NULL))return 0;
   FlushInstructionCache(process,entry,1);
   if(e.u.CreateProcessInfo.hFile)CloseHandle(e.u.CreateProcessInfo.hFile);
  }
  if(e.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT && e.u.LoadDll.hFile)CloseHandle(e.u.LoadDll.hFile);
  if(e.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){
   if(e.u.Exception.ExceptionRecord.ExceptionCode==EXCEPTION_BREAKPOINT){
    if(e.u.Exception.ExceptionRecord.ExceptionAddress==entry){CONTEXT c;memset(&c,0,sizeof(c));c.ContextFlags=CONTEXT_CONTROL;
     if(!WriteProcessMemory(process,entry,&saved,1,NULL) || !GetThreadContext(main_thread,&c))return 0;
     c.Eip=(DWORD)entry;if(!SetThreadContext(main_thread,&c))return 0;FlushInstructionCache(process,entry,1);
     if(SuspendThread(main_thread)==(DWORD)-1)return 0;ready=1;
    }
   }else status=DBG_EXCEPTION_NOT_HANDLED;
  }
  ContinueDebugEvent(e.dwProcessId,e.dwThreadId,status);
  if(ready)return DebugActiveProcessStop(pid)!=0;
  if(e.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT)return 0;
 }return 0;}
static void modules(const char*out){char path[MAX_PATH];FILE*f;MODULEENTRY32 m;HANDLE h;
 snprintf(path,sizeof(path),"%s/modules.tsv",out);f=fopen(path,"wb");if(!f)return;
 h=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid);m.dwSize=sizeof(m);
 if(h!=INVALID_HANDLE_VALUE){if(Module32First(h,&m))do{fprintf(f,"%08lx\t%lu\t%s\n",(unsigned long)m.modBaseAddr,m.modBaseSize,m.szExePath);}while(Module32Next(h,&m));CloseHandle(h);}fclose(f);}
int main(int argc,char**argv){STARTUPINFOA si={0};PROCESS_INFORMATION pi={0};char cmd[1024],cwd[MAX_PATH];uintptr_t probe,start,stop;DWORD begin,code;int duration,script=0,step=-1;HWND win=NULL;POINT saved_cursor;
 if(argc!=8){fprintf(stderr,"usage: launcher game dxwnd.dll probe.dll output seconds capture interval_ms\n");return 2;}
 duration=atoi(argv[5]);if(duration<1||duration>300)return 2;
 if(GetEnvironmentVariableA("R4DIFF_SCENARIO",cmd,sizeof(cmd)))script=!strcmp(cmd,"setup")?2:!strcmp(cmd,"game")?3:1;
 GetCursorPos(&saved_cursor);
 SetEnvironmentVariableA("R4DIFF_OUT",argv[4]);SetEnvironmentVariableA("R4DIFF_CAPTURE",argv[6]);SetEnvironmentVariableA("R4DIFF_INTERVAL",argv[7]);
 snprintf(cmd,sizeof(cmd),"\"%s\"",argv[1]);strncpy(cwd,argv[1],sizeof(cwd));cwd[sizeof(cwd)-1]=0;*strrchr(cwd,'\\')=0;
 si.cb=sizeof(si);if(!CreateProcessA(argv[1],cmd,NULL,NULL,FALSE,DEBUG_ONLY_THIS_PROCESS,NULL,cwd,&si,&pi)){fprintf(stderr,"CreateProcess failed %lu\n",GetLastError());return 3;}
 process=pi.hProcess;pid=pi.dwProcessId;printf("pid=%lu\n",pid);fflush(stdout);
 {JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;memset(&limits,0,sizeof(limits));limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  job=CreateJobObject(NULL,NULL);if(!job || !SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)) || !AssignProcessToJobObject(job,process)){fprintf(stderr,"job containment failed\n");goto failed;}}
 if(!loader_barrier(pi.hThread)){fprintf(stderr,"loader barrier failed\n");goto failed;}
 if(GetEnvironmentVariableA("R4DIFF_DEBUG",cmd,sizeof(cmd))){debug_ready=CreateEvent(NULL,TRUE,FALSE,NULL);CreateThread(NULL,0,debugger,NULL,0,NULL);WaitForSingleObject(debug_ready,5000);}
 if(!load_remote(argv[2]) || !remote_module("dxwnd.dll")){fprintf(stderr,"DxWnd injection failed\n");goto failed;}
 probe=load_remote(argv[3]);if(!probe){fprintf(stderr,"probe injection failed\n");goto failed;}
 start=exported(probe,argv[3],"ProbeStart@4");stop=exported(probe,argv[3],"ProbeStop@4");
 if(!start)start=exported(probe,argv[3],"ProbeStart");if(!stop)stop=exported(probe,argv[3],"ProbeStop");
 if(!start||!stop||remote_call(start,NULL,15000)!=0){fprintf(stderr,"probe bootstrap failed\n");goto failed;}
 modules(argv[4]);ResumeThread(pi.hThread);begin=GetTickCount();
 while(GetTickCount()-begin<(DWORD)duration*1000){if(WaitForSingleObject(process,100)==WAIT_OBJECT_0)break;
  EnumWindows(find_window,(LPARAM)&win);
  if(script && win){int current=(GetTickCount()-begin)/1000;
   if(current!=step){step=current;
    if(step==3 || step==4){SetForegroundWindow(win);PostMessage(win,WM_KEYDOWN,VK_ESCAPE,1);PostMessage(win,WM_KEYUP,VK_ESCAPE,0xc0000001);printf("input_ms=%lu escape\n",GetTickCount()-begin);}
    if(step>=5 && step<=8){RECT r;POINT p;GetClientRect(win,&r);p.x=(100+(step-5)*100)*r.right/640;p.y=300*r.bottom/480;ClientToScreen(win,&p);SetCursorPos(p.x,p.y);printf("input_ms=%lu move %d 300\n",GetTickCount()-begin,100+(step-5)*100);}
    if(step==9){click_logical(win,script>=2?190:468,378);printf("input_ms=%lu %s_click\n",GetTickCount()-begin,script>=2?"start":"options");}
    if(script==3 && step>=11 && step<=14){click_logical(win,38+74*(step-11),45);printf("input_ms=%lu character %d\n",GetTickCount()-begin,step-11);}
    if(script==3 && step==15){click_logical(win,495,195);printf("input_ms=%lu setup_ok\n",GetTickCount()-begin);}
    if(step==12 && script==1){PostMessage(win,WM_KEYDOWN,VK_ESCAPE,1);PostMessage(win,WM_KEYUP,VK_ESCAPE,0xc0000001);printf("input_ms=%lu escape\n",GetTickCount()-begin);}
   }
  }
 }
 if(WaitForSingleObject(process,0)!=WAIT_OBJECT_0){modules(argv[4]);code=remote_call(stop,NULL,10000);printf("probe_stop=%lu\n",code);
  if(win)PostMessage(win,WM_CLOSE,0,0);
  if(WaitForSingleObject(process,1500)!=WAIT_OBJECT_0)TerminateProcess(process,0);
 }else {GetExitCodeProcess(process,&code);fprintf(stderr,"target exited before capture deadline code=%08lx\n",code);CloseHandle(pi.hThread);CloseHandle(process);return !code&&GetEnvironmentVariableA("R4DIFF_LINK",cmd,sizeof(cmd))?0:5;}
 if(script)SetCursorPos(saved_cursor.x,saved_cursor.y);
 CloseHandle(pi.hThread);CloseHandle(process);CloseHandle(job);return code?6:0;
failed:
 GetExitCodeProcess(process,&code);fprintf(stderr,"target status=%08lx error=%lu\n",code,GetLastError());
 modules(argv[4]);TerminateProcess(process,1);CloseHandle(pi.hThread);CloseHandle(process);return 4;
}
