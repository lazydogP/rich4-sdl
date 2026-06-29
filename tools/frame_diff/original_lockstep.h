#ifndef ORIGINAL_LOCKSTEP_H
#define ORIGINAL_LOCKSTEP_H
#include "probe.h"
#include "lockstep_wire.h"
#include <mmsystem.h>
int ols_init(void);
DWORD ols_clock(uintptr_t caller);
BOOL ols_peek(LPMSG msg,HWND win,UINT low,UINT high,UINT flags,uintptr_t caller);
UINT ols_set_mm(UINT delay,UINT resolution,LPTIMECALLBACK cb,DWORD_PTR user,UINT flags);
MMRESULT ols_kill_mm(UINT id);
UINT_PTR ols_set_window(HWND win,UINT_PTR id,UINT delay,TIMERPROC fn);
BOOL ols_kill_window(HWND win,UINT_PTR id);
void ols_rng(unsigned kind,unsigned site,unsigned before,unsigned after,unsigned result,unsigned arg);
void probe_graphics_checkpoint(void);
int probe_patch_sites(unsigned slot_rva,const unsigned *sites,void *hook,void **real);
#endif
