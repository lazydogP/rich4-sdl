"""Bounded two-process virtual-time driver. Windows, protocol v1."""
from __future__ import annotations
import argparse
import ctypes
import csv
from ctypes import wintypes as W
import hashlib
import html
import json
import mmap
import os
from pathlib import Path
import shutil
import struct
import subprocess
import time
import uuid

from r4diff import ROOT, EXE_SHA, DXWND_SHA, sha, profile_text, png_rgb

MAGIC=0x32443452
TRACE_OFFSET=256
PIXEL_OFFSET=256+4096*32
PIXEL_BYTES=640*480*2
FRAME_OFFSET=PIXEL_OFFSET+PIXEL_BYTES
FRAME_STRIDE=32+PIXEL_BYTES
SIZE=FRAME_OFFSET+32*FRAME_STRIDE
HEADER=struct.Struct('<64I')

def api():
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    for name,args,result in [
        ('CreateEventW',[ctypes.c_void_p,W.BOOL,W.BOOL,W.LPCWSTR],W.HANDLE),
        ('SetEvent',[W.HANDLE],W.BOOL),('WaitForSingleObject',[W.HANDLE,W.DWORD],W.DWORD),
        ('CloseHandle',[W.HANDLE],W.BOOL)]:
        fn=getattr(k,name);fn.argtypes=args;fn.restype=result
    return k

class Endpoint:
    def __init__(self,role,origin,date):
        self.role=role;self.k=api();self.name='Local\\r4diff-'+uuid.uuid4().hex
        self.memory=mmap.mmap(-1,SIZE,tagname=self.name)
        self.ready=self.k.CreateEventW(None,False,False,self.name+'.ready')
        self.resume=self.k.CreateEventW(None,False,False,self.name+'.resume')
        if not self.ready or not self.resume: raise ctypes.WinError(ctypes.get_last_error())
        words=[0]*64;words[0]=MAGIC;words[1]=1;words[6]=origin;words[12:15]=date
        self.memory[:256]=HEADER.pack(*words)

    def receive(self,process=None,timeout=15000):
        deadline=time.monotonic()+timeout/1000
        while self.k.WaitForSingleObject(self.ready,100)!=0:
            if process is not None and process.poll() is not None: raise ChildProcessError(f'{self.role} exited: {process.returncode}')
            if time.monotonic()>=deadline: raise TimeoutError(f'{self.role}: no safe point within {timeout}ms')
        return self.snapshot()

    def snapshot(self):
        h=HEADER.unpack(self.memory[:256])
        if h[0]!=MAGIC or h[1]!=1:raise ValueError('invalid protocol magic/version')
        if h[39]>4096 or h[46]>32 or h[36] not in (0,PIXEL_BYTES): raise ValueError('invalid shared buffer length')
        traces=[list(struct.unpack_from('<8I',self.memory,TRACE_OFFSET+i*32)) for i in range(h[39])]
        pixels=bytes(self.memory[PIXEL_OFFSET:PIXEL_OFFSET+h[36]])
        submissions=[]
        for i in range(h[46]):
            p=FRAME_OFFSET+i*FRAME_STRIDE;f=struct.unpack_from('<8I',self.memory,p)
            if f[3]!=PIXEL_BYTES or f[7]!=i or (f[0],f[1])!=(h[32],h[33]):raise ValueError('invalid submission header')
            data=bytes(self.memory[p+32:p+32+PIXEL_BYTES])
            submissions.append(dict(epoch=f[0],ms=f[1],source=f[2],masks=list(f[4:7]),seq=f[7],
                sha256=hashlib.sha256(data).hexdigest(),pixels=data))
        return dict(epoch=h[32],ms=h[33],scene=h[34],error=h[35],present_count=h[37],ack=h[38],
            next_ms=h[40],site=h[41],masks=list(h[42:45]),role=h[45],traces=traces,
            sha256=hashlib.sha256(pixels).hexdigest(),pixels=pixels,submissions=submissions)

    def step(self,epoch,ms,event=None):
        h=list(HEADER.unpack(self.memory[:256]));h[2]=1;h[3]=epoch;h[4]=ms
        h[7:12]=[event['id'],event['kind'],event.get('x',0),event.get('y',0),event.get('key',0)] if event else [0]*5
        self.memory[:128]=struct.pack('<32I',*h[:32]);self.k.SetEvent(self.resume)

    def stop(self):
        struct.pack_into('<I',self.memory,8,2);self.k.SetEvent(self.resume)

    def close(self):
        self.memory.close();self.k.CloseHandle(self.ready);self.k.CloseHandle(self.resume)

def rgb(pixels,masks):
    import numpy as np
    p=np.frombuffer(pixels,dtype='<u2').reshape(480,640)
    channels=[]
    for mask in masks:
        shift=(mask&-mask).bit_length()-1;bits=(mask>>shift).bit_length()
        if not mask or bits>8 or mask>>shift!=(1<<bits)-1: raise ValueError('unsupported RGB mask')
        channels.append(((p&mask)>>shift)<<(8-bits))
    return np.stack(channels,axis=-1).astype('uint8')

def pixel_diff(a,b):
    import numpy as np
    if len(a['pixels'])!=PIXEL_BYTES or len(b['pixels'])!=PIXEL_BYTES: return dict(status='INVALID_CAPTURE')
    if a['masks']!=b['masks']: return dict(status='UNSUPPORTED',reason='pixel_format')
    x=np.frombuffer(a['pixels'],dtype='<u2').reshape(480,640)
    y=np.frombuffer(b['pixels'],dtype='<u2').reshape(480,640)
    changed=x!=y;count=int(changed.sum());box=None
    if count:
        ys,xs=np.where(changed);box=[int(xs.min()),int(ys.min()),int(xs.max())+1,int(ys.max())+1]
    return dict(status='DIFF_PIXELS' if count else 'PASS',changed=count,total=640*480,bbox=box)

def trace_rng(frame):
    # Initial reseed replaces an unspecified runtime default; subsequent state is checked.
    return [[t[0],t[2],0 if t[0]==1 and t[1]==0 else t[4],t[5],t[6],t[7]] for t in frame['traces'] if t[0] in (1,2)]

def validate_frame(frame,epoch,ms,role):
    if (frame['epoch'],frame['ms'],frame['role'])!=(epoch,ms,role):
        return 'response epoch/time/role differs from command'
    for t in frame['traces']:
        if (t[1],t[2])!=(epoch,ms):return 'trace epoch/time differs from response'
        if t[0]==1 and t[5]!=t[6]:return 'seed state differs from seed argument'
        if t[0]==2 and (t[5]!=(t[4]*1103515245+12345)&0xffffffff or t[6]!=(t[5]>>16)&32767):
            return 'invalid original LCG transition/result'
    return None

def save_frame(folder,epoch,frame,persist_raw=True):
    result={k:v for k,v in frame.items() if k not in ('pixels','submissions')}
    result['submissions']=[]
    if not persist_raw:
        return result
    (folder/f'{epoch:05d}.rgb565').write_bytes(frame['pixels'])
    for sub in frame['submissions']:
        name=f'{epoch:05d}-submit-{sub["seq"]:02d}.rgb565'
        (folder/name).write_bytes(sub['pixels'])
        result['submissions'].append({**{k:v for k,v in sub.items() if k!='pixels'},'file':name})
    return result

def compare_frame(a,b,event,epoch=None,ms=None):
    if a['error'] or b['error']:
        errors=[f['error'] for f in (a,b) if f['error']]
        return dict(status='INVALID_CAPTURE' if any(e>=70 or e in (1,2,5,20,21,22,30) for e in errors) else 'UNSUPPORTED',reason='adapter_error',codes=errors)
    if epoch is not None:
        for role,f in enumerate((a,b),1):
            error=validate_frame(f,epoch,ms,role)
            if error:return dict(status='INVALID_CAPTURE',reason=error,role=role)
    if (a['epoch'],a['ms'])!=(b['epoch'],b['ms']): return dict(status='DIFF_SCHEDULE')
    expected=event['id'] if event else 0
    if a['ack']!=expected or b['ack']!=expected: return dict(status='DIFF_INPUT')
    if trace_rng(a)!=trace_rng(b): return dict(status='DIFF_RNG')
    if a['scene']!=b['scene']: return dict(status='DIFF_STATE',reason='scene')
    return pixel_diff(a,b)

def validate_events(events):
    previous=-1
    for i,event in enumerate(events,1):
        if not isinstance(event.get('ms'),int) or not previous<event['ms']<=300000:
            raise ValueError('input timestamps must strictly increase within 0..300000 ms')
        if event['ms']==0:raise ValueError('epoch zero is the bootstrap barrier; inputs start after 0 ms')
        if event.get('kind') not in (1,2,3,4,5):raise ValueError('unsupported input kind')
        if event['kind']<=3 and not (0<=event.get('x',-1)<640 and 0<=event.get('y',-1)<480):
            raise ValueError('pointer outside logical 640x480 canvas')
        if event['kind']>=4 and event.get('key') not in (*range(48,58),*range(65,91),37,38,39,40,8,9,13,27,32):
            raise ValueError('unsupported virtual key')
        event['id']=i;previous=event['ms']
    return events

def audit_original_log(path):
    with path.open(newline='') as stream:rows=list(csv.DictReader(stream))
    kinds=[int(r['type']) for r in rows]
    if kinds.count(24)!=15 or kinds.count(1)!=2:return 'incomplete original hook installation'
    if not any(int(r['type'])==1 and int(r['result'])==1 for r in rows):return 'original hooks not ready'
    if kinds.count(21)!=1 or kinds[-1]!=21:return 'missing original END marker'
    if int(rows[-1]['a']):return 'original event queue overflow'
    if any(int(r['type'])==20 or (6<=int(r['type'])<=15 and int(r['result'])<0) for r in rows):
        return 'original hook/graphics error'
    return None

def write_images(out,epoch,a,b):
    import numpy as np
    colors=[]
    for role,f in [('original',a),('sdl',b)]:
        if len(f['pixels'])!=PIXEL_BYTES: return
        c=rgb(f['pixels'],f['masks']);colors.append(c)
        (out/f'{epoch:05d}-{role}.png').write_bytes(png_rgb(640,480,[r.tobytes() for r in c]))
    heat=np.zeros_like(colors[0]);heat[np.any(colors[0]!=colors[1],axis=-1)]=[255,32,64]
    (out/f'{epoch:05d}-diff.png').write_bytes(png_rgb(640,480,[r.tobytes() for r in heat]))

def write_report(out,result):
    frames=[r for r in result['frames'] if (out/f'{r["epoch"]:05d}-diff.png').exists()]
    options=''.join(f'<option value="{r["epoch"]:05d}">{r["epoch"]} / {r["ms"]} ms / {r["comparison"]["status"]}</option>' for r in frames)
    cards=''.join(f'<section><h2>epoch {r["epoch"]} / {r["ms"]} ms / {r["comparison"]["status"]}</h2>'+
        ''.join(f'<img loading="lazy" width="640" src="{r["epoch"]:05d}-{name}.png">' for name in ('original','sdl','diff'))+'</section>' for r in frames)
    summary={k:result[k] for k in ('status','failure','first_difference','first_structural_difference','exit_codes','scope')}
    viewer='''<p><label>Frame <select id="frame">OPTIONS</select></label>
<label>SDL opacity <input id="opacity" type="range" min="0" max="100" value="50"></label></p>
<div id="overlay"><img id="original"><img id="sdl"></div>
<script>
const f=document.getElementById('frame'),o=document.getElementById('opacity');
function show(){document.getElementById('original').src=f.value+'-original.png';document.getElementById('sdl').src=f.value+'-sdl.png';}
f.onchange=show;o.oninput=()=>document.getElementById('sdl').style.opacity=o.value/100;if(f.value)show();
</script>'''.replace('OPTIONS',options)
    (out/'report.html').write_text('<!doctype html><meta charset="utf-8"><title>Rich4 lockstep comparison</title><style>body{font:15px system-ui;background:#17202b;color:#eee;padding:16px}section img{max-width:32%;height:auto}section{margin:25px 0}pre{white-space:pre-wrap}#overlay{position:relative;width:640px;height:480px}#overlay img{position:absolute;width:640px;height:480px}#sdl{opacity:.5}label{margin-right:24px}</style>'+
        f'<h1>Original / SDL lockstep — {result["status"]}</h1><pre>{html.escape(json.dumps(summary,indent=2))}</pre>'+
        '<p>Original / SDL / changed pixels. Exact RGB565 differences; same-time submissions are retained in raw files and result.json. No automatic resynchronization.</p>'+viewer+cards,encoding='utf-8')

def scenario(name):
    events=[dict(ms=100,kind=1,x=100,y=300),dict(ms=200,kind=1,x=400,y=300)]
    if name in ('setup','options','newgame'):
        x,y=(190,378) if name in ('setup','newgame') else (468,378)
        events += [dict(ms=300,kind=1,x=x,y=y),dict(ms=320,kind=2,x=x,y=y),dict(ms=340,kind=3,x=x,y=y)]
    if name=='newgame':
        events += [dict(ms=500,kind=1,x=38,y=45),dict(ms=520,kind=2,x=38,y=45),dict(ms=540,kind=3,x=38,y=45),
            dict(ms=600,kind=1,x=495,y=195),dict(ms=620,kind=2,x=495,y=195),dict(ms=640,kind=3,x=495,y=195)]
    return validate_events(events)

def stage(args,out):
    source=Path(args.data_dir).resolve();dx=Path(args.dxwnd_dir).resolve()
    if sha(source/'Rich4.exe')!=EXE_SHA or sha(dx/'dxwnd.dll')!=DXWND_SHA: raise ValueError('unverified original/DxWnd binary')
    if '"dxwnd.exe"' in subprocess.check_output(['tasklist','/FI','IMAGENAME eq dxwnd.exe','/FO','CSV'],text=True).lower():
        raise ValueError('an existing DxWnd instance is using the global target mapping')
    out.mkdir(parents=True,exist_ok=False)
    game=out/'original'/'game';runtime=out/'dxwnd';sdl=out/'sdl'
    game.mkdir(parents=True);runtime.mkdir();sdl.mkdir()
    # Explicit shared no-video/no-audio fixture. No executable or game state patch.
    for p in source.iterdir():
        if p.is_file() and (p.suffix.lower() in ('.exe','.mkf','.mid','.wav','.ico','.txt') or p.name.lower()=='over.avi'): shutil.copy2(p,game/p.name)
    config=bytearray((ROOT/'RICH4.CFG').read_bytes());config[2:4]=b'\0\0'
    for folder in (game,sdl): (folder/'RICH4.CFG').write_bytes(config)
    # Original 0x4502FE opens MKFs with share mode zero. Independent files are
    # required even for two readers; hard links would still conflict.
    for p in game.iterdir():
        if p.suffix.lower()=='.mkf':shutil.copy2(p,sdl/p.name)
    for p in dx.iterdir():
        if p.is_file() and p.suffix.lower() in ('.exe','.dll','.ini'):shutil.copy2(p,runtime/p.name)
    profile=profile_text(dx/'dxwnd.ini',game/'Rich4.exe',{'renderer0':'5'})
    (runtime/'dxwnd.ini').write_bytes(profile.encode('mbcs'))
    return game,runtime,sdl,dict(exe_sha256=EXE_SHA,dxwnd_sha256=DXWND_SHA,
        profile=profile,probe_sha256=sha(ROOT/'bin/frame_diff/original_probe.dll'),
        sdl_sha256=sha(ROOT/'.local/frame-diff-bin/rich4_sdl.exe'),
        sdl_inputs={str(p.relative_to(ROOT)):sha(p) for p in
            [ROOT/'assets/fonts/rich4.ttc',ROOT/'config/input.toml',*sorted((ROOT/'.local/frame-diff-bin').glob('*.dll'))]},
        resources={p.name:sha(p) for p in game.iterdir() if p.is_file()},
        fixture='Startup AVI omitted; OVER.AVI retained for original disc check; music/sound_effect config bytes zero',source_profile_sha256=sha(dx/'dxwnd.ini'))

def run(args):
    if os.name!='nt':raise SystemExit('Windows required')
    events=validate_events(json.loads(Path(args.script).read_text(encoding='utf-8'))) if args.script else scenario(args.scenario)
    out=Path(args.out).resolve();game,runtime,sdl,manifest=stage(args,out)
    manifest.update(scenario=args.scenario,events=events,origin=args.origin,
        date=[2000,1,1],duration_ms=args.duration_ms,delay_ms=args.delay_ms,protocol=1)
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    endpoints=[Endpoint(role,args.origin,[2000,1,1]) for role in ('original','sdl')]
    processes=[];logs=[];records=[];status='PASS';failure=None;server=None
    try:
        startup=subprocess.STARTUPINFO();startup.dwFlags=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        server=subprocess.Popen([str(runtime/'dxwnd.exe'),'/T','/Q'],cwd=runtime,startupinfo=startup)
        time.sleep(1)
        for endpoint in endpoints:
            env=os.environ.copy();env.pop('R4DIFF_SCENARIO',None);env['R4DIFF_LINK']=endpoint.name
            if args.debug_original and endpoint.role=='original':env['R4DIFF_DEBUG']='1'
            folder=out/endpoint.role;log=(folder/'process.log').open('w');logs.append(log)
            if endpoint.role=='original':
                command=[str(ROOT/'bin/frame_diff/original_launcher.exe'),str(game/'Rich4.exe'),
                    str(runtime/'dxwnd.dll'),str(ROOT/'bin/frame_diff/original_probe.dll'),str(folder),'300','0','0']
            else:
                command=[str(ROOT/'.local/frame-diff-bin/rich4_sdl.exe'),'--data-dir',str(sdl),
                    '--font',str(ROOT/'assets/fonts/rich4.ttc'),'--input-config',str(ROOT/'config/input.toml')]
            processes.append(subprocess.Popen(command,cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT))
        epoch=0;now=0;event=None
        while True:
            frames=[e.receive(p) for e,p in zip(endpoints,processes)]
            comparison=compare_frame(*frames,event,epoch,now)
            persist_raw = (epoch == 0 or event is not None or
                           now == args.duration_ms or epoch % 50 == 0)
            record=dict(epoch=epoch,ms=now,comparison=comparison,
                **{role:save_frame(out/role,epoch,f,persist_raw) for role,f in zip(('original','sdl'),frames)})
            records.append(record)
            if comparison['status']!='PASS' and status=='PASS': status=comparison['status']
            # Keep every protocol epoch in result.json, but sample the PNG
            # gallery for long diagnostic runs.  Always retain input epochs,
            # the terminal frame, and ordinary 200 ms checkpoints; this keeps
            # the HTML report navigable without changing comparison coverage.
            if (epoch == 0 or event is not None or now == args.duration_ms or
                    epoch % 10 == 0):
                write_images(out,epoch,*frames)
            if comparison['status'] not in ('PASS','DIFF_PIXELS'):
                if status in ('PASS','DIFF_PIXELS') or comparison['status'] in ('UNSUPPORTED','INVALID_CAPTURE'):
                    status=comparison['status']
                if comparison['status'] in ('UNSUPPORTED','INVALID_CAPTURE') or not args.continue_after_diff:break
            if now>=args.duration_ms:break
            candidates=[f['next_ms'] for f in frames]+[e['ms'] for e in events if e['ms']>now]+[args.duration_ms]
            next_ms=min(candidates)
            if next_ms<=now: raise ValueError('adapter supplied non-future deadline')
            now=next_ms;epoch+=1;event=next((e for e in events if e['ms']==now),None)
            endpoints[0].step(epoch,now,event)
            if args.delay_ms:time.sleep(args.delay_ms/1000)
            endpoints[1].step(epoch,now,event)
    except Exception as exc:
        status='HANG' if isinstance(exc,TimeoutError) else 'CRASH' if isinstance(exc,ChildProcessError) else 'INVALID_CAPTURE';failure=str(exc)
        for e in endpoints:
            try:
                snapshot=save_frame(out/e.role,99999,e.snapshot())
                (out/e.role/'last-snapshot.json').write_text(json.dumps(snapshot,indent=2),encoding='utf-8')
            except Exception as snapshot_error:
                (out/e.role/'snapshot-error.txt').write_text(str(snapshot_error),encoding='utf-8')
    finally:
        for e in endpoints:e.stop()
        for process in processes:
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.kill();process.wait()
        if server is not None:
            server.terminate()
            try:server.wait(timeout=5)
            except subprocess.TimeoutExpired:server.kill();server.wait()
        for log in logs:log.close()
        for e in endpoints:e.close()
    if status in ('PASS','DIFF_PIXELS','DIFF_INPUT','DIFF_RNG','DIFF_STATE','DIFF_SCHEDULE') and any(p.returncode for p in processes):
        status='CRASH';failure=f'nonzero process exit: {[p.returncode for p in processes]}'
    if status in ('PASS','DIFF_PIXELS','DIFF_INPUT','DIFF_RNG','DIFF_STATE','DIFF_SCHEDULE'):
        try:failure=audit_original_log(out/'original/events.csv')
        except Exception as exc:failure=f'original event log: {exc}'
        if failure:status='INVALID_CAPTURE'
    differences=[dict(epoch=r['epoch'],ms=r['ms'],**r['comparison']) for r in records if r['comparison']['status']!='PASS']
    structural=[r for r in differences if r['status']!='DIFF_PIXELS']
    result=dict(status=status,failure=failure,diagnostic_continue=args.continue_after_diff,
        first_difference=next(iter(differences),None),first_structural_difference=next(iter(structural),None),
        frames=records,exit_codes=[p.returncode for p in processes],
        scope='protocol epochs at verified message-pump safe points; no arbitrary-thread equivalence',strict_equivalence=False)
    (out/'result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    write_report(out,result)
    print(json.dumps(dict(status=status,failure=failure,epochs=len(records),last_ms=records[-1]['ms'] if records else None)))
    return 0 if status=='PASS' else 2

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--data-dir',required=True);p.add_argument('--dxwnd-dir',required=True);p.add_argument('--out',required=True)
    p.add_argument('--scenario',choices=('menu','setup','options','newgame'),default='menu')
    p.add_argument('--script',help='JSON input array: ms, kind (1..5), x/y or Windows virtual key; overrides canned scenario')
    p.add_argument('--duration-ms',type=int,default=1200);p.add_argument('--delay-ms',type=int,choices=(0,50,200),default=0)
    p.add_argument('--origin',type=lambda s:int(s,0),default=100000)
    p.add_argument('--debug-original',action='store_true')
    p.add_argument('--continue-after-diff',action='store_true',help='diagnostic continuation; never restores strict pass after divergence')
    args=p.parse_args()
    if not 0<args.duration_ms<=300000 or not 0<=args.origin<=0xffffffff:p.error('invalid duration/origin')
    raise SystemExit(run(args))

if __name__=='__main__':main()
