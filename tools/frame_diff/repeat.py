"""Compare each endpoint to itself across runs with different transport delays."""
import argparse
import hashlib
import json
from pathlib import Path

def signature(result,role):
    return [{k:r[role][k] for k in ('epoch','ms','scene','error','ack','next_ms','masks','traces','sha256','submissions')}
            for r in result['frames']]

def compare_runs(paths):
    manifests=[json.loads((p/'manifest.json').read_text()) for p in paths]
    results=[json.loads((p/'result.json').read_text()) for p in paths]
    mismatches=[]
    for index,(manifest,result) in enumerate(zip(manifests[1:],results[1:]),1):
        for key in ('probe_sha256','sdl_sha256','exe_sha256','dxwnd_sha256','events','origin','date','duration_ms','resources'):
            if manifest[key]!=manifests[0][key]:mismatches.append(dict(run=index,field=key))
        if manifest.get('sdl_inputs')!=manifests[0].get('sdl_inputs'):mismatches.append(dict(run=index,field='sdl_inputs'))
        profiles=[[line for line in m['profile'].splitlines() if not line.startswith(('path0=','startfolder0='))] for m in (manifests[0],manifest)]
        if profiles[0]!=profiles[1]:mismatches.append(dict(run=index,field='dxwnd_profile'))
        for role in ('original','sdl'):
            a,b=signature(results[0],role),signature(result,role)
            if a!=b:
                first=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
                mismatches.append(dict(run=index,role=role,first_epoch=first,
                    fields=[k for k in a[first] if a[first][k]!=b[first][k]] if first<min(len(a),len(b)) else ['length']))
    for i,result in enumerate(results):
        if result['status'] in ('HANG','CRASH','INVALID_CAPTURE','UNSUPPORTED') or any(result['exit_codes']):
            mismatches.append(dict(run=i,field='invalid_run',status=result['status']))
        for frame in result['frames']:
            for role in ('original','sdl'):
                f=frame[role]
                files=[(f'{frame["epoch"]:05d}.rgb565',f['sha256'])]+[(s['file'],s['sha256']) for s in f['submissions']]
                for name,digest in files:
                    path=paths[i]/role/name
                    if not path.is_file() or path.stat().st_size!=640*480*2 or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
                        mismatches.append(dict(run=i,field='corrupt_frame',file=str(path)))
    return dict(status='REPRODUCIBLE' if not mismatches else 'NOT_REPRODUCIBLE',
                runs=[str(p.resolve()) for p in paths],delays=[m['delay_ms'] for m in manifests],mismatches=mismatches)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runs',nargs='+',type=Path);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    if len(args.runs)<2:parser.error('at least two runs are required')
    result=compare_runs(args.runs)
    args.out.write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result))
    return 0 if result['status']=='REPRODUCIBLE' else 2

if __name__=='__main__':raise SystemExit(main())
