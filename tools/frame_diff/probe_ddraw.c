#include "probe.h"
#include "original_lockstep.h"
#include <string.h>

/* Shared vtable slots preserve the DxWnd next hop and object identity. Interface 1 is verified
   for this Rich4 binary. Other interface requests are explicit coverage gaps. */
typedef struct { void *object;void **vt; void *next[36],*table[36]; DDSURFACEDESC desc,lock;
 RECT rect; int has_rect,locked; } Surface;
static Surface surfaces[128];static LONG surface_count;
static void *dd_object,*dd_next[23],*dd_table[23];
static void**dd_vt;
static void write_table(void**vt,void**table,unsigned bytes){DWORD old;
 if(VirtualProtect(vt,bytes,PAGE_READWRITE,&old)){memcpy(vt,table,bytes);VirtualProtect(vt,bytes,old,&old);}else probe_error(58,(uintptr_t)vt);}
static HRESULT (WINAPI *next_create)(GUID*,LPDIRECTDRAW*,IUnknown*);
static void **create_slot;
static unsigned char*create_callsite;
static Surface *find(void*obj){int i;for(i=0;i<surface_count&&i<128;i++)if(surfaces[i].object==obj)return &surfaces[i];return NULL;}
static int primary(Surface*s){return (s->desc.ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE)!=0;}
static void submission(Surface*s,unsigned reason){
 if(ls_active()&&primary(s)){probe_graphics_checkpoint();ls_commit(reason);}
}
static void snapshot(Surface*s,unsigned reason){
 DDSURFACEDESC d;uint64_t start;HRESULT hr;
 if(!probe_capture_enabled() || !primary(s) || s->locked)return;
 memset(&d,0,sizeof(d));d.dwSize=sizeof(d);start=probe_now();
 hr=((HRESULT(WINAPI*)(void*,RECT*,DDSURFACEDESC*,DWORD,HANDLE))s->next[25])(s->object,NULL,&d,DDLOCK_WAIT|DDLOCK_READONLY,NULL);
 if(hr==DD_OK){probe_frame(s->object,&d,NULL,reason,hr);
  hr=((HRESULT(WINAPI*)(void*,void*))s->next[32])(s->object,NULL);}
 if(hr!=DD_OK)probe_error(50,(uintptr_t)s->object);
 probe_event(EV_SURFACE,start,0,(uintptr_t)s->object,hr,99,reason,0,0);
}
static void copy_patch(Surface*dst,void*source,RECT*r,DWORD x,DWORD y,DWORD flags){
 Surface*src=find(source);DDSURFACEDESC d;RECT area;HRESULT hr;uint64_t t;
 if(!probe_capture_enabled() || !primary(dst))return;
 if(!src || !r || (flags&~DDBLTFAST_WAIT)){probe_error(59,(uintptr_t)source);return;}
 memset(&d,0,sizeof(d));d.dwSize=sizeof(d);t=probe_now();
 hr=((HRESULT(WINAPI*)(void*,RECT*,DDSURFACEDESC*,DWORD,HANDLE))src->next[25])(source,r,&d,DDLOCK_WAIT|DDLOCK_READONLY,NULL);
 if(hr==DD_OK){area.left=x;area.top=y;area.right=x+r->right-r->left;area.bottom=y+r->bottom-r->top;
  probe_frame(dst->object,&d,&area,EV_BLTFAST,hr);
  hr=((HRESULT(WINAPI*)(void*,void*))src->next[32])(source,d.lpSurface);}
 if(hr!=DD_OK)probe_error(60,(uintptr_t)source);
 probe_event(EV_SURFACE,t,0,(uintptr_t)source,hr,98,EV_BLTFAST,0,0);
}
static HRESULT WINAPI surf_query(void*o,REFIID iid,void**out){Surface*s=find(o);HRESULT hr;
 hr=((HRESULT(WINAPI*)(void*,REFIID,void**))s->next[0])(o,iid,out);
 if(hr==DD_OK && *out!=o)probe_error(51,(uintptr_t)*out);return hr;}
static ULONG WINAPI surf_release(void*o){Surface*s=find(o);ULONG n;
 n=((ULONG(WINAPI*)(void*))s->next[2])(o);
 probe_event(EV_SURFACE,0,CALLER,(uintptr_t)o,n,2,0,0,0);if(!n)s->object=NULL;return n;}
static HRESULT WINAPI surf_blt(void*o,RECT*d,void*src,RECT*r,DWORD flags,DDBLTFX*fx){Surface*s=find(o);uint64_t t=probe_now();HRESULT hr;
 hr=((HRESULT(WINAPI*)(void*,RECT*,void*,RECT*,DWORD,DDBLTFX*))s->next[5])(o,d,src,r,flags,fx);
 probe_event(EV_BLT,t,CALLER,(uintptr_t)o,hr,flags,(uint32_t)src,d?(uint32_t)d->left:0,d?(uint32_t)d->top:0);
 if(hr==DD_OK){snapshot(s,EV_BLT);submission(s,EV_BLT);}return hr;}
static HRESULT WINAPI surf_fast(void*o,DWORD x,DWORD y,void*src,RECT*r,DWORD flags){Surface*s=find(o);uint64_t t=probe_now();HRESULT hr;
 hr=((HRESULT(WINAPI*)(void*,DWORD,DWORD,void*,RECT*,DWORD))s->next[7])(o,x,y,src,r,flags);
 probe_event(EV_BLTFAST,t,CALLER,(uintptr_t)o,hr,flags,(uint32_t)src,x,y);
 if(hr==DD_OK){copy_patch(s,src,r,x,y,flags);submission(s,EV_BLTFAST);}return hr;}
static HRESULT WINAPI surf_flip(void*o,void*target,DWORD flags){Surface*s=find(o);uint64_t t=probe_now();HRESULT hr;
 hr=((HRESULT(WINAPI*)(void*,void*,DWORD))s->next[11])(o,target,flags);
 probe_event(EV_FLIP,t,CALLER,(uintptr_t)o,hr,flags,0,0,0);if(hr==DD_OK){snapshot(s,EV_FLIP);submission(s,EV_FLIP);}return hr;}
static HRESULT WINAPI surf_lock(void*o,RECT*r,DDSURFACEDESC*d,DWORD flags,HANDLE h){Surface*s=find(o);uint64_t t=probe_now();HRESULT hr;
 hr=((HRESULT(WINAPI*)(void*,RECT*,DDSURFACEDESC*,DWORD,HANDLE))s->next[25])(o,r,d,flags,h);
 if(hr==DD_OK){s->lock=*d;s->locked=1;s->has_rect=r!=NULL;if(r)s->rect=*r;}
 probe_event(EV_LOCK,t,CALLER,(uintptr_t)o,hr,flags,hr==DD_OK?d->lPitch:0,hr==DD_OK?d->dwWidth:0,hr==DD_OK?d->dwHeight:0);return hr;}
static HRESULT WINAPI surf_unlock(void*o,void*ptr){Surface*s=find(o);uint64_t t;HRESULT hr;
 /* This is a pre-unlock raw snapshot; validity is checked against the
    following Unlock result by the report, never labelled a full frame. */
 if(s->locked && primary(s))probe_frame(o,&s->lock,s->has_rect?&s->rect:NULL,EV_UNLOCK,DD_OK);
 t=probe_now();hr=((HRESULT(WINAPI*)(void*,void*))s->next[32])(o,ptr);
 if(hr==DD_OK)s->locked=0;
 if(hr==DD_OK)submission(s,EV_UNLOCK);
 probe_event(EV_UNLOCK,t,CALLER,(uintptr_t)o,hr,0,0,0,0);return hr;}
static HRESULT WINAPI surf_getdc(void*o,HDC*dc){Surface*s=find(o);uint64_t t=probe_now();HRESULT hr;
 hr=((HRESULT(WINAPI*)(void*,HDC*))s->next[17])(o,dc);probe_event(EV_GETDC,t,CALLER,(uintptr_t)o,hr,0,0,0,0);return hr;}
static HRESULT WINAPI surf_releasedc(void*o,HDC dc){Surface*s=find(o);uint64_t t=probe_now();HRESULT hr;
 hr=((HRESULT(WINAPI*)(void*,HDC))s->next[26])(o,dc);probe_event(EV_RELEASEDC,t,CALLER,(uintptr_t)o,hr,0,0,0,0);
 if(hr==DD_OK){snapshot(s,EV_RELEASEDC);submission(s,EV_RELEASEDC);}return hr;}
static HRESULT WINAPI surf_restore(void*o){Surface*s=find(o);uint64_t t=probe_now();HRESULT hr;
 hr=((HRESULT(WINAPI*)(void*))s->next[27])(o);s->locked=0;
 probe_event(EV_SURFACE,t,CALLER,(uintptr_t)o,hr,27,0,0,0);return hr;}
static void wrap_surface(void*o){LONG index,j;Surface*s;void**vt;
 if(find(o))return;index=InterlockedIncrement(&surface_count)-1;
 if(index>=128){probe_error(52,(uintptr_t)o);return;}s=&surfaces[index];vt=*(void***)o;
 s->vt=vt;memcpy(s->next,vt,sizeof(s->next));
 for(j=0;j<index;j++)if(surfaces[j].vt==vt){memcpy(s->next,surfaces[j].next,sizeof(s->next));break;}
 memcpy(s->table,s->next,sizeof(s->table));s->object=o;
 s->desc.dwSize=sizeof(s->desc);((HRESULT(WINAPI*)(void*,DDSURFACEDESC*))vt[22])(o,&s->desc);
 s->table[0]=surf_query;s->table[2]=surf_release;s->table[5]=surf_blt;s->table[7]=surf_fast;
 s->table[11]=surf_flip;s->table[17]=surf_getdc;s->table[25]=surf_lock;s->table[26]=surf_releasedc;
 s->table[27]=surf_restore;s->table[32]=surf_unlock;write_table(vt,s->table,sizeof(s->table));
 probe_event(EV_SURFACE,0,0,(uintptr_t)o,0,s->desc.dwWidth,s->desc.dwHeight,s->desc.ddpfPixelFormat.dwRGBBitCount,s->desc.ddsCaps.dwCaps);
}
static HRESULT WINAPI dd_surface(void*o,DDSURFACEDESC*d,void**out,IUnknown*outer){HRESULT hr;uint64_t t=probe_now();
 hr=((HRESULT(WINAPI*)(void*,DDSURFACEDESC*,void**,IUnknown*))dd_next[6])(o,d,out,outer);
 probe_event(EV_DDRAW,t,CALLER,(uintptr_t)o,hr,6,0,0,0);if(hr==DD_OK)wrap_surface(*out);return hr;}
static HRESULT WINAPI dd_vblank(void*o,DWORD flags,HANDLE h){HRESULT hr;uint64_t t=probe_now();
 hr=((HRESULT(WINAPI*)(void*,DWORD,HANDLE))dd_next[22])(o,flags,h);
 probe_event(EV_VBLANK,t,CALLER,(uintptr_t)o,hr,flags,0,0,0);return hr;}
static HRESULT WINAPI dd_query(void*o,REFIID iid,void**out){HRESULT hr;
 hr=((HRESULT(WINAPI*)(void*,REFIID,void**))dd_next[0])(o,iid,out);
 if(hr==DD_OK && *out!=o)probe_error(53,(uintptr_t)*out);return hr;}
static HRESULT WINAPI create(GUID*g,LPDIRECTDRAW*out,IUnknown*outer){HRESULT hr;uint64_t t=probe_now();
 next_create=(void*)*create_slot;
 hr=next_create(g,out,outer);
 probe_event(EV_DDRAW,t,CALLER,hr==DD_OK?(uintptr_t)*out:0,hr,0,0,0,0);
 if(hr==DD_OK){if(dd_object){probe_error(54,(uintptr_t)*out);return hr;}dd_object=*out;
  dd_vt=*(void***)*out;memcpy(dd_next,dd_vt,sizeof(dd_next));memcpy(dd_table,dd_next,sizeof(dd_table));
  dd_table[0]=dd_query;dd_table[6]=dd_surface;dd_table[22]=dd_vblank;write_table(dd_vt,dd_table,sizeof(dd_table));}
 return hr;}
int probe_graphics_install(void**slot){DWORD old;int32_t relative;create_slot=slot;next_create=(void*)*slot;
 /* 0x4015F8 is the sole call to the original 0x461222 import thunk.
    Patch this game call, leaving DxWnd's import bookkeeping intact. */
 create_callsite=(unsigned char*)GetModuleHandle(NULL)+0x15f8;
 if(memcmp(create_callsite,"\xe8\x25\xfc\x05\x00",5)){probe_error(57,(uintptr_t)create_callsite);return 0;}
 if(!VirtualProtect(create_callsite,5,PAGE_EXECUTE_READWRITE,&old))return 0;
 relative=(int32_t)((unsigned char*)create-(create_callsite+5));memcpy(create_callsite+1,&relative,4);
 VirtualProtect(create_callsite,5,old,&old);FlushInstructionCache(GetCurrentProcess(),create_callsite,5);
 probe_event(EV_HOOK,0,0,(uintptr_t)next_create,0,(uint32_t)slot,6,0,0);return 1;}
void probe_graphics_audit(void){int i;int32_t rel;memcpy(&rel,create_callsite+1,4);
 if(create_callsite+5+rel!=(unsigned char*)create)probe_error(55,(uintptr_t)create_callsite);
 for(i=0;i<surface_count&&i<128;i++)if(surfaces[i].object && (surfaces[i].vt[25]!=surf_lock || surfaces[i].vt[32]!=surf_unlock))probe_error(56,(uintptr_t)surfaces[i].object);
}
void probe_graphics_checkpoint(void){
 int i;Surface*s=NULL;DDSURFACEDESC d;HRESULT hr;
 for(i=0;i<surface_count&&i<128;i++)if(surfaces[i].object && primary(&surfaces[i])){s=&surfaces[i];break;}
 if(!s || s->locked)ls_fail(20,0);
 memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
 hr=((HRESULT(WINAPI*)(void*,RECT*,DDSURFACEDESC*,DWORD,HANDLE))s->next[25])(s->object,NULL,&d,DDLOCK_WAIT|DDLOCK_READONLY,NULL);
 if(hr!=DD_OK)ls_fail(21,(unsigned)hr);
 ls_pixels(d.lpSurface,d.lPitch,d.dwWidth,d.dwHeight,d.ddpfPixelFormat.dwRGBBitCount,
           d.ddpfPixelFormat.dwRBitMask,d.ddpfPixelFormat.dwGBitMask,d.ddpfPixelFormat.dwBBitMask);
 hr=((HRESULT(WINAPI*)(void*,void*))s->next[32])(s->object,NULL);
 if(hr!=DD_OK)ls_fail(22,(unsigned)hr);
}
