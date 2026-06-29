"""A/B check: compiling the adapter must not change unconnected smoke runs."""
import importlib.util
import json
import os
from pathlib import Path

root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('runtime_smoke',root/'tests/sdl/test_runtime_smoke.py')
smoke=importlib.util.module_from_spec(spec);spec.loader.exec_module(smoke)
os.environ.pop('R4DIFF_LINK',None)
results=[]
for game,count in ((False,2),(True,120)):
    a=smoke.run_smoke(root,root/'bin/rich4_sdl.exe',game,count)
    b=smoke.run_smoke(root,root/'.local/frame-diff-bin/rich4_sdl.exe',game,count)
    results.append(dict(game=game,frames=count,normal=a,instrumented_disconnected=b,equal=a==b))
path=root/'.local/frame-diff-runs/inactive-adapter.json'
path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(results,indent=2))
print(json.dumps(results))
raise SystemExit(0 if all(r['equal'] for r in results) else 1)
