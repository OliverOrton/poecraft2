"""One-run deadline wrapper; uses the existing process-tree watchdog owner."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
RUN = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'tools/ingest'))
from poecraft_ingest.solver_worker import run_isolated_process

label, seconds, *command = sys.argv[1:]
limit = float(seconds)
stop = datetime(2026, 9, 8, 0, 50, tzinfo=timezone.utc)
started = datetime.now(timezone.utc)
if not command or limit <= 0 or (stop - started).total_seconds() < limit + 10:
    raise SystemExit('Operation cannot fit before 17:50 Vancouver cleanup cutoff')
prefix = RUN / label
if prefix.with_suffix('.process.json').exists():
    raise SystemExit('Refusing to overwrite an existing run record')
lock = RUN / 'active-process.json'
with lock.open('x', encoding='utf-8') as f:
    json.dump({'wrapper_pid': os.getpid(), 'command': command, 'started_utc': started.isoformat()}, f)
def began(pid, token):
    lock.write_text(json.dumps({'wrapper_pid': os.getpid(), 'process_id': pid,
        'process_identity_token': token, 'command': command,
        'started_utc': started.isoformat()}, indent=2), encoding='utf-8')
try:
    result = run_isolated_process(command, watchdog_seconds=limit, cwd=ROOT,
        on_started=began,
        cancel_requested=lambda: datetime.now(timezone.utc) >= stop or (RUN/'cancel').exists())
    prefix.with_suffix('.stdout.txt').write_text(result.pop('output'), encoding='utf-8')
    result.update(command=command, started_utc=started.isoformat(),
        ended_utc=datetime.now(timezone.utc).isoformat(), timeout_seconds=limit)
    prefix.with_suffix('.process.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result))
    raise SystemExit(0 if result['exit_code'] == 0 and not result['timed_out'] and
        not result['canceled'] and not result['survivor'] else 1)
finally:
    lock.unlink()
