"""Phase-one original-game observation and DxWnd runner. No lockstep yet."""
from __future__ import annotations
import argparse
import configparser
import csv
import ctypes
import hashlib
import html
import json
import math
import os
import platform
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import zlib
from collections import Counter

ROOT = Path(__file__).resolve().parents[2]
EXE_SHA = '5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550'
DXWND_SHA = 'b0dfd9b439595c2079b303fb15e7393b2ec22a00dfea7b24d3a4f86418b229e1'
NAMES = ['unused', 'boot', 'clock', 'timer_set', 'timer_kill', 'timer', 'ddraw',
         'surface', 'blt', 'bltfast', 'flip', 'lock', 'unlock', 'getdc', 'releasedc',
         'vblank', 'rng', 'seed', 'wait', 'frame', 'error', 'end', 'peek', 'input', 'hook']

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()

def profile_text(source, game, overrides):
    cp = configparser.ConfigParser(interpolation=None, strict=False)
    cp.read_string(Path(source).read_bytes().decode('mbcs' if os.name == 'nt' else 'latin1', errors='replace'))
    target = {k: v for k, v in cp['target'].items() if re.fullmatch(r'[a-z_]+0', k)}
    target.update(path0=str(game), title0='Rich4 observation', startfolder0=str(game.parent),
                  launchpath0='', cmdline0='', icon0='')
    for key,value in overrides.items():
        if key not in target or not target[key].isdigit() or not value.isdigit() or not 0 <= int(value) <= 0xffffffff:
            raise ValueError(f'Override must be an existing numeric target-0 field: {key}')
    target.update(overrides)
    # Required even when overriding flags: initialize DxWnd before EXE entry.
    target['flagl0'] = str(int(target.get('flagl0', '0')) | 0x8000)
    return '[window]\nshowhelp=NO\nexpert=1\n[target]\n' + ''.join(f'{k}={v}\n' for k,v in target.items())

def png_rgb(width, height, rows):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind+data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB',width,height,8,2,0,0,0))
            + chunk(b'IDAT', zlib.compress(b''.join(b'\0'+row for row in rows))) + chunk(b'IEND',b''))

def decode_frame(path):
    data=path.read_bytes()
    if len(data)<64: raise ValueError(f'truncated frame: {path.name}')
    h=struct.unpack('<16I',data[:64]); w,ht,bpp,pitch=h[4:8]
    if h[0]!=0x31463452 or bpp!=16 or not 0<w<=1024 or not 0<ht<=768 or pitch!=w*2 or len(data)!=64+pitch*ht:
        raise ValueError(f'invalid frame: {path.name}')
    masks=h[8:11]
    if any(not m or m>0xffff for m in masks) or masks[0]&masks[1] or masks[0]&masks[2] or masks[1]&masks[2]:
        raise ValueError('unsupported masks')
    channels=[]
    for mask in masks:
        shift=(mask & -mask).bit_length()-1
        bits=(mask>>shift).bit_length()
        if bits>8 or (mask>>shift)!=(1<<bits)-1: raise ValueError('unsupported channel mask')
        channels.append((mask,shift,8-bits))
    try:
        import numpy as np
        pixels=np.frombuffer(data,dtype='<u2',offset=64).reshape(ht,w)
        rgb=np.stack([((pixels&mask)>>shift)<<pad for mask,shift,pad in channels],axis=-1).astype('uint8')
        rows=[row.tobytes() for row in rgb]
    except ImportError:
        rows=[]
        lut=[bytes(((p&mask)>>shift)<<pad for mask,shift,pad in channels) for p in range(65536)]
        for y in range(ht):
            rows.append(b''.join(lut[p] for (p,) in struct.iter_unpack('<H',data[64+y*pitch:64+(y+1)*pitch])))
    return h,png_rgb(w,ht,rows),hashlib.sha256(data[64:]).hexdigest()

def validate_rng(rows):
    """Check the actual Watcom return/state transition, including reseeds."""
    previous={}; errors=[]; count=0
    for r in sorted(rows,key=lambda r:r['us']):
        if r['type'] not in (16,17): continue
        if r['thread'] in previous and previous[r['thread']]!=r['a']:
            errors.append(dict(reason='rng_state_discontinuity',event=r))
        if r['type']==16:
            count+=1
            if r['b']!=((r['a']*1103515245+12345)&0xffffffff) or r['result']!=((r['b']>>16)&0x7fff):
                errors.append(dict(reason='rng_transition',event=r))
        elif r['b']!=r['c']:
            errors.append(dict(reason='seed_transition',event=r))
        previous[r['thread']]=r['b']
    return dict(calls=count,status='VALID' if count and not errors else 'INVALID' if errors else 'NOT_EXERCISED',errors=errors)

def measured(row):
    return row['type'] in {3,4,5,6,8,9,10,11,12,13,14,15,16,17,18,19,22} or (row['type']==7 and row['a'] in (98,99))

def distribution(values):
    values=sorted(values); n=len(values)
    return dict(count=n,total_ms=sum(values)/1000,mean_ms=sum(values)/n/1000,
                p95_ms=values[max(0,math.ceil(n*.95)-1)]/1000,max_ms=values[-1]/1000)

def report(out):
    out=Path(out); stats={}; errors=[]; frames=[]; ends=[]; counts={}; seed=[]; rows=[]
    manifest=json.loads((out/'manifest.json').read_text(encoding='utf-8')) if (out/'manifest.json').exists() else {}
    with (out/'events.csv').open(newline='') as f:
        for r in csv.DictReader(f):
            row={k:int(v,16 if k in ('caller','object') else 10) for k,v in r.items()}
            rows.append(row); typ=NAMES[row['type']] if row['type']<len(NAMES) else str(row['type'])
            counts[typ]=counts.get(typ,0)+1
            if typ=='error': errors.append(row)
            if typ=='end': ends.append(row)
            if typ=='seed': seed.append(row)
            if not 0<row['type']<len(NAMES): errors.append(dict(reason='unknown_event',event=row))
            if measured(row):
                key=f'{typ}@{row["caller"]:x}/thread:{row["thread"]}'
                stats.setdefault(key,[]).append(row['duration_us'])
    canvases={}
    for path in sorted(out.glob('frame-*.r4f')):
        try: h,png,digest=decode_frame(path)
        except ValueError as e:
            errors.append(dict(reason=str(e),file=path.name));continue
        raw=path.read_bytes()[64:]
        key=h[11]
        if h[4:6]==(640,480) and h[13:15]==(0,0): canvases[key]=bytearray(raw)
        elif key in canvases:
            if h[13]+h[4]>640 or h[14]+h[5]>480:
                errors.append(dict(reason='patch_outside_canvas',frame=h[1]));continue
            for y in range(h[5]):
                start=((h[14]+y)*640+h[13])*2
                canvases[key][start:start+h[4]*2]=raw[y*h[7]:(y+1)*h[7]]
            # Reuse the strict decoder for the reconstructed RGB565 canvas.
            full=list(h);full[4:8]=[640,480,16,1280];full[13:15]=[0,0]
            temp=out/'reconstructed.r4f.tmp';temp.write_bytes(struct.pack('<16I',*full)+canvases[key])
            _,png,_=decode_frame(temp);temp.unlink()
        else: errors.append(dict(reason='patch_without_initial_canvas',frame=h[1]))
        dest=path.with_suffix('.png'); dest.write_bytes(png)
        frames.append(dict(file=dest.name,seq=h[1],us=h[2]+(h[3]<<32),width=h[4],height=h[5],
                           object=f'{h[11]:x}',reason=NAMES[h[12]] if h[12]<len(NAMES) else str(h[12]),
                           sha256=digest,preview_sha256=hashlib.sha256(png).hexdigest()))
    if Counter(f['seq'] for f in frames)!=Counter(r['a'] for r in rows if r['type']==19):
        errors.append(dict(reason='frame_event_file_mismatch'))
    if len(ends)!=1: errors.append(dict(reason='missing_or_duplicate_end'))
    if counts.get('boot')!=2 or not any(r['type']==1 and r['result']==1 for r in rows) or counts.get('hook')!=8:
        errors.append(dict(reason='incomplete_hook_bootstrap'))
    if not manifest: errors.append(dict(reason='missing_manifest'))
    if manifest.get('launcher_exit',0): errors.append(dict(reason='launcher_failed',exit=manifest['launcher_exit']))
    rng=validate_rng(rows);errors.extend(rng['errors'])
    spans=[]
    for name,values in stats.items():
        spans.append(dict(name=name,**distribution(values)))
    spans.sort(key=lambda x:x['total_ms'],reverse=True)
    # Only containment within the same thread; no summing across async threads.
    timed=sorted((r for r in rows if r['duration_us']),key=lambda r:(r['thread'],r['us'],-r['duration_us']))
    exclusive={}; stacks={}
    for r in timed:
        stack=stacks.setdefault(r['thread'],[]); end=r['us']+r['duration_us']
        while stack and (r['us']>=stack[-1][0] or end>stack[-1][0]): stack.pop()
        typ=NAMES[r['type']] if 0<=r['type']<len(NAMES) else str(r['type'])
        key=f'{typ}@{r["caller"]:x}/thread:{r["thread"]}'
        exclusive[key]=exclusive.get(key,0)+r['duration_us']
        if stack: exclusive[stack[-1][1]]-=r['duration_us']
        stack.append((end,key))
    for s in spans: s['exclusive_ms']=exclusive.get(s['name'],0)/1000
    invalid=bool(errors or not ends or any(r['a'] for r in ends))
    # Nonzero DirectDraw HRESULTs, including failed Unlock, invalidate capture.
    gfx_errors=[r for r in rows if r['type'] in range(6,16) and r['result']<0]
    invalid |= bool(gfx_errors)
    summary=dict(status='INVALID_CAPTURE' if invalid else 'OBSERVED',strict_equivalence=False,
                 counts=counts,errors=errors,graphics_errors=gfx_errors,seed_events=seed,spans=spans,
                 rng=rng,capture_mode=manifest.get('capture'),
                 preview_complete=bool(frames) and not invalid and manifest.get('capture')=='full',
                 frames=frames,notes=['Raw surface observations; no logical frame alignment.',
                 'Sample mode skips full snapshots: composites are illustrative and can be stale.',
                 'Clock calls coalesced by returned millisecond and thread; count in b.',
                 'Observation timing includes probe nesting; profile captures separately.'])
    (out/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    table=''.join('<tr>'+''.join(f'<td>{html.escape(str(s[k]))}</td>' for k in ('name','count','total_ms','exclusive_ms','p95_ms','max_ms'))+'</tr>' for s in spans[:35])
    pics=''.join(f'<figure><img loading="lazy" src="{f["file"]}"><figcaption>{f["seq"]} · {f["us"]/1e6:.3f}s · {f["reason"]}</figcaption></figure>' for f in frames[::max(1,len(frames)//60)])
    (out/'report.html').write_text('<!doctype html><meta charset="utf-8"><title>Rich4 original observation</title><style>body{font:15px system-ui;margin:24px;background:#18202a;color:#eee}td,th{padding:6px;border-bottom:1px solid #555}img{max-width:640px}figure{display:inline-block;margin:12px}table{border-collapse:collapse}</style>'
        f'<h1>Rich4 original observation — {summary["status"]}</h1><p>Raw surface snapshots; this is not a lockstep equivalence result. Capture: {html.escape(str(manifest.get("capture")))}. Sample composites may contain stale regions.</p><p>RNG: {rng["status"]}, calls: {rng["calls"]}; lost events: {sum(r["a"] for r in ends)}</p><pre>{html.escape(json.dumps(errors+gfx_errors,indent=2))}</pre><p>{html.escape(str(counts))}</p><table><tr><th>Span / caller RVA</th><th>Count</th><th>Total ms</th><th>Exclusive ms</th><th>p95 ms</th><th>Max ms</th></tr>{table}</table>{pics}',encoding='utf-8')
    print(json.dumps(dict(status=summary['status'],counts=counts,top=spans[:8]),ensure_ascii=False))
    return summary

def run(args):
    if os.name!='nt': raise SystemExit('Windows required')
    if not 1<=args.seconds<=300 or not 0<=args.interval<=60000: raise SystemExit('seconds must be 1..300; interval must be 0..60000 ms')
    source=Path(args.data_dir).resolve(); dx=Path(args.dxwnd_dir).resolve(); out=Path(args.out).resolve()
    if out.exists(): raise SystemExit('Output must be a new directory')
    if sha(source/'Rich4.exe')!=EXE_SHA: raise SystemExit('Unsupported original EXE hash')
    if sha(dx/'dxwnd.dll')!=DXWND_SHA: raise SystemExit('Unsupported DxWnd DLL; this probe is validated with 2.05.68 only')
    # Do not disturb an existing user DxWnd instance or its global target mapping.
    active=subprocess.check_output(['tasklist','/FI','IMAGENAME eq dxwnd.exe','/FO','CSV'],text=True,creationflags=subprocess.CREATE_NO_WINDOW)
    if '"dxwnd.exe"' in active.lower(): raise SystemExit('An existing DxWnd instance must be closed before this isolated run')
    game=out/'game'; runtime=out/'dxwnd'
    if len(str(game/'Rich4.exe'))>=240: raise SystemExit('Output path exceeds native tool path limit')
    overrides=dict(x.split('=',1) for x in args.set)
    text=profile_text(args.profile or dx/'dxwnd.ini',game/'Rich4.exe',overrides)
    out.mkdir(parents=True);game.mkdir();runtime.mkdir()
    for path in source.iterdir():
        if path.is_file() and path.suffix.lower() not in ('.cfg','.ini','.log','.id0','.id1','.id2','.nam','.til','.i64'):
            shutil.copy2(path,game/path.name)
    config=ROOT/'RICH4.CFG'
    if config.exists(): shutil.copy2(config,game/config.name)
    for path in dx.iterdir():
        if path.is_file() and path.suffix.lower() in ('.dll','.exe','.ini'):
            shutil.copy2(path,runtime/path.name)
    (runtime/'dxwnd.ini').write_bytes(text.encode('mbcs'))
    manifest=dict(exe_sha256=EXE_SHA,profile=text,source_profile_sha256=sha(args.profile or dx/'dxwnd.ini'),
        dxwnd_sha256=sha(runtime/'dxwnd.dll'),probe_sha256=sha(ROOT/'bin/frame_diff/original_probe.dll'),
        seconds=args.seconds,capture=args.capture,interval_ms=args.interval,overrides=overrides,scenario=args.scenario,
        platform=platform.platform(),python=platform.python_version(),
        launcher_sha256=sha(ROOT/'bin/frame_diff/original_launcher.exe'),
        resources={p.name:sha(p) for p in game.iterdir() if p.is_file()},run_started=time.time())
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    startup=subprocess.STARTUPINFO();startup.dwFlags=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    server=subprocess.Popen([str(runtime/'dxwnd.exe'),'/T','/Q'],cwd=runtime,startupinfo=startup)
    try:
        ctypes.windll.user32.WaitForInputIdle(int(server._handle),10000)
        time.sleep(.5)
        if server.poll() is not None: raise RuntimeError('DxWnd server exited during bootstrap')
        command=[str(ROOT/'bin/frame_diff/original_launcher.exe'),str(game/'Rich4.exe'),str(runtime/'dxwnd.dll'),
                 str(ROOT/'bin/frame_diff/original_probe.dll'),str(out),str(args.seconds),
                 '0' if args.capture=='none' else '1',str(0 if args.capture=='full' else args.interval)]
        env=os.environ.copy()
        env.pop('R4DIFF_SCENARIO',None)
        if args.scenario!='idle': env['R4DIFF_SCENARIO']=args.scenario
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=args.seconds+65,env=env)
        (out/'launcher.log').write_text(result.stdout,encoding='utf-8'); print(result.stdout)
        manifest['launcher_exit']=result.returncode
    finally:
        server.terminate()
        try:server.wait(timeout=5)
        except subprocess.TimeoutExpired:server.kill();server.wait()
        (out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    summary=report(out) if (out/'events.csv').exists() else None
    if result.returncode: raise SystemExit(result.returncode)
    if not summary or summary['status']!='OBSERVED': raise SystemExit(7)

def compare(args):
    """Compare setup-window costs without claiming synchronized gameplay."""
    records=[]
    for directory in args.runs:
        out=Path(directory)
        manifest=json.loads((out/'manifest.json').read_text(encoding='utf-8'))
        summary=json.loads((out/'summary.json').read_text(encoding='utf-8'))
        with (out/'events.csv').open(newline='') as f:
            rows=[{k:int(v,16 if k in ('caller','object') else 10) for k,v in r.items()} for r in csv.DictReader(f)]
        window=[r for r in rows if args.start*1e6<=r['us']<args.end*1e6]
        costs={}
        for typ,caller in ((9,0x5c2b),(12,0x2348),(12,0x244c)):
            values=[r['duration_us'] for r in window if r['type']==typ and r['caller']==caller]
            if values: costs[f'{NAMES[typ]}@{caller:x}']=distribution(values)
        dispatch=sorted(r['us'] for r in window if r['type']==22 and r['a']==0x113 and r['c']&1)
        gaps=[b-a for a,b in zip(dispatch,dispatch[1:])]
        records.append(dict(run=out.name,renderer=manifest.get('overrides',{}).get('renderer0','profile'),
            capture=manifest['capture'],status=summary['status'],graphics_errors=len(summary['graphics_errors']),
            probe_sha256=manifest['probe_sha256'],resources=manifest['resources'],costs=costs,
            timer_dispatch_gap=distribution(gaps) if gaps else None))
    result=dict(window_seconds=[args.start,args.end],strict_equivalence=False,runs=records,
                same_probe=len({r['probe_sha256'] for r in records})==1,
                same_initial_resources=all(r['resources']==records[0]['resources'] for r in records))
    dest=Path(args.out);dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text(json.dumps(result,indent=2),encoding='utf-8')
    for r in records:
        print(r['run'],r['status'],{k:round(v['mean_ms'],3) for k,v in r['costs'].items()},'graphics_errors',r['graphics_errors'])
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('probe');p.add_argument('--data-dir',required=True);p.add_argument('--dxwnd-dir',required=True)
    p.add_argument('--profile');p.add_argument('--out',required=True);p.add_argument('--seconds',type=int,default=15)
    p.add_argument('--capture',choices=('none','sample','full'),default='sample');p.add_argument('--interval',type=int,default=200)
    p.add_argument('--set',action='append',default=[],metavar='KEY=VALUE')
    p.add_argument('--scenario',choices=('idle','menu','setup','game'),default='idle')
    p=sub.add_parser('report');p.add_argument('out')
    p=sub.add_parser('compare');p.add_argument('runs',nargs='+');p.add_argument('--out',required=True)
    p.add_argument('--start',type=float,default=10);p.add_argument('--end',type=float,default=16)
    args=parser.parse_args()
    if args.command=='probe': run(args)
    elif args.command=='compare': compare(args)
    else:
        if report(args.out)['status']!='OBSERVED': raise SystemExit(7)

if __name__=='__main__':main()
