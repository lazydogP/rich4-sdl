"""Run the animation regression bundle, preserving every failure's output."""
import json
from pathlib import Path
import subprocess
import sys

results = []
for name in sys.argv[1:]:
    path = Path(name)
    if not path.exists() and path.with_suffix('.exe').exists():
        path = path.with_suffix('.exe')
    try:
        result = subprocess.run([str(path.resolve())], capture_output=True,
                                timeout=45)
        output = (result.stdout + result.stderr).decode('utf-8', errors='replace')
        code = result.returncode
    except subprocess.TimeoutExpired:
        output, code = 'Test exceeded 45 seconds', -1
    results.append({'test': path.name, 'exit_code': code, 'output': output})
    print(f'{path.name}: {"PASS" if code == 0 else "FAIL"}', flush=True)
    if code:
        print(output[-5000:], flush=True)
destination = Path('build/miyoo/flc-regression.json')
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding='utf-8')
sys.exit(any(item['exit_code'] for item in results))
